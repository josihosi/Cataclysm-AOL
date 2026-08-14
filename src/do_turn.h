#pragma once
#ifndef CATA_SRC_DO_TURN_H
#define CATA_SRC_DO_TURN_H

#include "coordinates.h"

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
struct structural_outing_plan;
struct response_member_power_read;
} // namespace bandit_live_world

void handle_key_blocking_activity();
bool process_live_bandit_aftermath_for_test();
bool materialize_live_bandit_structural_handoffs_for_test();
int materialize_live_bandit_response_members_for_test( const std::string &site_id );
std::vector<bandit_live_world::response_member_power_read>
live_bandit_response_member_power_reads_for_test( const bandit_live_world::site_record &site );
std::size_t maintain_live_bandit_local_pair_cohesion_for_test();
bool dematerialize_live_bandit_structural_handoffs_for_test();
void process_monsters_and_npcs_turn_for_test();
void process_overmap_npc_move_for_test();
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
