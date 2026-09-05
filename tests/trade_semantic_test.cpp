#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "character_id.h"
#include "imgui/imgui.h"
#include "item.h"
#include "item_location.h"
#include "json.h"
#include "json_loader.h"
#include "map_helpers.h"
#include "npc.h"
#include "npctrade.h"
#include "player_helpers.h"
#include "semantic_surface.h"
#include "type_id.h"

namespace
{
class trade_imgui_context
{
    public:
        trade_imgui_context() : owns( ImGui::GetCurrentContext() == nullptr ) {
            if( owns ) {
                ImGui::CreateContext();
                ImGuiIO &io = ImGui::GetIO();
                io.DisplaySize = ImVec2( 800.0f, 600.0f );
                io.DeltaTime = 1.0f / 60.0f;
                io.Fonts->AddFontDefault();
                io.Fonts->Build();
                ImGui::NewFrame();
            }
        }
        ~trade_imgui_context() {
            if( owns ) {
                ImGui::EndFrame();
                ImGui::DestroyContext();
            }
        }
    private:
        bool owns;
};
} // namespace

TEST_CASE( "native trade switches identified parties and preserves selected quantities",
           "[semantic_surface][trade]" )
{
    const bool complete_trade = GENERATE( false, true );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    npc &trader = spawn_npc( you.pos_bub().xy() + point::east, "test_talker" );
    clear_character( trader );
    trader.set_fac( faction_id( "your_followers" ) );
    trader.set_attitude( NPCATT_FOLLOW );
    REQUIRE( trader.will_exchange_items_freely() );
    REQUIRE( you.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    REQUIRE( trader.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    const itype_id ammo_type( "9mm" );
    const itype_id phone_type( "smart_phone" );
    item_location ammo = you.i_add( item( ammo_type, calendar::turn_zero, 5 ), false );
    item_location phone = trader.i_add( item( phone_type ), false );
    const item_location bystander = you.i_add( item( itype_id( "wallet" ) ), false );
    REQUIRE( ammo );
    REQUIRE( phone );
    REQUIRE( bystander );
    ammo->set_owner( you );
    phone->set_owner( trader );
    const std::string ammo_uid = std::to_string( ammo->uid().get_value() );
    const std::string phone_uid = std::to_string( phone->uid().get_value() );
    const std::string trader_id = "character:" + std::to_string( trader.getID().get_value() );
    const std::string player_id = "character:" + std::to_string( you.getID().get_value() );
    const int ammo_before = you.charges_of( ammo_type );
    const int phone_before = trader.amount_of( phone_type );
    const int player_phone_before = you.amount_of( phone_type );
    const int trader_ammo_before = trader.charges_of( ammo_type );
    trade_imgui_context imgui;
    semantic_surface_manager manager( "trade-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    int stage = 0;
    int sequence = 0;
    bool confirmed = false;
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
        CHECK( receipt.accepted );
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & descriptor ) {
        const auto submit = [&]( const std::string & action,
        const std::optional<std::string> &uid = std::nullopt ) {
            const auto found = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            [&]( const semantic_action_descriptor & candidate ) {
                return candidate.id == action && candidate.stable_id == uid.value_or( "" ) &&
                       candidate.enabled;
            } );
            REQUIRE( found != descriptor.valid_actions.end() );
            REQUIRE( manager.submit_request( { "trade-run", descriptor.surface_id, descriptor.frame_id,
                                               std::to_string( ++sequence ), action, uid, {} } ) );
        };
        if( descriptor.kind == "prompt" ) {
            REQUIRE( complete_trade );
            CHECK( descriptor.payload.at( "text" ).find( "Accept this trade" ) != std::string::npos );
            const auto yes = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            []( const semantic_action_descriptor & action ) {
                return action.id == "prompt.choose" && action.label == "YES" && action.enabled;
            } );
            REQUIRE( yes != descriptor.valid_actions.end() );
            confirmed = true;
            submit( yes->id, yes->stable_id );
            return;
        }
        if( descriptor.kind != "inventory" ) {
            return;
        }
        for( const std::string &breadcrumb : descriptor.breadcrumbs ) {
            CHECK_FALSE( breadcrumb.empty() );
        }
        CHECK_FALSE( descriptor.payload.at( "title" ).empty() );
        CHECK( descriptor.payload.at( "trader_actor_id" ) == trader_id );
        CHECK( descriptor.payload.at( "player_actor_id" ) == player_id );
        const JsonObject selected = json_loader::from_string( descriptor.payload.at( "selected_items" ) );
        const auto selected_item = [&selected]( const std::string &uid ) {
            JsonObject entry = selected.get_object( uid );
            entry.allow_omitted_members();
            return entry;
        };
        if( stage == 0 ) {
            CHECK( descriptor.payload.at( "active_actor_id" ) == trader_id );
            ++stage;
            submit( "inventory.toggle", phone_uid );
        } else if( stage == 1 ) {
            CHECK( selected_item( phone_uid ).get_int( "count" ) == 1 );
            ++stage;
            submit( "trade.switch_pane" );
        } else if( stage == 2 ) {
            CHECK( descriptor.payload.at( "active_party" ) == "player" );
            CHECK( descriptor.payload.at( "active_actor_id" ) == player_id );
            ++stage;
            submit( "inventory.increase_quantity", ammo_uid );
        } else if( stage == 3 ) {
            CHECK( selected_item( ammo_uid ).get_int( "count" ) == 1 );
            ++stage;
            submit( "inventory.increase_quantity", ammo_uid );
        } else if( stage == 4 ) {
            CHECK( selected_item( ammo_uid ).get_int( "count" ) == 2 );
            CHECK( selected_item( ammo_uid ).get_string( "unit" ) == "charges" );
            ++stage;
            submit( "inventory.decrease_quantity", ammo_uid );
        } else if( stage == 5 ) {
            CHECK( selected_item( ammo_uid ).get_int( "count" ) == 1 );
            ++stage;
            submit( "trade.switch_pane" );
        } else {
            REQUIRE( stage == 6 );
            CHECK( descriptor.payload.at( "active_actor_id" ) == trader_id );
            CHECK( selected_item( phone_uid ).get_int( "count" ) == 1 );
            ++stage;
            submit( complete_trade ? "inventory.commit" : "inventory.cancel" );
        }
    } );
    CHECK( npc_trading::trade( trader, 0, "Trade test", 0 ) == complete_trade );
    CHECK( stage == 7 );
    CHECK( confirmed == complete_trade );
    CHECK( you.charges_of( ammo_type ) == ammo_before - ( complete_trade ? 1 : 0 ) );
    CHECK( trader.charges_of( ammo_type ) == trader_ammo_before + ( complete_trade ? 1 : 0 ) );
    CHECK( trader.amount_of( phone_type ) == phone_before - ( complete_trade ? 1 : 0 ) );
    CHECK( you.amount_of( phone_type ) == player_phone_before + ( complete_trade ? 1 : 0 ) );
    CHECK( bystander );
    CHECK( bystander.held_by( you ) );
    CHECK( receipts.size() == ( complete_trade ? 8 : 7 ) );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == world.surface_id() );
}
