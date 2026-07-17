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
    site.headcount = 2;
    state.bandit_live_world.sites.push_back( site );

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

TEST_CASE( "C-AOL numeric npc mission save layout remains stable",
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

TEST_CASE( "C-AOL numeric npc attitude save layout remains stable",
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

TEST_CASE( "npc validates legacy mission fields independently",
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

TEST_CASE( "npc validates legacy attitude fields independently",
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

TEST_CASE( "npc loads established C-AOL numeric mission values",
           "[savegame][npc][regression]" )
{
    JsonValue old_save = legacy_npc_save_with( "\"mission\":8,\"previous_mission\":10" );
    npc loaded;

    loaded.deserialize( old_save.get_object() );

    CHECK( loaded.mission == NPC_MISSION_GUARD_PATROL );
    CHECK( loaded.get_previous_mission() == NPC_MISSION_TRAVELLING );
}

TEST_CASE( "overmap global save fields coexist across a round trip",
           "[savegame][overmap][regression]" )
{
    const overmap_global_state loaded = round_trip_global_state( populated_global_state() );

    REQUIRE( loaded.bandit_live_world.sites.size() == 1 );
    CHECK( loaded.bandit_live_world.sites.front().site_id == "save_compatibility_site" );
    CHECK( loaded.bandit_live_world.sites.front().headcount == 2 );

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

TEST_CASE( "overmap global load clears newer fields when an older save omits them",
           "[savegame][overmap][regression]" )
{
    overmap_global_state loaded = populated_global_state();
    JsonValue old_save = json_loader::from_string( R"({"placed_unique_specials":[]})" );
    JsonObject old_save_object = old_save.get_object();
    old_save_object.allow_omitted_members();

    loaded.deserialize( old_save_object );

    CHECK( loaded.bandit_live_world.sites.empty() );
    CHECK( loaded.zombie_rider_light_memory.empty() );
    CHECK( loaded.zombie_rider_light_memory_last_turn == calendar::turn );
    CHECK( loaded.placed_regions.empty() );
}
