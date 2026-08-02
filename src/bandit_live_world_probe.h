#pragma once
#ifndef CATA_SRC_BANDIT_LIVE_WORLD_PROBE_H
#define CATA_SRC_BANDIT_LIVE_WORLD_PROBE_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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
    site_services
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

struct snapshot {
    std::array<section_samples, section_count> sections;
    std::array<std::uint64_t, counter_count> counters = {};
    std::vector<site_service_record> site_services;
    bool timings_collected = false;
    bool site_services_collected = false;
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

void increment( counter target, std::uint64_t amount = 1 );
void record_site_service( const std::string &site_id, site_service target,
                          std::uint64_t amount = 1 );

std::string_view to_string( section target );
std::string_view to_string( counter target );
std::string_view to_string( site_service target );
} // namespace bandit_live_world_probe

#endif // CATA_SRC_BANDIT_LIVE_WORLD_PROBE_H
