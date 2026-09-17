#pragma once

#include "bandit_mark_generation.h"
#include "coordinates.h"

#include <optional>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class monster;
class JsonObject;
class JsonOut;
struct predator_lifecycle_state;

namespace zombie_rider_overmap_ai
{
constexpr int mature_world_gate_seasons = 8;
constexpr int max_riders_drawn_by_light = 2;
// Abstract candidate envelope.  The live adapter currently supplies loaded monsters only.
constexpr int rider_convergence_response_radius_omt = 36;
constexpr int rider_band_minimum_size = 2;
constexpr int rider_light_memory_decay_interval_turns = 60;

int mature_world_gate_days();

struct rider_light_interest {
    bool should_investigate = false;
    int interest_score = 0;
    int memory_turns = 0;
    int max_riders_drawn = 0;
    std::string reason = "none";
    std::vector<std::string> notes;
};

struct rider_light_memory {
    int interest_score = 0;
    int turns_remaining = 0;
    int max_riders_drawn = 0;
    int decay_turn_remainder = 0;
    // A light lead is evidence, rather than a durable assertion about a
    // source.  These absolute turn timestamps survive save/load and ensure a
    // stale delivery cannot turn an old blink into a current beacon.
    int observed_at_turn = -1;
    int expires_at_turn = -1;
    std::string sample_id;
    std::string reason = "none";

    bool active() const {
        return interest_score > 0 && turns_remaining > 0 && max_riders_drawn > 0;
    }
};

struct rider_overmap_agent {
    std::string rider_id;
    tripoint_abs_omt pos = tripoint_abs_omt::zero;
    bool available = true;
    bool already_in_band = false;
    int cooldown_turns = 0;
};

// The global band owner deliberately contains no monster pointers or map
// positions.  Actor IDs survive materialization and saving; positions only
// prove a particular live encounter.
struct rider_band_encounter {
    std::string first_actor_id;
    std::string second_actor_id;
    int observed_turn = -1;
    bool reciprocal_perception = false;
    bool reachable_route = false;
};

struct rider_band_observation {
    std::uint64_t target_identity = 0;
    tripoint_abs_ms position = tripoint_abs_ms::zero;
    int observed_turn = -1;
    int expires_at_turn = -1;
    // The actor whose direct sight created this finite record.  A recipient
    // retains this origin across a relay, so persistence and diagnostics can
    // distinguish a genuine direct sighting from encounter-bound sharing.
    std::string source_actor_id;
    std::string provenance;
    int uncertainty = 0;

    bool valid_at( int turn ) const {
        return observed_turn >= 0 && turn >= observed_turn &&
               turn <= expires_at_turn;
    }
};

class rider_band_registry
{
    public:
        // Returns true only for a new, reciprocal, route-connected encounter.
        // Root selection is lexical, so replay and reversed argument order are
        // deterministic.
        bool register_encounter( const rider_band_encounter &encounter );
        bool record_casualty( const std::string &actor_id, int turn );
        bool share_observation( const std::string &from_actor_id, const std::string &to_actor_id,
                                int turn );
        void observe( const std::string &actor_id, const rider_band_observation &observation );
        std::optional<rider_band_observation> observation_for( const std::string &actor_id,
                int turn ) const;
        std::string band_for( const std::string &actor_id ) const;
        bool in_same_band( const std::string &first_actor_id, const std::string &second_actor_id ) const;
        int living_members( const std::string &actor_id ) const;
        std::uint64_t revision_for( const std::string &actor_id ) const;
        void reconcile_cache( const std::string &actor_id, predator_lifecycle_state &state ) const;
        // Read-only serialization for a fixture-gated native harness diagnostic.
        // Production persistence continues to use serialize().
        std::string diagnostic_json() const;
        void clear();
        void serialize( JsonOut &json ) const;
        void deserialize( const JsonObject &json );

    private:
        struct member {
            std::string parent;
            bool alive = true;
            int casualty_turn = -1;
        };
        std::map<std::string, member> members;
        std::map<std::string, std::uint64_t> revisions;
        std::map<std::string, int> encounter_turns;
        std::map<std::string, rider_band_observation> observations;

        std::string root_for( const std::string &actor_id ) const;
        std::vector<std::string> malformed_references() const;
        std::string encounter_key( const std::string &first, const std::string &second ) const;
        void ensure_member( const std::string &actor_id );
        void bump_revision( const std::string &root );
};

struct rider_convergence_result {
    bool should_converge = false;
    bool band_formed = false;
    int selected_riders = 0;
    int band_size = 0;
    int cap = 0;
    std::string posture = "none";
    std::string reason = "none";
    std::vector<std::string> rider_ids;
    std::vector<std::string> notes;
};

enum class rider_camp_pressure_posture {
    none,
    investigate,
    circle_harass,
    direct_attack,
    withdraw,
};

struct rider_camp_pressure_input {
    bool light_memory_active = false;
    int rider_count = 0;
    bool band_formed = false;
    bool breach_or_opening = false;
    int defender_strength = 0;
    bool rider_wounded = false;
};

struct rider_camp_pressure_result {
    rider_camp_pressure_posture posture = rider_camp_pressure_posture::none;
    std::string reason = "none";
    std::vector<std::string> notes;
};

struct rider_camp_pressure_intent {
    rider_camp_pressure_posture posture = rider_camp_pressure_posture::none;
    tripoint_abs_ms source = tripoint_abs_ms::zero;
    int formation_slot = 0;
    int turns_remaining = 0;
};

enum class pursuit_phase : std::uint8_t { idle, pursuing, searching };

// Durable, rider-local evidence and physical routing state.  The waypoint is
// deliberately separate from the logical prey destination held by monster.
struct rider_pursuit_state {
    pursuit_phase phase = pursuit_phase::idle;
    std::uint64_t target_identity = 0;
    tripoint_abs_ms last_observed_position;
    bool has_last_observed_position = false;
    int last_observed_turn = -1;
    // Direct and relayed evidence use the same bounded routing path, but keep
    // their origin so diagnostics cannot mistake a relay for fresh local sight.
    std::string evidence_source_actor_id;
    std::string evidence_provenance;
    tripoint_abs_ms movement_waypoint;
    bool has_movement_waypoint = false;
    int search_until_turn = -1;
    bool impact_ready = false;
    int impact_ready_turn = -1;
    // A completed impact yields exactly one player recovery turn before this
    // rider resumes its ordinary pursuit.  This is rider-local, persisted
    // combat state rather than a global monster pause.
    int impact_recovery_until_turn = -1;

    void advance_to( int now_turn );
    bool evidence_valid( int now_turn ) const;
    void observe( std::uint64_t identity, const tripoint_abs_ms &position, int now_turn );
    void relay( const rider_band_observation &observation );
    void begin_search( int now_turn );
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &json );
};

rider_light_interest evaluate_light_attraction(
    const bandit_mark_generation::light_projection &projection,
    int world_age_days,
    int eligible_riders_nearby = 1 );
void refresh_light_memory( rider_light_memory &memory, const rider_light_interest &interest );
// Returns true only when this is a new physical detection.  Re-delivering a
// cached sample is deliberately a no-op; a later detection receives a new
// sample id and writes a new finite expiry.
bool refresh_light_memory( rider_light_memory &memory, const rider_light_interest &interest,
                           const std::string &sample_id, int observed_at_turn );
void advance_light_memory( rider_light_memory &memory, int elapsed_turns );
rider_convergence_result evaluate_rider_convergence(
    const rider_light_memory &memory,
    const tripoint_abs_omt &light_omt,
    const std::vector<rider_overmap_agent> &riders );
void reserve_rider_convergence( std::vector<rider_overmap_agent> &riders,
                                const rider_convergence_result &convergence );
rider_camp_pressure_result choose_camp_pressure_posture(
    const rider_camp_pressure_input &input );
std::string to_string( rider_camp_pressure_posture posture );
void set_camp_pressure_intent( monster &rider, rider_camp_pressure_posture posture,
                               const tripoint_abs_ms &source, int duration_turns,
                               int formation_slot = 0 );
std::optional<rider_camp_pressure_intent> get_camp_pressure_intent( monster &rider );
void clear_camp_pressure_intent( monster &rider );

} // namespace zombie_rider_overmap_ai
