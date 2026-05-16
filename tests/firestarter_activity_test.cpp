#include "activity_actor_definitions.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "field_type.h"
#include "game.h"
#include "item.h"
#include "item_location.h"
#include "map.h"
#include "map_helpers.h"
#include "player_activity.h"
#include "type_id.h"

static void complete_activity_for_firestarter_test( Character &u, const activity_actor &act )
{
    u.assign_activity( act );
    while( !u.activity.is_null() ) {
        u.set_moves( u.get_speed() );
        u.activity.do_turn( u );
    }
}

TEST_CASE( "fire_start_activity_exact_brazier_wood_lighter_creates_fd_fire",
           "[firestarter][activity][bandit_live_world]" )
{
    clear_map_without_vision();

    map &here = get_map();
    avatar &u = get_avatar();
    const tripoint_bub_ms player_pos( 60, 60, 0 );
    const tripoint_bub_ms fire_pos = player_pos + tripoint::east;

    u.setpos( here, player_pos );
    here.ter_set( player_pos, ter_id( "t_dirt" ) );
    here.furn_set( player_pos, furn_id( "f_null" ) );
    here.ter_set( fire_pos, ter_id( "t_dirt" ) );
    here.furn_set( fire_pos, furn_id( "f_brazier" ) );
    here.remove_field( fire_pos, fd_fire );

    here.add_item_or_charges( fire_pos, item( itype_id( "2x4" ), calendar::turn ) );

    item lighter( itype_id( "lighter" ), calendar::turn );
    lighter.charges = 100;
    item_location lighter_loc = u.i_add( lighter );

    REQUIRE( here.is_flammable( fire_pos ) );
    REQUIRE( !here.get_field( fire_pos, fd_fire ) );

    complete_activity_for_firestarter_test( u,
                                            fire_start_activity_actor( here.get_abs( fire_pos ), lighter_loc,
                                                    0, 100 ) );

    CHECK( here.get_field_intensity( fire_pos, fd_fire ) > 0 );
}
