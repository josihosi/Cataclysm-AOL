#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "game.h"
#include "game_inventory.h"
#include "item.h"
#include "item_location.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "player_helpers.h"
#include "semantic_surface.h"
#include "type_id.h"

TEST_CASE( "pickup selection publishes native quantities before completing toggle receipts",
           "[semantic_surface][pickup]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    you.wear_item( item( itype_id( "pants" ) ) );
    REQUIRE( you.is_wearing( itype_id( "pants" ) ) );
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    std::string item_type;
    std::string quantity_unit;
    SECTION( "charge counted rocks" ) {
        item_type = "rock";
        quantity_unit = "charges";
    }
    SECTION( "individual smartphones" ) {
        item_type = "smart_phone";
        quantity_unit = "items";
    }
    item &placed = here.add_item_or_charges( position, item( itype_id( item_type ) ) );
    const std::string uid = std::to_string( placed.uid().get_value() );
    const time_point turn = calendar::turn;
    const int moves = you.get_moves();

    semantic_surface_manager manager( "pickup-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    std::string initial_frame;
    std::string selected_frame;
    int publications = 0;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "inventory" ) {
            return;
        }
        ++publications;
        CHECK( descriptor.payload.at( "selection_source" ) == "pickup_selector::to_use" );
        if( publications == 1 ) {
            initial_frame = descriptor.frame_id;
            CHECK( descriptor.payload.at( "selected_items" ) == "{}" );
            REQUIRE( manager.submit_request( { "pickup-run", descriptor.surface_id,
                                               descriptor.frame_id, "toggle", "inventory.toggle", uid, {} } ) );
        } else {
            REQUIRE( publications == 2 );
            selected_frame = descriptor.frame_id;
            CHECK( selected_frame != initial_frame );
            const JsonObject selected = json_loader::from_string(
                                            descriptor.payload.at( "selected_items" ) );
            const JsonObject quantity = selected.get_object( uid );
            CHECK( quantity.get_int( "count" ) == 1 );
            CHECK( quantity.get_string( "unit" ) == quantity_unit );
            REQUIRE( manager.submit_request( { "pickup-run", descriptor.surface_id,
                                               descriptor.frame_id, "commit", "inventory.commit", std::nullopt, {} } ) );
        }
    } );
    const drop_locations selected = game_menus::inv::pickup( { position } );
    REQUIRE( selected.size() == 1 );
    CHECK( selected.front().first.get_item() == &placed );
    CHECK( selected.front().second == 1 );
    CHECK( publications == 2 );
    REQUIRE( receipts.size() == 2 );
    CHECK( receipts[0].request_id == "toggle" );
    CHECK( receipts[0].accepted );
    CHECK( receipts[0].resulting_frame_id == selected_frame );
    CHECK( receipts[1].request_id == "commit" );
    CHECK( receipts[1].accepted );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == world.surface_id() );
    CHECK( receipts[1].resulting_frame_id == manager.top()->frame_id );
    CHECK( here.i_at( position ).only_item().uid() == placed.uid() );
    CHECK( calendar::turn == turn );
    CHECK( you.get_moves() == moves );
}
