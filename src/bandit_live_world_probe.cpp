#include "bandit_live_world_probe.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <memory>

namespace
{
thread_local bandit_live_world_probe::session *active_session = nullptr;

template<typename Enum>
constexpr std::size_t enum_index( Enum value )
{
    return static_cast<std::size_t>( value );
}

std::int64_t bucket_upper_bound( const std::size_t index )
{
    constexpr std::size_t subdivisions = 64;
    const std::size_t exponent = index / subdivisions;
    const std::size_t subdivision = index % subdivisions;
    const std::uint64_t base = std::uint64_t( 1 ) << exponent;
    std::uint64_t upper = 0;
    if( exponent >= 6 ) {
        upper = base + ( subdivision + 1 ) * ( base >> 6 ) - 1;
    } else {
        const std::uint64_t delta = ( ( subdivision + 1 ) * base - 1 ) / subdivisions;
        upper = base + delta;
    }
    return static_cast<std::int64_t>( std::min<std::uint64_t>(
            upper, std::numeric_limits<std::int64_t>::max() ) );
}
} // namespace

namespace bandit_live_world_probe
{
void bounded_latency_histogram::add( const std::int64_t nanoseconds ) noexcept
{
    const std::uint64_t value = static_cast<std::uint64_t>( std::max<std::int64_t>( 0,
                                nanoseconds ) );
    if( count_ == 0 ) {
        minimum_ns_ = static_cast<std::int64_t>( value );
    } else {
        minimum_ns_ = std::min<std::int64_t>( minimum_ns_, static_cast<std::int64_t>( value ) );
    }
    maximum_ns_ = std::max<std::int64_t>( maximum_ns_, static_cast<std::int64_t>( value ) );
    if( std::numeric_limits<std::uint64_t>::max() - total_ns_ < value ) {
        total_ns_ = std::numeric_limits<std::uint64_t>::max();
        overflow_ = true;
    } else {
        total_ns_ += value;
    }
    if( count_ == std::numeric_limits<std::uint64_t>::max() ) {
        overflow_ = true;
    } else {
        ++count_;
    }

    if( value == 0 ) {
        if( zero_count_ == std::numeric_limits<std::uint64_t>::max() ) {
            overflow_ = true;
        } else {
            ++zero_count_;
        }
        return;
    }

    std::size_t exponent = 0;
    for( std::uint64_t shifted = value; shifted > 1; shifted >>= 1 ) {
        ++exponent;
    }
    const std::uint64_t base = std::uint64_t( 1 ) << exponent;
    const std::uint64_t delta = value - base;
    const std::size_t subdivision = exponent >= 6 ?
                                    static_cast<std::size_t>( delta / ( base >> 6 ) ) :
                                    static_cast<std::size_t>( delta * subdivisions / base );
    const std::size_t index = exponent * subdivisions +
                              std::min<std::size_t>( subdivision, subdivisions - 1 );
    if( index >= buckets_.size() || buckets_[index] == std::numeric_limits<std::uint32_t>::max() ) {
        overflow_ = true;
    } else {
        ++buckets_[index];
    }
}

latency_summary bounded_latency_histogram::summarize() const noexcept
{
    latency_summary result;
    result.count = count_;
    result.total_ns = total_ns_;
    result.minimum_ns = minimum_ns_;
    result.maximum_ns = maximum_ns_;
    result.overflow = overflow_;
    if( count_ == 0 ) {
        return result;
    }

    const auto percentile = [this]( const std::uint64_t numerator ) {
        const std::uint64_t rank = numerator * ( count_ / 100 ) +
                                   ( numerator * ( count_ % 100 ) + 99 ) / 100;
        std::uint64_t cumulative = zero_count_;
        if( cumulative >= rank ) {
            return std::int64_t( 0 );
        }
        for( std::size_t index = 0; index < buckets_.size(); ++index ) {
            cumulative += buckets_[index];
            if( cumulative >= rank ) {
                return std::min<std::int64_t>( bucket_upper_bound( index ), maximum_ns_ );
            }
        }
        return maximum_ns_;
    };
    result.p50_ns = percentile( 50 );
    result.p95_ns = percentile( 95 );
    result.p99_ns = percentile( 99 );
    return result;
}

struct session::timing_state {
    std::array<bounded_latency_histogram, section_count> inclusive;
    std::array<bounded_latency_histogram, section_count> self;
};

session::session( const collection_mode mode, const std::size_t expected_samples,
                  const std::size_t expected_sites )
{
    previous_ = active_session;
    result_.timings_collected = mode == collection_mode::timings;
    result_.site_services_collected = mode == collection_mode::site_services;
    if( result_.timings_collected ) {
        timing_state_ = std::make_unique<timing_state>();
    }
    ( void )expected_samples;
    if( result_.site_services_collected && expected_sites > 0 ) {
        result_.site_services.reserve( expected_sites );
        site_service_indices_.reserve( expected_sites );
    }
    active_session = this;
}

session::~session()
{
    assert( active_session == this );
    assert( stack_depth_ == 0 );
    active_session = previous_;
}

const snapshot &session::result()
{
    if( timing_state_ ) {
        for( std::size_t index = 0; index < section_count; ++index ) {
            result_.sections[index].inclusive = timing_state_->inclusive[index].summarize();
            result_.sections[index].self = timing_state_->self[index].summarize();
        }
    }
    return result_;
}

void session::record_timing( const section target, const std::int64_t inclusive_ns,
                             const std::int64_t self_ns ) noexcept
{
    assert( timing_state_ );
    timing_state_->inclusive[enum_index( target )].add( inclusive_ns );
    timing_state_->self[enum_index( target )].add( self_ns );
}

scoped_section::scoped_section( const section target ) noexcept
{
    session_ = active_session;
    if( session_ == nullptr || !session_->result_.timings_collected ) {
        session_ = nullptr;
        return;
    }
    if( session_->stack_depth_ >= session::max_nested_sections ) {
        session_->result_.stack_overflow = true;
        session_ = nullptr;
        return;
    }

    frame_index_ = session_->stack_depth_;
    session_->stack_[frame_index_] = { target, 0 };
    session_->stack_depth_++;
    started_ = std::chrono::steady_clock::now();
}

scoped_section::~scoped_section()
{
    if( session_ == nullptr ) {
        return;
    }

    const std::int64_t inclusive_ns = std::max<std::int64_t>( 0,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started_ ).count() );
    assert( session_->stack_depth_ == frame_index_ + 1 );
    const session::active_frame frame = session_->stack_[frame_index_];
    session_->stack_depth_--;

    const std::int64_t self_ns = std::max<std::int64_t>( 0, inclusive_ns - frame.child_ns );
    session_->record_timing( frame.target, inclusive_ns, self_ns );

    if( session_->stack_depth_ > 0 ) {
        session_->stack_[session_->stack_depth_ - 1].child_ns += inclusive_ns;
    }
}

void increment( const counter target, const std::uint64_t amount )
{
    if( active_session == nullptr ) {
        return;
    }
    active_session->result_.counters[enum_index( target )] += amount;
}

void record_site_service( const std::string &site_id, const site_service target,
                          const std::uint64_t amount )
{
    if( active_session == nullptr || !active_session->result_.site_services_collected ) {
        return;
    }

    std::vector<site_service_record> &records = active_session->result_.site_services;
    const auto existing = active_session->site_service_indices_.find( site_id );
    std::size_t record_index = 0;
    if( existing == active_session->site_service_indices_.end() ) {
        record_index = records.size();
        records.push_back( site_service_record{} );
        records.back().site_id = site_id;
        active_session->site_service_indices_.emplace( records.back().site_id, record_index );
    } else {
        record_index = existing->second;
    }
    records[record_index].counts[enum_index( target )] += amount;
}

std::string_view to_string( const section target )
{
    static constexpr std::array<std::string_view, section_count> names = {
        "world_serialize",
        "world_deserialize",
        "structural_maintenance",
        "structural_outings",
        "structural_scan",
        "structural_dispatch",
        "live_dispatch_plan",
        "live_dispatch_apply",
        "live_return_apply",
    };
    return names[enum_index( target )];
}

std::string_view to_string( const counter target )
{
    static constexpr std::array<std::string_view, counter_count> names = {
        "world_serialize_calls",
        "world_deserialize_calls",
        "structural_maintenance_updates",
        "structural_scan_sites_considered",
        "structural_scan_candidates_sampled",
        "structural_scan_sites_skipped_not_camp",
        "structural_outing_sites_considered",
        "structural_dispatch_sites_considered",
        "live_dispatch_plans",
        "live_dispatch_applies",
        "live_return_applies",
    };
    return names[enum_index( target )];
}

std::string_view to_string( const site_service target )
{
    static constexpr std::array<std::string_view, site_service_count> names = {
        "scan_considered",
        "scan_samples",
        "outing_considered",
        "dispatch_considered",
        "live_dispatch_plan",
        "live_return_apply",
    };
    return names[enum_index( target )];
}
} // namespace bandit_live_world_probe
