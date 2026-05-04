#include <map>
#include <set>
#include <string>
#include <vector>

#include "cata_catch.h"
#include "coordinate_conversions.h"
#include "debug_menu.h"
#include "mongroup.h"
#include "point.h"
#include "type_id.h"

static const mongroup_id GROUP_DEBUG_MEDIUM_HORDE( "GROUP_DEBUG_MEDIUM_HORDE" );
static const mongroup_id GROUP_DEBUG_WRITHING_STALKER( "GROUP_DEBUG_WRITHING_STALKER" );
static const mongroup_id GROUP_DEBUG_ZOMBIE_RIDER( "GROUP_DEBUG_ZOMBIE_RIDER" );

static const mtype_id mon_writhing_stalker( "mon_writhing_stalker" );
static const mtype_id mon_zombie( "mon_zombie" );
static const mtype_id mon_zombie_rider( "mon_zombie_rider" );

static std::map<mtype_id, int> spawn_counts_for_option(
    const debug_menu::overmap_spawn_option &option )
{
    std::map<mtype_id, int> counts;
    int remaining_population = option.population;
    while( remaining_population > 0 ) {
        const int previous_population = remaining_population;
        bool found = false;
        std::vector<MonsterGroupResult> results = MonsterGroupManager::GetResultFromGroup(
                    option.group, &remaining_population, &found );
        REQUIRE( found );
        REQUIRE_FALSE( results.empty() );
        REQUIRE( remaining_population < previous_population );
        for( const MonsterGroupResult &result : results ) {
            counts[result.id] += result.pack_size;
        }
    }
    return counts;
}

TEST_CASE( "debug_overmap_spawn_options_cover_requested_distances_and_threats",
           "[debug_menu][mongroup]" )
{
    const std::vector<debug_menu::overmap_spawn_option> options = debug_menu::overmap_spawn_options();
    REQUIRE( options.size() == 6 );

    std::set<std::string> ids;
    std::map<std::string, debug_menu::overmap_spawn_option> by_id;
    for( const debug_menu::overmap_spawn_option &option : options ) {
        CHECK( ids.insert( option.id ).second );
        CHECK( MonsterGroupManager::isValidMonsterGroup( option.group ) );
        by_id.emplace( option.id, option );
    }

    REQUIRE( by_id.count( "medium_horde_5_omt" ) == 1 );
    CHECK( by_id.at( "medium_horde_5_omt" ).group == GROUP_DEBUG_MEDIUM_HORDE );
    CHECK( by_id.at( "medium_horde_5_omt" ).population == 30 );
    CHECK( by_id.at( "medium_horde_5_omt" ).distance_omt == 5 );

    REQUIRE( by_id.count( "medium_horde_10_omt" ) == 1 );
    CHECK( by_id.at( "medium_horde_10_omt" ).group == GROUP_DEBUG_MEDIUM_HORDE );
    CHECK( by_id.at( "medium_horde_10_omt" ).population == 30 );
    CHECK( by_id.at( "medium_horde_10_omt" ).distance_omt == 10 );

    REQUIRE( by_id.count( "writhing_stalker_5_omt" ) == 1 );
    CHECK( by_id.at( "writhing_stalker_5_omt" ).group == GROUP_DEBUG_WRITHING_STALKER );
    CHECK( by_id.at( "writhing_stalker_5_omt" ).population == 1 );
    CHECK( by_id.at( "writhing_stalker_5_omt" ).distance_omt == 5 );

    REQUIRE( by_id.count( "writhing_stalker_10_omt" ) == 1 );
    CHECK( by_id.at( "writhing_stalker_10_omt" ).group == GROUP_DEBUG_WRITHING_STALKER );
    CHECK( by_id.at( "writhing_stalker_10_omt" ).population == 1 );
    CHECK( by_id.at( "writhing_stalker_10_omt" ).distance_omt == 10 );

    REQUIRE( by_id.count( "zombie_rider_5_omt" ) == 1 );
    CHECK( by_id.at( "zombie_rider_5_omt" ).group == GROUP_DEBUG_ZOMBIE_RIDER );
    CHECK( by_id.at( "zombie_rider_5_omt" ).population == 1 );
    CHECK( by_id.at( "zombie_rider_5_omt" ).distance_omt == 5 );

    REQUIRE( by_id.count( "zombie_rider_10_omt" ) == 1 );
    CHECK( by_id.at( "zombie_rider_10_omt" ).group == GROUP_DEBUG_ZOMBIE_RIDER );
    CHECK( by_id.at( "zombie_rider_10_omt" ).population == 1 );
    CHECK( by_id.at( "zombie_rider_10_omt" ).distance_omt == 10 );
}

TEST_CASE( "debug_overmap_spawn_destination_uses_omt_distance_north", "[debug_menu][coordinates]" )
{
    const tripoint_abs_ms player_abs_ms( 1234, 5678, 0 );
    const int omt_ms = coords::map_squares_per( coords::scale::overmap_terrain );

    CHECK( debug_menu::overmap_spawn_destination( player_abs_ms, 5 ) ==
           project_to<coords::sm>( player_abs_ms + point{ 0, -5 * omt_ms } ) );
    CHECK( debug_menu::overmap_spawn_destination( player_abs_ms, 10 ) ==
           project_to<coords::sm>( player_abs_ms + point{ 0, -10 * omt_ms } ) );
}

TEST_CASE( "debug_overmap_spawn_groups_resolve_requested_monsters", "[debug_menu][mongroup]" )
{
    const std::vector<debug_menu::overmap_spawn_option> options = debug_menu::overmap_spawn_options();
    std::map<std::string, std::map<mtype_id, int>> counts_by_option;
    for( const debug_menu::overmap_spawn_option &option : options ) {
        counts_by_option.emplace( option.id, spawn_counts_for_option( option ) );
    }

    CHECK( counts_by_option.at( "medium_horde_5_omt" ) == std::map<mtype_id, int>{ { mon_zombie, 30 } } );
    CHECK( counts_by_option.at( "medium_horde_10_omt" ) == std::map<mtype_id, int>{ { mon_zombie, 30 } } );
    CHECK( counts_by_option.at( "writhing_stalker_5_omt" ) == std::map<mtype_id, int>{ { mon_writhing_stalker, 1 } } );
    CHECK( counts_by_option.at( "writhing_stalker_10_omt" ) == std::map<mtype_id, int>{ { mon_writhing_stalker, 1 } } );
    CHECK( counts_by_option.at( "zombie_rider_5_omt" ) == std::map<mtype_id, int>{ { mon_zombie_rider, 1 } } );
    CHECK( counts_by_option.at( "zombie_rider_10_omt" ) == std::map<mtype_id, int>{ { mon_zombie_rider, 1 } } );
}
