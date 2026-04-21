#pragma once

#include "bandit_dry_run.h"

#include <array>
#include <string>
#include <vector>

namespace bandit_mark_generation
{
enum class cadence_tier {
    nearby_active,
    distant_inactive,
    daily_cleanup,
};

enum class smoke_weather_band {
    clear,
    windy,
    rain,
    fog,
};

enum class light_time_band {
    daylight,
    twilight,
    night,
};

enum class light_weather_band {
    clear,
    rain,
    fog,
};

enum class light_exposure_band {
    contained,
    screened,
    exposed,
};

enum class light_source_band {
    ordinary,
    searchlight,
};

enum class light_terrain_band {
    open,
    built_cover,
    forest,
};

enum class human_route_kind {
    direct_sighting,
    route_activity,
};

enum class human_route_origin {
    same_camp,
    ambiguous,
    external,
    shared,
};

enum class human_route_corroboration {
    none,
    corridor,
    site,
};

struct signal_input {
    std::string id;
    std::string kind;
    std::string envelope_id;
    std::string region_id;
    bandit_dry_run::lead_family family = bandit_dry_run::lead_family::none;
    int strength = 0;
    int confidence = 0;
    int threat_add = 0;
    int bounty_add = 0;
    int monster_pressure_add = 0;
    int target_coherence_subtract = 0;
    int reward_profile_match = 0;
    double distance_multiplier = 1.0;
    bandit_dry_run::threat_gate threat_gate_result = bandit_dry_run::threat_gate::discount_only;
    std::vector<bandit_dry_run::job_template> hard_blocked_jobs;
    bool confirmed_threat = false;
    bool soft_decay = true;
    std::vector<std::string> notes;
};

struct smoke_packet {
    std::string id;
    std::string envelope_id;
    std::string region_id;
    bandit_dry_run::lead_family family = bandit_dry_run::lead_family::site;
    int observed_range_omt = 0;
    int source_strength = 0;
    int persistence = 0;
    int height_bias = 0;
    int spread_bias = 0;
    smoke_weather_band weather = smoke_weather_band::clear;
    std::vector<std::string> notes;
};

struct smoke_projection {
    smoke_packet packet;
    signal_input signal;
    int visibility_score = 0;
    int projected_range_omt = 0;
    bool viable = false;
};

struct light_packet {
    std::string id;
    std::string envelope_id;
    std::string region_id;
    bandit_dry_run::lead_family family = bandit_dry_run::lead_family::site;
    int observed_range_omt = 0;
    int source_strength = 0;
    int persistence = 0;
    int side_leakage = 0;
    light_time_band time = light_time_band::night;
    light_weather_band weather = light_weather_band::clear;
    light_exposure_band exposure = light_exposure_band::screened;
    light_source_band source = light_source_band::ordinary;
    light_terrain_band terrain = light_terrain_band::open;
    std::vector<std::string> notes;
};

struct light_concealment_packet {
    int daylight_penalty = 0;
    int weather_penalty = 0;
    int exposure_bonus = 0;
    int side_leakage_bonus = 0;
    int terrain_penalty = 0;
    int visibility_modifier = 0;
    std::string verdict;
    std::string summary;
};

struct light_projection {
    light_packet packet;
    signal_input signal;
    light_concealment_packet concealment;
    int visibility_score = 0;
    int projected_range_omt = 0;
    bool viable = false;
    std::string review_summary;
};

struct human_route_packet {
    std::string id;
    std::string envelope_id;
    std::string region_id;
    bandit_dry_run::lead_family family = bandit_dry_run::lead_family::corridor;
    int observed_range_omt = 0;
    int source_strength = 0;
    int persistence = 0;
    human_route_kind kind = human_route_kind::route_activity;
    human_route_origin origin = human_route_origin::ambiguous;
    human_route_corroboration corroboration = human_route_corroboration::none;
    std::vector<std::string> notes;
};

struct human_route_projection {
    human_route_packet packet;
    signal_input signal;
    int visibility_score = 0;
    int projected_range_omt = 0;
    bandit_dry_run::lead_family projected_family = bandit_dry_run::lead_family::none;
    bool viable = false;
};

struct repeated_site_reinforcement_packet {
    int total_site_hits = 0;
    int distinct_signal_count = 0;
    int confidence_bonus = 0;
    int bounty_bonus = 0;
    bool saw_smoke = false;
    bool saw_light = false;
    bool saw_route_activity = false;
    bool viable = false;
};

struct typed_mark {
    std::string id;
    std::string kind;
    std::string envelope_id;
    std::string region_id;
    bandit_dry_run::lead_family family = bandit_dry_run::lead_family::none;
    int strength = 0;
    int confidence = 0;
    int threat_add = 0;
    int bounty_add = 0;
    int monster_pressure_add = 0;
    int target_coherence_subtract = 0;
    int reward_profile_match = 0;
    double distance_multiplier = 1.0;
    bandit_dry_run::threat_gate threat_gate_result = bandit_dry_run::threat_gate::discount_only;
    std::vector<bandit_dry_run::job_template> hard_blocked_jobs;
    bool confirmed_threat = false;
    bool soft_decay = true;
    int age_turns = 0;
    int last_refresh_tick = 0;
    repeated_site_reinforcement_packet repeated_site_reinforcement;
    std::vector<std::string> notes;
};

struct heat_cell {
    std::string region_id;
    int threat_heat = 0;
    int bounty_heat = 0;
    size_t mark_count = 0;
    size_t confirmed_threat_marks = 0;
};

struct ledger_metrics {
    size_t events_ingested = 0;
    size_t marks_created = 0;
    size_t marks_refreshed = 0;
    size_t marks_cooled = 0;
    size_t marks_pruned = 0;
    size_t sticky_threat_preserved = 0;
    size_t leads_emitted = 0;
};

struct ledger_state {
    int last_tick = 0;
    std::vector<typed_mark> marks;
    std::vector<heat_cell> heat;
};

struct update_result {
    ledger_metrics metrics;
    std::vector<bandit_dry_run::lead_input> leads;
};

update_result advance_state( ledger_state &state, int tick, cadence_tier tier,
                             const std::vector<signal_input> &signals );
std::vector<bandit_dry_run::lead_input> emit_leads( const ledger_state &state,
        ledger_metrics *metrics = nullptr );
std::string render_report( const ledger_state &state,
                           const std::vector<bandit_dry_run::lead_input> *leads = nullptr );
smoke_projection adapt_smoke_packet( const smoke_packet &packet );
light_projection adapt_light_packet( const light_packet &packet );
human_route_projection adapt_human_route_packet( const human_route_packet &packet );
std::string to_string( cadence_tier tier );
std::string to_string( smoke_weather_band weather );
std::string to_string( light_time_band time );
std::string to_string( light_weather_band weather );
std::string to_string( light_exposure_band exposure );
std::string to_string( light_source_band source );
std::string to_string( light_terrain_band terrain );
std::string to_string( human_route_kind kind );
std::string to_string( human_route_origin origin );
std::string to_string( human_route_corroboration corroboration );
} // namespace bandit_mark_generation
