#pragma once
#ifndef CATA_SRC_DO_TURN_H
#define CATA_SRC_DO_TURN_H

#include "coordinates.h"
#include "live_light.h"

#include <cstddef>
#include <string>
#include <vector>

class avatar;
class map;
class monster;

namespace bandit_live_world
{
struct site_record;
struct active_outing_state;
struct structural_threat_observer_request;
struct structural_route_read;
struct structural_signal_read;
struct structural_outing_plan;
struct response_member_power_read;
} // namespace bandit_live_world

void handle_key_blocking_activity();
// Clear the transient physical-light packet when a world/save becomes the
// active simulation.  Observations are never persisted in this cache.
void reset_live_light_sample_cache();
void run_live_light_delivery_for_test();
int observe_live_hostile_signal_sources_for_test(
    const std::vector<live_bandit_signal_observation> &signals );
void maintain_live_bandit_structural_bounty_for_test(
    const std::vector<live_bandit_signal_observation> &signals );
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
bool materialize_live_bandit_structural_handoffs_for_test();
int materialize_live_bandit_response_members_for_test( const std::string &site_id );
std::vector<bandit_live_world::response_member_power_read>
live_bandit_response_member_power_reads_for_test( const bandit_live_world::site_record &site );
std::size_t maintain_live_bandit_local_pair_cohesion_for_test();
bool dematerialize_live_bandit_structural_handoffs_for_test();
void process_monsters_and_npcs_turn_for_test();
void process_overmap_npc_move_for_test();
bool materialize_committed_bandit_shakedown_for_test( bandit_live_world::site_record &site );
bool complete_live_bandit_homeward_boundary_for_test();
void note_live_bandit_aftermath_for_test();
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
