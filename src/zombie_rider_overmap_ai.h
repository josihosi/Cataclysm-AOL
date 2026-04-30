#pragma once

#include "bandit_mark_generation.h"

#include <string>
#include <vector>

namespace zombie_rider_overmap_ai
{
constexpr int mature_world_gate_days = 730;
constexpr int max_riders_drawn_by_light = 2;

struct rider_light_interest {
    bool should_investigate = false;
    int interest_score = 0;
    int memory_turns = 0;
    int max_riders_drawn = 0;
    std::string reason = "none";
    std::vector<std::string> notes;
};

struct rider_light_memory {
    int interest_score = 0;
    int turns_remaining = 0;
    int max_riders_drawn = 0;
    std::string reason = "none";

    bool active() const {
        return interest_score > 0 && turns_remaining > 0 && max_riders_drawn > 0;
    }
};

rider_light_interest evaluate_light_attraction(
    const bandit_mark_generation::light_projection &projection,
    int world_age_days,
    int eligible_riders_nearby = 1 );
void refresh_light_memory( rider_light_memory &memory, const rider_light_interest &interest );
void advance_light_memory( rider_light_memory &memory, int elapsed_turns );

} // namespace zombie_rider_overmap_ai
