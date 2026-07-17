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

TEST_CASE( "npc validates a legacy previous mission independently",
           "[savegame][npc][regression]" )
{
    for( const int legacy_mission : std::array<int, 3> { 1, 4, 5 } ) {
        CAPTURE( legacy_mission );
        JsonValue old_save = json_loader::from_string(
                                 "{\"mission\":0,\"previous_mission\":" +
                                 std::to_string( legacy_mission ) + "}" );
        npc loaded;

        loaded.deserialize( old_save.get_object() );

        CHECK( loaded.mission == NPC_MISSION_NULL );
        CHECK( loaded.get_previous_mission() == NPC_MISSION_NULL );
    }
}

TEST_CASE( "npc loads established C-AOL numeric mission values",
           "[savegame][npc][regression]" )
{
    JsonValue old_save = json_loader::from_string( R"({"mission":8,"previous_mission":10})" );
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
