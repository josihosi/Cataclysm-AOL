#include <memory>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "inventory_ui.h"
#include "item.h"
#include "item_location.h"
#include "json.h"
#include "json_loader.h"
#include "map_helpers.h"
#include "player_helpers.h"
#include "semantic_surface.h"
#include "type_id.h"

TEST_CASE( "drop and generic multiselect publish selected UID quantities before receipts",
           "[semantic_surface][inventory_selection]" )
{
    const bool drop_mode = GENERATE( false, true );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const item_location selected = you.i_add( item( itype_id( "smart_phone" ) ), false );
    REQUIRE( selected );
    const std::string uid = std::to_string( selected->uid().get_value() );
    const time_point turn = calendar::turn;
    const int moves = you.get_moves();

    std::unique_ptr<inventory_multiselector> selector;
    if( drop_mode ) {
        selector = std::make_unique<inventory_drop_selector>( you );
    } else {
        selector = std::make_unique<inventory_multiselector>( you );
    }
    selector->add_character_items( you );
    semantic_surface_manager manager( "selection-run" );
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
        CHECK( descriptor.payload.at( "selection_source" ) ==
               ( drop_mode ? "inventory_drop_selector::to_use" : "inventory_multiselector::to_use" ) );
        if( publications == 1 ) {
            initial_frame = descriptor.frame_id;
            CHECK( descriptor.payload.at( "selected_items" ) == "{}" );
            REQUIRE( manager.submit_request( { "selection-run", descriptor.surface_id,
                                               descriptor.frame_id, "toggle", "inventory.toggle", uid, {} } ) );
        } else {
            REQUIRE( publications == 2 );
            selected_frame = descriptor.frame_id;
            CHECK( selected_frame != initial_frame );
            const JsonObject selection = json_loader::from_string( descriptor.payload.at( "selected_items" ) );
            const JsonObject quantity = selection.get_object( uid );
            CHECK( quantity.get_int( "count" ) == 1 );
            CHECK( quantity.get_string( "unit" ) == "items" );
            REQUIRE( manager.submit_request( { "selection-run", descriptor.surface_id,
                                               descriptor.frame_id, "commit", "inventory.commit", std::nullopt, {} } ) );
        }
    } );
    const drop_locations result = drop_mode ?
                                  static_cast<inventory_drop_selector &>( *selector ).execute() : selector->execute();
    REQUIRE( result.size() == 1 );
    CHECK( result.front().first == selected );
    CHECK( result.front().second == 1 );
    REQUIRE( receipts.size() == 2 );
    CHECK( receipts.front().request_id == "toggle" );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().resulting_frame_id == selected_frame );
    CHECK( receipts.back().request_id == "commit" );
    CHECK( receipts.back().accepted );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == world.surface_id() );
    CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
    CHECK( selected );
    CHECK( calendar::turn == turn );
    CHECK( you.get_moves() == moves );
}

TEST_CASE( "ammo selector publishes native reload quantities and rejects unchanged limits",
           "[semantic_surface][inventory_selection][reload]" )
{
    const bool at_limit = GENERATE( false, true );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const item_location magazine = you.i_add( item( itype_id( "glockmag" ), calendar::turn_zero, 0 ), false );
    const item_location ammo = you.i_add( item( itype_id( "9mm" ), calendar::turn_zero, 10 ), false );
    REQUIRE( magazine );
    REQUIRE( ammo );
    const std::string uid = std::to_string( ammo->uid().get_value() );
    ammo_inventory_selector selector( you, magazine );
    selector.add_character_items( you );
    // A cleared avatar has no storage; i_add may leave the ammo at their feet.
    // Match the production reload selector, which includes nearby ammo too.
    selector.add_nearby_items( 1 );
    selector.set_all_entries_chosen_count();
    semantic_surface_manager manager( "ammo-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
        if( receipt.request_id == "quantity" && at_limit ) {
            CHECK_FALSE( receipt.accepted );
            CHECK( receipt.rejection_reason == "quantity_unchanged" );
            REQUIRE( manager.top() );
            // The rejection keeps this owner usable. Consume a distinct cancel
            // directly: test_mode deliberately disallows physical input waits.
            REQUIRE( manager.submit_request( { "ammo-run", manager.top()->surface_id,
                                               manager.top()->frame_id, "cancel", "inventory.cancel", std::nullopt, {} } ) );
            REQUIRE( manager.consume_top_request() );
        }
    } );
    int publications = 0;
    std::string quantity_frame;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "inventory" ) {
            return;
        }
        ++publications;
        CHECK( descriptor.payload.at( "selection_source" ) == "ammo_inventory_selector::highlighted" );
        if( publications == 1 ) {
            bool ammo_is_advertised = false;
            for( const semantic_action_descriptor &action : descriptor.valid_actions ) {
                ammo_is_advertised = ammo_is_advertised ||
                                     ( action.id == "inventory.select" && action.stable_id == uid && action.enabled );
            }
            REQUIRE( ammo_is_advertised );
            REQUIRE( manager.submit_request( { "ammo-run", descriptor.surface_id,
                                               descriptor.frame_id, "quantity",
                                               at_limit ? "inventory.increase_quantity" : "inventory.decrease_quantity", uid, {} } ) );
        } else {
            REQUIRE_FALSE( at_limit );
            REQUIRE( publications == 2 );
            quantity_frame = descriptor.frame_id;
            const JsonObject selection = json_loader::from_string( descriptor.payload.at( "selected_items" ) );
            const JsonObject quantity = selection.get_object( uid );
            CHECK( quantity.get_int( "count" ) == 9 );
            CHECK( quantity.get_string( "unit" ) == "charges" );
            REQUIRE( manager.submit_request( { "ammo-run", descriptor.surface_id,
                                               descriptor.frame_id, "commit", "inventory.commit", std::nullopt, {} } ) );
        }
    } );
    const drop_location result = selector.execute();
    REQUIRE( receipts.size() == 2 );
    if( at_limit ) {
        CHECK_FALSE( result.first );
        CHECK( publications == 1 );
        CHECK( receipts.back().request_id == "cancel" );
    } else {
        CHECK( result.first == ammo );
        CHECK( result.second == 9 );
        CHECK( receipts.front().accepted );
        CHECK( receipts.front().resulting_frame_id == quantity_frame );
        CHECK( receipts.back().request_id == "commit" );
    }
    CHECK( receipts.back().accepted );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == world.surface_id() );
    CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
    CHECK( magazine->ammo_remaining() == 0 );
    CHECK( ammo->charges == 10 );
}
