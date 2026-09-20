#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "coordinates.h"

class monster;
class Creature;
class map;
class JsonObject;
class JsonOut;

namespace writhing_stalker
{

using actor_identity = std::uint64_t;
using resolution_id = std::uint64_t;

enum class lifecycle_phase : std::uint8_t {
    idle,
    shadowing,
    approaching,
    attacking,
    retreating,
    searching,
    cooldown
};

// The sole durable owner of a stalker's cross-turn/local-overmap commitment.
// Old caol_writhing_stalker_* diagnostic values are intentionally not migrated
// into this state.
struct persistent_state {
    lifecycle_phase phase = lifecycle_phase::idle;
    actor_identity evidence_target = 0;
    int evidence_turn = -1;
    tripoint_abs_ms last_observed_position;
    bool has_last_observed_position = false;
    // A glow is an area observation, not recognition of a creature.  Keep it
    // apart from direct-prey evidence so a later plan cannot turn a lamp into
    // a live target identity.
    tripoint_abs_ms light_observed_position;
    bool has_light_observed_position = false;
    int light_observed_turn = -1;
    int light_expires_turn = -1;
    std::string light_sample_id;
    tripoint_abs_ms committed_waypoint;
    bool has_committed_waypoint = false;
    tripoint_abs_ms retreat_waypoint;
    bool has_retreat_waypoint = false;
    int attempts_spent = 0;
    std::uint64_t attempt_sequence = 0;
    int phase_entered_turn = -1;
    int cooldown_until_turn = -1;
    int last_progress_turn = -1;
    int last_advanced_turn = -1;
    int remaining_route_progress = -1;
    int search_until_turn = -1;

    void advance_to( int now_turn );
    bool cooldown_active( int now_turn ) const;
    bool has_direct_evidence( int now_turn ) const;
    bool light_interest_active( int now_turn ) const;
    // Returns false for an old/replayed sample or while fresher direct prey
    // evidence owns the stalker's commitment.
    bool observe_light_interest( const tripoint_abs_ms &position, const std::string &sample_id,
                                 int observed_turn, int duration_turns );
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

// Persisted on monster::value; this is an attempt count, not a plan/turn count.
inline constexpr const char *burst_count_key = "caol_writhing_stalker_burst_count";

// Called only after a melee actor has selected a target and is entering attack
// resolution. Walking, planning, misses and blocked routes do not call it.
void record_attack_attempt( monster &stalker );
void record_counterpressure_attempt( monster &stalker, const Creature &attacker,
                                     resolution_id resolution );
resolution_id next_resolution_id();
void observe_attack_resolution( const Creature *attacker, const Creature *target, int turn,
                                resolution_id resolution );
void record_observed_attack( const Creature *observer, const Creature *attacker,
                             const Creature *target, int turn, resolution_id resolution );
bool has_recent_observed_attack( const Creature *observer, const Creature *attacker,
                                const Creature *target, int now_turn, int max_age );
int observed_attack_count( const Creature *observer, const Creature *attacker,
                           const Creature *target, resolution_id resolution );

struct pressure_observation {
    bool hostile = false;
    bool visible_to_stalker = false;
    bool same_target = false;
    bool attacking = false;
    bool closing = false;
};

// A nearby zombie contributes when its observable local intent is directly
// aimed at this target (native attack target and destination).
bool is_meaningful_pressure( const pressure_observation &observation );

// Ephemeral, perception-only pressure history.  It is deliberately separate
// from persistent stalker state and observed attack resolution history.
int observed_zombie_pressure( const monster &stalker, map &here, const Creature &target );
void reset_transient_pressure_history();
std::size_t transient_pressure_sample_count();

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

enum class handoff_intent {
    none,
    overmatched_stalk,
    shadowing,
    opportunity_probe,
    committed_ambush,
    spent_disengage
};

enum class threat_state {
    none,
    overmatched_retreat,
    watching,
    opportunity_probe,
    committed_ambush,
    spent_disengage
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
    bool night_outside_reachable_target = false;
    int zombie_pressure = 0;
    int allied_support_nearby = 0;
    bool near_cover_or_clutter = false;
    bool bright_exposure = false;
    bool player_focused = false;
    bool stalker_hurt = false;
    bool counterpressure_recent = false;
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
    handoff_intent overmap_intent = handoff_intent::none;
    int overmap_stalk_distance_omt = 0;
    int overmap_cooldown_minutes = 0;
    int overmap_threat_memory = 0;
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
    bool night_outside_reachable_target = false;
    int zombie_pressure = 0;
    int allied_support_nearby = 0;
    bool quiet_side_cutoff_available = false;
    bool near_cover_or_clutter = false;
    bool stalker_hurt = false;
    bool counterpressure_recent = false;
    bool on_cooldown = false;
    int burst_strikes = 0;
    int bad_position_loiter_turns = 0;
};

struct live_response {
    decision next = decision::ignore;
    approach_class route = approach_class::none;
    handoff_intent writeback_intent = handoff_intent::none;
    int opportunity = 0;
    int confidence = 0;
    int burst_limit = 1;
    int retreat_distance = 8;
    int overmap_stalk_distance_omt = 0;
    int writeback_cooldown_minutes = 0;
    int writeback_threat_memory = 0;
    bool persistent_state_required = false;
    bool anti_gnome_triggered = false;
    std::string reason;
};

struct threat_context {
    bool daylight_or_bright = false;
    bool target_has_focus = false;
    bool same_or_close_overmap = false;
    int allied_support_nearby = 0;
    int zombie_pressure = 0;
    int burst_strikes = 0;
    bool close_overmap_pressure = false;
    bool strong_visibility = false;
    bool has_distraction = false;
    bool night_or_dark = false;
    bool outside_reachable_player = false;
    bool valid_evidence_or_path = false;
    int bad_position_loiter_turns = 0;
    int burst_limit = 2;
    bool spent_cooldown = false;
};

struct threat_report {
    bool overmatched = false;
    decision next = decision::ignore;
    handoff_intent intent = handoff_intent::none;
    int stalk_distance_omt = 0;
    int threat_memory = 0;
    bool avoid_sight_tiles = false;
    threat_state state = threat_state::none;
    int threat_score = 0;
    int opportunity_score = 0;
    int stalking_distance_omt = 0;
    bool avoids_sight_tiles = false;
    bool anti_gnome_fired = false;
    std::string reason;
};

struct anti_gnome_context {
    bool night_outside_reachable_target = false;
    bool has_believable_evidence_or_handoff = false;
    bool dark_or_covered_route_available = false;
    bool high_threat = false;
    int distance_to_target = 0;
    int loiter_turns = 0;
};

struct handoff_memory {
    handoff_intent intent = handoff_intent::none;
    int strike_budget_spent = 0;
    int cooldown_minutes = 0;
    int threat_memory = 0;
    int stalk_distance_omt = 0;
    std::string reason;
};

interest_report evaluate_interest( const interest_context &ctx );
latch_update advance_latch( const latch_context &ctx );
approach_report choose_approach( const approach_context &ctx );
// Canonical threat/distraction state machine for packet-level scoring and tests.
threat_report evaluate_threat_state( const threat_context &ctx );
opportunity_report evaluate_opportunity( const opportunity_context &ctx );
quiet_side_report evaluate_quiet_side( const std::vector<relative_point> &zombies );
quiet_candidate_report choose_quiet_side_cutoff( const std::vector<relative_point> &zombies,
        const std::vector<quiet_candidate> &candidates );
confidence_report evaluate_confidence( const confidence_context &ctx );
// Live handoff facade: normalizes older compact threat_context fields, then delegates to
// evaluate_threat_state so live planning and deterministic threat tests share one scorer.
threat_report evaluate_threat( const threat_context &ctx );
live_response resolve_anti_gnome( const anti_gnome_context &ctx );
live_response evaluate_live_response( const live_context &ctx );
handoff_memory writeback_handoff_memory( const handoff_memory &incoming,
        const live_response &response );

} // namespace writhing_stalker
