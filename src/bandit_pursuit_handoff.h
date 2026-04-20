#pragma once

#include "bandit_dry_run.h"

#include <string>
#include <vector>

namespace bandit_pursuit_handoff
{
enum class contact_certainty {
    broad,
    localized,
    contact_ready,
};

enum class return_pressure_state {
    normal,
    withdraw_now,
};

enum class entry_mode {
    scout,
    probe,
    shadow,
    withdrawal,
};

enum class wound_burden_state {
    none,
    light,
    moderate,
    heavy,
    crippled,
};

enum class morale_state {
    steady,
    shaken,
    breaking,
    routed,
};

enum class mission_result {
    no_contact,
    scouted,
    partial_success,
    clean_success,
    withdrawn,
    repelled,
    broken,
    destroyed,
};

enum class lead_resolution {
    still_valid,
    narrowed,
    harvested,
    cleared,
    route_blocked,
    target_lost,
};

enum class return_posture {
    resume_route,
    escape_home,
    escape_safe,
    shadow_then_break,
    broken_flee,
};

enum class remaining_return_pressure_state {
    ample,
    tight,
    plain_return_only,
    collapsed,
};

struct cargo_profile {
    int food = 0;
    int meds = 0;
    int ammo = 0;
    int gear = 0;
    int fuel = 0;
    int trade_goods = 0;
};

struct anchored_identity_state {
    std::string id;
    std::string status = "alive";
};

struct abstract_group_state {
    std::string group_id;
    std::string source_camp_id;
    int group_strength = 0;
    int confidence = 0;
    int panic_threshold = 0;
    int cargo_capacity = 0;
    std::string current_target_or_mark;
    int current_threat_estimate = 0;
    int current_bounty_estimate = 0;
    int mission_urgency = 0;
    int retreat_bias = 0;
    int goal_stickiness = 0;
    int goal_preemption_posture = 0;
    int return_clock = 0;
    std::vector<anchored_identity_state> anchored_identities;
    std::vector<std::string> known_recent_marks;
    cargo_profile carried_cargo;
    wound_burden_state wound_burden = wound_burden_state::none;
    morale_state morale = morale_state::steady;
    return_posture last_return_posture = return_posture::resume_route;
    remaining_return_pressure_state remaining_pressure = remaining_return_pressure_state::ample;
};

struct entry_context {
    contact_certainty contact = contact_certainty::broad;
    return_pressure_state return_pressure = return_pressure_state::normal;
};

struct entry_payload {
    bool valid = false;
    std::vector<std::string> notes;
    std::string group_id;
    std::string source_camp_id;
    bandit_dry_run::job_template job_type = bandit_dry_run::job_template::hold_chill;
    bandit_dry_run::lead_family lead_carrier = bandit_dry_run::lead_family::none;
    entry_mode mode = entry_mode::withdrawal;
    std::string current_target_or_mark;
    int group_strength = 0;
    int confidence = 0;
    int panic_threshold = 0;
    int cargo_capacity = 0;
    int current_threat_estimate = 0;
    int current_bounty_estimate = 0;
    int mission_urgency = 0;
    int retreat_bias = 0;
    int goal_stickiness = 0;
    int goal_preemption_posture = 0;
    int return_clock = 0;
    bandit_dry_run::threat_gate danger_posture = bandit_dry_run::threat_gate::discount_only;
    std::vector<anchored_identity_state> anchored_identities;
    std::vector<std::string> known_recent_marks;
};

struct carrier_writeback {
    std::string carrier_id;
    std::string rewrite;
    int strength = 0;
};

struct local_outcome {
    int survivors_remaining = 0;
    std::vector<anchored_identity_state> anchored_identity_updates;
    wound_burden_state wound_burden = wound_burden_state::none;
    morale_state morale = morale_state::steady;
    cargo_profile cargo_profile_carried;
    cargo_profile camp_stockpile_delta;
    mission_result result = mission_result::no_contact;
    lead_resolution resolution = lead_resolution::still_valid;
    std::vector<carrier_writeback> threat_writeback;
    std::vector<carrier_writeback> bounty_writeback;
    std::vector<std::string> new_marks_learned;
    std::string loss_site_if_any;
    return_posture posture = return_posture::resume_route;
    remaining_return_pressure_state remaining_pressure = remaining_return_pressure_state::ample;
};

struct return_packet {
    bool valid = false;
    std::vector<std::string> notes;
    std::string group_id;
    std::string source_camp_id;
    bandit_dry_run::job_template job_type = bandit_dry_run::job_template::hold_chill;
    entry_mode mode = entry_mode::withdrawal;
    std::string current_target_or_mark;
    int survivors_remaining = 0;
    std::vector<anchored_identity_state> anchored_identity_updates;
    wound_burden_state wound_burden = wound_burden_state::none;
    morale_state morale = morale_state::steady;
    cargo_profile cargo_profile_carried;
    cargo_profile camp_stockpile_delta;
    mission_result result = mission_result::no_contact;
    lead_resolution resolution = lead_resolution::still_valid;
    std::vector<carrier_writeback> threat_writeback;
    std::vector<carrier_writeback> bounty_writeback;
    std::vector<std::string> new_marks_learned;
    std::string loss_site_if_any;
    return_posture posture = return_posture::resume_route;
    remaining_return_pressure_state remaining_pressure = remaining_return_pressure_state::ample;
};

bool supports_pursuit_handoff( const bandit_dry_run::candidate_debug &candidate );
entry_mode choose_entry_mode( const bandit_dry_run::candidate_debug &candidate,
                              contact_certainty contact,
                              return_pressure_state return_pressure );
entry_payload build_entry_payload( const abstract_group_state &group,
                                   const bandit_dry_run::candidate_debug &winner,
                                   const entry_context &context );
return_packet build_return_packet( const entry_payload &entry, const local_outcome &outcome );
void apply_return_packet( abstract_group_state &group, const return_packet &packet );

std::string to_string( contact_certainty certainty );
std::string to_string( return_pressure_state pressure );
std::string to_string( entry_mode mode );
std::string to_string( wound_burden_state wound );
std::string to_string( morale_state morale );
std::string to_string( mission_result result );
std::string to_string( lead_resolution resolution );
std::string to_string( return_posture posture );
std::string to_string( remaining_return_pressure_state pressure );
std::string render_report( const entry_payload &entry, const return_packet &packet );
} // namespace bandit_pursuit_handoff
