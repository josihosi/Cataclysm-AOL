#include "flesh_raptor_ai.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace flesh_raptor
{

namespace
{

constexpr int minimum_orbit_score = 78;
constexpr int swoop_cadence_phase = 0;

int squared_length( const int x, const int y )
{
    return x * x + y * y;
}

int vector_scale( const int numerator, const int denominator, const int scale )
{
    if( denominator <= 0 ) {
        return 0;
    }
    return numerator * scale / denominator;
}

} // namespace

int score_orbit_candidate( const orbit_context &ctx, const orbit_candidate &candidate )
{
    const int current_len_sq = squared_length( ctx.current_rel_x, ctx.current_rel_y );
    const int candidate_len_sq = squared_length( candidate.rel_x, candidate.rel_y );
    const int vector_denominator = static_cast<int>( std::sqrt( current_len_sq * candidate_len_sq ) );
    const int dot = ctx.current_rel_x * candidate.rel_x + ctx.current_rel_y * candidate.rel_y;
    const int cross = ctx.current_rel_x * candidate.rel_y - ctx.current_rel_y * candidate.rel_x;

    int score = 100;
    score -= std::abs( candidate.distance_to_target - 5 ) * 12;
    score -= candidate.distance_from_raptor * 2;
    score -= candidate.crowding * 35;
    score += vector_scale( std::abs( cross ), vector_denominator, 36 );

    if( dot > 0 ) {
        score -= vector_scale( dot, vector_denominator, 24 );
    }
    if( candidate.held ) {
        score += 48;
    }
    if( ctx.retreating ) {
        score += candidate.distance_to_target >= 4 ? 12 : -20;
    }

    return score;
}

orbit_response choose_orbit_destination( const orbit_context &ctx,
        const std::vector<orbit_candidate> &candidates )
{
    orbit_response response;

    if( !ctx.retreating && ctx.distance_to_target >= 4 && ctx.distance_to_target <= 6 &&
        ctx.cadence_phase == swoop_cadence_phase ) {
        response.next = decision::swoop;
        response.reason = "cadence_swoop_window";
        return response;
    }

    if( candidates.empty() ) {
        response.next = decision::fallback;
        response.reason = "no_orbit_candidates";
        return response;
    }

    int best_score = -100000;
    const orbit_candidate *best = nullptr;
    for( const orbit_candidate &candidate : candidates ) {
        const int score = score_orbit_candidate( ctx, candidate );
        if( best == nullptr || score > best_score ) {
            best_score = score;
            best = &candidate;
        }
    }

    response.score = best_score;
    response.chosen = *best;
    response.has_candidate = true;

    if( best_score < minimum_orbit_score ) {
        response.next = decision::fallback;
        response.reason = "no_readable_lateral_orbit";
        return response;
    }

    response.next = decision::orbit;
    response.reason = best->held ? "held_orbit_intention" : "best_open_orbit_arc";
    return response;
}

} // namespace flesh_raptor
