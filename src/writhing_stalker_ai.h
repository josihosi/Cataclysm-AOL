#pragma once

#include <string>
#include <vector>

namespace writhing_stalker
{

enum class interest_source {
    none,
    human,
    light,
    terrain,
    zombie_pressure,
    smoke
};

enum class approach_class {
    none,
    cover_shadow,
    edge_shadow,
    direct_forced,
    hold_exposed
};

enum class decision {
    ignore,
    interested,
    shadow,
    hold,
    strike,
    withdraw,
    cooling_off
};

struct interest_context {
    bool recent_human_evidence = false;
    int evidence_age_minutes = 0;
    bool exposed_night_light = false;
    bool smoke = false;
    bool forest_or_building_edge = false;
    bool town_or_road_edge = false;
    int zombie_pressure = 0;
};

struct interest_report {
    interest_source source = interest_source::none;
    int score = 0;
    int confidence = 0;
    bool can_latch = false;
    std::string reason;
};

struct latch_state {
    bool active = false;
    int age_minutes = 0;
    int confidence = 0;
    int leash_tiles_remaining = 0;
    int cooldown_minutes = 0;
};

struct latch_update {
    latch_state state;
    decision next = decision::ignore;
    std::string reason;
};

struct latch_context {
    latch_state current;
    interest_report interest;
    int elapsed_minutes = 0;
    int distance_tiles = 0;
    bool exposed_or_focused = false;
};

struct approach_context {
    latch_state latch;
    bool cover_route_available = false;
    bool edge_route_available = false;
    bool direct_open_route_available = false;
    bool bright_exposure = false;
    bool forced_no_cover = false;
};

struct approach_report {
    approach_class route = approach_class::none;
    decision next = decision::ignore;
    bool avoids_direct_line = false;
    std::string reason;
};

struct opportunity_context {
    latch_state latch;
    bool player_bleeding = false;
    bool player_hurt = false;
    bool player_low_stamina = false;
    bool player_distracted = false;
    bool player_noisy = false;
    int zombie_pressure = 0;
    int allied_support_nearby = 0;
    bool near_cover_or_clutter = false;
    bool bright_exposure = false;
    bool player_focused = false;
    bool stalker_hurt = false;
    int distance_to_target = 0;
    int burst_strikes = 0;
};

struct opportunity_report {
    int opportunity = 0;
    int vulnerability = 0;
    int zombie_distraction = 0;
    int exposure_penalty = 0;
    int burst_limit = 1;
    int retreat_distance = 8;
    decision next = decision::ignore;
    std::string reason;
};

struct relative_point {
    int rel_x = 0;
    int rel_y = 0;
    int weight = 1;
};

struct quiet_side_report {
    int pressure_x = 0;
    int pressure_y = 0;
    int pressure_count = 0;
    bool has_dominant_pressure = false;
    bool ambiguous_pressure = false;
    int quiet_x = 0;
    int quiet_y = 0;
};

struct quiet_candidate {
    int rel_x = 0;
    int rel_y = 0;
    int distance_to_stalker = 0;
    bool passable = true;
    bool occupied = false;
    bool shadow_or_cover = false;
    bool broken_line_of_sight = false;
    bool bright_exposure = false;
    int retreat_alignment = 0;
};

struct quiet_candidate_report {
    bool has_candidate = false;
    quiet_candidate chosen;
    quiet_side_report pressure;
    int score = 0;
    int quiet_alignment = 0;
    int crowding_penalty = 0;
    std::string reason;
};

struct confidence_context {
    bool has_believable_local_evidence = false;
    bool has_overmap_interest_footing = false;
    int zombie_pressure = 0;
    bool quiet_side_cutoff_available = false;
    bool target_in_bright_exposure = false;
    bool stalker_in_bright_exposure = false;
    bool target_has_focus = false;
    bool open_exposure = false;
    bool stalker_hurt = false;
};

struct confidence_report {
    int evidence = 0;
    int interest = 0;
    int zombie_pressure = 0;
    int quiet_side_cutoff = 0;
    int counterpressure = 0;
    int total = 0;
    bool pressure_allowed = false;
    bool cutoff_allowed = false;
    std::string reason;
};

struct live_context {
    bool has_believable_local_evidence = false;
    bool has_overmap_interest_footing = false;
    int evidence_age_minutes = 0;
    int distance_to_target = 0;
    bool target_in_bright_exposure = false;
    bool stalker_in_bright_exposure = false;
    bool target_has_focus = false;
    bool open_exposure = false;
    bool cover_route_available = false;
    bool edge_route_available = false;
    bool direct_open_route_available = false;
    bool forced_no_cover = false;
    bool player_bleeding = false;
    bool player_hurt = false;
    bool player_low_stamina = false;
    bool player_distracted = false;
    bool player_noisy = false;
    int zombie_pressure = 0;
    int allied_support_nearby = 0;
    bool quiet_side_cutoff_available = false;
    bool near_cover_or_clutter = false;
    bool stalker_hurt = false;
    bool on_cooldown = false;
    int burst_strikes = 0;
};

struct live_response {
    decision next = decision::ignore;
    approach_class route = approach_class::none;
    int opportunity = 0;
    int confidence = 0;
    int burst_limit = 1;
    int retreat_distance = 8;
    bool persistent_state_required = false;
    std::string reason;
};

interest_report evaluate_interest( const interest_context &ctx );
latch_update advance_latch( const latch_context &ctx );
approach_report choose_approach( const approach_context &ctx );
opportunity_report evaluate_opportunity( const opportunity_context &ctx );
quiet_side_report evaluate_quiet_side( const std::vector<relative_point> &zombies );
quiet_candidate_report choose_quiet_side_cutoff( const std::vector<relative_point> &zombies,
        const std::vector<quiet_candidate> &candidates );
confidence_report evaluate_confidence( const confidence_context &ctx );
live_response evaluate_live_response( const live_context &ctx );

} // namespace writhing_stalker
