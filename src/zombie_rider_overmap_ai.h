#pragma once

#include "bandit_mark_generation.h"
#include "coordinates.h"

#include <optional>
#include <string>
#include <vector>

class monster;

namespace zombie_rider_overmap_ai
{
constexpr int mature_world_gate_seasons = 8;
constexpr int max_riders_drawn_by_light = 2;
// Abstract candidate envelope.  The live adapter currently supplies loaded monsters only.
constexpr int rider_convergence_response_radius_omt = 36;
constexpr int rider_band_minimum_size = 2;
constexpr int rider_light_memory_decay_interval_turns = 60;

int mature_world_gate_days();

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
    int decay_turn_remainder = 0;
    std::string reason = "none";

    bool active() const {
        return interest_score > 0 && turns_remaining > 0 && max_riders_drawn > 0;
    }
};

struct rider_overmap_agent {
    std::string rider_id;
    tripoint_abs_omt pos = tripoint_abs_omt::zero;
    bool available = true;
    bool already_in_band = false;
    int cooldown_turns = 0;
};

struct rider_convergence_result {
    bool should_converge = false;
    bool band_formed = false;
    int selected_riders = 0;
    int band_size = 0;
    int cap = 0;
    std::string posture = "none";
    std::string reason = "none";
    std::vector<std::string> rider_ids;
    std::vector<std::string> notes;
};

enum class rider_camp_pressure_posture {
    none,
    investigate,
    circle_harass,
    direct_attack,
    withdraw,
};

struct rider_camp_pressure_input {
    bool light_memory_active = false;
    int rider_count = 0;
    bool band_formed = false;
    bool breach_or_opening = false;
    int defender_strength = 0;
    bool rider_wounded = false;
};

struct rider_camp_pressure_result {
    rider_camp_pressure_posture posture = rider_camp_pressure_posture::none;
    std::string reason = "none";
    std::vector<std::string> notes;
};

struct rider_camp_pressure_intent {
    rider_camp_pressure_posture posture = rider_camp_pressure_posture::none;
    tripoint_abs_ms source = tripoint_abs_ms::zero;
    int formation_slot = 0;
    int turns_remaining = 0;
};

rider_light_interest evaluate_light_attraction(
    const bandit_mark_generation::light_projection &projection,
    int world_age_days,
    int eligible_riders_nearby = 1 );
void refresh_light_memory( rider_light_memory &memory, const rider_light_interest &interest );
void advance_light_memory( rider_light_memory &memory, int elapsed_turns );
rider_convergence_result evaluate_rider_convergence(
    const rider_light_memory &memory,
    const tripoint_abs_omt &light_omt,
    const std::vector<rider_overmap_agent> &riders );
void reserve_rider_convergence( std::vector<rider_overmap_agent> &riders,
                                const rider_convergence_result &convergence );
rider_camp_pressure_result choose_camp_pressure_posture(
    const rider_camp_pressure_input &input );
std::string to_string( rider_camp_pressure_posture posture );
void set_camp_pressure_intent( monster &rider, rider_camp_pressure_posture posture,
                               const tripoint_abs_ms &source, int duration_turns,
                               int formation_slot = 0 );
std::optional<rider_camp_pressure_intent> get_camp_pressure_intent( monster &rider );
void clear_camp_pressure_intent( monster &rider );

} // namespace zombie_rider_overmap_ai
