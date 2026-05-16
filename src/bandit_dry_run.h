#pragma once

#include <map>
#include <string>
#include <vector>

namespace bandit_dry_run
{
enum class lead_family {
    none,
    site,
    corridor,
    moving_carrier,
    friendly_pressure,
};

enum class job_template {
    hold_chill,
    scout,
    scavenge,
    toll,
    stalk,
    steal,
    raid,
    reinforce,
};

enum class shortage_band {
    stable,
    low,
    critical,
};

enum class threat_gate {
    discount_only,
    soft_veto,
    hard_veto,
};

struct lead_input {
    std::string id;
    std::string envelope_id;
    lead_family family = lead_family::none;
    bool has_real_envelope = true;
    bool still_valid = true;
    bool has_path = true;
    double distance_multiplier = 1.0;
    int lead_bounty_value = 0;
    int lead_confidence_bonus = 0;
    int threat_penalty = 0;
    int monster_pressure_bonus = 0;
    int target_coherence_bonus = 0;
    int active_pressure_penalty = 0;
    int reward_profile_match = 0;
    threat_gate threat_gate_result = threat_gate::discount_only;
    std::vector<job_template> hard_blocked_jobs;
    std::vector<std::string> validity_notes;
};

struct camp_input {
    int available_manpower = 0;
    shortage_band shortage = shortage_band::stable;
    bool allow_bounded_explore = false;
    int explore_pressure = 0;
    double explore_distance_multiplier = 0.35;
    std::map<job_template, int> temperament_bias;
    std::map<job_template, int> job_type_bonus;
};

struct lead_debug {
    std::string id;
    std::string envelope_id;
    lead_family family = lead_family::none;
    bool has_real_envelope = true;
    bool still_valid = true;
    bool deduped = false;
    std::vector<std::string> notes;
};

struct score_debug {
    int lead_bounty_value = 0;
    int lead_confidence_bonus = 0;
    int job_lead_fit_bonus = 0;
    int ordinary_need_alignment = 0;
    int temperament_bias = 0;
    int job_type_bonus = 0;
    double positive_pull = 0.0;
    double distance_multiplier = 1.0;
    double travel_shaped_pull = 0.0;
    int threat_penalty = 0;
    int monster_pressure_bonus = 0;
    int target_coherence_bonus = 0;
    int effective_threat_penalty = 0;
    int active_pressure_penalty = 0;
    int opportunism_bonus = 0;
    double pre_veto_job_score = 0.0;
    int shortage_band_bonus = 0;
    int reward_profile_match = 0;
    int need_override_bonus = 0;
    double need_adjusted_job_score = 0.0;
    threat_gate threat_gate_result = threat_gate::discount_only;
    double final_job_score = 0.0;
};

struct candidate_debug {
    job_template job = job_template::hold_chill;
    std::string lead_id;
    std::string envelope_id;
    lead_family family = lead_family::none;
    bool generated = false;
    bool valid = true;
    bool winner = false;
    score_debug score;
    std::vector<std::string> notes;
};

struct evaluation_metrics {
    size_t input_lead_count = 0;
    size_t accepted_lead_count = 0;
    size_t rejected_no_envelope_count = 0;
    size_t rejected_invalid_lead_count = 0;
    size_t deduped_lead_count = 0;
    size_t compatible_job_checks = 0;
    size_t hard_blocked_job_count = 0;
    size_t manpower_rejection_count = 0;
    size_t candidates_generated = 0;
    size_t score_evaluations = 0;
    size_t path_checks = 0;
    size_t valid_outward_candidates = 0;
    size_t invalid_outward_candidates = 0;
    size_t need_override_rescues = 0;
    size_t soft_veto_caps = 0;
    size_t soft_veto_collapses = 0;
    size_t hard_veto_invalidations = 0;
    size_t no_path_invalidations = 0;
    size_t winner_comparisons = 0;
};

struct evaluation_result {
    std::vector<lead_debug> leads;
    std::vector<candidate_debug> candidates;
    std::vector<std::string> return_packet_fields_touched;
    evaluation_metrics metrics;
    size_t winner_index = 0;
    std::string winner_reason;
};

evaluation_result evaluate( const camp_input &camp, const std::vector<lead_input> &leads );
std::string render_report( const evaluation_result &result );
std::string to_string( lead_family family );
std::string to_string( job_template job );
std::string to_string( shortage_band shortage );
std::string to_string( threat_gate gate );
} // namespace bandit_dry_run
