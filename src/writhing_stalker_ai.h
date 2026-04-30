#pragma once

#include <string>

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
    bool near_cover_or_clutter = false;
    bool bright_exposure = false;
    bool player_focused = false;
    bool stalker_hurt = false;
};

struct opportunity_report {
    int opportunity = 0;
    int vulnerability = 0;
    int zombie_distraction = 0;
    int exposure_penalty = 0;
    decision next = decision::ignore;
    std::string reason;
};

struct live_context {
    bool has_believable_local_evidence = false;
    int evidence_age_minutes = 0;
    int distance_to_target = 0;
    bool target_in_bright_exposure = false;
    bool stalker_in_bright_exposure = false;
    bool target_has_focus = false;
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
    bool near_cover_or_clutter = false;
    bool stalker_hurt = false;
    bool on_cooldown = false;
};

struct live_response {
    decision next = decision::ignore;
    approach_class route = approach_class::none;
    int opportunity = 0;
    bool persistent_state_required = false;
    std::string reason;
};

interest_report evaluate_interest( const interest_context &ctx );
latch_update advance_latch( const latch_context &ctx );
approach_report choose_approach( const approach_context &ctx );
opportunity_report evaluate_opportunity( const opportunity_context &ctx );
live_response evaluate_live_response( const live_context &ctx );

} // namespace writhing_stalker
