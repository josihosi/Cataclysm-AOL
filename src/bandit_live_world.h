#pragma once

#include "bandit_dry_run.h"
#include "bandit_pursuit_handoff.h"

#include <functional>
#include <optional>
#include <string>
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
    dead,
    missing,
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

enum class camp_lead_status {
    suspected,
    scout_confirmed,
    active,
    harvested,
    stale,
    invalidated,
    dangerous,
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

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct spawn_tile_record {
    tripoint_abs_ms tile;
    int headcount = 0;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

struct camp_map_lead {
    std::string lead_id;
    camp_lead_kind kind = camp_lead_kind::human_activity;
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
    int schema_version = 1;
    int last_daily_cleanup_minutes = -1;
    int next_near_tick_minutes = -1;
    int next_mid_tick_minutes = -1;
    int next_far_tick_minutes = -1;
    int next_frontier_tick_minutes = -1;
    int known_radius_omt = 0;
    int frontier_radius_omt = 0;
    std::vector<camp_map_lead> leads;

    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );

    camp_map_lead *find_lead( const std::string &lead_id );
    const camp_map_lead *find_lead( const std::string &lead_id ) const;
};

struct site_record {
    std::string site_id;
    anchor_source_kind source_kind = anchor_source_kind::none;
    owned_site_kind site_kind = owned_site_kind::none;
    hostile_site_profile profile = hostile_site_profile::none;
    std::string source_id;
    tripoint_abs_omt anchor;
    int headcount = 0;
    std::vector<tripoint_abs_omt> footprint;
    std::vector<member_record> members;
    std::vector<spawn_tile_record> spawn_tiles;
    std::string active_group_id;
    std::string active_target_id;
    tripoint_abs_omt active_target_omt;
    std::string active_job_type;
    std::vector<character_id> active_member_ids;
    int active_sortie_started_minutes = -1;
    int active_sortie_local_contact_minutes = -1;
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
    int count_home_side_signals() const;
    int dispatchable_member_capacity() const;
    bool has_active_outside_pressure() const;
    bool eligible_for_empty_site_retirement() const;
};

struct world_state {
    std::string owner_id = "hells_raiders_live_owner_v0";
    std::vector<site_record> sites;

    void clear();
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );

    site_record *find_site( const std::string &site_id );
    const site_record *find_site( const std::string &site_id ) const;
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

struct structural_outing_plan {
    bool valid = false;
    std::string site_id;
    std::string lead_id;
    tripoint_abs_omt target_omt;
    bandit_dry_run::job_template job = bandit_dry_run::job_template::hold_chill;
    std::vector<character_id> member_ids;
    int effective_interest = 0;
    int known_threat = 0;
    int expected_stalking_minutes = -1;
    int expected_arrival_minutes = -1;
    std::vector<std::string> notes;
};

struct structural_threat_read {
    int threat = 0;
    bool observed = false;
    std::string summary;
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

struct structural_bounty_maintenance_result {
    structural_outing_result outing;
    structural_bounty_scan_result scan;
    int sites_considered_for_dispatch = 0;
    int dispatches_planned = 0;
    int dispatches_applied = 0;
    int dispatches_blocked = 0;
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
    bool local_contact_established = false;
};

struct local_gate_decision {
    bool valid = false;
    local_gate_posture posture = local_gate_posture::abort;
    int dispatch_strength = 0;
    int pressure_margin = 0;
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
bool claim_tracked_spawn( world_state &state, const std::string &npc_template_id,
                          character_id npc_id, const tripoint_abs_ms &spawn_tile,
                          const std::optional<std::string> &overmap_special_id,
                          const std::optional<std::string> &map_extra_id,
                          const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup );
dispatch_plan plan_site_dispatch( const site_record &site, const tripoint_abs_omt &target_omt,
                                  const std::string &target_id );
dispatch_plan plan_site_dispatch_from_camp_map_lead( const site_record &site,
        const camp_map_lead &lead,
        const camp_map_dispatch_pressure &pressure = camp_map_dispatch_pressure() );
camp_map_dispatch_decision choose_camp_map_dispatch( const site_record &site,
        const camp_map_lead &lead,
        const camp_map_dispatch_pressure &pressure = camp_map_dispatch_pressure() );
const camp_map_lead *find_camp_map_dispatch_lead_for_target( const site_record &site,
        const tripoint_abs_omt &target_omt,
        const std::string &target_id );
structural_bounty_read classify_structural_bounty_terrain( const std::string &overmap_terrain_id );
std::string make_structural_bounty_lead_id( const std::string &site_id,
        const tripoint_abs_omt &omt, const std::string &terrain_class );
bool structural_bounty_memory_suppresses_refresh( const camp_intelligence_map &intelligence_map,
        const tripoint_abs_omt &omt, const std::string &terrain_class );
bool upsert_structural_bounty_lead( site_record &site, const tripoint_abs_omt &omt,
                                   const structural_bounty_read &read, int now_minutes );
structural_bounty_scan_result advance_structural_bounty_scan( world_state &state,
        int now_minutes, int scan_budget,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &terrain_lookup );
structural_outing_plan plan_structural_bounty_outing( const site_record &site,
        const camp_map_lead &lead, int now_minutes );
structural_outing_plan plan_structural_bounty_outing( const site_record &site, int now_minutes );
bool apply_structural_bounty_outing_plan( site_record &site, const structural_outing_plan &plan,
        int now_minutes );
structural_outing_result advance_structural_bounty_outings( world_state &state, int now_minutes,
        const std::function<structural_threat_read( const site_record &, const camp_map_lead & )> &threat_lookup );
structural_bounty_maintenance_result advance_structural_bounty_maintenance( world_state &state,
        int now_minutes, int scan_budget, int dispatch_cap,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &terrain_lookup,
        const std::function<structural_threat_read( const site_record &, const camp_map_lead & )> &threat_lookup );
std::string render_structural_bounty_maintenance_report(
    const structural_bounty_maintenance_result &result );
bool apply_dispatch_plan( site_record &site, const dispatch_plan &plan );
local_gate_decision choose_local_gate_posture( const site_record &site,
        const local_gate_input &input );
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
        const std::vector<sight_avoid_candidate> &candidates );
bool note_active_sortie_started( site_record &site, int current_minutes );
bool note_active_sortie_local_contact( site_record &site, int current_minutes );
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
bool record_live_signal_mark( site_record &site, const live_signal_mark &mark );
std::string render_empty_site_retirement_report( const site_record &site );
int retire_empty_hostile_sites( world_state &state, std::vector<std::string> *reports = nullptr );
bool apply_return_packet( site_record &site, const bandit_pursuit_handoff::return_packet &packet );
std::optional<bandit_pursuit_handoff::return_packet> resolve_active_group_aftermath(
    const site_record &site, const std::vector<active_member_observation> &observations );
bool update_member_state( site_record &site, character_id npc_id, member_state new_state,
                          const std::string &summary );

std::string to_string( anchor_source_kind source_kind );
std::string to_string( owned_site_kind site_kind );
std::string to_string( hostile_site_profile profile );
std::string to_string( member_state state );
std::string to_string( active_member_observation_state state );
std::string to_string( local_gate_posture posture );
std::string to_string( camp_lead_kind kind );
std::string to_string( camp_lead_status status );
std::string render_local_gate_report( const site_record &site, const local_gate_input &input,
                                      const local_gate_decision &decision );
std::string render_shakedown_surface_report( const site_record &site,
        const shakedown_surface &surface );
} // namespace bandit_live_world
