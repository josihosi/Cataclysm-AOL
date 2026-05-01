#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "calendar.h"
#include "cata_catch.h"
#include "character.h"
#include "damage.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "mongroup.h"
#include "mtype.h"
#include "type_id.h"
#include "writhing_stalker_ai.h"

static const damage_type_id damage_cut( "cut" );
static const mongroup_id GROUP_ZOMBIE( "GROUP_ZOMBIE" );
static const mtype_id mon_zombie( "mon_zombie" );
static const mtype_id mon_writhing_stalker( "mon_writhing_stalker" );
static const species_id species_HUMAN( "HUMAN" );
static const species_id species_ZOMBIE( "ZOMBIE" );

namespace
{

const char *decision_trace_name( const writhing_stalker::decision value )
{
    using writhing_stalker::decision;
    switch( value ) {
        case decision::ignore:
            return "ignore";
        case decision::interested:
            return "interested";
        case decision::shadow:
            return "shadow";
        case decision::hold:
            return "hold";
        case decision::strike:
            return "strike";
        case decision::withdraw:
            return "withdraw";
        case decision::cooling_off:
            return "cooling_off";
    }
    return "unknown";
}

const char *route_trace_name( const writhing_stalker::approach_class value )
{
    using writhing_stalker::approach_class;
    switch( value ) {
        case approach_class::none:
            return "none";
        case approach_class::cover_shadow:
            return "cover_shadow";
        case approach_class::edge_shadow:
            return "edge_shadow";
        case approach_class::direct_forced:
            return "direct_forced";
        case approach_class::hold_exposed:
            return "hold_exposed";
    }
    return "unknown";
}

struct stalker_pattern_row {
    int turn = 0;
    int distance = 0;
    int hp_percent = 100;
    int cooldown_turns = 0;
    int strike_count = 0;
    writhing_stalker::decision next = writhing_stalker::decision::ignore;
    writhing_stalker::approach_class route = writhing_stalker::approach_class::none;
    std::string reason;
};

std::string trace_rows( const std::vector<stalker_pattern_row> &rows )
{
    std::ostringstream out;
    for( const stalker_pattern_row &row : rows ) {
        out << 't' << row.turn << " dist=" << row.distance << " hp=" << row.hp_percent
            << " cd=" << row.cooldown_turns << " decision=" << decision_trace_name( row.next )
            << " route=" << route_trace_name( row.route ) << " strikes=" << row.strike_count
            << " reason=" << row.reason << '\n';
    }
    return out.str();
}

writhing_stalker::live_context pattern_base_context()
{
    writhing_stalker::live_context ctx;
    ctx.has_believable_local_evidence = true;
    ctx.cover_route_available = true;
    ctx.direct_open_route_available = true;
    ctx.near_cover_or_clutter = true;
    return ctx;
}

std::vector<stalker_pattern_row> run_vulnerable_stalker_pattern( const int turns )
{
    std::vector<stalker_pattern_row> rows;
    writhing_stalker::live_context ctx = pattern_base_context();
    ctx.player_bleeding = true;
    ctx.player_hurt = true;
    ctx.player_low_stamina = true;
    ctx.player_distracted = true;
    ctx.player_noisy = true;
    ctx.zombie_pressure = 2;

    int distance = 5;
    int cooldown_turns = 0;
    int hp_percent = 100;
    int strike_count = 0;

    for( int turn = 0; turn < turns; ++turn ) {
        ctx.distance_to_target = distance;
        ctx.on_cooldown = cooldown_turns > 0;
        ctx.stalker_hurt = hp_percent <= 55;

        const writhing_stalker::live_response response = writhing_stalker::evaluate_live_response( ctx );
        rows.push_back( stalker_pattern_row{ turn, distance, hp_percent, cooldown_turns, strike_count,
                                             response.next, response.route, response.reason } );

        if( response.next == writhing_stalker::decision::strike ) {
            ++strike_count;
            cooldown_turns = 2;
            distance = 4;
            if( strike_count == 2 ) {
                hp_percent = 50;
                cooldown_turns = 0;
            }
        } else if( response.next == writhing_stalker::decision::cooling_off ) {
            cooldown_turns = std::max( 0, cooldown_turns - 1 );
            distance = std::min( 5, distance + 1 );
        } else if( response.next == writhing_stalker::decision::shadow ) {
            distance = std::max( 2, distance - 2 );
        } else if( response.next == writhing_stalker::decision::withdraw ) {
            distance += 3;
        }
    }

    return rows;
}

int count_decisions( const std::vector<stalker_pattern_row> &rows,
                     const writhing_stalker::decision next )
{
    int count = 0;
    for( const stalker_pattern_row &row : rows ) {
        if( row.next == next ) {
            ++count;
        }
    }
    return count;
}

void refresh_writhing_stalker_arena( map &here )
{
    g->reset_light_level();
    here.invalidate_visibility_cache();
    here.update_visibility_cache( 0 );
    here.invalidate_map_cache( 0 );
    here.set_transparency_cache_dirty( 0 );
    here.set_pathfinding_cache_dirty( 0 );
    here.build_map_cache( 0 );
}

void prepare_writhing_stalker_arena( map &here, const tripoint_bub_ms &center )
{
    const ter_id t_floor( "t_floor" );
    const ter_id t_wall( "t_wall" );
    for( const tripoint_bub_ms &p : here.points_in_radius( center, 12 ) ) {
        here.ter_set( p, t_floor );
        here.furn_clear( p );
        here.clear_fields( p );
    }

    // A little adjacent clutter gives the live context an honest cover route without
    // blocking the east/west sight line used by the seam-consumption proof.
    here.ter_set( center + point::north, t_wall );
    refresh_writhing_stalker_arena( here );
}

} // namespace

TEST_CASE( "writhing_stalker_monster_footing", "[writhing_stalker][monster]" )
{
    const mtype &stalker = *mon_writhing_stalker;

    CHECK( stalker.in_species( species_ZOMBIE ) );
    CHECK( stalker.in_species( species_HUMAN ) );
    CHECK( stalker.hp >= 85 );
    CHECK( stalker.hp <= 100 );
    CHECK( stalker.speed >= 115 );
    CHECK( stalker.speed <= 125 );
    CHECK( stalker.sk_dodge >= 4 );
    CHECK( stalker.sk_dodge <= 5 );
    CHECK( stalker.melee_damage.type_damage( damage_cut ) >= 4.0f );
    CHECK( stalker.has_special_attack( "scratch" ) );
    CHECK( stalker.has_special_attack( "bite" ) );
    CHECK( stalker.has_flag( mon_flag_KEENNOSE ) );
    CHECK( stalker.has_flag( mon_flag_PATH_AVOID_DANGER ) );
    CHECK( stalker.has_flag( mon_flag_HARDTOSHOOT ) );
    CHECK( stalker.has_flag( mon_flag_HIT_AND_RUN ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_UNBREAKABLE_MORALE ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_GRABS ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_GROUP_BASH ) );
    CHECK( stalker.has_anger_trigger( mon_trigger::STALK ) );
    CHECK( stalker.has_anger_trigger( mon_trigger::HOSTILE_WEAK ) );
    CHECK( stalker.has_fear_trigger( mon_trigger::HURT ) );
    CHECK( stalker.has_fear_trigger( mon_trigger::BRIGHT_LIGHT ) );
    CHECK_FALSE( stalker.upgrades );
}

TEST_CASE( "writhing_stalker_spawn_footing_is_rare_singleton", "[writhing_stalker][monster][mongroup]" )
{
    const MonsterGroup &group = GROUP_ZOMBIE.obj();

    std::optional<MonsterGroupEntry> stalker_entry;
    int direct_entries = 0;
    int total_direct_weight = 0;
    for( const MonsterGroupEntry &entry : group.monsters ) {
        if( entry.is_group() ) {
            continue;
        }
        total_direct_weight += entry.frequency;
        if( entry.mtype == mon_writhing_stalker ) {
            stalker_entry = entry;
            direct_entries++;
        }
    }

    REQUIRE( direct_entries == 1 );
    REQUIRE( stalker_entry.has_value() );
    CHECK( stalker_entry->frequency == 1 );
    CHECK( stalker_entry->cost_multiplier >= 50 );
    CHECK( stalker_entry->pack_minimum == 1 );
    CHECK( stalker_entry->pack_maximum == 1 );
    CHECK( stalker_entry->starts == 0_turns );
    CHECK( total_direct_weight > 9000 );
    CHECK( stalker_entry->frequency * 5000 < total_direct_weight );
}

TEST_CASE( "writhing_stalker_live_plan_consumes_quiet_side_cutoff_seam",
           "[writhing_stalker][monster][map][zombie_shadow]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms stalker_start = center + point::east * 5;
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 0_hours );
    refresh_writhing_stalker_arena( here );

    monster &stalker = spawn_test_monster( mon_writhing_stalker.str(), stalker_start );
    spawn_test_monster( mon_zombie.str(), center + point::east * 3 );
    spawn_test_monster( mon_zombie.str(), center + point::east * 4 + point::north );
    spawn_test_monster( mon_zombie.str(), center + point::east * 2 + point::south );
    stalker.anger = 100;
    stalker.aggro_character = true;

    REQUIRE( stalker.sees( here, you ) );
    stalker.plan();

    const tripoint_bub_ms dest = here.get_bub( stalker.get_dest() );
    CHECK( dest.x() < you.pos_bub().x() );
    CHECK( rl_dist( dest, you.pos_bub() ) >= 2 );
    CHECK( rl_dist( dest, you.pos_bub() ) <= 4 );
    CHECK_FALSE( dest == you.pos_bub() );
    clear_map_without_vision();
}

TEST_CASE( "writhing_stalker_interest_sources_are_ranked_without_bandit_economy",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    const interest_report empty = evaluate_interest( interest_context{} );
    interest_context human_ctx;
    human_ctx.recent_human_evidence = true;
    human_ctx.evidence_age_minutes = 4;
    const interest_report human = evaluate_interest( human_ctx );

    CHECK( human.source == interest_source::human );
    CHECK( human.can_latch );
    CHECK( human.confidence >= 4 );
    CHECK( human.score > empty.score );
    CHECK( human.reason == "recent_human_evidence" );

    interest_context smoke_ctx;
    smoke_ctx.smoke = true;
    const interest_report smoke = evaluate_interest( smoke_ctx );
    CHECK( smoke.source == interest_source::smoke );
    CHECK_FALSE( smoke.can_latch );
    CHECK( smoke.score < human.score / 4 );
    CHECK( smoke.reason == "smoke_indirect_human_hint" );

    interest_context light_ctx;
    light_ctx.exposed_night_light = true;
    const interest_report light = evaluate_interest( light_ctx );
    CHECK( light.source == interest_source::light );
    CHECK_FALSE( light.can_latch );
    CHECK( light.score >= 50 );
    CHECK( light.score > smoke.score * 3 );

    interest_context terrain_ctx;
    terrain_ctx.forest_or_building_edge = true;
    const interest_report terrain = evaluate_interest( terrain_ctx );
    CHECK( terrain.source == interest_source::terrain );
    CHECK( terrain.score > empty.score );
    CHECK_FALSE( terrain.can_latch );
}

TEST_CASE( "writhing_stalker_latch_is_bounded_and_refresh_requires_evidence",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    interest_context human_ctx;
    human_ctx.recent_human_evidence = true;
    human_ctx.evidence_age_minutes = 3;
    latch_update latched = advance_latch( latch_context{ latch_state{},
            evaluate_interest( human_ctx ), 0, 12, false } );

    REQUIRE( latched.state.active );
    CHECK( latched.next == decision::shadow );
    CHECK( latched.state.leash_tiles_remaining == 60 );
    CHECK( latched.reason == "latch_created_by_believable_evidence" );

    latch_update continuing = advance_latch( latch_context{ latched.state,
            interest_report{}, 12, 20, false } );
    CHECK( continuing.state.active );
    CHECK( continuing.state.age_minutes == 12 );
    CHECK( continuing.reason == "latch_continues_without_refresh" );

    latch_update timed_out = advance_latch( latch_context{ continuing.state,
            interest_report{}, 20, 20, false } );
    CHECK_FALSE( timed_out.state.active );
    CHECK( timed_out.next == decision::cooling_off );
    CHECK( timed_out.state.cooldown_minutes > 0 );
    CHECK( timed_out.reason == "latch_timed_out" );

    human_ctx.evidence_age_minutes = 90;
    latch_update stale = advance_latch( latch_context{ latch_state{},
            evaluate_interest( human_ctx ), 0, 10, false } );
    CHECK_FALSE( stale.state.active );
    CHECK( stale.next == decision::interested );
    CHECK( stale.reason == "interest_without_latch" );
}

TEST_CASE( "writhing_stalker_approach_prefers_cover_and_holds_at_exposure",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    latch_state latch;
    latch.active = true;
    latch.confidence = 4;
    latch.leash_tiles_remaining = 60;

    approach_context cover_ctx;
    cover_ctx.latch = latch;
    cover_ctx.cover_route_available = true;
    cover_ctx.direct_open_route_available = true;
    const approach_report cover = choose_approach( cover_ctx );
    CHECK( cover.route == approach_class::cover_shadow );
    CHECK( cover.next == decision::shadow );
    CHECK( cover.avoids_direct_line );
    CHECK( cover.reason == "cover_route_preferred" );

    approach_context exposed_ctx;
    exposed_ctx.latch = latch;
    exposed_ctx.cover_route_available = true;
    exposed_ctx.bright_exposure = true;
    const approach_report exposed = choose_approach( exposed_ctx );
    CHECK( exposed.route == approach_class::hold_exposed );
    CHECK( exposed.next == decision::hold );
    CHECK( exposed.avoids_direct_line );
    CHECK( exposed.reason == "bright_exposure_holds_shadow" );

    approach_context forced_ctx;
    forced_ctx.latch = latch;
    forced_ctx.direct_open_route_available = true;
    forced_ctx.forced_no_cover = true;
    const approach_report forced = choose_approach( forced_ctx );
    CHECK( forced.route == approach_class::direct_forced );
    CHECK_FALSE( forced.avoids_direct_line );
    CHECK( forced.reason == "direct_route_only_when_forced_by_map" );
}

TEST_CASE( "writhing_stalker_opportunity_uses_zombies_without_magic_tracking",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    latch_state no_latch;
    opportunity_context zombies_without_latch;
    zombies_without_latch.latch = no_latch;
    zombies_without_latch.zombie_pressure = 6;
    zombies_without_latch.player_bleeding = true;
    const opportunity_report ignored = evaluate_opportunity( zombies_without_latch );
    CHECK( ignored.next == decision::ignore );
    CHECK( ignored.reason == "no_latch_no_strike" );

    latch_state latch;
    latch.active = true;
    latch.confidence = 4;
    latch.leash_tiles_remaining = 60;

    opportunity_context calm_ctx;
    calm_ctx.latch = latch;
    calm_ctx.near_cover_or_clutter = true;
    const opportunity_report calm = evaluate_opportunity( calm_ctx );
    CHECK( calm.next == decision::hold );

    opportunity_context zombie_ctx = calm_ctx;
    zombie_ctx.zombie_pressure = 3;
    const opportunity_report zombie = evaluate_opportunity( zombie_ctx );
    CHECK( zombie.zombie_distraction > calm.zombie_distraction );
    CHECK( zombie.opportunity > calm.opportunity );
    CHECK( zombie.next != decision::strike );

    opportunity_context vulnerable_ctx = zombie_ctx;
    vulnerable_ctx.player_bleeding = true;
    vulnerable_ctx.player_hurt = true;
    vulnerable_ctx.player_low_stamina = true;
    vulnerable_ctx.player_distracted = true;
    const opportunity_report vulnerable = evaluate_opportunity( vulnerable_ctx );
    CHECK( vulnerable.vulnerability > calm.vulnerability );
    CHECK( vulnerable.next == decision::strike );
    CHECK( vulnerable.reason == "vulnerability_window_strike" );
}

TEST_CASE( "writhing_stalker_quiet_side_cutoff_prefers_the_side_zombies_are_not",
           "[writhing_stalker][ai][zombie_shadow]" )
{
    using namespace writhing_stalker;

    const std::vector<relative_point> east_zombies = { { 3, 0, 1 }, { 4, 1, 1 }, { 2, -1, 1 } };
    const std::vector<quiet_candidate> candidates = {
        { -3, 0, 8, true, false, true, true, false, 0 },
        { 3, 0, 3, true, false, true, true, false, 0 }
    };

    const quiet_candidate_report chosen = choose_quiet_side_cutoff( east_zombies, candidates );
    REQUIRE( chosen.has_candidate );
    CHECK( chosen.pressure.has_dominant_pressure );
    CHECK( chosen.pressure.pressure_x > 0 );
    CHECK( chosen.pressure.quiet_x < 0 );
    CHECK( chosen.chosen.rel_x < 0 );
    CHECK( chosen.quiet_alignment > 0 );
    CHECK( chosen.reason == "quiet_side_cutoff_preferred" );
}

TEST_CASE( "writhing_stalker_quiet_side_cutoff_avoids_fake_precision_when_pressure_splits",
           "[writhing_stalker][ai][zombie_shadow]" )
{
    using namespace writhing_stalker;

    const std::vector<relative_point> split_zombies = { { 3, 0, 1 }, { -3, 0, 1 } };
    const std::vector<quiet_candidate> candidates = {
        { -3, 0, 6, true, false, false, false, false, 0 },
        { 0, -3, 6, true, false, true, true, false, 0 }
    };

    const quiet_candidate_report chosen = choose_quiet_side_cutoff( split_zombies, candidates );
    REQUIRE( chosen.has_candidate );
    CHECK_FALSE( chosen.pressure.has_dominant_pressure );
    CHECK( chosen.pressure.ambiguous_pressure );
    CHECK( chosen.quiet_alignment == 0 );
    CHECK( chosen.chosen.rel_y < 0 );
    CHECK( chosen.reason == "ambiguous_pressure_no_precise_quiet_side" );
}

TEST_CASE( "writhing_stalker_quiet_side_cutoff_can_follow_retreat_route_when_it_matches_pressure",
           "[writhing_stalker][ai][zombie_shadow]" )
{
    using namespace writhing_stalker;

    const std::vector<relative_point> north_zombies = { { 0, -3, 1 }, { 1, -2, 1 } };
    const std::vector<quiet_candidate> candidates = {
        { 0, 3, 3, true, false, true, true, false, 0 },
        { 1, 3, 6, true, false, true, true, false, 4 }
    };

    const quiet_candidate_report chosen = choose_quiet_side_cutoff( north_zombies, candidates );
    REQUIRE( chosen.has_candidate );
    CHECK( chosen.pressure.quiet_y > 0 );
    CHECK( chosen.chosen.rel_y > 0 );
    CHECK( chosen.chosen.retreat_alignment > 0 );
    CHECK( chosen.reason == "quiet_side_cutoff_preferred" );
}

TEST_CASE( "writhing_stalker_confidence_gates_zombie_pressure_and_suppresses_cutoff_with_light_focus",
           "[writhing_stalker][ai][zombie_shadow]" )
{
    using namespace writhing_stalker;

    confidence_context no_evidence;
    no_evidence.zombie_pressure = 5;
    no_evidence.quiet_side_cutoff_available = true;
    const confidence_report ignored = evaluate_confidence( no_evidence );
    CHECK_FALSE( ignored.pressure_allowed );
    CHECK_FALSE( ignored.cutoff_allowed );
    CHECK( ignored.zombie_pressure == 0 );
    CHECK( ignored.quiet_side_cutoff == 0 );
    CHECK( ignored.reason == "no_evidence_or_interest_pressure_ignored" );

    confidence_context evidence = no_evidence;
    evidence.has_believable_local_evidence = true;
    const confidence_report pressure = evaluate_confidence( evidence );
    CHECK( pressure.pressure_allowed );
    CHECK( pressure.cutoff_allowed );
    CHECK( pressure.evidence > 0 );
    CHECK( pressure.zombie_pressure > 0 );
    CHECK( pressure.quiet_side_cutoff > 0 );
    CHECK( pressure.total > ignored.total );
    CHECK( pressure.reason == "zombie_shadow_quiet_cutoff_confidence" );

    confidence_context interest = no_evidence;
    interest.has_overmap_interest_footing = true;
    const confidence_report overmap_pressure = evaluate_confidence( interest );
    CHECK( overmap_pressure.pressure_allowed );
    CHECK( overmap_pressure.zombie_pressure > 0 );
    CHECK( overmap_pressure.quiet_side_cutoff > 0 );

    confidence_context countered = evidence;
    countered.target_in_bright_exposure = true;
    countered.stalker_in_bright_exposure = true;
    countered.target_has_focus = true;
    countered.open_exposure = true;
    const confidence_report suppressed = evaluate_confidence( countered );
    CHECK( suppressed.counterpressure > 0 );
    CHECK_FALSE( suppressed.cutoff_allowed );
    CHECK( suppressed.quiet_side_cutoff == 0 );
    CHECK( suppressed.total < pressure.total );
    CHECK( suppressed.reason == "counterpressure_suppresses_cutoff" );
}

TEST_CASE( "writhing_stalker_withdrawal_and_cooldown_block_repeat_spam",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    latch_state latch;
    latch.active = true;
    latch.confidence = 4;
    latch.leash_tiles_remaining = 60;

    opportunity_context exposed_ctx;
    exposed_ctx.latch = latch;
    exposed_ctx.bright_exposure = true;
    exposed_ctx.player_focused = true;
    const opportunity_report exposed = evaluate_opportunity( exposed_ctx );
    CHECK( exposed.next == decision::withdraw );
    CHECK( exposed.reason == "exposed_and_focused_withdraw" );

    opportunity_context hurt_ctx;
    hurt_ctx.latch = latch;
    hurt_ctx.player_bleeding = true;
    hurt_ctx.player_hurt = true;
    hurt_ctx.player_low_stamina = true;
    hurt_ctx.player_distracted = true;
    hurt_ctx.stalker_hurt = true;
    const opportunity_report hurt = evaluate_opportunity( hurt_ctx );
    CHECK( hurt.next == decision::withdraw );
    CHECK( hurt.reason == "stalker_hurt_withdraw" );

    latch.cooldown_minutes = 8;
    opportunity_context cooldown_ctx;
    cooldown_ctx.latch = latch;
    cooldown_ctx.player_bleeding = true;
    cooldown_ctx.player_hurt = true;
    cooldown_ctx.player_low_stamina = true;
    cooldown_ctx.player_distracted = true;
    const opportunity_report cooldown = evaluate_opportunity( cooldown_ctx );
    CHECK( cooldown.next == decision::cooling_off );
    CHECK( cooldown.reason == "cooldown_blocks_repeat_strike" );
}

TEST_CASE( "writhing_stalker_live_response_requires_evidence_and_uses_cooldown",
           "[writhing_stalker][ai][live]" )
{
    using namespace writhing_stalker;

    live_context no_evidence;
    no_evidence.distance_to_target = 8;
    no_evidence.player_bleeding = true;
    no_evidence.player_hurt = true;
    no_evidence.player_low_stamina = true;
    no_evidence.player_distracted = true;
    const live_response ignored = evaluate_live_response( no_evidence );
    CHECK( ignored.next == decision::ignore );
    CHECK( ignored.reason == "live_no_believable_evidence" );

    live_context shadow_ctx;
    shadow_ctx.has_believable_local_evidence = true;
    shadow_ctx.distance_to_target = 8;
    shadow_ctx.cover_route_available = true;
    shadow_ctx.near_cover_or_clutter = true;
    const live_response shadow = evaluate_live_response( shadow_ctx );
    CHECK( shadow.next == decision::shadow );
    CHECK( shadow.route == approach_class::cover_shadow );
    CHECK( shadow.reason == "live_shadowing_believable_evidence" );

    live_context far_strike_window_ctx = shadow_ctx;
    far_strike_window_ctx.player_bleeding = true;
    far_strike_window_ctx.player_hurt = true;
    far_strike_window_ctx.player_low_stamina = true;
    far_strike_window_ctx.player_distracted = true;
    far_strike_window_ctx.zombie_pressure = 2;
    const live_response far_strike_window = evaluate_live_response( far_strike_window_ctx );
    CHECK( far_strike_window.next == decision::shadow );
    CHECK( far_strike_window.reason == "live_shadowing_before_strike_window" );

    live_context strike_ctx = far_strike_window_ctx;
    strike_ctx.distance_to_target = 2;
    const live_response strike = evaluate_live_response( strike_ctx );
    CHECK( strike.next == decision::strike );
    CHECK( strike.reason == "live_vulnerability_window_strike" );

    live_context cooldown_ctx = strike_ctx;
    cooldown_ctx.on_cooldown = true;
    const live_response cooldown = evaluate_live_response( cooldown_ctx );
    CHECK( cooldown.next == decision::cooling_off );
    CHECK( cooldown.reason == "live_latch_cooldown_blocks_relatched_pressure" );
}

TEST_CASE( "writhing_stalker_pattern_helper_covers_fair_dread_baselines",
           "[writhing_stalker][ai][pattern]" )
{
    using namespace writhing_stalker;

    live_context no_evidence = pattern_base_context();
    no_evidence.has_believable_local_evidence = false;
    no_evidence.distance_to_target = 2;
    no_evidence.player_bleeding = true;
    no_evidence.player_hurt = true;
    no_evidence.player_low_stamina = true;
    no_evidence.player_distracted = true;
    no_evidence.zombie_pressure = 4;
    const live_response no_magic = evaluate_live_response( no_evidence );
    CHECK( no_magic.next == decision::ignore );
    CHECK( no_magic.reason == "live_no_believable_evidence" );

    interest_context weak_evidence;
    weak_evidence.recent_human_evidence = true;
    weak_evidence.evidence_age_minutes = 25;
    latch_update weak = advance_latch( latch_context{ latch_state{}, evaluate_interest( weak_evidence ),
            0, 8, false } );
    REQUIRE( weak.state.active );
    weak = advance_latch( latch_context{ weak.state, interest_report{}, 31, 8, false } );
    CHECK_FALSE( weak.state.active );
    CHECK( weak.next == decision::cooling_off );
    CHECK( weak.reason == "latch_timed_out" );

    live_context cover = pattern_base_context();
    cover.distance_to_target = 7;
    const live_response covered_route = evaluate_live_response( cover );
    CHECK( covered_route.next == decision::shadow );
    CHECK( covered_route.route == approach_class::cover_shadow );
    CHECK( covered_route.reason == "live_shadowing_believable_evidence" );

    live_context exposed = cover;
    exposed.stalker_in_bright_exposure = true;
    exposed.target_has_focus = true;
    const live_response counterplay = evaluate_live_response( exposed );
    CHECK( counterplay.next == decision::withdraw );
    CHECK( counterplay.reason == "live_exposed_and_focused_withdraw" );

    live_context zombie_only = pattern_base_context();
    zombie_only.distance_to_target = 2;
    zombie_only.zombie_pressure = 4;
    const live_response distraction_without_vulnerability = evaluate_live_response( zombie_only );
    CHECK( distraction_without_vulnerability.next != decision::strike );

    live_context vulnerable = zombie_only;
    vulnerable.player_bleeding = true;
    vulnerable.player_hurt = true;
    vulnerable.player_low_stamina = true;
    vulnerable.player_distracted = true;
    const live_response strike_window = evaluate_live_response( vulnerable );
    CHECK( strike_window.next == decision::strike );
    CHECK( strike_window.reason == "live_vulnerability_window_strike" );
}

TEST_CASE( "writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat",
           "[writhing_stalker][ai][pattern]" )
{
    using namespace writhing_stalker;

    const std::vector<stalker_pattern_row> rows = run_vulnerable_stalker_pattern( 8 );
    INFO( "writhing stalker pattern trace:\n" << trace_rows( rows ) );

    REQUIRE( rows.size() == 8 );
    CHECK( rows[0].next == decision::shadow );
    CHECK( rows[0].route == approach_class::cover_shadow );
    CHECK( rows[1].next == decision::strike );
    CHECK( rows[2].next == decision::cooling_off );
    CHECK( rows[3].next == decision::cooling_off );
    CHECK( rows[4].next == decision::shadow );
    CHECK( rows[5].next == decision::strike );
    CHECK( rows[6].hp_percent == 50 );
    CHECK( rows[6].next == decision::withdraw );
    CHECK( rows[6].reason == "live_stalker_hurt_withdraw" );

    CHECK( count_decisions( rows, decision::strike ) == 2 );
    CHECK( count_decisions( rows, decision::withdraw ) >= 1 );
    CHECK_FALSE( rows[2].next == decision::strike );
    CHECK_FALSE( rows[3].next == decision::strike );

    int jitter_count = 0;
    for( size_t i = 1; i < rows.size(); ++i ) {
        if( rows[i - 1].next == decision::withdraw && rows[i].next == decision::strike ) {
            ++jitter_count;
        }
    }
    CHECK( jitter_count == 0 );
}
