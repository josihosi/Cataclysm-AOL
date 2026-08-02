#include "bandit_live_world.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "bandit_live_world_probe.h"
#include "json.h"

namespace
{
using bandit_live_world::anchor_source_kind;
using bandit_live_world::camp_decision_state;
using bandit_live_world::camp_lead_kind;
using bandit_live_world::camp_lead_status;
using bandit_live_world::camp_map_lead;
using bandit_live_world::hostile_site_profile;
using bandit_live_world::hostile_operation_kind;
using bandit_live_world::hostile_operation_phase;
using bandit_live_world::member_state;
using bandit_live_world::owned_site_kind;
using bandit_live_world::outing_kind;
using bandit_live_world::scout_phase;
using bandit_live_world::simulation_owner;

constexpr std::size_t max_active_outing_members = 16;
constexpr std::size_t max_active_outing_route_steps = 256;
constexpr std::size_t max_hostile_operation_members = 6;
constexpr std::size_t max_active_outing_observations = 16;
constexpr std::size_t max_active_outing_casualties = 16;
constexpr std::size_t max_sortie_fact_key_length = 128;
constexpr std::size_t max_sortie_summary_length = 512;
constexpr std::size_t max_camp_decision_reason_length = 256;
constexpr int max_finite_resource_units = 3;
constexpr int max_finite_resource_claim_units = 2;
constexpr int camp_supply_days_at_capacity = 14;
constexpr int legacy_camp_supply_seed_days = 7;
constexpr int max_camp_supply_units = 256;
constexpr int minutes_per_member_day = 24 * 60;
constexpr int scout_return_cohesion_minutes = 2 * 60;
constexpr int scout_missing_grace_minutes = 24 * 60;

int minutes_after_saturated( const int base_minutes, const int delta_minutes )
{
    if( base_minutes < 0 || delta_minutes < 0 ) {
        return -1;
    }
    const long long result = static_cast<long long>( base_minutes ) + delta_minutes;
    return static_cast<int>( std::min<long long>( result, std::numeric_limits<int>::max() ) );
}

bool finite_resource_record_is_valid( const bandit_live_world::finite_resource_record &record )
{
    return record.remaining_units >= 0 && record.remaining_units < max_finite_resource_units &&
           record.revision > 0 && record.revision <= max_finite_resource_units &&
           record.remaining_units + record.revision <= max_finite_resource_units;
}

void seed_camp_supply( bandit_live_world::site_record &site )
{
    const int living_total = bandit_live_world::camp_supply_living_total( site );
    const long long seeded_units = static_cast<long long>( legacy_camp_supply_seed_days ) *
                                   living_total;
    site.supply_units = static_cast<int>( std::min<long long>(
                            bandit_live_world::camp_supply_cap( site ), seeded_units ) );
    site.supply_last_update_minutes = -1;
    site.supply_accounted_living_total = living_total;
    site.supply_member_minute_remainder = 0;
}

void seed_uninitialized_camp_supply( bandit_live_world::site_record &site )
{
    if( site.supply_last_update_minutes != -1 ||
        site.supply_member_minute_remainder != 0 ) {
        return;
    }
    const int prior_living_total = std::max( 0, site.supply_accounted_living_total );
    const int prior_cap = static_cast<int>( std::min<long long>( max_camp_supply_units,
                          static_cast<long long>( camp_supply_days_at_capacity ) *
                          std::max( 1, prior_living_total ) ) );
    const int prior_seed = static_cast<int>( std::min<long long>( prior_cap,
                           static_cast<long long>( legacy_camp_supply_seed_days ) *
                           prior_living_total ) );
    if( site.supply_units == prior_seed ) {
        seed_camp_supply( site );
    }
}

struct bounded_route_state {
    std::vector<tripoint_abs_omt> route;
    int waypoint_index = 0;
};

bounded_route_state make_bounded_route_state( const std::vector<tripoint_abs_omt> &route,
        const int waypoint_index )
{
    bounded_route_state result;
    const std::size_t clamped_waypoint = static_cast<std::size_t>( std::clamp( waypoint_index, 0,
                                           static_cast<int>( route.size() ) ) );
    if( route.size() <= max_active_outing_route_steps ) {
        result.route = route;
        result.waypoint_index = static_cast<int>( clamped_waypoint );
        return result;
    }

    if( clamped_waypoint >= route.size() ) {
        const auto first = route.end() - max_active_outing_route_steps;
        result.route.assign( first, route.end() );
        result.waypoint_index = static_cast<int>( result.route.size() );
        return result;
    }

    const auto first = route.begin() + clamped_waypoint;
    const auto last = first + std::min( max_active_outing_route_steps,
                                       route.size() - clamped_waypoint );
    result.route.assign( first, last );
    if( result.route.size() == max_active_outing_route_steps &&
        result.route.back() != route.back() ) {
        result.route.back() = route.back();
    }
    result.waypoint_index = 0;
    return result;
}

std::vector<bandit_live_world::sortie_observation> make_bounded_sortie_observations(
            const std::vector<bandit_live_world::sortie_observation> &observations )
{
    if( observations.size() <= max_active_outing_observations ) {
        return observations;
    }

    std::vector<std::size_t> retained_indices;
    retained_indices.reserve( max_active_outing_observations );
    for( std::size_t index = observations.size(); index > 0 &&
         retained_indices.size() < max_active_outing_observations; --index ) {
        if( observations[index - 1].critical ) {
            retained_indices.push_back( index - 1 );
        }
    }
    for( std::size_t index = observations.size(); index > 0 &&
         retained_indices.size() < max_active_outing_observations; --index ) {
        if( !observations[index - 1].critical ) {
            retained_indices.push_back( index - 1 );
        }
    }
    std::sort( retained_indices.begin(), retained_indices.end() );

    std::vector<bandit_live_world::sortie_observation> result;
    result.reserve( retained_indices.size() );
    for( const std::size_t index : retained_indices ) {
        result.push_back( observations[index] );
    }
    return result;
}

std::optional<anchor_source_kind> anchor_source_kind_from_string( const std::string &value )
{
    if( value == "overmap_special" ) {
        return anchor_source_kind::overmap_special;
    }
    if( value == "map_extra" ) {
        return anchor_source_kind::map_extra;
    }
    if( value == "none" ) {
        return anchor_source_kind::none;
    }
    return std::nullopt;
}

std::optional<owned_site_kind> owned_site_kind_from_string( const std::string &value )
{
    if( value == "bandit_camp" ) {
        return owned_site_kind::bandit_camp;
    }
    if( value == "bandit_work_camp" ) {
        return owned_site_kind::bandit_work_camp;
    }
    if( value == "bandit_cabin" ) {
        return owned_site_kind::bandit_cabin;
    }
    if( value == "cannibal_camp" ) {
        return owned_site_kind::cannibal_camp;
    }
    if( value == "looters" ) {
        return owned_site_kind::looters;
    }
    if( value == "bandits_block" ) {
        return owned_site_kind::bandits_block;
    }
    if( value == "none" ) {
        return owned_site_kind::none;
    }
    return std::nullopt;
}

std::optional<hostile_site_profile> hostile_site_profile_from_string( const std::string &value )
{
    if( value == "camp_style" ) {
        return hostile_site_profile::camp_style;
    }
    if( value == "cannibal_camp" ) {
        return hostile_site_profile::cannibal_camp;
    }
    if( value == "small_hostile_site" ) {
        return hostile_site_profile::small_hostile_site;
    }
    if( value == "none" ) {
        return hostile_site_profile::none;
    }
    return std::nullopt;
}

std::optional<member_state> member_state_from_string( const std::string &value )
{
    if( value == "at_home" ) {
        return member_state::at_home;
    }
    if( value == "outbound" ) {
        return member_state::outbound;
    }
    if( value == "local_contact" ) {
        return member_state::local_contact;
    }
    if( value == "dead" ) {
        return member_state::dead;
    }
    if( value == "missing" ) {
        return member_state::missing;
    }
    return std::nullopt;
}

std::optional<camp_lead_kind> camp_lead_kind_from_string( const std::string &value )
{
    if( value == "structural_bounty" ) {
        return camp_lead_kind::structural_bounty;
    }
    if( value == "harvested_site" ) {
        return camp_lead_kind::harvested_site;
    }
    if( value == "human_activity" ) {
        return camp_lead_kind::human_activity;
    }
    if( value == "basecamp_activity" ) {
        return camp_lead_kind::basecamp_activity;
    }
    if( value == "moving_actor" ) {
        return camp_lead_kind::moving_actor;
    }
    if( value == "route_activity" ) {
        return camp_lead_kind::route_activity;
    }
    if( value == "smoke_signal" ) {
        return camp_lead_kind::smoke_signal;
    }
    if( value == "light_signal" ) {
        return camp_lead_kind::light_signal;
    }
    if( value == "sound_signal" ) {
        return camp_lead_kind::sound_signal;
    }
    if( value == "threat_memory" ) {
        return camp_lead_kind::threat_memory;
    }
    if( value == "loss_site" ) {
        return camp_lead_kind::loss_site;
    }
    if( value == "false_lead" ) {
        return camp_lead_kind::false_lead;
    }
    if( value == "frontier_probe" ) {
        return camp_lead_kind::frontier_probe;
    }
    return std::nullopt;
}

std::optional<camp_lead_status> camp_lead_status_from_string( const std::string &value )
{
    if( value == "suspected" ) {
        return camp_lead_status::suspected;
    }
    if( value == "scout_confirmed" ) {
        return camp_lead_status::scout_confirmed;
    }
    if( value == "active" ) {
        return camp_lead_status::active;
    }
    if( value == "harvested" ) {
        return camp_lead_status::harvested;
    }
    if( value == "stale" ) {
        return camp_lead_status::stale;
    }
    if( value == "invalidated" ) {
        return camp_lead_status::invalidated;
    }
    if( value == "dangerous" ) {
        return camp_lead_status::dangerous;
    }
    return std::nullopt;
}

std::optional<outing_kind> outing_kind_from_string( const std::string &value )
{
    if( value == "scout_sortie" ) {
        return outing_kind::scout_sortie;
    }
    if( value == "hostile_operation" ) {
        return outing_kind::hostile_operation;
    }
    if( value == "structural_sortie" ) {
        return outing_kind::structural_sortie;
    }
    if( value == "none" ) {
        return outing_kind::none;
    }
    return std::nullopt;
}

std::optional<simulation_owner> simulation_owner_from_string( const std::string &value )
{
    if( value == "abstract" ) {
        return simulation_owner::abstract;
    }
    if( value == "local" ) {
        return simulation_owner::local;
    }
    return std::nullopt;
}

std::optional<scout_phase> scout_phase_from_string( const std::string &value )
{
    if( value == "assembling" ) {
        return scout_phase::assembling;
    }
    if( value == "outbound" ) {
        return scout_phase::outbound;
    }
    if( value == "searching" ) {
        return scout_phase::searching;
    }
    if( value == "observing" ) {
        return scout_phase::observing;
    }
    if( value == "harvesting" ) {
        return scout_phase::harvesting;
    }
    if( value == "burned_withdrawal" ) {
        return scout_phase::burned_withdrawal;
    }
    if( value == "returning_exposed" ) {
        return scout_phase::returning_exposed;
    }
    if( value == "returning_report" ) {
        return scout_phase::returning_report;
    }
    if( value == "returning_home" ) {
        return scout_phase::returning_home;
    }
    if( value == "lost" ) {
        return scout_phase::lost;
    }
    return std::nullopt;
}

std::optional<camp_decision_state> camp_decision_state_from_string( const std::string &value )
{
    if( value == "idle" ) {
        return camp_decision_state::idle;
    }
    if( value == "report_awaiting_assessment" ) {
        return camp_decision_state::report_awaiting_assessment;
    }
    if( value == "preparing_follow_on" ) {
        return camp_decision_state::preparing_follow_on;
    }
    if( value == "cooldown" ) {
        return camp_decision_state::cooldown;
    }
    if( value == "abandoned" ) {
        return camp_decision_state::abandoned;
    }
    return std::nullopt;
}

std::optional<hostile_operation_kind> hostile_operation_kind_from_string(
    const std::string &value )
{
    if( value == "shakedown" ) {
        return hostile_operation_kind::shakedown;
    }
    if( value == "raid" ) {
        return hostile_operation_kind::raid;
    }
    if( value == "none" ) {
        return hostile_operation_kind::none;
    }
    return std::nullopt;
}

std::optional<hostile_operation_phase> hostile_operation_phase_from_string(
    const std::string &value )
{
    if( value == "assembling" ) {
        return hostile_operation_phase::assembling;
    }
    if( value == "outbound" ) {
        return hostile_operation_phase::outbound;
    }
    if( value == "rallying" ) {
        return hostile_operation_phase::rallying;
    }
    if( value == "waiting_night" ) {
        return hostile_operation_phase::waiting_night;
    }
    if( value == "approaching" ) {
        return hostile_operation_phase::approaching;
    }
    if( value == "committed_contact" ) {
        return hostile_operation_phase::committed_contact;
    }
    if( value == "returning_home" ) {
        return hostile_operation_phase::returning_home;
    }
    if( value == "lost" ) {
        return hostile_operation_phase::lost;
    }
    return std::nullopt;
}

outing_kind classify_legacy_outing_kind( const std::string &activity_id,
        const std::string &job_type )
{
    if( activity_id.find( "#structural" ) != std::string::npos ) {
        return outing_kind::structural_sortie;
    }
    if( job_type == "scout" || job_type == "scavenge" ) {
        return outing_kind::scout_sortie;
    }
    return outing_kind::hostile_operation;
}

std::optional<bandit_pursuit_handoff::remaining_return_pressure_state>
remaining_return_pressure_state_from_string( const std::string &value )
{
    using bandit_pursuit_handoff::remaining_return_pressure_state;
    if( value == "ample" ) {
        return remaining_return_pressure_state::ample;
    }
    if( value == "tight" ) {
        return remaining_return_pressure_state::tight;
    }
    if( value == "plain_return_only" ) {
        return remaining_return_pressure_state::plain_return_only;
    }
    if( value == "collapsed" ) {
        return remaining_return_pressure_state::collapsed;
    }
    return std::nullopt;
}

std::optional<bandit_dry_run::job_template> job_template_from_string( const std::string &value )
{
    using bandit_dry_run::job_template;
    if( value == "scout" ) {
        return job_template::scout;
    }
    if( value == "scavenge" ) {
        return job_template::scavenge;
    }
    if( value == "toll" ) {
        return job_template::toll;
    }
    if( value == "stalk" ) {
        return job_template::stalk;
    }
    if( value == "steal" ) {
        return job_template::steal;
    }
    if( value == "raid" ) {
        return job_template::raid;
    }
    if( value == "reinforce" ) {
        return job_template::reinforce;
    }
    if( value == "hold / chill" || value == "hold_chill" || value.empty() ) {
        return job_template::hold_chill;
    }
    return std::nullopt;
}

void push_unique_mark( std::vector<std::string> &marks, const std::string &mark )
{
    if( mark.empty() ) {
        return;
    }
    if( std::find( marks.begin(), marks.end(), mark ) == marks.end() ) {
        marks.push_back( mark );
    }
}

std::string camp_lead_id_for( const std::string &site_id, const camp_lead_kind kind,
                              const std::string &target_id, const tripoint_abs_omt &omt )
{
    std::ostringstream out;
    out << site_id << "#lead:" << bandit_live_world::to_string( kind ) << ':'
        << target_id << '@' << omt.x() << ',' << omt.y() << ',' << omt.z();
    return out.str();
}

std::string lowercase_copy( const std::string &value )
{
    std::string lowered = value;
    std::transform( lowered.begin(), lowered.end(), lowered.begin(), []( const unsigned char ch ) {
        return static_cast<char>( std::tolower( ch ) );
    } );
    return lowered;
}

bool contains_any_token( const std::string &haystack, const std::vector<std::string> &needles )
{
    for( const std::string &needle : needles ) {
        if( haystack.find( needle ) != std::string::npos ) {
            return true;
        }
    }
    return false;
}

camp_lead_kind signal_kind_to_camp_lead_kind( const std::string &kind )
{
    if( kind == "smoke" ) {
        return camp_lead_kind::smoke_signal;
    }
    if( kind == "light" ) {
        return camp_lead_kind::light_signal;
    }
    if( kind == "sound" ) {
        return camp_lead_kind::sound_signal;
    }
    return camp_lead_kind::human_activity;
}

bandit_dry_run::lead_family family_for_camp_map_lead( const camp_map_lead &lead )
{
    switch( lead.kind ) {
        case camp_lead_kind::moving_actor:
            return bandit_dry_run::lead_family::moving_carrier;
        case camp_lead_kind::route_activity:
        case camp_lead_kind::smoke_signal:
        case camp_lead_kind::light_signal:
        case camp_lead_kind::sound_signal:
            return bandit_dry_run::lead_family::corridor;
        case camp_lead_kind::structural_bounty:
        case camp_lead_kind::harvested_site:
        case camp_lead_kind::human_activity:
        case camp_lead_kind::basecamp_activity:
        case camp_lead_kind::threat_memory:
        case camp_lead_kind::loss_site:
        case camp_lead_kind::false_lead:
        case camp_lead_kind::frontier_probe:
            return bandit_dry_run::lead_family::site;
    }

    return bandit_dry_run::lead_family::site;
}

bandit_dry_run::candidate_debug make_camp_map_dispatch_candidate( const camp_map_lead &lead,
        const bandit_live_world::camp_map_dispatch_decision &decision )
{
    bandit_dry_run::candidate_debug candidate;
    candidate.job = decision.intent;
    candidate.lead_id = lead.lead_id.empty() ? lead.target_id : lead.lead_id;
    candidate.envelope_id = lead.target_id.empty() ? candidate.lead_id : lead.target_id;
    candidate.family = family_for_camp_map_lead( lead );
    candidate.generated = true;
    candidate.valid = decision.intent == bandit_dry_run::job_template::scout ||
                      decision.intent == bandit_dry_run::job_template::stalk ||
                      decision.intent == bandit_dry_run::job_template::toll ||
                      decision.intent == bandit_dry_run::job_template::raid;
    candidate.winner = candidate.valid;
    candidate.score.lead_bounty_value = lead.bounty;
    candidate.score.lead_confidence_bonus = lead.confidence;
    candidate.score.threat_penalty = lead.threat;
    candidate.score.threat_gate_result = lead.threat_confirmed ?
                                         bandit_dry_run::threat_gate::soft_veto :
                                         bandit_dry_run::threat_gate::discount_only;
    candidate.score.reward_profile_match = decision.reward_score;
    candidate.score.effective_threat_penalty = decision.risk_score;
    candidate.score.final_job_score = decision.margin;
    candidate.notes = decision.notes;
    candidate.notes.push_back( "camp-map remembered lead " + candidate.lead_id +
                               " selected " + bandit_dry_run::to_string( decision.intent ) +
                               " reward=" + std::to_string( decision.reward_score ) +
                               " risk=" + std::to_string( decision.risk_score ) +
                               " margin=" + std::to_string( decision.margin ) );
    return candidate;
}

void upsert_camp_map_lead( bandit_live_world::camp_intelligence_map &intelligence_map,
                           const camp_map_lead &lead )
{
    if( lead.lead_id.empty() ) {
        return;
    }
    if( camp_map_lead *existing = intelligence_map.find_lead( lead.lead_id ) ) {
        *existing = lead;
        return;
    }
    intelligence_map.leads.push_back( lead );
}

void migrate_scalar_memory_to_intelligence_map( bandit_live_world::site_record &site,
        const bool intelligence_map_was_present )
{
    if( intelligence_map_was_present || !site.intelligence_map.leads.empty() ||
        ( site.remembered_target_or_mark.empty() && site.remembered_bounty_estimate <= 0 &&
          site.remembered_threat_estimate <= 0 ) ) {
        return;
    }

    camp_map_lead lead;
    lead.kind = camp_lead_kind::human_activity;
    lead.status = camp_lead_status::suspected;
    lead.target_id = site.remembered_target_or_mark.empty() ? site.active_outing.target_id :
                     site.remembered_target_or_mark;
    lead.omt = site.active_outing.target_omt;
    lead.source_key = lead.target_id;
    lead.source_summary = "migrated from legacy remembered_* site memory";
    lead.bounty = std::max( 0, site.remembered_bounty_estimate );
    lead.threat = std::max( 0, site.remembered_threat_estimate );
    lead.confidence = lead.target_id.empty() ? 1 : 2;
    lead.threat_confirmed = lead.threat > 0;
    lead.last_outcome = "legacy_memory";
    lead.lead_id = camp_lead_id_for( site.site_id, lead.kind, lead.target_id, lead.omt );
    upsert_camp_map_lead( site.intelligence_map, lead );
}

void record_scout_return_lead( bandit_live_world::site_record &site,
                               const bandit_pursuit_handoff::return_packet &packet,
                               const int bandit_losses )
{
    if( packet.job_type != bandit_dry_run::job_template::scout ||
        packet.current_target_or_mark.empty() || packet.survivors_remaining <= 0 ) {
        return;
    }

    camp_map_lead lead;
    lead.kind = camp_lead_kind::basecamp_activity;
    lead.status = camp_lead_status::scout_confirmed;
    lead.target_id = packet.current_target_or_mark;
    lead.omt = site.active_outing.target_omt;
    lead.source_key = packet.group_id;
    lead.source_summary = "scout-return writeback from active owned outing";
    lead.last_scouted_minutes = site.active_outing.started_minutes;
    lead.last_checked_minutes = site.active_outing.local_contact_minutes;
    lead.bounty = std::max( 1, site.remembered_bounty_estimate );
    lead.threat = std::max( 0, site.remembered_threat_estimate );
    lead.confidence = std::max( 2, packet.survivors_remaining + 1 );
    lead.threat_confirmed = lead.threat > 0 || packet.resolution == bandit_pursuit_handoff::lead_resolution::still_valid;
    lead.target_alert = packet.resolution == bandit_pursuit_handoff::lead_resolution::target_lost ||
                        packet.posture == bandit_pursuit_handoff::return_posture::broken_flee;
    lead.scout_seen = lead.target_alert;
    lead.prior_bandit_losses = bandit_losses;
    lead.last_outcome = bandit_pursuit_handoff::to_string( packet.result );
    lead.lead_id = camp_lead_id_for( site.site_id, lead.kind, lead.target_id, lead.omt );
    upsert_camp_map_lead( site.intelligence_map, lead );
}

bandit_pursuit_handoff::abstract_group_state make_site_memory_group(
    const bandit_live_world::site_record &site )
{
    const bandit_live_world::active_outing_state *outing = site.active_external_outing();
    bandit_pursuit_handoff::abstract_group_state group;
    group.group_id = outing == nullptr ? site.site_id + "#dispatch" : outing->activity_id;
    group.source_camp_id = site.site_id;
    group.activity_generation = outing == nullptr ? site.next_outing_generation :
                                outing->generation;
    group.handoff_epoch = outing == nullptr ? 0 : outing->handoff_epoch;
    group.return_application_key = outing == nullptr ?
                                   group.group_id + ":return:" +
                                   std::to_string( group.activity_generation ) :
                                   outing->return_application_key;
    group.group_strength = site.count_live_members();
    group.current_target_or_mark = site.remembered_target_or_mark.empty() && outing != nullptr ?
                                   outing->target_id : site.remembered_target_or_mark;
    group.current_threat_estimate = site.remembered_threat_estimate;
    group.current_bounty_estimate = site.remembered_bounty_estimate;
    group.retreat_bias = site.remembered_retreat_bias;
    group.return_clock = site.remembered_return_clock;
    group.remaining_pressure = site.remembered_pressure;
    group.known_recent_marks = site.known_recent_marks;
    if( outing != nullptr ) {
        for( const character_id &member_id : outing->member_ids ) {
            group.anchored_identities.push_back( { std::to_string( member_id.get_value() ), "alive" } );
        }
    }
    return group;
}

std::string provisional_report_application_key( const bandit_live_world::site_record &site )
{
    std::string key = site.active_outing.report_application_key + ":members";
    for( const character_id &member_id : site.active_outing.resolved_member_ids ) {
        const bandit_live_world::member_record *member = site.find_member( member_id );
        if( member != nullptr && member->state == bandit_live_world::member_state::at_home ) {
            key += ":" + std::to_string( member_id.get_value() );
        }
    }
    return key;
}

void apply_group_memory( bandit_live_world::site_record &site,
                         const bandit_pursuit_handoff::abstract_group_state &group )
{
    site.remembered_target_or_mark = group.current_target_or_mark;
    site.remembered_threat_estimate = group.current_threat_estimate;
    site.remembered_bounty_estimate = group.current_bounty_estimate;
    site.remembered_retreat_bias = group.retreat_bias;
    site.remembered_return_clock = group.return_clock;
    site.remembered_pressure = group.remaining_pressure;
    site.known_recent_marks = group.known_recent_marks;
}

int shakedown_demand_modifier_percent( const bandit_live_world::site_record &site )
{
    if( site.shakedown_reopen_available && !site.shakedown_reopen_used ) {
        return 140;
    }
    if( site.shakedown_caution > 0 || site.shakedown_bandit_losses > 0 ) {
        return std::max( 50, 100 - 25 * std::max( site.shakedown_caution, site.shakedown_bandit_losses ) );
    }
    return 100;
}

std::string shakedown_outcome_label( const bandit_live_world::shakedown_outcome &outcome )
{
    if( outcome.paid ) {
        return "paid";
    }
    if( outcome.fought && outcome.bandit_losses > 0 ) {
        return "fight_bandit_loss";
    }
    if( outcome.fought && outcome.defender_losses > 0 ) {
        return "fight_defender_loss";
    }
    if( outcome.fought ) {
        return "fight_unresolved";
    }
    return "unknown";
}

struct shakedown_opening_beat {
    std::string id;
    std::string summary;
    std::string bark;
};

shakedown_opening_beat choose_shakedown_opening_beat( const bandit_live_world::site_record &site,
        const bandit_live_world::local_gate_input &input,
        const bandit_live_world::local_gate_decision &decision )
{
    if( site.shakedown_reopen_available && !site.shakedown_reopen_used ) {
        return { "reopened_demand",
                 "seen-you-before reopened demand after prior bloodshed",
                 "Last time you made this expensive.  Now you pay the higher cut, or we finish it." };
    }
    if( input.basecamp_or_camp_scene ) {
        return { "basecamp_pressure",
                 "basecamp leverage against supplies and workers",
                 "Nice camp.  Lots of hands, lots of supplies.  Pay our share and nobody has to count bodies." };
    }
    if( input.darkness_or_concealment || input.standoff_distance >= 2 ) {
        return { "warning_from_cover",
                 "bandits call from cover before closing the fork",
                 "You hear us before you see all of us.  Put the goods down and walk away breathing." };
    }
    if( input.local_threat <= 1 && decision.pressure_margin >= 3 ) {
        return { "weakness_read",
                 "bandits read the player's weak odds before demanding payment",
                 "You look light on friends and heavy on things worth taking.  Make this easy." };
    }
    return { "roadblock_toll",
             "roadblock toll demand",
             "Road's taxed now.  Pay the toll or fight for the privilege." };
}

int special_footprint_radius( const std::string &special_id )
{
    if( special_id == "bandit_camp" || special_id == "bandit_work_camp" ||
        special_id == "cannibal_camp" ) {
        return 1;
    }
    return 0;
}

std::vector<int> special_footprint_z_levels( const tripoint_abs_omt &origin )
{
    std::vector<int> z_levels;
    z_levels.reserve( 9 );
    for( int z = -2; z <= 5; ++z ) {
        z_levels.push_back( z );
    }
    if( std::find( z_levels.begin(), z_levels.end(), origin.z() ) == z_levels.end() ) {
        z_levels.push_back( origin.z() );
        std::sort( z_levels.begin(), z_levels.end() );
    }
    return z_levels;
}

bool omt_less( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs )
{
    if( lhs.z() != rhs.z() ) {
        return lhs.z() < rhs.z();
    }
    if( lhs.y() != rhs.y() ) {
        return lhs.y() < rhs.y();
    }
    return lhs.x() < rhs.x();
}

int required_dispatch_members( bandit_dry_run::job_template job )
{
    switch( job ) {
        case bandit_dry_run::job_template::hold_chill:
            return 0;
        case bandit_dry_run::job_template::scout:
        case bandit_dry_run::job_template::scavenge:
        case bandit_dry_run::job_template::stalk:
        case bandit_dry_run::job_template::steal:
            return 1;
        case bandit_dry_run::job_template::toll:
        case bandit_dry_run::job_template::raid:
        case bandit_dry_run::job_template::reinforce:
            return 2;
    }

    return 0;
}

bool counts_toward_live_headcount( member_state state )
{
    return state == member_state::at_home || state == member_state::outbound ||
           state == member_state::local_contact;
}

struct hostile_site_profile_rules {
    hostile_site_profile profile = hostile_site_profile::none;
    std::string id;
    int home_reserve = 0;
    int scout_job_bonus = 0;
    int threat_penalty = 1;
    int retreat_bias_floor = 1;
    int return_clock_floor = 1;
    bandit_pursuit_handoff::remaining_return_pressure_state default_remaining_pressure =
        bandit_pursuit_handoff::remaining_return_pressure_state::ample;
    std::string writeback_expectation;
};

hostile_site_profile effective_profile( const bandit_live_world::site_record &site )
{
    return site.profile == hostile_site_profile::none ?
           bandit_live_world::profile_for_site_kind( site.site_kind ) : site.profile;
}

hostile_site_profile_rules rules_for_profile( hostile_site_profile profile )
{
    switch( profile ) {
        case hostile_site_profile::camp_style:
            return { profile, "camp_style", 1, 1, 1, 1, 2,
                bandit_pursuit_handoff::remaining_return_pressure_state::ample,
                "checks the shared 30-minute cadence, keeps a home reserve, and writes back as persistent camp pressure" };
        case hostile_site_profile::cannibal_camp:
            return { profile, "cannibal_camp", 2, 2, 0, 3, 3,
                bandit_pursuit_handoff::remaining_return_pressure_state::tight,
                "checks the shared 30-minute cadence, keeps a larger home larder guard, and writes back as hungry camp pressure" };
        case hostile_site_profile::small_hostile_site:
            return { profile, "small_hostile_site", 0, 1, 0, 2, 1,
                bandit_pursuit_handoff::remaining_return_pressure_state::tight,
                "can commit its whole small roster and writes back as brittle local pressure" };
        case hostile_site_profile::none:
            return { profile, "none", 0, 0, 1, 1, 1,
                bandit_pursuit_handoff::remaining_return_pressure_state::ample,
                "falls back to minimal hostile-site defaults" };
    }

    return { hostile_site_profile::none, "none", 0, 0, 1, 1, 1,
        bandit_pursuit_handoff::remaining_return_pressure_state::ample,
        "falls back to minimal hostile-site defaults" };
}

int required_home_reserve( const bandit_live_world::site_record &site )
{
    const hostile_site_profile profile = effective_profile( site );
    if( profile != hostile_site_profile::camp_style ) {
        return rules_for_profile( profile ).home_reserve;
    }

    const int living_roster = site.count_live_members();
    if( living_roster <= 1 ) {
        return living_roster;
    }
    if( living_roster == 2 ) {
        return 1;
    }
    if( living_roster <= 4 ) {
        return 1;
    }
    if( living_roster <= 7 ) {
        return 2;
    }
    return std::max( 3, ( living_roster * 35 + 99 ) / 100 );
}

bool hostile_operation_party_preserves_home( const bandit_live_world::site_record &site,
        const std::size_t party_size )
{
    const int ready_at_home = static_cast<int>( std::count_if( site.members.begin(),
                              site.members.end(), []( const bandit_live_world::member_record & member ) {
        return member.state == member_state::at_home && !member.wounded_or_unready;
    } ) );
    const int remaining_ready = ready_at_home - static_cast<int>( party_size );
    return remaining_ready >= required_home_reserve( site );
}

bool cannibal_job_requires_attack_pack( bandit_dry_run::job_template job )
{
    switch( job ) {
        case bandit_dry_run::job_template::toll:
        case bandit_dry_run::job_template::stalk:
        case bandit_dry_run::job_template::steal:
        case bandit_dry_run::job_template::raid:
        case bandit_dry_run::job_template::reinforce:
            return true;
        case bandit_dry_run::job_template::hold_chill:
        case bandit_dry_run::job_template::scout:
        case bandit_dry_run::job_template::scavenge:
            return false;
    }

    return false;
}

int required_dispatch_members_for_profile( const bandit_live_world::site_record &site,
        bandit_dry_run::job_template job )
{
    const int generic_required = required_dispatch_members( job );
    if( generic_required <= 0 ) {
        return generic_required;
    }

    if( effective_profile( site ) != hostile_site_profile::cannibal_camp ||
        !cannibal_job_requires_attack_pack( job ) ) {
        return generic_required;
    }

    // Cannibal attack pressure is a pack choice.  Explicit scouts may remain small, but a stalk/raid
    // handoff must not turn one disposable hunter into the whole fight.
    const int available = site.dispatchable_member_capacity();
    if( available < 2 ) {
        return 2;
    }
    return std::clamp( available, 2, 3 );
}

bandit_dry_run::camp_input make_dispatch_camp_input( const bandit_live_world::site_record &site )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    bandit_dry_run::camp_input camp;
    camp.available_manpower = site.dispatchable_member_capacity();
    if( camp.available_manpower >= 3 ) {
        camp.shortage = bandit_dry_run::shortage_band::stable;
    } else if( camp.available_manpower == 2 ) {
        camp.shortage = bandit_dry_run::shortage_band::low;
    } else {
        camp.shortage = bandit_dry_run::shortage_band::critical;
    }
    camp.job_type_bonus[bandit_dry_run::job_template::scout] = rules.scout_job_bonus;
    return camp;
}

bandit_dry_run::lead_input make_nearby_target_lead( const bandit_live_world::site_record &site,
        const tripoint_abs_omt &target_omt, const std::string &target_id )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    bandit_dry_run::lead_input lead;
    lead.id = target_id;
    lead.envelope_id = target_id;
    lead.family = bandit_dry_run::lead_family::site;
    const int distance = rl_dist( site.anchor, target_omt );
    lead.distance_multiplier = std::clamp( 1.0 - static_cast<double>( distance ) / 20.0, 0.35, 1.0 );
    lead.lead_bounty_value = distance <= 10 ? 2 : 1;
    lead.lead_confidence_bonus = 1;
    lead.threat_penalty = rules.threat_penalty;
    lead.threat_gate_result = bandit_dry_run::threat_gate::soft_veto;
    lead.hard_blocked_jobs = {
        bandit_dry_run::job_template::scavenge,
        bandit_dry_run::job_template::steal,
    };
    if( rules.profile == hostile_site_profile::cannibal_camp ) {
        if( site.dispatchable_member_capacity() >= 2 ) {
            lead.family = bandit_dry_run::lead_family::corridor;
            lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::scout );
            lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::toll );
            lead.validity_notes.push_back(
                "cannibal_camp pack pressure: nearby target promotes stalk pressure only after reserve leaves a pack" );
        } else {
            lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::raid );
            lead.validity_notes.push_back(
                "cannibal_camp scout/probe pressure: lone available member may scout but cannot become the whole attack pack" );
        }
    } else {
        lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::raid );
    }
    lead.validity_notes.push_back( "live-world nearby target envelope from owned site " + site.site_id );
    lead.validity_notes.push_back( "hostile profile " + rules.id + ": " + rules.writeback_expectation );
    lead.validity_notes.push_back( rules.profile == hostile_site_profile::cannibal_camp ?
                                   "bounded v0 dispatch separates cannibal scout/probe pressure from pack attack pressure" :
                                   "bounded v0 dispatch only promotes scout pursuit from real owned members" );
    return lead;
}

std::vector<character_id> select_dispatch_members( const bandit_live_world::site_record &site, int count )
{
    std::vector<character_id> member_ids;
    member_ids.reserve( std::max( count, 0 ) );
    for( const bandit_live_world::member_record &member : site.members ) {
        if( member.state != member_state::at_home || member.wounded_or_unready ) {
            continue;
        }
        member_ids.push_back( member.npc_id );
        if( static_cast<int>( member_ids.size() ) >= count ) {
            break;
        }
    }
    return member_ids;
}

bandit_pursuit_handoff::abstract_group_state make_dispatch_group( const bandit_live_world::site_record &site,
        const std::vector<character_id> &member_ids, const std::string &target_id )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    bandit_pursuit_handoff::abstract_group_state group = make_site_memory_group( site );
    group.group_id = site.site_id + "#dispatch";
    group.source_camp_id = site.site_id;
    group.group_strength = member_ids.size();
    group.confidence = std::clamp( site.count_live_members(), 1, 3 );
    group.panic_threshold = std::max( 1, static_cast<int>( member_ids.size() ) );
    group.cargo_capacity = std::max( 1, static_cast<int>( member_ids.size() ) * 2 );
    group.current_target_or_mark = target_id;
    group.current_threat_estimate = std::max( 1, group.current_threat_estimate );
    group.current_bounty_estimate = std::max( 2, group.current_bounty_estimate );
    group.mission_urgency = 1;
    group.retreat_bias = std::max( group.retreat_bias, rules.retreat_bias_floor );
    group.goal_stickiness = 1;
    group.goal_preemption_posture = 1;
    group.return_clock = std::max( group.return_clock, rules.return_clock_floor );
    group.remaining_pressure = rules.default_remaining_pressure;
    group.anchored_identities.clear();
    for( const character_id &member_id : member_ids ) {
        group.anchored_identities.push_back( { std::to_string( member_id.get_value() ), "alive" } );
    }
    push_unique_mark( group.known_recent_marks, target_id );
    return group;
}

bool report_matches_camp_decision( const bandit_live_world::scout_report_record &report,
                                   const bandit_live_world::camp_decision_record &decision )
{
    return report.is_present() && !report.provisional && report.source_job_type == "scout" &&
           decision.has_pinned_report() &&
           decision.source_report_revision == report.revision &&
           decision.source_report_generation == report.source_generation &&
           decision.source_report_activity_id == report.source_activity_id &&
           decision.source_report_application_key == report.application_key &&
           decision.target_id == report.target_id && decision.target_omt == report.target_omt &&
           decision.target_lead_revision == report.target_lead_revision;
}

bool hostile_operation_job_matches( const hostile_operation_kind operation_kind,
                                    const std::string &job_type )
{
    switch( operation_kind ) {
        case hostile_operation_kind::shakedown:
            return job_type == "stalk" || job_type == "toll";
        case hostile_operation_kind::raid:
            return job_type == "raid";
        case hostile_operation_kind::none:
            return false;
    }

    return false;
}

hostile_operation_kind hostile_operation_kind_for_job( const std::string &job_type )
{
    return job_type == "raid" ? hostile_operation_kind::raid :
           hostile_operation_kind::shakedown;
}

hostile_operation_kind hostile_operation_kind_for_profile( const hostile_site_profile profile )
{
    return profile == hostile_site_profile::cannibal_camp ? hostile_operation_kind::raid :
           hostile_operation_kind::shakedown;
}

bool report_matches_hostile_operation( const bandit_live_world::scout_report_record &report,
                                       const bandit_live_world::hostile_operation_state &operation )
{
    return report.is_present() && !report.provisional && report.source_job_type == "scout" &&
           operation.source_report_revision == report.revision &&
           operation.source_report_generation == report.source_generation &&
           operation.source_report_activity_id == report.source_activity_id &&
           operation.source_report_application_key == report.application_key &&
           operation.reservation.target_id == report.target_id &&
           operation.reservation.target_omt == report.target_omt &&
           operation.reservation.target_lead_revision == report.target_lead_revision;
}

bool hostile_operation_phase_matches_reservation(
    const bandit_live_world::hostile_operation_state &operation )
{
    switch( operation.phase ) {
        case hostile_operation_phase::assembling:
            return operation.reservation.phase == scout_phase::assembling;
        case hostile_operation_phase::committed_contact:
            return operation.reservation.phase == scout_phase::observing;
        case hostile_operation_phase::returning_home:
            return operation.reservation.phase == scout_phase::returning_home;
        case hostile_operation_phase::lost:
            return operation.reservation.phase == scout_phase::lost;
        default:
            return operation.reservation.phase == scout_phase::outbound;
    }
}

bool simulation_owner_state_is_consistent(
    const bandit_live_world::active_outing_state &outing )
{
    const bool owner_matches_epoch =
        ( outing.owner == simulation_owner::abstract && outing.handoff_epoch % 2 == 0 ) ||
        ( outing.owner == simulation_owner::local && outing.handoff_epoch % 2 == 1 );
    const int minimum_advanced_minutes = std::max( { outing.started_minutes,
            outing.local_contact_minutes, outing.last_progress_minutes } );
    return outing.handoff_epoch >= 0 && owner_matches_epoch &&
           outing.last_advanced_minutes >= minimum_advanced_minutes;
}

bool simulation_cursor_matches(
    const bandit_live_world::active_outing_state &outing,
    const bandit_live_world::simulation_advance_cursor &cursor )
{
    return outing.is_active() && simulation_owner_state_is_consistent( outing ) &&
           outing.activity_id == cursor.activity_id &&
           outing.generation == cursor.generation && outing.owner == cursor.owner &&
           outing.handoff_epoch == cursor.handoff_epoch &&
           outing.last_advanced_minutes == cursor.last_advanced_minutes;
}

void normalize_legacy_simulation_owner_state(
    bandit_live_world::active_outing_state &outing )
{
    if( !outing.is_active() || outing.schema_version >= 4 ) {
        return;
    }
    outing.handoff_epoch = std::max( 0, outing.handoff_epoch );
    const bool parity_matches =
        ( outing.owner == simulation_owner::abstract && outing.handoff_epoch % 2 == 0 ) ||
        ( outing.owner == simulation_owner::local && outing.handoff_epoch % 2 == 1 );
    if( !parity_matches ) {
        if( outing.handoff_epoch < std::numeric_limits<int>::max() ) {
            outing.handoff_epoch++;
        } else {
            outing.handoff_epoch--;
        }
    }
    outing.last_advanced_minutes = std::max( { outing.last_advanced_minutes,
            outing.started_minutes, outing.local_contact_minutes,
            outing.last_progress_minutes } );
    outing.schema_version = 4;
}

bool camp_decision_allows_dispatch( const bandit_live_world::camp_decision_record &decision,
                                    const bandit_dry_run::job_template job )
{
    if( job == bandit_dry_run::job_template::scout ) {
        return decision.state == camp_decision_state::idle ||
               decision.state == camp_decision_state::abandoned;
    }
    return decision.state == camp_decision_state::preparing_follow_on;
}
} // namespace

namespace bandit_live_world
{
std::string to_string( anchor_source_kind source_kind )
{
    switch( source_kind ) {
        case anchor_source_kind::none:
            return "none";
        case anchor_source_kind::overmap_special:
            return "overmap_special";
        case anchor_source_kind::map_extra:
            return "map_extra";
    }

    return "none";
}

std::string to_string( owned_site_kind site_kind )
{
    switch( site_kind ) {
        case owned_site_kind::none:
            return "none";
        case owned_site_kind::bandit_camp:
            return "bandit_camp";
        case owned_site_kind::bandit_work_camp:
            return "bandit_work_camp";
        case owned_site_kind::bandit_cabin:
            return "bandit_cabin";
        case owned_site_kind::cannibal_camp:
            return "cannibal_camp";
        case owned_site_kind::looters:
            return "looters";
        case owned_site_kind::bandits_block:
            return "bandits_block";
    }

    return "none";
}

std::string to_string( hostile_site_profile profile )
{
    switch( profile ) {
        case hostile_site_profile::none:
            return "none";
        case hostile_site_profile::camp_style:
            return "camp_style";
        case hostile_site_profile::cannibal_camp:
            return "cannibal_camp";
        case hostile_site_profile::small_hostile_site:
            return "small_hostile_site";
    }

    return "none";
}

std::string to_string( member_state state )
{
    switch( state ) {
        case member_state::at_home:
            return "at_home";
        case member_state::outbound:
            return "outbound";
        case member_state::local_contact:
            return "local_contact";
        case member_state::dead:
            return "dead";
        case member_state::missing:
            return "missing";
    }

    return "at_home";
}

std::string to_string( active_member_observation_state state )
{
    switch( state ) {
        case active_member_observation_state::unresolved:
            return "unresolved";
        case active_member_observation_state::local_contact:
            return "local_contact";
        case active_member_observation_state::returning_home:
            return "returning_home";
        case active_member_observation_state::home:
            return "home";
        case active_member_observation_state::dead:
            return "dead";
        case active_member_observation_state::missing:
            return "missing";
    }

    return "unresolved";
}

std::string to_string( local_gate_posture posture )
{
    switch( posture ) {
        case local_gate_posture::stalk:
            return "stalk";
        case local_gate_posture::hold_off:
            return "hold_off";
        case local_gate_posture::probe:
            return "probe";
        case local_gate_posture::open_shakedown:
            return "open_shakedown";
        case local_gate_posture::attack_now:
            return "attack_now";
        case local_gate_posture::abort:
            return "abort";
    }

    return "abort";
}

std::string to_string( camp_lead_kind kind )
{
    switch( kind ) {
        case camp_lead_kind::structural_bounty:
            return "structural_bounty";
        case camp_lead_kind::harvested_site:
            return "harvested_site";
        case camp_lead_kind::human_activity:
            return "human_activity";
        case camp_lead_kind::basecamp_activity:
            return "basecamp_activity";
        case camp_lead_kind::moving_actor:
            return "moving_actor";
        case camp_lead_kind::route_activity:
            return "route_activity";
        case camp_lead_kind::smoke_signal:
            return "smoke_signal";
        case camp_lead_kind::light_signal:
            return "light_signal";
        case camp_lead_kind::sound_signal:
            return "sound_signal";
        case camp_lead_kind::threat_memory:
            return "threat_memory";
        case camp_lead_kind::loss_site:
            return "loss_site";
        case camp_lead_kind::false_lead:
            return "false_lead";
        case camp_lead_kind::frontier_probe:
            return "frontier_probe";
    }

    return "human_activity";
}

std::string to_string( camp_lead_status status )
{
    switch( status ) {
        case camp_lead_status::suspected:
            return "suspected";
        case camp_lead_status::scout_confirmed:
            return "scout_confirmed";
        case camp_lead_status::active:
            return "active";
        case camp_lead_status::harvested:
            return "harvested";
        case camp_lead_status::stale:
            return "stale";
        case camp_lead_status::invalidated:
            return "invalidated";
        case camp_lead_status::dangerous:
            return "dangerous";
    }

    return "suspected";
}

std::string to_string( outing_kind kind )
{
    switch( kind ) {
        case outing_kind::none:
            return "none";
        case outing_kind::scout_sortie:
            return "scout_sortie";
        case outing_kind::hostile_operation:
            return "hostile_operation";
        case outing_kind::structural_sortie:
            return "structural_sortie";
    }

    return "none";
}

std::string to_string( simulation_owner owner )
{
    switch( owner ) {
        case simulation_owner::abstract:
            return "abstract";
        case simulation_owner::local:
            return "local";
    }

    return "abstract";
}

std::string to_string( scout_phase phase )
{
    switch( phase ) {
        case scout_phase::assembling:
            return "assembling";
        case scout_phase::outbound:
            return "outbound";
        case scout_phase::searching:
            return "searching";
        case scout_phase::observing:
            return "observing";
        case scout_phase::harvesting:
            return "harvesting";
        case scout_phase::burned_withdrawal:
            return "burned_withdrawal";
        case scout_phase::returning_exposed:
            return "returning_exposed";
        case scout_phase::returning_report:
            return "returning_report";
        case scout_phase::returning_home:
            return "returning_home";
        case scout_phase::lost:
            return "lost";
    }

    return "assembling";
}

std::string to_string( camp_decision_state state )
{
    switch( state ) {
        case camp_decision_state::idle:
            return "idle";
        case camp_decision_state::report_awaiting_assessment:
            return "report_awaiting_assessment";
        case camp_decision_state::preparing_follow_on:
            return "preparing_follow_on";
        case camp_decision_state::cooldown:
            return "cooldown";
        case camp_decision_state::abandoned:
            return "abandoned";
    }

    return "abandoned";
}

std::string to_string( hostile_operation_kind kind )
{
    switch( kind ) {
        case hostile_operation_kind::none:
            return "none";
        case hostile_operation_kind::shakedown:
            return "shakedown";
        case hostile_operation_kind::raid:
            return "raid";
    }

    return "none";
}

std::string to_string( hostile_operation_phase phase )
{
    switch( phase ) {
        case hostile_operation_phase::assembling:
            return "assembling";
        case hostile_operation_phase::outbound:
            return "outbound";
        case hostile_operation_phase::rallying:
            return "rallying";
        case hostile_operation_phase::waiting_night:
            return "waiting_night";
        case hostile_operation_phase::approaching:
            return "approaching";
        case hostile_operation_phase::committed_contact:
            return "committed_contact";
        case hostile_operation_phase::returning_home:
            return "returning_home";
        case hostile_operation_phase::lost:
            return "lost";
    }

    return "lost";
}

bool is_valid_scout_phase_transition( const scout_phase previous_phase,
                                      const scout_phase next_phase )
{
    if( previous_phase == next_phase ) {
        return true;
    }
    if( next_phase == scout_phase::lost ) {
        return previous_phase != scout_phase::lost;
    }

    switch( previous_phase ) {
        case scout_phase::assembling:
            return next_phase == scout_phase::outbound;
        case scout_phase::outbound:
            return next_phase == scout_phase::searching ||
                   next_phase == scout_phase::observing ||
                   next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::searching:
            return next_phase == scout_phase::observing ||
                   next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::observing:
            return next_phase == scout_phase::harvesting ||
                   next_phase == scout_phase::burned_withdrawal ||
                   next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::harvesting:
            return next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::burned_withdrawal:
            return next_phase == scout_phase::returning_exposed ||
                   next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::returning_exposed:
            return next_phase == scout_phase::returning_report ||
                   next_phase == scout_phase::returning_home;
        case scout_phase::returning_report:
            return next_phase == scout_phase::returning_home;
        case scout_phase::returning_home:
        case scout_phase::lost:
            return false;
    }

    return false;
}

simulation_owner_transition_result transition_external_simulation_owner(
    site_record &site, const std::string &expected_activity_id,
    const int expected_generation, const simulation_owner expected_owner,
    const simulation_owner next_owner, const int expected_handoff_epoch,
    const int expected_last_advanced_minutes, const int current_minutes )
{
    const active_outing_state *current = site.active_external_outing();
    if( current == nullptr || !current->is_active() ||
        !simulation_owner_state_is_consistent( *current ) ||
        current->activity_id != expected_activity_id ||
        current->generation != expected_generation || current->owner != expected_owner ||
        current->handoff_epoch != expected_handoff_epoch || expected_handoff_epoch < 0 ||
        current->last_advanced_minutes != expected_last_advanced_minutes ||
        current_minutes < 0 || current_minutes < current->last_advanced_minutes ) {
        return simulation_owner_transition_result::rejected;
    }
    if( expected_owner == next_owner ) {
        return simulation_owner_transition_result::unchanged;
    }
    if( current->handoff_epoch == std::numeric_limits<int>::max() ) {
        return simulation_owner_transition_result::rejected;
    }

    site_record candidate = site;
    active_outing_state *next = candidate.active_external_outing();
    if( next == nullptr ) {
        return simulation_owner_transition_result::rejected;
    }
    next->owner = next_owner;
    next->handoff_epoch++;
    next->last_advanced_minutes = current_minutes;
    site = std::move( candidate );
    return simulation_owner_transition_result::applied;
}

simulation_owner_transition_result advance_external_simulation(
    site_record &site, const std::string &expected_activity_id,
    const int expected_generation, const simulation_owner expected_owner,
    const int expected_handoff_epoch, const int expected_last_advanced_minutes,
    const int current_minutes )
{
    const active_outing_state *current = site.active_external_outing();
    if( current == nullptr || !current->is_active() ||
        !simulation_owner_state_is_consistent( *current ) ||
        current->activity_id != expected_activity_id ||
        current->generation != expected_generation || current->owner != expected_owner ||
        current->handoff_epoch != expected_handoff_epoch || expected_handoff_epoch < 0 ||
        current->last_advanced_minutes != expected_last_advanced_minutes ||
        current_minutes < 0 || current_minutes < current->last_advanced_minutes ) {
        return simulation_owner_transition_result::rejected;
    }
    if( current_minutes == current->last_advanced_minutes ) {
        return simulation_owner_transition_result::rejected;
    }

    site_record candidate = site;
    active_outing_state *next = candidate.active_external_outing();
    if( next == nullptr ) {
        return simulation_owner_transition_result::rejected;
    }
    next->last_advanced_minutes = current_minutes;
    site = std::move( candidate );
    return simulation_owner_transition_result::applied;
}

scout_phase scout_phase_after_burned_evacuation( const bool concealed_rally_reached )
{
    return concealed_rally_reached ? scout_phase::returning_report :
           scout_phase::returning_exposed;
}

bool scout_phase_requires_homeward_only( const scout_phase phase )
{
    return phase == scout_phase::burned_withdrawal ||
           phase == scout_phase::returning_exposed ||
           phase == scout_phase::returning_report ||
           phase == scout_phase::returning_home || phase == scout_phase::lost;
}

scout_phase_transition_result transition_active_scout_phase( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const scout_phase expected_phase, const scout_phase next_phase,
        const int current_minutes )
{
    if( !site.active_outing.is_active() ||
        !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        site.active_outing.kind != outing_kind::scout_sortie ||
        ( site.active_outing.job_type != "scout" &&
          site.active_outing.job_type != "scavenge" ) || current_minutes < 0 ||
        site.active_outing.phase != expected_phase ||
        !is_valid_scout_phase_transition( expected_phase, next_phase ) ||
        current_minutes < site.active_outing.last_progress_minutes ||
        current_minutes < site.active_outing.last_advanced_minutes ) {
        return scout_phase_transition_result::rejected;
    }
    if( expected_phase == next_phase ) {
        return scout_phase_transition_result::unchanged;
    }
    if( current_minutes <= site.active_outing.last_advanced_minutes ) {
        return scout_phase_transition_result::rejected;
    }

    site.active_outing.phase = next_phase;
    site.active_outing.last_progress_minutes = current_minutes;
    site.active_outing.last_advanced_minutes = current_minutes;
    return scout_phase_transition_result::applied;
}

bool is_valid_hostile_operation_phase_transition(
    const hostile_operation_phase previous_phase,
    const hostile_operation_phase next_phase )
{
    if( previous_phase == next_phase ) {
        return true;
    }
    if( next_phase == hostile_operation_phase::lost ) {
        return previous_phase != hostile_operation_phase::lost;
    }

    switch( previous_phase ) {
        case hostile_operation_phase::assembling:
            return next_phase == hostile_operation_phase::outbound;
        case hostile_operation_phase::outbound:
            return next_phase == hostile_operation_phase::rallying ||
                   next_phase == hostile_operation_phase::returning_home;
        case hostile_operation_phase::rallying:
            return next_phase == hostile_operation_phase::waiting_night ||
                   next_phase == hostile_operation_phase::approaching ||
                   next_phase == hostile_operation_phase::returning_home;
        case hostile_operation_phase::waiting_night:
            return next_phase == hostile_operation_phase::approaching ||
                   next_phase == hostile_operation_phase::returning_home;
        case hostile_operation_phase::approaching:
            return next_phase == hostile_operation_phase::committed_contact ||
                   next_phase == hostile_operation_phase::returning_home;
        case hostile_operation_phase::committed_contact:
            return next_phase == hostile_operation_phase::returning_home;
        case hostile_operation_phase::returning_home:
        case hostile_operation_phase::lost:
            return false;
    }

    return false;
}

hostile_operation_transition_result transition_hostile_operation_phase(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const hostile_operation_phase expected_phase,
    const hostile_operation_phase next_phase, const int current_minutes,
    const std::string &reason )
{
    const hostile_operation_state &operation = site.active_hostile_operation;
    const active_outing_state &reservation = operation.reservation;
    if( !operation.is_active() ||
        !simulation_cursor_matches( reservation, expected_cursor ) ||
        operation.phase != expected_phase ||
        current_minutes < 0 || current_minutes < reservation.last_progress_minutes ||
        current_minutes < reservation.last_advanced_minutes ||
        !is_valid_hostile_operation_phase_transition( expected_phase, next_phase ) ||
        ( expected_phase != next_phase && reason.empty() ) ||
        ( operation.legacy_unpinned && expected_phase != next_phase &&
          next_phase != hostile_operation_phase::lost ) ) {
        return hostile_operation_transition_result::rejected;
    }
    if( expected_phase == next_phase ) {
        return hostile_operation_transition_result::unchanged;
    }
    if( current_minutes <= reservation.last_advanced_minutes ) {
        return hostile_operation_transition_result::rejected;
    }

    site_record candidate = site;
    hostile_operation_state &next_operation = candidate.active_hostile_operation;
    active_outing_state &next_reservation = next_operation.reservation;
    if( expected_phase == hostile_operation_phase::assembling &&
        next_phase == hostile_operation_phase::outbound ) {
        if( !hostile_operation_party_preserves_home( candidate,
                next_reservation.member_ids.size() ) ) {
            return hostile_operation_transition_result::rejected;
        }
        for( const character_id &member_id : next_reservation.member_ids ) {
            member_record *member = candidate.find_member( member_id );
            if( member == nullptr || member->state != member_state::at_home ||
                member->wounded_or_unready ) {
                return hostile_operation_transition_result::rejected;
            }
        }
        for( const character_id &member_id : next_reservation.member_ids ) {
            member_record *member = candidate.find_member( member_id );
            member->state = member_state::outbound;
            member->last_writeback_summary = "hostile operation departed camp";
        }
    } else if( expected_phase == hostile_operation_phase::assembling &&
               next_phase == hostile_operation_phase::lost ) {
        next_reservation.resolved_member_ids = next_reservation.member_ids;
    }
    if( next_phase == hostile_operation_phase::committed_contact ) {
        if( next_reservation.owner == simulation_owner::abstract ) {
            if( next_reservation.handoff_epoch == std::numeric_limits<int>::max() ) {
                return hostile_operation_transition_result::rejected;
            }
            next_reservation.owner = simulation_owner::local;
            next_reservation.handoff_epoch++;
        }
        for( const character_id &member_id : next_reservation.member_ids ) {
            member_record *member = candidate.find_member( member_id );
            if( member != nullptr && member->state == member_state::outbound &&
                !next_reservation.member_is_resolved( member_id ) ) {
                member->state = member_state::local_contact;
                member->last_writeback_summary = "hostile operation committed contact";
            }
        }
        if( next_reservation.local_contact_minutes < 0 ) {
            next_reservation.local_contact_minutes = current_minutes;
        }
    } else if( next_phase == hostile_operation_phase::returning_home ) {
        if( next_reservation.owner == simulation_owner::local ) {
            if( next_reservation.handoff_epoch == std::numeric_limits<int>::max() ) {
                return hostile_operation_transition_result::rejected;
            }
            next_reservation.owner = simulation_owner::abstract;
            next_reservation.handoff_epoch++;
        }
        for( const character_id &member_id : next_reservation.member_ids ) {
            member_record *member = candidate.find_member( member_id );
            if( member != nullptr && member->state == member_state::local_contact &&
                !next_reservation.member_is_resolved( member_id ) ) {
                member->state = member_state::outbound;
                member->last_writeback_summary = "hostile operation withdrawing home";
            }
        }
    }

    next_operation.phase = next_phase;
    switch( next_phase ) {
        case hostile_operation_phase::assembling:
            next_reservation.phase = scout_phase::assembling;
            break;
        case hostile_operation_phase::committed_contact:
            next_reservation.phase = scout_phase::observing;
            break;
        case hostile_operation_phase::returning_home:
            next_reservation.phase = scout_phase::returning_home;
            break;
        case hostile_operation_phase::lost:
            next_reservation.phase = scout_phase::lost;
            break;
        default:
            next_reservation.phase = scout_phase::outbound;
            break;
    }
    next_reservation.last_progress_minutes = current_minutes;
    next_reservation.last_advanced_minutes = current_minutes;
    next_operation.last_transition_reason = reason.substr( 0,
            max_camp_decision_reason_length );
    site = std::move( candidate );
    return hostile_operation_transition_result::applied;
}

bool is_valid_camp_decision_transition( const camp_decision_state previous_state,
                                        const camp_decision_state next_state )
{
    if( previous_state == next_state ) {
        return true;
    }
    if( next_state == camp_decision_state::abandoned ) {
        return previous_state != camp_decision_state::abandoned;
    }

    switch( previous_state ) {
        case camp_decision_state::idle:
            return next_state == camp_decision_state::report_awaiting_assessment;
        case camp_decision_state::report_awaiting_assessment:
            return next_state == camp_decision_state::preparing_follow_on ||
                   next_state == camp_decision_state::cooldown;
        case camp_decision_state::preparing_follow_on:
            return next_state == camp_decision_state::cooldown;
        case camp_decision_state::cooldown:
            return next_state == camp_decision_state::idle ||
                   next_state == camp_decision_state::report_awaiting_assessment;
        case camp_decision_state::abandoned:
            return next_state == camp_decision_state::report_awaiting_assessment;
    }

    return false;
}

camp_decision_transition_result accept_current_scout_report_for_assessment( site_record &site )
{
    const scout_report_record &report = site.current_scout_report;
    camp_decision_record &decision = site.camp_decision;
    if( site.active_outing.is_active() || !report.is_present() || report.provisional ||
        report.source_job_type != "scout" ||
        decision.state == camp_decision_state::preparing_follow_on ||
        !is_valid_camp_decision_transition( decision.state,
                                            camp_decision_state::report_awaiting_assessment ) ) {
        return camp_decision_transition_result::rejected;
    }
    const bool same_report = report_matches_camp_decision( report, decision );
    if( same_report ) {
        return decision.state == camp_decision_state::report_awaiting_assessment ?
               camp_decision_transition_result::unchanged :
               camp_decision_transition_result::rejected;
    }
    const bool newer_report = report.source_generation > decision.source_report_generation ||
                              ( report.source_generation == decision.source_report_generation &&
                                report.revision > decision.source_report_revision );
    if( decision.has_pinned_report() && ( !newer_report ||
                                         report.delivered_minutes <
                                         decision.last_transition_minutes ) ) {
        return camp_decision_transition_result::rejected;
    }

    decision.state = camp_decision_state::report_awaiting_assessment;
    decision.source_report_revision = report.revision;
    decision.source_report_generation = report.source_generation;
    decision.source_report_activity_id = report.source_activity_id;
    decision.source_report_application_key = report.application_key;
    decision.target_id = report.target_id;
    decision.target_omt = report.target_omt;
    decision.target_lead_revision = report.target_lead_revision;
    decision.last_transition_minutes = report.delivered_minutes;
    decision.next_eligible_minutes = -1;
    decision.transition_reason = "final scout report delivered for assessment";
    return camp_decision_transition_result::applied;
}

camp_decision_transition_result transition_camp_decision_state( site_record &site,
        const camp_decision_state expected_state, const camp_decision_state next_state,
        const int expected_report_revision, const int expected_report_generation,
        const int current_minutes, const int next_eligible_minutes, const std::string &reason )
{
    camp_decision_record &decision = site.camp_decision;
    if( decision.state != expected_state || current_minutes < 0 ||
        current_minutes < decision.last_transition_minutes ||
        !is_valid_camp_decision_transition( expected_state, next_state ) ) {
        return camp_decision_transition_result::rejected;
    }
    if( expected_state == next_state ) {
        if( decision.has_pinned_report() &&
            ( decision.source_report_revision != expected_report_revision ||
              decision.source_report_generation != expected_report_generation ) ) {
            return camp_decision_transition_result::rejected;
        }
        return camp_decision_transition_result::unchanged;
    }
    if( reason.empty() || !decision.has_pinned_report() ||
        decision.source_report_revision != expected_report_revision ||
        decision.source_report_generation != expected_report_generation ||
        next_state == camp_decision_state::report_awaiting_assessment ) {
        return camp_decision_transition_result::rejected;
    }
    if( next_state == camp_decision_state::preparing_follow_on ) {
        const scout_report_record &report = site.current_scout_report;
        if( site.active_outing.is_active() ||
            !report_matches_camp_decision( report, decision ) ) {
            return camp_decision_transition_result::rejected;
        }
    }
    if( next_state == camp_decision_state::cooldown &&
        next_eligible_minutes < current_minutes ) {
        return camp_decision_transition_result::rejected;
    }
    if( next_state == camp_decision_state::idle ) {
        if( expected_state != camp_decision_state::cooldown ||
            decision.next_eligible_minutes < 0 ||
            current_minutes < decision.next_eligible_minutes ) {
            return camp_decision_transition_result::rejected;
        }
        decision.state = camp_decision_state::idle;
        decision.last_transition_minutes = current_minutes;
        decision.next_eligible_minutes = -1;
        decision.transition_reason = reason.substr( 0, max_camp_decision_reason_length );
        return camp_decision_transition_result::applied;
    }

    decision.state = next_state;
    decision.last_transition_minutes = current_minutes;
    decision.next_eligible_minutes = next_state == camp_decision_state::cooldown ?
                                     next_eligible_minutes : -1;
    decision.transition_reason = reason.substr( 0, max_camp_decision_reason_length );
    return camp_decision_transition_result::applied;
}

void member_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "npc_id", npc_id.get_value() );
    json.member( "npc_template_id", npc_template_id );
    json.member( "home_spawn_tile", home_spawn_tile );
    json.member( "state", to_string( state ) );
    json.member( "wounded_or_unready", wounded_or_unready );
    json.member( "last_writeback_summary", last_writeback_summary );
    json.end_object();
}

void member_record::deserialize( const JsonObject &jo )
{
    int raw_npc_id = -1;
    jo.read( "npc_id", raw_npc_id );
    npc_id.deserialize( raw_npc_id );
    jo.read( "npc_template_id", npc_template_id );
    jo.read( "home_spawn_tile", home_spawn_tile );
    std::string state_string = "at_home";
    jo.read( "state", state_string );
    state = member_state_from_string( state_string ).value_or( member_state::at_home );
    jo.read( "wounded_or_unready", wounded_or_unready );
    jo.read( "last_writeback_summary", last_writeback_summary );
}

void spawn_tile_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "tile", tile );
    json.member( "headcount", headcount );
    json.end_object();
}

void spawn_tile_record::deserialize( const JsonObject &jo )
{
    jo.read( "tile", tile );
    jo.read( "headcount", headcount );
}

void camp_map_lead::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "lead_id", lead_id );
    json.member( "kind", to_string( kind ) );
    json.member( "status", to_string( status ) );
    json.member( "target_id", target_id );
    json.member( "omt", omt );
    json.member( "radius_omt", radius_omt );
    json.member( "source_key", source_key );
    json.member( "source_summary", source_summary );
    json.member( "first_seen_minutes", first_seen_minutes );
    json.member( "last_seen_minutes", last_seen_minutes );
    json.member( "last_checked_minutes", last_checked_minutes );
    json.member( "last_scouted_minutes", last_scouted_minutes );
    json.member( "bounty", bounty );
    json.member( "threat", threat );
    json.member( "confidence", confidence );
    json.member( "threat_confirmed", threat_confirmed );
    json.member( "target_alert", target_alert );
    json.member( "scout_seen", scout_seen );
    json.member( "generated_by_this_camp_routine", generated_by_this_camp_routine );
    json.member( "prior_bandit_losses", prior_bandit_losses );
    json.member( "prior_defender_losses", prior_defender_losses );
    json.member( "times_checked_empty", times_checked_empty );
    json.member( "times_harvested", times_harvested );
    json.member( "last_outcome", last_outcome );
    json.end_object();
}

void camp_map_lead::deserialize( const JsonObject &jo )
{
    jo.read( "lead_id", lead_id );
    std::string kind_string = "human_activity";
    jo.read( "kind", kind_string );
    kind = camp_lead_kind_from_string( kind_string ).value_or( camp_lead_kind::human_activity );
    std::string status_string = "suspected";
    jo.read( "status", status_string );
    status = camp_lead_status_from_string( status_string ).value_or( camp_lead_status::suspected );
    jo.read( "target_id", target_id );
    jo.read( "omt", omt );
    jo.read( "radius_omt", radius_omt );
    jo.read( "source_key", source_key );
    jo.read( "source_summary", source_summary );
    jo.read( "first_seen_minutes", first_seen_minutes );
    jo.read( "last_seen_minutes", last_seen_minutes );
    jo.read( "last_checked_minutes", last_checked_minutes );
    jo.read( "last_scouted_minutes", last_scouted_minutes );
    jo.read( "bounty", bounty );
    jo.read( "threat", threat );
    jo.read( "confidence", confidence );
    jo.read( "threat_confirmed", threat_confirmed );
    jo.read( "target_alert", target_alert );
    jo.read( "scout_seen", scout_seen );
    jo.read( "generated_by_this_camp_routine", generated_by_this_camp_routine );
    jo.read( "prior_bandit_losses", prior_bandit_losses );
    jo.read( "prior_defender_losses", prior_defender_losses );
    jo.read( "times_checked_empty", times_checked_empty );
    jo.read( "times_harvested", times_harvested );
    jo.read( "last_outcome", last_outcome );
}

void camp_intelligence_map::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "last_daily_cleanup_minutes", last_daily_cleanup_minutes );
    json.member( "next_near_tick_minutes", next_near_tick_minutes );
    json.member( "next_mid_tick_minutes", next_mid_tick_minutes );
    json.member( "next_far_tick_minutes", next_far_tick_minutes );
    json.member( "next_frontier_tick_minutes", next_frontier_tick_minutes );
    json.member( "known_radius_omt", known_radius_omt );
    json.member( "frontier_radius_omt", frontier_radius_omt );
    json.member( "leads", leads );
    json.end_object();
}

void camp_intelligence_map::deserialize( const JsonObject &jo )
{
    jo.read( "schema_version", schema_version );
    jo.read( "last_daily_cleanup_minutes", last_daily_cleanup_minutes );
    jo.read( "next_near_tick_minutes", next_near_tick_minutes );
    jo.read( "next_mid_tick_minutes", next_mid_tick_minutes );
    jo.read( "next_far_tick_minutes", next_far_tick_minutes );
    jo.read( "next_frontier_tick_minutes", next_frontier_tick_minutes );
    jo.read( "known_radius_omt", known_radius_omt );
    jo.read( "frontier_radius_omt", frontier_radius_omt );
    jo.read( "leads", leads );
}

camp_map_lead *camp_intelligence_map::find_lead( const std::string &lead_id )
{
    auto iter = std::find_if( leads.begin(), leads.end(), [&lead_id]( const camp_map_lead & lead ) {
        return lead.lead_id == lead_id;
    } );
    return iter != leads.end() ? &*iter : nullptr;
}

const camp_map_lead *camp_intelligence_map::find_lead( const std::string &lead_id ) const
{
    auto iter = std::find_if( leads.begin(), leads.end(), [&lead_id]( const camp_map_lead & lead ) {
        return lead.lead_id == lead_id;
    } );
    return iter != leads.end() ? &*iter : nullptr;
}

void sortie_observation::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "fact_key", fact_key.substr( 0, max_sortie_fact_key_length ) );
    json.member( "summary", summary.substr( 0, max_sortie_summary_length ) );
    json.member( "confidence", std::clamp( confidence, 0, 100 ) );
    json.member( "observed_minutes", std::max( -1, observed_minutes ) );
    json.member( "critical", critical );
    json.end_object();
}

void sortie_observation::deserialize( const JsonObject &jo )
{
    sortie_observation candidate;
    jo.read( "fact_key", candidate.fact_key );
    jo.read( "summary", candidate.summary );
    jo.read( "confidence", candidate.confidence );
    jo.read( "observed_minutes", candidate.observed_minutes );
    jo.read( "critical", candidate.critical );
    candidate.fact_key.resize( std::min( candidate.fact_key.size(), max_sortie_fact_key_length ) );
    candidate.summary.resize( std::min( candidate.summary.size(), max_sortie_summary_length ) );
    candidate.confidence = std::clamp( candidate.confidence, 0, 100 );
    candidate.observed_minutes = std::max( -1, candidate.observed_minutes );
    *this = std::move( candidate );
}

void sortie_cargo::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "supply_units", std::max( 0, supply_units ) );
    json.member( "trade_value", std::max( 0, trade_value ) );
    json.end_object();
}

void sortie_cargo::deserialize( const JsonObject &jo )
{
    sortie_cargo candidate;
    jo.read( "supply_units", candidate.supply_units );
    jo.read( "trade_value", candidate.trade_value );
    candidate.supply_units = std::max( 0, candidate.supply_units );
    candidate.trade_value = std::max( 0, candidate.trade_value );
    *this = candidate;
}

void scout_report_record::clear()
{
    *this = scout_report_record();
}

bool scout_report_record::is_present() const
{
    return revision > 0 && source_generation > 0 && !source_activity_id.empty() &&
           !application_key.empty();
}

void scout_report_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "revision", std::max( 0, revision ) );
    json.member( "source_activity_id", source_activity_id );
    json.member( "source_generation", std::max( 0, source_generation ) );
    json.member( "source_job_type", source_job_type );
    json.member( "target_id", target_id );
    json.member( "target_omt", target_omt );
    json.member( "target_lead_revision", std::max( 0, target_lead_revision ) );
    json.member( "application_key", application_key );
    json.member( "observations", make_bounded_sortie_observations( observations ) );
    std::vector<int> raw_casualty_ids;
    raw_casualty_ids.reserve( std::min( casualty_ids.size(), max_active_outing_casualties ) );
    for( std::size_t index = 0; index < casualty_ids.size() &&
         index < max_active_outing_casualties; ++index ) {
        raw_casualty_ids.push_back( casualty_ids[index].get_value() );
    }
    json.member( "casualty_ids", raw_casualty_ids );
    json.member( "delivered_minutes", std::max( -1, delivered_minutes ) );
    json.member( "provisional", provisional );
    json.end_object();
}

void scout_report_record::deserialize( const JsonObject &jo )
{
    scout_report_record candidate;
    jo.read( "schema_version", candidate.schema_version );
    jo.read( "revision", candidate.revision );
    jo.read( "source_activity_id", candidate.source_activity_id );
    jo.read( "source_generation", candidate.source_generation );
    jo.read( "source_job_type", candidate.source_job_type );
    jo.read( "target_id", candidate.target_id );
    jo.read( "target_omt", candidate.target_omt );
    jo.read( "target_lead_revision", candidate.target_lead_revision );
    jo.read( "application_key", candidate.application_key );
    jo.read( "observations", candidate.observations );
    candidate.observations = make_bounded_sortie_observations( candidate.observations );
    std::vector<int> raw_casualty_ids;
    jo.read( "casualty_ids", raw_casualty_ids );
    for( const int raw_casualty_id : raw_casualty_ids ) {
        character_id casualty_id;
        casualty_id.deserialize( raw_casualty_id );
        if( std::find( candidate.casualty_ids.begin(), candidate.casualty_ids.end(), casualty_id ) ==
            candidate.casualty_ids.end() ) {
            candidate.casualty_ids.push_back( casualty_id );
        }
        if( candidate.casualty_ids.size() >= max_active_outing_casualties ) {
            break;
        }
    }
    jo.read( "delivered_minutes", candidate.delivered_minutes );
    jo.read( "provisional", candidate.provisional );
    candidate.revision = std::max( 0, candidate.revision );
    candidate.source_generation = std::max( 0, candidate.source_generation );
    candidate.target_lead_revision = std::max( 0, candidate.target_lead_revision );
    candidate.delivered_minutes = std::max( -1, candidate.delivered_minutes );
    if( candidate.schema_version < 3 && candidate.source_job_type.empty() &&
        candidate.source_activity_id.find( "#scout" ) != std::string::npos ) {
        candidate.source_job_type = "scout";
    }
    candidate.schema_version = 3;
    if( !candidate.is_present() ) {
        candidate.clear();
    }
    *this = std::move( candidate );
}

void camp_decision_record::clear()
{
    *this = camp_decision_record();
}

bool camp_decision_record::has_pinned_report() const
{
    return source_report_revision > 0 && source_report_generation > 0 &&
           !source_report_activity_id.empty() && !source_report_application_key.empty();
}

void camp_decision_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "state", to_string( state ) );
    json.member( "source_report_revision", std::max( 0, source_report_revision ) );
    json.member( "source_report_generation", std::max( 0, source_report_generation ) );
    json.member( "source_report_activity_id", source_report_activity_id );
    json.member( "source_report_application_key", source_report_application_key );
    json.member( "target_id", target_id );
    json.member( "target_omt", target_omt );
    json.member( "target_lead_revision", std::max( 0, target_lead_revision ) );
    json.member( "last_transition_minutes", std::max( -1, last_transition_minutes ) );
    json.member( "next_eligible_minutes", std::max( -1, next_eligible_minutes ) );
    json.member( "transition_reason",
                 transition_reason.substr( 0, max_camp_decision_reason_length ) );
    json.end_object();
}

void camp_decision_record::deserialize( const JsonObject &jo )
{
    camp_decision_record candidate;
    jo.read( "schema_version", candidate.schema_version );
    const bool state_was_present = jo.has_member( "state" );
    std::string state_string = "idle";
    jo.read( "state", state_string );
    candidate.state = camp_decision_state_from_string( state_string ).value_or(
                          state_was_present ? camp_decision_state::abandoned :
                          camp_decision_state::idle );
    jo.read( "source_report_revision", candidate.source_report_revision );
    jo.read( "source_report_generation", candidate.source_report_generation );
    jo.read( "source_report_activity_id", candidate.source_report_activity_id );
    jo.read( "source_report_application_key", candidate.source_report_application_key );
    jo.read( "target_id", candidate.target_id );
    jo.read( "target_omt", candidate.target_omt );
    jo.read( "target_lead_revision", candidate.target_lead_revision );
    jo.read( "last_transition_minutes", candidate.last_transition_minutes );
    jo.read( "next_eligible_minutes", candidate.next_eligible_minutes );
    jo.read( "transition_reason", candidate.transition_reason );
    candidate.schema_version = 1;
    candidate.source_report_revision = std::max( 0, candidate.source_report_revision );
    candidate.source_report_generation = std::max( 0, candidate.source_report_generation );
    candidate.target_lead_revision = std::max( 0, candidate.target_lead_revision );
    candidate.last_transition_minutes = std::max( -1, candidate.last_transition_minutes );
    candidate.next_eligible_minutes = std::max( -1, candidate.next_eligible_minutes );
    candidate.transition_reason.resize( std::min( candidate.transition_reason.size(),
                                        max_camp_decision_reason_length ) );
    if( !camp_decision_state_from_string( state_string ) &&
        candidate.transition_reason.empty() ) {
        candidate.transition_reason = "unknown persisted camp decision state";
    }
    *this = std::move( candidate );
}

void active_outing_state::clear()
{
    *this = active_outing_state();
}

bool active_outing_state::is_active() const
{
    return !activity_id.empty() && kind != outing_kind::none && generation > 0;
}

bool active_outing_state::member_is_resolved( const character_id npc_id ) const
{
    return std::find( resolved_member_ids.begin(), resolved_member_ids.end(), npc_id ) !=
           resolved_member_ids.end();
}

void active_outing_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "kind", to_string( kind ) );
    json.member( "activity_id", activity_id );
    json.member( "camp_id", camp_id );
    json.member( "generation", generation );
    std::vector<int> raw_member_ids;
    raw_member_ids.reserve( member_ids.size() );
    for( const character_id &member_id : member_ids ) {
        raw_member_ids.push_back( member_id.get_value() );
    }
    json.member( "member_ids", raw_member_ids );
    json.member( "leader_id", leader_id.get_value() );
    const bounded_route_state bounded_route = make_bounded_route_state( shared_route, waypoint_index );
    json.member( "shared_route", bounded_route.route );
    json.member( "waypoint_index", bounded_route.waypoint_index );
    json.member( "target_id", target_id );
    json.member( "target_omt", target_omt );
    json.member( "job_type", job_type );
    json.member( "target_lead_revision", std::max( 0, target_lead_revision ) );
    json.member( "phase", to_string( phase ) );
    json.member( "observations", make_bounded_sortie_observations( observations ) );
    json.member( "cargo", cargo );
    std::vector<int> raw_casualty_ids;
    raw_casualty_ids.reserve( std::min( casualty_ids.size(), max_active_outing_casualties ) );
    for( std::size_t index = 0; index < casualty_ids.size() &&
         index < max_active_outing_casualties; ++index ) {
        raw_casualty_ids.push_back( casualty_ids[index].get_value() );
    }
    json.member( "casualty_ids", raw_casualty_ids );
    std::vector<int> raw_resolved_member_ids;
    raw_resolved_member_ids.reserve( std::min( resolved_member_ids.size(),
                                     max_active_outing_members ) );
    for( std::size_t index = 0; index < resolved_member_ids.size() &&
         index < max_active_outing_members; ++index ) {
        raw_resolved_member_ids.push_back( resolved_member_ids[index].get_value() );
    }
    json.member( "resolved_member_ids", raw_resolved_member_ids );
    json.member( "started_minutes", std::max( -1, started_minutes ) );
    json.member( "local_contact_minutes", std::max( -1, local_contact_minutes ) );
    json.member( "last_progress_minutes", std::max( -1, last_progress_minutes ) );
    json.member( "expected_return_minutes", std::max( -1, expected_return_minutes ) );
    json.member( "missing_deadline_minutes", std::max( -1, missing_deadline_minutes ) );
    json.member( "simulation_owner", to_string( owner ) );
    json.member( "handoff_epoch", handoff_epoch );
    json.member( "last_advanced_minutes", last_advanced_minutes );
    json.member( "return_application_key", return_application_key );
    json.member( "report_application_key", report_application_key );
    json.member( "cargo_application_key", cargo_application_key );
    json.end_object();
}

void active_outing_state::deserialize( const JsonObject &jo )
{
    active_outing_state candidate;
    jo.read( "schema_version", candidate.schema_version );
    std::string kind_string = "none";
    jo.read( "kind", kind_string );
    candidate.kind = outing_kind_from_string( kind_string ).value_or( outing_kind::none );
    jo.read( "activity_id", candidate.activity_id );
    jo.read( "camp_id", candidate.camp_id );
    jo.read( "generation", candidate.generation );
    std::vector<int> raw_member_ids;
    jo.read( "member_ids", raw_member_ids );
    for( const int raw_member_id : raw_member_ids ) {
        character_id member_id;
        member_id.deserialize( raw_member_id );
        candidate.member_ids.push_back( member_id );
    }
    int raw_leader_id = -1;
    jo.read( "leader_id", raw_leader_id );
    candidate.leader_id.deserialize( raw_leader_id );
    jo.read( "shared_route", candidate.shared_route );
    jo.read( "waypoint_index", candidate.waypoint_index );
    const bounded_route_state bounded_route = make_bounded_route_state( candidate.shared_route,
                                              candidate.waypoint_index );
    candidate.shared_route = bounded_route.route;
    candidate.waypoint_index = bounded_route.waypoint_index;
    jo.read( "target_id", candidate.target_id );
    jo.read( "target_omt", candidate.target_omt );
    jo.read( "job_type", candidate.job_type );
    jo.read( "target_lead_revision", candidate.target_lead_revision );
    candidate.target_lead_revision = std::max( 0, candidate.target_lead_revision );
    const bool phase_was_present = jo.has_member( "phase" );
    std::string phase_string = "assembling";
    jo.read( "phase", phase_string );
    candidate.phase = scout_phase_from_string( phase_string ).value_or(
                          phase_was_present ? scout_phase::lost : scout_phase::assembling );
    jo.read( "observations", candidate.observations );
    candidate.observations = make_bounded_sortie_observations( candidate.observations );
    jo.read( "cargo", candidate.cargo );
    std::vector<int> raw_casualty_ids;
    jo.read( "casualty_ids", raw_casualty_ids );
    for( const int raw_casualty_id : raw_casualty_ids ) {
        character_id casualty_id;
        casualty_id.deserialize( raw_casualty_id );
        if( std::find( candidate.member_ids.begin(), candidate.member_ids.end(), casualty_id ) !=
            candidate.member_ids.end() &&
            std::find( candidate.casualty_ids.begin(), candidate.casualty_ids.end(), casualty_id ) ==
            candidate.casualty_ids.end() ) {
            candidate.casualty_ids.push_back( casualty_id );
        }
        if( candidate.casualty_ids.size() >= max_active_outing_casualties ) {
            break;
        }
    }
    std::vector<int> raw_resolved_member_ids;
    jo.read( "resolved_member_ids", raw_resolved_member_ids );
    for( const int raw_resolved_member_id : raw_resolved_member_ids ) {
        character_id resolved_member_id;
        resolved_member_id.deserialize( raw_resolved_member_id );
        if( std::find( candidate.member_ids.begin(), candidate.member_ids.end(), resolved_member_id ) !=
            candidate.member_ids.end() &&
            std::find( candidate.resolved_member_ids.begin(), candidate.resolved_member_ids.end(),
                       resolved_member_id ) == candidate.resolved_member_ids.end() ) {
            candidate.resolved_member_ids.push_back( resolved_member_id );
        }
        if( candidate.resolved_member_ids.size() >= max_active_outing_members ) {
            break;
        }
    }
    for( const character_id &casualty_id : candidate.casualty_ids ) {
        if( std::find( candidate.resolved_member_ids.begin(), candidate.resolved_member_ids.end(),
                      casualty_id ) == candidate.resolved_member_ids.end() ) {
            candidate.resolved_member_ids.push_back( casualty_id );
        }
    }
    jo.read( "started_minutes", candidate.started_minutes );
    jo.read( "local_contact_minutes", candidate.local_contact_minutes );
    jo.read( "last_progress_minutes", candidate.last_progress_minutes );
    jo.read( "expected_return_minutes", candidate.expected_return_minutes );
    jo.read( "missing_deadline_minutes", candidate.missing_deadline_minutes );
    candidate.started_minutes = std::max( -1, candidate.started_minutes );
    candidate.local_contact_minutes = std::max( -1, candidate.local_contact_minutes );
    candidate.last_progress_minutes = std::max( -1, candidate.last_progress_minutes );
    candidate.expected_return_minutes = std::max( -1, candidate.expected_return_minutes );
    candidate.missing_deadline_minutes = std::max( -1, candidate.missing_deadline_minutes );
    if( candidate.expected_return_minutes >= 0 &&
        candidate.missing_deadline_minutes < candidate.expected_return_minutes ) {
        candidate.missing_deadline_minutes = minutes_after_saturated( candidate.expected_return_minutes,
                                             scout_missing_grace_minutes );
    }
    std::string owner_string = "abstract";
    jo.read( "simulation_owner", owner_string );
    candidate.owner = simulation_owner_from_string( owner_string ).value_or(
                          simulation_owner::abstract );
    jo.read( "handoff_epoch", candidate.handoff_epoch );
    jo.read( "last_advanced_minutes", candidate.last_advanced_minutes );
    jo.read( "return_application_key", candidate.return_application_key );
    jo.read( "report_application_key", candidate.report_application_key );
    jo.read( "cargo_application_key", candidate.cargo_application_key );
    if( candidate.activity_id.empty() || candidate.kind == outing_kind::none ||
        candidate.generation <= 0 ) {
        std::vector<character_id> reserved_member_ids = std::move( candidate.member_ids );
        candidate.clear();
        candidate.member_ids = std::move( reserved_member_ids );
    } else {
        if( candidate.member_ids.empty() ) {
            candidate.leader_id = character_id();
        } else if( std::find( candidate.member_ids.begin(), candidate.member_ids.end(),
                             candidate.leader_id ) == candidate.member_ids.end() ) {
            candidate.leader_id = candidate.member_ids.front();
        }
    }
    *this = std::move( candidate );
}

void hostile_operation_state::clear()
{
    *this = hostile_operation_state();
}

bool hostile_operation_state::is_active() const
{
    return operation_kind != hostile_operation_kind::none && reservation.is_active() &&
           reservation.kind == outing_kind::hostile_operation;
}

void hostile_operation_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "operation_kind", to_string( operation_kind ) );
    json.member( "phase", to_string( phase ) );
    json.member( "reservation", reservation );
    json.member( "source_report_revision", std::max( 0, source_report_revision ) );
    json.member( "source_report_generation", std::max( 0, source_report_generation ) );
    json.member( "source_report_activity_id", source_report_activity_id );
    json.member( "source_report_application_key", source_report_application_key );
    json.member( "has_rally", has_rally );
    json.member( "rally_omt", rally_omt );
    json.member( "last_transition_reason",
                 last_transition_reason.substr( 0, max_camp_decision_reason_length ) );
    json.member( "legacy_unpinned", legacy_unpinned );
    json.end_object();
}

void hostile_operation_state::deserialize( const JsonObject &jo )
{
    hostile_operation_state candidate;
    jo.read( "schema_version", candidate.schema_version );
    std::string kind_string = "none";
    jo.read( "operation_kind", kind_string );
    candidate.operation_kind = hostile_operation_kind_from_string( kind_string ).value_or(
                                   hostile_operation_kind::none );
    const bool phase_was_present = jo.has_member( "phase" );
    std::string phase_string = "assembling";
    jo.read( "phase", phase_string );
    candidate.phase = hostile_operation_phase_from_string( phase_string ).value_or(
                          phase_was_present ? hostile_operation_phase::lost :
                          hostile_operation_phase::assembling );
    if( jo.has_member( "reservation" ) ) {
        jo.read( "reservation", candidate.reservation );
    }
    jo.read( "source_report_revision", candidate.source_report_revision );
    jo.read( "source_report_generation", candidate.source_report_generation );
    jo.read( "source_report_activity_id", candidate.source_report_activity_id );
    jo.read( "source_report_application_key", candidate.source_report_application_key );
    jo.read( "has_rally", candidate.has_rally );
    jo.read( "rally_omt", candidate.rally_omt );
    jo.read( "last_transition_reason", candidate.last_transition_reason );
    jo.read( "legacy_unpinned", candidate.legacy_unpinned );
    candidate.schema_version = 1;
    candidate.source_report_revision = std::max( 0, candidate.source_report_revision );
    candidate.source_report_generation = std::max( 0, candidate.source_report_generation );
    candidate.last_transition_reason.resize( std::min( candidate.last_transition_reason.size(),
                                             max_camp_decision_reason_length ) );
    if( candidate.reservation.is_active() ) {
        candidate.reservation.kind = outing_kind::hostile_operation;
    }
    *this = std::move( candidate );
}

void site_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "site_id", site_id );
    json.member( "source_kind", to_string( source_kind ) );
    json.member( "site_kind", to_string( site_kind ) );
    json.member( "hostile_profile", to_string( effective_profile( *this ) ) );
    json.member( "source_id", source_id );
    json.member( "anchor", anchor );
    json.member( "headcount", headcount );
    json.member( "supply_units", supply_units );
    json.member( "supply_last_update_minutes", supply_last_update_minutes );
    json.member( "supply_accounted_living_total", supply_accounted_living_total );
    json.member( "supply_member_minute_remainder", supply_member_minute_remainder );
    json.member( "footprint", footprint );
    json.member( "members", members );
    json.member( "spawn_tiles", spawn_tiles );
    json.member( "next_outing_generation", next_outing_generation );
    json.member( "applied_return_generation", applied_return_generation );
    json.member( "applied_report_generation", applied_report_generation );
    json.member( "applied_cargo_generation", applied_cargo_generation );
    json.member( "last_cargo_application_key", last_cargo_application_key );
    json.member( "current_scout_report", current_scout_report );
    json.member( "camp_decision", camp_decision );
    json.member( "returned_cargo_stock", returned_cargo_stock );
    json.member( "active_outing", active_outing );
    json.member( "active_hostile_operation", active_hostile_operation );
    json.member( "remembered_target_or_mark", remembered_target_or_mark );
    json.member( "remembered_threat_estimate", remembered_threat_estimate );
    json.member( "remembered_bounty_estimate", remembered_bounty_estimate );
    json.member( "remembered_retreat_bias", remembered_retreat_bias );
    json.member( "remembered_return_clock", remembered_return_clock );
    json.member( "remembered_pressure", bandit_pursuit_handoff::to_string( remembered_pressure ) );
    json.member( "known_recent_marks", known_recent_marks );
    json.member( "intelligence_map", intelligence_map );
    json.member( "last_shakedown_outcome", last_shakedown_outcome );
    json.member( "shakedown_last_demanded_value", shakedown_last_demanded_value );
    json.member( "shakedown_last_surrendered_value", shakedown_last_surrendered_value );
    json.member( "shakedown_last_reachable_value", shakedown_last_reachable_value );
    json.member( "shakedown_loot_value", shakedown_loot_value );
    json.member( "shakedown_defender_losses", shakedown_defender_losses );
    json.member( "shakedown_bandit_losses", shakedown_bandit_losses );
    json.member( "shakedown_anger", shakedown_anger );
    json.member( "shakedown_caution", shakedown_caution );
    json.member( "shakedown_basecamp_defenders_at_fight", shakedown_basecamp_defenders_at_fight );
    json.member( "shakedown_basecamp_defender_observation_pending",
                 shakedown_basecamp_defender_observation_pending );
    json.member( "shakedown_reopen_available", shakedown_reopen_available );
    json.member( "shakedown_reopen_used", shakedown_reopen_used );
    json.member( "retired_empty_site", retired_empty_site );
    json.member( "retirement_summary", retirement_summary );
    json.end_object();
}

void site_record::deserialize( const JsonObject &jo )
{
    schema_version = 5;
    supply_units = 0;
    supply_last_update_minutes = -1;
    supply_accounted_living_total = 0;
    supply_member_minute_remainder = 0;
    next_outing_generation = 1;
    applied_return_generation = 0;
    applied_report_generation = 0;
    applied_cargo_generation = 0;
    last_cargo_application_key.clear();
    current_scout_report.clear();
    camp_decision.clear();
    returned_cargo_stock = sortie_cargo();
    active_outing.clear();
    active_hostile_operation.clear();
    jo.read( "schema_version", schema_version );
    const int loaded_schema_version = schema_version;
    jo.read( "site_id", site_id );
    std::string source_kind_string = "none";
    jo.read( "source_kind", source_kind_string );
    source_kind = anchor_source_kind_from_string( source_kind_string ).value_or( anchor_source_kind::none );
    std::string site_kind_string = "none";
    jo.read( "site_kind", site_kind_string );
    site_kind = owned_site_kind_from_string( site_kind_string ).value_or( owned_site_kind::none );
    std::string profile_string = "none";
    jo.read( "hostile_profile", profile_string );
    profile = hostile_site_profile_from_string( profile_string ).value_or( profile_for_site_kind( site_kind ) );
    if( profile == hostile_site_profile::none ) {
        profile = profile_for_site_kind( site_kind );
    }
    jo.read( "source_id", source_id );
    jo.read( "anchor", anchor );
    jo.read( "headcount", headcount );
    const bool complete_supply_payload = jo.has_member( "supply_units" ) &&
                                         jo.has_member( "supply_last_update_minutes" ) &&
                                         jo.has_member( "supply_accounted_living_total" ) &&
                                         jo.has_member( "supply_member_minute_remainder" );
    jo.read( "supply_units", supply_units );
    jo.read( "supply_last_update_minutes", supply_last_update_minutes );
    jo.read( "supply_accounted_living_total", supply_accounted_living_total );
    jo.read( "supply_member_minute_remainder", supply_member_minute_remainder );
    jo.read( "footprint", footprint );
    jo.read( "members", members );
    jo.read( "spawn_tiles", spawn_tiles );
    jo.read( "next_outing_generation", next_outing_generation );
    jo.read( "applied_return_generation", applied_return_generation );
    jo.read( "applied_report_generation", applied_report_generation );
    jo.read( "applied_cargo_generation", applied_cargo_generation );
    jo.read( "last_cargo_application_key", last_cargo_application_key );
    jo.read( "current_scout_report", current_scout_report );
    const bool camp_decision_was_present = jo.has_member( "camp_decision" );
    if( camp_decision_was_present ) {
        jo.read( "camp_decision", camp_decision );
    }
    jo.read( "returned_cargo_stock", returned_cargo_stock );
    const bool active_outing_was_present = jo.has_member( "active_outing" );
    bool active_outing_has_embedded_payload = false;
    std::string legacy_active_group_id;
    if( active_outing_was_present ) {
        JsonObject active_outing_json = jo.get_object( "active_outing" );
        active_outing_has_embedded_payload = active_outing_json.has_member( "member_ids" );
        active_outing.deserialize( active_outing_json );
    } else {
        jo.read( "active_group_id", legacy_active_group_id );
    }
    const bool active_hostile_operation_was_present =
        jo.has_member( "active_hostile_operation" );
    if( active_hostile_operation_was_present ) {
        jo.read( "active_hostile_operation", active_hostile_operation );
    }
    std::string legacy_active_target_id;
    tripoint_abs_omt legacy_active_target_omt;
    std::string legacy_active_job_type;
    int legacy_active_sortie_started_minutes = -1;
    int legacy_active_sortie_local_contact_minutes = -1;
    jo.read( "active_target_id", legacy_active_target_id );
    jo.read( "active_target_omt", legacy_active_target_omt );
    jo.read( "active_job_type", legacy_active_job_type );
    jo.read( "active_sortie_started_minutes", legacy_active_sortie_started_minutes );
    jo.read( "active_sortie_local_contact_minutes", legacy_active_sortie_local_contact_minutes );
    std::vector<int> raw_active_member_ids;
    jo.read( "active_member_ids", raw_active_member_ids );
    std::vector<character_id> legacy_active_member_ids;
    legacy_active_member_ids.reserve( raw_active_member_ids.size() );
    for( const int raw_member_id : raw_active_member_ids ) {
        character_id member_id;
        member_id.deserialize( raw_member_id );
        legacy_active_member_ids.push_back( member_id );
    }
    const bool import_transitional_active_payload = !active_outing_was_present ||
            active_outing.schema_version < 2 ||
            ( active_outing.schema_version < 4 && !active_outing_has_embedded_payload );
    const bool malformed_current_active_payload = active_outing_was_present &&
            active_outing.schema_version >= 4 && !active_outing_has_embedded_payload;
    if( import_transitional_active_payload ) {
        active_outing.member_ids = legacy_active_member_ids;
        active_outing.leader_id = active_outing.member_ids.empty() ? character_id() :
                                  active_outing.member_ids.front();
        active_outing.target_id = legacy_active_target_id;
        active_outing.target_omt = legacy_active_target_omt;
        active_outing.job_type = legacy_active_job_type;
        active_outing.started_minutes = legacy_active_sortie_started_minutes;
        active_outing.local_contact_minutes = legacy_active_sortie_local_contact_minutes;
        active_outing.last_progress_minutes = std::max( legacy_active_sortie_started_minutes,
                                              legacy_active_sortie_local_contact_minutes );
        active_outing.last_advanced_minutes = std::max( active_outing.last_advanced_minutes,
                                               active_outing.last_progress_minutes );
        if( legacy_active_sortie_local_contact_minutes >= 0 ) {
            active_outing.owner = simulation_owner::local;
        }
        active_outing.phase = legacy_active_sortie_local_contact_minutes >= 0 ?
                              scout_phase::observing : scout_phase::outbound;
        const int return_clock_anchor = active_outing.local_contact_minutes >= 0 ?
                                        active_outing.local_contact_minutes : active_outing.started_minutes;
        if( return_clock_anchor >= 0 ) {
            active_outing.expected_return_minutes = minutes_after_saturated(
                    return_clock_anchor,
                    ordinary_scout_sortie_limit_minutes() + scout_return_cohesion_minutes );
            active_outing.missing_deadline_minutes = minutes_after_saturated(
                    active_outing.expected_return_minutes, scout_missing_grace_minutes );
        }
    } else if( malformed_current_active_payload ) {
        for( member_record &member : members ) {
            const bool reserved_for_hostile_operation =
                active_hostile_operation.reservation.is_active() &&
                std::find( active_hostile_operation.reservation.member_ids.begin(),
                           active_hostile_operation.reservation.member_ids.end(),
                           member.npc_id ) !=
                active_hostile_operation.reservation.member_ids.end();
            if( !reserved_for_hostile_operation &&
                ( member.state == member_state::outbound ||
                  member.state == member_state::local_contact ) ) {
                member.state = member_state::at_home;
                member.last_writeback_summary =
                    "closed incomplete current-schema active outing";
            }
        }
        active_outing.clear();
    }
    jo.read( "remembered_target_or_mark", remembered_target_or_mark );
    jo.read( "remembered_threat_estimate", remembered_threat_estimate );
    jo.read( "remembered_bounty_estimate", remembered_bounty_estimate );
    jo.read( "remembered_retreat_bias", remembered_retreat_bias );
    jo.read( "remembered_return_clock", remembered_return_clock );
    std::string remembered_pressure_string = "ample";
    jo.read( "remembered_pressure", remembered_pressure_string );
    remembered_pressure = remaining_return_pressure_state_from_string( remembered_pressure_string ).value_or(
                              bandit_pursuit_handoff::remaining_return_pressure_state::ample );
    jo.read( "known_recent_marks", known_recent_marks );
    const bool intelligence_map_was_present = jo.has_member( "intelligence_map" );
    if( intelligence_map_was_present ) {
        jo.read( "intelligence_map", intelligence_map );
    }
    migrate_scalar_memory_to_intelligence_map( *this, intelligence_map_was_present );
    jo.read( "last_shakedown_outcome", last_shakedown_outcome );
    jo.read( "shakedown_last_demanded_value", shakedown_last_demanded_value );
    jo.read( "shakedown_last_surrendered_value", shakedown_last_surrendered_value );
    jo.read( "shakedown_last_reachable_value", shakedown_last_reachable_value );
    jo.read( "shakedown_loot_value", shakedown_loot_value );
    jo.read( "shakedown_defender_losses", shakedown_defender_losses );
    jo.read( "shakedown_bandit_losses", shakedown_bandit_losses );
    jo.read( "shakedown_anger", shakedown_anger );
    jo.read( "shakedown_caution", shakedown_caution );
    jo.read( "shakedown_basecamp_defenders_at_fight", shakedown_basecamp_defenders_at_fight );
    jo.read( "shakedown_basecamp_defender_observation_pending",
             shakedown_basecamp_defender_observation_pending );
    jo.read( "shakedown_reopen_available", shakedown_reopen_available );
    jo.read( "shakedown_reopen_used", shakedown_reopen_used );
    jo.read( "retired_empty_site", retired_empty_site );
    jo.read( "retirement_summary", retirement_summary );

    if( !active_outing_was_present && !legacy_active_group_id.empty() ) {
        active_outing.schema_version = 1;
        active_outing.activity_id = legacy_active_group_id;
        active_outing.camp_id = site_id;
        active_outing.generation = std::max( 1, applied_return_generation + 1 );
        active_outing.kind = classify_legacy_outing_kind( legacy_active_group_id,
                             active_outing.job_type );
        active_outing.owner = legacy_active_sortie_local_contact_minutes >= 0 ?
                              simulation_owner::local : simulation_owner::abstract;
        active_outing.last_advanced_minutes = std::max( active_outing.started_minutes,
                                               active_outing.local_contact_minutes );
        active_outing.return_application_key = legacy_active_group_id + ":return:" +
                                                std::to_string( active_outing.generation );
    }
    if( active_outing.is_active() ) {
        normalize_legacy_simulation_owner_state( active_outing );
        active_outing.schema_version = 4;
        if( active_outing.camp_id.empty() ) {
            active_outing.camp_id = site_id;
        }
        if( active_outing.return_application_key.empty() ) {
            active_outing.return_application_key = active_outing.activity_id + ":return:" +
                                                    std::to_string( active_outing.generation );
        }
        if( active_outing.report_application_key.empty() ) {
            active_outing.report_application_key = active_outing.activity_id + ":report:" +
                                                    std::to_string( active_outing.generation );
        }
        if( active_outing.cargo_application_key.empty() ) {
            active_outing.cargo_application_key = active_outing.activity_id + ":cargo:" +
                                                   std::to_string( active_outing.generation );
        }
        next_outing_generation = std::max( next_outing_generation, active_outing.generation + 1 );
    }
    const bool new_hostile_payload_present = active_hostile_operation.is_active() ||
            !active_hostile_operation.reservation.member_ids.empty();
    if( active_outing.is_active() && active_outing.kind == outing_kind::hostile_operation &&
        !new_hostile_payload_present ) {
        active_hostile_operation.clear();
        active_hostile_operation.operation_kind = hostile_operation_kind_for_job(
                    active_outing.job_type );
        active_hostile_operation.reservation = std::move( active_outing );
        active_hostile_operation.reservation.kind = outing_kind::hostile_operation;
        switch( active_hostile_operation.reservation.phase ) {
            case scout_phase::observing:
                active_hostile_operation.phase = hostile_operation_phase::committed_contact;
                break;
            case scout_phase::returning_home:
                active_hostile_operation.phase = hostile_operation_phase::returning_home;
                break;
            case scout_phase::lost:
                active_hostile_operation.phase = hostile_operation_phase::lost;
                break;
            default:
                active_hostile_operation.phase = hostile_operation_phase::outbound;
                break;
        }
        const bool exact_report_pin = report_matches_camp_decision( current_scout_report,
                                      camp_decision ) &&
                                      active_hostile_operation.reservation.target_id ==
                                      current_scout_report.target_id &&
                                      active_hostile_operation.reservation.target_omt ==
                                      current_scout_report.target_omt &&
                                      active_hostile_operation.reservation.target_lead_revision ==
                                      current_scout_report.target_lead_revision;
        if( exact_report_pin ) {
            active_hostile_operation.source_report_revision = current_scout_report.revision;
            active_hostile_operation.source_report_generation = current_scout_report.source_generation;
            active_hostile_operation.source_report_activity_id = current_scout_report.source_activity_id;
            active_hostile_operation.source_report_application_key = current_scout_report.application_key;
        }
        const active_outing_state &reservation = active_hostile_operation.reservation;
        if( !reservation.shared_route.empty() ) {
            const int rally_index = std::clamp( reservation.waypoint_index, 0,
                                                static_cast<int>( reservation.shared_route.size() ) - 1 );
            active_hostile_operation.has_rally = true;
            active_hostile_operation.rally_omt = reservation.shared_route[rally_index];
        }
        active_hostile_operation.legacy_unpinned = true;
        if( active_hostile_operation.legacy_unpinned &&
            active_hostile_operation.phase != hostile_operation_phase::lost ) {
            active_hostile_operation.phase = hostile_operation_phase::returning_home;
            active_hostile_operation.reservation.phase = scout_phase::returning_home;
            active_hostile_operation.last_transition_reason =
                "legacy hostile operation retained for safe withdrawal";
        } else {
            active_hostile_operation.last_transition_reason =
                "migrated legacy hostile outing owner";
        }
        active_outing.clear();
    }
    if( active_hostile_operation.reservation.is_active() ) {
        active_hostile_operation.schema_version = 1;
        normalize_legacy_simulation_owner_state(
            active_hostile_operation.reservation );
        active_hostile_operation.reservation.schema_version = 4;
        active_hostile_operation.reservation.kind = outing_kind::hostile_operation;
        if( active_hostile_operation.reservation.camp_id.empty() ) {
            active_hostile_operation.reservation.camp_id = site_id;
        }
        if( active_hostile_operation.reservation.return_application_key.empty() ) {
            active_hostile_operation.reservation.return_application_key =
                active_hostile_operation.reservation.activity_id + ":return:" +
                std::to_string( active_hostile_operation.reservation.generation );
        }
        if( active_hostile_operation.reservation.cargo_application_key.empty() ) {
            active_hostile_operation.reservation.cargo_application_key =
                active_hostile_operation.reservation.activity_id + ":cargo:" +
                std::to_string( active_hostile_operation.reservation.generation );
        }
        next_outing_generation = std::max( next_outing_generation,
                                           active_hostile_operation.reservation.generation + 1 );
    }
    applied_return_generation = std::max( 0, applied_return_generation );
    if( !current_scout_report.provisional ) {
        applied_report_generation = std::max( applied_report_generation,
                                              current_scout_report.source_generation );
    }
    applied_report_generation = std::max( 0, applied_report_generation );
    applied_cargo_generation = std::max( 0, applied_cargo_generation );
    next_outing_generation = std::max( next_outing_generation, applied_return_generation + 1 );
    next_outing_generation = std::max( next_outing_generation, applied_report_generation + 1 );
    next_outing_generation = std::max( next_outing_generation, applied_cargo_generation + 1 );

    const bool both_external_owners_present =
        ( active_outing.is_active() || !active_outing.member_ids.empty() ) &&
        ( active_hostile_operation.is_active() ||
          !active_hostile_operation.reservation.member_ids.empty() );
    if( both_external_owners_present ) {
        active_outing_state &duplicate = active_outing.kind == outing_kind::hostile_operation ?
                                         active_outing : active_hostile_operation.reservation;
        const active_outing_state &retained = active_outing.kind == outing_kind::hostile_operation ?
                                              active_hostile_operation.reservation : active_outing;
        for( const character_id &member_id : duplicate.member_ids ) {
            if( std::find( retained.member_ids.begin(), retained.member_ids.end(), member_id ) !=
                retained.member_ids.end() ) {
                continue;
            }
            member_record *member = find_member( member_id );
            if( member != nullptr && ( member->state == member_state::outbound ||
                                      member->state == member_state::local_contact ) ) {
                member->state = member_state::at_home;
                member->last_writeback_summary = "closed duplicate persisted external owner";
            }
        }
        if( active_outing.kind == outing_kind::hostile_operation ) {
            active_outing.clear();
        } else {
            active_hostile_operation.clear();
        }
    }

    const bool scout_job_is_consistent = active_outing.kind != outing_kind::scout_sortie ||
                                         active_outing.job_type == "scout" ||
                                         active_outing.job_type == "scavenge";
    bool active_outing_is_consistent = active_outing.is_active() && scout_job_is_consistent &&
                                       active_outing.camp_id == site_id &&
                                       active_outing.generation > applied_return_generation &&
                                       active_outing.generation > applied_cargo_generation &&
                                       active_outing.generation > applied_report_generation &&
                                       simulation_owner_state_is_consistent( active_outing ) &&
                                       !active_outing.member_ids.empty() &&
                                       active_outing.member_ids.size() <= max_active_outing_members;
    std::vector<character_id> checked_active_member_ids;
    checked_active_member_ids.reserve( active_outing.member_ids.size() );
    for( const character_id &member_id : active_outing.member_ids ) {
        const member_record *member = find_member( member_id );
        const bool recorded_casualty = std::find( active_outing.casualty_ids.begin(),
                                       active_outing.casualty_ids.end(), member_id ) !=
                                       active_outing.casualty_ids.end();
        const bool resolved_member = active_outing.member_is_resolved( member_id );
        const bool member_state_matches_reservation = member != nullptr &&
                ( recorded_casualty ?
                  ( resolved_member && ( member->state == member_state::dead ||
                                         member->state == member_state::missing ) ) :
                  ( resolved_member ? member->state == member_state::at_home :
                    ( member->state == member_state::outbound ||
                      member->state == member_state::local_contact ) ) );
        if( !member_state_matches_reservation ||
            std::find( checked_active_member_ids.begin(), checked_active_member_ids.end(), member_id ) !=
            checked_active_member_ids.end() ) {
            active_outing_is_consistent = false;
            break;
        }
        checked_active_member_ids.push_back( member_id );
    }
    if( !active_outing_is_consistent && ( active_outing.is_active() ||
                                         !active_outing.member_ids.empty() ) ) {
        for( const character_id &member_id : active_outing.member_ids ) {
            member_record *member = find_member( member_id );
            if( member != nullptr && ( member->state == member_state::outbound ||
                                      member->state == member_state::local_contact ) ) {
                member->state = member_state::at_home;
                member->last_writeback_summary = "closed inconsistent persisted active outing";
            }
        }
        active_outing.clear();
    }
    active_outing_state &hostile_reservation = active_hostile_operation.reservation;
    const bool legacy_withdrawal_only = active_hostile_operation.legacy_unpinned &&
                                        ( active_hostile_operation.phase ==
                                          hostile_operation_phase::returning_home ||
                                          active_hostile_operation.phase == hostile_operation_phase::lost );
    const bool rally_is_on_route = active_hostile_operation.has_rally &&
                                   std::find( hostile_reservation.shared_route.begin(),
                                           hostile_reservation.shared_route.end(),
                                           active_hostile_operation.rally_omt ) !=
                                   hostile_reservation.shared_route.end();
    const bool hostile_job_is_consistent = hostile_operation_job_matches(
                active_hostile_operation.operation_kind, hostile_reservation.job_type ) ||
            legacy_withdrawal_only;
    const bool hostile_report_is_consistent = legacy_withdrawal_only ||
            ( camp_decision.state == camp_decision_state::preparing_follow_on &&
              report_matches_camp_decision( current_scout_report, camp_decision ) &&
              report_matches_hostile_operation( current_scout_report,
                                                active_hostile_operation ) );
    const bool hostile_route_is_consistent = legacy_withdrawal_only ||
            ( hostile_reservation.shared_route.size() >= 2 &&
              hostile_reservation.shared_route.front() == anchor &&
              hostile_reservation.shared_route.back() == hostile_reservation.target_omt &&
              rally_is_on_route );
    const std::string expected_hostile_activity_id = site_id + "#hostile:" +
            std::to_string( hostile_reservation.generation );
    const bool hostile_identity_is_consistent = legacy_withdrawal_only ||
            ( hostile_reservation.activity_id == expected_hostile_activity_id &&
              hostile_reservation.generation >
              active_hostile_operation.source_report_generation &&
              hostile_reservation.return_application_key == expected_hostile_activity_id +
              ":return:" + std::to_string( hostile_reservation.generation ) &&
              hostile_reservation.report_application_key == expected_hostile_activity_id +
              ":report:" + std::to_string( hostile_reservation.generation ) &&
              hostile_reservation.cargo_application_key == expected_hostile_activity_id +
              ":cargo:" + std::to_string( hostile_reservation.generation ) );
    const bool hostile_reserve_is_consistent = legacy_withdrawal_only ||
            active_hostile_operation.phase != hostile_operation_phase::assembling ||
            hostile_operation_party_preserves_home( *this,
                    hostile_reservation.member_ids.size() );
    bool hostile_operation_is_consistent = active_hostile_operation.is_active() &&
            hostile_reservation.camp_id == site_id &&
            hostile_reservation.generation > applied_return_generation &&
            hostile_reservation.generation > applied_cargo_generation &&
            !hostile_reservation.member_ids.empty() &&
            ( legacy_withdrawal_only ?
              hostile_reservation.member_ids.size() <= max_active_outing_members :
              hostile_reservation.member_ids.size() >= 2 &&
              hostile_reservation.member_ids.size() <= max_hostile_operation_members ) &&
            hostile_reservation.shared_route.size() <= max_active_outing_route_steps &&
            hostile_route_is_consistent && hostile_job_is_consistent &&
            hostile_report_is_consistent && hostile_identity_is_consistent &&
            hostile_reserve_is_consistent &&
            simulation_owner_state_is_consistent( hostile_reservation ) &&
            hostile_operation_phase_matches_reservation( active_hostile_operation );
    std::vector<character_id> checked_hostile_member_ids;
    checked_hostile_member_ids.reserve( hostile_reservation.member_ids.size() );
    for( const character_id &member_id : hostile_reservation.member_ids ) {
        const member_record *member = find_member( member_id );
        const bool recorded_casualty = std::find( hostile_reservation.casualty_ids.begin(),
                                       hostile_reservation.casualty_ids.end(), member_id ) !=
                                       hostile_reservation.casualty_ids.end();
        const bool resolved_member = hostile_reservation.member_is_resolved( member_id );
        bool member_state_matches_reservation = member != nullptr;
        if( member_state_matches_reservation && recorded_casualty ) {
            member_state_matches_reservation = resolved_member &&
                                               ( member->state == member_state::dead ||
                                                 member->state == member_state::missing );
        } else if( member_state_matches_reservation && resolved_member ) {
            member_state_matches_reservation = member->state == member_state::at_home;
        } else if( member_state_matches_reservation &&
                   active_hostile_operation.phase == hostile_operation_phase::assembling ) {
            member_state_matches_reservation = member->state == member_state::at_home;
        } else if( member_state_matches_reservation &&
                   active_hostile_operation.phase == hostile_operation_phase::committed_contact ) {
            member_state_matches_reservation = member->state == member_state::local_contact ||
                                               member->state == member_state::outbound;
        } else if( member_state_matches_reservation ) {
            member_state_matches_reservation = member->state == member_state::outbound ||
                                               member->state == member_state::local_contact;
        }
        if( !member_state_matches_reservation ||
            std::find( checked_hostile_member_ids.begin(), checked_hostile_member_ids.end(),
                       member_id ) != checked_hostile_member_ids.end() ) {
            hostile_operation_is_consistent = false;
            break;
        }
        checked_hostile_member_ids.push_back( member_id );
    }
    if( !hostile_operation_is_consistent &&
        ( active_hostile_operation.is_active() ||
          !hostile_reservation.member_ids.empty() ) ) {
        for( const character_id &member_id : hostile_reservation.member_ids ) {
            member_record *member = find_member( member_id );
            if( member != nullptr && ( member->state == member_state::outbound ||
                                      member->state == member_state::local_contact ) ) {
                member->state = member_state::at_home;
                member->last_writeback_summary =
                    "closed inconsistent persisted hostile operation";
            }
        }
        active_hostile_operation.clear();
    }
    if( current_scout_report.provisional ) {
        const int returned_member_count = static_cast<int>( std::count_if(
                    active_outing.resolved_member_ids.begin(),
                    active_outing.resolved_member_ids.end(), [this]( const character_id & member_id ) {
            const member_record *member = find_member( member_id );
            return member != nullptr && member->state == member_state::at_home;
        } ) );
        const bool provisional_report_is_consistent =
            active_outing.is_active() && active_outing.kind == outing_kind::scout_sortie &&
            current_scout_report.source_activity_id == active_outing.activity_id &&
            current_scout_report.source_generation == active_outing.generation &&
            current_scout_report.source_job_type == active_outing.job_type &&
            current_scout_report.target_id == active_outing.target_id &&
            current_scout_report.target_omt == active_outing.target_omt &&
            current_scout_report.target_lead_revision == active_outing.target_lead_revision &&
            current_scout_report.application_key == provisional_report_application_key( *this ) &&
            returned_member_count > 0;
        if( !provisional_report_is_consistent ) {
            current_scout_report.clear();
        }
    }
    if( !camp_decision_was_present && !active_outing.is_active() &&
        current_scout_report.is_present() && !current_scout_report.provisional ) {
        accept_current_scout_report_for_assessment( *this );
    }
    if( camp_decision.state == camp_decision_state::abandoned &&
        !camp_decision.has_pinned_report() && current_scout_report.is_present() &&
        !current_scout_report.provisional ) {
        camp_decision.source_report_revision = current_scout_report.revision;
        camp_decision.source_report_generation = current_scout_report.source_generation;
        camp_decision.source_report_activity_id = current_scout_report.source_activity_id;
        camp_decision.source_report_application_key = current_scout_report.application_key;
        camp_decision.target_id = current_scout_report.target_id;
        camp_decision.target_omt = current_scout_report.target_omt;
        camp_decision.target_lead_revision = current_scout_report.target_lead_revision;
        camp_decision.last_transition_minutes = std::max(
                    camp_decision.last_transition_minutes,
                    current_scout_report.delivered_minutes );
    }
    const bool decision_requires_current_report =
        camp_decision.state == camp_decision_state::report_awaiting_assessment ||
        camp_decision.state == camp_decision_state::preparing_follow_on;
    const bool decision_matches_current_report = report_matches_camp_decision(
                current_scout_report, camp_decision );
    const bool malformed_cooldown = camp_decision.state == camp_decision_state::cooldown &&
                                    ( !camp_decision.has_pinned_report() ||
                                      camp_decision.next_eligible_minutes < 0 );
    if( ( decision_requires_current_report && !decision_matches_current_report ) ||
        malformed_cooldown ) {
        camp_decision.state = camp_decision_state::abandoned;
        camp_decision.next_eligible_minutes = -1;
        camp_decision.transition_reason = "repaired inconsistent persisted camp decision";
    }
    if( loaded_schema_version < 6 ) {
        seed_camp_supply( *this );
    } else if( !complete_supply_payload ) {
        supply_units = 0;
        supply_last_update_minutes = -1;
        supply_accounted_living_total = camp_supply_living_total( *this );
        supply_member_minute_remainder = 0;
    } else {
        supply_units = std::clamp( supply_units, 0, camp_supply_cap( *this ) );
        supply_last_update_minutes = std::max( -1, supply_last_update_minutes );
        supply_accounted_living_total = std::max( 0, supply_accounted_living_total );
        supply_member_minute_remainder = std::clamp( supply_member_minute_remainder, 0,
                                         minutes_per_member_day - 1 );
        if( supply_last_update_minutes < 0 ) {
            supply_accounted_living_total = camp_supply_living_total( *this );
            supply_member_minute_remainder = 0;
        }
    }
    schema_version = 6;
}

bool site_record::has_member( character_id target_npc_id ) const
{
    return std::any_of( members.begin(), members.end(), [target_npc_id]( const member_record & member ) {
        return member.npc_id == target_npc_id;
    } );
}

member_record *site_record::find_member( character_id target_npc_id )
{
    auto iter = std::find_if( members.begin(), members.end(), [target_npc_id]( const member_record & member ) {
        return member.npc_id == target_npc_id;
    } );
    return iter != members.end() ? &*iter : nullptr;
}

const member_record *site_record::find_member( character_id target_npc_id ) const
{
    auto iter = std::find_if( members.begin(), members.end(), [target_npc_id]( const member_record & member ) {
        return member.npc_id == target_npc_id;
    } );
    return iter != members.end() ? &*iter : nullptr;
}

spawn_tile_record *site_record::find_spawn_tile( const tripoint_abs_ms &tile )
{
    auto iter = std::find_if( spawn_tiles.begin(), spawn_tiles.end(), [&tile]( const spawn_tile_record & record ) {
        return record.tile == tile;
    } );
    return iter != spawn_tiles.end() ? &*iter : nullptr;
}

const spawn_tile_record *site_record::find_spawn_tile( const tripoint_abs_ms &tile ) const
{
    auto iter = std::find_if( spawn_tiles.begin(), spawn_tiles.end(), [&tile]( const spawn_tile_record & record ) {
        return record.tile == tile;
    } );
    return iter != spawn_tiles.end() ? &*iter : nullptr;
}

int site_record::count_members_in_state( member_state target_state ) const
{
    return static_cast<int>( std::count_if( members.begin(), members.end(),
    [target_state]( const member_record & member ) {
        return member.state == target_state;
    } ) );
}

int site_record::count_live_members() const
{
    return static_cast<int>( std::count_if( members.begin(), members.end(), []( const member_record & member ) {
        return counts_toward_live_headcount( member.state );
    } ) );
}

int camp_supply_living_total( const site_record &site )
{
    return std::max( std::max( 0, site.headcount ), site.count_live_members() );
}

int camp_supply_cap( const site_record &site )
{
    const long long capacity = static_cast<long long>( camp_supply_days_at_capacity ) *
                               std::max( 1, camp_supply_living_total( site ) );
    return static_cast<int>( std::min<long long>( max_camp_supply_units, capacity ) );
}

bool advance_camp_supply( site_record &site, const int now_minutes )
{
    if( now_minutes < 0 || ( site.supply_last_update_minutes >= 0 &&
                             now_minutes < site.supply_last_update_minutes ) ) {
        return false;
    }

    const int previous_units = site.supply_units;
    const int previous_update = site.supply_last_update_minutes;
    const int previous_accounted = site.supply_accounted_living_total;
    const int previous_remainder = site.supply_member_minute_remainder;
    const int current_living_total = camp_supply_living_total( site );
    site.supply_units = std::clamp( site.supply_units, 0, camp_supply_cap( site ) );
    site.supply_accounted_living_total = std::max( 0, site.supply_accounted_living_total );
    site.supply_member_minute_remainder = std::clamp( site.supply_member_minute_remainder, 0,
                                         minutes_per_member_day - 1 );

    if( site.supply_last_update_minutes < 0 ) {
        site.supply_last_update_minutes = now_minutes;
        site.supply_accounted_living_total = current_living_total;
        site.supply_member_minute_remainder = 0;
    } else {
        const long long elapsed_minutes = static_cast<long long>( now_minutes ) -
                                          site.supply_last_update_minutes;
        const long long member_minutes = site.supply_member_minute_remainder +
                                         elapsed_minutes * site.supply_accounted_living_total;
        const long long consumed_units = member_minutes / minutes_per_member_day;
        site.supply_units = static_cast<int>( std::max<long long>( 0,
                            static_cast<long long>( site.supply_units ) - consumed_units ) );
        site.supply_member_minute_remainder = static_cast<int>(
                    member_minutes % minutes_per_member_day );
        site.supply_last_update_minutes = now_minutes;
        site.supply_accounted_living_total = current_living_total;
        site.supply_units = std::min( site.supply_units, camp_supply_cap( site ) );
    }

    return site.supply_units != previous_units ||
           site.supply_last_update_minutes != previous_update ||
           site.supply_accounted_living_total != previous_accounted ||
           site.supply_member_minute_remainder != previous_remainder;
}

int advance_world_camp_supplies( world_state &state, const int now_minutes )
{
    int sites_changed = 0;
    for( site_record &site : state.sites ) {
        if( advance_camp_supply( site, now_minutes ) ) {
            sites_changed++;
        }
    }
    return sites_changed;
}

int site_record::active_outing_survivor_count() const
{
    const active_outing_state *outing = active_external_outing();
    if( outing == nullptr ) {
        return 0;
    }
    return static_cast<int>( std::count_if( outing->member_ids.begin(),
    outing->member_ids.end(), [this, outing]( const character_id & member_id ) {
        if( outing->member_is_resolved( member_id ) ) {
            return false;
        }
        if( std::find( outing->casualty_ids.begin(), outing->casualty_ids.end(), member_id ) !=
            outing->casualty_ids.end() ) {
            return false;
        }
        const member_record *member = find_member( member_id );
        return member != nullptr && member->state != member_state::dead &&
               member->state != member_state::missing;
    } ) );
}

int site_record::count_home_side_signals() const
{
    int home_side_signals = count_members_in_state( member_state::at_home );
    home_side_signals += std::max( 0, headcount );
    for( const spawn_tile_record &spawn_tile : spawn_tiles ) {
        home_side_signals += std::max( 0, spawn_tile.headcount );
    }
    return home_side_signals;
}

int site_record::dispatchable_member_capacity() const
{
    if( retired_empty_site ) {
        return 0;
    }
    const int ready_at_home_members = static_cast<int>( std::count_if( members.begin(), members.end(),
    [this]( const member_record & member ) {
        const bool reserved_for_hostile_operation = active_hostile_operation.is_active() &&
                std::find( active_hostile_operation.reservation.member_ids.begin(),
                           active_hostile_operation.reservation.member_ids.end(), member.npc_id ) !=
                active_hostile_operation.reservation.member_ids.end();
        return member.state == member_state::at_home && !member.wounded_or_unready &&
               !reserved_for_hostile_operation;
    } ) );
    return std::max( 0, ready_at_home_members - required_home_reserve( *this ) );
}

bool site_record::has_active_outside_pressure() const
{
    return active_outing.is_active() || !active_outing.member_ids.empty() ||
           active_hostile_operation.is_active() ||
           !active_hostile_operation.reservation.member_ids.empty() ||
           count_members_in_state( member_state::outbound ) > 0 ||
           count_members_in_state( member_state::local_contact ) > 0;
}

active_outing_state *site_record::active_external_outing()
{
    const bool active_outing_present = active_outing.is_active() ||
                                       !active_outing.member_ids.empty();
    const bool hostile_operation_present = active_hostile_operation.is_active() ||
            !active_hostile_operation.reservation.member_ids.empty();
    if( active_outing_present == hostile_operation_present ) {
        return nullptr;
    }
    if( active_outing_present ) {
        return &active_outing;
    }
    if( hostile_operation_present ) {
        return &active_hostile_operation.reservation;
    }
    return nullptr;
}

const active_outing_state *site_record::active_external_outing() const
{
    const bool active_outing_present = active_outing.is_active() ||
                                       !active_outing.member_ids.empty();
    const bool hostile_operation_present = active_hostile_operation.is_active() ||
            !active_hostile_operation.reservation.member_ids.empty();
    if( active_outing_present == hostile_operation_present ) {
        return nullptr;
    }
    if( active_outing_present ) {
        return &active_outing;
    }
    if( hostile_operation_present ) {
        return &active_hostile_operation.reservation;
    }
    return nullptr;
}

std::optional<simulation_advance_cursor> current_external_simulation_cursor(
    const site_record &site )
{
    const active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr || !outing->is_active() ||
        !simulation_owner_state_is_consistent( *outing ) ) {
        return std::nullopt;
    }
    simulation_advance_cursor cursor;
    cursor.activity_id = outing->activity_id;
    cursor.generation = outing->generation;
    cursor.owner = outing->owner;
    cursor.handoff_epoch = outing->handoff_epoch;
    cursor.last_advanced_minutes = outing->last_advanced_minutes;
    return cursor;
}

bool site_record::eligible_for_empty_site_retirement() const
{
    return !retired_empty_site && site_kind != owned_site_kind::none &&
           profile_for_site_kind( site_kind ) != hostile_site_profile::none &&
           count_home_side_signals() == 0 && !has_active_outside_pressure();
}

void world_state::clear()
{
    schema_version = 4;
    owner_id = "hells_raiders_live_owner_v0";
    sites.clear();
    finite_resources.clear();
}

void world_state::serialize( JsonOut &json ) const
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::world_serialize );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::world_serialize_calls );
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "owner_id", owner_id );
    json.member( "sites", sites );
    if( !finite_resources.empty() ) {
        json.member( "finite_resources" );
        json.start_array();
        for( const auto &entry : finite_resources ) {
            json.start_array();
            json.write( entry.first.x() );
            json.write( entry.first.y() );
            json.write( entry.first.z() );
            json.write( entry.second.remaining_units );
            json.write( entry.second.revision );
            json.end_array();
        }
        json.end_array();
    }
    json.end_object();
}

void world_state::deserialize( const JsonObject &jo )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::world_deserialize );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::world_deserialize_calls );
    world_state candidate;
    int loaded_schema_version = 0;
    jo.read( "schema_version", loaded_schema_version );
    jo.read( "owner_id", candidate.owner_id );
    jo.read( "sites", candidate.sites );
    if( jo.has_member( "finite_resources" ) ) {
        if( loaded_schema_version < 4 ) {
            jo.throw_error( "pre-v4 world cannot contain finite resource state" );
        }
        for( JsonArray packed_resource : jo.get_array( "finite_resources" ) ) {
            if( packed_resource.size() != 5 ) {
                packed_resource.throw_error( "finite resource must be [x,y,z,remaining,revision]" );
            }
            const tripoint_abs_omt omt( packed_resource.get_int( 0 ),
                                        packed_resource.get_int( 1 ),
                                        packed_resource.get_int( 2 ) );
            finite_resource_record resource;
            resource.remaining_units = packed_resource.get_int( 3 );
            resource.revision = packed_resource.get_int( 4 );
            if( !finite_resource_record_is_valid( resource ) ) {
                packed_resource.throw_error( "finite resource has invalid remaining units or revision" );
            }
            if( !candidate.finite_resources.emplace( omt, resource ).second ) {
                packed_resource.throw_error( "finite resource OMT is duplicated" );
            }
        }
    }
    if( loaded_schema_version < 4 ) {
        for( const site_record &site : candidate.sites ) {
            for( const camp_map_lead &lead : site.intelligence_map.leads ) {
                if( lead.kind == camp_lead_kind::structural_bounty &&
                    lead.status == camp_lead_status::harvested ) {
                    candidate.finite_resources.emplace( lead.omt, finite_resource_record { 0, 1 } );
                }
            }
        }
    }
    candidate.schema_version = 4;
    *this = std::move( candidate );
}

site_record *world_state::find_site( const std::string &site_id )
{
    auto iter = std::find_if( sites.begin(), sites.end(), [&site_id]( const site_record & site ) {
        return site.site_id == site_id;
    } );
    return iter != sites.end() ? &*iter : nullptr;
}

const site_record *world_state::find_site( const std::string &site_id ) const
{
    auto iter = std::find_if( sites.begin(), sites.end(), [&site_id]( const site_record & site ) {
        return site.site_id == site_id;
    } );
    return iter != sites.end() ? &*iter : nullptr;
}

const finite_resource_record *world_state::find_finite_resource(
    const tripoint_abs_omt &omt ) const
{
    const auto iter = finite_resources.find( omt );
    return iter != finite_resources.end() ? &iter->second : nullptr;
}

finite_resource_record finite_resource_snapshot( const world_state &state,
        const tripoint_abs_omt &omt, const int undiscovered_units )
{
    if( const finite_resource_record *resource = state.find_finite_resource( omt ) ) {
        return *resource;
    }
    return finite_resource_record { std::clamp( undiscovered_units, 0,
                                    max_finite_resource_units ), 0 };
}

finite_resource_claim_result claim_finite_resource_units( world_state &state,
        const tripoint_abs_omt &omt, const finite_resource_record &expected,
        const int requested_units )
{
    finite_resource_claim_result result;
    if( requested_units <= 0 || requested_units > max_finite_resource_claim_units ||
        expected.remaining_units < 0 || expected.remaining_units > max_finite_resource_units ||
        expected.revision < 0 || expected.revision > max_finite_resource_units ) {
        return result;
    }

    const auto current = state.finite_resources.find( omt );
    if( current != state.finite_resources.end() ) {
        result.remaining_units = current->second.remaining_units;
        result.revision = current->second.revision;
        if( !finite_resource_record_is_valid( current->second ) ) {
            return result;
        }
        if( current->second.remaining_units != expected.remaining_units ||
            current->second.revision != expected.revision ) {
            result.status = finite_resource_claim_status::stale;
            return result;
        }
    } else if( expected.revision != 0 ) {
        result.status = finite_resource_claim_status::stale;
        return result;
    }

    if( expected.remaining_units == 0 ) {
        result.status = finite_resource_claim_status::depleted;
        return result;
    }

    const int claimed_units = std::min( requested_units, expected.remaining_units );
    finite_resource_record updated;
    updated.remaining_units = expected.remaining_units - claimed_units;
    updated.revision = expected.revision + 1;
    if( !finite_resource_record_is_valid( updated ) ) {
        return result;
    }

    if( current == state.finite_resources.end() ) {
        state.finite_resources.emplace( omt, updated );
    } else {
        current->second = updated;
    }
    state.schema_version = 4;
    result.status = finite_resource_claim_status::applied;
    result.claimed_units = claimed_units;
    result.remaining_units = updated.remaining_units;
    result.revision = updated.revision;
    return result;
}

bool is_tracked_hostile_template( const std::string &npc_template_id )
{
    static const std::array<std::string, 9> tracked_templates = {
        "bandit",
        "thug",
        "bandit_trader",
        "bandit_quartermaster",
        "bandit_mechanic",
        "hells_raiders_boss",
        "cannibal_hunter",
        "cannibal_butcher",
        "cannibal_camp_leader",
    };

    return std::find( tracked_templates.begin(), tracked_templates.end(),
                      npc_template_id ) != tracked_templates.end();
}

std::optional<owned_site_kind> classify_tracked_source( anchor_source_kind source_kind,
        const std::string &source_id )
{
    switch( source_kind ) {
        case anchor_source_kind::overmap_special:
            if( source_id == "bandit_camp" ) {
                return owned_site_kind::bandit_camp;
            }
            if( source_id == "bandit_work_camp" ) {
                return owned_site_kind::bandit_work_camp;
            }
            if( source_id == "bandit_cabin" ) {
                return owned_site_kind::bandit_cabin;
            }
            if( source_id == "cannibal_camp" ) {
                return owned_site_kind::cannibal_camp;
            }
            break;
        case anchor_source_kind::map_extra:
            if( source_id == "mx_looters" ) {
                return owned_site_kind::looters;
            }
            if( source_id == "mx_bandits_block" ) {
                return owned_site_kind::bandits_block;
            }
            break;
        case anchor_source_kind::none:
            break;
    }

    return std::nullopt;
}

hostile_site_profile profile_for_site_kind( owned_site_kind site_kind )
{
    switch( site_kind ) {
        case owned_site_kind::bandit_camp:
        case owned_site_kind::bandit_work_camp:
        case owned_site_kind::bandit_cabin:
            return hostile_site_profile::camp_style;
        case owned_site_kind::cannibal_camp:
            return hostile_site_profile::cannibal_camp;
        case owned_site_kind::looters:
        case owned_site_kind::bandits_block:
            return hostile_site_profile::small_hostile_site;
        case owned_site_kind::none:
            return hostile_site_profile::none;
    }

    return hostile_site_profile::none;
}

footprint_snapshot make_special_footprint( const std::string &special_id,
        const tripoint_abs_omt &origin,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup )
{
    footprint_snapshot snapshot;
    snapshot.anchor = origin;
    snapshot.footprint.push_back( origin );

    if( !special_lookup ) {
        return snapshot;
    }

    snapshot.footprint.clear();
    const int radius = special_footprint_radius( special_id );
    for( const int z : special_footprint_z_levels( origin ) ) {
        for( int dx = -radius; dx <= radius; dx++ ) {
            for( int dy = -radius; dy <= radius; dy++ ) {
                const tripoint_abs_omt candidate( origin.x() + dx, origin.y() + dy, z );
                if( special_lookup( candidate ) == std::optional<std::string>( special_id ) ) {
                    snapshot.footprint.push_back( candidate );
                }
            }
        }
    }

    if( snapshot.footprint.empty() ) {
        snapshot.footprint.push_back( origin );
    }

    std::sort( snapshot.footprint.begin(), snapshot.footprint.end(), omt_less );
    snapshot.footprint.erase( std::unique( snapshot.footprint.begin(), snapshot.footprint.end() ),
                              snapshot.footprint.end() );
    snapshot.anchor = snapshot.footprint.front();
    return snapshot;
}

std::string make_site_id( anchor_source_kind source_kind, const std::string &source_id,
                          const tripoint_abs_omt &anchor )
{
    std::ostringstream out;
    out << to_string( source_kind ) << ':' << source_id << '@'
        << anchor.x() << ',' << anchor.y() << ',' << anchor.z();
    return out.str();
}

int abstract_roster_seed_for_site_kind( owned_site_kind site_kind )
{
    switch( site_kind ) {
        case owned_site_kind::bandit_camp:
        case owned_site_kind::bandit_work_camp:
            return 6;
        case owned_site_kind::cannibal_camp:
            return 5;
        case owned_site_kind::bandit_cabin:
            return 3;
        case owned_site_kind::looters:
        case owned_site_kind::bandits_block:
            return 2;
        case owned_site_kind::none:
            return 0;
    }

    return 0;
}

bool register_abstract_site( world_state &state, anchor_source_kind source_kind,
                             const std::string &source_id, const tripoint_abs_omt &origin,
                             const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup,
                             int abstract_headcount )
{
    const std::optional<owned_site_kind> site_kind = classify_tracked_source( source_kind, source_id );
    if( !site_kind ) {
        return false;
    }

    footprint_snapshot footprint;
    if( source_kind == anchor_source_kind::overmap_special ) {
        footprint = make_special_footprint( source_id, origin, special_lookup );
    } else {
        footprint.anchor = origin;
        footprint.footprint = { origin };
    }

    const std::string site_id = make_site_id( source_kind, source_id, footprint.anchor );
    site_record *site = state.find_site( site_id );
    if( site == nullptr ) {
        site_record new_site;
        new_site.site_id = site_id;
        new_site.source_kind = source_kind;
        new_site.site_kind = *site_kind;
        new_site.profile = profile_for_site_kind( *site_kind );
        new_site.source_id = source_id;
        new_site.anchor = footprint.anchor;
        new_site.footprint = footprint.footprint;
        new_site.headcount = std::max( 0, abstract_headcount );
        seed_uninitialized_camp_supply( new_site );
        state.sites.push_back( new_site );
        return true;
    }

    if( site->retired_empty_site ) {
        return false;
    }

    if( site->footprint.size() < footprint.footprint.size() ) {
        site->footprint = footprint.footprint;
        site->anchor = footprint.anchor;
    }
    if( site->source_kind == anchor_source_kind::none ) {
        site->source_kind = source_kind;
    }
    if( site->site_kind == owned_site_kind::none ) {
        site->site_kind = *site_kind;
    }
    if( site->profile == hostile_site_profile::none ) {
        site->profile = profile_for_site_kind( site->site_kind );
    }
    if( site->source_id.empty() ) {
        site->source_id = source_id;
    }
    site->headcount = std::max( site->headcount, std::max( 0, abstract_headcount ) );
    seed_uninitialized_camp_supply( *site );
    return true;
}

abstract_bootstrap_result register_abstract_sites_near(
    world_state &state, const tripoint_abs_omt &center, int radius_omt,
    const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup )
{
    abstract_bootstrap_result result;
    if( !special_lookup || radius_omt < 0 ) {
        return result;
    }

    for( int dx = -radius_omt; dx <= radius_omt; ++dx ) {
        for( int dy = -radius_omt; dy <= radius_omt; ++dy ) {
            const tripoint_abs_omt candidate( center.x() + dx, center.y() + dy, center.z() );
            if( rl_dist( center, candidate ) > radius_omt ) {
                continue;
            }
            const std::optional<std::string> source_id = special_lookup( candidate );
            if( !source_id ) {
                continue;
            }
            const std::optional<owned_site_kind> site_kind = classify_tracked_source(
                        anchor_source_kind::overmap_special, *source_id );
            if( !site_kind ) {
                continue;
            }
            result.recognized_tiles++;
            const size_t old_site_count = state.sites.size();
            register_abstract_site( state, anchor_source_kind::overmap_special, *source_id, candidate,
                                    special_lookup, abstract_roster_seed_for_site_kind( *site_kind ) );
            if( state.sites.size() > old_site_count ) {
                result.created_sites++;
            }
        }
    }

    return result;
}

bool claim_tracked_spawn( world_state &state, const std::string &npc_template_id,
                          character_id npc_id, const tripoint_abs_ms &spawn_tile,
                          const std::optional<std::string> &overmap_special_id,
                          const std::optional<std::string> &map_extra_id,
                          const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup )
{
    if( !is_tracked_hostile_template( npc_template_id ) ) {
        return false;
    }

    anchor_source_kind source_kind = anchor_source_kind::none;
    std::string source_id;
    std::optional<owned_site_kind> site_kind;

    if( overmap_special_id ) {
        site_kind = classify_tracked_source( anchor_source_kind::overmap_special, *overmap_special_id );
        if( site_kind ) {
            source_kind = anchor_source_kind::overmap_special;
            source_id = *overmap_special_id;
        }
    }

    if( !site_kind && map_extra_id ) {
        site_kind = classify_tracked_source( anchor_source_kind::map_extra, *map_extra_id );
        if( site_kind ) {
            source_kind = anchor_source_kind::map_extra;
            source_id = *map_extra_id;
        }
    }

    if( !site_kind ) {
        return false;
    }

    const tripoint_abs_omt spawn_omt = project_to<coords::omt>( spawn_tile );
    footprint_snapshot footprint;
    if( source_kind == anchor_source_kind::overmap_special ) {
        footprint = make_special_footprint( source_id, spawn_omt, special_lookup );
    } else {
        footprint.anchor = spawn_omt;
        footprint.footprint = { spawn_omt };
    }

    const std::string site_id = make_site_id( source_kind, source_id, footprint.anchor );
    site_record *site = state.find_site( site_id );
    if( site == nullptr ) {
        site_record new_site;
        new_site.site_id = site_id;
        new_site.source_kind = source_kind;
        new_site.site_kind = *site_kind;
        new_site.profile = profile_for_site_kind( *site_kind );
        new_site.source_id = source_id;
        new_site.anchor = footprint.anchor;
        new_site.footprint = footprint.footprint;
        state.sites.push_back( new_site );
        site = &state.sites.back();
    } else if( site->footprint.size() < footprint.footprint.size() ) {
        site->footprint = footprint.footprint;
        site->anchor = footprint.anchor;
    }
    if( site->profile == hostile_site_profile::none ) {
        site->profile = profile_for_site_kind( site->site_kind );
    }
    if( site->retired_empty_site ) {
        site->retired_empty_site = false;
        site->retirement_summary = "reactivated by tracked hostile spawn " + std::to_string( npc_id.get_value() );
    }

    if( site->has_member( npc_id ) ) {
        seed_uninitialized_camp_supply( *site );
        return true;
    }

    member_record member;
    member.npc_id = npc_id;
    member.npc_template_id = npc_template_id;
    member.home_spawn_tile = spawn_tile;
    site->members.push_back( member );
    site->headcount = std::max( site->headcount, site->count_live_members() );

    spawn_tile_record *spawn_tile_record_ptr = site->find_spawn_tile( spawn_tile );
    if( spawn_tile_record_ptr == nullptr ) {
        spawn_tile_record new_spawn_tile;
        new_spawn_tile.tile = spawn_tile;
        site->spawn_tiles.push_back( new_spawn_tile );
        spawn_tile_record_ptr = &site->spawn_tiles.back();
    }
    spawn_tile_record_ptr->headcount++;
    seed_uninitialized_camp_supply( *site );
    return true;
}


static int ready_at_home_member_count( const bandit_live_world::site_record &site )
{
    return static_cast<int>( std::count_if( site.members.begin(), site.members.end(),
    []( const bandit_live_world::member_record & member ) {
        return member.state == member_state::at_home && !member.wounded_or_unready;
    } ) );
}

static int wounded_or_unready_member_count( const bandit_live_world::site_record &site )
{
    return static_cast<int>( std::count_if( site.members.begin(), site.members.end(),
    []( const bandit_live_world::member_record & member ) {
        return counts_toward_live_headcount( member.state ) && member.wounded_or_unready;
    } ) );
}

static int active_outside_member_count( const bandit_live_world::site_record &site )
{
    std::vector<character_id> outside_members;
    outside_members.reserve( site.active_outing.member_ids.size() );
    for( const character_id &member_id : site.active_outing.member_ids ) {
        if( site.active_outing.member_is_resolved( member_id ) ) {
            continue;
        }
        if( std::find( outside_members.begin(), outside_members.end(), member_id ) == outside_members.end() ) {
            outside_members.push_back( member_id );
        }
    }
    for( const bandit_live_world::member_record &member : site.members ) {
        if( member.state != member_state::outbound && member.state != member_state::local_contact ) {
            continue;
        }
        if( std::find( outside_members.begin(), outside_members.end(), member.npc_id ) == outside_members.end() ) {
            outside_members.push_back( member.npc_id );
        }
    }
    return static_cast<int>( outside_members.size() );
}

static int ceil_percent( const int value, const int percent )
{
    if( value <= 0 || percent <= 0 ) {
        return 0;
    }
    return ( value * percent + 99 ) / 100;
}

static int camp_map_home_reserve_for_lead( const bandit_live_world::site_record &site,
                                    const bandit_live_world::camp_map_lead &lead,
                                    const int stockpile_pressure )
{
    int reserve = required_home_reserve( site );
    const int living_roster = site.count_live_members();
    const bool scout_confirmed_buddy_camp = effective_profile( site ) == hostile_site_profile::camp_style &&
            living_roster == 2 && lead.status == bandit_live_world::camp_lead_status::scout_confirmed;
    if( scout_confirmed_buddy_camp ) {
        reserve = 0;
    }
    if( effective_profile( site ) == hostile_site_profile::camp_style && !scout_confirmed_buddy_camp &&
        ( lead.prior_bandit_losses > 0 || lead.target_alert || lead.scout_seen ) ) {
        reserve += 1;
    }
    if( stockpile_pressure >= 3 ) {
        const int minimum_reserve = scout_confirmed_buddy_camp ? 0 : living_roster >= 5 ? 2 : 1;
        reserve = std::max( minimum_reserve, reserve - 1 );
    }
    return std::clamp( reserve, 0, living_roster );
}

static int stalk_pressure_member_count( const int living_roster, const int dispatchable )
{
    if( dispatchable < 2 ) {
        return 0;
    }
    if( living_roster == 2 ) {
        return 2;
    }
    const int upper_bound = std::min( dispatchable, ceil_percent( living_roster, 35 ) );
    if( upper_bound < 2 ) {
        return 0;
    }
    return std::clamp( ceil_percent( dispatchable, 40 ), 2, upper_bound );
}

static int toll_pressure_member_count( const int living_roster, const int dispatchable )
{
    if( dispatchable < 4 || living_roster < 7 ) {
        return 0;
    }
    const int upper_bound = std::min( dispatchable, ceil_percent( living_roster, 40 ) );
    if( upper_bound < 2 ) {
        return 0;
    }
    return std::clamp( ceil_percent( dispatchable, 45 ), 2, upper_bound );
}

static int cannibal_attack_member_count( const int living_roster, const int dispatchable,
                                  const int margin )
{
    if( dispatchable < 2 ) {
        return 0;
    }
    if( margin >= 6 ) {
        return dispatchable;
    }
    const int percent = margin >= 4 ? 60 : 45;
    const int upper_bound = std::min( dispatchable, ceil_percent( living_roster, percent ) );
    if( upper_bound < 2 ) {
        return 0;
    }
    return std::clamp( upper_bound, 2, dispatchable );
}

camp_map_dispatch_decision choose_camp_map_dispatch( const site_record &site,
        const camp_map_lead &lead, const camp_map_dispatch_pressure &pressure )
{
    camp_map_dispatch_decision decision;
    decision.valid = true;
    decision.living_roster = site.count_live_members();
    decision.ready_at_home = ready_at_home_member_count( site );
    decision.wounded_or_unready = wounded_or_unready_member_count( site );
    decision.active_outside = active_outside_member_count( site );
    decision.hard_home_reserve = camp_map_home_reserve_for_lead( site, lead,
                                 pressure.stockpile_pressure );
    decision.dispatchable = std::max( 0, decision.ready_at_home - decision.hard_home_reserve );

    decision.reward_score = std::max( 0, lead.bounty ) + std::max( 0, lead.confidence ) +
                            std::clamp( pressure.stockpile_pressure, 0, 3 ) +
                            std::min( 2, std::max( 0, lead.prior_defender_losses ) );
    if( pressure.opening_available ) {
        decision.reward_score += 1;
    }

    decision.risk_score = std::max( 0, lead.threat ) + std::max( 0, 2 - lead.confidence ) +
                          std::min( 4, std::max( 0, lead.prior_bandit_losses ) * 2 );
    if( lead.target_alert ) {
        decision.risk_score += 2;
    }
    if( lead.scout_seen ) {
        decision.risk_score += 1;
    }
    if( !pressure.opening_available ) {
        decision.risk_score += 2;
    }
    decision.margin = decision.reward_score - decision.risk_score;

    decision.notes.push_back( "camp-map decision uses ready roster, wounded/unready, reserve, and lead pressure" );
    decision.notes.push_back( "camp-map roster living=" + std::to_string( decision.living_roster ) +
                              " ready_at_home=" + std::to_string( decision.ready_at_home ) +
                              " wounded_or_unready=" + std::to_string( decision.wounded_or_unready ) +
                              " active_outside=" + std::to_string( decision.active_outside ) +
                              " reserve=" + std::to_string( decision.hard_home_reserve ) +
                              " dispatchable=" + std::to_string( decision.dispatchable ) );
    if( pressure.stockpile_pressure >= 3 ) {
        decision.notes.push_back( "stockpile pressure may loosen reserve by one but cannot cross hard minimum" );
    }
    if( effective_profile( site ) == hostile_site_profile::camp_style && decision.living_roster == 2 &&
        lead.status == camp_lead_status::scout_confirmed && decision.hard_home_reserve == 0 ) {
        decision.notes.push_back(
            "two-bandit camp: scout-confirmed pressure commits the buddy pair instead of preserving reserve" );
    }
    decision.notes.push_back( "camp-map opening_state=" + pressure.opening_state +
                              " opening_available=" + std::string( pressure.opening_available ? "yes" : "no" ) );
    if( lead.prior_bandit_losses > 0 || lead.target_alert || lead.scout_seen ) {
        decision.notes.push_back( "losses/alert add caution before greed can size the outing" );
    }

    if( site.retired_empty_site ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: retired empty site" );
        return decision;
    }
    if( site.has_active_outside_pressure() ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: unresolved active outside group/contact blocks dogpile" );
        return decision;
    }
    if( decision.dispatchable <= 0 ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: no ready members remain after reserve" );
        return decision;
    }

    if( !pressure.opening_available && lead.status == camp_lead_status::active ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: active stalk pressure found no opening and should return/decay" );
        return decision;
    }

    if( lead.confidence <= 1 || lead.status == camp_lead_status::stale ) {
        decision.intent = bandit_dry_run::job_template::scout;
        decision.selected_member_count = 1;
        decision.notes.push_back( "scout: low-confidence or stale memory needs eyes before pressure" );
        return decision;
    }

    if( decision.margin <= -2 || ( lead.threat >= lead.bounty + 2 && decision.margin <= 1 ) ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: high threat or poor reward does not escalate by itself" );
        return decision;
    }

    if( decision.margin >= 2 ) {
        const hostile_site_profile profile = effective_profile( site );
        const bool scout_confirmed_basecamp = lead.status == camp_lead_status::scout_confirmed &&
                lead.kind == camp_lead_kind::basecamp_activity;
        if( profile == hostile_site_profile::cannibal_camp && scout_confirmed_basecamp ) {
            const int attackers = cannibal_attack_member_count( decision.living_roster,
                                  decision.dispatchable, decision.margin );
            if( attackers >= 2 ) {
                decision.intent = bandit_dry_run::job_template::raid;
                decision.selected_member_count = attackers;
                decision.notes.push_back(
                    "raid: scout-confirmed basecamp lead lets cannibal camp commit an attack pack instead of another scout" );
                return decision;
            }
            decision.intent = bandit_dry_run::job_template::scout;
            decision.selected_member_count = 1;
            decision.notes.push_back(
                "scout: cannibal camp lacks an at-home attack pack after reserve, so it does not dogpile" );
            return decision;
        }

        if( profile == hostile_site_profile::camp_style && scout_confirmed_basecamp ) {
            const int toll_members = toll_pressure_member_count( decision.living_roster,
                                     decision.dispatchable );
            if( toll_members >= 2 ) {
                decision.intent = bandit_dry_run::job_template::toll;
                decision.selected_member_count = toll_members;
                decision.notes.push_back(
                    "toll: scout-confirmed basecamp lead promotes to a shakedown-capable party" );
                return decision;
            }
        }

        const int stalkers = stalk_pressure_member_count( decision.living_roster, decision.dispatchable );
        if( stalkers >= 2 ) {
            decision.intent = bandit_dry_run::job_template::stalk;
            decision.selected_member_count = stalkers;
            decision.notes.push_back( "stalk: remembered high-value lead permits larger-than-scout pressure" );
            return decision;
        }
        decision.intent = bandit_dry_run::job_template::scout;
        decision.selected_member_count = 1;
        decision.notes.push_back( "scout: pressure margin is good but reserve leaves no stalk pair" );
        return decision;
    }

    decision.intent = bandit_dry_run::job_template::hold_chill;
    decision.notes.push_back( "hold: marginal remembered lead waits for better evidence" );
    return decision;
}

const camp_map_lead *find_camp_map_dispatch_lead_for_target( const site_record &site,
        const tripoint_abs_omt &target_omt,
        const std::string &target_id )
{
    const camp_map_lead *best_lead = nullptr;
    int best_distance = 0;
    int best_score = 0;
    for( const camp_map_lead &lead : site.intelligence_map.leads ) {
        if( lead.target_id.empty() && lead.lead_id.empty() ) {
            continue;
        }
        if( lead.status == camp_lead_status::invalidated ||
            lead.status == camp_lead_status::harvested ||
            lead.status == camp_lead_status::dangerous ) {
            continue;
        }

        const bool target_matches = !target_id.empty() &&
                                    ( lead.target_id == target_id || lead.lead_id == target_id );
        const int radius = std::max( 2, lead.radius_omt );
        const tripoint_abs_omt routed_target_omt = reachable_ground_dispatch_target( site, target_omt );
        const tripoint_abs_omt routed_lead_omt = reachable_ground_dispatch_target( site, lead.omt );
        const bool direct_omt_matches = lead.omt.z() == target_omt.z() &&
                                        rl_dist( lead.omt, target_omt ) <= radius;
        const bool routed_omt_matches = routed_lead_omt.z() == routed_target_omt.z() &&
                                        rl_dist( routed_lead_omt, routed_target_omt ) <= radius;
        const bool omt_matches = direct_omt_matches || routed_omt_matches;
        if( !target_matches && !omt_matches ) {
            continue;
        }

        const int distance = rl_dist( site.anchor, lead.omt );
        const int score = std::max( 0, lead.confidence ) + std::max( 0, lead.bounty ) -
                          std::max( 0, lead.threat );
        if( best_lead == nullptr || distance < best_distance ||
            ( distance == best_distance && score > best_score ) ) {
            best_lead = &lead;
            best_distance = distance;
            best_score = score;
        }
    }
    return best_lead;
}

structural_bounty_read classify_structural_bounty_terrain( const std::string &overmap_terrain_id )
{
    const std::string id = lowercase_copy( overmap_terrain_id );
    structural_bounty_read read;
    read.terrain_class = "open";
    read.summary = "no structural bounty";

    if( id.empty() || contains_any_token( id, { "open", "field", "meadow", "road", "empty" } ) ) {
        return read;
    }

    if( contains_any_token( id, { "forest", "woods", "wood", "swamp", "wetland" } ) ) {
        read.terrain_class = "forest";
        read.bounty = 1;
        read.confidence = 1;
        read.latent_threat = 0;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "low structural forest/woods bounty";
        return read;
    }

    if( contains_any_token( id, { "downtown", "city", "mall", "office_tower", "apartment" } ) ) {
        read.terrain_class = "town";
        read.bounty = 3;
        read.confidence = 1;
        read.latent_threat = 2;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "medium-high structural urban bounty with latent threat";
        return read;
    }

    if( contains_any_token( id, { "town", "house", "home", "farm", "cabin", "building",
                                  "shop", "store", "garage", "shelter" } ) ) {
        read.terrain_class = "town";
        read.bounty = 2;
        read.confidence = 1;
        read.latent_threat = 1;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "medium structural town/building bounty";
        return read;
    }

    return read;
}

std::string make_structural_bounty_lead_id( const std::string &site_id,
        const tripoint_abs_omt &omt, const std::string &terrain_class )
{
    std::ostringstream out;
    out << site_id << ":structural_bounty:" << omt.x() << ',' << omt.y() << ',' << omt.z()
        << ':' << ( terrain_class.empty() ? "unknown" : terrain_class );
    return out.str();
}

bool structural_bounty_memory_suppresses_refresh( const camp_intelligence_map &intelligence_map,
        const tripoint_abs_omt &omt, const std::string &terrain_class )
{
    const std::string terrain = terrain_class.empty() ? "unknown" : terrain_class;
    for( const camp_map_lead &lead : intelligence_map.leads ) {
        if( lead.omt != omt ) {
            continue;
        }
        if( lead.kind != camp_lead_kind::structural_bounty &&
            lead.kind != camp_lead_kind::harvested_site ) {
            continue;
        }
        if( !lead.target_id.empty() && lead.target_id != terrain ) {
            continue;
        }
        if( lead.status == camp_lead_status::harvested ||
            lead.status == camp_lead_status::dangerous ) {
            return true;
        }
    }
    return false;
}

static bool structural_bounty_scan_memory_suppresses_refresh( const camp_intelligence_map &intelligence_map,
        const tripoint_abs_omt &omt, const std::string &terrain_class, const int now_minutes )
{
    if( structural_bounty_memory_suppresses_refresh( intelligence_map, omt, terrain_class ) ) {
        return true;
    }

    constexpr int recent_structural_check_cooldown_minutes = 6 * 60;
    const std::string terrain = terrain_class.empty() ? "unknown" : terrain_class;
    for( const camp_map_lead &lead : intelligence_map.leads ) {
        if( lead.omt != omt || lead.kind != camp_lead_kind::structural_bounty ) {
            continue;
        }
        if( !lead.target_id.empty() && lead.target_id != terrain ) {
            continue;
        }
        if( lead.last_checked_minutes >= 0 && now_minutes >= 0 &&
            now_minutes - lead.last_checked_minutes < recent_structural_check_cooldown_minutes ) {
            return true;
        }
    }
    return false;
}

bool upsert_structural_bounty_lead( site_record &site, const tripoint_abs_omt &omt,
                                   const structural_bounty_read &read, const int now_minutes )
{
    if( !read.eligible || read.bounty <= 0 ) {
        return false;
    }
    if( structural_bounty_memory_suppresses_refresh( site.intelligence_map, omt,
            read.terrain_class ) ) {
        return false;
    }

    camp_map_lead lead;
    lead.lead_id = make_structural_bounty_lead_id( site.site_id, omt, read.terrain_class );
    lead.kind = camp_lead_kind::structural_bounty;
    lead.status = camp_lead_status::suspected;
    lead.target_id = read.terrain_class;
    lead.omt = omt;
    lead.radius_omt = read.radius_omt;
    lead.source_key = "structural_bounty:" + read.terrain_class;
    lead.source_summary = read.summary;
    lead.first_seen_minutes = now_minutes;
    lead.last_seen_minutes = now_minutes;
    lead.bounty = std::max( 0, read.bounty );
    lead.threat = 0;
    lead.confidence = std::max( 0, read.confidence );
    lead.threat_confirmed = false;
    lead.last_outcome = "structural_bounty_suspected";

    if( camp_map_lead *existing = site.intelligence_map.find_lead( lead.lead_id ) ) {
        lead.first_seen_minutes = existing->first_seen_minutes;
        lead.times_checked_empty = existing->times_checked_empty;
        lead.times_harvested = existing->times_harvested;
        *existing = lead;
        return true;
    }
    site.intelligence_map.leads.push_back( lead );
    return true;
}

bool record_camp_resource_estimate( site_record &site, const std::string &lead_id,
                                    const int estimated_units, const int confidence,
                                    const int observed_minutes )
{
    if( lead_id.empty() || estimated_units < 0 || estimated_units > max_finite_resource_units ||
        confidence < 0 || confidence > max_finite_resource_units || observed_minutes < 0 ) {
        return false;
    }
    camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    if( lead == nullptr || lead->kind != camp_lead_kind::structural_bounty ||
        observed_minutes <= lead->last_checked_minutes ||
        ( lead->status == camp_lead_status::harvested && estimated_units > 0 ) ) {
        return false;
    }

    lead->bounty = estimated_units;
    lead->confidence = confidence;
    lead->last_seen_minutes = std::max( lead->last_seen_minutes, observed_minutes );
    lead->last_checked_minutes = observed_minutes;
    lead->last_scouted_minutes = std::max( lead->last_scouted_minutes, observed_minutes );
    if( estimated_units == 0 ) {
        lead->status = camp_lead_status::harvested;
        lead->last_outcome = "physical_resource_depletion_observed";
    } else {
        lead->last_outcome = "physical_resource_estimate_updated";
    }
    return true;
}

structural_bounty_scan_result advance_structural_bounty_scan( world_state &state,
        const int now_minutes, const int scan_budget,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &terrain_lookup )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::structural_scan );
    structural_bounty_scan_result result;
    result.scan_budget = std::max( 0, scan_budget );
    if( result.scan_budget == 0 ) {
        result.notes.push_back( "structural scan skipped: zero budget" );
        return result;
    }
    if( !terrain_lookup ) {
        result.notes.push_back( "structural scan skipped: no terrain lookup" );
        return result;
    }

    static const std::array<std::pair<int, int>, 12> near_offsets = { {
            { -4, 0 }, { 4, 0 }, { 0, -4 }, { 0, 4 },
            { -5, -1 }, { 5, 1 }, { -1, 5 }, { 1, -5 },
            { -6, 0 }, { 6, 0 }, { 0, -6 }, { 0, 6 },
        } };
    constexpr int per_site_near_sample_cap = 4;
    constexpr int near_scan_cadence_minutes = 60;
    constexpr int near_scan_radius_omt = 8;
    const int time_bucket = now_minutes >= 0 ? now_minutes / near_scan_cadence_minutes : 0;

    for( site_record &site : state.sites ) {
        if( result.budget_used >= result.scan_budget ) {
            result.budget_exhausted = true;
            break;
        }

        result.sites_considered++;
        bandit_live_world_probe::increment(
            bandit_live_world_probe::counter::structural_scan_sites_considered );
        bandit_live_world_probe::record_site_service( site.site_id,
                bandit_live_world_probe::site_service::scan_considered );
        if( effective_profile( site ) != hostile_site_profile::camp_style ) {
            result.sites_skipped_not_camp++;
            bandit_live_world_probe::increment(
                bandit_live_world_probe::counter::structural_scan_sites_skipped_not_camp );
            continue;
        }
        if( site.retired_empty_site ) {
            result.sites_skipped_retired++;
            continue;
        }
        if( site.has_active_outside_pressure() ) {
            result.sites_skipped_active_outside++;
            continue;
        }
        const bool has_ready_home_presence = ready_at_home_member_count( site ) > 0 ||
                                             site.count_home_side_signals() > 0;
        if( !has_ready_home_presence ) {
            result.sites_skipped_no_ready_home++;
            continue;
        }
        if( site.intelligence_map.next_near_tick_minutes >= 0 &&
            now_minutes >= 0 && now_minutes < site.intelligence_map.next_near_tick_minutes ) {
            result.sites_deferred_by_cadence++;
            continue;
        }

        int samples_for_site = 0;
        const int rotation = static_cast<int>( ( static_cast<long long>( std::max( 0, time_bucket ) ) *
                                                per_site_near_sample_cap ) % near_offsets.size() );
        for( int offset_index = 0;
             offset_index < static_cast<int>( near_offsets.size() ) &&
             samples_for_site < per_site_near_sample_cap && result.budget_used < result.scan_budget;
             offset_index++ ) {
            const std::pair<int, int> &offset = near_offsets[( rotation + offset_index ) %
                                               near_offsets.size()];
            const tripoint_abs_omt candidate( site.anchor.x() + offset.first,
                                              site.anchor.y() + offset.second, site.anchor.z() );
            result.candidates_sampled++;
            result.budget_used++;
            samples_for_site++;
            bandit_live_world_probe::increment(
                bandit_live_world_probe::counter::structural_scan_candidates_sampled );
            bandit_live_world_probe::record_site_service( site.site_id,
                    bandit_live_world_probe::site_service::scan_samples );

            const std::optional<std::string> terrain_id = terrain_lookup( candidate );
            if( !terrain_id ) {
                continue;
            }
            const structural_bounty_read read = classify_structural_bounty_terrain( *terrain_id );
            if( !read.eligible || read.bounty <= 0 ) {
                continue;
            }
            if( structural_bounty_scan_memory_suppresses_refresh( site.intelligence_map, candidate,
                    read.terrain_class, now_minutes ) ) {
                result.leads_suppressed_by_memory++;
                continue;
            }
            if( upsert_structural_bounty_lead( site, candidate, read, now_minutes ) ) {
                result.leads_seeded++;
            }
        }

        if( samples_for_site > 0 ) {
            site.intelligence_map.known_radius_omt = std::max( site.intelligence_map.known_radius_omt,
                    near_scan_radius_omt );
            if( now_minutes >= 0 ) {
                site.intelligence_map.next_near_tick_minutes = now_minutes + near_scan_cadence_minutes;
            }
        }

        if( result.budget_used >= result.scan_budget ) {
            result.budget_exhausted = true;
        }
    }

    result.notes.push_back( "structural scan bounded to near-ring per-camp samples" );
    return result;
}

namespace
{
int structural_outing_stalking_delay_minutes( const site_record &site, const camp_map_lead &lead )
{
    const int distance = std::max( 1, rl_dist( site.anchor, lead.omt ) );
    return std::clamp( distance * 15, 30, 240 );
}

int structural_outing_arrival_delay_minutes( const site_record &site, const camp_map_lead &lead )
{
    const int distance = std::max( 1, rl_dist( site.anchor, lead.omt ) );
    return structural_outing_stalking_delay_minutes( site, lead ) + std::clamp( distance * 10, 30, 180 );
}

int structural_known_threat_for_interest( const camp_map_lead &lead )
{
    return lead.threat_confirmed ? std::max( 0, lead.threat ) : 0;
}

int structural_effective_interest( const camp_map_lead &lead, const int threat )
{
    return std::max( 0, lead.bounty ) + std::max( 0, lead.confidence ) - std::max( 0, threat );
}

bool structural_lead_recently_checked( const camp_map_lead &lead, const int now_minutes )
{
    constexpr int recent_structural_check_cooldown_minutes = 6 * 60;
    return lead.last_checked_minutes >= 0 && now_minutes >= 0 &&
           now_minutes - lead.last_checked_minutes < recent_structural_check_cooldown_minutes;
}

void clear_structural_active_group( site_record &site, const std::string &summary )
{
    for( const character_id &member_id : site.active_outing.member_ids ) {
        update_member_state( site, member_id, member_state::at_home, summary );
    }
    site.active_outing.clear();
}
} // namespace

structural_outing_plan plan_structural_bounty_outing( const site_record &site,
        const camp_map_lead &lead, const int now_minutes )
{
    structural_outing_plan plan;
    plan.site_id = site.site_id;
    plan.lead_id = lead.lead_id;
    plan.target_omt = lead.omt;
    plan.known_threat = structural_known_threat_for_interest( lead );
    plan.effective_interest = structural_effective_interest( lead, plan.known_threat );

    if( effective_profile( site ) != hostile_site_profile::camp_style ) {
        plan.notes.push_back( "structural outing blocked: only camp-style bandit sites run routine structural outings" );
        return plan;
    }
    if( site.retired_empty_site ) {
        plan.notes.push_back( "structural outing blocked: retired empty site" );
        return plan;
    }
    if( site.has_active_outside_pressure() ) {
        plan.notes.push_back( "structural outing blocked: active outside group/contact blocks dogpile" );
        return plan;
    }
    if( lead.kind != camp_lead_kind::structural_bounty ) {
        plan.notes.push_back( "structural outing blocked: lead is not structural bounty" );
        return plan;
    }
    if( lead.status == camp_lead_status::active || lead.status == camp_lead_status::harvested ||
        lead.status == camp_lead_status::dangerous || lead.status == camp_lead_status::invalidated ) {
        plan.notes.push_back( "structural outing blocked: lead status suppresses dispatch" );
        return plan;
    }
    if( lead.bounty <= 0 ) {
        plan.notes.push_back( "structural outing blocked: no remaining structural bounty" );
        return plan;
    }
    if( structural_lead_recently_checked( lead, now_minutes ) ) {
        plan.notes.push_back( "structural outing blocked: recently checked structural lead is cooling down" );
        return plan;
    }
    if( plan.effective_interest <= 0 ) {
        plan.notes.push_back( "structural outing blocked: known threat cancels bounty interest" );
        return plan;
    }

    const int reserve = camp_map_home_reserve_for_lead( site, lead, 0 );
    const int ready = ready_at_home_member_count( site );
    const int dispatchable = std::max( 0, ready - reserve );
    if( dispatchable <= 0 ) {
        plan.notes.push_back( "structural outing blocked: no ready member remains after home reserve" );
        return plan;
    }

    plan.job = lead.target_id == "forest" ? bandit_dry_run::job_template::scavenge :
               bandit_dry_run::job_template::scout;
    plan.member_ids = select_dispatch_members( site, 1 );
    if( plan.member_ids.empty() ) {
        plan.notes.push_back( "structural outing blocked: no selectable at-home member" );
        return plan;
    }
    plan.expected_stalking_minutes = now_minutes >= 0 ?
                                     now_minutes + structural_outing_stalking_delay_minutes( site, lead ) : -1;
    plan.expected_arrival_minutes = now_minutes >= 0 ?
                                    now_minutes + structural_outing_arrival_delay_minutes( site, lead ) : -1;
    plan.valid = true;
    plan.notes.push_back( "structural outing candidate=" + lead.lead_id +
                          " bounty=" + std::to_string( lead.bounty ) +
                          " known_threat=" + std::to_string( plan.known_threat ) +
                          " confidence=" + std::to_string( lead.confidence ) +
                          " effective_interest=" + std::to_string( plan.effective_interest ) +
                          " decision=" + bandit_dry_run::to_string( plan.job ) );
    plan.notes.push_back( "structural outing is non-player camp routine traffic, not pursuit handoff" );
    return plan;
}

structural_outing_plan plan_structural_bounty_outing( const site_record &site, const int now_minutes )
{
    structural_outing_plan best;
    for( const camp_map_lead &lead : site.intelligence_map.leads ) {
        structural_outing_plan candidate = plan_structural_bounty_outing( site, lead, now_minutes );
        if( !candidate.valid ) {
            continue;
        }
        const int candidate_distance = rl_dist( site.anchor, candidate.target_omt );
        const int best_distance = best.valid ? rl_dist( site.anchor, best.target_omt ) : 0;
        if( !best.valid || candidate.effective_interest > best.effective_interest ||
            ( candidate.effective_interest == best.effective_interest && candidate_distance < best_distance ) ) {
            best = candidate;
        }
    }
    if( !best.valid ) {
        best.site_id = site.site_id;
        best.notes.push_back( "structural outing planner found no eligible structural bounty lead" );
    }
    return best;
}

bool apply_structural_bounty_outing_plan( site_record &site, const structural_outing_plan &plan,
        const int now_minutes )
{
    if( !plan.valid || plan.site_id != site.site_id || plan.lead_id.empty() || plan.member_ids.empty() ||
        plan.member_ids.size() > max_active_outing_members ) {
        return false;
    }
    if( site.has_active_outside_pressure() ) {
        return false;
    }
    camp_map_lead *lead = site.intelligence_map.find_lead( plan.lead_id );
    if( lead == nullptr || lead->kind != camp_lead_kind::structural_bounty || lead->bounty <= 0 ||
        lead->status == camp_lead_status::active || lead->status == camp_lead_status::harvested ||
        lead->status == camp_lead_status::dangerous ||
        lead->status == camp_lead_status::invalidated ||
        structural_lead_recently_checked( *lead, now_minutes ) ) {
        return false;
    }
    if( structural_effective_interest( *lead, structural_known_threat_for_interest( *lead ) ) <= 0 ) {
        return false;
    }
    const int reserve = camp_map_home_reserve_for_lead( site, *lead, 0 );
    const int ready = ready_at_home_member_count( site );
    if( static_cast<int>( plan.member_ids.size() ) > std::max( 0, ready - reserve ) ) {
        return false;
    }
    std::vector<character_id> checked_member_ids;
    checked_member_ids.reserve( plan.member_ids.size() );
    for( const character_id &member_id : plan.member_ids ) {
        if( std::find( checked_member_ids.begin(), checked_member_ids.end(), member_id ) !=
            checked_member_ids.end() ) {
            return false;
        }
        const member_record *member = site.find_member( member_id );
        if( member == nullptr || member->state != member_state::at_home || member->wounded_or_unready ) {
            return false;
        }
        checked_member_ids.push_back( member_id );
    }

    const std::string summary = "structural " + bandit_dry_run::to_string( plan.job ) +
                                " outing toward " + plan.lead_id;
    for( const character_id &member_id : plan.member_ids ) {
        if( !update_member_state( site, member_id, member_state::outbound, summary ) ) {
            return false;
        }
    }
    site.active_outing.clear();
    site.active_outing.kind = outing_kind::structural_sortie;
    site.active_outing.activity_id = site.site_id + "#structural";
    site.active_outing.camp_id = site.site_id;
    site.active_outing.generation = site.next_outing_generation++;
    site.active_outing.member_ids = plan.member_ids;
    site.active_outing.leader_id = plan.member_ids.front();
    site.active_outing.phase = scout_phase::outbound;
    site.active_outing.owner = simulation_owner::abstract;
    site.active_outing.last_advanced_minutes = now_minutes;
    site.active_outing.target_id = plan.lead_id;
    site.active_outing.target_omt = plan.target_omt;
    site.active_outing.job_type = bandit_dry_run::to_string( plan.job );
    site.active_outing.started_minutes = now_minutes;
    site.active_outing.local_contact_minutes = -1;
    site.active_outing.last_progress_minutes = now_minutes;
    site.active_outing.return_application_key = site.active_outing.activity_id + ":return:" +
            std::to_string( site.active_outing.generation );
    site.active_outing.report_application_key = site.active_outing.activity_id + ":report:" +
            std::to_string( site.active_outing.generation );
    site.active_outing.cargo_application_key = site.active_outing.activity_id + ":cargo:" +
            std::to_string( site.active_outing.generation );
    lead->status = camp_lead_status::active;
    lead->last_outcome = "structural_outing_active";
    return true;
}

structural_outing_result advance_structural_bounty_outings( world_state &state, const int now_minutes,
        const std::function<structural_threat_read( const site_record &, const camp_map_lead & )> &threat_lookup )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::structural_outings );
    structural_outing_result result;
    for( site_record &site : state.sites ) {
        result.sites_considered++;
        bandit_live_world_probe::increment(
            bandit_live_world_probe::counter::structural_outing_sites_considered );
        bandit_live_world_probe::record_site_service( site.site_id,
                bandit_live_world_probe::site_service::outing_considered );
        if( site.active_outing.kind != outing_kind::structural_sortie ||
            site.active_outing.activity_id != site.site_id + "#structural" ||
            site.active_outing.target_id.empty() ) {
            continue;
        }
        result.active_outings_considered++;
        const simulation_owner_transition_result advance = advance_external_simulation(
                    site, site.active_outing.activity_id, site.active_outing.generation,
                    simulation_owner::abstract, site.active_outing.handoff_epoch,
                    site.active_outing.last_advanced_minutes, now_minutes );
        if( advance != simulation_owner_transition_result::applied ) {
            continue;
        }
        camp_map_lead *lead = site.intelligence_map.find_lead( site.active_outing.target_id );
        if( lead == nullptr || lead->kind != camp_lead_kind::structural_bounty ) {
            clear_structural_active_group( site, "structural outing cleared missing structural lead" );
            result.notes.push_back( "structural outing cleared: active target lead was missing" );
            continue;
        }
        if( site.active_outing.started_minutes < 0 ) {
            site.active_outing.started_minutes = now_minutes;
            site.active_outing.last_progress_minutes = now_minutes;
            continue;
        }

        const int elapsed = now_minutes - site.active_outing.started_minutes;
        if( site.active_outing.local_contact_minutes < 0 &&
            elapsed >= structural_outing_stalking_delay_minutes( site, *lead ) ) {
            structural_threat_read threat;
            if( threat_lookup ) {
                threat = threat_lookup( site, *lead );
            }
            lead->threat = std::max( 0, threat.threat );
            lead->threat_confirmed = true;
            lead->last_scouted_minutes = now_minutes;
            lead->last_checked_minutes = now_minutes;
            lead->source_summary = threat.summary.empty() ?
                                   "stalking-distance structural threat check" : threat.summary;
            site.active_outing.local_contact_minutes = now_minutes;
            site.active_outing.phase = scout_phase::observing;
            site.active_outing.last_progress_minutes = now_minutes;
            result.stalking_checks_processed++;

            const int effective_interest = structural_effective_interest( *lead, lead->threat );
            if( effective_interest <= 0 ) {
                const int returned = static_cast<int>( site.active_outing.member_ids.size() );
                lead->status = lead->threat > 0 ? camp_lead_status::dangerous : camp_lead_status::stale;
                lead->last_outcome = "threat_revealed_lost_interest";
                clear_structural_active_group( site,
                                               "structural outing turned back before arrival after threat reveal" );
                result.lost_interest_returns++;
                result.members_returned += returned;
                result.notes.push_back( "structural outing turned back before arrival lead=" +
                                        lead->lead_id + " effective_interest=" +
                                        std::to_string( effective_interest ) );
                continue;
            }

            lead->status = camp_lead_status::scout_confirmed;
            lead->last_outcome = "threat_revealed_interest_survives";
            result.notes.push_back( "structural outing stalking check kept arrival open lead=" +
                                    lead->lead_id + " effective_interest=" +
                                    std::to_string( effective_interest ) );
            continue;
        }

        if( site.active_outing.local_contact_minutes >= 0 &&
            elapsed >= structural_outing_arrival_delay_minutes( site, *lead ) ) {
            const int returned = static_cast<int>( site.active_outing.member_ids.size() );
            lead->status = camp_lead_status::harvested;
            lead->bounty = 0;
            lead->times_harvested++;
            lead->last_checked_minutes = now_minutes;
            lead->last_outcome = "harvested_structural_bounty";
            clear_structural_active_group( site,
                                           "structural outing arrived and harvested structural bounty" );
            result.arrivals_processed++;
            result.members_returned += returned;
            result.notes.push_back( "structural outing harvested lead=" + lead->lead_id );
        }
    }
    return result;
}

structural_bounty_maintenance_result advance_structural_bounty_maintenance( world_state &state,
        const int now_minutes, const int scan_budget, const int dispatch_cap,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &terrain_lookup,
        const std::function<structural_threat_read( const site_record &, const camp_map_lead & )> &threat_lookup )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::structural_maintenance );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::structural_maintenance_updates );
    structural_bounty_maintenance_result result;
    advance_world_camp_supplies( state, now_minutes );
    result.dispatch_cap = std::max( 0, dispatch_cap );
    result.outing = advance_structural_bounty_outings( state, now_minutes, threat_lookup );
    result.scan = advance_structural_bounty_scan( state, now_minutes, scan_budget, terrain_lookup );

    bandit_live_world_probe::scoped_section dispatch_probe_section(
        bandit_live_world_probe::section::structural_dispatch );

    if( result.dispatch_cap == 0 ) {
        result.notes.push_back( "structural maintenance dispatch skipped: zero cap" );
        return result;
    }

    for( site_record &site : state.sites ) {
        result.sites_considered_for_dispatch++;
        bandit_live_world_probe::increment(
            bandit_live_world_probe::counter::structural_dispatch_sites_considered );
        bandit_live_world_probe::record_site_service( site.site_id,
                bandit_live_world_probe::site_service::dispatch_considered );
        if( result.dispatches_applied >= result.dispatch_cap ) {
            result.dispatch_cap_reached = true;
            break;
        }
        const structural_outing_plan plan = plan_structural_bounty_outing( site, now_minutes );
        if( !plan.valid ) {
            continue;
        }
        result.dispatches_planned++;
        for( const std::string &note : plan.notes ) {
            result.notes.push_back( note );
        }
        if( apply_structural_bounty_outing_plan( site, plan, now_minutes ) ) {
            result.dispatches_applied++;
            result.notes.push_back( "structural maintenance dispatched site=" + site.site_id +
                                    " lead=" + plan.lead_id );
        } else {
            result.dispatches_blocked++;
            result.notes.push_back( "structural maintenance dispatch apply blocked site=" + site.site_id +
                                    " lead=" + plan.lead_id );
        }
    }

    return result;
}

std::string render_structural_bounty_maintenance_report(
    const structural_bounty_maintenance_result &result )
{
    std::ostringstream out;
    out << "bandit_live_world structural maintenance:"
        << " scan_budget=" << result.scan.scan_budget
        << " budget_used=" << result.scan.budget_used
        << " budget_exhausted=" << ( result.scan.budget_exhausted ? "yes" : "no" )
        << " sites_scanned=" << result.scan.sites_considered
        << " candidates_sampled=" << result.scan.candidates_sampled
        << " leads_seeded=" << result.scan.leads_seeded
        << " leads_suppressed=" << result.scan.leads_suppressed_by_memory
        << " dispatch_cap=" << result.dispatch_cap
        << " dispatches_planned=" << result.dispatches_planned
        << " dispatches_applied=" << result.dispatches_applied
        << " dispatch_cap_reached=" << ( result.dispatch_cap_reached ? "yes" : "no" )
        << " active_outings=" << result.outing.active_outings_considered
        << " stalking_checks=" << result.outing.stalking_checks_processed
        << " turnbacks=" << result.outing.lost_interest_returns
        << " arrivals=" << result.outing.arrivals_processed
        << " members_returned=" << result.outing.members_returned << '\n';
    for( const std::string &note : result.outing.notes ) {
        out << "- " << note << '\n';
    }
    for( const std::string &note : result.scan.notes ) {
        out << "- " << note << '\n';
    }
    for( const std::string &note : result.notes ) {
        out << "- " << note << '\n';
    }
    return out.str();
}

tripoint_abs_omt reachable_ground_dispatch_target( const site_record &site,
        const tripoint_abs_omt &target_omt )
{
    if( effective_profile( site ) == hostile_site_profile::none ) {
        return target_omt;
    }

    int route_z = site.anchor.z();
    for( const tripoint_abs_omt &foot : site.footprint ) {
        if( foot.z() == 0 ) {
            route_z = 0;
            break;
        }
    }
    if( route_z > 0 ) {
        route_z = 0;
    }

    return tripoint_abs_omt( target_omt.x(), target_omt.y(), route_z );
}

dispatch_plan plan_site_dispatch_from_camp_map_lead( const site_record &site,
        const camp_map_lead &lead,
        const camp_map_dispatch_pressure &pressure )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    dispatch_plan plan;
    plan.site_id = site.site_id;
    plan.profile = rules.profile;
    plan.target_id = lead.target_id.empty() ? lead.lead_id : lead.target_id;
    const tripoint_abs_omt original_target_omt = lead.omt;
    plan.target_omt = reachable_ground_dispatch_target( site, original_target_omt );

    if( plan.target_id.empty() ) {
        plan.notes.push_back( "camp-map dispatch blocked: missing remembered target id" );
        return plan;
    }

    const camp_map_dispatch_decision decision = choose_camp_map_dispatch( site, lead, pressure );
    plan.notes = decision.notes;
    plan.notes.push_back( "camp-map dispatch lead=" + ( lead.lead_id.empty() ? plan.target_id : lead.lead_id ) +
                          " status=" + to_string( lead.status ) +
                          " bounty=" + std::to_string( lead.bounty ) +
                          " threat=" + std::to_string( lead.threat ) +
                          " confidence=" + std::to_string( lead.confidence ) +
                          " selected=" + bandit_dry_run::to_string( decision.intent ) );
    if( decision.intent == bandit_dry_run::job_template::hold_chill ||
        decision.selected_member_count <= 0 ) {
        plan.notes.push_back( "camp-map dispatch blocked: remembered risk/reward decision held pressure" );
        return plan;
    }
    if( !camp_decision_allows_dispatch( site.camp_decision, decision.intent ) ) {
        plan.notes.push_back( "camp-map dispatch blocked: camp decision state " +
                              to_string( site.camp_decision.state ) );
        return plan;
    }

    plan.member_ids = select_dispatch_members( site, decision.selected_member_count );
    if( static_cast<int>( plan.member_ids.size() ) != decision.selected_member_count ) {
        plan.notes.push_back( "camp-map dispatch blocked: not enough at-home members survived selection" );
        return plan;
    }

    bandit_dry_run::candidate_debug winner = make_camp_map_dispatch_candidate( lead, decision );
    if( !bandit_pursuit_handoff::supports_pursuit_handoff( winner ) ) {
        plan.notes.push_back( "camp-map dispatch blocked: remembered decision stayed outside bounded scout/stalk handoff" );
        return plan;
    }

    plan.group = make_dispatch_group( site, plan.member_ids, plan.target_id );
    plan.group.current_threat_estimate = std::max( 0, lead.threat );
    plan.group.current_bounty_estimate = std::max( 0, lead.bounty );
    plan.group.confidence = std::max( plan.group.confidence, lead.confidence );
    bandit_pursuit_handoff::entry_context context;
    context.contact = rl_dist( site.anchor, plan.target_omt ) <= 4 ?
                      bandit_pursuit_handoff::contact_certainty::localized :
                      bandit_pursuit_handoff::contact_certainty::broad;
    plan.entry = bandit_pursuit_handoff::build_entry_payload( plan.group, winner, context );
    plan.notes = plan.entry.notes;
    if( plan.target_omt != original_target_omt ) {
        plan.notes.push_back( "vertical dispatch fallback: target " + original_target_omt.to_string() +
                              " routes via reachable ground " + plan.target_omt.to_string() );
    }
    if( !plan.entry.valid ) {
        plan.notes.push_back( "camp-map dispatch blocked: entry payload stayed outside the bounded handoff contract" );
        return plan;
    }

    plan.valid = true;
    plan.notes.push_back( "camp-map dispatch ready: " + bandit_dry_run::to_string( decision.intent ) +
                          " toward " + plan.target_id +
                          " members=" + std::to_string( plan.member_ids.size() ) +
                          " reserve=" + std::to_string( decision.hard_home_reserve ) +
                          " dispatchable=" + std::to_string( decision.dispatchable ) );
    plan.notes.push_back( "profile " + rules.id + ": " + rules.writeback_expectation );
    return plan;
}

dispatch_plan plan_site_dispatch( const site_record &site, const tripoint_abs_omt &target_omt,
                                  const std::string &target_id )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::live_dispatch_plan );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::live_dispatch_plans );
    bandit_live_world_probe::record_site_service( site.site_id,
            bandit_live_world_probe::site_service::live_dispatch_plan );
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    dispatch_plan plan;
    plan.site_id = site.site_id;
    plan.profile = rules.profile;
    plan.target_id = target_id;
    const tripoint_abs_omt original_target_omt = target_omt;
    plan.target_omt = reachable_ground_dispatch_target( site, original_target_omt );

    if( target_id.empty() ) {
        plan.notes.push_back( "dispatch blocked: missing target id" );
        return plan;
    }

    if( site.retired_empty_site ) {
        plan.notes.push_back( "dispatch blocked: retired_empty_site" );
        return plan;
    }

    if( site.has_active_outside_pressure() ) {
        plan.notes.push_back( "dispatch blocked: site already has an active outside group/contact" );
        return plan;
    }

    const bandit_dry_run::camp_input camp = make_dispatch_camp_input( site );
    if( camp.available_manpower <= 0 ) {
        plan.notes.push_back( "dispatch blocked: no dispatchable at-home members remain after home reserve" );
        return plan;
    }

    const bandit_dry_run::lead_input lead = make_nearby_target_lead( site, plan.target_omt, target_id );
    plan.evaluation = bandit_dry_run::evaluate( camp, { lead } );
    const bandit_dry_run::candidate_debug &winner = plan.evaluation.candidates[plan.evaluation.winner_index];
    if( !camp_decision_allows_dispatch( site.camp_decision, winner.job ) ) {
        plan.notes.push_back( "dispatch blocked: camp decision state " +
                              to_string( site.camp_decision.state ) );
        return plan;
    }
    if( !bandit_pursuit_handoff::supports_pursuit_handoff( winner ) ) {
        plan.notes.push_back( "dispatch blocked: " + plan.evaluation.winner_reason );
        return plan;
    }

    const int required_members = required_dispatch_members_for_profile( site, winner.job );
    if( required_members <= 0 ) {
        plan.notes.push_back( "dispatch blocked: winning job needs no live member handoff" );
        return plan;
    }
    if( rules.profile == hostile_site_profile::cannibal_camp && required_members > camp.available_manpower ) {
        plan.notes.push_back( "dispatch blocked: cannibal_camp pack pressure requires at least 2 at-home members after reserve" );
        return plan;
    }

    plan.member_ids = select_dispatch_members( site, required_members );
    if( static_cast<int>( plan.member_ids.size() ) != required_members ) {
        plan.notes.push_back( "dispatch blocked: not enough at-home members survived selection" );
        return plan;
    }

    plan.group = make_dispatch_group( site, plan.member_ids, target_id );
    bandit_pursuit_handoff::entry_context context;
    context.contact = rl_dist( site.anchor, plan.target_omt ) <= 4 ?
                      bandit_pursuit_handoff::contact_certainty::localized :
                      bandit_pursuit_handoff::contact_certainty::broad;
    plan.entry = bandit_pursuit_handoff::build_entry_payload( plan.group, winner, context );
    plan.notes = plan.entry.notes;
    if( plan.target_omt != original_target_omt ) {
        plan.notes.push_back( "vertical dispatch fallback: target " + original_target_omt.to_string() +
                              " routes via reachable ground " + plan.target_omt.to_string() );
    }
    if( !plan.entry.valid ) {
        plan.notes.push_back( "dispatch blocked: entry payload stayed outside the bounded handoff contract" );
        return plan;
    }

    plan.valid = true;
    plan.notes.push_back( "profile " + rules.id + ": reserve " +
                          std::to_string( required_home_reserve( site ) ) +
                          ", retreat_floor " + std::to_string( rules.retreat_bias_floor ) +
                          ", return_clock_floor " + std::to_string( rules.return_clock_floor ) );
    plan.notes.push_back( "profile writeback: " + rules.writeback_expectation );
    if( rules.profile == hostile_site_profile::cannibal_camp ) {
        plan.notes.push_back( "cannibal_camp pack pressure: pack_size " +
                              std::to_string( plan.member_ids.size() ) +
                              ", available_after_reserve " + std::to_string( camp.available_manpower ) );
    }
    plan.notes.push_back( "dispatch ready: " + bandit_dry_run::to_string( winner.job ) + " toward " + target_id );
    return plan;
}

hostile_operation_plan plan_hostile_operation( const site_record &site,
        const hostile_operation_kind operation_kind,
        const std::vector<character_id> &member_ids,
        const std::vector<tripoint_abs_omt> &route,
        const tripoint_abs_omt &rally_omt, const int current_minutes )
{
    hostile_operation_plan plan;
    const hostile_operation_kind expected_kind = hostile_operation_kind_for_profile(
                effective_profile( site ) );
    if( site.retired_empty_site || site.has_active_outside_pressure() ||
        site.camp_decision.state != camp_decision_state::preparing_follow_on ||
        !report_matches_camp_decision( site.current_scout_report, site.camp_decision ) ||
        operation_kind != expected_kind || current_minutes < 0 ||
        current_minutes < site.camp_decision.last_transition_minutes ) {
        plan.notes.push_back( "hostile operation blocked: camp decision/report/profile is not ready" );
        return plan;
    }
    if( member_ids.size() < 2 || member_ids.size() > max_hostile_operation_members ||
        !hostile_operation_party_preserves_home( site, member_ids.size() ) ) {
        plan.notes.push_back( "hostile operation blocked: fresh party violates size or home reserve" );
        return plan;
    }
    if( route.size() < 2 || route.size() > max_active_outing_route_steps ||
        route.front() != site.anchor || route.back() != site.camp_decision.target_omt ||
        std::find( route.begin(), route.end(), rally_omt ) == route.end() ) {
        plan.notes.push_back( "hostile operation blocked: route/rally does not connect camp to report target" );
        return plan;
    }

    std::vector<character_id> checked_member_ids;
    checked_member_ids.reserve( member_ids.size() );
    for( const character_id &member_id : member_ids ) {
        const member_record *member = site.find_member( member_id );
        if( member == nullptr || member->state != member_state::at_home ||
            member->wounded_or_unready ||
            std::find( checked_member_ids.begin(), checked_member_ids.end(), member_id ) !=
            checked_member_ids.end() ) {
            plan.notes.push_back( "hostile operation blocked: party is not a fresh unique home reservation" );
            return plan;
        }
        checked_member_ids.push_back( member_id );
    }

    hostile_operation_state &operation = plan.operation;
    active_outing_state &reservation = operation.reservation;
    operation.operation_kind = operation_kind;
    operation.phase = hostile_operation_phase::assembling;
    operation.source_report_revision = site.current_scout_report.revision;
    operation.source_report_generation = site.current_scout_report.source_generation;
    operation.source_report_activity_id = site.current_scout_report.source_activity_id;
    operation.source_report_application_key = site.current_scout_report.application_key;
    operation.has_rally = true;
    operation.rally_omt = rally_omt;
    operation.last_transition_reason = "fresh hostile operation reserved from final scout report";
    reservation.kind = outing_kind::hostile_operation;
    reservation.activity_id = site.site_id + "#hostile:" +
                              std::to_string( site.next_outing_generation );
    reservation.camp_id = site.site_id;
    reservation.generation = site.next_outing_generation;
    reservation.member_ids = checked_member_ids;
    reservation.leader_id = checked_member_ids.front();
    reservation.shared_route = route;
    reservation.waypoint_index = 0;
    reservation.target_id = site.camp_decision.target_id;
    reservation.target_omt = site.camp_decision.target_omt;
    reservation.job_type = operation_kind == hostile_operation_kind::raid ? "raid" : "toll";
    reservation.target_lead_revision = site.camp_decision.target_lead_revision;
    reservation.phase = scout_phase::assembling;
    reservation.started_minutes = current_minutes;
    reservation.last_progress_minutes = current_minutes;
    reservation.last_advanced_minutes = current_minutes;
    reservation.owner = simulation_owner::abstract;
    reservation.return_application_key = reservation.activity_id + ":return:" +
                                         std::to_string( reservation.generation );
    reservation.report_application_key = reservation.activity_id + ":report:" +
                                         std::to_string( reservation.generation );
    reservation.cargo_application_key = reservation.activity_id + ":cargo:" +
                                        std::to_string( reservation.generation );
    plan.valid = true;
    plan.notes.push_back( "hostile operation ready: fresh report-pinned party at rally " +
                          rally_omt.to_string() );
    return plan;
}

bool apply_hostile_operation_plan( site_record &site, const hostile_operation_plan &plan )
{
    const hostile_operation_state &operation = plan.operation;
    const active_outing_state &reservation = operation.reservation;
    const hostile_operation_kind expected_kind = hostile_operation_kind_for_profile(
                effective_profile( site ) );
    const std::string expected_activity_id = site.site_id + "#hostile:" +
            std::to_string( site.next_outing_generation );
    const bool rally_is_on_route = operation.has_rally &&
                                   std::find( reservation.shared_route.begin(),
                                           reservation.shared_route.end(),
                                           operation.rally_omt ) != reservation.shared_route.end();
    if( !plan.valid || site.retired_empty_site || site.has_active_outside_pressure() ||
        site.camp_decision.state != camp_decision_state::preparing_follow_on ||
        !report_matches_camp_decision( site.current_scout_report, site.camp_decision ) ||
        !report_matches_hostile_operation( site.current_scout_report, operation ) ||
        operation.operation_kind != expected_kind || operation.legacy_unpinned ||
        operation.phase != hostile_operation_phase::assembling ||
        reservation.kind != outing_kind::hostile_operation ||
        reservation.phase != scout_phase::assembling || reservation.camp_id != site.site_id ||
        reservation.activity_id != expected_activity_id ||
        reservation.generation != site.next_outing_generation ||
        reservation.member_ids.size() < 2 ||
        reservation.member_ids.size() > max_hostile_operation_members ||
        !hostile_operation_party_preserves_home( site, reservation.member_ids.size() ) ||
        reservation.shared_route.size() < 2 ||
        reservation.shared_route.size() > max_active_outing_route_steps ||
        reservation.shared_route.front() != site.anchor ||
        reservation.shared_route.back() != site.camp_decision.target_omt ||
        !rally_is_on_route || reservation.target_id != site.camp_decision.target_id ||
        reservation.target_omt != site.camp_decision.target_omt ||
        reservation.target_lead_revision != site.camp_decision.target_lead_revision ||
        !hostile_operation_job_matches( operation.operation_kind, reservation.job_type ) ||
        reservation.started_minutes < site.camp_decision.last_transition_minutes ||
        reservation.started_minutes != reservation.last_progress_minutes ||
        reservation.started_minutes != reservation.last_advanced_minutes ||
        reservation.owner != simulation_owner::abstract || reservation.handoff_epoch != 0 ||
        reservation.return_application_key != reservation.activity_id + ":return:" +
        std::to_string( reservation.generation ) ||
        reservation.report_application_key != reservation.activity_id + ":report:" +
        std::to_string( reservation.generation ) ||
        reservation.cargo_application_key != reservation.activity_id + ":cargo:" +
        std::to_string( reservation.generation ) ||
        !reservation.observations.empty() || !reservation.casualty_ids.empty() ||
        !reservation.resolved_member_ids.empty() || reservation.cargo.supply_units != 0 ||
        reservation.cargo.trade_value != 0 ) {
        return false;
    }

    std::vector<character_id> checked_member_ids;
    checked_member_ids.reserve( reservation.member_ids.size() );
    for( const character_id &member_id : reservation.member_ids ) {
        const member_record *member = site.find_member( member_id );
        if( member == nullptr || member->state != member_state::at_home ||
            member->wounded_or_unready ||
            std::find( checked_member_ids.begin(), checked_member_ids.end(), member_id ) !=
            checked_member_ids.end() ) {
            return false;
        }
        checked_member_ids.push_back( member_id );
    }

    site_record candidate = site;
    candidate.active_hostile_operation = operation;
    candidate.next_outing_generation++;
    const camp_map_lead *lead = find_camp_map_dispatch_lead_for_target(
                                    candidate, reservation.target_omt,
                                    reservation.target_id );
    if( lead != nullptr ) {
        candidate.remembered_target_or_mark = reservation.target_id;
        candidate.remembered_threat_estimate = std::max( 0, lead->threat );
        candidate.remembered_bounty_estimate = std::max( 0, lead->bounty );
        push_unique_mark( candidate.known_recent_marks, reservation.target_id );
    }
    site = std::move( candidate );
    return true;
}

bool apply_dispatch_plan( site_record &site, const dispatch_plan &plan )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::live_dispatch_apply );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::live_dispatch_applies );
    if( !plan.valid || plan.entry.job_type != bandit_dry_run::job_template::scout ||
        plan.site_id != site.site_id || plan.member_ids.empty() ||
        plan.member_ids.size() > max_active_outing_members ||
        !camp_decision_allows_dispatch( site.camp_decision, plan.entry.job_type ) ||
        site.has_active_outside_pressure() || plan.group.activity_generation != site.next_outing_generation ||
        plan.entry.activity_generation != plan.group.activity_generation ||
        plan.group.handoff_epoch != 0 ||
        plan.entry.handoff_epoch != plan.group.handoff_epoch ||
        plan.group.activity_generation <= site.applied_return_generation ||
        plan.entry.return_application_key.empty() ||
        plan.entry.return_application_key != plan.group.return_application_key ) {
        return false;
    }

    std::vector<character_id> checked_member_ids;
    checked_member_ids.reserve( plan.member_ids.size() );
    for( const character_id &member_id : plan.member_ids ) {
        const member_record *member = site.find_member( member_id );
        if( member == nullptr || member->state != member_state::at_home || member->wounded_or_unready ||
            std::find( checked_member_ids.begin(), checked_member_ids.end(), member_id ) !=
            checked_member_ids.end() ) {
            return false;
        }
        checked_member_ids.push_back( member_id );
    }

    const std::string summary = "dispatch " + bandit_dry_run::to_string( plan.entry.job_type ) +
                                " toward " + plan.target_id;
    for( const character_id &member_id : plan.member_ids ) {
        if( !update_member_state( site, member_id, member_state::outbound, summary ) ) {
            return false;
        }
    }
    site.active_outing.clear();
    site.active_outing.kind = outing_kind::scout_sortie;
    site.active_outing.activity_id = plan.entry.group_id;
    site.active_outing.camp_id = site.site_id;
    site.active_outing.generation = plan.entry.activity_generation;
    site.active_outing.member_ids = plan.member_ids;
    site.active_outing.leader_id = plan.member_ids.front();
    site.active_outing.phase = scout_phase::outbound;
    site.active_outing.owner = simulation_owner::abstract;
    site.active_outing.handoff_epoch = plan.entry.handoff_epoch;
    site.active_outing.return_application_key = plan.entry.return_application_key;
    site.active_outing.report_application_key = plan.entry.group_id + ":report:" +
            std::to_string( plan.entry.activity_generation );
    site.active_outing.cargo_application_key = plan.entry.group_id + ":cargo:" +
            std::to_string( plan.entry.activity_generation );
    site.next_outing_generation++;
    site.active_outing.target_id = plan.target_id;
    site.active_outing.target_omt = plan.target_omt;
    site.active_outing.job_type = bandit_dry_run::to_string( plan.entry.job_type );
    site.active_outing.started_minutes = -1;
    site.active_outing.local_contact_minutes = -1;
    site.active_outing.last_progress_minutes = -1;
    site.remembered_target_or_mark = plan.entry.current_target_or_mark;
    site.remembered_threat_estimate = plan.group.current_threat_estimate;
    site.remembered_bounty_estimate = plan.group.current_bounty_estimate;
    site.remembered_retreat_bias = plan.group.retreat_bias;
    site.remembered_return_clock = plan.group.return_clock;
    site.remembered_pressure = plan.group.remaining_pressure;
    site.known_recent_marks = plan.group.known_recent_marks;
    return true;
}

local_gate_decision choose_local_gate_posture( const site_record &site,
        const local_gate_input &input )
{
    local_gate_decision decision;
    const hostile_site_profile profile = effective_profile( site );
    const active_outing_state *outing = site.active_external_outing();
    decision.dispatch_strength = site.active_outing_survivor_count();
    decision.pressure_margin = decision.dispatch_strength + input.local_opportunity - input.local_threat;

    if( outing == nullptr || !outing->is_active() || outing->member_ids.empty() ) {
        decision.notes.push_back( "local gate blocked: no active owned outing is present" );
        return decision;
    }

    decision.valid = true;
    decision.notes.push_back( "active owned outing " + outing->activity_id + " toward " +
                              outing->target_id );
    decision.notes.push_back( "inputs: strength " + std::to_string( decision.dispatch_strength ) +
                              ", threat " + std::to_string( input.local_threat ) +
                              ", opportunity " + std::to_string( input.local_opportunity ) +
                              ", margin " + std::to_string( decision.pressure_margin ) );
    const std::optional<bandit_dry_run::job_template> active_job =
        job_template_from_string( outing->job_type );
    const bool cannibal_attack_intent = profile == hostile_site_profile::cannibal_camp &&
                                        active_job.has_value() &&
                                        cannibal_job_requires_attack_pack( *active_job );
    decision.shakedown_capable = profile != hostile_site_profile::cannibal_camp &&
                                  !input.rolling_travel_scene && decision.dispatch_strength >= 1 &&
                                  decision.pressure_margin >= 2;
    if( decision.shakedown_capable ) {
        decision.notes.push_back(
            "bandit pressure is shakedown-capable once local contact is established" );
    }
    if( input.smoke_obscured_lead ) {
        decision.notes.push_back( std::string( "smoke-obscured lead: watcher_tile=" ) +
                                  ( input.smoke_on_watcher_tile ? "yes" : "no" ) +
                                  " sightline=" +
                                  ( input.smoke_between_watcher_and_camp ? "yes" : "no" ) );
    }

    if( input.rolling_travel_scene ) {
        if( profile == hostile_site_profile::cannibal_camp &&
            ( !cannibal_attack_intent || decision.dispatch_strength < 2 ) ) {
            decision.posture = local_gate_posture::probe;
            decision.notes.push_back( "rolling travel cannibal scout/probe contact stays below attack until a pack attack intent exists" );
            return decision;
        }
        if( decision.pressure_margin >= 0 ) {
            decision.posture = local_gate_posture::attack_now;
            decision.combat_forward = true;
            decision.notes.push_back( "rolling travel scene skips polite shakedown and reads as an ambush window" );
            return decision;
        }
        decision.posture = local_gate_posture::probe;
        decision.notes.push_back( "rolling travel scene is tempting but still too protected for an immediate attack" );
        return decision;
    }

    if( site.last_shakedown_outcome == "fight_unresolved" && input.local_contact_established ) {
        decision.posture = local_gate_posture::attack_now;
        decision.combat_forward = true;
        decision.notes.push_back( "fight branch is already selected; active shakedown pressure commits to hostile contact instead of reopening the demand" );
        return decision;
    }

    if( decision.pressure_margin <= -3 ) {
        decision.posture = local_gate_posture::abort;
        decision.valid = false;
        decision.notes.push_back( "local gate aborts because local threat overwhelms dispatched pressure" );
        return decision;
    }

    if( input.smoke_obscured_lead ) {
        if( profile == hostile_site_profile::cannibal_camp ) {
            if( input.basecamp_or_camp_scene ) {
                decision.posture = local_gate_posture::hold_off;
                decision.notes.push_back(
                    "smoke/sight obscurity makes cannibal camp hold off instead of camping the smoked tile" );
                return decision;
            }
            if( input.local_opportunity > 0 && decision.pressure_margin >= 0 ) {
                decision.posture = local_gate_posture::probe;
                decision.notes.push_back(
                    "smoke-obscured cannibal lead probes around the concealment without opening a shakedown" );
                return decision;
            }
            decision.posture = local_gate_posture::stalk;
            decision.notes.push_back(
                "smoke-obscured cannibal lead stays cautious until the killing window is clearer" );
            return decision;
        }
        if( input.basecamp_or_camp_scene ) {
            decision.posture = local_gate_posture::hold_off;
            decision.notes.push_back(
                "smoke-obscured defended-camp watcher backs off/waits instead of camping the smoked tile" );
            return decision;
        }
        if( input.local_opportunity > 0 && decision.pressure_margin >= 0 ) {
            decision.posture = local_gate_posture::probe;
            decision.notes.push_back(
                "smoke-obscured bandit lead becomes uncertain probe-around pressure rather than a clear beeline" );
            return decision;
        }
        decision.posture = local_gate_posture::stalk;
        decision.notes.push_back(
            "smoke-obscured bandit lead stays in cautious stalking until the lead clears" );
        return decision;
    }

    if( profile == hostile_site_profile::cannibal_camp ) {
        const int cannibal_pressure_margin = decision.pressure_margin +
                                             ( input.darkness_or_concealment ? 1 : 0 );
        if( input.darkness_or_concealment ) {
            decision.notes.push_back( "darkness/concealment improves the cannibal killing window without overriding pack or threat gates" );
        }
        if( input.basecamp_or_camp_scene && ( input.current_exposure || input.recent_exposure ) ) {
            decision.posture = local_gate_posture::hold_off;
            decision.notes.push_back( "sight/exposure makes the cannibal camp hold off instead of continuing a visible beeline" );
            return decision;
        }
        if( input.local_contact_established &&
            ( !cannibal_attack_intent || decision.dispatch_strength < 2 ) ) {
            decision.posture = local_gate_posture::probe;
            decision.notes.push_back(
                "cannibal camp refuses to turn scout/probe contact or a lone hunter into the whole attack pack" );
            return decision;
        }
        if( input.local_contact_established && cannibal_attack_intent && decision.dispatch_strength >= 2 &&
            cannibal_pressure_margin >= 1 ) {
            decision.posture = local_gate_posture::attack_now;
            decision.combat_forward = true;
            decision.notes.push_back( "cannibal camp pressure does not negotiate; favorable pack contact becomes attack-to-kill pressure" );
            return decision;
        }
        if( input.basecamp_or_camp_scene && !input.darkness_or_concealment &&
            decision.pressure_margin <= 0 ) {
            decision.posture = local_gate_posture::hold_off;
            decision.notes.push_back( "daylight/no-cover camp pressure holds off instead of becoming a suicide rush" );
            return decision;
        }
        if( input.local_opportunity > 0 && cannibal_pressure_margin >= 0 ) {
            decision.posture = local_gate_posture::probe;
            decision.notes.push_back( "cannibal camp probes for a killing window instead of opening a shakedown" );
            return decision;
        }
        decision.posture = local_gate_posture::stalk;
        decision.notes.push_back( "cannibal camp pressure stalks until the kill window improves" );
        return decision;
    }

    if( input.basecamp_or_camp_scene &&
        ( input.current_exposure || input.recent_exposure || decision.pressure_margin <= 0 ) ) {
        decision.posture = local_gate_posture::hold_off;
        decision.notes.push_back( "camp-adjacent pressure holds off instead of collapsing onto the player tile" );
        return decision;
    }

    if( input.local_contact_established && decision.dispatch_strength >= 1 &&
        decision.pressure_margin >= 2 ) {
        decision.posture = local_gate_posture::open_shakedown;
        decision.opens_shakedown_surface = true;
        decision.notes.push_back( "contact is established and pressure is strong enough to open the later shakedown surface" );
        return decision;
    }

    if( input.local_opportunity > 0 && decision.pressure_margin >= 0 ) {
        decision.posture = local_gate_posture::probe;
        decision.notes.push_back( "opportunity is real but not yet strong enough for the robbery surface" );
        return decision;
    }

    decision.posture = local_gate_posture::stalk;
    decision.notes.push_back( "pressure stays readable as stalking until the scene changes" );
    return decision;
}

int ordinary_scout_watch_standoff_omt()
{
    return 5;
}

int minimum_hold_off_standoff_omt()
{
    return ordinary_scout_watch_standoff_omt();
}

tripoint_abs_omt choose_hold_off_standoff_goal( const tripoint_abs_omt &site_anchor,
        const tripoint_abs_omt &player_omt, const int requested_distance )
{
    const int desired_distance = std::max( requested_distance, minimum_hold_off_standoff_omt() );
    const int dx = ( site_anchor.x() > player_omt.x() ) -
                   ( site_anchor.x() < player_omt.x() );
    const int dy = ( site_anchor.y() > player_omt.y() ) -
                   ( site_anchor.y() < player_omt.y() );
    if( dx == 0 && dy == 0 ) {
        return player_omt;
    }
    return tripoint_abs_omt( player_omt.x() + dx * desired_distance,
                             player_omt.y() + dy * desired_distance, player_omt.z() );
}

bool hot_defended_doorstep_blocks_pickup( const site_record &site,
        const local_gate_input &input, const local_gate_decision &decision,
        const character_id &member_id )
{
    const active_outing_state *outing = site.active_external_outing();
    if( outing == nullptr || !outing->is_active() || outing->member_ids.empty() ) {
        return false;
    }
    if( std::find( outing->member_ids.begin(), outing->member_ids.end(), member_id ) ==
        outing->member_ids.end() ) {
        return false;
    }
    if( !input.basecamp_or_camp_scene || input.rolling_travel_scene ) {
        return false;
    }
    if( decision.combat_forward || decision.opens_shakedown_surface ) {
        return false;
    }
    if( decision.posture != local_gate_posture::stalk &&
        decision.posture != local_gate_posture::hold_off ) {
        return false;
    }
    const bool hot_by_sight_smoke_or_doorstep = input.current_exposure || input.recent_exposure ||
            input.smoke_obscured_lead || input.standoff_distance <= 1;
    return hot_by_sight_smoke_or_doorstep;
}

std::string render_local_gate_report( const site_record &site, const local_gate_input &input,
                                      const local_gate_decision &decision )
{
    const active_outing_state *outing = site.active_external_outing();
    const active_outing_state empty_outing;
    if( outing == nullptr ) {
        outing = &empty_outing;
    }
    std::ostringstream out;
    out << "local_gate site=" << site.site_id
        << " active_group=" << outing->activity_id
        << " target=" << outing->target_id
        << " live_dispatch_goal=" << outing->target_omt.x() << ',' << outing->target_omt.y() << ','
        << outing->target_omt.z()
        << " active_job=" << ( outing->job_type.empty() ? "unknown" : outing->job_type )
        << " profile=" << to_string( effective_profile( site ) )
        << " posture=" << to_string( decision.posture )
        << " strength=" << decision.dispatch_strength
        << " pack_size=" << decision.dispatch_strength
        << " threat=" << input.local_threat
        << " opportunity=" << input.local_opportunity
        << " margin=" << decision.pressure_margin
        << " darkness_or_concealment=" << ( input.darkness_or_concealment ? "yes" : "no" )
        << " standoff_distance=" << input.standoff_distance
        << " basecamp_or_camp=" << ( input.basecamp_or_camp_scene ? "yes" : "no" )
        << " current_exposure=" << ( input.current_exposure ? "yes" : "no" )
        << " recent_exposure=" << ( input.recent_exposure ? "yes" : "no" )
        << " sight_exposure=" << ( input.current_exposure ? "current" :
                                      ( input.recent_exposure ? "recent" : "none" ) )
        << " smoke_obscured=" << ( input.smoke_obscured_lead ? "yes" : "no" )
        << " smoke_on_watcher=" << ( input.smoke_on_watcher_tile ? "yes" : "no" )
        << " smoke_sightline=" << ( input.smoke_between_watcher_and_camp ? "yes" : "no" )
        << " local_contact=" << ( input.local_contact_established ? "yes" : "no" )
        << " rolling_travel=" << ( input.rolling_travel_scene ? "yes" : "no" )
        << " shakedown_capable=" << ( decision.shakedown_capable ? "yes" : "no" )
        << " shakedown=" << ( decision.opens_shakedown_surface ? "yes" : "no" )
        << " combat_forward=" << ( decision.combat_forward ? "yes" : "no" )
        << '\n';
    for( const std::string &note : decision.notes ) {
        out << "- " << note << '\n';
    }
    return out.str();
}

sight_avoid_decision choose_sight_avoid_reposition( const tripoint_abs_ms &current_tile,
        const bool current_exposure, const bool recent_exposure,
        const std::vector<sight_avoid_candidate> &candidates,
        const bool current_smoke_obscured )
{
    sight_avoid_decision decision;
    decision.valid = true;
    decision.destination = current_tile;

    if( !current_exposure && !recent_exposure && !current_smoke_obscured ) {
        decision.reason = "still stalking";
        decision.notes.push_back(
            "sight_avoid: no current/recent exposure or smoke-obscured tile, no reposition needed" );
        return decision;
    }

    int best_score = -1000000;
    std::optional<sight_avoid_candidate> best_candidate;
    for( const sight_avoid_candidate &candidate : candidates ) {
        if( !candidate.passable || candidate.tile == current_tile || rl_dist( candidate.tile, current_tile ) > 1 ) {
            continue;
        }
        int score = candidate.cover_score;
        if( !candidate.visible_to_player ) {
            score += 80;
        }
        if( !candidate.visible_to_camp ) {
            score += 40;
        }
        if( current_smoke_obscured && !candidate.smoke_obscured ) {
            score += 60;
        }
        if( candidate.smoke_obscured ) {
            score -= 40;
        }
        if( candidate.visible_to_player && candidate.visible_to_camp ) {
            score -= 60;
        }
        if( score > best_score ) {
            best_score = score;
            best_candidate = candidate;
        }
    }

    if( !best_candidate.has_value() ) {
        decision.reason = current_smoke_obscured ?
                          "blocked: smoke-obscured no adjacent passable reposition candidate" :
                          "blocked: exposed no adjacent passable reposition candidate";
        decision.notes.push_back(
            current_smoke_obscured ?
            "sight_avoid: smoke-obscured but no adjacent passable local reposition candidate" :
            "sight_avoid: exposed but no adjacent passable local reposition candidate" );
        return decision;
    }

    const bool breaks_player_sight = !best_candidate->visible_to_player;
    const bool breaks_camp_sight = !best_candidate->visible_to_camp;
    const bool clears_smoke = current_smoke_obscured && !best_candidate->smoke_obscured;
    if( !breaks_player_sight && !breaks_camp_sight && !clears_smoke && best_candidate->cover_score <= 0 &&
        !current_smoke_obscured ) {
        decision.reason = "still stalking";
        decision.notes.push_back(
            "sight_avoid: exposed but adjacent candidates do not improve cover or line of sight" );
        return decision;
    }

    decision.repositions = true;
    decision.destination = best_candidate->tile;
    decision.reason = current_smoke_obscured ?
                      "repositioning because smoke obscures lead" : "repositioning because exposed";
    decision.notes.push_back( current_smoke_obscured ?
                              "sight_avoid: smoke-obscured -> bounded adjacent reposition" :
                              "sight_avoid: exposed -> bounded adjacent reposition" );
    decision.notes.push_back( std::string( "sight_avoid: breaks_player_los=" ) +
                              ( breaks_player_sight ? "yes" : "no" ) +
                              " breaks_camp_los=" + ( breaks_camp_sight ? "yes" : "no" ) +
                              " clears_smoke=" + ( clears_smoke ? "yes" : "no" ) +
                              " cover_score=" + std::to_string( best_candidate->cover_score ) );
    return decision;
}

bool note_active_sortie_started( site_record &site,
                                 const simulation_advance_cursor &expected_cursor,
                                 const int current_minutes )
{
    if( !site.active_outing.is_active() ||
        !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        site.active_outing.member_ids.empty() || current_minutes < 0 ) {
        return false;
    }
    site_record candidate = site;
    bool changed = false;
    if( candidate.active_outing.started_minutes < 0 ) {
        if( candidate.active_outing.kind == outing_kind::scout_sortie &&
            !is_valid_scout_phase_transition( candidate.active_outing.phase,
                                              scout_phase::outbound ) ) {
            return false;
        }
        if( candidate.active_outing.kind == outing_kind::scout_sortie ) {
            const scout_phase_transition_result transition = transition_active_scout_phase(
                        candidate, expected_cursor, candidate.active_outing.phase,
                        scout_phase::outbound, current_minutes );
            if( transition == scout_phase_transition_result::rejected ) {
                return false;
            }
        } else {
            candidate.active_outing.phase = scout_phase::outbound;
            candidate.active_outing.last_progress_minutes = current_minutes;
            candidate.active_outing.last_advanced_minutes = current_minutes;
        }
        candidate.active_outing.started_minutes = current_minutes;
        changed = true;
    }
    if( candidate.active_outing.expected_return_minutes < 0 ) {
        candidate.active_outing.expected_return_minutes = minutes_after_saturated(
                candidate.active_outing.started_minutes,
                ordinary_scout_sortie_limit_minutes() + scout_return_cohesion_minutes );
        changed = true;
    }
    if( candidate.active_outing.missing_deadline_minutes <
        candidate.active_outing.expected_return_minutes ) {
        candidate.active_outing.missing_deadline_minutes = minutes_after_saturated(
                candidate.active_outing.expected_return_minutes, scout_missing_grace_minutes );
        changed = true;
    }
    if( changed ) {
        if( current_minutes <= site.active_outing.last_advanced_minutes ) {
            return false;
        }
        candidate.active_outing.last_advanced_minutes = current_minutes;
        site = std::move( candidate );
    }
    return changed;
}

bool note_active_sortie_local_contact( site_record &site,
                                       const simulation_advance_cursor &expected_cursor,
                                       const character_id contact_member_id,
                                       const int current_minutes )
{
    if( !site.active_outing.is_active() ||
        !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        site.active_outing.member_ids.empty() || current_minutes < 0 ||
        current_minutes < site.active_outing.last_advanced_minutes ||
        site.active_outing.member_is_resolved( contact_member_id ) ||
        std::find( site.active_outing.member_ids.begin(), site.active_outing.member_ids.end(),
                   contact_member_id ) == site.active_outing.member_ids.end() ) {
        return false;
    }
    site_record candidate = site;
    const bool first_local_contact = site.active_outing.local_contact_minutes < 0;
    const bool same_minute_initial_handoff =
        first_local_contact &&
        current_minutes == site.active_outing.last_advanced_minutes &&
        site.active_outing.owner == simulation_owner::abstract &&
        site.active_outing.started_minutes == current_minutes;
    if( current_minutes == site.active_outing.last_advanced_minutes &&
        !same_minute_initial_handoff ) {
        return false;
    }
    member_record *contact_member = candidate.find_member( contact_member_id );
    if( contact_member == nullptr ||
        ( contact_member->state != member_state::outbound &&
          contact_member->state != member_state::local_contact ) ||
        ( !first_local_contact &&
          contact_member->state == member_state::local_contact ) ) {
        return false;
    }
    if( first_local_contact &&
        candidate.active_outing.kind == outing_kind::scout_sortie &&
        !is_valid_scout_phase_transition( candidate.active_outing.phase,
                                          scout_phase::observing ) ) {
        return false;
    }
    if( first_local_contact &&
        candidate.active_outing.kind == outing_kind::scout_sortie ) {
        if( same_minute_initial_handoff ) {
            if( !is_valid_scout_phase_transition( candidate.active_outing.phase,
                                                  scout_phase::observing ) ) {
                return false;
            }
            candidate.active_outing.phase = scout_phase::observing;
            candidate.active_outing.last_progress_minutes = current_minutes;
            candidate.active_outing.last_advanced_minutes = current_minutes;
        } else {
            const scout_phase_transition_result transition = transition_active_scout_phase(
                        candidate, expected_cursor, candidate.active_outing.phase,
                        scout_phase::observing, current_minutes );
            if( transition == scout_phase_transition_result::rejected ) {
                return false;
            }
        }
    } else if( first_local_contact ) {
        candidate.active_outing.phase = scout_phase::observing;
        candidate.active_outing.last_progress_minutes = current_minutes;
        candidate.active_outing.last_advanced_minutes = current_minutes;
    }
    if( first_local_contact ) {
        candidate.active_outing.local_contact_minutes = current_minutes;
    } else {
        candidate.active_outing.last_progress_minutes = current_minutes;
        candidate.active_outing.last_advanced_minutes = current_minutes;
    }
    if( !update_member_state( candidate, contact_member_id, member_state::local_contact,
                              "local contact near " + candidate.active_outing.target_id ) ) {
        return false;
    }
    if( first_local_contact ) {
        candidate.active_outing.expected_return_minutes = minutes_after_saturated(
                current_minutes,
                ordinary_scout_sortie_limit_minutes() + scout_return_cohesion_minutes );
        candidate.active_outing.missing_deadline_minutes = minutes_after_saturated(
                candidate.active_outing.expected_return_minutes,
                scout_missing_grace_minutes );
    }
    if( candidate.active_outing.owner != simulation_owner::local ) {
        const simulation_owner_transition_result handoff =
            transition_external_simulation_owner(
                candidate, candidate.active_outing.activity_id,
                candidate.active_outing.generation, candidate.active_outing.owner,
                simulation_owner::local, candidate.active_outing.handoff_epoch,
                candidate.active_outing.last_advanced_minutes,
                current_minutes );
        if( handoff != simulation_owner_transition_result::applied ) {
            return false;
        }
    }
    site = std::move( candidate );
    return true;
}

int ordinary_scout_sortie_limit_minutes()
{
    return 720;
}

bool scout_sortie_should_return_home( const site_record &site, const int current_minutes,
                                      const int sortie_limit_minutes )
{
    if( !site.active_outing.is_active() || site.active_outing.member_ids.size() != 1 ||
        sortie_limit_minutes <= 0 || current_minutes < 0 ||
        site.last_shakedown_outcome == "fight_unresolved" ) {
        return false;
    }

    const std::optional<bandit_dry_run::job_template> active_job =
        job_template_from_string( site.active_outing.job_type );
    if( active_job.has_value() && *active_job != bandit_dry_run::job_template::scout &&
        *active_job != bandit_dry_run::job_template::hold_chill ) {
        return false;
    }

    const int anchor_minutes = site.active_outing.local_contact_minutes >= 0 ?
                               site.active_outing.local_contact_minutes : site.active_outing.started_minutes;
    return anchor_minutes >= 0 && current_minutes - anchor_minutes >= sortie_limit_minutes;
}

shakedown_surface build_shakedown_surface( const site_record &site, const local_gate_input &input,
        const local_gate_decision &decision, const shakedown_goods_pool &goods_pool )
{
    shakedown_surface surface;

    if( !decision.valid || !decision.opens_shakedown_surface ||
        decision.posture != local_gate_posture::open_shakedown ||
        !site.active_outing.is_active() ) {
        surface.notes.push_back( "shakedown blocked: local gate did not open the robbery surface" );
        return surface;
    }

    if( effective_profile( site ) == hostile_site_profile::cannibal_camp ) {
        surface.notes.push_back( "shakedown blocked: cannibal camp profile attacks to kill instead of extorting" );
        return surface;
    }

    if( input.rolling_travel_scene ) {
        surface.notes.push_back( "shakedown blocked: rolling travel scene remains a direct-ambush context" );
        return surface;
    }

    surface.includes_basecamp_inventory = input.basecamp_or_camp_scene ||
                                          goods_pool.basecamp_or_camp_scene;
    surface.includes_vehicle_inventory = !surface.includes_basecamp_inventory;
    surface.reachable_goods_value = goods_pool.player_carried_value +
                                    goods_pool.companion_carried_value;
    if( surface.includes_basecamp_inventory ) {
        surface.reachable_goods_value += goods_pool.reachable_basecamp_value;
        surface.notes.push_back( "pool includes player, nearby companion, and reachable Basecamp goods" );
    } else {
        surface.reachable_goods_value += goods_pool.vehicle_carried_value;
        surface.notes.push_back( "pool includes player, companion, and current vehicle goods only" );
    }

    if( surface.reachable_goods_value <= 0 ) {
        surface.notes.push_back( "shakedown blocked: no honest reachable goods are present" );
        return surface;
    }

    const shakedown_opening_beat opening = choose_shakedown_opening_beat( site, input, decision );
    surface.opening_id = opening.id;
    surface.opening_summary = opening.summary;
    surface.bark = opening.bark;

    surface.valid = true;
    surface.pay_available = true;
    surface.fight_available = true;
    const int base_demanded_value = std::max( 1, ( surface.reachable_goods_value * 35 + 99 ) / 100 );
    const int demand_modifier_percent = shakedown_demand_modifier_percent( site );
    surface.demanded_value = ( base_demanded_value * demand_modifier_percent + 99 ) / 100;
    surface.demanded_value = std::clamp( surface.demanded_value, 1, surface.reachable_goods_value );
    if( site.shakedown_reopen_available && !site.shakedown_reopen_used ) {
        surface.notes.push_back( "renegotiation reopen: previous defender loss raises this one bounded demand" );
    } else if( demand_modifier_percent < 100 ) {
        surface.notes.push_back( "aftermath caution: previous bandit loss cools or shrinks this demand" );
    }
    surface.notes.push_back( "scenic opening beat: " + surface.opening_summary );
    surface.notes.push_back( "visible responses are Pay/Fight only; backout enters the fight/refusal branch" );
    surface.notes.push_back( "pay branch opens the NPC trade UI with the demanded toll as debt before any goods are surrendered" );
    surface.notes.push_back( "fight branch stays explicit whenever this surface is invoked" );
    surface.notes.push_back( "source site " + site.site_id + " opened the surface from " +
                             site.active_outing.activity_id );
    return surface;
}

std::string render_shakedown_surface_report( const site_record &site,
        const shakedown_surface &surface )
{
    std::ostringstream out;
    out << "shakedown_surface site=" << site.site_id
        << " active_group=" << site.active_outing.activity_id
        << " profile=" << to_string( effective_profile( site ) )
        << " posture=open_shakedown"
        << " valid=" << ( surface.valid ? "yes" : "no" )
        << " pay_option=" << ( surface.pay_available ? "yes" : "no" )
        << " fight_option=" << ( surface.fight_available ? "yes" : "no" )
        << " visible_responses=pay/fight"
        << " payment_surface=npc_trade_ui"
        << " reachable_goods=" << surface.reachable_goods_value
        << " demanded_toll=" << surface.demanded_value
        << " basecamp_inventory=" << ( surface.includes_basecamp_inventory ? "yes" : "no" )
        << " vehicle_inventory=" << ( surface.includes_vehicle_inventory ? "yes" : "no" )
        << " opening=" << ( surface.opening_id.empty() ? "none" : surface.opening_id )
        << " bark=\"" << surface.bark << "\"\n";
    for( const std::string &note : surface.notes ) {
        out << "- " << note << '\n';
    }
    return out.str();
}

bool is_active_shakedown_parley_member( const world_state &state, const character_id npc_id )
{
    for( const site_record &site : state.sites ) {
        if( site.retired_empty_site || !site.active_outing.is_active() ||
            site.active_outing.owner != simulation_owner::local ||
            site.active_outing.member_ids.empty() || site.active_outing.job_type != "toll" ) {
            continue;
        }
        if( site.last_shakedown_outcome.rfind( "fight", 0 ) == 0 ) {
            continue;
        }
        if( std::find( site.active_outing.member_ids.begin(), site.active_outing.member_ids.end(),
                      npc_id ) == site.active_outing.member_ids.end() ) {
            continue;
        }
        const member_record *member = site.find_member( npc_id );
        if( member == nullptr || member->state == member_state::dead ||
            member->state == member_state::missing ) {
            continue;
        }
        return true;
    }
    return false;
}

shakedown_aftermath_effect apply_shakedown_outcome( site_record &site,
        const shakedown_outcome &outcome )
{
    shakedown_aftermath_effect effect;
    if( ( !outcome.paid && !outcome.fought ) || outcome.demanded_value <= 0 ) {
        effect.notes.push_back( "shakedown aftermath ignored: no concrete paid/fought outcome" );
        return effect;
    }

    effect.valid = true;
    site.last_shakedown_outcome = shakedown_outcome_label( outcome );
    site.shakedown_last_demanded_value = outcome.demanded_value;
    site.shakedown_last_surrendered_value = outcome.surrendered_value;
    site.shakedown_last_reachable_value = outcome.reachable_goods_value;

    if( outcome.paid ) {
        site.shakedown_loot_value += std::max( 0, outcome.surrendered_value );
        site.remembered_bounty_estimate += std::max( 1, outcome.surrendered_value / 1000 );
        effect.notes.push_back( "paid shakedown writes surrendered value into abstract bounty" );
    }

    if( outcome.fought ) {
        site.shakedown_anger += 1 + outcome.defender_losses;
        site.remembered_threat_estimate = std::max( 0,
                                          site.remembered_threat_estimate - outcome.defender_losses );
        effect.notes.push_back( "fight outcome writes anger and changed local threat into site memory" );
    }

    if( outcome.basecamp_or_camp_scene && outcome.defender_losses > 0 ) {
        site.shakedown_defender_losses += outcome.defender_losses;
        if( !site.shakedown_reopen_used ) {
            site.shakedown_reopen_available = true;
            effect.stronger_reopen = true;
            effect.notes.push_back( "defender loss opens exactly one stronger renegotiation demand" );
        }
    }

    if( outcome.bandit_losses > 0 || outcome.extraction_failed ) {
        site.shakedown_bandit_losses += outcome.bandit_losses;
        site.shakedown_caution += std::max( 1, outcome.bandit_losses );
        site.remembered_retreat_bias += std::max( 1, outcome.bandit_losses );
        site.remembered_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
        effect.cools_later_pressure = true;
        effect.notes.push_back( "bandit loss or failed extraction cools later pressure instead of only escalating" );
    }

    effect.demand_modifier_percent = shakedown_demand_modifier_percent( site );
    return effect;
}


void begin_shakedown_basecamp_defender_observation( site_record &site, const int live_defenders )
{
    site.shakedown_basecamp_defenders_at_fight = std::max( 0, live_defenders );
    site.shakedown_basecamp_defender_observation_pending =
        site.shakedown_basecamp_defenders_at_fight > 0;
}

shakedown_aftermath_effect apply_shakedown_basecamp_defender_observation( site_record &site,
        const int live_defenders )
{
    if( !site.shakedown_basecamp_defender_observation_pending ) {
        shakedown_aftermath_effect effect;
        effect.notes.push_back( "basecamp defender observation ignored: no pending shakedown fight" );
        return effect;
    }

    const int current_live_defenders = std::max( 0, live_defenders );
    const int defender_losses = site.shakedown_basecamp_defenders_at_fight - current_live_defenders;
    if( defender_losses <= 0 ) {
        shakedown_aftermath_effect effect;
        effect.notes.push_back( "basecamp defender observation unchanged: no defender strength drop" );
        return effect;
    }

    shakedown_outcome outcome;
    outcome.fought = true;
    outcome.basecamp_or_camp_scene = true;
    outcome.demanded_value = std::max( 1, site.shakedown_last_demanded_value );
    outcome.reachable_goods_value = site.shakedown_last_reachable_value;
    outcome.defender_losses = defender_losses;
    site.shakedown_basecamp_defender_observation_pending = false;
    return apply_shakedown_outcome( site, outcome );
}

bool mark_shakedown_reopen_used( site_record &site )
{
    if( !site.shakedown_reopen_available || site.shakedown_reopen_used ) {
        return false;
    }
    site.shakedown_reopen_used = true;
    site.shakedown_reopen_available = false;
    return true;
}

bool record_live_signal_mark( site_record &site, const live_signal_mark &mark )
{
    if( site.retired_empty_site || mark.mark_id.empty() || mark.range_cap_omt <= 0 ) {
        return false;
    }

    bool changed = site.remembered_target_or_mark != mark.mark_id;
    site.remembered_target_or_mark = mark.mark_id;
    const int old_bounty = site.remembered_bounty_estimate;
    const int old_threat = site.remembered_threat_estimate;
    site.remembered_bounty_estimate = std::max( site.remembered_bounty_estimate, mark.bounty_add );
    site.remembered_threat_estimate = std::max( site.remembered_threat_estimate, mark.threat_add );
    changed |= site.remembered_bounty_estimate != old_bounty;
    changed |= site.remembered_threat_estimate != old_threat;

    if( std::find( site.known_recent_marks.begin(), site.known_recent_marks.end(), mark.mark_id ) ==
        site.known_recent_marks.end() ) {
        static constexpr size_t max_live_signal_marks = 8;
        if( site.known_recent_marks.size() >= max_live_signal_marks ) {
            site.known_recent_marks.erase( site.known_recent_marks.begin() );
        }
        site.known_recent_marks.push_back( mark.mark_id );
        changed = true;
    }

    camp_map_lead lead;
    lead.kind = signal_kind_to_camp_lead_kind( mark.kind );
    lead.status = camp_lead_status::suspected;
    lead.target_id = mark.mark_id;
    lead.omt = mark.source_omt;
    lead.radius_omt = mark.range_cap_omt;
    lead.source_key = mark.mark_id;
    lead.source_summary = "live " + mark.kind + " signal mark";
    if( mark.kind == "smoke" ) {
        lead.source_summary += " (obscured/uncertain lead)";
    }
    lead.last_seen_minutes = -1;
    lead.bounty = std::max( 0, mark.bounty_add );
    lead.threat = std::max( 0, mark.threat_add );
    lead.confidence = std::max( 1, mark.confidence );
    lead.threat_confirmed = lead.threat > 0;
    lead.generated_by_this_camp_routine = true;
    lead.last_outcome = "live_signal";
    lead.lead_id = camp_lead_id_for( site.site_id, lead.kind, lead.target_id, lead.omt );
    const camp_map_lead *old_lead = site.intelligence_map.find_lead( lead.lead_id );
    changed |= old_lead == nullptr || old_lead->bounty != lead.bounty ||
               old_lead->threat != lead.threat || old_lead->confidence != lead.confidence ||
               old_lead->radius_omt != lead.radius_omt ||
               old_lead->source_summary != lead.source_summary;
    upsert_camp_map_lead( site.intelligence_map, lead );

    return changed;
}

std::string render_empty_site_retirement_report( const site_record &site )
{
    int spawn_tile_headcount = 0;
    for( const spawn_tile_record &spawn_tile : site.spawn_tiles ) {
        spawn_tile_headcount += std::max( 0, spawn_tile.headcount );
    }

    std::ostringstream out;
    out << "bandit_live_world retired_empty_site: site=" << site.site_id
        << " site_kind=" << to_string( site.site_kind )
        << " headcount=" << site.headcount
        << " at_home=" << site.count_members_in_state( member_state::at_home )
        << " spawn_tile_headcount=" << spawn_tile_headcount
        << " active_group=" << ( !site.active_outing.is_active() ? "no" : site.active_outing.activity_id )
        << " active_member_ids=" << site.active_outing.member_ids.size()
        << " outbound=" << site.count_members_in_state( member_state::outbound )
        << " local_contact=" << site.count_members_in_state( member_state::local_contact )
        << " home_side_signals=" << site.count_home_side_signals()
        << " active_outside=" << ( site.has_active_outside_pressure() ? "yes" : "no" );
    return out.str();
}

int retire_empty_hostile_sites( world_state &state, std::vector<std::string> *reports )
{
    int retired_count = 0;
    for( site_record &site : state.sites ) {
        if( !site.eligible_for_empty_site_retirement() ) {
            continue;
        }
        site.retired_empty_site = true;
        site.retirement_summary = render_empty_site_retirement_report( site );
        if( reports != nullptr ) {
            reports->push_back( site.retirement_summary );
        }
        retired_count++;
    }
    return retired_count;
}

bool apply_return_packet( site_record &site, const bandit_pursuit_handoff::return_packet &packet )
{
    bandit_live_world_probe::scoped_section probe_section(
        bandit_live_world_probe::section::live_return_apply );
    bandit_live_world_probe::increment(
        bandit_live_world_probe::counter::live_return_applies );
    bandit_live_world_probe::record_site_service( site.site_id,
            bandit_live_world_probe::site_service::live_return_apply );
    const bool scout_return = site.active_outing.kind == outing_kind::scout_sortie;
    const bool assessment_scout_return = scout_return &&
                                         site.active_outing.job_type == "scout";
    if( !packet.valid || packet.source_camp_id != site.site_id ||
        !site.active_outing.is_active() ||
        site.active_outing.kind == outing_kind::structural_sortie ||
        site.active_outing.return_application_key.empty() ||
        site.active_outing.cargo_application_key.empty() ||
        ( scout_return && site.active_outing.report_application_key.empty() ) ||
        packet.group_id != site.active_outing.activity_id ||
        packet.activity_generation != site.active_outing.generation ||
        packet.handoff_epoch != site.active_outing.handoff_epoch ||
        packet.return_application_key != site.active_outing.return_application_key ||
        packet.activity_generation <= site.applied_return_generation ||
        packet.activity_generation <= site.applied_cargo_generation ||
        packet.activity_generation <= site.applied_report_generation ||
        bandit_dry_run::to_string( packet.job_type ) != site.active_outing.job_type ||
        site.active_outing.member_ids.empty() ) {
        return false;
    }

    const auto status_for_member = [&packet]( character_id member_id ) -> std::string {
        const std::string member_token = std::to_string( member_id.get_value() );
        const auto iter = std::find_if( packet.anchored_identity_updates.begin(),
        packet.anchored_identity_updates.end(), [&member_token]( const bandit_pursuit_handoff::anchored_identity_state & update ) {
            return update.id == member_token;
        } );
        return iter != packet.anchored_identity_updates.end() ? iter->status : "alive";
    };

    std::vector<std::string> active_member_tokens;
    active_member_tokens.reserve( site.active_outing.member_ids.size() );
    for( const character_id &member_id : site.active_outing.member_ids ) {
        const std::string member_token = std::to_string( member_id.get_value() );
        if( site.find_member( member_id ) == nullptr ||
            std::find( active_member_tokens.begin(), active_member_tokens.end(), member_token ) !=
            active_member_tokens.end() ) {
            return false;
        }
        active_member_tokens.push_back( member_token );
    }

    std::vector<std::string> update_tokens;
    update_tokens.reserve( packet.anchored_identity_updates.size() );
    for( const bandit_pursuit_handoff::anchored_identity_state &update :
         packet.anchored_identity_updates ) {
        const bool known_status = update.status == "alive" || update.status == "wounded" ||
                                  update.status == "dead" || update.status == "missing";
        if( !known_status ||
            std::find( active_member_tokens.begin(), active_member_tokens.end(), update.id ) ==
            active_member_tokens.end() ||
            std::find( update_tokens.begin(), update_tokens.end(), update.id ) != update_tokens.end() ) {
            return false;
        }
        update_tokens.push_back( update.id );
    }

    for( const character_id &resolved_member_id : site.active_outing.resolved_member_ids ) {
        const member_record *resolved_member = site.find_member( resolved_member_id );
        const bool casualty = std::find( site.active_outing.casualty_ids.begin(),
                                        site.active_outing.casualty_ids.end(), resolved_member_id ) !=
                              site.active_outing.casualty_ids.end();
        const std::string resolved_status = status_for_member( resolved_member_id );
        if( resolved_member == nullptr ||
            ( casualty && resolved_member->state != member_state::dead &&
              resolved_member->state != member_state::missing ) ||
            ( !casualty && ( resolved_member->state != member_state::at_home ||
                             resolved_status == "dead" || resolved_status == "missing" ) ) ) {
            return false;
        }
    }

    for( const character_id &casualty_id : site.active_outing.casualty_ids ) {
        const member_record *casualty = site.find_member( casualty_id );
        const std::string casualty_status = status_for_member( casualty_id );
        if( std::find( site.active_outing.member_ids.begin(), site.active_outing.member_ids.end(),
                      casualty_id ) == site.active_outing.member_ids.end() ||
            casualty == nullptr ||
            ( casualty->state != member_state::dead && casualty->state != member_state::missing ) ||
            ( casualty->state == member_state::dead && casualty_status != "dead" ) ||
            ( casualty->state == member_state::missing && casualty_status != "missing" ) ) {
            return false;
        }
    }

    int expected_survivors = static_cast<int>( site.active_outing.member_ids.size() );
    for( const bandit_pursuit_handoff::anchored_identity_state &update :
         packet.anchored_identity_updates ) {
        if( update.status == "dead" || update.status == "missing" ) {
            expected_survivors--;
        }
    }
    if( packet.survivors_remaining != expected_survivors ) {
        return false;
    }

    const std::string target_label = packet.current_target_or_mark.empty() ? site.active_outing.target_id :
                                     packet.current_target_or_mark;
    const std::string base_summary = "return " + bandit_pursuit_handoff::to_string( packet.result ) +
                                     " from " + target_label;
    bool shakedown_fight_outing = site.last_shakedown_outcome == "fight_unresolved";
    for( const character_id &member_id : site.active_outing.member_ids ) {
        const member_record *member = site.find_member( member_id );
        shakedown_fight_outing |= member != nullptr &&
                                  member->last_writeback_summary.find( "shakedown_surface fight" ) != std::string::npos;
    }
    for( const character_id &member_id : site.active_outing.member_ids ) {
        const std::string status = status_for_member( member_id );
        member_state new_state = member_state::at_home;
        std::string summary = base_summary;
        if( status == "dead" ) {
            new_state = member_state::dead;
            summary += " (dead)";
        } else if( status == "missing" ) {
            new_state = member_state::missing;
            summary += " (missing)";
        } else {
            if( status != "alive" ) {
                summary += " (" + status + ")";
            }
        }
        if( !update_member_state( site, member_id, new_state, summary ) ) {
            return false;
        }
    }

    int bandit_losses = 0;
    for( const bandit_pursuit_handoff::anchored_identity_state &update :
         packet.anchored_identity_updates ) {
        if( update.status == "dead" || update.status == "missing" ) {
            bandit_losses++;
        }
    }
    if( shakedown_fight_outing && bandit_losses > 0 ) {
        shakedown_outcome outcome;
        outcome.fought = true;
        outcome.extraction_failed = true;
        outcome.demanded_value = std::max( 1, site.shakedown_last_demanded_value );
        outcome.reachable_goods_value = site.shakedown_last_reachable_value;
        outcome.bandit_losses = bandit_losses;
        apply_shakedown_outcome( site, outcome );
    }

    record_scout_return_lead( site, packet, bandit_losses );

    bandit_pursuit_handoff::abstract_group_state remembered_group = make_site_memory_group( site );
    bandit_pursuit_handoff::apply_return_packet( remembered_group, packet );
    apply_group_memory( site, remembered_group );

    if( scout_return && packet.survivors_remaining > 0 ) {
        scout_report_record report;
        report.revision = std::max( 0, site.current_scout_report.revision ) + 1;
        report.source_activity_id = site.active_outing.activity_id;
        report.source_generation = site.active_outing.generation;
        report.source_job_type = site.active_outing.job_type;
        report.target_id = site.active_outing.target_id;
        report.target_omt = site.active_outing.target_omt;
        report.target_lead_revision = site.active_outing.target_lead_revision;
        report.application_key = site.active_outing.report_application_key;
        report.observations = make_bounded_sortie_observations( site.active_outing.observations );
        report.casualty_ids = site.active_outing.casualty_ids;
        for( const bandit_pursuit_handoff::anchored_identity_state &update :
             packet.anchored_identity_updates ) {
            if( update.status != "dead" && update.status != "missing" ) {
                continue;
            }
            const auto member_token = std::find( active_member_tokens.begin(), active_member_tokens.end(),
                                                 update.id );
            if( member_token == active_member_tokens.end() ) {
                continue;
            }
            const std::size_t member_index = static_cast<std::size_t>(
                    std::distance( active_member_tokens.begin(), member_token ) );
            const character_id casualty_id = site.active_outing.member_ids[member_index];
            if( std::find( report.casualty_ids.begin(), report.casualty_ids.end(), casualty_id ) ==
                report.casualty_ids.end() ) {
                report.casualty_ids.push_back( casualty_id );
            }
        }
        report.delivered_minutes = std::max( { site.active_outing.started_minutes,
                                     site.active_outing.local_contact_minutes,
                                     site.active_outing.last_progress_minutes,
                                     site.active_outing.last_advanced_minutes } );
        site.current_scout_report = std::move( report );
    }
    site.applied_report_generation = scout_return ? packet.activity_generation :
                                     site.applied_report_generation;

    if( packet.survivors_remaining > 0 ) {
        const long long supply_total = static_cast<long long>( site.returned_cargo_stock.supply_units ) +
                                       std::max( 0, site.active_outing.cargo.supply_units );
        const long long trade_total = static_cast<long long>( site.returned_cargo_stock.trade_value ) +
                                      std::max( 0, site.active_outing.cargo.trade_value );
        site.returned_cargo_stock.supply_units = static_cast<int>( std::min<long long>(
                    supply_total, std::numeric_limits<int>::max() ) );
        site.returned_cargo_stock.trade_value = static_cast<int>( std::min<long long>(
                    trade_total, std::numeric_limits<int>::max() ) );
    }
    site.applied_cargo_generation = packet.activity_generation;
    site.last_cargo_application_key = site.active_outing.cargo_application_key;
    site.applied_return_generation = packet.activity_generation;
    site.active_outing.clear();
    if( assessment_scout_return && packet.survivors_remaining > 0 ) {
        accept_current_scout_report_for_assessment( site );
    }
    return true;
}

static bool record_active_outing_casualty_unchecked( site_record &site,
        const character_id npc_id, const member_state casualty_state,
        const int current_minutes, const std::string &summary )
{
    if( !site.active_outing.is_active() ||
        ( casualty_state != member_state::dead && casualty_state != member_state::missing ) ||
        std::find( site.active_outing.member_ids.begin(), site.active_outing.member_ids.end(),
                   npc_id ) == site.active_outing.member_ids.end() ) {
        return false;
    }
    if( casualty_state == member_state::missing &&
        ( site.active_outing.missing_deadline_minutes < 0 ||
          current_minutes < site.active_outing.missing_deadline_minutes ) ) {
        return false;
    }

    site_record candidate = site;
    advance_camp_supply( candidate, current_minutes );
    member_record *member = candidate.find_member( npc_id );
    if( member == nullptr ) {
        return false;
    }
    const bool state_changed = member->state != casualty_state;
    const bool casualty_was_recorded = std::find(
            candidate.active_outing.casualty_ids.begin(),
            candidate.active_outing.casualty_ids.end(), npc_id ) !=
        candidate.active_outing.casualty_ids.end();
    if( !update_member_state( candidate, npc_id, casualty_state, summary ) ) {
        return false;
    }
    advance_camp_supply( candidate, current_minutes );
    if( !casualty_was_recorded ) {
        candidate.active_outing.casualty_ids.push_back( npc_id );
    }
    const bool resolution_was_recorded =
        candidate.active_outing.member_is_resolved( npc_id );
    if( !resolution_was_recorded ) {
        candidate.active_outing.resolved_member_ids.push_back( npc_id );
    }
    candidate.active_outing.last_progress_minutes = std::max(
                candidate.active_outing.last_progress_minutes, current_minutes );
    candidate.active_outing.last_advanced_minutes = current_minutes;
    if( candidate.active_outing.casualty_ids.size() >=
        candidate.active_outing.member_ids.size() ) {
        candidate.active_outing.phase = scout_phase::lost;
    }
    site = std::move( candidate );
    return state_changed || !casualty_was_recorded || !resolution_was_recorded;
}

scout_resolution_effect apply_active_scout_observations( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const std::vector<active_member_observation> &observations,
        const int current_minutes )
{
    scout_resolution_effect effect;
    if( !site.active_outing.is_active() ||
        !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        expected_cursor.owner != simulation_owner::local ||
        site.active_outing.kind != outing_kind::scout_sortie ||
        site.active_outing.job_type != "scout" || current_minutes < 0 ||
        current_minutes <= site.active_outing.last_advanced_minutes ||
        observations.size() != site.active_outing.member_ids.size() ) {
        return effect;
    }

    std::vector<character_id> observed_member_ids;
    observed_member_ids.reserve( observations.size() );
    for( const active_member_observation &observation : observations ) {
        if( std::find( site.active_outing.member_ids.begin(), site.active_outing.member_ids.end(),
                      observation.npc_id ) == site.active_outing.member_ids.end() ||
            std::find( observed_member_ids.begin(), observed_member_ids.end(), observation.npc_id ) !=
            observed_member_ids.end() ) {
            return effect;
        }
        observed_member_ids.push_back( observation.npc_id );
    }

    site_record candidate = site;
    std::vector<character_id> newly_returned_ids;
    int newly_resolved = 0;
    for( const character_id &member_id : candidate.active_outing.member_ids ) {
        const auto observation_iter = std::find_if( observations.begin(), observations.end(),
        [&member_id]( const active_member_observation & observation ) {
            return observation.npc_id == member_id;
        } );
        if( observation_iter == observations.end() ) {
            return effect;
        }

        member_record *member = candidate.find_member( member_id );
        if( member == nullptr ) {
            return effect;
        }
        if( candidate.active_outing.member_is_resolved( member_id ) ) {
            const bool observation_matches_resolution =
                ( member->state == member_state::at_home &&
                  observation_iter->state == active_member_observation_state::home ) ||
                ( member->state == member_state::dead &&
                  observation_iter->state == active_member_observation_state::dead ) ||
                ( member->state == member_state::missing &&
                  observation_iter->state == active_member_observation_state::missing );
            if( !observation_matches_resolution ) {
                return effect;
            }
            continue;
        }

        switch( observation_iter->state ) {
            case active_member_observation_state::unresolved:
            case active_member_observation_state::local_contact:
            case active_member_observation_state::returning_home:
                break;
            case active_member_observation_state::home:
                if( !update_member_state( candidate, member_id, member_state::at_home,
                                         observation_iter->summary ) ) {
                    return effect;
                }
                candidate.active_outing.resolved_member_ids.push_back( member_id );
                newly_returned_ids.push_back( member_id );
                newly_resolved++;
                break;
            case active_member_observation_state::dead:
            case active_member_observation_state::missing: {
                const member_state casualty_state =
                    observation_iter->state == active_member_observation_state::dead ?
                    member_state::dead : member_state::missing;
                if( !record_active_outing_casualty_unchecked(
                        candidate, member_id, casualty_state, current_minutes,
                        observation_iter->summary ) ) {
                    return effect;
                }
                newly_resolved++;
                break;
            }
        }
    }

    effect.valid = true;
    effect.newly_resolved = newly_resolved;
    effect.newly_returned = static_cast<int>( newly_returned_ids.size() );
    const bool all_members_resolved =
        candidate.active_outing.resolved_member_ids.size() ==
        candidate.active_outing.member_ids.size();
    if( newly_resolved == 0 && !all_members_resolved ) {
        return effect;
    }
    if( newly_resolved > 0 ) {
        candidate.active_outing.last_progress_minutes = std::max(
                    candidate.active_outing.last_progress_minutes, current_minutes );
        candidate.active_outing.last_advanced_minutes = std::max(
                    candidate.active_outing.last_advanced_minutes, current_minutes );
    }
    if( all_members_resolved ) {
        const std::optional<bandit_pursuit_handoff::return_packet> packet =
            resolve_active_group_aftermath( candidate, observations );
        if( !packet || !apply_return_packet( candidate, *packet ) ) {
            return scout_resolution_effect();
        }
        site = std::move( candidate );
        effect.changed = true;
        effect.completed = true;
        return effect;
    }

    if( !newly_returned_ids.empty() ) {
        scout_report_record report;
        report.revision = std::max( 0, candidate.current_scout_report.revision ) +
                          static_cast<int>( newly_returned_ids.size() );
        report.source_activity_id = candidate.active_outing.activity_id;
        report.source_generation = candidate.active_outing.generation;
        report.source_job_type = candidate.active_outing.job_type;
        report.target_id = candidate.active_outing.target_id;
        report.target_omt = candidate.active_outing.target_omt;
        report.target_lead_revision = candidate.active_outing.target_lead_revision;
        report.application_key = provisional_report_application_key( candidate );
        report.observations = make_bounded_sortie_observations(
                                  candidate.active_outing.observations );
        report.casualty_ids = candidate.active_outing.casualty_ids;
        report.delivered_minutes = current_minutes;
        report.provisional = true;
        candidate.current_scout_report = std::move( report );
        effect.provisional_report_applied = true;

        const int carried_supply = std::max( 0, candidate.active_outing.cargo.supply_units );
        const int carried_trade = std::max( 0, candidate.active_outing.cargo.trade_value );
        const long long supply_total =
            static_cast<long long>( candidate.returned_cargo_stock.supply_units ) + carried_supply;
        const long long trade_total =
            static_cast<long long>( candidate.returned_cargo_stock.trade_value ) + carried_trade;
        candidate.returned_cargo_stock.supply_units = static_cast<int>( std::min<long long>(
                    supply_total, std::numeric_limits<int>::max() ) );
        candidate.returned_cargo_stock.trade_value = static_cast<int>( std::min<long long>(
                    trade_total, std::numeric_limits<int>::max() ) );
        candidate.active_outing.cargo = sortie_cargo();
        effect.cargo_credited = carried_supply > 0 || carried_trade > 0;
    }

    site = std::move( candidate );
    effect.changed = true;
    return effect;
}

std::optional<bandit_pursuit_handoff::return_packet> resolve_active_group_aftermath(
    const site_record &site, const std::vector<active_member_observation> &observations )
{
    if( !site.active_outing.is_active() || site.active_outing.member_ids.empty() ||
        observations.size() != site.active_outing.member_ids.size() ) {
        return std::nullopt;
    }

    bandit_pursuit_handoff::return_packet packet;
    packet.valid = true;
    packet.group_id = site.active_outing.activity_id;
    packet.source_camp_id = site.site_id;
    packet.activity_generation = site.active_outing.generation;
    packet.handoff_epoch = site.active_outing.handoff_epoch;
    packet.return_application_key = site.active_outing.return_application_key;
    packet.job_type = job_template_from_string( site.active_outing.job_type ).value_or(
                          bandit_dry_run::job_template::hold_chill );
    packet.current_target_or_mark = site.active_outing.target_id;
    packet.result = bandit_pursuit_handoff::mission_result::withdrawn;
    packet.resolution = bandit_pursuit_handoff::lead_resolution::target_lost;
    packet.posture = bandit_pursuit_handoff::return_posture::escape_home;
    packet.remaining_pressure = rules_for_profile( effective_profile( site ) ).default_remaining_pressure;

    bool saw_loss = false;
    for( const character_id &member_id : site.active_outing.member_ids ) {
        const auto iter = std::find_if( observations.begin(), observations.end(),
        [&member_id]( const active_member_observation &observation ) {
            return observation.npc_id == member_id;
        } );
        if( iter == observations.end() ) {
            return std::nullopt;
        }

        switch( iter->state ) {
            case active_member_observation_state::unresolved:
            case active_member_observation_state::local_contact:
            case active_member_observation_state::returning_home:
                return std::nullopt;
            case active_member_observation_state::home:
                packet.survivors_remaining++;
                break;
            case active_member_observation_state::dead:
                saw_loss = true;
                packet.anchored_identity_updates.push_back( { std::to_string( member_id.get_value() ), "dead" } );
                break;
            case active_member_observation_state::missing:
                saw_loss = true;
                packet.anchored_identity_updates.push_back( { std::to_string( member_id.get_value() ), "missing" } );
                break;
        }
    }

    if( saw_loss ) {
        packet.result = packet.survivors_remaining > 0 ?
                        bandit_pursuit_handoff::mission_result::repelled :
                        bandit_pursuit_handoff::mission_result::broken;
        packet.posture = packet.survivors_remaining > 0 ?
                         bandit_pursuit_handoff::return_posture::broken_flee :
                         bandit_pursuit_handoff::return_posture::escape_safe;
        packet.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
    }

    packet.notes.push_back( "resolved live aftermath observations for active owned outing" );
    return packet;
}

bool update_member_state( site_record &site, character_id npc_id, member_state new_state,
                          const std::string &summary )
{
    member_record *member = site.find_member( npc_id );
    if( member == nullptr ) {
        return false;
    }

    const bool old_live = counts_toward_live_headcount( member->state );
    const bool new_live = counts_toward_live_headcount( new_state );
    member->state = new_state;
    if( old_live != new_live ) {
        site.headcount = std::max( 0, site.headcount + ( new_live ? 1 : -1 ) );
        if( spawn_tile_record *spawn_record = site.find_spawn_tile( member->home_spawn_tile ) ) {
            spawn_record->headcount = std::max( 0, spawn_record->headcount + ( new_live ? 1 : -1 ) );
        }
        site.supply_units = std::min( site.supply_units, camp_supply_cap( site ) );
    }

    member->last_writeback_summary = summary;
    return true;
}

bool record_active_outing_casualty( site_record &site,
                                    const simulation_advance_cursor &expected_cursor,
                                    const character_id npc_id,
                                    const member_state casualty_state,
                                    const int current_minutes,
                                    const std::string &summary )
{
    if( !simulation_cursor_matches( site.active_outing, expected_cursor ) ||
        expected_cursor.owner != simulation_owner::local ||
        current_minutes <= site.active_outing.last_advanced_minutes ) {
        return false;
    }
    site_record candidate = site;
    if( !record_active_outing_casualty_unchecked(
            candidate, npc_id, casualty_state, current_minutes, summary ) ) {
        return false;
    }
    site = std::move( candidate );
    return true;
}
} // namespace bandit_live_world
