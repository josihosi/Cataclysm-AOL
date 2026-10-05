#include <fstream>
#include <iterator>
#include <sstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "avatar.h"
#include "bodypart.h"
#include "basecamp.h"
#include "bandit_live_world.h"
#include "bandit_live_world_probe.h"
#include "do_turn.h"
#include "rng.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "character_id.h"
#include "clzones.h"
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
#include "overmapbuffer.h"
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

TEST_CASE( "NPC inspection reads patrol cache without initializing or refreshing it",
           "[semantic_surface][npc_inspection][camp][patrol]" )
{
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    clear_avatar();
    clear_map();
    clear_creatures();
    zone_manager::get_manager().clear();
    on_out_of_scope clear_zones( []() { zone_manager::get_manager().clear(); } );

    map &here = get_map();
    const tripoint_abs_ms patrol_abs = here.get_abs( tripoint_bub_ms{ 10, 10, 0 } );
    static const zone_type_id zone_type_CAMP_PATROL( "CAMP_PATROL" );
    zone_manager::get_manager().add( "Patrol Post", zone_type_CAMP_PATROL, your_fac, false,
                                     true, patrol_abs, patrol_abs, nullptr, false );
    const tripoint_abs_omt camp_omt = project_to<coords::omt>( patrol_abs );
    here.add_camp( camp_omt, "faction_camp" );
    const std::optional<basecamp *> found_camp = overmap_buffer.find_camp( camp_omt.xy() );
    REQUIRE( found_camp );
    basecamp &camp = **found_camp;
    camp.set_owner( your_fac );
    camp.set_bb_pos( patrol_abs );

    npc &actor = spawn_npc( tripoint_bub_ms{ 6, 5, 0 }.xy(), "thug" );
    actor.set_mission( NPC_MISSION_CAMP_RESIDENT );
    actor.assigned_camp = camp_omt;
    static const activity_id ACT_CAMP_PATROL( "ACT_CAMP_PATROL" );
    REQUIRE( actor.job.set_task_priority( ACT_CAMP_PATROL, 9 ) );
    camp.add_assignee( actor.getID() );
    calendar::turn = sunrise( calendar::turn_zero ) + 2_hours;

    const npc_mission mission_before = actor.mission;
    const auto guard_post_before = actor.get_guard_post();
    const auto move_target_before = actor.goto_to_this_pos;
    const time_point unavailable_observation_turn = calendar::turn;
    CHECK( camp.get_cached_current_patrol_shift_plan().freshness ==
           camp_patrol_cache_freshness::unavailable );
    const auto unavailable_facts = npc_inspection_payload( actor, get_avatar() );
    const JsonObject unavailable =
        json_loader::from_string( unavailable_facts.at( "diagnostic_camp_patrol" ) );
    unavailable.allow_omitted_members();
    CHECK( unavailable.get_string( "shift_cache_state" ) == "unavailable" );
    CHECK_FALSE( unavailable.get_bool( "shift_cache_available" ) );
    CHECK( camp.get_cached_current_patrol_shift_plan().freshness ==
           camp_patrol_cache_freshness::unavailable );
    CHECK( calendar::turn == unavailable_observation_turn );
    CHECK( actor.mission == mission_before );
    CHECK( actor.get_guard_post() == guard_post_before );
    CHECK( actor.goto_to_this_pos == move_target_before );
    CHECK_FALSE( actor.has_camp_patrol_order() );

    const camp_patrol_shift_plan *const refreshed = camp.get_current_patrol_shift_plan();
    REQUIRE( refreshed != nullptr );
    const std::vector<character_id> cached_roster = refreshed->roster;
    calendar::turn = sunset( calendar::turn_zero ) + 2_hours;
    const time_point stale_observation_turn = calendar::turn;
    const auto stale_facts = npc_inspection_payload( actor, get_avatar() );
    const JsonObject stale = json_loader::from_string( stale_facts.at( "diagnostic_camp_patrol" ) );
    stale.allow_omitted_members();
    CHECK( stale.get_string( "shift_cache_state" ) == "stale" );
    CHECK( stale.get_string( "runtime_state" ) == "stale" );
    const camp_patrol_shift_cache_view stale_view = camp.get_cached_current_patrol_shift_plan();
    REQUIRE( stale_view.plan != nullptr );
    CHECK( stale_view.freshness == camp_patrol_cache_freshness::stale );
    CHECK( stale_view.plan->roster == cached_roster );
    CHECK( calendar::turn == stale_observation_turn );
    CHECK( actor.mission == mission_before );
    CHECK( actor.get_guard_post() == guard_post_before );
    CHECK( actor.goto_to_this_pos == move_target_before );
    CHECK_FALSE( actor.has_camp_patrol_order() );
}

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
    CHECK( resolve_npc_inspection_actor( viewer, "character:99999999" ) == nullptr );
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

namespace
{
std::string r067_diagnostic_actor_bytes( const npc &actor )
{
    std::ostringstream bytes;
    JsonOut out( bytes );
    actor.serialize( out );
    return bytes.str();
}
}

TEST_CASE( "outing object diagnostic distinguishes current tracker and persistent copies without mutation",
           "[npc_inspection][scout_authority_067]" )
{
    clear_avatar();
    clear_map();
    clear_creatures();
    on_out_of_scope cleanup( []() { clear_creatures(); } );
    restore_on_out_of_scope restore_turn( calendar::turn );
    npc &actor = spawn_npc( tripoint_bub_ms{ 6, 5, 0 }.xy(), "thug" );
    const character_id id = actor.getID();
    auto persistent = overmap_buffer.find_npc( id );
    REQUIRE( persistent );
    REQUIRE( persistent.get() == &actor );
    REQUIRE( actor.is_active() );
    SECTION( "same pointer is explicit and not independent copy proof" ) {
        auto row = json_loader::from_string( npc_outing_member_diagnostic( id ) ).get_object();
        row.allow_omitted_members();
        CHECK( row.get_string( "selected_object_source" ) == "persistent_overmap_in_memory" );
        CHECK( row.get_string( "game_lookup_source" ) == "game_find_npc_aliases_overmap_buffer" );
        CHECK_FALSE( row.has_member( "hp" ) ); // Preserve the legacy projection shape.
        auto copy = row.get_array( "tracker_objects" ).next_object();
        copy.allow_omitted_members();
        CHECK( copy.get_bool( "same_object" ) );
        CHECK( copy.get_bool( "lease_matches_persistent" ) );
    }
    SECTION( "pointer distinct position body clock and lease disagreement are exposed" ) {
        REQUIRE( overmap_buffer.remove_npc( id ) == persistent );
        auto stored = make_shared_fast<npc>();
        stored->deserialize( json_loader::from_string( r067_diagnostic_actor_bytes( actor ) ).get_object() );
        overmap_buffer.insert_npc( stored );
        stored->spawn_at_precise( actor.pos_abs() + point( 24, 0 ) );
        calendar::turn += 1_minutes;
        stored->on_load( &get_map() );
        auto lease = stored->get_bandit_live_world_projection_lease();
        lease.present = true;
        lease.site_id = "controlled_same_actor_other_lease";
        lease.generation = 2;
        stored->set_bandit_live_world_projection_lease( lease );
        const auto row = json_loader::from_string( npc_outing_member_diagnostic( id ) ).get_object();
        row.allow_omitted_members();
        auto copy = row.get_array( "tracker_objects" ).next_object();
        copy.allow_omitted_members();
        CHECK_FALSE( copy.get_bool( "same_object" ) );
        CHECK_FALSE( copy.get_bool( "position_matches_persistent" ) );
        CHECK_FALSE( copy.get_bool( "update_clock_matches_persistent" ) );
        CHECK_FALSE( copy.get_bool( "lease_matches_persistent" ) );
        CHECK( g->find_npc( id ) == stored.get() );
        const auto stored_before = r067_diagnostic_actor_bytes( *stored );
        const auto actor_before = r067_diagnostic_actor_bytes( actor );
        const auto rng_before = rng_get_engine();
        const auto turn_before = calendar::turn;
        npc_outing_member_diagnostic( id );
        CHECK( stored_before == r067_diagnostic_actor_bytes( *stored ) );
        CHECK( actor_before == r067_diagnostic_actor_bytes( actor ) );
        CHECK( rng_before == rng_get_engine() );
        CHECK( turn_before == calendar::turn );
    }
    SECTION( "neither object available has no invented position or agreement" ) {
        auto row = json_loader::from_string( npc_outing_member_diagnostic( character_id( -999 ) ) ).get_object();
        row.allow_omitted_members();
        CHECK_FALSE( row.get_bool( "persistent_object_available" ) );
        CHECK_FALSE( row.get_bool( "tracker_objects_available" ) );
        CHECK_FALSE( row.has_member( "absolute_ms" ) );
        CHECK_FALSE( row.has_member( "body_update_turn" ) );
        CHECK_FALSE( row.has_member( "hp" ) );
        CHECK_FALSE( row.has_member( "local_move_target" ) );
        CHECK( row.get_array( "tracker_objects" ).empty() );
    }
    SECTION( "tracker available while persistent lookup unavailable is not absence" ) {
        REQUIRE( overmap_buffer.remove_npc( id ) == persistent );
        const auto row = json_loader::from_string( npc_outing_member_diagnostic( id ) ).get_object();
        row.allow_omitted_members();
        CHECK_FALSE( row.get_bool( "found" ) );
        CHECK( row.get_bool( "tracker_objects_available" ) );
        CHECK( row.get_string( "selected_object_source" ) == "unavailable" );
        auto copy = row.get_array( "tracker_objects" ).next_object();
        copy.allow_omitted_members();
        CHECK_FALSE( copy.get_bool( "persistent_comparison_available" ) );
        CHECK_FALSE( copy.has_member( "lease_matches_persistent" ) );
    }
}

TEST_CASE( "retained R5 overmap arrival refusal is observable without fabricating copy authority",
           "[npc_inspection][scout_authority_067][retained_save]" )
{
    clear_avatar();
    clear_map();
    clear_creatures();
    const auto old_world = overmap_buffer.global_state.bandit_live_world;
    restore_on_out_of_scope restore_turn( calendar::turn );
    restore_on_out_of_scope restore_start( calendar::start_of_cataclysm );
    on_out_of_scope cleanup( [&]() {
        clear_creatures();
        for( int id : { 4, 5 } ) {
            overmap_buffer.remove_npc( character_id( id ) );
        }
        overmap_buffer.global_state.bandit_live_world = old_world;
    } );
    std::ifstream input( "tests/data/r067_scout_authority.json" );
    REQUIRE( input.good() );
    auto fixture = json_loader::from_string( std::string( std::istreambuf_iterator<char>( input ), {} ) ).get_object();
    fixture.allow_omitted_members();
    auto &world = overmap_buffer.global_state.bandit_live_world;
    world.clear();
    bandit_live_world::site_record site;
    site.deserialize( fixture.get_object( "site" ) );
    world.sites.push_back( site );
    for( JsonObject record : fixture.get_array( "actors" ) ) {
        auto actor = make_shared_fast<npc>();
        actor->deserialize( record );
        overmap_buffer.insert_npc( actor );
    }
    calendar::turn = time_point::from_turn( 5301599 );
    calendar::start_of_cataclysm = calendar::turn - 9159_minutes;
    auto &outing = world.sites.front().active_outing;
    REQUIRE( outing.phase == bandit_live_world::scout_phase::observing );
    REQUIRE( outing.owner == bandit_live_world::simulation_owner::abstract );
    REQUIRE( outing.waypoint_index == 1 );
    REQUIRE( outing.actor_route_waypoint == 1 );
    for( int id : { 4, 5 } ) {
        auto actor = overmap_buffer.find_npc( character_id( id ) );
        REQUIRE( actor );
        CHECK( actor->pos_abs_omt() == outing.selected_watch_omt );
        CHECK( actor->goal == npc::no_goal_point );
        const auto row = json_loader::from_string( npc_outing_member_diagnostic( character_id( id ) ) ).get_object();
        row.allow_omitted_members();
        CHECK( row.get_bool( "persistent_object_available" ) );
        CHECK_FALSE( row.get_bool( "tracker_objects_available" ) );
        CHECK( row.get_int( "body_update_turn" ) == 5216727 );
    }
    // Controlled positive and foreign-lease inputs identify the actual caller
    // boundary; neither variant is native arrival credit or a production repair.
    const int route_control = GENERATE( 0, 1, 2, 3, 4 );
    if( route_control > 0 ) {
        for( int id : { 4, 5 } ) {
            auto actor = overmap_buffer.find_npc( character_id( id ) );
            actor->goal = outing.selected_watch_omt;
            if( route_control == 3 ) {
                // Controlled departure square within the same logged waypoint1;
                // the native motor, not setup, must move into the watch OMT.
                actor->spawn_at_precise( project_to<coords::ms>( outing.shared_route[1] ) + point( id, 12 ) );
                actor->set_mission( NPC_MISSION_TRAVELLING );
                actor->omt_path = { outing.selected_watch_omt, outing.shared_route[1] };
            }
            if( route_control == 2 || route_control == 4 ) {
                auto lease = actor->get_bandit_live_world_projection_lease();
                lease.present = true;
                lease.generation = outing.generation + 1;
                actor->set_bandit_live_world_projection_lease( lease );
                if( route_control == 4 ) {
                    actor->set_mission( NPC_MISSION_TRAVELLING );
                }
            }
        }
    }
    const bool tracing = GENERATE( false, true );
    std::optional<bandit_live_world_probe::session> trace;
    if( tracing ) {
        trace.emplace( bandit_live_world_probe::collection_mode::transition_events );
    }
    if( route_control == 3 || route_control == 4 ) {
        process_overmap_npc_move_for_test();
    }
    if( route_control == 3 ) {
        for( int id : { 4, 5 } ) {
            auto actor = overmap_buffer.find_npc( character_id( id ) );
            CHECK( actor->pos_abs_omt() == outing.selected_watch_omt );
            CHECK( actor->goal == npc::no_goal_point );
            CHECK( actor->omt_path.empty() );
            CHECK( actor->mission == NPC_MISSION_GUARD );
        }
    }
    const auto rng_before = rng_get_engine();
    prepare_live_bandit_abstract_scout_travel_for_test();
    CHECK( rng_before == rng_get_engine() );
    CHECK( outing.waypoint_index == ( route_control == 1 || route_control == 3 ? 2 : 1 ) );
    CHECK( outing.actor_route_waypoint == ( route_control == 1 || route_control == 3 ? 2 : 1 ) );
    CHECK( outing.assessment.observation_started_minutes == ( route_control == 1 || route_control == 3 ? 9159 : -1 ) );
    CHECK( outing.observations.empty() );
    if( trace ) {
        const auto &events = trace->result().transition_events;
        int arrival_events = 0;
        int motor_events = 0;
        for( const auto &event : events ) {
            if( event.transition == "scout_motor_service" ) {
                ++motor_events;
                CHECK( event.reason.find( route_control == 3 ? "moved=1" : "moved=0" ) != std::string::npos );
                CHECK( event.outcome == ( route_control == 3 ? "travel_overmap_returned" : "identity_held" ) );
            }
            if( event.transition != "scout_watch_arrival_service" ) {
                continue;
            }
            ++arrival_events;
            CHECK( event.outcome == ( route_control == 1 || route_control == 3 ? "applied" : "rejected" ) );
            const bool confirmed_reason = event.reason.find( route_control == 0 ? "goal_matches=0 lease_present=0" :
                                      route_control == 1 || route_control == 3 ? "goal_matches=1 lease_present=0" :
                                      "goal_matches=1 lease_present=1" ) != std::string::npos ||
                   ( route_control == 4 && event.reason.find( "goal_matches=0 lease_present=1" ) != std::string::npos );
            CHECK( confirmed_reason );
            CHECK( event.reason.find( "attempted=1" ) != std::string::npos );
        }
        CHECK( arrival_events >= 2 );
        if( route_control == 3 || route_control == 4 ) {
            CHECK( motor_events == ( route_control == 3 ? 2 : 0 ) );
        }
    }
}
TEST_CASE( "actual watch motor retains only its unconsumed pair order",
           "[npc_inspection][scout_authority_067][watch_order_067]" )
{
    restore_on_out_of_scope restore_rng( rng_get_engine() );
    rng_set_engine_seed( 671 );
    clear_avatar();
    clear_map();
    clear_creatures();
    const auto old_world = overmap_buffer.global_state.bandit_live_world;
    restore_on_out_of_scope restore_turn( calendar::turn );
    restore_on_out_of_scope restore_start( calendar::start_of_cataclysm );
    on_out_of_scope cleanup( [&]() {
        clear_creatures();
        for( int id : { 4, 5 } ) {
            overmap_buffer.remove_npc( character_id( id ) );
        }
        overmap_buffer.global_state.bandit_live_world = old_world;
    } );
    std::ifstream input( "tests/data/r067_scout_authority.json" );
    REQUIRE( input.good() );
    auto fixture = json_loader::from_string( std::string( std::istreambuf_iterator<char>( input ), {} ) ).get_object();
    fixture.allow_omitted_members();
    auto &world = overmap_buffer.global_state.bandit_live_world;
    world.clear();
    bandit_live_world::site_record site;
    site.deserialize( fixture.get_object( "site" ) );
    world.sites.push_back( site );
    for( JsonObject record : fixture.get_array( "actors" ) ) {
        auto actor = make_shared_fast<npc>();
        actor->deserialize( record );
        actor->spawn_at_precise( project_to<coords::ms>( site.active_outing.shared_route[1] ) +
                                point( actor->getID().get_value(), 12 ) );
        actor->goal = site.active_outing.selected_watch_omt;
        actor->omt_path = { actor->goal };
        actor->set_mission( NPC_MISSION_TRAVELLING );
        overmap_buffer.insert_npc( actor );
    }
    calendar::turn = time_point::from_turn( 5301599 );
    calendar::start_of_cataclysm = calendar::turn - 9159_minutes;
    auto first = overmap_buffer.find_npc( character_id( 4 ) );
    auto second = overmap_buffer.find_npc( character_id( 5 ) );
    const auto watch = first->goal;
    auto site_bytes = [&]() {
        std::ostringstream out;
        JsonOut json( out );
        world.sites.front().serialize( json );
        return out.str();
    };
    SECTION( "partial completion reload then second motor commits once and returns" ) {
        first->travel_overmap( watch );
        CHECK( first->goal == watch );
        CHECK( first->is_travelling() );
        CHECK( first->omt_path.empty() );
        const auto first_guard = first->guard_pos;
        REQUIRE( live_bandit_scout_watch_order_pending( *first ) );
        const auto arrived_bytes = r067_diagnostic_actor_bytes( *first );
        for( int repeat = 0; repeat != 2; ++repeat ) {
            prepare_live_bandit_abstract_scout_travel_for_test();
            CHECK( r067_diagnostic_actor_bytes( *first ) == arrived_bytes );
            CHECK( first->guard_pos == first_guard );
            CHECK( world.sites.front().active_outing.waypoint_index == 1 );
            CHECK( world.sites.front().active_outing.assessment.observation_started_minutes == -1 );
        }
        // Real NPC/site serialization between pair arrivals preserves the live
        // order; no arrival bit, synthetic cursor or restored goal is supplied.
        const auto saved_site = site_bytes();
        const auto saved_second = r067_diagnostic_actor_bytes( *second );
        for( int id : { 4, 5 } ) {
            overmap_buffer.remove_npc( character_id( id ) );
            auto loaded = make_shared_fast<npc>();
            loaded->deserialize( json_loader::from_string( id == 4 ? arrived_bytes : saved_second ).get_object() );
            overmap_buffer.insert_npc( loaded );
        }
        world.sites.front().deserialize( json_loader::from_string( saved_site ).get_object() );
        first = overmap_buffer.find_npc( character_id( 4 ) );
        second = overmap_buffer.find_npc( character_id( 5 ) );
        REQUIRE( live_bandit_scout_watch_order_pending( *first ) );
        process_overmap_npc_move_for_test();
        REQUIRE( second->pos_abs_omt() == watch );
        prepare_live_bandit_abstract_scout_travel_for_test();
        auto &outing = world.sites.front().active_outing;
        CHECK( outing.waypoint_index == 2 );
        CHECK( outing.actor_route_waypoint == 2 );
        CHECK( outing.assessment.observation_started_minutes == 9159 );
        CHECK( first->goal == npc::no_goal_point );
        CHECK( second->goal == npc::no_goal_point );
        CHECK( first->mission == NPC_MISSION_GUARD );
        CHECK( second->mission == NPC_MISSION_GUARD );
        const auto complete = site_bytes();
        prepare_live_bandit_abstract_scout_travel_for_test();
        CHECK( site_bytes() == complete );
        world.sites.front().deserialize( json_loader::from_string( complete ).get_object() );
        prepare_live_bandit_abstract_scout_travel_for_test();
        CHECK( site_bytes() == complete );
        calendar::turn += 481_minutes;
        const auto &current = world.sites.front().active_outing;
        CHECK( bandit_live_world::advance_structural_scout_assessment(
                   world.sites.front(), current.activity_id, current.generation,
                   current.target_lead_revision, 9640 ) != bandit_live_world::scout_assessment_result::rejected );
        CHECK( world.sites.front().active_outing.phase == bandit_live_world::scout_phase::returning_report );
        prepare_live_bandit_abstract_scout_travel_for_test();
        REQUIRE( first->is_travelling() );
        REQUIRE_FALSE( first->omt_path.empty() );
        const auto before = first->pos_abs_omt();
        process_overmap_npc_move_for_test();
        CHECK( first->pos_abs_omt() != before );
        CHECK( first->getID() == character_id( 4 ) );
        CHECK( second->getID() == character_id( 5 ) );
        CHECK_FALSE( world.sites.front().current_scout_report.is_present() );
    }
    SECTION( "same-square completed motors are refused and orders are released" ) {
        // Controlled identical motor landings; both motors still traverse the
        // real OMT edge, rather than setting post-arrival positions.
        rng_set_engine_seed( 67 );
        first->travel_overmap( watch );
        rng_set_engine_seed( 67 );
        second->travel_overmap( watch );
        REQUIRE( first->pos_abs() == second->pos_abs() );
        prepare_live_bandit_abstract_scout_travel_for_test();
        CHECK( world.sites.front().active_outing.assessment.observation_started_minutes == -1 );
        CHECK( first->goal == npc::no_goal_point );
        CHECK( second->goal == npc::no_goal_point );
        const auto refused = site_bytes();
        prepare_live_bandit_abstract_scout_travel_for_test();
        CHECK( site_bytes() == refused );
    }
    SECTION( "temporary survival refusal retains order without starting assessment" ) {
        const int interruption = GENERATE( 0, 1, 2 );
        first->travel_overmap( watch );
        second->travel_overmap( watch );
        if( interruption == 0 ) {
            second->add_effect( efftype_id( "narcosis" ), 1_hours );
        } else if( interruption == 1 ) {
            second->add_effect( efftype_id( "downed" ), 1_hours );
        } else {
            second->set_attitude( NPCATT_FLEE );
        }
        prepare_live_bandit_abstract_scout_travel_for_test();
        CHECK( world.sites.front().active_outing.waypoint_index == 1 );
        CHECK( world.sites.front().active_outing.assessment.observation_started_minutes == -1 );
        CHECK( first->goal == watch );
        CHECK( second->goal == watch );
        second->remove_effect( efftype_id( "narcosis" ) );
        second->remove_effect( efftype_id( "downed" ) );
        second->set_attitude( NPCATT_NULL );
        CAPTURE( first->pos_abs().to_string(), second->pos_abs().to_string(), first->goal.to_string(), second->goal.to_string() );
        prepare_live_bandit_abstract_scout_travel_for_test();
        CHECK( world.sites.front().active_outing.assessment.observation_started_minutes == 9159 );
    }
    SECTION( "partial pair survival interruption does not move the unready member" ) {
        first->travel_overmap( watch );
        second->set_attitude( NPCATT_FLEE );
        const auto second_position = second->pos_abs();
        process_overmap_npc_move_for_test();
        CHECK( second->pos_abs() == second_position );
        CHECK( ( second->get_attitude() == NPCATT_FLEE ||
                 second->get_attitude() == NPCATT_FLEE_TEMP ) );
        CHECK( first->goal == watch );
        CHECK( world.sites.front().active_outing.assessment.observation_started_minutes == -1 );
        second->set_attitude( NPCATT_NULL );
        process_overmap_npc_move_for_test();
        CHECK( second->pos_abs_omt() == watch );
        CHECK( world.sites.front().active_outing.assessment.observation_started_minutes == 9159 );
    }
    SECTION( "release abort casualty and invalid ownership clean the pending destination" ) {
        first->travel_overmap( watch );
        REQUIRE( first->goal == watch );
        const int invalidation = GENERATE( 0, 1, 2, 3, 4, 5, 6, 7, 8 );
        CAPTURE( invalidation );
        auto &outing = world.sites.front().active_outing;
        switch( invalidation ) {
            case 0: outing.clear(); break;
            case 1: outing.phase = bandit_live_world::scout_phase::burned_withdrawal; break;
            case 2:
                second->set_part_hp_cur( bodypart_id( "torso" ), 0 );
                REQUIRE( second->is_dead() );
                break;
            case 3: {
                auto lease = first->get_bandit_live_world_projection_lease();
                lease.present = true;
                lease.site_id = "foreign";
                lease.generation = outing.generation + 1;
                first->set_bandit_live_world_projection_lease( lease );
                break;
            }
            case 4: world.sites.push_back( world.sites.front() ); break;
            case 5: outing.selected_watch_omt += point( 1, 0 ); break;
            case 6: outing.local_projection_reconciliation_rejected = true; break;
            case 7: second->goal += point( 1, 0 ); break;
            case 8: {
                auto &hostile = world.sites.front().active_hostile_operation;
                hostile.operation_kind = bandit_live_world::hostile_operation_kind::raid;
                hostile.phase = bandit_live_world::hostile_operation_phase::approaching;
                hostile.reservation = outing;
                hostile.reservation.activity_id += "#foreign";
                hostile.reservation.generation += 1;
                hostile.reservation.member_ids = { character_id( 14 ), character_id( 15 ) };
                break;
            }
        }
        CHECK_FALSE( live_bandit_scout_watch_order_pending( *first ) );
        process_overmap_npc_move_for_test();
        CHECK( first->goal != watch );
        if( invalidation == 2 ) {
            // Existing casualty service may immediately give the survivor a
            // real homeward route; cleanup must not erase that new order.
            CHECK( first->is_travelling() );
            CHECK_FALSE( first->omt_path.empty() );
            CHECK( world.sites.front().active_outing.casualty_ids.size() == 1 );
        } else {
            CHECK( first->omt_path.empty() );
            CHECK( first->mission == NPC_MISSION_GUARD );
        }
        CHECK( world.sites.front().active_outing.assessment.observation_started_minutes ==
               ( invalidation == 7 ? 9159 : -1 ) );
        // A wrong pending partner goal can be re-assigned by the real current
        // route writer, followed by actual motor completion. That is progress,
        // not acceptance of the old wrong order.
    }
    SECTION( "unaffiliated travel and empty path cannot claim pair completion" ) {
        const int invalid_completion = GENERATE( 0, 1, 2 );
        if( invalid_completion == 1 ) {
            first->omt_path.clear();
        } else if( invalid_completion == 2 ) {
            // A one-point order at the same position is not route completion.
            first->spawn_at_precise( project_to<coords::ms>( watch ) + point( 4, 12 ) );
        } else {
            world.clear();
        }
        first->travel_overmap( watch );
        CHECK( first->goal == npc::no_goal_point );
        CHECK( first->mission == NPC_MISSION_GUARD );
        CHECK( first->omt_path.empty() );
        if( invalid_completion != 0 ) {
            prepare_live_bandit_abstract_scout_travel_for_test();
            CHECK( world.sites.front().active_outing.assessment.observation_started_minutes == -1 );
        }
    }
}

TEST_CASE( "R8 saved abstract return leaves its intermediate waypoint on the next motor cadence",
           "[npc_inspection][abstract_return_8041_067]" )
{
    restore_on_out_of_scope restore_rng( rng_get_engine() );
    rng_set_engine_seed( 671 );
    clear_avatar();
    clear_map();
    clear_creatures();
    const auto old_world = overmap_buffer.global_state.bandit_live_world;
    restore_on_out_of_scope restore_turn( calendar::turn );
    restore_on_out_of_scope restore_start( calendar::start_of_cataclysm );
    std::map<tripoint_abs_omt, oter_id> old_terrain;
    on_out_of_scope cleanup( [&]() {
        clear_creatures();
        for( int id : { 4, 5 } ) {
            overmap_buffer.remove_npc( character_id( id ) );
        }
        for( const auto &entry : old_terrain ) {
            overmap_buffer.ter_set( entry.first, entry.second );
        }
        overmap_buffer.global_state.bandit_live_world = old_world;
    } );
    std::ifstream input( "tests/data/r067_return_8041.json" );
    REQUIRE( input.good() );
    auto fixture = json_loader::from_string( std::string( std::istreambuf_iterator<char>( input ), {} ) ).get_object();
    fixture.allow_omitted_members();
    auto &world = overmap_buffer.global_state.bandit_live_world;
    world.clear();
    bandit_live_world::site_record site;
    site.deserialize( fixture.get_object( "site" ) );
    world.sites.push_back( site );
    for( JsonObject tile : fixture.get_array( "terrain" ) ) {
        tile.allow_omitted_members();
        const tripoint_abs_omt point( tile.get_int( "x" ), tile.get_int( "y" ), 0 );
        old_terrain.emplace( point, overmap_buffer.ter( point ) );
        overmap_buffer.ter_set( point, oter_id( tile.get_string( "id" ) ) );
    }
    for( JsonObject record : fixture.get_array( "actors" ) ) {
        auto actor = make_shared_fast<npc>();
        actor->deserialize( record );
        overmap_buffer.insert_npc( actor );
    }
    calendar::turn = time_point::from_turn( 5234511 );
    calendar::start_of_cataclysm = time_point::from_turn( 4752000 );
    const auto first = overmap_buffer.find_npc( character_id( 4 ) );
    const auto second = overmap_buffer.find_npc( character_id( 5 ) );
    REQUIRE( first );
    REQUIRE( second );
    REQUIRE( world.sites.front().active_outing.actor_route_waypoint == 3 );
    REQUIRE( world.sites.front().active_outing.phase == bandit_live_world::scout_phase::returning_report );
    REQUIRE_FALSE( first->is_travelling() );
    REQUIRE( first->goal == tripoint_abs_omt( 129, 145, 0 ) );
    REQUIRE( first->omt_path.empty() );
    REQUIRE_FALSE( first->is_active() );
    REQUIRE_FALSE( second->is_active() );
    REQUIRE_FALSE( world.sites.front().current_scout_report.is_present() );
    const auto home = world.sites.front().anchor;
    const auto before = first->pos_abs_omt();
    calendar::turn = time_point::from_turn( 5234700 ); // next actual five-minute service
    process_overmap_npc_move_for_test();
    CHECK( first->goal == home );
    CHECK( first->is_travelling() );
    CHECK( first->pos_abs_omt() != before );
    CHECK( second->pos_abs_omt() != before );
    CHECK( world.sites.front().active_outing.actor_route_waypoint == 3 );
    CHECK( world.sites.front().active_outing.member_return_receipts.empty() );
    CHECK_FALSE( world.sites.front().current_scout_report.is_present() );
    for( int minute : { 8050, 8055, 8060 } ) {
        calendar::turn += 5_minutes;
        process_overmap_npc_move_for_test();
        CAPTURE( minute, first->pos_abs_omt(), second->pos_abs_omt() );
        CHECK( first->getID() == character_id( 4 ) );
        CHECK( second->getID() == character_id( 5 ) );
        CHECK_FALSE( first->is_dead() );
        CHECK_FALSE( second->is_dead() );
    }
    CHECK( first->pos_abs_omt() == home );
    CHECK( second->pos_abs_omt() == home );
    // Actual completed return receipts replace the temporary resume cursor.
    // The existing consumer clears that cursor when both members are terminal.
    const auto &returned = world.sites.front().active_outing;
    CHECK_FALSE( returned.local_handoff.is_abstract_resume() );
    REQUIRE( returned.member_return_receipts.size() == 2 );
    CHECK( returned.member_return_receipts[0].member_id == character_id( 4 ) );
    CHECK( returned.member_return_receipts[1].member_id == character_id( 5 ) );
    CHECK( returned.member_return_receipts[0].returned_minutes == 8060 );
    CHECK( returned.member_return_receipts[1].returned_minutes == 8060 );
    CHECK( returned.member_return_receipts[0].application_key !=
           returned.member_return_receipts[1].application_key );
    CHECK_FALSE( world.sites.front().current_scout_report.is_present() );
    calendar::turn = time_point::from_turn( 5238000 ); // ordinary hourly report service at 8100
    process_overmap_npc_move_for_test();
    const auto &report = world.sites.front().current_scout_report;
    CHECK( report.is_present() );
    CHECK( report.source_generation == 1 );
    CHECK( report.carrier_ids == std::vector<character_id>{ character_id( 4 ), character_id( 5 ) } );
    CHECK( report.delivered_minutes == 8100 );
}

TEST_CASE( "site scoped outing catalog preserves the requested retained body and handoff",
           "[npc_inspection][site_outing_067]" )
{
    clear_avatar();
    clear_map();
    clear_creatures();
    const auto old_world = overmap_buffer.global_state.bandit_live_world;
    on_out_of_scope cleanup( [&]() {
        clear_creatures();
        for( int id : { 2, 3, 4, 5, 6, 7, 8, 9 } ) {
            overmap_buffer.remove_npc( character_id( id ) );
        }
        overmap_buffer.global_state.bandit_live_world = old_world;
    } );
    std::ifstream input( "tests/data/r067_light_watch_boundary_8703.json" );
    REQUIRE( input.good() );
    auto fixture = json_loader::from_string( std::string( std::istreambuf_iterator<char>( input ), {} ) ).get_object();
    fixture.allow_omitted_members();
    auto &world = overmap_buffer.global_state.bandit_live_world;
    world.deserialize( fixture.get_object( "world" ) );
    for( JsonObject record : fixture.get_array( "actors" ) ) {
        auto actor = make_shared_fast<npc>();
        actor->deserialize( record );
        overmap_buffer.insert_npc( actor );
    }
    const auto object = []( const JsonObject &parent, const std::string &key ) {
        auto child = parent.get_object( key );
        child.allow_omitted_members();
        return child;
    };
    const std::string requested = "overmap_special:bandit_camp@129,149,0";
    REQUIRE( world.sites.front().site_id == "overmap_special:bandit_cabin@103,167,0" );
    REQUIRE( world.sites.back().site_id == requested );
    SECTION( "first cabin does not replace requested camp or unknown site" ) {
        const auto catalog = json_loader::from_string( openclaw_harness_site_outing_snapshot() ).get_object();
        catalog.allow_omitted_members();
        const auto sites = object( catalog, "by_site" );
        sites.allow_omitted_members();
        CHECK_FALSE( sites.has_member( "unknown-site" ) );
        auto old = object( object( sites, world.sites.front().site_id ), "outing" );
        old.allow_omitted_members();
        CHECK( old.get_array( "member_ids" ).next_int() == 7 );
        auto outing = object( object( sites, requested ), "outing" );
        outing.allow_omitted_members();
        CHECK( outing.get_string( "site_id" ) == requested );
        auto ids = outing.get_array( "member_ids" );
        CHECK( ids.next_int() == 4 );
        CHECK( ids.next_int() == 5 );
        auto body = outing.get_array( "assigned_members" ).next_object();
        body.allow_omitted_members();
        CHECK( body.get_string( "selected_object_source" ) == "persistent_overmap_in_memory" );
        CHECK( body.get_array( "absolute_ms" ).get_int( 0 ) == 3227 );
        CHECK( body.get_array( "absolute_ms" ).get_int( 1 ) == 3480 );
        CHECK( body.get_int( "hp" ) == overmap_buffer.find_npc( character_id( 4 ) )->get_hp() );
        auto snapshot = outing.get_object( "handoff_snapshot" );
        snapshot.allow_omitted_members();
        CHECK( snapshot.get_string( "source" ) == "durable_local_handoff_snapshot" );
        auto prior = snapshot.get_array( "members" ).next_object();
        prior.allow_omitted_members();
        CHECK( prior.get_array( "entry_position" ).get_int( 0 ) == 3192 );
        CHECK( prior.get_array( "entry_position" ).get_int( 1 ) == 3503 );
    }
    SECTION( "committed contact reuses exact member body and is read only" ) {
        auto &site = world.sites.back();
        auto &op = site.active_hostile_operation;
        op.operation_kind = bandit_live_world::hostile_operation_kind::shakedown;
        op.phase = bandit_live_world::hostile_operation_phase::committed_contact;
        op.reservation = site.active_outing;
        op.reservation.kind = bandit_live_world::outing_kind::hostile_operation;
        op.reservation.activity_id = requested + "#hostile:2";
        op.reservation.generation = 2;
        const auto actor = overmap_buffer.find_npc( character_id( 4 ) );
        REQUIRE( actor );
        actor->set_attitude( NPCATT_KILL );
        const auto actor_before = r067_diagnostic_actor_bytes( *actor );
        std::ostringstream world_before;
        JsonOut before_json( world_before );
        world.serialize( before_json );
        const auto turn_before = calendar::turn;
        const auto rng_before = rng_get_engine();
        const auto catalog = json_loader::from_string( openclaw_harness_site_outing_snapshot() ).get_object();
        catalog.allow_omitted_members();
        auto contact = object( object( object( catalog, "by_site" ), requested ), "contact" );
        contact.allow_omitted_members();
        REQUIRE( contact.get_bool( "present" ) );
        CHECK( contact.get_string( "activity_id" ) == op.reservation.activity_id );
        CHECK( contact.get_int( "generation" ) == 2 );
        auto member = contact.get_array( "members" ).next_object();
        member.allow_omitted_members();
        CHECK( member.get_int( "id" ) == 4 );
        auto body = member.get_object( "body" );
        body.allow_omitted_members();
        CHECK( body.get_int( "id" ) == 4 );
        CHECK( body.get_string( "attitude" ) == npc_attitude_name( NPCATT_KILL ) );
        CHECK( body.get_string( "selected_object_source" ) == "persistent_overmap_in_memory" );
        CHECK_FALSE( body.get_bool( "runtime_target_available" ) );
        CHECK_FALSE( body.get_bool( "llm_pending_available" ) );
        CHECK( r067_diagnostic_actor_bytes( *actor ) == actor_before );
        std::ostringstream world_after;
        JsonOut after_json( world_after );
        world.serialize( after_json );
        CHECK( world_after.str() == world_before.str() );
        CHECK( calendar::turn == turn_before );
        CHECK( rng_get_engine() == rng_before );
    }
    SECTION( "known inactive site has no substituted active operation" ) {
        world.sites.back().active_outing.clear();
        const auto catalog = json_loader::from_string( openclaw_harness_site_outing_snapshot() ).get_object();
        catalog.allow_omitted_members();
        auto site = object( object( catalog, "by_site" ), requested );
        site.allow_omitted_members();
        CHECK( site.get_string( "site_id" ) == requested );
        CHECK_FALSE( object( site, "outing" ).get_bool( "present" ) );
        CHECK_FALSE( object( site, "contact" ).get_bool( "present" ) );
    }
}
