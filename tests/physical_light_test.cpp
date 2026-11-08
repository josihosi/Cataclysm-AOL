#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>

#include "sounds.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "do_turn.h"
#include "debug.h"
#include "effect.h"
#include "field_type.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "mapdata.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "monster_helpers.h"
#include "monster.h"
#include "mtype.h"
#include "overmapbuffer.h"
#include "overmap.h"
#include "options_helpers.h"
#include "path_info.h"
#include "json_loader.h"
#include "physical_light.h"
#include "player_helpers.h"
#include "pocket_type.h"
#include "vehicle.h"
#include "veh_type.h"
#include "weather_type.h"
#include "worldfactory.h"

namespace
{
class scoped_transition_environment
{
    public:
        scoped_transition_environment() :
            previous_event_path( get( "OPENCLAW_HARNESS_TRANSITION_EVENT_PATH" ) ),
            previous_run_id( get( "OPENCLAW_HARNESS_RUN_ID" ) ),
            previous_semantic_run_id( get( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID" ) ),
            previous_semantic_trace_path( get( "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH" ) ) {}

        ~scoped_transition_environment() {
            restore( "OPENCLAW_HARNESS_TRANSITION_EVENT_PATH", previous_event_path );
            restore( "OPENCLAW_HARNESS_RUN_ID", previous_run_id );
            restore( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", previous_semantic_run_id );
            restore( "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", previous_semantic_trace_path );
        }

        void set_semantic( const std::string &run_id ) const {
            assign( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run_id.c_str() );
            assign( "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", nullptr );
        }

        void set( const std::string &event_path, const std::string &run_id ) const {
            assign( "OPENCLAW_HARNESS_TRANSITION_EVENT_PATH", event_path.c_str() );
            assign( "OPENCLAW_HARNESS_RUN_ID", run_id.c_str() );
        }

    private:
        static std::optional<std::string> get( const char *name ) {
            if( const char *value = std::getenv( name ) ) {
                return value;
            }
            return std::nullopt;
        }

        static void assign( const char *name, const char *value ) {
#if defined(_WIN32)
            _putenv_s( name, value ? value : "" );
#else
            if( value ) {
                setenv( name, value, 1 );
            } else {
                unsetenv( name );
            }
#endif
        }

        static void restore( const char *name, const std::optional<std::string> &value ) {
            assign( name, value ? value->c_str() : nullptr );
        }

        std::optional<std::string> previous_event_path;
        std::optional<std::string> previous_run_id;
        std::optional<std::string> previous_semantic_run_id;
        std::optional<std::string> previous_semantic_trace_path;
};

TEST_CASE( "resident light queries preserve exact seams without loading saved owners",
           "[physical_light][hordes][resident_history]" )
{
    clear_avatar();
    clear_map_without_vision();
    overmap_buffer.clear();
    std::vector<std::filesystem::path> created_files;
    on_out_of_scope cleanup( [&] {
        clear_map_without_vision();
        overmap_buffer.clear();
        for( const auto &path : created_files ) {
            std::filesystem::remove( path );
        }
    } );
    const auto save_blank = [&]( const point_abs_om & address ) {
        const auto terrain = world_generator->active_world->has_compression_enabled() ?
                             PATH_INFO::current_dimension_save_path() / zzip_overmap_directory /
                             overmapbuffer::terrain_filename( address ) + zzip_suffix :
                             PATH_INFO::current_dimension_save_path() /
                             overmapbuffer::terrain_filename( address );
        if( std::filesystem::exists( terrain.get_unrelative_path() ) ) {
            return;
        }
        created_files.push_back( terrain.get_unrelative_path() );
        created_files.push_back( overmapbuffer::player_filename( address ).get_unrelative_path() );
        auto empty = std::make_unique<overmap>( address );
        empty->save();
    };
    std::set<point_abs_om> addresses;
    for( int x = -1; x <= 0; ++x ) {
        for( int y = -1; y <= 0; ++y ) {
            const point_abs_om address( x, y );
            save_blank( address );
            overmap_buffer.get( address ).hordes.clear();
            addresses.insert( address );
        }
    }
    // An actual serialized owner exists here, but the resident-only query
    // must leave it unopened, just like a genuinely missing key.
    const point_abs_om saved_only( 1, 1 );
    save_blank( saved_only );
    REQUIRE( overmap_buffer.find_loaded_overmap( saved_only ) == nullptr );
    const auto map_count = overmap_buffer.global_state.overmap_count;
    const auto found = overmap_buffer.get_loaded_overmaps_near( point_abs_sm( -1, -1 ), 1 );
    std::set<point_abs_om> actual;
    for( const overmap *owner : found ) {
        actual.insert( owner->pos() );
    }
    CHECK( actual == addresses );
    CHECK( found.size() == actual.size() );
    REQUIRE( found.size() == 4 );
    CHECK( found.front()->pos() == point_abs_om( -1, -1 ) );
    // Nearest remains nearest. Equal-distance entries have no extra actor,
    // target or cardinal-direction priority in the native query contract.
    CHECK( std::is_sorted( found.begin(), found.end(), []( const overmap * a, const overmap * b ) {
        return trig_dist( point_abs_om( -1, -1 ), a->pos() ) <
               trig_dist( point_abs_om( -1, -1 ), b->pos() );
    } ) );
    const auto exact_negative = overmap_buffer.get_loaded_overmaps_near( point_abs_sm( -1, -1 ), 0 );
    REQUIRE( exact_negative.size() == 1 );
    CHECK( exact_negative.front()->pos() == point_abs_om( -1, -1 ) );
    const auto exact_zero = overmap_buffer.get_loaded_overmaps_near( point_abs_sm(), 0 );
    REQUIRE( exact_zero.size() == 1 );
    CHECK( exact_zero.front()->pos() == point_abs_om() );
    CHECK( overmap_buffer.get_loaded_overmaps_near(
               coords::project_to<coords::sm>( saved_only ), 0 ).empty() );
    CHECK( overmap_buffer.get_loaded_overmaps_near(
               coords::project_to<coords::sm>( point_abs_om( 2, 2 ) ), 0 ).empty() );
    CHECK( overmap_buffer.find_loaded_overmap( saved_only ) == nullptr );

    const tripoint_abs_ms source( 24, 24, 0 );
    const tripoint_abs_ms near( 36, 24, 0 );
    const tripoint_abs_ms blind( 24, 36, 0 );
    const tripoint_abs_ms friendly( 36, 36, 0 );
    overmap_buffer.ter_set( coords::project_to<coords::omt>( source ), oter_id( "field" ) );
    overmap &owner = *overmap_buffer.find_loaded_overmap( point_abs_om() );
    REQUIRE( owner.hordes.spawn_entity( near, mtype_id( "mon_zombie" ) ) );
    monster blind_body( mtype_id( "mon_zombie" ) );
    blind_body.add_effect( efftype_id( "blind" ), 1_hours );
    REQUIRE( owner.hordes.spawn_entity( blind, blind_body ) );
    monster friendly_body( mtype_id( "mon_zombie" ) );
    friendly_body.friendly = 1;
    REQUIRE( owner.hordes.spawn_entity( friendly, friendly_body ) );
    const auto time = calendar::turn;
    int candidates = 0;
    CHECK( overmap_buffer.attract_hordes_to_light( source, 6, 4, 40, true,
            "resident-history-light", calendar::turn, &candidates ) == 1 );
    CHECK( candidates == 3 );
    REQUIRE( overmap_buffer.entity_at( near ) != nullptr );
    CHECK( overmap_buffer.entity_at( near )->destination == source );
    CHECK( overmap_buffer.entity_at( near )->light_sample_id == "resident-history-light" );
    CHECK( overmap_buffer.entity_at( blind )->light_sample_id.empty() );
    CHECK( overmap_buffer.entity_at( friendly )->light_sample_id.empty() );
    const auto expiry = overmap_buffer.entity_at( near )->light_expires;
    CHECK( overmap_buffer.attract_hordes_to_light( source, 6, 4, 40, true,
            "resident-history-light", calendar::turn ) == 0 );
    CHECK( overmap_buffer.entity_at( near )->light_expires == expiry );
    CHECK( overmap_buffer.global_state.overmap_count == map_count );
    CHECK( calendar::turn == time );
    CHECK( overmap_buffer.find_loaded_overmap( saved_only ) == nullptr );
}

item powered_flashlight( const int charges = 56 )
{
    item battery( itype_id( "medium_battery_cell" ) );
    battery.ammo_set( battery.ammo_default(), charges );
    item light( itype_id( "flashlight_on" ) );
    light.put_in( battery, pocket_type::MAGAZINE_WELL );
    return light;
}

void setup_dark_native_lightmap()
{
    clear_avatar();
    clear_map_without_vision();
    calendar::turn = calendar::turn_zero;
    get_avatar().recalc_sight_limits();
}

void rebuild_native_lightmap()
{
    map &here = get_map();
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
}
}

TEST_CASE( "physical_light_staggered_discovery_rechecks_sources_without_full_sweeps",
           "[physical_light][staggered_signals]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms lamp( SEEX - 1, SEEY, 1 );
    const tripoint_bub_ms smoke( SEEX, SEEY, 1 );
    const tripoint_abs_ms lamp_abs = here.get_abs( lamp );
    const tripoint_abs_ms smoke_abs = here.get_abs( smoke );
    const field_type_id smoke_type = field_type_str_id( "fd_smoke" );
    here.i_clear( lamp );
    here.remove_field( smoke, smoke_type );
    on_out_of_scope cleanup( [&]() {
        here.i_clear( here.get_bub( lamp_abs ) );
        here.remove_field( here.get_bub( smoke_abs ), smoke_type );
    } );
    here.add_item( lamp, powered_flashlight() );
    here.add_field( smoke, smoke_type, 2 );
    physical_light::loaded_source_sampler sampler;
    const auto has_lamp = [&lamp_abs]( const physical_light::loaded_z_source_index & index ) {
        return std::count_if( index.item_emitters.begin(), index.item_emitters.end(),
        [&lamp_abs]( const physical_light::emitter & source ) {
            return source.position == lamp_abs;
        } );
    };
    const auto has_smoke = [&smoke_abs]( const physical_light::loaded_z_source_index & index ) {
        return std::count_if( index.field_sources.begin(), index.field_sources.end(),
        [&smoke_abs]( const physical_light::loaded_z_source_index::field_source & source ) {
            return source.position == smoke_abs && source.smoke_intensity == 2;
        } );
    };
    const auto full = physical_light::index_loaded_z_sources( guy, here );
    REQUIRE( has_lamp( full ) == 1 );
    REQUIRE( has_smoke( full ) == 1 );
    const int period = physical_light::loaded_source_sampler::discovery_interval_turns;
    // Actual source counts bound the recheck portion, rather than a timing
    // assertion tied to the test machine's speed or a fake collector.
    const int known_bound = full.item_emitters.size() + full.stationary_emitters.size() +
                            full.field_sources.size();
    const int tile_bound = ( full.tiles_examined + period - 1 ) / period + known_bound + 1;
    bool lamp_seen = false;
    bool smoke_seen = false;
    for( int turn = 0; turn < period; ++turn ) {
        const auto index = sampler.sample( guy, here, turn );
        CAPTURE( turn, index.tiles_examined, tile_bound );
        CHECK( index.tiles_examined <= tile_bound );
        CHECK( has_lamp( index ) <= 1 );
        lamp_seen = lamp_seen || has_lamp( index ) == 1;
        smoke_seen = smoke_seen || has_smoke( index ) == 1;
    }
    REQUIRE( lamp_seen );
    REQUIRE( smoke_seen );
    CHECK( has_lamp( sampler.sample( guy, here, period ) ) == 1 );
    CHECK( has_smoke( sampler.sample( guy, here, period + 1 ) ) == 1 );

    // Crossing onto and away from a known ground source does not lose or
    // duplicate it through the carried-item collection path.
    const tripoint_bub_ms original = guy.pos_bub();
    guy.setpos( here, lamp );
    CHECK( has_lamp( sampler.sample( guy, here, period + 2 ) ) == 1 );
    guy.setpos( here, original );
    CHECK( has_lamp( sampler.sample( guy, here, period + 3 ) ) == 1 );
    here.i_clear( lamp );
    here.remove_field( smoke, smoke_type );
    const auto extinguished = sampler.sample( guy, here, period + 4 );
    CHECK( has_lamp( extinguished ) == 0 );
    CHECK( has_smoke( extinguished ) == 0 );
    sampler.reset();
    const auto reset = sampler.sample( guy, here, 0 );
    CHECK( has_lamp( reset ) == 0 );
    CHECK( has_smoke( reset ) == 0 );
}

TEST_CASE( "physical_light_discovery_uses_absolute_positions_across_bubble_shifts",
           "[physical_light][staggered_signals]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_abs_ms source = here.get_abs( guy.pos_bub() + tripoint::east );
    here.add_item( here.get_bub( source ), powered_flashlight() );
    physical_light::loaded_source_sampler sampler;
    const auto count = [&source]( const physical_light::loaded_z_source_index & index ) {
        return std::count_if( index.item_emitters.begin(), index.item_emitters.end(),
        [&source]( const physical_light::emitter & emitter ) {
            return emitter.position == source;
        } );
    };
    int turn = 0;
    while( turn < physical_light::loaded_source_sampler::discovery_interval_turns &&
           count( sampler.sample( guy, here, turn ) ) == 0 ) {
        ++turn;
    }
    REQUIRE( turn < physical_light::loaded_source_sampler::discovery_interval_turns );
    here.shift( point_rel_sm::east );
    on_out_of_scope cleanup( [&]() {
        here.i_clear( here.get_bub( source ) );
        here.shift( point_rel_sm::west );
    } );
    CHECK( count( sampler.sample( guy, here, turn + 1 ) ) == 1 );
    here.i_clear( here.get_bub( source ) );
    CHECK( count( sampler.sample( guy, here, turn + 2 ) ) == 0 );
}

TEST_CASE( "physical_light_source_scan_benchmark", "[.][physical_light][signal_benchmark]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    physical_light::loaded_source_sampler sampler;
    const auto measure = []( const auto & scan, const int turns ) {
        std::int64_t total_us = 0;
        std::int64_t maximum_us = 0;
        std::int64_t tiles = 0;
        for( int turn = 0; turn < turns; ++turn ) {
            const auto start = std::chrono::steady_clock::now();
            const auto sources = scan( turn );
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now() - start ).count();
            total_us += elapsed;
            maximum_us = std::max<std::int64_t>( maximum_us, elapsed );
            tiles += sources.tiles_examined;
        }
        std::cout << " turns=" << turns << " total_us=" << total_us
                  << " mean_us=" << total_us / turns << " max_us=" << maximum_us
                  << " tiles=" << tiles << '\n';
    };
    std::cout << "full_source_scan";
    measure( [&]( int ) {
        return physical_light::index_loaded_z_sources( guy, here );
    }, 20 );
    std::cout << "staggered_source_scan";
    measure( [&]( int turn ) {
        return sampler.sample( guy, here, turn );
    }, physical_light::loaded_source_sampler::discovery_interval_turns );
}

TEST_CASE( "physical_light_collects_carried_and_stationary_sources_independently",
           "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    here.add_item( origin + tripoint::east, powered_flashlight() );

    item_location carried = guy.try_add( powered_flashlight() );
    REQUIRE( carried != item_location::nowhere );
    const int before = carried->ammo_remaining();
    const std::vector<physical_light::emitter> emitters =
        physical_light::collect_item_emitters( guy, here, 3 );

    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::carried_item;
    } ) == 1 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::ground_item;
    } ) == 1 );
    CHECK( carried->ammo_remaining() == before );
}

TEST_CASE( "physical_light_loaded_z_index_finds_remote_loaded_level_and_excludes_depleted_source",
           "[physical_light][continuity]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms remote_loaded_source = guy.pos_bub() + tripoint_rel_ms( 4, 0, 1 );
    REQUIRE( here.inbounds( remote_loaded_source ) );
    const tripoint_bub_ms depleted_source = guy.pos_bub() + tripoint_rel_ms( 5, 0, 1 );
    // clear_map_without_vision() intentionally clears only through z=0; this
    // test owns z=1 items and must remove them for repeated Catch fuzz runs.
    here.i_clear( remote_loaded_source );
    here.i_clear( depleted_source );
    on_out_of_scope source_cleanup( [&here, remote_loaded_source, depleted_source]() {
        here.i_clear( remote_loaded_source );
        here.i_clear( depleted_source );
    } );
    here.add_item( remote_loaded_source, powered_flashlight() );

    item depleted( itype_id( "flashlight_on" ) );
    item empty_battery( itype_id( "medium_battery_cell" ) );
    depleted.put_in( empty_battery, pocket_type::MAGAZINE_WELL );
    here.add_item( depleted_source, depleted );

    const physical_light::loaded_z_source_index index =
        physical_light::index_loaded_z_sources( guy, here );
    const tripoint_abs_ms remote_abs = here.get_abs( remote_loaded_source );
    const tripoint_abs_ms depleted_abs = here.get_abs( depleted_source );
    CHECK( std::count_if( index.item_emitters.begin(), index.item_emitters.end(),
    [&remote_abs]( const physical_light::emitter & emitter ) {
        return emitter.position == remote_abs;
    } ) == 1 );
    CHECK( std::none_of( index.item_emitters.begin(), index.item_emitters.end(),
    [&depleted_abs]( const physical_light::emitter & emitter ) {
        return emitter.position == depleted_abs;
    } ) );
}

TEST_CASE( "physical_light_loaded_z_index_tracks_beside_onto_and_away_movement",
           "[physical_light][continuity]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    const tripoint_bub_ms beside = origin + tripoint::east;
    const tripoint_bub_ms away = origin + tripoint::west;
    const tripoint_abs_ms fixture_abs = here.get_abs( origin );
    ter_t &terrain = const_cast<ter_t &>( here.ter( origin ).obj() );
    restore_on_out_of_scope terrain_restore( terrain.light_emitted );
    on_out_of_scope fixture_cleanup( [&here, &origin]() {
        here.i_clear( origin );
    } );
    terrain.light_emitted = 31;
    here.add_item( origin, powered_flashlight() );
    guy.setpos( here, beside );

    const auto beside_index = physical_light::index_loaded_z_sources( guy, here );
    const auto check_fixture = [&fixture_abs]( const physical_light::loaded_z_source_index & index ) {
        const auto terrain_source = std::find_if( index.stationary_emitters.begin(),
        index.stationary_emitters.end(), [&fixture_abs]( const physical_light::emitter & emitter ) {
            return emitter.kind == physical_light::source_kind::terrain &&
                   emitter.position == fixture_abs;
        } );
        REQUIRE( terrain_source != index.stationary_emitters.end() );
        CHECK( terrain_source->provenance.rfind( "terrain:", 0 ) == 0 );
        CHECK( terrain_source->luminance == 31 );
        return std::count_if( index.item_emitters.begin(), index.item_emitters.end(),
        [&fixture_abs]( const physical_light::emitter & emitter ) {
            return emitter.kind == physical_light::source_kind::ground_item &&
                   emitter.position == fixture_abs && emitter.provenance == "flashlight_on";
        } );
    };
    const auto check_both_discovery_routes = [&here, &origin, &fixture_abs,
           &check_fixture]( const physical_light::loaded_z_source_index & index ) {
        CHECK( check_fixture( index ) == 1 );
        const auto selected = physical_light::collect_stationary_emitters( here, origin, 0 );
        const auto terrain_source = std::find_if( selected.begin(), selected.end(),
        [&fixture_abs]( const physical_light::emitter & emitter ) {
            return emitter.kind == physical_light::source_kind::terrain &&
                   emitter.position == fixture_abs;
        } );
        REQUIRE( terrain_source != selected.end() );
        CHECK( terrain_source->provenance.rfind( "terrain:", 0 ) == 0 );
        CHECK( terrain_source->luminance == 31 );
    };
    check_both_discovery_routes( beside_index );

    // Stand directly on the shared fixture: the old carrier-tile continue
    // dropped terrain and the lamp's map enumeration at exactly this point.
    guy.setpos( here, origin );
    const auto onto = physical_light::index_loaded_z_sources( guy, here );
    check_both_discovery_routes( onto );

    guy.setpos( here, away );
    const auto away_index = physical_light::index_loaded_z_sources( guy, here );
    check_both_discovery_routes( away_index );
}

TEST_CASE( "physical_light_loaded_z_index_keeps_player_tile_stationary_sources_and_restores_fixtures",
           "[physical_light][continuity]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    ter_t &terrain = const_cast<ter_t &>( here.ter( origin ).obj() );
    furn_t &furniture = const_cast<furn_t &>( here.furn( origin ).obj() );
    const int original_terrain_light = terrain.light_emitted;
    const int original_furniture_light = furniture.light_emitted;
    {
        restore_on_out_of_scope terrain_restore( terrain.light_emitted );
        restore_on_out_of_scope furniture_restore( furniture.light_emitted );
        terrain.light_emitted = 31;
        furniture.light_emitted = 37;
        const auto index = physical_light::index_loaded_z_sources( guy, here );
        CHECK( std::count_if( index.stationary_emitters.begin(), index.stationary_emitters.end(),
        [&here, &origin]( const physical_light::emitter & emitter ) {
            return emitter.position == here.get_abs( origin ) &&
                   ( emitter.kind == physical_light::source_kind::terrain ||
                     emitter.kind == physical_light::source_kind::furniture );
        } ) == 2 );
    }
    CHECK( terrain.light_emitted == original_terrain_light );
    CHECK( furniture.light_emitted == original_furniture_light );
    const auto throw_with_fixture = [&terrain, &furniture]() {
        restore_on_out_of_scope terrain_restore( terrain.light_emitted );
        restore_on_out_of_scope furniture_restore( furniture.light_emitted );
        terrain.light_emitted = 41;
        furniture.light_emitted = 43;
        throw std::runtime_error( "fixture restoration probe" );
    };
    try {
        throw_with_fixture();
    } catch( const std::runtime_error & ) {
    }
    CHECK( terrain.light_emitted == original_terrain_light );
    CHECK( furniture.light_emitted == original_furniture_light );
}

TEST_CASE( "physical_light_samples_once_per_real_advancing_turn",
           "[physical_light][continuity]" )
{
    physical_light::turn_sample_gate gate;
    // Non-advancing callers (render, inventory, debug, and scheduler probes)
    // cannot turn a current state query into a physical observation.
    CHECK_FALSE( gate.begin_advancing_turn( 40, false ) );
    CHECK( gate.begin_advancing_turn( 40, true ) );
    CHECK_FALSE( gate.begin_advancing_turn( 40, true ) );
    // Every distinct real turn is admitted, including turns between the old
    // five-minute maintenance boundaries.  Re-entry in that same turn is a
    // no-op, and a timeline rewind starts a fresh sequence.
    CHECK_FALSE( gate.begin_advancing_turn( 41, false ) );
    CHECK( gate.begin_advancing_turn( 41, true ) );
    CHECK( gate.begin_advancing_turn( 42, true ) );
    CHECK_FALSE( gate.begin_advancing_turn( 42, true ) );
    CHECK( gate.begin_advancing_turn( 7, true ) );
}

TEST_CASE( "production light delivery preserves real attraction with compact diagnostics",
           "[physical_light][hordes][diagnostic_writes]" )
{
    clear_avatar();
    clear_map_without_vision();
    reset_live_light_sample_cache();
    restore_on_out_of_scope<time_point> restore_clock( calendar::turn );
    const auto dark = night_time( calendar::turn ) + 2_hours;
    const auto ticks = to_turns<int>( dark - calendar::turn_zero );
    calendar::turn = calendar::turn_zero + time_duration::from_turns( ( ticks / 300 + 1 ) * 300 );
    REQUIRE( is_night( calendar::turn ) );
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    const tripoint_bub_ms source = get_avatar().pos_bub() + tripoint( 6, 0, 0 );
    const tripoint_abs_ms source_abs = here.get_abs( source );
    overmap &om = overmap_buffer.get( coords::project_to<coords::om>( source_abs ).xy() );
    restore_on_out_of_scope<horde_map> restore_hordes( om.hordes );
    om.hordes.clear();
    for( int x = 0; x < 3; ++x ) {
        here.add_field( source + tripoint( x * 2, 0, 0 ), field_type_id( "fd_fire" ), 3 );
    }
    // The production sampler always checks the carrier tile; all other tiles
    // have a 900-phase discovery schedule. Establish these three real sources
    // through that owner, not a fabricated packet or injected sampler cache.
    for( int x = 0; x < 2; ++x ) {
        get_avatar().setpos( here, source + tripoint( x * 2, 0, 0 ), false );
        run_live_light_delivery_for_test();
        calendar::turn += 1_turns;
    }
    get_avatar().setpos( here, source + tripoint( 4, 0, 0 ), false );
    const auto near = source_abs + point_rel_ms( 24, 0 );
    const auto blind = source_abs + point_rel_ms( 0, 24 );
    const auto friendly = source_abs + point_rel_ms( 24, 12 );
    for( const auto &pos : { source_abs, near, blind, friendly } ) {
        overmap_buffer.ter_set( coords::project_to<coords::omt>( pos ), oter_id( "field" ) );
    }
    REQUIRE( om.hordes.spawn_entity( near, mtype_id( "mon_zombie" ) ) );
    monster blind_body( mtype_id( "mon_zombie" ) );
    blind_body.add_effect( efftype_id( "blind" ), 1_hours );
    REQUIRE( om.hordes.spawn_entity( blind, blind_body ) );
    monster friendly_body( mtype_id( "mon_zombie" ) );
    friendly_body.friendly = 1;
    REQUIRE( om.hordes.spawn_entity( friendly, friendly_body ) );

    std::ostringstream captured;
    std::ostream &log = DebugLog( D_INFO, DC_ALL );
    auto *const original = log.rdbuf( captured.rdbuf() );
    on_out_of_scope restore_log( [&] { log.rdbuf( original ); } );
    run_live_light_delivery_for_test();
    const auto &samples = live_light::samples_for_turn( calendar::turn );
    const auto packets = std::count_if( samples.begin(), samples.end(), []( const auto & sample ) {
        return sample.horde_signal_power > 0;
    } );
    REQUIRE( packets > 1 );
    auto *attracted = overmap_buffer.entity_at( near );
    REQUIRE( attracted );
    CHECK_FALSE( attracted->light_sample_id.empty() );
    CHECK( attracted->destination == attracted->light_source );
    CHECK( attracted->light_expires > calendar::turn );
    CHECK( std::any_of( samples.begin(), samples.end(), [&]( const auto & sample ) {
        return sample.sample_id == attracted->light_sample_id && sample.source_ms &&
               *sample.source_ms == attracted->light_source;
    } ) );
    REQUIRE( overmap_buffer.entity_at( blind ) );
    CHECK( overmap_buffer.entity_at( blind )->light_sample_id.empty() );
    REQUIRE( overmap_buffer.entity_at( friendly ) );
    CHECK( overmap_buffer.entity_at( friendly )->light_sample_id.empty() );
    const auto text = captured.str();
    const std::string marker = "bandit_live_world horde light interest:";
    std::size_t records = 0;
    for( auto at = text.find( marker ); at != std::string::npos; at = text.find( marker, at + 1 ) ) {
        ++records;
    }
    CHECK( records == 1 );
    CHECK( text.find( "aggregate=yes packets=" + std::to_string( packets ) ) != std::string::npos );
    CHECK( text.find( "current_light=yes light_interest=" ) != std::string::npos );
    CHECK( text.find( "attracted_entities=0" ) == std::string::npos );
    const auto sample_id = attracted->light_sample_id;
    const auto expires = attracted->light_expires;
    run_live_light_delivery_for_test();
    CHECK( captured.str() == text );
    CHECK( overmap_buffer.entity_at( near )->light_sample_id == sample_id );
    CHECK( overmap_buffer.entity_at( near )->light_expires == expires );
    for( int x = 0; x < 3; ++x ) {
        here.remove_field( source + tripoint( x * 2, 0, 0 ), field_type_id( "fd_fire" ) );
    }
    calendar::turn += 1_turns;
    run_live_light_delivery_for_test();
    CHECK( overmap_buffer.entity_at( near )->light_sample_id == sample_id );
    CHECK( overmap_buffer.entity_at( near )->light_expires == expires );
    CHECK_FALSE( debug_has_error_been_observed() );
}

TEST_CASE( "production channel audit batches identical bound packet records",
           "[physical_light][diagnostic_writes][channel_output]" )
{
    clear_avatar();
    clear_map_without_vision();
    reset_live_light_sample_cache();
    restore_on_out_of_scope<time_point> restore_clock( calendar::turn );
    const auto ticks = to_turns<int>( calendar::turn - calendar::turn_zero );
    calendar::turn = calendar::turn_zero + time_duration::from_turns( ( ticks / 300 + 1 ) * 300 );
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    const auto source = get_avatar().pos_bub();
    here.add_field( source, field_type_id( "fd_fire" ), 3 );
    here.add_field( source, field_type_id( "fd_smoke" ), 2 );
    run_live_light_delivery_for_test();
    const auto &samples = live_light::samples_for_turn( calendar::turn );
    REQUIRE_FALSE( samples.empty() );

    const auto path = std::filesystem::path( PATH_INFO::user_dir() ) / "channel-output-control.jsonl";
    std::filesystem::remove_all( path );
    const std::map<std::string, std::string> settings = {
        { "OPENCLAW_HARNESS_R008_CHANNEL_PATH", path.string() },
        { "OPENCLAW_HARNESS_RUN_ID", "channel-output-control" },
        { "OPENCLAW_HARNESS_SCENARIO", "physical-light-control" },
        { "OPENCLAW_HARNESS_RUNTIME_SOURCE_SHA256", std::string( 64, '1' ) },
        { "OPENCLAW_HARNESS_EXECUTABLE_SHA256", std::string( 64, '2' ) },
        { "OPENCLAW_HARNESS_BINDING_ID", std::string( 64, '3' ) }
    };
    const auto assign = []( const std::string & key, const std::optional<std::string> &value ) {
#if defined(_WIN32)
        _putenv_s( key.c_str(), value ? value->c_str() : "" );
#else
        if( value ) {
            setenv( key.c_str(), value->c_str(), 1 );
        } else {
            unsetenv( key.c_str() );
        }
#endif
    };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &[key, value] : settings ) {
        const char *const old = std::getenv( key.c_str() );
        previous[key] = old ? std::optional<std::string>( old ) : std::nullopt;
        assign( key, value );
    }
    on_out_of_scope restore_environment( [&] {
        for( const auto &[key, value] : previous ) {
            assign( key, value );
        }
    } );
    const auto before = live_channel_output_statistics_for_test();
    process_overmap_npc_move_for_test();
    const auto first = live_channel_output_statistics_for_test();
    REQUIRE( first.records > before.records + 1 );
    CHECK( first.open_calls == before.open_calls + 1 );
    CHECK( first.flush_calls == before.flush_calls + 1 );
    CHECK( first.logical_bytes == std::filesystem::file_size( path ) );
    CHECK_FALSE( first.write_failed );
    std::ifstream input( path );
    std::string line;
    std::size_t count = 0;
    std::set<std::string> observed_samples;
    while( std::getline( input, line ) ) {
        const auto object = json_loader::from_string( line ).get_object();
        object.allow_omitted_members();
        CHECK( object.get_int( "sequence" ) == static_cast<int>( ++count ) );
        CHECK( object.get_string( "schema" ) == "caol-r008-production-channel-v1" );
        CHECK( object.get_string( "run_id" ) == "channel-output-control" );
        CHECK( object.get_string( "scan_id" ) == "channel-output-control:1" );
        const auto binding = object.get_object( "binding" );
        binding.allow_omitted_members();
        CHECK( binding.get_string( "runtime_source_sha256" ) == std::string( 64, '1' ) );
        CHECK( binding.get_string( "executable_sha256" ) == std::string( 64, '2' ) );
        CHECK( binding.get_string( "binding_id" ) == std::string( 64, '3' ) );
        CHECK( binding.get_string( "scenario_id" ) == "physical-light-control" );
        if( object.get_bool( "observed" ) && object.get_string( "signal_origin" ) == "local_field" ) {
            CHECK( object.get_bool( "sampled" ) );
            observed_samples.insert( object.get_string( "detail" ) );
        }
    }
    CHECK( count == first.records - before.records );
    for( const auto &sample : samples ) {
        if( sample.mark.kind == "smoke" || sample.mark.kind == "light" || sample.mark.kind == "searchlight" ) {
            CHECK( observed_samples.count( sample.mark.mark_id ) == 1 );
        }
    }
    input.close();
    process_overmap_npc_move_for_test();
    const auto second = live_channel_output_statistics_for_test();
    CHECK( second.records == first.records + count );
    CHECK( second.open_calls == first.open_calls + 1 );
    CHECK( second.flush_calls == first.flush_calls + 1 );
    CHECK( second.logical_bytes == std::filesystem::file_size( path ) );
    {
        std::ifstream appended( path );
        std::string last;
        while( std::getline( appended, line ) ) {
            last = line;
        }
        const auto object = json_loader::from_string( last ).get_object();
        object.allow_omitted_members();
        CHECK( object.get_int( "sequence" ) == static_cast<int>( second.records ) );
        CHECK( object.get_string( "scan_id" ) == "channel-output-control:2" );
    }
    sounds::sound( get_avatar().pos_bub(), 120, sounds::sound_t::combat, "gunfire", false,
                   "", "default", sounds::significant_sound_t::gunfire );
    CHECK( observe_live_bandit_sounds_for_test() == 1 );
    {
        std::ifstream appended( path );
        int optical_rows = 0;
        while( std::getline( appended, line ) ) {
            const auto object = json_loader::from_string( line ).get_object();
            object.allow_omitted_members();
            if( object.get_string( "scan_id" ) == "channel-output-control:3" &&
                ( object.get_string( "channel" ) == "smoke" ||
                  object.get_string( "channel" ) == "light" ) ) {
                ++optical_rows;
                CHECK_FALSE( object.get_bool( "sampled" ) );
                CHECK_FALSE( object.get_bool( "observed" ) );
                CHECK( object.get_string( "detail" ) == "not_sampled" );
            }
        }
        CHECK( optical_rows == 2 );
    }
    here.remove_field( source, field_type_id( "fd_fire" ) );
    here.remove_field( source, field_type_id( "fd_smoke" ) );
    calendar::turn += 5_minutes;
    reset_live_light_sample_cache();
    run_live_light_delivery_for_test();
    process_overmap_npc_move_for_test();
    const auto absent_stats = live_channel_output_statistics_for_test();
    {
        std::ifstream appended( path );
        int optical_rows = 0;
        while( std::getline( appended, line ) ) {
            const auto object = json_loader::from_string( line ).get_object();
            object.allow_omitted_members();
            if( object.get_string( "scan_id" ) == "channel-output-control:4" &&
                ( object.get_string( "channel" ) == "smoke" ||
                  object.get_string( "channel" ) == "light" ) ) {
                ++optical_rows;
                CHECK( object.get_bool( "sampled" ) );
                CHECK_FALSE( object.get_bool( "observed" ) );
                CHECK( object.get_string( "detail" ) == "absent" );
            }
        }
        CHECK( optical_rows == 2 );
    }
    const auto bytes = std::filesystem::file_size( path );
    assign( "OPENCLAW_HARNESS_RUN_ID", "foreign-channel-run" );
    process_overmap_npc_move_for_test();
    CHECK( std::filesystem::file_size( path ) == bytes );
    CHECK( live_channel_output_statistics_for_test().open_calls == absent_stats.open_calls );
    assign( "OPENCLAW_HARNESS_RUN_ID", "channel-output-control" );
    assign( "OPENCLAW_HARNESS_BINDING_ID", std::nullopt );
    process_overmap_npc_move_for_test();
    CHECK( std::filesystem::file_size( path ) == bytes );
    CHECK( live_channel_output_statistics_for_test().open_calls == absent_stats.open_calls );
    assign( "OPENCLAW_HARNESS_BINDING_ID", std::string( 64, '3' ) );
    // A failed open disables this exact audit stream. Recovery must not append
    // a partial or foreign successor behind a previously failed writer.
    const auto retained = path.string() + ".consumed";
    std::filesystem::rename( path, retained );
    std::filesystem::create_directory( path );
    process_overmap_npc_move_for_test();
    const auto failed = live_channel_output_statistics_for_test();
    CHECK( failed.write_failed );
    CHECK( failed.records == absent_stats.records );
    CHECK( failed.open_calls == absent_stats.open_calls + 1 );
    CHECK( failed.flush_calls == absent_stats.flush_calls );
    std::filesystem::remove( path );
    process_overmap_npc_move_for_test();
    CHECK_FALSE( std::filesystem::exists( path ) );
    CHECK( live_channel_output_statistics_for_test().open_calls == failed.open_calls );
    std::filesystem::rename( retained, path );
    CHECK_FALSE( debug_has_error_been_observed() );
}

TEST_CASE( "physical_light_production_facade_records_real_delivery_order",
           "[physical_light][continuity][production_order]" )
{
    clear_avatar();
    clear_map_without_vision();
    reset_live_light_sample_cache();
    run_live_light_delivery_for_test();
    CHECK( live_light_sample_is_current_for_test() );
    CHECK( live_light_delivery_trace_for_test() == std::vector<live_light_delivery_stage> {
        live_light_delivery_stage::rider_reconciliation,
        live_light_delivery_stage::rider_memory_aging,
        live_light_delivery_stage::fresh_sampling,
        live_light_delivery_stage::horde_delivery,
        live_light_delivery_stage::stalker_delivery,
        live_light_delivery_stage::rider_delivery,
        live_light_delivery_stage::cannibal_delivery,
        live_light_delivery_stage::migo_delivery,
    } );
    // Re-entry in the same real turn does not resample.  The packet remains
    // current for the later cadence owner, without a query/redraw refresh.
    run_live_light_delivery_for_test();
    CHECK( live_light_sample_is_current_for_test() );
    CHECK( live_light_delivery_trace_for_test().empty() );
    run_live_light_staffed_observer_for_test();
    CHECK( live_light_sample_is_current_for_test() );
    CHECK( live_light_delivery_trace_for_test().back() ==
           live_light_delivery_stage::staffed_observer_recording );
}

TEST_CASE( "live_light_discovers_each_advancing_turn_without_same_turn_replay",
           "[physical_light][continuity][production_order]" )
{
    live_light::reset();
    int reconciliations = 0;
    int memory_aging = 0;
    int discoveries = 0;
    int deliveries = 0;
    std::vector<live_bandit_signal_observation> cannibal_packets;
    live_light::callbacks callbacks;
    callbacks.reconcile_riders = [&reconciliations]() {
        ++reconciliations;
    };
    callbacks.age_rider_memory = [&memory_aging]() {
        ++memory_aging;
    };
    callbacks.discover = [&discoveries]() {
        ++discoveries;
        live_bandit_signal_observation sample;
        sample.has_light_projection = true;
        sample.mark.kind = "light";
        sample.sample_id = "cadence-fixture";
        return std::vector<live_bandit_signal_observation> { sample };
    };
    const auto count_delivery = [&deliveries]( const std::vector<live_bandit_signal_observation> & ) {
        ++deliveries;
    };
    callbacks.deliver_hordes = count_delivery;
    callbacks.deliver_stalkers = count_delivery;
    callbacks.deliver_riders = count_delivery;
    callbacks.deliver_cannibals = [&cannibal_packets](
    const std::vector<live_bandit_signal_observation> &samples ) {
        cannibal_packets.insert( cannibal_packets.end(), samples.begin(), samples.end() );
    };

    const time_point first_turn = calendar::turn_zero + 100_turns;
    live_light::run_advancing_turn( first_turn, callbacks );
    CHECK( reconciliations == 1 );
    CHECK( memory_aging == 1 );
    CHECK( discoveries == 1 );
    CHECK( deliveries == 3 );
    REQUIRE( cannibal_packets.size() == 1 );
    CHECK( cannibal_packets.back().sample_id == "cadence-fixture" );
    REQUIRE( live_light::samples_for_turn( first_turn ).size() == 1 );

    // A duplicate call in one real turn is a no-op, but the next advancing
    // turn must discover and deliver independently; this is intentionally not
    // the staffed observer's five-minute cadence.
    live_light::run_advancing_turn( first_turn, callbacks );
    CHECK( reconciliations == 1 );
    CHECK( memory_aging == 1 );
    CHECK( discoveries == 1 );
    CHECK( deliveries == 3 );
    CHECK( cannibal_packets.size() == 1 );
    CHECK( live_light::delivery_trace().empty() );

    const time_point second_turn = first_turn + 1_turns;
    live_light::run_advancing_turn( second_turn, callbacks );
    CHECK( reconciliations == 2 );
    CHECK( memory_aging == 2 );
    CHECK( discoveries == 2 );
    CHECK( deliveries == 6 );
    CHECK( cannibal_packets.size() == 2 );
    CHECK( live_light::sample_is_current( second_turn ) );
    CHECK( live_light::delivery_trace() == std::vector<live_light_delivery_stage> {
        live_light_delivery_stage::rider_reconciliation,
        live_light_delivery_stage::rider_memory_aging,
        live_light_delivery_stage::fresh_sampling,
        live_light_delivery_stage::horde_delivery,
        live_light_delivery_stage::stalker_delivery,
        live_light_delivery_stage::rider_delivery,
        live_light_delivery_stage::cannibal_delivery,
        live_light_delivery_stage::migo_delivery,
    } );
}

TEST_CASE( "live_light_routes_smoke_to_cannibals_but_keeps_optics_light_only",
           "[physical_light][continuity][smoke_first]" )
{
    live_light::reset();
    std::vector<std::vector<live_bandit_signal_observation>> optical_packets;
    std::vector<std::vector<live_bandit_signal_observation>> cannibal_packets;
    live_light::callbacks callbacks;
    callbacks.reconcile_riders = []() {};
    callbacks.age_rider_memory = []() {};
    const auto capture_optical = [&optical_packets](
    const std::vector<live_bandit_signal_observation> &samples ) {
        optical_packets.push_back( samples );
    };
    callbacks.deliver_hordes = capture_optical;
    callbacks.deliver_stalkers = capture_optical;
    callbacks.deliver_riders = capture_optical;
    callbacks.deliver_cannibals = [&cannibal_packets](
    const std::vector<live_bandit_signal_observation> &samples ) {
        cannibal_packets.push_back( samples );
    };

    const tripoint_abs_omt smoke_source( 135, 137, 0 );
    const auto smoke = [&smoke_source]( const std::string & id ) {
        live_bandit_signal_observation sample;
        sample.mark.mark_id = id;
        sample.mark.kind = "smoke";
        sample.mark.source_omt = smoke_source;
        sample.source_omt = smoke_source;
        sample.range_cap_omt = 8;
        sample.sample_id = id + "#100";
        sample.observed_turn = 100;
        sample.observed_minutes = 5;
        return sample;
    };
    const auto light = []() {
        live_bandit_signal_observation sample;
        sample.mark.mark_id = "light-mark";
        sample.mark.kind = "light";
        sample.sample_id = "light-mark#101";
        sample.has_light_projection = true;
        sample.observed_turn = 101;
        sample.observed_minutes = 5;
        return sample;
    };
    const time_point first_turn = calendar::turn_zero + 100_turns;
    callbacks.discover = [&smoke]() {
        return std::vector<live_bandit_signal_observation> { smoke( "smoke-first" ) };
    };
    live_light::run_advancing_turn( first_turn, callbacks );
    REQUIRE( cannibal_packets.size() == 1 );
    REQUIRE( cannibal_packets.back().size() == 1 );
    CHECK( cannibal_packets.back().front().mark.kind == "smoke" );
    CHECK( cannibal_packets.back().front().source_omt == smoke_source );
    CHECK( cannibal_packets.back().front().sample_id == "smoke-first#100" );
    CHECK( cannibal_packets.back().front().observed_turn == 100 );
    CHECK( cannibal_packets.back().front().observed_minutes == 5 );
    CHECK( optical_packets.empty() );

    callbacks.discover = [&light]() {
        return std::vector<live_bandit_signal_observation> { light() };
    };
    live_light::run_advancing_turn( first_turn + 1_turns, callbacks );
    REQUIRE( cannibal_packets.size() == 2 );
    REQUIRE( cannibal_packets.back().size() == 1 );
    CHECK( cannibal_packets.back().front().mark.kind == "light" );
    REQUIRE( optical_packets.size() == 3 );
    CHECK( optical_packets.back().front().mark.kind == "light" );

    callbacks.discover = [&smoke, &light]() {
        return std::vector<live_bandit_signal_observation> { smoke( "mixed-smoke" ), light() };
    };
    live_light::run_advancing_turn( first_turn + 2_turns, callbacks );
    REQUIRE( cannibal_packets.size() == 3 );
    REQUIRE( cannibal_packets.back().size() == 2 );
    CHECK( cannibal_packets.back().front().mark.kind == "smoke" );
    CHECK( cannibal_packets.back().back().mark.kind == "light" );
    REQUIRE( optical_packets.size() == 6 );
    for( const std::vector<live_bandit_signal_observation> &packet : optical_packets ) {
        REQUIRE( packet.size() == 1 );
        CHECK( packet.front().mark.kind == "light" );
    }

    live_light::run_advancing_turn( first_turn + 2_turns, callbacks );
    CHECK( cannibal_packets.size() == 3 );
    CHECK( optical_packets.size() == 6 );
}

TEST_CASE( "live_light_delivers_empty_current_samples_to_known_migo_owners_once",
           "[physical_light][continuity]" )
{
    live_light::reset();
    live_light::callbacks callbacks;
    callbacks.reconcile_riders = []() {};
    callbacks.age_rider_memory = []() {};
    callbacks.discover = []() { return std::vector<live_bandit_signal_observation>(); };
    int optical = 0;
    const auto count_optical = [&]( const std::vector<live_bandit_signal_observation> & ) { ++optical; };
    callbacks.deliver_hordes = count_optical;
    callbacks.deliver_stalkers = count_optical;
    callbacks.deliver_riders = count_optical;
    callbacks.deliver_cannibals = count_optical;
    int migo = 0;
    callbacks.deliver_migo = [&]( const std::vector<live_bandit_signal_observation> &samples ) {
        CHECK( samples.empty() );
        ++migo;
    };
    const auto turn = calendar::turn_zero + 500_turns;
    live_light::run_advancing_turn( turn, callbacks );
    CHECK( migo == 1 );
    CHECK( optical == 0 );
    live_light::run_advancing_turn( turn, callbacks );
    CHECK( migo == 1 );
    live_light::run_advancing_turn( turn + 1_turns, callbacks );
    CHECK( migo == 2 );
    CHECK( optical == 0 );
    live_light::reset();
}

TEST_CASE( "live_light_raw_source_presence_is_current_and_independent_of_packet_threshold",
           "[physical_light][continuity][raid_sweep]" )
{
    live_light::reset();
    live_light::callbacks callbacks;
    callbacks.reconcile_riders = []() {};
    callbacks.age_rider_memory = []() {};
    callbacks.deliver_hordes = []( const auto & ) {};
    callbacks.deliver_stalkers = []( const auto & ) {};
    callbacks.deliver_riders = []( const auto & ) {};
    callbacks.deliver_cannibals = []( const auto & ) {};
    const tripoint_abs_ms source( 20, 30, 0 );
    const auto turn = calendar::turn_zero + 600_turns;
    callbacks.discover = [&]() {
        physical_light::loaded_z_source_index index;
        index.field_sources.push_back( {source, 0, 1} );
        index.item_emitters.push_back( {source, physical_light::source_kind::ground_item, 1, false, "fixture"} );
        live_light::record_loaded_sources( turn, index );
        return std::vector<live_bandit_signal_observation>(); // no qualifying packet
    };
    live_light::run_advancing_turn( turn, callbacks );
    CHECK( live_light::samples_for_turn( turn ).empty() );
    CHECK( live_light::source_present_for_turn( turn, source, "smoke" ) == true );
    CHECK( live_light::source_present_for_turn( turn, source, "light" ) == true );
    CHECK_FALSE( live_light::source_present_for_turn( turn + 1_turns, source, "light" ) );
    callbacks.discover = [&]() {
        physical_light::loaded_z_source_index absent;
        absent.examined_positions.push_back( source );
        live_light::record_loaded_sources( turn + 1_turns, absent );
        return std::vector<live_bandit_signal_observation>();
    };
    live_light::run_advancing_turn( turn + 1_turns, callbacks );
    REQUIRE( live_light::source_present_for_turn( turn + 1_turns, source, "light" ) );
    CHECK( *live_light::source_present_for_turn( turn + 1_turns, source, "light" ) == false );
    CHECK( *live_light::source_present_for_turn( turn + 1_turns, source, "smoke" ) == false );
    CHECK_FALSE( live_light::source_present_for_turn( turn, source, "light" ) );
    callbacks.discover = [&]() {
        live_light::record_loaded_sources( turn + 2_turns, {} );
        return std::vector<live_bandit_signal_observation>();
    };
    live_light::run_advancing_turn( turn + 2_turns, callbacks );
    CHECK_FALSE( live_light::source_present_for_turn( turn + 2_turns, source, "light" ) );
    CHECK_FALSE( live_light::source_present_for_turn( turn + 2_turns, source, "smoke" ) );
    live_light::reset();
}

TEST_CASE( "production_smoke_signal_records_staffed_camp_leads_once_and_keeps_failures",
           "[physical_light][smoke_first][dispatch]" )
{
    restore_on_out_of_scope restore_turn( calendar::turn );
    const bandit_live_world::world_state previous_world =
        overmap_buffer.global_state.bandit_live_world;
    std::vector<character_id> transient_npcs;
    on_out_of_scope restore_world( [&previous_world, &transient_npcs]() {
        for( const character_id id : transient_npcs ) {
            overmap_buffer.remove_npc( id );
        }
        overmap_buffer.global_state.bandit_live_world = previous_world;
        reset_live_light_sample_cache();
        clear_creatures();
    } );
    setup_dark_native_lightmap();
    calendar::turn = calendar::turn_zero + 100_turns;
    reset_live_light_sample_cache();
    overmap_buffer.global_state.bandit_live_world.clear();

    map &here = get_map();
    const tripoint_bub_ms origin = get_avatar().pos_bub();
    // The carrier tile is always rechecked by the production sampler, so this
    // smoke is guaranteed to be the first discovered source on the turn.
    const tripoint_bub_ms smoke_pos = origin;
    const tripoint_abs_ms smoke_abs = here.get_abs( smoke_pos );
    const tripoint_abs_omt camp_omt = coords::project_to<coords::omt>( smoke_abs );
    const field_type_id smoke_type( "fd_smoke" );
    here.add_field( smoke_pos, smoke_type, 2 );
    on_out_of_scope cleanup_smoke( [&here, smoke_pos, smoke_type]() {
        here.remove_field( smoke_pos, smoke_type );
    } );

    const auto make_site = []( const std::string & id,
    const tripoint_abs_omt & anchor ) {
        bandit_live_world::site_record site;
        site.site_id = id;
        site.source_kind = bandit_live_world::anchor_source_kind::overmap_special;
        site.site_kind = bandit_live_world::owned_site_kind::cannibal_camp;
        site.profile = bandit_live_world::hostile_site_profile::cannibal_camp;
        site.source_id = "cannibal_camp";
        site.anchor = anchor;
        site.footprint.push_back( anchor );
        site.living_total = 2;
        site.supply_accounted_living_total = 2;
        return site;
    };

    const tripoint_bub_ms member_pos_a = origin + tripoint::north;
    const tripoint_bub_ms member_pos_b = origin + tripoint::south;
    npc &member_a = spawn_npc( member_pos_a.xy(), "cannibal_hunter" );
    npc &member_b = spawn_npc( member_pos_b.xy(), "cannibal_hunter" );
    clear_character( member_a, true );
    clear_character( member_b, true );
    member_a.setpos( here, member_pos_a );
    member_b.setpos( here, member_pos_b );

    bandit_live_world::site_record &dispatchable =
        overmap_buffer.global_state.bandit_live_world.sites.emplace_back(
            make_site( "test:smoke_dispatchable", camp_omt ) );
    for( const npc *member : {
             &member_a, &member_b
         } ) {
        const tripoint_abs_ms position = member->pos_abs();
        dispatchable.members.push_back( { member->getID(), "cannibal_hunter", position,
                                          bandit_live_world::member_state::at_home, false, "" } );
        dispatchable.spawn_tiles.push_back( { position, 1 } );
    }

    const tripoint_abs_omt failed_route_anchor( camp_omt.x(), camp_omt.y() + 1,
            camp_omt.z() );
    bandit_live_world::site_record &failed_route =
        overmap_buffer.global_state.bandit_live_world.sites.emplace_back(
            make_site( "test:smoke_failed_route", failed_route_anchor ) );
    failed_route.members.push_back( { character_id( 970001 ), "cannibal_hunter",
                                      project_to<coords::ms>( failed_route_anchor ),
                                      bandit_live_world::member_state::at_home, false, "" } );
    failed_route.members.push_back( { character_id( 970002 ), "cannibal_hunter",
                                      project_to<coords::ms>( failed_route_anchor ),
                                      bandit_live_world::member_state::at_home, false, "" } );

    const auto camp_special_at = [&camp_omt]( const tripoint_abs_omt & candidate ) ->
    std::optional<std::string> {
        if( candidate == camp_omt )
        {
            return "cannibal_camp";
        }
        return std::nullopt;
    };
    REQUIRE( bandit_live_world::register_abstract_site(
                 overmap_buffer.global_state.bandit_live_world,
                 bandit_live_world::anchor_source_kind::overmap_special, "cannibal_camp",
                 camp_omt, camp_special_at, 3 ) );
    const std::string abstract_site_id =
        overmap_buffer.global_state.bandit_live_world.sites.back().site_id;

    const tripoint_abs_omt out_of_range_anchor( camp_omt.x() + 40, camp_omt.y(),
            camp_omt.z() );
    overmap_buffer.global_state.bandit_live_world.sites.push_back(
        make_site( "test:smoke_out_of_range", out_of_range_anchor ) );

    const std::filesystem::path event_path = std::filesystem::absolute(
                "build_logs/first-smoke-001/adapter-events.jsonl" );
    std::filesystem::create_directories( event_path.parent_path() );
    std::filesystem::remove( event_path );
    scoped_transition_environment transition_environment;
    transition_environment.set( event_path.string(), "smoke-adapter-test" );
    const auto read_transition_stream = [&event_path]() {
        std::ifstream input( event_path );
        return std::string( std::istreambuf_iterator<char>( input ),
                            std::istreambuf_iterator<char>() );
    };

    run_live_light_delivery_for_test();
    const std::vector<live_bandit_signal_observation> &samples =
        live_light::samples_for_turn( calendar::turn );
    const auto smoke = std::find_if( samples.begin(), samples.end(),
    []( const live_bandit_signal_observation & sample ) {
        return sample.mark.kind == "smoke";
    } );
    REQUIRE( smoke != samples.end() );
    CHECK_FALSE( smoke->has_light_projection );
    CHECK_FALSE( std::any_of( samples.begin(), samples.end(),
    []( const live_bandit_signal_observation & sample ) {
        return sample.mark.kind == "light" || sample.mark.kind == "searchlight";
    } ) );
    CHECK( smoke->source_ms == smoke_abs );
    CHECK( smoke->observed_turn == 100 );
    CHECK( smoke->sample_id == smoke->mark.mark_id + "#100" );

    const auto find_site = []( const std::string & id ) -> const bandit_live_world::site_record * {
        return overmap_buffer.global_state.bandit_live_world.find_site( id );
    };
    // Physical discovery records a staffed observer's real signal lead; it
    // cannot bypass the ordinary structural assignment/watch/return owner.
    const live_bandit_signal_observation original_smoke = *smoke;
    const auto check_observed_lead = [&original_smoke, &camp_omt](
    const bandit_live_world::site_record * site ) {
        REQUIRE( site != nullptr );
        CHECK_FALSE( site->active_outing.is_active() );
        CHECK( site->active_outing.member_ids.empty() );
        REQUIRE( site->intelligence_map.leads.size() == 1 );
        const auto &lead = site->intelligence_map.leads.front();
        CHECK( lead.kind == bandit_live_world::camp_lead_kind::smoke_signal );
        CHECK( lead.origin == bandit_live_world::camp_lead_origin::signal );
        CHECK( lead.omt == camp_omt );
        CHECK( lead.source_sample_id == original_smoke.sample_id );
        CHECK( lead.first_seen_minutes == original_smoke.observed_minutes );
        CHECK( lead.last_seen_minutes == original_smoke.observed_minutes );
        CHECK( lead.last_checked_minutes == -1 );
        for( const auto &member : site->members ) {
            CHECK( member.state == bandit_live_world::member_state::at_home );
        }
    };
    const auto *observed_site = find_site( "test:smoke_dispatchable" );
    check_observed_lead( observed_site );
    const auto first_lead = observed_site->intelligence_map.leads.front();
    const auto *abstract_site = find_site( abstract_site_id );
    check_observed_lead( abstract_site );
    CHECK( abstract_site->members.size() == 3 );
    const auto first_abstract_lead = abstract_site->intelligence_map.leads.front();
    for( const auto &member : abstract_site->members ) {
        transient_npcs.push_back( member.npc_id );
    }
    for( const std::string id : {
             "test:smoke_failed_route", "test:smoke_out_of_range"
         } ) {
        const auto *refused_site = find_site( id );
        REQUIRE( refused_site != nullptr );
        CHECK( refused_site->intelligence_map.leads.empty() );
        CHECK_FALSE( refused_site->active_outing.is_active() );
    }

    const std::string transition_stream = read_transition_stream();
    CAPTURE( transition_stream );
    CHECK( transition_stream.find( "staffed_camp_signal_read" ) != std::string::npos );
    CHECK( transition_stream.find( "home_sensor_missing" ) != std::string::npos );
    CHECK( transition_stream.find( "no_in_range_production_signal" ) != std::string::npos );
    CHECK( transition_stream.find( std::string( "\"site_id\":\"" ) + abstract_site_id + "\"" ) !=
           std::string::npos );
    CHECK( transition_stream.find( "production_lazy_materialization=3" ) != std::string::npos );
    CHECK( transition_stream.find( "concrete_roster_ready" ) != std::string::npos );

    // Same-turn replay cannot rerun the producer, refresh knowledge or allocate
    // another roster. A later packet in this same observation minute likewise
    // cannot manufacture a new lead revision or a sortie.
    run_live_light_delivery_for_test();
    CHECK( read_transition_stream() == transition_stream );
    calendar::turn += 1_turns;
    run_live_light_delivery_for_test();
    observed_site = find_site( "test:smoke_dispatchable" );
    check_observed_lead( observed_site );
    CHECK( observed_site->intelligence_map.leads.front().revision == first_lead.revision );
    CHECK( observed_site->intelligence_map.leads.front().source_sample_id ==
           first_lead.source_sample_id );
    abstract_site = find_site( abstract_site_id );
    check_observed_lead( abstract_site );
    CHECK( abstract_site->members.size() == 3 );
    CHECK( abstract_site->intelligence_map.leads.front().revision == first_abstract_lead.revision );
    CHECK( abstract_site->intelligence_map.leads.front().source_sample_id ==
           first_abstract_lead.source_sample_id );
}

TEST_CASE( "production loaded strong fire emits the calibrated smoke envelope",
           "[physical_light][smoke_first]" )
{
    restore_on_out_of_scope restore_turn( calendar::turn );
    const bandit_live_world::world_state previous_world =
        overmap_buffer.global_state.bandit_live_world;
    on_out_of_scope restore_world( [&]() {
        overmap_buffer.global_state.bandit_live_world = previous_world;
    } );
    setup_dark_native_lightmap();
    overmap_buffer.global_state.bandit_live_world.clear();
    calendar::turn = calendar::turn_zero + 101_turns;
    reset_live_light_sample_cache();
    map &here = get_map();
    const tripoint_bub_ms source = get_avatar().pos_bub();
    const tripoint_abs_ms source_abs = here.get_abs( source );
    here.add_field( source, field_type_id( "fd_fire" ), 2 );
    here.add_field( source, field_type_id( "fd_smoke" ), 1 );
    on_out_of_scope cleanup( [&]() {
        here.remove_field( source, field_type_id( "fd_fire" ) );
        here.remove_field( source, field_type_id( "fd_smoke" ) );
        reset_live_light_sample_cache();
    } );
    scoped_weather_override clear_weather( WEATHER_CLEAR );
    run_live_light_delivery_for_test();
    const auto &samples = live_light::samples_for_turn( calendar::turn );
    const auto smoke = std::find_if( samples.begin(), samples.end(), [&]( const auto &sample ) {
        return sample.mark.kind == "smoke" && sample.source_ms == source_abs;
    } );
    REQUIRE( smoke != samples.end() );
    CHECK( smoke->range_cap_omt == 30 );
    CHECK( smoke->smoke_observable_ms );
}

TEST_CASE( "production powered roof lamp emits the elevated light envelope",
           "[physical_light][optics]" )
{
    restore_on_out_of_scope restore_turn( calendar::turn );
    const bandit_live_world::world_state previous_world =
        overmap_buffer.global_state.bandit_live_world;
    on_out_of_scope restore_world( [&]() {
        overmap_buffer.global_state.bandit_live_world = previous_world;
        reset_live_light_sample_cache();
    } );
    clear_avatar();
    clear_map_with_vision( -1, 3, false );
    map &here = get_map();
    avatar &u = get_avatar();
    const tripoint_bub_ms roof( u.pos_bub().x(), u.pos_bub().y(), 1 );
    const ter_str_id roof_terrain = GENERATE( ter_str_id( "t_flat_roof" ),
                                    ter_str_id( "t_shingle_flat_roof" ),
                                    ter_str_id( "t_tar_flat_roof" ), ter_str_id( "t_metal_flat_roof" ),
                                    ter_str_id( "t_tile_flat_roof" ), ter_str_id( "t_thatch_roof" ),
                                    ter_str_id( "t_concrete_roof" ), ter_str_id( "t_wood_roof" ),
                                    ter_str_id( "t_drystone_roof" ), ter_str_id( "t_log_sod_roof" ) );
    CAPTURE( roof_terrain.str() );
    here.ter_set( roof, roof_terrain );
    here.invalidate_map_cache( 1 );
    here.build_map_cache( 1 );
    u.setpos( here, roof, false );
    item battery( itype_id( "light_battery_cell" ) );
    battery.ammo_set( battery.ammo_default(), 1000 );
    item lamp( itype_id( "diving_flashlight_small_hipower_on" ) );
    lamp.put_in( battery, pocket_type::MAGAZINE_WELL );
    here.add_item( roof, lamp );
    on_out_of_scope cleanup( [&]() {
        here.i_clear( roof );
    } );
    overmap_buffer.global_state.bandit_live_world.clear();
    calendar::turn = calendar::turn_zero + 101_turns;
    scoped_weather_override clear_weather( WEATHER_CLEAR );
    reset_live_light_sample_cache();
    run_live_light_delivery_for_test();
    const auto &samples = live_light::samples_for_turn( calendar::turn );
    const tripoint_abs_ms source = here.get_abs( roof );
    const auto light = std::find_if( samples.begin(), samples.end(), [&]( const auto &sample ) {
        return sample.has_light_projection && sample.source_ms == source;
    } );
    REQUIRE( light != samples.end() );
    CHECK( light->light_projection.concealment.elevated_exposure_extended );
    CHECK( light->range_cap_omt == 40 );
}

TEST_CASE( "elevated light requires an exposed roof deck", "[physical_light][optics]" )
{
    clear_avatar();
    clear_map_with_vision( -1, 3, false );
    map &here = get_map();
    const tripoint_bub_ms base = get_avatar().pos_bub();
    const tripoint_bub_ms upper( base.x(), base.y(), 1 );

    here.ter_set( base, ter_str_id( "t_shingle_flat_roof" ) );
    here.ter_set( upper, ter_str_id( "t_open_air" ) );
    for( int z = 0; z <= 2; ++z ) {
        here.invalidate_map_cache( z );
        here.build_map_cache( z );
    }
    CHECK_FALSE( physical_light::evaluate_escape( here, base ).elevated_exposed );
    CHECK_FALSE( physical_light::evaluate_escape( here, upper ).elevated_exposed );
    here.ter_set( upper, ter_str_id( "t_floor" ) );
    here.invalidate_map_cache( 1 );
    here.build_map_cache( 1 );
    CHECK_FALSE( physical_light::evaluate_escape( here, upper ).elevated_exposed );

    here.ter_set( upper, ter_str_id( "t_shingle_flat_roof" ) );
    here.ter_set( upper + tripoint( 1, 0, 0 ), ter_str_id( "t_floor" ) );
    for( int x = -1; x <= 1; ++x ) {
        for( int y = -1; y <= 1; ++y ) {
            here.ter_set( upper + tripoint( x, y, 1 ), ter_str_id( "t_flat_roof" ) );
        }
    }
    for( int z = 1; z <= 2; ++z ) {
        here.invalidate_map_cache( z );
        here.build_map_cache( z );
    }
    CHECK_FALSE( physical_light::evaluate_escape( here, upper ).exposed_to_sky );
    CHECK_FALSE( physical_light::evaluate_escape( here, upper ).elevated_exposed );
}

TEST_CASE( "production rider light clusters respect walls and source escape",
           "[physical_light][rider_cluster]" )
{
    restore_on_out_of_scope restore_turn( calendar::turn );
    auto &memories = overmap_buffer.global_state.zombie_rider_light_memory;
    const auto previous_memories = memories;
    on_out_of_scope restore( [&]() {
        memories = previous_memories;
        reset_live_light_sample_cache();
    } );
    clear_avatar();
    clear_map_with_vision( -1, 3, false );
    map &here = get_map();
    avatar &u = get_avatar();
    const tripoint_bub_ms a = u.pos_bub();
    const tripoint_bub_ms b = a + tripoint( 24, 0, 0 );
    REQUIRE( here.inbounds( b ) );
    REQUIRE( coords::project_to<coords::omt>( here.get_abs( a ) ) !=
             coords::project_to<coords::omt>( here.get_abs( b ) ) );
    here.add_field( a, field_type_id( "fd_fire" ), 3 );
    here.add_field( b, field_type_id( "fd_fire" ), 3 );
    calendar::turn = calendar::turn_zero + 101_turns;
    scoped_weather_override clear_weather( WEATHER_CLEAR );
    reset_live_light_sample_cache();
    run_live_light_delivery_for_test();
    u.setpos( here, b, false );
    calendar::turn += 1_turns;
    run_live_light_delivery_for_test();
    memories.clear();
    calendar::turn += 1_turns;
    run_live_light_delivery_for_test();
    CHECK( memories.size() == 1 );

    here.ter_set( a + tripoint( 12, 0, 0 ), ter_str_id( "t_wall" ) );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    memories.clear();
    calendar::turn += 1_turns;
    run_live_light_delivery_for_test();
    CHECK( memories.size() == 2 );

    // Enclose both sources without an aperture; a roof alone would not
    // establish containment if a nearby side path still escaped.
    for( const tripoint_bub_ms source : { a, b } ) {
        here.ter_set( source, ter_str_id( "t_floor" ) );
        for( int x = -1; x <= 1; ++x ) {
            for( int y = -1; y <= 1; ++y ) {
                if( x != 0 || y != 0 ) {
                    here.ter_set( source + tripoint( x, y, 0 ), ter_str_id( "t_wall" ) );
                }
                here.ter_set( source + tripoint( x, y, 1 ), ter_str_id( "t_flat_roof" ) );
            }
        }
    }
    for( int z = 0; z <= 1; ++z ) {
        here.invalidate_map_cache( z );
        here.build_map_cache( z );
    }
    CHECK_FALSE( physical_light::evaluate_escape( here, a ).exposed_to_sky );
    CHECK_FALSE( physical_light::evaluate_escape( here, b ).exposed_to_sky );
    CHECK( physical_light::evaluate_escape( here, a ).side_leakage == 0 );
    CHECK( physical_light::evaluate_escape( here, b ).side_leakage == 0 );
    memories.clear();
    calendar::turn += 1_turns;
    run_live_light_delivery_for_test();
    CHECK( memories.empty() );
}

TEST_CASE( "physical_light_respects_depletion_and_movement", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    item depleted( itype_id( "flashlight_on" ) );
    item dead_battery( itype_id( "medium_battery_cell" ) );
    depleted.put_in( dead_battery, pocket_type::MAGAZINE_WELL );
    here.add_item( origin + tripoint::east, depleted );
    CHECK( physical_light::collect_item_emitters( guy, here, 3 ).empty() );

    here.add_item( origin + tripoint::north, powered_flashlight() );
    const auto moved = physical_light::collect_item_emitters( guy, here, 3 );
    REQUIRE( moved.size() == 1 );
    CHECK( moved.front().position == here.get_abs( origin + tripoint::north ) );
}

TEST_CASE( "physical_light_item_power_states_match_native_lightmap", "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms source_pos = guy.pos_bub() + tripoint::east * 3;
    const tripoint_abs_ms source_abs = here.get_abs( source_pos );

    rebuild_native_lightmap();
    const float baseline_native = here.ambient_light_at( source_pos );
    here.add_item( source_pos, powered_flashlight() );
    const auto full = physical_light::collect_item_emitters( guy, here, 4 );
    const auto full_source = std::find_if( full.begin(), full.end(),
    [&source_abs]( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::ground_item && e.position == source_abs;
    } );
    REQUIRE( full_source != full.end() );
    const int full_luminance = full_source->luminance;
    rebuild_native_lightmap();
    const float full_native = here.ambient_light_at( source_pos );
    CHECK( here.light_at( source_pos ) == lit_level::BRIGHT );
    CHECK( full_native > baseline_native );

    // The same source remains source-bound when its battery is in the native
    // CHARGEDIM region; neither side may silently retain the full value.
    clear_map_without_vision();
    here.add_item( source_pos, powered_flashlight( 1 ) );
    const auto dimmed = physical_light::collect_item_emitters( guy, here, 4 );
    REQUIRE( dimmed.size() == 1 );
    REQUIRE( dimmed.front().position == source_abs );
    CHECK( dimmed.front().luminance > 0 );
    CHECK( dimmed.front().luminance < full_luminance );
    rebuild_native_lightmap();
    const float dimmed_native = here.ambient_light_at( source_pos );
    CHECK( dimmed_native > baseline_native );
    CHECK( dimmed_native < full_native );

    // Turning the item off and removing its battery are negative controls for
    // both the collector and native map lightmap.
    clear_map_without_vision();
    item off( itype_id( "flashlight" ) );
    off.put_in( item( itype_id( "medium_battery_cell" ) ), pocket_type::MAGAZINE_WELL );
    here.add_item( source_pos, off );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) == Approx( baseline_native ) );

    clear_map_without_vision();
    item depleted( itype_id( "flashlight_on" ) );
    depleted.put_in( item( itype_id( "medium_battery_cell" ) ), pocket_type::MAGAZINE_WELL );
    here.add_item( source_pos, depleted );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) == Approx( baseline_native ) );

    clear_map_without_vision();
    here.add_item( source_pos, powered_flashlight() );
    REQUIRE_FALSE( physical_light::collect_item_emitters( guy, here, 4 ).empty() );
    here.i_clear( source_pos );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) == Approx( baseline_native ) );
}

TEST_CASE( "physical_light_containment_matches_native_lightmap", "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms source_pos = guy.pos_bub() + tripoint::east * 3;
    const tripoint_abs_ms source_abs = here.get_abs( source_pos );

    rebuild_native_lightmap();
    const float baseline_native = here.ambient_light_at( source_pos );
    item transparent( itype_id( "bag_plastic" ) );
    transparent.put_in( powered_flashlight(), pocket_type::CONTAINER );
    here.add_item( source_pos, transparent );
    const auto escaped = physical_light::collect_item_emitters( guy, here, 4 );
    REQUIRE( escaped.size() == 1 );
    CHECK( escaped.front().position == source_abs );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) > baseline_native );

    clear_map_without_vision();
    item opaque( itype_id( "backpack" ) );
    opaque.put_in( powered_flashlight(), pocket_type::CONTAINER );
    here.add_item( source_pos, opaque );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) == Approx( baseline_native ) );
}

TEST_CASE( "physical_light_carried_record_matches_native_character_light",
           "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_abs_ms source_abs = here.get_abs( guy.pos_bub() );
    const tripoint_bub_ms neighbour_pos = guy.pos_bub() + tripoint::east;
    rebuild_native_lightmap();
    const float baseline_native = here.ambient_light_at( neighbour_pos );
    REQUIRE( guy.try_add( powered_flashlight() ) != item_location::nowhere );

    const auto carried = physical_light::collect_item_emitters( guy, here, 1 );
    const auto carried_source = std::find_if( carried.begin(), carried.end(),
    [&source_abs]( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::carried_item && e.position == source_abs;
    } );
    REQUIRE( carried_source != carried.end() );
    rebuild_native_lightmap();
    CHECK( here.light_at( guy.pos_bub() ) == lit_level::BRIGHT );
    CHECK( here.ambient_light_at( neighbour_pos ) > baseline_native );
}

TEST_CASE( "physical_light_native_same_omt_sources_remain_independent",
           "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    // A loaded map can start on either submap of an OMT. Local tile65 does
    // not guarantee that the unchanged +/-3 source pair shares an OMT.
    const auto omt = coords::project_to<coords::omt>( guy.pos_abs() );
    const auto center = here.get_bub( coords::project_to<coords::ms>( omt ) +
                                    tripoint_rel_ms( SEEX, SEEY, 0 ) );
    REQUIRE( here.inbounds( center ) );
    guy.setpos( here, center );
    const tripoint_bub_ms bright_pos = guy.pos_bub() + tripoint::east * 3;
    const tripoint_bub_ms dim_pos = guy.pos_bub() + tripoint::west * 3;
    REQUIRE( coords::project_to<coords::omt>( here.get_abs( bright_pos ) ) ==
             coords::project_to<coords::omt>( here.get_abs( dim_pos ) ) );
    rebuild_native_lightmap();
    const float dim_baseline = here.ambient_light_at( dim_pos );

    here.add_item( bright_pos, powered_flashlight() );
    here.add_item( dim_pos, powered_flashlight( 1 ) );
    const auto combined = physical_light::collect_item_emitters( guy, here, 4 );
    REQUIRE( combined.size() == 2 );
    CHECK( std::count_if( combined.begin(), combined.end(),
    [&here, &bright_pos]( const physical_light::emitter & e ) {
        return e.position == here.get_abs( bright_pos );
    } ) == 1 );
    CHECK( std::count_if( combined.begin(), combined.end(),
    [&here, &dim_pos]( const physical_light::emitter & e ) {
        return e.position == here.get_abs( dim_pos );
    } ) == 1 );
    rebuild_native_lightmap();
    CHECK( here.light_at( bright_pos ) == lit_level::BRIGHT );
    CHECK( here.ambient_light_at( dim_pos ) > dim_baseline );

    // Removing the bright source leaves the dim source's own native result;
    // its cache observation cannot be borrowed from the other OMT source.
    here.i_clear( bright_pos );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).size() == 1 );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( dim_pos ) > dim_baseline );

    // The reciprocal control binds the bright result to its own source.
    here.i_clear( dim_pos );
    here.add_item( bright_pos, powered_flashlight() );
    CHECK( physical_light::collect_item_emitters( guy, here, 4 ).size() == 1 );
    rebuild_native_lightmap();
    CHECK( here.light_at( bright_pos ) == lit_level::BRIGHT );
}

TEST_CASE( "physical_light_respects_container_transparency", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    item bag( itype_id( "bag_plastic" ) );
    bag.put_in( powered_flashlight(), pocket_type::CONTAINER );
    REQUIRE( guy.try_add( bag ) != item_location::nowhere );
    const auto emitters = physical_light::collect_item_emitters( guy, here, 1 );
    REQUIRE( emitters.size() == 1 );
    CHECK( emitters.front().provenance == "flashlight_on" );
}

TEST_CASE( "physical_light_handles_ground_container_escape", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms p = guy.pos_bub() + tripoint::east;
    item transparent( itype_id( "bag_plastic" ) );
    transparent.put_in( powered_flashlight(), pocket_type::CONTAINER );
    here.add_item( p, transparent );
    CHECK( physical_light::collect_item_emitters( guy, here, 2 ).size() == 1 );

    item opaque( itype_id( "backpack" ) );
    opaque.put_in( powered_flashlight(), pocket_type::CONTAINER );
    here.add_item( p + tripoint::east, opaque );
    const auto emitters = physical_light::collect_item_emitters( guy, here, 3 );
    CHECK( emitters.size() == 1 );
}

TEST_CASE( "physical_light_weapon_mount_has_one_source", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    item gun( itype_id( "glock_19" ) );
    item mounted( itype_id( "mounted_flashlight_on" ) );
    item mount_battery( itype_id( "light_battery_cell" ) );
    mount_battery.ammo_set( mount_battery.ammo_default(), 56 );
    REQUIRE( mounted.put_in( mount_battery, pocket_type::MAGAZINE_WELL ).success() );
    REQUIRE( gun.put_in( mounted, pocket_type::MOD ).success() );
    REQUIRE( guy.wield( gun ) );
    const auto emitters = physical_light::collect_item_emitters( guy, here, 1 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::weapon_mounted;
    } ) == 1 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter & e ) {
        return e.provenance == "glock_19";
    } ) == 0 );
}

TEST_CASE( "physical_light_loaded_z_index_handles_ground_gun_light_mod", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms source_pos = guy.pos_bub() + tripoint::east;

    // This is the topology from the advancing-turn crash: a ground gun whose
    // powered mounted light is reached while index_loaded_z_sources traverses
    // every loaded z-level.  The forwarding root must not be emitted twice.
    item gun( itype_id( "glock_19" ) );
    item mounted( itype_id( "mounted_flashlight_on" ) );
    item battery( itype_id( "light_battery_cell" ) );
    battery.ammo_set( battery.ammo_default(), 56 );
    REQUIRE( mounted.put_in( battery, pocket_type::MAGAZINE_WELL ).success() );
    REQUIRE( gun.put_in( mounted, pocket_type::MOD ).success() );
    here.add_item( source_pos, gun );

    const physical_light::loaded_z_source_index index =
        physical_light::index_loaded_z_sources( guy, here );
    const tripoint_abs_ms source_abs = here.get_abs( source_pos );
    CHECK( std::count_if( index.item_emitters.begin(), index.item_emitters.end(),
    [&source_abs]( const physical_light::emitter & emitter ) {
        return emitter.position == source_abs && emitter.kind == physical_light::source_kind::ground_item &&
               emitter.provenance == "mounted_flashlight_on";
    } ) == 1 );
    CHECK( std::none_of( index.item_emitters.begin(), index.item_emitters.end(),
    [&source_abs]( const physical_light::emitter & emitter ) {
        return emitter.position == source_abs && emitter.provenance == "glock_19";
    } ) );
}

TEST_CASE( "physical_light_weapon_mount_matches_native_lightmap", "[physical_light][lightmap]" )
{
    const auto mounted_gun = []( const bool powered ) {
        item gun( itype_id( "glock_19" ) );
        item mounted( itype_id( "mounted_flashlight_on" ) );
        item battery( itype_id( "light_battery_cell" ) );
        if( powered ) {
            battery.ammo_set( battery.ammo_default(), 56 );
        }
        REQUIRE( mounted.put_in( battery, pocket_type::MAGAZINE_WELL ).success() );
        REQUIRE( gun.put_in( mounted, pocket_type::MOD ).success() );
        return gun;
    };

    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms neighbour = guy.pos_bub() + tripoint::east;
    rebuild_native_lightmap();
    const float baseline_native = here.ambient_light_at( neighbour );
    item powered_gun = mounted_gun( true );
    REQUIRE( guy.wield( powered_gun ) );

    const auto powered = physical_light::collect_item_emitters( guy, here, 1 );
    const auto source_abs = here.get_abs( guy.pos_bub() );
    const auto mounted_source = std::find_if( powered.begin(), powered.end(),
    [&source_abs]( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::weapon_mounted &&
               e.position == source_abs;
    } );
    REQUIRE( mounted_source != powered.end() );
    CHECK( mounted_source->provenance == "mounted_flashlight_on" );
    CHECK( mounted_source->luminance > 0 );
    rebuild_native_lightmap();
    CHECK( here.light_at( guy.pos_bub() ) == lit_level::BRIGHT );
    CHECK( here.ambient_light_at( neighbour ) > baseline_native );
}

TEST_CASE( "physical_light_weapon_mount_depleted_matches_native_lightmap",
           "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &depleted_guy = get_avatar();
    map &depleted_here = get_map();
    const tripoint_bub_ms depleted_neighbour = depleted_guy.pos_bub() + tripoint::east;
    rebuild_native_lightmap();
    const float depleted_baseline = depleted_here.ambient_light_at( depleted_neighbour );
    item depleted_gun( itype_id( "glock_19" ) );
    item depleted_mount( itype_id( "mounted_flashlight_on" ) );
    item depleted_battery( itype_id( "light_battery_cell" ) );
    REQUIRE( depleted_mount.put_in( depleted_battery, pocket_type::MAGAZINE_WELL ).success() );
    REQUIRE( depleted_gun.put_in( depleted_mount, pocket_type::MOD ).success() );
    REQUIRE( depleted_guy.wield( depleted_gun ) );
    CHECK( physical_light::collect_item_emitters( depleted_guy, depleted_here, 1 ).empty() );
    rebuild_native_lightmap();
    CHECK( depleted_here.ambient_light_at( depleted_neighbour ) == Approx( depleted_baseline ) );
}

TEST_CASE( "physical_light_detects_each_escaped_source_without_terrain_recognition",
           "[physical_light][optics]" )
{
    const auto detected = []( int distance, int vertical, bool exposed, bool vertical_clear,
    std::vector<int> attenuation = {} ) {
        physical_light::route route;
        route.distance_omt = distance;
        route.vertical_offset = vertical;
        route.source_exposed = exposed;
        route.vertical_sightline = vertical_clear;
        route.brightness_range_omt = 5;
        route.terrain_see_costs = std::move( attenuation );
        return physical_light::detect( route );
    };

    // This query accepts no observer terrain-recognition range.  The light is
    // a location clue only, never player/camp identification.
    const physical_light::detection ground_exposed = detected( 2, 0, true, true );
    CHECK( ground_exposed.visible );
    CHECK_FALSE( ground_exposed.recognizes_identity );

    // Exposed elevation is visible in both directions when its cross-level
    // route is clear.  A bright source is not rejected merely for z mismatch.
    CHECK( detected( 2, 1, true, true ).visible ); // observer below
    CHECK( detected( 2, -1, true, true ).visible ); // observer above

    // A parapet/solid wall, a sealed upper room, opaque curtain or shutter,
    // and a deep hall without a local aperture all prevent detection.
    CHECK_FALSE( detected( 2, 1, true, true, { -1 } ).visible ); // parapet
    CHECK_FALSE( detected( 2, 1, false, false ).visible ); // sealed upper room/floor
    CHECK_FALSE( detected( 2, 0, true, true, { -1 } ).visible ); // opaque covering
    CHECK_FALSE( detected( 2, 0, false, true ).visible ); // deep interior corner

    // Clear glass attenuates but transmits; it is not equivalent to a curtain.
    CHECK( detected( 2, 0, true, true, { 1 } ).visible );
}

TEST_CASE( "physical light maximum requires an exposed clear route", "[physical_light][optics]" )
{
    physical_light::route route;
    route.brightness_range_omt = 40;
    route.distance_omt = 40;
    route.vertical_offset = 1;
    route.source_exposed = true;
    route.vertical_sightline = true;
    CHECK( physical_light::detect( route ).visible );
    CHECK_FALSE( physical_light::detect( route ).recognizes_identity );
    route.distance_omt = 41;
    CHECK_FALSE( physical_light::detect( route ).visible );
    route.distance_omt = 40;
    route.terrain_see_costs = { 1 };
    CHECK_FALSE( physical_light::detect( route ).visible );
    route.terrain_see_costs.clear();
    route.source_exposed = false;
    CHECK_FALSE( physical_light::detect( route ).visible );
}

TEST_CASE( "physical_light_keeps_same_omt_escape_and_position_per_source",
           "[physical_light][optics]" )
{
    physical_light::route window_lamp;
    window_lamp.distance_omt = 1;
    window_lamp.source_exposed = true;
    window_lamp.vertical_sightline = true;
    window_lamp.brightness_range_omt = 3;

    physical_light::route sealed_lamp = window_lamp;
    sealed_lamp.source_exposed = false;

    const physical_light::detection window_result = physical_light::detect( window_lamp );
    const physical_light::detection sealed_result = physical_light::detect( sealed_lamp );
    CHECK( window_result.visible );
    CHECK_FALSE( sealed_result.visible );
    CHECK_FALSE( window_result.recognizes_identity );
    CHECK_FALSE( sealed_result.recognizes_identity );
}

TEST_CASE( "physical_light_collects_stationary_native_owners", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    const tripoint_bub_ms field_pos = origin + tripoint::east;
    here.add_field( field_pos, field_type_id( "fd_bile_adv" ), 2 );

    monster &luminous = spawn_test_monster( "mon_fungal_blossom", origin + tripoint::north );
    REQUIRE( luminous.type->luminance > 0 );

    vehicle *veh = here.add_vehicle( vproto_id( "car" ), origin + tripoint::south, 0_degrees, 0,
                                     veh_spawn_status::UNDAMAGED );
    REQUIRE( veh != nullptr );
    bool enabled_light = false;
    for( const vpart_reference &ref : veh->get_all_parts() ) {
        vehicle_part &part = ref.part();
        if( part.is_light() ) {
            part.enabled = true;
            enabled_light = true;
        }
    }
    REQUIRE( enabled_light );

    const auto emitters = physical_light::collect_stationary_emitters( here, origin, 2 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::field;
    } ) == 1 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::luminous_monster;
    } ) == 1 );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::vehicle_part;
    } ) >= 1 );
}

TEST_CASE( "physical_light_stationary_records_are_source_bound", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const auto source_omt = coords::project_to<coords::omt>( guy.pos_abs() );
    const tripoint_bub_ms origin = here.get_bub( coords::project_to<coords::ms>( source_omt ) +
                                   point( SEEX, SEEY ) );
    REQUIRE( here.inbounds( origin ) );
    const tripoint_bub_ms terrain_pos = origin + tripoint::east;
    const tripoint_bub_ms furniture_pos = origin + tripoint::west;
    const tripoint_bub_ms field_a = origin + tripoint::north;
    const tripoint_bub_ms field_b = origin + tripoint::south;

    ter_t &terrain = const_cast<ter_t &>( here.ter( terrain_pos ).obj() );
    furn_t &furniture = const_cast<furn_t &>( here.furn( furniture_pos ).obj() );
    restore_on_out_of_scope terrain_light_restore( terrain.light_emitted );
    restore_on_out_of_scope furniture_light_restore( furniture.light_emitted );
    terrain.light_emitted = 17;
    furniture.light_emitted = 23;
    here.add_field( field_a, field_type_id( "fd_bile_adv" ), 2 );
    here.add_field( field_b, field_type_id( "fd_bile_adv" ), 3 );

    const auto emitters = physical_light::collect_stationary_emitters( here, origin, 2 );
    const auto find = [&emitters]( physical_light::source_kind kind, const tripoint_abs_ms & pos ) {
        return std::find_if( emitters.begin(), emitters.end(), [kind,
        &pos]( const physical_light::emitter & e ) {
            return e.kind == kind && e.position == pos;
        } );
    };
    const auto terrain_source = find( physical_light::source_kind::terrain,
                                      here.get_abs( terrain_pos ) );
    const auto furniture_source = find( physical_light::source_kind::furniture,
                                        here.get_abs( furniture_pos ) );
    REQUIRE( terrain_source != emitters.end() );
    REQUIRE( furniture_source != emitters.end() );
    CHECK( terrain_source->provenance == "terrain:" + here.ter( terrain_pos ).obj().id.str() );
    CHECK( terrain_source->luminance == 17 );
    CHECK( furniture_source->provenance == "furniture:" + here.furn( furniture_pos ).obj().id.str() );
    CHECK( furniture_source->luminance == 23 );
    CHECK( terrain_source->position == here.get_abs( terrain_pos ) );
    CHECK( furniture_source->position == here.get_abs( furniture_pos ) );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::field;
    } ) == 2 );
    CHECK( coords::project_to<coords::omt>( here.get_abs( field_a ) ) ==
           coords::project_to<coords::omt>( here.get_abs( field_b ) ) );
    CHECK( std::count_if( emitters.begin(), emitters.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::field && e.position.x() != 0;
    } ) == 2 );

}

TEST_CASE( "physical_light_stationary_records_match_native_lightmap", "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    const tripoint_bub_ms terrain_pos = origin + tripoint::east * 3;
    const tripoint_bub_ms field_pos = origin + tripoint::north * 3;
    const tripoint_bub_ms monster_pos = origin + tripoint::west * 3;

    rebuild_native_lightmap();
    const float terrain_baseline = here.ambient_light_at( terrain_pos );
    const float field_baseline = here.ambient_light_at( field_pos );
    const float monster_baseline = here.ambient_light_at( monster_pos );
    here.ter_set( terrain_pos, ter_str_id( "t_test_red_light" ).id() );
    here.add_field( field_pos, field_type_id( "fd_test_green_glow" ), 1 );
    monster &luminous = spawn_test_monster( "mon_fungal_blossom", monster_pos );
    REQUIRE( luminous.type->luminance > 0 );

    const auto emitters = physical_light::collect_stationary_emitters( here, origin, 4 );
    const auto has_source = [&emitters]( physical_light::source_kind kind,
    const tripoint_abs_ms & position ) {
        return std::find_if( emitters.begin(), emitters.end(),
        [kind, &position]( const physical_light::emitter & e ) {
            return e.kind == kind && e.position == position && e.luminance > 0;
        } ) != emitters.end();
    };
    REQUIRE( has_source( physical_light::source_kind::terrain, here.get_abs( terrain_pos ) ) );
    REQUIRE( has_source( physical_light::source_kind::field, here.get_abs( field_pos ) ) );
    REQUIRE( has_source( physical_light::source_kind::luminous_monster,
                         here.get_abs( monster_pos ) ) );

    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( terrain_pos ) > terrain_baseline );
    CHECK( here.ambient_light_at( field_pos ) > field_baseline );
    CHECK( here.ambient_light_at( monster_pos ) > monster_baseline );
}

TEST_CASE( "physical_light_excludes_disabled_or_unavailable_vehicle_parts", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    vehicle *veh = here.add_vehicle( vproto_id( "car" ), origin + tripoint::south, 0_degrees, 0,
                                     veh_spawn_status::UNDAMAGED );
    REQUIRE( veh != nullptr );
    for( const vpart_reference &ref : veh->get_all_parts() ) {
        vehicle_part &part = ref.part();
        if( part.is_light() ) {
            part.enabled = false;
        }
    }
    const auto disabled = physical_light::collect_stationary_emitters( here, origin, 2 );
    CHECK( std::none_of( disabled.begin(), disabled.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::vehicle_part;
    } ) );
    for( const vpart_reference &ref : veh->get_all_parts() ) {
        vehicle_part &part = ref.part();
        if( part.is_light() ) {
            part.enabled = true;
            item broken_base = part.get_base();
            broken_base.force_set_damage( broken_base.max_damage() );
            part.set_base( std::move( broken_base ) );
        }
    }
    const auto unavailable = physical_light::collect_stationary_emitters( here, origin, 2 );
    CHECK( std::none_of( unavailable.begin(),
    unavailable.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::vehicle_part;
    } ) );
}

TEST_CASE( "physical_light_vehicle_parts_match_native_lightmap", "[physical_light][lightmap]" )
{
    setup_dark_native_lightmap();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    vehicle *veh = here.add_vehicle( vproto_id( "car" ), origin + tripoint::south, 0_degrees, 0,
                                     veh_spawn_status::UNDAMAGED );
    REQUIRE( veh != nullptr );
    for( const vpart_reference &ref : veh->get_all_parts() ) {
        if( ref.part().is_light() ) {
            ref.part().enabled = true;
        }
    }

    const std::vector<vehicle_part *> lights = veh->lights();
    REQUIRE_FALSE( lights.empty() );
    const vehicle_part *chosen = lights.front();
    const tripoint_bub_ms source_pos = veh->bub_part_pos( here, *chosen );
    const tripoint_abs_ms source_abs = here.get_abs( source_pos );
    const std::string provenance = "vehicle:" + chosen->info().id.str();
    const auto emitters = physical_light::collect_stationary_emitters( here, origin, 6 );
    const auto source = std::find_if( emitters.begin(), emitters.end(),
    [&source_abs, &provenance]( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::vehicle_part &&
               e.position == source_abs && e.provenance == provenance;
    } );
    REQUIRE( source != emitters.end() );
    CHECK( source->luminance == chosen->info().bonus );

    rebuild_native_lightmap();
    const float lit_native = here.ambient_light_at( source_pos );
    CHECK( lit_native > 0.0f );

    for( const vpart_reference &ref : veh->get_all_parts() ) {
        if( ref.part().is_light() ) {
            ref.part().enabled = false;
        }
    }
    CHECK( physical_light::collect_stationary_emitters( here, origin, 6 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) < lit_native );

    for( const vpart_reference &ref : veh->get_all_parts() ) {
        vehicle_part &part = ref.part();
        if( part.is_light() ) {
            part.enabled = true;
            item broken = part.get_base();
            broken.force_set_damage( broken.max_damage() );
            part.set_base( std::move( broken ) );
        }
    }
    CHECK( physical_light::collect_stationary_emitters( here, origin, 6 ).empty() );
    rebuild_native_lightmap();
    CHECK( here.ambient_light_at( source_pos ) < lit_native );
}

TEST_CASE( "physical_light_stationary_excludes_stale_and_hallucinated_sources", "[physical_light]" )
{
    clear_avatar();
    clear_map_without_vision();
    avatar &guy = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms origin = guy.pos_bub();
    monster &hallucination = spawn_test_monster( "mon_fungal_blossom", origin + tripoint::east );
    hallucination.hallucination = true;
    monster &real = spawn_test_monster( "mon_fungal_blossom", origin + tripoint::west );
    REQUIRE( real.type->luminance > 0 );
    const auto before = physical_light::collect_stationary_emitters( here, origin, 2 );
    CHECK( std::count_if( before.begin(), before.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::luminous_monster;
    } ) == 1 );
    g->remove_zombie( real );
    const auto after = physical_light::collect_stationary_emitters( here, origin, 2 );
    CHECK( std::none_of( after.begin(), after.end(), []( const physical_light::emitter & e ) {
        return e.kind == physical_light::source_kind::luminous_monster;
    } ) );
}

TEST_CASE( "current smoke support follows actual building roof geometry",
           "[physical_light][smoke_support_067]" )
{
    clear_avatar();
    clear_map_with_vision( -1, 4, false );
    map &here = get_map();
    const tripoint_bub_ms source( 40, 40, 0 );
    const auto rebuild = [&]() {
        for( int z = -1; z <= 4; ++z ) {
            here.invalidate_map_cache( z );
            here.build_map_cache( z );
        }
    };
    const auto support = [&]() {
        rebuild();
        return physical_light::evaluate_smoke_support( here, source );
    };
    const auto air = [&]( const int z ) {
        return here.get_abs( tripoint_bub_ms( 40, 40, z ) );
    };
    CHECK( support().position == here.get_abs( source ) );
    CHECK( support().reason == "exposed_emitter" );
    here.ter_set( source, ter_str_id( "t_floor" ) );
    here.ter_set( tripoint_bub_ms( 40, 40, 1 ), ter_str_id( "t_shingle_flat_roof" ) );
    rebuild();
    CHECK_FALSE( physical_light::evaluate_escape( here, source ).exposed_to_sky );
    REQUIRE( support().position );
    CHECK( support().position == air( 2 ) );
    CHECK( support().reason == "above_building_roof" );
    // The same column, with a real second floor/roof, moves support to z3.
    here.ter_set( tripoint_bub_ms( 40, 40, 1 ), ter_str_id( "t_floor" ) );
    here.ter_set( tripoint_bub_ms( 40, 40, 2 ), ter_str_id( "t_shingle_flat_roof" ) );
    CHECK( support().position == air( 3 ) );
    here.ter_set( tripoint_bub_ms( 40, 40, 1 ), ter_str_id( "t_rock" ) );
    CHECK_FALSE( support().position );
    CHECK( support().reason == "unsupported_roof_geometry" );
    // No favorable offset or alternate bearing is searched around a blocker.
    here.ter_set( tripoint_bub_ms( 40, 40, 1 ), ter_str_id( "t_shingle_flat_roof" ) );
    here.ter_set( tripoint_bub_ms( 40, 40, 2 ), ter_str_id( "t_wall" ) );
    CHECK_FALSE( support().position );
    CHECK( support().reason == "exterior_air_unavailable" );
    here.ter_set( tripoint_bub_ms( 40, 40, 1 ), ter_str_id::NULL_ID() );
    CHECK_FALSE( support().position );
    CHECK( support().reason == "roof_geometry_unavailable" );
    const tripoint_bub_ms roof( 42, 40, 1 );
    here.ter_set( roof, ter_str_id( "t_flat_roof" ) );
    CHECK( physical_light::evaluate_smoke_support( here, roof ).position == here.get_abs( roof ) );
    const tripoint_bub_ms unknown( 40, 40, OVERMAP_HEIGHT );
    here.ter_set( unknown, ter_str_id( "t_floor" ) );
    here.invalidate_map_cache( unknown.z() );
    here.build_map_cache( unknown.z() );
    CHECK_FALSE( physical_light::evaluate_smoke_support( here, unknown ).position );
    CHECK_FALSE( physical_light::evaluate_smoke_support( here, tripoint_bub_ms( -1, 40,
                 0 ) ).position );
}

TEST_CASE( "underground or smoke-suppressing sources need an actual outlet",
           "[physical_light][smoke_support_067]" )
{
    clear_avatar();
    clear_map_with_vision( -1, 3, false );
    map &here = get_map();
    const tripoint_bub_ms below( 40, 40, -1 );
    const tripoint_bub_ms ground( 40, 40, 0 );
    here.ter_set( below, ter_str_id( "t_floor" ) );
    here.ter_set( ground, ter_str_id( "t_floor" ) );
    CHECK_FALSE( physical_light::evaluate_smoke_support( here, below ).position );
    CHECK( physical_light::evaluate_smoke_support( here, below ).reason == "no_supported_outlet" );
    here.ter_set( ground, ter_str_id( "t_open_air" ) );
    REQUIRE( here.valid_move( below, ground, false, true ) );
    CHECK( physical_light::evaluate_smoke_support( here, below ).position == here.get_abs( ground ) );
    CHECK( physical_light::evaluate_smoke_support( here, below ).reason == "open_outlet" );
    here.ter_set( ground, ter_str_id( "t_floor" ) );
    here.furn_set( ground, furn_str_id( "f_woodstove" ) );
    here.ter_set( tripoint_bub_ms( 40, 40, 1 ), ter_str_id( "t_flat_roof" ) );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    here.invalidate_map_cache( 1 );
    here.build_map_cache( 1 );
    REQUIRE( here.has_flag( ter_furn_flag::TFLAG_SUPPRESS_SMOKE, ground ) );
    CHECK_FALSE( physical_light::evaluate_smoke_support( here, ground ).position );
}

TEST_CASE( "raid_sweep_ignores_removed_field_light_before_field_cleanup",
           "[physical_light][raid_sweep]" )
{
    clear_map();
    map &here = get_map();
    const tripoint_bub_ms source( 60, 60, 0 );
    here.add_field( source, field_type_id( "fd_fire" ), 3 );
    const auto before = physical_light::collect_stationary_emitters( here, source, 0 );
    REQUIRE( std::any_of( before.begin(), before.end(), []( const auto & emitter ) {
        return emitter.provenance == "field:fd_fire";
    } ) );
    here.remove_field( source, field_type_id( "fd_fire" ) );
    REQUIRE( here.get_field_intensity( source, field_type_id( "fd_fire" ) ) == 0 );
    const auto after = physical_light::collect_stationary_emitters( here, source, 0 );
    CHECK( std::none_of( after.begin(), after.end(), []( const auto & emitter ) {
        return emitter.provenance == "field:fd_fire";
    } ) );
}

class discovery_summary_counting_buffer : public std::stringbuf {
    public:
        int syncs = 0;
        int sync() override {
            ++syncs;
            return std::stringbuf::sync();
        }
};
TEST_CASE( "light discovery summary respects harness and explicit debug", "[physical_light][signal_log]" )
{
    const int mode = GENERATE( 0, 1, 2, 3 ); // ordinary, bound, opt-in, mismatched
    const bool weak_source = GENERATE( false, true );
    CAPTURE( mode, weak_source );
    clear_avatar();
    clear_map_without_vision();
    reset_live_light_sample_cache();
    restore_on_out_of_scope<time_point> restore_clock( calendar::turn );
    calendar::turn = calendar::turn_zero + 12_hours;
    map &here = get_map();
    here.ter_set( get_avatar().pos_bub(), ter_str_id( "t_radio_tower" ) );
    ter_t &source_terrain = const_cast<ter_t &>( here.ter( get_avatar().pos_bub() ).obj() );
    restore_on_out_of_scope restore_light( source_terrain.light_emitted );
    source_terrain.light_emitted = weak_source ? 1 : 0;
    const int prior_level = getDebugLevel();
    const int prior_class = getDebugClass();
    on_out_of_scope restore_masks( [&] {
        limitDebugLevel( prior_level );
        limitDebugClass( prior_class );
    } );
    limitDebugLevel( mode == 2 ? D_ERROR | D_INFO : D_ERROR );
    limitDebugClass( mode == 2 ? D_MAIN | D_GAME : D_MAIN );
    scoped_transition_environment environment;
    environment.set( "", mode == 1 || mode == 3 ? "disk-control" : "" );
    environment.set_semantic( mode == 1 ? "disk-control" : mode == 3 ? "other" : "" );
    discovery_summary_counting_buffer captured;
    std::ostream &log = DebugLog( D_INFO, DC_ALL );
    auto *original = log.rdbuf( &captured );
    on_out_of_scope restore_log( [&] { log.rdbuf( original ); } );
    for( int i = 0; i < 6; ++i ) {
        calendar::turn += 1_turns;
        run_live_light_delivery_for_test();
        CHECK( live_light::samples_for_turn( calendar::turn ).empty() );
    }
    const std::string text = captured.str();
    const std::string marker = "bandit_live_world signal scan: signal_packet=no kind=smoke/fire/light";
    std::size_t count = 0;
    for( auto pos = text.find( marker ); pos != std::string::npos; pos = text.find( marker, pos + marker.size() ) ) {
        ++count;
    }
    const bool enabled = mode == 1 || mode == 2;
    CHECK( count == ( enabled ? 6 : 0 ) );
    if( enabled ) {
        CHECK( text.find( "tiles_examined=" ) != std::string::npos );
        CHECK( text.find( "discovery_interval_turns=" ) != std::string::npos );
        if( !weak_source ) {
            CHECK( captured.syncs <= 120 );
        }
    }
    const std::string rejected = "bandit_live_world signal rejected: packet=";
    std::size_t rejected_count = 0;
    for( auto pos = text.find( rejected ); pos != std::string::npos;
         pos = text.find( rejected, pos + rejected.size() ) ) {
        ++rejected_count;
    }
    CHECK( rejected_count == ( enabled && weak_source ? 6 : 0 ) );
    CHECK( debug_log_enabled( D_ERROR, D_GAME ) );
}
