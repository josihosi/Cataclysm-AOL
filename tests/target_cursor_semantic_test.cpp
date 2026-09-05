#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "map_helpers.h"
#include "player_helpers.h"
#include "ranged.h"
#include "semantic_surface.h"

TEST_CASE( "target cursor boundary is rejected without stranding the owner",
           "[semantic_surface][target_cursor]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const time_point turn = calendar::turn;
    const int moves = you.get_moves();
    semantic_surface_manager manager( "target-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    std::vector<semantic_action_receipt> receipts;
    std::string initial_cursor;
    std::string moved_frame;
    int publications = 0;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
        if( receipt.request_id == "boundary" ) {
            CHECK_FALSE( receipt.accepted );
            CHECK( receipt.rejection_reason == "cursor_at_boundary" );
            REQUIRE( manager.top() );
            const auto &owner = *manager.top();
            CHECK( owner.kind == "target" );
            REQUIRE( manager.submit_request( { "target-run", owner.surface_id, owner.frame_id,
                                               "cancel", "target.cancel", std::nullopt, {} } ) );
        }
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "target" ) {
            return;
        }
        ++publications;
        if( publications == 1 ) {
            initial_cursor = descriptor.payload.at( "cursor" );
            REQUIRE( manager.submit_request( { "target-run", descriptor.surface_id, descriptor.frame_id,
                                               "move", "target.move_cursor", "east", {} } ) );
        } else {
            REQUIRE( publications == 2 );
            CHECK( descriptor.payload.at( "cursor" ) != initial_cursor );
            moved_frame = descriptor.frame_id;
            REQUIRE( manager.submit_request( { "target-run", descriptor.surface_id, descriptor.frame_id,
                                               "boundary", "target.move_cursor", "east", {} } ) );
        }
    } );
    CHECK( target_handler::mode_select_only( you, 1 ).empty() );
    REQUIRE( receipts.size() == 3 );
    CHECK( receipts[0].request_id == "move" );
    CHECK( receipts[0].accepted );
    CHECK( receipts[0].resulting_frame_id == moved_frame );
    CHECK( receipts[1].request_id == "boundary" );
    CHECK_FALSE( receipts[1].accepted );
    CHECK( receipts[2].request_id == "cancel" );
    CHECK( receipts[2].accepted );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == world.surface_id() );
    CHECK( receipts[2].resulting_frame_id == manager.top()->frame_id );
    CHECK( you.get_moves() == moves );
    CHECK( calendar::turn == turn );
}
