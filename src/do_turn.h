#pragma once
#ifndef CATA_SRC_DO_TURN_H
#define CATA_SRC_DO_TURN_H

#include "coordinates.h"
#include "character_id.h"
#include "live_light.h"

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <array>
#include <ctime>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class avatar;
class Character;
class map;
class monster;
class npc;
class openclaw_harness_turn_trace;

// Publish only already-selected, pending diagnostic receipts at a safe
// lifecycle boundary. It is inert without an active monitored run.
void flush_native_slow_turn_receipts();

int signal_live_zombie_riders_from_light_observations_for_test(
    const std::vector<live_bandit_signal_observation> &signals );

// Existing bounded diagnostic row, shared only to measure actual native owners.
// WALL scopes/call counts are inert without an active run-bound diagnostic row.
// Scopes are synchronous children of that row; they must not outlive its turn.
enum class raid_perf_stage : std::size_t {
    operations, horde_movement, population, fields, items, map_cache,
    signal_total, signal_discovery, signal_hordes, creatures, overmap_npcs,
    pre_input_world, post_input_action,
    npc_site_bootstrap, npc_signal_copy, npc_channel_audit, human_activation,
    watch_preflight, watch_optical_read, watch_record_view,
    migo_service_local_navigation, migo_service_abstract_navigation,
    migo_local_plan, migo_local_move,
    migo_local_prepare_navigation, migo_abstract_prepare_navigation,
    physical_navigation_read, physical_navigation_view_build, physical_navigation_native_route,
    nav_view_lookup, nav_view_miss_load, nav_view_level_setup,
    migo_plan_native_targeting, migo_plan_species_hook, migo_plan_hostile_recognition,
    scout_actor_read, scout_view_build, scout_view_cache, scout_light_read,
    scout_light_dependencies, scout_light_native, scout_actor_predicate, scout_reader_retire,
    creature_monsters, creature_npc_preparation, creature_npc_actions,
    turn_setup, turn_activity_prep, turn_input_loop, turn_world_tail,
    tail_prepare, tail_world_work, tail_finish,
    signal_rider_maintenance, signal_stalkers, signal_riders,
    signal_cannibals, signal_migo,
    stalker_target_acquisition, stalker_target_recheck,
    stalker_invisible_avatar_read,
    camp_locker_downtime, camp_locker_collection, camp_locker_service,
    npc_find_item, npc_pick_up_item, count
};
enum class raid_perf_nav_event : std::size_t {
    resident_hit, resident_null, buffer_miss, archive_load_calls,
    archive_open_attempts, archive_open_failures, missing_archive, missing_quad,
    omitted_submap, loaded_submap, load_failures, plain_read_calls,
    level_fresh, level_reused, level_inside_lookup, count
};
// Branch counts only: no query identity/history, timing or cached refusal.
// Like the existing NAV loader counters, these belong to the current row.
enum class raid_perf_query_event : std::size_t {
    no_segment, segment_empty, segment_present, context_empty_reset, context_positive_reset,
    goal_changed, capability_or_level_changed, origin_changed, segment_exhausted, validation_failed, cost_changed,
    segment_reused, segment_stored, store_rejected, cross_z,
    deferred_probe, deferred_member_ineligible,
    walk, open_door, goal_reached, unknown_geometry, fire_refused, no_route, other_refusal, count
};
bool raid_perf_query_active();
void raid_perf_note_query( raid_perf_query_event event );

// Attribution context belongs only to the current synchronous diagnostic row.
// These reads/counters never load geometry or establish game knowledge.
bool raid_perf_nav_build_active();
bool raid_perf_nav_lookup_active();
void raid_perf_note_nav( raid_perf_nav_event event );
struct raid_perf_row;
class raid_perf_scope
{
    public:
        explicit raid_perf_scope( raid_perf_stage stage, bool enabled = true );
        ~raid_perf_scope();
        void restart( raid_perf_stage stage );
        raid_perf_scope( const raid_perf_scope & ) = delete;
        raid_perf_scope &operator=( const raid_perf_scope & ) = delete;

    private:
        void start( raid_perf_stage stage, bool enabled );
        void finish();
        raid_perf_row *row;
        raid_perf_stage stage;
        std::chrono::steady_clock::time_point started;
};

// Opt-in process-wide CPU and Darwin resource deltas for the synchronous
// Locker collection only. No sampling is performed outside a diagnostic row.
class camp_locker_resource_scope
{
    public:
        camp_locker_resource_scope();
        ~camp_locker_resource_scope();
        camp_locker_resource_scope( const camp_locker_resource_scope & ) = delete;
        camp_locker_resource_scope &operator=( const camp_locker_resource_scope & ) = delete;

    private:
        raid_perf_row *row = nullptr;
#if !defined(_WIN32)
        std::clock_t cpu_started = static_cast<std::clock_t>( -1 );
#endif
        std::chrono::steady_clock::time_point probe_started;
        std::chrono::steady_clock::time_point body_started;
#if defined(__APPLE__)
        std::array<std::uint64_t, 5> resource_started{};
        bool resource_valid = false;
#endif
};

// Exclude only a native input acquisition outside handle_action's existing
// input phase. Nested acquisitions leave their parent's phase untouched.
class openclaw_harness_native_input_scope
{
    public:
        openclaw_harness_native_input_scope();
        ~openclaw_harness_native_input_scope();
        openclaw_harness_native_input_scope( const openclaw_harness_native_input_scope & ) = delete;
        openclaw_harness_native_input_scope &operator=( const openclaw_harness_native_input_scope & ) =
            delete;

    private:
        openclaw_harness_turn_trace *trace_ = nullptr;
};

// Shared human camp/party clock boundary, also used before native on_load.
namespace bandit_live_world
{
struct activation_clock;
}
void refresh_live_bandit_activation();
bandit_live_world::activation_clock *live_bandit_body_activation( character_id id );
namespace raid_sweep
{
struct home_rally_member;
}
// A synchronous value view of one already evaluated owner. It performs no
// discovery, eligibility evaluation, movement or second pause debit.
class live_bandit_body_interval
{
    public:
        explicit live_bandit_body_interval( bandit_live_world::activation_clock *clock = nullptr );
        explicit live_bandit_body_interval( raid_sweep::home_rally_member *member,
                                           bool authenticated = true );
        explicit operator bool() const;
        bool active() const;
        std::int64_t active_delta_turns() const;
        std::int64_t suspended_body_turns( character_id id, int anchor ) const;
        void settle_body( character_id id, int anchor );
        // Actual loaded-owner release only, never an eligibility/read query.
        void finish_loaded_interval();
    private:
        bandit_live_world::activation_clock *clock_ = nullptr;
        raid_sweep::home_rally_member *member_ = nullptr;
        bool authenticated_ = true;
};
live_bandit_body_interval live_bandit_body_interval_for( const npc &body );

namespace bandit_live_world
{
struct world_state;
struct site_record;
struct active_outing_state;
struct structural_threat_observer_request;
struct structural_route_read;
struct structural_signal_read;
struct structural_outing_plan;
struct response_member_power_read;
struct canonical_hostile_operation_route;
struct shakedown_goods_pool;
struct shakedown_surface;
struct local_gate_input;
} // namespace bandit_live_world

namespace sounds
{
struct robbery_demand;
}
// Called only after the existing sound pipeline establishes actual audibility.
// It records a candidate in the current operation, never contact or a target.
void hear_bandit_shakedown_demand( const sounds::robbery_demand &demand, Character &hearer );

// Shared avatar activity step before input and after a native action.
void process_avatar_activity_turn( avatar &u );
void handle_key_blocking_activity();
// A successful unrelated native conversation cancels only a deferred Pay
// choice. The demand and its trade successor do not enter avatar::talk_to.
void cancel_bandit_shakedown_pending_pay_on_dialogue();
// Read-only current encounter facts; unavailable local actors/receivers are
// explicit. This does not establish contact, emit sound or select an AI target.
std::string bandit_shakedown_communication_diagnostic( const npc &actor );
// Clear the transient physical-light packet when a world/save becomes the
// active simulation.  Observations are never persisted in this cache.
void reset_live_light_sample_cache();
void run_live_light_delivery_for_test();
// Read-only counters for the existing run-bound channel audit. These describe
// logical output and writer calls, never physical disk I/O or game authority.
struct live_channel_output_statistics {
    std::size_t records = 0;
    std::size_t open_calls = 0;
    std::size_t flush_calls = 0;
    std::size_t logical_bytes = 0;
    bool write_failed = false;
};
live_channel_output_statistics live_channel_output_statistics_for_test();
int observe_live_hostile_signal_sources_for_test(
    const std::vector<live_bandit_signal_observation> &signals );
void maintain_live_bandit_structural_bounty_for_test(
    const std::vector<live_bandit_signal_observation> &signals );
// Pure ownership read for an actual completed abstract watch motor. The live
// assigned destination remains pending until its pair consumer commits.
bool live_bandit_scout_watch_order_pending( const npc &member );
// Shared declarations for existing production-backed test seams.
std::map<character_id, tripoint_abs_omt> live_bandit_elevated_recovery_orders_for_test();
std::map<character_id, std::pair<tripoint_abs_ms, tripoint_abs_ms>>
        live_bandit_elevated_recovery_boundary_for_test();
std::map<character_id, std::pair<tripoint_abs_ms, tripoint_abs_ms>>
        live_bandit_ingress_boundary_steps_for_test();
void live_bandit_elevated_recovery_npc_turn_for_test( bool overmap_step );
int record_live_bandit_stationary_watch_signals_for_test( bandit_live_world::world_state &state,
        const std::vector<live_bandit_signal_observation> &signals, bool cadence_due );
int advance_live_bandit_local_scout_assessments_for_test();
bool persist_live_bandit_local_projection_leases_for_test( const bandit_live_world::site_record
        &site );
bool persist_live_bandit_local_progress_for_test( const bandit_live_world::site_record &before,
        const bandit_live_world::site_record &after );
bool record_live_bandit_structural_member_returns_for_test();
bool complete_loaded_live_bandit_route_arrivals_for_test();
void prepare_live_bandit_abstract_scout_travel_for_test();
int observe_live_bandit_sounds_for_test();
void run_live_light_staffed_observer_for_test();
bool live_light_sample_is_current_for_test();
std::vector<live_light_delivery_stage> live_light_delivery_order_for_test();
std::vector<live_light_delivery_stage> live_light_delivery_trace_for_test();
bool process_live_bandit_aftermath_for_test();
std::vector<bandit_live_world::structural_signal_read> live_bandit_structural_signal_reads_for_test(
    const std::vector<live_bandit_signal_observation> &signals,
    const bandit_live_world::site_record &site,
    const bandit_live_world::active_outing_state &outing,
    const bandit_live_world::structural_threat_observer_request &request );
std::vector<bandit_live_world::structural_signal_read>
live_bandit_staffed_camp_signal_reads_for_test(
    const std::vector<live_bandit_signal_observation> &signals,
    const bandit_live_world::site_record &site,
    character_id observer_id );
bandit_live_world::camp_signal_observer_resolution resolve_live_bandit_staffed_observer_for_test(
    bandit_live_world::world_state &state, std::size_t site_index );
bandit_live_world::camp_signal_observation_result record_live_bandit_staffed_camp_signals_for_test(
    bandit_live_world::world_state &state,
    const std::vector<live_bandit_signal_observation> &signals );
bool live_bandit_overmap_los_from_for_test( const tripoint_abs_omt &origin,
        const tripoint_abs_omt &target, int sight_points );
bool materialize_live_bandit_structural_handoffs_for_test();
int materialize_live_bandit_response_members_for_test( const std::string &site_id );
std::vector<bandit_live_world::response_member_power_read>
live_bandit_response_member_power_reads_for_test( const bandit_live_world::site_record &site );
std::size_t maintain_live_bandit_local_pair_cohesion_for_test();
bool dematerialize_live_bandit_structural_handoffs_for_test();
bool dematerialize_live_bandit_structural_handoffs_with_quiesce_failure_for_test( int member_id );
void process_monsters_and_npcs_turn_for_test();
void process_overmap_npc_move_for_test();
bool reject_local_coarse_candidate_for_test_once();
bool advance_live_bandit_hostile_returns_for_test();
bool materialize_committed_bandit_shakedown_for_test( bandit_live_world::site_record &site );
bool complete_live_bandit_homeward_boundary_for_test();
void note_live_bandit_aftermath_for_test();
bool live_cannibal_raid_advance_site_search_for_test( bandit_live_world::site_record &site );
void note_live_bandit_local_turn_sight_avoid_for_test();
bool advance_live_bandit_hostile_approaches_for_test();
bool commit_bandit_shakedown_payment_for_test( bandit_live_world::site_record &site,
        const bandit_live_world::shakedown_surface &surface, int value );
bool bandit_shakedown_response_matches_for_test( const bandit_live_world::site_record &site,
        const std::string &activity_id, int generation, character_id receiver_id );
bool advance_bandit_hidden_shakedown_search_for_test( bandit_live_world::site_record &site );
void choose_bandit_shakedown_fight_for_test( bandit_live_world::site_record &site,
        const bandit_live_world::shakedown_surface &surface, const Character &receiver );
bool consume_explicit_bandit_shakedown_fight_for_test( bandit_live_world::site_record &site,
        const bandit_live_world::shakedown_surface &surface, const std::string &activity_id,
        int generation, character_id receiver_id );
bool establish_bandit_shakedown_communication_for_test( bandit_live_world::site_record &site );
bool handle_bandit_shakedown_contact_for_test( bandit_live_world::site_record &site );
std::optional<character_id> bandit_shakedown_speaker_id_for_test( const
        bandit_live_world::site_record &site );
bandit_live_world::shakedown_goods_pool bandit_encounter_goods_pool_for_test(
    const bandit_live_world::local_gate_input &input, Character &receiver );
std::optional<bandit_live_world::canonical_hostile_operation_route>
live_bandit_hostile_operation_route_read_for_test( const bandit_live_world::site_record &site );
void run_live_bandit_structural_route_analyzer_for_debug();
std::string live_bandit_local_reality_safety_record_for_test( const map &here,
        const avatar &observer, monster &critter );
bool write_harness_new_world_feasibility_artifact();
bandit_live_world::structural_route_read live_bandit_structural_route_read_for_test(
    const bandit_live_world::site_record &site,
    const bandit_live_world::structural_outing_plan &plan, int &watch_path_budget );
std::vector<bandit_live_world::structural_route_read>
live_bandit_structural_route_analyzer_reads_for_test(
    const bandit_live_world::site_record &site,
    const std::vector<bandit_live_world::structural_outing_plan> &plans,
    int &watch_path_budget );
std::string live_bandit_structural_route_analyzer_record_for_test(
    const bandit_live_world::site_record &site,
    const bandit_live_world::structural_outing_plan &plan,
    const std::string &selector,
    const bandit_live_world::structural_route_read &read );
std::string live_bandit_structural_signal_request_diagnostic_for_test(
    const bandit_live_world::active_outing_state &outing,
    const bandit_live_world::structural_threat_observer_request &request );
bool live_bandit_local_handoff_position_is_motor_addressable(
    const tripoint_abs_ms &position, const tripoint_abs_sm &motor_center,
    int motor_radius_sm );

#endif // CATA_SRC_DO_TURN_H
