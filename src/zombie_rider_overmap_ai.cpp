#include "zombie_rider_overmap_ai.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <tuple>

#include "calendar.h"
#include "debug.h"
#include "json.h"
#include "math_parser_diag_value.h"
#include "monster.h"
#include "predator_state.h"

namespace zombie_rider_overmap_ai
{

namespace
{

// Keep equal-turn evidence deterministic.  A new observation never gets a
// synthetic timestamp/TTL here: this is solely a tie-break between facts that
// were already observed at the same turn.
bool observation_is_stronger( const rider_band_observation &candidate,
                              const rider_band_observation &current )
{
    if( candidate.observed_turn != current.observed_turn ) {
        return candidate.observed_turn > current.observed_turn;
    }
    const bool candidate_identified = candidate.target_identity != 0;
    const bool current_identified = current.target_identity != 0;
    if( candidate_identified != current_identified ) {
        return candidate_identified;
    }
    const bool candidate_direct = candidate.provenance == "direct_rider_sight";
    const bool current_direct = current.provenance == "direct_rider_sight";
    if( candidate_direct != current_direct ) {
        return candidate_direct;
    }
    if( candidate.uncertainty != current.uncertainty ) {
        return candidate.uncertainty < current.uncertainty;
    }
    return std::make_tuple( candidate.source_actor_id, candidate.provenance,
                            candidate.target_identity, candidate.position.x(), candidate.position.y(), candidate.position.z(),
                            candidate.expires_at_turn ) <
           std::make_tuple( current.source_actor_id, current.provenance,
                            current.target_identity, current.position.x(), current.position.y(), current.position.z(),
                            current.expires_at_turn );
}

} // namespace

std::string rider_band_registry::root_for( const std::string &actor_id ) const
{
    const auto found = members.find( actor_id );
    if( found == members.end() || !found->second.alive ) {
        return std::string();
    }
    std::string root = actor_id;
    std::set<std::string> visited;
    while( true ) {
        const auto current = members.find( root );
        if( current == members.end() || !visited.insert( root ).second ) {
            return std::string();
        }
        // A canonical root is an identity/alias, not a living member lease.
        // Its death must not orphan living descendants or make their saved
        // cache point at a different band.  Only the queried actor's liveness
        // decides whether it is currently a member.
        if( current->second.parent == root ) {
            return root;
        }
        root = current->second.parent;
    }
}

std::vector<std::string> rider_band_registry::malformed_references() const
{
    std::vector<std::string> issues;
    for( const auto &entry : members ) {
        const std::string &actor_id = entry.first;
        std::string current = actor_id;
        std::set<std::string> visited;
        while( true ) {
            const auto found = members.find( current );
            if( found == members.end() ) {
                issues.push_back( actor_id + ":missing_parent:" + current );
                break;
            }
            if( found->second.parent == current ) {
                break;
            }
            if( !visited.insert( current ).second ) {
                issues.push_back( actor_id + ":parent_cycle:" + current );
                break;
            }
            current = found->second.parent;
        }
    }
    return issues;
}

std::string rider_band_registry::encounter_key( const std::string &first, const std::string &second ) const
{
    return first < second ? first + "\n" + second : second + "\n" + first;
}

void rider_band_registry::ensure_member( const std::string &actor_id )
{
    if( !actor_id.empty() && members.find( actor_id ) == members.end() ) {
        members.emplace( actor_id, member { actor_id } );
        revisions.emplace( actor_id, 1 );
    }
}

void rider_band_registry::bump_revision( const std::string &root )
{
    if( !root.empty() ) {
        ++revisions[root];
    }
}

bool rider_band_registry::register_encounter( const rider_band_encounter &encounter )
{
    if( encounter.first_actor_id.empty() || encounter.second_actor_id.empty() ||
        encounter.first_actor_id == encounter.second_actor_id || encounter.observed_turn < 0 ||
        !encounter.reciprocal_perception || !encounter.reachable_route ) {
        return false;
    }
    const std::string key = encounter_key( encounter.first_actor_id, encounter.second_actor_id );
    const auto previous = encounter_turns.find( key );
    if( previous != encounter_turns.end() && encounter.observed_turn <= previous->second ) {
        return false;
    }
    ensure_member( encounter.first_actor_id );
    ensure_member( encounter.second_actor_id );
    if( !members[encounter.first_actor_id].alive || !members[encounter.second_actor_id].alive ) {
        return false;
    }
    encounter_turns[key] = encounter.observed_turn;
    const std::string first_root = root_for( encounter.first_actor_id );
    const std::string second_root = root_for( encounter.second_actor_id );
    if( first_root.empty() || second_root.empty() || first_root == second_root ) {
        return true;
    }
    const std::string root = std::min( first_root, second_root );
    const std::string absorbed = std::max( first_root, second_root );
    members[absorbed].parent = root;
    // A merge changes the canonical band's membership exactly once.  The
    // absorbed root remains an alias so old loaded caches can be reconciled.
    const std::uint64_t inherited = std::max( revisions[root], revisions[absorbed] );
    revisions[root] = inherited + 1;
    return true;
}

bool rider_band_registry::record_casualty( const std::string &actor_id, const int turn )
{
    const auto found = members.find( actor_id );
    if( found == members.end() || !found->second.alive ) {
        return false;
    }
    const std::string root = root_for( actor_id );
    found->second.alive = false;
    found->second.casualty_turn = turn;
    observations.erase( actor_id );
    bump_revision( root );
    return true;
}

bool rider_band_registry::share_observation( const std::string &from_actor_id,
        const std::string &to_actor_id, const int turn )
{
    // A band alone is not a radio: sharing is allowed only in the encounter
    // turn that established reciprocal contact, and never extends the source TTL.
    const auto contact = encounter_turns.find( encounter_key( from_actor_id, to_actor_id ) );
    const auto source = observations.find( from_actor_id );
    if( contact == encounter_turns.end() || contact->second != turn || source == observations.end() ||
        !source->second.valid_at( turn ) || !in_same_band( from_actor_id, to_actor_id ) ) {
        return false;
    }
    rider_band_observation relayed = source->second;
    relayed.provenance = "relayed_direct_rider_sight";
    const auto destination = observations.find( to_actor_id );
    if( destination != observations.end() && !observation_is_stronger( relayed, destination->second ) ) {
        return false;
    }
    observations[to_actor_id] = relayed;
    return true;
}

void rider_band_registry::observe( const std::string &actor_id,
                                   const rider_band_observation &observation )
{
    if( actor_id.empty() || !observation.valid_at( observation.observed_turn ) ) {
        return;
    }
    const auto old = observations.find( actor_id );
    if( old == observations.end() || observation_is_stronger( observation, old->second ) ) {
        observations[actor_id] = observation;
    }
}

std::optional<rider_band_observation> rider_band_registry::observation_for( const std::string &actor_id,
        const int turn ) const
{
    const auto found = observations.find( actor_id );
    return found != observations.end() && found->second.valid_at( turn ) ?
           std::optional<rider_band_observation>( found->second ) : std::nullopt;
}

std::string rider_band_registry::band_for( const std::string &actor_id ) const
{
    return root_for( actor_id );
}

bool rider_band_registry::in_same_band( const std::string &first_actor_id,
        const std::string &second_actor_id ) const
{
    const std::string first = root_for( first_actor_id );
    return !first.empty() && first == root_for( second_actor_id );
}

int rider_band_registry::living_members( const std::string &actor_id ) const
{
    const std::string root = root_for( actor_id );
    if( root.empty() ) {
        return 0;
    }
    return std::count_if( members.begin(), members.end(), [&root, this]( const auto & entry ) {
        return entry.second.alive && root_for( entry.first ) == root;
    } );
}

std::uint64_t rider_band_registry::revision_for( const std::string &actor_id ) const
{
    const auto found = revisions.find( root_for( actor_id ) );
    return found == revisions.end() ? 0 : found->second;
}

void rider_band_registry::reconcile_cache( const std::string &actor_id,
        predator_lifecycle_state &state ) const
{
    const std::string root = root_for( actor_id );
    state.band_reference = root;
    state.band_revision_cache = revision_for( actor_id );
}

std::string rider_band_registry::diagnostic_json() const
{
    std::ostringstream result;
    JsonOut json( result );
    serialize( json );
    return result.str();
}

void rider_band_registry::clear()
{
    members.clear();
    revisions.clear();
    encounter_turns.clear();
    observations.clear();
}

void rider_band_registry::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "members" );
    json.start_array();
    for( const auto &entry : members ) {
        json.start_object();
        json.member( "actor_id", entry.first );
        json.member( "parent", entry.second.parent );
        json.member( "alive", entry.second.alive );
        json.member( "casualty_turn", entry.second.casualty_turn );
        json.end_object();
    }
    json.end_array();
    json.member( "revisions", revisions );
    json.member( "encounter_turns", encounter_turns );
    json.member( "observations" );
    json.start_array();
    for( const auto &entry : observations ) {
        json.start_object();
        json.member( "actor_id", entry.first );
        json.member( "target_identity", entry.second.target_identity );
        json.member( "position", entry.second.position );
        json.member( "observed_turn", entry.second.observed_turn );
        json.member( "expires_at_turn", entry.second.expires_at_turn );
        json.member( "source_actor_id", entry.second.source_actor_id );
        json.member( "provenance", entry.second.provenance );
        json.member( "uncertainty", entry.second.uncertainty );
        json.end_object();
    }
    json.end_array();
    // Derived only: preserve a deterministic diagnostic for corrupted save
    // references without repairing them by manufacturing members.
    json.member( "malformed_references", malformed_references() );
    json.end_object();
}

void rider_band_registry::deserialize( const JsonObject &json )
{
    clear();
    if( !json.has_array( "members" ) ) {
        return;
    }
    for( JsonObject entry : json.get_array( "members" ) ) {
        entry.allow_omitted_members();
        std::string actor_id;
        member saved;
        entry.read( "actor_id", actor_id );
        entry.read( "parent", saved.parent );
        entry.read( "alive", saved.alive );
        entry.read( "casualty_turn", saved.casualty_turn );
        if( !actor_id.empty() ) {
            saved.parent = saved.parent.empty() ? actor_id : saved.parent;
            members.emplace( actor_id, saved );
        }
    }
    json.read( "revisions", revisions );
    json.read( "encounter_turns", encounter_turns );
    // This is derived diagnostic output, never restored state.  Consume it
    // when loading our own saves so strict JSON visitation remains clean.
    if( json.has_array( "malformed_references" ) ) {
        json.get_array( "malformed_references" );
    }
    if( json.has_array( "observations" ) ) {
        for( JsonObject entry : json.get_array( "observations" ) ) {
            entry.allow_omitted_members();
            std::string actor_id;
            rider_band_observation observation;
            entry.read( "actor_id", actor_id );
            entry.read( "target_identity", observation.target_identity );
            entry.read( "position", observation.position );
            entry.read( "observed_turn", observation.observed_turn );
        entry.read( "expires_at_turn", observation.expires_at_turn );
        entry.read( "source_actor_id", observation.source_actor_id );
        entry.read( "provenance", observation.provenance );
            entry.read( "uncertainty", observation.uncertainty );
            observe( actor_id, observation );
        }
    }
}

void rider_pursuit_state::advance_to( const int now_turn )
{
    if( has_last_observed_position && last_observed_turn >= 0 &&
        now_turn >= last_observed_turn && now_turn - last_observed_turn > 200 ) {
        phase = pursuit_phase::idle;
        target_identity = 0;
        has_last_observed_position = false;
        has_movement_waypoint = false;
        last_observed_turn = -1;
        evidence_source_actor_id.clear();
        evidence_provenance.clear();
        search_until_turn = -1;
        return;
    }
    if( phase == pursuit_phase::searching && search_until_turn >= 0 && now_turn >= search_until_turn ) {
        phase = pursuit_phase::idle;
        target_identity = 0;
        has_last_observed_position = false;
        has_movement_waypoint = false;
        last_observed_turn = -1;
        evidence_source_actor_id.clear();
        evidence_provenance.clear();
        search_until_turn = -1;
    }
}

bool refresh_light_memory( rider_light_memory &memory, const rider_light_interest &interest,
                           const std::string &sample_id, const int observed_at_turn )
{
    if( sample_id.empty() || observed_at_turn < 0 || memory.sample_id == sample_id ||
        observed_at_turn <= memory.observed_at_turn ) {
        return false;
    }
    if( !interest.should_investigate || interest.interest_score <= 0 ||
        interest.memory_turns <= 0 || interest.max_riders_drawn <= 0 ) {
        return false;
    }
    // A real later detection replaces this record's finite observation; it
    // does not inherit remaining TTL or strength from a previous source in
    // the same OMT.
    memory.interest_score = interest.interest_score;
    memory.turns_remaining = interest.memory_turns;
    memory.max_riders_drawn = std::min( max_riders_drawn_by_light, interest.max_riders_drawn );
    memory.decay_turn_remainder = 0;
    memory.reason = interest.reason;
    memory.sample_id = sample_id;
    memory.observed_at_turn = observed_at_turn;
    memory.expires_at_turn = observed_at_turn + memory.turns_remaining;
    return true;
}

bool rider_pursuit_state::evidence_valid( const int now_turn ) const
{
    return has_last_observed_position && target_identity != 0 && last_observed_turn >= 0 &&
           now_turn >= last_observed_turn && now_turn - last_observed_turn <= 200;
}

void rider_pursuit_state::observe( const std::uint64_t identity,
                                   const tripoint_abs_ms &position, const int now_turn )
{
    // Planning refreshes the same prey observation every turn.  Do not erase
    // a readiness token earned by the immediately preceding physical closing
    // step; only a newly acquired identity invalidates that token.
    const bool identity_changed = target_identity != identity;
    if( debug_log_enabled( D_INFO, D_GAME ) ) {
        DebugLog( D_INFO, D_GAME ) << "zombie_rider impact_observe turn=" << now_turn
                                   << " old_target=" << target_identity << " new_target=" << identity
                                   << " ready_before=" << ( impact_ready ? "yes" : "no" )
                                   << " identity_changed=" << ( identity_changed ? "yes" : "no" ) << '\n';
    }
    if( identity_changed ) {
        impact_ready = false;
        impact_ready_turn = -1;
    }
    phase = pursuit_phase::pursuing;
    target_identity = identity;
    last_observed_position = position;
    has_last_observed_position = true;
    last_observed_turn = now_turn;
    movement_waypoint = position;
    has_movement_waypoint = true;
    search_until_turn = -1;
}

void rider_pursuit_state::relay( const rider_band_observation &observation )
{
    if( !observation.valid_at( observation.observed_turn ) ||
        observation.source_actor_id.empty() ) {
        return;
    }
    // Relaying does not create a new sighting: retain the source's timestamp
    // and finite expiry while routing only toward that observed area.
    phase = pursuit_phase::pursuing;
    target_identity = observation.target_identity;
    last_observed_position = observation.position;
    has_last_observed_position = true;
    last_observed_turn = observation.observed_turn;
    movement_waypoint = observation.position;
    has_movement_waypoint = true;
    search_until_turn = -1;
    evidence_source_actor_id = observation.source_actor_id;
    evidence_provenance = observation.provenance;
}

void rider_pursuit_state::begin_search( const int now_turn )
{
    phase = pursuit_phase::searching;
    search_until_turn = now_turn + 20;
    has_movement_waypoint = false;
}

void rider_pursuit_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "phase", static_cast<int>( phase ) );
    json.member( "target_identity", target_identity );
    json.member( "has_last_observed_position", has_last_observed_position );
    json.member( "last_observed_position", last_observed_position );
    json.member( "last_observed_turn", last_observed_turn );
    json.member( "evidence_source_actor_id", evidence_source_actor_id );
    json.member( "evidence_provenance", evidence_provenance );
    json.member( "has_movement_waypoint", has_movement_waypoint );
    json.member( "movement_waypoint", movement_waypoint );
    json.member( "search_until_turn", search_until_turn );
    json.member( "impact_ready", impact_ready );
    json.member( "impact_ready_turn", impact_ready_turn );
    json.member( "impact_recovery_until_turn", impact_recovery_until_turn );
    json.end_object();
}

void rider_pursuit_state::deserialize( const JsonObject &json )
{
    json.allow_omitted_members();
    const int raw_phase = json.get_int( "phase", static_cast<int>( pursuit_phase::idle ) );
    phase = raw_phase >= static_cast<int>( pursuit_phase::idle ) &&
            raw_phase <= static_cast<int>( pursuit_phase::searching ) ?
            static_cast<pursuit_phase>( raw_phase ) : pursuit_phase::idle;
    json.read( "target_identity", target_identity );
    json.read( "has_last_observed_position", has_last_observed_position );
    json.read( "last_observed_position", last_observed_position );
    json.read( "last_observed_turn", last_observed_turn );
    json.read( "evidence_source_actor_id", evidence_source_actor_id );
    json.read( "evidence_provenance", evidence_provenance );
    json.read( "has_movement_waypoint", has_movement_waypoint );
    json.read( "movement_waypoint", movement_waypoint );
    json.read( "search_until_turn", search_until_turn );
    json.read( "impact_ready", impact_ready );
    json.read( "impact_ready_turn", impact_ready_turn );
    json.read( "impact_recovery_until_turn", impact_recovery_until_turn );
}

int mature_world_gate_days()
{
    return to_days<int>( mature_world_gate_seasons * calendar::season_length() );
}

namespace
{
const std::string camp_posture_key( "caol_zombie_rider_camp_posture" );
const std::string camp_source_key( "caol_zombie_rider_camp_source" );
const std::string camp_intent_until_key( "caol_zombie_rider_camp_intent_until" );
const std::string camp_slot_key( "caol_zombie_rider_camp_slot" );

int current_turn_number()
{
    return to_turns<int>( calendar::turn - calendar::turn_zero );
}

rider_camp_pressure_posture posture_from_string( const std::string &posture )
{
    if( posture == "investigate" ) {
        return rider_camp_pressure_posture::investigate;
    }
    if( posture == "circle_harass" ) {
        return rider_camp_pressure_posture::circle_harass;
    }
    if( posture == "direct_attack" ) {
        return rider_camp_pressure_posture::direct_attack;
    }
    if( posture == "withdraw" ) {
        return rider_camp_pressure_posture::withdraw;
    }
    return rider_camp_pressure_posture::none;
}

void clear_memory( rider_light_memory &memory, const std::string &reason )
{
    memory.interest_score = 0;
    memory.turns_remaining = 0;
    memory.max_riders_drawn = 0;
    memory.decay_turn_remainder = 0;
    memory.reason = reason;
}

int omt_distance( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs )
{
    return std::max( std::abs( lhs.x() - rhs.x() ), std::abs( lhs.y() - rhs.y() ) );
}
} // namespace

rider_light_interest evaluate_light_attraction(
    const bandit_mark_generation::light_projection &projection,
    int world_age_days,
    int eligible_riders_nearby )
{
    rider_light_interest interest;
    interest.notes.push_back( projection.review_summary );

    if( world_age_days < mature_world_gate_days() ) {
        interest.reason = "early_world_gate";
        interest.notes.push_back( "zombie rider light interest suppressed before mature-world gate" );
        return interest;
    }

    if( eligible_riders_nearby <= 0 ) {
        interest.reason = "no_riders_available";
        interest.notes.push_back( "no eligible late-game riders are available to investigate" );
        return interest;
    }

    if( !projection.viable ) {
        interest.reason = "no_viable_light_signal";
        return interest;
    }

    const int horde_signal_power = bandit_mark_generation::horde_signal_power_from_light_projection(
                                       projection );
    if( horde_signal_power <= 0 ) {
        interest.reason = "below_rider_light_threshold";
        interest.notes.push_back(
            "light is visible enough for local review but not exposed/large enough to call endpoint cavalry" );
        return interest;
    }

    interest.should_investigate = true;
    interest.reason = projection.concealment.elevated_exposure_extended ?
                      "elevated_bright_light" : "exposed_bright_light";
    interest.interest_score = std::clamp( horde_signal_power / 10 + projection.visibility_score / 4, 1, 6 );
    interest.memory_turns = std::clamp( 60 + horde_signal_power * 4, 90, 300 );
    const int desired_riders = interest.interest_score >= 5 ? max_riders_drawn_by_light : 1;
    interest.max_riders_drawn = std::min( eligible_riders_nearby, desired_riders );
    interest.notes.push_back( "rider light interest: horde_signal_power=" +
                              std::to_string( horde_signal_power ) +
                              ", score=" + std::to_string( interest.interest_score ) +
                              ", memory_turns=" + std::to_string( interest.memory_turns ) +
                              ", max_riders_drawn=" + std::to_string( interest.max_riders_drawn ) );
    interest.notes.push_back(
        "bounded investigation only: light creates temporary pressure, not permanent camp doom" );
    return interest;
}

void refresh_light_memory( rider_light_memory &memory, const rider_light_interest &interest )
{
    if( !interest.should_investigate || interest.interest_score <= 0 || interest.memory_turns <= 0 ||
        interest.max_riders_drawn <= 0 ) {
        return;
    }

    memory.interest_score = std::max( memory.interest_score, interest.interest_score );
    memory.turns_remaining = std::max( memory.turns_remaining, interest.memory_turns );
    memory.max_riders_drawn = std::min( max_riders_drawn_by_light,
                                        std::max( memory.max_riders_drawn, interest.max_riders_drawn ) );
    memory.decay_turn_remainder = 0;
    memory.reason = interest.reason;
}

void advance_light_memory( rider_light_memory &memory, int elapsed_turns )
{
    if( elapsed_turns <= 0 || !memory.active() ) {
        return;
    }

    if( elapsed_turns >= memory.turns_remaining ) {
        clear_memory( memory, "decayed_after_light_off" );
        return;
    }

    memory.turns_remaining -= elapsed_turns;
    const int decay_turns = memory.decay_turn_remainder + elapsed_turns;
    const int decay_steps = decay_turns / rider_light_memory_decay_interval_turns;
    memory.decay_turn_remainder = decay_turns % rider_light_memory_decay_interval_turns;
    if( decay_steps > 0 ) {
        memory.interest_score = std::max( 0, memory.interest_score - decay_steps );
    }
    if( memory.interest_score == 0 ) {
        clear_memory( memory, "decayed_after_light_off" );
    }
}

rider_convergence_result evaluate_rider_convergence(
    const rider_light_memory &memory,
    const tripoint_abs_omt &light_omt,
    const std::vector<rider_overmap_agent> &riders )
{
    rider_convergence_result result;
    result.cap = std::min( memory.max_riders_drawn, max_riders_drawn_by_light );

    if( !memory.active() ) {
        result.reason = "light_memory_inactive";
        result.notes.push_back( "no rider convergence: light interest is inactive or decayed" );
        return result;
    }

    if( result.cap <= 0 ) {
        result.reason = "zero_rider_cap";
        result.notes.push_back( "no rider convergence: active light memory has no draw budget" );
        return result;
    }

    std::vector<rider_overmap_agent> candidates;
    for( const rider_overmap_agent &rider : riders ) {
        if( !rider.available || rider.already_in_band || rider.cooldown_turns > 0 ) {
            continue;
        }
        if( rider.pos.z() != light_omt.z() ) {
            continue;
        }
        if( omt_distance( rider.pos, light_omt ) > rider_convergence_response_radius_omt ) {
            continue;
        }
        candidates.push_back( rider );
    }

    if( candidates.empty() ) {
        result.reason = "no_eligible_riders_in_response_radius";
        result.notes.push_back( "no rider convergence: no loose mature riders can answer the light" );
        return result;
    }

    const auto by_light_distance_then_id = [&light_omt]( const rider_overmap_agent & lhs,
    const rider_overmap_agent & rhs ) {
        const int lhs_distance = omt_distance( lhs.pos, light_omt );
        const int rhs_distance = omt_distance( rhs.pos, light_omt );
        if( lhs_distance != rhs_distance ) {
            return lhs_distance < rhs_distance;
        }
        return lhs.rider_id < rhs.rider_id;
    };
    std::sort( candidates.begin(), candidates.end(), by_light_distance_then_id );

    const int selected = std::min<int>( result.cap, candidates.size() );
    for( int index = 0; index < selected; ++index ) {
        result.rider_ids.push_back( candidates[index].rider_id );
    }
    result.selected_riders = selected;
    result.should_converge = selected > 0;
    // Convergence is dispatch only.  It deliberately does not infer durable
    // social state from a shared destination or a shared light sample.
    result.band_formed = false;
    result.band_size = 0;
    result.posture = "lone_rider_harass";
    result.reason = "rider_converges_to_light_interest";
    result.notes.push_back( "selected_riders=" + std::to_string( selected ) +
                            ", cap=" + std::to_string( result.cap ) +
                            ", available_candidates=" + std::to_string( candidates.size() ) );
    result.notes.push_back(
        "convergence uses temporary light memory and the rider draw cap; no permanent horde magic" );
    return result;
}

void reserve_rider_convergence( std::vector<rider_overmap_agent> &riders,
                                const rider_convergence_result &convergence )
{
    for( rider_overmap_agent &rider : riders ) {
        if( std::find( convergence.rider_ids.begin(), convergence.rider_ids.end(), rider.rider_id ) !=
            convergence.rider_ids.end() ) {
            // This is a transient per-dispatch reservation; persistent
            // membership is owned solely by rider_band_registry.
            rider.available = false;
        }
    }
}

rider_camp_pressure_result choose_camp_pressure_posture(
    const rider_camp_pressure_input &input )
{
    rider_camp_pressure_result result;

    if( input.rider_wounded ) {
        result.posture = rider_camp_pressure_posture::withdraw;
        result.reason = "wounded_rider_disengages";
        result.notes.push_back( "wounded riders preserve local disengagement instead of final-shot spam" );
        return result;
    }

    if( !input.light_memory_active || input.rider_count <= 0 ) {
        result.posture = rider_camp_pressure_posture::none;
        result.reason = "no_active_light_pressure";
        return result;
    }

    if( input.breach_or_opening && input.rider_count >= std::max( 1, input.defender_strength ) ) {
        result.posture = rider_camp_pressure_posture::direct_attack;
        result.reason = "breach_or_opening_with_advantage";
        result.notes.push_back( "direct attack requires an opening or advantage, not blind wall charge" );
        return result;
    }

    if( input.band_formed || input.rider_count >= rider_band_minimum_size ) {
        result.posture = rider_camp_pressure_posture::circle_harass;
        result.reason = "band_without_breach_circles_and_harasses";
        result.notes.push_back(
            "rider band holds mounted pressure instead of suiciding into defended walls" );
        return result;
    }

    result.posture = rider_camp_pressure_posture::investigate;
    result.reason = "lone_rider_investigates_light";
    return result;
}

std::string to_string( rider_camp_pressure_posture posture )
{
    switch( posture ) {
        case rider_camp_pressure_posture::none:
            return "none";
        case rider_camp_pressure_posture::investigate:
            return "investigate";
        case rider_camp_pressure_posture::circle_harass:
            return "circle_harass";
        case rider_camp_pressure_posture::direct_attack:
            return "direct_attack";
        case rider_camp_pressure_posture::withdraw:
            return "withdraw";
    }
    return "none";
}

void clear_camp_pressure_intent( monster &rider )
{
    rider.remove_value( camp_posture_key );
    rider.remove_value( camp_source_key );
    rider.remove_value( camp_intent_until_key );
    rider.remove_value( camp_slot_key );
}

void set_camp_pressure_intent( monster &rider, rider_camp_pressure_posture posture,
                               const tripoint_abs_ms &source, int duration_turns,
                               int formation_slot )
{
    if( posture == rider_camp_pressure_posture::none || duration_turns <= 0 ) {
        clear_camp_pressure_intent( rider );
        return;
    }

    rider.set_value( camp_posture_key, to_string( posture ) );
    rider.set_value( camp_source_key, source );
    rider.set_value( camp_intent_until_key, current_turn_number() + std::max( 1, duration_turns ) );
    rider.set_value( camp_slot_key, std::max( 0, formation_slot ) );
}

std::optional<rider_camp_pressure_intent> get_camp_pressure_intent( monster &rider )
{
    const diag_value *posture_value = rider.maybe_get_value( camp_posture_key );
    const diag_value *source_value = rider.maybe_get_value( camp_source_key );
    const diag_value *until_value = rider.maybe_get_value( camp_intent_until_key );
    const diag_value *slot_value = rider.maybe_get_value( camp_slot_key );
    if( posture_value == nullptr || !posture_value->is_str() ||
        source_value == nullptr || !source_value->is_tripoint() ||
        until_value == nullptr || !until_value->is_dbl() ||
        slot_value == nullptr || !slot_value->is_dbl() ) {
        clear_camp_pressure_intent( rider );
        return std::nullopt;
    }

    rider_camp_pressure_intent intent;
    intent.posture = posture_from_string( posture_value->str() );
    intent.source = source_value->tripoint();
    intent.formation_slot = std::max( 0, static_cast<int>( slot_value->dbl() ) );
    intent.turns_remaining = static_cast<int>( until_value->dbl() ) - current_turn_number();
    if( intent.posture == rider_camp_pressure_posture::none || intent.turns_remaining <= 0 ) {
        clear_camp_pressure_intent( rider );
        return std::nullopt;
    }

    return intent;
}

} // namespace zombie_rider_overmap_ai
