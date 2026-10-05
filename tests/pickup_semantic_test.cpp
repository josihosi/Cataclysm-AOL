#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cached_options.h"
#include "cata_scope_helpers.h"
#include "sdl_renderer_recovery.h"
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
#include "pickup.h"
#include "semantic_surface.h"
#include "type_id.h"

#if defined(TILES)
TEST_CASE( "pickup quantity applies a request arriving in blocked native input",
           "[semantic_surface][pickup][pickup_quantity_wake_067]" )
{
    const bool cancel = GENERATE( false, true );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    REQUIRE( you.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    std::string uid;
    for( int i = 0; i < 242; ++i ) {
        item &placed = here.add_item_or_charges( position, item( itype_id( "2x4" ) ) );
        uid = std::to_string( placed.uid().get_value() );
    }
    const time_point turn = calendar::turn;
    const int moves = you.get_moves();
    software_render_fixture renderer;
    REQUIRE( renderer.available() );
    renderer_recovery_test_support::setup_software_ui();
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;

    const std::filesystem::path path = std::filesystem::temp_directory_path() /
                                      ( "pickup-quantity-wake-" + std::to_string(
                                            std::chrono::steady_clock::now().time_since_epoch().count() ) + ".jsonl" );
    const char *old_path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    const std::optional<std::string> previous = old_path ?
            std::optional<std::string>( old_path ) : std::nullopt;
    on_out_of_scope cleanup( [&]() {
        if( previous ) {
            setenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", previous->c_str(), 1 );
        } else {
            unsetenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
        }
        std::filesystem::remove( path );
    } );
    setenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", path.c_str(), 1 );

    semantic_surface_manager manager( "pickup-wake" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    std::optional<semantic_surface_descriptor> prompt;
    int prompt_count = 0;
    int stage = 0;
    int selected_count = -1;
    bool closing_reopened = false;
    bool arrived_inside_input = false;
    int accepted_quantity_receipts = 0;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        if( receipt.request_id == "arrival-boundary" ) {
            CHECK_FALSE( receipt.accepted );
            CHECK( receipt.rejection_reason == "wrong_run" );
        } else if( receipt.request_id == "quantity" ) {
            CHECK( receipt.accepted );
            ++accepted_quantity_receipts;
        }
    } );
    manager.set_transport_observer( [&]( const semantic_request_transport_event &event ) {
        if( event.event != "jsonl_record" || event.request_id != "arrival-boundary" ) {
            return;
        }
        if( arrived_inside_input ) {
            return;
        }
        CAPTURE( prompt_count, event.queued, manager.has_pending_request() );
        // Only the SDL input backend polls this file.  The inert wrong-run
        // marker proves handle_input is already running before the real
        // quantity request arrives; no request is queued on publication.
        REQUIRE( prompt );
        CHECK_FALSE( manager.consume_top_request() ); // It consumes but rejects the wrong-run marker.
        CHECK_FALSE( manager.has_pending_request() );
        arrived_inside_input = true;
        REQUIRE( manager.submit_request( { "pickup-wake", prompt->surface_id,
                                           prompt->frame_id, "quantity", cancel ? "prompt.cancel" : "prompt.submit",
                                           std::nullopt, cancel ? std::map<std::string, std::string>{} :
                                           std::map<std::string, std::string>{{ "text", "1" }} } ) );
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind == "string_prompt" ) {
            ++prompt_count;
            if( prompt_count == 1 ) {
                prompt = descriptor;
                CHECK_FALSE( manager.has_pending_request() );
                std::ofstream stream( path );
                stream << "{\"run_id\":\"inert-marker\",\"request_id\":\"arrival-boundary\"}\n";
            } else {
                // Bound the old failing implementation: close its reopened
                // prompt instead of leaving a regression test blocked.
                REQUIRE( manager.submit_request( { "pickup-wake", descriptor.surface_id,
                                                   descriptor.frame_id, "close-reopened", "prompt.cancel", std::nullopt, {} } ) );
            }
            return;
        }
        if( descriptor.kind != "inventory" ) {
            return;
        }
        const JsonObject selected = json_loader::from_string( descriptor.payload.at( "selected_items" ) );
        int count = 0;
        for( const JsonMember &member : selected ) {
            const JsonObject quantity = member.get_object();
            count += quantity.get_int( "count" );
            CHECK( quantity.get_string( "unit" ) == "items" );
        }
        const auto submit = [&]( const std::string &id, const std::string &request_id ) {
            REQUIRE( manager.submit_request( { "pickup-wake", descriptor.surface_id, descriptor.frame_id,
                                               request_id, id, ( id == "inventory.commit" || id == "inventory.cancel" ) ? std::nullopt :
                                               std::optional<std::string>( uid ), {} } ) );
        };
        if( closing_reopened ) {
            return;
        }
        if( prompt_count > 1 ) {
            selected_count = count;
            closing_reopened = true;
            submit( "inventory.cancel", "close-old-selector" );
            return;
        }
        if( stage == 0 ) {
            ++stage;
            submit( "inventory.toggle", "select-all" );
        } else if( stage == 1 ) {
            CHECK( count == 242 );
            ++stage;
            submit( "inventory.set_quantity", "open-quantity" );
        } else if( count == 242 && !cancel && prompt_count == 1 ) {
            // The restored parent is published before on_input applies 1.
            return;
        } else {
            selected_count = count;
            submit( "inventory.commit", "commit" );
        }
    } );
    const drop_locations selection = game_menus::inv::pickup( { position } );
    CHECK( arrived_inside_input );
    CHECK( accepted_quantity_receipts == 1 );
    CHECK( prompt_count == 1 );
    CHECK( selected_count == ( cancel ? 242 : 1 ) );
    int total = 0;
    for( const auto &entry : selection ) {
        total += entry.second;
    }
    CHECK( total == ( cancel ? 242 : 1 ) );
    CHECK( here.i_at( position ).size() == 242 );
    CHECK( calendar::turn == turn );
    CHECK( you.get_moves() == moves );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == world.surface_id() );
}
#endif

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
            const auto toggle = std::find_if( descriptor.valid_actions.begin(),
            descriptor.valid_actions.end(), [&uid]( const semantic_action_descriptor &action ) {
                return action.id == "inventory.toggle" && action.stable_id == uid;
            } );
            REQUIRE( toggle != descriptor.valid_actions.end() );
            const auto select = std::find_if( descriptor.valid_actions.begin(), toggle,
            [&uid]( const semantic_action_descriptor &action ) {
                return action.id == "inventory.select" && action.stable_id == uid;
            } );
            CHECK( select != toggle );
            CHECK( std::count_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            []( const semantic_action_descriptor &action ) {
                return action.id == "inventory.commit";
            } ) == 1 );
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

#if defined(TILES)
TEST_CASE( "pickup native partial quantities preserve current frames and physical items",
           "[semantic_surface][pickup][pickup_quantity_067]" )
{
    const bool commit = GENERATE( false, true );
    const bool charges = GENERATE( false, true );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    REQUIRE( you.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    const itype_id type( charges ? "9mm" : "2x4" );
    std::string uid;
    if( charges ) {
        item &placed = here.add_item_or_charges( position, item( type, calendar::turn_zero, 40 ) );
        uid = std::to_string( placed.uid().get_value() );
    } else {
        for( int i = 0; i < 40; ++i ) {
            item &placed = here.add_item_or_charges( position, item( type ) );
            uid = std::to_string( placed.uid().get_value() );
        }
    }
    const auto ground_count = [&]() {
        int count = 0;
        for( const item &it : here.i_at( position ) ) {
            if( it.typeId() == type ) {
                count += charges ? it.charges : 1;
            }
        }
        return count;
    };
    REQUIRE( ground_count() == 40 );
    const time_point turn = calendar::turn;
    const int moves = you.get_moves();
    software_render_fixture renderer;
    REQUIRE( renderer.available() );
    renderer_recovery_test_support::setup_software_ui();
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    semantic_surface_manager manager( "pickup-quantity" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    int stage = 0;
    int prompt = 0;
    int sequence = 0;
    std::string initial_frame;
    bool stale_rejected = false;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        if( receipt.request_id == "old-frame" ) {
            CHECK_FALSE( receipt.accepted );
            stale_rejected = true;
        } else {
            CHECK( receipt.accepted );
        }
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        const auto submit = [&]( const std::string &id, const std::optional<std::string> &target,
                                 const std::map<std::string, std::string> &params = {} ) {
            const auto found = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            [&]( const semantic_action_descriptor &action ) {
                return action.id == id && action.stable_id == target.value_or( "" );
            } );
            REQUIRE( found != descriptor.valid_actions.end() );
            REQUIRE( found->enabled );
            REQUIRE( manager.submit_request( { "pickup-quantity", descriptor.surface_id,
                                               descriptor.frame_id, std::to_string( ++sequence ), id, target, params } ) );
        };
        if( descriptor.kind == "string_prompt" ) {
            ++prompt;
            if( prompt == 1 ) {
                submit( "prompt.cancel", std::nullopt );
            } else {
                submit( "prompt.submit", std::nullopt, {{ "text", prompt == 3 ? "1000" : "3" }} );
            }
            return;
        }
        if( descriptor.kind != "inventory" ) {
            return;
        }
        const JsonObject selected = json_loader::from_string( descriptor.payload.at( "selected_items" ) );
        int chosen = 0;
        for( const JsonMember &member : selected ) {
            const JsonObject quantity = member.get_object();
            chosen += quantity.get_int( "count" );
            CHECK( quantity.get_string( "unit" ) == ( charges ? "charges" : "items" ) );
        }
        // Closing a count prompt first restores the parent's preceding frame;
        // its native on_input applies the count before the next publication.
        if( ( stage == 3 && chosen == 1 ) || ( stage == 5 && chosen == 2 ) ||
            ( stage == 7 && chosen == 39 ) ) {
            return;
        }
        const auto enabled = [&]( const std::string &id ) {
            const auto action = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            [&]( const semantic_action_descriptor &candidate ) { return candidate.id == id && candidate.stable_id == uid; } );
            REQUIRE( action != descriptor.valid_actions.end() );
            return action->enabled;
        };
        if( stage == 0 ) {
            CHECK( chosen == 0 );
            CHECK_FALSE( enabled( "inventory.decrease_quantity" ) );
            initial_frame = descriptor.frame_id;
            ++stage;
            submit( "inventory.increase_quantity", uid );
        } else if( stage == 1 ) {
            CHECK( chosen == 1 );
            CHECK( descriptor.frame_id != initial_frame );
            REQUIRE( manager.submit_request( { "pickup-quantity", descriptor.surface_id, initial_frame,
                                               "old-frame", "inventory.increase_quantity", uid, {} } ) );
            manager.consume_top_request();
            REQUIRE( stale_rejected );
            ++stage;
            submit( "inventory.set_quantity", uid );
        } else if( stage == 2 ) {
            CHECK( chosen == 1 ); // Canceling the count prompt did not change the basket.
            ++stage;
            submit( "inventory.set_quantity", uid );
        } else if( stage == 3 ) {
            CHECK( chosen == 3 );
            ++stage;
            submit( "inventory.decrease_quantity", uid );
        } else if( stage == 4 ) {
            CHECK( chosen == 2 );
            ++stage;
            submit( "inventory.set_quantity", uid );
        } else if( stage == 5 ) {
            CHECK( chosen == 40 ); // Existing native count clamping.
            CHECK_FALSE( enabled( "inventory.increase_quantity" ) );
            ++stage;
            submit( "inventory.decrease_quantity", uid );
        } else if( stage == 6 ) {
            CHECK( chosen == 39 );
            ++stage;
            submit( "inventory.set_quantity", uid );
        } else {
            REQUIRE( stage == 7 );
            CHECK( chosen == 3 );
            ++stage;
            submit( commit ? "inventory.commit" : "inventory.cancel", std::nullopt );
        }
    } );
    drop_locations selected;
    try {
        selected = game_menus::inv::pickup( { position } );
    } catch( const std::exception &error ) {
        FAIL( "pickup stage=" << stage << " prompt=" << prompt << " error=" << error.what() );
    }
    CHECK( stage == 8 );
    CHECK( prompt == 4 );
    CHECK( calendar::turn == turn );
    CHECK( you.get_moves() == moves );
    CHECK( ground_count() == 40 );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == world.surface_id() );
    std::vector<item_location> targets;
    std::vector<int> quantities;
    for( const drop_location &entry : selected ) {
        targets.push_back( entry.first );
        quantities.push_back( entry.second );
    }
    int selected_count = 0;
    for( const int quantity : quantities ) {
        selected_count += quantity;
    }
    CHECK( selected_count == ( commit ? 3 : 0 ) );
    if( commit ) {
        bool stashed = true;
        Pickup::pick_info info;
        you.set_moves( 10000 );
        CHECK( Pickup::do_pickup( targets, quantities, false, stashed, info ) );
        CHECK( targets.empty() );
    }
    CHECK( ground_count() == ( commit ? 37 : 40 ) );
    CHECK( ( charges ? you.charges_of( type ) : you.amount_of( type ) ) == ( commit ? 3 : 0 ) );
}

#endif
