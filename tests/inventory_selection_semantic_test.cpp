#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
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
#include "map.h"
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

namespace
{
class collated_water_preset : public inventory_selector_preset
{
    public:
        collated_water_preset() {
            _collate_entries = true;
            _indent_entries = false;
        }
        bool is_shown( const item_location &loc ) const override {
            return loc && loc->typeId() == itype_id( "water_clean" );
        }
};

class uid_pick_control : public inventory_pick_selector
{
    public:
        uid_pick_control( Character &who, const inventory_selector_preset &preset ) :
            inventory_pick_selector( who, preset ) {}
        void prepare_native_columns() {
            for( inventory_column *column : get_all_columns() ) {
                column->prepare_paging( get_filter() );
            }
        }
};

class uid_inventory_control : public inventory_multiselector
{
    public:
        using inventory_selector::semantic_actions;
        using inventory_selector::handle_semantic_request;
        using inventory_multiselector::on_input;
        using inventory_multiselector::to_use;
        using inventory_selector::get_all_columns;
        using inventory_selector::append_column;
        void prepare_native_columns() {
            for( inventory_column *column : get_all_columns() ) {
                column->prepare_paging( get_filter() );
            }
        }
        uid_inventory_control( Character &who, const inventory_selector_preset &preset ) :
            inventory_multiselector( who, preset ) {}
};

void retain_uid_descriptor( const semantic_surface_descriptor &descriptor, const std::string &name )
{
    const char *directory = std::getenv( "R067_INVENTORY_DESCRIPTOR_DIR" );
    if( !directory ) {
        return;
    }
    std::ofstream out( std::filesystem::path( directory ) / ( name + ".jsonl" ), std::ios::app );
    REQUIRE( out );
    JsonOut json( out );
    json.start_object();
    json.member( "event", "surface_descriptor" );
    json.member( "schema_version", descriptor.schema_version );
    json.member( "run_id", descriptor.run_id );
    json.member( "surface_id", descriptor.surface_id );
    json.member( "frame_id", descriptor.frame_id );
    json.member( "kind", descriptor.kind );
    json.member( "breadcrumbs", descriptor.breadcrumbs );
    json.member( "payload", descriptor.payload );
    json.member( "valid_actions" );
    json.start_array();
    for( const semantic_action_descriptor &action : descriptor.valid_actions ) {
        json.start_object();
        json.member( "id", action.id );
        json.member( "stable_id", action.stable_id );
        json.member( "label", action.label );
        json.member( "enabled", action.enabled );
        json.end_object();
    }
    json.end_array();
    json.end_object();
    out << '\n';
}

std::vector<item_location> collated_water_items( avatar &you )
{
    std::vector<item_location> result;
    for( int i = 0; i < 3; ++i ) {
        item bottle( itype_id( "jug_plastic" ) );
        REQUIRE( bottle.put_in( item( itype_id( "water_clean" ), calendar::turn, i + 2 ),
                               pocket_type::CONTAINER ).success() );
        item_location container;
        if( i == 2 ) {
            you.wield( bottle );
            container = you.get_wielded_item();
        } else {
            const tripoint_bub_ms pos = you.pos_bub();
            container = item_location( map_cursor( pos ),
                                      &get_map().add_item_or_charges( pos, bottle ) );
        }
        REQUIRE( container );
        result.emplace_back( container, &container->only_item() );
    }
    return result;
}

void check_unique_uid_actions( const semantic_surface_descriptor &descriptor )
{
    std::set<std::pair<std::string, std::string>> keys;
    for( const semantic_action_descriptor &action : descriptor.valid_actions ) {
        CHECK( keys.emplace( action.id, action.stable_id ).second );
    }
}
} // namespace

TEST_CASE( "collated inventory publishes each physical UID and selects a nonfront member once",
           "[semantic_surface][inventory_uid_unique_067]" )
{
    const bool cancel = GENERATE( false, true );
    const bool filtered = GENERATE( false, true );
    CAPTURE( cancel, filtered );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const auto items = collated_water_items( you );
    const std::string uid = std::to_string( items[1]->uid().get_value() );
    REQUIRE( items[0]->tname() == items[1]->tname() );
    collated_water_preset preset;
    uid_pick_control selector( you, preset );
    selector.set_title( "Consume item" );
    selector.add_character_items( you );
    selector.add_nearby_items( 1 );
    if( filtered ) {
        selector.set_filter( "clean water" );
    }
    selector.prepare_native_columns();
    semantic_surface_manager manager( "uid-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    std::optional<semantic_action_request> submitted;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "inventory" ) {
            return;
        }
        check_unique_uid_actions( descriptor );
        retain_uid_descriptor( descriptor, std::string( cancel ? "pick-cancel" : "pick-select" ) +
                               ( filtered ? "-filtered" : "" ) );
        for( const auto &location : items ) {
            CHECK( std::count_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            [&]( const semantic_action_descriptor & action ) {
                return action.id == "inventory.select" && action.enabled &&
                       action.stable_id == std::to_string( location->uid().get_value() );
            } ) == 1 );
        }
        if( !submitted ) {
            submitted = semantic_action_request{ descriptor.run_id, descriptor.surface_id,
                        descriptor.frame_id, "choose-once", cancel ? "inventory.cancel" : "inventory.select",
                        cancel ? std::nullopt : std::optional<std::string>( uid ), {} };
            REQUIRE( manager.submit_request( *submitted ) );
        }
    } );
    const item_location result = selector.execute();
    CHECK( result == ( cancel ? item_location() : items[1] ) );
    REQUIRE( submitted );
    // Selection's actual successor is a caller-owned surface, not a redraw.
    semantic_surface_scope successor( manager, "world", "World" );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().accepted );
    CHECK_FALSE( manager.submit_request( *submitted ) );
    CHECK_FALSE( manager.consume_top_request() );
    CHECK( receipts.size() == 2 ); // replayed receipt, no second native choice
    for( int i = 0; i < 3; ++i ) {
        CHECK( items[i]->charges == i + 2 );
    }
}

TEST_CASE( "collated group controls retain ordered native quantities without duplicate UID actions",
           "[semantic_surface][inventory_uid_unique_067]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const auto items = collated_water_items( you );
    collated_water_preset preset;
    uid_inventory_control selector( you, preset );
    selector.add_character_items( you );
    selector.add_nearby_items( 1 );
    selector.prepare_native_columns();
    inventory_entry *header = nullptr;
    for( inventory_column *column : selector.get_all_columns() ) {
        for( inventory_entry *entry : column->get_entries( []( const inventory_entry & e ) {
        return e.is_collation_header(); } ) ) {
            header = entry;
        }
    }
    REQUIRE( header );
    REQUIRE( header->locations.size() == 2 );
    const std::vector<item_location> ordered = header->locations;
    const std::string uid = std::to_string( ordered.back()->uid().get_value() );
    // Compare semantic group selection to the ordinary native keyboard path,
    // including charged-item collation semantics (not a summed estimate).
    REQUIRE( selector.highlight_one_of( { ordered.front() } ) );
    selector.on_input( inventory_input{ "TOGGLE_ENTRY" } );
    const auto native_selected = selector.to_use;
    REQUIRE_FALSE( native_selected.empty() );
    selector.on_input( inventory_input{ "TOGGLE_ENTRY" } );
    REQUIRE( selector.to_use.empty() );
    const std::vector<semantic_action_descriptor> modes = {
        { "inventory.toggle", "", "Toggle", true },
        { "inventory.set_quantity", "", "Quantity", true },
        { "inventory.increase_quantity", "", "Increase", true },
        { "inventory.decrease_quantity", "", "Decrease", true },
        { "inventory.wield", "", "Wield", true },
        { "inventory.wear", "", "Wear", true }
    };
    semantic_surface_manager manager( "uid-group" );
    std::optional<inventory_input> input;
    int dispatches = 0;
    semantic_surface_scope owner( manager, "inventory", "Inventory", {}, selector.semantic_actions( modes ),
    [&]( const semantic_action_request &request ) {
        ++dispatches;
        auto result = selector.handle_semantic_request( request, input );
        result.defer_receipt_to_successor = false;
        return result;
    } );
    check_unique_uid_actions( *manager.top() );
    retain_uid_descriptor( *manager.top(), "group-before" );
    const auto first = *manager.top();
    REQUIRE( manager.submit_request( { first.run_id, first.surface_id, first.frame_id,
                                       "toggle-once", "inventory.toggle", uid, {} } ) );
    REQUIRE( owner.consume_request() );
    REQUIRE( input );
    CHECK( input->semantic_target == ordered.back() );
    CHECK( selector.get_highlighted().locations == ordered );
    selector.on_input( *input );
    input.reset();
    const auto selected = selector.to_use;
    CHECK( selected == native_selected );
    CHECK( selector.get_highlighted().locations == ordered );
    selector.prepare_native_columns();
    REQUIRE( owner.publish( {}, selector.semantic_actions( modes ) ) );
    check_unique_uid_actions( *manager.top() );
    retain_uid_descriptor( *manager.top(), "group-after" );
    const auto selected_frame = manager.top()->frame_id;
    REQUIRE( owner.publish( {}, selector.semantic_actions( modes ) ) );
    CHECK( manager.top()->frame_id == selected_frame );
    CHECK_FALSE( manager.submit_request( { first.run_id, first.surface_id, first.frame_id,
                                           "toggle-once", "inventory.toggle", uid, {} } ) );
    CHECK( dispatches == 1 );
    REQUIRE( manager.submit_request( { first.run_id, first.surface_id, first.frame_id,
                                       "stale-toggle", "inventory.toggle", uid, {} } ) );
    CHECK_FALSE( owner.consume_request() );
    CHECK( dispatches == 1 );
    // Quantity changes must address this same ordered collation group, too.
    selector.on_input( inventory_input{ "DECREASE_COUNT" } );
    const auto native_decreased = selector.to_use;
    selector.on_input( inventory_input{ "INCREASE_COUNT" } );
    REQUIRE( selector.to_use == selected );
    REQUIRE( manager.submit_request( { manager.top()->run_id, manager.top()->surface_id,
                                       manager.top()->frame_id, "decrease", "inventory.decrease_quantity", uid, {} } ) );
    REQUIRE( owner.consume_request() );
    REQUIRE( input );
    selector.on_input( *input );
    input.reset();
    CHECK( selector.to_use == native_decreased );
    CHECK( selector.get_highlighted().locations == ordered );
    REQUIRE( owner.publish( {}, selector.semantic_actions( modes ) ) );
    check_unique_uid_actions( *manager.top() );
    retain_uid_descriptor( *manager.top(), "group-quantity" );
    REQUIRE( manager.top() );
    REQUIRE( manager.submit_request( { manager.top()->run_id, manager.top()->surface_id,
                                       manager.top()->frame_id, "cancel", "inventory.cancel", std::nullopt, {} } ) );
    REQUIRE( owner.consume_request() );
    REQUIRE( input );
    CHECK( input->action == "QUIT" );
    CHECK( selector.to_use == native_decreased );
}

TEST_CASE( "repeated physical rows deduplicate while contradictory UID groups refuse dispatch",
           "[semantic_surface][inventory_uid_unique_067]" )
{
    const std::string conflict = GENERATE( "identical", "denial", "quantity", "membership" );
    CAPTURE( conflict );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const item_location first( map_cursor( you.pos_bub() ),
                               &get_map().add_item_or_charges( you.pos_bub(), item( itype_id( "rock" ) ) ) );
    const item_location second( map_cursor( you.pos_bub() ),
                                &get_map().add_item_or_charges( you.pos_bub(), item( itype_id( "2x4" ) ) ) );
    REQUIRE( first );
    REQUIRE( second );
    inventory_selector_preset preset;
    uid_inventory_control selector( you, preset );
    selector.add_nearby_items( 1 );
    selector.prepare_native_columns();
    const std::string uid = std::to_string( first->uid().get_value() );
    inventory_column alias_column( preset );
    inventory_entry alias( { first } );
    alias.cache_denial( preset );
    if( conflict == "denial" ) {
        alias.enabled = false;
        alias.denial = "native test denial";
    } else if( conflict == "quantity" ) {
        alias.chosen_count = 1;
    } else if( conflict == "membership" ) {
        alias.locations.push_back( second );
    }
    alias_column.add_entry( alias );
    selector.append_column( alias_column );
    const auto actions = selector.semantic_actions( {
        { "inventory.toggle", "", "Toggle", true },
        { "inventory.wield", "", "Wield", true },
        { "inventory.wear", "", "Wear", true }
    } );
    semantic_surface_manager manager( "uid-conflict" );
    std::optional<inventory_input> input;
    semantic_surface_scope owner( manager, "inventory", "Inventory", {}, actions,
    [&]( const semantic_action_request &request ) {
        return selector.handle_semantic_request( request, input );
    } );
    check_unique_uid_actions( *manager.top() );
    retain_uid_descriptor( *manager.top(), "conflict-" + conflict );
    const auto action = std::find_if( actions.begin(), actions.end(), [&]( const auto & candidate ) {
        return candidate.id == "inventory.select" && candidate.stable_id == uid;
    } );
    REQUIRE( action != actions.end() );
    CHECK( action->enabled == ( conflict == "identical" ) );
    const auto result = selector.handle_semantic_request( {
        "uid-conflict", owner.surface_id(), manager.top()->frame_id, "select", "inventory.select", uid, {}
    }, input );
    CHECK( result.accepted == ( conflict == "identical" ) );
    if( conflict == "identical" ) {
        REQUIRE( input );
        CHECK( input->semantic_target == first );
    } else {
        CHECK( result.rejection_reason == "ambiguous_stable_id" );
        CHECK_FALSE( input );
    }
    input.reset();
    CHECK( selector.handle_semantic_request( {
        "uid-conflict", owner.surface_id(), manager.top()->frame_id, "cancel", "inventory.cancel", std::nullopt, {}
    }, input ).accepted );
    REQUIRE( input );
    CHECK( input->action == "QUIT" );
    CHECK( selector.to_use.empty() );
}
