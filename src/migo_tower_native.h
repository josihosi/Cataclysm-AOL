#pragma once
#ifndef CATA_SRC_MIGO_TOWER_NATIVE_H
#define CATA_SRC_MIGO_TOWER_NATIVE_H

#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <string>

#include "migo_population.h"
#include "physical_navigation.h"

struct live_bandit_signal_observation;
class monster;
class Creature;
class JsonOut;

namespace migo_tower
{
struct receiver_result {
    int actual_sites = 0;
    int committed_populations = 0;
    int eligible_observers = 0;
    int received = 0;
};

// Optional value sink for one real receiver scan. Storage is allocated only
// after the cadence gate; it retains values, never native object pointers.
class receiver_observation
{
        struct implementation;
        std::unique_ptr<implementation> data_;
        bool entered_ = false;
        friend receiver_result receive_signals(
            const std::vector<live_bandit_signal_observation> &, int, receiver_observation * );
    public:
        receiver_observation();
        ~receiver_observation();
        receiver_observation( receiver_observation && ) noexcept;
        receiver_observation &operator=( receiver_observation && ) noexcept;
        bool entered() const;
        void serialize( JsonOut &js ) const;
};

// Production recipient of the shared, current physical emitter packet. It
// discovers existing site content before selecting a real home observer. This
// is independent of every human scout/report/dispatch prerequisite.
receiver_result receive_signals( const std::vector<live_bandit_signal_observation> &samples,
                                 int now_turn, receiver_observation *observation = nullptr );
// Revalidate the received evidence and exact current bodies before committing
// all selected population leases. A decision estimate alone cannot dispatch.
bool begin_response( const std::string &site_id, int now_turn );
// Native movement ownership includes an individually authenticated home recall;
// assault membership and its witness/encounter policies remain separate.
bool owns_intent( const monster &body, migo_population::representation owner );
// Shared geographic interval and native payload pause boundary. Calls never
// grant movement; the existing native process/energy guards still own credit.
void refresh_activation( int now_turn );
enum class activity_boundary { process, load, unload };
bool activity_for( monster &body, migo_population::representation owner, int now_turn,
                   activity_boundary boundary = activity_boundary::process,
                   int *active_turns = nullptr );
struct coarse_step_read {
    // Only an authenticated local phase may invoke physical navigation.
    // Missing state or a rejected distant route remains a held distant result.
    bool local_handoff = false;
    bool funded = false;
    // A distant transit handoff stops at the shared-selected next OMT.
    std::optional<tripoint_abs_ms> local_waypoint;
    physical_navigation::step_read step;
};
// A horde batch may temporarily hold an authenticated peer in its rollback
// source while that peer's destination is undecided. The resolver is used
// synchronously and never transfers or retains ownership of the body.
using pending_source_reader = std::function<const monster *( const migo_population::identity &,
                              const tripoint_abs_ms & )>;
coarse_step_read prepare_coarse_step( monster &body, int active_turns, int current_moves,
                                      const pending_source_reader &pending_source = {} );
physical_navigation::step_read prepare_step( monster &body,
        migo_population::representation owner,
        const std::optional<tripoint_abs_ms> &transit_waypoint = std::nullopt );
// Diagnostic-only bounded value samples, published after a measured interval.
// These never retain actor/map pointers or influence movement.
void serialize_prepare_step_samples( JsonOut &json );
void clear_prepare_step_samples();
bool plan_local( monster &body, bool fleeing, Creature *target );
void record_combat_action( monster &body );
// Called after native movement effects and immediate special/escape handling.
bool move_local( monster &body );
void service_operations( int now_turn, int active_elapsed = 1 );
void witness_death( const monster &body );
} // namespace migo_tower
#endif
