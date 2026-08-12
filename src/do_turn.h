#pragma once
#ifndef CATA_SRC_DO_TURN_H
#define CATA_SRC_DO_TURN_H

#include "coordinates.h"

#include <string>
#include <vector>

namespace bandit_live_world
{
struct site_record;
struct structural_route_read;
struct structural_outing_plan;
} // namespace bandit_live_world

void handle_key_blocking_activity();
bool process_live_bandit_aftermath_for_test();
bool materialize_live_bandit_structural_handoffs_for_test();
void maintain_live_bandit_local_pair_cohesion_for_test();
bool dematerialize_live_bandit_structural_handoffs_for_test();
void process_monsters_and_npcs_turn_for_test();
void process_overmap_npc_move_for_test();
void run_live_bandit_structural_route_analyzer_for_debug();
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
bool live_bandit_local_handoff_position_is_motor_addressable(
    const tripoint_abs_ms &position, const tripoint_abs_sm &motor_center,
    int motor_radius_sm );

#endif // CATA_SRC_DO_TURN_H
