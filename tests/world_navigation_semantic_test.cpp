#include <array>
#include <string>

#include "action.h"
#include "avatar.h"
#include "avatar_action.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "game.h"
#include "input_context.h"
#include "map.h"
#include "map_helpers.h"
#include "player_helpers.h"

TEST_CASE( "registered diagonal actions reach native diagonal movement",
           "[semantic_surface][world_navigation]" )
{
    const int index = GENERATE( 0, 1, 2, 3 );
    const std::array<action_id, 4> actions = {
        ACTION_MOVE_FORTH_LEFT, ACTION_MOVE_FORTH_RIGHT,
        ACTION_MOVE_BACK_LEFT, ACTION_MOVE_BACK_RIGHT
    };
    const std::array<point, 4> offsets = {
        point( -1, -1 ), point( 1, -1 ), point( -1, 1 ), point( 1, 1 )
    };
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const input_context context = get_default_mode_input_context();
    REQUIRE( context.is_registered_action( action_ident( actions[index] ) ) );
    const point_rel_ms delta = get_delta_from_movement_action( actions[index], iso_rotate::yes );
    CHECK( delta.raw() == offsets[index] );
    const tripoint_abs_ms before = you.pos_abs();
    REQUIRE( avatar_action::move( you, here, tripoint_rel_ms( delta, 0 ) ) );
    CHECK( you.pos_abs() == before + tripoint_rel_ms( delta, 0 ) );
}

TEST_CASE( "registered level actions preserve native stairs and refusal",
           "[semantic_surface][world_navigation]" )
{
    const int direction = GENERATE( -1, 1 );
    const bool stairs = GENERATE( false, true );
    clear_avatar();
    clear_map( -1, 1 );
    avatar &you = get_avatar();
    map &here = get_map();
    g->vertical_shift( 0 );
    const tripoint_bub_ms start( 60, 60, 0 );
    you.setpos( here, start );
    on_out_of_scope restore_level( [&]() {
        g->vertical_shift( 0 );
        you.setpos( here, start );
    } );
    for( int level = -1; level <= 1; ++level ) {
        for( int x = -2; x <= 2; ++x ) {
            for( int y = -2; y <= 2; ++y ) {
                here.ter_set( start + tripoint( x, y, level ), ter_id( "t_floor" ) );
            }
        }
    }
    if( stairs ) {
        here.ter_set( start, ter_id( direction > 0 ? "t_stairs_up" : "t_stairs_down" ) );
        here.ter_set( start + tripoint( 0, 0, direction ),
                      ter_id( direction > 0 ? "t_stairs_down" : "t_stairs_up" ) );
    }
    const input_context context = get_default_mode_input_context();
    const action_id action = direction > 0 ? ACTION_MOVE_UP : ACTION_MOVE_DOWN;
    REQUIRE( context.is_registered_action( action_ident( action ) ) );
    const tripoint_abs_ms before = you.pos_abs();
    g->vertical_move( direction, false );
    CHECK( you.pos_abs() == before + tripoint_rel_ms( 0, 0, stairs ? direction : 0 ) );
}
