#include "zombie_rider_overmap_ai.h"

#include <algorithm>
#include <cstdlib>

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

int omt_distance( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs )
{
    return std::max( std::abs( lhs.x() - rhs.x() ), std::abs( lhs.y() - rhs.y() ) );
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

rider_convergence_result evaluate_rider_convergence(
    const rider_light_memory &memory,
    const tripoint_abs_omt &light_omt,
    const std::vector<rider_overmap_agent> &riders )
{
    rider_convergence_result result;
    result.cap = std::min( memory.max_riders_drawn, max_riders_drawn_by_light );

    if( !memory.active() ) {
        result.reason = "light_memory_inactive";
        result.notes.push_back( "no rider convergence: light interest is inactive or decayed" );
        return result;
    }

    if( result.cap <= 0 ) {
        result.reason = "zero_rider_cap";
        result.notes.push_back( "no rider convergence: active light memory has no draw budget" );
        return result;
    }

    std::vector<rider_overmap_agent> candidates;
    for( const rider_overmap_agent &rider : riders ) {
        if( !rider.available || rider.already_in_band || rider.cooldown_turns > 0 ) {
            continue;
        }
        if( omt_distance( rider.pos, light_omt ) > rider_convergence_response_radius_omt ) {
            continue;
        }
        candidates.push_back( rider );
    }

    if( candidates.empty() ) {
        result.reason = "no_eligible_riders_in_response_radius";
        result.notes.push_back( "no rider convergence: no loose mature riders can answer the light" );
        return result;
    }

    const auto by_light_distance_then_id = [&light_omt]( const rider_overmap_agent & lhs,
    const rider_overmap_agent & rhs ) {
        const int lhs_distance = omt_distance( lhs.pos, light_omt );
        const int rhs_distance = omt_distance( rhs.pos, light_omt );
        if( lhs_distance != rhs_distance ) {
            return lhs_distance < rhs_distance;
        }
        return lhs.rider_id < rhs.rider_id;
    };
    std::sort( candidates.begin(), candidates.end(), by_light_distance_then_id );

    const int selected = std::min<int>( result.cap, candidates.size() );
    for( int index = 0; index < selected; ++index ) {
        result.rider_ids.push_back( candidates[index].rider_id );
    }
    result.selected_riders = selected;
    result.should_converge = selected > 0;
    result.band_formed = selected >= rider_band_minimum_size;
    result.band_size = result.band_formed ? selected : 0;
    result.posture = result.band_formed ? "rider_band_harass" : "lone_rider_harass";
    result.reason = result.band_formed ? "riders_converged_to_light_band" :
                    "rider_converges_to_light_interest";
    result.notes.push_back( "selected_riders=" + std::to_string( selected ) +
                            ", cap=" + std::to_string( result.cap ) +
                            ", available_candidates=" + std::to_string( candidates.size() ) );
    result.notes.push_back(
        "convergence uses temporary light memory and the rider draw cap; no permanent horde magic" );
    return result;
}

rider_camp_pressure_result choose_camp_pressure_posture(
    const rider_camp_pressure_input &input )
{
    rider_camp_pressure_result result;

    if( input.rider_wounded ) {
        result.posture = rider_camp_pressure_posture::withdraw;
        result.reason = "wounded_rider_disengages";
        result.notes.push_back( "wounded riders preserve local disengagement instead of final-shot spam" );
        return result;
    }

    if( !input.light_memory_active || input.rider_count <= 0 ) {
        result.posture = rider_camp_pressure_posture::none;
        result.reason = "no_active_light_pressure";
        return result;
    }

    if( input.breach_or_opening && input.rider_count >= std::max( 1, input.defender_strength ) ) {
        result.posture = rider_camp_pressure_posture::direct_attack;
        result.reason = "breach_or_opening_with_advantage";
        result.notes.push_back( "direct attack requires an opening or advantage, not blind wall charge" );
        return result;
    }

    if( input.band_formed || input.rider_count >= rider_band_minimum_size ) {
        result.posture = rider_camp_pressure_posture::circle_harass;
        result.reason = "band_without_breach_circles_and_harasses";
        result.notes.push_back(
            "rider band holds mounted pressure instead of suiciding into defended walls" );
        return result;
    }

    result.posture = rider_camp_pressure_posture::investigate;
    result.reason = "lone_rider_investigates_light";
    return result;
}

std::string to_string( rider_camp_pressure_posture posture )
{
    switch( posture ) {
        case rider_camp_pressure_posture::none:
            return "none";
        case rider_camp_pressure_posture::investigate:
            return "investigate";
        case rider_camp_pressure_posture::circle_harass:
            return "circle_harass";
        case rider_camp_pressure_posture::direct_attack:
            return "direct_attack";
        case rider_camp_pressure_posture::withdraw:
            return "withdraw";
    }
    return "none";
}

} // namespace zombie_rider_overmap_ai
