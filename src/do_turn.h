#pragma once
#ifndef CATA_SRC_DO_TURN_H
#define CATA_SRC_DO_TURN_H

#include "coordinates.h"

void handle_key_blocking_activity();
bool process_live_bandit_aftermath_for_test();
bool materialize_live_bandit_structural_handoffs_for_test();
void process_monsters_and_npcs_turn_for_test();
void process_overmap_npc_move_for_test();
bool live_bandit_local_handoff_position_is_motor_addressable(
    const tripoint_abs_ms &position, const tripoint_abs_sm &motor_center,
    int motor_radius_sm );

#endif // CATA_SRC_DO_TURN_H
