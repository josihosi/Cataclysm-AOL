#include <map>
#include <set>
#include <string>
#include <vector>

#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_catch.h"
#include "character_id.h"
#include "game.h"
#include "imgui/imgui.h"
#include "item.h"
#include "item_location.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "npc.h"
#include "npc_inspection.h"
#include "player_helpers.h"
#include "pocket_type.h"
#include "semantic_surface.h"
#include "type_id.h"

namespace
{
class inspection_imgui_context
{
    public:
        inspection_imgui_context() : owns( ImGui::GetCurrentContext() == nullptr ) {
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
        ~inspection_imgui_context() {
            if( owns ) {
                ImGui::EndFrame();
                ImGui::DestroyContext();
            }
        }
    private:
        bool owns;
};
} // namespace

TEST_CASE( "NPC inspection retains exact health orders and every stored item UID",
           "[semantic_surface][npc_inspection]" )
{
    clear_avatar();
    clear_map();
    npc &actor = spawn_npc( get_avatar().pos_bub().xy() + point::east, "test_talker" );
    clear_character( actor );
    const std::string actor_identity = npc_inspection_actor_id( actor );
    REQUIRE( actor.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    actor.name = "Same name";
    actor.assigned_camp = tripoint_abs_omt( 12, 15, 0 );
    actor.set_mission( NPC_MISSION_CAMP_RESIDENT );
    actor.rules.clear_flags();
    actor.rules.clear_overrides();
    actor.rules.set_specific_override_state( ally_rule::use_guns, true );
    const bodypart_id torso( "torso" );
    actor.set_part_hp_cur( torso, 37 );

    item gun( itype_id( "test_glock" ) );
    gun.ammo_set( itype_id( "test_9mm_ammo" ), 7 );
    REQUIRE( gun.magazine_current() != nullptr );
    const item_location first = actor.i_add( gun, false );
    REQUIRE( first );
    const std::string gun_uid = std::to_string( first->uid().get_value() );
    // Read identities after insertion: item copy construction creates new identities.
    const std::string inserted_magazine_uid =
        std::to_string( first->magazine_current()->uid().get_value() );
    const item_location second = actor.i_add( item( itype_id( "test_glock" ) ), false );
    REQUIRE( second );
    const std::string second_uid = std::to_string( second->uid().get_value() );
    REQUIRE( gun_uid != second_uid );

    item rifle( itype_id( "debug_modular_m4_carbine" ) );
    REQUIRE( rifle.put_in( item( itype_id( "shoulder_strap" ) ), pocket_type::MOD ).success() );
    REQUIRE( actor.i_add( rifle, false ) );
    const int moves = actor.get_moves();
    const time_point turn = calendar::turn;
    const auto facts = npc_inspection_payload( actor, get_avatar() );
    CHECK( facts == npc_inspection_payload( actor, get_avatar() ) );
    CHECK( actor.get_moves() == moves );
    CHECK( calendar::turn == turn );
    CHECK( facts.at( "actor_id" ) == actor_identity );
    CHECK( facts.at( "provenance" ).find( "not avatar knowledge" ) != std::string::npos );

    const JsonObject health = json_loader::from_string( facts.at( "diagnostic_health" ) );
    health.allow_omitted_members();
    const JsonObject torso_health = health.get_object( "torso" );
    torso_health.allow_omitted_members();
    CHECK( torso_health.get_int( "hp" ) == 37 );
    const JsonObject orders = json_loader::from_string( facts.at( "diagnostic_orders" ) );
    orders.allow_omitted_members();
    CHECK( orders.get_string( "mission" ) == "CAMP_RESIDENT" );
    CHECK( orders.get_array( "assigned_camp" ).get_int( 0 ) == 12 );
    const JsonObject rules = json_loader::from_string( facts.at( "diagnostic_rules" ) );
    rules.allow_omitted_members();
    const JsonArray rule_rows = json_loader::from_string( rules.get_string( "rules" ) );
    bool found_override = false;
    for( const JsonObject row : rule_rows ) {
        row.allow_omitted_members();
        if( row.get_string( "id" ) == "use_guns" ) {
            CHECK( row.get_bool( "enabled" ) );
            CHECK_FALSE( row.get_bool( "base_enabled" ) );
            CHECK( row.get_bool( "override_enabled" ) );
            found_override = true;
        }
    }
    REQUIRE( found_override );

    const JsonObject items = json_loader::from_string( facts.at( "diagnostic_items" ) );
    items.allow_omitted_members();
    CHECK( items.has_object( gun_uid ) );
    CHECK( items.has_object( second_uid ) );
    const JsonObject inserted_magazine = items.get_object( inserted_magazine_uid );
    inserted_magazine.allow_omitted_members();
    CHECK( inserted_magazine.get_string( "parent_uid" ) == gun_uid );
    std::set<std::string> slots;
    for( const JsonMember member : items ) {
        const JsonObject row = member.get_object();
        row.allow_omitted_members();
        slots.insert( row.get_string( "slot" ) );
        const auto detail = npc_inspection_item_payload( actor, row.get_string( "item_uid" ) );
        REQUIRE_FALSE( detail.empty() );
        CHECK( detail.at( "actor_id" ) == actor_identity );
        CHECK_FALSE( detail.at( "item_info_text" ).empty() );
    }
    CHECK( slots.count( "MAGAZINE_WELL" ) == 1 );
    CHECK( slots.count( "MAGAZINE" ) == 1 );
    CHECK( slots.count( "MOD" ) == 1 );

    npc &other = spawn_npc( get_avatar().pos_bub().xy() + point::south, "test_talker" );
    clear_character( other );
    REQUIRE( other.wear_item( item( itype_id( "debug_backpack" ) ), false ) );
    other.name = actor.name;
    CHECK( npc_inspection_item_payload( other, gun_uid ).empty() );
    item transferred = actor.i_rem( first.get_item() );
    const item_location received = other.i_add( transferred, false );
    REQUIRE( received );
    CHECK( npc_inspection_item_payload( actor, gun_uid ).empty() );
    CHECK( npc_inspection_item_payload( other, std::to_string( received->uid().get_value() ) ).at(
               "actor_id" ) == npc_inspection_actor_id( other ) );
    clear_npcs();
}

TEST_CASE( "NPC inspection opens actor bound item details and restores World without a turn",
           "[semantic_surface][npc_inspection]" )
{
    clear_avatar();
    clear_map();
    avatar &viewer = get_avatar();
    npc &actor = spawn_npc( viewer.pos_bub().xy() + point::east, "test_talker" );
    npc &other = spawn_npc( viewer.pos_bub().xy() + point::south, "test_talker" );
    actor.name = other.name = "Same name";
    const item_location selected = actor.i_add( item( itype_id( "rock" ) ), false );
    REQUIRE( selected );
    const std::string uid = std::to_string( selected->uid().get_value() );
    const std::string identity = npc_inspection_actor_id( actor );
    REQUIRE( resolve_npc_inspection_actor( viewer, identity ) == &actor );
    REQUIRE( resolve_npc_inspection_actor( viewer, npc_inspection_actor_id( other ) ) == &other );
    CHECK( resolve_npc_inspection_actor( viewer, actor.name ) == nullptr );
    const auto world_actions = npc_inspection_world_actions( viewer );
    REQUIRE( world_actions.size() == 2 );
    CHECK( world_actions[0].stable_id != world_actions[1].stable_id );
    const int moves = actor.get_moves();
    const int viewer_moves = viewer.get_moves();
    const time_point turn = calendar::turn;
    inspection_imgui_context imgui;
    semantic_surface_manager manager( "inspection-run" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World", {}, world_actions );
    const std::string world_frame = manager.top()->frame_id;
    int inspector_publications = 0;
    std::string inspection_frame;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind == "npc_inspection" ) {
            CHECK( descriptor.payload.at( "actor_id" ) == identity );
            CHECK( descriptor.valid_actions.size() == 2 );
            ++inspector_publications;
            if( inspector_publications == 1 ) {
                inspection_frame = descriptor.frame_id;
                REQUIRE( manager.submit_request( { "inspection-run", descriptor.surface_id,
                                                   descriptor.frame_id, "invalid-item", "npc_inspection.item_details",
                                                   std::nullopt, { { "item_uid", "missing-uid" } } } ) );
                CHECK_FALSE( manager.consume_top_request() );
                REQUIRE_FALSE( receipts.empty() );
                CHECK( receipts.back().rejection_reason == "stale_actor_item_uid" );
                REQUIRE( manager.submit_request( { "inspection-run", descriptor.surface_id,
                                                   descriptor.frame_id, "details", "npc_inspection.item_details",
                                                   std::nullopt, { { "item_uid", uid } } } ) );
            } else {
                REQUIRE( inspector_publications == 2 );
                CHECK( descriptor.frame_id != inspection_frame );
                REQUIRE( manager.submit_request( { "inspection-run", descriptor.surface_id,
                                                   descriptor.frame_id, "close-inspector", "npc_inspection.close",
                                                   std::nullopt, {} } ) );
            }
        } else if( descriptor.kind == "npc_item_info" ) {
            CHECK( descriptor.payload.at( "actor_id" ) == identity );
            CHECK( descriptor.payload.at( "item_uid" ) == uid );
            CHECK_FALSE( descriptor.payload.at( "item_info_text" ).empty() );
            REQUIRE( descriptor.valid_actions.size() == 1 );
            REQUIRE( manager.submit_request( { "inspection-run", descriptor.surface_id,
                                               descriptor.frame_id, "close-item", "npc_item_info.close",
                                               std::nullopt, {} } ) );
        }
    } );
    show_npc_inspection( actor.getID() );
    REQUIRE( inspector_publications == 2 );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == world.surface_id() );
    CHECK( manager.top()->frame_id != world_frame );
    CHECK( receipts.size() == 4 );
    CHECK( actor.get_moves() == moves );
    CHECK( viewer.get_moves() == viewer_moves );
    CHECK( calendar::turn == turn );
    clear_npcs();
}
