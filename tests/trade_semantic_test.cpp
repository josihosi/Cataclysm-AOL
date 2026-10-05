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
#include "map.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "pocket_type.h"
#include "output.h"
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

TEST_CASE( "native trade Auto Balance uses highlighted physical coin group",
           "[semantic_surface][trade][trade_controls_067]" )
{
    const bool commit = GENERATE( false, true );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    npc &trader = spawn_npc( you.pos_bub().xy() + point::east, "thug" );
    clear_character( trader );
    trader.set_fac( faction_id( "hells_raiders" ) );
    trader.set_attitude( NPCATT_NULL );
    REQUIRE_FALSE( trader.is_shopkeeper() );
    trader.op_of_u.owed = 0;
    REQUIRE_FALSE( trader.will_exchange_items_freely() );
    REQUIRE( you.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    REQUIRE( trader.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    item_location coin;
    for( int i = 0; i < 300; ++i ) {
        coin = you.i_add( item( itype_id( "coin_gold" ) ), false );
        REQUIRE( coin );
        coin->set_owner( you );
    }
    const std::string uid = std::to_string( coin->uid().get_value() );
    const int price = npc_trading::trading_price( trader, you, { coin, 1 } );
    REQUIRE( price > 0 );
    trade_imgui_context imgui;
    semantic_surface_manager manager( "coin-controls" );
    semantic_surface_manager_session session( manager );
    int stage = 0;
    int sequence = 0;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & descriptor ) {
        const auto action = [&]( const std::string & id, const std::string & target = "" ) {
            const auto found = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            [&]( const semantic_action_descriptor & candidate ) {
                return candidate.id == id && candidate.stable_id == target;
            } );
            REQUIRE( found != descriptor.valid_actions.end() );
            return *found;
        };
        const auto submit = [&]( const std::string & id, const std::string & target = "" ) {
            REQUIRE( action( id, target ).enabled );
            REQUIRE( manager.submit_request( { "coin-controls", descriptor.surface_id, descriptor.frame_id,
                                               std::to_string( ++sequence ), id,
                                               target.empty() ? std::nullopt : std::optional<std::string>( target ), {} } ) );
        };
        if( descriptor.kind == "prompt" ) {
            REQUIRE( commit );
            const auto yes = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            []( const semantic_action_descriptor & candidate ) { return candidate.label == "YES"; } );
            REQUIRE( yes != descriptor.valid_actions.end() );
            submit( yes->id, yes->stable_id );
        } else if( descriptor.kind == "inventory" ) {
            if( stage == 0 ) {
                ++stage;
                submit( "trade.switch_pane" );
            } else if( stage == 1 ) {
                CHECK_FALSE( action( "inventory.decrease_quantity", uid ).enabled );
                ++stage;
                submit( "inventory.toggle", uid );
            } else if( stage == 2 ) {
                CHECK_FALSE( action( "inventory.increase_quantity", uid ).enabled );
                CHECK_FALSE( action( "trade.auto_balance" ).enabled );
                ++stage;
                submit( "inventory.toggle", uid );
            } else if( stage == 3 ) {
                CHECK( descriptor.payload.at( "highlighted_group_available" ) == "300" );
                CHECK( descriptor.payload.at( "highlighted_group_selected" ) == "0" );
                ++stage;
                submit( "trade.auto_balance" );
            } else {
                REQUIRE( stage == 4 );
                CHECK( descriptor.payload.at( "highlighted_group_selected" ) == "167" );
                JsonArray rows = json_loader::from_string( descriptor.payload.at( "trade_rows" ) );
                bool found_group = false;
                for( JsonObject row : rows ) {
                    row.allow_omitted_members();
                    bool has_coin = false;
                    for( JsonObject location : row.get_array( "locations" ) ) {
                        location.allow_omitted_members();
                        has_coin |= location.get_string( "uid" ) == uid;
                    }
                    if( has_coin ) {
                        found_group = true;
                        CHECK( row.get_int( "available" ) == 300 );
                        CHECK( row.get_int( "selected" ) == 167 );
                        CHECK( row.get_string( "unit" ) == "items" );
                        CHECK( row.get_string( "unit_price" ) == format_money( price ) );
                        CHECK( row.get_string( "selected_value" ) == descriptor.payload.at( "player_offer_value" ) );
                    }
                }
                CHECK( found_group );
                CHECK( action( "inventory.increase_quantity", uid ).enabled );
                CHECK( action( "inventory.decrease_quantity", uid ).enabled );
                ++stage;
                submit( commit ? "inventory.commit" : "inventory.cancel" );
            }
        }
    } );
    CHECK( npc_trading::trade( trader, 167 * price - 1, "Coin controls", 0 ) == commit );
    CHECK( stage == 5 );
    CHECK( you.amount_of( itype_id( "coin_gold" ) ) == ( commit ? 133 : 300 ) );
    int received = 0;
    trader.visit_items( [&]( const item *it, const item * ) {
        if( it->typeId() == itype_id( "coin_gold" ) ) {
            ++received;
        }
        return VisitResponse::NEXT;
    } );
    CHECK( received == ( commit ? 167 : 0 ) );
}


TEST_CASE( "native Trade rows and letters use exact payer groups and authenticated selection",
           "[semantic_surface][trade][trade_rows_067]" )
{
    const bool camp_payer = GENERATE( false, true );
    const bool commit = GENERATE( false, true );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    npc &trader = spawn_npc( you.pos_bub().xy() + point::east, "thug" );
    clear_character( trader );
    trader.set_fac( faction_id( "hells_raiders" ) );
    trader.set_attitude( NPCATT_NULL );
    trader.op_of_u.owed = 0;
    Character *payer = &you;
    if( camp_payer ) {
        npc &resident = spawn_npc( you.pos_bub().xy() + point( 4, 0 ), "test_talker" );
        clear_character( resident );
        resident.set_fac( faction_id( "your_followers" ) );
        resident.set_attitude( NPCATT_FOLLOW );
        payer = &resident;
    }
    REQUIRE( payer->wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    REQUIRE( trader.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    item_location ammo = payer->i_add( item( itype_id( "9mm" ), calendar::turn_zero, 5 ), false );
    REQUIRE( ammo );
    ammo->set_owner( *payer );
    ammo->invlet = 'a';
    std::string ammo_uid = std::to_string( ammo->uid().get_value() );
    item local( itype_id( "coin_gold" ) );
    local.set_owner( *payer );
    local.invlet = 'b';
    item_location coin = get_map().add_item_or_charges_ret_loc( payer->pos_bub() + tripoint::south, local );
    REQUIRE( coin );
    const std::string coin_uid = std::to_string( coin->uid().get_value() );
    item_location remote = get_map().add_item_or_charges_ret_loc( payer->pos_bub() + tripoint( 0, 6, 0 ), local );
    REQUIRE( remote );
    const std::string remote_uid = std::to_string( remote->uid().get_value() );
    item foreign( itype_id( "coin_gold" ) );
    foreign.set_owner( trader );
    item_location other_owner = get_map().add_item_or_charges_ret_loc( payer->pos_bub() + tripoint::north, foreign );
    REQUIRE( other_owner );
    const std::string foreign_uid = std::to_string( other_owner->uid().get_value() );
    item_location wallet = trader.i_add( item( itype_id( "wallet" ) ), false );
    REQUIRE( wallet );
    wallet->set_owner( trader );
    wallet->invlet = 'w';
    const std::string wallet_uid = std::to_string( wallet->uid().get_value() );
    const int coin_price = npc_trading::trading_price( trader, *payer, { coin, 1 } );
    const int ammo_price = npc_trading::trading_price( trader, *payer, { ammo, 5 } );
    REQUIRE( coin_price > 0 );
    REQUIRE( ammo_price > 0 );
    const int cost = coin_price + ammo_price;
    const std::string payer_id = "character:" + std::to_string( payer->getID().get_value() );
    trade_imgui_context imgui;
    semantic_surface_manager manager( "row-controls" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    int stage = 0;
    int sequence = 0;
    std::string letters;
    std::string last_error;
    int accepted = 0;
    int rejected = 0;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        if( receipt.accepted ) { ++accepted; } else { ++rejected; }
        last_error = receipt.rejection_reason;
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        const auto submit = [&]( const std::string &id,
        std::map<std::string, std::string> params = {},
        std::optional<std::string> uid = std::nullopt, std::string frame = "" ) {
            REQUIRE( manager.submit_request( { manager.run_id(), descriptor.surface_id,
                                               frame.empty() ? descriptor.frame_id : frame,
                                               std::to_string( ++sequence ), id, uid, params } ) );
        };
        if( descriptor.kind == "prompt" ) {
            REQUIRE( commit );
            const auto yes = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            []( const semantic_action_descriptor &action ) { return action.label == "YES"; } );
            REQUIRE( yes != descriptor.valid_actions.end() );
            submit( yes->id, {}, yes->stable_id );
            return;
        }
        if( descriptor.kind != "inventory" ) { return; }
        CHECK( descriptor.payload.at( "player_actor_id" ) == payer_id );
        CHECK( descriptor.payload.at( "demand_amount" ) == format_money( cost ) );
        JsonArray rows = json_loader::from_string( descriptor.payload.at( "trade_rows" ) );
        const auto row = [&]( const std::string &uid ) -> JsonObject {
            for( JsonObject candidate : rows ) {
                candidate.allow_omitted_members();
                if( candidate.get_string( "group_uid" ) == uid ) {
                    candidate.allow_omitted_members();
                    return candidate;
                }
            }
            FAIL( "Native group not advertised: " << uid );
            return JsonObject();
        };
        if( stage == 0 ) {
            JsonObject entry = row( wallet_uid );
            CHECK( entry.get_string( "party" ) == "npc" );
            CHECK( ( [&] { auto loc = entry.get_array( "locations" ).get_object( 0 ); loc.allow_omitted_members(); return loc.get_int( "carrier_id" ); }() ) == trader.getID().get_value() );
            ++stage;
            submit( "trade.switch_pane" );
            return;
        }
        CHECK( descriptor.payload.at( "active_party" ) == "player" );
        JsonObject a = row( ammo_uid );
        JsonObject c = row( coin_uid );
        CHECK( a.get_string( "unit" ) == "charges" );
        CHECK( a.get_int( "available" ) == 5 );
        CHECK( a.get_string( "unit_price" ) == format_money( npc_trading::trading_price( trader, *payer, { ammo, 1 } ) ) );
        CHECK( c.get_string( "unit_price" ) == format_money( coin_price ) );
        CHECK( ( [&] { auto loc = c.get_array( "locations" ).get_object( 0 ); loc.allow_omitted_members(); return loc.get_string( "root_source" ); }() ) == "map" );
        CHECK( ( [&] { auto loc = c.get_array( "locations" ).get_object( 0 ); loc.allow_omitted_members(); return loc.get_string( "owner_faction" ); }() ) == "your_followers" );
        for( JsonObject r : rows ) { r.allow_omitted_members(); CHECK( r.get_string( "group_uid" ) != remote_uid ); CHECK( r.get_string( "group_uid" ) != foreign_uid ); }
        const std::map<std::string, std::string> identity = {
            { "party", "player" }, { "actor_id", payer_id }, { "letters", letters }
        };
        if( stage == 1 ) {
            REQUIRE( a.get_string( "letter" ).size() == 1 );
            REQUIRE( c.get_string( "letter" ).size() == 1 );
            letters = a.get_string( "letter" ) + c.get_string( "letter" );
            REQUIRE( letters[0] != letters[1] );
            auto params = identity;
            params["letters"] = letters + letters.substr( 0, 1 );
            submit( "trade.toggle_letters", params );
            CHECK_FALSE( manager.consume_top_request() );
            CHECK( last_error == "duplicate_trade_letter" );
            params["letters"] = letters + " ";
            submit( "trade.toggle_letters", params );
            CHECK_FALSE( manager.consume_top_request() );
            CHECK( last_error == "unavailable_trade_letter" );
            params["letters"] = letters;
            params["actor_id"] = descriptor.payload.at( "trader_actor_id" );
            submit( "trade.toggle_letters", params );
            CHECK_FALSE( manager.consume_top_request() );
            CHECK( last_error == "foreign_trade_pane" );
            params["actor_id"] = payer_id;
            submit( "trade.toggle_letters", params, std::nullopt, descriptor.frame_id + ":stale" );
            CHECK_FALSE( manager.consume_top_request() );
            CHECK( last_error == "stale_frame" );
            CHECK( a.get_int( "selected" ) == 0 );
            CHECK( c.get_int( "selected" ) == 0 );
            ++stage;
            submit( "trade.toggle_letters", params );
        } else {
            REQUIRE( stage == 2 );
            CHECK( a.get_int( "selected" ) == 5 );
            CHECK( c.get_int( "selected" ) == 1 );
            CHECK( a.get_string( "selected_value" ) == format_money( ammo_price ) );
            CHECK( c.get_string( "selected_value" ) == format_money( coin_price ) );
            CHECK( descriptor.payload.at( "player_offer_value" ) == format_money( cost ) );
            CHECK( descriptor.payload.at( "balance" ) == format_money( 0 ) );
            ++stage;
            submit( commit ? "inventory.commit" : "inventory.cancel" );
        }
    } );
    CHECK( npc_trading::trade( trader, cost, "Local payment rows", 2, 0, nullptr, payer, true ) == commit );
    CHECK( stage == 3 );
    CHECK( rejected == 4 );
    CHECK( accepted == ( commit ? 4 : 3 ) );
    CHECK( payer->charges_of( itype_id( "9mm" ) ) == ( commit ? 0 : 5 ) );
    CHECK( trader.charges_of( itype_id( "9mm" ) ) == ( commit ? 5 : 0 ) );
    CHECK( get_map().i_at( payer->pos_bub() + tripoint::south ).empty() == commit );
    CHECK_FALSE( get_map().i_at( payer->pos_bub() + tripoint( 0, 6, 0 ) ).empty() );
    CHECK( wallet.held_by( trader ) );
    CHECK( manager.top()->surface_id == world.surface_id() );
}


TEST_CASE( "shakedown stash payment conserves the exact basket and survives map reload",
           "[semantic_surface][home_stash_067]" )
{
    const bool resident = GENERATE( false, true );
    const std::string outcome = GENERATE( std::string( "paid" ), std::string( "cancel" ),
                                         std::string( "blocked" ), std::string( "duplicate" ),
                                         std::string( "quantity" ), std::string( "overlap" ) );
    INFO( resident << " " << outcome );
    clear_avatar();
    clear_map();
    avatar &avatar_payer = get_avatar();
    npc &trader = spawn_npc( avatar_payer.pos_bub().xy() + point::east, "test_talker" );
    clear_character( trader );
    trader.set_fac( faction_id( "your_followers" ) );
    Character *payer = &avatar_payer;
    if( resident ) {
        npc &receiver = spawn_npc( avatar_payer.pos_bub().xy() + point::south, "test_talker" );
        clear_character( receiver );
        receiver.set_fac( faction_id( "your_followers" ) );
        payer = &receiver;
    }
    REQUIRE( payer->wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    item_location ammo = payer->i_add( item( itype_id( "9mm" ), calendar::turn_zero, 5 ), false );
    item decoy_goods( itype_id( "9mm" ), calendar::turn_zero, 7 );
    decoy_goods.set_var( "stash_unselected", 1 );
    item_location decoy = payer->i_add( decoy_goods, false );
    // Keep the unselected same-type charges in a distinct container.
    item purse( itype_id( "wallet" ) );
    for( int i = 0; i < 2; ++i ) {
        REQUIRE( purse.put_in( item( itype_id( "coin_gold" ) ),
                              pocket_type::CONTAINER ).success() );
    }
    item_location wallet = payer->i_add( purse, false );
    REQUIRE( ammo );
    REQUIRE( decoy );
    REQUIRE( wallet );
    // A distinct unselected source must not be debited by item type.
    REQUIRE( ammo.get_item() != decoy.get_item() );
    ammo->set_owner( *payer );
    wallet->set_owner( *payer );
    const auto payer_position = payer->pos_abs();
    const auto trader_position = trader.pos_abs();
    const int cash_before = payer->cash;
    const int owed_before = trader.op_of_u.owed;

    const tripoint_abs_omt home = avatar_payer.pos_abs_omt() + tripoint( 20, 20, 0 );
    REQUIRE_FALSE( get_map().inbounds( home ) );
    {
        tinymap home_map;
        home_map.load( home, false );
        map &map = *home_map.cast_to_map();
        for( const tripoint_bub_ms &tile : map.points_on_zlevel( home.z() ) ) {
            map.ter_set( tile, ter_str_id( outcome == "blocked" ? "t_wall" : "t_floor" ) );
            map.furn_set( tile, furn_str_id::NULL_ID() );
            map.i_clear( tile );
        }
        home_map.save();
    }
    trade_ui::trade_result_t result;
    result.traded = outcome != "cancel";
    result.items_you = { { ammo, 3 }, { wallet, 1 } };
    const auto loose = payer->pos_bub() + tripoint::south_east;
    get_map().i_clear( loose );
    std::vector<item_location> guns;
    for( int i = 0; i < 100; ++i ) {
        item &gun = get_map().add_item( loose, item( itype_id( "m240" ) ) );
        guns.emplace_back( map_cursor( loose ), &gun );
        result.items_you.emplace_back( guns.back(), 1 );
    }
    CHECK_FALSE( npc_trading::npc_can_fit_items( trader, result.items_you ) );
    if( outcome == "duplicate" ) {
        result.items_you.emplace_back( ammo, 1 );
    } else if( outcome == "quantity" ) {
        result.items_you.front().second = 6;
    } else if( outcome == "overlap" ) {
        result.items_you.emplace_back( item_location( wallet, wallet->all_items_top().front() ), 1 );
    }
    result.value_you = npc_trading::trading_price( trader, *payer, { ammo, 3 } );
    const bool paid = outcome == "paid";
    CHECK( npc_trading::complete_trade_to_stash( trader, *payer, result, home ) == paid );
    CHECK( ammo->charges == ( paid ? 2 : 5 ) );
    CHECK( decoy->charges == 7 );
    CHECK( static_cast<bool>( wallet ) == !paid );
    CHECK( get_map().i_at( loose ).size() == ( paid ? 0 : 100 ) );
    CHECK( trader.amount_of( itype_id( "m240" ) ) == 0 );
    CHECK( trader.charges_of( itype_id( "9mm" ) ) == 0 );
    CHECK( payer->pos_abs() == payer_position );
    CHECK( trader.pos_abs() == trader_position );
    CHECK( payer->cash == cash_before );
    CHECK( trader.op_of_u.owed == owed_before );
    if( paid ) {
        CHECK_FALSE( result.traded );
        CHECK_FALSE( npc_trading::complete_trade_to_stash( trader, *payer, result, home ) );
    } else if( outcome == "blocked" ) {
        tinymap unblocked;
        unblocked.load( home, false );
        for( const tripoint_bub_ms &tile : unblocked.cast_to_map()->points_on_zlevel( home.z() ) ) {
            unblocked.cast_to_map()->ter_set( tile, ter_str_id( "t_floor" ) );
        }
        unblocked.save();
        REQUIRE( npc_trading::complete_trade_to_stash( trader, *payer, result, home ) );
        CHECK_FALSE( npc_trading::complete_trade_to_stash( trader, *payer, result, home ) );
    }
    const bool eventually_paid = paid || outcome == "blocked";
    // Actual mapbuffer disk persistence, eviction and read, not a copied summary.
    get_map().save();
    MAPBUFFER.save();
    MAPBUFFER.clear_outside_reality_bubble();
    tinymap reloaded;
    reloaded.load( home, false );
    int stored_ammo = 0;
    int stored_guns = 0;
    int stored_wallets = 0;
    for( const tripoint_bub_ms &tile : reloaded.cast_to_map()->points_on_zlevel( home.z() ) ) {
        for( const item &goods : reloaded.cast_to_map()->i_at( tile ) ) {
            CHECK( goods.get_owner() == trader.get_fac_id() );
            if( goods.typeId() == itype_id( "9mm" ) ) {
                stored_ammo += goods.charges;
            } else if( goods.typeId() == itype_id( "m240" ) ) {
                ++stored_guns;
            } else if( goods.typeId() == itype_id( "wallet" ) ) {
                ++stored_wallets;
                REQUIRE( goods.all_items_top().size() == 2 );
                for( const item *coin : goods.all_items_top() ) {
                    CHECK( coin->typeId() == itype_id( "coin_gold" ) );
                    CHECK( coin->get_owner() == trader.get_fac_id() );
                }
            }
        }
    }
    CHECK( stored_ammo == ( eventually_paid ? 3 : 0 ) );
    CHECK( stored_guns == ( eventually_paid ? 100 : 0 ) );
    CHECK( stored_wallets == ( eventually_paid ? 1 : 0 ) );
}

TEST_CASE( "native shakedown trade uses home stash rather than collector capacity",
           "[semantic_surface][home_stash_ui_067]" )
{
    const std::string outcome = GENERATE( std::string( "paid" ), std::string( "cancel" ),
                                         std::string( "failure_retry" ), std::string( "failure_cancel" ) );
    const bool complete = outcome == "paid" || outcome == "failure_retry";
    const bool initially_blocked = outcome == "failure_retry" || outcome == "failure_cancel";
    const bool resident = GENERATE( false, true );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    npc &trader = spawn_npc( you.pos_bub().xy() + point::east, "test_talker" );
    clear_character( trader );
    trader.set_fac( faction_id( "your_followers" ) );
    Character *payer = &you;
    if( resident ) {
        npc &receiver = spawn_npc( you.pos_bub().xy() + point::south, "test_talker" );
        clear_character( receiver );
        receiver.set_fac( faction_id( "your_followers" ) );
        payer = &receiver;
    }
    const auto source = payer->pos_bub() + tripoint::south_east;
    get_map().i_clear( source );
    item &gun = get_map().add_item( source, item( itype_id( "m240" ) ) );
    gun.set_owner( *payer );
    const std::string uid = std::to_string( gun.uid().get_value() );
    REQUIRE_FALSE( npc_trading::npc_can_fit_items( trader, { { item_location( map_cursor( source ), &gun ), 1 } } ) );
    const tripoint_abs_omt home = you.pos_abs_omt() + tripoint( 1, 1, 0 );
    REQUIRE( get_map().inbounds( home ) );
    for( const tripoint_bub_ms &tile : get_map().points_on_zlevel( home.z() ) ) {
        if( project_to<coords::omt>( get_map().get_abs( tile ) ) == home ) {
            get_map().i_clear( tile );
            get_map().ter_set( tile, ter_str_id( initially_blocked ? "t_wall" : "t_floor" ) );
            get_map().furn_set( tile, furn_str_id::NULL_ID() );
        }
    }
    trade_imgui_context imgui;
    semantic_surface_manager manager( "stash-trade" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    int stage = 0;
    int seq = 0;
    int confirmations = 0;
    bool storage_failure = false;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        const auto submit = [&]( const std::string &action, const std::string &target = "" ) {
            REQUIRE( manager.submit_request( { "stash-trade", descriptor.surface_id, descriptor.frame_id,
                                               std::to_string( ++seq ), action,
                                               target.empty() ? std::nullopt : std::optional<std::string>( target ), {} } ) );
        };
        if( descriptor.kind == "prompt" ) {
            if( descriptor.payload.at( "text" ).find( "cannot store this payment" ) != std::string::npos ) {
                REQUIRE( initially_blocked );
                CHECK_FALSE( storage_failure );
                storage_failure = true;
                CHECK_FALSE( get_map().i_at( source ).empty() );
                CHECK( trader.amount_of( itype_id( "m240" ) ) == 0 );
                if( complete ) {
                    get_map().ter_set( get_map().get_bub( project_to<coords::ms>( home ) +
                                                        tripoint( SEEX, SEEY, 0 ) ), ter_str_id( "t_floor" ) );
                }
                const auto &ack = descriptor.valid_actions.front();
                submit( ack.id, ack.stable_id );
                return;
            }
            REQUIRE( ( complete || ( initially_blocked && !storage_failure ) ) );
            CHECK( descriptor.payload.at( "text" ).find( "Accept this trade" ) != std::string::npos );
            const auto yes = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            []( const semantic_action_descriptor &action ) { return action.label == "YES"; } );
            REQUIRE( yes != descriptor.valid_actions.end() );
            ++confirmations;
            submit( yes->id, yes->stable_id );
        } else if( descriptor.kind == "inventory" ) {
            CHECK( descriptor.payload.at( "payment_destination" ) == "bandit_home_stash" );
            if( stage++ == 0 ) {
                submit( "trade.switch_pane" );
            } else if( stage == 2 ) {
                submit( "inventory.toggle", uid );
            } else {
                submit( complete || ( initially_blocked && !storage_failure ) ? "inventory.commit" : "inventory.cancel" );
            }
        }
    } );
    CHECK( npc_trading::trade_to_stash( trader, *payer, home, 0, "Pay:", 2, 0, nullptr, true ) == complete );
    CHECK( confirmations == ( initially_blocked ? ( complete ? 2 : 1 ) : ( complete ? 1 : 0 ) ) );
    CHECK( storage_failure == initially_blocked );
    CHECK( get_map().i_at( source ).empty() == complete );
    CHECK( trader.amount_of( itype_id( "m240" ) ) == 0 );
    int deposited = 0;
    for( const tripoint_bub_ms &tile : get_map().points_on_zlevel( home.z() ) ) {
        if( project_to<coords::omt>( get_map().get_abs( tile ) ) == home ) {
            for( const item &goods : get_map().i_at( tile ) ) {
                deposited += goods.typeId() == itype_id( "m240" ) ? 1 : 0;
            }
        }
    }
    CHECK( deposited == ( complete ? 1 : 0 ) );
    CHECK( manager.top()->surface_id == world.surface_id() );
}

TEST_CASE( "native grouped stash payment expands every discrete item before persisted transfer",
           "[semantic_surface][home_stash_group_067]" )
{
    const bool charged = GENERATE( false, true );
    const bool paid = GENERATE( false, true );
    INFO( "charged=" << charged << " paid=" << paid );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    npc &payer = spawn_npc( you.pos_bub().xy() + point::south, "test_talker" );
    npc &trader = spawn_npc( you.pos_bub().xy() + point::east, "test_talker" );
    clear_character( payer );
    clear_character( trader );
    payer.set_fac( faction_id( "your_followers" ) );
    trader.set_fac( faction_id( "your_followers" ) );
    const itype_id type( charged ? "9mm" : "coin_gold" );
    const auto source = payer.pos_bub() + tripoint::south_east;
    get_map().i_clear( source );
    item goods( type, calendar::turn_zero, charged ? 1000 : -1 );
    goods.set_owner( payer );
    REQUIRE( goods.count_by_charges() == charged );
    item &first = get_map().add_item( source, goods, charged ? 1 : 1000 );
    REQUIRE_FALSE( first.is_null() );
    const std::string uid = std::to_string( first.uid().get_value() );
    REQUIRE( get_map().i_at( source ).size() == ( charged ? 1 : 1000 ) );
    const tripoint_abs_omt home = you.pos_abs_omt() + tripoint( 20, 20, 0 );
    {
        tinymap remote;
        remote.load( home, false );
        for( const tripoint_bub_ms &tile : remote.cast_to_map()->points_on_zlevel( home.z() ) ) {
            remote.cast_to_map()->ter_set( tile, ter_str_id( "t_floor" ) );
            remote.cast_to_map()->furn_set( tile, furn_str_id::NULL_ID() );
            remote.cast_to_map()->i_clear( tile );
        }
        remote.save();
    }
    trade_imgui_context imgui;
    semantic_surface_manager manager( "stash-native-group" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    int stage = 0;
    int sequence = 0;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        const auto submit = [&]( const std::string &action, const std::string &target = "" ) {
            REQUIRE( manager.submit_request( { "stash-native-group", descriptor.surface_id,
                                               descriptor.frame_id, std::to_string( ++sequence ), action,
                                               target.empty() ? std::nullopt : std::optional<std::string>( target ), {} } ) );
        };
        if( descriptor.kind == "prompt" ) {
            REQUIRE( paid );
            REQUIRE( descriptor.payload.at( "text" ).find( "Accept this trade" ) != std::string::npos );
            const auto yes = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            []( const semantic_action_descriptor &action ) { return action.label == "YES"; } );
            REQUIRE( yes != descriptor.valid_actions.end() );
            submit( yes->id, yes->stable_id );
        } else if( descriptor.kind == "inventory" ) {
            if( stage++ == 0 ) {
                submit( "trade.switch_pane" );
            } else if( stage == 2 ) {
                submit( "inventory.toggle", uid );
            } else {
                submit( paid ? "inventory.commit" : "inventory.cancel" );
            }
        }
    } );
    trade_ui ui( payer, trader, 0, "Pay:", 2, 0, nullptr, true, true );
    auto result = ui.perform_trade();
    REQUIRE( result.traded == paid );
    if( paid ) {
        // This is the real selector result, not a hand-built location vector.
        REQUIRE( result.items_you.size() == ( charged ? 1 : 1000 ) );
        int selected = 0;
        for( const auto &entry : result.items_you ) {
            REQUIRE( entry.first );
            CHECK( entry.first->typeId() == type );
            CHECK( entry.second == ( charged ? 1000 : 1 ) );
            selected += entry.second;
        }
        CHECK( selected == 1000 );
    } else {
        CHECK( result.items_you.empty() );
    }
    CHECK( npc_trading::complete_trade_to_stash( trader, payer, result, home ) == paid );
    CHECK( get_map().i_at( source ).size() == ( paid ? 0 : ( charged ? 1 : 1000 ) ) );
    CHECK( trader.amount_of( type ) == 0 );
    CHECK_FALSE( npc_trading::complete_trade_to_stash( trader, payer, result, home ) );
    get_map().save();
    MAPBUFFER.save();
    MAPBUFFER.clear_outside_reality_bubble();
    tinymap reloaded;
    reloaded.load( home, false );
    int stored = 0;
    int objects = 0;
    for( const tripoint_bub_ms &tile : reloaded.cast_to_map()->points_on_zlevel( home.z() ) ) {
        for( const item &coin : reloaded.cast_to_map()->i_at( tile ) ) {
            REQUIRE( coin.typeId() == type );
            CHECK( coin.get_owner() == trader.get_fac_id() );
            stored += charged ? coin.charges : 1;
            ++objects;
        }
    }
    CHECK( stored == ( paid ? 1000 : 0 ) );
    CHECK( objects == ( paid ? ( charged ? 1 : 1000 ) : 0 ) );
}
