#include "writhing_stalker_ai.h"

#include <algorithm>
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

} // namespace writhing_stalker
