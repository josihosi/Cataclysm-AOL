#include <array>
#include <sstream>
#include <string>

#include "calendar.h"
#include "cata_catch.h"
#include "json.h"
#include "json_loader.h"
#include "npc.h"
#include "overmapbuffer.h"
#include "regional_settings.h"

namespace
{
overmap_global_state populated_global_state()
{
    overmap_global_state state;

    bandit_live_world::site_record site;
    site.site_id = "save_compatibility_site";
    site.living_total = 2;
    site.supply_units = 13;
    site.supply_last_update_minutes = 100;
    site.supply_accounted_living_total = 2;
    site.supply_member_minute_remainder = 720;
    state.bandit_live_world.sites.push_back( site );
    bandit_live_world::site_record &stored_site = state.bandit_live_world.sites.back();
    const tripoint_abs_omt resource_omt( 8, 9, 0 );
    const bandit_live_world::finite_resource_record resource_snapshot =
        bandit_live_world::finite_resource_snapshot( state.bandit_live_world, resource_omt, 3 );
    const std::string resource_application_key =
        bandit_live_world::finite_resource_claim_application_key(
            stored_site.site_id + "#resource", 1, resource_omt );
    stored_site.active_outing.clear();
    stored_site.active_outing.kind = bandit_live_world::outing_kind::structural_sortie;
    stored_site.active_outing.activity_id = stored_site.site_id + "#resource";
    stored_site.active_outing.camp_id = stored_site.site_id;
    stored_site.active_outing.generation = 1;
    stored_site.active_outing.target_omt = resource_omt;
    stored_site.active_outing.job_type = "scavenge";
    stored_site.active_outing.owner = bandit_live_world::simulation_owner::abstract;
    stored_site.active_outing.return_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            stored_site.active_outing.activity_id, 1, "return" );
    stored_site.active_outing.report_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            stored_site.active_outing.activity_id, 1, "report" );
    stored_site.active_outing.cargo_application_key =
        bandit_pursuit_handoff::make_operation_component_key(
            stored_site.active_outing.activity_id, 1, "cargo" );
    stored_site.next_outing_generation = 2;
    bandit_live_world::claim_finite_resource_units( state.bandit_live_world, stored_site.site_id,
            resource_omt, resource_snapshot, 1, stored_site.site_id + "#resource", 1,
            resource_application_key );
    stored_site.active_outing.clear();

    zombie_rider_overmap_ai::rider_light_memory rider_memory;
    rider_memory.interest_score = 8;
    rider_memory.turns_remaining = 40;
    rider_memory.max_riders_drawn = 2;
    rider_memory.decay_turn_remainder = 17;
    rider_memory.reason = "save compatibility test";
    state.zombie_rider_light_memory.emplace( tripoint_abs_omt( 4, 5, 0 ), rider_memory );
    state.zombie_rider_light_memory_last_turn = calendar::turn_zero + 123_turns;

    state.placed_regions.emplace( tripoint_abs_om( 6, 7, 0 ), region_settings_id( "default" ) );
    return state;
}

overmap_global_state round_trip_global_state( const overmap_global_state &state )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    state.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    overmap_global_state loaded;
    loaded.deserialize( jsin.get_object() );
    return loaded;
}

JsonValue legacy_npc_save_with( const std::string &fields )
{
    return json_loader::from_string(
               "{\"posx\":0,\"posy\":0,\"posz\":0," + fields + "}" );
}
} // namespace

TEST_CASE( "C-AOL_numeric_npc_mission_save_layout_remains_stable",
           "[savegame][npc][regression]" )
{
    STATIC_REQUIRE( static_cast<int>( NPC_MISSION_NULL ) == 0 );
    STATIC_REQUIRE( static_cast<int>( NPC_MISSION_SHELTER ) == 2 );
    STATIC_REQUIRE( static_cast<int>( NPC_MISSION_SHOPKEEP ) == 3 );
    STATIC_REQUIRE( static_cast<int>( NPC_MISSION_GUARD_ALLY ) == 6 );
    STATIC_REQUIRE( static_cast<int>( NPC_MISSION_GUARD ) == 7 );
    STATIC_REQUIRE( static_cast<int>( NPC_MISSION_GUARD_PATROL ) == 8 );
    STATIC_REQUIRE( static_cast<int>( NPC_MISSION_ACTIVITY ) == 9 );
    STATIC_REQUIRE( static_cast<int>( NPC_MISSION_TRAVELLING ) == 10 );
    STATIC_REQUIRE( static_cast<int>( NPC_MISSION_CAMP_RESIDENT ) == 11 );
}

TEST_CASE( "C-AOL_numeric_npc_attitude_save_layout_remains_stable",
           "[savegame][npc][regression]" )
{
    STATIC_REQUIRE( static_cast<int>( NPCATT_NULL ) == 0 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_TALK ) == 1 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_LEGACY_1 ) == 2 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_FOLLOW ) == 3 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_LEGACY_2 ) == 4 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_LEAD ) == 5 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_WAIT ) == 6 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_LEGACY_6 ) == 7 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_MUG ) == 8 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_WAIT_FOR_LEAVE ) == 9 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_KILL ) == 10 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_FLEE ) == 11 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_LEGACY_3 ) == 12 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_HEAL ) == 13 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_LEGACY_4 ) == 14 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_LEGACY_5 ) == 15 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_ACTIVITY ) == 16 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_FLEE_TEMP ) == 17 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_RECOVER_GOODS ) == 18 );
    STATIC_REQUIRE( static_cast<int>( NPCATT_END ) == 19 );
}

TEST_CASE( "npc_validates_legacy_mission_fields_independently",
           "[savegame][npc][regression]" )
{
    for( const int legacy_mission : std::array<int, 3> { 1, 4, 5 } ) {
        CAPTURE( legacy_mission );
        JsonValue legacy_current_save = legacy_npc_save_with(
                                            "\"mission\":" + std::to_string( legacy_mission ) +
                                            ",\"previous_mission\":10" );
        npc legacy_current;
        legacy_current.deserialize( legacy_current_save.get_object() );

        CHECK( legacy_current.mission == NPC_MISSION_NULL );
        CHECK( legacy_current.get_previous_mission() == NPC_MISSION_TRAVELLING );

        JsonValue legacy_previous_save = legacy_npc_save_with(
                                             "\"mission\":8,\"previous_mission\":" +
                                             std::to_string( legacy_mission ) );
        npc legacy_previous;
        legacy_previous.deserialize( legacy_previous_save.get_object() );

        CHECK( legacy_previous.mission == NPC_MISSION_GUARD_PATROL );
        CHECK( legacy_previous.get_previous_mission() == NPC_MISSION_NULL );
    }
}

TEST_CASE( "npc_validates_legacy_attitude_fields_independently",
           "[savegame][npc][regression]" )
{
    for( const int legacy_attitude : std::array<int, 6> { 2, 4, 7, 12, 14, 15 } ) {
        CAPTURE( legacy_attitude );
        JsonValue legacy_current_save = legacy_npc_save_with(
                                            "\"attitude\":" +
                                            std::to_string( legacy_attitude ) +
                                            ",\"previous_attitude\":3" );
        npc legacy_current;
        legacy_current.deserialize( legacy_current_save.get_object() );

        CHECK( legacy_current.get_attitude() == NPCATT_NULL );
        CHECK( legacy_current.get_previous_attitude() == NPCATT_FOLLOW );

        JsonValue legacy_previous_save = legacy_npc_save_with(
                                             "\"attitude\":1,\"previous_attitude\":" +
                                             std::to_string( legacy_attitude ) );
        npc legacy_previous;
        legacy_previous.deserialize( legacy_previous_save.get_object() );

        CHECK( legacy_previous.get_attitude() == NPCATT_TALK );
        CHECK( legacy_previous.get_previous_attitude() == NPCATT_NULL );
    }
}

TEST_CASE( "npc_loads_established_C-AOL_numeric_mission_values",
           "[savegame][npc][regression]" )
{
    JsonValue old_save = legacy_npc_save_with( "\"mission\":8,\"previous_mission\":10" );
    npc loaded;

    loaded.deserialize( old_save.get_object() );

    CHECK( loaded.mission == NPC_MISSION_GUARD_PATROL );
    CHECK( loaded.get_previous_mission() == NPC_MISSION_TRAVELLING );
}

TEST_CASE( "overmap_global_save_fields_coexist_across_a_round_trip",
           "[savegame][overmap][regression]" )
{
    const overmap_global_state loaded = round_trip_global_state( populated_global_state() );

    REQUIRE( loaded.bandit_live_world.sites.size() == 1 );
    CHECK( loaded.bandit_live_world.sites.front().site_id == "save_compatibility_site" );
    CHECK( loaded.bandit_live_world.sites.front().living_total == 2 );
    CHECK( loaded.bandit_live_world.sites.front().supply_units == 13 );
    CHECK( loaded.bandit_live_world.sites.front().supply_last_update_minutes == 100 );
    CHECK( loaded.bandit_live_world.sites.front().supply_accounted_living_total == 2 );
    CHECK( loaded.bandit_live_world.sites.front().supply_member_minute_remainder == 720 );
    const bandit_live_world::finite_resource_record *resource =
        loaded.bandit_live_world.find_finite_resource( tripoint_abs_omt( 8, 9, 0 ) );
    REQUIRE( resource != nullptr );
    CHECK( resource->remaining_units == 2 );
    CHECK( resource->revision == 1 );

    const auto rider = loaded.zombie_rider_light_memory.find( tripoint_abs_omt( 4, 5, 0 ) );
    REQUIRE( rider != loaded.zombie_rider_light_memory.end() );
    CHECK( rider->second.interest_score == 8 );
    CHECK( rider->second.turns_remaining == 40 );
    CHECK( rider->second.max_riders_drawn == 2 );
    CHECK( rider->second.decay_turn_remainder == 17 );
    CHECK( rider->second.reason == "save compatibility test" );
    CHECK( loaded.zombie_rider_light_memory_last_turn == calendar::turn_zero + 123_turns );

    const auto region = loaded.placed_regions.find( tripoint_abs_om( 6, 7, 0 ) );
    REQUIRE( region != loaded.placed_regions.end() );
    CHECK( region->second == region_settings_id( "default" ) );
}

TEST_CASE( "overmap_global_load_clears_newer_fields_when_an_older_save_omits_them",
           "[savegame][overmap][regression]" )
{
    overmap_global_state loaded = populated_global_state();
    JsonValue old_save = json_loader::from_string( R"({"placed_unique_specials":[]})" );
    JsonObject old_save_object = old_save.get_object();
    old_save_object.allow_omitted_members();

    loaded.deserialize( old_save_object );

    CHECK( loaded.bandit_live_world.sites.empty() );
    CHECK( loaded.bandit_live_world.finite_resources.empty() );
    CHECK( loaded.zombie_rider_light_memory.empty() );
    CHECK( loaded.zombie_rider_light_memory_last_turn == calendar::turn );
    CHECK( loaded.placed_regions.empty() );
}
