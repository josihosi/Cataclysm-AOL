#pragma once
#ifndef CATA_SRC_BANDIT_LIVE_WORLD_PROBE_H
#define CATA_SRC_BANDIT_LIVE_WORLD_PROBE_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>


class monster;

namespace bandit_live_world_probe
{
enum class section : std::size_t {
    world_serialize,
    world_deserialize,
    structural_maintenance,
    structural_outings,
    structural_scan,
    structural_dispatch,
    live_dispatch_plan,
    live_dispatch_apply,
    live_return_apply,
    loaded_covert_prepass,
    loaded_covert_member_motor,
    loaded_covert_overmap_route_solve,
    loaded_covert_local_path_solve,
    count
};

enum class counter : std::size_t {
    world_serialize_calls,
    world_deserialize_calls,
    structural_maintenance_updates,
    structural_scan_sites_considered,
    structural_scan_candidates_sampled,
    structural_scan_sites_skipped_not_camp,
    structural_outing_sites_considered,
    structural_dispatch_sites_considered,
    live_dispatch_plans,
    live_dispatch_applies,
    live_return_applies,
    loaded_covert_prepass_calls,
    loaded_covert_members_processed,
    loaded_covert_overmap_route_solves,
    loaded_covert_local_path_solves,
    count
};

enum class site_service : std::size_t {
    scan_considered,
    scan_samples,
    outing_considered,
    dispatch_considered,
    live_dispatch_plan,
    live_return_apply,
    count
};

enum class collection_mode : std::size_t {
    timings,
    site_services,
    transition_events
};

constexpr std::size_t section_count = static_cast<std::size_t>( section::count );
constexpr std::size_t counter_count = static_cast<std::size_t>( counter::count );
constexpr std::size_t site_service_count = static_cast<std::size_t>( site_service::count );

struct latency_summary {
    std::uint64_t count = 0;
    std::uint64_t total_ns = 0;
    std::int64_t minimum_ns = 0;
    std::int64_t p50_ns = 0;
    std::int64_t p95_ns = 0;
    std::int64_t p99_ns = 0;
    std::int64_t maximum_ns = 0;
    std::uint32_t relative_resolution_ppm = 15625;
    bool quantiles_are_upper_bounds = true;
    bool overflow = false;
};

class bounded_latency_histogram
{
    public:
        void add( std::int64_t nanoseconds ) noexcept;
        latency_summary summarize() const noexcept;

    private:
        static constexpr std::size_t subdivisions = 64;
        static constexpr std::size_t bucket_count = 64 * subdivisions;

        std::array<std::uint32_t, bucket_count> buckets_ = {};
        std::uint64_t zero_count_ = 0;
        std::uint64_t count_ = 0;
        std::uint64_t total_ns_ = 0;
        std::int64_t minimum_ns_ = 0;
        std::int64_t maximum_ns_ = 0;
        bool overflow_ = false;
};

struct section_samples {
    latency_summary inclusive;
    latency_summary self;
};

struct site_service_record {
    std::string site_id;
    std::array<std::uint64_t, site_service_count> counts = {};
};

constexpr std::size_t max_transition_events = 64;
constexpr std::size_t max_transition_event_field_length = 256;
constexpr std::size_t max_transition_event_reason_length = 256;

struct transition_event {
    int schema_version = 1;
    std::uint64_t sequence = 0;
    std::string run_id;
    int game_minutes = -1;
    std::string domain;
    std::string transition;
    std::string outcome;
    std::string site_id;
    std::string operation_id;
    std::string target_lead_id;
    int target_lead_revision = 0;
    int generation = 0;
    int handoff_epoch = -1;
    std::string simulation_owner;
    std::string previous_phase;
    std::string new_phase;
    std::string reason;
    int at_minutes = -1;
    std::vector<std::int64_t> actor_ids;
    std::string certification_round_id;
    std::string certification_lease_id;
    std::string certification_proof;
    std::string certification_previous_world_tree_sha256;
    std::string certification_previous_world_save_sha256;
    std::string certification_current_world_tree_sha256;
    std::string certification_current_world_save_sha256;
    std::uint64_t certification_save_sequence = 0;
    std::uint64_t certification_process_pid = 0;
    int turn = -1;
    std::string fixture_actor_id;
    std::string lifecycle_event;
    std::string monster_type;
    std::string absolute_position;
    std::string relative_position;
    int hitpoints = 0;
    bool dead = false;
    bool visible = false;
    // Per-read staffed-camp observations are deliberately carried in the
    // run-bound transition stream.  Aggregate scheduler counters are useful
    // diagnostics, but cannot stand in for a source/channel/lead receipt.
    bool has_staffed_camp_signal_read = false;
    std::string observer_id;
    std::string observer_capability;
    std::string observer_home;
    bool observer_at_home = false;
    bool observer_eligible = false;
    std::string camp_omt;
    std::string signal_channel;
    std::string source_omt;
    int source_intensity = -1;
    int range_actual = -1;
    int range_cap = -1;
    bool line_of_sight = false;
    int elevation_delta = 0;
    std::string weather;
    int visibility_input = -1;
    bool visibility_result = false;
    int emitted_minutes = -1;
    std::string lead_id;
    std::string lead_outcome;
    int work_reads = 0;
    int work_callbacks = 0;
    std::string persistence_site_id;
    std::string persistence_lead_id;
    int persistence_lead_count_before = -1;
    int persistence_lead_count_after = -1;
    std::string persistence_lead_hash_before;
    std::string persistence_lead_hash_after;
    std::string drive_state_before;
    std::string drive_state_after;
    // Aging receipts are emitted only after the production maintenance path
    // has committed an expired returned signal lead.  They make the retained
    // lead's exact channel horizon observable without changing that path.
    bool has_staffed_camp_signal_aging = false;
    std::string aging_lead_id;
    std::string aging_channel;
    std::string aging_source_omt;
    std::string aging_source_key;
    int aging_previous_last_seen_minutes = -1;
    std::int64_t aging_previous_age_minutes = -1;
    std::string aging_previous_status;
    int aging_result_last_seen_minutes = -1;
    std::int64_t aging_result_age_minutes = -1;
    std::string aging_result_status;
    bool aging_expired_removed = false;
    std::string aging_lead_set_hash_before;
    std::string aging_lead_set_hash_after;
    std::string aging_drive_state_before;
    std::string aging_drive_state_after;
};

struct snapshot {
    std::array<section_samples, section_count> sections;
    std::array<std::uint64_t, counter_count> counters = {};
    std::vector<site_service_record> site_services;
    std::vector<transition_event> transition_events;
    std::uint64_t dropped_transition_events = 0;
    bool timings_collected = false;
    bool site_services_collected = false;
    bool transition_events_collected = false;
    bool stack_overflow = false;
};

class scoped_section;

class session
{
    public:
        explicit session( collection_mode mode, std::size_t expected_samples = 0,
                          std::size_t expected_sites = 0 );
        ~session();

        session( const session & ) = delete;
        session &operator=( const session & ) = delete;

        const snapshot &result();

    private:
        friend class scoped_section;
        friend void increment( counter target, std::uint64_t amount );
        friend void record_site_service( const std::string &site_id, site_service target,
                                         std::uint64_t amount );
        friend bool transition_events_enabled() noexcept;
        friend void record_transition_event( std::string_view operation_id, int generation,
                                             std::string_view simulation_owner,
                                             std::string_view previous_phase,
                                             std::string_view new_phase, std::string_view reason,
                                             int at_minutes );
        friend void record_transition_event( transition_event event );

        struct active_frame {
            section target = section::world_serialize;
            std::int64_t child_ns = 0;
        };

        struct timing_state;

        static constexpr std::size_t max_nested_sections = 8;

        void record_timing( section target, std::int64_t inclusive_ns,
                            std::int64_t self_ns ) noexcept;

        snapshot result_;
        std::unique_ptr<timing_state> timing_state_;
        std::unordered_map<std::string, std::size_t> site_service_indices_;
        std::array<active_frame, max_nested_sections> stack_ = {};
        std::size_t stack_depth_ = 0;
        session *previous_ = nullptr;
};

class scoped_section
{
    public:
        explicit scoped_section( section target ) noexcept;
        ~scoped_section();

        scoped_section( const scoped_section & ) = delete;
        scoped_section &operator=( const scoped_section & ) = delete;

    private:
        session *session_ = nullptr;
        std::size_t frame_index_ = 0;
        std::chrono::steady_clock::time_point started_;
};

class scoped_loaded_covert_member
{
    public:
        explicit scoped_loaded_covert_member( bool enabled ) noexcept;
        ~scoped_loaded_covert_member();

        scoped_loaded_covert_member( const scoped_loaded_covert_member & ) = delete;
        scoped_loaded_covert_member &operator=( const scoped_loaded_covert_member & ) = delete;

    private:
        bool enabled_ = false;
};

class scoped_loaded_covert_local_path_solve
{
    public:
        scoped_loaded_covert_local_path_solve() noexcept;

        scoped_loaded_covert_local_path_solve(
            const scoped_loaded_covert_local_path_solve & ) = delete;
        scoped_loaded_covert_local_path_solve &operator=(
            const scoped_loaded_covert_local_path_solve & ) = delete;

    private:
        std::optional<scoped_section> section_;
};

void increment( counter target, std::uint64_t amount = 1 );
void record_site_service( const std::string &site_id, site_service target,
                          std::uint64_t amount = 1 );
bool active() noexcept;
bool loaded_covert_member_active() noexcept;
bool transition_events_enabled() noexcept;
void record_transition_event( std::string_view operation_id, int generation,
                              std::string_view simulation_owner,
                              std::string_view previous_phase, std::string_view new_phase,
                              std::string_view reason, int at_minutes );
void record_transition_event( transition_event event );
// Diagnostic-only fixture lifecycle events share the launch-bound transition
// stream.  They are inert unless the exact fixture tag is selected by the
// child environment, so ordinary monster processing never becomes telemetry.
void record_fixture_monster_lifecycle( const monster &critter, std::string_view event,
                                       std::string_view owner );
void record_live_transition_event( transition_event event );
// Writes one opt-in, launch-bound, canonical persistence snapshot for a committed
// local-pair handoff.  The caller owns serializing the exact site payload so this
// probe stays independent of the live-world persistence types.
void record_local_pair_handoff_snapshot( const transition_event &event,
        std::string_view site_payload, std::string_view owner_transition,
        std::string_view omt );
void record_certification_save_receipt( int game_minutes, const std::string &world_path );

std::string_view to_string( section target );
std::string_view to_string( counter target );
std::string_view to_string( site_service target );
} // namespace bandit_live_world_probe

#endif // CATA_SRC_BANDIT_LIVE_WORLD_PROBE_H
