#include "live_light.h"

#include "physical_light.h"

namespace live_light
{
namespace
{
std::vector<live_bandit_signal_observation> sample_cache;
time_point sample_turn = calendar::turn_zero;
bool sample_valid = false;
physical_light::turn_sample_gate sample_gate;
std::vector<live_light_delivery_stage> trace;

void note( const live_light_delivery_stage stage )
{
    trace.push_back( stage );
}
}

void reset()
{
    sample_cache.clear();
    sample_turn = calendar::turn_zero;
    sample_valid = false;
    sample_gate = physical_light::turn_sample_gate{};
    trace.clear();
}

void run_advancing_turn( const time_point turn, const callbacks &callbacks )
{
    trace.clear();
    const int turn_number = to_turns<int>( turn - calendar::turn_zero );
    // The advancing-turn owner is the only caller allowed to reconcile,
    // age, sample or deliver for a turn.  Same-turn re-entry retains the
    // immutable packet for later staffed access but cannot replay work.
    if( !sample_gate.begin_advancing_turn( turn_number, true ) ) {
        return;
    }
    note( live_light_delivery_stage::rider_reconciliation );
    callbacks.reconcile_riders();
    note( live_light_delivery_stage::rider_memory_aging );
    callbacks.age_rider_memory();
    note( live_light_delivery_stage::fresh_sampling );
    sample_cache = callbacks.discover();
    sample_turn = turn;
    sample_valid = true;
    if( !sample_valid || sample_turn != turn ) {
        return;
    }
    std::vector<live_bandit_signal_observation> light_samples;
    for( const live_bandit_signal_observation &sample : sample_cache ) {
        if( sample.has_light_projection && !sample.sample_id.empty() ) {
            light_samples.push_back( sample );
        }
    }
    note( live_light_delivery_stage::horde_delivery );
    if( !light_samples.empty() ) {
        callbacks.deliver_hordes( light_samples );
    }
    note( live_light_delivery_stage::stalker_delivery );
    if( !light_samples.empty() ) {
        callbacks.deliver_stalkers( light_samples );
    }
    note( live_light_delivery_stage::rider_delivery );
    if( !light_samples.empty() ) {
        callbacks.deliver_riders( light_samples );
    }
    note( live_light_delivery_stage::cannibal_delivery );
    if( !light_samples.empty() ) {
        callbacks.deliver_cannibals( light_samples );
    }
}

const std::vector<live_bandit_signal_observation> &samples_for_turn( const time_point turn )
{
    static const std::vector<live_bandit_signal_observation> empty;
    return sample_valid && sample_turn == turn ? sample_cache : empty;
}

void record_staffed_observer( const time_point turn,
                              const std::function<void( const std::vector<live_bandit_signal_observation> & )> &record )
{
    note( live_light_delivery_stage::staffed_observer_recording );
    record( samples_for_turn( turn ) );
}

bool sample_is_current( const time_point turn )
{
    return sample_valid && sample_turn == turn;
}

std::vector<live_light_delivery_stage> delivery_order()
{
    return { live_light_delivery_stage::rider_reconciliation,
             live_light_delivery_stage::rider_memory_aging,
             live_light_delivery_stage::fresh_sampling,
             live_light_delivery_stage::horde_delivery,
             live_light_delivery_stage::stalker_delivery,
             live_light_delivery_stage::rider_delivery,
             live_light_delivery_stage::cannibal_delivery,
             live_light_delivery_stage::staffed_observer_recording };
}

std::vector<live_light_delivery_stage> delivery_trace()
{
    return trace;
}
}
