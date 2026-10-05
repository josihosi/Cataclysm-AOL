#if defined(TILES)
#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "activity_actor_definitions.h"
#include "avatar.h"
#include "cached_options.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "game.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "item.h"
#include "item_pocket.h"
#include "inventory_ui.h"
#include "json_loader.h"
#include "input.h"
#include "map.h"
#include "output.h"
#include "recipe.h"
#include "json.h"
#include "map_helpers.h"
#include "npc.h"
#include "npc_opinion.h"
#include "npctrade.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "sdl_renderer_recovery.h"
#include "semantic_surface.h"
#include "ui_manager.h"

namespace
{
std::string activity_bytes( const player_activity &activity )
{
    std::ostringstream out;
    JsonOut json( out );
    activity.serialize( json );
    return out.str();
}

ImGuiWindow *active_progress_popup()
{
    for( ImGuiWindow *window : GImGui->Windows ) {
        if( window->Active && std::string( window->Name ).rfind( "QUERY_POPUP##", 0 ) == 0 ) {
            return window;
        }
    }
    return nullptr;
}
} // namespace

TEST_CASE( "activity popup is covered by native trade and restored on exit",
           "[tiles][trade][trade_wait_overlay_067]" )
{
    const std::string exit = GENERATE( std::string( "cancel" ), std::string( "prompt_no" ),
                                     std::string( "prompt_yes" ) );
    const std::string activity = GENERATE( std::string( "wait" ), std::string( "craft" ),
                                         std::string( "sort" ), std::string( "sleep" ) );
    INFO( exit << " activity=" << activity );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    npc &trader = spawn_npc( you.pos_bub().xy() + point::east, "test_talker" );
    clear_character( trader );
    trader.set_attitude( NPCATT_NULL );
    trader.op_of_u.trust = 100;
    REQUIRE_FALSE( trader.will_exchange_items_freely() );
    REQUIRE( trader.max_credit_extended() >= 1000 );
    REQUIRE( you.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    REQUIRE( trader.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    you.i_add( item( itype_id( "smart_phone" ) ), false );
    trader.i_add( item( itype_id( "wallet" ) ), false );

    software_render_fixture renderer;
    REQUIRE( renderer.available() ); // This test must draw, not silently skip.
    renderer_recovery_test_support::setup_software_ui();
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    on_out_of_scope reset_progress( [] { g->wait_popup_reset(); } );
    semantic_surface_manager manager( "trade-render" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    int sequence = 0;
    int requests = 0;
    int receipts = 0;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        ++receipts;
        REQUIRE( receipt.accepted );
    } );
    const auto submit = [&]( const semantic_surface_descriptor &descriptor, const std::string &id,
    const std::optional<std::string> &stable = std::nullopt ) {
        ++requests;
        REQUIRE( manager.submit_request( { manager.run_id(), descriptor.surface_id,
                                           descriptor.frame_id, std::to_string( ++sequence ), id, stable, {} } ) );
    };
    if( activity == "wait" ) {
        you.assign_activity( wait_activity_actor( 1_hours ) );
        you.activity.moves_left -= 100;
    } else if( activity == "sort" ) {
        you.assign_activity( zone_sort_activity_actor() );
    } else if( activity == "craft" ) {
        item ingredient( itype_id( "2x4" ), calendar::turn );
        item craft( &recipe_id( "cudgel_test_steps_basic" ).obj(), 1, ingredient );
        craft.item_counter = 2500000;
        item_location work = you.i_add( craft, false );
        REQUIRE( work );
        you.activity = player_activity( craft_activity_actor( work, false ) );
        you.activity.targets = { work };
    } else {
        you.add_effect( efftype_id( "sleep" ), 1_hours );
    }
    const std::string before = activity_bytes( you.activity );
    const auto turn = calendar::turn;
    const int moves = you.get_moves();
    const size_t backlog = you.backlog.size();
    std::vector<uint32_t> unobscured_debt;

    const SDL_Rect debt_rect{ 320, 32, 320, 16 };
    const SDL_Rect trader_items{ 12, 112, 444, 160 };
    const SDL_Rect player_items{ 492, 112, 444, 160 };
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind == "inventory" ) {
            ui_manager::redraw();
            ui_manager::redraw();
            unobscured_debt = renderer_recovery_test_support::read_display_pixels( debt_rect );

            REQUIRE_FALSE( active_progress_popup() );
            submit( descriptor, "inventory.cancel" );
        }
    } );
    REQUIRE_FALSE( npc_trading::trade( trader, 1000, "Shakedown payment", 0, 0, nullptr, &you, true ) );
    REQUIRE_FALSE( unobscured_debt.empty() );
    // Actual rendered debt text has foreground pixels, rather than an empty region.
    REQUIRE( std::adjacent_find( unobscured_debt.begin(), unobscured_debt.end(),
    std::not_equal_to<uint32_t>() ) != unobscured_debt.end() );

    g->handle_progress_ui();
    ui_manager::redraw();
    ImGuiWindow *const progress = active_progress_popup();
    REQUIRE( progress );
    REQUIRE( progress->Pos.y < 48 );
    if( activity != "sleep" ) { REQUIRE( progress->Pos.y + progress->Size.y > 32 ); }
    bool trade_drawn = false;
    bool prompt_drawn = false;
    bool prompt_refused = false;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "inventory" && descriptor.kind != "prompt" ) {
            return;
        }
        ui_manager::redraw();
        ui_manager::redraw();
        CHECK_FALSE( progress->Active );
        CHECK( activity_bytes( you.activity ) == before );
        CHECK( calendar::turn == turn );
        CHECK( you.get_moves() == moves );
        if( descriptor.kind == "prompt" ) {
            prompt_drawn = true;
            REQUIRE( active_progress_popup() );
            const bool yes = exit == "prompt_yes";
            const auto choice = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            [yes]( const semantic_action_descriptor &action ) {
                return action.id == "prompt.choose" && action.label == ( yes ? "YES" : "NO" );
            } );
            REQUIRE( choice != descriptor.valid_actions.end() );
            prompt_refused = !yes;
            submit( descriptor, choice->id, choice->stable_id );
        } else {
            trade_drawn = true;
            CHECK_FALSE( active_progress_popup() );
            CHECK( ( renderer_recovery_test_support::read_display_pixels( debt_rect ) == unobscured_debt ) );
            for( const SDL_Rect &pane : { trader_items, player_items } ) {
                const auto pixels = renderer_recovery_test_support::read_display_pixels( pane );
                CHECK( std::any_of( pixels.begin(), pixels.end(), []( uint32_t pixel ) {
                    return pixel != 0xff000000;
                } ) );
            }

            submit( descriptor, exit == "cancel" || prompt_refused ?
                    "inventory.cancel" : "inventory.commit" );
        }
    } );
    CHECK( npc_trading::trade( trader, 1000, "Shakedown payment", 0, 0, nullptr, &you, true ) ==
           ( exit == "prompt_yes" ) );
    CHECK( trade_drawn );
    CHECK( prompt_drawn == ( exit == "prompt_yes" || exit == "prompt_no" ) );
    CHECK( receipts == requests );
    CHECK_FALSE( manager.has_pending_request() );
    CHECK( manager.top()->kind == "world" );
    CHECK( active_semantic_surface_manager() == &manager );
    CHECK( activity_bytes( you.activity ) == before );
    CHECK( calendar::turn == turn );
    CHECK( you.get_moves() == moves );
    CHECK( you.backlog.size() == backlog );
    ui_manager::redraw();
    ui_manager::redraw();
    CHECK( progress->Active );
}

TEST_CASE( "rendered Trade publishes native letters cells pages and filtered payer rows",
           "[tiles][trade][trade_rows_render_067]" )
{
    const bool camp_payer = GENERATE( false, true );
    const bool collapsed = GENERATE( false, true );
    restore_on_out_of_scope<inventory_selector_save_state> restore_inventory( inventory_ui_default_state );
    inventory_ui_default_state.uimode = collapsed ? inventory_selector::uimode::hierarchy :
                                      inventory_selector::uimode::categories;
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    npc &trader = spawn_npc( you.pos_bub().xy() + point::east, "thug" );
    clear_character( trader );
    trader.set_fac( faction_id( "hells_raiders" ) );
    trader.set_attitude( NPCATT_NULL );
    Character *payer = &you;
    if( camp_payer ) {
        npc &resident = spawn_npc( you.pos_bub().xy() + point( 4, 0 ), "test_talker" );
        clear_character( resident );
        resident.set_fac( faction_id( "your_followers" ) );
        resident.set_attitude( NPCATT_FOLLOW );
        payer = &resident;
    }
    REQUIRE( payer->wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    item_location ammo = payer->i_add( item( itype_id( "9mm" ), calendar::turn_zero, 5 ), false );
    REQUIRE( ammo );
    ammo->set_owner( *payer );
    ammo->invlet = 'a';
    const std::string ammo_uid = std::to_string( ammo->uid().get_value() );
    for( int i = 0; i < 80; ++i ) {
        item separate( itype_id( "wallet" ) );
        separate.set_var( "trade_group", i );
        separate.set_owner( *payer );
        REQUIRE( payer->i_add( separate, false ) );
    }
    if( collapsed ) {
        REQUIRE( ammo.has_parent() );
        for( item_pocket *pocket : ammo.parent_item()->get_standard_pockets() ) {
            pocket->settings.set_collapse( true );
        }
    }
    REQUIRE( trader.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    item_location phone = trader.i_add( item( itype_id( "smart_phone" ) ), false );
    REQUIRE( phone );
    phone->set_owner( trader );
    phone->invlet = 'p';
    software_render_fixture renderer;
    REQUIRE( renderer.available() );
    renderer_recovery_test_support::setup_software_ui();
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    const int input_timeout = inp_mngr.get_timeout();
    on_out_of_scope restore_timeout( [&] { inp_mngr.set_timeout( input_timeout ); } );
    inp_mngr.set_timeout( 0 ); // Let synchronous test-only filter submission return to the real redraw loop.
    semantic_surface_manager manager( "trade-rows-render" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    const auto before_turn = calendar::turn;
    const auto before_moves = payer->get_moves();
    int stage = 0;
    int sequence = 0;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "inventory" ) { return; }
        JsonArray rows = json_loader::from_string( descriptor.payload.at( "trade_rows" ) );
        const auto submit = [&]( const std::string &id, std::map<std::string, std::string> params = {} ) {
            REQUIRE( manager.submit_request( { manager.run_id(), descriptor.surface_id,
                                               descriptor.frame_id, std::to_string( ++sequence ), id, std::nullopt, params } ) );
            // A filter has no keyboard action; exercise the same transport-wake return as a real request.
            manager.mark_transport_wake();
        };
        CHECK( calendar::turn == before_turn );
        CHECK( payer->get_moves() == before_moves );
        if( stage == 0 ) {
            CHECK( descriptor.payload.at( "active_party" ) == "npc" );
            bool found = false;
            for( JsonObject row : rows ) {
                row.allow_omitted_members();
                if( row.get_string( "group_uid" ) == std::to_string( phone->uid().get_value() ) ) {
                    found = true;
                    CHECK( row.get_string( "letter" ) == "p" );
                    CHECK( row.get_string( "unit_price" ) == format_money( npc_trading::trading_price( *payer, trader, { phone, 1 } ) ) );
                }
            }
            CHECK( found );
            ++stage;
            submit( "trade.switch_pane" );
        } else if( stage == 1 ) {
            CHECK( descriptor.payload.at( "active_party" ) == "player" );
            bool next_page = false;
            bool hidden = false;
            for( JsonObject row : rows ) {
                row.allow_omitted_members();
                if( !row.get_bool( "native_row_available" ) ) {
                    hidden = true;
                    CHECK( row.get_member( "native_page" ).test_null() );
                    CHECK( row.get_member( "letter" ).test_null() );
                } else {
                    next_page |= row.get_int( "native_page" ) > 0;
                }
            }
            if( collapsed ) {
                CHECK( hidden );
                stage = 3;
                submit( "inventory.cancel" );
                return;
            }
            CHECK( next_page );
            ++stage;
            submit( "inventory.filter", { { "text", "9x19" } } );
        } else {
            REQUIRE( stage == 2 );
            CHECK( descriptor.payload.at( "filter" ) == "9x19" );
            REQUIRE( rows.size() == 1 );
            JsonObject row = rows.get_object( 0 );
            row.allow_omitted_members();
            CHECK( row.get_string( "group_uid" ) == ammo_uid );
            CHECK( row.get_string( "letter" ) == "a" );
            CHECK( row.get_int( "available" ) == 5 );
            CHECK( row.get_bool( "on_native_page" ) );
            CHECK( row.get_string( "unit_price" ) == format_money( npc_trading::trading_price( trader, *payer, { ammo, 1 } ) ) );
            const auto pixels = renderer_recovery_test_support::read_display_pixels( SDL_Rect{ 492, 112, 444, 160 } );
            CHECK( std::any_of( pixels.begin(), pixels.end(), []( uint32_t p ) { return p != 0xff000000; } ) );
            ++stage;
            submit( "inventory.cancel" );
        }
    } );
    CHECK_FALSE( npc_trading::trade( trader, 1000, "Rendered local payment", 0, 0, nullptr, payer, true ) );
    CHECK( stage == 3 );
    CHECK( payer->charges_of( itype_id( "9mm" ) ) == 5 );
    CHECK( phone.held_by( trader ) );
    CHECK_FALSE( manager.has_pending_request() );
    CHECK( manager.top()->surface_id == world.surface_id() );
}
#endif
