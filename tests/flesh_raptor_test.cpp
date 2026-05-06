#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "calendar.h"
#include "cata_catch.h"
#include "character.h"
#include "effect.h"
#include "flesh_raptor_ai.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "mtype.h"
#include "player_helpers.h"
#include "type_id.h"

static const efftype_id effect_run( "run" );
static const mtype_id mon_eigenspectre_1( "mon_eigenspectre_1" );
static const mtype_id mon_fungal_raptor( "mon_fungal_raptor" );
static const mtype_id mon_spawn_raptor( "mon_spawn_raptor" );
static const mtype_id mon_spawn_raptor_dusted( "mon_spawn_raptor_dusted" );
static const mtype_id mon_spawn_raptor_electric( "mon_spawn_raptor_electric" );
static const mtype_id mon_spawn_raptor_fungalize( "mon_spawn_raptor_fungalize" );
static const mtype_id mon_spawn_raptor_shady( "mon_spawn_raptor_shady" );
static const mtype_id mon_spawn_raptor_unstable( "mon_spawn_raptor_unstable" );

namespace
{

void refresh_flesh_raptor_arena( map &here )
{
    g->reset_light_level();
    here.invalidate_visibility_cache();
    here.update_visibility_cache( 0 );
    here.invalidate_map_cache( 0 );
    here.set_transparency_cache_dirty( 0 );
    here.set_pathfinding_cache_dirty( 0 );
    here.build_map_cache( 0 );
}

void prepare_flesh_raptor_arena( map &here, const tripoint_bub_ms &center )
{
    const ter_id t_floor( "t_floor" );
    for( const tripoint_bub_ms &p : here.points_in_radius( center, 12 ) ) {
        here.ter_set( p, t_floor );
        here.furn_clear( p );
        here.clear_fields( p );
    }
    refresh_flesh_raptor_arena( here );
}

flesh_raptor::orbit_context east_raptor_context()
{
    flesh_raptor::orbit_context ctx;
    ctx.current_rel_x = 5;
    ctx.current_rel_y = 0;
    ctx.distance_to_target = 5;
    ctx.cadence_phase = 1;
    return ctx;
}

} // namespace

TEST_CASE( "flesh_raptor_variants_keep_skirmisher_footing", "[flesh_raptor][monster]" )
{
    const std::vector<mtype_id> raptor_variants = {
        mon_spawn_raptor,
        mon_spawn_raptor_shady,
        mon_spawn_raptor_unstable,
        mon_spawn_raptor_electric,
        mon_spawn_raptor_dusted,
        mon_spawn_raptor_fungalize,
        mon_fungal_raptor,
    };

    for( const mtype_id &id : raptor_variants ) {
        CAPTURE( id.str() );
        const mtype &raptor = *id;
        CHECK( raptor.has_flag( mon_flag_HIT_AND_RUN ) );
        CHECK( raptor.has_flag( mon_flag_FLIES ) );
        CHECK( raptor.has_flag( mon_flag_SEES ) );
        CHECK( raptor.has_special_attack( "impale" ) );
    }
}

TEST_CASE( "flesh_raptor_orbit_scorer_prefers_lateral_open_arc", "[flesh_raptor][ai]" )
{
    const flesh_raptor::orbit_context ctx = east_raptor_context();
    const std::vector<flesh_raptor::orbit_candidate> candidates = {
        { 7, 0, 7, 2, 0, false, "straight_retreat" },
        { 0, 5, 5, 7, 0, false, "lateral_orbit" },
        { 3, 3, 3, 4, 0, false, "too_close_diagonal" },
    };

    const flesh_raptor::orbit_response response = flesh_raptor::choose_orbit_destination( ctx,
            candidates );

    CHECK( response.next == flesh_raptor::decision::orbit );
    REQUIRE( response.has_candidate );
    CHECK( response.chosen.label == "lateral_orbit" );
}

TEST_CASE( "flesh_raptor_orbit_scorer_prefers_under_occupied_arc", "[flesh_raptor][ai]" )
{
    const flesh_raptor::orbit_context ctx = east_raptor_context();
    const std::vector<flesh_raptor::orbit_candidate> candidates = {
        { 0, -5, 5, 7, 3, false, "crowded_north_arc" },
        { 0, 5, 5, 7, 0, false, "open_south_arc" },
    };

    const flesh_raptor::orbit_response response = flesh_raptor::choose_orbit_destination( ctx,
            candidates );

    CHECK( response.next == flesh_raptor::decision::orbit );
    REQUIRE( response.has_candidate );
    CHECK( response.chosen.label == "open_south_arc" );
}

TEST_CASE( "flesh_raptor_orbit_cadence_and_hysteresis_guard", "[flesh_raptor][ai]" )
{
    flesh_raptor::orbit_context ctx = east_raptor_context();
    std::vector<flesh_raptor::orbit_candidate> candidates = {
        { 0, 5, 5, 7, 0, false, "new_lateral_arc" },
        { 5, 0, 5, 0, 0, true, "held_arc" },
    };

    flesh_raptor::orbit_response response = flesh_raptor::choose_orbit_destination( ctx, candidates );
    CHECK( response.next == flesh_raptor::decision::orbit );
    REQUIRE( response.has_candidate );
    CHECK( response.chosen.label == "held_arc" );
    CHECK( response.reason == "held_orbit_intention" );

    ctx.cadence_phase = 0;
    response = flesh_raptor::choose_orbit_destination( ctx, candidates );
    CHECK( response.next == flesh_raptor::decision::swoop );
    CHECK_FALSE( response.has_candidate );
}

TEST_CASE( "flesh_raptor_orbit_corridor_falls_back_when_no_readable_arc", "[flesh_raptor][ai]" )
{
    const flesh_raptor::orbit_context ctx = east_raptor_context();
    const std::vector<flesh_raptor::orbit_candidate> candidates = {
        { 8, 0, 8, 3, 0, false, "narrow_straight_corridor" },
    };

    const flesh_raptor::orbit_response response = flesh_raptor::choose_orbit_destination( ctx,
            candidates );

    CHECK( response.next == flesh_raptor::decision::fallback );
    CHECK( response.reason == "no_readable_lateral_orbit" );
}

TEST_CASE( "flesh_raptor_live_plan_consumes_orbit_scorer_for_spawn_raptor", "[flesh_raptor][monster][map]" )
{
    clear_map_without_vision();
    clear_avatar();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms raptor_start = center + point::east * 5;
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );
    prepare_flesh_raptor_arena( here, center );
    you.setpos( here, center );

    monster &raptor = spawn_test_monster( mon_spawn_raptor.str(), raptor_start );
    raptor.anger = 100;
    raptor.aggro_character = true;
    raptor.add_effect( effect_run, 1_turns, true );

    REQUIRE( raptor.sees( here, you ) );
    raptor.plan();

    const tripoint_bub_ms dest = here.get_bub( raptor.get_dest() );
    CHECK( rl_dist( dest, you.pos_bub() ) >= 4 );
    CHECK( rl_dist( dest, you.pos_bub() ) <= 6 );
    CHECK( std::abs( dest.y() - you.pos_bub().y() ) >= 4 );
    CHECK_FALSE( dest == you.pos_bub() );
    clear_map_without_vision();
}

TEST_CASE( "flesh_raptor_live_plan_does_not_rewrite_other_hit_and_run_monsters", "[flesh_raptor][monster][map]" )
{
    clear_map_without_vision();
    clear_avatar();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms eigenspectre_start = center + point::east * 5;
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );
    prepare_flesh_raptor_arena( here, center );
    you.setpos( here, center );

    monster &eigenspectre = spawn_test_monster( mon_eigenspectre_1.str(), eigenspectre_start );
    REQUIRE( eigenspectre.type->has_flag( mon_flag_HIT_AND_RUN ) );
    eigenspectre.anger = 100;
    eigenspectre.aggro_character = true;

    REQUIRE( eigenspectre.sees( here, you ) );
    eigenspectre.plan();

    CHECK( eigenspectre.get_dest() == you.pos_abs() );
    CHECK_FALSE( eigenspectre.has_effect( effect_run ) );
    clear_map_without_vision();
}
