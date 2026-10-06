#pragma once
#ifndef CATA_SRC_DO_TURN_H
#define CATA_SRC_DO_TURN_H

#include "coordinates.h"
#include "live_light.h"

#include <cstddef>
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
bool persist_live_bandit_local_projection_leases_for_test( const bandit_live_world::site_record &site );
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
std::vector<bandit_live_world::structural_signal_read> live_bandit_staffed_camp_signal_reads_for_test(
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
void process_monsters_and_npcs_turn_for_test();
void process_overmap_npc_move_for_test();
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
std::optional<character_id> bandit_shakedown_speaker_id_for_test( const bandit_live_world::site_record &site );
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
