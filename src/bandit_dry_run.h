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
    int active_pressure_penalty = 0;
    int reward_profile_match = 0;
    threat_gate threat_gate_result = threat_gate::discount_only;
    std::vector<job_template> hard_blocked_jobs;
    std::vector<std::string> validity_notes;
};

struct camp_input {
    int available_manpower = 0;
    shortage_band shortage = shortage_band::stable;
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
    int active_pressure_penalty = 0;
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

struct evaluation_result {
    std::vector<lead_debug> leads;
    std::vector<candidate_debug> candidates;
    std::vector<std::string> return_packet_fields_touched;
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
