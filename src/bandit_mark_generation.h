#pragma once

#include "bandit_dry_run.h"

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
std::string to_string( cadence_tier tier );
std::string to_string( smoke_weather_band weather );
} // namespace bandit_mark_generation
