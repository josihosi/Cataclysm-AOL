#include <algorithm>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "game_inventory.h"
#include "imgui/imgui.h"
#include "item.h"
#include "item_location.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "output.h"
#include "player_helpers.h"
#include "semantic_surface.h"
#include "translations.h"
#include "uistate.h"

TEST_CASE( "pickup explains a native pocket denial and permits read-only details",
           "[semantic_surface][pickup_denial]" )
{
    std::string operation = "select";
    SECTION( "details and storage selection" ) {}
    SECTION( "wield an item that cannot be stored" ) {
        operation = "inventory.wield";
    }
    SECTION( "wear an item that cannot be stored" ) {
        operation = "inventory.wear";
    }
    clear_avatar();
    clear_map();
    on_out_of_scope clear_reopened_menu( []() {
        uistate.open_menu.reset();
    } );
    restore_on_out_of_scope<int> restore_width( TERMX );
    restore_on_out_of_scope<int> restore_height( TERMY );
    restore_on_out_of_scope<int> restore_full_width( FULL_SCREEN_WIDTH );
    restore_on_out_of_scope<int> restore_full_height( FULL_SCREEN_HEIGHT );
    TERMX = FULL_SCREEN_WIDTH = 80;
    TERMY = FULL_SCREEN_HEIGHT = 24;
    const bool owns_imgui = ImGui::GetCurrentContext() == nullptr;
    if( owns_imgui ) {
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2( 800, 600 );
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->AddFontDefault();
        io.Fonts->Build();
        ImGui::NewFrame();
    }
    on_out_of_scope cleanup_imgui( [&]() {
        if( owns_imgui ) {
            ImGui::EndFrame();
            ImGui::DestroyContext();
        }
    } );
    avatar &you = get_avatar();
    // clear_avatar is naked: supply an ordinary pocket so this fixture can
    // distinguish an oversized item from a small, genuinely storable one.
    you.wear_item( item( itype_id( "pants" ) ) );
    REQUIRE( you.is_wearing( itype_id( "pants" ) ) );
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    const itype_id large_type( operation == "inventory.wear" ? "backpack" : "log" );
    item &log = here.add_item_or_charges( position, item( large_type ) );
    const std::string log_uid = std::to_string( log.uid().get_value() );
    const std::string log_name = log.tname();
    REQUIRE_FALSE( you.can_pickVolume_partial( log, false, nullptr, false, true ) );
    REQUIRE( you.can_pickWeight_partial( log, false ) );
    item &small = here.add_item_or_charges( position, item( itype_id( "smart_phone" ) ) );
    const std::string small_uid = std::to_string( small.uid().get_value() );
    REQUIRE( you.can_pickVolume_partial( small, false, nullptr, false, true ) );
    const time_point turn = calendar::turn;
    const int moves = you.get_moves();
    semantic_surface_manager manager( "pickup-denial-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope parent( manager, "world", "World" );
    int state = 0;
    int request_sequence = 0;
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        auto submit = [&]( const std::string &action, const std::optional<std::string> &uid = std::nullopt ) {
            REQUIRE( manager.submit_request( { "pickup-denial-run", descriptor.surface_id,
                                               descriptor.frame_id, std::to_string( ++request_sequence ), action, uid, {} } ) );
        };
        if( descriptor.kind == "inventory" ) {
            const auto advertised = [&]( const std::string &action, const std::string &uid ) ->
            const semantic_action_descriptor & {
                const auto found = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
                [&]( const semantic_action_descriptor &candidate ) {
                    return candidate.id == action && candidate.stable_id == uid;
                } );
                REQUIRE( found != descriptor.valid_actions.end() );
                return *found;
            };
            const auto &blocked = advertised( "inventory.select", log_uid );
            CHECK_FALSE( blocked.enabled );
            CHECK( blocked.label == log_name + " — " + _( "Does not fit in any pocket!" ) );
            CHECK( advertised( "inventory.details", log_uid ).enabled );
            CHECK( advertised( "inventory.wield", log_uid ).enabled );
            CHECK( advertised( "inventory.wear", log_uid ).enabled == ( operation == "inventory.wear" ) );
            CHECK( advertised( "inventory.select", small_uid ).enabled );
            if( operation != "select" ) {
                REQUIRE( state == 0 );
                state = 4;
                submit( operation, log_uid );
            } else if( state == 0 ) {
                CHECK( descriptor.payload.at( "selected_items" ) == "{}" );
                state = 1;
                submit( "inventory.details", log_uid );
            } else if( state == 2 ) {
                CHECK( descriptor.payload.at( "selected_items" ) == "{}" );
                state = 3;
                submit( "inventory.toggle", small_uid );
            } else {
                REQUIRE( state == 3 );
                const JsonObject selected = json_loader::from_string( descriptor.payload.at( "selected_items" ) );
                CHECK_FALSE( selected.has_member( log_uid ) );
                REQUIRE( selected.has_member( small_uid ) );
                selected.allow_omitted_members();
                state = 4;
                submit( "inventory.commit" );
            }
        } else if( descriptor.kind == "item_info" ) {
            REQUIRE( state == 1 );
            CHECK( descriptor.payload.at( "item_name" ) == log_name );
            state = 2;
            submit( "item_info.close" );
        }
    } );
    const drop_locations selected = game_menus::inv::pickup( { position } );
    if( operation != "select" ) {
        CHECK( selected.empty() );
        CHECK( state == 4 );
        REQUIRE( receipts.size() == 1 );
        CHECK( receipts.front().accepted );
        REQUIRE_FALSE( you.activity.is_null() );
        process_activity( you );
        if( operation == "inventory.wield" ) {
            REQUIRE( you.get_wielded_item() );
            CHECK( you.get_wielded_item()->typeId() == large_type );
        } else {
            CHECK( you.is_wearing( large_type ) );
        }
        REQUIRE( here.i_at( position ).size() == 1 );
        CHECK( here.i_at( position ).begin()->uid() == small.uid() );
        return;
    }
    REQUIRE( selected.size() == 1 );
    CHECK( std::to_string( selected.front().first->uid().get_value() ) == small_uid );
    CHECK( selected.front().second == 1 );
    CHECK( state == 4 );
    REQUIRE( receipts.size() == 4 );
    for( const semantic_action_receipt &receipt : receipts ) {
        CHECK( receipt.accepted );
        CHECK_FALSE( receipt.resulting_frame_id.empty() );
    }
    CHECK( calendar::turn == turn );
    CHECK( you.get_moves() == moves );
}

#if !defined(TILES)
#include <clocale>
#include <cstdlib>
#include <optional>
#include <map>
#include "compatibility.h"
#include "cached_options.h"
#include "do_turn.h"
#include "options_helpers.h"
#include "worldfactory.h"
#include "cursesdef.h"
#include "game.h"
#include "input.h"
#include "player_activity.h"
extern "C" int ungetch( int );
extern "C" int flushinp();

namespace
{
class pickup_environment
{
    public:
        pickup_environment( const char *name, const std::string &value ) : name( name ) {
            if( const char *old = std::getenv( name ) ) {
                previous = old;
            }
            setenv( name, value.c_str(), 1 );
        }
        ~pickup_environment() {
            if( previous ) {
                setenv( name.c_str(), previous->c_str(), 1 );
            } else {
                unsetenv( name.c_str() );
            }
        }
    private:
        std::string name;
        std::optional<std::string> previous;
};
}

TEST_CASE( "queued pickup after Wield owns a fresh native cancel and World successor",
           "[semantic_surface][pickup_denial][queued_pickup_067]" )
{
    const bool outer = GENERATE( false, true );
    CAPTURE( outer );
    const std::string run = outer ? "queued-pickup-outer" : "queued-pickup-bare";
    pickup_environment run_env( "OPENCLAW_HARNESS_RUN_ID", run );
    pickup_environment bound_env( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run );
    pickup_environment request_env( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", "" );
    auto &manager = openclaw_harness_semantic_surface_manager();
    REQUIRE( manager.stack().empty() );
    semantic_surface_manager enclosing( run + "-enclosing" );
    semantic_surface_manager &pickup_manager = outer ? enclosing : manager;
    std::optional<semantic_surface_manager_session> enclosing_session;
    if( outer ) {
        enclosing_session.emplace( enclosing );
    }
    const auto expected_manager = active_semantic_surface_manager();
    clear_avatar();
    clear_map();
    // The public turn caller needs the ordinary native game mode, which Catch
    // has not started. Restore it through the established save/load path.
    REQUIRE( world_generator->active_world );
    const std::string world_name = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world_name ) );
    clear_avatar();
    clear_map();
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    restore_on_out_of_scope<time_point> restore_turn( calendar::turn );
    restore_on_out_of_scope<decltype( g->uquit )> restore_quit( g->uquit );
    g->uquit = QUIT_NO;
    const std::string locale = std::setlocale( LC_CTYPE, nullptr );
    REQUIRE( std::setlocale( LC_CTYPE, "en_US.UTF-8" ) );
    on_out_of_scope restore_locale( [&]() { std::setlocale( LC_CTYPE, locale.c_str() ); } );
    static bool initialized = false;
    if( !initialized ) {
        catacurses::init_interface();
        catacurses::resizeterm();
        initialized = true;
    }
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    const int timeout = inp_mngr.get_timeout();
    inp_mngr.set_timeout( 1 );
    on_out_of_scope cleanup( [&]() {
        uistate.open_menu.reset();
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
        enclosing.set_descriptor_observer( {} );
        enclosing.set_receipt_observer( {} );
        inp_mngr.set_timeout( timeout );
        ::flushinp();
        catacurses::endwin();
    } );
    avatar &you = get_avatar();
    you.wear_item( item( itype_id( "pants" ) ) );
    map &here = get_map();
    const auto position = you.pos_bub();
    item &large = here.add_item_or_charges( position, item( itype_id( "log" ) ) );
    const auto large_uid = std::to_string( large.uid().get_value() );
    item &small = here.add_item_or_charges( position, item( itype_id( "smart_phone" ) ) );
    const auto small_uid = std::to_string( small.uid().get_value() );
    REQUIRE_FALSE( you.can_pickVolume_partial( large, false, nullptr, false, true ) );
    semantic_surface_descriptor first;
    semantic_surface_descriptor reopened;
    std::map<std::string, semantic_action_receipt> receipts;
    int phase = 0;
    const auto observe = [&]( const semantic_surface_descriptor &descriptor ) {
        auto &owner = descriptor.kind == "world" ? manager : pickup_manager;
        const auto submit = [&]( const std::string &id, const std::string &action,
                                 std::optional<std::string> uid = std::nullopt ) {
            REQUIRE( owner.submit_request( { descriptor.run_id, descriptor.surface_id,
                                             descriptor.frame_id, id, action, uid, {} } ) );
        };
        if( descriptor.kind == "inventory" && phase == 0 ) {
            first = descriptor;
            phase = 1;
            submit( "wield-once", "inventory.wield", large_uid );
        } else if( descriptor.kind == "inventory" && phase == 2 ) {
            reopened = descriptor;
            phase = 3;
            CHECK( descriptor.surface_id != first.surface_id );
            auto invalid = semantic_action_request{ descriptor.run_id, first.surface_id,
                           first.frame_id, "stale-original", "inventory.cancel", std::nullopt, {} };
            REQUIRE( owner.submit_request( invalid ) );
            CHECK_FALSE( owner.consume_top_request() );
            invalid.surface_id = descriptor.surface_id;
            invalid.frame_id = descriptor.frame_id;
            invalid.run_id = "foreign-run";
            invalid.request_id = "foreign-run";
            REQUIRE( owner.submit_request( invalid ) );
            CHECK_FALSE( owner.consume_top_request() );
            CHECK_FALSE( owner.submit_request( { first.run_id, first.surface_id, first.frame_id,
                                                "wield-once", "inventory.wield", large_uid, {} } ) );
            CHECK_FALSE( owner.has_pending_request() );
            submit( "reopened-cancel", "inventory.cancel" );
        } else if( descriptor.kind == "world" && phase >= 2 && phase < 5 ) {
            const bool second_turn = phase == 4;
            phase = second_turn ? 5 : 4;
            submit( second_turn ? "world-next-turn" : "world-successor", "world.pause" );
        }
    };
    const auto receipt = [&]( const semantic_action_receipt &value ) {
        receipts[value.request_id] = value;
    };
    manager.set_descriptor_observer( observe );
    manager.set_receipt_observer( receipt );
    enclosing.set_descriptor_observer( observe );
    enclosing.set_receipt_observer( receipt );
    {
        semantic_surface_manager_session initial( pickup_manager );
        CHECK( game_menus::inv::pickup( { position } ).empty() );
        REQUIRE( phase == 1 );
        REQUIRE_FALSE( you.activity.is_null() );
        process_activity( you );
    }
    REQUIRE( you.get_wielded_item() );
    CHECK( you.get_wielded_item()->typeId() == itype_id( "log" ) );
    REQUIRE( uistate.open_menu );
    REQUIRE( active_semantic_surface_manager() == expected_manager );
    phase = 2;
    // A normal keyboard cancellation bounds the unfixed before control. It
    // neither creates semantic authority nor repairs an unknown native action.
    REQUIRE( ::ungetch( 27 ) != -1 );
    you.set_moves( 100 );
    CHECK_FALSE( g->do_turn() );
    ::flushinp();
    REQUIRE_FALSE( reopened.surface_id.empty() );
    REQUIRE( phase == 4 );
    CHECK_FALSE( uistate.open_menu );
    CHECK( active_semantic_surface_manager() == expected_manager );
    REQUIRE( receipts.count( "reopened-cancel" ) == 1 );
    CHECK( receipts.at( "reopened-cancel" ).accepted );
    CHECK_FALSE( receipts.at( "stale-original" ).accepted );
    CHECK_FALSE( receipts.at( "foreign-run" ).accepted );
    CHECK( you.activity.is_null() );
    REQUIRE( here.i_at( position ).size() == 1 );
    CHECK( std::to_string( here.i_at( position ).begin()->uid().get_value() ) == small_uid );
    you.set_moves( 100 );
    CHECK_FALSE( g->do_turn() );
    CHECK( phase == 5 );
    CHECK( manager.stack().empty() );
    CHECK( pickup_manager.stack().empty() );
    CHECK( active_semantic_surface_manager() == expected_manager );
    REQUIRE( receipts.count( "world-successor" ) == 1 );
    CHECK( receipts.at( "world-successor" ).accepted );
}
#endif
