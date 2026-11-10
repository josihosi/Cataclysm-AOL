#include "writhing_stalker_ai.h"

#include "creature.h"
#include "json.h"
#include "calendar.h"
#include "monster.h"
#include "mtype.h"
#include "overmapbuffer.h"
#include "rng.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include <tuple>
#include <utility>

namespace writhing_stalker
{

void advance_active_work( monster &stalker, const int now_turn )
{
    if( stalker.type->id.str() != "mon_writhing_stalker" || stalker.is_dead() ||
        now_turn != to_turn<int>( calendar::turn ) ) {
        return;
    }
    auto &life = stalker.predator_state();
    const auto &world = overmap_buffer.global_state.predator_activity;
    const auto *actor = world.find( life.actor_id );
    if( !actor || !life.activity_bound ||
        life.activity_adopted_world != actor->activity.adopted_world ||
        !actor->activity.active || actor->activity.last_world != now_turn ||
        actor->activity.adopted_world < 0 || actor->activity.adopted_world > now_turn ||
        actor->type != stalker.type->id.str() ||
        !world.owns( life.actor_id, life.handoff_epoch, actor->owner, stalker.pos_abs() ) ) {
        return;
    }
    auto &state = stalker.writhing_stalker_state();
    const auto active = actor->activity.active_at( now_turn );
    if( now_turn <= state.last_advanced_turn || active < life.activity_active_turns ) {
        return; // A future anchor or repeated callback cannot spend work.
    }
    const int earned = static_cast<int>( std::min<std::int64_t>(
                           std::numeric_limits<int>::max(), active - life.activity_active_turns ) );
    const bool retained = overmap_buffer.predator_light_work_alive( stalker );
    if( state.last_advanced_turn < 0 ) {
        // The native registry already authenticated this interval. Initialize
        // its origin without inventing work before that adoption boundary.
        state.advance_to( actor->activity.adopted_world, 0, retained );
    }
    state.advance_to( now_turn, earned, retained );
    life.activity_active_turns = active;
}

void familiar_place::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "valid", valid );
    json.member( "position", position );
    json.member( "last_seen_world", last_seen_world );
    json.member( "last_seen_active", last_seen_active );
    json.member( "revisit_at_active", revisit_at_active );
    json.end_object();
}

void familiar_place::deserialize( const JsonObject &json )
{
    json.read( "valid", valid );
    json.read( "position", position );
    json.read( "last_seen_world", last_seen_world );
    json.read( "last_seen_active", last_seen_active );
    json.read( "revisit_at_active", revisit_at_active );
}

void persistent_state::suspend_work( const int paused_turns )
{
    if( paused_turns <= 0 ) {
        return;
    }
    // Route progress is earned work too: suspension cannot spend its
    // no-progress allowance while a body is frozen or captured.
    for( int *deadline : { &cooldown_until_turn, &search_until_turn, &last_progress_turn } ) {
        if( *deadline >= 0 ) {
            *deadline = static_cast<int>( std::min<std::int64_t>(
                    std::numeric_limits<int>::max(), static_cast<std::int64_t>( *deadline ) + paused_turns ) );
        }
    }
}

void persistent_state::advance_to( const int now_turn, const int active_elapsed,
                                 const bool retained_light_work )
{
    if( now_turn <= last_advanced_turn ) {
        return;
    }
    const int elapsed = last_advanced_turn < 0 ? 0 :
                        active_elapsed < 0 ? now_turn - last_advanced_turn : active_elapsed;
    active_turns += std::max( 0, elapsed );
    last_advanced_turn = now_turn;
    for( familiar_place &place : familiar ) {
        if( place.valid && active_turns - place.last_seen_active >= to_turns<int>( 7_days ) ) {
            place.valid = false;
        }
    }
    if( light_observed_turn >= 0 && !light_sample_id.empty() &&
        light_expires_turn >= 0 && now_turn >= light_expires_turn ) {
        const bool expired_route = sighting_active_turn < 0 && search_started_active < 0 &&
                                   has_committed_waypoint &&
                                   ( phase == lifecycle_phase::shadowing || phase == lifecycle_phase::approaching );
        has_light_observed_position = false;
        // Acquisition remains a world fact. Retained work may remember this
        // old location, but light_interest_active never calls it current sight.
        if( expired_route && !retained_light_work ) {
            has_committed_waypoint = false;
            phase = lifecycle_phase::idle;
            phase_entered_turn = now_turn;
            remaining_route_progress = -1;
        }
    }
    if( cooldown_until_turn >= 0 && now_turn >= cooldown_until_turn ) {
        const int recovered_at = cooldown_until_turn;
        cooldown_until_turn = -1;
        attempts_spent = 0;
        has_retreat_waypoint = false;
        if( phase == lifecycle_phase::cooldown || phase == lifecycle_phase::retreating ) {
            phase = lifecycle_phase::searching;
            phase_entered_turn = recovered_at;
            search_until_turn = -1;
            if( has_pursuit_memory && sighting_active_turn >= 0 ) {
                begin_search();
                // A delayed executor/reload cannot restart recovery's search.
                search_started_active -= std::min<std::int64_t>( std::max( 0, elapsed ),
                                         now_turn - recovered_at );
            } else {
                search_until_turn = recovered_at + 20;
            }
        }
    }
    // Legacy investigation and bounded pursuit search own navigation only.
    // Combat recovery is consumed above, before a new burst can begin.
    if( search_until_turn >= 0 && now_turn >= search_until_turn ) {
        search_until_turn = -1;
        if( search_started_active < 0 && phase == lifecycle_phase::searching ) {
            phase = lifecycle_phase::idle;
            phase_entered_turn = now_turn;
            has_committed_waypoint = false;
            has_retreat_waypoint = false;
            remaining_route_progress = -1;
        }
    }
}

void persistent_state::record_attack( const int now_turn )
{
    advance_to( now_turn );
    // Real combat takes priority over an older recovery/legacy investigation.
    // Bounded pursuit work keeps its independent elapsed-time budget, but
    // no navigation completion or old recovery can erase these new attempts.
    cooldown_until_turn = -1;
    search_until_turn = -1;
    has_retreat_waypoint = false;
    ++attempts_spent;
    ++attempt_sequence;
    phase = lifecycle_phase::attacking;
    phase_entered_turn = now_turn;
}

bool persistent_state::owns_waypoint( const tripoint_abs_ms &position ) const
{
    return ( has_committed_waypoint && committed_waypoint == position ) ||
           ( has_retreat_waypoint && retreat_waypoint == position );
}

bool persistent_state::arrive_at( const tripoint_abs_ms &position )
{
    if( has_retreat_waypoint && position == retreat_waypoint &&
        ( phase == lifecycle_phase::retreating || phase == lifecycle_phase::cooldown ) ) {
        if( phase == lifecycle_phase::retreating ) {
            phase = lifecycle_phase::cooldown;
            phase_entered_turn = last_advanced_turn;
        }
        return true;
    }
    if( sighting_active_turn < 0 && search_started_active < 0 &&
        light_observed_turn >= 0 && !light_sample_id.empty() &&
        has_committed_waypoint && committed_waypoint == light_observed_position &&
        position == light_observed_position &&
        ( phase == lifecycle_phase::shadowing || phase == lifecycle_phase::approaching ) ) {
        phase = lifecycle_phase::searching;
        phase_entered_turn = last_advanced_turn;
        search_until_turn = last_advanced_turn + 20;
        has_committed_waypoint = false;
        remaining_route_progress = -1;
        return true;
    }
    return false;
}

bool persistent_state::observe_target( const actor_identity target, const int character_id,
                                      const tripoint_abs_ms &position, const int world_turn )
{
    if( target == 0 || world_turn < 0 ||
        ( has_pursuit_memory && world_turn <= evidence_turn ) ) {
        return false;
    }
    const bool same = has_pursuit_memory && ( character_id >= 0 ?
                      observed_character_id == character_id : evidence_target == target );
    if( !same || position != last_observed_position ) {
        has_committed_waypoint = false;
    }
    heading = same ? position - last_observed_position : tripoint_rel_ms::zero;
    heading_interval = same && heading != tripoint_rel_ms::zero ? world_turn - evidence_turn : 0;
    evidence_target = target;
    observed_character_id = character_id;
    evidence_turn = world_turn;
    last_observed_position = position;
    has_last_observed_position = true;
    has_pursuit_memory = true;
    if( phase == lifecycle_phase::searching ) {
        // Recognition ends navigation search, independently of combat recovery.
        search_until_turn = -1;
    }
    if( phase == lifecycle_phase::idle || phase == lifecycle_phase::searching ) {
        phase = lifecycle_phase::shadowing;
    }
    sighting_active_turn = active_turns;
    search_started_active = -1;
    revisiting_slot = -1;
    search_cursor = 0;
    searched_positions = 0;
    const tripoint_abs_omt omt = project_to<coords::omt>( position );
    auto slot = std::find_if( familiar.begin(), familiar.end(), [&]( const familiar_place & place ) {
        return place.valid && place.position == omt;
    } );
    if( slot == familiar.end() ) {
        slot = std::min_element( familiar.begin(), familiar.end(), []( const familiar_place & a,
        const familiar_place & b ) {
            return ( a.valid ? a.last_seen_world : -1 ) < ( b.valid ? b.last_seen_world : -1 );
        } );
    }
    *slot = familiar_place{ true, omt, world_turn, active_turns, -1 };
    return true;
}

void persistent_state::begin_search()
{
    if( search_started_active >= 0 ) {
        return;
    }
    phase = lifecycle_phase::searching;
    phase_entered_turn = last_advanced_turn;
    search_started_active = active_turns;
    search_origin = revisiting_slot >= 0 ? familiar[revisiting_slot].position :
                    project_to<coords::omt>( last_observed_position );
    search_cursor = 0;
    searched_positions = 0;
    has_committed_waypoint = false;
    remaining_route_progress = -1;
}

void persistent_state::finish_search()
{
    phase = lifecycle_phase::idle;
    phase_entered_turn = last_advanced_turn;
    search_started_active = -1;
    revisiting_slot = -1;
    has_committed_waypoint = false;
    has_retreat_waypoint = false;
    heading = tripoint_rel_ms::zero;
    heading_interval = 0;
    sighting_active_turn = -1;
    remaining_route_progress = -1;
    for( familiar_place &place : familiar ) {
        if( place.valid && place.revisit_at_active < active_turns + to_turns<int>( 6_hours ) ) {
            place.revisit_at_active = active_turns + rng( to_turns<int>( 6_hours ),
                                      to_turns<int>( 12_hours ) );
        }
    }
}

std::optional<tripoint_abs_ms> persistent_state::pursuit_waypoint(
    const tripoint_abs_ms &position, const route_resolver &resolve )
{
    if( !has_pursuit_memory || phase == lifecycle_phase::retreating ||
        phase == lifecycle_phase::cooldown ) {
        return std::nullopt;
    }
    if( phase != lifecycle_phase::idle && sighting_active_turn < 0 &&
        search_started_active < 0 ) {
        return std::nullopt;
    }
    if( phase == lifecycle_phase::idle ) {
        for( size_t i = 0; i < familiar.size(); ++i ) {
            if( familiar[i].valid && familiar[i].revisit_at_active >= 0 &&
                active_turns >= familiar[i].revisit_at_active ) {
                revisiting_slot = i;
                familiar[i].revisit_at_active = -1;
                begin_search();
                break;
            }
        }
        if( phase == lifecycle_phase::idle ) {
            return std::nullopt;
        }
    }
    if( search_started_active < 0 ) {
        const std::int64_t age = active_turns - sighting_active_turn;
        if( age <= to_turns<int>( 60_seconds ) ) {
            tripoint_rel_ms offset = tripoint_rel_ms::zero;
            if( heading_interval > 0 ) {
                const double scale = static_cast<double>( std::max<std::int64_t>( 0, age ) ) / heading_interval;
                const double bound = std::max( std::abs( heading.x() ), std::abs( heading.y() ) ) * scale;
                const double limited = bound > 24 ? scale * 24 / bound : scale;
                offset = tripoint_rel_ms( static_cast<int>( std::lround( heading.x() * limited ) ),
                                         static_cast<int>( std::lround( heading.y() * limited ) ), 0 );
            }
            const auto waypoint = resolve( last_observed_position + offset, false );
            if( waypoint && *waypoint != position ) {
                committed_waypoint = *waypoint;
                has_committed_waypoint = true;
                return waypoint;
            }
        }
        begin_search();
        if( age > to_turns<int>( 60_seconds ) ) {
            // A delayed/reloaded executor cannot renew the immediate search
            // budget merely by first noticing that its projection expired.
            search_started_active = sighting_active_turn + to_turns<int>( 60_seconds );
        }
    }
    const int budget = revisiting_slot >= 0 ? to_turns<int>( 10_minutes ) :
                       to_turns<int>( 30_minutes );
    if( active_turns - search_started_active >= budget ) {
        finish_search();
        return std::nullopt;
    }
    static const std::array<point, 9> offsets = {{ point::zero, point::north, point::north_east,
            point::east, point::south_east, point::south, point::south_west, point::west, point::north_west
        }};
    const int limit = revisiting_slot >= 0 ? 1 : offsets.size();
    while( search_cursor < limit ) {
        const tripoint_abs_omt omt = search_origin + offsets[search_cursor];
        if( has_committed_waypoint && position == committed_waypoint ) {
            has_committed_waypoint = false;
            if( project_to<coords::omt>( position ) == omt ) {
                searched_positions |= 1u << search_cursor;
                ++search_cursor;
                continue;
            }
        }
        const tripoint_abs_ms center = project_to<coords::ms>( omt ) + point( 12, 12 );
        const auto waypoint = resolve( center, true );
        if( waypoint && ( *waypoint != position || project_to<coords::omt>( position ) == omt ) ) {
            committed_waypoint = *waypoint;
            has_committed_waypoint = true;
            return waypoint;
        }
        // A refusal is not a searched position and cannot refresh place memory.
        ++search_cursor;
        has_committed_waypoint = false;
    }
    finish_search();
    return std::nullopt;
}

bool persistent_state::has_direct_evidence( const int now_turn ) const
{
    return evidence_target != 0 && has_last_observed_position && evidence_turn >= 0 &&
           now_turn >= evidence_turn && now_turn - evidence_turn <= 200;
}

bool persistent_state::light_interest_active( const int now_turn ) const
{
    return has_light_observed_position && light_observed_turn >= 0 && light_expires_turn > now_turn;
}

bool persistent_state::observe_light_interest( const tripoint_abs_ms &position,
        const std::string &sample_id, const int observed_turn, const int duration_turns )
{
    if( sample_id.empty() || observed_turn < 0 || duration_turns <= 0 ||
        has_direct_evidence( observed_turn ) || sample_id == light_sample_id ||
        observed_turn <= light_observed_turn || phase == lifecycle_phase::approaching ||
        phase == lifecycle_phase::attacking || phase == lifecycle_phase::retreating ||
        phase == lifecycle_phase::cooldown ) {
        return false;
    }
    light_observed_position = position;
    has_light_observed_position = true;
    light_observed_turn = observed_turn;
    light_expires_turn = observed_turn + duration_turns;
    light_sample_id = sample_id;
    if( phase == lifecycle_phase::idle ) {
        phase = lifecycle_phase::shadowing;
        phase_entered_turn = observed_turn;
        committed_waypoint = position;
        has_committed_waypoint = true;
        remaining_route_progress = -1;
    }
    return true;
}

bool persistent_state::cooldown_active( const int now_turn ) const
{
    return cooldown_until_turn >= 0 && now_turn < cooldown_until_turn;
}

void persistent_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "phase", static_cast<int>( phase ) );
    json.member( "evidence_target", evidence_target );
    json.member( "evidence_turn", evidence_turn );
    json.member( "has_last_observed_position", has_last_observed_position );
    json.member( "last_observed_position", last_observed_position );
    json.member( "has_light_observed_position", has_light_observed_position );
    json.member( "light_observed_position", light_observed_position );
    json.member( "light_observed_turn", light_observed_turn );
    json.member( "light_expires_turn", light_expires_turn );
    json.member( "light_sample_id", light_sample_id );
    json.member( "has_committed_waypoint", has_committed_waypoint );
    json.member( "committed_waypoint", committed_waypoint );
    json.member( "has_retreat_waypoint", has_retreat_waypoint );
    json.member( "retreat_waypoint", retreat_waypoint );
    json.member( "attempts_spent", attempts_spent );
    json.member( "attempt_sequence", attempt_sequence );
    json.member( "phase_entered_turn", phase_entered_turn );
    json.member( "cooldown_until_turn", cooldown_until_turn );
    json.member( "last_progress_turn", last_progress_turn );
    json.member( "last_advanced_turn", last_advanced_turn );
    json.member( "remaining_route_progress", remaining_route_progress );
    json.member( "search_until_turn", search_until_turn );
    json.member( "active_turns", active_turns );
    json.member( "observed_character_id", observed_character_id );
    json.member( "has_pursuit_memory", has_pursuit_memory );
    json.member( "sighting_active_turn", sighting_active_turn );
    json.member( "heading", heading );
    json.member( "heading_interval", heading_interval );
    json.member( "search_started_active", search_started_active );
    json.member( "search_cursor", search_cursor );
    json.member( "searched_positions", searched_positions );
    json.member( "search_origin", search_origin );
    json.member( "revisiting_slot", revisiting_slot );
    json.member( "familiar", familiar );
    json.end_object();
}

void persistent_state::deserialize( const JsonObject &json )
{
    json.allow_omitted_members();
    const int raw_phase = json.get_int( "phase", static_cast<int>( lifecycle_phase::idle ) );
    phase = raw_phase >= static_cast<int>( lifecycle_phase::idle ) &&
            raw_phase <= static_cast<int>( lifecycle_phase::cooldown ) ?
            static_cast<lifecycle_phase>( raw_phase ) : lifecycle_phase::idle;
    json.read( "evidence_target", evidence_target );
    json.read( "evidence_turn", evidence_turn );
    json.read( "has_last_observed_position", has_last_observed_position );
    json.read( "last_observed_position", last_observed_position );
    json.read( "has_light_observed_position", has_light_observed_position );
    json.read( "light_observed_position", light_observed_position );
    json.read( "light_observed_turn", light_observed_turn );
    json.read( "light_expires_turn", light_expires_turn );
    json.read( "light_sample_id", light_sample_id );
    json.read( "has_committed_waypoint", has_committed_waypoint );
    json.read( "committed_waypoint", committed_waypoint );
    json.read( "has_retreat_waypoint", has_retreat_waypoint );
    json.read( "retreat_waypoint", retreat_waypoint );
    json.read( "attempts_spent", attempts_spent );
    json.read( "attempt_sequence", attempt_sequence );
    json.read( "phase_entered_turn", phase_entered_turn );
    json.read( "cooldown_until_turn", cooldown_until_turn );
    json.read( "last_progress_turn", last_progress_turn );
    json.read( "last_advanced_turn", last_advanced_turn );
    json.read( "remaining_route_progress", remaining_route_progress );
    json.read( "search_until_turn", search_until_turn );
    json.read( "active_turns", active_turns );
    json.read( "observed_character_id", observed_character_id );
    json.read( "has_pursuit_memory", has_pursuit_memory );
    json.read( "sighting_active_turn", sighting_active_turn );
    json.read( "heading", heading );
    json.read( "heading_interval", heading_interval );
    json.read( "search_started_active", search_started_active );
    json.read( "search_cursor", search_cursor );
    json.read( "searched_positions", searched_positions );
    json.read( "search_origin", search_origin );
    json.read( "revisiting_slot", revisiting_slot );
    json.read( "familiar", familiar );
    if( !json.has_member( "active_turns" ) ) {
        last_advanced_turn = -1;
    }
    active_turns = std::max<std::int64_t>( 0, active_turns );
    search_cursor = std::clamp( search_cursor, 0, 9 );
    revisiting_slot = std::clamp( revisiting_slot, -1, 1 );
}

namespace
{

struct observed_attack {
    actor_identity observer = 0;
    actor_identity attacker = 0;
    actor_identity target = 0;
    resolution_id resolution = 0;
    int turn = 0;
    bool response_claimed = false;
};

std::vector<observed_attack> observed_attacks;
resolution_id next_resolution = 0;

constexpr int fresh_human_evidence_minutes = 20;
constexpr int stale_human_evidence_minutes = 45;
constexpr int latch_timeout_minutes = 30;
constexpr int default_latch_leash_tiles = 60;
constexpr int withdrawal_cooldown_minutes = 10;
constexpr int faded_latch_cooldown_minutes = 5;
constexpr int cautious_stalk_distance_omt = 5;

int signum( const int value )
{
    if( value < 0 ) {
        return -1;
    }
    if( value > 0 ) {
        return 1;
    }
    return 0;
}

void keep_strongest( interest_report &best, interest_source source, int score, int confidence,
                     bool can_latch, std::string reason )
{
    if( score > best.score ) {
        best.source = source;
        best.score = score;
        best.confidence = confidence;
        best.can_latch = can_latch;
        best.reason = std::move( reason );
    }
}

int caution_pressure( const opportunity_context &ctx )
{
    int pressure = 0;
    if( ctx.bright_exposure ) {
        pressure += 2;
    }
    if( ctx.player_focused ) {
        pressure += 1;
    }
    if( ctx.stalker_hurt ) {
        pressure += 3;
    }
    if( ctx.counterpressure_recent ) {
        pressure += 3;
    }
    pressure += std::min( 2, std::max( 0, ctx.allied_support_nearby ) );
    return pressure;
}

int vulnerability_pressure( const opportunity_context &ctx )
{
    int pressure = 0;
    if( ctx.player_bleeding ) {
        ++pressure;
    }
    if( ctx.player_hurt ) {
        ++pressure;
    }
    if( ctx.player_low_stamina ) {
        ++pressure;
    }
    if( ctx.player_distracted ) {
        ++pressure;
    }
    if( ctx.player_noisy ) {
        ++pressure;
    }
    if( ctx.zombie_pressure >= 2 ) {
        ++pressure;
    }
    if( ctx.zombie_pressure >= 4 ) {
        ++pressure;
    }
    return pressure;
}

int burst_limit_for( const opportunity_context &ctx )
{
    int limit = 2;
    if( vulnerability_pressure( ctx ) >= 4 && caution_pressure( ctx ) == 0 ) {
        ++limit;
    }
    if( vulnerability_pressure( ctx ) >= 6 && caution_pressure( ctx ) == 0 ) {
        ++limit;
    }
    if( ctx.bright_exposure || ctx.player_focused ) {
        --limit;
    }
    if( ctx.allied_support_nearby >= 2 ) {
        --limit;
    }
    if( ctx.stalker_hurt ) {
        limit = 1;
    }
    return std::max( 1, std::min( 4, limit ) );
}

int retreat_distance_for( const opportunity_context &ctx )
{
    int distance = 8;
    if( ctx.bright_exposure ) {
        distance += 2;
    }
    if( ctx.player_focused ) {
        distance += 1;
    }
    if( ctx.allied_support_nearby >= 2 ) {
        distance += 2;
    }
    if( ctx.stalker_hurt ) {
        distance += 2;
    }
    return std::min( 14, distance );
}

} // namespace

resolution_id next_resolution_id()
{
    return ++next_resolution;
}

void record_observed_attack( const Creature *observer, const Creature *attacker,
                             const Creature *target, const int turn,
                             const resolution_id resolution )
{
    if( observer == nullptr || attacker == nullptr || target == nullptr || resolution == 0 ) {
        return;
    }
    const observed_attack event{ observer->get_identity(), attacker->get_identity(),
                                 target->get_identity(), resolution, turn };
    const auto duplicate = std::find_if( observed_attacks.begin(), observed_attacks.end(),
    [&event]( const observed_attack & existing ) {
        return existing.observer == event.observer && existing.attacker == event.attacker &&
               existing.target == event.target && existing.resolution == event.resolution;
    } );
    if( duplicate != observed_attacks.end() ) {
        return;
    }
    observed_attacks.push_back( event );
    if( observed_attacks.size() > 512 ) {
        observed_attacks.erase( observed_attacks.begin(), observed_attacks.begin() + 128 );
    }
}

resolution_id recent_observed_attack_resolution( const Creature *observer, const Creature *attacker,
        const Creature *target, const int now_turn, const int max_age )
{
    if( observer == nullptr || attacker == nullptr || target == nullptr ) {
        return 0;
    }
    const actor_identity observer_id = observer->get_identity();
    const actor_identity attacker_id = attacker->get_identity();
    const actor_identity target_id = target->get_identity();
    for( auto it = observed_attacks.rbegin(); it != observed_attacks.rend(); ++it ) {
        if( it->observer == observer_id && it->attacker == attacker_id &&
            it->target == target_id && it->turn <= now_turn &&
            now_turn - it->turn <= max_age ) {
            return it->resolution;
        }
    }
    return 0;
}

bool has_recent_observed_attack( const Creature *observer, const Creature *attacker,
                                 const Creature *target, const int now_turn, const int max_age )
{
    return recent_observed_attack_resolution( observer, attacker, target, now_turn, max_age ) != 0;
}

bool claim_observed_attack_response( const Creature *observer, const Creature *attacker,
                                     const Creature *target, const resolution_id resolution )
{
    if( observer == nullptr || attacker == nullptr || target == nullptr || resolution == 0 ) {
        return false;
    }
    for( observed_attack &event : observed_attacks ) {
        if( event.observer == observer->get_identity() && event.attacker == attacker->get_identity() &&
            event.target == target->get_identity() && event.resolution == resolution ) {
            if( event.response_claimed ) {
                return false;
            }
            event.response_claimed = true;
            return true;
        }
    }
    return false;
}

int observed_attack_count( const Creature *observer, const Creature *attacker,
                           const Creature *target, const resolution_id resolution )
{
    if( observer == nullptr || attacker == nullptr || target == nullptr || resolution == 0 ) {
        return 0;
    }
    const actor_identity observer_id = observer->get_identity();
    const actor_identity attacker_id = attacker->get_identity();
    const actor_identity target_id = target->get_identity();
    return static_cast<int>( std::count_if( observed_attacks.begin(), observed_attacks.end(),
    [&]( const observed_attack & event ) {
        return event.observer == observer_id && event.attacker == attacker_id &&
               event.target == target_id && event.resolution == resolution;
    } ) );
}

bool is_meaningful_pressure( const pressure_observation &observation )
{
    // A monster's native attack target and destination are direct evidence of
    // current pursuit.  Do not make a newly arrived stalker wait for a second
    // frame merely to rediscover the same pursuit through motion or damage:
    // the bright-exposure latch is evaluated on that first frame too.
    return observation.hostile && observation.visible_to_stalker && observation.same_target;
}

interest_report evaluate_interest( const interest_context &ctx )
{
    interest_report best;
    best.reason = "empty_terrain";

    if( ctx.recent_human_evidence ) {
        if( ctx.evidence_age_minutes <= fresh_human_evidence_minutes ) {
            keep_strongest( best, interest_source::human, 90, 4, true,
                            "recent_human_evidence" );
        } else if( ctx.evidence_age_minutes <= stale_human_evidence_minutes ) {
            keep_strongest( best, interest_source::human, 42, 2, true,
                            "fading_human_evidence" );
        } else {
            keep_strongest( best, interest_source::human, 8, 1, false,
                            "stale_human_evidence" );
        }
    }

    if( ctx.exposed_night_light ) {
        keep_strongest( best, interest_source::light, 60, 3, false,
                        "exposed_night_light" );
    }

    if( ctx.forest_or_building_edge ) {
        keep_strongest( best, interest_source::terrain, 34, 2, false,
                        "forest_or_building_edge_cover" );
    } else if( ctx.town_or_road_edge ) {
        keep_strongest( best, interest_source::terrain, 24, 1, false,
                        "town_or_road_edge_scent" );
    }

    if( ctx.zombie_pressure > 0 ) {
        keep_strongest( best, interest_source::zombie_pressure,
                        std::min( 36, 10 + ctx.zombie_pressure * 6 ), 2, false,
                        "zombie_pressure_opportunity" );
    }

    if( ctx.smoke ) {
        keep_strongest( best, interest_source::smoke, 14, 1, false,
                        "smoke_indirect_human_hint" );
    }

    return best;
}

latch_update advance_latch( const latch_context &ctx )
{
    latch_update result;
    result.state = ctx.current;
    result.state.cooldown_minutes = std::max( 0, result.state.cooldown_minutes -
                                    ctx.elapsed_minutes );

    if( result.state.active ) {
        result.state.age_minutes += std::max( 0, ctx.elapsed_minutes );

        if( ctx.exposed_or_focused ) {
            result.state.active = false;
            result.state.cooldown_minutes = withdrawal_cooldown_minutes;
            result.next = decision::cooling_off;
            result.reason = "latch_broken_by_exposure_or_focus";
            return result;
        }

        if( result.state.age_minutes > latch_timeout_minutes ) {
            result.state.active = false;
            result.state.cooldown_minutes = faded_latch_cooldown_minutes;
            result.next = decision::cooling_off;
            result.reason = "latch_timed_out";
            return result;
        }

        if( ctx.distance_tiles > 0 && ctx.distance_tiles > result.state.leash_tiles_remaining ) {
            result.state.active = false;
            result.state.cooldown_minutes = faded_latch_cooldown_minutes;
            result.next = decision::cooling_off;
            result.reason = "latch_leash_broken";
            return result;
        }
    }

    if( ctx.interest.can_latch && result.state.cooldown_minutes == 0 ) {
        result.state.active = true;
        result.state.age_minutes = 0;
        result.state.confidence = std::max( result.state.confidence, ctx.interest.confidence );
        result.state.leash_tiles_remaining = default_latch_leash_tiles;
        result.next = decision::shadow;
        result.reason = ctx.current.active ? "latch_refreshed_by_new_evidence" :
                        "latch_created_by_believable_evidence";
        return result;
    }

    if( result.state.active ) {
        result.next = decision::shadow;
        result.reason = "latch_continues_without_refresh";
    } else if( result.state.cooldown_minutes > 0 ) {
        result.next = decision::cooling_off;
        result.reason = "cooldown_blocks_relatched_pressure";
    } else {
        result.next = ctx.interest.score > 0 ? decision::interested : decision::ignore;
        result.reason = ctx.interest.score > 0 ? "interest_without_latch" : "no_believable_evidence";
    }

    return result;
}

approach_report choose_approach( const approach_context &ctx )
{
    approach_report report;

    if( !ctx.latch.active ) {
        report.reason = "no_latch_no_approach";
        return report;
    }

    if( ctx.bright_exposure ) {
        report.route = approach_class::hold_exposed;
        report.next = decision::hold;
        report.avoids_direct_line = true;
        report.reason = "bright_exposure_holds_shadow";
        return report;
    }

    if( ctx.cover_route_available ) {
        report.route = approach_class::cover_shadow;
        report.next = decision::shadow;
        report.avoids_direct_line = true;
        report.reason = "cover_route_preferred";
        return report;
    }

    if( ctx.edge_route_available ) {
        report.route = approach_class::edge_shadow;
        report.next = decision::shadow;
        report.avoids_direct_line = true;
        report.reason = "edge_route_preferred";
        return report;
    }

    if( ctx.direct_open_route_available && ctx.forced_no_cover ) {
        report.route = approach_class::direct_forced;
        report.next = decision::shadow;
        report.avoids_direct_line = false;
        report.reason = "direct_route_only_when_forced_by_map";
        return report;
    }

    report.route = approach_class::hold_exposed;
    report.next = decision::hold;
    report.avoids_direct_line = true;
    report.reason = "no_safe_shadow_route";
    return report;
}


threat_report evaluate_threat_state( const threat_context &ctx )
{
    threat_report report;
    report.reason = "no_threat_state";

    report.threat_score = ctx.allied_support_nearby * 18;
    if( ctx.daylight_or_bright ) {
        report.threat_score += 30;
    }
    if( ctx.strong_visibility ) {
        report.threat_score += 20;
    }
    const bool close_pressure = ctx.close_overmap_pressure || ctx.same_or_close_overmap;
    if( close_pressure ) {
        report.threat_score += 12;
    }
    if( ctx.spent_cooldown || ctx.burst_strikes >= ctx.burst_limit ) {
        report.threat_score += 18;
    }
    if( ctx.has_distraction ) {
        report.threat_score -= 24;
    }

    if( ctx.night_or_dark ) {
        report.opportunity_score += 24;
    }
    if( ctx.outside_reachable_player ) {
        report.opportunity_score += 24;
    }
    if( ctx.valid_evidence_or_path ) {
        report.opportunity_score += 16;
    }
    if( ctx.has_distraction ) {
        report.opportunity_score += 30;
    }
    if( !( ctx.has_distraction && ctx.night_or_dark ) ) {
        report.opportunity_score -= std::min( 30, ctx.allied_support_nearby * 10 );
    }
    if( ctx.daylight_or_bright ) {
        report.opportunity_score -= 20;
    }

    if( ctx.spent_cooldown || ctx.burst_strikes >= ctx.burst_limit ) {
        report.state = threat_state::spent_disengage;
        report.next = decision::withdraw;
        report.stalk_distance_omt = cautious_stalk_distance_omt;
        report.stalking_distance_omt = cautious_stalk_distance_omt;
        report.avoid_sight_tiles = true;
        report.avoids_sight_tiles = true;
        report.reason = "spent_disengage_memory_withdraw";
        return report;
    }

    if( close_pressure && ctx.daylight_or_bright && ctx.allied_support_nearby >= 3 &&
        ( ctx.strong_visibility || ctx.target_has_focus ) && !ctx.has_distraction ) {
        report.overmatched = true;
        report.state = threat_state::overmatched_retreat;
        report.next = decision::withdraw;
        report.intent = handoff_intent::overmatched_stalk;
        report.stalk_distance_omt = cautious_stalk_distance_omt;
        report.stalking_distance_omt = cautious_stalk_distance_omt;
        report.threat_memory = std::max( 3, ctx.allied_support_nearby );
        report.avoid_sight_tiles = true;
        report.avoids_sight_tiles = true;
        report.reason = "high_threat_allied_light_retreat_stalk";
        return report;
    }

    if( ctx.night_or_dark && ctx.outside_reachable_player && ctx.valid_evidence_or_path &&
        ctx.bad_position_loiter_turns >= 3 ) {
        report.state = threat_state::opportunity_probe;
        report.next = decision::shadow;
        report.anti_gnome_fired = true;
        report.avoids_sight_tiles = true;
        report.reason = "anti_gnome_dark_reposition_probe";
        return report;
    }

    if( report.opportunity_score >= 70 ) {
        report.state = threat_state::committed_ambush;
        report.next = decision::strike;
        report.reason = "dark_distraction_committed_ambush";
    } else if( report.opportunity_score >= 45 ) {
        report.state = threat_state::opportunity_probe;
        report.next = decision::shadow;
        report.avoids_sight_tiles = true;
        report.reason = "dark_or_distraction_opportunity_probe";
    } else if( report.threat_score >= 55 && !ctx.has_distraction &&
               ctx.allied_support_nearby >= 3 ) {
        report.state = threat_state::overmatched_retreat;
        report.next = decision::withdraw;
        report.overmatched = true;
        report.intent = handoff_intent::overmatched_stalk;
        report.stalk_distance_omt = cautious_stalk_distance_omt;
        report.stalking_distance_omt = cautious_stalk_distance_omt;
        report.threat_memory = std::max( 3, ctx.allied_support_nearby );
        report.avoid_sight_tiles = true;
        report.avoids_sight_tiles = true;
        report.reason = "overmatched_retreat_by_threat_score";
    } else {
        report.state = threat_state::watching;
        report.next = decision::hold;
        report.avoids_sight_tiles = true;
        report.reason = "watching_no_commitment";
    }

    return report;
}

opportunity_report evaluate_opportunity( const opportunity_context &ctx )
{
    opportunity_report report;

    if( !ctx.latch.active ) {
        report.next = decision::ignore;
        report.reason = "no_latch_no_strike";
        return report;
    }

    if( ctx.latch.cooldown_minutes > 0 ) {
        report.next = decision::cooling_off;
        report.reason = "cooldown_blocks_repeat_strike";
        return report;
    }

    if( ctx.bright_exposure && ctx.allied_support_nearby >= 3 && ctx.zombie_pressure == 0 &&
        ctx.burst_strikes == 0 ) {
        report.burst_limit = burst_limit_for( ctx );
        report.retreat_distance = retreat_distance_for( ctx );
        report.next = decision::withdraw;
        report.reason = "overmatched_three_allies_daylight_withdraw";
        return report;
    }

    if( ctx.player_bleeding ) {
        report.vulnerability += 22;
    }
    if( ctx.player_hurt ) {
        report.vulnerability += 18;
    }
    if( ctx.player_low_stamina ) {
        report.vulnerability += 16;
    }
    if( ctx.player_distracted ) {
        report.vulnerability += 18;
    }
    if( ctx.player_noisy ) {
        report.vulnerability += 10;
    }

    report.zombie_distraction = std::min( 28, ctx.zombie_pressure * 7 );

    if( ctx.bright_exposure ) {
        // Brightness is a finite risk, not an absolute veto. Active same-target
        // pressure is precisely the opening this predator exploits in daylight.
        report.exposure_penalty += std::max( 0, 35 - std::min( 28, ctx.zombie_pressure * 7 ) );
    }
    if( ctx.player_focused ) {
        report.exposure_penalty += 25;
    }
    if( ctx.allied_support_nearby > 0 ) {
        report.exposure_penalty += std::min( 36, ctx.allied_support_nearby * 12 );
    }
    if( ctx.stalker_hurt ) {
        report.exposure_penalty += 45;
    }

    report.burst_limit = burst_limit_for( ctx );
    report.retreat_distance = retreat_distance_for( ctx );
    report.opportunity = 18 + report.vulnerability + report.zombie_distraction -
                         report.exposure_penalty;
    if( ctx.near_cover_or_clutter ) {
        report.opportunity += 12;
    }

    // Being seen in bright light normally breaks an ambush.  Do not turn that
    // caution into immunity when several observed zombies are actively
    // pressuring the same target: at close range that is the bounded opening
    // handled by the heavy-pressure strike branch below.
    if( ctx.stalker_hurt || ( ctx.bright_exposure && ctx.player_focused &&
                              ctx.zombie_pressure < 4 ) ) {
        report.next = decision::withdraw;
        report.reason = ctx.stalker_hurt ? "stalker_hurt_withdraw" :
                        "exposed_and_focused_withdraw";
    } else if( ctx.burst_strikes >= report.burst_limit &&
               ( ctx.distance_to_target <= 0 || ctx.distance_to_target < report.retreat_distance ) ) {
        report.next = decision::withdraw;
        report.reason = "burst_limit_reached_withdraw";
    } else if( ctx.bright_exposure && ctx.allied_support_nearby >= 2 && ctx.burst_strikes > 0 ) {
        report.next = decision::withdraw;
        report.reason = "lit_allied_support_withdraw";
    } else if( report.opportunity >= 70 || ( ctx.zombie_pressure >= 4 &&
               ctx.distance_to_target > 0 &&
               ctx.distance_to_target <= 3 &&
               ctx.allied_support_nearby < 3 ) ) {
        report.next = decision::strike;
        report.reason = report.opportunity >= 70 ? "vulnerability_window_strike" :
                        "heavy_zombie_pressure_strike";
    } else if( report.opportunity >= 36 ) {
        report.next = decision::shadow;
        report.reason = "opportunity_building_shadow";
    } else {
        report.next = decision::hold;
        report.reason = "alert_or_exposed_hold";
    }

    return report;
}

quiet_side_report evaluate_quiet_side( const std::vector<relative_point> &zombies )
{
    quiet_side_report report;

    for( const relative_point &zombie : zombies ) {
        const int weight = std::max( 1, zombie.weight );
        report.pressure_x += signum( zombie.rel_x ) * weight;
        report.pressure_y += signum( zombie.rel_y ) * weight;
        report.pressure_count += weight;
    }

    const int abs_x = std::abs( report.pressure_x );
    const int abs_y = std::abs( report.pressure_y );
    const int strength = abs_x + abs_y;
    report.has_dominant_pressure = report.pressure_count > 0 && strength >=
                                   std::max( 1, report.pressure_count / 2 );
    report.ambiguous_pressure = report.pressure_count > 1 && !report.has_dominant_pressure;

    if( report.has_dominant_pressure ) {
        if( abs_x >= abs_y ) {
            report.quiet_x = -signum( report.pressure_x );
        } else {
            report.quiet_y = -signum( report.pressure_y );
        }
    }

    return report;
}

quiet_candidate_report choose_quiet_side_cutoff( const std::vector<relative_point> &zombies,
        const std::vector<quiet_candidate> &candidates )
{
    quiet_candidate_report report;
    report.pressure = evaluate_quiet_side( zombies );
    report.score = INT_MIN;
    report.reason = "no_candidate";

    for( const quiet_candidate &candidate : candidates ) {
        if( !candidate.passable || candidate.occupied ) {
            continue;
        }

        int score = 20 - candidate.distance_to_stalker;
        if( candidate.shadow_or_cover ) {
            score += 18;
        }
        if( candidate.broken_line_of_sight ) {
            score += 14;
        }
        if( !candidate.bright_exposure ) {
            score += 8;
        }
        score += std::max( -2, std::min( 4, candidate.retreat_alignment ) ) * 4;

        int quiet_alignment = 0;
        int crowding_penalty = 0;
        if( report.pressure.has_dominant_pressure ) {
            quiet_alignment = signum( candidate.rel_x ) * report.pressure.quiet_x +
                              signum( candidate.rel_y ) * report.pressure.quiet_y;
            const int pressure_alignment = signum( candidate.rel_x ) * signum( report.pressure.pressure_x ) +
                                           signum( candidate.rel_y ) * signum( report.pressure.pressure_y );
            if( quiet_alignment > 0 ) {
                score += quiet_alignment * 12;
            } else {
                score -= 10;
            }
            if( pressure_alignment > 0 ) {
                crowding_penalty = pressure_alignment * 8;
                score -= crowding_penalty;
            }
        }

        if( !report.has_candidate || score > report.score ) {
            report.has_candidate = true;
            report.chosen = candidate;
            report.score = score;
            report.quiet_alignment = quiet_alignment;
            report.crowding_penalty = crowding_penalty;
            if( report.pressure.ambiguous_pressure ) {
                report.reason = "ambiguous_pressure_no_precise_quiet_side";
            } else if( quiet_alignment > 0 ) {
                report.reason = "quiet_side_cutoff_preferred";
            } else {
                report.reason = "best_shadow_without_quiet_side";
            }
        }
    }

    return report;
}

confidence_report evaluate_confidence( const confidence_context &ctx )
{
    confidence_report report;
    report.evidence = ctx.has_believable_local_evidence ? 36 : 0;
    report.interest = ctx.has_overmap_interest_footing ? 24 : 0;
    report.pressure_allowed = report.evidence > 0 || report.interest > 0;
    if( report.pressure_allowed ) {
        report.zombie_pressure = std::min( 28, ctx.zombie_pressure * 7 );
    }

    if( ctx.target_in_bright_exposure ) {
        report.counterpressure += 25;
    }
    if( ctx.stalker_in_bright_exposure ) {
        report.counterpressure += 25;
    }
    if( ctx.target_has_focus ) {
        report.counterpressure += 25;
    }
    if( ctx.open_exposure ) {
        report.counterpressure += 15;
    }
    if( ctx.stalker_hurt ) {
        report.counterpressure += 45;
    }

    report.cutoff_allowed = report.pressure_allowed && ctx.quiet_side_cutoff_available &&
                            report.counterpressure == 0;
    if( report.cutoff_allowed ) {
        report.quiet_side_cutoff = 16;
    }

    report.total = report.evidence + report.interest + report.zombie_pressure +
                   report.quiet_side_cutoff - report.counterpressure;
    if( !report.pressure_allowed && ctx.zombie_pressure > 0 ) {
        report.reason = "no_evidence_or_interest_pressure_ignored";
    } else if( report.counterpressure > 0 ) {
        report.reason = "counterpressure_suppresses_cutoff";
    } else if( report.quiet_side_cutoff > 0 ) {
        report.reason = "zombie_shadow_quiet_cutoff_confidence";
    } else if( report.zombie_pressure > 0 ) {
        report.reason = "zombie_shadow_pressure_confidence";
    } else {
        report.reason = report.pressure_allowed ? "evidence_confidence" : "no_confidence";
    }

    return report;
}


threat_report evaluate_threat( const threat_context &ctx )
{
    threat_context normalized = ctx;
    normalized.close_overmap_pressure = normalized.close_overmap_pressure ||
                                        normalized.same_or_close_overmap;
    normalized.strong_visibility = normalized.strong_visibility || normalized.target_has_focus;
    normalized.has_distraction = normalized.has_distraction || normalized.zombie_pressure >= 2;

    threat_report report = evaluate_threat_state( normalized );

    if( report.overmatched && ctx.allied_support_nearby >= 3 && ctx.daylight_or_bright &&
        ctx.target_has_focus && ctx.same_or_close_overmap && !normalized.has_distraction ) {
        report.reason = ctx.burst_strikes > 0 ? "lit_allied_burst_spent_retreat_stalk" :
                        "high_threat_allied_light_retreat_stalk";
    }

    if( !report.overmatched ) {
        report.reason = "threat_not_overmatched";
    }

    return report;
}

live_response resolve_anti_gnome( const anti_gnome_context &ctx )
{
    live_response response;
    response.reason = "anti_gnome_not_triggered";

    if( !ctx.night_outside_reachable_target || !ctx.has_believable_evidence_or_handoff ||
        ctx.high_threat || ctx.loiter_turns < 2 ) {
        return response;
    }

    response.anti_gnome_triggered = true;
    response.persistent_state_required = true;

    if( ctx.distance_to_target <= 3 ) {
        response.next = decision::strike;
        response.writeback_intent = handoff_intent::committed_ambush;
        response.reason = "anti_gnome_night_reachable_probe_strike";
        return response;
    }

    if( ctx.dark_or_covered_route_available ) {
        response.next = decision::shadow;
        response.route = approach_class::cover_shadow;
        response.writeback_intent = handoff_intent::opportunity_probe;
        response.reason = "anti_gnome_night_reposition_to_dark_route";
        return response;
    }

    response.next = decision::withdraw;
    response.writeback_intent = handoff_intent::spent_disengage;
    response.writeback_cooldown_minutes = 5;
    response.reason = "anti_gnome_no_reachable_dark_route_retreat";
    return response;
}

handoff_memory writeback_handoff_memory( const handoff_memory &incoming,
        const live_response &response )
{
    handoff_memory out = incoming;
    out.reason = response.reason;

    // A response is a plan/handoff, not proof that attack resolution ran.
    // The monster-local attack seam owns this counter, so walking or a blocked
    // route cannot consume the burst through writeback.
    out.strike_budget_spent = incoming.strike_budget_spent;
    out.cooldown_minutes = std::max( incoming.cooldown_minutes, response.writeback_cooldown_minutes );
    out.threat_memory = std::max( incoming.threat_memory, response.writeback_threat_memory );
    out.stalk_distance_omt = std::max( incoming.stalk_distance_omt,
                                       response.overmap_stalk_distance_omt );

    if( response.writeback_intent != handoff_intent::none ) {
        out.intent = response.writeback_intent;
    } else {
        switch( response.next ) {
            case decision::strike:
                out.intent = handoff_intent::committed_ambush;
                break;
            case decision::shadow:
                out.intent = handoff_intent::shadowing;
                break;
            case decision::withdraw:
                out.intent = handoff_intent::spent_disengage;
                out.cooldown_minutes = std::max( out.cooldown_minutes, 5 );
                break;
            case decision::cooling_off:
                out.intent = handoff_intent::spent_disengage;
                break;
            case decision::hold:
            case decision::interested:
            case decision::ignore:
                break;
        }
    }

    if( out.intent == handoff_intent::overmatched_stalk && out.stalk_distance_omt == 0 ) {
        out.stalk_distance_omt = cautious_stalk_distance_omt;
    }

    return out;
}

live_response evaluate_live_response( const live_context &ctx )
{
    live_response response;
    response.writeback_intent = ctx.overmap_intent;
    response.overmap_stalk_distance_omt = ctx.overmap_stalk_distance_omt;
    response.writeback_cooldown_minutes = ctx.overmap_cooldown_minutes;
    response.writeback_threat_memory = ctx.overmap_threat_memory;

    opportunity_context opportunity_ctx;
    opportunity_ctx.player_bleeding = ctx.player_bleeding;
    opportunity_ctx.player_hurt = ctx.player_hurt;
    opportunity_ctx.player_low_stamina = ctx.player_low_stamina;
    opportunity_ctx.player_distracted = ctx.player_distracted;
    opportunity_ctx.player_noisy = ctx.player_noisy;
    opportunity_ctx.zombie_pressure = ctx.zombie_pressure;
    opportunity_ctx.allied_support_nearby = ctx.allied_support_nearby;
    opportunity_ctx.near_cover_or_clutter = ctx.near_cover_or_clutter;
    opportunity_ctx.bright_exposure = ctx.stalker_in_bright_exposure || ctx.target_in_bright_exposure;
    opportunity_ctx.player_focused = ctx.target_has_focus;
    opportunity_ctx.stalker_hurt = ctx.stalker_hurt;
    opportunity_ctx.counterpressure_recent = ctx.counterpressure_recent;
    opportunity_ctx.distance_to_target = ctx.distance_to_target;
    opportunity_ctx.burst_strikes = ctx.burst_strikes;
    opportunity_report opportunity = evaluate_opportunity( opportunity_ctx );
    response.burst_limit = opportunity.burst_limit;
    response.retreat_distance = opportunity.retreat_distance;

    const bool handoff_has_target_footing = ctx.has_overmap_interest_footing &&
                                            ctx.overmap_intent != handoff_intent::none;
    if( !ctx.has_believable_local_evidence && !handoff_has_target_footing ) {
        response.next = decision::ignore;
        response.reason = "live_no_believable_evidence";
        return response;
    }

    const threat_report threat = evaluate_threat( threat_context{
        ctx.target_in_bright_exposure || ctx.stalker_in_bright_exposure, ctx.target_has_focus,
        ctx.distance_to_target <= 18 || ctx.has_overmap_interest_footing, ctx.allied_support_nearby,
        ctx.zombie_pressure, ctx.burst_strikes } );
    if( threat.overmatched ) {
        response.next = threat.next;
        response.writeback_intent = threat.intent;
        response.retreat_distance = std::max( response.retreat_distance, 14 );
        response.overmap_stalk_distance_omt = threat.stalk_distance_omt;
        response.writeback_threat_memory = threat.threat_memory;
        response.persistent_state_required = true;
        response.reason = "live_" + threat.reason;
        return response;
    }

    interest_context interest_ctx;
    interest_ctx.recent_human_evidence = true;
    interest_ctx.evidence_age_minutes = ctx.evidence_age_minutes;
    interest_ctx.exposed_night_light = ctx.target_in_bright_exposure;
    interest_ctx.forest_or_building_edge = ctx.cover_route_available;
    interest_ctx.town_or_road_edge = ctx.edge_route_available;
    interest_ctx.zombie_pressure = ctx.zombie_pressure;

    latch_state live_latch;
    live_latch.cooldown_minutes = std::max( ctx.on_cooldown ? 1 : 0,
                                            ctx.overmap_cooldown_minutes );
    live_latch.active = handoff_has_target_footing;
    live_latch.confidence = handoff_has_target_footing ? 3 : 0;
    live_latch.leash_tiles_remaining = handoff_has_target_footing ? default_latch_leash_tiles : 0;
    // Bright reciprocal sight normally breaks the ambush latch.  It must not
    // discard a witnessed, same-target crowd opening before the pressure
    // approach/strike rules can consume it.
    const bool exposed_or_focused_without_pressure = ctx.stalker_in_bright_exposure &&
            ctx.target_has_focus && ctx.zombie_pressure < 4;
    latch_update latch = advance_latch( latch_context{ live_latch, evaluate_interest( interest_ctx ), 0,
                                        ctx.distance_to_target, exposed_or_focused_without_pressure } );

    if( !latch.state.active ) {
        response.next = latch.next;
        response.reason = "live_latch_" + latch.reason;
        return response;
    }

    approach_context approach_ctx;
    approach_ctx.latch = latch.state;
    approach_ctx.cover_route_available = ctx.cover_route_available;
    approach_ctx.edge_route_available = ctx.edge_route_available;
    approach_ctx.direct_open_route_available = ctx.direct_open_route_available;
    approach_ctx.bright_exposure = ctx.stalker_in_bright_exposure;
    approach_ctx.forced_no_cover = ctx.forced_no_cover;
    const approach_report approach = choose_approach( approach_ctx );

    opportunity_ctx.latch = latch.state;
    opportunity = evaluate_opportunity( opportunity_ctx );
    response.burst_limit = opportunity.burst_limit;
    response.retreat_distance = opportunity.retreat_distance;

    const confidence_report confidence = evaluate_confidence( confidence_context{
        ctx.has_believable_local_evidence, ctx.has_overmap_interest_footing, ctx.zombie_pressure,
        ctx.quiet_side_cutoff_available, ctx.target_in_bright_exposure, ctx.stalker_in_bright_exposure,
        ctx.target_has_focus, ctx.open_exposure, ctx.stalker_hurt } );

    response.route = approach.route;
    response.opportunity = opportunity.opportunity;
    response.confidence = confidence.total;

    const live_response anti_gnome = resolve_anti_gnome( anti_gnome_context{
        ctx.night_outside_reachable_target, ctx.has_believable_local_evidence || handoff_has_target_footing,
        ctx.cover_route_available || ctx.edge_route_available || ctx.quiet_side_cutoff_available,
        threat.overmatched, ctx.distance_to_target, ctx.bad_position_loiter_turns } );
    if( anti_gnome.anti_gnome_triggered ) {
        response.next = anti_gnome.next;
        response.route = anti_gnome.route;
        response.writeback_intent = anti_gnome.writeback_intent;
        response.writeback_cooldown_minutes = std::max( response.writeback_cooldown_minutes,
                                              anti_gnome.writeback_cooldown_minutes );
        response.persistent_state_required = true;
        response.anti_gnome_triggered = true;
        response.reason = "live_" + anti_gnome.reason;
        return response;
    }

    if( opportunity.next == decision::withdraw || opportunity.next == decision::cooling_off ) {
        response.next = opportunity.next;
        response.writeback_intent = opportunity.next == decision::withdraw ?
                                    handoff_intent::spent_disengage :
                                    handoff_intent::none;
        response.writeback_cooldown_minutes = opportunity.next == decision::withdraw ? 5 :
                                              response.writeback_cooldown_minutes;
        if( opportunity.next == decision::withdraw ) {
            response.overmap_stalk_distance_omt = std::max( response.overmap_stalk_distance_omt,
                                                  cautious_stalk_distance_omt );
        }
        response.persistent_state_required = true;
        response.reason = "live_" + opportunity.reason;
        return response;
    }

    // Once same-target pressure is strong enough, daylight changes the route
    // cost but must not cancel the approach. Keep the commitment alive until
    // contact range, where evaluate_opportunity can authorize the strike.
    if( ctx.zombie_pressure >= 4 && ctx.distance_to_target > 3 &&
        !ctx.stalker_hurt && ctx.allied_support_nearby < 3 ) {
        response.next = decision::shadow;
        response.route = ctx.cover_route_available ? approach_class::cover_shadow :
                         approach_class::direct_forced;
        response.writeback_intent = handoff_intent::committed_ambush;
        response.persistent_state_required = true;
        response.reason = "live_heavy_zombie_pressure_approach";
        return response;
    }

    const bool vulnerable_player = ctx.player_bleeding || ctx.player_hurt || ctx.player_low_stamina ||
                                   ctx.player_distracted || ctx.player_noisy;
    if( !vulnerable_player && ctx.zombie_pressure >= 2 && ctx.distance_to_target <= 3 &&
        !( ctx.target_in_bright_exposure || ctx.stalker_in_bright_exposure || ctx.target_has_focus ) &&
        ( ctx.near_cover_or_clutter || ctx.cover_route_available || ctx.quiet_side_cutoff_available ) ) {
        response.next = decision::strike;
        response.writeback_intent = handoff_intent::committed_ambush;
        response.persistent_state_required = true;
        response.reason = "live_zombie_distraction_dark_square_strike";
        return response;
    }

    if( !vulnerable_player && ctx.zombie_pressure >= 2 && opportunity.next != decision::strike &&
        !( ctx.target_in_bright_exposure || ctx.stalker_in_bright_exposure || ctx.target_has_focus ) &&
        ( ctx.cover_route_available || ctx.quiet_side_cutoff_available ) ) {
        response.next = decision::shadow;
        response.route = approach_class::cover_shadow;
        response.writeback_intent = handoff_intent::opportunity_probe;
        response.persistent_state_required = true;
        response.reason = "live_zombie_distraction_dark_square_probe";
        return response;
    }

    if( ctx.target_has_focus && !vulnerable_player && ctx.zombie_pressure == 0 ) {
        if( approach.next == decision::shadow && approach.route != approach_class::direct_forced ) {
            response.next = decision::shadow;
            response.route = approach.route;
            response.writeback_intent = handoff_intent::shadowing;
            response.overmap_stalk_distance_omt = std::max( response.overmap_stalk_distance_omt,
                                                  cautious_stalk_distance_omt );
            response.persistent_state_required = true;
            response.reason = "live_sighted_stalker_breaks_los_shadow";
            return response;
        }
        response.next = decision::withdraw;
        response.writeback_intent = handoff_intent::spent_disengage;
        response.writeback_cooldown_minutes = std::max( response.writeback_cooldown_minutes, 5 );
        response.overmap_stalk_distance_omt = std::max( response.overmap_stalk_distance_omt,
                                              cautious_stalk_distance_omt );
        response.persistent_state_required = true;
        response.reason = "live_sighted_stalker_no_cover_withdraw";
        return response;
    }

    if( opportunity.next == decision::strike ) {
        if( ctx.distance_to_target > 3 && approach.next == decision::shadow ) {
            response.next = decision::shadow;
            response.reason = "live_shadowing_before_strike_window";
            return response;
        }
        response.next = decision::strike;
        response.writeback_intent = handoff_intent::committed_ambush;
        response.persistent_state_required = true;
        response.reason = "live_" + opportunity.reason;
        return response;
    }

    if( approach.next == decision::hold ) {
        response.next = decision::hold;
        response.reason = "live_" + approach.reason;
        return response;
    }

    if( ctx.distance_to_target <= 2 && opportunity.next == decision::hold ) {
        if( !ctx.target_in_bright_exposure && !ctx.stalker_in_bright_exposure &&
            ( ctx.direct_open_route_available || ctx.edge_route_available || ctx.cover_route_available ) &&
            ctx.allied_support_nearby < 2 ) {
            response.next = decision::shadow;
            response.reason = "live_dark_reachable_opportunity_probe";
            return response;
        }
        response.next = decision::hold;
        response.reason = "live_close_alert_target_hold";
        return response;
    }

    response.next = approach.next == decision::shadow ? decision::shadow : opportunity.next;
    response.writeback_intent = response.next == decision::shadow ? handoff_intent::shadowing :
                                response.writeback_intent;
    response.persistent_state_required = ctx.has_overmap_interest_footing;
    response.reason = response.next == decision::shadow ? "live_shadowing_believable_evidence" :
                      "live_" + opportunity.reason;
    return response;
}

} // namespace writhing_stalker
