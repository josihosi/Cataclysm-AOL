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
