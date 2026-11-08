#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

// Fixed-key, fixed-size per-process alarm accumulator. Classification belongs
// to the existing alarm producer; this type never supplies a timing threshold.
struct slow_turn_identity {
    std::uint64_t turn_id = 0;
    std::uint64_t start_sequence = 0;
    std::uint64_t end_sequence = 0;
    int game_turn = 0;
};

struct slow_turn_candidate {
    // Producer-defined bounded enum values, never arbitrary caller strings.
    std::uint8_t alarm_kind = 0;
    std::uint16_t dominant_stage = 0;
    bool details_available = true;
    std::uint64_t metric_epoch = 1;
    slow_turn_identity first;
    slow_turn_identity last;
    slow_turn_identity dominant;
    std::uint16_t sample_count = 0;
    double threshold_seconds = 0;
    double simulation_seconds = 0;
    double dominant_simulation_seconds = 0;
    double start_wall_seconds = 0;
    double end_wall_seconds = 0;
    std::optional<double> simulation_cpu_seconds;
    std::optional<double> input_seconds;
    std::optional<double> save_seconds;
    std::array<std::uint16_t, 3> stage_ids{};
    std::array<double, 3> stage_seconds{};
    std::array<std::uint64_t, 6> workload_counts{};
};

struct slow_turn_record {
    bool used = false;
    std::uint64_t count = 0;
    slow_turn_identity first;
    slow_turn_identity last;
    slow_turn_candidate worst;
};

class native_slow_turn_receipts
{
    public:
        static constexpr std::size_t capacity = 8;
        static constexpr double publication_interval_seconds = 30.0;

        void add( const slow_turn_candidate &candidate ) {
            std::size_t free = capacity;
            std::size_t least = capacity;
            for( std::size_t i = 0; i < capacity; ++i ) {
                slow_turn_record &r = records_[i];
                if( !r.used ) {
                    if( free == capacity ) { free = i; }
                    continue;
                }
                if( r.worst.alarm_kind == candidate.alarm_kind &&
                    r.worst.dominant_stage == candidate.dominant_stage &&
                    r.worst.metric_epoch == candidate.metric_epoch &&
                    r.worst.threshold_seconds == candidate.threshold_seconds ) {
                    ++r.count;
                    r.last = candidate.last;
                    if( candidate.simulation_seconds > r.worst.simulation_seconds ) {
                        r.worst = candidate;
                    }
                    pending_ = true;
                    return;
                }
                if( least == capacity || r.worst.simulation_seconds < records_[least].worst.simulation_seconds ) {
                    least = i;
                }
            }
            if( free == capacity ) {
                if( candidate.simulation_seconds <= records_[least].worst.simulation_seconds ) {
                    ++dropped_;
                    pending_ = true;
                    return;
                }
                dropped_ += records_[least].count;
                free = least;
            }
            records_[free] = { true, 1, candidate.first, candidate.last, candidate };
            pending_ = true;
        }

        // First selected event can publish immediately. Subsequent updates are
        // coalesced for at least 30 seconds; an explicit lifecycle flush may
        // publish earlier. A failed publication leaves the data pending.
        bool should_publish( double now_seconds, bool lifecycle_flush = false ) const {
            return pending_ && ( lifecycle_flush || !last_attempted_ ||
                                 now_seconds - *last_attempted_ >= publication_interval_seconds );
        }
        // One capability snapshot per monitored process makes the enabled
        // path distinguishable from an older or monitoring-off producer.
        void mark_started() { pending_ = true; }
        void published( double now_seconds, bool success ) {
            last_attempted_ = now_seconds;
            if( success ) {
                pending_ = false;
                ++publication_sequence_;
            }
        }
        const std::array<slow_turn_record, capacity> &records() const { return records_; }
        std::uint64_t dropped() const { return dropped_; }
        std::uint64_t publication_sequence() const { return publication_sequence_; }
        bool pending() const { return pending_; }
    private:
        std::array<slow_turn_record, capacity> records_{};
        std::uint64_t dropped_ = 0;
        std::uint64_t publication_sequence_ = 0;
        std::optional<double> last_attempted_;
        bool pending_ = false;
};

static_assert( sizeof( native_slow_turn_receipts ) < 32768,
               "slow-turn receipt memory must remain bounded" );

// The caller supplies its validated scenario thresholds; missing thresholds
// remain missing. The only fixed selection here is the already-established
// 100-turn waiting alarm, including its sliding-window/crossing semantics.
class native_slow_turn_detector
{
    public:
        void observe( const slow_turn_candidate &sample, std::uint64_t waiting_generation,
                      std::optional<double> slow_seconds, std::optional<double> spike_seconds,
                      std::optional<int> sustained_turns = std::nullopt ) {
            if( last_turn_ && sample.last.turn_id <= *last_turn_ ) {
                return;
            }
            if( last_turn_ && sample.last.turn_id != *last_turn_ + 1 ) {
                reset_wait();
            }
            last_turn_ = sample.last.turn_id;
            if( waiting_generation == 0 ) {
                if( spike_seconds && sample.simulation_seconds > *spike_seconds ) {
                    slow_turn_candidate alarm = sample;
                    alarm.alarm_kind = 1;
                    alarm.threshold_seconds = *spike_seconds;
                    receipts_.add( alarm );
                }
                if( slow_seconds && sample.simulation_seconds > *slow_seconds ) {
                    slow_turn_candidate alarm = sample;
                    alarm.alarm_kind = 2;
                    alarm.threshold_seconds = *slow_seconds;
                    receipts_.add( alarm );
                }
                if( !slow_seconds || !sustained_turns || *sustained_turns < 1 ||
                    *sustained_turns > 256 || sample.simulation_seconds <= *slow_seconds ) {
                    reset_wait();
                    return;
                }
                const std::size_t limit = static_cast<std::size_t>( *sustained_turns );
                if( mode_ != 4 || limit_ != limit || threshold_ != *slow_seconds ) {
                    reset_wait();
                    mode_ = 4;
                    limit_ = limit;
                    threshold_ = *slow_seconds;
                }
                append( sample );
                if( count_ == limit_ ) {
                    emit_window( sample, 4, threshold_ );
                }
                return;
            }
            if( mode_ != 3 || waiting_generation != waiting_generation_ ) {
                reset_wait();
                mode_ = 3;
                limit_ = 100;
                waiting_generation_ = waiting_generation;
            }
            append( sample );
            if( count_ != limit_ ) {
                return;
            }
            const bool slow = window_mean() > 0.100;
            if( slow && !waiting_alarmed_ ) {
                emit_window( sample, 3, 0.100 );
            }
            waiting_alarmed_ = slow;
        }
        native_slow_turn_receipts &receipts() { return receipts_; }
        const native_slow_turn_receipts &receipts() const { return receipts_; }
        void reset_waiting() { reset_wait(); }
    private:
        struct compact_turn {
            slow_turn_identity identity;
            double simulation_seconds = 0;
            double start_wall_seconds = 0;
            double end_wall_seconds = 0;
        };
        void reset_wait() {
            waiting_generation_ = 0;
            mode_ = 0;
            count_ = next_ = limit_ = 0;
            threshold_ = 0;
            waiting_alarmed_ = false;
            detail_used_.fill( false );
        }
        void append( const slow_turn_candidate &sample ) {
            window_[next_] = { sample.first, sample.simulation_seconds,
                               sample.start_wall_seconds, sample.end_wall_seconds };
            if( count_ < limit_ ) { ++count_; }
            next_ = ( next_ + 1 ) % limit_;
            const auto oldest = count_ == limit_ ? window_[next_].identity.turn_id :
                                window_[0].identity.turn_id;
            std::size_t free = detail_.size();
            std::size_t least = detail_.size();
            for( std::size_t i = 0; i < detail_.size(); ++i ) {
                if( detail_used_[i] && detail_[i].dominant.turn_id < oldest ) {
                    detail_used_[i] = false;
                }
                if( !detail_used_[i] ) {
                    if( free == detail_.size() ) { free = i; }
                } else if( least == detail_.size() ||
                           detail_[i].simulation_seconds < detail_[least].simulation_seconds ) {
                    least = i;
                }
            }
            if( free == detail_.size() && sample.simulation_seconds > detail_[least].simulation_seconds ) {
                free = least;
            }
            if( free != detail_.size() ) {
                detail_[free] = sample;
                detail_used_[free] = true;
            }
        }
        double window_mean() const {
            long double sum = 0;
            for( std::size_t i = 0; i < count_; ++i ) {
                sum += window_[i].simulation_seconds;
            }
            return static_cast<double>( sum / count_ );
        }
        void emit_window( const slow_turn_candidate &sample, std::uint8_t kind, double threshold ) {
            const compact_turn *worst = &window_[next_];
            for( std::size_t i = 0; i < limit_; ++i ) {
                const compact_turn &candidate = window_[( next_ + i ) % limit_];
                if( candidate.simulation_seconds > worst->simulation_seconds ) {
                    worst = &candidate;
                }
            }
            slow_turn_candidate alarm;
            alarm.details_available = false;
            alarm.dominant_stage = 65535;
            alarm.stage_ids.fill( 65535 );
            for( std::size_t i = 0; i < detail_.size(); ++i ) {
                if( detail_used_[i] && detail_[i].dominant.turn_id == worst->identity.turn_id ) {
                    alarm = detail_[i];
                    break;
                }
            }
            alarm.alarm_kind = kind;
            alarm.metric_epoch = sample.metric_epoch;
            alarm.threshold_seconds = threshold;
            alarm.first = window_[next_].identity;
            alarm.last = sample.last;
            alarm.dominant = worst->identity;
            alarm.start_wall_seconds = window_[next_].start_wall_seconds;
            alarm.end_wall_seconds = sample.end_wall_seconds;
            alarm.sample_count = static_cast<std::uint16_t>( limit_ );
            alarm.simulation_seconds = window_mean();
            alarm.dominant_simulation_seconds = worst->simulation_seconds;
            receipts_.add( alarm );
        }
        native_slow_turn_receipts receipts_;
        std::array<compact_turn, 256> window_{};
        std::array<slow_turn_candidate, 8> detail_{};
        std::array<bool, 8> detail_used_{};
        std::size_t count_ = 0;
        std::size_t next_ = 0;
        std::size_t limit_ = 0;
        std::uint64_t waiting_generation_ = 0;
        std::optional<std::uint64_t> last_turn_;
        double threshold_ = 0;
        std::uint8_t mode_ = 0;
        bool waiting_alarmed_ = false;
};

static_assert( sizeof( native_slow_turn_detector ) <= 32768,
               "slow-turn detector state must remain bounded" );
