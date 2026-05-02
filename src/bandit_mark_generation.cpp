#include "bandit_mark_generation.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>

namespace bandit_mark_generation
{
namespace
{
int clamp_nonnegative( int value )
{
    return std::max( 0, value );
}

std::string join_notes( const std::vector<std::string> &notes )
{
    if( notes.empty() ) {
        return "none";
    }

    std::ostringstream out;
    for( size_t i = 0; i < notes.size(); ++i ) {
        if( i > 0 ) {
            out << "; ";
        }
        out << notes[i];
    }
    return out.str();
}

int smoke_weather_range_penalty( smoke_weather_band weather )
{
    switch( weather ) {
        case smoke_weather_band::clear:
            return 0;
        case smoke_weather_band::windy:
            return 1;
        case smoke_weather_band::rain:
            return 2;
        case smoke_weather_band::fog:
            return 3;
        case smoke_weather_band::portal_storm:
            return 1;
    }

    return 0;
}

int smoke_weather_confidence_penalty( smoke_weather_band weather )
{
    switch( weather ) {
        case smoke_weather_band::clear:
            return 0;
        case smoke_weather_band::windy:
            return 1;
        case smoke_weather_band::rain:
            return 1;
        case smoke_weather_band::fog:
            return 2;
        case smoke_weather_band::portal_storm:
            return 2;
    }

    return 0;
}

int smoke_weather_source_precision_penalty( smoke_weather_band weather )
{
    switch( weather ) {
        case smoke_weather_band::clear:
            return 0;
        case smoke_weather_band::windy:
            return 2;
        case smoke_weather_band::rain:
            return 1;
        case smoke_weather_band::fog:
            return 1;
        case smoke_weather_band::portal_storm:
            return 2;
    }

    return 0;
}

int smoke_weather_displacement_bias( smoke_weather_band weather )
{
    switch( weather ) {
        case smoke_weather_band::clear:
        case smoke_weather_band::rain:
        case smoke_weather_band::fog:
            return 0;
        case smoke_weather_band::windy:
            return 1;
        case smoke_weather_band::portal_storm:
            return 2;
    }

    return 0;
}

std::string smoke_weather_origin_hint( const smoke_packet &packet,
                                       const smoke_weather_effect &effect,
                                       bool viable )
{
    if( !viable ) {
        return "none";
    }
    if( effect.displacement_bias >= 2 || packet.spread_bias > packet.height_bias ) {
        return "corridorish";
    }
    if( effect.displacement_bias > 0 || effect.source_precision_penalty > 0 ) {
        return "drifted";
    }
    return "localized";
}

std::string smoke_weather_verdict( smoke_weather_band weather, bool viable )
{
    if( !viable ) {
        return "blocked";
    }
    switch( weather ) {
        case smoke_weather_band::clear:
            return "allowed";
        case smoke_weather_band::windy:
        case smoke_weather_band::portal_storm:
            return "fuzzed";
        case smoke_weather_band::rain:
        case smoke_weather_band::fog:
            return "reduced";
    }

    return "unknown";
}

bool nearby_cross_z_visible( int vertical_offset, bool vertical_sightline )
{
    return vertical_sightline && vertical_offset != 0 && std::abs( vertical_offset ) <= 2;
}

std::string describe_smoke_weather( const smoke_packet &packet,
                                    const smoke_weather_effect &effect,
                                    int projected_range_omt,
                                    int visibility_score,
                                    bool viable )
{
    return "smoke weather: verdict=" + smoke_weather_verdict( packet.weather, viable ) +
           ", weather=" + to_string( packet.weather ) +
           ", range_penalty=" + std::to_string( effect.range_penalty ) +
           ", confidence_penalty=" + std::to_string( effect.confidence_penalty ) +
           ", source_precision_penalty=" + std::to_string( effect.source_precision_penalty ) +
           ", displacement_bias=" + std::to_string( effect.displacement_bias ) +
           ", vertical_offset=" + std::to_string( packet.vertical_offset ) +
           ", vertical_sightline=" + std::string( packet.vertical_sightline ? "yes" : "no" ) +
           ", nearby_cross_z_visible=" + std::string( effect.nearby_cross_z_visible ? "yes" : "no" ) +
           ", vertical_range_bonus=" + std::to_string( effect.vertical_range_bonus ) +
           ", origin_hint=" + smoke_weather_origin_hint( packet, effect, viable ) +
           ", projected_range_omt=" + std::to_string( projected_range_omt ) +
           ", visibility_score=" + std::to_string( visibility_score );
}

int light_time_penalty( light_time_band time )
{
    switch( time ) {
        case light_time_band::daylight:
            return 6;
        case light_time_band::twilight:
            return 2;
        case light_time_band::night:
            return 0;
    }

    return 0;
}

bool portal_storm_bright_light_window( const light_packet &packet )
{
    return packet.weather == light_weather_band::portal_storm &&
           packet.time != light_time_band::daylight &&
           packet.exposure == light_exposure_band::exposed &&
           ( packet.source == light_source_band::searchlight || packet.source_strength >= 2 );
}

int light_weather_penalty( const light_packet &packet )
{
    switch( packet.weather ) {
        case light_weather_band::clear:
            return 0;
        case light_weather_band::rain:
            return 2;
        case light_weather_band::fog:
            return 3;
        case light_weather_band::portal_storm: {
            int penalty = 3;
            if( packet.exposure == light_exposure_band::contained ) {
                penalty += 1;
            }
            if( portal_storm_bright_light_window( packet ) ) {
                penalty -= 2;
            }
            return std::max( 1, penalty );
        }
    }

    return 0;
}

int light_vertical_penalty( const light_packet &packet )
{
    if( packet.vertical_offset == 0 || packet.vertical_sightline ) {
        return 0;
    }
    return 2 + std::min( std::abs( packet.vertical_offset ), 2 );
}

int light_nearby_cross_z_bonus( const light_packet &packet )
{
    if( !nearby_cross_z_visible( packet.vertical_offset, packet.vertical_sightline ) ||
        packet.exposure == light_exposure_band::contained ) {
        return 0;
    }
    return std::min( std::abs( packet.vertical_offset ), 2 );
}

int light_elevated_exposure_bonus( const light_packet &packet )
{
    if( !packet.vertical_sightline || packet.exposure != light_exposure_band::exposed ) {
        return 0;
    }
    return std::clamp( packet.elevation_bonus + std::max( 0, std::abs( packet.vertical_offset ) - 1 ),
                       0, 10 );
}

int light_exposure_bonus( light_exposure_band exposure )
{
    switch( exposure ) {
        case light_exposure_band::contained:
            return -3;
        case light_exposure_band::screened:
            return 0;
        case light_exposure_band::exposed:
            return 2;
    }

    return 0;
}

int light_source_bonus( light_source_band source )
{
    switch( source ) {
        case light_source_band::ordinary:
            return 0;
        case light_source_band::searchlight:
            return 2;
    }

    return 0;
}

int light_terrain_penalty( light_terrain_band terrain )
{
    switch( terrain ) {
        case light_terrain_band::open:
            return 0;
        case light_terrain_band::built_cover:
            return 1;
        case light_terrain_band::forest:
            return 2;
    }

    return 0;
}

std::string light_concealment_verdict( const light_packet &packet,
                                       const light_concealment_packet &concealment,
                                       bool viable )
{
    if( !viable ) {
        return "blocked";
    }
    if( concealment.daylight_penalty == 0 && concealment.weather_penalty == 0 &&
        concealment.terrain_penalty == 0 && packet.exposure == light_exposure_band::exposed ) {
        return "allowed";
    }
    return "reduced";
}

std::string describe_light_concealment( const light_packet &packet,
                                        const light_concealment_packet &concealment,
                                        int projected_range_omt,
                                        int visibility_score,
                                        bool viable )
{
    return "light concealment: verdict=" + light_concealment_verdict( packet, concealment, viable ) +
           ", weather=" + to_string( packet.weather ) +
           ", time_penalty=" + std::to_string( concealment.daylight_penalty ) +
           ", weather_penalty=" + std::to_string( concealment.weather_penalty ) +
           ", exposure=" + to_string( packet.exposure ) +
           ", side_leakage=" + std::to_string( concealment.side_leakage_bonus ) +
           ", terrain=" + to_string( packet.terrain ) +
           ", terrain_penalty=" + std::to_string( concealment.terrain_penalty ) +
           ", vertical_offset=" + std::to_string( packet.vertical_offset ) +
           ", vertical_sightline=" + std::string( packet.vertical_sightline ? "yes" : "no" ) +
           ", vertical_penalty=" + std::to_string( concealment.vertical_penalty ) +
           ", nearby_cross_z_visible=" + std::string( concealment.nearby_cross_z_visible ? "yes" : "no" ) +
           ", nearby_cross_z_bonus=" + std::to_string( concealment.nearby_cross_z_bonus ) +
           ", elevation_bonus=" + std::to_string( packet.elevation_bonus ) +
           ", elevated_exposure_bonus=" + std::to_string( concealment.elevated_exposure_bonus ) +
           ", elevated_exposure_extended=" + std::string( concealment.elevated_exposure_extended ? "yes" : "no" ) +
           ", storm_bright_light_preserved=" +
           std::string( concealment.storm_bright_light_preserved ? "yes" : "no" ) +
           ", projected_range_omt=" + std::to_string( projected_range_omt ) +
           ", visibility_score=" + std::to_string( visibility_score );
}

int human_route_origin_bonus( human_route_origin origin )
{
    switch( origin ) {
        case human_route_origin::same_camp:
            return -4;
        case human_route_origin::ambiguous:
            return 0;
        case human_route_origin::external:
            return 1;
        case human_route_origin::shared:
            return 1;
    }

    return 0;
}

int human_route_corroboration_bonus( human_route_corroboration corroboration )
{
    switch( corroboration ) {
        case human_route_corroboration::none:
            return 0;
        case human_route_corroboration::corridor:
            return 1;
        case human_route_corroboration::site:
            return 2;
    }

    return 0;
}

bandit_dry_run::lead_family human_route_projected_family( const human_route_packet &packet )
{
    if( packet.kind == human_route_kind::direct_sighting ) {
        return bandit_dry_run::lead_family::moving_carrier;
    }
    if( packet.family == bandit_dry_run::lead_family::site &&
        packet.corroboration == human_route_corroboration::site ) {
        return bandit_dry_run::lead_family::site;
    }
    return bandit_dry_run::lead_family::corridor;
}

bool human_route_has_external_footing( const human_route_packet &packet )
{
    return packet.origin == human_route_origin::external ||
           packet.origin == human_route_origin::shared ||
           packet.corroboration != human_route_corroboration::none;
}

std::string effective_mark_id( const signal_input &signal )
{
    if( !signal.id.empty() ) {
        return signal.id;
    }
    if( !signal.envelope_id.empty() ) {
        return signal.envelope_id;
    }
    return signal.region_id;
}

std::string effective_envelope_id( const signal_input &signal )
{
    if( !signal.envelope_id.empty() ) {
        return signal.envelope_id;
    }
    return effective_mark_id( signal );
}

std::string effective_region_id( const signal_input &signal )
{
    if( !signal.region_id.empty() ) {
        return signal.region_id;
    }
    return effective_envelope_id( signal );
}

bool is_repeated_site_signal_kind( const std::string &kind )
{
    return kind == "smoke" || kind == "light" || kind == "route_activity";
}

void record_repeated_site_signal_kind( repeated_site_reinforcement_packet &packet,
                                       const std::string &kind )
{
    if( kind == "smoke" ) {
        packet.saw_smoke = true;
    } else if( kind == "light" ) {
        packet.saw_light = true;
    } else if( kind == "route_activity" ) {
        packet.saw_route_activity = true;
    }

    packet.distinct_signal_count = ( packet.saw_smoke ? 1 : 0 ) +
                                   ( packet.saw_light ? 1 : 0 ) +
                                   ( packet.saw_route_activity ? 1 : 0 );
}

std::string repeated_site_signal_mix( const repeated_site_reinforcement_packet &packet )
{
    std::vector<std::string> kinds;
    if( packet.saw_smoke ) {
        kinds.emplace_back( "smoke" );
    }
    if( packet.saw_light ) {
        kinds.emplace_back( "light" );
    }
    if( packet.saw_route_activity ) {
        kinds.emplace_back( "route_activity" );
    }
    return join_notes( kinds );
}

bool qualifies_for_repeated_site_reinforcement( const signal_input &signal )
{
    return signal.family == bandit_dry_run::lead_family::site &&
           !signal.confirmed_threat &&
           is_repeated_site_signal_kind( signal.kind );
}

void seed_repeated_site_reinforcement( typed_mark &mark, const signal_input &signal )
{
    if( !qualifies_for_repeated_site_reinforcement( signal ) ) {
        return;
    }

    repeated_site_reinforcement_packet &packet = mark.repeated_site_reinforcement;
    packet.total_site_hits = 1;
    record_repeated_site_signal_kind( packet, signal.kind );
}

void apply_repeated_site_reinforcement( typed_mark &mark, const signal_input &signal )
{
    if( !qualifies_for_repeated_site_reinforcement( signal ) ) {
        return;
    }

    repeated_site_reinforcement_packet &packet = mark.repeated_site_reinforcement;
    if( packet.total_site_hits <= 0 ) {
        seed_repeated_site_reinforcement( mark, signal );
        return;
    }

    const int old_confidence_bonus = packet.confidence_bonus;
    const int old_bounty_bonus = packet.bounty_bonus;

    packet.total_site_hits = std::min( 6, packet.total_site_hits + 1 );
    record_repeated_site_signal_kind( packet, signal.kind );
    packet.viable = packet.total_site_hits >= 2 && packet.distinct_signal_count >= 2;
    if( packet.viable ) {
        packet.confidence_bonus = std::clamp( packet.distinct_signal_count - 1, 0, 2 );
        packet.bounty_bonus = std::clamp( packet.distinct_signal_count - 1, 0, 2 );
    } else {
        packet.confidence_bonus = 0;
        packet.bounty_bonus = 0;
    }

    const int confidence_delta = packet.confidence_bonus - old_confidence_bonus;
    const int bounty_delta = packet.bounty_bonus - old_bounty_bonus;
    if( confidence_delta > 0 ) {
        mark.confidence = std::min( 6, mark.confidence + confidence_delta );
    }
    if( bounty_delta > 0 ) {
        mark.bounty_add = std::min( 6, mark.bounty_add + bounty_delta );
    }
}

bool is_moving_family( bandit_dry_run::lead_family family )
{
    return family == bandit_dry_run::lead_family::moving_carrier ||
           family == bandit_dry_run::lead_family::corridor;
}

bool same_mark_key( const typed_mark &mark, const signal_input &signal )
{
    return mark.family == signal.family && mark.envelope_id == effective_envelope_id( signal );
}

int moving_memory_leash_reset_for( bandit_dry_run::lead_family family )
{
    switch( family ) {
        case bandit_dry_run::lead_family::moving_carrier:
            return 100;
        case bandit_dry_run::lead_family::corridor:
            return 120;
        case bandit_dry_run::lead_family::none:
        case bandit_dry_run::lead_family::site:
        case bandit_dry_run::lead_family::friendly_pressure:
            return 0;
    }

    return 0;
}

int moving_memory_confidence_band( const signal_input &signal )
{
    return std::clamp( clamp_nonnegative( signal.confidence ), 1, 3 );
}

int moving_memory_opportunity_band( const signal_input &signal )
{
    return std::clamp( clamp_nonnegative( signal.strength ) + clamp_nonnegative( signal.bounty_add ), 0, 3 );
}

int moving_memory_threat_band( const signal_input &signal )
{
    return std::clamp( clamp_nonnegative( signal.threat_add ) +
                       clamp_nonnegative( signal.monster_pressure_add ), 0, 3 );
}

std::string describe_moving_memory( const moving_bounty_memory &memory )
{
    std::ostringstream out;
    out << "active=" << ( memory.active ? "yes" : "no" )
        << ", source_family=" << bandit_dry_run::to_string( memory.source_family )
        << ", last_known_region=" << ( memory.last_known_region_id.empty() ? "none" : memory.last_known_region_id )
        << ", confidence_band=" << memory.confidence_band
        << ", leash_remaining=" << memory.leash_turns_remaining
        << ", opportunity_band=" << memory.opportunity_band
        << ", threat_band=" << memory.threat_band
        << ", last_transition=" << memory.last_transition;
    if( !memory.drop_reason.empty() ) {
        out << ", drop_reason=" << memory.drop_reason;
    }
    if( memory.review_pending ) {
        out << ", review_pending=yes";
    }
    return out.str();
}

void seed_moving_memory( typed_mark &mark, const signal_input &signal )
{
    if( !is_moving_family( signal.family ) ) {
        return;
    }

    mark.moving_memory.active = true;
    mark.moving_memory.review_pending = false;
    mark.moving_memory.source_family = signal.family;
    mark.moving_memory.last_known_region_id = effective_region_id( signal );
    mark.moving_memory.confidence_band = moving_memory_confidence_band( signal );
    mark.moving_memory.leash_turns_remaining = moving_memory_leash_reset_for( signal.family );
    mark.moving_memory.opportunity_band = moving_memory_opportunity_band( signal );
    mark.moving_memory.threat_band = moving_memory_threat_band( signal );
    mark.moving_memory.last_transition = "refreshed";
    mark.moving_memory.drop_reason.clear();
}

void drop_moving_memory( typed_mark &mark, const std::string &reason, int tick )
{
    if( mark.moving_memory.source_family == bandit_dry_run::lead_family::none ) {
        mark.moving_memory.source_family = mark.family;
    }
    mark.moving_memory.active = false;
    mark.moving_memory.review_pending = true;
    mark.moving_memory.last_transition = "dropped";
    mark.moving_memory.drop_reason = reason;
    mark.moving_memory.leash_turns_remaining = 0;
    mark.moving_memory.confidence_band = 0;
    mark.moving_memory.opportunity_band = 0;
    mark.family = bandit_dry_run::lead_family::none;
    mark.strength = 0;
    mark.confidence = 0;
    mark.bounty_add = 0;
    mark.target_coherence_subtract = 0;
    mark.reward_profile_match = 0;
    mark.distance_multiplier = 0.0;
    mark.hard_blocked_jobs.clear();
    mark.last_refresh_tick = tick;
}

void refresh_moving_memory( typed_mark &mark, const signal_input &signal )
{
    if( !is_moving_family( signal.family ) ) {
        return;
    }

    if( signal.threat_gate_result == bandit_dry_run::threat_gate::hard_veto ||
        moving_memory_threat_band( signal ) >= 3 ) {
        drop_moving_memory( mark, "danger spike closed the moving pursuit window", mark.last_refresh_tick );
        return;
    }

    const std::string old_region = mark.moving_memory.last_known_region_id;
    mark.moving_memory.active = true;
    mark.moving_memory.review_pending = false;
    mark.moving_memory.source_family = signal.family;
    mark.moving_memory.last_known_region_id = effective_region_id( signal );
    mark.moving_memory.confidence_band = moving_memory_confidence_band( signal );
    mark.moving_memory.leash_turns_remaining = moving_memory_leash_reset_for( signal.family );
    mark.moving_memory.opportunity_band = moving_memory_opportunity_band( signal );
    mark.moving_memory.threat_band = moving_memory_threat_band( signal );
    mark.moving_memory.last_transition = old_region.empty() || old_region == mark.moving_memory.last_known_region_id ?
                                         "refreshed" : "narrowed";
    mark.moving_memory.drop_reason.clear();
}

int strength_decay_for( cadence_tier tier )
{
    switch( tier ) {
        case cadence_tier::nearby_active:
            return 0;
        case cadence_tier::distant_inactive:
            return 1;
        case cadence_tier::daily_cleanup:
            return 1;
    }

    return 0;
}

int confidence_decay_for( cadence_tier tier )
{
    switch( tier ) {
        case cadence_tier::nearby_active:
            return 1;
        case cadence_tier::distant_inactive:
            return 1;
        case cadence_tier::daily_cleanup:
            return 1;
    }

    return 0;
}

int bounty_decay_for( cadence_tier tier )
{
    switch( tier ) {
        case cadence_tier::nearby_active:
            return 0;
        case cadence_tier::distant_inactive:
            return 1;
        case cadence_tier::daily_cleanup:
            return 1;
    }

    return 0;
}

int threat_decay_for_soft_marks( cadence_tier tier )
{
    switch( tier ) {
        case cadence_tier::nearby_active:
            return 0;
        case cadence_tier::distant_inactive:
            return 1;
        case cadence_tier::daily_cleanup:
            return 1;
    }

    return 0;
}

const heat_cell *find_heat( const ledger_state &state, const std::string &region_id )
{
    const auto it = std::find_if( state.heat.begin(), state.heat.end(), [&region_id]( const heat_cell &cell ) {
        return cell.region_id == region_id;
    } );
    return it == state.heat.end() ? nullptr : &*it;
}

void rebuild_heat( ledger_state &state )
{
    std::map<std::string, heat_cell> rebuilt;
    for( const typed_mark &mark : state.marks ) {
        heat_cell &cell = rebuilt[mark.region_id];
        cell.region_id = mark.region_id;
        cell.threat_heat += mark.threat_add + mark.monster_pressure_add;
        cell.bounty_heat += mark.bounty_add + mark.target_coherence_subtract;
        cell.mark_count++;
        if( mark.confirmed_threat ) {
            cell.confirmed_threat_marks++;
        }
    }

    state.heat.clear();
    state.heat.reserve( rebuilt.size() );
    for( const auto &entry : rebuilt ) {
        state.heat.push_back( entry.second );
    }
}

void sort_ledger( ledger_state &state )
{
    std::sort( state.marks.begin(), state.marks.end(), []( const typed_mark &lhs, const typed_mark &rhs ) {
        if( lhs.region_id != rhs.region_id ) {
            return lhs.region_id < rhs.region_id;
        }
        if( lhs.envelope_id != rhs.envelope_id ) {
            return lhs.envelope_id < rhs.envelope_id;
        }
        return lhs.id < rhs.id;
    } );
    std::sort( state.heat.begin(), state.heat.end(), []( const heat_cell &lhs, const heat_cell &rhs ) {
        return lhs.region_id < rhs.region_id;
    } );
}

std::string describe_repeated_site_reinforcement( const repeated_site_reinforcement_packet &packet )
{
    if( packet.total_site_hits <= 0 ) {
        return "none";
    }

    std::ostringstream out;
    out << "hits=" << packet.total_site_hits
        << ", kinds=" << repeated_site_signal_mix( packet )
        << ", distinct=" << packet.distinct_signal_count
        << ", confidence_bonus=" << packet.confidence_bonus
        << ", bounty_bonus=" << packet.bounty_bonus
        << ", viable=" << ( packet.viable ? "yes" : "no" );
    return out.str();
}
} // namespace

std::string to_string( cadence_tier tier )
{
    switch( tier ) {
        case cadence_tier::nearby_active:
            return "nearby_active";
        case cadence_tier::distant_inactive:
            return "distant_inactive";
        case cadence_tier::daily_cleanup:
            return "daily_cleanup";
    }

    return "unknown";
}

std::string to_string( smoke_weather_band weather )
{
    switch( weather ) {
        case smoke_weather_band::clear:
            return "clear";
        case smoke_weather_band::windy:
            return "windy";
        case smoke_weather_band::rain:
            return "rain";
        case smoke_weather_band::fog:
            return "fog";
        case smoke_weather_band::portal_storm:
            return "portal_storm";
    }

    return "unknown";
}

std::string to_string( light_time_band time )
{
    switch( time ) {
        case light_time_band::daylight:
            return "daylight";
        case light_time_band::twilight:
            return "twilight";
        case light_time_band::night:
            return "night";
    }

    return "unknown";
}

std::string to_string( light_weather_band weather )
{
    switch( weather ) {
        case light_weather_band::clear:
            return "clear";
        case light_weather_band::rain:
            return "rain";
        case light_weather_band::fog:
            return "fog";
        case light_weather_band::portal_storm:
            return "portal_storm";
    }

    return "unknown";
}

std::string to_string( light_exposure_band exposure )
{
    switch( exposure ) {
        case light_exposure_band::contained:
            return "contained";
        case light_exposure_band::screened:
            return "screened";
        case light_exposure_band::exposed:
            return "exposed";
    }

    return "unknown";
}

std::string to_string( light_source_band source )
{
    switch( source ) {
        case light_source_band::ordinary:
            return "ordinary";
        case light_source_band::searchlight:
            return "searchlight";
    }

    return "unknown";
}

std::string to_string( light_terrain_band terrain )
{
    switch( terrain ) {
        case light_terrain_band::open:
            return "open";
        case light_terrain_band::built_cover:
            return "built_cover";
        case light_terrain_band::forest:
            return "forest";
    }

    return "unknown";
}

std::string to_string( human_route_kind kind )
{
    switch( kind ) {
        case human_route_kind::direct_sighting:
            return "direct_sighting";
        case human_route_kind::route_activity:
            return "route_activity";
    }

    return "unknown";
}

std::string to_string( human_route_origin origin )
{
    switch( origin ) {
        case human_route_origin::same_camp:
            return "same_camp";
        case human_route_origin::ambiguous:
            return "ambiguous";
        case human_route_origin::external:
            return "external";
        case human_route_origin::shared:
            return "shared";
    }

    return "unknown";
}

std::string to_string( human_route_corroboration corroboration )
{
    switch( corroboration ) {
        case human_route_corroboration::none:
            return "none";
        case human_route_corroboration::corridor:
            return "corridor";
        case human_route_corroboration::site:
            return "site";
    }

    return "unknown";
}

smoke_projection adapt_smoke_packet( const smoke_packet &packet )
{
    smoke_projection projection;
    projection.packet = packet;

    projection.weather_effect.range_penalty = smoke_weather_range_penalty( packet.weather );
    projection.weather_effect.confidence_penalty = smoke_weather_confidence_penalty( packet.weather );
    projection.weather_effect.source_precision_penalty =
        smoke_weather_source_precision_penalty( packet.weather );
    projection.weather_effect.displacement_bias = smoke_weather_displacement_bias( packet.weather );
    projection.weather_effect.vertical_range_bonus = 0;
    projection.weather_effect.nearby_cross_z_visible = nearby_cross_z_visible( packet.vertical_offset,
                                                     packet.vertical_sightline );

    projection.visibility_score = std::max( 0, packet.source_strength + packet.persistence +
                                            packet.height_bias - packet.spread_bias -
                                            projection.weather_effect.range_penalty +
                                            projection.weather_effect.vertical_range_bonus );
    projection.projected_range_omt = std::clamp( 4 + packet.source_strength * 2 + packet.persistence * 2 +
                                     packet.height_bias - packet.spread_bias -
                                     projection.weather_effect.range_penalty +
                                     projection.weather_effect.vertical_range_bonus, 1, 15 );
    projection.viable = projection.visibility_score > 0 &&
                        packet.observed_range_omt <= projection.projected_range_omt;
    projection.weather_effect.verdict = smoke_weather_verdict( packet.weather, projection.viable );
    projection.weather_effect.origin_hint = smoke_weather_origin_hint( packet,
                                          projection.weather_effect, projection.viable );
    projection.weather_effect.summary = describe_smoke_weather( packet, projection.weather_effect,
                                      projection.projected_range_omt, projection.visibility_score,
                                      projection.viable );
    projection.review_summary = projection.weather_effect.summary;

    if( !projection.viable ) {
        return projection;
    }

    signal_input signal;
    signal.id = packet.id;
    signal.kind = "smoke";
    signal.envelope_id = packet.envelope_id;
    signal.region_id = packet.region_id;
    signal.family = packet.family;
    signal.strength = 1;
    signal.confidence = std::max( 0, 1 - projection.weather_effect.confidence_penalty );
    signal.threat_add = 0;
    signal.bounty_add = 1;
    signal.reward_profile_match = 0;
    signal.distance_multiplier = std::clamp( 1.10 - static_cast<double>( packet.observed_range_omt ) /
                                 static_cast<double>( std::max( 1, projection.projected_range_omt ) ) -
                                 0.05 * static_cast<double>( projection.weather_effect.source_precision_penalty ),
                                 0.20, 1.0 );
    signal.threat_gate_result = bandit_dry_run::threat_gate::discount_only;
    signal.hard_blocked_jobs = {
        bandit_dry_run::job_template::scavenge,
        bandit_dry_run::job_template::steal,
        bandit_dry_run::job_template::raid,
    };
    signal.confirmed_threat = false;
    signal.soft_decay = true;
    signal.notes = packet.notes;
    signal.notes.push_back( "smoke packet: weather=" + to_string( packet.weather ) +
                            ", observed_range_omt=" + std::to_string( packet.observed_range_omt ) +
                            ", projected_range_omt=" + std::to_string( projection.projected_range_omt ) +
                            ", visibility_score=" + std::to_string( projection.visibility_score ) );
    signal.notes.push_back( projection.weather_effect.summary );
    signal.notes.push_back( "smoke stays bounty-first: this is worth scoping out, not free loot or identity truth" );

    projection.signal = signal;
    return projection;
}

light_projection adapt_light_packet( const light_packet &packet )
{
    light_projection projection;
    projection.packet = packet;

    const int source_bonus = light_source_bonus( packet.source );
    projection.concealment.daylight_penalty = light_time_penalty( packet.time );
    projection.concealment.weather_penalty = light_weather_penalty( packet );
    projection.concealment.exposure_bonus = light_exposure_bonus( packet.exposure );
    projection.concealment.side_leakage_bonus = std::clamp( packet.side_leakage, 0, 2 );
    projection.concealment.terrain_penalty = light_terrain_penalty( packet.terrain );
    projection.concealment.vertical_penalty = light_vertical_penalty( packet );
    projection.concealment.nearby_cross_z_bonus = light_nearby_cross_z_bonus( packet );
    projection.concealment.elevated_exposure_bonus = light_elevated_exposure_bonus( packet );
    projection.concealment.storm_bright_light_preserved = portal_storm_bright_light_window( packet );
    projection.concealment.nearby_cross_z_visible = nearby_cross_z_visible( packet.vertical_offset,
                                                  packet.vertical_sightline );
    projection.concealment.elevated_exposure_extended =
        projection.concealment.elevated_exposure_bonus > 0;
    projection.concealment.visibility_modifier = projection.concealment.exposure_bonus +
                                                 projection.concealment.side_leakage_bonus +
                                                 projection.concealment.nearby_cross_z_bonus +
                                                 projection.concealment.elevated_exposure_bonus -
                                                 projection.concealment.terrain_penalty -
                                                 projection.concealment.weather_penalty -
                                                 projection.concealment.daylight_penalty -
                                                 projection.concealment.vertical_penalty;
    projection.visibility_score = std::max( 0, packet.source_strength + packet.persistence +
                                            source_bonus + projection.concealment.visibility_modifier );
    projection.projected_range_omt = std::clamp( 1 + packet.source_strength * 2 + packet.persistence +
                                     source_bonus + projection.concealment.visibility_modifier, 0, 30 );
    projection.viable = projection.visibility_score > 0 &&
                        packet.observed_range_omt <= projection.projected_range_omt;
    projection.concealment.verdict = light_concealment_verdict( packet, projection.concealment,
                                    projection.viable );
    projection.concealment.summary = describe_light_concealment( packet, projection.concealment,
                                    projection.projected_range_omt, projection.visibility_score,
                                    projection.viable );
    projection.review_summary = projection.concealment.summary;

    if( !projection.viable ) {
        return projection;
    }

    signal_input signal;
    signal.id = packet.id;
    signal.kind = packet.source == light_source_band::searchlight ? "searchlight" : "light";
    signal.envelope_id = packet.envelope_id;
    signal.region_id = packet.region_id;
    signal.family = packet.family;
    signal.strength = 1;
    signal.confidence = 1;
    signal.threat_add = packet.source == light_source_band::searchlight ? 1 : 0;
    signal.bounty_add = packet.source == light_source_band::searchlight ? 0 : 1;
    signal.reward_profile_match = 0;
    signal.distance_multiplier = std::clamp( 1.10 - static_cast<double>( packet.observed_range_omt ) /
                                 static_cast<double>( std::max( 1, projection.projected_range_omt ) ),
                                 0.25, 1.0 );
    signal.threat_gate_result = packet.source == light_source_band::searchlight ?
                                bandit_dry_run::threat_gate::soft_veto :
                                bandit_dry_run::threat_gate::discount_only;
    signal.hard_blocked_jobs = {
        bandit_dry_run::job_template::scavenge,
        bandit_dry_run::job_template::steal,
        bandit_dry_run::job_template::raid,
    };
    if( packet.source == light_source_band::searchlight &&
        packet.family == bandit_dry_run::lead_family::corridor ) {
        signal.hard_blocked_jobs.push_back( bandit_dry_run::job_template::scout );
        signal.hard_blocked_jobs.push_back( bandit_dry_run::job_template::toll );
    }
    signal.confirmed_threat = false;
    signal.soft_decay = true;
    signal.notes = packet.notes;
    signal.notes.push_back( "light packet: time=" + to_string( packet.time ) +
                            ", weather=" + to_string( packet.weather ) +
                            ", exposure=" + to_string( packet.exposure ) +
                            ", source=" + to_string( packet.source ) +
                            ", side_leakage=" + std::to_string( projection.concealment.side_leakage_bonus ) +
                            ", terrain=" + to_string( packet.terrain ) +
                            ", vertical_offset=" + std::to_string( packet.vertical_offset ) +
                            ", elevation_bonus=" + std::to_string( packet.elevation_bonus ) +
                            ", observed_range_omt=" + std::to_string( packet.observed_range_omt ) +
                            ", projected_range_omt=" + std::to_string( projection.projected_range_omt ) +
                            ", visibility_score=" + std::to_string( projection.visibility_score ) );
    signal.notes.push_back( projection.concealment.summary );
    if( packet.source == light_source_band::searchlight ) {
        signal.notes.push_back( "searchlight stays threat-first: this is a guarded clue, not a free extraction lane" );
    } else {
        signal.notes.push_back( "night light stays bounty-first: this is worth scoping out when exposure supports it, not free loot or identity truth" );
    }

    projection.signal = signal;
    return projection;
}

local_field_signal_projection adapt_local_field_signal_reading(
    const local_field_signal_reading &reading )
{
    local_field_signal_projection result;
    if( reading.fire_intensity <= 0 && reading.smoke_intensity <= 0 ) {
        return result;
    }

    smoke_packet smoke;
    smoke.id = reading.smoke_id;
    smoke.envelope_id = reading.envelope_id;
    smoke.region_id = reading.region_id;
    smoke.observed_range_omt = reading.observed_range_omt;
    smoke.source_strength = std::clamp( reading.fire_intensity + reading.smoke_intensity, 1, 3 );
    smoke.persistence = reading.smoke_intensity > 0 ? 1 : 0;
    smoke.height_bias = reading.fire_intensity >= 2 ? 1 : 0;
    smoke.spread_bias = 0;
    smoke.weather = reading.smoke_weather;
    smoke.notes.push_back( "live source hook: fd_fire=" +
                           std::to_string( reading.fire_intensity ) +
                           ", fd_smoke=" + std::to_string( reading.smoke_intensity ) );
    smoke.notes.push_back( "live source hook: weather=" +
                           to_string( reading.smoke_weather ) );
    smoke.notes.push_back( "local field adapter: fd_fire/fd_smoke -> smoke packet" );
    result.has_smoke_packet = true;
    result.smoke = adapt_smoke_packet( smoke );

    if( reading.fire_intensity <= 0 ) {
        return result;
    }

    light_packet light;
    light.id = reading.light_id;
    light.envelope_id = reading.envelope_id;
    light.region_id = reading.region_id;
    light.observed_range_omt = reading.observed_range_omt;
    light.source_strength = std::clamp( reading.fire_intensity, 1, 3 );
    light.persistence = reading.fire_intensity >= 2 ? 1 : 0;
    const bool exposed_to_sky = reading.outside || reading.elevated_roof_exposed;
    light.side_leakage = exposed_to_sky ? 1 : 0;
    light.time = reading.light_time;
    light.weather = reading.light_weather;
    light.exposure = exposed_to_sky ? light_exposure_band::exposed : light_exposure_band::contained;
    light.source = light_source_band::ordinary;
    light.terrain = exposed_to_sky ? light_terrain_band::open : light_terrain_band::built_cover;
    if( reading.elevated_roof_exposed ) {
        light.vertical_sightline = true;
        light.elevation_bonus = 2;
    }
    light.notes.push_back( "live source hook: fd_fire=" +
                           std::to_string( reading.fire_intensity ) +
                           ", exposure=" + to_string( light.exposure ) +
                           ", elevated_roof_exposed=" +
                           std::string( reading.elevated_roof_exposed ? "yes" : "no" ) );
    light.notes.push_back( "live source hook: time=" + to_string( reading.light_time ) +
                           ", weather=" + to_string( reading.light_weather ) );
    light.notes.push_back( "local field adapter: fd_fire/time/weather -> light packet" );
    result.has_light_packet = true;
    result.light = adapt_light_packet( light );
    return result;
}

int horde_signal_power_from_light_projection( const light_projection &projection )
{
    if( !projection.viable ) {
        return 0;
    }
    const light_packet &packet = projection.packet;
    if( packet.time == light_time_band::daylight ) {
        return 0;
    }
    if( packet.exposure != light_exposure_band::exposed &&
        packet.source != light_source_band::searchlight ) {
        return 0;
    }
    if( projection.projected_range_omt < 8 ) {
        return 0;
    }

    int signal_power = projection.projected_range_omt * 2;
    if( packet.source == light_source_band::searchlight ) {
        signal_power += 4;
    }
    if( projection.concealment.elevated_exposure_extended ) {
        signal_power += 4;
    }
    return std::clamp( signal_power, 8, 60 );
}

human_route_projection adapt_human_route_packet( const human_route_packet &packet )
{
    human_route_projection projection;
    projection.packet = packet;
    projection.projected_family = human_route_projected_family( packet );

    const int origin_bonus = human_route_origin_bonus( packet.origin );
    const int corroboration_bonus = human_route_corroboration_bonus( packet.corroboration );
    projection.visibility_score = std::max( 0, packet.source_strength + packet.persistence +
                                            origin_bonus + corroboration_bonus );
    projection.projected_range_omt = std::clamp( 1 + packet.source_strength * 2 + packet.persistence +
                                     origin_bonus + corroboration_bonus, 0, 12 );

    bool allowed = true;
    if( packet.origin == human_route_origin::same_camp ) {
        allowed = false;
    } else if( packet.kind == human_route_kind::route_activity && !human_route_has_external_footing( packet ) ) {
        allowed = false;
    }

    projection.viable = allowed && projection.visibility_score > 0 &&
                        packet.observed_range_omt <= projection.projected_range_omt;
    if( !projection.viable ) {
        return projection;
    }

    signal_input signal;
    signal.id = packet.id;
    signal.kind = packet.kind == human_route_kind::direct_sighting ? "human_sighting" : "route_activity";
    signal.envelope_id = packet.envelope_id;
    signal.region_id = packet.region_id;
    signal.family = projection.projected_family;
    signal.strength = 1;
    signal.confidence = packet.kind == human_route_kind::direct_sighting ? 2 :
                        1 + ( packet.corroboration != human_route_corroboration::none ? 1 : 0 );
    signal.threat_add = 0;
    signal.bounty_add = packet.kind == human_route_kind::direct_sighting ? 2 :
                        1 + ( projection.projected_family == bandit_dry_run::lead_family::site ? 1 : 0 );
    signal.reward_profile_match = 0;
    signal.distance_multiplier = std::clamp( 1.10 - static_cast<double>( packet.observed_range_omt ) /
                                 static_cast<double>( std::max( 1, projection.projected_range_omt ) ),
                                 0.25, 1.0 );
    signal.threat_gate_result = bandit_dry_run::threat_gate::discount_only;
    if( projection.projected_family == bandit_dry_run::lead_family::site ) {
        signal.hard_blocked_jobs = {
            bandit_dry_run::job_template::scavenge,
            bandit_dry_run::job_template::steal,
            bandit_dry_run::job_template::raid,
        };
    }
    signal.confirmed_threat = false;
    signal.soft_decay = true;
    signal.notes = packet.notes;
    signal.notes.push_back( "human/route packet: kind=" + to_string( packet.kind ) +
                            ", origin=" + to_string( packet.origin ) +
                            ", corroboration=" + to_string( packet.corroboration ) +
                            ", requested_family=" + bandit_dry_run::to_string( packet.family ) +
                            ", projected_family=" + bandit_dry_run::to_string( projection.projected_family ) +
                            ", observed_range_omt=" + std::to_string( packet.observed_range_omt ) +
                            ", projected_range_omt=" + std::to_string( projection.projected_range_omt ) +
                            ", visibility_score=" + std::to_string( projection.visibility_score ) );
    if( packet.kind == human_route_kind::direct_sighting ) {
        signal.notes.push_back( "direct human sighting stays a moving-carrier clue first, not automatic site truth" );
    } else if( projection.projected_family == bandit_dry_run::lead_family::site ) {
        signal.notes.push_back( "corroborated route activity can refresh a bounded site clue, but extraction stays blocked" );
    } else {
        signal.notes.push_back( "repeated route activity only hardens when it plausibly belongs to somebody else, a shared corridor, or corroborated traffic" );
    }

    projection.signal = signal;
    return projection;
}

update_result advance_state( ledger_state &state, int tick, cadence_tier tier,
                             const std::vector<signal_input> &signals )
{
    update_result result;
    const int previous_tick = state.last_tick;
    state.last_tick = tick;

    for( typed_mark &mark : state.marks ) {
        mark.age_turns = std::max( 0, tick - mark.last_refresh_tick );
    }

    for( const signal_input &signal : signals ) {
        result.metrics.events_ingested++;

        const auto it = std::find_if( state.marks.begin(), state.marks.end(), [&signal]( const typed_mark &mark ) {
            return same_mark_key( mark, signal );
        } );

        if( it == state.marks.end() ) {
            typed_mark mark;
            mark.id = effective_mark_id( signal );
            mark.kind = signal.kind;
            mark.envelope_id = effective_envelope_id( signal );
            mark.region_id = effective_region_id( signal );
            mark.family = signal.family;
            mark.strength = clamp_nonnegative( signal.strength );
            mark.confidence = clamp_nonnegative( signal.confidence );
            mark.threat_add = clamp_nonnegative( signal.threat_add );
            mark.bounty_add = clamp_nonnegative( signal.bounty_add );
            mark.monster_pressure_add = clamp_nonnegative( signal.monster_pressure_add );
            mark.target_coherence_subtract = clamp_nonnegative( signal.target_coherence_subtract );
            mark.reward_profile_match = clamp_nonnegative( signal.reward_profile_match );
            mark.distance_multiplier = std::clamp( signal.distance_multiplier, 0.0, 1.0 );
            mark.threat_gate_result = signal.threat_gate_result;
            mark.hard_blocked_jobs = signal.hard_blocked_jobs;
            mark.confirmed_threat = signal.confirmed_threat;
            mark.soft_decay = signal.soft_decay;
            mark.last_refresh_tick = tick;
            mark.age_turns = 0;
            seed_repeated_site_reinforcement( mark, signal );
            seed_moving_memory( mark, signal );
            mark.notes = signal.notes;
            if( mark.notes.empty() ) {
                mark.notes.push_back( "created from " + mark.kind + " signal" );
            }
            state.marks.push_back( mark );
            result.metrics.marks_created++;
            continue;
        }

        typed_mark &mark = *it;
        mark.id = effective_mark_id( signal );
        if( !signal.kind.empty() ) {
            mark.kind = signal.kind;
        }
        mark.region_id = effective_region_id( signal );
        mark.strength = std::min( 6, std::max( mark.strength, clamp_nonnegative( signal.strength ) ) + 1 );
        mark.confidence = std::min( 6, std::max( mark.confidence, clamp_nonnegative( signal.confidence ) ) + 1 );
        mark.threat_add = std::max( mark.threat_add, clamp_nonnegative( signal.threat_add ) );
        mark.bounty_add = std::max( mark.bounty_add, clamp_nonnegative( signal.bounty_add ) );
        mark.monster_pressure_add = std::max( mark.monster_pressure_add,
                                              clamp_nonnegative( signal.monster_pressure_add ) );
        mark.target_coherence_subtract = std::max( mark.target_coherence_subtract,
                                                   clamp_nonnegative( signal.target_coherence_subtract ) );
        mark.reward_profile_match = std::max( mark.reward_profile_match,
                                              clamp_nonnegative( signal.reward_profile_match ) );
        mark.distance_multiplier = std::max( mark.distance_multiplier,
                                             std::clamp( signal.distance_multiplier, 0.0, 1.0 ) );
        mark.threat_gate_result = signal.threat_gate_result;
        mark.hard_blocked_jobs = signal.hard_blocked_jobs;
        mark.confirmed_threat = mark.confirmed_threat || signal.confirmed_threat;
        mark.soft_decay = mark.soft_decay && signal.soft_decay;
        mark.last_refresh_tick = tick;
        mark.age_turns = 0;
        apply_repeated_site_reinforcement( mark, signal );
        refresh_moving_memory( mark, signal );
        if( !signal.notes.empty() ) {
            mark.notes = signal.notes;
        }
        result.metrics.marks_refreshed++;
    }

    for( typed_mark &mark : state.marks ) {
        if( mark.last_refresh_tick == tick ) {
            continue;
        }

        const int old_strength = mark.strength;
        const int elapsed_turns = std::max( 0, tick - previous_tick );
        const int old_confidence = mark.confidence;
        const int old_threat = mark.threat_add;
        const int old_bounty = mark.bounty_add;
        const int old_target_coherence = mark.target_coherence_subtract;

        if( mark.confirmed_threat ) {
            result.metrics.sticky_threat_preserved++;
            mark.bounty_add = clamp_nonnegative( mark.bounty_add - bounty_decay_for( tier ) );
            mark.target_coherence_subtract = clamp_nonnegative( mark.target_coherence_subtract - bounty_decay_for( tier ) );
            if( mark.soft_decay ) {
                mark.confidence = std::max( 1, mark.confidence - confidence_decay_for( tier ) );
                mark.strength = clamp_nonnegative( mark.strength - strength_decay_for( tier ) );
            }
        } else if( mark.soft_decay ) {
            mark.strength = clamp_nonnegative( mark.strength - strength_decay_for( tier ) );
            mark.confidence = clamp_nonnegative( mark.confidence - confidence_decay_for( tier ) );
            mark.bounty_add = clamp_nonnegative( mark.bounty_add - bounty_decay_for( tier ) );
            mark.target_coherence_subtract = clamp_nonnegative( mark.target_coherence_subtract - bounty_decay_for( tier ) );
            mark.threat_add = clamp_nonnegative( mark.threat_add - threat_decay_for_soft_marks( tier ) );
            mark.monster_pressure_add = clamp_nonnegative( mark.monster_pressure_add - threat_decay_for_soft_marks( tier ) );
        }

        if( mark.moving_memory.active && elapsed_turns > 0 ) {
            mark.moving_memory.leash_turns_remaining = std::max( 0,
                                                    mark.moving_memory.leash_turns_remaining - elapsed_turns );
            if( mark.moving_memory.leash_turns_remaining == 0 ) {
                drop_moving_memory( mark, "leash expired without fresh moving contact", tick );
            }
        } else if( !mark.moving_memory.active && mark.moving_memory.review_pending ) {
            mark.moving_memory.review_pending = false;
        }

        if( mark.strength != old_strength || mark.confidence != old_confidence ||
            mark.threat_add != old_threat || mark.bounty_add != old_bounty ||
            mark.target_coherence_subtract != old_target_coherence ) {
            result.metrics.marks_cooled++;
        }
        mark.age_turns = std::max( 0, tick - mark.last_refresh_tick );
    }

    const size_t before_prune = state.marks.size();
    state.marks.erase( std::remove_if( state.marks.begin(), state.marks.end(), []( const typed_mark &mark ) {
        if( mark.confirmed_threat || mark.moving_memory.active || mark.moving_memory.review_pending ) {
            return false;
        }
        return mark.strength == 0 && mark.confidence == 0 && mark.threat_add == 0 &&
               mark.bounty_add == 0 && mark.monster_pressure_add == 0 &&
               mark.target_coherence_subtract == 0;
    } ), state.marks.end() );
    result.metrics.marks_pruned = before_prune - state.marks.size();

    rebuild_heat( state );
    sort_ledger( state );
    result.leads = emit_leads( state, &result.metrics );
    return result;
}

std::vector<bandit_dry_run::lead_input> emit_leads( const ledger_state &state,
        ledger_metrics *metrics )
{
    std::vector<bandit_dry_run::lead_input> leads;
    leads.reserve( state.marks.size() );

    for( const typed_mark &mark : state.marks ) {
        const bool has_active_moving_memory = mark.moving_memory.active &&
                                              is_moving_family( mark.family );
        if( mark.family == bandit_dry_run::lead_family::none ) {
            continue;
        }
        if( mark.strength == 0 && mark.confidence == 0 && mark.threat_add == 0 &&
            mark.bounty_add == 0 && mark.target_coherence_subtract == 0 && !has_active_moving_memory ) {
            continue;
        }

        bandit_dry_run::lead_input lead;
        lead.id = mark.id;
        lead.envelope_id = mark.envelope_id;
        lead.family = mark.family;
        lead.distance_multiplier = has_active_moving_memory ?
                                   std::max( mark.distance_multiplier, 0.35 ) : mark.distance_multiplier;
        lead.lead_bounty_value = mark.strength + mark.bounty_add + mark.target_coherence_subtract;
        lead.lead_confidence_bonus = mark.confidence;
        if( has_active_moving_memory ) {
            lead.lead_bounty_value = std::max( lead.lead_bounty_value, mark.moving_memory.opportunity_band );
            lead.lead_confidence_bonus = std::max( lead.lead_confidence_bonus,
                                                   mark.moving_memory.confidence_band );
        }
        lead.threat_penalty = mark.threat_add + mark.monster_pressure_add;
        lead.monster_pressure_bonus = mark.monster_pressure_add;
        lead.target_coherence_bonus = mark.target_coherence_subtract;
        lead.reward_profile_match = mark.reward_profile_match;
        lead.threat_gate_result = mark.threat_gate_result;
        lead.hard_blocked_jobs = mark.hard_blocked_jobs;
        lead.validity_notes = mark.notes;
        lead.validity_notes.push_back( "generated from " + mark.kind + " mark in " + mark.region_id );
        if( mark.repeated_site_reinforcement.total_site_hits > 0 ) {
            lead.validity_notes.push_back( "repeated-site reinforcement packet: " +
                                           describe_repeated_site_reinforcement( mark.repeated_site_reinforcement ) );
        }
        if( has_active_moving_memory ) {
            lead.validity_notes.push_back( "moving memory: " + describe_moving_memory( mark.moving_memory ) );
        }
        if( mark.monster_pressure_add > 0 || mark.target_coherence_subtract > 0 ) {
            lead.validity_notes.push_back( "pressure/coherence packet: monster_pressure=" +
                                           std::to_string( mark.monster_pressure_add ) +
                                           ", target_coherence=" +
                                           std::to_string( mark.target_coherence_subtract ) );
        }
        if( mark.confirmed_threat ) {
            lead.validity_notes.push_back( "confirmed threat stays sticky until a later real rewrite" );
        }
        if( const heat_cell *heat = find_heat( state, mark.region_id ) ) {
            lead.active_pressure_penalty = std::max( 0, heat->threat_heat - lead.threat_penalty );
            if( heat->bounty_heat >= lead.lead_bounty_value + 2 ) {
                lead.lead_confidence_bonus += 1;
            }
            lead.validity_notes.push_back( "regional heat threat=" + std::to_string( heat->threat_heat ) +
                                           ", bounty=" + std::to_string( heat->bounty_heat ) );
        }
        leads.push_back( lead );
    }

    std::sort( leads.begin(), leads.end(), []( const bandit_dry_run::lead_input &lhs,
    const bandit_dry_run::lead_input &rhs ) {
        if( lhs.envelope_id != rhs.envelope_id ) {
            return lhs.envelope_id < rhs.envelope_id;
        }
        return lhs.id < rhs.id;
    } );

    if( metrics != nullptr ) {
        metrics->leads_emitted = leads.size();
    }
    return leads;
}

std::string render_report( const ledger_state &state,
                           const std::vector<bandit_dry_run::lead_input> *leads )
{
    const std::vector<bandit_dry_run::lead_input> local_leads = leads == nullptr ? emit_leads( state ) :
            *leads;

    std::ostringstream out;
    out << "bandit mark-generation report\n";
    out << "last_tick=" << state.last_tick << "\n";
    out << "heat cells:\n";
    if( state.heat.empty() ) {
        out << "- none\n";
    } else {
        for( const heat_cell &cell : state.heat ) {
            out << "- " << cell.region_id << " threat_heat=" << cell.threat_heat
                << ", bounty_heat=" << cell.bounty_heat
                << ", mark_count=" << cell.mark_count
                << ", confirmed_threat_marks=" << cell.confirmed_threat_marks << "\n";
        }
    }

    out << "typed marks:\n";
    if( state.marks.empty() ) {
        out << "- none\n";
    } else {
        for( const typed_mark &mark : state.marks ) {
            out << "- " << mark.envelope_id << " [kind=" << mark.kind
                << ", family=" << bandit_dry_run::to_string( mark.family )
                << ", region=" << mark.region_id
                << ", strength=" << mark.strength
                << ", confidence=" << mark.confidence
                << ", threat_add=" << mark.threat_add
                << ", bounty_add=" << mark.bounty_add
                << ", monster_pressure_add=" << mark.monster_pressure_add
                << ", target_coherence_subtract=" << mark.target_coherence_subtract
                << ", confirmed_threat=" << ( mark.confirmed_threat ? "yes" : "no" )
                << ", soft_decay=" << ( mark.soft_decay ? "yes" : "no" )
                << ", age_turns=" << mark.age_turns
                << ", last_refresh_tick=" << mark.last_refresh_tick
                << ", repeated_site_reinforcement="
                << describe_repeated_site_reinforcement( mark.repeated_site_reinforcement )
                << ", moving_memory=" << describe_moving_memory( mark.moving_memory ) << "]\n";
            out << "  notes=" << join_notes( mark.notes ) << "\n";
        }
    }

    out << "generated leads:\n";
    if( local_leads.empty() ) {
        out << "- none\n";
    } else {
        for( const bandit_dry_run::lead_input &lead : local_leads ) {
            out << "- " << lead.envelope_id
                << " [family=" << bandit_dry_run::to_string( lead.family )
                << ", bounty=" << lead.lead_bounty_value
                << ", confidence=" << lead.lead_confidence_bonus
                << ", threat=" << lead.threat_penalty
                << ", active_pressure_penalty=" << lead.active_pressure_penalty
                << ", distance_multiplier=" << std::fixed << std::setprecision( 2 )
                << lead.distance_multiplier
                << ", threat_gate=" << bandit_dry_run::to_string( lead.threat_gate_result ) << "]\n";
            out << "  notes=" << join_notes( lead.validity_notes ) << "\n";
        }
    }

    return out.str();
}
} // namespace bandit_mark_generation
