#pragma once
#ifndef CATA_SRC_NATIVE_PERFORMANCE_INTERVAL_H
#define CATA_SRC_NATIVE_PERFORMANCE_INTERVAL_H

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

// Fixed-size summary of completed simulation durations. Percentiles are
// upper bounds in 1 ms buckets; values beyond 2 s are counted separately and
// cannot be assigned a finite percentile bound.
class native_performance_interval
{
    public:
        void add( double seconds ) {
            ++count;
            if( !std::isfinite( seconds ) || seconds < 0 ) {
                ++invalid;
                return;
            }
            ++valid;
            if( !total_overflow ) {
                if( seconds > std::numeric_limits<double>::max() - total_seconds ) {
                    total_overflow = true;
                } else {
                    total_seconds += seconds;
                }
            }
            if( seconds > 0.1 ) {
                ++over_100ms;
            }
            if( seconds > 2.0 ) {
                ++overflow;
            } else {
                const auto bucket = static_cast<std::size_t>( std::ceil( seconds * 1000.0 ) );
                ++histogram[bucket];
            }
        }

        std::optional<int> percentile_upper_ms( double fraction ) const {
            if( valid == 0 || !std::isfinite( fraction ) || fraction <= 0 || fraction > 1 ) {
                return std::nullopt;
            }
            const auto target = static_cast<std::size_t>( std::ceil( fraction * valid ) );
            std::size_t seen = 0;
            for( std::size_t i = 0; i < histogram.size(); ++i ) {
                seen += histogram[i];
                if( seen >= target ) {
                    return static_cast<int>( i );
                }
            }
            return std::nullopt;
        }

        std::size_t count = 0;
        std::size_t valid = 0;
        std::size_t invalid = 0;
        std::size_t overflow = 0;
        std::size_t over_100ms = 0;
        double total_seconds = 0;
        bool total_overflow = false;

    private:
        std::array<std::size_t, 2001> histogram{};
};
#endif
