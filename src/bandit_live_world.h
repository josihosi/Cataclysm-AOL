#pragma once

#include "bandit_dry_run.h"
#include "bandit_pursuit_handoff.h"

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "character_id.h"
#include "coordinates.h"

class JsonObject;
class JsonOut;

namespace bandit_live_world
{
enum class anchor_source_kind {
    none,
    overmap_special,
    map_extra,
};

enum class owned_site_kind {
    none,
    bandit_camp,
    bandit_work_camp,
    bandit_cabin,
    cannibal_camp,
    looters,
    bandits_block,
};

enum class hostile_site_profile {
    none,
    camp_style,
    cannibal_camp,
    small_hostile_site,
};

enum class member_state {
    at_home,
    outbound,
    local_contact,
    orphaned,
    dead,
    missing,
};

enum class origin_disposition {
    active_hostile,
    captured_non_hostile,
    deleted,
    invalidated,
};

struct origin_loss_resolution_effect {
    bool valid = false;
    bool changed = false;
    bool reservation_released = false;
    int orphaned_survivors = 0;
    int dead_members = 0;
    int missing_members = 0;
};

enum class active_member_observation_state {
    unresolved,
    local_contact,
    returning_home,
    home,
    dead,
    missing,
};

enum class local_gate_posture {
    stalk,
    hold_off,
    probe,
    open_shakedown,
    attack_now,
    abort,
};

enum class camp_lead_kind {
    structural_bounty,
    terrain_opportunity,
    harvested_site,
    human_activity,
    basecamp_activity,
    moving_actor,
    route_activity,
    smoke_signal,
    light_signal,
    sound_signal,
    threat_memory,
    loss_site,
    false_lead,
    frontier_probe,
};

enum class camp_lead_origin {
    legacy_radar,
    observer,
    signal,
    returned_report,
    structural_routine,
};

enum class camp_lead_status {
    suspected,
    scout_confirmed,
    active,
    harvested,
    stale,
    invalidated,
    dangerous,
};

enum class outing_kind {
    none,
    scout_sortie,
    hostile_operation,
    structural_sortie,
};

enum class structural_watch_kind {
    none,
    exact,
    fallback,
};

enum class simulation_owner {
    abstract,
    local,
};

enum class simulation_owner_transition_result {
    rejected,
    unchanged,
    applied,
};

struct simulation_advance_cursor {
    std::string activity_id;
    int generation = 0;
    simulation_owner owner = simulation_owner::abstract;
    int handoff_epoch = -1;
    int last_advanced_minutes = -1;
    int covert_egress_revision = 0;
};

enum class scout_phase {
    assembling,
    outbound,
    searching,
    observing,
    harvesting,
    burned_withdrawal,
    returning_exposed,
    returning_report,
    returning_home,
    lost,
};

enum class scout_phase_transition_result {
    rejected,
    unchanged,
    applied,
};

enum class scout_assessment_threshold_class {
    none,
    normal,
    burned,
};

enum class scout_assessment_result {
    rejected,
    unchanged,
    updated,
    alternate_watch_reposition_required,
    alternate_watch_started,
    normal_success,
    inconclusive,
};

enum class sortie_observation_kind {
    routine,
    certainty,
    bounds,
    route_state,
    alert,
    target_revision,
    hard_danger,
    contradiction,
    casualty,
    burn,
};

enum class sortie_observation_sense {
    visual,
    smoke,
    light,
    sound,
};

enum class sortie_observation_share_state {
    observer_private,
    shared,
    reported,
};

enum class camp_decision_state {
    idle,
    report_awaiting_assessment,
    preparing_follow_on,
    cooldown,
    abandoned,
};

enum class camp_decision_transition_result {
    rejected,
    unchanged,
    applied,
};

enum class camp_report_policy {
    none,
    bandit_shakedown,
    cannibal_night_raid,
};

enum class hostile_operation_kind {
    none,
    shakedown,
    raid,
};

enum class hostile_operation_phase {
    assembling,
    outbound,
    rallying,
    waiting_night,
    approaching,
    committed_contact,
    returning_home,
    lost,
};

enum class hostile_operation_transition_result {
    rejected,
    unchanged,
    applied,
};

struct active_member_observation {
    character_id npc_id;
    active_member_observation_state state = active_member_observation_state::unresolved;
    std::string summary;
};

struct member_record {
    character_id npc_id;
    std::string npc_template_id;
    tripoint_abs_ms home_spawn_tile;
    member_state state = member_state::at_home;
    bool wounded_or_unready = false;
    std::string last_writeback_summary;
    int abstract_wound_until_minutes = -1;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct spawn_tile_record {
    tripoint_abs_ms tile;
    int assigned_living_total = 0;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct camp_map_lead {
    std::string lead_id;
    int revision = 1;
    camp_lead_kind kind = camp_lead_kind::human_activity;
    camp_lead_origin origin = camp_lead_origin::legacy_radar;
    camp_lead_status status = camp_lead_status::suspected;
    std::string target_id;
    tripoint_abs_omt omt;
    int radius_omt = 0;
    std::string source_key;
    std::string source_summary;
    int first_seen_minutes = -1;
    int last_seen_minutes = -1;
    int last_checked_minutes = -1;
    int last_scouted_minutes = -1;
    int bounty = 0;
    int threat = 0;
    int confidence = 0;
    bool threat_confirmed = false;
    bool target_alert = false;
    bool scout_seen = false;
    bool generated_by_this_camp_routine = false;
    int prior_bandit_losses = 0;
    int prior_defender_losses = 0;
    int times_checked_empty = 0;
    int times_harvested = 0;
    std::string last_outcome;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct camp_intelligence_map {
    int schema_version = 5;
    int last_daily_cleanup_minutes = -1;
    int next_near_tick_minutes = -1;
    int next_mid_tick_minutes = -1;
    int next_far_tick_minutes = -1;
    int next_frontier_tick_minutes = -1;
    int known_radius_omt = 0;
    int terrain_scan_cursor = 0;
    std::string last_routine_target_lead_id;
    std::string previous_routine_target_lead_id;
    int frontier_radius_omt = 0;
    int frontier_sector_cursor = 0;
    std::vector<int> frontier_last_resolved_minutes = std::vector<int>( 8, -1 );
    std::vector<camp_map_lead> leads;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );

    camp_map_lead *find_lead( const std::string &lead_id );
    const camp_map_lead *find_lead( const std::string &lead_id ) const;
};

struct sortie_observation {
    sortie_observation() = default;
    sortie_observation( const std::string &fact_key, const std::string &summary, int confidence,
                        int observed_minutes, bool critical, sortie_observation_kind kind,
                        const std::string &state_key ) :
        fact_key( fact_key ), summary( summary ), confidence( confidence ),
        observed_minutes( observed_minutes ), critical( critical ), kind( kind ),
        state_key( state_key ) {}

    std::string fact_key;
    std::string summary;
    int confidence = 0;
    int observed_minutes = -1;
    bool critical = false;
    sortie_observation_kind kind = sortie_observation_kind::routine;
    std::string state_key;
    int record_schema_version = 0;
    std::string source_id;
    sortie_observation_sense sense = sortie_observation_sense::visual;
    character_id observer_id;
    tripoint_abs_omt source_omt;
    tripoint_abs_omt receiver_omt;
    int bucket_start_minutes = -1;
    int strength = 0;
    int visual_quality = 0;
    std::vector<std::string> defender_ids;
    int observed_defender_count = -1;
    int simultaneity_start_minutes = -1;
    int simultaneity_end_minutes = -1;
    int observed_power_low = 0;
    int observed_power_high = 0;
    int equipment_detail = 0;
    int target_revision = 0;
    int uncertainty_radius_omt = 0;
    int expiry_minutes = -1;
    sortie_observation_share_state share_state =
        sortie_observation_share_state::observer_private;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct sortie_observation_effect {
    bool valid = false;
    bool changed = false;
    bool progress = false;
    int inserted = 0;
    int replaced = 0;
    int evicted = 0;
};

struct sortie_cargo {
    int supply_units = 0;
    int trade_value = 0;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct local_handoff_member_snapshot {
    character_id npc_id;
    tripoint_abs_ms prior_position;
    tripoint_abs_ms entry_position;
    tripoint_abs_ms staging_position;
    tripoint_abs_ms exit_position;
    int hp_percent = 0;
    bool dead = false;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct local_handoff_snapshot {
    int schema_version = 3;
    std::string activity_id;
    int activity_generation = 0;
    int handoff_epoch = -1;
    int waypoint_index = 0;
    scout_phase phase = scout_phase::assembling;
    tripoint_abs_omt route_position;
    tripoint_abs_omt approach_from;
    tripoint_abs_omt egress_omt;
    sortie_cargo cargo;
    std::vector<character_id> casualty_ids;
    std::vector<local_handoff_member_snapshot> members;
    character_id cohesion_leader_id;
    int cohesion_deadline_minutes = -1;
    int cohesion_reroutes_used = 0;
    bool cohesion_assembled = false;
    bool cohesion_abort_return = false;
    int committed_minutes = -1;

    void clear();
    bool is_active() const;
    bool is_abstract_resume() const;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct abstract_encounter_state {
    int schema_version = 1;
    int episode = 0;
    int last_applied_episode = 0;
    bool active = false;
    tripoint_abs_omt overlap_omt;
    std::vector<std::string> stable_threat_ids;
    int danger_low = 0;
    int danger_high = 0;
    int absent_segment_advances = 0;
    int detour_attempts = 0;
    bool has_selected_detour = false;
    tripoint_abs_omt selected_detour_omt;
    bool local_claimed = false;
    bool outcome_applied = false;
    std::string outcome;

    void clear_active();
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct local_handoff_member_read {
    character_id npc_id;
    bool bindable = false;
    bool dead = false;
    int hp_percent = 0;
    tripoint_abs_ms current_position;
    tripoint_abs_ms entry_position;
    tripoint_abs_ms staging_position;
};

struct local_pair_casualty_read {
    character_id npc_id;
    member_state state = member_state::dead;
    tripoint_abs_ms last_position;
};

struct local_handoff_plan {
    bool valid = false;
    simulation_advance_cursor expected_cursor;
    local_handoff_snapshot snapshot;
    std::vector<std::string> notes;
};

struct local_dematerialization_member_read {
    character_id npc_id;
    bool readable = false;
    bool dead = false;
    bool homeward_route_confirmed = false;
    int hp_percent = 0;
    tripoint_abs_ms current_position;
};

struct local_dematerialization_plan {
    bool valid = false;
    simulation_advance_cursor expected_cursor;
    local_handoff_snapshot resume_snapshot;
    std::vector<std::string> notes;
};

struct local_alternate_watch_member_read {
    character_id npc_id;
    bool readable = false;
    bool dead = false;
    bool alternate_route_confirmed = false;
    int hp_percent = 0;
    tripoint_abs_ms current_position;
};

struct local_alternate_watch_reposition_plan {
    bool valid = false;
    simulation_advance_cursor expected_cursor;
    int expected_target_revision = 0;
    std::vector<character_id> expected_member_ids;
    structural_watch_kind expected_selected_watch_kind = structural_watch_kind::none;
    tripoint_abs_omt expected_selected_watch_omt;
    int expected_selected_watch_route_cost = -1;
    structural_watch_kind expected_alternate_watch_kind = structural_watch_kind::none;
    tripoint_abs_omt expected_alternate_watch_omt;
    int expected_alternate_watch_route_cost = -1;
    std::vector<tripoint_abs_omt> expected_shared_route;
    std::vector<tripoint_abs_omt> expected_alternate_watch_shared_route;
    local_handoff_snapshot resume_snapshot;
    std::vector<std::string> notes;
};

struct local_cohesion_member_read {
    character_id npc_id;
    bool present = false;
    bool dead = false;
    tripoint_abs_ms current_position;
};

struct local_cohesion_plan {
    bool valid = false;
    simulation_advance_cursor expected_cursor;
    local_handoff_snapshot snapshot;
    character_id leader_id;
    character_id follower_id;
    std::vector<std::pair<character_id, tripoint_abs_ms>> movement_orders;
    bool share_private_observations = false;
    int observations_shared = 0;
    bool reroute_needed = false;
    bool abort_return = false;
    std::vector<std::string> notes;
};

enum class local_handoff_commit_result {
    rejected,
    unchanged,
    applied,
    rolled_back,
};

struct scout_assessment_state {
    int schema_version = 2;
    int observation_started_minutes = -1;
    int last_progress_minutes = -1;
    int burned_minutes = -1;
    tripoint_abs_omt burn_origin_omt;
    int certainty = 0;
    bool readiness_latched = false;
    scout_assessment_threshold_class threshold_class =
        scout_assessment_threshold_class::none;
    int strong_visual_windows = 0;
    int defenders_low = 0;
    int defenders_high = 0;
    int danger_low = 0;
    int danger_high = 0;
    int bounty_estimate = 0;
    int route_danger_high = 0;
    int target_alert = 0;
    int pinned_target_revision = 0;
    int next_eligible_minutes = -1;
    std::string exit_reason;

    void clear();
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct scout_report_record {
    int schema_version = 5;
    int revision = 0;
    camp_report_policy action_policy = camp_report_policy::none;
    std::string source_activity_id;
    int source_generation = 0;
    std::string source_job_type;
    std::string target_id;
    tripoint_abs_omt target_omt;
    std::string target_lead_id;
    int target_lead_revision = 0;
    std::string application_key;
    std::vector<sortie_observation> observations;
    scout_assessment_state assessment;
    std::vector<character_id> casualty_ids;
    int delivered_minutes = -1;
    bool provisional = false;

    void clear();
    bool is_present() const;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct scout_report_effective_state {
    bool valid = false;
    int age_minutes = -1;
    int latest_contact_minutes = -1;
    int contact_age_minutes = -1;
    int certainty = 0;
    bool assessment_ready = false;
    int defenders_low = 0;
    int defenders_high = 0;
    int danger_low = 0;
    int danger_high = 0;
    int bounty_estimate = 0;
    int route_danger_high = -1;
    int target_alert = 0;
    int scout_losses = 0;
    bool attack_authorization_usable = false;
};

scout_report_effective_state evaluate_scout_report_at(
    const scout_report_record &report, int current_minutes );

struct acted_report_summary {
    std::string target_id;
    tripoint_abs_omt target_omt;
    camp_report_policy policy = camp_report_policy::none;
    int source_generation = 0;
    int report_revision = 0;
    int acted_minutes = -1;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct camp_decision_record {
    int schema_version = 2;
    camp_decision_state state = camp_decision_state::idle;
    camp_report_policy report_policy = camp_report_policy::none;
    int source_report_revision = 0;
    int source_report_generation = 0;
    std::string source_report_activity_id;
    std::string source_report_application_key;
    std::string target_id;
    tripoint_abs_omt target_omt;
    std::string target_lead_id;
    int target_lead_revision = 0;
    int last_transition_minutes = -1;
    int next_eligible_minutes = -1;
    std::string transition_reason;

    void clear();
    bool has_pinned_report() const;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct active_outing_state {
    int schema_version = 5;
    outing_kind kind = outing_kind::none;
    std::string activity_id;
    std::string camp_id;
    int generation = 0;
    std::vector<character_id> member_ids;
    character_id leader_id;
    std::vector<tripoint_abs_omt> shared_route;
    int waypoint_index = 0;
    std::string target_id;
    tripoint_abs_omt target_omt;
    std::string job_type;
    std::string target_lead_id;
    int target_lead_revision = 0;
    scout_phase phase = scout_phase::assembling;
    std::vector<sortie_observation> observations;
    sortie_cargo cargo;
    std::vector<character_id> casualty_ids;
    std::vector<character_id> resolved_member_ids;
    int started_minutes = -1;
    int local_contact_minutes = -1;
    int last_progress_minutes = -1;
    int expected_return_minutes = -1;
    int missing_deadline_minutes = -1;
    simulation_owner owner = simulation_owner::abstract;
    int handoff_epoch = 0;
    int last_advanced_minutes = -1;
    std::string return_application_key;
    std::string report_application_key;
    std::string cargo_application_key;
    local_handoff_snapshot local_handoff;
    abstract_encounter_state abstract_encounter;
    int abstract_detour_attempts = 0;
    bool has_withdrawal_detour = false;
    tripoint_abs_omt withdrawal_detour_omt;
    std::vector<tripoint_abs_omt> target_footprint;
    structural_watch_kind selected_watch_kind = structural_watch_kind::none;
    tripoint_abs_omt selected_watch_omt;
    int selected_watch_route_cost = -1;
    structural_watch_kind alternate_watch_kind = structural_watch_kind::none;
    tripoint_abs_omt alternate_watch_omt;
    int alternate_watch_route_cost = -1;
    std::vector<tripoint_abs_omt> alternate_watch_shared_route;
    bool alternate_watch_attempted = false;
    bool alternate_watch_reposition_pending = false;
    int covert_egress_chain_version = 0;
    int covert_egress_attempts = 0;
    int covert_egress_revision = 0;
    std::vector<tripoint_abs_omt> failed_covert_egress_omts;
    std::vector<tripoint_abs_omt> current_covert_egress_route_omts;
    std::vector<tripoint_abs_omt> failed_covert_egress_route_omts;
    scout_assessment_state assessment;

    void clear();
    bool is_active() const;
    bool member_is_resolved( character_id npc_id ) const;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct hostile_operation_state {
    int schema_version = 1;
    hostile_operation_kind operation_kind = hostile_operation_kind::none;
    hostile_operation_phase phase = hostile_operation_phase::assembling;
    active_outing_state reservation;
    int source_report_revision = 0;
    int source_report_generation = 0;
    std::string source_report_activity_id;
    std::string source_report_application_key;
    bool has_rally = false;
    tripoint_abs_omt rally_omt;
    std::string last_transition_reason;
    bool legacy_unpinned = false;

    void clear();
    bool is_active() const;
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct hostile_operation_plan {
    bool valid = false;
    hostile_operation_state operation;
    std::vector<std::string> notes;
};

struct scout_resolution_effect {
    bool valid = false;
    bool changed = false;
    bool completed = false;
    int newly_resolved = 0;
    int newly_returned = 0;
    bool provisional_report_applied = false;
    bool cargo_credited = false;
};

struct finite_resource_record {
    int remaining_units = 0;
    int revision = 0;
};

enum class finite_resource_claim_status {
    rejected,
    stale,
    already_applied,
    depleted,
    applied,
};

struct finite_resource_claim_result {
    finite_resource_claim_status status = finite_resource_claim_status::rejected;
    int claimed_units = 0;
    int remaining_units = 0;
    int revision = 0;
    std::string application_key;
};

struct roster_view {
    bool valid = false;
    int living_total = 0;
    int materialized_living_total = 0;
    int unmaterialized_home_total = 0;
    std::vector<character_id> physically_present_ids;
    int physically_present_total = 0;
    std::vector<character_id> away_ids;
    std::vector<character_id> orphaned_ids;
    std::vector<character_id> reserved_unresolved_ids;
    std::vector<character_id> ready_concrete_ids;
    int ready_concrete_total = 0;
    int ready_total = 0;
};

struct routine_scout_policy_result {
    bool applies = false;
    bool eligible = false;
    int party_size = 0;
    int required_local_reserve = 0;
    int concrete_ready_goal = 0;
    std::string rejection_reason;
};

struct routine_member_readiness_snapshot {
    bool present = true;
    bool dead = false;
    int hp_percent = 100;
    bool sleeping = false;
    bool incapacitated = false;
};

struct routine_scout_pair_selection_result {
    bool eligible = false;
    character_id observer_id;
    character_id escort_id;
    int observer_capability = 0;
    int escort_capability = 0;
    bool return_safe_escort = false;
    std::vector<character_id> member_ids;
    std::string rejection_reason;
};

struct response_party_policy_result {
    bool applies = false;
    bool eligible = false;
    int party_size = 0;
    int required_local_reserve = 0;
    std::string rejection_reason;
};

struct response_party_selection_result {
    bool eligible = false;
    bool threat_derived = false;
    bandit_dry_run::job_template job = bandit_dry_run::job_template::hold_chill;
    int party_size = 0;
    int required_local_reserve = 0;
    std::vector<character_id> member_ids;
    std::string rejection_reason;
};

struct site_record {
    int schema_version = 12;
    std::string site_id;
    anchor_source_kind source_kind = anchor_source_kind::none;
    owned_site_kind site_kind = owned_site_kind::none;
    hostile_site_profile profile = hostile_site_profile::none;
    std::string source_id;
    tripoint_abs_omt anchor;
    int living_total = 0;
    int supply_units = 0;
    int supply_last_update_minutes = -1;
    int supply_accounted_living_total = 0;
    int supply_member_minute_remainder = 0;
    int routine_activated_minutes = -1;
    int last_routine_resolved_minutes = -1;
    int next_routine_dispatch_eligible_minutes = -1;
    int routine_no_candidate_streak = 0;
    std::vector<tripoint_abs_omt> footprint;
    std::vector<member_record> members;
    std::vector<spawn_tile_record> spawn_tiles;
    int next_outing_generation = 1;
    int applied_return_generation = 0;
    int applied_report_generation = 0;
    int applied_cargo_generation = 0;
    std::string last_cargo_application_key;
    int applied_resource_generation = 0;
    std::string last_resource_application_key;
    int last_resource_claimed_units = 0;
    scout_report_record current_scout_report;
    camp_decision_record camp_decision;
    std::vector<acted_report_summary> acted_reports;
    sortie_cargo returned_cargo_stock;
    active_outing_state active_outing;
    hostile_operation_state active_hostile_operation;
    std::string remembered_target_or_mark;
    int remembered_threat_estimate = 0;
    int remembered_bounty_estimate = 0;
    int remembered_retreat_bias = 0;
    int remembered_return_clock = 0;
    bandit_pursuit_handoff::remaining_return_pressure_state remembered_pressure =
        bandit_pursuit_handoff::remaining_return_pressure_state::ample;
    std::vector<std::string> known_recent_marks;
    camp_intelligence_map intelligence_map;
    std::string last_shakedown_outcome;
    int shakedown_last_demanded_value = 0;
    int shakedown_last_surrendered_value = 0;
    int shakedown_last_reachable_value = 0;
    int shakedown_loot_value = 0;
    int shakedown_defender_losses = 0;
    int shakedown_bandit_losses = 0;
    int shakedown_anger = 0;
    int shakedown_caution = 0;
    int shakedown_basecamp_defenders_at_fight = 0;
    bool shakedown_basecamp_defender_observation_pending = false;
    bool shakedown_reopen_available = false;
    bool shakedown_reopen_used = false;
    origin_disposition origin = origin_disposition::active_hostile;
    int origin_changed_minutes = -1;
    std::string origin_summary;
    bool retired_empty_site = false;
    std::string retirement_summary;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );

    bool has_member( character_id npc_id ) const;
    member_record *find_member( character_id npc_id );
    const member_record *find_member( character_id npc_id ) const;
    spawn_tile_record *find_spawn_tile( const tripoint_abs_ms &tile );
    const spawn_tile_record *find_spawn_tile( const tripoint_abs_ms &tile ) const;
    int count_members_in_state( member_state state ) const;
    int count_live_members() const;
    roster_view roster() const;
    int active_outing_survivor_count() const;
    int count_home_side_signals() const;
    int dispatchable_member_capacity() const;
    bool has_active_outside_pressure() const;
    bool eligible_for_empty_site_retirement() const;
    active_outing_state *active_external_outing();
    const active_outing_state *active_external_outing() const;
};

struct world_state {
    int schema_version = 6;
    std::string owner_id = "hells_raiders_live_owner_v0";
    int routine_scheduler_cursor = 0;
    int routine_terrain_scan_cursor = 0;
    int routine_scheduler_last_hour = -1;
    std::vector<site_record> sites;
    std::map<tripoint_abs_omt, finite_resource_record> finite_resources;

    void clear();
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );

    site_record *find_site( const std::string &site_id );
    const site_record *find_site( const std::string &site_id ) const;
    const finite_resource_record *find_finite_resource( const tripoint_abs_omt &omt ) const;
};

struct footprint_snapshot {
    tripoint_abs_omt anchor;
    std::vector<tripoint_abs_omt> footprint;
};

struct dispatch_plan {
    bool valid = false;
    std::string site_id;
    hostile_site_profile profile = hostile_site_profile::none;
    std::string target_id;
    tripoint_abs_omt target_omt;
    std::string target_lead_id;
    int target_lead_revision = 0;
    std::vector<character_id> member_ids;
    bandit_dry_run::evaluation_result evaluation;
    bandit_pursuit_handoff::abstract_group_state group;
    bandit_pursuit_handoff::entry_payload entry;
    std::vector<std::string> notes;
};

struct camp_map_dispatch_pressure {
    int stockpile_pressure = 0;
    bool opening_available = true;
    std::string opening_state = "opening_present";
};

struct camp_map_dispatch_decision {
    bool valid = false;
    bandit_dry_run::job_template intent = bandit_dry_run::job_template::hold_chill;
    int selected_member_count = 0;
    int living_roster = 0;
    int ready_at_home = 0;
    int wounded_or_unready = 0;
    int active_outside = 0;
    int hard_home_reserve = 0;
    int dispatchable = 0;
    int reward_score = 0;
    int risk_score = 0;
    int margin = 0;
    std::vector<std::string> notes;
};

struct structural_bounty_read {
    std::string terrain_class;
    std::string terrain_fit_class;
    int bounty = 0;
    int confidence = 0;
    int latent_threat = 0;
    int radius_omt = 0;
    bool eligible = false;
    std::string summary;
};

struct structural_bounty_scan_result {
    int scan_budget = 0;
    int budget_used = 0;
    bool budget_exhausted = false;
    int sites_considered = 0;
    int sites_skipped_not_camp = 0;
    int sites_skipped_retired = 0;
    int sites_skipped_no_ready_home = 0;
    int sites_skipped_active_outside = 0;
    int sites_deferred_by_cadence = 0;
    int candidates_sampled = 0;
    int leads_seeded = 0;
    int leads_suppressed_by_memory = 0;
    std::vector<std::string> notes;
};

struct watch_selection_candidate {
    tripoint_abs_omt omt;
    bool reachable = false;
    bool concealed = false;
    bool two_intervening_omts_clear = false;
    int route_cost = -1;

    bool operator==( const watch_selection_candidate &rhs ) const {
        return omt == rhs.omt && reachable == rhs.reachable && concealed == rhs.concealed &&
               two_intervening_omts_clear == rhs.two_intervening_omts_clear &&
               route_cost == rhs.route_cost;
    }
    bool operator!=( const watch_selection_candidate &rhs ) const {
        return !( *this == rhs );
    }
};

enum class watch_selection_outcome {
    selected_exact,
    selected_fallback,
    abandoned_empty_target_footprint,
    abandoned_no_safe_candidate,
};

enum class structural_watch_route_apply_result {
    rejected,
    unchanged,
    applied,
};

struct watch_selection_result {
    bool valid = false;
    tripoint_abs_omt omt;
    int footprint_distance = -1;
    int route_cost = -1;
    watch_selection_outcome outcome = watch_selection_outcome::abandoned_no_safe_candidate;
};

struct structural_watch_terrain_read {
    bool concealed = false;
    bool intervening_omts_clear = false;
};

struct structural_watch_route_read {
    bool reachable = false;
    int route_cost = -1;
};

struct structural_watch_geography_read {
    bool valid_input = false;
    bool candidate_enumeration_truncated = false;
    int candidate_omts_considered = 0;
    int terrain_reads = 0;
    int route_reads = 0;
    std::vector<tripoint_abs_omt> target_footprint;
    std::vector<watch_selection_candidate> routed_candidates;
    watch_selection_result selection;
};

struct structural_outing_plan {
    bool valid = false;
    std::string site_id;
    std::string activity_id;
    int generation = 0;
    std::string lead_id;
    int lead_revision = 0;
    tripoint_abs_omt target_omt;
    bandit_dry_run::job_template job = bandit_dry_run::job_template::hold_chill;
    std::vector<character_id> member_ids;
    std::vector<tripoint_abs_omt> shared_route;
    int frontier_sector = -1;
    int frontier_prior_resolved_minutes = -1;
    int frontier_cursor = 0;
    int effective_interest = 0;
    int known_threat = 0;
    int terrain_fit = 0;
    int static_risk = 0;
    int estimate_freshness = 0;
    int repetition_penalty = 0;
    int cheap_route_quality = 0;
    int final_route_quality = 0;
    int full_route_cost = -1;
    int max_route_segment_risk = 0;
    int cheap_score = 0;
    int final_score = 0;
    bool route_solved = false;
    bool watch_geography_supplied = false;
    std::vector<tripoint_abs_omt> target_footprint;
    std::vector<watch_selection_candidate> watch_candidates;
    std::vector<tripoint_abs_omt> alternate_watch_shared_route;
    int expected_stalking_minutes = -1;
    int expected_arrival_minutes = -1;
    int expected_return_minutes = -1;
    std::vector<std::string> notes;
};

struct structural_route_read {
    bool reachable = false;
    int complete_route_cost = -1;
    int max_segment_risk = 0;
    std::string summary;
    bool watch_geography_supplied = false;
    std::vector<tripoint_abs_omt> target_footprint;
    std::vector<watch_selection_candidate> watch_candidates;
    std::vector<tripoint_abs_omt> watch_shared_route;
    std::vector<tripoint_abs_omt> alternate_watch_shared_route;

    structural_route_read() = default;
    structural_route_read( bool reachable_, int complete_route_cost_, int max_segment_risk_,
                           std::string summary_, bool watch_geography_supplied_ = false,
                           std::vector<tripoint_abs_omt> target_footprint_ = {},
                           std::vector<watch_selection_candidate> watch_candidates_ = {},
                           std::vector<tripoint_abs_omt> watch_shared_route_ = {},
                           std::vector<tripoint_abs_omt> alternate_watch_shared_route_ = {} ) :
        reachable( reachable_ ), complete_route_cost( complete_route_cost_ ),
        max_segment_risk( max_segment_risk_ ), summary( std::move( summary_ ) ),
        watch_geography_supplied( watch_geography_supplied_ ),
        target_footprint( std::move( target_footprint_ ) ),
        watch_candidates( std::move( watch_candidates_ ) ),
        watch_shared_route( std::move( watch_shared_route_ ) ),
        alternate_watch_shared_route( std::move( alternate_watch_shared_route_ ) ) {}
};

struct routine_dispatch_evaluation {
    int need = 0;
    int knowledge_gap = 0;
    int best_cheap_target = 0;
    int cadence = 0;
    int drive = 0;
    bool force_due = false;
};

struct structural_threat_read {
    int threat = 0;
    bool observed = false;
    std::string summary;
};

struct abstract_threat_detour_read {
    tripoint_abs_omt omt;
    bool passable = false;
};

struct structural_threat_observer_request {
    tripoint_abs_omt current_omt;
    int observation_window_start_minutes = -1;
    std::vector<tripoint_abs_omt> visible_forward_omts;
    std::optional<tripoint_abs_omt> retained_threat_omt;
    std::vector<std::string> retained_threat_ids;
    int retained_threat_age_minutes = -1;
    int party_power = 0;
};

struct structural_observer_visibility_read {
    int ordinary_sight_range_ms = 0;
    float weather_sight_penalty = 1.0f;
    int elevation_omt = 0;
    bool has_optic = false;
};

int structural_observer_omt_sight_range( const structural_observer_visibility_read &read );
bool structural_observer_route_is_visible( int sight_points,
        const std::vector<int> &terrain_see_costs );
int structural_observer_last_known_max_age_minutes();
bool structural_observer_route_is_retained( int sight_points,
        const std::vector<int> &terrain_see_costs, int last_known_age_minutes );
bool structural_observer_retained_threat_matches(
    const structural_threat_observer_request &request, const tripoint_abs_omt &threat_omt,
    const std::vector<std::string> &stable_threat_ids );

struct abstract_threat_read {
    bool observed = false;
    bool overlap = false;
    bool local_reality = false;
    tripoint_abs_omt threat_omt;
    int danger_low = 0;
    int danger_high = 0;
    int visual_quality = 1;
    int uncertainty_radius_omt = 1;
    int equipment_detail = 0;
    std::vector<std::string> stable_threat_ids;
    std::vector<abstract_threat_detour_read> detours;
    std::string summary;
};

struct structural_local_zombie_read {
    character_id observer_id;
    tripoint_abs_omt source_omt;
    int inspected_monsters = 0;
    int visible_count = 0;
    int danger_low = 0;
    int danger_high = 0;
    int visual_quality = 1;
    std::vector<std::string> stable_threat_ids;
};

std::optional<structural_local_zombie_read> read_live_structural_local_zombie_observation(
    const site_record &site );
int burn_live_covert_scouts();
int record_live_covert_visible_defenders();
int record_live_covert_vehicle_wealth_cues();
int record_live_covert_generation_infrastructure_cues();
int record_live_covert_cargo_handling_cues();
bool fail_live_covert_scout_burned_egress( character_id member_id );
bool structural_local_zombie_candidate_is_eligible( bool alive, bool hallucination,
        bool zombie_species, bool zombie_rider, bool hostile, bool visible,
        bool source_on_route );
sortie_observation_effect record_structural_local_zombie_observation(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const structural_local_zombie_read &read, int now_minutes );

enum class structural_sound_kind {
    none,
    gunfire,
    alarm,
    explosion,
};

struct structural_signal_read {
    sortie_observation_sense sense = sortie_observation_sense::smoke;
    structural_sound_kind sound_kind = structural_sound_kind::none;
    tripoint_abs_omt source_omt;
    int emitted_minutes = -1;
    int range_cap_omt = 0;
    int strength = 0;
    int confidence = 0;
    int uncertainty_radius_omt = 1;
    bool local_reality = false;
    std::string summary;
};

struct structural_signal_record_result {
    int sites_considered = 0;
    int active_outings_considered = 0;
    int callbacks_invoked = 0;
    int sites_recorded = 0;
    int facts_recorded = 0;
};

enum class abstract_threat_resolution_kind {
    none,
    deferred_to_local,
    observed_below_gate,
    withdrawal,
    wounded_pair,
    one_missing,
    all_missing,
};

struct abstract_threat_resolution {
    bool valid = false;
    bool changed = false;
    bool encounter_started = false;
    bool encounter_cleared = false;
    bool outcome_applied = false;
    abstract_threat_resolution_kind kind = abstract_threat_resolution_kind::none;
    int detour_attempts = 0;
    std::vector<std::string> notes;
};

struct structural_outing_result {
    int sites_considered = 0;
    int active_outings_considered = 0;
    int stalking_checks_processed = 0;
    int lost_interest_returns = 0;
    int arrivals_processed = 0;
    int members_returned = 0;
    std::vector<std::string> notes;
};

struct camp_intelligence_aging_result {
    int sites_considered = 0;
    int sites_cleaned = 0;
    int leads_considered = 0;
    int leads_aged = 0;
    int leads_pruned = 0;
};

struct structural_bounty_maintenance_result {
    camp_intelligence_aging_result intelligence_aging;
    structural_outing_result outing;
    structural_bounty_scan_result scan;
    int scheduler_hour = -1;
    int scheduler_cursor_before = 0;
    int scheduler_cursor_after = 0;
    int terrain_scan_cursor_before = 0;
    int terrain_scan_cursor_after = 0;
    int terrain_scan_sites_selected = 0;
    int scheduler_consider_cap = 16;
    int full_route_solve_cap = 8;
    int full_route_solves = 0;
    bool scheduler_replay_suppressed = false;
    int sites_considered_for_dispatch = 0;
    int dispatches_planned = 0;
    int dispatches_applied = 0;
    int dispatches_blocked = 0;
    int materialization_attempts = 0;
    int members_materialized = 0;
    int dispatch_cap = 0;
    bool dispatch_cap_reached = false;
    std::vector<std::string> notes;
};

struct local_gate_input {
    int local_threat = 0;
    int local_opportunity = 0;
    int standoff_distance = 0;
    bool darkness_or_concealment = false;
    bool basecamp_or_camp_scene = false;
    bool rolling_travel_scene = false;
    bool current_exposure = false;
    bool recent_exposure = false;
    bool smoke_obscured_lead = false;
    bool smoke_on_watcher_tile = false;
    bool smoke_between_watcher_and_camp = false;
    bool local_contact_established = false;
};

struct local_gate_decision {
    bool valid = false;
    local_gate_posture posture = local_gate_posture::abort;
    int dispatch_strength = 0;
    int pressure_margin = 0;
    bool shakedown_capable = false;
    bool opens_shakedown_surface = false;
    bool combat_forward = false;
    std::vector<std::string> notes;
};

struct sight_avoid_candidate {
    tripoint_abs_ms tile;
    bool passable = true;
    bool visible_to_player = false;
    bool visible_to_camp = false;
    int cover_score = 0;
    bool smoke_obscured = false;
};

struct sight_avoid_decision {
    bool valid = false;
    bool repositions = false;
    tripoint_abs_ms destination;
    std::string reason;
    std::vector<std::string> notes;
};

struct shakedown_goods_pool {
    int player_carried_value = 0;
    int companion_carried_value = 0;
    int vehicle_carried_value = 0;
    int reachable_basecamp_value = 0;
    bool basecamp_or_camp_scene = false;
};

struct shakedown_surface {
    bool valid = false;
    std::string opening_id;
    std::string opening_summary;
    std::string bark;
    int reachable_goods_value = 0;
    int demanded_value = 0;
    bool pay_available = false;
    bool fight_available = false;
    bool includes_basecamp_inventory = false;
    bool includes_vehicle_inventory = false;
    std::vector<std::string> notes;
};

struct shakedown_outcome {
    bool paid = false;
    bool fought = false;
    bool basecamp_or_camp_scene = false;
    bool extraction_failed = false;
    int demanded_value = 0;
    int surrendered_value = 0;
    int reachable_goods_value = 0;
    int defender_losses = 0;
    int bandit_losses = 0;
};

struct shakedown_aftermath_effect {
    bool valid = false;
    bool stronger_reopen = false;
    bool cools_later_pressure = false;
    int demand_modifier_percent = 100;
    std::vector<std::string> notes;
};

struct live_signal_mark {
    std::string mark_id;
    std::string kind;
    tripoint_abs_omt source_omt;
    int observed_range_omt = 0;
    int range_cap_omt = 0;
    int strength = 0;
    int confidence = 0;
    int bounty_add = 0;
    int threat_add = 0;
    std::vector<std::string> notes;
};

struct abstract_bootstrap_result {
    int created_sites = 0;
    int recognized_tiles = 0;
};

bool is_tracked_hostile_template( const std::string &npc_template_id );
std::optional<owned_site_kind> classify_tracked_source( anchor_source_kind source_kind,
        const std::string &source_id );
hostile_site_profile profile_for_site_kind( owned_site_kind site_kind );
footprint_snapshot make_special_footprint( const std::string &special_id,
        const tripoint_abs_omt &origin,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup );
std::string make_site_id( anchor_source_kind source_kind, const std::string &source_id,
                          const tripoint_abs_omt &anchor );
int abstract_roster_seed_for_site_kind( owned_site_kind site_kind );
bool register_abstract_site( world_state &state, anchor_source_kind source_kind,
                             const std::string &source_id, const tripoint_abs_omt &origin,
                             const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup,
                             int abstract_headcount = 0 );
abstract_bootstrap_result register_abstract_sites_near(
    world_state &state, const tripoint_abs_omt &center, int radius_omt,
    const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup );
bool claim_tracked_spawn( world_state &state, const std::string &npc_template_id,
                          character_id npc_id, const tripoint_abs_ms &spawn_tile,
                          const std::optional<std::string> &overmap_special_id,
                          const std::optional<std::string> &map_extra_id,
                          const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup );
finite_resource_claim_result claim_finite_resource_units( world_state &state,
        const std::string &claimant_site_id, const tripoint_abs_omt &omt,
        const finite_resource_record &expected,
        int requested_units, const std::string &operation_id, int operation_generation,
        const std::string &application_key );
finite_resource_record finite_resource_snapshot( const world_state &state,
        const tripoint_abs_omt &omt, int undiscovered_units );
std::string finite_resource_claim_application_key( const std::string &operation_id,
        int operation_generation, const tripoint_abs_omt &omt );
int camp_supply_living_total( const site_record &site );
int camp_supply_cap( const site_record &site );
bool advance_camp_supply( site_record &site, int now_minutes );
int advance_world_camp_supplies( world_state &state, int now_minutes );
routine_scout_policy_result routine_scout_policy( const site_record &site );
int routine_scout_materialization_count( const site_record &site );
bool routine_member_is_unready( const routine_member_readiness_snapshot &snapshot );
bool member_has_abstract_wound_recovery( const member_record &member, int now_minutes );
routine_scout_pair_selection_result select_routine_scout_pair( const site_record &site );
response_party_policy_result response_party_policy( const site_record &site,
        bandit_dry_run::job_template job, int requested_party_size = 0 );
response_party_selection_result select_fresh_response_party( const site_record &site,
        hostile_operation_kind operation_kind );
dispatch_plan plan_site_dispatch( const site_record &site, const tripoint_abs_omt &target_omt,
                                  const std::string &target_id );
dispatch_plan plan_site_dispatch_from_camp_map_lead( const site_record &site,
        const camp_map_lead &lead,
        const camp_map_dispatch_pressure &pressure = camp_map_dispatch_pressure() );
tripoint_abs_omt reachable_ground_dispatch_target( const site_record &site,
        const tripoint_abs_omt &target_omt );
camp_map_dispatch_decision choose_camp_map_dispatch( const site_record &site,
        const camp_map_lead &lead,
        const camp_map_dispatch_pressure &pressure = camp_map_dispatch_pressure() );
const camp_map_lead *find_camp_map_dispatch_lead_for_target( const site_record &site,
        const tripoint_abs_omt &target_omt,
        const std::string &target_id );
void normalize_camp_intelligence( site_record &site );
camp_intelligence_aging_result advance_camp_intelligence_aging( site_record &site,
        int now_minutes );
camp_intelligence_aging_result advance_camp_intelligence_aging( world_state &state,
        int now_minutes );
structural_bounty_read classify_structural_bounty_terrain( const std::string &overmap_terrain_id );
int hostile_camp_terrain_fit( hostile_site_profile profile,
                              const std::string &terrain_fit_class );
int structural_terrain_static_risk( const std::string &terrain_fit_class );
int normalize_ground_bounty_opportunity( int bounty_units );
int hostile_camp_dispatch_drive( int need, int knowledge_gap, int best_cheap_target,
                                 int cadence );
bool hostile_camp_routine_score_eligible( int score, bool retained_target );
bool hostile_camp_routine_risk_blocked( int risk );
bool hostile_camp_routine_route_risk_eligible( int risk, int max_segment_risk );
routine_dispatch_evaluation evaluate_hostile_camp_routine_dispatch(
    const site_record &site, int now_minutes, int best_cheap_target );
std::string make_structural_bounty_lead_id( const std::string &site_id,
        const tripoint_abs_omt &omt, const std::string &terrain_class );
bool structural_bounty_memory_suppresses_refresh( const camp_intelligence_map &intelligence_map,
        const tripoint_abs_omt &omt, const std::string &terrain_class );
bool upsert_structural_bounty_lead( site_record &site, const tripoint_abs_omt &omt,
                                   const structural_bounty_read &read, int now_minutes );
bool record_camp_resource_estimate( site_record &site, const std::string &lead_id,
                                    int estimated_units, int confidence, int observed_minutes );
structural_bounty_scan_result advance_structural_bounty_scan( world_state &state,
        int now_minutes, int scan_budget,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &terrain_lookup );
structural_outing_plan plan_structural_bounty_outing( const site_record &site,
        const camp_map_lead &lead, int now_minutes );
structural_outing_plan plan_structural_bounty_outing( const site_record &site, int now_minutes );
structural_outing_plan plan_frontier_outing( const site_record &site, int now_minutes );
bool apply_structural_bounty_outing_plan( site_record &site, const structural_outing_plan &plan,
        int now_minutes );
std::optional<int> release_matching_external_reservation( site_record &site,
        const std::string &expected_activity_id, int expected_generation,
        const std::string &summary );
bool invalidate_site_origin( site_record &site, origin_disposition disposition,
                             int current_minutes, const std::string &summary );
bool request_origin_recall( site_record &site,
                            const simulation_advance_cursor &expected_cursor,
                            bool physical_signal, int current_minutes,
                            const std::string &summary );
origin_loss_resolution_effect resolve_origin_loss_return( site_record &site,
        const std::string &expected_activity_id, int expected_generation,
        const std::vector<active_member_observation> &observations,
        int current_minutes, const std::string &summary );
std::optional<int> release_structural_outing_reservation( site_record &site,
        const std::string &expected_activity_id, int expected_generation,
        const std::string &summary );
int structural_outing_party_power( const site_record &site );
abstract_threat_resolution resolve_structural_abstract_threat( site_record &site,
        const tripoint_abs_omt &current_omt, const abstract_threat_read &read,
        int now_minutes );
structural_outing_result advance_structural_bounty_outings( world_state &state, int now_minutes,
        const std::function<structural_threat_read( const site_record &, const camp_map_lead & )> &threat_lookup,
        const std::function<abstract_threat_read( const site_record &, const active_outing_state &,
                const structural_threat_observer_request & )> &abstract_threat_lookup = {},
        const std::function<std::vector<structural_signal_read>( const site_record &,
                const active_outing_state &,
                const structural_threat_observer_request & )> &signal_lookup = {} );
structural_signal_record_result record_structural_signal_observations( world_state &state,
        int now_minutes,
        const std::function<std::vector<structural_signal_read>( const site_record &,
                const active_outing_state &,
                const structural_threat_observer_request & )> &signal_lookup );
structural_bounty_maintenance_result advance_structural_bounty_maintenance( world_state &state,
        int now_minutes, int scan_budget, int dispatch_cap,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &terrain_lookup,
        const std::function<structural_threat_read( const site_record &, const camp_map_lead & )> &threat_lookup,
        const std::function<structural_route_read( const site_record &,
                const structural_outing_plan & )> &route_lookup = {},
        const std::function<abstract_threat_read( const site_record &, const active_outing_state &,
                const structural_threat_observer_request & )> &abstract_threat_lookup = {},
        const std::function<std::vector<structural_signal_read>( const site_record &,
                const active_outing_state &,
                const structural_threat_observer_request & )> &signal_lookup = {},
        const std::function<int( world_state &, std::size_t )> &materialize_for_dispatch = {} );
std::string render_structural_bounty_maintenance_report(
    const structural_bounty_maintenance_result &result );
std::string render_evidence_debug_report( const world_state &state, int current_minutes );
bool apply_dispatch_plan( site_record &site, const dispatch_plan &plan );
local_gate_decision choose_local_gate_posture( const site_record &site,
        const local_gate_input &input );
std::optional<int> target_footprint_watch_distance(
    const tripoint_abs_omt &observer_omt,
    const std::vector<tripoint_abs_omt> &target_footprint );
std::optional<tripoint_abs_omt> nearest_target_footprint_omt(
    const tripoint_abs_omt &observer_omt,
    const std::vector<tripoint_abs_omt> &target_footprint );
watch_selection_result select_exact_watch_ring_candidate(
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<watch_selection_candidate> &candidates );
watch_selection_result select_watch_ring_candidate(
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<watch_selection_candidate> &candidates );
watch_selection_result select_alternate_watch_ring_candidate(
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<watch_selection_candidate> &candidates,
    const tripoint_abs_omt &selected_watch_omt );
bool structural_watch_route_avoids_target_footprint(
    const std::vector<tripoint_abs_omt> &route,
    const std::vector<tripoint_abs_omt> &target_footprint );
std::vector<tripoint_abs_omt> make_structural_watch_shared_route(
    const tripoint_abs_omt &anchor, const tripoint_abs_omt &watch_omt,
    const std::vector<tripoint_abs_omt> &reverse_path,
    const std::vector<tripoint_abs_omt> &target_footprint );
bool structural_watch_shared_route_is_canonical(
    const std::vector<tripoint_abs_omt> &route,
    const tripoint_abs_omt &anchor, const tripoint_abs_omt &watch_omt,
    const std::vector<tripoint_abs_omt> &target_footprint );
structural_watch_geography_read read_structural_watch_geography(
    const std::vector<tripoint_abs_omt> &target_footprint,
    const tripoint_abs_omt &route_origin,
    const std::function<structural_watch_terrain_read( const tripoint_abs_omt &,
            const std::vector<tripoint_abs_omt> & )> &terrain_lookup,
    const std::function<structural_watch_route_read( const tripoint_abs_omt & )> &route_lookup );
structural_watch_route_apply_result apply_structural_watch_route_selection(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<watch_selection_candidate> &candidates,
    const std::vector<tripoint_abs_omt> &alternate_watch_shared_route = {} );
int ordinary_scout_watch_standoff_omt();
int minimum_hold_off_standoff_omt();
tripoint_abs_omt choose_hold_off_standoff_goal( const tripoint_abs_omt &site_anchor,
        const tripoint_abs_omt &player_omt, int requested_distance );
bool hot_defended_doorstep_blocks_pickup( const site_record &site,
        const local_gate_input &input, const local_gate_decision &decision,
        const character_id &member_id );
int ordinary_scout_sortie_limit_minutes();
sight_avoid_decision choose_sight_avoid_reposition( const tripoint_abs_ms &current_tile,
        bool current_exposure, bool recent_exposure,
        const std::vector<sight_avoid_candidate> &candidates, bool current_smoke_obscured = false );
std::optional<simulation_advance_cursor> current_external_simulation_cursor(
    const site_record &site );
bool note_active_sortie_started( site_record &site,
                                 const simulation_advance_cursor &expected_cursor,
                                 int current_minutes );
bool note_active_sortie_local_contact( site_record &site,
                                       const simulation_advance_cursor &expected_cursor,
                                       character_id contact_member_id, int current_minutes );
sortie_observation_effect record_active_sortie_observations( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const std::vector<sortie_observation> &observations, int current_minutes );
sortie_observation_effect record_active_typed_observations( site_record &site,
        const simulation_advance_cursor &expected_cursor, character_id observer_id,
        int expected_target_revision, const std::vector<sortie_observation> &observations,
        int current_minutes );
scout_assessment_state summarize_normal_scout_assessment(
    const active_outing_state &outing );
int scout_assessment_unknown_slots( int certainty );
scout_assessment_result advance_structural_scout_assessment(
    site_record &site, const std::string &expected_activity_id,
    int expected_generation, int expected_target_revision, int current_minutes );
bool scout_assessment_readiness_after_certainty(
    scout_assessment_threshold_class threshold_class, bool readiness_latched,
    int certainty );
simulation_owner_transition_result transition_external_simulation_owner( site_record &site,
        const std::string &expected_activity_id, int expected_generation,
        simulation_owner expected_owner, simulation_owner next_owner,
        int expected_handoff_epoch, int expected_last_advanced_minutes,
        int current_minutes );
simulation_owner_transition_result advance_external_simulation( site_record &site,
        const std::string &expected_activity_id, int expected_generation,
        simulation_owner expected_owner, int expected_handoff_epoch,
        int expected_last_advanced_minutes, int expected_covert_egress_revision,
        int current_minutes );
local_handoff_plan plan_local_pair_handoff( const site_record &site,
        const simulation_advance_cursor &expected_cursor, int current_minutes,
        const std::vector<local_handoff_member_read> &member_reads );
local_handoff_commit_result commit_local_pair_handoff( site_record &site,
        const local_handoff_plan &plan,
        const std::function<bool( const local_handoff_member_snapshot & )> &bind_member,
        const std::function<void( const local_handoff_member_snapshot & )> &rollback_member );
local_dematerialization_plan plan_local_pair_dematerialization( const site_record &site,
        const simulation_advance_cursor &expected_cursor, int current_minutes,
        const std::vector<local_dematerialization_member_read> &member_reads,
        const sortie_cargo &cargo );
local_handoff_commit_result commit_local_pair_dematerialization( site_record &site,
        const local_dematerialization_plan &plan,
        const std::function<bool( const local_handoff_member_snapshot & )> &quiesce_member,
        const std::function<void( const local_handoff_member_snapshot & )> &rollback_member );
local_handoff_commit_result start_local_pair_alternate_watch_reposition(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    int current_minutes );
local_handoff_commit_result abort_local_pair_alternate_watch_reposition(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    int current_minutes,
    std::string_view reason = "alternate watch route aborted" );
local_alternate_watch_reposition_plan plan_local_pair_alternate_watch_reposition(
    const site_record &site, const simulation_advance_cursor &expected_cursor,
    int current_minutes, const std::vector<local_alternate_watch_member_read> &member_reads );
local_handoff_commit_result commit_local_pair_alternate_watch_reposition(
    site_record &site, const local_alternate_watch_reposition_plan &plan,
    const std::function<bool( const local_handoff_member_snapshot & )> &quiesce_member,
    const std::function<void( const local_handoff_member_snapshot & )> &rollback_member );
local_handoff_commit_result commit_loaded_local_pair_alternate_watch_reposition(
    site_record &site, const local_alternate_watch_reposition_plan &plan );
bool record_local_pair_member_death( site_record &site,
                                     const simulation_advance_cursor &expected_cursor,
                                     character_id member_id,
                                     const tripoint_abs_ms &death_position,
                                     int current_minutes );
bool reconcile_local_pair_casualties( site_record &site,
                                      const simulation_advance_cursor &expected_cursor,
                                      const std::vector<local_pair_casualty_read> &reads,
                                      int current_minutes );
local_cohesion_plan plan_local_pair_cohesion( const site_record &site,
        const simulation_advance_cursor &expected_cursor, int current_minutes,
        const std::vector<local_cohesion_member_read> &member_reads );
bool commit_local_pair_cohesion( site_record &site, const local_cohesion_plan &plan,
                                 bool route_attempted, bool route_failed );
bool claim_local_pair_site_ownership( const site_record &site,
                                      std::set<character_id> &claimed_members );
std::map<character_id, tripoint_abs_ms> local_pair_assembly_orders(
    const active_outing_state &outing );
std::set<character_id> local_pair_homeward_travel_ids( const world_state &state );
std::map<character_id, tripoint_abs_omt> local_pair_alternate_watch_travel_destinations(
    const world_state &state );
bool is_valid_scout_phase_transition( scout_phase previous_phase, scout_phase next_phase );
scout_phase scout_phase_after_burned_evacuation( bool concealed_rally_reached );
bool scout_phase_requires_homeward_only( scout_phase phase );
bool active_outing_requires_homeward_routing( const active_outing_state &outing );
scout_phase_transition_result transition_active_scout_phase( site_record &site,
        const simulation_advance_cursor &expected_cursor, scout_phase expected_phase,
        scout_phase next_phase, int current_minutes,
        std::string_view reason = "explicit phase transition" );
bool is_valid_camp_decision_transition( camp_decision_state previous_state,
                                        camp_decision_state next_state );
camp_decision_transition_result accept_current_scout_report_for_assessment( site_record &site );
camp_decision_transition_result transition_camp_decision_state( site_record &site,
        camp_decision_state expected_state, camp_decision_state next_state,
        int expected_report_revision, int expected_report_generation, int current_minutes,
        int next_eligible_minutes, const std::string &reason );
bool is_valid_hostile_operation_phase_transition( hostile_operation_phase previous_phase,
        hostile_operation_phase next_phase );
hostile_operation_transition_result transition_hostile_operation_phase( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        hostile_operation_phase expected_phase, hostile_operation_phase next_phase,
        int current_minutes, const std::string &reason );
hostile_operation_plan plan_hostile_operation( const site_record &site,
        hostile_operation_kind operation_kind, const std::vector<tripoint_abs_omt> &route,
        const tripoint_abs_omt &rally_omt, int current_minutes );
bool apply_hostile_operation_plan( site_record &site, const hostile_operation_plan &plan );
bool scout_sortie_should_return_home( const site_record &site, int current_minutes,
                                      int sortie_limit_minutes );
shakedown_surface build_shakedown_surface( const site_record &site, const local_gate_input &input,
        const local_gate_decision &decision, const shakedown_goods_pool &goods_pool );
shakedown_aftermath_effect apply_shakedown_outcome( site_record &site,
        const shakedown_outcome &outcome );
void begin_shakedown_basecamp_defender_observation( site_record &site, int live_defenders );
shakedown_aftermath_effect apply_shakedown_basecamp_defender_observation( site_record &site,
        int live_defenders );
bool mark_shakedown_reopen_used( site_record &site );
bool is_active_shakedown_parley_member( const world_state &state, character_id npc_id );
struct covert_scout_relationship_read {
    scout_phase phase = scout_phase::assembling;
    std::vector<tripoint_abs_omt> target_footprint;
    tripoint_abs_omt egress_omt;
    int minimum_target_distance = -1;
    std::vector<tripoint_abs_omt> forbidden_route_omts;
};
struct covert_scout_member_acquire_read {
    character_id npc_id;
    tripoint_abs_omt position;
    bool position_known = false;
    bool returning_home = false;
    bool mutual_target_visibility = false;
    bool mutual_target_visibility_evaluated = false;
};
struct covert_scout_burn_read {
    character_id npc_id;
    tripoint_abs_omt position;
    bool present = false;
    std::string target_observer_id;
    tripoint_abs_omt target_observer_position;
    bool target_saw_scout = false;
    bool scout_saw_target = false;
    std::vector<tripoint_abs_omt> perceived_target_observer_positions;
    struct visible_defender_read {
        std::string stable_id;
        tripoint_abs_omt position;
        int normalized_power = 0;
        int equipment_detail = 0;
    };
    std::vector<visible_defender_read> visible_defenders;
};
sortie_observation_effect record_covert_visible_defender_observations(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    character_id observer_id, const tripoint_abs_omt &observer_position,
    const std::vector<covert_scout_burn_read::visible_defender_read> &visible_defenders,
    int current_minutes );
struct covert_vehicle_wealth_read {
    tripoint_abs_ms origin;
    std::vector<tripoint_abs_ms> ordinarily_visible_occupied_points;
};
int covert_vehicle_wealth_cue_cap();
sortie_observation_effect record_covert_vehicle_wealth_observations(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    character_id observer_id, const tripoint_abs_omt &observer_position,
    const std::vector<covert_vehicle_wealth_read> &vehicles, int current_minutes );
struct covert_generation_infrastructure_read {
    tripoint_abs_ms appliance_origin;
    tripoint_abs_ms generation_part_position;
};
int covert_generation_infrastructure_cue_cap();
sortie_observation_effect record_covert_generation_infrastructure_observations(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    character_id observer_id, const tripoint_abs_omt &observer_position,
    const std::vector<covert_generation_infrastructure_read> &installations,
    int current_minutes );
struct covert_cargo_handling_read {
    character_id handler_id;
    tripoint_abs_omt position;
};
int covert_cargo_handling_cue_cap();
sortie_observation_effect record_covert_cargo_handling_observations(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    character_id observer_id, const tripoint_abs_omt &observer_position,
    const std::vector<covert_cargo_handling_read> &handlers, int current_minutes );
int covert_scout_burn_observer_cap();
int covert_visible_defender_read_cap();
int covert_scout_egress_route_omt_cap();
struct covert_scout_egress_candidate {
    tripoint_abs_omt omt;
    bool reachable = false;
    bool concealed = false;
    bool hard_danger = false;
    int soft_danger = 0;
    int route_cost = -1;
    std::vector<tripoint_abs_omt> route_omts;

    covert_scout_egress_candidate() = default;
    covert_scout_egress_candidate( const tripoint_abs_omt &candidate_omt,
                                   bool candidate_reachable, bool candidate_concealed,
                                   bool candidate_hard_danger, int candidate_soft_danger,
                                   int candidate_route_cost,
                                   const std::vector<tripoint_abs_omt> &candidate_route_omts = {} ) :
        omt( candidate_omt ), reachable( candidate_reachable ), concealed( candidate_concealed ),
        hard_danger( candidate_hard_danger ), soft_danger( candidate_soft_danger ),
        route_cost( candidate_route_cost ),
        route_omts( candidate_route_omts.empty() && candidate_reachable ?
                    std::vector<tripoint_abs_omt> { candidate_omt } : candidate_route_omts ) {}
};
enum class covert_scout_burn_result {
    rejected,
    unchanged,
    applied,
};
struct covert_scout_burn_effect {
    covert_scout_burn_result result = covert_scout_burn_result::rejected;
    character_id observer_id;
    std::string target_observer_id;
    tripoint_abs_omt burn_origin_omt;
    tripoint_abs_omt egress_omt;
    tripoint_abs_omt rally_omt;
};
enum class local_structural_watch_exit_kind {
    none,
    hard_danger_return,
    hard_danger_unreachable,
};
struct local_structural_watch_exit_plan {
    bool applicable = false;
    bool valid = false;
    local_structural_watch_exit_kind kind = local_structural_watch_exit_kind::none;
    simulation_advance_cursor expected_cursor;
    std::string expected_site_id;
    int expected_target_revision = 0;
    std::vector<character_id> expected_member_ids;
    tripoint_abs_omt expected_watch_omt;
    active_outing_state next_outing;
    std::vector<active_member_observation> unreachable_observations;
    int committed_minutes = -1;
};
enum class covert_scout_egress_failure_result {
    rejected,
    retried,
    exhausted,
};
struct covert_scout_egress_failure_effect {
    covert_scout_egress_failure_result result = covert_scout_egress_failure_result::rejected;
    tripoint_abs_omt failed_egress_omt;
    tripoint_abs_omt egress_omt;
};
std::optional<covert_scout_relationship_read> read_active_covert_scout_member(
    const world_state &state, character_id npc_id );
std::optional<covert_scout_relationship_read> read_active_covert_scout_homeward_member(
    const world_state &state, character_id npc_id );
bool is_active_covert_scout_member( const world_state &state, character_id npc_id );
std::optional<covert_scout_egress_candidate> select_covert_scout_egress(
    const tripoint_abs_omt &burn_origin,
    const std::vector<tripoint_abs_omt> &target_footprint,
    const std::vector<covert_scout_egress_candidate> &candidates,
    const std::optional<tripoint_abs_omt> &route_floor_origin = std::nullopt );
bool covert_scout_egress_route_respects_retry_memory(
    const active_outing_state &outing, const tripoint_abs_omt &member_start,
    const std::vector<tripoint_abs_omt> &route, bool current_route_failed );
covert_scout_burn_effect apply_covert_scout_burn(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<covert_scout_burn_read> &member_reads,
    const std::vector<covert_scout_egress_candidate> &egress_candidates,
    int current_minutes,
    const std::optional<structural_local_zombie_read> &danger_read = std::nullopt );
local_structural_watch_exit_plan plan_local_structural_watch_exit(
    const site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<covert_scout_burn_read> &member_reads,
    const std::vector<covert_scout_egress_candidate> &egress_candidates,
    const structural_local_zombie_read &danger_read, int current_minutes,
    bool home_routes_ready,
    const std::vector<active_member_observation> &unreachable_observations = {} );
local_handoff_commit_result commit_local_structural_watch_exit(
    site_record &site, const local_structural_watch_exit_plan &plan,
    const std::function<bool( character_id )> &prepare_member,
    const std::function<void( character_id )> &rollback_member );
bool complete_covert_scout_burned_egress(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<covert_scout_member_acquire_read> &member_reads,
    int current_minutes );
covert_scout_egress_failure_effect resolve_covert_scout_burned_egress_failure(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<covert_scout_egress_candidate> &egress_candidates,
    int current_minutes );
bool abandon_covert_scout_unreachable_return(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<active_member_observation> &observations,
    int current_minutes );
bool covert_scout_party_cleared_target_acquire_range(
    const active_outing_state &outing,
    const std::vector<covert_scout_member_acquire_read> &member_reads );
bool release_covert_cohesion_abort_after_target_clear(
    site_record &site, const simulation_advance_cursor &expected_cursor,
    const std::vector<covert_scout_member_acquire_read> &member_reads );
std::string render_empty_site_retirement_report( const site_record &site );
int retire_empty_hostile_sites( world_state &state, std::vector<std::string> *reports = nullptr );
bool apply_return_packet( site_record &site, const bandit_pursuit_handoff::return_packet &packet );
scout_resolution_effect apply_active_scout_observations( site_record &site,
        const simulation_advance_cursor &expected_cursor,
        const std::vector<active_member_observation> &observations, int current_minutes );
std::optional<bandit_pursuit_handoff::return_packet> resolve_active_group_aftermath(
    const site_record &site, const std::vector<active_member_observation> &observations );
bool update_member_state( site_record &site, character_id npc_id, member_state new_state,
                          const std::string &summary );
bool record_matching_external_outing_casualty( site_record &site,
        const std::string &expected_activity_id, int expected_generation,
        character_id npc_id, member_state casualty_state, int current_minutes,
        const std::string &summary );
bool record_active_outing_casualty( site_record &site,
                                    const simulation_advance_cursor &expected_cursor,
                                    character_id npc_id,
                                    member_state casualty_state, int current_minutes,
                                    const std::string &summary );
bool upsert_camp_map_lead( site_record &site, camp_map_lead lead );

std::string to_string( anchor_source_kind source_kind );
std::string to_string( owned_site_kind site_kind );
std::string to_string( hostile_site_profile profile );
std::string to_string( member_state state );
std::string to_string( origin_disposition disposition );
std::string to_string( active_member_observation_state state );
std::string to_string( local_gate_posture posture );
std::string to_string( camp_lead_kind kind );
std::string to_string( camp_lead_origin origin );
std::string to_string( camp_lead_status status );
std::string to_string( outing_kind kind );
std::string to_string( simulation_owner owner );
std::string to_string( scout_phase phase );
std::string to_string( sortie_observation_kind kind );
std::string to_string( camp_decision_state state );
std::string to_string( camp_report_policy policy );
std::string to_string( hostile_operation_kind kind );
std::string to_string( hostile_operation_phase phase );
std::string render_local_gate_report( const site_record &site, const local_gate_input &input,
                                      const local_gate_decision &decision );
std::string render_shakedown_surface_report( const site_record &site,
        const shakedown_surface &surface );
} // namespace bandit_live_world
