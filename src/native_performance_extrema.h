#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <memory>

struct raid_perf_row;

// Per-native-process, fixed-size, non-overlapping 100-turn extrema. Equal
// maxima keep the first occurrence; incomplete windows are never promoted.
struct native_performance_sample {
    std::uint64_t turn_id = 0;
    std::uint64_t start_sequence = 0;
    std::uint64_t end_sequence = 0;
    int game_turn = 0;
    double simulation_seconds = 0;
    double start_wall_seconds = 0;
    double end_wall_seconds = 0;
    std::optional<double> process_cpu_seconds;
    std::optional<double> input_seconds;
    std::optional<double> save_seconds;
    std::optional<std::array<double, 4>> stages_seconds;
    std::shared_ptr<const raid_perf_row> detailed_row;
};

struct native_performance_window {
    native_performance_sample first;
    native_performance_sample last;
    native_performance_sample dominant;
    double mean_simulation_seconds_per_turn = 0;
    std::optional<double> total_process_cpu_seconds;
    std::optional<double> total_input_seconds;
    std::optional<double> total_save_seconds;
};

class native_performance_extrema
{
    public:
        // Capture the current row only when it can become an individual or
        // partial-window maximum. Ties retain the first observed turn.
        bool needs_detail( const native_performance_sample &sample ) const {
            if( last_game_turn_ && sample.game_turn < *last_game_turn_ ) {
                return true;
            }
            if( !individual_ || sample.simulation_seconds > individual_->simulation_seconds ) {
                return true;
            }
            if( count_ == 0 || sample.turn_id != partial_[count_ - 1].turn_id + 1 ) {
                return true;
            }
            for( std::size_t i = 0; i < count_; ++i ) {
                if( sample.simulation_seconds <= partial_[i].simulation_seconds ) {
                    return false;
                }
            }
            return true;
        }
        bool add( const native_performance_sample &sample ) {
            bool changed = false;
            if( last_game_turn_ && sample.game_turn < *last_game_turn_ ) {
                individual_.reset();
                window_.reset();
                count_ = 0;
                ++epoch_;
            }
            last_game_turn_ = sample.game_turn;
            if( !individual_ || sample.simulation_seconds > individual_->simulation_seconds ) {
                individual_ = sample;
                changed = true;
            }
            if( count_ && sample.turn_id != partial_[count_ - 1].turn_id + 1 ) {
                count_ = 0; // a gap or reset cannot become a complete window
            }
            partial_[count_++] = sample;
            if( count_ == partial_.size() ) {
                native_performance_window next;
                next.first = partial_.front();
                next.last = partial_.back();
                next.dominant = partial_.front();
                double sim = 0, cpu = 0, input = 0, save = 0;
                bool all_cpu = true, all_input = true, all_save = true;
                for( const auto &row : partial_ ) {
                    sim += row.simulation_seconds;
                    if( row.simulation_seconds > next.dominant.simulation_seconds ) {
                        next.dominant = row;
                    }
                    if( row.process_cpu_seconds ) { cpu += *row.process_cpu_seconds; } else { all_cpu = false; }
                    if( row.input_seconds ) { input += *row.input_seconds; } else { all_input = false; }
                    if( row.save_seconds ) { save += *row.save_seconds; } else { all_save = false; }
                }
                next.mean_simulation_seconds_per_turn = sim / partial_.size();
                if( all_cpu ) { next.total_process_cpu_seconds = cpu; }
                if( all_input ) { next.total_input_seconds = input; }
                if( all_save ) { next.total_save_seconds = save; }
                if( !window_ || next.mean_simulation_seconds_per_turn > window_->mean_simulation_seconds_per_turn ) {
                    window_ = next;
                    changed = true;
                }
                count_ = 0;
            }
            return changed;
        }
        std::uint64_t epoch() const { return epoch_; }
        const std::optional<native_performance_sample> &individual() const { return individual_; }
        const std::optional<native_performance_window> &window() const { return window_; }
    private:
        std::array<native_performance_sample, 100> partial_{};
        std::size_t count_ = 0;
        std::uint64_t epoch_ = 1;
        std::optional<int> last_game_turn_;
        std::optional<native_performance_sample> individual_;
        std::optional<native_performance_window> window_;
};
