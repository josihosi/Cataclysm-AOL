#pragma once
#ifndef CATA_SRC_HARNESS_CREATURE_DEBUG_H
#define CATA_SRC_HARNESS_CREATURE_DEBUG_H

#include <string>
#include <vector>

#include "semantic_surface.h"

class avatar;
class Creature;

// Character IDs persist; monster addresses are handles for the current process/frame only.
std::string harness_creature_identity( const Creature &creature );
std::vector<semantic_action_descriptor> harness_creature_debug_actions( avatar &viewer );
// Empty means the exact visible living target is unavailable. No other creature is selected.
std::string harness_debug_kill_creature( avatar &viewer, const std::string &identity,
        const std::string &request_id, const std::string &run_id );
std::string harness_last_debug_creature_intervention( const std::string &run_id );

#endif // CATA_SRC_HARNESS_CREATURE_DEBUG_H
