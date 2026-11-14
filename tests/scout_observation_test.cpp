#include <algorithm>
#include <array>
#include <flatbuffers/flexbuffers.h>
#include <zstd/zstd.h>
#include <zstd/common/mem.h>
#include <zstd/common/xxhash.h>
#include <cmath>
#include <chrono>
#include <ctime>
#include <memory>
#include <set>
#include <functional>
#include "lightmap.h"
#include "level_cache.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include "cata_catch.h"
#include "cata_path.h"
#include "json.h"
#include "json_loader.h"
#include "mapbuffer.h"
#include "overmapbuffer.h"
#include "path_info.h"
#include "string_formatter.h"
#include "submap.h"
#include "scout_observation.h"
#include "avatar.h"
#include "bandit_live_world.h"
#include "monster.h"
#include "mtype.h"
#include "faction.h"
#include "field_type.h"
#include "calendar.h"
#include "cata_scope_helpers.h"
#include "effect.h"
#include "debug.h"
#include "do_turn.h"
#include "game.h"
#include "map.h"
#include "line.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "npc.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "weather.h"
#include "vehicle.h"
#include "vpart_range.h"
#include "veh_type.h"
#include "units.h"
#include "item.h"
#include "memory_fast.h"
#include "pocket_type.h"
#include "weather_type.h"
#include "worldfactory.h"
#include "zzip.h"
#include "rng.h"

std::vector<tripoint_abs_omt> live_bandit_member_route_to_for_test(
    const npc &member, const bandit_live_world::site_record &site,
    const tripoint_abs_omt &destination );

TEST_CASE( "stationary binocular optics preserves physical visibility at three OMT",
           "[binocular_056][visibility]" )
{
    map &here = get_map();
    const auto saved_map_origin = here.get_abs_sub();
    const auto saved_turn = calendar::turn;
    const auto saved_position = get_avatar().pos_abs();
    on_out_of_scope restore( [&]() {
        if( here.get_abs_sub() != saved_map_origin ) {
            here.load( saved_map_origin, false, false );
        }
        get_avatar().setpos( saved_position, false );
        set_time( saved_turn );
        CHECK( here.get_abs_sub() == saved_map_origin );
        CHECK( get_avatar().pos_abs() == saved_position );
    } );
    clear_avatar();
    clear_npcs();
    // The fixed local coordinates describe one three-OMT, target-local
    // fixture only on an OMT boundary. Derive that nearby native window and
    // clear it after loading; retain the exact incoming player/map context.
    const auto aligned_origin = project_to<coords::sm>( project_to<coords::omt>(
                                    tripoint_abs_sm( here.get_abs_sub().xy(), 0 ) ) );
    if( here.get_abs_sub() != aligned_origin ) {
        here.load( aligned_origin, false, false );
    }
    clear_map_with_vision( -1, 4, false );
    REQUIRE( here.get_abs_sub() == aligned_origin );
    set_time_to_day();
    scoped_weather_override weather( weather_type_id( "clear" ) );
    npc observer;
    observer.normalize();
    observer.set_fake( true );
    observer.recalc_sight_limits();
    observer.setpos( here, tripoint_bub_ms( 12, 66, 0 ), false );
    avatar &target = get_avatar();
    target.setpos( here, tripoint_bub_ms( 84, 66, 0 ), false );
    here.build_map_cache( 0 );
    REQUIRE( rl_dist( observer.pos_abs_omt().xy(), target.pos_abs_omt().xy() ) == 3 );
    CHECK_FALSE( observer.sees_without_clairvoyance( here, target ) );
    const auto first_read = scout_observation::read_character( observer, target );
    CAPTURE( first_read.reason, first_read.known, here.is_outside( target.pos_bub( here ) ),
             here.ambient_light_at( target.pos_bub( here ) ), observer.sight_range( 100.0f, 100.0f ),
             observer.is_blind(), observer.in_sleep_state(), target.is_invisible() );
    REQUIRE( first_read.visible );
    npc person;
    person.normalize();
    person.set_fake( true );
    person.recalc_sight_limits();
    person.setpos( here, tripoint_bub_ms( 84, 67, 0 ), false );
    CHECK( scout_observation::read_character( observer, person ).visible );

    SECTION( "existing exterior actors remain scout visible outside player vision" ) {
        // Establish the player's ordinary view before the read; do not reveal cells
        // or write seen caches to make the binocular result succeed.
        target.setpos( here, tripoint_bub_ms( 12, 12, 0 ), false );
        here.build_map_cache( 0 );
        here.update_visibility_cache( 0 );
        REQUIRE_FALSE( target.sees( here, person ) );
        const auto &cache = here.get_cache_ref( 0 );
        REQUIRE( cache.seen_cache[person.pos_bub( here ).xy()] == 0.0f );
        const auto before_seen = cache.seen_cache;
        const auto before_camera = cache.camera_cache;
        const auto before_memory_terrain = cache.map_memory_cache_ter;
        const auto before_memory_decoration = cache.map_memory_cache_dec;
        const auto player_position = target.pos_abs();
        const auto existing = scout_observation::read_character( observer, person );
        CHECK( existing.known );
        CHECK( existing.visible );
        bool unchanged_seen = true;
        bool unchanged_camera = true;
        for( size_t x = 0; x < decltype( before_seen )::size_x; ++x ) {
            unchanged_seen = unchanged_seen && cache.seen_cache[x] == before_seen[x];
            unchanged_camera = unchanged_camera && cache.camera_cache[x] == before_camera[x];
        }
        CHECK( unchanged_seen );
        CHECK( unchanged_camera );
        CHECK( cache.map_memory_cache_ter == before_memory_terrain );
        CHECK( cache.map_memory_cache_dec == before_memory_decoration );
        CHECK( target.pos_abs() == player_position );
        CHECK_FALSE( target.sees( here, person ) );
    }
    SECTION( "lookout and intervening forest are ignored but target entry stays physical" ) {
        scout_observation::site_reader reader;
        const std::vector<tripoint_abs_omt> footprint = { person.pos_abs_omt() };
        REQUIRE( reader.read_character( observer, person, footprint ).visible );
        REQUIRE( reader.read_signal( observer, observer.pos_abs(), person.pos_abs(), footprint ).visible );
        here.ter_set( observer.pos_bub( here ), ter_str_id( "t_tree" ) );
        for( int x = 24; x < 72; ++x ) {
            here.ter_set( tripoint_bub_ms( x, 66, 0 ), ter_str_id( "t_tree" ) );
            here.ter_set( tripoint_bub_ms( x, 67, 0 ), ter_str_id( "t_tree" ) );
        }
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        scout_observation::site_reader forest;
        CHECK( forest.read_character( observer, person, footprint ).visible );
        CHECK( forest.read_signal( observer, observer.pos_abs(), person.pos_abs(), footprint ).visible );
        here.ter_set( tripoint_bub_ms( 72, 67, 0 ), ter_str_id( "t_tree" ) );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        scout_observation::site_reader target_blocked;
        REQUIRE( project_to<coords::omt>( here.get_abs( tripoint_bub_ms( 72, 67, 0 ) ) ) ==
                 footprint.front() );
        CHECK_FALSE( target_blocked.read_character( observer, person, footprint ).visible );
        CHECK_FALSE( target_blocked.read_signal( observer, observer.pos_abs(), person.pos_abs(),
                     footprint ).visible );
    }
    SECTION( "an unavailable lookout and corridor do not hide an existing target" ) {
        std::optional<tripoint_abs_ms> absent_lookout;
        for( int x : { 180, 204 } ) {
            const auto candidate = here.get_abs( tripoint_bub_ms( x, 66, 0 ) );
            if( !MAPBUFFER.lookup_submap_existing( project_to<coords::sm>( candidate ) ) ) {
                absent_lookout = candidate;
                break;
            }
        }
        REQUIRE( absent_lookout );
        observer.setpos( *absent_lookout, false );
        REQUIRE_FALSE( here.inbounds( observer.pos_abs() ) );
        const auto missing = project_to<coords::sm>( observer.pos_abs() );
        REQUIRE( MAPBUFFER.lookup_submap_existing( missing ) == nullptr );
        const auto before_player = target.pos_abs();
        {
            scout_observation::site_reader reader;
            const auto actual = reader.read_character( observer, person );
            CHECK( actual.known );
            CHECK( actual.visible );
            CHECK( reader.read_signal( observer, observer.pos_abs(), person.pos_abs(),
                { person.pos_abs_omt() } ).visible );
            CHECK( reader.cached_view_count() > 0 );
            CHECK( reader.cached_view_count() <= 2 );
            CHECK( target.pos_abs() == before_player );
            CHECK( MAPBUFFER.lookup_submap_cached( missing ) == nullptr );
        } // End this immutable geometry decision before changing the physical scene.
        // The original bearing enters the east edge of this footprint at x95.
        here.ter_set( tripoint_bub_ms( 95, 66, 0 ), ter_str_id( "t_wall" ) );
        here.ter_set( tripoint_bub_ms( 95, 67, 0 ), ter_str_id( "t_wall" ) );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        scout_observation::site_reader reader;
        const auto bearing = scout_observation::site_sightline( observer.pos_abs(), person.pos_abs() );
        const auto entry = std::find_if( bearing.begin(), bearing.end(), [&]( const auto &p ) {
            return project_to<coords::omt>( p ) == person.pos_abs_omt();
        } );
        REQUIRE( entry != bearing.end() );
        CHECK( *entry == here.get_abs( tripoint_bub_ms( 95, 67, 0 ) ) );
        CHECK_FALSE( reader.read_character( observer, person ).visible );
    }
    SECTION( "long diagonal rays use the configured native distance" ) {
        const bool previous_distance = trigdist;
        const bool circular = GENERATE( false, true );
        trigdist = circular;
        on_out_of_scope restore_distance( [&]() {
            trigdist = previous_distance;
        } );
        observer.setpos( here, tripoint_bub_ms( 0, 0, 0 ), false );
        target.setpos( here, tripoint_bub_ms( 95, 71, 0 ), false );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        REQUIRE( square_dist( observer.pos_abs_omt().raw(), target.pos_abs_omt().raw() ) == 3 );
        CHECK_FALSE( observer.sees_without_clairvoyance( here, target ) );
        CHECK( here.sees_with_optics( observer.pos_bub( here ), target.pos_bub( here ), 120, 2.0f ) );
        CHECK( scout_observation::read_character( observer, target ).visible );
    }
    SECTION( "native vision-only barriers and cumulative smoke stay opaque to optics" ) {
        const bool smoke = GENERATE( false, true );
        for( int x = 16; x <= 28; ++x ) {
            if( smoke ) {
                here.add_field( tripoint_bub_ms( x, 66, 0 ), field_type_id( "fd_smoke" ), 2 );
            }
        }
        if( !smoke ) {
            here.ter_set( tripoint_bub_ms( 20, 66, 0 ), ter_str_id( "t_door_glass_frosted_c" ) );
        }
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        CHECK( scout_observation::read_character( observer, target ).visible );
        if( smoke ) {
            for( int x = 74; x <= 82; ++x ) {
                here.add_field( tripoint_bub_ms( x, 66, 0 ), field_type_id( "fd_smoke" ), 2 );
            }
        } else {
            here.ter_set( tripoint_bub_ms( 76, 66, 0 ), ter_str_id( "t_door_glass_frosted_c" ) );
        }
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
        person.setpos( here, tripoint_bub_ms( 32, 66, 0 ), false );
        target.setpos( here, tripoint_bub_ms( 12, 66, 0 ), false );
        here.build_map_cache( 0 );
        CHECK_FALSE( target.sees( here, person ) );
        CHECK_FALSE( observer.sees_with_optics( here, person, 2.0f ) );
        CHECK( scout_observation::read_character( observer, person ).visible );
    }
    SECTION( "wall blocks the same clear daylight ray" ) {
        here.ter_set( tripoint_bub_ms( 76, 66, 0 ), ter_str_id( "t_wall" ) );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
    }
    SECTION( "actual roofs retain z and floors obstruct" ) {
        const int roof = GENERATE( 1, 3 );
        target.setpos( here, tripoint_bub_ms( 84, 66, roof ), false );
        here.ter_set( target.pos_bub( here ), ter_str_id( "t_flat_roof" ) );
        here.invalidate_map_cache( roof );
        here.build_map_cache( roof );
        CHECK( scout_observation::read_character( observer, target ).visible );
        person.setpos( here, tripoint_bub_ms( 84, 67, roof ), false );
        here.ter_set( person.pos_bub( here ), ter_str_id( "t_flat_roof" ) );
        here.invalidate_map_cache( roof );
        here.build_map_cache( roof );
        const auto physical_line = scout_observation::site_sightline( observer.pos_abs(),
                                   person.pos_abs() );
        REQUIRE( physical_line.size() == 72 );
        CHECK( physical_line.back() == person.pos_abs() );
        const std::vector<tripoint_abs_omt> roof_footprint = { person.pos_abs_omt() };
        const auto roof_person = scout_observation::read_character( observer, person );
        CAPTURE( roof, roof_person.reason, here.ambient_light_at( person.pos_bub( here ) ),
                 here.ledge_coverage( observer, person.pos_bub( here ),
                                      &roof_footprint ) );
        CHECK( roof_person.visible );
        // An intervening solid wall at every ray height blocks the exposed target.
        for( int z = 0; z <= roof; ++z ) {
            for( int y = 62; y <= 70; ++y ) {
                here.ter_set( tripoint_bub_ms( 76, y, z ), ter_str_id( "t_wall" ) );
            }
            here.invalidate_map_cache( z );
        }
        here.build_map_cache( roof );
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
        CHECK_FALSE( scout_observation::read_character( observer, person ).visible );
    }
    SECTION( "opaque floors and the far side of a house block exterior actors" ) {
        target.setpos( here, tripoint_bub_ms( 84, 66, 3 ), false );
        here.ter_set( target.pos_bub( here ), ter_str_id( "t_flat_roof" ) );
        for( int z = 1; z <= 3; ++z ) {
            for( int x = 74; x <= 82; ++x ) {
                for( int y = 62; y <= 70; ++y ) {
                    here.ter_set( tripoint_bub_ms( x, y, z ), ter_str_id( "t_floor" ) );
                }
            }
            here.invalidate_map_cache( z );
        }
        here.build_map_cache( 3 );
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
    }
    SECTION( "indoor occupants stay unknown even through a clear open doorway" ) {
        here.ter_set( target.pos_bub( here ), ter_str_id( "t_floor" ) );
        here.ter_set( target.pos_bub( here ) + tripoint( 0, 0, 1 ), ter_str_id( "t_floor" ) );
        here.invalidate_map_cache( 0 );
        here.invalidate_map_cache( 1 );
        here.build_map_cache( 0 );
        REQUIRE_FALSE( here.is_outside( target.pos_bub( here ) ) );
        const auto hidden = scout_observation::read_character( observer, target );
        CHECK_FALSE( hidden.visible );
        CHECK( hidden.reason == "interior_unknown" );
    }
    SECTION( "optics cannot reveal an invisible native actor" ) {
        target.set_mutation( trait_id( "DEBUG_CLOAK" ) );
        REQUIRE( target.is_invisible() );
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
        CHECK_FALSE( observer.sees_without_clairvoyance( here, target ) );
        target.unset_mutation( trait_id( "DEBUG_CLOAK" ) );
    }
    SECTION( "native avatar stealth still reduces binocular recognition distance" ) {
        target.set_mutation( trait_id( "CRAFTY" ) );
        target.set_mutation( trait_id( "CAMO2" ) );
        target.recalculate_enchantment_cache();
        REQUIRE( target.visibility() == 40 );
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
        CHECK_FALSE( observer.sees_without_clairvoyance( here, target ) );
        target.unset_mutation( trait_id( "CRAFTY" ) );
        target.unset_mutation( trait_id( "CAMO2" ) );
        target.recalculate_enchantment_cache();
        CHECK( scout_observation::read_character( observer, target ).visible );
    }
    SECTION( "fresh native eye impairment applies before cached sight limits" ) {
        observer.set_part_hp_cur( bodypart_id( "torso" ), 1 );
        REQUIRE_FALSE( observer.is_dead_state() );
        REQUIRE( scout_observation::read_character( observer, target ).visible );
        const int cached_range = observer.sight_range( 100.0f, 100.0f );
        REQUIRE( cached_range >= MAX_VIEW_DISTANCE );
        observer.add_effect( efftype_id( "boomered" ), 1_hours );
        REQUIRE_FALSE( observer.is_blind() );
        REQUIRE( observer.sight_range( 100.0f, 100.0f ) == cached_range );
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
        scout_observation::site_reader reader;
        CHECK_FALSE( reader.read_signal( observer, observer.pos_abs(), target.pos_abs(),
        { target.pos_abs_omt() } ).visible );
        CHECK( observer.sight_range( 100.0f, 100.0f ) == cached_range );
        CHECK( observer.get_part_hp_cur( bodypart_id( "torso" ) ) == 1 );
        observer.set_mutation( trait_id( "PER_SLIME_OK" ) );
        observer.recalc_sight_limits();
        REQUIRE( observer.has_trait( trait_id( "PER_SLIME_OK" ) ) );
        CHECK( scout_observation::read_character( observer, target ).visible );
        CHECK( reader.read_signal( observer, observer.pos_abs(), target.pos_abs(),
        { target.pos_abs_omt() } ).visible );
        CHECK( observer.get_part_hp_cur( bodypart_id( "torso" ) ) == 1 );
        observer.unset_mutation( trait_id( "PER_SLIME_OK" ) );
        observer.recalc_sight_limits();
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
    }
    SECTION( "sleep and blindness do not acquire people" ) {
        observer.add_effect( efftype_id( "sleep" ), 1_hours );
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
        observer.remove_effect( efftype_id( "sleep" ) );
        observer.add_effect( efftype_id( "blind" ), 1_hours );
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
    }
    SECTION( "dense weather reduces the optical range" ) {
        scoped_weather_override fog( weather_type_id( "fog" ) );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
    }
    SECTION( "unlit night targets disappear while illuminated targets remain visible" ) {
        set_time( calendar::turn_zero );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        CHECK_FALSE( scout_observation::read_character( observer, target ).visible );
        scout_observation::site_reader unlit_signal;
        CHECK_FALSE( unlit_signal.read_signal( observer, observer.pos_abs(), target.pos_abs(),
        { target.pos_abs_omt() } ).visible );
        here.add_field( target.pos_bub( here ), field_type_id( "fd_fire" ), 3 );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        CHECK( scout_observation::read_character( observer, target ).visible );
        scout_observation::site_reader planner;
        CHECK( planner.read( observer, observer.pos_abs(), target.pos_abs() ).visible );
        CHECK( planner.read_signal( observer, observer.pos_abs(), target.pos_abs(),
        { target.pos_abs_omt() } ).visible );
    }
    SECTION( "unavailable geometry stays unknown without map generation" ) {
        observer.setpos( tripoint_abs_ms( 987654, 987654, 0 ), false );
        target.setpos( tripoint_abs_ms( 987726, 987654, 0 ), false );
        const auto read = scout_observation::read_character( observer, target );
        CHECK_FALSE( read.known );
        CHECK_FALSE( read.visible );
        CHECK( read.reason == "geometry_unavailable" );
    }
}

TEST_CASE( "site association follows only an actually observed leaving identity",
           "[binocular_056][site_association]" )
{
    const std::vector<tripoint_abs_omt> site = { tripoint_abs_omt( 10, 10, 0 ) };
    CHECK( scout_observation::associated_with_site( tripoint_abs_omt( 10, 10, 3 ), site ) );
    CHECK_FALSE( scout_observation::associated_with_site( tripoint_abs_omt( 11, 10, 0 ), site ) );
    CHECK( scout_observation::associated_with_site( tripoint_abs_omt( 11, 10, 0 ), site,
    { tripoint_abs_omt( 10, 10, 1 ) } ) );
    CHECK_FALSE( scout_observation::associated_with_site( tripoint_abs_omt( 12, 10, 0 ), site,
    { tripoint_abs_omt( 10, 10, 1 ) } ) );
}

TEST_CASE( "loaded target remains visible to an abstract scout through existing geometry",
           "[binocular_056][abstract_visibility]" )
{
    const auto saved_map_origin = get_map().get_abs_sub();
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 1, false );
    const auto saved_turn = calendar::turn;
    const auto saved_position = get_avatar().pos_abs();
    on_out_of_scope restore( [&]() {
        if( get_map().get_abs_sub() != saved_map_origin ) {
            get_map().load( saved_map_origin, false, false );
        }
        get_avatar().setpos( saved_position, false );
        set_time( saved_turn );
    } );
    set_time_to_day();
    scoped_weather_override weather( weather_type_id( "clear" ) );
    map &here = get_map();
    // Fixed bubble coordinates below describe target-local obstacles only when
    // the window starts on an OMT boundary. Ordinary player relocation can leave
    // an odd submap origin; preserve that incoming window on scope exit.
    const auto aligned_origin = project_to<coords::sm>( project_to<coords::omt>( here.get_abs_sub() ) );
    if( here.get_abs_sub() != aligned_origin ) {
        here.load( aligned_origin, false, false );
    }
    npc observer;
    observer.normalize();
    observer.set_fake( true );
    observer.recalc_sight_limits();
    observer.setpos( here, tripoint_bub_ms( 12, 66, 0 ), false );
    get_avatar().setpos( here, tripoint_bub_ms( 84, 66, 0 ), false );
    here.build_map_cache( 0 );
    REQUIRE( scout_observation::read_character( observer, get_avatar() ).visible );
    // Prepare controlled existing submaps. Only the test generates this fixture;
    // the production reader subsequently uses existing-only retrieval and never calls load.
    map fixture;
    fixture.load( here.get_abs_sub() + point( -6, 0 ), false );
    // Native illumination needs the same full map extent around each remote
    // observer. Generate this explicit test context only; the reader never does.
    std::vector<std::unique_ptr<map>> lighting_context;
    for( const point &offset : {
             point( -7, -4 ), point( -7, 4 ),
             point( -6, -4 ), point( -6, 4 )
         } ) {
        auto context = std::make_unique<map>();
        context->load( here.get_abs_sub() + offset, false );
        for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
            for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
                context->ter_set( tripoint_bub_ms( x, y, 0 ), ter_str_id( "t_grass" ) );
                context->ter_set( tripoint_bub_ms( x, y, 1 ), ter_str_id( "t_open_air" ) );
                context->furn_set( tripoint_bub_ms( x, y, 0 ), furn_str_id( "f_null" ) );
                context->field_at( tripoint_bub_ms( x, y, 0 ) ).clear();
            }
        }
        lighting_context.push_back( std::move( context ) );
    }
    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
            fixture.ter_set( tripoint_bub_ms( x, y, 0 ), ter_str_id( "t_grass" ) );
            fixture.ter_set( tripoint_bub_ms( x, y, 1 ), ter_str_id( "t_open_air" ) );
            fixture.furn_set( tripoint_bub_ms( x, y, 0 ), furn_str_id( "f_null" ) );
            fixture.field_at( tripoint_bub_ms( x, y, 0 ) ).clear();
        }
    }
    here.invalidate_map_cache( 0 );
    here.invalidate_map_cache( 1 );
    here.build_map_cache( 0 );
    observer.setpos( here.get_abs( tripoint_bub_ms( -12, 66, 0 ) ), false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 66, 0 ), false );
    REQUIRE_FALSE( here.inbounds( observer.pos_abs() ) );
    REQUIRE( here.inbounds( get_avatar().pos_abs() ) );
    REQUIRE( rl_dist( observer.pos_abs_omt().xy(), get_avatar().pos_abs_omt().xy() ) == 3 );
    const auto abstract = scout_observation::read_character( observer, get_avatar() );
    CAPTURE( abstract.reason );
    CHECK( abstract.known );
    CHECK( abstract.visible );
    SECTION( "one planning decision reuses overlapping existing geometry" ) {
        scout_observation::site_reader reader;
        const auto resident_count = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
        CHECK( reader.read_character( observer, get_avatar() ).visible );
        const auto prepared = reader.cached_view_count();
        REQUIRE( prepared > 0 );
        REQUIRE( prepared <= 2 );
        CHECK( reader.cached_vehicle_scan_count() == 0 );
        get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
        CHECK_FALSE( reader.read_character( observer, get_avatar() ).visible );
        get_avatar().unset_mutation( trait_id( "DEBUG_CLOAK" ) );
        CHECK( reader.read_character( observer, get_avatar() ).visible );
        CHECK( reader.cached_view_count() == prepared );
        const auto next_origin = here.get_abs( tripoint_bub_ms( -12, 78, 0 ) );
        const auto next_target = here.get_abs( tripoint_bub_ms( 60, 78, 0 ) );
        CHECK( reader.read( observer, next_origin, next_target ).visible );
        CHECK( reader.cached_view_count() > prepared );
        CHECK( reader.cached_view_count() <= prepared + 2 );
        CHECK( reader.cached_vehicle_scan_count() == 0 );
        CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == resident_count );
        // A different decision constructs fresh caches from current physical geometry.
        fixture.ter_set( fixture.get_bub( here.get_abs( tripoint_bub_ms( 54, 78, 0 ) ) ),
                         ter_str_id( "t_wall" ) );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        scout_observation::site_reader next_decision;
        CHECK_FALSE( next_decision.read( observer, next_origin, next_target ).visible );
    }
    SECTION( "remote ledge floors match native visibility" ) {
        observer.setpos( here.get_abs( tripoint_bub_ms( -24, 66, 0 ) ), false );
        get_avatar().setpos( here, tripoint_bub_ms( 48, 66, 1 ), false );
        fixture.ter_set( fixture.get_bub( get_avatar().pos_abs() ), ter_str_id( "t_flat_roof" ) );
        fixture.ter_set( fixture.get_bub( here.get_abs( tripoint_bub_ms( -20, 66, 1 ) ) ),
                         ter_str_id( "t_flat_roof" ) );
        fixture.invalidate_map_cache( 0 );
        fixture.invalidate_map_cache( 1 );
        fixture.build_map_cache( 1 );
        here.invalidate_map_cache( 1 );
        here.build_map_cache( 1 );
        CHECK_FALSE( observer.sees_with_optics( fixture, get_avatar(), 2.0f ) );
        const auto remote = scout_observation::read_character( observer, get_avatar() );
        CAPTURE( remote.reason );
        CHECK( remote.known );
        CHECK( remote.visible );
        // The old distant floor stays irrelevant; an actual target-local wall blocks.
        get_avatar().setpos( here, tripoint_bub_ms( 60, 66, 1 ), false );
        here.ter_set( tripoint_bub_ms( 54, 66, 1 ), ter_str_id( "t_wall" ) );
        here.invalidate_map_cache( 1 );
        here.build_map_cache( 1 );
        CHECK_FALSE( scout_observation::read_character( observer, get_avatar() ).visible );
    }
    SECTION( "abstract physical interruptions survive transient-cache reload" ) {
        bandit_live_world::site_record site;
        auto &outing = site.active_outing;
        observer.setID( character_id( 956220 ), true );
        outing.schema_version = 10;
        outing.kind = bandit_live_world::outing_kind::structural_sortie;
        outing.phase = bandit_live_world::scout_phase::observing;
        outing.owner = bandit_live_world::simulation_owner::abstract;
        outing.selected_watch_kind = bandit_live_world::structural_watch_kind::exact;
        outing.selected_watch_omt = observer.pos_abs_omt();
        outing.target_footprint = { get_avatar().pos_abs_omt() };
        outing.member_ids = { observer.getID() };
        CAPTURE( observer.is_dead_state(), observer.is_blind(), observer.in_sleep_state(),
                 observer.path.size(), observer.omt_path.size(), observer.mission,
                 observer.goal, observer.get_attitude(), observer.get_ai_danger(),
                 observer.pos_abs(), outing.selected_watch_omt,
                 bandit_live_world::target_footprint_watch_distance( outing.selected_watch_omt,
                         outing.target_footprint ), outing.member_is_resolved( observer.getID() ) );
        REQUIRE( scout_observation::watching_member( site, observer ) );
        fixture.add_field( fixture.get_bub( observer.pos_abs() ), field_type_id( "fd_fire" ), 3 );
        CHECK_FALSE( scout_observation::watching_member( site, observer ) );
        fixture.delete_field( fixture.get_bub( observer.pos_abs() ), field_type_id( "fd_fire" ) );
        REQUIRE( scout_observation::watching_member( site, observer ) );
        fixture.invalidate_map_cache( 0 );
        fixture.build_map_cache( 0 );
        SECTION( "resident hostile NPC interrupts outside the active player list" ) {
            observer.set_fac( faction_id( "hells_raiders" ) );
            auto enemy = make_shared_fast<npc>();
            enemy->normalize();
            enemy->setID( character_id( 956225 ), true );
            enemy->set_fac( faction_id( "your_followers" ) );
            enemy->spawn_at_precise( fixture.get_abs( observer.pos_bub( fixture ) + point( 4, 0 ) ) );
            overmap_buffer.insert_npc( enemy );
            on_out_of_scope remove_resident( [&]() {
                overmap_buffer.remove_npc( enemy->getID() );
            } );
            REQUIRE_FALSE( enemy->is_active() );
            REQUIRE( observer.attitude_to( *enemy ) == Creature::Attitude::HOSTILE );
            REQUIRE( observer.sees_without_clairvoyance( fixture, *enemy ) );
            CHECK_FALSE( scout_observation::watching_member( site, observer ) );
            enemy->add_effect( efftype_id( "invisibility" ), 1_hours );
            CHECK( scout_observation::watching_member( site, observer ) );
            observer.set_fac( faction_id( "no_faction" ) );
        }
        SECTION( "ordinary hostile avatar perception interrupts without a temporary seen cache" ) {
            observer.set_fac( faction_id( "hells_raiders" ) );
            get_avatar().setpos( here, tripoint_bub_ms( 2, 66, 0 ), false );
            REQUIRE( observer.attitude_to( get_avatar() ) == Creature::Attitude::HOSTILE );
            REQUIRE( observer.sees_with_optics( fixture, get_avatar(), 1.0f ) );
            CHECK_FALSE( scout_observation::watching_member( site, observer ) );
            const bool partial_smoke = GENERATE( false, true );
            if( partial_smoke ) {
                for( int x = -10; x <= -4; ++x ) {
                    fixture.add_field( fixture.get_bub( here.get_abs( tripoint_bub_ms( x, 66, 0 ) ) ),
                                       field_type_id( "fd_smoke" ), 2 );
                }
                fixture.invalidate_map_cache( 0 );
                fixture.build_map_cache( 0 );
                REQUIRE_FALSE( observer.sees_with_optics( fixture, get_avatar(), 1.0f ) );
                REQUIRE( observer.sees_without_clairvoyance_physical( fixture, get_avatar() ) );
                CHECK_FALSE( scout_observation::watching_member( site, observer ) );
            }
            get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
            CHECK( scout_observation::watching_member( site, observer ) );
            get_avatar().unset_mutation( trait_id( "DEBUG_CLOAK" ) );
            get_avatar().setpos( here, tripoint_bub_ms( 60, 66, 0 ), false );
            CHECK( scout_observation::watching_member( site, observer ) );
            observer.set_fac( faction_id( "no_faction" ) );
        }
        monster *enemy = g->place_critter_at( mtype_id( "mon_zombie" ), tripoint_bub_ms( 2, 66, 0 ) );
        REQUIRE( enemy );
        on_out_of_scope remove_enemy( [&]() {
            g->clear_zombies();
        } );
        fixture.invalidate_map_cache( 0 );
        fixture.build_map_cache( 0 );
        REQUIRE( observer.sees_without_clairvoyance( fixture, *enemy ) );
        CHECK_FALSE( scout_observation::watching_member( site, observer ) );
        SECTION( "remote ordinary threat recognition uses its actual observer light" ) {
            const bool lamp_at_night = GENERATE( false, true );
            if( lamp_at_night ) {
                set_time( calendar::turn_zero );
                fixture.add_field( observer.pos_bub( fixture ), field_type_id( "fd_biolum" ), 3 );
            }
            // fd_shadow is a native physical field with a final 3.7 light
            // override. It exercises real adaptation, not a fabricated lm.
            REQUIRE( fixture.add_field( enemy->pos_bub( fixture ), field_type_id( "fd_shadow" ), 1 ) );
            fixture.invalidate_map_cache( 0 );
            fixture.invalidate_map_cache( 1 );
            fixture.build_map_cache( 0 );
            here.invalidate_map_cache( 0 );
            here.invalidate_map_cache( 1 );
            here.build_map_cache( 0 );
            const float origin_light = fixture.ambient_light_at( observer.pos_bub( fixture ) );
            const float wrong_origin_light = here.ambient_light_at( observer.pos_bub( here ) );
            REQUIRE( origin_light > wrong_origin_light );
            const float shadow_light = fixture.ambient_light_at( enemy->pos_bub( fixture ) );
            const int chosen_distance = observer.sight_range( shadow_light, origin_light ) + 1;
            REQUIRE( chosen_distance < MAPSIZE * SEEX / 2 );
            enemy->setpos( observer.pos_abs() + point( chosen_distance, 0 ) );
            REQUIRE( fixture.add_field( enemy->pos_bub( fixture ), field_type_id( "fd_shadow" ), 1 ) );
            fixture.invalidate_map_cache( 0 );
            fixture.build_map_cache( 0 );
            here.invalidate_map_cache( 0 );
            here.build_map_cache( 0 );
            const int distance = rl_dist( observer.pos_abs(), enemy->pos_abs() );
            const float target_light = fixture.ambient_light_at( enemy->pos_bub( fixture ) );
            CAPTURE( origin_light, wrong_origin_light, target_light, distance );
            REQUIRE( observer.sight_range( target_light, origin_light ) < distance );
            REQUIRE( observer.sight_range( target_light, wrong_origin_light ) >= distance );
            if( lamp_at_night ) {
                REQUIRE( fixture.get_field_intensity( observer.pos_bub( fixture ),
                         field_type_id( "fd_biolum" ) ) == 3 );
            }
            const bool native_visible = observer.sees_without_clairvoyance( fixture, *enemy );
            if( !lamp_at_night ) {
                CHECK_FALSE( native_visible );
            }
            // Native recognition also permits a target brighter than natural
            // night light; preserve that rule instead of forcing a dark result.
            CHECK( scout_observation::watching_member( site, observer ) == !native_visible );
        }
        SECTION( "ordinary non-avatar perception survives partial smoke and translucent terrain" ) {
            const bool translucent = GENERATE( false, true );
            if( translucent ) {
                fixture.ter_set( fixture.get_bub( here.get_abs( tripoint_bub_ms( -5, 66, 0 ) ) ),
                                 ter_str_id( "t_door_glass_frosted_c" ) );
            } else {
                for( int x = -10; x <= -4; ++x ) {
                    fixture.add_field( fixture.get_bub( here.get_abs( tripoint_bub_ms( x, 66, 0 ) ) ),
                                       field_type_id( "fd_smoke" ), 2 );
                }
            }
            fixture.invalidate_map_cache( 0 );
            fixture.build_map_cache( 0 );
            REQUIRE( observer.sees_without_clairvoyance( fixture, *enemy ) );
            CHECK_FALSE( observer.sees_with_optics( fixture, *enemy, 1.0f ) );
            CHECK_FALSE( scout_observation::watching_member( site, observer ) );
        }
        std::ostringstream bytes;
        JsonOut writer( bytes );
        observer.serialize( writer );
        npc loaded;
        loaded.deserialize( json_loader::from_string( bytes.str() ).get_object() );
        CHECK( loaded.get_ai_danger() == 0 );
        const bool native_threat_visible = observer.sees_without_clairvoyance( fixture, *enemy );
        CHECK( loaded.sees_without_clairvoyance( fixture, *enemy ) == native_threat_visible );
        CHECK( scout_observation::watching_member( site, loaded ) == !native_threat_visible );
    }
    SECTION( "unknown hostile optics do not veto an independently visible target" ) {
        bandit_live_world::site_record site;
        auto &outing = site.active_outing;
        observer.setID( character_id( 956221 ), true );
        outing.schema_version = 10;
        outing.kind = bandit_live_world::outing_kind::structural_sortie;
        outing.job_type = "scout";
        outing.phase = bandit_live_world::scout_phase::observing;
        outing.owner = bandit_live_world::simulation_owner::abstract;
        outing.selected_watch_kind = bandit_live_world::structural_watch_kind::exact;
        outing.selected_watch_omt = observer.pos_abs_omt();
        outing.target_footprint = { get_avatar().pos_abs_omt() };
        outing.member_ids = { observer.getID() };
        monster *enemy = g->place_critter_at( mtype_id( "mon_zombie" ), tripoint_bub_ms( 2, 66, 0 ) );
        REQUIRE( enemy );
        on_out_of_scope remove_enemy( [&]() {
            g->clear_zombies();
        } );
        // Its ray fits the prepared remote map, but its full native-lighting
        // neighbourhood extends into geometry this fixture has not generated.
        enemy->setpos( here.get_abs( tripoint_bub_ms( -60, 66, 0 ) ) );
        REQUIRE( rl_dist( observer.pos_abs(), enemy->pos_abs() ) <= MAX_VIEW_DISTANCE );
        scout_observation::site_reader optics;
        REQUIRE( optics.read_character( observer, get_avatar() ).visible );
        std::string reason;
        CHECK( optics.watching_member( site, observer, &reason ) );
        INFO( reason );
        CHECK( reason == "eligible_hostile_optics_unknown" );
        CHECK( optics.perceived_threats( observer ).empty() );
        std::ostringstream bytes;
        JsonOut writer( bytes );
        observer.serialize( writer );
        npc loaded;
        loaded.deserialize( json_loader::from_string( bytes.str() ).get_object() );
        CHECK( optics.watching_member( site, loaded ) );
        CHECK( optics.read_character( loaded, get_avatar() ).visible );
    }
    SECTION( "a concealed populated horde uses one bounded observer window" ) {
        bandit_live_world::site_record site;
        auto &outing = site.active_outing;
        observer.setID( character_id( 956220 ), true );
        outing.schema_version = 10;
        outing.kind = bandit_live_world::outing_kind::structural_sortie;
        outing.phase = bandit_live_world::scout_phase::observing;
        outing.owner = bandit_live_world::simulation_owner::abstract;
        outing.selected_watch_kind = bandit_live_world::structural_watch_kind::exact;
        outing.selected_watch_omt = observer.pos_abs_omt();
        outing.target_footprint = { get_avatar().pos_abs_omt() };
        outing.member_ids = { observer.getID() };
        for( int y = 54; y <= 78; ++y ) {
            here.ter_set( tripoint_bub_ms( 0, y, 0 ), ter_str_id( "t_wall" ) );
        }
        for( int y = 61; y <= 70; ++y ) {
            for( int x : {
                     2, 4
                 } ) {
                REQUIRE( g->place_critter_at( mtype_id( "mon_zombie" ), tripoint_bub_ms( x, y, 0 ) ) );
            }
        }
        on_out_of_scope remove_horde( [&]() {
            g->clear_zombies();
        } );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        CHECK( scout_observation::watching_member( site, observer ) );
        here.ter_set( tripoint_bub_ms( 0, 66, 0 ), ter_str_id( "t_grass" ) );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        CHECK_FALSE( scout_observation::watching_member( site, observer ) );
        here.ter_set( tripoint_bub_ms( 0, 66, 0 ), ter_str_id( "t_wall" ) );
    }
    SECTION( "a loaded vehicle crossing the bubble edge retains opaque geometry without moving" ) {
        vehicle *barrier = here.add_vehicle( vproto_id( "vehicle_camera_test" ),
                                             tripoint_bub_ms( 1, 66, 0 ), 0_degrees, 0, veh_spawn_status::UNDAMAGED );
        REQUIRE( barrier );
        on_out_of_scope remove_vehicle( [&]() {
            here.destroy_vehicle( barrier );
        } );
        const auto position = barrier->pos_abs();
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        const auto blocked_vehicle = scout_observation::read_character( observer, get_avatar() );
        CAPTURE( blocked_vehicle.reason );
        CHECK( blocked_vehicle.known );
        CHECK( blocked_vehicle.visible );
        CHECK( barrier->pos_abs() == position );
    }
    SECTION( "authenticated remote overhangs preserve native target-local geometry" ) {
        const int ray_x = GENERATE( -13, -12 );
        observer.setpos( here.get_abs( tripoint_bub_ms( ray_x, 30, 0 ) ), false );
        get_avatar().setpos( here.get_abs( tripoint_bub_ms( ray_x, 102, 0 ) ), false );
        REQUIRE( scout_observation::read_character( observer, get_avatar() ).visible );
        observer.setpos( here.get_abs( tripoint_bub_ms( ray_x, 48, 0 ) ), false );
        get_avatar().setpos( here.get_abs( tripoint_bub_ms( ray_x, 78, 0 ) ), false );
        observer.set_fac( faction_id( "hells_raiders" ) );
        observer.setID( character_id( 956220 ), true );
        bandit_live_world::site_record site;
        auto &outing = site.active_outing;
        outing.schema_version = 10;
        outing.kind = bandit_live_world::outing_kind::structural_sortie;
        outing.phase = bandit_live_world::scout_phase::observing;
        outing.owner = bandit_live_world::simulation_owner::abstract;
        outing.selected_watch_kind = bandit_live_world::structural_watch_kind::exact;
        outing.selected_watch_omt = observer.pos_abs_omt();
        outing.target_footprint = { project_to<coords::omt>( here.get_abs( tripoint_bub_ms( 60, 66, 0 ) ) ) };
        outing.member_ids = { observer.getID() };
        REQUIRE_FALSE( scout_observation::watching_member( site, observer ) );
        vehicle *barrier = fixture.add_vehicle( vproto_id( "vehicle_camera_test" ),
                                                fixture.get_bub( here.get_abs( tripoint_bub_ms( ray_x == -13 ? -12 : -13, 66, 0 ) ) ),
                                                0_degrees, 0, veh_spawn_status::UNDAMAGED );
        REQUIRE( barrier );
        on_out_of_scope remove_vehicle( [&]() {
            fixture.destroy_vehicle( barrier );
        } );
        const auto position = barrier->pos_abs();
        const auto blocked = scout_observation::read_character( observer, get_avatar() );
        CAPTURE( blocked.reason );
        CHECK( blocked.known );
        CHECK( blocked.visible );
        CHECK( scout_observation::watching_member( site, observer ) );
        CHECK( barrier->pos_abs() == position );
        REQUIRE( fixture.displace_vehicle( *barrier, tripoint_rel_ms( 0, 12, 0 ) ) );
        fixture.invalidate_map_cache( 0 );
        fixture.build_map_cache( 0 );
        const bool native = observer.sees_site_with_optics( fixture, get_avatar(), 2.0f,
                            { get_avatar().pos_abs_omt() }, g->natural_light_level( 0 ) );
        const auto moved_position = barrier->pos_abs();
        const auto target_local = scout_observation::read_character( observer, get_avatar() );
        CHECK( target_local.known );
        CHECK( target_local.visible == native );
        CHECK( barrier->pos_abs() == moved_position );
    }
    SECTION( "opaque vehicle parts beyond the bubble stay unknown" ) {
        observer.setpos( here.get_abs( tripoint_bub_ms( -1, 30, 0 ) ), false );
        get_avatar().setpos( here.get_abs( tripoint_bub_ms( -1, 102, 0 ) ), false );
        REQUIRE( scout_observation::read_character( observer, get_avatar() ).visible );
        vehicle *barrier = here.add_vehicle( vproto_id( "vehicle_camera_test" ),
                                             tripoint_bub_ms( 1, 66, 0 ), 0_degrees, 0, veh_spawn_status::UNDAMAGED );
        REQUIRE( barrier );
        on_out_of_scope remove_vehicle( [&]() {
            here.destroy_vehicle( barrier );
        } );
        REQUIRE( here.displace_vehicle( *barrier, tripoint_rel_ms::west ) );
        const auto position = barrier->pos_abs();
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        const auto blocked = scout_observation::read_character( observer, get_avatar() );
        CAPTURE( blocked.reason );
        CHECK( blocked.known );
        CHECK( blocked.visible );
        CHECK( barrier->pos_abs() == position );
    }
    if( get_avatar().pos_abs() == here.get_abs( tripoint_bub_ms( 60, 66, 0 ) ) ) {
        here.ter_set( tripoint_bub_ms( 54, 66, 0 ), ter_str_id( "t_wall" ) );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        const auto blocked = scout_observation::read_character( observer, get_avatar() );
        const auto wall = here.get_abs( tripoint_bub_ms( 54, 66, 0 ) );
        const auto *stored = MAPBUFFER.lookup_submap_existing( project_to<coords::sm>( wall ) );
        point_sm_ms wall_offset;
        auto wall_local = tripoint_bub_ms( 54, 66, 0 );
        const auto *loaded = map_meddler::unsafe_get_submap_at( wall_local, wall_offset );
        INFO( "optical_fixture map=" << here.get_abs_sub() << " remote=" << fixture.get_abs_sub()
              << " wall=" << wall << " terrain=" << here.ter( tripoint_bub_ms( 54, 66, 0 ) ).id()
              << " stored=" << static_cast<bool>( stored ) << " same_submap=" << ( stored == loaded )
              << " stored_wall=" << ( stored ? stored->get_ter( wall_offset ).id().str() : "missing" )
              << " target=" << get_avatar().pos_abs() << " observer=" << observer.pos_abs()
              << " reason=" << blocked.reason );
        CHECK( blocked.known );
        CHECK_FALSE( blocked.visible );
    }
}

namespace
{
class observation_light_fixture : public map
{
        std::vector<std::unique_ptr<submap>> uniform_inputs;
    public:
        using map::get_submap_at_grid;
        using map::build_transparency_cache;
        using map::do_vehicle_caching;
        void install_input( const tripoint_rel_sm &p, submap *input ) {
            setsubmap( get_nonant( p ), input );
            invalidate_map_cache( p.z() );
        }
        void uniform_input_window( const tripoint_abs_sm &origin, bool include_roof ) {
            set_abs_sub( origin );
            for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
                for( int x = 0; x < MAPSIZE; ++x ) {
                    for( int y = 0; y < MAPSIZE; ++y ) {
                        auto input = std::make_unique<submap>();
                        const ter_str_id terrain( z == 5 ? "t_flat_roof" :
                                                  z == 0 ? "t_floor" : "t_open_air" );
                        input->set_all_ter( terrain, true );
                        if( include_roof && z == 5 && x == 0 && y == 0 ) {
                            // Same terrain, a genuine native nonuniform flag.
                            input->set_ter( point_sm_ms::zero, terrain );
                        }
                        install_input( tripoint_rel_sm( x, y, z ), input.get() );
                        uniform_inputs.push_back( std::move( input ) );
                    }
                }
            }
        }
        void observe_light( const int z ) {
            set_lightmap_cache_dirty( z );
            generate_lightmap( z, true );
        }
        std::optional<float> observe_point( const tripoint_bub_ms &target,
                const std::function<bool( const tripoint_bub_ms & )> &require_cell,
                std::optional<float> *source = nullptr,
                const std::function<void( int, int )> *prepare = nullptr ) {
            return generate_observed_lightmap( target, require_cell, nullptr, nullptr, source, prepare );
        }
        std::optional<float> observe_decision( const tripoint_bub_ms &target,
                const std::function<bool( const tripoint_bub_ms & )> &require_cell,
                const std::function<bool( float )> &sufficient,
                std::optional<float> *source = nullptr ) {
            return generate_observed_lightmap( target, require_cell, nullptr, &sufficient, source );
        }
};

std::string sunlight_entry_bytes( const submap &sample, const tripoint_abs_sm &position )
{
    std::ostringstream bytes;
    JsonOut out( bytes );
    out.start_object();
    out.member( "version", savegame_version );
    out.member( "coordinates", position.raw() );
    sample.store( out );
    out.end_object();
    return bytes.str();
}

void install_uniform_observation_inputs( const tripoint_abs_sm &origin, bool serialized = false )
{
    // Native owners created before loading/observer placement. The ordinary
    // terrain-wipe helper would make every upper submap nonuniform.
    for( int z = -1; z <= OVERMAP_HEIGHT; ++z ) {
        for( int x = 0; x < MAPSIZE + 4; ++x ) {
            for( int y = 0; y < MAPSIZE + 4; ++y ) {
                const tripoint_abs_sm p( origin.xy() + point( x, y ), z );
                CAPTURE( p );
                REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( p ) );
                auto value = std::make_unique<submap>();
                value->set_all_ter( ter_str_id( z < 0 ? "t_rock" : z == 0 ? "t_grass" : "t_open_air" ),
                                    true );
                if( serialized ) {
                    value->ensure_nonuniform(); // Native save must retain these physical inputs.
                }
                REQUIRE( MAPBUFFER.add_submap( p, value ) );
            }
        }
    }
}
}

TEST_CASE( "deferred optical preparation preserves the native physical request and result",
           "[binocular_056][observation_cache_preparation]" )
{
    scoped_weather_override weather( weather_type_id( "clear" ) );
    const auto previous_turn = calendar::turn;
    const auto previous_rng = rng_get_engine();
    on_out_of_scope restore( [&]() { set_time( previous_turn ); rng_get_engine() = previous_rng; } );
    set_time_to_day();
    const auto now = calendar::turn;
    const auto residents = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    for( const std::string mode : { "solar_roof", "field_override", "missing_target", "partial_plane" } ) {
        DYNAMIC_SECTION( mode ) {
            observation_light_fixture eager;
            observation_light_fixture deferred;
            const tripoint_abs_sm origin( 2300, 2300, 0 );
            const tripoint_bub_ms target( 66, 66, 0 );
            eager.uniform_input_window( origin, true );
            deferred.uniform_input_window( origin, true );
            for( auto *view : { &eager, &deferred } ) {
                REQUIRE( view->add_field( target, field_type_id( "fd_spotlight" ), 1 ) );
                if( mode == "field_override" ) {
                    REQUIRE( view->add_field( target, field_type_id( "fd_darkness" ), 1 ) );
                }
            }
            std::vector<tripoint_bub_ms> eager_inputs;
            std::vector<tripoint_bub_ms> deferred_inputs;
            std::vector<std::pair<int, int>> preparations;
            const auto available = [&]( const tripoint_bub_ms &cell ) {
                return eager.inbounds( cell ) &&
                       !( mode == "missing_target" && cell == target ) &&
                       !( mode == "partial_plane" && cell == target + point( -MAX_VIEW_DISTANCE - 1,
                               -MAX_VIEW_DISTANCE - 1 ) );
            };
            // Both use native cache builders over the same fully installed
            // physical inputs. The old callback prepares eagerly; the new
            // owner prepares only the levels requested by the native consumer.
            std::set<int> eager_ready_levels;
            std::set<int> deferred_ready_levels;
            const auto prepare = []( observation_light_fixture &view, int low, int high,
            std::set<int> &ready ) {
                std::set<int> fresh;
                for( int z = low; z <= high; ++z ) {
                    if( ready.insert( z ).second ) {
                        fresh.insert( z );
                    }
                }
                for( int z : fresh ) {
                    view.build_outside_cache( z );
                    view.build_transparency_cache( z );
                    view.build_floor_cache( z );
                }
                std::set<int> vehicles = fresh;
                for( int z : fresh ) {
                    if( z > -OVERMAP_DEPTH ) {
                        vehicles.insert( z - 1 );
                    }
                }
                for( int z : vehicles ) {
                    view.do_vehicle_caching( z );
                }
            };
            bool eager_ready = false;
            const auto require_eager = [&]( const tripoint_bub_ms &point ) {
                eager_inputs.push_back( point );
                if( !available( point ) ) {
                    return false;
                }
                if( !eager_ready ) {
                    prepare( eager, -OVERMAP_DEPTH, OVERMAP_HEIGHT, eager_ready_levels );
                    eager_ready = true;
                }
                return true;
            };
            const auto require_deferred = [&]( const tripoint_bub_ms &point ) {
                deferred_inputs.push_back( point );
                return available( point );
            };
            const std::function<void( int, int )> consume = [&]( int low, int high ) {
                preparations.emplace_back( low, high );
                prepare( deferred, low, high, deferred_ready_levels );
            };
            std::optional<float> eager_source;
            std::optional<float> deferred_source;
            // Catch's sectionStarting callback reseeds before this section.
            // Snapshot the actual native read boundary, after that framework
            // setup and physical fixture construction; never rewrite its RNG.
            const auto engine = rng_get_engine();
            const auto expected = eager.observe_point( target, require_eager, &eager_source );
            CHECK( rng_get_engine() == engine );
            const auto actual = deferred.observe_point( target, require_deferred, &deferred_source, &consume );
            CHECK( rng_get_engine() == engine );
            CHECK( eager_inputs == deferred_inputs );
            CHECK( actual == expected );
            CHECK( deferred_source == eager_source );
            if( mode == "solar_roof" || mode == "field_override" ) {
                REQUIRE( expected );
                REQUIRE( eager_source );
                CHECK( *eager_source > 0 );
                CHECK_FALSE( preparations.empty() );
                if( mode == "field_override" ) {
                    CHECK( *actual == 0 );
                }
            } else {
                CHECK_FALSE( actual );
                CHECK_FALSE( deferred_source );
            }
            if( mode == "missing_target" ) {
                CHECK( preparations.empty() );
            }
            CHECK( calendar::turn == now );
            CHECK( rng_get_engine() == engine );
            CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == residents );
        }
    }
}

TEST_CASE( "uniform native rain transparency includes its northwest outside halo",
           "[binocular_056][observation_uniform_corner]" )
{
    clear_avatar();
    clear_npcs();
    map &here = get_map();
    const auto old_origin = here.get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    const auto old_time = calendar::turn;
    on_out_of_scope restore( [&]() {
        here.load( old_origin, false, false );
        get_avatar().setpos( old_player, false );
        set_time( old_time );
        MAPBUFFER.clear_outside_reality_bubble();
    } );
    const tripoint_abs_sm input_origin( 600, 600, 0 );
    install_uniform_observation_inputs( input_origin );
    here.load( input_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    REQUIRE( here.inbounds( get_avatar().pos_abs() ) );
    scoped_weather_override weather( weather_type_id( "rain" ) );
    set_time_to_day();
    const tripoint_bub_ms target( 100, 71, 0 );
    here.ter_set( target, ter_str_id( "t_floor" ) );
    here.ter_set( target + tripoint::above, ter_str_id( "t_open_air_rooved" ) );
    // Both native windows include this off-ray upper fast-fill blocker.
    here.ter_set( tripoint_bub_ms( 40, 4, 3 ), ter_str_id( "t_wall" ) );
    // This actual indoor cell is outside the requested upper point's halo,
    // but determines the uniform submap's native northwest weather sample.
    here.ter_set( tripoint_bub_ms( 95, 59, 2 ), ter_str_id( "t_open_air_rooved" ) );
    here.invalidate_map_cache( 2 );
    here.build_map_cache( 0 );
    REQUIRE_FALSE( here.get_cache_ref( 1 ).outside_cache[target.x()][target.y()] );
    REQUIRE_FALSE( here.get_cache_ref( 1 ).floor_cache[target.x()][target.y()] );
    observation_light_fixture oracle;
    oracle.load( input_origin, false );
    oracle.build_map_cache( 0 );
    oracle.observe_light( 0 );
    const float native = oracle.ambient_light_at( target );
    const auto upper_sm = project_to<coords::sm>( here.get_abs( target + tripoint( 0, 0, 2 ) ) );
    const submap *upper = MAPBUFFER.lookup_submap_cached( upper_sm );
    REQUIRE( upper );
    REQUIRE( upper->is_uniform() );
    const auto sample = here.get_bub( project_to<coords::ms>( upper_sm ) );
    REQUIRE_FALSE( here.get_cache_ref( 2 ).outside_cache[sample.x()][sample.y()] );
    CHECK( here.get_cache_ref( 2 ).transparency_cache[target.x()][target.y()] ==
           Approx( LIGHT_TRANSPARENCY_OPEN_AIR ) );
    monster native_body( mtype_id( "mon_mi_go" ) );
    const int range = 2 * native_body.sight_range( native );
    REQUIRE( range < 100 );
    const tripoint_bub_ms origin( target.x() - range, target.y(), target.z() );
    monster *observer = g->place_critter_at( mtype_id( "mon_mi_go" ), origin );
    REQUIRE( observer );
    on_out_of_scope remove_observer( [&]() { g->remove_zombie( *observer ); } );
    const bool expected = oracle.sees_with_optics( origin, target, range, 2.0f );
    scout_observation::site_reader reader;
    const auto actual = reader.read_access( *observer, here.get_abs( target ) );
    const auto point = reader.cached_lighting_at( here.get_abs( target ) );
    REQUIRE( point );
    std::cout << "uniform_corner native=" << native << " native_range=" << range
              << " actual_known=" << actual.visibility.known
              << " actual_visible=" << actual.visibility.visible
              << " native_visible=" << expected << " projected=" << *point
              << " reason=" << actual.visibility.reason << std::endl;
    REQUIRE( actual.visibility.known );
    CHECK( *point == Approx( native ) );
    CHECK( actual.visibility.visible == expected );
    CHECK( actual.facts.empty() == !expected );
}

TEST_CASE( "cold serialized light vehicles need authoritative native coordinates",
           "[binocular_056][observation_cold_vehicle]" )
{
    const bool overhang = GENERATE( false, true );
    CAPTURE( overhang );
    clear_avatar();
    clear_npcs();
    map &here = get_map();
    const auto old_origin = here.get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    const auto old_time = calendar::turn;
    on_out_of_scope restore( [&]() {
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            here.clear_vehicle_list( z );
        }
        here.load( old_origin, false, false );
        get_avatar().setpos( old_player, false );
        set_time( old_time );
        MAPBUFFER.clear_outside_reality_bubble();
    } );
    const tripoint_abs_sm input_origin( 700, 700, 0 );
    install_uniform_observation_inputs( input_origin, true );
    here.load( input_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    REQUIRE( here.inbounds( get_avatar().pos_abs() ) );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time( calendar::turn_zero );
    const auto origin = here.get_abs( tripoint_bub_ms( 12, 71, 0 ) );
    const auto target = here.get_abs( tripoint_bub_ms( overhang ? 100 : 84, 71, 0 ) );
    vehicle *lamp = here.add_vehicle( vproto_id( "none" ), tripoint_bub_ms( overhang ? 6 : 84, 73, 0 ),
                                    0_degrees, 0, veh_spawn_status::UNDAMAGED );
    REQUIRE( lamp );
    REQUIRE( lamp->install_part( here, point_rel_ms::zero, vpart_id( "frame" ) ) >= 0 );
    REQUIRE( lamp->install_part( here, point_rel_ms::zero, vpart_id( "small_storage_battery" ) ) >= 0 );
    if( overhang ) {
        for( int x = 1; x <= 100; ++x ) {
            REQUIRE( lamp->install_part( here, point_rel_ms( x, 0 ), vpart_id( "frame" ) ) >= 0 );
        }
    }
    const int light = lamp->install_part( here, point_rel_ms( overhang ? 100 : 0, 0 ),
                                        vpart_id( "floodlight" ) );
    REQUIRE( light >= 0 );
    lamp->refresh();
    here.add_vehicle_to_cache( lamp );
    lamp->charge_battery( here, 500 );
    lamp->part( light ).enabled = true;
    lamp->power_parts( here );
    REQUIRE_FALSE( lamp->lights().empty() );
    const auto owner = lamp->sm_pos;
    REQUIRE( owner != tripoint_abs_sm::zero );
    const auto position = lamp->pos_abs();
    const auto light_position = lamp->abs_part_pos( light );
    if( overhang ) {
        const auto target_origin = project_to<coords::sm>( target ) - point( MAPSIZE / 2, MAPSIZE / 2 );
        REQUIRE( owner.x() < target_origin.x() );
        REQUIRE( project_to<coords::sm>( light_position ).x() >= target_origin.x() );
    }
    const auto serialize = []( const vehicle &v ) {
        std::ostringstream bytes;
        JsonOut out( bytes );
        v.serialize( out );
        return bytes.str();
    };
    const auto body_before = serialize( *lamp );
    monster *observer = g->place_critter_at( mtype_id( "mon_mi_go" ), here.get_bub( origin ) );
    REQUIRE( observer );
    on_out_of_scope remove_observer( [&]() { g->remove_zombie( *observer ); } );
    const auto near = [&]() {
        scout_observation::site_reader reader;
        return reader.read_access( *observer, target );
    }();
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    const float native_light = here.ambient_light_at( here.get_bub( target ) );
    const int native_range = 2 * observer->sight_range( native_light );
    const bool native_visible = here.sees_with_optics( here.get_bub( origin ), here.get_bub( target ),
                                native_range, 2.0f );
    CAPTURE( native_light, native_range, native_visible, near.visibility.reason );
    REQUIRE( native_visible );
    REQUIRE( near.visibility.known );
    REQUIRE( near.visibility.visible );
    REQUIRE_FALSE( near.facts.empty() );
    CHECK( serialize( *lamp ) == body_before );
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        here.clear_vehicle_list( z );
    }
    here.load( input_origin + point( 40, 40 ), false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    MAPBUFFER.save();
    MAPBUFFER.clear_outside_reality_bubble();
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( owner ) );
    // Existing-only native decode establishes the serialized owner and index,
    // while deliberately not invoking map::loadn's coordinate binding.
    const submap *stored = MAPBUFFER.lookup_submap_existing( owner );
    REQUIRE( stored );
    REQUIRE( stored->vehicles.size() == 1 );
    const vehicle &unbound = *stored->vehicles.front();
    REQUIRE( unbound.sm_pos == tripoint_abs_sm::zero );
    REQUIRE( unbound.pos_abs() != position );
    const auto unbound_before = serialize( unbound );
    const auto cold = [&]() {
        scout_observation::site_reader reader;
        return reader.read_access( *observer, target );
    }();
    CHECK_FALSE( cold.visibility.known );
    CHECK_FALSE( cold.visibility.visible );
    CHECK( cold.facts.empty() );
    {
        scout_observation::site_reader repeated;
        CHECK_FALSE( repeated.read_access( *observer, target ).visibility.known );
    }
    CHECK( serialize( unbound ) == unbound_before );
    CHECK( unbound.sm_pos == tripoint_abs_sm::zero );
    CHECK( calendar::turn == calendar::turn_zero );
    CHECK( observer->pos_abs() == origin );
}

TEST_CASE( "a genuine native zero vehicle owner remains usable for observation",
           "[binocular_056][observation_zero_vehicle]" )
{
    clear_avatar();
    clear_npcs();
    map &here = get_map();
    const auto old_origin = here.get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    const auto old_time = calendar::turn;
    on_out_of_scope restore( [&]() {
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            here.clear_vehicle_list( z );
        }
        here.load( tripoint_abs_sm::zero, true, false );
        clear_vehicles( &here );
        here.load( old_origin, false, false );
        get_avatar().setpos( old_player, false );
        set_time( old_time );
    } );
    here.load( tripoint_abs_sm::zero, true, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    clear_map_with_vision( -1, 2, false );
    std::vector<std::unique_ptr<map>> physical_inputs;
    for( const point &offset : { point( -6, -6 ), point( -6, 0 ), point( 0, -6 ) } ) {
        auto input = std::make_unique<map>();
        input->load( here.get_abs_sub() + offset, false, false );
        wipe_map_terrain_with_vision( input.get(), false );
        physical_inputs.push_back( std::move( input ) );
    }
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time_to_day();
    vehicle *body = here.add_vehicle( vproto_id( "none" ), tripoint_bub_ms( 6, 6, 0 ),
                                     0_degrees, 0, veh_spawn_status::UNDAMAGED );
    REQUIRE( body );
    REQUIRE( body->install_part( here, point_rel_ms::zero, vpart_id( "frame" ) ) >= 0 );
    body->refresh();
    here.add_vehicle_to_cache( body );
    REQUIRE( body->sm_pos == tripoint_abs_sm::zero );
    const auto position = body->pos_abs();
    const auto origin = here.get_abs( tripoint_bub_ms( 12, 24, 0 ) );
    const auto target = here.get_abs( tripoint_bub_ms( 36, 24, 0 ) );
    monster *observer = g->place_critter_at( mtype_id( "mon_mi_go" ), here.get_bub( origin ) );
    REQUIRE( observer );
    on_out_of_scope remove_observer( [&]() { g->remove_zombie( *observer ); } );
    const auto read = [&]() {
        scout_observation::site_reader reader;
        return reader.read_access( *observer, target );
    };
    const auto near = read();
    REQUIRE( near.visibility.known );
    REQUIRE( near.visibility.visible );
    REQUIRE_FALSE( near.facts.empty() );
    here.load( tripoint_abs_sm( 20, 20, 0 ), false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    const auto far = read();
    CHECK( far.visibility.known == near.visibility.known );
    CHECK( far.visibility.visible == near.visibility.visible );
    CHECK( far.facts == near.facts );
    CHECK( body->sm_pos == tripoint_abs_sm::zero );
    CHECK( body->pos_abs() == position );
}

TEST_CASE( "prospective lookout geometry preserves footing range and physical occlusion",
           "[binocular_056][prospective_lookout_geometry]" )
{
    const auto old_turn = calendar::turn;
    const auto old_position = get_avatar().pos_abs();
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 1, false );
    map &here = get_map();
    on_out_of_scope restore( [&]() {
        clear_map_with_vision( -1, 1, false );
        get_avatar().setpos( old_position, false );
        set_time( old_turn );
    } );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time( calendar::turn_zero + 1_days );
    const tripoint_bub_ms standing( 12, 66, 0 );
    const tripoint_bub_ms target( 84, 66, 0 );
    const auto from = here.get_abs( standing );
    const auto to = here.get_abs( target );
    here.build_map_cache( 0 );
    scout_observation::site_reader initial;
    REQUIRE( initial.read_lookout( from, to, MAX_VIEW_DISTANCE * 2 ).visible );
    CHECK( initial.cached_lighting_submap_count() == 0 );
    CHECK( initial.cached_sunlight_summary_count() == 0 );
    const auto limited = initial.read_lookout( from, to, MAX_VIEW_DISTANCE );
    CHECK( limited.known );
    CHECK_FALSE( limited.visible );
    CHECK( limited.reason == "lookout_potential_range" );

    SECTION( "an unusable standing cell is not a lookout" ) {
        here.ter_set( standing, ter_str_id( "t_wall" ) );
    }
    SECTION( "a floorless standing cell is not a lookout" ) {
        here.ter_set( standing, ter_str_id( "t_open_air" ) );
    }
    SECTION( "a genuine current hazardous field is not usable footing" ) {
        REQUIRE( here.add_field( standing, field_type_id( "fd_fire" ), 3 ) );
    }
    SECTION( "a physical wall blocks the target area in darkness too" ) {
        for( int y = 0; y < MAPSIZE_Y; ++y ) {
            here.ter_set( tripoint_bub_ms( 48, y, 0 ), ter_str_id( "t_wall" ) );
        }
    }
    here.build_map_cache( 0 );
    scout_observation::site_reader changed;
    const auto refused = changed.read_lookout( from, to, MAX_VIEW_DISTANCE * 2 );
    CHECK( refused.known );
    CHECK_FALSE( refused.visible );
    CHECK( changed.cached_lighting_submap_count() == 0 );
}

TEST_CASE( "a geometric lookout does not grant actual detail or access in darkness",
           "[binocular_056][prospective_lookout_evidence]" )
{
    const auto old_origin = get_map().get_abs_sub();
    const auto old_position = get_avatar().pos_abs();
    const auto old_turn = calendar::turn;
    clear_avatar();
    clear_npcs();
    const tripoint_abs_sm input_origin( 900, 900, 0 );
    install_uniform_observation_inputs( input_origin );
    map &here = get_map();
    here.load( input_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 90, 0 ), false );
    on_out_of_scope restore( [&]() {
        clear_npcs();
        g->clear_zombies();
        here.load( old_origin, false, false );
        get_avatar().setpos( old_position, false );
        set_time( old_turn );
    } );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time_to_day();
    npc &observer = spawn_npc( point_bub_ms( 12, 66 ), "test_talker" );
    npc &target = spawn_npc( point_bub_ms( 84, 66 ), "test_talker" );
    observer.recalc_sight_limits();
    monster *migo = g->place_critter_at( mtype_id( "mon_mi_go" ), tripoint_bub_ms( 12, 67, 0 ) );
    REQUIRE( migo );
    const auto origin = observer.pos_abs();
    const auto point = target.pos_abs();
    const auto migo_origin = migo->pos_abs();
    const std::vector<tripoint_abs_omt> assigned_area{ target.pos_abs_omt() };
    REQUIRE( scout_observation::associated_with_site( target.pos_abs_omt(), assigned_area ) );
    CHECK_FALSE( scout_observation::associated_with_site( target.pos_abs_omt() + tripoint( 1, 0, 0 ),
                 assigned_area ) );
    REQUIRE( here.add_field( target.pos_bub( here ), field_type_id( "fd_biolum" ), 3 ) );
    here.build_map_cache( 0 );
    {
        scout_observation::site_reader reader;
        REQUIRE( reader.read_lookout( origin, point, MAX_VIEW_DISTANCE * 2 ).visible );
        const auto detail = reader.read_character( observer, target, assigned_area );
        const auto access = reader.read_access( *migo, point );
        REQUIRE( detail.known );
        REQUIRE( detail.visible );
        REQUIRE( access.visibility.known );
        REQUIRE( access.visibility.visible );
        REQUIRE_FALSE( access.facts.empty() );
    }
    // Native darkness is a final local light override, not invented unknown
    // geometry. It must still prevent recognition at this otherwise useful site.
    here.remove_field( target.pos_bub( here ), field_type_id( "fd_biolum" ) );
    REQUIRE( here.add_field( target.pos_bub( here ), field_type_id( "fd_darkness" ), 1 ) );
    set_time( calendar::turn_zero + 1_days );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    REQUIRE( here.ambient_light_at( target.pos_bub( here ) ) == 0.0f );
    {
        scout_observation::site_reader reader;
        REQUIRE( reader.read_lookout( origin, point, MAX_VIEW_DISTANCE * 2 ).visible );
        const auto detail = reader.read_character( observer, target, assigned_area );
        const auto access = reader.read_access( *migo, point );
        CHECK( detail.known );
        CHECK_FALSE( detail.visible );
        CHECK( access.visibility.known );
        CHECK_FALSE( access.visibility.visible );
        CHECK( access.facts.empty() );
    }
    here.remove_field( target.pos_bub( here ), field_type_id( "fd_darkness" ) );
    set_time_to_day();
    for( int y = 0; y < MAPSIZE_Y; ++y ) {
        here.ter_set( tripoint_bub_ms( 48, y, 0 ), ter_str_id( "t_wall" ) );
    }
    here.build_map_cache( 0 );
    {
        scout_observation::site_reader reader;
        CHECK_FALSE( reader.read_lookout( origin, point, MAX_VIEW_DISTANCE * 2 ).visible );
        CHECK_FALSE( reader.read_character( observer, target, assigned_area ).visible );
        CHECK_FALSE( reader.read_access( *migo, point ).visibility.visible );
    }
    CHECK( observer.pos_abs() == origin );
    CHECK( target.pos_abs() == point );
    CHECK( migo->pos_abs() == migo_origin );
}

TEST_CASE( "sufficient native target light settles optics without an unrelated caster halo",
           "[binocular_056][recognition_light_floor]" )
{
    const auto old_origin = get_map().get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    const auto old_turn = calendar::turn;
    clear_avatar();
    clear_npcs();
    map &here = get_map();
    const tripoint_abs_sm input_origin( 1100, 1100, 0 );
    here.load( input_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 90, 0 ), false );
    clear_map_with_vision( -1, 4, false );
    clear_vehicles( &here );
    const auto absent = input_origin + tripoint( 12, 0, 0 );
    const auto absent_omt = project_to<coords::omt>( absent );
    const auto old_terrain = overmap_buffer.ter( absent_omt );
    overmap_buffer.ter_set( absent_omt, oter_str_id( "forest" ).id() );
    on_out_of_scope restore( [&]() {
        clear_npcs();
        g->clear_zombies();
        overmap_buffer.ter_set( absent_omt, old_terrain );
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            here.clear_vehicle_list( z );
        }
        here.load( old_origin, false, false );
        get_avatar().setpos( old_player, false );
        set_time( old_turn );
    } );
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( absent ) );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time_to_day();
    npc &observer = spawn_npc( point_bub_ms( 12, 66 ), "test_talker" );
    npc &target = spawn_npc( point_bub_ms( 84, 66 ), "test_talker" );
    observer.recalc_sight_limits();
    monster *migo = g->place_critter_at( mtype_id( "mon_mi_go" ), tripoint_bub_ms( 12, 67, 0 ) );
    REQUIRE( migo );
    const auto origin = observer.pos_abs();
    const auto point = target.pos_abs();
    const auto migo_origin = migo->pos_abs();
    const auto bytes = []( const auto &body ) {
        std::ostringstream out;
        JsonOut json( out );
        body.serialize( json );
        return out.str();
    };
    const std::vector<tripoint_abs_omt> footprint{ target.pos_abs_omt() };
    const scout_observation::actor_capability capability{
        std::min( MAX_VIEW_DISTANCE * 2, 2 * std::max( migo->sight_range( 0 ),
                  migo->sight_range( default_daylight_level() ) ) ), 2.0f
    };
    const auto read_visible = [&]() {
        const auto observer_before = bytes( observer );
        const auto target_before = bytes( target );
        const auto migo_before = bytes( *migo );
        scout_observation::site_reader reader;
        const auto detail = reader.read_character( observer, target, footprint );
        const auto actor = reader.read_actor( *migo, target, capability );
        const auto access = reader.read_access( *migo, point );
        CAPTURE( detail.reason, actor.reason, access.visibility.reason );
        CHECK( detail.known );
        CHECK( detail.visible );
        CHECK( actor.known );
        CHECK( actor.visible );
        CHECK( access.visibility.known );
        CHECK( access.visibility.visible );
        CHECK_FALSE( access.facts.empty() );
        // A sufficient decision never advertises an exact scalar it did not read.
        CHECK_FALSE( reader.cached_lighting_at( point ).has_value() );
        CHECK( bytes( observer ) == observer_before );
        CHECK( bytes( target ) == target_before );
        CHECK( bytes( *migo ) == migo_before );
    };
    here.build_map_cache( 0 );
    REQUIRE( here.ambient_light_at( target.pos_bub( here ) ) >= default_daylight_level() );
    const float native_observer_light = g->natural_light_level( observer.posz() );
    const float native_optics = 2.0f / std::max( 1.0f, get_weather().weather_id->sight_penalty );
    const auto original_traits = observer.get_mutations();
    // The generated native doctor may be nearsighted. Binoculars preserve that
    // real limit: retain its actual refusal before supplying corrective gear.
    if( ( observer.has_flag( json_character_flag( "MYOPIC" ) ) ||
          observer.has_flag( json_character_flag( "MYOPIC_IN_LIGHT" ) ) ) &&
        !observer.worn_with_flag( flag_id( "FIX_NEARSIGHT" ) ) ) {
        const auto before = bytes( observer );
        const int maximum = static_cast<int>( std::floor( native_optics * std::max(
                            observer.sight_range( default_daylight_level(), native_observer_light ),
                            observer.sight_range( 0, native_observer_light ) ) ) );
        REQUIRE( maximum < rl_dist( observer.pos_abs(), target.pos_abs() ) );
        CHECK_FALSE( observer.sees_site_with_optics( here, target, native_optics, footprint,
                     native_observer_light ) );
        scout_observation::site_reader uncorrected;
        const auto refusal = uncorrected.read_character( observer, target, footprint );
        CHECK( refusal.known );
        CHECK_FALSE( refusal.visible );
        CHECK( bytes( observer ) == before );
    }
    // Native rigid eyeglasses cannot share the eye layer with the doctor's
    // generated sunglasses. Retain only that displaced item in this scoped
    // fixture owner rather than deleting it or clearing unrelated clothing.
    auto displaced_sunglasses = observer.remove_worn_items_with( []( item &it ) {
        return it.typeId() == itype_id( "sunglasses" );
    } );
    for( const item &it : displaced_sunglasses ) {
        CHECK( it.typeId() == itype_id( "sunglasses" ) );
        CHECK( it.uid().get_value() != 0 );
    }
    if( !observer.worn_with_flag( flag_id( "FIX_NEARSIGHT" ) ) ) {
        const item correction( itype_id( "glasses_eye" ) );
        const auto wearable = observer.can_wear( correction );
        CAPTURE( wearable.str() );
        REQUIRE( wearable.success() );
        REQUIRE( observer.wear_item( correction, false ) );
    }
    observer.recalc_sight_limits();
    REQUIRE( observer.worn_with_flag( flag_id( "FIX_NEARSIGHT" ) ) );
    CHECK( observer.get_mutations() == original_traits );
    REQUIRE( observer.sees_site_with_optics( here, target, native_optics, footprint,
                                          native_observer_light ) );
    read_visible();
    // Retain only this original native window's physical input, not the larger
    // target-centred light plane. Current native serialization otherwise omits
    // uniform submaps; authentic expanded payloads make the cold control real.
    for( int z = -1; z <= OVERMAP_HEIGHT; ++z ) {
        for( int x = 0; x < MAPSIZE; ++x ) {
            for( int y = 0; y < MAPSIZE; ++y ) {
                submap *sm = MAPBUFFER.lookup_submap_cached( input_origin + tripoint( x, y, z ) );
                REQUIRE( sm );
                sm->ensure_nonuniform();
            }
        }
    }
    here.save();
    const auto far_origin = input_origin + ::point( 20, 20 );
    here.load( far_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    read_visible();
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        here.clear_vehicle_list( z );
    }
    MAPBUFFER.save( true );
    here.load( far_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    read_visible();

    here.load( input_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 90, 0 ), false );
    set_time( calendar::turn_zero + 1_days );
    REQUIRE( here.add_field( here.get_bub( point ), field_type_id( "fd_fire" ), 3 ) );
    here.build_map_cache( 0 );
    // The native source at the actual target is itself sufficient at night;
    // a missing unrelated incoming-light cell cannot erase this real emitter.
    REQUIRE( here.get_field_intensity( here.get_bub( point ), field_type_id( "fd_fire" ) ) == 3 );
    read_visible();
    REQUIRE( here.add_field( here.get_bub( point ), field_type_id( "fd_darkness" ), 1 ) );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    REQUIRE( here.ambient_light_at( here.get_bub( point ) ) == 0.0f );
    {
        scout_observation::site_reader reader;
        const auto detail = reader.read_character( observer, target, footprint );
        const auto access = reader.read_access( *migo, point );
        CHECK( detail.known );
        CHECK_FALSE( detail.visible );
        CHECK( access.visibility.known );
        CHECK_FALSE( access.visibility.visible );
        CHECK( access.facts.empty() );
        REQUIRE( reader.cached_lighting_at( point ).has_value() );
        CHECK( *reader.cached_lighting_at( point ) == 0.0f );
    }
    here.remove_field( here.get_bub( point ), field_type_id( "fd_darkness" ) );
    target.add_effect( efftype_id( "invisibility" ), 1_hours );
    REQUIRE( target.is_invisible() );
    {
        scout_observation::site_reader reader;
        const auto detail = reader.read_character( observer, target, footprint );
        CHECK( detail.known );
        CHECK_FALSE( detail.visible );
    }
    CHECK( observer.pos_abs() == origin );
    CHECK( target.pos_abs() == point );
    CHECK( migo->pos_abs() == migo_origin );
}

TEST_CASE( "complete artificial inputs can settle optics while necessary solar inputs stay unknown",
           "[binocular_056][observation_sufficient_complete]" )
{
    clear_avatar();
    clear_npcs();
    const auto old_turn = calendar::turn;
    on_out_of_scope restore( [&]() { set_time( old_turn ); } );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time_to_day();
    observation_light_fixture view;
    view.uniform_input_window( get_map().get_abs_sub() + point( 20, 20 ), true );
    const tripoint_bub_ms endpoint( 84, 66, 0 );
    REQUIRE( view.add_field( endpoint, field_type_id( "fd_biolum" ), 3 ) );
    npc observer;
    observer.normalize();
    observer.set_fake( true );
    observer.recalc_sight_limits();
    observer.setpos( view.get_abs( tripoint_bub_ms( 60, 66, 0 ) ), false );
    npc target;
    target.normalize();
    target.set_fake( true );
    target.setpos( view.get_abs( endpoint ), false );
    const std::vector<tripoint_abs_omt> footprint{ target.pos_abs_omt() };
    view.build_map_cache( 0 );
    view.observe_light( 0 );
    const float native = view.ambient_light_at( endpoint );
    REQUIRE( native > LIGHT_AMBIENT_LOW );
    REQUIRE( native < g->natural_light_level( 0 ) );
    REQUIRE( observer.sees_site_with_optics( view, target, 2.0f, footprint,
                                           g->natural_light_level( 0 ) ) );
    int called = 0;
    bool visible = false;
    const std::function<bool( float )> sufficient = [&]( float lower ) {
        ++called;
        CHECK( lower <= native );
        // Match the real adapter: apply the supplied bound to an isolated
        // value cache before executing Creature's final visibility gates.
        // The partial solar cast itself is deliberately not an exact cache.
        auto &cache = const_cast<level_cache &>( view.get_cache_ref( endpoint.z() ) );
        const auto partial = cache.lm[endpoint.x()][endpoint.y()];
        on_out_of_scope restore_partial( [&]() {
            cache.lm[endpoint.x()][endpoint.y()] = partial;
        } );
        cache.lm[endpoint.x()][endpoint.y()] = four_quadrants( lower );
        visible = observer.sees_site_with_optics( view, target, 2.0f, footprint,
                                                g->natural_light_level( 0 ) );
        return visible;
    };
    const auto scalar = view.observe_decision( endpoint, []( const tripoint_bub_ms &p ) {
        return p.z() <= 1;
    }, sufficient );
    CHECK_FALSE( scalar );
    CHECK( called > 0 );
    CHECK( visible );
}

TEST_CASE( "aborted native solar projection preserves its authenticated artificial source",
           "[binocular_056][observation_source_solar_abort]" )
{
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 5, false );
    map &here = get_map();
    const auto old_turn = calendar::turn;
    on_out_of_scope restore( [&]() {
        g->clear_zombies();
        set_time( old_turn );
    } );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time_to_day();
    observation_light_fixture view;
    view.uniform_input_window( here.get_abs_sub(), true );
    const tripoint_bub_ms endpoint( 84, 66, 0 );
    REQUIRE( view.add_field( endpoint, field_type_id( "fd_biolum" ), 3 ) );
    monster *target = g->place_critter_at( mtype_id( "mon_zombie_shady" ), endpoint );
    monster *observer = g->place_critter_at( mtype_id( "mon_mi_go" ), endpoint - point( 6, 0 ) );
    REQUIRE( target );
    REQUIRE( observer );
    here.build_map_cache( 0 );
    const auto main_lm = here.get_cache_ref( 0 ).lm;
    const auto main_sm = here.get_cache_ref( 0 ).sm;
    view.build_map_cache( 0 );
    view.observe_light( 0 );
    const float native = view.ambient_light_at( endpoint );
    const float native_source = view.get_cache_ref( 0 ).sm[endpoint.x()][endpoint.y()];
    CAPTURE( native, native_source );
    REQUIRE( native_source >= LIGHT_SOURCE_BRIGHT );
    REQUIRE( native < g->natural_light_level( 0 ) );
    const std::vector<tripoint_abs_omt> footprint{ target->pos_abs_omt() };
    REQUIRE( observer->sees_site_with_range( view, *target, 60, footprint ) );
    const auto bytes = []( const auto &body ) {
        std::ostringstream out;
        JsonOut json( out );
        body.serialize( json );
        return out.str();
    };
    const auto target_before = bytes( *target );
    const auto observer_before = bytes( *observer );
    const auto now = calendar::turn;
    // Every artificial source/peer cell is available, but the genuine roof and
    // solar dependencies above z1 cannot be admitted by this bounded reader.
    const auto available = []( const tripoint_bub_ms &p ) { return p.z() <= 1; };
    std::optional<float> source;
    const auto exact = view.observe_point( endpoint, available, &source );
    CHECK_FALSE( exact );
    REQUIRE( source );
    CHECK( *source == Approx( native_source ) );
    bool called = false;
    bool visible = false;
    const std::function<bool( float )> sufficient = [&]( float lower ) {
        called = true;
        REQUIRE( source );
        CHECK( *source == Approx( native_source ) );
        CHECK( lower <= native );
        // Same isolated native predicate inputs as the actual actor adapter.
        // The source is the authenticated output, not an assumed bright flag.
        auto &cache = const_cast<level_cache &>( view.get_cache_ref( 0 ) );
        const auto partial_lm = cache.lm[endpoint.x()][endpoint.y()];
        const float partial_sm = cache.sm[endpoint.x()][endpoint.y()];
        on_out_of_scope restore_partial( [&]() {
            cache.lm[endpoint.x()][endpoint.y()] = partial_lm;
            cache.sm[endpoint.x()][endpoint.y()] = partial_sm;
        } );
        cache.lm[endpoint.x()][endpoint.y()] = four_quadrants( lower );
        cache.sm[endpoint.x()][endpoint.y()] = *source;
        visible = observer->sees_site_with_range( view, *target, 60, footprint );
        return visible;
    };
    const auto partial = view.observe_decision( endpoint, available, sufficient, &source );
    CHECK_FALSE( partial );
    CHECK( called );
    CHECK( visible );
    REQUIRE( source );
    CHECK( *source == Approx( native_source ) );
    view.delete_field( endpoint, field_type_id( "fd_biolum" ) );
    const auto absent = view.observe_point( endpoint, available, &source );
    CHECK_FALSE( absent );
    REQUIRE( source );
    CHECK( *source == 0.0f );
    source = native_source;
    const auto unknown = view.observe_point( endpoint, [&]( const tripoint_bub_ms &p ) {
        return available( p ) && p != endpoint;
    }, &source );
    CHECK_FALSE( unknown );
    CHECK_FALSE( source );
    CHECK( bytes( *target ) == target_before );
    CHECK( bytes( *observer ) == observer_before );
    CHECK( calendar::turn == now );
    bool main_unchanged = true;
    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
            main_unchanged &= here.get_cache_ref( 0 ).lm[x][y].values == main_lm[x][y].values &&
                              here.get_cache_ref( 0 ).sm[x][y] == main_sm[x][y];
        }
    }
    CHECK( main_unchanged );
}

TEST_CASE( "lazy upper terrain loads preserve an already admitted native vehicle roof",
           "[binocular_056][observation_lazy_vehicle_roof]" )
{
    clear_avatar();
    clear_npcs();
    map &here = get_map();
    const auto old_origin = here.get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    const auto old_turn = calendar::turn;
    const tripoint_abs_sm input_origin( 1350, 1350, 0 );
    on_out_of_scope restore( [&]() {
        here.load( input_origin, false, false );
        clear_vehicles( &here );
        here.load( old_origin, false, false );
        get_avatar().setpos( old_player, false );
        set_time( old_turn );
        MAPBUFFER.clear_outside_reality_bubble();
    } );
    install_uniform_observation_inputs( input_origin );
    here.load( input_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 90, 0 ), false );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time_to_day();
    const tripoint_bub_ms target( 84, 66, 0 );
    const auto fixed_turn = calendar::turn;
    vehicle *roof = here.add_vehicle( vproto_id( "none" ), target, 0_degrees, 0,
                                     veh_spawn_status::UNDAMAGED );
    REQUIRE( roof );
    for( int x = -1; x <= 1; ++x ) {
        for( int y = -1; y <= 1; ++y ) {
            REQUIRE( roof->install_part( here, point_rel_ms( x, y ), vpart_id( "frame" ) ) >= 0 );
            REQUIRE( roof->install_part( here, point_rel_ms( x, y ), vpart_id( "roof" ) ) >= 0 );
        }
    }
    roof->refresh();
    here.add_vehicle_to_cache( roof );
    here.invalidate_map_cache( 0 );
    here.invalidate_map_cache( 1 );
    here.build_map_cache( 0 );
    monster *observer = g->place_critter_at( mtype_id( "mon_mi_go" ),
                                           tripoint_bub_ms( 12, 66, 0 ) );
    REQUIRE( observer );
    on_out_of_scope remove_observer( [&]() { g->remove_zombie( *observer ); } );
    const auto endpoint = here.get_abs( target );
    const std::vector<tripoint_abs_omt> footprint{ project_to<coords::omt>( endpoint ) };
    REQUIRE_FALSE( here.is_outside( target ) );
    REQUIRE( here.dont_draw_lower_floor( target + tripoint::above ) );
    const float native = here.ambient_light_at( target );
    REQUIRE( native < g->natural_light_level( 0 ) );
    const int range = 2 * observer->sight_range( native );
    const bool expected = here.sees_site_with_optics( observer->pos_bub( here ), target, range,
                          2.0f, footprint );
    CAPTURE( native, range, expected );
    std::ostringstream bytes;
    JsonOut out( bytes );
    roof->serialize( out );
    const auto before = bytes.str();
    scout_observation::site_reader reader;
    const auto actual = reader.read_access( *observer, endpoint ).visibility;
    const auto projected = reader.cached_lighting_at( endpoint );
    CAPTURE( native, projected, actual.known, actual.visible, actual.reason,
             reader.cached_lighting_submap_count() );
    REQUIRE( projected );
    CHECK( *projected == Approx( native ) );
    CHECK( actual.known );
    CHECK( actual.visible == expected );
    CHECK( reader.cached_lighting_submap_count() > 4 );
    CHECK( here.dont_draw_lower_floor( target + tripoint::above ) );
    std::ostringstream after;
    JsonOut after_out( after );
    roof->serialize( after_out );
    CHECK( after.str() == before );
    CHECK( calendar::turn == fixed_turn );
}

TEST_CASE( "native source brightness remains distinct from ambient actor illumination",
           "[binocular_056][observation_actor_source_light]" )
{
    clear_avatar();
    clear_npcs();
    g->clear_zombies();
    map &here = get_map();
    const auto old_origin = here.get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    const auto old_turn = calendar::turn;
    on_out_of_scope restore( [&]() {
        g->clear_zombies();
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            here.clear_vehicle_list( z );
        }
        here.load( old_origin, false, false );
        get_avatar().setpos( old_player, false );
        set_time( old_turn );
        MAPBUFFER.clear_outside_reality_bubble();
    } );
    const tripoint_abs_sm input_origin( 1600, 1600, 0 );
    install_uniform_observation_inputs( input_origin, true );
    here.load( input_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 90, 0 ), false );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time( calendar::turn_zero );
    const tripoint_bub_ms tile( 84, 66, 0 );
    monster *target = g->place_critter_at( mtype_id( "mon_zombie_shady" ), tile );
    monster *migo = g->place_critter_at( mtype_id( "mon_mi_go" ), tile - point( 6, 0 ) );
    REQUIRE( target );
    REQUIRE( migo );
    npc observer;
    observer.normalize();
    observer.set_fake( true );
    observer.setpos( here.get_abs( tile - point( 2, 0 ) ), false );
    observer.recalc_sight_limits();
    const auto endpoint = target->pos_abs();
    const auto bytes = []( const auto &body ) {
        std::ostringstream out;
        JsonOut json( out );
        body.serialize( json );
        return out.str();
    };
    REQUIRE( here.add_field( tile, field_type_id( "fd_spotlight" ), 1 ) );
    REQUIRE( here.add_field( tile, field_type_id( "fd_shadow" ), 1 ) );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    const auto &cache = here.get_cache_ref( 0 );
    const float ambient = here.ambient_light_at( tile );
    const float source = cache.sm[tile.x()][tile.y()];
    CAPTURE( ambient, source );
    REQUIRE( ambient < LIGHT_AMBIENT_LIT );
    REQUIRE( source >= LIGHT_SOURCE_BRIGHT );
    REQUIRE( here.light_at( tile ) == lit_level::BRIGHT );
    const auto original_lm = cache.lm;
    const auto original_sm = cache.sm;
    const auto target_before = bytes( *target );
    const auto migo_before = bytes( *migo );
    const auto observer_before = bytes( observer );
    const auto turn = calendar::turn;
    const scout_observation::actor_capability capability{ 60, 2.0f };
    const std::vector<tripoint_abs_omt> footprint{ target->pos_abs_omt() };
    const bool human_expected = observer.sees_site_with_range( here, *target, 60, footprint );
    const bool migo_expected = migo->sees_site_with_range( here, *target, 60, footprint );
    REQUIRE( human_expected );
    REQUIRE( migo_expected );
    // Bright-source classification does not grant ordinary unmagnified range.
    const bool native_unmagnified = observer.sees_without_clairvoyance( here, *target );
    REQUIRE_FALSE( native_unmagnified );
    REQUIRE( observer.scouting_creature_requires_response( *target, "watch", here ) );
    const auto read = [&]( const char *context ) {
        // read_actor scales native sight, not the capability ceiling alone.
        // This non-adjacent target is inside the actual range in every context.
        REQUIRE( 2 * observer.sight_range( ambient ) >= rl_dist( observer.pos_abs(), endpoint ) );
        scout_observation::site_reader reader;
        const auto human = reader.read_actor( observer, *target, capability );
        const auto alien = reader.read_actor( *migo, *target, capability );
        scout_observation::site_reader predicate_reader;
        const auto human_predicate = predicate_reader.read_actor( observer, *target, capability, true );
        const auto alien_predicate = predicate_reader.read_actor( *migo, *target, capability, true );
        CAPTURE( context, human.reason, alien.reason, observer.sight_range( ambient ),
                 observer.sight_range( default_daylight_level() ) );
        CHECK( human.known );
        CHECK( human.visible == human_expected );
        CHECK( alien.known );
        CHECK( alien.visible == migo_expected );
        CHECK( human_predicate.known == human.known );
        CHECK( human_predicate.visible == human.visible );
        CHECK( alien_predicate.known == alien.known );
        CHECK( alien_predicate.visible == alien.visible );
        const auto threats = reader.perceived_threats( observer );
        CHECK( ( std::find( threats.begin(), threats.end(), endpoint ) != threats.end() ) ==
               native_unmagnified );
        CHECK( bytes( *target ) == target_before );
        CHECK( bytes( *migo ) == migo_before );
        CHECK( bytes( observer ) == observer_before );
        CHECK( calendar::turn == turn );
    };
    read( "near resident" ); // Near player, resident physical source.
    bool unchanged = true;
    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
            unchanged &= cache.lm[x][y].values == original_lm[x][y].values &&
                         cache.sm[x][y] == original_sm[x][y];
        }
    }
    CHECK( unchanged );
    here.save();
    here.load( input_origin + point( 30, 30 ), false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    read( "far resident" ); // Far player, same resident physical source.
    MAPBUFFER.save( true );
    MAPBUFFER.clear_outside_reality_bubble();
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( endpoint ) ) );
    read( "far saved" ); // Far player, native saved physical source.
    here.load( input_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 90, 0 ), false );
    read( "near reloaded" ); // Near player, reloaded physical source.
    here.delete_field( tile, field_type_id( "fd_spotlight" ) );
    here.set_lightmap_cache_dirty( 0 );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    REQUIRE( here.light_at( tile ) <= lit_level::LOW );
    REQUIRE_FALSE( observer.sees_site_with_range( here, *target, 60, footprint ) );
    scout_observation::site_reader dark;
    const auto hidden = dark.read_actor( observer, *target, capability );
    CHECK( hidden.known );
    CHECK_FALSE( hidden.visible );
    scout_observation::site_reader dark_predicate;
    CHECK_FALSE( dark_predicate.read_actor( observer, *target, capability, true ).visible );

    // Same native source/caster channel for a genuine charged item.
    item battery( itype_id( "medium_battery_cell" ) );
    battery.ammo_set( battery.ammo_default(), 56 );
    item lamp_item( itype_id( "flashlight_on" ) );
    lamp_item.put_in( battery, pocket_type::MAGAZINE_WELL );
    lamp_item.active = true;
    here.add_item_or_charges( tile, std::move( lamp_item ) );
    here.set_lightmap_cache_dirty( 0 );
    here.build_map_cache( 0 );
    REQUIRE( here.ambient_light_at( tile ) == Approx( ambient ) );
    REQUIRE( here.light_at( tile ) == lit_level::BRIGHT );
    REQUIRE_FALSE( here.i_at( tile ).empty() );
    const auto item_before = bytes( *here.i_at( tile ).begin() );
    read( "native item source" );
    CHECK( bytes( *here.i_at( tile ).begin() ) == item_before );
    here.i_clear( tile );

    // And an authenticated powered vehicle. A cold decoded unbound owner
    // must stay unknown until ordinary native map loading actually binds it.
    vehicle *lamp = here.add_vehicle( vproto_id( "none" ), tile, 0_degrees, 0,
                                     veh_spawn_status::UNDAMAGED );
    REQUIRE( lamp );
    REQUIRE( lamp->install_part( here, point_rel_ms::zero, vpart_id( "frame" ) ) >= 0 );
    REQUIRE( lamp->install_part( here, point_rel_ms::zero,
                               vpart_id( "small_storage_battery" ) ) >= 0 );
    const int part = lamp->install_part( here, point_rel_ms::zero, vpart_id( "floodlight" ) );
    REQUIRE( part >= 0 );
    lamp->refresh();
    here.add_vehicle_to_cache( lamp );
    lamp->charge_battery( here, 500 );
    lamp->part( part ).enabled = true;
    lamp->power_parts( here );
    const auto vehicle_before = bytes( *lamp );
    here.set_lightmap_cache_dirty( 0 );
    here.build_map_cache( 0 );
    REQUIRE( here.ambient_light_at( tile ) == Approx( ambient ) );
    REQUIRE( here.light_at( tile ) == lit_level::BRIGHT );
    read( "near native vehicle source" );
    CHECK( bytes( *lamp ) == vehicle_before );
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        here.clear_vehicle_list( z );
    }
    here.load( input_origin + point( 30, 30 ), false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    read( "far resident vehicle source" );
    CHECK( bytes( *lamp ) == vehicle_before );
    MAPBUFFER.save( true );
    MAPBUFFER.clear_outside_reality_bubble();
    scout_observation::site_reader unbound;
    const auto refused = unbound.read_actor( *migo, *target, capability );
    CHECK_FALSE( refused.known );
    CHECK_FALSE( refused.visible );
    scout_observation::site_reader unbound_predicate;
    const auto refused_predicate = unbound_predicate.read_actor( *migo, *target, capability, true );
    CHECK_FALSE( refused_predicate.known );
    CHECK_FALSE( refused_predicate.visible );
    CHECK( bytes( *target ) == target_before );
    CHECK( bytes( *migo ) == migo_before );
    CHECK( calendar::turn == turn );
    here.load( input_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 90, 0 ), false );
    read( "near natively rebound vehicle source" );
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        here.clear_vehicle_list( z );
    }
}

TEST_CASE( "loaded actor optics uses the same fixed native light window as remote optics",
           "[binocular_056][observation_exact_actor_window]" )
{
    clear_avatar();
    clear_npcs();
    map &here = get_map();
    const auto old_origin = here.get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    const auto old_turn = calendar::turn;
    // This fixture changes the main map's native window. Borrowed vehicle
    // pointers must end before their submap owners can be retired; native
    // loading then binds the new current window, including its vehicles.
    const auto load_window = [&]( const tripoint_abs_sm &origin ) {
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            here.clear_vehicle_list( z );
        }
        here.load( origin, true, false );
    };
    on_out_of_scope restore( [&]() {
        struct owned_pointer {
            vehicle *body;
            tripoint_abs_sm owner;
            int z;
        };
        std::vector<owned_pointer> before;
        const auto loaded_origin = here.get_abs_sub();
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            const auto &cached = here.get_cache_ref( z ).vehicle_list;
            for( int x = 0; x < MAPSIZE; ++x ) {
                for( int y = 0; y < MAPSIZE; ++y ) {
                    const tripoint_abs_sm owner( loaded_origin.xy() + point( x, y ), z );
                    const submap *sm = MAPBUFFER.lookup_submap_cached( owner );
                    if( sm ) {
                        for( const auto &body : sm->vehicles ) {
                            if( cached.count( body.get() ) ) {
                                before.push_back( { body.get(), owner, z } );
                            }
                        }
                    }
                }
            }
        }
        load_window( old_origin );
        get_avatar().setpos( old_player, false );
        set_time( old_turn );
        MAPBUFFER.clear_outside_reality_bubble();
        for( const auto &entry : before ) {
            // Compare addresses only: never dereference a possibly retired body.
            const bool cached = here.get_cache_ref( entry.z ).vehicle_list.count( entry.body );
            const bool owner_present = MAPBUFFER.lookup_submap_cached( entry.owner );
            CHECK_FALSE( ( cached && !owner_present ) );
        }
        CHECK( here.get_abs_sub() == old_origin );
        CHECK( get_avatar().pos_abs() == old_player );
        CHECK( calendar::turn == old_turn );
    } );
    const tripoint_abs_sm input_origin( 1400, 1400, 0 );
    install_uniform_observation_inputs( input_origin );
    load_window( input_origin );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 90, 0 ), false );
    scoped_weather_override weather( weather_type_id( "fog" ) );
    set_time_to_day();
    const tripoint_bub_ms target_tile( 84, 66, 0 );
    const auto endpoint = here.get_abs( target_tile );
    for( int z = 1; z < OVERMAP_HEIGHT; z += 2 ) {
        here.ter_set( tripoint_bub_ms( target_tile.xy(), z ), ter_str_id( "t_open_air_rooved" ) );
    }
    observation_light_fixture oracle;
    oracle.load( project_to<coords::sm>( endpoint ) - point_rel_sm( MAPSIZE / 2, MAPSIZE / 2 ),
                 false );
    const auto upper_blocker = here.get_abs( tripoint_bub_ms( 150, 4, OVERMAP_HEIGHT ) );
    REQUIRE_FALSE( here.inbounds( upper_blocker ) );
    REQUIRE( oracle.inbounds( upper_blocker ) );
    oracle.ter_set( oracle.get_bub( upper_blocker ), ter_str_id( "t_wall" ) );
    oracle.build_map_cache( 0 );
    oracle.observe_light( 0 );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    const float fixed_native = oracle.ambient_light_at( oracle.get_bub( endpoint ) );
    const float main_native = here.ambient_light_at( target_tile );
    CAPTURE( fixed_native, main_native, upper_blocker );
    REQUIRE( fixed_native < main_native );
    REQUIRE( here.is_outside( target_tile ) );
    npc observer;
    observer.normalize();
    observer.set_fake( true );
    observer.recalc_sight_limits();
    const float optics = 2.0f / std::max( 1.0f, get_weather().weather_id->sight_penalty );
    const int low_range = std::floor( optics * observer.sight_range( fixed_native,
                                     g->natural_light_level( 0 ) ) );
    const int high_range = std::floor( optics * observer.sight_range( main_native,
                                      g->natural_light_level( 0 ) ) );
    CAPTURE( low_range, high_range );
    REQUIRE( low_range < high_range );
    const int distance = low_range + 1;
    REQUIRE( distance <= MAX_VIEW_DISTANCE * 2 );
    observer.setpos( endpoint - point_rel_ms( distance, 0 ), false );
    npc target;
    target.normalize();
    target.set_fake( true );
    target.setpos( endpoint, false );
    const std::vector<tripoint_abs_omt> footprint{ target.pos_abs_omt() };
    const bool expected = observer.sees_site_with_optics( oracle, target, optics, footprint,
                          g->natural_light_level( 0 ) );
    const bool main_expected = observer.sees_site_with_optics( here, target, optics, footprint,
                               g->natural_light_level( 0 ) );
    REQUIRE_FALSE( expected );
    REQUIRE( main_expected );
    const auto lm = here.get_cache_ref( 0 ).lm;
    scout_observation::site_reader reader;
    const auto loaded = reader.read_character( observer, target, footprint );
    const auto projected = reader.cached_lighting_at( endpoint );
    REQUIRE( projected );
    CHECK( *projected == Approx( fixed_native ) );
    CHECK( loaded.known );
    CHECK( loaded.visible == expected );
    bool cache_unchanged = true;
    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
            cache_unchanged &= here.get_cache_ref( 0 ).lm[x][y].values == lm[x][y].values;
        }
    }
    CHECK( cache_unchanged );
    load_window( input_origin + point( 30, 30 ) );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    scout_observation::site_reader far;
    const auto remote = far.read_character( observer, target, footprint );
    CHECK( remote.known == loaded.known );
    CHECK( remote.visible == expected );
    CHECK( far.cached_lighting_at( endpoint ) == projected );
}

TEST_CASE( "authenticated vehicle roof preserves native ledge cover away from the player",
           "[binocular_056][observation_vehicle_geometry]" )
{
    const bool whole_actor_ray = GENERATE( false, true );
    const bool include_roof = GENERATE( false, true );
    CAPTURE( whole_actor_ray, include_roof );
    clear_avatar();
    clear_npcs();
    map &here = get_map();
    const auto old_origin = here.get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    const auto old_turn = calendar::turn;
    const tripoint_abs_sm input_origin( 1250, 1250, 0 );
    on_out_of_scope restore( [&]() {
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            here.clear_vehicle_list( z );
        }
        here.load( input_origin, true, false );
        clear_vehicles( &here );
        here.load( old_origin, false, false );
        get_avatar().setpos( old_player, false );
        set_time( old_turn );
    } );
    here.load( input_origin, false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 90, 0 ), false );
    clear_map_with_vision( -1, 3, false );
    clear_vehicles( &here );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time_to_day();
    npc observer;
    observer.normalize();
    observer.set_fake( true );
    observer.recalc_sight_limits();
    observer.setpos( here.get_abs( tripoint_bub_ms( 60, 66, 0 ) ), false );
    npc target;
    target.normalize();
    target.set_fake( true );
    target.setpos( here.get_abs( tripoint_bub_ms( whole_actor_ray ? 96 : 84, 66, 1 ) ), false );
    target.set_movement_mode( move_mode_id( "prone" ) );
    REQUIRE( target.is_prone() );
    here.ter_set( target.pos_bub( here ), ter_str_id( "t_flat_roof" ) );
    const int roof_x = whole_actor_ray ? 84 : 74;
    vehicle *roof = here.add_vehicle( vproto_id( "none" ), tripoint_bub_ms( roof_x, 66, 0 ),
                                     0_degrees, 0, veh_spawn_status::UNDAMAGED );
    REQUIRE( roof );
    REQUIRE( roof->install_part( here, point_rel_ms::zero, vpart_id( "frame" ) ) >= 0 );
    if( include_roof ) {
        REQUIRE( roof->install_part( here, point_rel_ms::zero, vpart_id( "roof" ) ) >= 0 );
    }
    roof->refresh();
    here.add_vehicle_to_cache( roof );
    here.invalidate_map_cache( 0 );
    here.invalidate_map_cache( 1 );
    here.build_map_cache( 1 );
    const auto from = observer.pos_abs();
    const auto to = target.pos_abs();
    const auto vehicle_position = roof->pos_abs();
    const auto owner = roof->sm_pos;
    std::vector<tripoint_abs_omt> footprint{ target.pos_abs_omt() };
    monster *migo = nullptr;
    on_out_of_scope remove_migo( [&]() { if( migo ) { g->remove_zombie( *migo ); } } );
    scout_observation::actor_capability capability;
    if( whole_actor_ray ) {
        migo = g->place_critter_at( mtype_id( "mon_mi_go" ), observer.pos_bub( here ) );
        REQUIRE( migo );
        capability = { std::min( MAX_VIEW_DISTANCE * 2,
                                2 * std::max( migo->sight_range( 0 ),
                                              migo->sight_range( default_daylight_level() ) ) ), 2.0f };
        footprint = { project_to<coords::omt>( from ) };
        for( const auto &point : scout_observation::site_sightline( from, to ) ) {
            const auto omt = project_to<coords::omt>( point );
            if( std::find( footprint.begin(), footprint.end(), omt ) == footprint.end() ) {
                footprint.push_back( omt );
            }
        }
    }
    const auto above_roof = here.get_abs( tripoint_bub_ms( roof_x, 66, 1 ) );
    REQUIRE_FALSE( here.veh_at( here.get_bub( above_roof ) ) );
    REQUIRE_FALSE( here.has_floor( here.get_bub( above_roof ) ) );
    REQUIRE( here.dont_draw_lower_floor( here.get_bub( above_roof ) ) == include_roof );
    REQUIRE( ( here.ledge_coverage( observer, target.pos_bub( here ), &footprint ) >= 33 ) == include_roof );
    REQUIRE( here.sees_site_with_optics( observer.pos_bub( here ), target.pos_bub( here ),
                                       MAX_VIEW_DISTANCE * 2, 2.0f, footprint ) );
    const bool native = whole_actor_ray ? migo->sees_site_with_range( here, target,
                        std::min( capability.maximum_range, 2 * migo->sight_range(
                                      here.ambient_light_at( target.pos_bub( here ) ) ) ), footprint ) :
                        observer.sees_site_with_optics( here, target, 2.0f, footprint,
                                g->natural_light_level( 0 ) );
    REQUIRE( native == !include_roof );
    const auto bytes = []( const auto &body ) {
        std::ostringstream text;
        JsonOut out( text );
        body.serialize( out );
        return text.str();
    };
    const auto vehicle_before = bytes( *roof );
    const auto observer_before = bytes( observer );
    const auto target_before = bytes( target );
    const auto migo_before = migo ? bytes( *migo ) : "";
    const auto fixed_turn = calendar::turn;
    const auto near = [&]() {
        scout_observation::site_reader reader;
        return whole_actor_ray ? reader.read_actor( *migo, target, capability ) :
               reader.read_character( observer, target, footprint );
    }();
    REQUIRE( near.known );
    CHECK( near.visible == native );
    if( whole_actor_ray ) {
        scout_observation::site_reader visible_only;
        CHECK( visible_only.read_actor( *migo, target, capability, true ).visible == near.visible );
    }
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        here.clear_vehicle_list( z );
    }
    here.load( input_origin + point( 40, 40 ), false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    REQUIRE( MAPBUFFER.lookup_submap_cached( owner ) );
    scout_observation::site_reader reader;
    const auto ray = reader.read_ray( from, to );
    CAPTURE( ray.known, ray.visible, ray.reason );
    REQUIRE( ray.known );
    REQUIRE( ray.visible );
    const auto far = whole_actor_ray ? reader.read_actor( *migo, target, capability ) :
                     reader.read_character( observer, target, footprint );
    CAPTURE( far.reason );
    CHECK( far.known );
    CHECK( far.visible == native );
    if( whole_actor_ray ) {
        scout_observation::site_reader visible_only;
        CHECK( visible_only.read_actor( *migo, target, capability, true ).visible == far.visible );
    }
    CHECK( roof->pos_abs() == vehicle_position );
    CHECK( roof->sm_pos == owner );
    CHECK( observer.pos_abs() == from );
    CHECK( target.pos_abs() == to );
    CHECK( bytes( *roof ) == vehicle_before );
    CHECK( bytes( observer ) == observer_before );
    CHECK( bytes( target ) == target_before );
    CHECK( ( migo ? bytes( *migo ) : "" ) == migo_before );
    CHECK( calendar::turn == fixed_turn );
    if( include_roof ) {
        // The real native index owner may forget a body before transfer. A
        // fresh decision cannot use that unconfirmed above-floor contribution.
        MAPBUFFER.forget_vehicle( *roof );
        scout_observation::site_reader unconfirmed;
        const auto refused = whole_actor_ray ? unconfirmed.read_actor( *migo, target, capability ) :
                             unconfirmed.read_character( observer, target, footprint );
        CHECK_FALSE( refused.known );
        CHECK_FALSE( refused.visible );
        if( whole_actor_ray ) {
            scout_observation::site_reader visible_only;
            CHECK_FALSE( visible_only.read_actor( *migo, target, capability, true ).visible );
        }
        CHECK( bytes( *roof ) == vehicle_before );
        CHECK( calendar::turn == fixed_turn );
    }
}

TEST_CASE( "native uniform flags determine the actual sunlight start",
           "[binocular_056][native_sunlight_uniform_start]" )
{
    const bool include_roof = GENERATE( false, true );
    CAPTURE( include_roof );
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 4, false );
    const auto old_time = calendar::turn;
    on_out_of_scope restore( [&]() { set_time( old_time ); } );
    scoped_weather_override weather( weather_type_id( "rain" ) );
    set_time_to_day();
    observation_light_fixture view;
    view.uniform_input_window( get_map().get_abs_sub() + point( -6, 0 ), include_roof );
    view.build_map_cache( 0 );
    view.observe_light( 0 );
    const tripoint_bub_ms target( 66, 66, 0 );
    const float native = view.ambient_light_at( target );
    int highest_physical_request = 0;
    const auto projected = view.observe_point( target, [&]( const tripoint_bub_ms &p ) {
        highest_physical_request = std::max( highest_physical_request, p.z() );
        return view.inbounds( p ) && ( include_roof || p.z() <= 1 );
    } );
    CAPTURE( native, highest_physical_request );
    REQUIRE( projected );
    CHECK( *projected == Approx( native ) );
    CHECK( native == Approx( include_roof ? LIGHT_AMBIENT_DIM * 0.8f : g->natural_light_level( 0 ) ) );
}

TEST_CASE( "native physical sunlight projection agrees with native caches without entity decoding",
           "[binocular_056][native_sunlight_summary]" )
{
    const std::string condition = GENERATE( "uniform", "floor", "opaque_furniture", "below_roof",
                                           "smoke", "compound_fields", "item_payload" );
    CAPTURE( condition );
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 2, false );
    scoped_weather_override weather( weather_type_id( "rain" ) );
    submap sample;
    sample.set_all_ter( ter_str_id( "t_open_air" ), true );
    const point_sm_ms cell( 3, 4 );
    if( condition == "smoke" || condition == "compound_fields" || condition == "item_payload" ) {
        // Mutable get_field/get_items on a uniform submap return native empty
        // sentinels. Establish a genuine owned tile before fixture mutation.
        sample.ensure_nonuniform();
    }
    if( condition == "floor" ) {
        sample.set_ter( cell, ter_str_id( "t_grass" ) );
    }
    if( condition == "opaque_furniture" ) {
        REQUIRE_FALSE( furn_str_id( "f_canvas_wall" )->transparent );
        sample.set_furn( cell, furn_str_id( "f_canvas_wall" ) );
    }
    if( condition == "smoke" || condition == "compound_fields" ) {
        REQUIRE( sample.get_field( cell ).add_field( field_type_id( "fd_smoke" ), 3 ) );
    }
    if( condition == "compound_fields" ) {
        REQUIRE( sample.get_field( cell ).add_field( field_type_id( "fd_fire" ), 1 ) );
    }
    if( condition == "item_payload" ) {
        sample.get_items( cell ).insert( item( itype_id( "rock" ) ) );
    }
    observation_light_fixture view;
    view.load( get_map().get_abs_sub() + point( -6, 0 ), false );
    wipe_map_terrain_with_vision( &view, false );
    const tripoint_rel_sm local( 5, 5, 1 );
    const tripoint_abs_sm position( view.get_abs_sub().xy() + point( 5, 5 ), 1 );
    view.install_input( local, &sample );
    if( condition == "below_roof" ) {
        view.furn_set( tripoint_bub_ms( 5 * SEEX + cell.x(), 5 * SEEY + cell.y(), 0 ),
                       furn_str_id( "f_large_skin_groundsheet" ) );
    }
    view.build_map_cache( 1 );
    const auto bytes = sunlight_entry_bytes( sample, position );
    const float penalty = get_weather().weather_id->sight_penalty;
    const auto resident = sample.sunlight_input( penalty );
    const auto count = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto time = calendar::turn;
    const auto uid_before = generate_next_item_uid();
    const auto projected = submap::read_sunlight_input(
                               json_loader::from_string( bytes ).get_object(), position, penalty );
    const auto uid_after = generate_next_item_uid();
    REQUIRE( projected );
    CHECK( uid_after == uid_before + 1 );
    CHECK_FALSE( projected->uniform ); // Native serialized load ensures nonuniform.
    CHECK( projected->vehicles == resident.vehicles );
    CHECK( projected->floors == resident.floors );
    CHECK( projected->sun_roofs == resident.sun_roofs );
    CHECK( projected->clear == resident.clear );
    CHECK( projected->nonclear == resident.nonclear );
    CHECK( projected->opaque == resident.opaque );
    CHECK( projected->air == resident.air );
    const submap *below = view.get_submap_at_grid( local - tripoint::above );
    REQUIRE( below );
    const auto floors = projected->floors | below->sunlight_input( penalty ).sun_roofs;
    const auto &native = view.get_cache_ref( 1 );
    for( int y = 0; y < SEEY; ++y ) {
        for( int x = 0; x < SEEX; ++x ) {
            const int index = y * SEEX + x;
            const int px = local.x() * SEEX + x;
            const int py = local.y() * SEEY + y;
            CHECK( floors[index] == native.floor_cache[px][py] );
            if( projected->clear[index] ) {
                CHECK( native.transparency_cache[px][py] >= LIGHT_TRANSPARENCY_OPEN_AIR );
            }
            if( projected->nonclear[index] ) {
                CHECK( native.transparency_cache[px][py] < LIGHT_TRANSPARENCY_OPEN_AIR );
            }
        }
    }
    CHECK( sunlight_entry_bytes( sample, position ) == bytes );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == count );
    CHECK( calendar::turn == time );
}

TEST_CASE( "native sunlight physical format refuses malformed or migration required inputs",
           "[binocular_056][native_sunlight_malformed]" )
{
    const std::string condition = GENERATE( "version_missing", "version_future", "version_obsolete",
                                           "coordinates_missing", "coordinates_wrong", "coordinates_overflow",
                                           "coordinates_type", "terrain_missing", "terrain_short", "terrain_long",
                                           "terrain_negative", "terrain_overflow", "terrain_type", "terrain_unknown",
                                           "terrain_migration", "furniture_coordinate", "furniture_migration",
                                           "field_type", "field_intensity", "field_tuple", "field_duplicate",
                                           "field_source", "vehicle_shape" );
    CAPTURE( condition );
    std::string version = "39";
    std::string coordinates = "[240,240,6]";
    std::string terrain = "[[\"t_open_air\",144]]";
    std::string furniture = "[]";
    std::string fields = "[]";
    std::string vehicles = "[]";
    if( condition == "version_future" ) { version = "2147483647"; }
    if( condition == "version_obsolete" ) { version = "21"; }
    if( condition == "coordinates_wrong" ) { coordinates = "[241,240,6]"; }
    if( condition == "coordinates_overflow" ) { coordinates = "[4294967536,240,6]"; }
    if( condition == "coordinates_type" ) { coordinates = "[\"240\",240,6]"; }
    if( condition == "terrain_short" ) { terrain = "[[\"t_open_air\",143]]"; }
    if( condition == "terrain_long" ) { terrain = "[[\"t_open_air\",145]]"; }
    if( condition == "terrain_negative" ) { terrain = "[[\"t_open_air\",-1]]"; }
    if( condition == "terrain_overflow" ) { terrain = "[[\"t_open_air\",4294967440]]"; }
    if( condition == "terrain_type" ) { terrain = "[42]"; }
    if( condition == "terrain_unknown" ) { terrain = "[[\"not_a_real_native_terrain\",144]]"; }
    if( condition == "terrain_migration" ) { terrain = "[[\"test_t_migration_old_id\",144]]"; }
    if( condition == "furniture_coordinate" ) { furniture = "[[12,0,\"f_canvas_wall\"]]"; }
    if( condition == "furniture_migration" ) { furniture = "[[0,0,\"test_f_migration_old_id\"]]"; }
    if( condition == "field_type" ) { fields = "[0,0,[\"test_fd_migration_old_id\",1,0,{}]]"; }
    if( condition == "field_intensity" ) { fields = "[0,0,[\"fd_smoke\",99,0,{}]]"; }
    if( condition == "field_tuple" ) { fields = "[0,0,[\"fd_smoke\",1]]"; }
    if( condition == "field_duplicate" ) { fields = "[0,0,[\"fd_smoke\",1,0,{},\"fd_smoke\",2,0,{}]]"; }
    if( condition == "field_source" ) { fields = "[0,0,[\"fd_smoke\",1,0,42]]"; }
    if( condition == "vehicle_shape" ) { vehicles = "{}"; }
    std::ostringstream bytes;
    bytes << '{';
    if( condition != "version_missing" ) { bytes << "\"version\":" << version << ','; }
    if( condition != "coordinates_missing" ) { bytes << "\"coordinates\":" << coordinates << ','; }
    if( condition != "terrain_missing" ) { bytes << "\"terrain\":" << terrain << ','; }
    bytes << "\"furniture\":" << furniture << ",\"fields\":" << fields
          << ",\"vehicles\":" << vehicles << '}';
    CHECK_FALSE( submap::read_sunlight_input( json_loader::from_string( bytes.str() ).get_object(),
                                           tripoint_abs_sm( 240, 240, 6 ), 1.0f ) );
}

TEST_CASE( "native sunlight legacy tuples preserve current physical field ordering",
           "[binocular_056][native_sunlight_legacy]" )
{
    const int version = GENERATE( 22, 38, savegame_version );
    CAPTURE( version );
    submap sample;
    sample.set_all_ter( ter_str_id( "t_open_air" ), true );
    const point_sm_ms cell( 3, 4 );
    sample.ensure_nonuniform();
    REQUIRE( sample.get_field( cell ).add_field( field_type_id( "fd_smoke" ), 3 ) );
    REQUIRE( sample.get_field( cell ).add_field( field_type_id( "fd_fire" ), 1 ) );
    const auto expected = sample.sunlight_input( 1.5f );
    const auto first = field_type_id( "fd_smoke" ) < field_type_id( "fd_fire" ) ?
                       "fd_fire" : "fd_smoke";
    const auto second = std::string( first ) == "fd_fire" ? "fd_smoke" : "fd_fire";
    const auto tuple = [&]( const std::string &id ) {
        return "\"" + id + "\"," + ( id == "fd_smoke" ? "3" : "1" ) + ",0" +
               ( version >= 39 ? ",{}" : "" );
    };
    std::ostringstream bytes;
    bytes << "{\"version\":" << version << ",\"coordinates\":[240,240,6],\"terrain\":[";
    for( int cell = 0; cell < SEEX * SEEY; ++cell ) {
        bytes << ( cell ? "," : "" ) << "\"t_open_air\"";
    }
    // Deliberately reverse native field ID order. No decoded field/item/body
    // owner is created; physical multiplication still follows native order.
    bytes << "],\"fields\":[3,4,[" << tuple( first ) << ',' << tuple( second ) << "]]}";
    const auto before_uid = generate_next_item_uid();
    const auto result = submap::read_sunlight_input( json_loader::from_string( bytes.str() ).get_object(),
                        tripoint_abs_sm( 240, 240, 6 ), 1.5f );
    REQUIRE( result );
    CHECK( generate_next_item_uid() == before_uid + 1 );
    CHECK( result->floors == expected.floors );
    CHECK( result->opaque == expected.opaque );
    CHECK( result->clear == expected.clear );
    CHECK( result->nonclear == expected.nonclear );
    CHECK( result->air == expected.air );
}

TEST_CASE( "checked sunlight transport preserves physical input and native ownership",
           "[binocular_056][native_sunlight_transport]" )
{
    const bool compressed = GENERATE( false, true );
    CAPTURE( compressed );
    REQUIRE( world_generator->active_world );
    WORLD &world = *world_generator->active_world;
    const bool old_compression = world.has_compression_enabled();
    REQUIRE( world.set_compression_enabled( compressed ) );
    on_out_of_scope restore_compression( [&]() {
        CHECK( world.set_compression_enabled( old_compression ) );
    } );
    const tripoint_abs_sm stored( 240, 240, 6 );
    const auto omitted = stored + point( 1, 0 );
    const auto malformed = stored + point( 2, 0 );
    const auto seg = project_to<coords::seg>( project_to<coords::omt>( stored ) );
    const auto directory = PATH_INFO::current_dimension_save_path() / "maps" /
                           string_format( "%d.%d.%d", seg.x(), seg.y(), seg.z() );
    const auto name_for = []( const tripoint_abs_sm &p ) {
        const auto omt = project_to<coords::omt>( p );
        return string_format( "%d.%d.%d.map", omt.x(), omt.y(), omt.z() );
    };
    REQUIRE_FALSE( std::filesystem::exists( directory.get_unrelative_path() ) );
    REQUIRE_FALSE( std::filesystem::exists( ( directory + ".zzip" ).get_unrelative_path() ) );
    std::filesystem::create_directories( directory.get_unrelative_path() );
    on_out_of_scope remove_fixture( [&]() {
        std::filesystem::remove_all( directory.get_unrelative_path() );
        std::filesystem::remove( ( directory + ".zzip" ).get_unrelative_path() );
    } );
    const auto dictionary = ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path();
    const auto write_quad = [&]( const tripoint_abs_sm &p, const std::string &bytes ) {
        if( compressed ) {
            auto archive = zzip::load( ( directory + ".zzip" ).get_unrelative_path(), dictionary );
            REQUIRE( archive );
            REQUIRE( archive->add_file( name_for( p ), bytes ) );
        } else {
            std::ofstream out( ( directory / name_for( p ) ).get_unrelative_path() );
            out << bytes;
            REQUIRE( out.good() );
        }
    };
    const auto read_bytes = []( const std::filesystem::path &path ) {
        std::ifstream input( path, std::ios::binary );
        return std::string( std::istreambuf_iterator<char>( input ), {} );
    };
    submap sample;
    sample.set_all_ter( ter_str_id( "t_open_air" ), true );
    sample.set_ter( point_sm_ms( 3, 4 ), ter_str_id( "t_grass" ) );
    sample.set_furn( point_sm_ms( 4, 4 ), furn_str_id( "f_canvas_wall" ) );
    REQUIRE( sample.get_field( point_sm_ms( 5, 4 ) ).add_field( field_type_id( "fd_smoke" ), 3 ) );
    sample.get_items( point_sm_ms( 6, 4 ) ).insert( item( itype_id( "rock" ) ) );
    const auto payload = sunlight_entry_bytes( sample, stored );
    write_quad( stored, "[" + payload + "]" );
    // One malformed sibling invalidates this quad, without affecting another
    // quad in the same checked archive or installing partial native owners.
    write_quad( malformed, "[" + sunlight_entry_bytes( sample, malformed ) +
                ",{\"version\":39,\"coordinates\":[243,240,6],\"terrain\":[]}]" );
    const auto path = ( compressed ? directory + ".zzip" :
                        directory / name_for( stored ) ).get_unrelative_path();
    const auto bad_path = ( compressed ? directory + ".zzip" :
                            directory / name_for( malformed ) ).get_unrelative_path();
    const auto before_bytes = read_bytes( path );
    const auto bad_bytes = read_bytes( bad_path );
    const auto before_mtime = std::filesystem::last_write_time( path );
    const auto bad_mtime = std::filesystem::last_write_time( bad_path );
    const auto before_time = calendar::turn;
    const auto global_count = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto uid_before = generate_next_item_uid();
    mapbuffer actual;
    {
        mapbuffer::existing_submap_reader reader( actual );
        for( const float penalty : { 1.0f, 0.5f, 2.0f, 1.0f } ) {
            const auto expected = sample.sunlight_input( penalty );
            const auto result = reader.read_sunlight_input( stored, penalty );
            REQUIRE( result );
            CHECK_FALSE( result->uniform );
            CHECK( result->floors == expected.floors );
            CHECK( result->sun_roofs == expected.sun_roofs );
            CHECK( result->opaque == expected.opaque );
            CHECK( result->clear == expected.clear );
            CHECK( result->nonclear == expected.nonclear );
            CHECK( result->air == expected.air );
            CHECK_FALSE( reader.read_sunlight_input( omitted, penalty ) );
            CHECK_FALSE( reader.read_sunlight_input( malformed, penalty ) );
        }
    }
    CHECK( generate_next_item_uid() == uid_before + 1 );
    CHECK( actual.begin() == actual.end() );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == global_count );
    CHECK( read_bytes( path ) == before_bytes );
    CHECK( read_bytes( bad_path ) == bad_bytes );
    CHECK( ( std::filesystem::last_write_time( path ) == before_mtime ) );
    CHECK( ( std::filesystem::last_write_time( bad_path ) == bad_mtime ) );
    CHECK( sunlight_entry_bytes( sample, stored ) == payload );
    CHECK( calendar::turn == before_time );
    auto resident = std::make_unique<submap>();
    resident->set_all_ter( ter_str_id( "t_wall" ) );
    const auto resident_expected = resident->sunlight_input( 1.0f );
    REQUIRE( actual.add_submap( stored, resident ) );
    {
        mapbuffer::existing_submap_reader reader( actual );
        const auto result = reader.read_sunlight_input( stored, 1.0f );
        REQUIRE( result );
        CHECK( result->opaque == resident_expected.opaque );
        CHECK( result->floors == resident_expected.floors );
        CHECK( result->uniform == resident_expected.uniform );
    }
    actual.clear();
    // Neither a corrupt archive nor a corrupt plain document may be recovered
    // and then mistaken for known absence. Preserve the exact bytes and mtime.
    {
        std::ofstream out( path, std::ios::binary | std::ios::trunc );
        out << "malformed-native-input";
    }
    const auto corrupt_bytes = read_bytes( path );
    const auto corrupt_mtime = std::filesystem::last_write_time( path );
    const std::string warning = capture_debugmsg_during( [&]() {
        mapbuffer::existing_submap_reader reader( actual );
        CHECK_FALSE( reader.read_sunlight_input( stored, 1.0f ) );
    } );
    if( compressed ) {
        CHECK_FALSE( warning.empty() );
    }
    CHECK( read_bytes( path ) == corrupt_bytes );
    CHECK( ( std::filesystem::last_write_time( path ) == corrupt_mtime ) );
    CHECK( actual.begin() == actual.end() );
    CHECK( calendar::turn == before_time );
}

TEST_CASE( "read-only native lighting uses actual sources and supplied map coordinates",
           "[binocular_056][native_observation_light]" )
{
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 2, false );
    const auto saved_turn = calendar::turn;
    on_out_of_scope restore( [&]() {
        clear_avatar();
        set_time( saved_turn );
    } );
    set_time( calendar::turn_zero );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    map &here = get_map();
    observation_light_fixture view;
    view.load( here.get_abs_sub() + point( -6, 0 ), false );
    get_avatar().setpos( here, tripoint_bub_ms( 12, 66, 0 ), false );
    const auto source = get_avatar().pos_bub( view );
    const auto wrong_source = get_avatar().pos_bub( here );
    REQUIRE( view.inbounds( source ) );
    REQUIRE( source != wrong_source );
    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
            view.ter_set( tripoint_bub_ms( x, y, 0 ), ter_str_id( "t_grass" ) );
            view.ter_set( tripoint_bub_ms( x, y, 1 ), ter_str_id( "t_open_air" ) );
            view.furn_set( tripoint_bub_ms( x, y, 0 ), furn_str_id( "f_null" ) );
            view.field_at( tripoint_bub_ms( x, y, 0 ) ).clear();
            view.i_clear( tripoint_bub_ms( x, y, 0 ) );
        }
    }
    view.invalidate_map_cache( 0 );
    view.invalidate_map_cache( 1 );
    view.build_map_cache( 0 );
    const float dark = view.ambient_light_at( source );
    item battery( itype_id( "medium_battery_cell" ) );
    battery.ammo_set( battery.ammo_default(), 56 );
    item lamp( itype_id( "flashlight_on" ) );
    lamp.put_in( battery, pocket_type::MAGAZINE_WELL );
    lamp.active = true;
    get_avatar().i_add( lamp );
    REQUIRE( get_avatar().active_light() > 4 );
    REQUIRE_FALSE( get_avatar().has_effect( efftype_id( "haslight" ) ) );
    const auto position = get_avatar().pos_abs();
    const auto turn = calendar::turn;
    view.observe_light( 0 );
    CHECK( view.ambient_light_at( source ) > dark );
    CHECK( view.ambient_light_at( source ) > view.ambient_light_at( wrong_source ) );
    CHECK_FALSE( get_avatar().has_effect( efftype_id( "haslight" ) ) );
    CHECK( get_avatar().pos_abs() == position );
    CHECK( ( calendar::turn == turn ) );
    const float lit = view.ambient_light_at( source );
    view.observe_light( 0 );
    CHECK( view.ambient_light_at( source ) == Approx( lit ) );
    CHECK_FALSE( get_avatar().has_effect( efftype_id( "haslight" ) ) );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 66, 0 ), false );
    REQUIRE_FALSE( view.inbounds( get_avatar().pos_abs() ) );
    const auto outside_position = get_avatar().pos_abs();
    view.observe_light( 0 );
    CHECK( view.ambient_light_at( source ) == Approx( dark ) );
    CHECK_FALSE( get_avatar().has_effect( efftype_id( "haslight" ) ) );
    CHECK( get_avatar().pos_abs() == outside_position );
    clear_avatar();
    view.add_field( source, field_type_id( "fd_biolum" ), 3 );
    view.observe_light( 0 );
    CHECK( view.ambient_light_at( source ) > dark );
    CHECK( view.get_field_intensity( source, field_type_id( "fd_biolum" ) ) == 3 );
    view.remove_field( source, field_type_id( "fd_biolum" ) );
    auto remote = make_shared_fast<npc>();
    remote->normalize();
    remote->setID( character_id( 956224 ), true );
    const auto remote_at = here.get_abs( tripoint_bub_ms( -12, 66, 0 ) );
    remote->spawn_at_omt( project_to<coords::omt>( remote_at ) );
    overmap_buffer.insert_npc( remote );
    remote->setpos( remote_at, false );
    remote->i_add( lamp );
    REQUIRE( remote->active_light() > 4 );
    REQUIRE_FALSE( here.inbounds( remote->pos_abs() ) );
    on_out_of_scope remove_remote( [&]() {
        if( overmap_buffer.find_npc( remote->getID() ) ) {
            overmap_buffer.remove_npc( remote->getID() );
        }
    } );
    REQUIRE_FALSE( remote->is_active() );
    const auto remote_position = remote->pos_abs();
    view.observe_light( 0 );
    CHECK( view.ambient_light_at( remote->pos_bub( view ) ) > dark );
    CHECK_FALSE( remote->has_effect( efftype_id( "haslight" ) ) );
    CHECK( remote->pos_abs() == remote_position );
    overmap_buffer.remove_npc( remote->getID() );
    set_time_to_day();
    view.observe_light( 0 );
    const float daylight = view.ambient_light_at( source );
    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
            view.ter_set( tripoint_bub_ms( x, y, 1 ), ter_str_id( "t_flat_roof" ) );
        }
    }
    view.invalidate_map_cache( 1 );
    view.build_map_cache( 0 );
    view.observe_light( 0 );
    CHECK( view.ambient_light_at( source ) < daylight );
}

TEST_CASE( "native point illumination matches the complete native calculator",
           "[binocular_056][native_point_light_projection]" )
{
    const std::string condition = GENERATE( "day", "roof", "aperture", "rain_inside",
                                           "fire", "item", "needed_missing", "irrelevant_high" );
    CAPTURE( condition );
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 4, false );
    const auto old_time = calendar::turn;
    on_out_of_scope restore( [&]() { set_time( old_time ); } );
    scoped_weather_override weather( weather_type_id( condition == "rain_inside" ? "rain" : "clear" ) );
    if( condition == "fire" || condition == "item" ) {
        set_time( calendar::turn_zero );
    } else {
        set_time_to_day();
    }
    observation_light_fixture view;
    view.load( get_map().get_abs_sub() + point( -6, 0 ), false );
    wipe_map_terrain_with_vision( &view, false );
    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
            const tripoint_bub_ms p( x, y, 0 );
            view.field_at( p ).clear();
            view.i_clear( p );
        }
    }
    const tripoint_bub_ms target( 66, 66, 0 );
    if( condition == "roof" || condition == "irrelevant_high" ) {
        for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
            for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
                view.ter_set( tripoint_bub_ms( x, y, 1 ), ter_str_id( "t_flat_roof" ) );
            }
        }
    }
    if( condition == "rain_inside" ) {
        view.ter_set( target, ter_str_id( "t_floor" ) );
    }
    if( condition == "aperture" ) {
        for( int x = 60; x <= 72; ++x ) {
            for( int y = 60; y <= 72; ++y ) {
                view.ter_set( tripoint_bub_ms( x, y, 0 ), ter_str_id( "t_floor" ) );
                view.ter_set( tripoint_bub_ms( x, y, 1 ), ter_str_id( "t_flat_roof" ) );
            }
        }
    }
    if( condition == "fire" ) {
        view.add_field( target + point( 0, 1 ), field_type_id( "fd_fire" ), 3 );
    }
    if( condition == "item" ) {
        item lamp( itype_id( "flashlight_on" ) );
        item battery( itype_id( "medium_battery_cell" ) );
        battery.ammo_set( battery.ammo_default(), 56 );
        lamp.put_in( battery, pocket_type::MAGAZINE_WELL );
        lamp.active = true;
        view.add_item_or_charges( target + point( 0, 1 ), std::move( lamp ) );
    }
    view.invalidate_map_cache( 0 );
    view.invalidate_map_cache( 1 );
    view.build_map_cache( 0 );
    view.observe_light( 0 );
    const float native = view.ambient_light_at( target );
    std::set<int> requested_levels;
    const auto projected = view.observe_point( target, [&]( const tripoint_bub_ms &p ) {
        requested_levels.insert( p.z() );
        if( condition == "needed_missing" && p == target + point( 0, 1 ) ) {
            return false;
        }
        if( condition == "irrelevant_high" && p.z() > 1 ) {
            return false;
        }
        return view.inbounds( p );
    } );
    CAPTURE( native );
    if( condition == "needed_missing" ) {
        CHECK_FALSE( projected );
    } else {
        REQUIRE( projected );
        CHECK( *projected == Approx( native ) );
    }
    if( condition == "irrelevant_high" ) {
        CHECK( requested_levels.count( 2 ) == 0 );
    }
}

TEST_CASE( "native sunlight fast path has a whole-window physical dependency",
           "[binocular_056][native_sunlight_fastpath_dependency]" )
{
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 4, false );
    const auto old_time = calendar::turn;
    on_out_of_scope restore( [&]() { set_time( old_time ); } );
    scoped_weather_override weather( weather_type_id( "rain" ) );
    set_time_to_day();
    observation_light_fixture view;
    view.load( get_map().get_abs_sub() + point( -6, 0 ), false );
    wipe_map_terrain_with_vision( &view, false );
    const tripoint_bub_ms target( 66, 66, 0 );
    const tripoint_bub_ms unrelated( 0, 0, 1 );
    REQUIRE( square_dist( unrelated.xy(), target.xy() ) > MAX_VIEW_DISTANCE + 1 );
    view.ter_set( target, ter_str_id( "t_floor" ) );
    view.invalidate_map_cache( 0 );
    view.build_map_cache( 0 );
    view.observe_light( 0 );
    const float clear_upper = view.ambient_light_at( target );
    const auto point_clear = view.observe_point( target, [&]( const tripoint_bub_ms &p ) {
        return view.inbounds( p );
    } );
    REQUIRE( point_clear );
    // A real obstruction beyond the target's native light-casting radius
    // disables the ordinary whole-plane fast fill at the target level.
    view.ter_set( unrelated, ter_str_id( "t_wall" ) );
    view.invalidate_map_cache( 1 );
    view.invalidate_map_cache( 0 );
    view.build_map_cache( 0 );
    view.observe_light( 0 );
    const float blocked_upper = view.ambient_light_at( target );
    const auto point_blocked = view.observe_point( target, [&]( const tripoint_bub_ms &p ) {
        return view.inbounds( p );
    } );
    REQUIRE( point_blocked );
    monster observer( mtype_id( "mon_mi_go" ) );
    std::cout << "native_sunlight_dependency origin=" << view.get_abs_sub()
              << " target=" << view.get_abs( target )
              << " blocker=" << view.get_abs( unrelated )
              << " clear_upper=" << clear_upper << " blocked_upper=" << blocked_upper
              << " point_clear=" << *point_clear << " point_blocked=" << *point_blocked
              << " range_clear=" << observer.sight_range( clear_upper )
              << " range_blocked=" << observer.sight_range( blocked_upper ) << std::endl;
    CHECK( clear_upper > blocked_upper );
    CHECK( *point_clear == Approx( clear_upper ) );
    CHECK( *point_blocked == Approx( blocked_upper ) );
}

TEST_CASE( "native solar retirement preserves the underground baseline",
           "[binocular_056][native_sunlight_underground]" )
{
    const bool open_path = GENERATE( false, true );
    CAPTURE( open_path );
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -2, 3, false );
    const auto old_time = calendar::turn;
    on_out_of_scope restore( [&]() { set_time( old_time ); } );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time_to_day();
    observation_light_fixture view;
    view.load( get_map().get_abs_sub() + point( -6, 0 ), false );
    wipe_map_terrain_with_vision( &view, false );
    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
            for( int z = -1; z <= 2; ++z ) {
                const tripoint_bub_ms p( x, y, z );
                view.ter_set( p, ter_str_id( z == 2 ? "t_flat_roof" : "t_open_air" ) );
                view.field_at( p ).clear();
                view.i_clear( p );
            }
        }
    }
    if( open_path ) {
        // Actual off-ray sky channel keeps the full native plane active. One
        // covered target column alone must not retire the global calculation.
        view.ter_set( tripoint_bub_ms( 0, 0, 2 ), ter_str_id( "t_open_air" ) );
    }
    view.build_map_cache( -1 );
    view.observe_light( -1 );
    const tripoint_bub_ms target( 66, 66, -1 );
    const float native = view.ambient_light_at( target );
    const auto projected = view.observe_point( target, [&]( const tripoint_bub_ms &p ) {
        return view.inbounds( p );
    } );
    CAPTURE( native );
    REQUIRE( projected );
    CHECK( *projected == Approx( native ) );
    CHECK( native == Approx( open_path ? LIGHT_AMBIENT_DIM * 0.8f : LIGHT_AMBIENT_LOW ) );
}

TEST_CASE( "rainy indoor access preserves the native cold serialized prefix",
           "[binocular_056][native_sunlight_cold_prefix]" )
{
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 4, false );
    map &here = get_map();
    {
        map physical_inputs;
        physical_inputs.load( here.get_abs_sub() + point( 2, 0 ), false, false );
        wipe_map_terrain_with_vision( &physical_inputs, false );
    }
    const auto old_origin = here.get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    const auto old_time = calendar::turn;
    const std::string old_savedir = PATH_INFO::savedir();
    const auto dictionary = ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path();
    const auto fixture_savedir = std::filesystem::path( old_savedir ) / "optical-cold-prefix";
    REQUIRE_FALSE( std::filesystem::exists( fixture_savedir ) );
    PATH_INFO::set_savedir( fixture_savedir.string() + "/" );
    on_out_of_scope restore_savedir( [&]() {
        PATH_INFO::set_savedir( old_savedir );
        std::filesystem::remove_all( fixture_savedir );
    } );
    std::filesystem::create_directories( PATH_INFO::world_base_save_path().get_unrelative_path() );
    if( std::filesystem::exists( dictionary ) ) {
        std::filesystem::copy_file( dictionary,
                                   ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path() );
    }
    on_out_of_scope restore_map( [&]() {
        here.load( old_origin, false, false );
        get_avatar().setpos( old_player, false );
        set_time( old_time );
    } );
    scoped_weather_override weather( weather_type_id( "rain" ) );
    set_time_to_day();
    const auto fixed_time = calendar::turn;
    const auto origin = here.get_abs( tripoint_bub_ms( 12, 71, 0 ) );
    const auto target = here.get_abs( tripoint_bub_ms( 84, 71, 0 ) );
    here.ter_set( here.get_bub( target ), ter_str_id( "t_floor" ) );
    monster *observer = g->place_critter_at( mtype_id( "mon_mi_go" ), here.get_bub( origin ) );
    REQUIRE( observer );
    on_out_of_scope remove_observer( [&]() { g->remove_zombie( *observer ); } );
    const auto read = [&]( bool warm = false ) {
        scout_observation::site_reader reader;
        const auto start = std::chrono::steady_clock::now();
        const auto cpu = std::clock();
        const auto actual = reader.read_access( *observer, target );
        std::cout << "native_prefix_inputs known=" << actual.visibility.known
                  << " wall_seconds=" << std::chrono::duration<double>(
                      std::chrono::steady_clock::now() - start ).count()
                  << " cpu_seconds=" << static_cast<double>( std::clock() - cpu ) / CLOCKS_PER_SEC
                  << " physical_submaps=" << reader.cached_lighting_submap_count()
                  << " summary_inputs=" << reader.cached_sunlight_summary_count()
                  << " path_groups=" << reader.cached_lighting_path_group_count() << std::endl;
        if( warm ) {
            const auto warm_start = std::chrono::steady_clock::now();
            const auto warm_cpu = std::clock();
            const auto repeated = reader.read_access( *observer, target );
            const double cpu_seconds = static_cast<double>( std::clock() - warm_cpu ) / CLOCKS_PER_SEC;
            const double wall_seconds = std::chrono::duration<double>(
                                            std::chrono::steady_clock::now() - warm_start ).count();
            CHECK( repeated.visibility.known == actual.visibility.known );
            CHECK( repeated.visibility.visible == actual.visibility.visible );
            CHECK( repeated.visibility.reason == actual.visibility.reason );
            CHECK( repeated.facts == actual.facts );
            std::cout << "native_prefix_warm wall_seconds=" << wall_seconds
                      << " cpu_seconds=" << cpu_seconds
                      << " physical_submaps=" << reader.cached_lighting_submap_count()
                      << " summary_inputs=" << reader.cached_sunlight_summary_count()
                      << " path_groups=" << reader.cached_lighting_path_group_count() << std::endl;
        }
        return actual;
    };
    const auto start = std::chrono::steady_clock::now();
    const auto near = read( true );
    const auto near_end = std::chrono::steady_clock::now();
    CAPTURE( near.visibility.reason );
    REQUIRE( near.visibility.known );
    REQUIRE( near.visibility.visible );
    here.load( old_origin + point( 20, 20 ), false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    const auto far_resident = read();
    CHECK( far_resident.visibility.known == near.visibility.known );
    CHECK( far_resident.visibility.visible == near.visibility.visible );
    CHECK( far_resident.facts == near.facts );
    MAPBUFFER.save();
    MAPBUFFER.clear_outside_reality_bubble();
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( target ) ) );
    const auto cold_start = std::chrono::steady_clock::now();
    const auto far_saved = read( true );
    const auto cold_end = std::chrono::steady_clock::now();
    CAPTURE( far_saved.visibility.reason );
    CHECK( far_saved.visibility.known == near.visibility.known );
    CHECK( far_saved.visibility.visible == near.visibility.visible );
    CHECK( far_saved.facts == near.facts );
    CHECK( calendar::turn == fixed_time );
    CHECK( observer->pos_abs() == origin );
    std::cout << "native_cold_prefix near_seconds=" <<
              std::chrono::duration<double>( near_end - start ).count()
              << " cold_seconds=" << std::chrono::duration<double>( cold_end - cold_start ).count()
              << " near=" << near.visibility.reason << " cold=" << far_saved.visibility.reason << std::endl;
}

TEST_CASE( "vehicle physical caches use their shifted owning map",
           "[binocular_056][observation_vehicle_context]" )
{
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 2, false );
    map &here = get_map();
    observation_light_fixture view;
    view.load( here.get_abs_sub() + point( -6, 0 ), false );
    wipe_map_terrain_with_vision( &view, false );
    vehicle *car = view.add_vehicle( vproto_id( "test_rv" ), tripoint_bub_ms( 66, 66, 0 ),
                                   0_degrees, 0, veh_spawn_status::UNDAMAGED );
    REQUIRE( car );
    on_out_of_scope cleanup( [&]() { clear_vehicles( &view ); } );
    const auto snapshot = [&]() {
        std::ostringstream bytes;
        JsonOut out( bytes );
        car->serialize( out );
        return bytes.str();
    };
    int opaque = 0;
    int inside = 0;
    int floors = 0;
    int roofs = 0;
    int openable_opaque = 0;
    for( const bool opened : { false, true } ) {
        for( const vpart_reference &vp : car->get_all_parts() ) {
            if( vp.has_feature( VPFLAG_OPENABLE ) ) {
                if( opened ) {
                    car->open( view, vp.part_index() );
                } else {
                    car->close( view, vp.part_index() );
                }
            }
        }
        view.invalidate_map_cache( 0 );
        view.invalidate_map_cache( 1 );
        view.build_map_cache( 0 );
        const auto before = snapshot();
        for( const vpart_reference &vp : car->get_all_parts_with_fakes() ) {
            const auto p = car->bub_part_pos( view, vp.part() );
            REQUIRE( view.inbounds( p ) );
            REQUIRE( p != car->bub_part_pos( here, vp.part() ) );
            const auto &cache = view.get_cache_ref( p.z() );
            const int door = car->part_with_feature( vp.part_index(), VPFLAG_OPENABLE, true );
            const bool is_opaque = vp.has_feature( VPFLAG_OPAQUE ) &&
                                   !vp.part().is_broken() && ( door < 0 || !car->part( door ).open );
            if( is_opaque ) {
                ++opaque;
                CHECK( cache.transparency_cache[p.x()][p.y()] == LIGHT_TRANSPARENCY_SOLID );
                CHECK_FALSE( cache.outside_cache[p.x()][p.y()] );
            }
            if( vp.is_inside() ) {
                ++inside;
                CHECK_FALSE( cache.outside_cache[p.x()][p.y()] );
            }
            if( vp.has_feature( VPFLAG_BOARDABLE ) && !vp.part().is_broken() ) {
                ++floors;
                CHECK( cache.floor_cache[p.x()][p.y()] );
            }
            if( vp.has_feature( VPFLAG_ROOF ) || vp.has_feature( VPFLAG_OPAQUE ) ) {
                ++roofs;
                CHECK( view.get_cache_ref( p.z() + 1 ).floor_cache[p.x()][p.y()] );
            }
            if( opened && vp.has_feature( VPFLAG_OPAQUE ) && door >= 0 && car->part( door ).open ) {
                ++openable_opaque;
                CHECK( cache.transparency_cache[p.x()][p.y()] > LIGHT_TRANSPARENCY_SOLID );
            }
        }
        view.do_vehicle_caching( 0 );
        CHECK( snapshot() == before );
    }
    CHECK( opaque > 0 );
    CHECK( inside > 0 );
    CHECK( floors > 0 );
    CHECK( roofs > 0 );
    CHECK( openable_opaque > 0 );
}

TEST_CASE( "powered vehicle light preserves physical access across player contexts",
           "[binocular_056][observation_vehicle_light]" )
{
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 4, false );
    map &here = get_map();
    map physical_inputs;
    physical_inputs.load( here.get_abs_sub() + point( 2, 0 ), false, false );
    wipe_map_terrain_with_vision( &physical_inputs, false );
    const auto old_origin = here.get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    const auto old_time = calendar::turn;
    on_out_of_scope restore( [&]() {
        here.load( old_origin, false, false );
        get_avatar().setpos( old_player, false );
        clear_vehicles( &here );
        set_time( old_time );
    } );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time( calendar::turn_zero );
    const auto origin = here.get_abs( tripoint_bub_ms( 12, 71, 0 ) );
    const auto target = here.get_abs( tripoint_bub_ms( 84, 71, 0 ) );
    vehicle *lamp = here.add_vehicle( vproto_id( "none" ), tripoint_bub_ms( 84, 73, 0 ),
                                    0_degrees, 0, veh_spawn_status::UNDAMAGED );
    REQUIRE( lamp );
    REQUIRE( lamp->install_part( here, point_rel_ms::zero, vpart_id( "frame" ) ) >= 0 );
    REQUIRE( lamp->install_part( here, point_rel_ms::zero,
                               vpart_id( "small_storage_battery" ) ) >= 0 );
    const int light = lamp->install_part( here, point_rel_ms::zero, vpart_id( "floodlight" ) );
    REQUIRE( light >= 0 );
    lamp->refresh();
    here.add_vehicle_to_cache( lamp );
    lamp->charge_battery( here, 500 );
    lamp->part( light ).enabled = true;
    lamp->power_parts( here );
    REQUIRE_FALSE( lamp->lights().empty() );
    const auto snapshot = [&]() {
        std::ostringstream bytes;
        JsonOut out( bytes );
        lamp->serialize( out );
        return bytes.str();
    };
    const auto before = snapshot();
    monster *observer = g->place_critter_at( mtype_id( "mon_mi_go" ), here.get_bub( origin ) );
    REQUIRE( observer );
    on_out_of_scope remove_observer( [&]() { g->remove_zombie( *observer ); } );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    const auto near = [&]() {
        scout_observation::site_reader reader;
        return reader.read_access( *observer, target );
    }();
    CAPTURE( near.visibility.reason );
    REQUIRE( near.visibility.known );
    REQUIRE( near.visibility.visible );
    REQUIRE_FALSE( near.facts.empty() );
    CHECK( snapshot() == before );
    here.load( old_origin + point( 20, 20 ), false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    REQUIRE_FALSE( here.inbounds( target ) );
    const auto far = [&]() {
        scout_observation::site_reader reader;
        return reader.read_access( *observer, target );
    }();
    CAPTURE( far.visibility.reason );
    CHECK( far.visibility.known == near.visibility.known );
    CHECK( far.visibility.visible == near.visibility.visible );
    CHECK( far.facts == near.facts );
    CHECK( snapshot() == before );
    CHECK( calendar::turn == calendar::turn_zero );
    CHECK( observer->pos_abs() == origin );
}

TEST_CASE( "existing-only submap retrieval never fills omitted serialized geometry",
           "[binocular_056][existing_geometry]" )
{
    const tripoint_abs_sm stored( 100, 100, 0 );
    const tripoint_abs_sm omitted = stored + point( 1, 0 );
    const auto omt = project_to<coords::omt>( stored );
    const auto segment = project_to<coords::seg>( omt );
    const auto directory = PATH_INFO::current_dimension_save_path() / "maps" /
                           string_format( "%d.%d.%d", segment.x(), segment.y(), segment.z() );
    const auto file = directory / string_format( "%d.%d.%d.map", omt.x(), omt.y(), omt.z() );
    REQUIRE_FALSE( std::filesystem::exists( file.get_unrelative_path() ) );
    std::filesystem::create_directories( directory.get_unrelative_path() );
    on_out_of_scope remove_fixture( [&]() {
        std::filesystem::remove( file.get_unrelative_path() );
        std::filesystem::remove( ( directory + ".zzip" ).get_unrelative_path() );
        std::filesystem::remove( directory.get_unrelative_path() );
    } );
    submap sample;
    sample.set_all_ter( ter_str_id( "t_wall" ) );
    std::ostringstream bytes;
    JsonOut writer( bytes );
    writer.start_array();
    writer.start_object();
    writer.member( "version", savegame_version );
    writer.member( "coordinates", stored.raw() );
    sample.store( writer );
    writer.end_object();
    writer.end_array();
    std::ofstream output( file.get_unrelative_path() );
    output << bytes.str();
    output.close();
    if( world_generator->active_world->has_compression_enabled() ) {
        REQUIRE( zzip::create_from_folder( ( directory + ".zzip" ).get_unrelative_path(),
                                           directory.get_unrelative_path(),
                                           ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path() ) );
    }
    mapbuffer existing;
    CHECK_FALSE( existing.lookup_submap_cached( stored ) );
    CHECK_FALSE( existing.lookup_submap_cached( omitted ) );
    const auto count_before = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    REQUIRE( existing.lookup_submap_existing( stored ) );
    CHECK( existing.lookup_submap_cached( stored ) == existing.lookup_submap_existing( stored ) );
    CHECK( existing.lookup_submap_existing( stored )->get_ter( point_sm_ms( 0, 0 ) ) ==
           ter_str_id( "t_wall" ).id() );
    CHECK_FALSE( existing.lookup_submap_existing( omitted ) );
    CHECK_FALSE( existing.lookup_submap_cached( omitted ) );
    CHECK( std::distance( existing.begin(), existing.end() ) == 1 );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == count_before );
    SECTION( "ordinary loading completes the partial quad without reloading cached terrain" ) {
        overmap_buffer.ter_set( omt, oter_id( "open_air" ) );
        on_out_of_scope remove_global_fixture( [&]() {
            MAPBUFFER.clear_outside_reality_bubble();
        } );
        submap *original = MAPBUFFER.lookup_submap_existing( stored );
        REQUIRE( original );
        REQUIRE( MAPBUFFER.lookup_submap( omitted ) );
        CHECK( MAPBUFFER.lookup_submap( stored ) == original );
        CHECK( original->get_ter( point_sm_ms( 0, 0 ) ) == ter_str_id( "t_wall" ).id() );
        CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == count_before + 4 );
    }
}

// Called under the real turn-row lifetime by the semantic output control.
// The explicit build/lookup contexts below test attribution and error unwind;
// they do not claim a native NAV journey or a performance measurement.
std::map<std::string, int> exercise_nav_lookup_diagnostics_for_test( bool reuse_archive );
std::map<std::string, int> exercise_nav_lookup_diagnostics_for_test( bool reuse_archive )
{
    const tripoint_abs_sm stored( 100, 100, 0 );
    const auto omt = project_to<coords::omt>( stored );
    const auto segment = project_to<coords::seg>( omt );
    const auto directory = PATH_INFO::current_dimension_save_path() / "maps" /
                           string_format( "%d.%d.%d", segment.x(), segment.y(), segment.z() );
    const auto archive = directory + ".zzip";
    REQUIRE_FALSE( std::filesystem::exists( directory.get_unrelative_path() ) );
    REQUIRE_FALSE( std::filesystem::exists( archive.get_unrelative_path() ) );
    std::filesystem::create_directories( directory.get_unrelative_path() );
    on_out_of_scope remove_fixture( [&]() {
        std::filesystem::remove_all( directory.get_unrelative_path() );
        std::filesystem::remove( archive.get_unrelative_path() );
    } );
    const auto quad_path = [&]( const tripoint_abs_sm &p ) {
        const auto position = project_to<coords::omt>( p );
        return directory / string_format( "%d.%d.%d.map", position.x(), position.y(), position.z() );
    };
    submap sample;
    sample.set_all_ter( ter_str_id( "t_wall" ) );
    std::ostringstream bytes;
    JsonOut writer( bytes );
    writer.start_array();
    writer.start_object();
    writer.member( "version", savegame_version );
    writer.member( "coordinates", stored.raw() );
    sample.store( writer );
    writer.end_object();
    writer.end_array();
    { std::ofstream out( quad_path( stored ).get_unrelative_path() ); out << bytes.str(); }
    const auto corrupt = stored + point( 4, 0 );
    { std::ofstream out( quad_path( corrupt ).get_unrelative_path() ); out << "invalid-json"; }
    const bool compressed = world_generator->active_world->has_compression_enabled();
    const auto dictionary = ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path();
    if( compressed ) {
        REQUIRE( zzip::create_from_folder( archive.get_unrelative_path(),
                                          directory.get_unrelative_path(), dictionary ) );
    }
    const auto unchanged_path = compressed ? archive : quad_path( stored );
    const auto contents = [&]() {
        std::ifstream input( unchanged_path.get_unrelative_path(), std::ios::binary );
        return std::string( std::istreambuf_iterator<char>( input ), {} );
    };
    const auto before_file = contents();
    const auto global_count = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    // An unrelated real loader, even in a diagnostic row, earns no NAV count.
    mapbuffer outside;
    REQUIRE( outside.lookup_submap_existing( stored ) );
    CHECK_FALSE( raid_perf_nav_lookup_active() );
    mapbuffer existing;
    std::optional<mapbuffer::existing_submap_reader> reader;
    if( reuse_archive ) {
        reader.emplace( existing );
    }
    const auto lookup = [&]( const tripoint_abs_sm &p ) {
        return reader ? reader->lookup( p ) : existing.lookup_submap_existing( p );
    };
    {
        raid_perf_scope build( raid_perf_stage::physical_navigation_view_build );
        const bool active = raid_perf_nav_build_active();
        {
            raid_perf_scope nested( raid_perf_stage::physical_navigation_view_build );
            CHECK( raid_perf_nav_build_active() == active );
            REQUIRE( lookup( stored ) );
        }
        CHECK( raid_perf_nav_build_active() == active );
        CHECK_FALSE( raid_perf_nav_lookup_active() );
        const auto *original = existing.lookup_submap_cached( stored );
        CHECK( lookup( stored ) == original );
        CHECK( original->get_ter( point_sm_ms( 0, 0 ) ) == ter_str_id( "t_wall" ).id() );
        const auto omitted = stored + point( 1, 0 );
        CHECK_FALSE( lookup( omitted ) );
        CHECK_FALSE( lookup( omitted ) );
        CHECK_FALSE( existing.lookup_submap_cached( omitted ) );
        CHECK_FALSE( lookup( stored + point( 2, 0 ) ) );
        CHECK_FALSE( lookup( stored + point( 64, 0 ) ) );
        std::string warning;
        {
            restore_on_out_of_scope restore_mode( test_mode );
            test_mode = true; // capture the deliberate loader error without a native modal
            warning = capture_debugmsg_during( [&]() {
                CHECK_FALSE( lookup( corrupt ) );
            } );
        }
        CHECK_FALSE( warning.empty() );
        CHECK_FALSE( raid_perf_nav_lookup_active() ); // actual exception/catch restores lookup depth
        // Force the existing archive open failure without changing permissions:
        // this owned parent is a regular file. Explicit context is a counter
        // unit control, not an extra production lookup or an AI observation.
        const auto blocker = directory / "open-blocker";
        { std::ofstream out( blocker.get_unrelative_path() ); out << "blocker"; }
        {
            raid_perf_scope lookup( raid_perf_stage::nav_view_lookup, active );
            CHECK_FALSE( zzip::load( ( blocker / "child.zzip" ).get_unrelative_path(), dictionary ) );
        }
        CHECK_FALSE( raid_perf_nav_lookup_active() );
    }
    CHECK_FALSE( raid_perf_nav_build_active() );
    try {
        raid_perf_scope build( raid_perf_stage::physical_navigation_view_build );
        throw std::runtime_error( "diagnostic depth unwind" );
    } catch( const std::runtime_error & ) {
    }
    CHECK_FALSE( raid_perf_nav_build_active() );
    CHECK_FALSE( raid_perf_nav_lookup_active() );
    CHECK( std::distance( existing.begin(), existing.end() ) == 1 );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == global_count );
    CHECK( contents() == before_file );
    return {
        { "resident_hit", 1 }, { "resident_null", 0 }, { "buffer_miss", 6 },
        { "archive_load_calls", compressed ? ( reuse_archive ? 1 : 5 ) : 0 },
        { "archive_open_attempts", compressed ? ( reuse_archive ? 4 : 8 ) : 3 }, { "archive_open_failures", 3 },
        { "missing_archive", compressed ? 1 : 0 }, { "missing_quad", compressed ? 1 : 0 },
        { "omitted_submap", 2 }, { "loaded_submap", 1 }, { "load_failures", 3 },
        { "plain_read_calls", compressed ? 0 : 6 },
        { "level_fresh", 0 }, { "level_reused", 0 }, { "level_inside_lookup", 0 }
    };
}

TEST_CASE( "target vehicle queries ignore unrelated resident submaps and vehicles",
           "[binocular_056][bounded_vehicle_work]" )
{
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 2, false );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    const auto saved_turn = calendar::turn;
    on_out_of_scope cleanup( [&]() {
        MAPBUFFER.clear_outside_reality_bubble();
        set_time( saved_turn );
    } );
    set_time( calendar::turn_zero + 12_hours );
    map &here = get_map();
    {
        map fixture;
        fixture.load( here.get_abs_sub() + point( -6, 0 ), false );
        std::vector<std::unique_ptr<map>> lighting_context;
        for( const point &offset : {
                 point( -7, -4 ), point( -7, 4 ),
                 point( -6, -4 ), point( -6, 4 )
             } ) {
            auto context = std::make_unique<map>();
            context->load( here.get_abs_sub() + offset, false );
            for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
                for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
                    context->ter_set( tripoint_bub_ms( x, y, 0 ), ter_str_id( "t_grass" ) );
                    context->ter_set( tripoint_bub_ms( x, y, 1 ), ter_str_id( "t_open_air" ) );
                    context->furn_set( tripoint_bub_ms( x, y, 0 ), furn_str_id( "f_null" ) );
                    context->field_at( tripoint_bub_ms( x, y, 0 ) ).clear();
                }
            }
            lighting_context.push_back( std::move( context ) );
        }
        for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
            for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
                fixture.ter_set( tripoint_bub_ms( x, y, 0 ), ter_str_id( "t_grass" ) );
                fixture.ter_set( tripoint_bub_ms( x, y, 1 ), ter_str_id( "t_open_air" ) );
                fixture.furn_set( tripoint_bub_ms( x, y, 0 ), furn_str_id( "f_null" ) );
                fixture.field_at( tripoint_bub_ms( x, y, 0 ) ).clear();
                fixture.i_clear( tripoint_bub_ms( x, y, 0 ) );
            }
        }
        fixture.invalidate_map_cache( 0 );
        fixture.invalidate_map_cache( 1 );
        fixture.build_map_cache( 0 );
        npc observer;
        observer.normalize();
        observer.set_fake( true );
        observer.recalc_sight_limits();
        observer.setpos( here.get_abs( tripoint_bub_ms( -12, 30, 0 ) ), false );
        get_avatar().setpos( here.get_abs( tripoint_bub_ms( -12, 102, 0 ) ), false );
        const std::vector<tripoint_abs_omt> footprint = { get_avatar().pos_abs_omt() };
        scout_observation::site_reader before;
        const auto original = before.read_character( observer, get_avatar(), footprint );
        REQUIRE( original.known );
        REQUIRE( original.visible );
        REQUIRE( before.cached_view_count() > 0 );
        const auto original_queries = before.cached_vehicle_cell_query_count();
        const auto original_parts = before.cached_vehicle_part_read_count();
        const auto original_scans = before.cached_vehicle_scan_count();
        for( int i = 0; i < 2048; ++i ) {
            const tripoint_abs_sm pos( 700000 + i, 800000, 0 );
            REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( pos ) );
            auto sm = std::make_unique<submap>();
            sm->set_all_ter( ter_str_id( "t_grass" ) );
            // Real physical owners in unrelated cells exercise vehicle admission too.
            if( i % 32 == 0 ) {
                auto unrelated = std::make_unique<vehicle>( vproto_id( "vehicle_camera_test" ) );
                unrelated->sm_pos = pos;
                sm->vehicles.push_back( std::move( unrelated ) );
            }
            REQUIRE( MAPBUFFER.add_submap( pos, sm ) );
        }
        scout_observation::site_reader after;
        const auto populated = after.read_character( observer, get_avatar(), footprint );
        CHECK( populated.known == original.known );
        CHECK( populated.visible == original.visible );
        CAPTURE( original_scans, after.cached_vehicle_scan_count(), original_queries,
                 after.cached_vehicle_cell_query_count(), original_parts,
                 after.cached_vehicle_part_read_count() );
        CHECK( original_scans == 0 );
        CHECK( after.cached_vehicle_scan_count() == 0 );
        CHECK( after.cached_vehicle_cell_query_count() == original_queries );
        CHECK( after.cached_vehicle_part_read_count() == original_parts );
    }
}

TEST_CASE( "resident vehicle part index follows native ownership and geometry",
           "[binocular_056][resident_vehicle_index]" )
{
    SECTION( "admission finds a distant overhang without assuming a vehicle size limit" ) {
        mapbuffer resident;
        const tripoint_abs_sm owner( 900000, 900000, 0 );
        auto sm = std::make_unique<submap>();
        auto body = std::make_unique<vehicle>( vproto_id( "vehicle_camera_test" ) );
        vehicle *v = body.get();
        REQUIRE( v->part_count() > 0 );
        // The actual part is twenty OMT from the serialized owner. Admission
        // uses that owner frame even before native load establishes sm_pos.
        v->part( 0 ).mount = point_rel_ms( -480, 0 );
        v->refresh();
        const auto relative = v->abs_part_pos( v->part( 0 ) ) -
                              project_to<coords::ms>( v->sm_pos );
        const auto pos = project_to<coords::ms>( owner ) + relative;
        const auto cell = project_to<coords::sm>( pos );
        REQUIRE( rl_dist( cell.xy(), owner.xy() ) >= 39 );
        sm->vehicles.push_back( std::move( body ) );
        REQUIRE( resident.add_submap( owner, sm ) );
        const auto contains = [&]( const tripoint_abs_sm & at, const tripoint_abs_ms & part_pos ) {
            const auto &parts = resident.vehicle_parts_at( at );
            return std::any_of( parts.begin(), parts.end(), [&]( const auto & entry ) {
                return entry.first == v && entry.second == part_pos;
            } );
        };
        REQUIRE( contains( cell, pos ) );
        SECTION( "part changes refresh the owned index and discard the old cells" ) {
            v->part( 0 ).mount = point_rel_ms( -600, 0 );
            v->refresh();
            CHECK_FALSE( contains( cell, pos ) );
            const auto next_pos = project_to<coords::ms>( owner ) +
                                  ( v->abs_part_pos( v->part( 0 ) ) - project_to<coords::ms>( v->sm_pos ) );
            CHECK( contains( project_to<coords::sm>( next_pos ), next_pos ) );
            v->part( 0 ).removed = true;
            v->refresh();
            CHECK_FALSE( contains( project_to<coords::sm>( next_pos ), next_pos ) );
        }
        SECTION( "move assignment preserves each physical owner until native refresh" ) {
            const auto other_owner = owner + point( 100, 0 );
            auto other_sm = std::make_unique<submap>();
            auto other_body = std::make_unique<vehicle>( vproto_id( "vehicle_camera_test" ) );
            vehicle *other = other_body.get();
            other->sm_pos = other_owner;
            other->part( 0 ).mount = point_rel_ms( -720, 0 );
            other->refresh();
            other_sm->vehicles.push_back( std::move( other_body ) );
            REQUIRE( resident.add_submap( other_owner, other_sm ) );
            const auto source_pos = other->abs_part_pos( other->part( 0 ) );
            REQUIRE_FALSE( resident.vehicle_parts_at( project_to<coords::sm>( source_pos ) ).empty() );
            *v = std::move( *other );
            v->refresh();
            other->refresh();
            CHECK_FALSE( contains( cell, pos ) );
            const auto next_pos = project_to<coords::ms>( owner ) +
                                  ( v->abs_part_pos( v->part( 0 ) ) - project_to<coords::ms>( v->sm_pos ) );
            CHECK( contains( project_to<coords::sm>( next_pos ), next_pos ) );
            CHECK( resident.vehicle_parts_at( project_to<coords::sm>( source_pos ) ).empty() );
            resident.lookup_submap_cached( other_owner )->vehicles.clear();
            CHECK( contains( project_to<coords::sm>( next_pos ), next_pos ) );
        }
        SECTION( "destruction removes every overhanging reference" ) {
            resident.lookup_submap_cached( owner )->vehicles.clear();
            CHECK( resident.vehicle_parts_at( cell ).empty() );
        }
        resident.clear();
        CHECK( resident.vehicle_parts_at( cell ).empty() );
    }
    SECTION( "movement and detachment update native resident cells" ) {
        clear_map();
        map &here = get_map();
        vehicle *v = here.add_vehicle( vproto_id( "vehicle_camera_test" ),
                                       tripoint_bub_ms( 60, 60, 0 ), 0_degrees, 0,
                                       veh_spawn_status::UNDAMAGED );
        REQUIRE( v );
        const auto original = v->abs_part_pos( v->part( 0 ) );
        const auto count = [&]( const tripoint_abs_ms & pos ) {
            const auto &parts = MAPBUFFER.vehicle_parts_at( project_to<coords::sm>( pos ) );
            return std::count_if( parts.begin(), parts.end(), [&]( const auto & entry ) {
                return entry.first == v && entry.second == pos;
            } );
        };
        REQUIRE( count( original ) > 0 );
        REQUIRE( here.displace_vehicle( *v, tripoint_rel_ms( 12, 0, 0 ) ) );
        CHECK( count( original ) == 0 );
        const auto moved = v->abs_part_pos( v->part( 0 ) );
        REQUIRE( count( moved ) > 0 );
        auto detached = here.detach_vehicle( v );
        REQUIRE( detached.get() == v );
        CHECK( count( moved ) == 0 );
        REQUIRE_FALSE( here.place_vehicle( std::move( detached ) ) );
        // Native ownership reattachment, independently of visibility caches.
        CHECK( count( moved ) > 0 );
        here.destroy_vehicle( v );
        CHECK( count( moved ) == 0 );
    }
}

TEST_CASE( "active structural scouts separate coarse hostile optics from real danger",
           "[scout_observation][bandit][npc]" )
{
    const bool cannibal = GENERATE( false, true );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    const time_point old_turn = calendar::turn;
    const tripoint_abs_ms old_avatar_position = get_avatar().pos_abs();
    const bandit_live_world::world_state old_world =
        overmap_buffer.global_state.bandit_live_world;
    g->faction_manager_ptr->create_if_needed();
    clear_map_without_vision();
    clear_avatar();
    clear_npcs();
    g->clear_zombies();
    set_time_to_day();
    on_out_of_scope restore( [ = ]() {
        clear_npcs();
        g->clear_zombies();
        get_avatar().remove_weapon(); // retire this fixture's native attack weapon
        overmap_buffer.global_state.bandit_live_world = old_world;
        get_avatar().setpos( old_avatar_position, false );
        set_time( old_turn );
        sounds::reset_sounds();
    } );
    map &here = get_map();
    npc &observer = spawn_npc( point_bub_ms( 40, 66 ), "test_talker" );
    npc &partner = spawn_npc( point_bub_ms( 42, 66 ), "test_talker" );
    const faction_id camp_faction( cannibal ? "cannibal_camp" : "hells_raiders" );
    observer.set_fac( camp_faction );
    partner.set_fac( camp_faction );
    observer.i_add( item( itype_id( "binoculars" ) ) );
    observer.set_attitude( NPCATT_NULL );
    partner.set_attitude( NPCATT_NULL );
    observer.set_sleepiness( 0 );
    partner.set_sleepiness( 0 );
    get_avatar().setpos( here, tripoint_bub_ms( 112, 66, 0 ), false );
    set_time_to_day();
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );

    bandit_live_world::world_state &world =
        overmap_buffer.global_state.bandit_live_world;
    world = bandit_live_world::world_state();
    bandit_live_world::site_record site;
    site.site_id = cannibal ? "scout-optics-cannibal-site" : "scout-optics-bandit-site";
    site.site_kind = cannibal ? bandit_live_world::owned_site_kind::cannibal_camp :
                     bandit_live_world::owned_site_kind::bandit_camp;
    site.profile = cannibal ? bandit_live_world::hostile_site_profile::cannibal_camp :
                   bandit_live_world::hostile_site_profile::camp_style;
    site.living_total = 2;
    site.members = {
        {
            observer.getID(), "test_talker", observer.pos_abs(),
            bandit_live_world::member_state::outbound, false, ""
        },
        {
            partner.getID(), "test_talker", partner.pos_abs(),
            bandit_live_world::member_state::outbound, false, ""
        }
    };
    bandit_live_world::active_outing_state &outing = site.active_outing;
    outing.schema_version = 10;
    outing.kind = bandit_live_world::outing_kind::structural_sortie;
    outing.activity_id = site.site_id + "-active-scout";
    outing.camp_id = site.site_id;
    outing.generation = 1;
    outing.member_ids = { observer.getID(), partner.getID() };
    outing.leader_id = observer.getID();
    outing.job_type = "scout";
    outing.phase = bandit_live_world::scout_phase::observing;
    outing.owner = bandit_live_world::simulation_owner::abstract;
    outing.handoff_epoch = 0;
    outing.started_minutes = 0;
    outing.last_progress_minutes = 0;
    outing.last_advanced_minutes = 0;
    outing.target_omt = get_avatar().pos_abs_omt();
    outing.target_footprint = { outing.target_omt };
    outing.selected_watch_kind = bandit_live_world::structural_watch_kind::exact;
    outing.selected_watch_omt = observer.pos_abs_omt();
    outing.selected_watch_route_cost = 0;
    world.sites.push_back( std::move( site ) );
    bandit_live_world::site_record &active_site = world.sites.front();
    REQUIRE( bandit_live_world::current_external_simulation_cursor( active_site ) );
    REQUIRE( bandit_live_world::active_operation_duty_site_for( world, observer.getID() ) ==
             &active_site );
    SECTION( "current perception and received attacks keep their Native priority" ) {
        monster *coarse_hostile = g->place_critter_at( mtype_id( "mon_dog_gpyrenees" ),
                                  tripoint_bub_ms( 43, 66, 0 ) );
        REQUIRE( coarse_hostile );
        coarse_hostile->anger = 0;
        coarse_hostile->morale = 0;
        REQUIRE( coarse_hostile->has_flag( mon_flag_HIT_AND_RUN ) );
        REQUIRE( observer.sees_without_clairvoyance( here, *coarse_hostile ) );
        REQUIRE( observer.attitude_to( *coarse_hostile ) == Creature::Attitude::HOSTILE );
        REQUIRE( coarse_hostile->attitude( &observer ) == MATT_IGNORE );
        CHECK_FALSE( observer.scouting_creature_requires_response( *coarse_hostile,
                     "test_coarse_hostile_ignore" ) );
        scout_observation::site_reader optics;
        CHECK( optics.watching_member( active_site, observer ) );
        coarse_hostile->add_effect( efftype_id( "run" ), 1_minutes );
        REQUIRE( coarse_hostile->attitude( &observer ) == MATT_FLEE );
        CHECK_FALSE( observer.scouting_creature_requires_response( *coarse_hostile,
                     "test_coarse_hostile_flee" ) );
        CHECK( optics.watching_member( active_site, observer ) );

        g->clear_zombies();
        monster *actual_attacker = g->place_critter_at( mtype_id( "mon_zombie" ),
                                   tripoint_bub_ms( 41, 66, 0 ) );
        REQUIRE( actual_attacker );
        actual_attacker->anger = 100;
        REQUIRE( observer.sees_without_clairvoyance( here, *actual_attacker ) );
        REQUIRE( actual_attacker->attitude( &observer ) == MATT_ATTACK );
        REQUIRE( actual_attacker->type->melee_dice > 0 );
        REQUIRE( actual_attacker->can_reach_to( observer.pos_bub() ) );
        CHECK( observer.scouting_creature_requires_response( *actual_attacker,
                "test_native_melee_threat" ) );
        CHECK_FALSE( optics.watching_member( active_site, observer ) );

        g->clear_zombies();
        get_avatar().setpos( here, observer.pos_bub() + point::north, false );
        get_avatar().set_skill_level( skill_id( "melee" ), 8 );
        get_avatar().set_skill_level( skill_id( "bashing" ), 8 );
        // Random native doctor equipment can absorb every unarmed strike.
        // Establish a real injury without removing the observer's optics or
        // armor, or replacing native combat with direct damage injection.
        item bat( itype_id( "bat" ) );
        REQUIRE( get_avatar().wield( bat ) );
        const int initial_hp = observer.get_hp();
        std::vector<int> attempted_attacks;
        std::vector<int> spent_moves;
        const int starting_dodges = observer.get_dodges_left();
        for( int hit = 0; hit < 5 && observer.get_hp() == initial_hp; ++hit ) {
            get_avatar().set_moves( 100 );
            attempted_attacks.push_back( get_avatar().melee_attack( observer, false ) ? 1 : 0 );
            spent_moves.push_back( 100 - get_avatar().get_moves() );
        }
        CAPTURE( cannibal, attempted_attacks, spent_moves, starting_dodges,
                 observer.get_dodges_left(), get_avatar().get_str(), observer.get_dodge(),
                 get_avatar().pos_abs(), observer.pos_abs(),
                 observer.get_armor_type( damage_type_id( "bash" ), bodypart_id( "torso" ) ),
                 observer.get_armor_type( damage_type_id( "bash" ), bodypart_id( "arm_l" ) ),
                 get_avatar().is_adjacent( &observer, false ) );
        REQUIRE( observer.get_hp() < initial_hp );
        REQUIRE( observer.faction_alarm );
        REQUIRE( observer.faction_alarm->received_attack.value_or( false ) );
        REQUIRE( observer.faction_alarm_requires_response() );

        get_avatar().setpos( here, tripoint_bub_ms( 60, 66, 0 ), false );
        scout_observation::site_reader current_optics;
        // The earlier real attacker is gone; its received warning remains.
        // Missing geometry is covered by the dedicated unavailable-geometry section.
        CHECK( current_optics.perceived_threats( observer ).empty() );
        CHECK( observer.faction_alarm_requires_response() );
        CHECK_FALSE( current_optics.watching_member( active_site, observer ) );
        // A native bat hit can knock the scout down. Check that gate, then
        // model recovery for the separate post-injury sight checks; the injury
        // and received alarm remain in place.
        if( observer.has_effect( efftype_id( "downed" ) ) ) {
            CHECK_FALSE( current_optics.read_character( observer, get_avatar() ).visible );
            observer.remove_effect( efftype_id( "downed" ) );
        }
        const auto player_read = current_optics.read_character( observer, get_avatar() );
        const auto signal_read = current_optics.read_signal( observer, observer.pos_abs(),
                                 get_avatar().pos_abs(),
        { get_avatar().pos_abs_omt() } );
        INFO( player_read.reason );
        INFO( signal_read.reason );
        CHECK( player_read.known );
        CHECK( signal_read.known );
        CHECK( player_read.visible );
        CHECK( signal_read.visible );

        std::ostringstream bytes;
        JsonOut writer( bytes );
        observer.serialize( writer );
        npc loaded;
        loaded.deserialize( json_loader::from_string( bytes.str() ).get_object() );
        loaded.set_fac( camp_faction );
        CHECK( loaded.getID() == observer.getID() );
        CHECK( bandit_live_world::active_operation_duty_site_for( world, loaded.getID() ) ==
               &active_site );
        CHECK( loaded.faction_alarm_requires_response() );
        CHECK( current_optics.read_character( loaded, get_avatar() ).visible );
        CHECK( current_optics.read_signal( loaded, loaded.pos_abs(), get_avatar().pos_abs(),
        { get_avatar().pos_abs_omt() } ).visible );
        SECTION( "incapacitated or field-blocked partner retains actual perceived threats" ) {
            const bool field = GENERATE( false, true );
            g->clear_zombies();
            monster *threat = g->place_critter_at( mtype_id( "mon_zombie" ),
                                                   partner.pos_bub() + point::east );
            REQUIRE( threat );
            threat->anger = 100;
            REQUIRE( partner.sees_without_clairvoyance( here, *threat ) );
            REQUIRE( partner.scouting_creature_requires_response( *threat, "partner_route_knowledge" ) );
            REQUIRE( partner.raise_faction_alarm( threat->pos_abs() ) );
            on_out_of_scope remove_field( [&]() {
                if( field ) {
                    here.remove_field( partner.pos_bub(), field_type_id( "fd_fire" ) );
                }
            } );
            if( field ) {
                REQUIRE( here.add_field( partner.pos_bub(), field_type_id( "fd_fire" ), 2 ) );
            } else {
                partner.add_effect( efftype_id( "stunned" ), 1_minutes );
            }
            REQUIRE( partner.sees_without_clairvoyance( here, *threat ) );
            CHECK_FALSE( optics.watching_member( active_site, partner ) );
            const auto positions = optics.perceived_threats( partner );
            CHECK( std::find( positions.begin(), positions.end(), threat->pos_abs() ) != positions.end() );
            partner.fall_asleep( 1_minutes );
            CHECK( optics.perceived_threats( partner ).empty() );
            CHECK( partner.has_active_faction_alarm() );
            CHECK( partner.faction_alarm_requires_response() );
        }
    }
    SECTION( "bounded watch avoidance does not impose an incident-distance wall on home routes" ) {
        const tripoint_abs_omt origin = observer.pos_abs_omt();
        active_site.anchor = origin + point( -2, 0 );
        active_site.footprint = { active_site.anchor };
        std::vector<std::pair<tripoint_abs_omt, oter_id>> saved_terrain;
        for( int y = -3; y <= 3; ++y ) {
            for( int x = -3; x <= 3; ++x ) {
                const auto cell = origin + point( x, y );
                saved_terrain.emplace_back( cell, overmap_buffer.ter( cell ) );
                overmap_buffer.ter_set( cell, oter_id( "field" ) );
            }
        }
        on_out_of_scope restore_terrain( [&]() {
            for( const auto &entry : saved_terrain ) {
                overmap_buffer.ter_set( entry.first, entry.second );
            }
        } );
        // Declared authenticated incident stress input through the existing
        // alarm API, not a claim that this fixture witnessed a remote attack.
        // Neither alarm authentication nor reception bounds incident distance.
        const tripoint_abs_omt remote = origin + point( -10000, 0 );
        REQUIRE( partner.raise_faction_alarm( project_to<coords::ms>( remote ), true ) );
        REQUIRE( partner.has_active_faction_alarm() );
        REQUIRE( partner.faction_alarm_requires_response() );
        const auto toward = origin + point( -1, -1 );
        CHECK( live_bandit_member_route_to_for_test( observer, active_site, toward ).empty() );
        CHECK_FALSE( live_bandit_member_route_to_for_test( observer, active_site,
                     origin + point( 0, -1 ) ).empty() );
        const auto home = live_bandit_member_route_to_for_test( observer, active_site, active_site.anchor );
        REQUIRE_FALSE( home.empty() );
        CHECK( home.front() == active_site.anchor );
        // Native overmap paths retain the source as their last entry; the
        // ordinary mover consumes that entry before taking the preceding step.
        REQUIRE( home.size() > 1 );
        CHECK( home.back() == origin );
        CHECK( rl_dist( home[home.size() - 2], remote ) < rl_dist( origin, remote ) );
        auto returning = active_site;
        returning.active_outing.phase = bandit_live_world::scout_phase::returning_home;
        CHECK_FALSE( live_bandit_member_route_to_for_test( observer, returning, toward ).empty() );
        CHECK( partner.faction_alarm_requires_response() );
    }
}

TEST_CASE( "existing-only archive reuse ends with one geometry preparation",
           "[existing_geometry][navigation_preparation_lifetime]" )
{
    const bool compressed = GENERATE( false, true );
    CAPTURE( compressed );
    REQUIRE( world_generator->active_world );
    WORLD &world = *world_generator->active_world;
    const bool previous_compression = world.has_compression_enabled();
    REQUIRE( world.set_compression_enabled( compressed ) );
    on_out_of_scope restore_compression( [&] {
        CHECK( world.set_compression_enabled( previous_compression ) );
    } );
    const tripoint_abs_sm stored( 140, 140, 0 );
    const auto omitted = stored + point( 1, 0 );
    const auto absent_quad = stored + point( 2, 0 );
    const auto corrupt = stored + point( 4, 0 );
    const auto other_level = stored + tripoint_rel_sm( 0, 0, 1 );
    const auto directory_for = [&]( const tripoint_abs_sm & p ) {
        const auto seg = project_to<coords::seg>( project_to<coords::omt>( p ) );
        return PATH_INFO::current_dimension_save_path() / "maps" /
               string_format( "%d.%d.%d", seg.x(), seg.y(), seg.z() );
    };
    const auto name_for = [&]( const tripoint_abs_sm & p ) {
        const auto omt = project_to<coords::omt>( p );
        return string_format( "%d.%d.%d.map", omt.x(), omt.y(), omt.z() );
    };
    const auto directory = directory_for( stored );
    const auto upper_directory = directory_for( other_level );
    for( const auto &dir : {
             directory, upper_directory
         } ) {
        REQUIRE_FALSE( std::filesystem::exists( dir.get_unrelative_path() ) );
        REQUIRE_FALSE( std::filesystem::exists( ( dir + ".zzip" ).get_unrelative_path() ) );
        std::filesystem::create_directories( dir.get_unrelative_path() );
    }
    on_out_of_scope remove_fixture( [&] {
        for( const auto &dir : {directory, upper_directory} ) {
            std::filesystem::remove_all( dir.get_unrelative_path() );
            std::filesystem::remove( ( dir + ".zzip" ).get_unrelative_path() );
        }
    } );
    const auto bytes_for = [&]( const std::vector<tripoint_abs_sm> &positions ) {
        std::ostringstream bytes;
        JsonOut writer( bytes );
        writer.start_array();
        for( const auto &p : positions ) {
            submap sample;
            sample.set_all_ter( ter_str_id( "t_floor" ) );
            writer.start_object();
            writer.member( "version", savegame_version );
            writer.member( "coordinates", p.raw() );
            sample.store( writer );
            writer.end_object();
        }
        writer.end_array();
        return bytes.str();
    };
    const auto dictionary = ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path();
    const auto write_quad = [&]( const tripoint_abs_sm & p, const std::string & bytes ) {
        const auto dir = directory_for( p );
        if( compressed ) {
            auto archive = zzip::load( ( dir + ".zzip" ).get_unrelative_path(), dictionary );
            REQUIRE( archive );
            REQUIRE( archive->add_file( name_for( p ), bytes ) );
        } else {
            std::ofstream out( ( dir / name_for( p ) ).get_unrelative_path() );
            out << bytes;
            REQUIRE( out.good() );
        }
    };
    write_quad( stored, bytes_for( {stored} ) );
    write_quad( corrupt, "invalid-json" );
    write_quad( other_level, bytes_for( {other_level} ) );
    const auto archive_bytes = [&]( const cata_path & dir ) {
        const auto file = compressed ? dir + ".zzip" : dir / name_for( stored );
        std::ifstream input( file.get_unrelative_path(), std::ios::binary );
        return std::string( std::istreambuf_iterator<char>( input ), {} );
    };
    const auto before_bytes = archive_bytes( directory );
    const auto global_count = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto before_turn = calendar::turn;
    mapbuffer reference;
    mapbuffer actual;
    {
        mapbuffer::existing_submap_reader reader( actual );
        // Distinct submaps and quads share an archive. Interleave another z
        // archive then revisit, exercising replacement of the single handle.
        for( const auto &p : {
                 stored, absent_quad, omitted, omitted, other_level,
                 absent_quad, stored
             } ) {
            const submap *expected = reference.lookup_submap_existing( p );
            const submap *result = reader.lookup( p );
            CHECK( static_cast<bool>( result ) == static_cast<bool>( expected ) );
            if( result ) {
                CHECK( result->get_ter( point_sm_ms( 0, 0 ) ) ==
                       expected->get_ter( point_sm_ms( 0, 0 ) ) );
            }
        }
        CHECK( reader.lookup( stored ) == actual.lookup_submap_cached( stored ) );
        CHECK_FALSE( actual.lookup_submap_cached( omitted ) );
        CHECK_FALSE( actual.lookup_submap_cached( absent_quad ) );
        for( int repeat = 0; repeat < 2; ++repeat ) {
            const std::string warning = capture_debugmsg_during( [&] {
                CHECK_FALSE( reader.lookup( corrupt ) );
            } );
            CHECK_FALSE( warning.empty() ); // errors are retried/reported, not cached away
        }
    }
    CHECK( archive_bytes( directory ) == before_bytes );
    CHECK( std::distance( actual.begin(), actual.end() ) == 2 );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == global_count );
    CHECK( calendar::turn == before_turn );
    // Only after the preparation closes may authoritative state change.
    submap *resident = actual.lookup_submap_cached( stored );
    REQUIRE( resident );
    resident->set_ter( point_sm_ms( 0, 0 ), ter_str_id( "t_wall" ).id() );
    write_quad( stored, bytes_for( {stored, omitted} ) );
    write_quad( absent_quad, bytes_for( {absent_quad} ) );
    {
        mapbuffer::existing_submap_reader fresh( actual );
        REQUIRE( fresh.lookup( omitted ) );
        REQUIRE( fresh.lookup( absent_quad ) );
        CHECK( fresh.lookup( stored ) == resident );
        CHECK( resident->get_ter( point_sm_ms( 0, 0 ) ) == ter_str_id( "t_wall" ).id() );
        CHECK( fresh.lookup( omitted )->get_ter( point_sm_ms( 0, 0 ) ) == ter_str_id( "t_floor" ).id() );
    }
    // Clearing/reloading is a new preparation, with no retained map pointers.
    actual.clear();
    {
        mapbuffer::existing_submap_reader reloaded( actual );
        REQUIRE( reloaded.lookup( stored ) );
        CHECK( reloaded.lookup( stored )->get_ter( point_sm_ms( 0, 0 ) ) == ter_str_id( "t_floor" ).id() );
        REQUIRE( reloaded.lookup( omitted ) );
    }
    CHECK( calendar::turn == before_turn );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == global_count );
}

TEST_CASE( "remote site lighting is independent of player proximity and irrelevant upper availability",
           "[binocular_056][remote_lighting_player_independence]" )
{
    const bool roof = GENERATE( false, true );
    CAPTURE( roof );
    // Other native save controls legitimately leave archives in the unit
    // world's directory. This case owns a fresh input namespace before any
    // reader/generation or corruption preparation, never deleting their input.
    const std::string old_savedir = PATH_INFO::savedir();
    const auto dictionary = ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path();
    const auto fixture_savedir = std::filesystem::path( old_savedir ) /
                                 ( roof ? "remote-lighting-roof" : "remote-lighting-open" );
    REQUIRE_FALSE( std::filesystem::exists( fixture_savedir ) );
    PATH_INFO::set_savedir( fixture_savedir.string() + "/" );
    on_out_of_scope restore_savedir( [&]() {
        PATH_INFO::set_savedir( old_savedir );
        std::filesystem::remove_all( fixture_savedir );
    } );
    std::filesystem::create_directories( PATH_INFO::world_base_save_path().get_unrelative_path() );
    if( std::filesystem::exists( dictionary ) ) {
        std::filesystem::copy_file( dictionary,
                                   ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path() );
    }
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 1, false );
    {
        map physical_inputs;
        physical_inputs.load( get_map().get_abs_sub() + point( 2, 0 ), false, false );
        physical_inputs.main_cleanup_override( false );
        wipe_map_terrain_with_vision( &physical_inputs, false );
    }
    const auto old_turn = calendar::turn;
    const auto old_origin = get_map().get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    std::vector<std::pair<tripoint_abs_sm, std::unique_ptr<submap>>> withheld;
    const auto restore_upper = [&]() {
        for( auto &[position, body] : withheld ) {
            if( body ) {
                const auto wanted_position = position;
                const auto slot = std::find_if( MAPBUFFER.begin(), MAPBUFFER.end(),
                [&]( const auto & entry ) { return entry.first == wanted_position; } );
                REQUIRE( slot != MAPBUFFER.end() );
                REQUIRE_FALSE( slot->second );
                slot->second = std::move( body );
            }
        }
    };
    on_out_of_scope restore( [&]() {
        restore_upper();
        get_map().load( old_origin, false, false );
        get_avatar().setpos( old_player, false );
        clear_map_with_vision( -1, 1, false );
        set_time( old_turn );
    } );
    set_time_to_day();
    scoped_weather_override weather( weather_type_id( "clear" ) );
    map &here = get_map();
    npc observer;
    observer.normalize();
    observer.set_fake( true );
    observer.recalc_sight_limits();
    observer.setpos( here.get_abs( tripoint_bub_ms( 70, 66, 0 ) ), false );
    const auto target = here.get_abs( tripoint_bub_ms( 84, 66, 0 ) );
    const auto fixed_observer = observer.pos_abs();
    const auto fixed_time = calendar::turn;
    const std::vector<tripoint_abs_omt> footprint = { project_to<coords::omt>( target ) };
    const auto first = project_to<coords::sm>( footprint.front() );
    monster *access_observer = g->place_critter_at( mtype_id( "mon_mi_go" ),
                               here.get_bub( fixed_observer ) );
    REQUIRE( access_observer );
    on_out_of_scope remove_access_observer( [&]() {
        g->remove_zombie( *access_observer );
    } );
    std::vector<scout_observation::access_read> access_reads;
    std::vector<std::optional<float>> exact_lighting;
    double read_seconds = 0;
    size_t read_calls = 0;
    const auto read_current = [&]() {
        // Each decision ends before native geometry/time/file changes.
        const auto start = std::chrono::steady_clock::now();
        const auto result = [&]() {
            scout_observation::site_reader reader;
            access_reads.push_back( reader.read_access( *access_observer, target ) );
            const auto observed = reader.read_signal( observer, fixed_observer, target, footprint );
            exact_lighting.push_back( reader.cached_lighting_at( target ) );
            return observed;
        }();
        read_seconds += std::chrono::duration<double>( std::chrono::steady_clock::now() - start ).count();
        ++read_calls;
        return result;
    };
    std::set<tripoint_abs_sm> upper;
    for( int z = 0; z <= OVERMAP_HEIGHT; ++z ) {
        for( int x = 0; x < 2; ++x ) {
            for( int y = 0; y < 2; ++y ) {
                const tripoint_abs_sm cell( first.x() + x, first.y() + y, z );
                submap *sm = MAPBUFFER.lookup_submap_cached( cell );
                REQUIRE( sm );
                REQUIRE( sm->vehicles.empty() );
                if( z == OVERMAP_HEIGHT ) {
                    // Expanded native storage need not carry the uniform flag.
                    // Prove the actual physical sky cells instead of assuming it.
                    for( int sx = 0; sx < SEEX; ++sx ) {
                        for( int sy = 0; sy < SEEY; ++sy ) {
                            REQUIRE( sm->get_ter( point_sm_ms( sx, sy ) ) == ter_str_id( "t_open_air" ) );
                        }
                    }
                    upper.insert( cell );
                }
            }
        }
    }
    const auto corner = project_to<coords::ms>( footprint.front() );
    for( int x = 0; x < 2 * SEEX; ++x ) {
        for( int y = 0; y < 2 * SEEY; ++y ) {
            const auto tile = here.get_bub( corner + point( x, y ) );
            here.ter_set( tile, ter_str_id( "t_grass" ) );
            here.ter_set( tile + tripoint::above,
                          ter_str_id( roof ? "t_flat_roof" : "t_open_air" ) );
        }
    }
    // A native non-carried field source keeps the roofed exterior meaningful.
    // This is a focused illumination control, not a distant source admission story.
    here.add_field( here.get_bub( target ), field_type_id( "fd_biolum" ), 3 );
    here.invalidate_map_cache( 0 );
    here.invalidate_map_cache( 1 );
    here.build_map_cache( 0 );
    REQUIRE( here.is_outside( here.get_bub( target ) ) );
    const auto near_full = read_current();
    const auto access_near_full = access_reads.back();
    REQUIRE( near_full.known );
    REQUIRE( near_full.visible );
    const float near_ambient = here.ambient_light_at( here.get_bub( target ) );
    for( auto it = MAPBUFFER.begin(); it != MAPBUFFER.end(); ++it ) {
        if( upper.count( it->first ) ) {
            withheld.emplace_back( it->first, std::move( it->second ) );
        }
    }
    REQUIRE( withheld.size() == 4 );
    // Retain the real objects while withholding only their lookup ownership.
    // The live map's existing pointers remain valid; no unknown cell becomes air.
    const auto near_sparse = read_current();
    const auto access_near_sparse = access_reads.back();
    CHECK( near_sparse.known == near_full.known );
    CHECK( near_sparse.visible == near_full.visible );
    here.load( old_origin + point( 20, 20 ), false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    REQUIRE_FALSE( here.inbounds( target ) );
    REQUIRE_FALSE( here.inbounds( fixed_observer ) );
    for( const auto &cell : upper ) {
        REQUIRE( MAPBUFFER.lookup_submap_cached( cell ) == nullptr );
    }
    const auto resident_count_before_read = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto seen_before_read = here.get_cache_ref( 0 ).seen_cache;
    const auto memory_before_read = here.get_cache_ref( 0 ).map_memory_cache_ter;
    const auto far_sparse = read_current();
    const auto access_far_sparse = access_reads.back();
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == resident_count_before_read );
    CHECK( here.get_cache_ref( 0 ).map_memory_cache_ter == memory_before_read );
    for( size_t x = 0; x < decltype( seen_before_read )::size_x; ++x ) {
        CHECK( here.get_cache_ref( 0 ).seen_cache[x] == seen_before_read[x] );
    }
    restore_upper();
    const auto far_full = read_current();
    const auto access_far_full = access_reads.back();
    std::cout << "REMOTE_LIGHTING_CAUSAL roof=" << roof
              << " observer=" << fixed_observer << " target=" << target
              << " missing_z=" << OVERMAP_HEIGHT << " missing_cells=" << upper.size()
              << " near_ambient=" << near_ambient
              << " near_full=" << near_full.known << '/' << near_full.visible << ':' << near_full.reason
              << " near_sparse=" << near_sparse.known << '/' << near_sparse.visible << ':' << near_sparse.reason
              << " far_sparse=" << far_sparse.known << '/' << far_sparse.visible << ':' << far_sparse.reason
              << " far_full=" << far_full.known << '/' << far_full.visible << ':' << far_full.reason
              << std::endl;
    CHECK( observer.pos_abs() == fixed_observer );
    CHECK( calendar::turn == fixed_time );
    CHECK( far_full.known == near_full.known );
    CHECK( far_full.visible == near_full.visible );
    // Product contract: changing player proximity must not change the same read.
    // The roofed arm also makes the withheld high sky physically irrelevant.
    CHECK( far_sparse.known == near_sparse.known );
    CHECK( far_sparse.visible == near_sparse.visible );
    REQUIRE( access_near_full.visibility.known );
    REQUIRE( access_near_full.visibility.visible );
    CHECK_FALSE( access_near_full.facts.empty() );
    for( const auto &access : { access_near_sparse, access_far_sparse, access_far_full } ) {
        CHECK( access.visibility.known == access_near_full.visibility.known );
        CHECK( access.visibility.visible == access_near_full.visibility.visible );
        CHECK( access.facts == access_near_full.facts );
    }

    // The same reconstruction must retain genuine native darkness. Remove the
    // actual source through its loaded map owner, then compare the same actors
    // and sky omission with the player near/far at the same night-time instant.
    here.load( old_origin, false, false );
    here.remove_field( here.get_bub( target ), field_type_id( "fd_biolum" ) );
    set_time( calendar::turn_zero );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    const auto dark_near = read_current();
    const auto access_dark_near = access_reads.back();
    CAPTURE( here.ambient_light_at( here.get_bub( target ) ),
             g->natural_light_level( 0 ),
             here.get_field_intensity( here.get_bub( target ), field_type_id( "fd_biolum" ) ),
             observer.sight_range( here.ambient_light_at( here.get_bub( target ) ),
                                   g->natural_light_level( 0 ) ) );
    REQUIRE( dark_near.known );
    CHECK( is_night( calendar::turn ) );
    CHECK( here.get_field_intensity( here.get_bub( target ), field_type_id( "fd_biolum" ) ) == 0 );
    CHECK( here.ambient_light_at( here.get_bub( target ) ) < near_ambient );
    // The native signal policy may classify rounded moonlight (4 vs 3.7)
    // as lit. Preserve that existing decision rather than inventing a new
    // darkness/range rule here; reconstruction must give the same result.
    for( auto &[position, body] : withheld ) {
        const auto wanted_position = position;
        const auto slot = std::find_if( MAPBUFFER.begin(), MAPBUFFER.end(),
        [&]( const auto &entry ) { return entry.first == wanted_position; } );
        REQUIRE( slot != MAPBUFFER.end() );
        REQUIRE( slot->second );
        body = std::move( slot->second );
    }
    here.load( old_origin + point( 20, 20 ), false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    const auto dark_far = read_current();
    const auto access_dark_far = access_reads.back();
    CHECK( dark_far.known == dark_near.known );
    CHECK( dark_far.visible == dark_near.visible );
    CHECK( access_dark_far.visibility.known == access_dark_near.visibility.known );
    CHECK( access_dark_far.visibility.visible == access_dark_near.visibility.visible );
    CHECK( access_dark_far.facts == access_dark_near.facts );

    // Exercise the real observer constructor's preflight: ordinary zzip::load
    // would repair this archive. An observation must instead refuse unchanged.
    REQUIRE( world_generator->active_world );
    WORLD &world = *world_generator->active_world;
    const bool old_compression = world.has_compression_enabled();
    const tripoint_abs_omt sky( footprint.front().xy(), OVERMAP_HEIGHT );
    const auto segment = project_to<coords::seg>( sky );
    const auto archive_path = ( PATH_INFO::current_dimension_save_path() / "maps" /
                               string_format( "%d.%d.%d.zzip", segment.x(), segment.y(), segment.z() ) ).get_unrelative_path();
    REQUIRE_FALSE( std::filesystem::exists( archive_path ) );
    on_out_of_scope restore_archive( [&]() {
        std::filesystem::remove( archive_path );
        CHECK( world.set_compression_enabled( old_compression ) );
    } );
    REQUIRE( world.set_compression_enabled( true ) );
    std::filesystem::create_directories( archive_path.parent_path() );
    const std::string corrupt_bytes = "not-an-archive";
    {
        std::ofstream out( archive_path, std::ios::binary );
        out << corrupt_bytes;
        REQUIRE( out.good() );
    }
    const auto corrupt_mtime = std::filesystem::last_write_time( archive_path );
    const auto corrupt_uid = g->assign_item_uid();
    const auto corrupt_count = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto corrupt_warning = capture_debugmsg_during( [&]() {
        const auto corrupt_read = read_current();
        // The actual trace proved lower=4 settles both native predicates even
        // with this sky input unknown. It is not an exact scalar or a license
        // to infer missing physical target geometry.
        CHECK( corrupt_read.known == dark_far.known );
        CHECK( corrupt_read.visible == dark_far.visible );
        CHECK( access_reads.back().visibility.known == access_dark_far.visibility.known );
        CHECK( access_reads.back().visibility.visible == access_dark_far.visibility.visible );
        CHECK( access_reads.back().facts == access_dark_far.facts );
        CHECK_FALSE( exact_lighting.back() );
    } );
    CHECK_FALSE( corrupt_warning.empty() );
    CHECK( ( g->assign_item_uid() == corrupt_uid + 1 ) );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == corrupt_count );
    std::ifstream input( archive_path, std::ios::binary );
    CHECK( std::string( std::istreambuf_iterator<char>( input ), {} ) == corrupt_bytes );
    CHECK( ( std::filesystem::last_write_time( archive_path ) == corrupt_mtime ) );
    std::filesystem::remove( archive_path );
    restore_upper();

    // Now remove lookup ownership of the genuinely necessary target submap,
    // retaining its exact native value. A corrupt target archive must refuse
    // both callers, without installing/recovering data or consuming UIDs.
    const auto target_sm = project_to<coords::sm>( target );
    const auto target_slot = std::find_if( MAPBUFFER.begin(), MAPBUFFER.end(), [&]( const auto &entry ) {
        return entry.first == target_sm;
    } );
    REQUIRE( target_slot != MAPBUFFER.end() );
    REQUIRE( target_slot->second );
    const auto source_bytes = sunlight_entry_bytes( *target_slot->second, target_sm );
    withheld.emplace_back( target_sm, std::move( target_slot->second ) );
    const auto target_segment = project_to<coords::seg>( project_to<coords::omt>( target ) );
    const auto target_archive = ( PATH_INFO::current_dimension_save_path() / "maps" /
                                 string_format( "%d.%d.%d.zzip", target_segment.x(), target_segment.y(),
                                         target_segment.z() ) ).get_unrelative_path();
    REQUIRE_FALSE( std::filesystem::exists( target_archive ) );
    on_out_of_scope remove_target_archive( [&]() { std::filesystem::remove( target_archive ); } );
    std::filesystem::create_directories( target_archive.parent_path() );
    {
        std::ofstream out( target_archive, std::ios::binary );
        out << corrupt_bytes;
        REQUIRE( out.good() );
    }
    const auto target_mtime = std::filesystem::last_write_time( target_archive );
    const auto target_uid = g->assign_item_uid();
    const auto target_count = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto target_warning = capture_debugmsg_during( [&]() {
        const auto refused = read_current();
        CHECK_FALSE( refused.known );
        CHECK_FALSE( refused.visible );
        CHECK_FALSE( access_reads.back().visibility.known );
        CHECK_FALSE( access_reads.back().visibility.visible );
        CHECK( access_reads.back().facts.empty() );
        CHECK_FALSE( exact_lighting.back() );
    } );
    CHECK_FALSE( target_warning.empty() );
    CHECK( ( g->assign_item_uid() == target_uid + 1 ) );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == target_count );
    CHECK_FALSE( MAPBUFFER.lookup_submap_cached( target_sm ) );
    CHECK( sunlight_entry_bytes( *withheld.back().second, target_sm ) == source_bytes );
    std::ifstream target_input( target_archive, std::ios::binary );
    CHECK( std::string( std::istreambuf_iterator<char>( target_input ), {} ) == corrupt_bytes );
    CHECK( ( std::filesystem::last_write_time( target_archive ) == target_mtime ) );
    std::filesystem::remove( target_archive );
    restore_upper();
    here.load( old_origin, false, false );
    here.ter_set( here.get_bub( fixed_observer + point( 7, 0 ) ), ter_str_id( "t_wall" ) );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    scout_observation::site_reader blocked_reader;
    const auto blocked_access = blocked_reader.read_access( *access_observer, target );
    CHECK( blocked_access.visibility.known );
    CHECK_FALSE( blocked_access.visibility.visible );
    CHECK( blocked_access.facts.empty() );
    access_observer->set_hp( 0 );
    REQUIRE( access_observer->is_dead_state() );
    const auto unavailable_access = blocked_reader.read_access( *access_observer, target );
    CHECK( unavailable_access.visibility.known );
    CHECK_FALSE( unavailable_access.visibility.visible );
    CHECK( unavailable_access.facts.empty() );
    std::cout << "REMOTE_LIGHTING_COST roof=" << roof << " actual_reads=" << read_calls
              << " read_wall_seconds=" << read_seconds << std::endl;
}

TEST_CASE( "native off-ray illumination contributes to remote physical access",
           "[binocular_056][off_ray_access_light]" )
{
    clear_avatar();
    clear_npcs();
    clear_map_with_vision( -1, 4, false );
    map &here = get_map();
    // The target-centred native source plane extends past the original main
    // map. Establish those real fixture cells before emission/body setup;
    // the comparison then changes only player/map context.
    map physical_inputs;
    physical_inputs.load( here.get_abs_sub() + point( 2, 0 ), false, false );
    wipe_map_terrain_with_vision( &physical_inputs, false );
    const auto old_origin = here.get_abs_sub();
    const auto old_player = get_avatar().pos_abs();
    const auto old_time = calendar::turn;
    on_out_of_scope restore( [&]() {
        here.load( old_origin, false, false );
        get_avatar().setpos( old_player, false );
        set_time( old_time );
    } );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    set_time( calendar::turn_zero );
    const auto origin = here.get_abs( tripoint_bub_ms( 12, 71, 0 ) );
    const auto target = here.get_abs( tripoint_bub_ms( 84, 71, 0 ) );
    const auto lamp = here.get_abs( tripoint_bub_ms( 84, 72, 0 ) );
    REQUIRE( project_to<coords::omt>( lamp ).y() != project_to<coords::omt>( target ).y() );
    monster *observer = g->place_critter_at( mtype_id( "mon_mi_go" ), here.get_bub( origin ) );
    REQUIRE( observer );
    on_out_of_scope remove_observer( [&]() { g->remove_zombie( *observer ); } );
    here.add_field( here.get_bub( lamp ), field_type_id( "fd_fire" ), 3 );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    const auto time = calendar::turn;
    const auto near = [&]() {
        scout_observation::site_reader reader;
        return reader.read_access( *observer, target );
    }();
    CAPTURE( here.ambient_light_at( here.get_bub( target ) ), near.visibility.reason );
    REQUIRE( near.visibility.known );
    REQUIRE( near.visibility.visible );
    REQUIRE_FALSE( near.facts.empty() );
    here.load( old_origin + point( 20, 20 ), false, false );
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    REQUIRE_FALSE( here.inbounds( origin ) );
    REQUIRE_FALSE( here.inbounds( target ) );
    const auto far = [&]() {
        scout_observation::site_reader reader;
        return reader.read_access( *observer, target );
    }();
    CAPTURE( far.visibility.reason );
    CHECK( far.visibility.known == near.visibility.known );
    CHECK( far.visibility.visible == near.visibility.visible );
    CHECK( far.facts == near.facts );
    CHECK( observer->pos_abs() == origin );
    CHECK( calendar::turn == time );
}

TEST_CASE( "observation uniform reconstruction requires authoritative metadata and clean absence",
           "[binocular_056][existing_geometry][observation_uniform_data]" )
{
    const bool compressed = GENERATE( false, true );
    CAPTURE( compressed );
    REQUIRE( world_generator->active_world );
    WORLD &world = *world_generator->active_world;
    const bool previous_compression = world.has_compression_enabled();
    const tripoint_abs_sm absent( 142, 142, 9 );
    const auto existing = absent + point( 2, 0 );
    const auto sibling = existing + point( 1, 0 );
    const auto corrupt = absent + point( 4, 0 );
    const auto other_level = absent + tripoint_rel_sm( 0, 0, -1 );
    const auto missing_level = absent + tripoint_rel_sm( 0, 0, -2 );
    const auto directory_for = [&]( const tripoint_abs_sm & p ) {
        const auto seg = project_to<coords::seg>( project_to<coords::omt>( p ) );
        return PATH_INFO::current_dimension_save_path() / "maps" /
               string_format( "%d.%d.%d", seg.x(), seg.y(), seg.z() );
    };
    const auto name_for = [&]( const tripoint_abs_sm & p ) {
        const auto omt = project_to<coords::omt>( p );
        return string_format( "%d.%d.%d.map", omt.x(), omt.y(), omt.z() );
    };
    std::map<tripoint_abs_omt, oter_id> previous_terrain;
    std::set<std::filesystem::path> paths;
    on_out_of_scope restore( [&]() {
        for( const auto &p : paths ) {
            std::filesystem::remove_all( p );
        }
        for( const auto &[position, terrain] : previous_terrain ) {
            overmap_buffer.ter_set( position, terrain );
        }
        CHECK( world.set_compression_enabled( previous_compression ) );
    } );
    REQUIRE( world.set_compression_enabled( compressed ) );
    for( const auto &p : {absent, existing, corrupt, other_level, missing_level} ) {
        const auto omt = project_to<coords::omt>( p );
        previous_terrain.emplace( omt, overmap_buffer.ter( omt ) );
        overmap_buffer.ter_set( omt, oter_id( "open_air" ) );
    }
    for( const auto &p : {absent, other_level, missing_level} ) {
        const auto dir = directory_for( p );
        REQUIRE_FALSE( std::filesystem::exists( dir.get_unrelative_path() ) );
        REQUIRE_FALSE( std::filesystem::exists( ( dir + ".zzip" ).get_unrelative_path() ) );
        paths.insert( dir.get_unrelative_path() );
        paths.insert( ( dir + ".zzip" ).get_unrelative_path() );
    }
    const auto global_count = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto before_time = calendar::turn;
    const auto player_position = get_avatar().pos_abs();
    const auto write_quad = [&]( const tripoint_abs_sm & p, const std::string & bytes ) {
        const auto dir = directory_for( p );
        std::filesystem::create_directories( dir.get_unrelative_path() );
        if( compressed ) {
            auto archive = zzip::load( ( dir + ".zzip" ).get_unrelative_path(),
                                      ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path() );
            REQUIRE( archive );
            REQUIRE( archive->add_file( name_for( p ), bytes ) );
        } else {
            std::ofstream out( ( dir / name_for( p ) ).get_unrelative_path() );
            out << bytes;
            REQUIRE( out.good() );
        }
    };
    const auto native_quad = [&]( const tripoint_abs_sm & p ) {
        submap stored;
        stored.set_all_ter( ter_str_id( "t_flat_roof" ) );
        std::ostringstream bytes;
        JsonOut js( bytes );
        js.start_array();
        js.start_object();
        js.member( "version", savegame_version );
        js.member( "coordinates", p.raw() );
        stored.store( js );
        js.end_object();
        js.end_array();
        return bytes.str();
    };
    mapbuffer data;
    {
        mapbuffer::existing_submap_reader reader( data );
        const auto check_uniform = [&]( const tripoint_abs_sm & p ) {
            auto value = reader.read_omitted_uniform( p );
            REQUIRE( value );
            CHECK( value->is_uniform() );
            CHECK( value->get_ter( point_sm_ms::zero ) == ter_str_id( "t_open_air" ) );
            CHECK( value->field_count == 0 );
            CHECK( value->vehicles.empty() );
            CHECK_FALSE( data.lookup_submap_cached( p ) );
            CHECK( std::distance( data.begin(), data.end() ) == 0 );
            CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == global_count );
        };
        check_uniform( absent ); // missing archive/plain directory
        check_uniform( missing_level );
    }
    overmap_buffer.ter_set( project_to<coords::omt>( absent ), oter_id( "forest" ) );
    REQUIRE_FALSE( oter_id( "forest" )->get_uniform_terrain() );
    {
        mapbuffer::existing_submap_reader reader( data );
        CHECK_FALSE( reader.read_omitted_uniform( absent ) );
    }
    overmap_buffer.ter_set( project_to<coords::omt>( absent ), oter_id() );
    {
        mapbuffer::existing_submap_reader reader( data );
        CHECK_FALSE( reader.read_omitted_uniform( absent ) );
    }
    overmap_buffer.ter_set( project_to<coords::omt>( absent ), oter_id( "open_air" ) );
    write_quad( existing, native_quad( existing ) );
    write_quad( corrupt, "not-json" );
    if( compressed ) {
        CHECK( zzip::has_file_checked( ( directory_for( existing ) + ".zzip" ).get_unrelative_path(),
                                      name_for( existing ) ) == true );
        CHECK( zzip::has_file_checked( ( directory_for( existing ) + ".zzip" ).get_unrelative_path(),
                                      name_for( absent ) ) == false );
    }
    {
        mapbuffer::existing_submap_reader reader( data );
        auto uniform = reader.read_omitted_uniform( absent );
        REQUIRE( uniform ); // checked missing quad in an otherwise present archive
        CHECK( uniform->get_ter( point_sm_ms::zero ) == ter_str_id( "t_open_air" ) );
        CHECK_FALSE( reader.read_omitted_uniform( existing ) );
        CHECK_FALSE( reader.read_omitted_uniform( sibling ) ); // conservative partial quad
        CHECK_FALSE( reader.read_omitted_uniform( corrupt ) ); // no hiding invalid override
        const auto warning = capture_debugmsg_during( [&]() {
            CHECK_FALSE( reader.lookup( corrupt ) );
        } );
        CHECK_FALSE( warning.empty() );
        submap *override = reader.lookup( existing );
        REQUIRE( override );
        CHECK( override->get_ter( point_sm_ms::zero ) == ter_str_id( "t_flat_roof" ) );
        CHECK_FALSE( reader.read_omitted_uniform( existing ) );
        CHECK( data.lookup_submap_cached( existing ) == override );
        CHECK( std::distance( data.begin(), data.end() ) == 1 );
    }
    // A real resident value wins independently of an absent serialized quad.
    auto resident = std::make_unique<submap>();
    resident->set_all_ter( ter_str_id( "t_wall" ) );
    submap *resident_identity = resident.get();
    REQUIRE( data.add_submap( absent, resident ) );
    {
        mapbuffer::existing_submap_reader reader( data );
        CHECK_FALSE( reader.read_omitted_uniform( absent ) );
        CHECK( reader.lookup( absent ) == resident_identity );
    }
    const auto damaged = compressed ? directory_for( other_level ) + ".zzip" :
                         directory_for( other_level ) / name_for( other_level );
    std::filesystem::create_directories( damaged.get_unrelative_path().parent_path() );
    if( compressed ) {
        const auto read_bytes = [&]() {
            std::ifstream input( damaged.get_unrelative_path(), std::ios::binary );
            return std::string( std::istreambuf_iterator<char>( input ), {} );
        };
        const auto write_bytes = [&]( const std::string &bytes ) {
            std::ofstream out( damaged.get_unrelative_path(), std::ios::binary | std::ios::trunc );
            out.write( bytes.data(), bytes.size() );
            REQUIRE( out.good() );
        };
        // The native footer encoder produces both a valid empty index and
        // deliberately malformed structures with a genuinely matching checksum.
        const auto archive_with_footer = []( const std::vector<uint8_t> &footer ) {
            std::array<uint64_t, 2> metadata;
            MEM_writeLE64( &metadata[0], footer.size() );
            MEM_writeLE64( &metadata[1], XXH64( footer.data(), footer.size(), 0x1337C0DE ) );
            std::string bytes( ZSTD_SKIPPABLEHEADERSIZE + sizeof( metadata ), '\0' );
            REQUIRE( ZSTD_writeSkippableFrame( bytes.data(), bytes.size(), metadata.data(),
                                              sizeof( metadata ), 15 ) == bytes.size() );
            bytes.append( reinterpret_cast<const char *>( footer.data() ), footer.size() );
            return bytes;
        };
        flexbuffers::Builder valid_builder;
        const auto root = valid_builder.StartMap();
        const auto entries = valid_builder.StartMap( "entries" );
        valid_builder.EndMap( entries );
        const auto meta = valid_builder.StartMap( "meta" );
        valid_builder.UInt( "content_end", 24 );
        valid_builder.UInt( "total_content_size", 0 );
        valid_builder.EndMap( meta );
        valid_builder.EndMap( root );
        valid_builder.Finish();
        const auto valid = archive_with_footer( valid_builder.GetBuffer() );
        CHECK( zzip::has_file_checked( damaged.get_unrelative_path(), "missing.map" ) == std::nullopt );
        CHECK_FALSE( std::filesystem::exists( damaged.get_unrelative_path() ) );
        write_bytes( valid );
        REQUIRE( zzip::has_file_checked( damaged.get_unrelative_path(), "missing.map" ) == false );
        std::vector<std::pair<std::string, std::string>> broken;
        broken.emplace_back( "garbage", "not-an-archive" );
        broken.emplace_back( "truncated", valid.substr( 0, valid.size() - 1 ) );
        auto bad_checksum = valid;
        bad_checksum.back() ^= 1;
        broken.emplace_back( "wrong-checksum", bad_checksum );
        auto bad_length = valid;
        MEM_writeLE64( bad_length.data() + ZSTD_SKIPPABLEHEADERSIZE, valid.size() + 1 );
        broken.emplace_back( "footer-bounds", bad_length );
        flexbuffers::Builder wrong_shape;
        wrong_shape.UInt( 7 );
        wrong_shape.Finish();
        broken.emplace_back( "footer-not-map", archive_with_footer( wrong_shape.GetBuffer() ) );
        flexbuffers::Builder missing_entries;
        const auto empty_map = missing_entries.StartMap();
        missing_entries.EndMap( empty_map );
        missing_entries.Finish();
        broken.emplace_back( "missing-native-members", archive_with_footer( missing_entries.GetBuffer() ) );
        auto invalid_offset = valid_builder.GetBuffer();
        const auto root_width = invalid_offset.back();
        std::fill_n( invalid_offset.end() - 2 - root_width, root_width, 0xff );
        broken.emplace_back( "footer-invalid-offset", archive_with_footer( invalid_offset ) );
        for( const auto &[reason, bytes] : broken ) {
            CAPTURE( reason );
            write_bytes( bytes );
            const auto before_bytes = read_bytes();
            const auto before_mtime = std::filesystem::last_write_time( damaged.get_unrelative_path() );
            CHECK( zzip::has_file_checked( damaged.get_unrelative_path(), "missing.map" ) == std::nullopt );
            mapbuffer::existing_submap_reader reader( data );
            bool read_error = false;
            const auto warning = capture_debugmsg_during( [&]() {
                CHECK_FALSE( reader.read_omitted_uniform( other_level, &read_error ) );
            } );
            CHECK( read_error );
            CHECK_FALSE( warning.empty() );
            CHECK( read_bytes() == before_bytes );
            CHECK( ( std::filesystem::last_write_time( damaged.get_unrelative_path() ) == before_mtime ) );
        }
    } else {
        // An existing non-regular path is not evidence of absent terrain.
        REQUIRE( std::filesystem::create_directory( damaged.get_unrelative_path() ) );
        mapbuffer::existing_submap_reader reader( data );
        const auto warning = capture_debugmsg_during( [&]() {
            CHECK_FALSE( reader.read_omitted_uniform( other_level ) );
        } );
        CHECK_FALSE( warning.empty() );
    }
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == global_count );
    CHECK( calendar::turn == before_time );
    CHECK( get_avatar().pos_abs() == player_position );
}

TEST_CASE( "checked archive footer refuses missing required native members",
           "[binocular_056][observation_footer_required]" )
{
    const int malformed = GENERATE( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 );
    CAPTURE( malformed );
    const auto path = ( PATH_INFO::current_dimension_save_path() /
                        "observation-required-footer.zzip" ).get_unrelative_path();
    REQUIRE_FALSE( std::filesystem::exists( path ) );
    on_out_of_scope cleanup( [&]() { std::filesystem::remove( path ); } );
    flexbuffers::Builder builder;
    const auto root = builder.StartMap();
    const auto entries = builder.StartMap( "entries" );
    if( malformed >= 3 ) {
        if( malformed == 5 ) {
            builder.UInt( "present.map", 0 );
        } else {
            const auto entry = builder.StartMap( "present.map" );
            if( malformed != 3 ) {
                if( malformed == 11 ) {
                    builder.String( "offset", "24" );
                } else {
                    builder.UInt( "offset", malformed == 7 ? ( uint64_t( 1 ) << 32 ) + 24 : 24 );
                }
            }
            if( malformed != 4 ) {
                if( malformed == 12 ) {
                    builder.Int( "len", -1 );
                } else {
                    builder.UInt( "len", 0 );
                }
            }
            builder.EndMap( entry );
        }
    }
    builder.EndMap( entries );
    const auto meta = builder.StartMap( "meta" );
    if( malformed != 0 && malformed != 2 ) {
        if( malformed == 8 ) {
            builder.Int( "content_end", -1 );
        } else if( malformed == 9 ) {
            builder.Bool( "content_end", true );
        } else {
            builder.UInt( "content_end", malformed == 6 ? UINT64_MAX : 24 );
        }
    }
    if( malformed != 0 && malformed != 1 ) {
        if( malformed == 10 ) {
            builder.String( "total_content_size", "0" );
        } else {
            builder.UInt( "total_content_size", 0 );
        }
    }
    builder.EndMap( meta );
    builder.EndMap( root );
    builder.Finish();
    const auto &footer = builder.GetBuffer();
    std::array<uint64_t, 2> metadata;
    MEM_writeLE64( &metadata[0], footer.size() );
    MEM_writeLE64( &metadata[1], XXH64( footer.data(), footer.size(), 0x1337C0DE ) );
    std::string bytes( ZSTD_SKIPPABLEHEADERSIZE + sizeof( metadata ), '\0' );
    REQUIRE( ZSTD_writeSkippableFrame( bytes.data(), bytes.size(), metadata.data(),
                                      sizeof( metadata ), 15 ) == bytes.size() );
    bytes.append( reinterpret_cast<const char *>( footer.data() ), footer.size() );
    {
        std::ofstream out( path, std::ios::binary );
        out << bytes;
        REQUIRE( out.good() );
    }
    const auto mtime = std::filesystem::last_write_time( path );
    CHECK( zzip::has_file_checked( path, "missing.map" ) == std::nullopt );
    std::ifstream input( path, std::ios::binary );
    CHECK( std::string( std::istreambuf_iterator<char>( input ), {} ) == bytes );
    CHECK( ( std::filesystem::last_write_time( path ) == mtime ) );
}

TEST_CASE( "uniform observation refuses damaged cold overmap without loading or repair",
           "[binocular_056][observation_cold_owner]" )
{
    REQUIRE( world_generator->active_world );
    WORLD &world = *world_generator->active_world;
    const bool old_compression = world.has_compression_enabled();
    const point_abs_om owner( 37, 41 );
    const tripoint_abs_omt requested( project_to<coords::omt>( owner ) + point( 4, 5 ),
                                     OVERMAP_HEIGHT );
    const auto path = ( PATH_INFO::current_dimension_save_path() / "overmaps" /
                        ( overmapbuffer::terrain_filename( owner ) + ".zzip" ) ).get_unrelative_path();
    REQUIRE_FALSE( overmap_buffer.find_loaded_overmap( owner ) );
    REQUIRE_FALSE( std::filesystem::exists( path ) );
    on_out_of_scope cleanup( [&]() {
        std::filesystem::remove( path );
        CHECK( world.set_compression_enabled( old_compression ) );
    } );
    REQUIRE( world.set_compression_enabled( true ) );
    std::filesystem::create_directories( path.parent_path() );
    const std::string bytes = "broken-cold-overmap";
    {
        std::ofstream out( path, std::ios::binary );
        out << bytes;
        REQUIRE( out.good() );
    }
    const auto mtime = std::filesystem::last_write_time( path );
    const auto turn = calendar::turn;
    mapbuffer data;
    mapbuffer::existing_submap_reader reader( data );
    capture_debugmsg_during( [&]() {
        CHECK_FALSE( reader.read_omitted_uniform( project_to<coords::sm>( requested ) ) );
    } );
    CHECK_FALSE( overmap_buffer.find_loaded_overmap( owner ) );
    std::ifstream input( path, std::ios::binary );
    CHECK( std::string( std::istreambuf_iterator<char>( input ), {} ) == bytes );
    CHECK( ( std::filesystem::last_write_time( path ) == mtime ) );
    CHECK( calendar::turn == turn );
}

TEST_CASE( "cold terrain projection reads native layers without native owner side effects",
           "[binocular_056][observation_cold_projection]" )
{
    const bool compressed = GENERATE( false, true );
    CAPTURE( compressed );
    REQUIRE( world_generator->active_world );
    WORLD &world = *world_generator->active_world;
    const bool old_compression = world.has_compression_enabled();
    const point_abs_om owner( 38, 42 );
    const std::string name = overmapbuffer::terrain_filename( owner );
    const auto plain = ( PATH_INFO::current_dimension_save_path() / name ).get_unrelative_path();
    const auto packed = ( PATH_INFO::current_dimension_save_path() / "overmaps" /
                          ( name + ".zzip" ) ).get_unrelative_path();
    REQUIRE_FALSE( std::filesystem::exists( plain ) );
    REQUIRE_FALSE( std::filesystem::exists( packed ) );
    REQUIRE_FALSE( overmap_buffer.find_loaded_overmap( owner ) );
    on_out_of_scope cleanup( [&]() {
        std::filesystem::remove( plain );
        std::filesystem::remove( packed );
        CHECK( world.set_compression_enabled( old_compression ) );
    } );
    REQUIRE( world.set_compression_enabled( compressed ) );
    const auto path = compressed ? packed : plain;
    std::filesystem::create_directories( path.parent_path() );
    const tripoint_om_omt local( 4, 5, 9 );
    const tripoint_abs_omt target = project_combine( owner, local );
    auto stored = std::make_unique<overmap>( owner ); // fixture only: never installed/published
    stored->ter_set( local, oter_id( "open_air" ) );
    const auto body_at = project_to<coords::ms>( target );
    monster original( mtype_id( "mon_zombie" ) );
    original.add_item( item( itype_id( "rock" ) ) );
    REQUIRE( stored->hordes.spawn_entity( body_at, original ) );
    std::ostringstream saved;
    stored->serialize( saved ); // genuine native format, includes full body/item payload
    size_t last_entry_byte = 0;
    const auto write = [&]( const std::string &bytes ) {
        if( compressed ) {
            auto archive = zzip::load( path,
                                      ( PATH_INFO::world_base_save_path() / "overmaps.dict" ).get_unrelative_path() );
            REQUIRE( archive );
            REQUIRE( archive->add_file( std::filesystem::u8path( name ), bytes ) );
            for( const auto &entry : archive->get_layout() ) {
                if( entry.path == std::filesystem::u8path( name ) ) {
                    REQUIRE( entry.len > 0 );
                    last_entry_byte = entry.offset + entry.len - 1;
                }
            }
        } else {
            std::ofstream out( path, std::ios::binary | std::ios::trunc );
            out << bytes;
            REQUIRE( out.good() );
        }
    };
    const auto file_bytes = [&]() {
        std::ifstream input( path, std::ios::binary );
        return std::string( std::istreambuf_iterator<char>( input ), {} );
    };
    const auto dimension = [&]() {
        std::ostringstream bytes;
        JsonOut json( bytes );
        overmap_buffer.global_state.serialize( json );
        return bytes.str();
    };
    write( saved.str() );
    const auto original_file = file_bytes();
    const auto original_mtime = std::filesystem::last_write_time( path );
    // Two consecutive public allocations bracket each read. The control itself
    // consumes one UID; a hidden full-body decode/copy would create a gap.
    int64_t last_uid = g->assign_item_uid();
    const auto check_uid_unchanged = [&]() {
        const int64_t next = g->assign_item_uid();
        CHECK( next == last_uid + 1 );
        last_uid = next;
    };
    const auto original_dimension = dimension();
    const auto original_rng = rng_get_engine();
    const auto original_time = calendar::turn;
    const int original_loading_version = savegame_loading_version;
    overmapbuffer cold;
    {
        overmapbuffer::existing_terrain_reader reader( cold );
        const auto first_start = std::chrono::steady_clock::now();
        REQUIRE( reader.read( target ) == oter_id( "open_air" ) );
        const auto warm_start = std::chrono::steady_clock::now();
        for( int repeat = 0; repeat < 3; ++repeat ) {
            REQUIRE( reader.read( target ) == oter_id( "open_air" ) );
        }
        const auto finish = std::chrono::steady_clock::now();
        std::cout << "REMOTE_LIGHTING_COLD compressed=" << compressed
                  << " saved_entry_bytes=" << saved.str().size()
                  << " first_read_wall_seconds="
                  << std::chrono::duration<double>( warm_start - first_start ).count()
                  << " repeated_reads=3 repeated_read_wall_seconds="
                  << std::chrono::duration<double>( finish - warm_start ).count() << std::endl;
        CHECK_FALSE( cold.find_loaded_overmap( owner ) );
    }
    {
        mapbuffer data;
        mapbuffer::existing_submap_reader reader( data );
        const auto first = project_to<coords::sm>( target );
        for( const point offset : { point( 0, 0 ), point( 0, 1 ), point( 1, 0 ), point( 1, 1 ) } ) {
            auto uniform = reader.read_omitted_uniform( first + offset );
            REQUIRE( uniform );
            CHECK( uniform->get_ter( point_sm_ms::zero ) == ter_str_id( "t_open_air" ) );
            CHECK_FALSE( data.lookup_submap_cached( first + offset ) );
        }
        CHECK( std::distance( data.begin(), data.end() ) == 0 );
    }
    CHECK_FALSE( overmap_buffer.find_loaded_overmap( owner ) );
    CHECK( file_bytes() == original_file );
    CHECK( ( std::filesystem::last_write_time( path ) == original_mtime ) );
    check_uid_unchanged();
    CHECK( dimension() == original_dimension );
    CHECK( rng_get_engine() == original_rng );
    CHECK( calendar::turn == original_time );
    CHECK( savegame_loading_version == original_loading_version );

    if( compressed ) {
        // Keep the footer genuine/valid and damage only this actual native
        // entry. A checked index is not evidence that its payload is readable.
        REQUIRE( last_entry_byte < original_file.size() );
        auto damaged = original_file;
        damaged[last_entry_byte] ^= 1;
        const auto write_raw = [&]( const std::string &bytes ) {
            std::ofstream out( path, std::ios::binary | std::ios::trunc );
            out << bytes;
            REQUIRE( out.good() );
        };
        write_raw( damaged );
        const auto damaged_mtime = std::filesystem::last_write_time( path );
        {
            auto archive = zzip::checked_reader::open( path );
            REQUIRE( archive );
            CHECK( archive->has_file( std::filesystem::u8path( name ) ) );
            CHECK_FALSE( archive->get_file( std::filesystem::u8path( name ),
                                           ( PATH_INFO::world_base_save_path() / "overmaps.dict" ).get_unrelative_path() ) );
            overmapbuffer::existing_terrain_reader reader( cold );
            capture_debugmsg_during( [&]() { CHECK_FALSE( reader.read( target ) ); } );
        }
        CHECK( file_bytes() == damaged );
        CHECK( ( std::filesystem::last_write_time( path ) == damaged_mtime ) );
        CHECK_FALSE( cold.find_loaded_overmap( owner ) );
        check_uid_unchanged();
        CHECK( dimension() == original_dimension );
        CHECK( rng_get_engine() == original_rng );
        CHECK( calendar::turn == original_time );
        CHECK( savegame_loading_version == original_loading_version );
        write_raw( original_file );
    }

    // Same scope never spans file changes; new scope sees a new native value.
    stored->ter_set( local, oter_id( "forest" ) );
    std::ostringstream changed;
    stored->serialize( changed );
    write( changed.str() );
    {
        overmapbuffer::existing_terrain_reader reader( cold );
        REQUIRE( reader.read( target ) == oter_id( "forest" ) );
    }
    std::vector<std::pair<std::string, std::string>> malformed = {
        { "invalid-json", "{broken" }, { "missing-layers", "{}" },
        { "wrong-layer-shape", "{\"layers\":[7]}" }
    };
    const auto rle_layers = [&]( const std::string &run ) {
        std::ostringstream result;
        result << "# version 1\n{\"layers\":[";
        for( int z = 0; z < OVERMAP_LAYERS; ++z ) {
            result << ( z ? "," : "" ) << "[" << run << "]";
        }
        result << "]}";
        return result.str();
    };
    malformed.emplace_back( "zero-run", rle_layers( "[\"open_air\",0]" ) );
    malformed.emplace_back( "oversize-run", rle_layers( "[\"open_air\",999999999]" ) );
    malformed.emplace_back( "short-layer", rle_layers( "[\"open_air\",1]" ) );
    malformed.emplace_back( "unknown-terrain", rle_layers( "[\"not_a_native_oter\",32400]" ) );
    REQUIRE( overmap::is_oter_id_obsolete( "horse_farm_1" ) );
    malformed.emplace_back( "obsolete-terrain", rle_layers( "[\"horse_farm_1\",32400]" ) );
    std::string future_version = rle_layers( "[\"open_air\",32400]" );
    future_version.replace( 0, std::string( "# version 1" ).size(),
                            "# version " + std::to_string( savegame_version + 1 ) );
    malformed.emplace_back( "unknown-version", future_version );
    for( const auto &[reason, bytes] : malformed ) {
        CAPTURE( reason );
        write( bytes );
        const auto before_file = file_bytes();
        const auto before_mtime = std::filesystem::last_write_time( path );
        overmapbuffer::existing_terrain_reader reader( cold );
        capture_debugmsg_during( [&]() {
            CHECK_FALSE( reader.read( target ) );
        } );
        CHECK_FALSE( cold.find_loaded_overmap( owner ) );
        CHECK( file_bytes() == before_file );
        CHECK( ( std::filesystem::last_write_time( path ) == before_mtime ) );
        check_uid_unchanged();
        CHECK( dimension() == original_dimension );
        CHECK( rng_get_engine() == original_rng );
        CHECK( calendar::turn == original_time );
        CHECK( savegame_loading_version == original_loading_version );
    }
    std::filesystem::remove( path );
    {
        overmapbuffer::existing_terrain_reader reader( cold );
        CHECK_FALSE( reader.read( target ) );
        CHECK_FALSE( std::filesystem::exists( path ) );
    }
    // Real resident terrain beats even damaged backing, without inspecting it.
    const auto resident_target = project_to<coords::omt>( get_map().get_abs_sub() );
    REQUIRE( overmap_buffer.find_loaded_overmap( project_to<coords::om>( resident_target.xy() ) ) );
    overmapbuffer::existing_terrain_reader resident( overmap_buffer );
    CHECK( resident.read( resident_target ) == overmap_buffer.ter( resident_target ) );
}
