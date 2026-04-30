#include "zombie_rider_overmap_ai.h"

#include <algorithm>

namespace zombie_rider_overmap_ai
{
namespace
{
void clear_memory( rider_light_memory &memory, const std::string &reason )
{
    memory.interest_score = 0;
    memory.turns_remaining = 0;
    memory.max_riders_drawn = 0;
    memory.reason = reason;
}
} // namespace

rider_light_interest evaluate_light_attraction(
    const bandit_mark_generation::light_projection &projection,
    int world_age_days,
    int eligible_riders_nearby )
{
    rider_light_interest interest;
    interest.notes.push_back( projection.review_summary );

    if( world_age_days < mature_world_gate_days ) {
        interest.reason = "early_world_gate";
        interest.notes.push_back( "zombie rider light interest suppressed before mature-world gate" );
        return interest;
    }

    if( eligible_riders_nearby <= 0 ) {
        interest.reason = "no_riders_available";
        interest.notes.push_back( "no eligible late-game riders are available to investigate" );
        return interest;
    }

    if( !projection.viable ) {
        interest.reason = "no_viable_light_signal";
        return interest;
    }

    const int horde_signal_power = bandit_mark_generation::horde_signal_power_from_light_projection(
                                       projection );
    if( horde_signal_power <= 0 ) {
        interest.reason = "below_rider_light_threshold";
        interest.notes.push_back(
            "light is visible enough for local review but not exposed/large enough to call endpoint cavalry" );
        return interest;
    }

    interest.should_investigate = true;
    interest.reason = projection.concealment.elevated_exposure_extended ?
                      "elevated_bright_light" : "exposed_bright_light";
    interest.interest_score = std::clamp( horde_signal_power / 10 + projection.visibility_score / 4, 1, 6 );
    interest.memory_turns = std::clamp( 60 + horde_signal_power * 4, 90, 300 );
    const int desired_riders = interest.interest_score >= 5 ? max_riders_drawn_by_light : 1;
    interest.max_riders_drawn = std::min( eligible_riders_nearby, desired_riders );
    interest.notes.push_back( "rider light interest: horde_signal_power=" +
                              std::to_string( horde_signal_power ) +
                              ", score=" + std::to_string( interest.interest_score ) +
                              ", memory_turns=" + std::to_string( interest.memory_turns ) +
                              ", max_riders_drawn=" + std::to_string( interest.max_riders_drawn ) );
    interest.notes.push_back(
        "bounded investigation only: light creates temporary pressure, not permanent camp doom" );
    return interest;
}

void refresh_light_memory( rider_light_memory &memory, const rider_light_interest &interest )
{
    if( !interest.should_investigate || interest.interest_score <= 0 || interest.memory_turns <= 0 ||
        interest.max_riders_drawn <= 0 ) {
        return;
    }

    memory.interest_score = std::max( memory.interest_score, interest.interest_score );
    memory.turns_remaining = std::max( memory.turns_remaining, interest.memory_turns );
    memory.max_riders_drawn = std::min( max_riders_drawn_by_light,
                                        std::max( memory.max_riders_drawn, interest.max_riders_drawn ) );
    memory.reason = interest.reason;
}

void advance_light_memory( rider_light_memory &memory, int elapsed_turns )
{
    if( elapsed_turns <= 0 || !memory.active() ) {
        return;
    }

    if( elapsed_turns >= memory.turns_remaining ) {
        clear_memory( memory, "decayed_after_light_off" );
        return;
    }

    memory.turns_remaining -= elapsed_turns;
    const int decay_steps = elapsed_turns / 60;
    if( decay_steps > 0 ) {
        memory.interest_score = std::max( 0, memory.interest_score - decay_steps );
    }
    if( memory.interest_score == 0 ) {
        clear_memory( memory, "decayed_after_light_off" );
    }
}

} // namespace zombie_rider_overmap_ai
