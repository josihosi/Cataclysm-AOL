#include "writhing_stalker_ai.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <utility>

namespace writhing_stalker
{

namespace
{

constexpr int fresh_human_evidence_minutes = 20;
constexpr int stale_human_evidence_minutes = 45;
constexpr int latch_timeout_minutes = 30;
constexpr int default_latch_leash_tiles = 60;
constexpr int withdrawal_cooldown_minutes = 10;
constexpr int faded_latch_cooldown_minutes = 5;

int signum( const int value )
{
    if( value < 0 ) {
        return -1;
    }
    if( value > 0 ) {
        return 1;
    }
    return 0;
}

void keep_strongest( interest_report &best, interest_source source, int score, int confidence,
                     bool can_latch, std::string reason )
{
    if( score > best.score ) {
        best.source = source;
        best.score = score;
        best.confidence = confidence;
        best.can_latch = can_latch;
        best.reason = std::move( reason );
    }
}

} // namespace

interest_report evaluate_interest( const interest_context &ctx )
{
    interest_report best;
    best.reason = "empty_terrain";

    if( ctx.recent_human_evidence ) {
        if( ctx.evidence_age_minutes <= fresh_human_evidence_minutes ) {
            keep_strongest( best, interest_source::human, 90, 4, true,
                            "recent_human_evidence" );
        } else if( ctx.evidence_age_minutes <= stale_human_evidence_minutes ) {
            keep_strongest( best, interest_source::human, 42, 2, true,
                            "fading_human_evidence" );
        } else {
            keep_strongest( best, interest_source::human, 8, 1, false,
                            "stale_human_evidence" );
        }
    }

    if( ctx.exposed_night_light ) {
        keep_strongest( best, interest_source::light, 60, 3, false,
                        "exposed_night_light" );
    }

    if( ctx.forest_or_building_edge ) {
        keep_strongest( best, interest_source::terrain, 34, 2, false,
                        "forest_or_building_edge_cover" );
    } else if( ctx.town_or_road_edge ) {
        keep_strongest( best, interest_source::terrain, 24, 1, false,
                        "town_or_road_edge_scent" );
    }

    if( ctx.zombie_pressure > 0 ) {
        keep_strongest( best, interest_source::zombie_pressure,
                        std::min( 36, 10 + ctx.zombie_pressure * 6 ), 2, false,
                        "zombie_pressure_opportunity" );
    }

    if( ctx.smoke ) {
        keep_strongest( best, interest_source::smoke, 14, 1, false,
                        "smoke_indirect_human_hint" );
    }

    return best;
}

latch_update advance_latch( const latch_context &ctx )
{
    latch_update result;
    result.state = ctx.current;
    result.state.cooldown_minutes = std::max( 0, result.state.cooldown_minutes -
            ctx.elapsed_minutes );

    if( result.state.active ) {
        result.state.age_minutes += std::max( 0, ctx.elapsed_minutes );

        if( ctx.exposed_or_focused ) {
            result.state.active = false;
            result.state.cooldown_minutes = withdrawal_cooldown_minutes;
            result.next = decision::cooling_off;
            result.reason = "latch_broken_by_exposure_or_focus";
            return result;
        }

        if( result.state.age_minutes > latch_timeout_minutes ) {
            result.state.active = false;
            result.state.cooldown_minutes = faded_latch_cooldown_minutes;
            result.next = decision::cooling_off;
            result.reason = "latch_timed_out";
            return result;
        }

        if( ctx.distance_tiles > 0 && ctx.distance_tiles > result.state.leash_tiles_remaining ) {
            result.state.active = false;
            result.state.cooldown_minutes = faded_latch_cooldown_minutes;
            result.next = decision::cooling_off;
            result.reason = "latch_leash_broken";
            return result;
        }
    }

    if( ctx.interest.can_latch && result.state.cooldown_minutes == 0 ) {
        result.state.active = true;
        result.state.age_minutes = 0;
        result.state.confidence = std::max( result.state.confidence, ctx.interest.confidence );
        result.state.leash_tiles_remaining = default_latch_leash_tiles;
        result.next = decision::shadow;
        result.reason = ctx.current.active ? "latch_refreshed_by_new_evidence" :
                        "latch_created_by_believable_evidence";
        return result;
    }

    if( result.state.active ) {
        result.next = decision::shadow;
        result.reason = "latch_continues_without_refresh";
    } else if( result.state.cooldown_minutes > 0 ) {
        result.next = decision::cooling_off;
        result.reason = "cooldown_blocks_relatched_pressure";
    } else {
        result.next = ctx.interest.score > 0 ? decision::interested : decision::ignore;
        result.reason = ctx.interest.score > 0 ? "interest_without_latch" : "no_believable_evidence";
    }

    return result;
}

approach_report choose_approach( const approach_context &ctx )
{
    approach_report report;

    if( !ctx.latch.active ) {
        report.reason = "no_latch_no_approach";
        return report;
    }

    if( ctx.bright_exposure ) {
        report.route = approach_class::hold_exposed;
        report.next = decision::hold;
        report.avoids_direct_line = true;
        report.reason = "bright_exposure_holds_shadow";
        return report;
    }

    if( ctx.cover_route_available ) {
        report.route = approach_class::cover_shadow;
        report.next = decision::shadow;
        report.avoids_direct_line = true;
        report.reason = "cover_route_preferred";
        return report;
    }

    if( ctx.edge_route_available ) {
        report.route = approach_class::edge_shadow;
        report.next = decision::shadow;
        report.avoids_direct_line = true;
        report.reason = "edge_route_preferred";
        return report;
    }

    if( ctx.direct_open_route_available && ctx.forced_no_cover ) {
        report.route = approach_class::direct_forced;
        report.next = decision::shadow;
        report.avoids_direct_line = false;
        report.reason = "direct_route_only_when_forced_by_map";
        return report;
    }

    report.route = approach_class::hold_exposed;
    report.next = decision::hold;
    report.avoids_direct_line = true;
    report.reason = "no_safe_shadow_route";
    return report;
}

opportunity_report evaluate_opportunity( const opportunity_context &ctx )
{
    opportunity_report report;

    if( !ctx.latch.active ) {
        report.next = decision::ignore;
        report.reason = "no_latch_no_strike";
        return report;
    }

    if( ctx.latch.cooldown_minutes > 0 ) {
        report.next = decision::cooling_off;
        report.reason = "cooldown_blocks_repeat_strike";
        return report;
    }

    if( ctx.player_bleeding ) {
        report.vulnerability += 22;
    }
    if( ctx.player_hurt ) {
        report.vulnerability += 18;
    }
    if( ctx.player_low_stamina ) {
        report.vulnerability += 16;
    }
    if( ctx.player_distracted ) {
        report.vulnerability += 18;
    }
    if( ctx.player_noisy ) {
        report.vulnerability += 10;
    }

    report.zombie_distraction = std::min( 28, ctx.zombie_pressure * 7 );

    if( ctx.bright_exposure ) {
        report.exposure_penalty += 35;
    }
    if( ctx.player_focused ) {
        report.exposure_penalty += 25;
    }
    if( ctx.stalker_hurt ) {
        report.exposure_penalty += 45;
    }

    report.opportunity = 18 + report.vulnerability + report.zombie_distraction -
                         report.exposure_penalty;
    if( ctx.near_cover_or_clutter ) {
        report.opportunity += 12;
    }

    if( ctx.stalker_hurt || ( ctx.bright_exposure && ctx.player_focused ) ) {
        report.next = decision::withdraw;
        report.reason = ctx.stalker_hurt ? "stalker_hurt_withdraw" :
                        "exposed_and_focused_withdraw";
    } else if( report.opportunity >= 70 ) {
        report.next = decision::strike;
        report.reason = "vulnerability_window_strike";
    } else if( report.opportunity >= 36 ) {
        report.next = decision::shadow;
        report.reason = "opportunity_building_shadow";
    } else {
        report.next = decision::hold;
        report.reason = "alert_or_exposed_hold";
    }

    return report;
}

quiet_side_report evaluate_quiet_side( const std::vector<relative_point> &zombies )
{
    quiet_side_report report;

    for( const relative_point &zombie : zombies ) {
        const int weight = std::max( 1, zombie.weight );
        report.pressure_x += signum( zombie.rel_x ) * weight;
        report.pressure_y += signum( zombie.rel_y ) * weight;
        report.pressure_count += weight;
    }

    const int abs_x = std::abs( report.pressure_x );
    const int abs_y = std::abs( report.pressure_y );
    const int strength = abs_x + abs_y;
    report.has_dominant_pressure = report.pressure_count > 0 && strength >=
                                   std::max( 1, report.pressure_count / 2 );
    report.ambiguous_pressure = report.pressure_count > 1 && !report.has_dominant_pressure;

    if( report.has_dominant_pressure ) {
        if( abs_x >= abs_y ) {
            report.quiet_x = -signum( report.pressure_x );
        } else {
            report.quiet_y = -signum( report.pressure_y );
        }
    }

    return report;
}

quiet_candidate_report choose_quiet_side_cutoff( const std::vector<relative_point> &zombies,
        const std::vector<quiet_candidate> &candidates )
{
    quiet_candidate_report report;
    report.pressure = evaluate_quiet_side( zombies );
    report.score = INT_MIN;
    report.reason = "no_candidate";

    for( const quiet_candidate &candidate : candidates ) {
        if( !candidate.passable || candidate.occupied ) {
            continue;
        }

        int score = 20 - candidate.distance_to_stalker;
        if( candidate.shadow_or_cover ) {
            score += 18;
        }
        if( candidate.broken_line_of_sight ) {
            score += 14;
        }
        if( !candidate.bright_exposure ) {
            score += 8;
        }
        score += std::max( -2, std::min( 4, candidate.retreat_alignment ) ) * 4;

        int quiet_alignment = 0;
        int crowding_penalty = 0;
        if( report.pressure.has_dominant_pressure ) {
            quiet_alignment = signum( candidate.rel_x ) * report.pressure.quiet_x +
                              signum( candidate.rel_y ) * report.pressure.quiet_y;
            const int pressure_alignment = signum( candidate.rel_x ) * signum( report.pressure.pressure_x ) +
                                           signum( candidate.rel_y ) * signum( report.pressure.pressure_y );
            if( quiet_alignment > 0 ) {
                score += quiet_alignment * 12;
            } else {
                score -= 10;
            }
            if( pressure_alignment > 0 ) {
                crowding_penalty = pressure_alignment * 8;
                score -= crowding_penalty;
            }
        }

        if( !report.has_candidate || score > report.score ) {
            report.has_candidate = true;
            report.chosen = candidate;
            report.score = score;
            report.quiet_alignment = quiet_alignment;
            report.crowding_penalty = crowding_penalty;
            if( report.pressure.ambiguous_pressure ) {
                report.reason = "ambiguous_pressure_no_precise_quiet_side";
            } else if( quiet_alignment > 0 ) {
                report.reason = "quiet_side_cutoff_preferred";
            } else {
                report.reason = "best_shadow_without_quiet_side";
            }
        }
    }

    return report;
}

confidence_report evaluate_confidence( const confidence_context &ctx )
{
    confidence_report report;
    report.evidence = ctx.has_believable_local_evidence ? 36 : 0;
    report.interest = ctx.has_overmap_interest_footing ? 24 : 0;
    report.pressure_allowed = report.evidence > 0 || report.interest > 0;
    if( report.pressure_allowed ) {
        report.zombie_pressure = std::min( 28, ctx.zombie_pressure * 7 );
    }

    if( ctx.target_in_bright_exposure ) {
        report.counterpressure += 25;
    }
    if( ctx.stalker_in_bright_exposure ) {
        report.counterpressure += 25;
    }
    if( ctx.target_has_focus ) {
        report.counterpressure += 25;
    }
    if( ctx.open_exposure ) {
        report.counterpressure += 15;
    }
    if( ctx.stalker_hurt ) {
        report.counterpressure += 45;
    }

    report.cutoff_allowed = report.pressure_allowed && ctx.quiet_side_cutoff_available &&
                            report.counterpressure == 0;
    if( report.cutoff_allowed ) {
        report.quiet_side_cutoff = 16;
    }

    report.total = report.evidence + report.interest + report.zombie_pressure +
                   report.quiet_side_cutoff - report.counterpressure;
    if( !report.pressure_allowed && ctx.zombie_pressure > 0 ) {
        report.reason = "no_evidence_or_interest_pressure_ignored";
    } else if( report.counterpressure > 0 ) {
        report.reason = "counterpressure_suppresses_cutoff";
    } else if( report.quiet_side_cutoff > 0 ) {
        report.reason = "zombie_shadow_quiet_cutoff_confidence";
    } else if( report.zombie_pressure > 0 ) {
        report.reason = "zombie_shadow_pressure_confidence";
    } else {
        report.reason = report.pressure_allowed ? "evidence_confidence" : "no_confidence";
    }

    return report;
}

live_response evaluate_live_response( const live_context &ctx )
{
    live_response response;

    if( !ctx.has_believable_local_evidence ) {
        response.next = decision::ignore;
        response.reason = "live_no_believable_evidence";
        return response;
    }

    interest_context interest_ctx;
    interest_ctx.recent_human_evidence = true;
    interest_ctx.evidence_age_minutes = ctx.evidence_age_minutes;
    interest_ctx.exposed_night_light = ctx.target_in_bright_exposure;
    interest_ctx.forest_or_building_edge = ctx.cover_route_available;
    interest_ctx.town_or_road_edge = ctx.edge_route_available;
    interest_ctx.zombie_pressure = ctx.zombie_pressure;

    latch_state live_latch;
    live_latch.cooldown_minutes = ctx.on_cooldown ? 1 : 0;
    latch_update latch = advance_latch( latch_context{ live_latch, evaluate_interest( interest_ctx ), 0,
            ctx.distance_to_target, ctx.stalker_in_bright_exposure && ctx.target_has_focus } );

    if( !latch.state.active ) {
        response.next = latch.next;
        response.reason = "live_latch_" + latch.reason;
        return response;
    }

    approach_context approach_ctx;
    approach_ctx.latch = latch.state;
    approach_ctx.cover_route_available = ctx.cover_route_available;
    approach_ctx.edge_route_available = ctx.edge_route_available;
    approach_ctx.direct_open_route_available = ctx.direct_open_route_available;
    approach_ctx.bright_exposure = ctx.stalker_in_bright_exposure;
    approach_ctx.forced_no_cover = ctx.forced_no_cover;
    const approach_report approach = choose_approach( approach_ctx );

    opportunity_context opportunity_ctx;
    opportunity_ctx.latch = latch.state;
    opportunity_ctx.player_bleeding = ctx.player_bleeding;
    opportunity_ctx.player_hurt = ctx.player_hurt;
    opportunity_ctx.player_low_stamina = ctx.player_low_stamina;
    opportunity_ctx.player_distracted = ctx.player_distracted;
    opportunity_ctx.player_noisy = ctx.player_noisy;
    opportunity_ctx.zombie_pressure = ctx.zombie_pressure;
    opportunity_ctx.near_cover_or_clutter = ctx.near_cover_or_clutter;
    opportunity_ctx.bright_exposure = ctx.stalker_in_bright_exposure || ctx.target_in_bright_exposure;
    opportunity_ctx.player_focused = ctx.target_has_focus;
    opportunity_ctx.stalker_hurt = ctx.stalker_hurt;
    const opportunity_report opportunity = evaluate_opportunity( opportunity_ctx );

    const confidence_report confidence = evaluate_confidence( confidence_context{
        ctx.has_believable_local_evidence, ctx.has_overmap_interest_footing, ctx.zombie_pressure,
        ctx.quiet_side_cutoff_available, ctx.target_in_bright_exposure, ctx.stalker_in_bright_exposure,
        ctx.target_has_focus, ctx.open_exposure, ctx.stalker_hurt } );

    response.route = approach.route;
    response.opportunity = opportunity.opportunity;
    response.confidence = confidence.total;

    if( opportunity.next == decision::withdraw || opportunity.next == decision::cooling_off ) {
        response.next = opportunity.next;
        response.reason = "live_" + opportunity.reason;
        return response;
    }

    if( opportunity.next == decision::strike ) {
        if( ctx.distance_to_target > 3 && approach.next == decision::shadow ) {
            response.next = decision::shadow;
            response.reason = "live_shadowing_before_strike_window";
            return response;
        }
        response.next = decision::strike;
        response.reason = "live_" + opportunity.reason;
        return response;
    }

    if( approach.next == decision::hold ) {
        response.next = decision::hold;
        response.reason = "live_" + approach.reason;
        return response;
    }

    if( ctx.distance_to_target <= 2 && opportunity.next == decision::hold ) {
        response.next = decision::hold;
        response.reason = "live_close_alert_target_hold";
        return response;
    }

    response.next = approach.next == decision::shadow ? decision::shadow : opportunity.next;
    response.reason = response.next == decision::shadow ? "live_shadowing_believable_evidence" :
                      "live_" + opportunity.reason;
    return response;
}

} // namespace writhing_stalker
