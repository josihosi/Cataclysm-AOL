#include <algorithm>
#include <cmath>
#include <chrono>
#include <ctime>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "character.h"
#include "creature_tracker.h"
#include "damage.h"
#include "debug.h"
#include "effect.h"
#include "game.h"
#include "horde_entity.h"
#include "json.h"
#include "item.h"
#include "json_loader.h"
#include "level_cache.h"
#include "map.h"
#include "mapbuffer.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "memory_fast.h"
#include "monster.h"
#include "mongroup.h"
#include "mtype.h"
#include "npc.h"
#include "options_helpers.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "player_helpers.h"
#include "projectile.h"
#include "rng.h"
#include "scout_observation.h"
#include "scent_map.h"
#include "type_id.h"
#include "weather.h"
#include "weather_type.h"
#include "writhing_stalker_ai.h"

TEST_CASE( "stalker_immutable_observation_reader_lifetime_comparison",
           "[.][writhing_stalker][stalker_optical_lifetime]" )
{
    // Diagnostic only: the retained-reader arm uses the existing optical API,
    // not a changed native planner. No physical mutation/turn spans this batch.
    const std::string scene = GENERATE( std::string( "near_roof_day" ),
                                       std::string( "far_house_night" ), std::string( "far_open_day" ) );
    const bool roof = scene == "near_roof_day";
    const bool night = scene == "far_house_night";
    const bool distant = !roof;
    CAPTURE( scene );
    clear_map( -2, 2 );
    clear_npcs();
    clear_avatar();
    const auto old_turn = calendar::turn;
    const auto old_rng = rng_get_engine();
    on_out_of_scope restore( [&]() {
        clear_creatures();
        set_time( old_turn );
        rng_get_engine() = old_rng;
    } );
    set_time( calendar::turn_zero + ( night ? 0_hours : 12_hours ) );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    map &here = get_map();
    auto &you = get_avatar();
    you.setpos( here, tripoint_bub_ms( 66, 60, 0 ) );
    monster &stalker = spawn_test_monster( "mon_writhing_stalker",
                                         tripoint_bub_ms( distant ? 12 : 60, 60, 0 ) );
    monster &near = spawn_test_monster( "mon_zombie", tripoint_bub_ms( 66, 64, 0 ) );
    monster &blocked = spawn_test_monster( "mon_zombie", tripoint_bub_ms( 68, 66, 0 ) );
    for( int y = 50; y <= 80; ++y ) {
        here.ter_set( tripoint_bub_ms( 67, y, 0 ), ter_str_id( "t_wall" ) );
    }
    if( roof ) {
        for( const auto &p : here.points_in_radius( near.pos_bub() + tripoint::above, 12 ) ) {
            if( p.z() == 1 ) { here.ter_set( p, ter_str_id( "t_flat_roof" ) ); }
        }
    }
    if( night ) {
        // Outdoor targets beside a house still request its native aperture
        // inputs for exact light. This is the far night pressure workload,
        // with the same physical scene and eight calls in both arms.
        for( int x = 35; x <= 95; ++x ) {
            for( int y = 76; y <= 100; ++y ) {
                here.ter_set( tripoint_bub_ms( x, y, 0 ), ter_str_id( "t_floor" ) );
                here.ter_set( tripoint_bub_ms( x, y, 1 ), ter_str_id( "t_flat_roof" ) );
            }
        }
    }
    for( int z = -2; z <= OVERMAP_HEIGHT; ++z ) {
        here.build_map_cache( z );
    }
    REQUIRE( stalker.friendly == 0 );
    REQUIRE( stalker.can_see() );
    REQUIRE_FALSE( stalker.is_dead() );
    const scout_observation::actor_capability capability{
        72, 72.0f / std::max( 1, std::max( stalker.type->vision_day, stalker.type->vision_night ) )
    };
    const std::vector<const Creature *> targets{ &you, &near, &you, &blocked,
                &near, &blocked, &you, &near };
    const auto bytes = []( const auto &value ) {
        std::ostringstream out;
        { JsonOut js( out ); value.serialize( js ); }
        return out.str();
    };
    const auto body = bytes( stalker );
    const auto prey = bytes( you );
    const auto peer = bytes( near );
    const auto occluded = bytes( blocked );
    const auto world = bytes( overmap_buffer.global_state );
    const auto now = calendar::turn;
    const auto seed = rng_get_engine();
    const auto residents = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto cache_values = [&]() {
        std::vector<float> result;
        for( int z = -2; z <= OVERMAP_HEIGHT; ++z ) {
            const auto &cache = here.get_cache_ref( z );
            for( int x = 0; x < MAPSIZE_X; ++x ) {
                for( int y = 0; y < MAPSIZE_Y; ++y ) {
                    for( float q : cache.lm[x][y].values ) { result.push_back( q ); }
                    result.push_back( cache.sm[x][y] );
                    result.push_back( cache.transparency_cache[x][y] );
                    result.push_back( cache.outside_cache[x][y] );
                    result.push_back( cache.floor_cache[x][y] );
                }
            }
        }
        return result;
    };
    const auto main_cache = cache_values();
    const auto conserved = [&]() {
        CHECK( bytes( stalker ) == body );
        CHECK( bytes( you ) == prey );
        CHECK( bytes( near ) == peer );
        CHECK( bytes( blocked ) == occluded );
        CHECK( bytes( overmap_buffer.global_state ) == world );
        CHECK( calendar::turn == now );
        CHECK( rng_get_engine() == seed );
        CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == residents );
        CHECK( cache_values() == main_cache );
    };
    {
        scout_observation::site_reader mixed;
        const auto predicate = mixed.read_actor( stalker, near, capability, true );
        const auto exact = mixed.read_actor( stalker, near, capability );
        CHECK( predicate.visible == exact.visible );
        REQUIRE( exact.known );
        // A prior predicate result is not an exact ambient cache entry. A
        // later exact reader must still calculate and publish that value.
        CHECK( mixed.cached_lighting_at( near.pos_abs() ).has_value() );
        conserved();
    }
    for( int repeat = 0; repeat < 3; ++repeat ) {
        std::vector<bool> native;
        std::vector<scout_observation::visibility_read> fresh;
        std::vector<scout_observation::visibility_read> retained;
        std::vector<scout_observation::visibility_read> predicate;
        const auto timed = [&]( const auto &call ) {
            const auto start = std::chrono::steady_clock::now();
            const auto cpu = std::clock();
            call();
            const std::pair<double, double> result{
                std::chrono::duration<double>( std::chrono::steady_clock::now() - start ).count(),
                double( std::clock() - cpu ) / CLOCKS_PER_SEC
            };
            conserved(); // Outside timing and before another arm can run.
            return result;
        };
        size_t fresh_views = 0, fresh_physical = 0, fresh_summaries = 0;
        size_t reused_views = 0, reused_physical = 0, reused_summaries = 0;
        size_t predicate_summaries = 0;
        bool predicate_exact_light = false;
        const auto public_arm = [&]() {
            for( const auto *target : targets ) {
                native.push_back( writhing_stalker::can_observe_actor( stalker, *target ) );
            }
        };
        const auto fresh_arm = [&]() {
            for( const auto *target : targets ) {
                scout_observation::site_reader reader;
                fresh.push_back( reader.read_actor( stalker, *target, capability ) );
                fresh_views += reader.cached_view_count();
                fresh_physical += reader.cached_lighting_submap_count();
                fresh_summaries += reader.cached_sunlight_summary_count();
            }
        };
        const auto retained_arm = [&]() {
            scout_observation::site_reader reader;
            for( const auto *target : targets ) {
                retained.push_back( reader.read_actor( stalker, *target, capability ) );
            }
            reused_views = reader.cached_view_count();
            reused_physical = reader.cached_lighting_submap_count();
            reused_summaries = reader.cached_sunlight_summary_count();
        };
        const auto predicate_arm = [&]() {
            scout_observation::site_reader reader;
            for( const auto *target : targets ) {
                predicate.push_back( reader.read_actor( stalker, *target, capability, true ) );
            }
            predicate_summaries = reader.cached_sunlight_summary_count();
            predicate_exact_light = reader.cached_lighting_at( near.pos_abs() ).has_value();
        };
        std::pair<double, double> public_time, fresh_time, retained_time, predicate_time;
        // Reverse arm order as a bounded warm-order countercontrol.
        if( repeat == 1 ) {
            predicate_time = timed( predicate_arm );
            retained_time = timed( retained_arm );
            fresh_time = timed( fresh_arm );
            public_time = timed( public_arm );
        } else {
            public_time = timed( public_arm );
            fresh_time = timed( fresh_arm );
            retained_time = timed( retained_arm );
            predicate_time = timed( predicate_arm );
        }
        REQUIRE( native.size() == targets.size() );
        REQUIRE( fresh.size() == targets.size() );
        REQUIRE( retained.size() == targets.size() );
        REQUIRE( predicate.size() == targets.size() );
        CHECK( native[0] == !night );
        CHECK( native[1] == !night );
        CHECK_FALSE( native[3] );
        for( size_t i = 0; i < targets.size(); ++i ) {
            CHECK( fresh[i].known );
            CHECK( fresh[i].visible == native[i] );
            CHECK( retained[i].known == fresh[i].known );
            CHECK( retained[i].visible == fresh[i].visible );
            CHECK( retained[i].reason == fresh[i].reason );
            CHECK( predicate[i].visible == fresh[i].visible );
        }
        CHECK( predicate_exact_light == ( distant && !night ) );
        if( !predicate_exact_light ) { CHECK( predicate_summaries == 0 ); }
        CHECK( reused_physical <= fresh_physical );
        CHECK( reused_summaries <= fresh_summaries );
        std::ostringstream out;
        { JsonOut js( out );
            js.start_object();
            js.member( "scene", scene );
            js.member( "repeat", repeat );
            js.member( "queries", targets.size() );
            js.member( "public_fresh_wall_seconds", public_time.first );
            js.member( "public_fresh_process_cpu_seconds", public_time.second );
            js.member( "direct_fresh_wall_seconds", fresh_time.first );
            js.member( "direct_fresh_process_cpu_seconds", fresh_time.second );
            js.member( "decision_retained_wall_seconds", retained_time.first );
            js.member( "decision_retained_process_cpu_seconds", retained_time.second );
            js.member( "predicate_wall_seconds", predicate_time.first );
            js.member( "predicate_process_cpu_seconds", predicate_time.second );
            js.member( "predicate_summaries", predicate_summaries );
            js.member( "fresh_views_sum", fresh_views );
            js.member( "fresh_physical_sum", fresh_physical );
            js.member( "fresh_summaries_sum", fresh_summaries );
            js.member( "retained_views", reused_views );
            js.member( "retained_physical", reused_physical );
            js.member( "retained_summaries", reused_summaries );
            js.end_object();
        }
        std::cout << "STALKER_OPTICAL_LIFETIME " << out.str() << '\n';
    }
}

static const damage_type_id damage_cut( "cut" );
static const mongroup_id GROUP_ZOMBIE( "GROUP_ZOMBIE" );
static const mongroup_id GROUP_ZOMBIE_HORDE( "GROUP_ZOMBIE_HORDE" );
static const mtype_id mon_zombie( "mon_zombie" );
static const mtype_id mon_zombie_runner( "mon_zombie_runner" );
static const mtype_id mon_writhing_stalker( "mon_writhing_stalker" );
static const species_id species_HUMAN( "HUMAN" );
static const species_id species_ZOMBIE( "ZOMBIE" );

namespace
{

// Only NEW fixture births use this factory. Copies, handoffs and reloads
// retain their existing token and must still pass the native ownership guards.
monster new_stalker_fixture_body()
{
    std::optional<monster> fresh;
    for( int attempt = 0; attempt < 256; ++attempt ) {
        fresh.emplace( mon_writhing_stalker );
        const auto &token = fresh->predator_state().actor_id;
        bool owned = overmap_buffer.global_state.predator_activity.find( token ) != nullptr;
        for( const monster &body : g->all_monsters() ) {
            owned = owned || ( body.is_caol_predator() && body.predator_state().actor_id == token );
        }
        if( !owned ) {
            break;
        }
        // Native constructor retries consume RNG; never rewrite a token or
        // reset canonical authority to make a copied identity look new.
        fresh.reset();
    }
    REQUIRE( fresh ); // fail openly if the bounded fixture cannot obtain a birth
    return std::move( *fresh );
}

monster &spawn_new_stalker_fixture( const tripoint_bub_ms &start, bool death_drops = true )
{
    monster probe( mon_writhing_stalker ); // same predicate probe as spawn_test_monster
    REQUIRE( get_creature_tracker().creature_at( start ) == nullptr );
    REQUIRE( probe.will_move_to( start ) );
    REQUIRE( probe.know_danger_at( start ) );
    auto fresh = make_shared_fast<monster>( new_stalker_fixture_body() );
    fresh->ammo = fresh->type->starting_ammo; // native type-placement adapter
    monster *placed = g->place_critter_at( fresh, start );
    REQUIRE( placed );
    REQUIRE( placed == fresh.get() );
    placed->death_drops = death_drops;
    return *placed;
}

const char *decision_trace_name( const writhing_stalker::decision value )
{
    using writhing_stalker::decision;
    switch( value ) {
        case decision::ignore:
            return "ignore";
        case decision::interested:
            return "interested";
        case decision::shadow:
            return "shadow";
        case decision::hold:
            return "hold";
        case decision::strike:
            return "strike";
        case decision::withdraw:
            return "withdraw";
        case decision::cooling_off:
            return "cooling_off";
    }
    return "unknown";
}

const char *route_trace_name( const writhing_stalker::approach_class value )
{
    using writhing_stalker::approach_class;
    switch( value ) {
        case approach_class::none:
            return "none";
        case approach_class::cover_shadow:
            return "cover_shadow";
        case approach_class::edge_shadow:
            return "edge_shadow";
        case approach_class::direct_forced:
            return "direct_forced";
        case approach_class::hold_exposed:
            return "hold_exposed";
    }
    return "unknown";
}

struct stalker_pattern_row {
    int turn = 0;
    int distance = 0;
    int hp_percent = 100;
    int cooldown_turns = 0;
    int strike_count = 0;
    writhing_stalker::decision next = writhing_stalker::decision::ignore;
    writhing_stalker::approach_class route = writhing_stalker::approach_class::none;
    std::string reason;
};

std::string trace_rows( const std::vector<stalker_pattern_row> &rows )
{
    std::ostringstream out;
    for( const stalker_pattern_row &row : rows ) {
        out << 't' << row.turn << " dist=" << row.distance << " hp=" << row.hp_percent
            << " cd=" << row.cooldown_turns << " decision=" << decision_trace_name( row.next )
            << " route=" << route_trace_name( row.route ) << " strikes=" << row.strike_count
            << " reason=" << row.reason << '\n';
    }
    return out.str();
}

writhing_stalker::live_context pattern_base_context()
{
    writhing_stalker::live_context ctx;
    ctx.has_believable_local_evidence = true;
    ctx.cover_route_available = true;
    ctx.direct_open_route_available = true;
    ctx.near_cover_or_clutter = true;
    return ctx;
}

std::vector<stalker_pattern_row> run_vulnerable_stalker_pattern( const int turns )
{
    std::vector<stalker_pattern_row> rows;
    writhing_stalker::live_context ctx = pattern_base_context();
    ctx.player_bleeding = true;
    ctx.player_hurt = true;
    ctx.player_low_stamina = true;
    ctx.player_distracted = true;
    ctx.player_noisy = true;
    ctx.zombie_pressure = 2;

    int distance = 5;
    int cooldown_turns = 0;
    int hp_percent = 100;
    int strike_count = 0;

    for( int turn = 0; turn < turns; ++turn ) {
        ctx.distance_to_target = distance;
        ctx.on_cooldown = cooldown_turns > 0;
        ctx.stalker_hurt = hp_percent <= 55;
        ctx.burst_strikes = strike_count;

        const writhing_stalker::live_response response = writhing_stalker::evaluate_live_response( ctx );
        rows.push_back( stalker_pattern_row{ turn, distance, hp_percent, cooldown_turns, strike_count,
                                             response.next, response.route, response.reason } );

        if( response.next == writhing_stalker::decision::strike ) {
            ++strike_count;
            distance = 2;
            if( strike_count == 2 ) {
                hp_percent = 50;
            }
        } else if( response.next == writhing_stalker::decision::cooling_off ) {
            cooldown_turns = std::max( 0, cooldown_turns - 1 );
            distance = std::min( 5, distance + 1 );
        } else if( response.next == writhing_stalker::decision::shadow ) {
            distance = std::max( 2, distance - 2 );
        } else if( response.next == writhing_stalker::decision::withdraw ) {
            distance += 3;
        }
    }

    return rows;
}

int count_decisions( const std::vector<stalker_pattern_row> &rows,
                     const writhing_stalker::decision next )
{
    int count = 0;
    for( const stalker_pattern_row &row : rows ) {
        if( row.next == next ) {
            ++count;
        }
    }
    return count;
}

void refresh_writhing_stalker_arena( map &here )
{
    g->reset_light_level();
    here.invalidate_visibility_cache();
    here.update_visibility_cache( 0 );
    here.invalidate_map_cache( 0 );
    here.set_transparency_cache_dirty( 0 );
    here.set_pathfinding_cache_dirty( 0 );
    here.build_map_cache( 0 );
}

void prepare_writhing_stalker_arena( map &here, const tripoint_bub_ms &center )
{
    clear_avatar();
    const ter_id t_floor( "t_floor" );
    const ter_id t_wall( "t_wall" );
    for( const tripoint_bub_ms &p : here.points_in_radius( center, 12 ) ) {
        here.ter_set( p, t_floor );
        here.furn_clear( p );
        here.clear_fields( p );
    }

    // A little adjacent clutter gives the live context an honest cover route without
    // blocking the east/west sight line used by the seam-consumption proof.
    here.ter_set( center + point::north, t_wall );
    refresh_writhing_stalker_arena( here );
}

monster round_trip_writhing_stalker( const monster &stalker )
{
    std::ostringstream serialized;
    {
        JsonOut json( serialized, true );
        stalker.serialize( json );
    }
    monster loaded;
    loaded.deserialize( json_loader::from_string( serialized.str() ).get_object() );
    return loaded;
}

} // namespace

TEST_CASE( "stalker_native_plan_reader_parity_and_cost",
           "[.][writhing_stalker][stalker_plan_reader_parity]" )
{
    // The same test is run before/after the private caller change. It emits
    // bounded full state for an external byte-equality comparison, not a new
    // gameplay switch or a test-only production planner.
    const std::string scene = GENERATE( std::string( "open" ), std::string( "roof" ),
                                       std::string( "occluded" ), std::string( "unknown" ) );
    CAPTURE( scene );
    clear_map( -2, 2 );
    clear_npcs();
    clear_avatar();
    const auto old_turn = calendar::turn;
    const auto old_rng = rng_get_engine();
    on_out_of_scope restore( [&]() {
        clear_creatures();
        clear_npcs();
        writhing_stalker::reset_transient_pressure_history();
        set_time( old_turn );
        rng_get_engine() = old_rng;
    } );
    set_time( calendar::turn_zero + ( scene == "unknown" ? 0_hours : 12_hours ) );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    map &here = get_map();
    const tripoint_bub_ms center( scene == "unknown" ? 45 : 65, 65, 0 );
    get_avatar().setpos( here, center );
    monster &stalker = spawn_new_stalker_fixture( scene == "unknown" ?
                       tripoint_bub_ms( 10, 65, 0 ) : center + point( 20, 0 ) );
    stalker.anger = 100;
    stalker.aggro_character = true;
    std::vector<monster *> pressure;
    for( const point offset : { point( -1, 0 ), point( 1, 0 ), point( 0, -1 ) } ) {
        monster &zombie = spawn_test_monster( "mon_zombie", center + offset );
        zombie.aggro_character = true;
        zombie.set_dest( get_avatar().pos_abs() );
        pressure.push_back( &zombie );
    }
    npc &ally = spawn_npc( point_bub_ms( ( center + point( 0, 2 ) ).xy() ), "test_talker" );
    ally.set_fac( faction_id( "your_followers" ) );
    ally.set_attitude( NPCATT_FOLLOW );
    if( scene == "roof" ) {
        for( const auto &p : here.points_in_radius( center + tripoint::above, 7 ) ) {
            if( p.z() == 1 ) { here.ter_set( p, ter_str_id( "t_floor" ) ); }
        }
    }
    if( scene == "occluded" ) {
        for( int y = 50; y <= 80; ++y ) {
            here.ter_set( tripoint_bub_ms( 75, y, 0 ), ter_str_id( "t_wall" ) );
        }
    }
    if( scene == "unknown" ) {
        // A real nonuniform native terrain owner with no generated physical
        // submaps in the required off-ray illumination plane. Missing is not
        // air; neither the original nor candidate may generate it to see.
        const auto first = project_to<coords::sm>( get_avatar().pos_abs() ) -
                           point( MAPSIZE / 2, 0 );
        REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( first ) );
        overmap_buffer.ter_set( project_to<coords::omt>( first ), oter_id( "forest" ) );
    }
    for( int z = -2; z <= OVERMAP_HEIGHT; ++z ) { here.build_map_cache( z ); }
    const auto bytes = []( const auto &value ) {
        std::ostringstream out;
        { JsonOut js( out ); value.serialize( js ); }
        return out.str();
    };
    scout_observation::visibility_read observed;
    size_t summaries = 0;
    {
        scout_observation::site_reader reader;
        observed = reader.read_actor( stalker, get_avatar(),
        { 72, 72.0f / std::max( 1, std::max( stalker.type->vision_day, stalker.type->vision_night ) ) } );
        summaries = reader.cached_sunlight_summary_count();
    }
    if( scene == "unknown" ) {
        CHECK_FALSE( observed.known );
        CHECK_FALSE( observed.visible );
    } else {
        CHECK( observed.known );
        CHECK( observed.visible == ( scene != "occluded" ) );
    }
    if( scene == "roof" ) { CHECK( summaries > 0 ); }
    const monster initial( stalker );
    const auto initial_world = overmap_buffer.global_state;
    const auto initial_player = bytes( get_avatar() );
    const auto initial_ally = bytes( ally );
    std::vector<std::string> initial_pressure;
    for( const auto *zombie : pressure ) { initial_pressure.push_back( bytes( *zombie ) ); }
    const auto seed = rng_get_engine();
    const auto now = calendar::turn;
    const auto residents = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto cache_values = [&]() {
        std::vector<float> values;
        for( int z = -2; z <= OVERMAP_HEIGHT; ++z ) {
            const auto &cache = here.get_cache_ref( z );
            for( int x = 0; x < MAPSIZE_X; ++x ) {
                for( int y = 0; y < MAPSIZE_Y; ++y ) {
                    for( float q : cache.lm[x][y].values ) { values.push_back( q ); }
                    values.push_back( cache.sm[x][y] );
                    values.push_back( cache.transparency_cache[x][y] );
                    values.push_back( cache.outside_cache[x][y] );
                    values.push_back( cache.floor_cache[x][y] );
                }
            }
        }
        return values;
    };
    const auto native_cache = cache_values();
    for( int repeat = 0; repeat < 3; ++repeat ) {
        stalker = initial;
        overmap_buffer.global_state = initial_world;
        rng_get_engine() = seed;
        writhing_stalker::reset_transient_pressure_history();
        get_creature_tracker().invalidate_reachability_cache();
        const auto started = std::chrono::steady_clock::now();
        const auto cpu = std::clock();
        stalker.plan(); // Actual public native target selection and planner.
        const double cpu_seconds = double( std::clock() - cpu ) / CLOCKS_PER_SEC;
        const double seconds = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - started ).count();
        CHECK( bytes( get_avatar() ) == initial_player );
        CHECK( bytes( ally ) == initial_ally );
        for( size_t i = 0; i < pressure.size(); ++i ) {
            CHECK( bytes( *pressure[i] ) == initial_pressure[i] );
        }
        CHECK( calendar::turn == now );
        CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == residents );
        CHECK( cache_values() == native_cache );
        std::ostringstream rng_state;
        rng_state << rng_get_engine();
        std::ostringstream out;
        { JsonOut js( out );
            js.start_object();
            js.member( "scene", scene );
            js.member( "repeat", repeat );
            js.member( "wall_seconds", seconds );
            js.member( "process_cpu_seconds", cpu_seconds );
            js.member( "known", observed.known );
            js.member( "visible", observed.visible );
            js.member( "reason", observed.reason );
            js.member( "probe_solar_summaries", summaries );
            js.member( "stalker", bytes( stalker ) );
            js.member( "player", bytes( get_avatar() ) );
            js.member( "ally", bytes( ally ) );
            js.member( "pressure", initial_pressure );
            js.member( "world", bytes( overmap_buffer.global_state ) );
            js.member( "rng", rng_state.str() );
            js.member( "pressure_samples", writhing_stalker::transient_pressure_sample_count() );
            js.end_object();
        }
        std::cout << "STALKER_PLAN_PARITY " << out.str() << '\n';
    }
}

TEST_CASE( "stalker_new_fixture_birth_keeps_foreign_native_token_authority",
           "[writhing_stalker][monster][fixture_ownership]" )
{
    const auto original_rng = rng_get_engine();
    on_out_of_scope restore_rng( [&] { rng_get_engine() = original_rng; } );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    on_out_of_scope cleanup( [] { clear_map_without_vision(); } );
    clear_map_without_vision();
    clear_avatar();
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    get_avatar().setpos( here, center );
    const auto birth_rng = rng_get_engine();
    monster &first = spawn_new_stalker_fixture( center + point::east * 2 );
    REQUIRE( overmap_buffer.predator_activity_for( first,
             predator_activation::representation::local, first.pos_abs() ) );
    const auto id = first.predator_state().actor_id;
    const auto position = first.pos_abs();
    const auto epoch = first.predator_state().handoff_epoch;
    auto retained = get_creature_tracker().find( position );
    REQUIRE( retained.get() == &first );
    g->remove_zombie( first ); // absent is not dead or authority to reuse its token
    std::ostringstream before;
    { JsonOut json( before ); retained->serialize( json ); }
    const auto turn = calendar::turn;
    const auto map_origin = here.get_abs_sub();
    rng_get_engine() = birth_rng; // reproduce the independent fixture's native draws
    monster &second = spawn_new_stalker_fixture( center + point::east * 5 );
    CHECK( second.predator_state().actor_id != id );
    CHECK( second.predator_state().handoff_epoch == 0 );
    CHECK( get_creature_tracker().find( second.pos_abs() ).get() == &second );
    auto &world = overmap_buffer.global_state.predator_activity;
    CHECK_FALSE( world.confirm( id, mon_writhing_stalker.str(), epoch,
                               predator_activation::representation::local, second.pos_abs(),
                               to_turn<int>( calendar::turn ) ) );
    CHECK( world.owns( id, epoch, predator_activation::representation::local, position ) );
    std::ostringstream after;
    { JsonOut json( after ); retained->serialize( json ); }
    CHECK( after.str() == before.str() );
    CHECK( calendar::turn == turn );
    CHECK( here.get_abs_sub() == map_origin );
    REQUIRE( overmap_buffer.predator_activity_for( second,
             predator_activation::representation::local, second.pos_abs() ) );
    CHECK( world.owns( second.predator_state().actor_id, 0,
                       predator_activation::representation::local, second.pos_abs() ) );
    CHECK( overmap_buffer.predator_active_time( second ) == 0 );
}

TEST_CASE( "stalker_new_abstract_fixture_birth_preserves_prior_owner_and_reload",
           "[writhing_stalker][predator_activation][native][fixture_ownership]" )
{
    const auto original_rng = rng_get_engine();
    on_out_of_scope restore_rng( [&] { rng_get_engine() = original_rng; } );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    restore_on_out_of_scope restore_time( calendar::turn );
    const auto old_avatar = get_avatar().pos_abs();
    clear_map_and_put_player_underground();
    on_out_of_scope clear_local( [&] {
        clear_map_without_vision();
        get_avatar().setpos( old_avatar, false );
    } );
    set_time( calendar::turn_zero + 100000_turns );
    map &here = get_map();
    const auto source = here.get_abs( tripoint_bub_ms( -36, 65, 0 ) );
    const auto other = source + point::east * 3;
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    std::string first_id;
    std::string second_id;
    on_out_of_scope cleanup( [&] {
        om.release_monster_payload( source, first_id );
        om.release_monster_payload( other, second_id );
    } );
    REQUIRE( overmap_buffer.entity_at( source ) == nullptr );
    REQUIRE( overmap_buffer.entity_at( other ) == nullptr );
    const auto birth_rng = rng_get_engine();
    monster first = new_stalker_fixture_body();
    first_id = first.predator_state().actor_id;
    first.set_pos_abs_only( source );
    first.set_moves( 0 );
    REQUIRE( om.hordes.spawn_entity( source, first ) );
    auto *first_owner = overmap_buffer.entity_at( source );
    REQUIRE( first_owner );
    REQUIRE( first_owner->monster_data );
    REQUIRE( overmap_buffer.predator_activity_at( *first_owner, source ) );
    REQUIRE( overmap_buffer.predator_activity_for( *first_owner->monster_data,
             predator_activation::representation::abstract, source ) );
    auto &world = overmap_buffer.global_state.predator_activity;
    const auto bytes = []( const monster &body ) {
        std::ostringstream out;
        { JsonOut json( out ); body.serialize( json ); }
        return out.str();
    };
    const auto original_body = bytes( *first_owner->monster_data );
    const auto origin = here.get_abs_sub();
    const auto now = calendar::turn;
    // The same genuine constructor draw collides with a prior native owner.
    // Only a NEW birth may retry; the old body's copy must remain refused.
    rng_get_engine() = birth_rng;
    monster collision( mon_writhing_stalker );
    REQUIRE( world.find( collision.predator_state().actor_id ) != nullptr );
    rng_get_engine() = birth_rng;
    monster second = new_stalker_fixture_body();
    second_id = second.predator_state().actor_id;
    CHECK( second_id != first_id );
    CHECK( world.find( second_id ) == nullptr ); // factory does not authenticate
    second.set_pos_abs_only( other );
    second.set_moves( 0 );
    REQUIRE( om.hordes.spawn_entity( other, second ) );
    auto *second_owner = overmap_buffer.entity_at( other );
    REQUIRE( second_owner );
    REQUIRE( second_owner->monster_data );
    REQUIRE( overmap_buffer.predator_activity_at( *second_owner, other ) );
    REQUIRE( overmap_buffer.predator_activity_for( *second_owner->monster_data,
             predator_activation::representation::abstract, other ) );
    CHECK( overmap_buffer.predator_active_time( *second_owner->monster_data ) == 0 );
    CHECK( second_owner->monster_data->writhing_stalker_state().active_turns == 0 );
    CHECK( second_owner->monster_data->get_moves() == 0 );
    CHECK( world.owns( first_id, 0, predator_activation::representation::abstract, source ) );
    monster foreign = *first_owner->monster_data;
    foreign.set_pos_abs_only( other );
    const auto refused = bytes( foreign );
    CHECK_FALSE( overmap_buffer.predator_activity_for( foreign,
                 predator_activation::representation::abstract, other ) );
    CHECK( bytes( foreign ) == refused );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        *second_owner->monster_data = round_trip_writhing_stalker( *second_owner->monster_data );
        om.hordes.refresh_entity_bucket( other );
    }
    const auto loaded = bytes( *second_owner->monster_data );
    REQUIRE( overmap_buffer.predator_activity_for( *second_owner->monster_data,
             predator_activation::representation::abstract, other ) );
    CHECK( bytes( *second_owner->monster_data ) == loaded );
    CHECK( second_owner->monster_data->predator_state().actor_id == second_id );
    CHECK( second_owner->monster_data->predator_state().handoff_epoch == 0 );
    CHECK( world.owns( second_id, 0, predator_activation::representation::abstract, other ) );
    CHECK( bytes( *first_owner->monster_data ) == original_body );
    CHECK_FALSE( overmap_buffer.predator_activity_for( foreign,
                 predator_activation::representation::abstract, other ) );
    CHECK( bytes( foreign ) == refused );
    CHECK( calendar::turn == now );
    CHECK( here.get_abs_sub() == origin );
}

TEST_CASE( "stalker_activation_query_preserves_native_intent_owner",
           "[writhing_stalker][predator_activation][native][intent_owner]" )
{
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    restore_on_out_of_scope restore_time( calendar::turn );
    const auto old_avatar = get_avatar().pos_abs();
    on_out_of_scope cleanup( [&] {
        clear_map_without_vision();
        get_avatar().setpos( old_avatar, false );
    } );
    clear_map_without_vision();
    set_time( calendar::turn_zero + 100000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    get_avatar().setpos( here, center );
    monster &body = spawn_new_stalker_fixture( center + point::east * 2 );
    const auto position = body.pos_abs();
    REQUIRE( overmap_buffer.predator_activity_for( body,
             predator_activation::representation::local, position ) );
    auto &state = body.writhing_stalker_state();
    const int now = to_turn<int>( calendar::turn );
    state.advance_to( now );
    REQUIRE( state.observe_light_interest( position + point::east, "query-owner", now, 1 ) );
    const auto intent_bytes = [&]() {
        std::ostringstream out;
        { JsonOut json( out ); state.serialize( json ); }
        return out.str();
    };
    const auto before = intent_bytes();
    const auto consumed = body.predator_state().activity_active_turns;
    const auto moves = body.get_moves();
    calendar::turn += 2_turns;
    REQUIRE( overmap_buffer.predator_activity_for( body,
             predator_activation::representation::local, position ) );
    CHECK( intent_bytes() == before );
    CHECK( body.predator_state().activity_active_turns == consumed );
    CHECK( body.get_moves() == moves );
    CHECK( body.pos_abs() == position );
    REQUIRE( overmap_buffer.predator_activity_for( body,
             predator_activation::representation::local, position ) );
    CHECK( intent_bytes() == before );
}

TEST_CASE( "stalker_native_work_consumes_once_across_pause_and_transfer",
           "[writhing_stalker][predator_activation][native][intent_owner]" )
{
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    restore_on_out_of_scope restore_time( calendar::turn );
    const auto old_avatar = get_avatar().pos_abs();
    overmap *abstract_owner = nullptr;
    std::string actor;
    tripoint_abs_ms position;
    on_out_of_scope cleanup( [&] {
        clear_map_without_vision();
        if( abstract_owner ) {
            abstract_owner->release_monster_payload( position, actor );
        }
        get_avatar().setpos( old_avatar, false );
    } );
    clear_map_without_vision();
    set_time( calendar::turn_zero + 100000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    get_avatar().setpos( here, center );
    monster &body = spawn_new_stalker_fixture( center + point::east * 2 );
    position = body.pos_abs();
    actor = body.predator_state().actor_id;
    const auto bytes = []( const monster &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    const auto query = [&]() {
        return overmap_buffer.predator_activity_for( body,
                predator_activation::representation::local, position );
    };
    const auto advance = [&]() {
        writhing_stalker::advance_active_work( body, to_turn<int>( calendar::turn ) );
    };
    REQUIRE( query() );
    advance();
    auto &state = body.writhing_stalker_state();
    REQUIRE( state.observe_light_interest( position + point::east, "native-owner",
                                         to_turn<int>( calendar::turn ), 1 ) );
    calendar::turn += 2_turns;
    REQUIRE( query() );
    CHECK( body.predator_state().activity_active_turns == 0 );
    advance();
    CHECK( state.active_turns == 2 );
    CHECK( body.predator_state().activity_active_turns == 2 );
    CHECK( state.phase == writhing_stalker::lifecycle_phase::idle );
    CHECK_FALSE( state.has_committed_waypoint );
    const auto once = bytes( body );
    advance();
    REQUIRE( query() );
    advance();
    CHECK( bytes( body ) == once );

    monster foreign = round_trip_writhing_stalker( body );
    ++foreign.predator_state().handoff_epoch;
    const auto refused = bytes( foreign );
    CHECK_FALSE( overmap_buffer.predator_activity_for( foreign,
                 predator_activation::representation::local, position ) );
    writhing_stalker::advance_active_work( foreign, to_turn<int>( calendar::turn ) );
    CHECK( bytes( foreign ) == refused );
    CHECK( bytes( body ) == once );

    const auto current_state = state;
    state.advance_to( to_turn<int>( calendar::turn ) + 20, 0 );
    calendar::turn += 1_turns;
    REQUIRE( query() );
    const auto future = bytes( body );
    advance();
    CHECK( bytes( body ) == future );
    state = current_state;
    // This one authenticated active turn is still unconsumed at suspension.
    const auto where = project_to<coords::omt>( position );
    get_avatar().setpos( project_to<coords::ms>( where - point( 41, 0 ) ), false );
    CHECK_FALSE( query() );
    const auto frozen = bytes( body );
    calendar::turn += 30_turns;
    CHECK_FALSE( query() );
    advance();
    CHECK( bytes( body ) == frozen );
    body = round_trip_writhing_stalker( body );
    CHECK( bytes( body ) == frozen );
    advance();
    CHECK( bytes( body ) == frozen );
    get_avatar().setpos( project_to<coords::ms>( where - point( 40, 0 ) ), false );
    REQUIRE( query() );
    advance();
    CHECK( state.active_turns == 3 );
    CHECK( body.predator_state().activity_active_turns == 3 );
    const auto resumed = bytes( body );
    advance();
    REQUIRE( query() );
    CHECK( bytes( body ) == resumed );
    calendar::turn += 1_turns;
    REQUIRE( query() );
    advance();
    CHECK( state.active_turns == 4 );
    CHECK( body.predator_state().activity_active_turns == 4 );
    const int moves = body.get_moves();

    g->despawn_monster( body );
    abstract_owner = overmap_buffer.find_loaded_overmap( project_to<coords::om>( position ).xy() );
    REQUIRE( abstract_owner );
    auto *owner = overmap_buffer.entity_at( position );
    REQUIRE( owner );
    REQUIRE( owner->monster_data );
    calendar::turn += 1_turns;
    monster &payload = *owner->monster_data;
    REQUIRE( overmap_buffer.predator_activity_for( payload,
             predator_activation::representation::abstract, position ) );
    writhing_stalker::advance_active_work( payload, to_turn<int>( calendar::turn ) );
    CHECK( payload.writhing_stalker_state().active_turns == 5 );
    CHECK( payload.predator_state().activity_active_turns == 5 );
    CHECK( payload.get_moves() == moves );
    CHECK( payload.pos_abs() == position );
    const auto abstract_once = bytes( payload );
    writhing_stalker::advance_active_work( payload, to_turn<int>( calendar::turn ) );
    CHECK( bytes( payload ) == abstract_once );

    // The actual abstract sound writer and native intent owner retire an
    // arrived carrier once, after taking their pre-advance ownership snapshot.
    calendar::turn += 1_turns;
    abstract_owner->hordes.signal_entities( position, 20 );
    owner = overmap_buffer.entity_at( position );
    REQUIRE( owner );
    REQUIRE( owner->monster_data );
    monster &signaled = *owner->monster_data;
    REQUIRE( signaled.provocative_sound );
    REQUIRE( signaled.wandf > 0 );
    REQUIRE( overmap_buffer.predator_activity_for( signaled,
             predator_activation::representation::abstract, position ) );
    REQUIRE( owner->advance_predator_intent( position, to_turn<int>( calendar::turn ), 1 ) );
    CHECK_FALSE( signaled.provocative_sound );
    CHECK( signaled.wandf == 0 );
    CHECK( owner->tracking_intensity == 0 );
    CHECK( signaled.writhing_stalker_state().active_turns == 6 );
    CHECK( signaled.predator_state().activity_active_turns == 6 );
    const auto retired = bytes( signaled );
    REQUIRE( owner->advance_predator_intent( position, to_turn<int>( calendar::turn ), 1 ) );
    CHECK( bytes( signaled ) == retired );

    // Exact native materialization prepares a disposable body. An occupied
    // destination must retain the original body and consumption stamp, then
    // allow a genuine retry without spending either elapsed interval twice.
    monster &blocker = spawn_test_monster( mon_zombie.str(), here.get_bub( position ) );
    calendar::turn += 1_turns;
    overmap_buffer.spawn_monster( project_to<coords::sm>( position ), false );
    owner = overmap_buffer.entity_at( position );
    REQUIRE( owner );
    REQUIRE( owner->monster_data );
    CHECK( bytes( *owner->monster_data ) == retired );
    CHECK( get_creature_tracker().find( position ).get() == &blocker );
    g->remove_zombie( blocker );
    overmap_buffer.spawn_monster( project_to<coords::sm>( position ), false );
    CHECK( overmap_buffer.entity_at( position ) == nullptr );
    auto restored = get_creature_tracker().find( position );
    REQUIRE( restored );
    REQUIRE( restored->predator_state().actor_id == actor );
    writhing_stalker::advance_active_work( *restored, to_turn<int>( calendar::turn ) );
    CHECK( restored->writhing_stalker_state().active_turns == 7 );
    CHECK( restored->predator_state().activity_active_turns == 7 );
    const auto retried = bytes( *restored );
    writhing_stalker::advance_active_work( *restored, to_turn<int>( calendar::turn ) );
    CHECK( bytes( *restored ) == retried );
}

TEST_CASE( "stalker_light_anchor_is_independent_of_native_consumption_stamp",
           "[writhing_stalker][predator_activation][native][intent_owner][light]" )
{
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    restore_on_out_of_scope restore_time( calendar::turn );
    const auto old_avatar = get_avatar().pos_abs();
    on_out_of_scope cleanup( [&] {
        clear_map_without_vision();
        get_avatar().setpos( old_avatar, false );
    } );
    clear_map_without_vision();
    set_time( calendar::turn_zero + 600_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    get_avatar().setpos( here, center );
    monster &body = spawn_new_stalker_fixture( center + point::east * 2 );
    const auto position = body.pos_abs();
    REQUIRE_FALSE( body.predator_state().activity_bound );
    REQUIRE( overmap_buffer.confirm_predator_position( body,
             predator_activation::representation::local, position ) );
    overmap_buffer.global_state.predator_activity.refresh( get_avatar().pos_abs_omt().xy(),
            600, overmap_buffer.global_state.zombie_rider_bands );
    calendar::turn += 10_turns;
    // A genuine native owner was authenticated at600, while its legacy body
    // has not yet bound the interval. Project remaining carrier work first.
    overmap_buffer.retain_predator_light_work( body, "legacy-anchor", 600, 60 );
    CHECK( body.predator_state().light_work_until_active == 50 );
    REQUIRE( overmap_buffer.predator_activity_for( body,
             predator_activation::representation::local, position ) );
    CHECK( body.predator_state().light_work_until_active == 60 );
    CHECK( body.predator_state().activity_active_turns == 0 );
    writhing_stalker::advance_active_work( body, 610 );
    CHECK( body.writhing_stalker_state().active_turns == 10 );
    CHECK( body.predator_state().activity_active_turns == 10 );
    CHECK( body.predator_state().light_work_until_active == 60 );
    calendar::turn += 2_turns;
    REQUIRE( overmap_buffer.predator_activity_for( body,
             predator_activation::representation::local, position ) );
    CHECK( body.predator_state().activity_active_turns == 10 );
    overmap_buffer.retain_predator_light_work( body, "fresh-anchor", 612, 30 );
    CHECK( body.predator_state().light_work_until_active == 42 );
    writhing_stalker::advance_active_work( body, 612 );
    CHECK( body.writhing_stalker_state().active_turns == 12 );
    CHECK( body.predator_state().activity_active_turns == 12 );
    CHECK( body.predator_state().light_work_until_active == 42 );
}

TEST_CASE( "stalker_first_authentication_never_invents_newborn_elapsed",
           "[writhing_stalker][predator_activation][native][intent_owner]" )
{
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    restore_on_out_of_scope restore_time( calendar::turn );
    const auto old_avatar = get_avatar().pos_abs();
    on_out_of_scope cleanup( [&] {
        clear_map_without_vision();
        get_avatar().setpos( old_avatar, false );
    } );
    clear_map_without_vision();
    set_time( calendar::turn_zero + 100_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    get_avatar().setpos( here, center );
    monster &body = spawn_new_stalker_fixture( center + point::east * 2 );
    const bool authenticate_at_birth = GENERATE( false, true );
    if( authenticate_at_birth ) {
        REQUIRE( overmap_buffer.predator_activity_for( body,
                 predator_activation::representation::local, body.pos_abs() ) );
    }
    calendar::turn += 1_turns;
    REQUIRE( overmap_buffer.predator_activity_for( body,
             predator_activation::representation::local, body.pos_abs() ) );
    writhing_stalker::advance_active_work( body, 101 );
    const auto expected = authenticate_at_birth ? 1 : 0;
    CHECK( body.writhing_stalker_state().active_turns == expected );
    CHECK( body.predator_state().activity_active_turns == expected );
    writhing_stalker::advance_active_work( body, 101 );
    CHECK( body.writhing_stalker_state().active_turns == expected );
}

TEST_CASE( "stalker_zero_credit_native_opportunity_keeps_current_observation",
           "[writhing_stalker][predator_activation][native][intent_owner][zero_credit]" )
{
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    restore_on_out_of_scope restore_time( calendar::turn );
    const auto old_avatar = get_avatar().pos_abs();
    overmap *native_owner = nullptr;
    std::string actor;
    tripoint_abs_ms position;
    on_out_of_scope cleanup( [&] {
        clear_map_without_vision();
        if( native_owner ) { native_owner->release_monster_payload( position, actor ); }
        get_avatar().setpos( old_avatar, false );
    } );
    clear_map_without_vision();
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    get_avatar().setpos( here, center );
    monster &local = spawn_new_stalker_fixture( center + point::east * 2 );
    position = local.pos_abs();
    actor = local.predator_state().actor_id;
    const auto where = project_to<coords::omt>( position );
    REQUIRE( overmap_buffer.predator_activity_for( local,
             predator_activation::representation::local, position ) );
    g->despawn_monster( local );
    native_owner = overmap_buffer.find_loaded_overmap( project_to<coords::om>( position ).xy() );
    REQUIRE( native_owner );
    auto *entry = overmap_buffer.entity_at( position );
    REQUIRE( entry );
    REQUIRE( entry->monster_data );
    monster &body = *entry->monster_data;
    const auto bytes = []( const monster &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    get_avatar().setpos( project_to<coords::ms>( where - point( 41, 0 ) ), false );
    CHECK_FALSE( overmap_buffer.predator_activity_for( body,
                 predator_activation::representation::abstract, position ) );
    const auto frozen = bytes( body );
    calendar::turn += 30_turns;
    REQUIRE( entry->advance_predator_intent( position, 1030, 0 ) );
    CHECK( bytes( body ) == frozen );
    get_avatar().setpos( project_to<coords::ms>( where - point( 40, 0 ) ), false );
    int elapsed = -1;
    REQUIRE( overmap_buffer.predator_activity_for( body,
             predator_activation::representation::abstract, position, &elapsed ) );
    REQUIRE( elapsed == 0 );
    native_owner->hordes.signal_entities( position, 20 );
    entry = overmap_buffer.entity_at( position );
    REQUIRE( entry );
    REQUIRE( entry->monster_data );
    monster &heard = *entry->monster_data;
    REQUIRE( heard.provocative_sound );
    REQUIRE( heard.wandf > 0 );
    const auto moves = heard.get_moves();
    const auto active = heard.writhing_stalker_state().active_turns;
    REQUIRE( entry->advance_predator_intent( position, 1030, elapsed ) );
    CHECK_FALSE( heard.provocative_sound );
    CHECK( heard.wandf == 0 );
    CHECK( entry->tracking_intensity == 0 );
    CHECK( heard.writhing_stalker_state().active_turns == active );
    CHECK( heard.predator_state().activity_active_turns == 0 );
    CHECK( heard.get_moves() == moves );
    CHECK( heard.pos_abs() == position );
    const auto once = bytes( heard );
    REQUIRE( entry->advance_predator_intent( position, 1030, 0 ) );
    CHECK( bytes( heard ) == once );

    calendar::turn += 1_turns;
    horde_entity foreign( heard );
    ++foreign.monster_data->predator_state().handoff_epoch;
    const auto refused = bytes( *foreign.monster_data );
    REQUIRE( foreign.advance_predator_intent( position, 1031, 0 ) );
    CHECK( bytes( *foreign.monster_data ) == refused );
    get_avatar().setpos( project_to<coords::ms>( where - point( 41, 0 ) ), false );
    CHECK_FALSE( overmap_buffer.predator_activity_for( heard,
                 predator_activation::representation::abstract, position ) );
    const auto refrozen = bytes( heard );
    REQUIRE( entry->advance_predator_intent( position, 1031, 0 ) );
    CHECK( bytes( heard ) == refrozen );
    get_avatar().setpos( project_to<coords::ms>( where - point( 40, 0 ) ), false );
    REQUIRE( overmap_buffer.predator_activity_for( heard,
             predator_activation::representation::abstract, position ) );
    heard.writhing_stalker_state().advance_to( 1041, 0 );
    const auto future = bytes( heard );
    REQUIRE( entry->advance_predator_intent( position, 1031, 0 ) );
    CHECK( bytes( heard ) == future );
}

TEST_CASE( "writhing_stalker_monster_footing", "[writhing_stalker][monster]" )
{
    const mtype &stalker = *mon_writhing_stalker;
    const std::string exact_description =
        "A gaunt, trembling figure keeps to the edges of cover, its long arms clutched around a body "
        "that never stops writhing.  Its frantic eyes follow each struggle, and its wet teeth show "
        "whenever someone falters.";

    CHECK( stalker.nname() == "writhing stalker" );
    CHECK( stalker.get_description() == exact_description );
    CHECK( stalker.in_species( species_ZOMBIE ) );
    CHECK( stalker.in_species( species_HUMAN ) );
    CHECK( stalker.hp >= 85 );
    CHECK( stalker.hp <= 100 );
    CHECK( stalker.speed >= 115 );
    CHECK( stalker.speed <= 125 );
    CHECK( stalker.sk_dodge >= 4 );
    CHECK( stalker.sk_dodge <= 5 );
    CHECK( stalker.melee_damage.type_damage( damage_cut ) >= 4.0f );
    CHECK( stalker.has_special_attack( "scratch" ) );
    CHECK( stalker.has_special_attack( "bite" ) );
    CHECK( stalker.has_flag( mon_flag_KEENNOSE ) );
    CHECK( stalker.has_flag( mon_flag_PATH_AVOID_DANGER ) );
    CHECK( stalker.has_flag( mon_flag_HARDTOSHOOT ) );
    // The stalker now owns its burst/fade cadence in the custom live planner; the generic
    // HIT_AND_RUN flag would force one-hit fleeing before the bounded burst can happen.
    CHECK_FALSE( stalker.has_flag( mon_flag_HIT_AND_RUN ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_UNBREAKABLE_MORALE ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_GRABS ) );
    CHECK_FALSE( stalker.has_flag( mon_flag_GROUP_BASH ) );
    CHECK( stalker.has_anger_trigger( mon_trigger::STALK ) );
    CHECK( stalker.has_anger_trigger( mon_trigger::HOSTILE_WEAK ) );
    CHECK( stalker.has_fear_trigger( mon_trigger::HURT ) );
    CHECK( stalker.has_fear_trigger( mon_trigger::BRIGHT_LIGHT ) );
    CHECK_FALSE( stalker.upgrades );
}

TEST_CASE( "writhing_stalker_spawn_footing_is_early_city_singleton",
           "[writhing_stalker][monster][mongroup]" )
{
    const MonsterGroup &group = GROUP_ZOMBIE.obj();
    const MonsterGroup &horde_group = GROUP_ZOMBIE_HORDE.obj();

    std::optional<MonsterGroupEntry> stalker_entry;
    std::optional<MonsterGroupEntry> runner_entry;
    int direct_entries = 0;
    int direct_runner_entries = 0;
    for( const MonsterGroupEntry &entry : group.monsters ) {
        if( entry.is_group() ) {
            continue;
        }
        if( entry.mtype == mon_writhing_stalker ) {
            stalker_entry = entry;
            direct_entries++;
        }
        if( entry.mtype == mon_zombie_runner ) {
            runner_entry = entry;
            direct_runner_entries++;
        }
    }

    REQUIRE( direct_entries == 1 );
    REQUIRE( stalker_entry.has_value() );
    CHECK( stalker_entry->frequency == 200 );
    CHECK( stalker_entry->cost_multiplier == 25 );
    CHECK( stalker_entry->pack_minimum == 1 );
    CHECK( stalker_entry->pack_maximum == 1 );
    CHECK( stalker_entry->starts == 0_turns );
    // Runner weight is the established nearby early-city calibration used for
    // this starting value; it does not impose a global encounter-rate quota.
    REQUIRE( direct_runner_entries == 1 );
    REQUIRE( runner_entry.has_value() );
    CHECK( stalker_entry->frequency == runner_entry->frequency );

    std::optional<MonsterGroupEntry> horde_stalker_entry;
    std::optional<MonsterGroupEntry> horde_runner_entry;
    int horde_direct_entries = 0;
    int horde_runner_entries = 0;
    for( const MonsterGroupEntry &entry : horde_group.monsters ) {
        if( !entry.is_group() && entry.mtype == mon_writhing_stalker ) {
            horde_stalker_entry = entry;
            horde_direct_entries++;
        }
        if( !entry.is_group() && entry.mtype == mon_zombie_runner ) {
            horde_runner_entry = entry;
            horde_runner_entries++;
        }
    }

    REQUIRE( horde_direct_entries == 1 );
    REQUIRE( horde_stalker_entry.has_value() );
    CHECK( horde_stalker_entry->frequency == 200 );
    CHECK( horde_stalker_entry->cost_multiplier == 1 );
    CHECK( horde_stalker_entry->pack_minimum == 1 );
    CHECK( horde_stalker_entry->pack_maximum == 1 );
    CHECK( horde_stalker_entry->starts == 0_turns );
    REQUIRE( horde_runner_entries == 1 );
    REQUIRE( horde_runner_entry.has_value() );
    CHECK( horde_stalker_entry->frequency == horde_runner_entry->frequency );
}

TEST_CASE( "writhing_stalker_live_plan_consumes_quiet_side_cutoff_seam",
           "[writhing_stalker][monster][map][zombie_shadow]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms stalker_start = center + point::east * 5;
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 0_hours );
    refresh_writhing_stalker_arena( here );

    monster &stalker = spawn_new_stalker_fixture( stalker_start );
    monster &pressure_a = spawn_test_monster( mon_zombie.str(), center + point::east * 3 );
    monster &pressure_b = spawn_test_monster( mon_zombie.str(),
                          center + point::east * 4 + point::north );
    monster &pressure_c = spawn_test_monster( mon_zombie.str(),
                          center + point::east * 2 + point::south );
    // Pressure is target-directed evidence, not mere proximity.  Give the
    // fixture the same target commitment that the live resolver observes.
    pressure_a.set_dest( you.pos_abs() );
    pressure_b.set_dest( you.pos_abs() );
    pressure_c.set_dest( you.pos_abs() );
    pressure_a.aggro_character = true;
    pressure_b.aggro_character = true;
    pressure_c.aggro_character = true;
    stalker.anger = 100;
    stalker.aggro_character = true;

    REQUIRE( stalker.sees( here, you ) );
    const int evidence_turn = to_turn<int>( calendar::turn );
    writhing_stalker::record_observed_attack( &stalker, &pressure_a, &you, evidence_turn,
            writhing_stalker::next_resolution_id() );
    writhing_stalker::record_observed_attack( &stalker, &pressure_b, &you, evidence_turn,
            writhing_stalker::next_resolution_id() );
    writhing_stalker::record_observed_attack( &stalker, &pressure_c, &you, evidence_turn,
            writhing_stalker::next_resolution_id() );
    stalker.plan();

    const tripoint_bub_ms dest = here.get_bub( stalker.get_dest() );
    CHECK( dest.x() < you.pos_bub().x() );
    CHECK( rl_dist( dest, you.pos_bub() ) >= 2 );
    CHECK( rl_dist( dest, you.pos_bub() ) <= 4 );
    CHECK_FALSE( dest == you.pos_bub() );
    clear_map_without_vision();
}

TEST_CASE( "writhing_stalker_live_plan_is_independent_of_diagnostic_filter",
           "[writhing_stalker][monster][map][debug]" )
{
    // Both arms create disposable native bodies. Catch resets their RNG in
    // the next independent test, so do not leave these bodies' durable tokens
    // in the world after their local owners have been cleared.
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    on_out_of_scope clear_fixture_bodies( [] { clear_map_without_vision(); } );
    struct plan_state {
        tripoint_abs_ms destination;
        tripoint_abs_ms wander_destination;
        int wander_range;
        int moves;
        int anger;
        bool aggro_character;
        time_point turn;
    };

    const auto run_plan = []( bool diagnostics_enabled ) {
        const int previous_level = getDebugLevel();
        const int previous_class = getDebugClass();
        on_out_of_scope restore_filters( [previous_level, previous_class]() {
            limitDebugLevel( previous_level );
            limitDebugClass( previous_class );
        } );
        clear_map_without_vision();
        map &here = get_map();
        Character &you = get_player_character();
        const tripoint_bub_ms center{ 65, 65, 0 };
        restore_on_out_of_scope restore_calendar_turn( calendar::turn );
        set_time( calendar::turn_zero );
        prepare_writhing_stalker_arena( here, center );
        you.setpos( here, center );

        limitDebugLevel( diagnostics_enabled ? DL_ALL : 0 );
        limitDebugClass( D_GAME );
        monster &stalker = spawn_new_stalker_fixture( center + point::east * 5 );
        stalker.anger = 100;
        stalker.aggro_character = true;
        REQUIRE( stalker.sees( here, you ) );
        stalker.plan();

        return plan_state{ stalker.get_dest(), stalker.wander_pos, stalker.wandf, stalker.get_moves(),
                           stalker.anger, stalker.aggro_character, calendar::turn };
    };

    const int caller_level = getDebugLevel();
    const int caller_class = getDebugClass();
    const plan_state enabled = run_plan( true );
    CHECK( getDebugLevel() == caller_level );
    CHECK( getDebugClass() == caller_class );
    const plan_state disabled = run_plan( false );
    CHECK( getDebugLevel() == caller_level );
    CHECK( getDebugClass() == caller_class );
    CHECK( enabled.destination == disabled.destination );
    CHECK( enabled.wander_destination == disabled.wander_destination );
    CHECK( enabled.wander_range == disabled.wander_range );
    CHECK( enabled.moves == disabled.moves );
    CHECK( enabled.anger == disabled.anger );
    CHECK( enabled.aggro_character == disabled.aggro_character );
    CHECK( enabled.turn == disabled.turn );

    clear_map_without_vision();
}

TEST_CASE( "writhing_stalker_interest_sources_are_ranked_without_bandit_economy",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    const interest_report empty = evaluate_interest( interest_context{} );
    interest_context human_ctx;
    human_ctx.recent_human_evidence = true;
    human_ctx.evidence_age_minutes = 4;
    const interest_report human = evaluate_interest( human_ctx );

    CHECK( human.source == interest_source::human );
    CHECK( human.can_latch );
    CHECK( human.confidence >= 4 );
    CHECK( human.score > empty.score );
    CHECK( human.reason == "recent_human_evidence" );

    interest_context smoke_ctx;
    smoke_ctx.smoke = true;
    const interest_report smoke = evaluate_interest( smoke_ctx );
    CHECK( smoke.source == interest_source::smoke );
    CHECK_FALSE( smoke.can_latch );
    CHECK( smoke.score < human.score / 4 );
    CHECK( smoke.reason == "smoke_indirect_human_hint" );

    interest_context light_ctx;
    light_ctx.exposed_night_light = true;
    const interest_report light = evaluate_interest( light_ctx );
    CHECK( light.source == interest_source::light );
    CHECK_FALSE( light.can_latch );
    CHECK( light.score >= 50 );
    CHECK( light.score > smoke.score * 3 );

    interest_context terrain_ctx;
    terrain_ctx.forest_or_building_edge = true;
    const interest_report terrain = evaluate_interest( terrain_ctx );
    CHECK( terrain.source == interest_source::terrain );
    CHECK( terrain.score > empty.score );
    CHECK_FALSE( terrain.can_latch );
}

TEST_CASE( "writhing_stalker_latch_is_bounded_and_refresh_requires_evidence",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    interest_context human_ctx;
    human_ctx.recent_human_evidence = true;
    human_ctx.evidence_age_minutes = 3;
    latch_update latched = advance_latch( latch_context{ latch_state{},
                                          evaluate_interest( human_ctx ), 0, 12, false } );

    REQUIRE( latched.state.active );
    CHECK( latched.next == decision::shadow );
    CHECK( latched.state.leash_tiles_remaining == 60 );
    CHECK( latched.reason == "latch_created_by_believable_evidence" );

    latch_update continuing = advance_latch( latch_context{ latched.state,
                              interest_report{}, 12, 20, false } );
    CHECK( continuing.state.active );
    CHECK( continuing.state.age_minutes == 12 );
    CHECK( continuing.reason == "latch_continues_without_refresh" );

    latch_update timed_out = advance_latch( latch_context{ continuing.state,
                                            interest_report{}, 20, 20, false } );
    CHECK_FALSE( timed_out.state.active );
    CHECK( timed_out.next == decision::cooling_off );
    CHECK( timed_out.state.cooldown_minutes > 0 );
    CHECK( timed_out.reason == "latch_timed_out" );

    human_ctx.evidence_age_minutes = 90;
    latch_update stale = advance_latch( latch_context{ latch_state{},
                                        evaluate_interest( human_ctx ), 0, 10, false } );
    CHECK_FALSE( stale.state.active );
    CHECK( stale.next == decision::interested );
    CHECK( stale.reason == "interest_without_latch" );
}

TEST_CASE( "writhing_stalker_approach_prefers_cover_and_holds_at_exposure",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    latch_state latch;
    latch.active = true;
    latch.confidence = 4;
    latch.leash_tiles_remaining = 60;

    approach_context cover_ctx;
    cover_ctx.latch = latch;
    cover_ctx.cover_route_available = true;
    cover_ctx.direct_open_route_available = true;
    const approach_report cover = choose_approach( cover_ctx );
    CHECK( cover.route == approach_class::cover_shadow );
    CHECK( cover.next == decision::shadow );
    CHECK( cover.avoids_direct_line );
    CHECK( cover.reason == "cover_route_preferred" );

    approach_context exposed_ctx;
    exposed_ctx.latch = latch;
    exposed_ctx.cover_route_available = true;
    exposed_ctx.bright_exposure = true;
    const approach_report exposed = choose_approach( exposed_ctx );
    CHECK( exposed.route == approach_class::hold_exposed );
    CHECK( exposed.next == decision::hold );
    CHECK( exposed.avoids_direct_line );
    CHECK( exposed.reason == "bright_exposure_holds_shadow" );

    approach_context forced_ctx;
    forced_ctx.latch = latch;
    forced_ctx.direct_open_route_available = true;
    forced_ctx.forced_no_cover = true;
    const approach_report forced = choose_approach( forced_ctx );
    CHECK( forced.route == approach_class::direct_forced );
    CHECK_FALSE( forced.avoids_direct_line );
    CHECK( forced.reason == "direct_route_only_when_forced_by_map" );
}

TEST_CASE( "writhing_stalker_opportunity_uses_zombies_without_magic_tracking",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    latch_state no_latch;
    opportunity_context zombies_without_latch;
    zombies_without_latch.latch = no_latch;
    zombies_without_latch.zombie_pressure = 6;
    zombies_without_latch.player_bleeding = true;
    const opportunity_report ignored = evaluate_opportunity( zombies_without_latch );
    CHECK( ignored.next == decision::ignore );
    CHECK( ignored.reason == "no_latch_no_strike" );

    latch_state latch;
    latch.active = true;
    latch.confidence = 4;
    latch.leash_tiles_remaining = 60;

    opportunity_context calm_ctx;
    calm_ctx.latch = latch;
    calm_ctx.near_cover_or_clutter = true;
    const opportunity_report calm = evaluate_opportunity( calm_ctx );
    CHECK( calm.next == decision::hold );

    opportunity_context zombie_ctx = calm_ctx;
    zombie_ctx.zombie_pressure = 3;
    const opportunity_report zombie = evaluate_opportunity( zombie_ctx );
    CHECK( zombie.zombie_distraction > calm.zombie_distraction );
    CHECK( zombie.opportunity > calm.opportunity );
    CHECK( zombie.next != decision::strike );

    opportunity_context vulnerable_ctx = zombie_ctx;
    vulnerable_ctx.player_bleeding = true;
    vulnerable_ctx.player_hurt = true;
    vulnerable_ctx.player_low_stamina = true;
    vulnerable_ctx.player_distracted = true;
    const opportunity_report vulnerable = evaluate_opportunity( vulnerable_ctx );
    CHECK( vulnerable.vulnerability > calm.vulnerability );
    CHECK( vulnerable.next == decision::strike );
    CHECK( vulnerable.reason == "vulnerability_window_strike" );
}

TEST_CASE( "writhing_stalker_heavy_same_target_pressure_can_open_in_bright_exposure",
           "[writhing_stalker][ai][pressure]" )
{
    using namespace writhing_stalker;

    latch_state latch;
    latch.active = true;
    opportunity_context ctx;
    ctx.latch = latch;
    ctx.bright_exposure = true;
    ctx.distance_to_target = 2;
    ctx.zombie_pressure = 4;

    const opportunity_report report = evaluate_opportunity( ctx );
    CHECK( report.next == decision::strike );
    CHECK( report.reason == "heavy_zombie_pressure_strike" );
    CHECK( report.zombie_distraction > 0 );

    live_context approach;
    approach.has_believable_local_evidence = true;
    approach.distance_to_target = 6;
    approach.zombie_pressure = 4;
    approach.stalker_in_bright_exposure = true;
    approach.target_has_focus = true;
    approach.cover_route_available = true;
    approach.has_overmap_interest_footing = true;
    approach.overmap_intent = handoff_intent::committed_ambush;
    const live_response committed = evaluate_live_response( approach );
    CHECK( committed.next == decision::shadow );
    CHECK( committed.writeback_intent == handoff_intent::committed_ambush );
    CHECK( committed.persistent_state_required );
    CHECK( committed.reason == "live_heavy_zombie_pressure_approach" );
}

TEST_CASE( "writhing_stalker_pressure_requires_observable_target_directed_intent",
           "[writhing_stalker][ai][pressure]" )
{
    using namespace writhing_stalker;

    CHECK( is_meaningful_pressure( pressure_observation{ true, true, true, true, false } ) );
    CHECK( is_meaningful_pressure( pressure_observation{ true, true, true, false, true } ) );
    // Direct native target plus destination is already current pursuit; a
    // newly spawned observer must not need a second frame of motion/damage.
    CHECK( is_meaningful_pressure( pressure_observation{ true, true, true, false, false } ) );
    CHECK_FALSE( is_meaningful_pressure( pressure_observation{ true, true, false, true, false } ) );
    CHECK_FALSE( is_meaningful_pressure( pressure_observation{ true, false, true, false, true } ) );
    CHECK_FALSE( is_meaningful_pressure( pressure_observation{ false, true, true, true, false } ) );
}

TEST_CASE( "writhing_stalker_linked_melee_miss_binds_resolution_time_observers",
           "[writhing_stalker][ai][pressure][counterpressure][resolution]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    monster &stalker = spawn_new_stalker_fixture( center + point::east * 5 );
    monster &attacker = spawn_test_monster( mon_zombie.str(), center + point::east );
    attacker.aggro_character = true;
    attacker.set_dest( you.pos_abs() );
    refresh_writhing_stalker_arena( here );

    REQUIRE( stalker.sees( here, attacker ) );
    REQUIRE( stalker.sees( here, you ) );
    const int turn = to_turn<int>( calendar::turn );
    CHECK( you.deal_melee_attack( &attacker, -1000 ) < 0 );
    CHECK( writhing_stalker::has_recent_observed_attack( &stalker, &attacker, &you, turn, 0 ) );
    clear_map_without_vision();
}

TEST_CASE( "writhing_stalker_actor_observation_tracks_current_geometry",
           "[writhing_stalker][ai][pressure][resolution]" )
{
    clear_avatar();
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    monster &observer = spawn_new_stalker_fixture( center + point::east * 6 );
    monster &attacker = spawn_test_monster( mon_zombie.str(), center + point::east * 2 );
    refresh_writhing_stalker_arena( here );
    const auto reference = [&]( const Creature &target ) {
        scout_observation::site_reader reader;
        return reader.read_actor( observer, target, { 72, 72.0f /
                                  std::max( 1, std::max( observer.type->vision_day,
                                            observer.type->vision_night ) ) }, true );
    };
    const auto sees_both = [&]() {
        return writhing_stalker::can_observe_actor( observer, attacker ) &&
               writhing_stalker::can_observe_actor( observer, you );
    };
    REQUIRE( sees_both() );
    CHECK( writhing_stalker::can_observe_actor( observer, you ) == reference( you ).visible );
    here.ter_set( center + point::east * 5, ter_id( "t_wall" ) );
    refresh_writhing_stalker_arena( here );
    CHECK_FALSE( sees_both() );
    CHECK( writhing_stalker::can_observe_actor( observer, you ) == reference( you ).visible );
    here.ter_set( center + point::east * 5, ter_id( "t_floor" ) );
    refresh_writhing_stalker_arena( here );
    CHECK( sees_both() );
    you.set_mutation( trait_id( "DEBUG_CLOAK" ) );
    REQUIRE( you.is_invisible() );
    CHECK_FALSE( reference( you ).visible );
    CHECK_FALSE( writhing_stalker::can_observe_actor( observer, you ) );
    you.setpos( here, center + point::east * 5 );
    refresh_writhing_stalker_arena( here );
    CHECK_FALSE( reference( you ).visible );
    CHECK_FALSE( writhing_stalker::can_observe_actor( observer, you ) );
    // A non-Character (including the observer itself) still uses the shared
    // reader's existing visibility/availability semantics.
    CHECK( writhing_stalker::can_observe_actor( observer, observer ) == reference( observer ).visible );
    you.unset_mutation( trait_id( "DEBUG_CLOAK" ) );
    clear_map_without_vision();
}

TEST_CASE( "writhing_stalker_linked_projectile_miss_is_idempotent_and_visibility_bound",
           "[writhing_stalker][ai][pressure][counterpressure][resolution]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    monster &observer = spawn_new_stalker_fixture( center + point::east * 6 );
    monster &target = spawn_new_stalker_fixture( center + point::east * 4 );
    monster &attacker = spawn_test_monster( mon_zombie.str(), center + point::east * 2 );
    attacker.aggro_character = true;
    attacker.set_dest( target.pos_abs() );
    refresh_writhing_stalker_arena( here );

    REQUIRE( observer.sees( here, attacker ) );
    REQUIRE( observer.sees( here, target ) );
    const int turn = to_turn<int>( calendar::turn );
    dealt_projectile_attack attack{};
    target.deal_projectile_attack( &here, &attacker, attack, 2.0, false );
    CHECK( writhing_stalker::has_recent_observed_attack( &observer, &attacker, &target, turn, 0 ) );

    const writhing_stalker::resolution_id duplicate = writhing_stalker::next_resolution_id();
    writhing_stalker::observe_attack_resolution( &attacker, &target, turn, duplicate );
    writhing_stalker::observe_attack_resolution( &attacker, &target, turn, duplicate );
    CHECK( writhing_stalker::observed_attack_count( &observer, &attacker, &target, duplicate ) == 1 );

    here.ter_set( center + point::east * 5, ter_id( "t_wall" ) );
    refresh_writhing_stalker_arena( here );
    dealt_projectile_attack hidden_attack{};
    target.deal_projectile_attack( &here, &attacker, hidden_attack, 2.0, false );
    const writhing_stalker::resolution_id hidden = hidden_attack.writhing_stalker_resolution;
    here.ter_set( center + point::east * 5, ter_id( "t_floor" ) );
    refresh_writhing_stalker_arena( here );
    CHECK( writhing_stalker::observed_attack_count( &observer, &attacker, &target, hidden ) == 0 );
    clear_map_without_vision();
}

TEST_CASE( "writhing_stalker_observed_attack_identity_survives_deletion_and_reuse",
           "[writhing_stalker][ai][pressure][resolution]" )
{
    monster observer( mon_writhing_stalker );
    monster target( mon_zombie );
    std::optional<monster> attacker;
    attacker.emplace( mon_zombie );
    const writhing_stalker::resolution_id resolution = writhing_stalker::next_resolution_id();
    writhing_stalker::record_observed_attack( &observer, &*attacker, &target, 100, resolution );
    CHECK( writhing_stalker::observed_attack_count( &observer, &*attacker, &target, resolution ) == 1 );
    attacker.reset();
    attacker.emplace( mon_zombie );
    CHECK_FALSE( writhing_stalker::has_recent_observed_attack( &observer, &*attacker, &target, 100,
                 3 ) );
}

TEST_CASE( "writhing_stalker_creature_copy_gets_fresh_observation_identity",
           "[writhing_stalker][ai][pressure][resolution][identity]" )
{
    monster observer( mon_writhing_stalker );
    monster attacker( mon_zombie );
    monster target( mon_zombie );
    const writhing_stalker::resolution_id resolution = writhing_stalker::next_resolution_id();

    writhing_stalker::record_observed_attack( &observer, &attacker, &target, 100, resolution );
    monster copy = attacker;

    CHECK( writhing_stalker::observed_attack_count( &observer, &attacker, &target, resolution ) == 1 );
    CHECK_FALSE( writhing_stalker::has_recent_observed_attack( &observer, &copy, &target, 100, 0 ) );
}

TEST_CASE( "writhing_stalker_creature_assignment_preserves_receiver_observation_identity",
           "[writhing_stalker][ai][pressure][resolution][identity]" )
{
    monster observer( mon_writhing_stalker );
    monster source( mon_zombie );
    monster receiver( mon_zombie );
    monster target( mon_zombie );
    const writhing_stalker::resolution_id resolution = writhing_stalker::next_resolution_id();

    writhing_stalker::record_observed_attack( &observer, &receiver, &target, 100, resolution );
    receiver = source;

    CHECK( writhing_stalker::has_recent_observed_attack( &observer, &receiver, &target, 100, 0 ) );
    CHECK_FALSE( writhing_stalker::has_recent_observed_attack( &observer, &source, &target, 100, 0 ) );
}

TEST_CASE( "writhing_stalker_handoff_does_not_spend_attack_budget_while_walking",
           "[writhing_stalker][ai][accounting]" )
{
    using namespace writhing_stalker;

    handoff_memory incoming;
    incoming.strike_budget_spent = 0;
    live_response planned;
    planned.next = decision::strike;
    planned.burst_limit = 2;
    planned.reason = "planned_but_not_resolved";

    const handoff_memory out = writeback_handoff_memory( incoming, planned );
    CHECK( out.strike_budget_spent == 0 );
    CHECK( out.reason == planned.reason );
}

TEST_CASE( "writhing_stalker_quiet_side_cutoff_prefers_the_side_zombies_are_not",
           "[writhing_stalker][ai][zombie_shadow]" )
{
    using namespace writhing_stalker;

    const std::vector<relative_point> east_zombies = { { 3, 0, 1 }, { 4, 1, 1 }, { 2, -1, 1 } };
    const std::vector<quiet_candidate> candidates = {
        { -3, 0, 8, true, false, true, true, false, 0 },
        { 3, 0, 3, true, false, true, true, false, 0 }
    };

    const quiet_candidate_report chosen = choose_quiet_side_cutoff( east_zombies, candidates );
    REQUIRE( chosen.has_candidate );
    CHECK( chosen.pressure.has_dominant_pressure );
    CHECK( chosen.pressure.pressure_x > 0 );
    CHECK( chosen.pressure.quiet_x < 0 );
    CHECK( chosen.chosen.rel_x < 0 );
    CHECK( chosen.quiet_alignment > 0 );
    CHECK( chosen.reason == "quiet_side_cutoff_preferred" );
}

TEST_CASE( "writhing_stalker_quiet_side_cutoff_avoids_fake_precision_when_pressure_splits",
           "[writhing_stalker][ai][zombie_shadow]" )
{
    using namespace writhing_stalker;

    const std::vector<relative_point> split_zombies = { { 3, 0, 1 }, { -3, 0, 1 } };
    const std::vector<quiet_candidate> candidates = {
        { -3, 0, 6, true, false, false, false, false, 0 },
        { 0, -3, 6, true, false, true, true, false, 0 }
    };

    const quiet_candidate_report chosen = choose_quiet_side_cutoff( split_zombies, candidates );
    REQUIRE( chosen.has_candidate );
    CHECK_FALSE( chosen.pressure.has_dominant_pressure );
    CHECK( chosen.pressure.ambiguous_pressure );
    CHECK( chosen.quiet_alignment == 0 );
    CHECK( chosen.chosen.rel_y < 0 );
    CHECK( chosen.reason == "ambiguous_pressure_no_precise_quiet_side" );
}

TEST_CASE( "writhing_stalker_quiet_side_cutoff_can_follow_retreat_route_when_it_matches_pressure",
           "[writhing_stalker][ai][zombie_shadow]" )
{
    using namespace writhing_stalker;

    const std::vector<relative_point> north_zombies = { { 0, -3, 1 }, { 1, -2, 1 } };
    const std::vector<quiet_candidate> candidates = {
        { 0, 3, 3, true, false, true, true, false, 0 },
        { 1, 3, 6, true, false, true, true, false, 4 }
    };

    const quiet_candidate_report chosen = choose_quiet_side_cutoff( north_zombies, candidates );
    REQUIRE( chosen.has_candidate );
    CHECK( chosen.pressure.quiet_y > 0 );
    CHECK( chosen.chosen.rel_y > 0 );
    CHECK( chosen.chosen.retreat_alignment > 0 );
    CHECK( chosen.reason == "quiet_side_cutoff_preferred" );
}

TEST_CASE( "writhing_stalker_confidence_gates_zombie_pressure_and_suppresses_cutoff_with_light_focus",
           "[writhing_stalker][ai][zombie_shadow]" )
{
    using namespace writhing_stalker;

    confidence_context no_evidence;
    no_evidence.zombie_pressure = 5;
    no_evidence.quiet_side_cutoff_available = true;
    const confidence_report ignored = evaluate_confidence( no_evidence );
    CHECK_FALSE( ignored.pressure_allowed );
    CHECK_FALSE( ignored.cutoff_allowed );
    CHECK( ignored.zombie_pressure == 0 );
    CHECK( ignored.quiet_side_cutoff == 0 );
    CHECK( ignored.reason == "no_evidence_or_interest_pressure_ignored" );

    confidence_context evidence = no_evidence;
    evidence.has_believable_local_evidence = true;
    const confidence_report pressure = evaluate_confidence( evidence );
    CHECK( pressure.pressure_allowed );
    CHECK( pressure.cutoff_allowed );
    CHECK( pressure.evidence > 0 );
    CHECK( pressure.zombie_pressure > 0 );
    CHECK( pressure.quiet_side_cutoff > 0 );
    CHECK( pressure.total > ignored.total );
    CHECK( pressure.reason == "zombie_shadow_quiet_cutoff_confidence" );

    confidence_context interest = no_evidence;
    interest.has_overmap_interest_footing = true;
    const confidence_report overmap_pressure = evaluate_confidence( interest );
    CHECK( overmap_pressure.pressure_allowed );
    CHECK( overmap_pressure.zombie_pressure > 0 );
    CHECK( overmap_pressure.quiet_side_cutoff > 0 );

    confidence_context countered = evidence;
    countered.target_in_bright_exposure = true;
    countered.stalker_in_bright_exposure = true;
    countered.target_has_focus = true;
    countered.open_exposure = true;
    const confidence_report suppressed = evaluate_confidence( countered );
    CHECK( suppressed.counterpressure > 0 );
    CHECK_FALSE( suppressed.cutoff_allowed );
    CHECK( suppressed.quiet_side_cutoff == 0 );
    CHECK( suppressed.total < pressure.total );
    CHECK( suppressed.reason == "counterpressure_suppresses_cutoff" );
}


TEST_CASE( "writhing_stalker_threat_handoff_overmatched_allies_retreats_to_stalking_distance",
           "[writhing_stalker][ai][handoff]" )
{
    using namespace writhing_stalker;

    const threat_report calm = evaluate_threat( threat_context{ true, true, true, 1, 0, 0 } );
    CHECK_FALSE( calm.overmatched );
    CHECK( calm.reason == "threat_not_overmatched" );

    const threat_report zombies_break_the_firing_squad = evaluate_threat( threat_context{ true, true,
            true, 3, 3, 0 } );
    CHECK_FALSE( zombies_break_the_firing_squad.overmatched );

    const threat_report overmatched = evaluate_threat( threat_context{ true, true, true, 3, 0, 0 } );
    REQUIRE( overmatched.overmatched );
    CHECK( overmatched.next == decision::withdraw );
    CHECK( overmatched.intent == handoff_intent::overmatched_stalk );
    CHECK( overmatched.stalk_distance_omt == 5 );
    CHECK( overmatched.threat_memory >= 3 );
    CHECK( overmatched.avoid_sight_tiles );
    CHECK( overmatched.reason == "high_threat_allied_light_retreat_stalk" );

    live_context ctx = pattern_base_context();
    ctx.distance_to_target = 6;
    ctx.target_in_bright_exposure = true;
    ctx.target_has_focus = true;
    ctx.allied_support_nearby = 3;
    const live_response response = evaluate_live_response( ctx );
    CHECK( response.next == decision::withdraw );
    CHECK( response.writeback_intent == handoff_intent::overmatched_stalk );
    CHECK( response.overmap_stalk_distance_omt == 5 );
    CHECK( response.persistent_state_required );
    CHECK( response.reason == "live_high_threat_allied_light_retreat_stalk" );
}

TEST_CASE( "writhing_stalker_sighted_stalk_uses_cover_or_withdraws_to_far_watch",
           "[writhing_stalker][ai][live][sight]" )
{
    using namespace writhing_stalker;

    live_context covered = pattern_base_context();
    covered.distance_to_target = 6;
    covered.target_has_focus = true;
    covered.cover_route_available = true;
    const live_response shadow = evaluate_live_response( covered );
    CHECK( shadow.next == decision::shadow );
    CHECK( shadow.route == approach_class::cover_shadow );
    CHECK( shadow.writeback_intent == handoff_intent::shadowing );
    CHECK( shadow.overmap_stalk_distance_omt == 5 );
    CHECK( shadow.persistent_state_required );
    CHECK( shadow.reason == "live_sighted_stalker_breaks_los_shadow" );

    live_context exposed = covered;
    exposed.cover_route_available = false;
    exposed.edge_route_available = false;
    exposed.direct_open_route_available = true;
    exposed.forced_no_cover = false;
    const live_response withdrawal = evaluate_live_response( exposed );
    CHECK( withdrawal.next == decision::withdraw );
    CHECK( withdrawal.writeback_intent == handoff_intent::spent_disengage );
    CHECK( withdrawal.overmap_stalk_distance_omt == 5 );
    CHECK( withdrawal.persistent_state_required );
    CHECK( withdrawal.reason == "live_sighted_stalker_no_cover_withdraw" );
}

TEST_CASE( "writhing_stalker_night_reachable_anti_gnome_resolves_loiter",
           "[writhing_stalker][ai][handoff]" )
{
    using namespace writhing_stalker;

    const live_response not_yet = resolve_anti_gnome( anti_gnome_context{ true, true, true, false,
                                  2, 1 } );
    CHECK_FALSE( not_yet.anti_gnome_triggered );

    const live_response strike = resolve_anti_gnome( anti_gnome_context{ true, true, true, false,
                                 2, 2 } );
    CHECK( strike.anti_gnome_triggered );
    CHECK( strike.next == decision::strike );
    CHECK( strike.writeback_intent == handoff_intent::committed_ambush );
    CHECK( strike.reason == "anti_gnome_night_reachable_probe_strike" );

    live_context gnome = pattern_base_context();
    gnome.distance_to_target = 2;
    gnome.night_outside_reachable_target = true;
    gnome.direct_open_route_available = true;
    gnome.bad_position_loiter_turns = 2;
    const live_response response = evaluate_live_response( gnome );
    CHECK( response.next == decision::strike );
    CHECK( response.anti_gnome_triggered );
    CHECK( response.reason == "live_anti_gnome_night_reachable_probe_strike" );
}

TEST_CASE( "writhing_stalker_zombie_distraction_enables_dark_square_strike_without_omniscience",
           "[writhing_stalker][ai][handoff][zombie_shadow]" )
{
    using namespace writhing_stalker;

    live_context no_evidence = pattern_base_context();
    no_evidence.has_believable_local_evidence = false;
    no_evidence.distance_to_target = 2;
    no_evidence.zombie_pressure = 3;
    no_evidence.near_cover_or_clutter = true;
    const live_response ignored = evaluate_live_response( no_evidence );
    CHECK( ignored.next == decision::ignore );
    CHECK( ignored.reason == "live_no_believable_evidence" );

    live_context distracted = no_evidence;
    distracted.has_believable_local_evidence = true;
    const live_response strike = evaluate_live_response( distracted );
    CHECK( strike.next == decision::strike );
    CHECK( strike.writeback_intent == handoff_intent::committed_ambush );
    CHECK( strike.reason == "live_zombie_distraction_dark_square_strike" );

    live_context probe = distracted;
    probe.distance_to_target = 6;
    probe.cover_route_available = true;
    const live_response shadow = evaluate_live_response( probe );
    CHECK( shadow.next == decision::shadow );
    CHECK( shadow.writeback_intent == handoff_intent::opportunity_probe );
    CHECK( shadow.reason == "live_zombie_distraction_dark_square_probe" );
}

TEST_CASE( "writhing_stalker_threat_drop_strikes_on_low_zombie_pressure_then_boots_out",
           "[writhing_stalker][ai][live][threat]" )
{
    using namespace writhing_stalker;

    live_context dropped = pattern_base_context();
    dropped.distance_to_target = 2;
    dropped.zombie_pressure = 2;
    const live_response strike = evaluate_live_response( dropped );
    CHECK( strike.next == decision::strike );
    CHECK( strike.writeback_intent == handoff_intent::committed_ambush );
    CHECK( strike.reason == "live_zombie_distraction_dark_square_strike" );

    live_context spent = dropped;
    spent.burst_strikes = strike.burst_limit;
    const live_response boot_out = evaluate_live_response( spent );
    CHECK( boot_out.next == decision::withdraw );
    CHECK( boot_out.writeback_intent == handoff_intent::spent_disengage );
    CHECK( boot_out.overmap_stalk_distance_omt == 5 );
    CHECK( boot_out.persistent_state_required );
    CHECK( boot_out.reason == "live_burst_limit_reached_withdraw" );
}

TEST_CASE( "writhing_stalker_handoff_writeback_preserves_spent_cooldown_and_threat_memory",
           "[writhing_stalker][ai][handoff]" )
{
    using namespace writhing_stalker;

    handoff_memory incoming;
    incoming.intent = handoff_intent::overmatched_stalk;
    incoming.strike_budget_spent = 1;
    incoming.cooldown_minutes = 2;
    incoming.threat_memory = 3;
    incoming.stalk_distance_omt = 5;

    live_context ctx = pattern_base_context();
    ctx.distance_to_target = 2;
    ctx.player_bleeding = true;
    ctx.player_hurt = true;
    ctx.player_low_stamina = true;
    ctx.player_distracted = true;
    ctx.overmap_intent = incoming.intent;
    ctx.has_overmap_interest_footing = true;
    ctx.overmap_stalk_distance_omt = incoming.stalk_distance_omt;
    ctx.overmap_cooldown_minutes = incoming.cooldown_minutes;
    ctx.overmap_threat_memory = incoming.threat_memory;
    const live_response response = evaluate_live_response( ctx );

    const handoff_memory out = writeback_handoff_memory( incoming, response );
    CHECK( out.reason == response.reason );
    CHECK( out.cooldown_minutes >= incoming.cooldown_minutes );
    CHECK( out.threat_memory >= incoming.threat_memory );
    CHECK( out.stalk_distance_omt == 5 );
    CHECK( out.strike_budget_spent >= incoming.strike_budget_spent );
}

TEST_CASE( "writhing_stalker_withdrawal_and_cooldown_block_repeat_spam",
           "[writhing_stalker][ai]" )
{
    using namespace writhing_stalker;

    latch_state latch;
    latch.active = true;
    latch.confidence = 4;
    latch.leash_tiles_remaining = 60;

    opportunity_context exposed_ctx;
    exposed_ctx.latch = latch;
    exposed_ctx.bright_exposure = true;
    exposed_ctx.player_focused = true;
    const opportunity_report exposed = evaluate_opportunity( exposed_ctx );
    CHECK( exposed.next == decision::withdraw );
    CHECK( exposed.reason == "exposed_and_focused_withdraw" );

    // Reciprocal daylight sight still gives the stalker a reason to withdraw
    // in a quiet scene.  Four observed, target-directed zombies are a real
    // local distraction, however, and preserve the close-range opportunity
    // instead of consuming it before the heavy-pressure rule can evaluate.
    opportunity_context pressured_exposed_ctx = exposed_ctx;
    pressured_exposed_ctx.zombie_pressure = 4;
    pressured_exposed_ctx.distance_to_target = 2;
    const opportunity_report pressured_exposed = evaluate_opportunity( pressured_exposed_ctx );
    CHECK( pressured_exposed.next == decision::strike );
    CHECK( pressured_exposed.reason == "heavy_zombie_pressure_strike" );

    opportunity_context hurt_ctx;
    hurt_ctx.latch = latch;
    hurt_ctx.player_bleeding = true;
    hurt_ctx.player_hurt = true;
    hurt_ctx.player_low_stamina = true;
    hurt_ctx.player_distracted = true;
    hurt_ctx.stalker_hurt = true;
    const opportunity_report hurt = evaluate_opportunity( hurt_ctx );
    CHECK( hurt.next == decision::withdraw );
    CHECK( hurt.reason == "stalker_hurt_withdraw" );

    latch.cooldown_minutes = 8;
    opportunity_context cooldown_ctx;
    cooldown_ctx.latch = latch;
    cooldown_ctx.player_bleeding = true;
    cooldown_ctx.player_hurt = true;
    cooldown_ctx.player_low_stamina = true;
    cooldown_ctx.player_distracted = true;
    const opportunity_report cooldown = evaluate_opportunity( cooldown_ctx );
    CHECK( cooldown.next == decision::cooling_off );
    CHECK( cooldown.reason == "cooldown_blocks_repeat_strike" );
}


TEST_CASE( "writhing_stalker_threat_distraction_state_handles_overmatch_and_anti_gnome",
           "[writhing_stalker][ai][threat]" )
{
    using namespace writhing_stalker;

    threat_context overmatched;
    overmatched.close_overmap_pressure = true;
    overmatched.allied_support_nearby = 3;
    overmatched.daylight_or_bright = true;
    overmatched.strong_visibility = true;
    overmatched.valid_evidence_or_path = true;
    const threat_report retreat = evaluate_threat_state( overmatched );
    CHECK( retreat.state == threat_state::overmatched_retreat );
    CHECK( retreat.next == decision::withdraw );
    CHECK( retreat.stalking_distance_omt == 5 );
    CHECK( retreat.avoids_sight_tiles );
    CHECK( retreat.reason == "high_threat_allied_light_retreat_stalk" );

    threat_context zombie_distraction = overmatched;
    zombie_distraction.has_distraction = true;
    zombie_distraction.night_or_dark = true;
    zombie_distraction.daylight_or_bright = false;
    const threat_report opportunity = evaluate_threat_state( zombie_distraction );
    CHECK( opportunity.next != decision::withdraw );
    CHECK( opportunity.opportunity_score > retreat.opportunity_score );

    threat_context window_gnome;
    window_gnome.night_or_dark = true;
    window_gnome.outside_reachable_player = true;
    window_gnome.valid_evidence_or_path = true;
    window_gnome.bad_position_loiter_turns = 3;
    const threat_report anti_gnome = evaluate_threat_state( window_gnome );
    CHECK( anti_gnome.state == threat_state::opportunity_probe );
    CHECK( anti_gnome.next == decision::shadow );
    CHECK( anti_gnome.anti_gnome_fired );
    CHECK( anti_gnome.reason == "anti_gnome_dark_reposition_probe" );
}

TEST_CASE( "writhing_stalker_live_response_retreats_from_daylight_three_allies_and_breaks_dark_loiter",
           "[writhing_stalker][ai][live][threat]" )
{
    using namespace writhing_stalker;

    live_context overmatched = pattern_base_context();
    overmatched.distance_to_target = 4;
    overmatched.target_in_bright_exposure = true;
    overmatched.target_has_focus = true;
    overmatched.allied_support_nearby = 3;
    overmatched.direct_open_route_available = true;
    const live_response retreat = evaluate_live_response( overmatched );
    CHECK( retreat.next == decision::withdraw );
    CHECK( retreat.retreat_distance >= 14 );
    CHECK( retreat.persistent_state_required );
    CHECK( retreat.writeback_intent == handoff_intent::overmatched_stalk );
    CHECK( retreat.overmap_stalk_distance_omt == 5 );
    CHECK( retreat.reason == "live_high_threat_allied_light_retreat_stalk" );

    live_context dark_window = pattern_base_context();
    dark_window.distance_to_target = 2;
    dark_window.cover_route_available = false;
    dark_window.edge_route_available = true;
    dark_window.direct_open_route_available = true;
    dark_window.forced_no_cover = false;
    dark_window.near_cover_or_clutter = false;
    dark_window.night_outside_reachable_target = true;
    dark_window.bad_position_loiter_turns = 3;
    const live_response anti_gnome = evaluate_live_response( dark_window );
    CHECK( anti_gnome.next == decision::strike );
    CHECK( anti_gnome.persistent_state_required );
    CHECK( anti_gnome.anti_gnome_triggered );
    CHECK( anti_gnome.reason == "live_anti_gnome_night_reachable_probe_strike" );
}

TEST_CASE( "writhing_stalker_live_response_requires_evidence_and_uses_cooldown",
           "[writhing_stalker][ai][live]" )
{
    using namespace writhing_stalker;

    live_context no_evidence;
    no_evidence.distance_to_target = 8;
    no_evidence.player_bleeding = true;
    no_evidence.player_hurt = true;
    no_evidence.player_low_stamina = true;
    no_evidence.player_distracted = true;
    const live_response ignored = evaluate_live_response( no_evidence );
    CHECK( ignored.next == decision::ignore );
    CHECK( ignored.reason == "live_no_believable_evidence" );

    live_context shadow_ctx;
    shadow_ctx.has_believable_local_evidence = true;
    shadow_ctx.distance_to_target = 8;
    shadow_ctx.cover_route_available = true;
    shadow_ctx.near_cover_or_clutter = true;
    const live_response shadow = evaluate_live_response( shadow_ctx );
    CHECK( shadow.next == decision::shadow );
    CHECK( shadow.route == approach_class::cover_shadow );
    CHECK( shadow.reason == "live_shadowing_believable_evidence" );

    live_context far_strike_window_ctx = shadow_ctx;
    far_strike_window_ctx.player_bleeding = true;
    far_strike_window_ctx.player_hurt = true;
    far_strike_window_ctx.player_low_stamina = true;
    far_strike_window_ctx.player_distracted = true;
    far_strike_window_ctx.zombie_pressure = 2;
    const live_response far_strike_window = evaluate_live_response( far_strike_window_ctx );
    CHECK( far_strike_window.next == decision::shadow );
    CHECK( far_strike_window.reason == "live_shadowing_before_strike_window" );

    live_context strike_ctx = far_strike_window_ctx;
    strike_ctx.distance_to_target = 2;
    const live_response strike = evaluate_live_response( strike_ctx );
    CHECK( strike.next == decision::strike );
    CHECK( strike.reason == "live_vulnerability_window_strike" );

    live_context cooldown_ctx = strike_ctx;
    cooldown_ctx.on_cooldown = true;
    const live_response cooldown = evaluate_live_response( cooldown_ctx );
    CHECK( cooldown.next == decision::cooling_off );
    CHECK( cooldown.reason == "live_latch_cooldown_blocks_relatched_pressure" );
}

TEST_CASE( "writhing_stalker_burst_limit_is_shaped_by_stress_and_counterpressure",
           "[writhing_stalker][ai][pattern]" )
{
    using namespace writhing_stalker;

    live_context base = pattern_base_context();
    base.distance_to_target = 2;
    base.near_cover_or_clutter = true;

    live_context cautious = base;
    cautious.player_bleeding = true;
    cautious.player_hurt = true;
    cautious.player_low_stamina = true;
    cautious.target_in_bright_exposure = true;
    cautious.target_has_focus = true;
    cautious.allied_support_nearby = 2;
    const live_response cautious_first = evaluate_live_response( cautious );
    CHECK( cautious_first.next == decision::withdraw );
    CHECK( cautious_first.burst_limit == 1 );
    CHECK( cautious_first.retreat_distance >= 11 );

    live_context pressure = base;
    pressure.player_bleeding = true;
    pressure.player_hurt = true;
    pressure.player_low_stamina = true;
    pressure.player_distracted = true;
    pressure.player_noisy = true;
    pressure.zombie_pressure = 4;
    const live_response pressure_first = evaluate_live_response( pressure );
    CHECK( pressure_first.next == decision::strike );
    CHECK( pressure_first.burst_limit == 4 );

    live_context post_burst = pressure;
    post_burst.burst_strikes = pressure_first.burst_limit;
    const live_response retreat = evaluate_live_response( post_burst );
    CHECK( retreat.next == decision::withdraw );
    CHECK( retreat.reason == "live_burst_limit_reached_withdraw" );
    CHECK( retreat.retreat_distance >= 8 );
}

TEST_CASE( "writhing_stalker_live_plan_retreats_about_eight_tiles_after_burst",
           "[writhing_stalker][monster][map][pattern]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms stalker_start = center + point::east * 2;
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 0_hours );
    refresh_writhing_stalker_arena( here );

    monster &stalker = spawn_new_stalker_fixture( stalker_start );
    stalker.anger = 100;
    stalker.aggro_character = true;
    stalker.writhing_stalker_state().attempts_spent = 2;

    REQUIRE( stalker.sees( here, you ) );
    const auto &life = stalker.predator_state();
    INFO( "withdrawal actor=" << life.actor_id << " epoch=" << life.handoff_epoch <<
          " pose=" << stalker.pos_abs().to_string() );
    const auto *prior = overmap_buffer.global_state.predator_activity.find( life.actor_id );
    INFO( "prior owner=" << ( prior ? static_cast<int>( prior->owner ) : -1 ) << " epoch=" <<
          ( prior ? prior->epoch : 0 ) << " confirmed=" << ( prior && prior->confirmed ) << " pose=" <<
          ( prior && prior->position ? prior->position->to_string() : "absent" ) );
    stalker.plan();

    const auto &withdrawal_state = stalker.writhing_stalker_state();
    REQUIRE( withdrawal_state.phase == writhing_stalker::lifecycle_phase::retreating );
    const int withdrawal_entered_turn = withdrawal_state.phase_entered_turn;
    const int withdrawal_cooldown_until = withdrawal_state.cooldown_until_turn;
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::cooldown );
    const int cooling_entered_turn = stalker.writhing_stalker_state().phase_entered_turn;
    const int cooling_until_turn = stalker.writhing_stalker_state().cooldown_until_turn;
    CHECK( cooling_entered_turn >= withdrawal_entered_turn );
    CHECK( cooling_until_turn >= withdrawal_cooldown_until );
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::cooldown );
    CHECK( stalker.writhing_stalker_state().phase_entered_turn == cooling_entered_turn );
    CHECK( stalker.writhing_stalker_state().cooldown_until_turn == cooling_until_turn );

    const tripoint_bub_ms dest = here.get_bub( stalker.get_dest() );
    INFO( "retreat destination=" << dest.to_string() << " player=" << you.pos_bub().to_string() );
    CHECK( rl_dist( dest, you.pos_bub() ) >= 8 );
    CHECK( dest.x() > you.pos_bub().x() );
    CHECK_FALSE( dest == you.pos_bub() );
    clear_map_without_vision();
}

TEST_CASE( "writhing_stalker_pattern_helper_covers_fair_dread_baselines",
           "[writhing_stalker][ai][pattern]" )
{
    using namespace writhing_stalker;

    live_context no_evidence = pattern_base_context();
    no_evidence.has_believable_local_evidence = false;
    no_evidence.distance_to_target = 2;
    no_evidence.player_bleeding = true;
    no_evidence.player_hurt = true;
    no_evidence.player_low_stamina = true;
    no_evidence.player_distracted = true;
    no_evidence.zombie_pressure = 4;
    const live_response no_magic = evaluate_live_response( no_evidence );
    CHECK( no_magic.next == decision::ignore );
    CHECK( no_magic.reason == "live_no_believable_evidence" );

    interest_context weak_evidence;
    weak_evidence.recent_human_evidence = true;
    weak_evidence.evidence_age_minutes = 25;
    latch_update weak = advance_latch( latch_context{ latch_state{}, evaluate_interest( weak_evidence ),
                                       0, 8, false } );
    REQUIRE( weak.state.active );
    weak = advance_latch( latch_context{ weak.state, interest_report{}, 31, 8, false } );
    CHECK_FALSE( weak.state.active );
    CHECK( weak.next == decision::cooling_off );
    CHECK( weak.reason == "latch_timed_out" );

    live_context cover = pattern_base_context();
    cover.distance_to_target = 7;
    const live_response covered_route = evaluate_live_response( cover );
    CHECK( covered_route.next == decision::shadow );
    CHECK( covered_route.route == approach_class::cover_shadow );
    CHECK( covered_route.reason == "live_shadowing_believable_evidence" );

    live_context exposed = cover;
    exposed.stalker_in_bright_exposure = true;
    exposed.target_has_focus = true;
    const live_response counterplay = evaluate_live_response( exposed );
    CHECK( counterplay.next == decision::withdraw );
    CHECK( counterplay.reason == "live_exposed_and_focused_withdraw" );

    live_context zombie_only = pattern_base_context();
    zombie_only.distance_to_target = 2;
    zombie_only.zombie_pressure = 4;
    const live_response distraction_without_vulnerability = evaluate_live_response( zombie_only );
    CHECK( distraction_without_vulnerability.next == decision::strike );
    CHECK( distraction_without_vulnerability.reason == "live_zombie_distraction_dark_square_strike" );

    live_context vulnerable = zombie_only;
    vulnerable.player_bleeding = true;
    vulnerable.player_hurt = true;
    vulnerable.player_low_stamina = true;
    vulnerable.player_distracted = true;
    const live_response strike_window = evaluate_live_response( vulnerable );
    CHECK( strike_window.next == decision::strike );
    CHECK( strike_window.reason == "live_vulnerability_window_strike" );
}

TEST_CASE( "writhing_stalker_persistent_state_round_trips_and_advances_once",
           "[writhing_stalker][persistence]" )
{
    using namespace writhing_stalker;

    persistent_state original;
    original.phase = lifecycle_phase::cooldown;
    original.evidence_target = 42;
    original.evidence_turn = 17;
    original.last_observed_position = tripoint_abs_ms( 10, 11, 0 );
    original.has_last_observed_position = true;
    original.committed_waypoint = tripoint_abs_ms( 12, 13, 0 );
    original.has_committed_waypoint = true;
    original.retreat_waypoint = tripoint_abs_ms( 8, 9, 0 );
    original.has_retreat_waypoint = true;
    original.attempts_spent = 3;
    original.attempt_sequence = 9;
    original.phase_entered_turn = 20;
    original.cooldown_until_turn = 30;
    original.last_progress_turn = 25;
    original.last_advanced_turn = 25;
    original.remaining_route_progress = 6;
    original.search_until_turn = -1;

    std::ostringstream serialized;
    {
        JsonOut json( serialized );
        original.serialize( json );
    }
    persistent_state restored;
    restored.deserialize( json_loader::from_string( serialized.str() ) );
    CHECK( restored.phase == lifecycle_phase::cooldown );
    CHECK( restored.evidence_target == 42 );
    CHECK( restored.evidence_turn == 17 );
    CHECK( restored.last_observed_position == tripoint_abs_ms( 10, 11, 0 ) );
    CHECK( restored.committed_waypoint == tripoint_abs_ms( 12, 13, 0 ) );
    CHECK( restored.retreat_waypoint == tripoint_abs_ms( 8, 9, 0 ) );
    CHECK( restored.attempts_spent == 3 );
    CHECK( restored.attempt_sequence == 9 );
    CHECK( restored.cooldown_active( 29 ) );

    restored.advance_to( 30 );
    CHECK( restored.phase == lifecycle_phase::searching );
    CHECK( restored.search_until_turn == 50 );
    const int attempts_after_expiry = restored.attempts_spent;
    restored.advance_to( 30 );
    CHECK( restored.attempts_spent == attempts_after_expiry );
    restored.advance_to( 50 );
    CHECK( restored.phase == lifecycle_phase::idle );
    CHECK( restored.attempts_spent == 0 );
    CHECK_FALSE( restored.has_committed_waypoint );
    CHECK_FALSE( restored.has_retreat_waypoint );
}

TEST_CASE( "writhing_stalker_no_target_plan_retains_committed_waypoint",
           "[writhing_stalker][monster][routing]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    here.ter_set( center + point::east, ter_id( "t_wall" ) );
    refresh_writhing_stalker_arena( here );

    monster &stalker = spawn_new_stalker_fixture( center + point::east * 2 );
    auto &state = stalker.writhing_stalker_state();
    stalker.anger = 0;
    stalker.aggro_character = false;
    const bool native_seen = stalker.sees( here, you );
    CAPTURE( stalker.pos_abs(), you.pos_abs(), here.ter( center + point::east ).id().str(),
             here.get_cache_ref( 0 ).seen_cache[stalker.pos_bub().x()][stalker.pos_bub().y()],
             here.get_cache_ref( 0 ).vision_transparency_cache[center.x() + 1][center.y()],
             stalker.sees_without_clairvoyance_physical( here, you ) );
    CAPTURE( trigdist, here.supports_zlevels(), you.clairvoyance(),
             get_avatar().get_active_moncams().size(),
             here.get_cache_ref( 0 ).no_floor_gaps,
             here.get_cache_ref( 1 ).floor_cache[center.x()][center.y()],
             here.get_cache_ref( -1 ).floor_cache[center.x()][center.y()] );
    REQUIRE_FALSE( native_seen );
    state.phase = writhing_stalker::lifecycle_phase::shadowing;
    state.committed_waypoint = stalker.pos_abs() + point::east * 3;
    state.has_committed_waypoint = true;
    stalker.plan();

    CHECK( here.get_bub( stalker.get_dest() ) == here.get_bub( state.committed_waypoint ) );

    // Reaching the remembered area starts bounded search instead of holding the
    // same destination forever, and repeated calls at one turn do not refresh it.
    state.phase = writhing_stalker::lifecycle_phase::shadowing;
    state.committed_waypoint = stalker.pos_abs();
    state.has_committed_waypoint = true;
    stalker.plan();
    REQUIRE( state.phase == writhing_stalker::lifecycle_phase::searching );
    const int search_deadline = state.search_until_turn;
    CHECK_FALSE( stalker.has_dest() );
    stalker.plan();
    CHECK( state.search_until_turn == search_deadline );

    // A route that makes no progress for the stall window must not be written
    // back as the same stale destination after entering bounded search.
    state.phase = writhing_stalker::lifecycle_phase::shadowing;
    state.committed_waypoint = stalker.pos_abs() + point::east * 3;
    state.has_committed_waypoint = true;
    state.last_observed_position = state.committed_waypoint;
    state.has_last_observed_position = true;
    state.remaining_route_progress = 3;
    state.last_progress_turn = 0;
    stalker.unset_dest();
    set_time( calendar::turn_zero + 51_turns );
    stalker.plan();
    CHECK( state.phase == writhing_stalker::lifecycle_phase::searching );
    CHECK_FALSE( stalker.has_dest() );

    // If a distinct remembered observation remains reachable, the stalled
    // route may replan to it, but must reset progress to the new waypoint.
    state.phase = writhing_stalker::lifecycle_phase::shadowing;
    state.committed_waypoint = stalker.pos_abs() + point::east * 3;
    state.has_committed_waypoint = true;
    state.last_observed_position = stalker.pos_abs() + point::north * 3;
    state.has_last_observed_position = true;
    state.remaining_route_progress = 3;
    state.last_progress_turn = 0;
    set_time( calendar::turn_zero + 102_turns );
    stalker.plan();
    CHECK( here.get_bub( stalker.get_dest() ) == here.get_bub( state.last_observed_position ) );
    CHECK( state.remaining_route_progress == 3 );
    CHECK( state.last_progress_turn == 102 );

    set_time( calendar::turn_zero + 122_turns );
    stalker.plan();
    CHECK( state.phase == writhing_stalker::lifecycle_phase::idle );
    clear_map_without_vision();
}

TEST_CASE( "writhing_stalker_hidden_target_cannot_refresh_observation_or_route",
           "[writhing_stalker][monster][routing][evidence]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms stalker_start = center + point::east * 2;
    const tripoint_bub_ms hidden_position = center + point::south * 3;
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );

    monster &stalker = spawn_new_stalker_fixture( stalker_start );
    stalker.anger = 100;
    stalker.aggro_character = true;
    REQUIRE( stalker.sees( here, you ) );
    stalker.plan();

    auto &state = stalker.writhing_stalker_state();
    REQUIRE( state.has_last_observed_position );
    const tripoint_abs_ms last_seen = state.last_observed_position;
    const int observation_turn = state.evidence_turn;
    REQUIRE( last_seen == you.pos_abs() );

    // Move the player behind a new wall. The stalker's native plan no longer
    // has a visible target, but this simulates the stale target pointer path
    // that must not overwrite the recognized observation.
    const ter_id t_wall( "t_wall" );
    for( int x = center.x() - 12; x <= center.x() + 12; ++x ) {
        here.ter_set( tripoint_bub_ms( x, center.y() + 1, center.z() ), t_wall );
    }
    you.setpos( here, hidden_position );
    refresh_writhing_stalker_arena( here );
    REQUIRE_FALSE( stalker.sees( here, you ) );
    stalker.plan();

    CHECK( state.last_observed_position == last_seen );
    CHECK( state.evidence_turn == observation_turn );
    const bool routed_to_hidden_position = stalker.has_dest() && stalker.get_dest() == you.pos_abs();
    CHECK_FALSE( routed_to_hidden_position );
    if( stalker.has_dest() ) {
        const bool routed_to_recognized_route = stalker.get_dest() == state.committed_waypoint ||
                                                stalker.get_dest() == last_seen;
        CHECK( routed_to_recognized_route );
    } else {
        const bool bounded_search_or_idle = state.phase == writhing_stalker::lifecycle_phase::searching ||
                                            state.phase == writhing_stalker::lifecycle_phase::idle;
        CHECK( bounded_search_or_idle );
    }
    clear_map_without_vision();
}

TEST_CASE( "writhing_stalker_pressure_history_is_visible_absolute_and_lifecycle_bounded",
           "[writhing_stalker][monster][pressure][lifecycle]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );

    monster &stalker = spawn_new_stalker_fixture( center + point::east * 6 );
    monster &pressure = spawn_test_monster( mon_zombie.str(), center + point::east * 3 );
    pressure.set_dest( you.pos_abs() );
    pressure.aggro_character = true;
    REQUIRE( stalker.sees( here, pressure ) );
    REQUIRE( pressure.sees( here, you ) );
    const scout_observation::actor_capability capability{ 72, 1.0f };
    const auto compare_visible_read = [&]() {
        scout_observation::site_reader complete;
        scout_observation::site_reader visible_only;
        const auto full = complete.read_actor( stalker, pressure, capability );
        const auto fast = visible_only.read_actor( stalker, pressure, capability, true );
        CHECK( fast.visible == full.visible );
        return fast;
    };
    CHECK( compare_visible_read().visible );

    writhing_stalker::reset_transient_pressure_history();
    CHECK( writhing_stalker::observed_zombie_pressure( stalker, here, you ) == 1 );
    CHECK( writhing_stalker::transient_pressure_sample_count() == 1 );
    // A repeated same-turn facade read must not duplicate a position sample.
    CHECK( writhing_stalker::observed_zombie_pressure( stalker, here, you ) == 1 );
    CHECK( writhing_stalker::transient_pressure_sample_count() == 1 );

    // The stalker can see the zombie, but the zombie cannot see the target.
    // Neither pressure nor a position sample may be recorded for that view.
    writhing_stalker::reset_transient_pressure_history();
    here.ter_set( center + point::east, ter_id( "t_wall" ) );
    refresh_writhing_stalker_arena( here );
    REQUIRE( stalker.sees( here, pressure ) );
    REQUIRE_FALSE( pressure.sees( here, you ) );
    CHECK( writhing_stalker::observed_zombie_pressure( stalker, here, you ) == 0 );
    CHECK( writhing_stalker::transient_pressure_sample_count() == 0 );
    here.ter_set( center + point::east, ter_id( "t_floor" ) );

    // Occluded motion supplies no sample. After reacquisition, current native
    // target intent qualifies independently of any unobserved closing motion.
    writhing_stalker::reset_transient_pressure_history();
    here.ter_set( center + point::east * 4, ter_id( "t_wall" ) );
    refresh_writhing_stalker_arena( here );
    REQUIRE_FALSE( stalker.sees( here, pressure ) );
    const auto blocked = compare_visible_read();
    CHECK_FALSE( blocked.visible );
    CHECK( blocked.reason == "actor_physical_ray_blocked" );
    CHECK( writhing_stalker::observed_zombie_pressure( stalker, here, you ) == 0 );
    CHECK( writhing_stalker::transient_pressure_sample_count() == 0 );
    here.ter_set( center + point::east * 4, ter_id( "t_floor" ) );
    pressure.setpos( here, center + point::east * 5 );
    refresh_writhing_stalker_arena( here );
    CHECK( compare_visible_read().visible ); // Native adjacent-actor rule.
    pressure.setpos( here, center + point::east * 2 );
    pressure.set_dest( you.pos_abs() );
    set_time( calendar::turn_zero + 1_turns );
    refresh_writhing_stalker_arena( here );
    REQUIRE( stalker.sees( here, pressure ) );
    CHECK( writhing_stalker::observed_zombie_pressure( stalker, here, you ) == 1 );
    CHECK( writhing_stalker::transient_pressure_sample_count() == 1 );

    // Replacing a live actor must discard its old identity sample rather than
    // accumulating a closing-history trail under the surviving stalker.
    g->remove_zombie( pressure );
    for( int replacement = 0; replacement < 5; ++replacement ) {
        monster &new_pressure = spawn_test_monster( mon_zombie.str(), center + point::east * 3 );
        new_pressure.set_dest( you.pos_abs() );
        new_pressure.aggro_character = true;
        set_time( calendar::turn_zero + time_duration::from_turns( 2 + replacement ) );
        refresh_writhing_stalker_arena( here );
        REQUIRE( stalker.sees( here, new_pressure ) );
        CHECK( writhing_stalker::observed_zombie_pressure( stalker, here, you ) == 1 );
        CHECK( writhing_stalker::transient_pressure_sample_count() == 1 );
        g->remove_zombie( new_pressure );
    }

    // clear_map_without_vision clears the creature tracker through game::clear_zombies,
    // the real reset owner for this transient cache in same-process test worlds.
    clear_map_without_vision();
    CHECK( writhing_stalker::transient_pressure_sample_count() == 0 );
}

TEST_CASE( "writhing_stalker_pressure_history_survives_a_real_bubble_shift",
           "[writhing_stalker][monster][pressure][lifecycle]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero );
    prepare_writhing_stalker_arena( here, center );
    // The player drives the bubble rebase; the NPC remains the observed target.
    you.setpos( here, center + point::south * 30 );
    npc &target = spawn_npc( point_bub_ms( center.xy() ), "test_talker" );
    monster &stalker = spawn_new_stalker_fixture( center + point::east * 6 );
    monster &pressure = spawn_test_monster( mon_zombie.str(), center + point::east * 3 );
    pressure.set_dest( target.pos_abs() );
    pressure.aggro_character = true;
    refresh_writhing_stalker_arena( here );
    REQUIRE( stalker.sees( here, pressure ) );
    REQUIRE( pressure.sees( here, target ) );
    REQUIRE( pressure.attack_target() == &target );

    writhing_stalker::reset_transient_pressure_history();
    CHECK( writhing_stalker::observed_zombie_pressure( stalker, here, target ) == 1 );

    // A real visible approach is still recognized as meaningful pressure.
    pressure.setpos( here, center + point::east * 2 );
    pressure.set_dest( target.pos_abs() );
    set_time( calendar::turn_zero + 1_turns );
    refresh_writhing_stalker_arena( here );
    REQUIRE( pressure.attack_target() == &target );
    CHECK( writhing_stalker::observed_zombie_pressure( stalker, here, target ) == 1 );

    const tripoint_abs_ms target_before_shift = target.pos_abs();
    const tripoint_abs_ms pressure_before_shift = pressure.pos_abs();
    const tripoint_abs_ms stalker_before_shift = stalker.pos_abs();
    // This crosses the production bubble boundary.  game::update_map shifts
    // local coordinates for all active creatures while retaining their world
    // positions, which is precisely the stale-bubble-coordinate failure mode.
    g->place_player( tripoint_bub_ms( 120, you.pos_bub().y(), you.pos_bub().z() ) );
    refresh_writhing_stalker_arena( here );
    CHECK( target.pos_abs() == target_before_shift );
    CHECK( pressure.pos_abs() == pressure_before_shift );
    CHECK( stalker.pos_abs() == stalker_before_shift );
    set_time( calendar::turn_zero + 2_turns );
    REQUIRE( stalker.sees( here, pressure ) );
    REQUIRE( pressure.sees( here, target ) );
    REQUIRE( pressure.attack_target() == &target );
    // No actor closed distance after the rebase. Current native target intent
    // still qualifies once; absolute positions and sample uniqueness survive.
    CHECK( writhing_stalker::observed_zombie_pressure( stalker, here, target ) == 1 );
    CHECK( writhing_stalker::transient_pressure_sample_count() == 1 );
    clear_map_without_vision();
}

TEST_CASE( "writhing_stalker_monster_save_load_owns_typed_state_without_legacy_migration",
           "[writhing_stalker][monster][persistence]" )
{
    monster original( mon_writhing_stalker );
    auto &state = original.writhing_stalker_state();
    original.predator_state().actor_id = "same-process-save-reload-actor";
    original.predator_state().ammo_initialization_version = 2;
    original.predator_state().band_reference = "opaque-band-cache";
    original.predator_state().band_revision_cache = 4;
    state.phase = writhing_stalker::lifecycle_phase::retreating;
    state.evidence_target = 77;
    state.evidence_turn = 123;
    state.light_observed_position = tripoint_abs_ms( 12, 13, 0 );
    state.has_light_observed_position = true;
    state.light_observed_turn = 119;
    state.light_expires_turn = 209;
    state.light_sample_id = "window-glow#119";
    state.retreat_waypoint = tripoint_abs_ms( 20, 21, 0 );
    state.has_retreat_waypoint = true;
    state.attempts_spent = 2;
    state.attempt_sequence = 8;
    state.phase_entered_turn = 120;
    state.cooldown_until_turn = 180;
    state.last_progress_turn = 125;
    state.remaining_route_progress = 4;

    // These are intentionally contradictory legacy diagnostics.  The nested
    // typed object must remain authoritative and no old value is migrated.
    original.set_value( "caol_writhing_stalker_handoff_intent", "shadowing" );
    original.set_value( "caol_writhing_stalker_handoff_cooldown", 1 );
    original.set_value( "caol_writhing_stalker_stalk_omt", 99 );

    const monster loaded = round_trip_writhing_stalker( original );
    const auto &restored = loaded.writhing_stalker_state();
    CHECK( loaded.predator_state().actor_id == original.predator_state().actor_id );
    CHECK( loaded.predator_state().ammo_initialization_version == 2 );
    CHECK( loaded.predator_state().band_reference == "opaque-band-cache" );
    CHECK( loaded.predator_state().band_revision_cache == 4 );
    CHECK( restored.phase == writhing_stalker::lifecycle_phase::retreating );
    CHECK( restored.evidence_target == 77 );
    CHECK( restored.evidence_turn == 123 );
    CHECK( restored.has_light_observed_position );
    CHECK( restored.light_observed_position == tripoint_abs_ms( 12, 13, 0 ) );
    CHECK( restored.light_observed_turn == 119 );
    CHECK( restored.light_expires_turn == 209 );
    CHECK( restored.light_sample_id == "window-glow#119" );
    CHECK( restored.retreat_waypoint == tripoint_abs_ms( 20, 21, 0 ) );
    CHECK( restored.attempts_spent == 2 );
    CHECK( restored.attempt_sequence == 8 );
    CHECK( restored.cooldown_until_turn == 180 );
    CHECK( restored.last_progress_turn == 125 );
    CHECK( restored.remaining_route_progress == 4 );
}

TEST_CASE( "writhing_stalker_light_interest_is_uncertain_finite_and_never_replaces_direct_prey",
           "[writhing_stalker][ai][light][memory]" )
{
    using namespace writhing_stalker;
    persistent_state state;
    const tripoint_abs_ms first_glow( 40, 50, 0 );
    const tripoint_abs_ms later_glow( 60, 70, 0 );

    CHECK( state.observe_light_interest( first_glow, "lamp#100", 100, 90 ) );
    CHECK( state.light_interest_active( 101 ) );
    CHECK( state.has_light_observed_position );
    CHECK( state.light_observed_position == first_glow );
    CHECK( state.evidence_target == 0 );
    CHECK( state.phase == lifecycle_phase::shadowing );
    CHECK( state.committed_waypoint == first_glow );
    CHECK_FALSE( state.observe_light_interest( later_glow, "lamp#100", 100, 90 ) );
    CHECK_FALSE( state.observe_light_interest( later_glow, "lamp#99", 99, 90 ) );
    CHECK( state.light_observed_position == first_glow );

    // A later physical sample can replace the uncertain area, but its expiry
    // remains anchored to that detection rather than the old blink.
    CHECK( state.observe_light_interest( later_glow, "lamp#120", 120, 90 ) );
    CHECK( state.light_observed_position == later_glow );
    CHECK( state.light_expires_turn == 210 );
    state.advance_to( 210 );
    CHECK_FALSE( state.light_interest_active( 210 ) );
    CHECK_FALSE( state.has_light_observed_position );

    state.evidence_target = 17;
    state.evidence_turn = 220;
    state.last_observed_position = tripoint_abs_ms( 21, 22, 0 );
    state.has_last_observed_position = true;
    CHECK( state.has_direct_evidence( 221 ) );
    CHECK_FALSE( state.observe_light_interest( first_glow, "lamp#221", 221, 90 ) );
    CHECK_FALSE( state.has_light_observed_position );

    state.evidence_target = 0;
    state.phase = lifecycle_phase::attacking;
    CHECK_FALSE( state.observe_light_interest( first_glow, "lamp#500", 500, 90 ) );
    CHECK_FALSE( state.has_light_observed_position );
}

TEST_CASE( "writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat",
           "[writhing_stalker][ai][pattern]" )
{
    using namespace writhing_stalker;

    const std::vector<stalker_pattern_row> rows = run_vulnerable_stalker_pattern( 8 );
    INFO( "writhing stalker pattern trace:\n" << trace_rows( rows ) );

    REQUIRE( rows.size() == 8 );
    CHECK( rows[0].next == decision::shadow );
    CHECK( rows[0].route == approach_class::cover_shadow );
    CHECK( rows[1].next == decision::strike );
    CHECK( rows[2].next == decision::strike );
    CHECK( rows[3].hp_percent == 50 );
    CHECK( rows[3].next == decision::withdraw );
    CHECK( rows[3].reason == "live_stalker_hurt_withdraw" );

    CHECK( count_decisions( rows, decision::strike ) == 2 );
    CHECK( count_decisions( rows, decision::withdraw ) >= 1 );
    CHECK_FALSE( rows[3].next == decision::strike );
    CHECK_FALSE( rows[4].next == decision::strike );

    int jitter_count = 0;
    for( size_t i = 1; i < rows.size(); ++i ) {
        if( rows[i - 1].next == decision::withdraw && rows[i].next == decision::strike ) {
            ++jitter_count;
        }
    }
    CHECK( jitter_count == 0 );
}

TEST_CASE( "stalker_pursuit_observed_heading_is_bounded_and_world_facts_survive_pause",
           "[writhing_stalker][pursuit]" )
{
    using namespace writhing_stalker;
    persistent_state state;
    state.advance_to( 100 );
    REQUIRE( state.observe_target( 77, 1, tripoint_abs_ms( 100, 100, 0 ), 100 ) );
    CHECK( state.heading_interval == 0 );
    state.advance_to( 110 );
    REQUIRE( state.observe_target( 77, 1, tripoint_abs_ms( 100, 90, 0 ), 110 ) );
    CHECK( state.heading == tripoint_rel_ms( 0, -10, 0 ) );
    CHECK( state.heading_interval == 10 );
    state.advance_to( 140 );
    const auto identity_route = []( const tripoint_abs_ms & p, bool ) {
        return std::optional<tripoint_abs_ms>( p );
    };
    CHECK( state.pursuit_waypoint( tripoint_abs_ms( 100, 120, 0 ), identity_route ) ==
           tripoint_abs_ms( 100, 66, 0 ) );
    CHECK( state.evidence_turn == 110 );
    CHECK( state.last_observed_position == tripoint_abs_ms( 100, 90, 0 ) );

    // A visible turn replaces the northbound heading. Reloading the observer
    // or target does not confuse a stable Character ID with a new runtime ID.
    state.advance_to( 150 );
    REQUIRE( state.observe_target( 88, 1, tripoint_abs_ms( 110, 90, 0 ), 150 ) );
    CHECK( state.heading == tripoint_rel_ms( 10, 0, 0 ) );
    CHECK( state.heading_interval == 40 );
    state.advance_to( 151 );
    REQUIRE( state.observe_target( 88, 1, tripoint_abs_ms( 110, 90, 0 ), 151 ) );
    CHECK( state.heading_interval == 0 );
    CHECK_FALSE( state.observe_target( 88, 1, tripoint_abs_ms( 999, 999, 0 ), 151 ) );

    std::ostringstream saved;
    { JsonOut json( saved ); state.serialize( json ); }
    persistent_state loaded;
    loaded.deserialize( json_loader::from_string( saved.str() ) );
    const int active = loaded.active_turns;
    loaded.advance_to( 100000, 0 );
    CHECK( loaded.active_turns == active );
    CHECK( loaded.evidence_turn == 151 );
    CHECK_FALSE( loaded.has_direct_evidence( 100000 ) );
    loaded.advance_to( 100001, 1 );
    CHECK( loaded.active_turns == active + 1 );
    loaded.advance_to( 100001, 200 );
    CHECK( loaded.active_turns == active + 1 );

    persistent_state old_save;
    old_save.deserialize( json_loader::from_string( "{\"last_advanced_turn\":10}" ) );
    old_save.advance_to( 100000 );
    CHECK( old_save.active_turns == 0 );
    CHECK_FALSE( old_save.has_pursuit_memory );

    // Cumulative active deadlines do not wrap at the old 32-bit turn bound.
    loaded.active_turns = std::numeric_limits<int>::max();
    loaded.advance_to( 100002, 10 );
    CHECK( loaded.active_turns == static_cast<std::int64_t>( std::numeric_limits<int>::max() ) + 10 );
    REQUIRE( loaded.observe_target( 88, 1, tripoint_abs_ms( 110, 90, 0 ), 100002 ) );
    loaded.finish_search();
    CHECK( loaded.familiar[0].revisit_at_active > loaded.active_turns );
}

TEST_CASE( "stalker_pursuit_search_revisits_are_finite_and_do_not_invent_sightings",
           "[writhing_stalker][pursuit]" )
{
    using namespace writhing_stalker;
    persistent_state state;
    state.advance_to( 100 );
    REQUIRE( state.observe_target( 77, 1, tripoint_abs_ms( 100, 100, 0 ), 100 ) );
    state.advance_to( 161 );
    const auto open_route = []( const tripoint_abs_ms & p, bool ) {
        return std::optional<tripoint_abs_ms>( p );
    };
    tripoint_abs_ms position( 80, 100, 0 );
    for( int step = 0; step < 9; ++step ) {
        const auto waypoint = state.pursuit_waypoint( position, open_route );
        REQUIRE( waypoint );
        CHECK( rl_dist( project_to<coords::omt>( *waypoint ),
                        project_to<coords::omt>( state.last_observed_position ) ) <= 1 );
        position = *waypoint;
        state.advance_to( 162 + step );
    }
    CHECK_FALSE( state.pursuit_waypoint( position, open_route ) );
    CHECK( state.phase == lifecycle_phase::idle );
    CHECK( state.searched_positions == 511 );
    CHECK( state.evidence_turn == 100 );
    CHECK( state.familiar[0].last_seen_world == 100 );
    const int due = state.familiar[0].revisit_at_active;
    CHECK( due >= state.active_turns + to_turns<int>( 6_hours ) );
    CHECK( due <= state.active_turns + to_turns<int>( 12_hours ) );
    CHECK_FALSE( state.pursuit_waypoint( position, open_route ) );
    CHECK( state.familiar[0].revisit_at_active == due );

    std::ostringstream serialized;
    { JsonOut json( serialized ); state.serialize( json ); }
    persistent_state reloaded;
    reloaded.deserialize( json_loader::from_string( serialized.str() ) );
    CHECK( reloaded.familiar[0].revisit_at_active == due );
    reloaded.advance_to( 100000, due - reloaded.active_turns );
    const auto revisit = reloaded.pursuit_waypoint( position + point::east, open_route );
    REQUIRE( revisit );
    CHECK( reloaded.revisiting_slot == 0 );
    const int revisit_start = reloaded.active_turns;
    reloaded.advance_to( 100600, to_turns<int>( 10_minutes ) );
    CHECK_FALSE( reloaded.pursuit_waypoint( position + point::east, open_route ) );
    CHECK( reloaded.phase == lifecycle_phase::idle );
    CHECK( reloaded.familiar[0].revisit_at_active >= revisit_start +
           to_turns<int>( 10_minutes + 6_hours ) );
    CHECK( reloaded.familiar[0].last_seen_world == 100 );
    CHECK_FALSE( reloaded.pursuit_waypoint( position, open_route ) );

    // An independently blocked observer may end a search without claiming it
    // searched the nine rejected positions or refreshing the learned place.
    persistent_state blocked;
    blocked.advance_to( 10 );
    blocked.observe_target( 99, 2, tripoint_abs_ms( 10, 10, 0 ), 10 );
    const auto no_route = []( const tripoint_abs_ms &, bool ) {
        return std::optional<tripoint_abs_ms>();
    };
    CHECK_FALSE( blocked.pursuit_waypoint( position, no_route ) );
    CHECK( blocked.searched_positions == 0 );
    CHECK( blocked.familiar[0].last_seen_world == 10 );
    CHECK( state.familiar[0].last_seen_world == 100 );
}

TEST_CASE( "stalker_pursuit_stalled_search_has_active_deadline_and_revisit_cannot_chain",
           "[writhing_stalker][pursuit]" )
{
    using namespace writhing_stalker;
    persistent_state state;
    state.advance_to( 100 );
    state.observe_target( 77, 1, tripoint_abs_ms( 100, 100, 0 ), 100 );
    state.advance_to( 161 );
    const tripoint_abs_ms waiting( 80, 100, 0 );
    const auto route = []( const tripoint_abs_ms & p, bool ) {
        return std::optional<tripoint_abs_ms>( p );
    };
    REQUIRE( state.pursuit_waypoint( waiting, route ) );
    const int started = state.search_started_active;
    state.advance_to( 100000, 0 );
    REQUIRE( state.pursuit_waypoint( waiting, route ) );
    CHECK( state.search_started_active == started );
    CHECK( state.searched_positions == 0 );
    state.advance_to( 100001, to_turns<int>( 30_minutes ) );
    CHECK_FALSE( state.pursuit_waypoint( waiting, route ) );
    CHECK( state.phase == lifecycle_phase::idle );
    CHECK( state.evidence_turn == 100 );

    state.familiar[1] = state.familiar[0];
    state.familiar[1].position += point::east;
    state.familiar[0].revisit_at_active = state.active_turns;
    state.familiar[1].revisit_at_active = state.active_turns;
    const auto blocked = []( const tripoint_abs_ms &, bool ) {
        return std::optional<tripoint_abs_ms>();
    };
    CHECK_FALSE( state.pursuit_waypoint( waiting, blocked ) );
    CHECK( state.phase == lifecycle_phase::idle );
    for( const auto &place : state.familiar ) {
        CHECK( place.revisit_at_active >= state.active_turns + to_turns<int>( 6_hours ) );
        CHECK( place.last_seen_world == 100 );
    }
    CHECK_FALSE( state.pursuit_waypoint( waiting, route ) );

    persistent_state delayed;
    delayed.advance_to( 10 );
    delayed.observe_target( 9, 1, tripoint_abs_ms( 100, 100, 0 ), 10 );
    delayed.advance_to( 100000, to_turns<int>( 32_minutes ) );
    CHECK_FALSE( delayed.pursuit_waypoint( waiting, route ) );
    CHECK( delayed.phase == lifecycle_phase::idle );
    CHECK( delayed.searched_positions == 0 );
}

TEST_CASE( "stalker_pursuit_familiar_places_use_two_slots_and_active_expiry",
           "[writhing_stalker][pursuit]" )
{
    using namespace writhing_stalker;
    persistent_state state;
    state.advance_to( 1 );
    state.observe_target( 1, 1, tripoint_abs_ms( 100, 100, 0 ), 1 );
    state.advance_to( 2 );
    state.observe_target( 1, 1, tripoint_abs_ms( 200, 100, 0 ), 2 );
    state.advance_to( 3 );
    state.observe_target( 1, 1, tripoint_abs_ms( 100, 100, 0 ), 3 );
    state.advance_to( 4 );
    state.observe_target( 1, 1, tripoint_abs_ms( 300, 100, 0 ), 4 );
    CHECK( state.familiar[0].last_seen_world == 3 );
    CHECK( state.familiar[1].last_seen_world == 4 );
    state.advance_to( 10000000, 0 );
    CHECK( state.familiar[0].valid );
    CHECK( state.familiar[1].valid );
    state.advance_to( 10000001, to_turns<int>( 7_days ) );
    CHECK_FALSE( state.familiar[0].valid );
    CHECK_FALSE( state.familiar[1].valid );
    CHECK( state.evidence_turn == 4 );
}

TEST_CASE( "stalker_closing_a_real_attack_opening_replaces_the_attack_destination",
           "[writhing_stalker][pursuit][monster][opening_transition]" )
{
    clear_map_without_vision();
    clear_npcs();
    map &here = get_map();
    Character &you = get_player_character();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1_days );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center + point::east * 3 );
    stalker.anger = 100;
    stalker.aggro_character = true;
    // Actual visible pressure supplies the evaluator's commitment footing;
    // dark solitude, rather than an invented attack, supplies the opening.
    std::vector<monster *> pressure;
    for( const point &offset : { point::west, point::south, point::east, point( -2, 2 ) } ) {
        monster &zombie = spawn_test_monster( mon_zombie.str(), center + offset );
        zombie.aggro_character = true;
        zombie.set_dest( you.pos_abs() );
        pressure.push_back( &zombie );
    }
    stalker.plan();
    auto &state = stalker.writhing_stalker_state();
    REQUIRE( state.phase == writhing_stalker::lifecycle_phase::attacking );
    REQUIRE( state.committed_waypoint == you.pos_abs() );
    REQUIRE( stalker.get_dest() == you.pos_abs() );
    const auto target_position = you.pos_abs();

    // The target stays still, so a new sighting cannot incidentally invalidate
    // the old attack waypoint. Cover both the explicit opening veto and a
    // natural evaluator shadow exit, while dark solitude itself remains true.
    const bool evaluator_shadow = GENERATE( false, true );
    if( evaluator_shadow ) {
        // Departing pressure is a fixture change, not a combat-victory claim.
        for( monster *zombie : pressure ) {
            g->remove_zombie( *zombie );
        }
    } else {
        npc &ally = spawn_npc( point_bub_ms( ( center + point::south_west ).xy() ), "test_talker" );
        ally.set_fac( faction_id( "your_followers" ) );
        ally.set_attitude( NPCATT_FOLLOW );
        refresh_writhing_stalker_arena( here );
        REQUIRE( ally.attitude_to( you ) == Creature::Attitude::FRIENDLY );
        REQUIRE( ally.sees( here, you ) );
        REQUIRE( writhing_stalker::can_observe_actor( stalker, ally ) );
    }
    set_time( calendar::turn + 1_turns );
    if( evaluator_shadow ) {
        REQUIRE( writhing_stalker::observed_zombie_pressure( stalker, here, you ) == 0 );
    }
    stalker.plan();
    REQUIRE( you.pos_abs() == target_position );
    REQUIRE( state.phase == writhing_stalker::lifecycle_phase::shadowing );
    CHECK_FALSE( ( state.has_committed_waypoint && state.committed_waypoint == target_position ) );
    REQUIRE( stalker.has_dest() );
    CHECK( rl_dist( stalker.get_dest(), target_position ) >= 2 );
    const int target_hp = you.get_hp();
    stalker.set_moves( 100 );
    stalker.move();
    CHECK( you.get_hp() == target_hp );
    CHECK( state.attempts_spent == 0 );
    CHECK( rl_dist( stalker.pos_abs(), target_position ) >= 2 );
    clear_npcs();
    clear_map_without_vision();
}

TEST_CASE( "stalker_finished_pursuit_then_light_arrival_expires_after_reload_and_repeat",
           "[writhing_stalker][pursuit][monster][light_transition]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1_days );
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center + point::east * 3 );
    stalker.anger = 100;
    stalker.aggro_character = true;
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().has_pursuit_memory );
    const int observed = stalker.writhing_stalker_state().evidence_turn;
    you.setpos( here, center + point::west * 20 );
    for( int y = 0; y < 132; ++y ) {
        here.ter_set( tripoint_bub_ms( 55, y, 0 ), ter_id( "t_wall" ) );
    }
    refresh_writhing_stalker_arena( here );
    REQUIRE_FALSE( writhing_stalker::can_observe_actor( stalker, you ) );
    set_time( calendar::turn + 32_minutes );
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    REQUIRE( stalker.writhing_stalker_state().sighting_active_turn == -1 );
    const auto glow = stalker.pos_abs() + point::east * 3;
    // A fresh physical-light adapter input, not a target sighting. The caller
    // route and actual movement/arrival below own the search deadline proof.
    REQUIRE( stalker.writhing_stalker_state().observe_light_interest(
                 glow, "post-pursuit-lamp", to_turn<int>( calendar::turn ), 90 ) );
    for( int steps = 0; steps < 12 && stalker.pos_abs() != glow; ++steps ) {
        stalker.plan();
        stalker.set_moves( 100 );
        stalker.move();
        set_time( calendar::turn + 1_turns );
    }
    REQUIRE( stalker.pos_abs() == glow );
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::searching );
    const int deadline = stalker.writhing_stalker_state().search_until_turn;
    REQUIRE( deadline == to_turn<int>( calendar::turn ) + 20 );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
        REQUIRE( stalker.writhing_stalker_state().search_until_turn == deadline );
    }
    set_time( calendar::turn_zero + time_duration::from_turns( deadline ) );
    for( int repeat = 0; repeat < 3; ++repeat ) {
        stalker.plan();
        CHECK( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
        CHECK_FALSE( stalker.writhing_stalker_state().has_committed_waypoint );
        CHECK_FALSE( ( stalker.has_dest() && stalker.get_dest() == glow ) );
        CHECK( stalker.writhing_stalker_state().evidence_turn == observed );
        CHECK( stalker.writhing_stalker_state().has_pursuit_memory );
    }
    clear_map_without_vision();
}

TEST_CASE( "stalker_actor_weather_uses_the_physical_ray_without_indoor_range_penalty",
           "[writhing_stalker][pursuit][monster][weather]" )
{
    clear_map_without_vision();
    clear_avatar();
    map &here = get_map();
    Character &you = get_player_character();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 12_hours );
    const bool indoors = GENERATE( false, true );
    for( int x = 0; x < 132; ++x ) {
        for( int y = 0; y < 132; ++y ) {
            here.ter_set( tripoint_bub_ms( x, y, 0 ), ter_id( indoors ? "t_floor" : "t_grass" ) );
            here.ter_set( tripoint_bub_ms( x, y, 1 ), ter_id( indoors ? "t_flat_roof" : "t_open_air" ) );
            here.furn_clear( tripoint_bub_ms( x, y, 0 ) );
        }
    }
    const tripoint_bub_ms origin( 10, 65, 0 );
    you.setpos( here, origin + point::east * ( indoors ? 20 : 71 ) );
    monster &stalker = spawn_new_stalker_fixture( origin );
    for( const char *weather_id : { "clear", "fog" } ) {
        scoped_weather_override weather{ weather_type_id( weather_id ) };
        here.invalidate_map_cache( 1 );
        refresh_writhing_stalker_arena( here );
        const int range = std::min( 72, static_cast<int>( std::floor(
                                       stalker.sight_range( here.ambient_light_at( you.pos_bub() ) ) * 72.0f / 35 ) ) );
        std::vector<tripoint_abs_omt> footprint;
        for( const auto &cell : scout_observation::site_sightline( stalker.pos_abs(), you.pos_abs() ) ) {
            footprint.push_back( project_to<coords::omt>( cell ) );
        }
        const bool physical = stalker.sees_site_with_range( here, you, range, footprint );
        scout_observation::site_reader reader;
        const auto read = reader.read_actor( stalker, you, { 72, 72.0f / 35 } );
        scout_observation::site_reader visible_only;
        const auto fast = visible_only.read_actor( stalker, you, { 72, 72.0f / 35 }, true );
        CAPTURE( indoors, weather_id, range, read.reason, physical );
        REQUIRE( here.is_outside( you.pos_bub() ) == !indoors );
        REQUIRE( read.known );
        CHECK( read.visible == physical );
        CHECK( fast.visible == read.visible );
        CHECK( read.visible == ( indoors || std::string( weather_id ) == "clear" ) );
    }
    clear_map_without_vision();
}

TEST_CASE( "stalker_native_retreat_authority_survives_the_abstract_intent_caller",
           "[writhing_stalker][predator][pursuit][monster]" )
{
    clear_overmaps();
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 100_turns );
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center + point::east * 2 );
    stalker.anger = 100;
    stalker.aggro_character = true;
    auto &state = stalker.writhing_stalker_state();
    state.advance_to( 90 );
    REQUIRE( state.observe_target( you.get_identity(), you.getID().get_value(), you.pos_abs(), 90 ) );
    state.committed_waypoint = you.pos_abs();
    state.has_committed_waypoint = true;
    state.attempts_spent = 2;
    stalker.plan();
    REQUIRE( state.phase == writhing_stalker::lifecycle_phase::retreating );
    const bool cooling = GENERATE( false, true );
    if( cooling ) {
        stalker.plan();
        REQUIRE( state.phase == writhing_stalker::lifecycle_phase::cooldown );
    }
    REQUIRE( state.has_retreat_waypoint );
    const auto retreat = state.retreat_waypoint;
    REQUIRE( stalker.get_dest() == retreat );
    REQUIRE( retreat != state.committed_waypoint );
    const auto position = stalker.pos_abs();
    const auto actor = stalker.predator_state().actor_id;
    const auto epoch = stalker.predator_state().handoff_epoch;
    g->despawn_monster( stalker );
    horde_entity *owner = overmap_buffer.entity_at( position );
    REQUIRE( owner != nullptr );
    REQUIRE( owner->monster_data != nullptr );
    REQUIRE( owner->destination == retreat );
    set_time( calendar::turn + 1_turns );
    REQUIRE( owner->advance_predator_intent( position, to_turn<int>( calendar::turn ) ) );
    CHECK( owner->destination == retreat );
    CHECK( owner->monster_data->wander_pos == retreat );
    CHECK( owner->monster_data->predator_state().actor_id == actor );
    CHECK( owner->monster_data->predator_state().handoff_epoch == epoch + 1 );
    const auto phase = owner->monster_data->writhing_stalker_state().phase;
    CHECK( phase == ( cooling ? writhing_stalker::lifecycle_phase::cooldown :
                     writhing_stalker::lifecycle_phase::retreating ) );
    REQUIRE( owner->advance_predator_intent( position, to_turn<int>( calendar::turn ) ) );
    CHECK( owner->destination == retreat );
    clear_overmaps();
    clear_map_without_vision();
}

TEST_CASE( "stalker_pursuit_actual_planner_acquires_at_three_OMT_and_rejects_hidden_turn",
           "[writhing_stalker][pursuit][monster]" )
{
    clear_map_without_vision();
    clear_avatar();
    map &here = get_map();
    Character &you = get_player_character();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 12_hours );
    const tripoint_bub_ms origin( 10, 65, 0 );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    for( int x = 5; x < 100; ++x ) {
        for( int y = 40; y < 80; ++y ) {
            here.ter_set( tripoint_bub_ms( x, y, 0 ), ter_id( "t_grass" ) );
            here.ter_set( tripoint_bub_ms( x, y, 1 ), ter_id( "t_open_air" ) );
            here.furn_clear( tripoint_bub_ms( x, y, 0 ) );
        }
    }
    you.setpos( here, tripoint_bub_ms( 82, 65, 0 ) );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( origin );
    monster &second = spawn_new_stalker_fixture( origin + point::north * 2 );
    stalker.anger = 100;
    stalker.aggro_character = true;
    second.anger = 100;
    second.aggro_character = true;
    REQUIRE_FALSE( stalker.sees( here, you ) );
    REQUIRE( rl_dist( project_to<coords::omt>( stalker.pos_abs() ),
                      project_to<coords::omt>( you.pos_abs() ) ) == 3 );
    scout_observation::site_reader sight;
    const auto direct = sight.read_actor( stalker, you, { 72, 72.0f / 35 } );
    CAPTURE( direct.known, direct.visible, direct.reason, you.visibility(),
             here.ambient_light_at( you.pos_bub() ), stalker.sight_range( 100.0f ),
             static_cast<int>( stalker.attitude_to( you ) ),
             static_cast<int>( stalker.attitude_to( second ) ) );
    // Native avatar visibility is 99%, so exactly72 squares is beyond this
    // target's recognition boundary. One square closer is still three OMT.
    REQUIRE_FALSE( direct.visible );
    you.setpos( here, tripoint_bub_ms( 81, 65, 0 ) );
    refresh_writhing_stalker_arena( here );
    REQUIRE( writhing_stalker::can_observe_actor( stalker, you ) );
    stalker.plan();
    second.plan();
    auto &state = stalker.writhing_stalker_state();
    REQUIRE( state.has_pursuit_memory );
    REQUIRE( second.writhing_stalker_state().has_pursuit_memory );
    const int second_first_sighting = second.writhing_stalker_state().evidence_turn;
    CHECK( state.last_observed_position == you.pos_abs() );
    CHECK( state.sighting_active_turn == state.active_turns );
    REQUIRE( stalker.has_dest() );
    CHECK( rl_dist( stalker.pos_abs(), stalker.get_dest() ) <= 10 );
    if( state.phase == writhing_stalker::lifecycle_phase::retreating ||
        state.phase == writhing_stalker::lifecycle_phase::cooldown ) {
        CHECK( rl_dist( stalker.get_dest(), you.pos_abs() ) >=
               rl_dist( stalker.pos_abs(), you.pos_abs() ) );
    }

    set_time( calendar::turn + 10_turns );
    you.setpos( here, tripoint_bub_ms( 81, 55, 0 ) );
    refresh_writhing_stalker_arena( here );
    stalker.plan();
    CHECK( state.heading == tripoint_rel_ms( 0, -10, 0 ) );
    CHECK( state.sighting_active_turn == state.active_turns );
    CHECK( second.writhing_stalker_state().evidence_turn == second_first_sighting );
    set_time( calendar::turn + 10_turns );
    you.setpos( here, tripoint_bub_ms( 71, 55, 0 ) );
    refresh_writhing_stalker_arena( here );
    stalker.plan();
    CHECK( state.heading == tripoint_rel_ms( -10, 0, 0 ) );
    CHECK( state.sighting_active_turn == state.active_turns );
    const int last_seen = state.evidence_turn;
    const auto last_position = state.last_observed_position;
    for( int y = 0; y < 132; ++y ) {
        here.ter_set( tripoint_bub_ms( 40, y, 0 ), ter_id( "t_wall" ) );
    }
    set_time( calendar::turn + 10_turns );
    you.setpos( here, tripoint_bub_ms( 71, 45, 0 ) );
    refresh_writhing_stalker_arena( here );
    stalker.plan();
    CHECK( state.evidence_turn == last_seen );
    CHECK( state.last_observed_position == last_position );
    CHECK( state.heading == tripoint_rel_ms( -10, 0, 0 ) );
    const bool magically_tracked = stalker.has_dest() && stalker.get_dest() == you.pos_abs();
    CHECK_FALSE( magically_tracked );
    clear_map_without_vision();
}

TEST_CASE( "stalker_pursuit_native_route_uses_short_steps_without_invented_search_progress",
           "[writhing_stalker][pursuit][monster]" )
{
    clear_map_without_vision();
    clear_avatar();
    map &here = get_map();
    Character &you = get_player_character();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time_to_day();
    scoped_weather_override weather( weather_type_id( "clear" ) );
    for( int x = 25; x < 90; ++x ) {
        for( int y = 50; y < 85; ++y ) {
            here.ter_set( tripoint_bub_ms( x, y, 0 ), ter_id( "t_grass" ) );
            here.ter_set( tripoint_bub_ms( x, y, 1 ), ter_id( "t_open_air" ) );
            here.furn_clear( tripoint_bub_ms( x, y, 0 ) );
        }
    }
    you.setpos( here, tripoint_bub_ms( 80, 66, 0 ) );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( tripoint_bub_ms( 30, 66, 0 ) );
    stalker.anger = 100;
    stalker.aggro_character = true;
    REQUIRE( writhing_stalker::observe_current_target( stalker ) == &you );
    auto &state = stalker.writhing_stalker_state();
    const int observation = state.evidence_turn;
    for( int y = 0; y < 132; ++y ) {
        here.ter_set( tripoint_bub_ms( 65, y, 0 ), ter_id( "t_wall" ) );
    }
    set_time( calendar::turn + 1_turns );
    refresh_writhing_stalker_arena( here );
    REQUIRE_FALSE( writhing_stalker::can_observe_actor( stalker, you ) );
    stalker.plan();
    REQUIRE( stalker.has_dest() );
    CHECK( rl_dist( stalker.pos_abs(), stalker.get_dest() ) <= 10 );
    CHECK( stalker.get_dest().x() > stalker.pos_abs().x() );
    const auto before = stalker.pos_abs();
    stalker.set_moves( 100 );
    stalker.move();
    CHECK( stalker.pos_abs() != before );
    CHECK( state.evidence_turn == observation );
    CHECK( state.searched_positions == 0 );

    // Search can travel through intermediate native points without claiming
    // an unreached OMT, and its absolute active deadline is not renewed.
    set_time( calendar::turn + 61_turns );
    stalker.plan();
    const int search_start = state.search_started_active;
    REQUIRE( search_start >= 0 );
    CHECK( state.searched_positions == 0 );
    set_time( calendar::turn + 30_minutes );
    stalker.plan();
    CHECK( state.phase == writhing_stalker::lifecycle_phase::idle );
    CHECK( state.search_started_active == -1 );
    CHECK( state.evidence_turn == observation );
    CHECK_FALSE( state.has_committed_waypoint );
    clear_map_without_vision();
}

TEST_CASE( "stalker_pursuit_native_flight_preempts_visible_and_remembered_approach",
           "[writhing_stalker][pursuit][monster]" )
{
    clear_map_without_vision();
    clear_avatar();
    map &here = get_map();
    Character &you = get_player_character();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time_to_day();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    monster &stalker = spawn_new_stalker_fixture( center + point::east * 5 );
    stalker.anger = 100;
    stalker.aggro_character = true;
    refresh_writhing_stalker_arena( here );
    REQUIRE( writhing_stalker::observe_current_target( stalker ) == &you );
    auto &state = stalker.writhing_stalker_state();
    state.begin_search();
    const bool arrived_sound = GENERATE( false, true );
    if( arrived_sound ) {
        stalker.hear_sound( stalker.pos_bub(), 30, 0, true );
        REQUIRE( stalker.provocative_sound );
    }

    SECTION( "current visible threat retains the native away destination" ) {
        stalker.add_effect( efftype_id( "run" ), 5_minutes );
        REQUIRE( stalker.is_fleeing( you ) );
        stalker.plan();
        CHECK_FALSE( stalker.provocative_sound );
        REQUIRE( stalker.has_dest() );
        CHECK( stalker.get_dest() == stalker.pos_abs() - you.pos_abs() + stalker.pos_abs() );
        CHECK( state.searched_positions == 0 );
        CHECK( state.attempts_spent == 0 );
    }
    SECTION( "hidden threat does not resume a remembered search during flight" ) {
        here.ter_set( center + point::east * 2, ter_id( "t_wall" ) );
        refresh_writhing_stalker_arena( here );
        REQUIRE_FALSE( writhing_stalker::can_observe_actor( stalker, you ) );
        set_time( calendar::turn + 1_turns );
        stalker.plan();
        CHECK_FALSE( stalker.provocative_sound );
        REQUIRE( stalker.has_dest() );
        REQUIRE( state.has_committed_waypoint );
        const auto approach = state.committed_waypoint;
        stalker.add_effect( efftype_id( "run" ), 5_minutes );
        REQUIRE( stalker.is_fleeing( you ) );
        const auto cursor = state.search_cursor;
        set_time( calendar::turn + 1_turns );
        stalker.plan();
        CHECK_FALSE( stalker.provocative_sound );
        CHECK_FALSE( stalker.has_dest() );
        CHECK( state.committed_waypoint == approach );
        CHECK( state.search_cursor == cursor );
        CHECK( state.searched_positions == 0 );
    }
    clear_map_without_vision();
}

TEST_CASE( "stalker_pursuit_native_opening_requires_dark_solitude_or_witnessed_attack",
           "[writhing_stalker][pursuit][monster][counterpressure]" )
{
    clear_map_without_vision();
    clear_avatar();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center( 65, 65, 0 );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    set_time_to_day();
    scoped_weather_override weather( weather_type_id( "clear" ) );
    monster &stalker = spawn_new_stalker_fixture( center + point::east * 3 );
    stalker.anger = 100;
    stalker.aggro_character = true;
    std::vector<monster *> zombies;
    for( const point &offset : { point::west, point::south, point::east, point::south_west } ) {
        auto &zombie = spawn_test_monster( mon_zombie.str(), center + offset );
        zombie.aggro_character = true;
        zombie.set_dest( you.pos_abs() );
        REQUIRE( zombie.attack_target() == &you );
        zombies.push_back( &zombie );
    }
    refresh_writhing_stalker_arena( here );
    REQUIRE( stalker.sees( here, you ) );
    REQUIRE( writhing_stalker::observed_zombie_pressure( stalker, here, you ) == 4 );
    stalker.plan();
    auto &state = stalker.writhing_stalker_state();
    REQUIRE( state.has_pursuit_memory );
    // Four merely closing zombies are not an actual bright-light opening.
    CHECK( state.phase != writhing_stalker::lifecycle_phase::attacking );
    CHECK( state.attempts_spent == 0 );

    SECTION( "a real visible attack resolution supplies the missing predicate" ) {
        CHECK( you.deal_melee_attack( zombies.front(), -1000 ) < 0 );
        REQUIRE( writhing_stalker::has_recent_observed_attack( &stalker, zombies.front(), &you,
                 to_turn<int>( calendar::turn ), 0 ) );
        set_time( calendar::turn + 1_turns );
        stalker.plan();
        CHECK( state.phase == writhing_stalker::lifecycle_phase::attacking );
        CHECK( stalker.get_dest() == you.pos_abs() );
        CHECK( state.attempts_spent == 0 ); // Planning does not spend an attack.
    }
    SECTION( "a target-directed attack hidden behind a wall is not witnessed" ) {
        const int observation = state.evidence_turn;
        here.ter_set( center + point::east * 2, ter_id( "t_wall" ) );
        refresh_writhing_stalker_arena( here );
        CHECK( you.deal_melee_attack( zombies.front(), -1000 ) < 0 );
        CHECK_FALSE( writhing_stalker::has_recent_observed_attack( &stalker, zombies.front(), &you,
                     to_turn<int>( calendar::turn ), 0 ) );
        set_time( calendar::turn + 1_turns );
        stalker.plan();
        CHECK( state.evidence_turn == observation );
        CHECK( state.phase != writhing_stalker::lifecycle_phase::attacking );
    }
    SECTION( "dark solitude supplies an opening without inventing an attack" ) {
        set_time( calendar::turn - time_past_midnight( calendar::turn ) + 1_days );
        for( const auto *zombie : zombies ) {
            CHECK_FALSE( writhing_stalker::has_recent_observed_attack( &stalker, zombie, &you,
                         to_turn<int>( calendar::turn ), 2 ) );
        }
        stalker.plan();
        CHECK( state.phase == writhing_stalker::lifecycle_phase::attacking );
    }
    SECTION( "visible actual allies remove solitude even in darkness" ) {
        for( const point &offset : { point( 2, 2 ), point( -2, 2 ), point( 0, 3 ) } ) {
            npc &ally = spawn_npc( point_bub_ms( ( center + offset ).xy() ), "test_talker" );
            ally.set_fac( faction_id( "your_followers" ) );
            ally.set_attitude( NPCATT_FOLLOW );
        }
        set_time( calendar::turn - time_past_midnight( calendar::turn ) + 1_days );
        refresh_writhing_stalker_arena( here );
        stalker.plan();
        CHECK( state.phase != writhing_stalker::lifecycle_phase::attacking );
    }
    clear_map_without_vision();
}

TEST_CASE( "stalker_real_horde_scheduler_retains_arrival_search_and_revisit",
           "[writhing_stalker][predator][pursuit][scheduler]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 161_turns );
    map &here = get_map();
    const auto learned = project_to<coords::omt>( here.get_abs( tripoint_bub_ms( -36, 65, 0 ) ) );
    const auto center = project_to<coords::ms>( learned ) + point( 12, 12 );
    const auto source = center + point::east;
    REQUIRE_FALSE( here.inbounds( source ) );
    overmap_buffer.get( project_to<coords::om>( source ).xy() );
    for( int x = -60; x <= 60; ++x ) {
        for( int y = -60; y <= 60; ++y ) {
            overmap_buffer.set_passable( center + point( x, y ), true );
        }
    }
    monster original = new_stalker_fixture_body();
    original.set_pos_abs_only( source );
    original.set_moves( 100 );
    auto &memory = original.writhing_stalker_state();
    // An authenticated sighting fixture feeds the real scheduler, not a
    // direct advance_predator_intent call that bypasses bucket admission.
    memory.advance_to( 100 );
    REQUIRE( memory.observe_target( 77, 1, center, 100 ) );
    memory.advance_to( 161 );
    memory.begin_search();
    original.set_dest( center );
    const auto actor = original.predator_state().actor_id;
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    REQUIRE( om.hordes.spawn_entity( source, original ) );
    overmap_buffer.move_hordes();
    horde_entity *owner = overmap_buffer.entity_at( center );
    REQUIRE( owner != nullptr );
    REQUIRE( owner->monster_data != nullptr );
    REQUIRE( owner->tracking_intensity == 0 );
    REQUIRE( owner->monster_data->writhing_stalker_state().search_cursor == 0 );
    const int arrival_turn = owner->monster_data->writhing_stalker_state().last_advanced_turn;
    set_time( calendar::turn + 1_turns );
    overmap_buffer.move_hordes();
    // Arrival must reach the planner again even without a sound/light alert.
    horde_entity *continued = nullptr;
    for( const auto &entry : om.horde_entities() ) {
        if( entry.second->monster_data &&
            entry.second->monster_data->predator_state().actor_id == actor ) {
            continued = entry.second;
        }
    }
    REQUIRE( continued != nullptr );
    const auto &after = continued->monster_data->writhing_stalker_state();
    CHECK( after.last_advanced_turn > arrival_turn );
    CHECK( after.search_cursor >= 1 );
    CHECK( ( after.searched_positions & 1 ) != 0 );
    set_time( calendar::turn + 31_minutes );
    overmap_buffer.move_hordes();
    continued = nullptr;
    tripoint_abs_ms position;
    for( const auto &entry : om.horde_entities() ) {
        if( entry.second->monster_data &&
            entry.second->monster_data->predator_state().actor_id == actor ) {
            continued = entry.second;
            position = entry.first;
        }
    }
    REQUIRE( continued != nullptr );
    auto &idle = continued->monster_data->writhing_stalker_state();
    REQUIRE( idle.phase == writhing_stalker::lifecycle_phase::idle );
    REQUIRE( idle.familiar[0].revisit_at_active > idle.active_turns );
    const auto due = idle.familiar[0].revisit_at_active;
    const auto until_due = due - idle.active_turns;
    const auto epoch = continued->monster_data->predator_state().handoff_epoch;
    const int seen = idle.evidence_turn;
    const bool reload = GENERATE( false, true );
    if( reload ) {
        *continued->monster_data = round_trip_writhing_stalker( *continued->monster_data );
        om.hordes.refresh_entity_bucket( position );
    }
    set_time( calendar::turn + time_duration::from_turns( until_due ) );
    overmap_buffer.move_hordes();
    continued = overmap_buffer.entity_at( position );
    // A revisit may take a real transport step. Find its sole owner again.
    int owners = 0;
    for( const auto &entry : om.horde_entities() ) {
        if( entry.second->monster_data &&
            entry.second->monster_data->predator_state().actor_id == actor ) {
            continued = entry.second;
            ++owners;
        }
    }
    REQUIRE( continued != nullptr );
    CHECK( owners == 1 );
    const auto &revisit = continued->monster_data->writhing_stalker_state();
    CHECK( revisit.revisiting_slot == 0 );
    CHECK( revisit.evidence_turn == seen );
    CHECK( continued->monster_data->predator_state().handoff_epoch == epoch );
    const auto active = revisit.active_turns;
    overmap_buffer.move_hordes();
    CHECK( continued->monster_data->writhing_stalker_state().active_turns == active );
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_transferred_route_fallback_ends_with_pursuit_but_real_sound_survives",
           "[writhing_stalker][predator][pursuit][fallback]" )
{
    clear_map_without_vision();
    clear_avatar();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 12_hours );
    map &here = get_map();
    Character &you = get_player_character();
    scoped_weather_override weather( weather_type_id( "clear" ) );
    for( int x = 25; x < 90; ++x ) {
        for( int y = 45; y < 90; ++y ) {
            here.ter_set( tripoint_bub_ms( x, y, 0 ), ter_id( "t_grass" ) );
            here.ter_set( tripoint_bub_ms( x, y, 1 ), ter_id( "t_open_air" ) );
            here.furn_clear( tripoint_bub_ms( x, y, 0 ) );
        }
    }
    you.setpos( here, tripoint_bub_ms( 80, 66, 0 ) );
    refresh_writhing_stalker_arena( here );
    const bool retreat = GENERATE( false, true );
    monster &stalker = spawn_new_stalker_fixture( tripoint_bub_ms( retreat ? 77 : 30, 66, 0 ) );
    stalker.anger = 100;
    stalker.aggro_character = true;
    REQUIRE( writhing_stalker::observe_current_target( stalker ) == &you );
    const int observed = stalker.writhing_stalker_state().evidence_turn;
    const auto actor = stalker.predator_state().actor_id;
    const auto epoch = stalker.predator_state().handoff_epoch;
    if( retreat ) {
        // A consumed native attack budget selects a real escape destination.
        stalker.writhing_stalker_state().attempts_spent = 2;
        stalker.plan();
        REQUIRE( stalker.writhing_stalker_state().has_retreat_waypoint );
    }
    for( int y = 0; y < 132; ++y ) {
        here.ter_set( tripoint_bub_ms( retreat ? 79 : 65, y, 0 ), ter_id( "t_wall" ) );
    }
    set_time( calendar::turn + 1_turns );
    refresh_writhing_stalker_arena( here );
    REQUIRE_FALSE( writhing_stalker::can_observe_actor( stalker, you ) );
    stalker.plan();
    REQUIRE( stalker.has_dest() );
    const auto source = stalker.pos_abs();
    overmap_buffer.get( project_to<coords::om>( source ).xy() ).hordes.clear();
    g->despawn_monster( stalker );
    REQUIRE( overmap_buffer.entity_at( source ) != nullptr );
    set_time( calendar::turn + 1_turns );
    overmap_buffer.move_hordes();
    monster *local = nullptr;
    for( monster &candidate : g->all_monsters() ) {
        if( candidate.predator_state().actor_id == actor ) {
            local = &candidate;
        }
    }
    REQUIRE( local != nullptr );
    REQUIRE( local->predator_state().handoff_epoch == epoch + 2 );
    REQUIRE( local->wandf > 1 );
    const auto &transported = local->writhing_stalker_state();
    REQUIRE( local->wander_pos == ( retreat ? transported.retreat_waypoint :
                                   transported.committed_waypoint ) );
    const int sound = GENERATE( 0, 1, 2 );
    const bool reload = GENERATE( false, true );
    tripoint_abs_ms sound_position;
    if( sound ) {
        local->morale = 20;
        // A real sound at the same coordinate is independent evidence too.
        const auto sound_source = sound == 2 ? here.get_bub( local->wander_pos ) :
                                  local->pos_bub() + point::south * 4;
        local->hear_sound( sound_source, 2000, 4, true );
        sound_position = here.get_abs( sound_source );
        REQUIRE( local->wander_pos == sound_position );
        REQUIRE( local->provocative_sound );
    }
    if( reload ) {
        *local = round_trip_writhing_stalker( *local );
    }
    set_time( calendar::turn + 32_minutes );
    local->plan();
    // Delayed cooldown recovery can enter search here. Let that real search
    // finish without inventing an arrival or changing its elapsed budget.
    set_time( calendar::turn + 31_minutes );
    local->plan();
    REQUIRE( local->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    CHECK( local->writhing_stalker_state().evidence_turn == observed );
    if( sound ) {
        CHECK( local->wandf > 0 );
        CHECK( local->wander_pos == sound_position );
    } else {
        CHECK( local->wandf == 0 );
        CHECK( horde_entity( *local ).tracking_intensity == 0 );
    }
    const auto before = local->pos_abs();
    local->set_moves( 100 );
    local->move();
    if( sound ) {
        CHECK( rl_dist( local->pos_abs(), sound_position ) < rl_dist( before, sound_position ) );
    } else {
        CHECK( local->wandf == 0 );
    }
    clear_map_without_vision();
}

TEST_CASE( "stalker_idle_memory_offload_schedules_without_fabricated_movement_and_eventually_forgets",
           "[writhing_stalker][predator][pursuit][scheduler]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 100_turns );
    const auto source = get_map().get_abs( tripoint_bub_ms( -36, 65, 0 ) );
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    monster original = new_stalker_fixture_body();
    original.set_pos_abs_only( source );
    CHECK_FALSE( horde_entity( original ).is_active() );
    auto &memory = original.writhing_stalker_state();
    memory.advance_to( 100 );
    REQUIRE( memory.observe_target( 77, 1, source + point::east * 3, 100 ) );
    memory.finish_search();
    const auto due = memory.familiar[0].revisit_at_active;
    const auto actor = original.predator_state().actor_id;
    REQUIRE_FALSE( original.has_dest() );
    REQUIRE( original.wandf == 0 );
    REQUIRE( om.hordes.spawn_entity( source, original ) );
    auto *owner = overmap_buffer.entity_at( source );
    REQUIRE( owner != nullptr );
    CHECK( owner->is_active() );
    CHECK( owner->tracking_intensity == 0 );
    // Birth becomes native authority at its first actual scheduler opportunity,
    // not retroactively at the fixture's constructor timestamp.
    const auto birth_moves = owner->monster_data->get_moves();
    const auto carrier_moves = owner->moves;
    overmap_buffer.move_hordes();
    owner = overmap_buffer.entity_at( source );
    REQUIRE( owner != nullptr );
    CHECK( owner->monster_data->predator_state().activity_adopted_world == 100 );
    CHECK( owner->monster_data->writhing_stalker_state().active_turns == 0 );
    CHECK( owner->monster_data->get_moves() == birth_moves );
    CHECK( owner->moves == carrier_moves );
    CHECK( owner->monster_data->pos_abs() == source );
    set_time( calendar::turn + 1_turns );
    overmap_buffer.move_hordes();
    owner = overmap_buffer.entity_at( source );
    REQUIRE( owner != nullptr );
    CHECK( owner->monster_data->writhing_stalker_state().active_turns == 1 );
    CHECK( owner->tracking_intensity == 0 );
    CHECK( owner->monster_data->wandf == 0 );
    CHECK( owner->monster_data->predator_state().actor_id == actor );
    CHECK( owner->monster_data->writhing_stalker_state().familiar[0].revisit_at_active == due );
    // The lifecycle's finite familiar memory, not permanent has_memory,
    // controls scheduling. Forgotten places do not leave a forever-active actor.
    set_time( calendar::turn + 7_days );
    overmap_buffer.move_hordes();
    set_time( calendar::turn + 1_turns );
    overmap_buffer.move_hordes();
    owner = overmap_buffer.entity_at( source );
    REQUIRE( owner != nullptr );
    CHECK_FALSE( owner->is_active() );
    CHECK_FALSE( owner->monster_data->writhing_stalker_state().familiar[0].valid );
    CHECK( owner->tracking_intensity == 0 );
    CHECK( owner->monster_data->writhing_stalker_state().evidence_turn == 100 );
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_idle_abstract_sound_survives_pending_familiar_revisit",
           "[writhing_stalker][predator][pursuit][scheduler][signal]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    const auto source = get_map().get_abs( tripoint_bub_ms( -36, 65, 0 ) );
    const auto sound = source + point::east * 5;
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    for( int x = 0; x <= 5; ++x ) {
        overmap_buffer.set_passable( source + point::east * x, true );
    }
    monster original = new_stalker_fixture_body();
    original.set_pos_abs_only( source );
    original.set_moves( 100 );
    const bool learned = GENERATE( false, true );
    if( learned ) {
        auto &state = original.writhing_stalker_state();
        state.advance_to( 100 );
        REQUIRE( state.observe_target( 77, 1, sound, 100 ) );
        state.finish_search();
        state.advance_to( 1000 );
        REQUIRE( state.familiar[0].revisit_at_active > state.active_turns );
    }
    const auto actor = original.predator_state().actor_id;
    REQUIRE( om.hordes.spawn_entity( source, original ) );
    // The actual horde sound owner assigns this independent order. No
    // tracking field is patched after intent dispatch.
    om.hordes.signal_entities( sound, 20 );
    auto *owner = overmap_buffer.entity_at( source );
    REQUIRE( owner != nullptr );
    REQUIRE( owner->destination == sound );
    REQUIRE( owner->tracking_intensity > 0 );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        *owner->monster_data = round_trip_writhing_stalker( *owner->monster_data );
        om.hordes.refresh_entity_bucket( source );
    }
    overmap_buffer.move_hordes();
    owner = overmap_buffer.entity_at( source + point::east );
    CHECK( owner != nullptr );
    if( owner ) {
        CHECK( owner->destination == sound );
        CHECK( owner->tracking_intensity > 0 );
        CHECK( owner->monster_data->predator_state().actor_id == actor );
        if( learned ) {
            CHECK( owner->monster_data->writhing_stalker_state().evidence_turn == 100 );
            CHECK( owner->monster_data->writhing_stalker_state().phase ==
                   writhing_stalker::lifecycle_phase::idle );
        }
        const auto active = owner->monster_data->writhing_stalker_state().active_turns;
        overmap_buffer.move_hordes();
        CHECK( overmap_buffer.entity_at( source + point::east ) == owner );
        CHECK( owner->monster_data->writhing_stalker_state().active_turns == active );
    }
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_abstract_retreat_arrival_and_recovery_are_finite",
           "[writhing_stalker][predator][pursuit][scheduler][recovery]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    const auto source = get_map().get_abs( tripoint_bub_ms( -36, 65, 0 ) );
    const auto retreat = source + point::east;
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    for( int x = -40; x <= 40; ++x ) {
        for( int y = -40; y <= 40; ++y ) {
            overmap_buffer.set_passable( source + point( x, y ), true );
        }
    }
    monster original = new_stalker_fixture_body();
    original.set_pos_abs_only( source );
    original.set_moves( 100 );
    auto &state = original.writhing_stalker_state();
    state.advance_to( 100 );
    REQUIRE( state.observe_target( 77, 1, source + point::west * 3, 100 ) );
    state.advance_to( 1000 );
    // Native withdraw/cooling-off payloads are already covered by the local
    // caller controls. This fixture follows their real abstract arrival.
    const bool cooling = GENERATE( false, true );
    state.phase = cooling ? writhing_stalker::lifecycle_phase::cooldown :
                  writhing_stalker::lifecycle_phase::retreating;
    state.retreat_waypoint = retreat;
    state.has_retreat_waypoint = true;
    state.committed_waypoint = source + point::west * 5;
    state.has_committed_waypoint = true;
    state.cooldown_until_turn = 1060;
    state.attempts_spent = 2;
    original.set_dest( retreat );
    const auto actor = original.predator_state().actor_id;
    REQUIRE( om.hordes.spawn_entity( source, original ) );
    overmap_buffer.move_hordes();
    auto *owner = overmap_buffer.entity_at( retreat );
    REQUIRE( owner != nullptr );
    REQUIRE( owner->tracking_intensity == 0 );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        *owner->monster_data = round_trip_writhing_stalker( *owner->monster_data );
        om.hordes.refresh_entity_bucket( retreat );
    }
    set_time( calendar::turn + 1_turns );
    overmap_buffer.move_hordes();
    owner = overmap_buffer.entity_at( retreat );
    REQUIRE( owner != nullptr );
    CHECK( owner->monster_data->writhing_stalker_state().phase ==
           writhing_stalker::lifecycle_phase::cooldown );
    CHECK( owner->tracking_intensity == 0 );
    CHECK( owner->monster_data->writhing_stalker_state().cooldown_until_turn == 1060 );
    const bool delayed = GENERATE( false, true );
    set_time( calendar::turn_zero + ( delayed ? 3000_turns : 1061_turns ) );
    overmap_buffer.move_hordes();
    owner = nullptr;
    for( const auto &entry : om.horde_entities() ) {
        if( entry.second->monster_data &&
            entry.second->monster_data->predator_state().actor_id == actor ) {
            owner = entry.second;
        }
    }
    REQUIRE( owner != nullptr );
    auto &recovered = owner->monster_data->writhing_stalker_state();
    CHECK( recovered.phase != writhing_stalker::lifecycle_phase::retreating );
    CHECK( recovered.phase != writhing_stalker::lifecycle_phase::cooldown );
    CHECK( recovered.cooldown_until_turn == -1 );
    if( delayed ) {
        CHECK( recovered.phase == writhing_stalker::lifecycle_phase::idle );
    }
    const auto active = recovered.active_turns;
    const auto search_start = recovered.search_started_active;
    overmap_buffer.move_hordes();
    CHECK( owner->monster_data->writhing_stalker_state().active_turns == active );
    CHECK( owner->monster_data->writhing_stalker_state().search_started_active == search_start );
    set_time( calendar::turn + 31_minutes );
    overmap_buffer.move_hordes();
    owner = nullptr;
    for( const auto &entry : om.horde_entities() ) {
        if( entry.second->monster_data &&
            entry.second->monster_data->predator_state().actor_id == actor ) {
            owner = entry.second;
        }
    }
    REQUIRE( owner != nullptr );
    CHECK( owner->monster_data->writhing_stalker_state().phase ==
           writhing_stalker::lifecycle_phase::idle );
    CHECK( owner->tracking_intensity == 0 );
    CHECK( owner->monster_data->writhing_stalker_state().evidence_turn == 100 );
    CHECK( owner->monster_data->writhing_stalker_state().familiar[0].revisit_at_active >
           owner->monster_data->writhing_stalker_state().active_turns );
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_abstract_light_arrival_expires_without_replacing_new_sound",
           "[writhing_stalker][predator][pursuit][scheduler][signal][light_transition]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    const auto source = get_map().get_abs( tripoint_bub_ms( -36, 65, 0 ) );
    const auto light = source + point::east;
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    for( int x = -5; x <= 5; ++x ) {
        for( int y = -5; y <= 5; ++y ) {
            overmap_buffer.set_passable( source + point( x, y ), true );
        }
    }
    monster original = new_stalker_fixture_body();
    original.set_pos_abs_only( source );
    original.set_moves( 100 );
    auto &state = original.writhing_stalker_state();
    state.advance_to( 100 );
    const bool learned = GENERATE( false, true );
    if( learned ) {
        REQUIRE( state.observe_target( 77, 1, source + point::west * 3, 100 ) );
        state.finish_search();
    }
    state.advance_to( 1000 );
    const auto actor = original.predator_state().actor_id;
    REQUIRE( om.hordes.spawn_entity( source, original ) );
    const bool blocked = GENERATE( false, true );
    if( blocked ) {
        for( const auto &candidate : squares_closer_to( source, light ) ) {
            overmap_buffer.set_passable( candidate, false );
        }
    }
    // A physical-light adapter packet fixture feeds the production owner;
    // this does not claim a native fire or newly observed target.
    REQUIRE( om.hordes.attract_entities_to_light( light, 6, 20, "abstract-arrival-light",
             calendar::turn, []( const tripoint_abs_ms & ) { return true; }, nullptr ) == 1 );
    overmap_buffer.move_hordes();
    const auto arrival = blocked ? source : light;
    auto *owner = overmap_buffer.entity_at( arrival );
    REQUIRE( owner != nullptr );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        *owner->monster_data = round_trip_writhing_stalker( *owner->monster_data );
        om.hordes.refresh_entity_bucket( arrival );
    }
    set_time( calendar::turn + 1_turns );
    overmap_buffer.move_hordes();
    owner = overmap_buffer.entity_at( arrival );
    REQUIRE( owner != nullptr );
    const auto &arrived = owner->monster_data->writhing_stalker_state();
    if( !blocked ) {
        CHECK( arrived.phase == writhing_stalker::lifecycle_phase::searching );
        CHECK( arrived.search_until_turn == 1021 );
        CHECK( owner->tracking_intensity == 0 );
    }
    set_time( calendar::turn_zero + 1022_turns );
    overmap_buffer.move_hordes();
    owner = overmap_buffer.entity_at( arrival );
    REQUIRE( owner != nullptr );
    CHECK( owner->monster_data->writhing_stalker_state().phase ==
           writhing_stalker::lifecycle_phase::idle );
    CHECK_FALSE( owner->monster_data->writhing_stalker_state().has_committed_waypoint );
    CHECK( owner->tracking_intensity == 0 );
    CHECK( owner->is_active() == learned );
    const int finished_turn = owner->monster_data->writhing_stalker_state().last_advanced_turn;
    set_time( calendar::turn_zero + 1200_turns );
    overmap_buffer.move_hordes();
    owner = overmap_buffer.entity_at( arrival );
    REQUIRE( owner != nullptr );
    if( !learned ) {
        // The actual scheduler must stop admitting this finite legacy job.
        CHECK( owner->monster_data->writhing_stalker_state().last_advanced_turn == finished_turn );
        CHECK_FALSE( owner->is_active() );
    }
    for( int x = -5; x <= 5; ++x ) {
        for( int y = -5; y <= 5; ++y ) {
            overmap_buffer.set_passable( source + point( x, y ), true );
        }
    }
    const auto sound = arrival + point::south * 3;
    om.hordes.signal_entities( sound, 20 );
    set_time( calendar::turn + 1_turns );
    overmap_buffer.move_hordes();
    owner = overmap_buffer.entity_at( arrival + point::south );
    CHECK( owner != nullptr );
    if( owner ) {
        CHECK( owner->destination == sound );
        CHECK( owner->monster_data->predator_state().actor_id == actor );
        CHECK( owner->monster_data->writhing_stalker_state().has_pursuit_memory == learned );
        if( learned ) {
            CHECK( owner->monster_data->writhing_stalker_state().evidence_turn == 100 );
        }
    }
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_recovery_budget_survives_visible_reacquisition_before_local_combat",
           "[writhing_stalker][predator][pursuit][recovery]" )
{
    clear_map_without_vision();
    clear_avatar();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero );
    set_time( calendar::turn - time_past_midnight( calendar::turn ) + 1_days );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &initial_stalker = spawn_new_stalker_fixture( center + point::east * 2 );
    initial_stalker.anger = 100;
    initial_stalker.aggro_character = true;
    initial_stalker.writhing_stalker_state().attempts_spent = 2;
    initial_stalker.plan();
    REQUIRE( initial_stalker.writhing_stalker_state().phase ==
             writhing_stalker::lifecycle_phase::retreating );
    initial_stalker.plan();
    REQUIRE( initial_stalker.writhing_stalker_state().phase ==
             writhing_stalker::lifecycle_phase::cooldown );
    const int recovery = initial_stalker.writhing_stalker_state().cooldown_until_turn;
    // Native withdrawal starts a short run effect. Let ordinary body turns
    // expire it before testing recognition after the longer recovery.
    for( int turn = 0; turn < 8; ++turn ) {
        set_time( calendar::turn + 1_turns );
        initial_stalker.process_turn();
    }
    const auto actor = initial_stalker.predator_state().actor_id;
    const auto position = initial_stalker.pos_abs();
    const auto epoch = initial_stalker.predator_state().handoff_epoch;
    on_out_of_scope release_native_fixture( [&] {
        if( auto *owner = overmap_buffer.find_loaded_overmap( project_to<coords::om>( position ).xy() ) ) {
            owner->release_monster_payload( position, actor );
        }
    } );
    const bool abstract_owner = GENERATE( false, true );
    monster *resumed = &initial_stalker;
    horde_entity *native = nullptr;
    if( abstract_owner ) {
        // A standalone local copy is not an accepted abstract owner.
        horde_entity refused( initial_stalker );
        std::ostringstream before;
        { JsonOut json( before ); refused.monster_data->serialize( json ); }
        CHECK_FALSE( overmap_buffer.predator_activity_for( *refused.monster_data,
                     predator_activation::representation::abstract, position ) );
        std::ostringstream after;
        { JsonOut json( after ); refused.monster_data->serialize( json ); }
        CHECK( after.str() == before.str() );
        REQUIRE( g->despawn_monster( initial_stalker ) );
        native = overmap_buffer.entity_at( position );
        REQUIRE( native != nullptr );
        REQUIRE( native->monster_data );
        CHECK( native->monster_data->predator_state().actor_id == actor );
        CHECK( native->monster_data->predator_state().handoff_epoch == epoch + 1 );
    }
    set_time( calendar::turn_zero + time_duration::from_turns( recovery + 1 ) );
    if( abstract_owner ) {
        // Run the actual accepted owner after its handoff world turn. The
        // same native clock refresh as the scheduler supplies earned work;
        // an arbitrary explicit elapsed argument cannot replace this owner.
        int elapsed = 0;
        REQUIRE( overmap_buffer.predator_activity_for( *native->monster_data,
                 predator_activation::representation::abstract, position, &elapsed ) );
        REQUIRE( native->advance_predator_intent( position, recovery + 1, elapsed ) );
        overmap_buffer.spawn_monster( project_to<coords::sm>( position ), false );
        CHECK( overmap_buffer.entity_at( position ) == nullptr );
        resumed = get_creature_tracker().creature_at<monster>( here.get_bub( position ) );
        REQUIRE( resumed != nullptr );
        CHECK( resumed->predator_state().handoff_epoch == epoch + 2 );
    } else {
        // observe_current_target is entered by the native planner after its
        // eligibility query; supply that same actual local owner boundary.
        REQUIRE( overmap_buffer.predator_activity_for( initial_stalker,
                 predator_activation::representation::local, position ) );
        REQUIRE( writhing_stalker::observe_current_target( initial_stalker ) == &you );
    }
    monster &stalker = *resumed;
    INFO( "reacquire anger=" << stalker.anger << " morale=" << stalker.morale <<
          " attitude=" << static_cast<int>( stalker.attitude_to( you ) ) <<
          " run=" << stalker.has_effect( efftype_id( "run" ) ) <<
          " fleeing=" << stalker.is_fleeing( you ) <<
          " physical=" << writhing_stalker::can_observe_actor( stalker, you ) );
    REQUIRE( stalker.writhing_stalker_state().evidence_turn == recovery + 1 );
    REQUIRE( stalker.writhing_stalker_state().search_started_active == -1 );
    REQUIRE( stalker.writhing_stalker_state().search_until_turn == -1 );
    CHECK( stalker.writhing_stalker_state().attempts_spent == 0 );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    set_time( calendar::turn_zero + time_duration::from_turns( recovery + 21 ) );
    // Supply the existing actual same-target pressure/witnessed-attack
    // opening. A healthy avatar alone may truthfully receive only shadow.
    std::vector<monster *> zombies;
    for( const point &offset : { point::west, point::south, point::south_east, point::south_west } ) {
        monster &zombie = spawn_test_monster( mon_zombie.str(), center + offset );
        zombie.aggro_character = true;
        zombie.set_dest( you.pos_abs() );
        REQUIRE( zombie.attack_target() == &you );
        zombies.push_back( &zombie );
    }
    // Keep the actual approach tile free: a friendly zombie there consumes
    // native blocked movement and is not a post-recovery combat opening.
    REQUIRE( get_creature_tracker().creature_at( center + point::east ) == nullptr );
    REQUIRE( here.passable( center + point::east ) );
    refresh_writhing_stalker_arena( here );
    REQUIRE( writhing_stalker::observed_zombie_pressure( stalker, here, you ) == 4 );
    CHECK( you.deal_melee_attack( zombies.front(), -1000 ) < 0 );
    REQUIRE( writhing_stalker::has_recent_observed_attack( &stalker, zombies.front(), &you,
             to_turn<int>( calendar::turn ), 0 ) );
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().attempts_spent == 0 );
    CHECK( stalker.writhing_stalker_state().search_until_turn == -1 );
    CHECK( stalker.writhing_stalker_state().phase ==
           writhing_stalker::lifecycle_phase::attacking );
    CHECK( stalker.has_dest() );
    CHECK( stalker.get_dest() == you.pos_abs() );
    CHECK( stalker.predator_state().actor_id == actor );
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().attempts_spent == 0 );
    // Execute real native combat after recovery, then prove the already
    // consumed deadline cannot erase the new burst's actual attempts.
    const auto sequence = stalker.writhing_stalker_state().attempt_sequence;
    for( int step = 0; step < 6 && stalker.writhing_stalker_state().attempts_spent == 0; ++step ) {
        set_time( calendar::turn + 1_turns );
        stalker.plan();
        stalker.set_moves( 100 );
        stalker.move();
    }
    const auto spent = stalker.writhing_stalker_state().attempts_spent;
    REQUIRE( spent > 0 );
    CHECK( stalker.writhing_stalker_state().attempt_sequence == sequence + spent );
    const bool reload_after_attack = GENERATE( false, true );
    if( reload_after_attack ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    set_time( calendar::turn + 1_turns );
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().attempts_spent == spent );
    CHECK( stalker.writhing_stalker_state().search_until_turn == -1 );
    clear_map_without_vision();
}

TEST_CASE( "stalker_local_unlearned_light_expiry_retires_native_route_preserving_sound",
           "[writhing_stalker][predator][pursuit][light_transition][fallback]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    // Arena setup resets the avatar; keep this light-only control physically
    // separated from character recognition after that setup.
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center );
    stalker.anger = 100;
    stalker.morale = 20;
    const auto light = stalker.pos_abs() + point::east * 6;
    auto &state = stalker.writhing_stalker_state();
    state.advance_to( 1000 );
    // Accepted physical-light adapter fixture; actual local planning and
    // native movement below judge executable route ownership, not a fire.
    REQUIRE( state.observe_light_interest( light, "local-unlearned-light", 1000, 5 ) );
    stalker.plan();
    REQUIRE_FALSE( state.has_pursuit_memory );
    REQUIRE( stalker.has_dest() );
    REQUIRE( stalker.get_dest() == light );
    const int sound = GENERATE( 0, 1, 2 );
    tripoint_abs_ms heard;
    if( sound ) {
        heard = sound == 2 ? light : stalker.pos_abs() + point::south * 6;
        stalker.hear_sound( here.get_bub( heard ), 2000, 6, true );
        REQUIRE( stalker.provocative_sound );
        REQUIRE( stalker.wander_pos == heard );
    }
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    set_time( calendar::turn_zero + 1006_turns );
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    CHECK_FALSE( stalker.writhing_stalker_state().has_committed_waypoint );
    CHECK_FALSE( ( stalker.has_dest() && stalker.get_dest() == light ) );
    const auto before = stalker.pos_abs();
    stalker.set_moves( 100 );
    stalker.move();
    if( sound ) {
        CHECK( stalker.provocative_sound );
        CHECK( stalker.wander_pos == heard );
        CHECK( rl_dist( stalker.pos_abs(), heard ) < rl_dist( before, heard ) );
    } else {
        CHECK( stalker.pos_abs() == before );
        CHECK( stalker.wandf == 0 );
    }
    CHECK_FALSE( stalker.writhing_stalker_state().has_pursuit_memory );
    clear_map_without_vision();
}

TEST_CASE( "stalker_legacy_light_search_acquisition_cannot_reset_a_new_native_burst",
           "[writhing_stalker][predator][pursuit][light_transition][recovery]" )
{
    clear_map_without_vision();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    Character &you = get_player_character();
    you.setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center );
    stalker.anger = 100;
    stalker.aggro_character = true;
    auto &state = stalker.writhing_stalker_state();
    state.advance_to( 1000 );
    // Actual local arrival starts a legacy light search without prey memory.
    REQUIRE( state.observe_light_interest( stalker.pos_abs(), "light-before-first-prey", 1000, 90 ) );
    stalker.plan();
    REQUIRE_FALSE( state.has_pursuit_memory );
    REQUIRE( state.phase == writhing_stalker::lifecycle_phase::searching );
    const int old_light_deadline = state.search_until_turn;
    REQUIRE( old_light_deadline == 1020 );
    you.setpos( here, center + point::west * 2 );
    refresh_writhing_stalker_arena( here );
    set_time( calendar::turn + 1_turns );
    REQUIRE( writhing_stalker::observe_current_target( stalker ) == &you );
    CHECK( state.search_until_turn == -1 );
    std::vector<monster *> zombies;
    for( const point &offset : { point::west, point::south, point::east, point::south_west } ) {
        monster &zombie = spawn_test_monster( mon_zombie.str(), you.pos_bub() + offset );
        zombie.aggro_character = true;
        zombie.set_dest( you.pos_abs() );
        REQUIRE( zombie.attack_target() == &you );
        zombies.push_back( &zombie );
    }
    refresh_writhing_stalker_arena( here );
    REQUIRE( writhing_stalker::observed_zombie_pressure( stalker, here, you ) == 4 );
    CHECK( you.deal_melee_attack( zombies.front(), -1000 ) < 0 );
    REQUIRE( writhing_stalker::has_recent_observed_attack( &stalker, zombies.front(), &you,
             to_turn<int>( calendar::turn ), 0 ) );
    stalker.plan();
    REQUIRE( state.phase == writhing_stalker::lifecycle_phase::attacking );
    for( int step = 0; step < 6 && state.attempts_spent == 0; ++step ) {
        set_time( calendar::turn + 1_turns );
        stalker.plan();
        stalker.set_moves( 100 );
        stalker.move();
    }
    const auto spent = state.attempts_spent;
    const auto sequence = state.attempt_sequence;
    REQUIRE( spent > 0 );
    REQUIRE( state.cooldown_until_turn == -1 );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    set_time( calendar::turn_zero + time_duration::from_turns( old_light_deadline + 1 ) );
    stalker.plan();
    CHECK( state.attempts_spent == spent );
    CHECK( state.attempt_sequence == sequence );
    CHECK( state.search_until_turn == -1 );
    clear_map_without_vision();
}

TEST_CASE( "stalker_old_recovery_cannot_erase_an_early_native_burst",
           "[writhing_stalker][predator][pursuit][recovery]" )
{
    clear_map_without_vision();
    clear_avatar();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero );
    const int old_spent = GENERATE( 1, 2 );
    if( old_spent == 1 ) {
        set_time_to_day();
    } else {
        set_time( calendar::turn - time_past_midnight( calendar::turn ) + 1_days );
    }
    scoped_weather_override weather( weather_type_id( "clear" ) );
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center + point::north_east );
    stalker.anger = 100;
    stalker.aggro_character = true;
    stalker.writhing_stalker_state().attempts_spent = old_spent;
    stalker.plan();
    REQUIRE( ( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::retreating ||
               stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::cooldown ) );
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::cooldown );
    const int recovered_at = stalker.writhing_stalker_state().cooldown_until_turn;
    for( int turn = 0; turn < 8; ++turn ) {
        set_time( calendar::turn + 1_turns );
        stalker.process_turn();
    }
    set_time( calendar::turn_zero + time_duration::from_turns( recovered_at + 1 ) );
    // The opening changes after the original, smaller burst was spent.
    // Actual low stamina/bleeding adds vulnerability to the new opening.
    you.set_stamina( 0 );
    you.add_effect( efftype_id( "bleed" ), 1_hours, bodypart_id( "arm_l" ) );
    refresh_writhing_stalker_arena( here );
    const bool abstract_owner = GENERATE( false, true );
    if( abstract_owner ) {
        horde_entity owner( stalker );
        REQUIRE( owner.advance_predator_intent( stalker.pos_abs(), recovered_at + 1 ) );
        stalker = *owner.monster_data;
    } else {
        REQUIRE( writhing_stalker::observe_current_target( stalker ) == &you );
    }
    std::vector<monster *> zombies;
    for( const point &offset : { point::west, point::south, point::east, point::south_west } ) {
        monster &zombie = spawn_test_monster( mon_zombie.str(), center + offset );
        zombie.aggro_character = true;
        zombie.set_dest( you.pos_abs() );
        REQUIRE( zombie.attack_target() == &you );
        zombies.push_back( &zombie );
    }
    refresh_writhing_stalker_arena( here );
    REQUIRE( writhing_stalker::observed_zombie_pressure( stalker, here, you ) == 4 );
    CHECK( you.deal_melee_attack( zombies.front(), -1000 ) < 0 );
    REQUIRE( writhing_stalker::has_recent_observed_attack( &stalker, zombies.front(), &you,
             to_turn<int>( calendar::turn ), 0 ) );
    const auto sequence = stalker.writhing_stalker_state().attempt_sequence;
    for( int step = 0; step < 6 && stalker.writhing_stalker_state().attempt_sequence == sequence; ++step ) {
        // A real adjacent attack uses native combat, independent of path occupancy.
        stalker.plan();
        INFO( "old_spent=" << old_spent << " phase=" << static_cast<int>( stalker.writhing_stalker_state().phase ) );
        CHECK( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::attacking );
        stalker.set_moves( 100 );
        stalker.attack_at( you.pos_bub() );
        set_time( calendar::turn + 1_turns );
    }
    const auto fresh_attempts = stalker.writhing_stalker_state().attempt_sequence - sequence;
    REQUIRE( fresh_attempts > 0 );
    REQUIRE( to_turn<int>( calendar::turn ) < recovered_at + 20 );
    CHECK( static_cast<std::uint64_t>( stalker.writhing_stalker_state().attempts_spent ) == fresh_attempts );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    set_time( calendar::turn_zero + time_duration::from_turns( recovered_at + 21 ) );
    stalker.plan();
    CHECK( static_cast<std::uint64_t>( stalker.writhing_stalker_state().attempts_spent ) == fresh_attempts );
    CHECK( stalker.writhing_stalker_state().attempt_sequence == sequence + fresh_attempts );
    clear_map_without_vision();
}

TEST_CASE( "stalker_hidden_recovery_cannot_own_a_later_withdrawal_toward_moved_prey",
           "[writhing_stalker][predator][pursuit][recovery][flight]" )
{
    clear_map_without_vision();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero );
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center + point::east * 2 );
    stalker.anger = 100;
    stalker.aggro_character = true;
    stalker.writhing_stalker_state().attempts_spent = 2;
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().has_retreat_waypoint );
    const auto old_retreat = stalker.writhing_stalker_state().retreat_waypoint;
    you.setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    refresh_writhing_stalker_arena( here );
    for( int step = 0; step < 30 && stalker.pos_abs() != old_retreat; ++step ) {
        set_time( calendar::turn + 1_turns );
        stalker.process_turn();
        stalker.plan();
        stalker.set_moves( 100 );
        stalker.move();
    }
    REQUIRE( stalker.pos_abs() == old_retreat );
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::cooldown );
    const int recovered_at = stalker.writhing_stalker_state().cooldown_until_turn;
    for( int turn = 0; turn < 8; ++turn ) {
        set_time( calendar::turn + 1_turns );
        stalker.process_turn();
    }
    set_time( calendar::turn_zero + time_duration::from_turns( recovered_at + 1 ) );
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().search_started_active >= 0 );
    CHECK_FALSE( stalker.writhing_stalker_state().has_retreat_waypoint );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    you.setpos( here, here.get_bub( old_retreat ) + point::west );
    refresh_writhing_stalker_arena( here );
    set_time( calendar::turn + 1_turns );
    REQUIRE( writhing_stalker::observe_current_target( stalker ) == &you );
    // Genuine native damage supplies a later withdrawal reason, independent
    // of either the old spent budget or the completed retreat episode.
    stalker.apply_damage( &you, bodypart_id( "torso" ), stalker.get_hp() / 2 );
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::retreating );
    REQUIRE( stalker.has_dest() );
    CHECK( stalker.get_dest() != old_retreat );
    CHECK( rl_dist( stalker.get_dest(), you.pos_abs() ) >= 8 );
    const int separation = rl_dist( stalker.pos_abs(), you.pos_abs() );
    const auto sequence = stalker.writhing_stalker_state().attempt_sequence;
    // Native stumbling/movement cost need not produce a tile in one call.
    for( int step = 0; step < 8 && rl_dist( stalker.pos_abs(), you.pos_abs() ) <= separation; ++step ) {
        set_time( calendar::turn + 1_turns );
        stalker.process_turn();
        stalker.plan();
        stalker.set_moves( 100 );
        stalker.move();
    }
    CHECK( rl_dist( stalker.pos_abs(), you.pos_abs() ) > separation );
    CHECK( stalker.writhing_stalker_state().attempt_sequence == sequence );
    clear_map_without_vision();
}

TEST_CASE( "stalker_native_combat_consumes_old_recovery_without_erasing_sound_or_attempts",
           "[writhing_stalker][predator][pursuit][recovery][fallback]" )
{
    clear_map_without_vision();
    clear_avatar();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero );
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center + point::north_east );
    stalker.anger = 100;
    stalker.aggro_character = true;
    stalker.writhing_stalker_state().attempts_spent = 2;
    stalker.plan();
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::cooldown );
    const int old_deadline = stalker.writhing_stalker_state().cooldown_until_turn;
    for( int turn = 0; turn < 8; ++turn ) {
        set_time( calendar::turn + 1_turns );
        stalker.process_turn();
    }
    set_time( calendar::turn_zero + time_duration::from_turns( old_deadline - 1 ) );
    const auto old_retreat = stalker.writhing_stalker_state().retreat_waypoint;
    horde_entity owner( stalker );
    owner.synchronize_payload( stalker.pos_abs() );
    stalker = *owner.monster_data;
    REQUIRE( stalker.wander_pos == old_retreat );
    REQUIRE( stalker.wandf > 0 );
    const int sound = GENERATE( 0, 1, 2 );
    const auto heard = sound == 2 ? stalker.pos_abs() : old_retreat;
    if( sound ) {
        stalker.morale = 20;
        stalker.hear_sound( here.get_bub( heard ), 2000, sound == 2 ? 0 : 6, true );
        REQUIRE( stalker.provocative_sound );
        REQUIRE( stalker.wander_pos == heard );
    }
    const auto sequence = stalker.writhing_stalker_state().attempt_sequence;
    stalker.attack_at( you.pos_bub() );
    REQUIRE( stalker.writhing_stalker_state().attempt_sequence > sequence );
    const auto spent = stalker.writhing_stalker_state().attempts_spent;
    CHECK( static_cast<std::uint64_t>( spent ) == 2 + stalker.writhing_stalker_state().attempt_sequence - sequence );
    CHECK( stalker.writhing_stalker_state().cooldown_until_turn == -1 );
    CHECK_FALSE( stalker.writhing_stalker_state().has_retreat_waypoint );
    CHECK_FALSE( stalker.has_dest() );
    CHECK( ( stalker.wandf > 0 ) == ( sound != 0 ) );
    if( sound ) {
        CHECK( stalker.wander_pos == heard );
    }
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    set_time( calendar::turn_zero + time_duration::from_turns( old_deadline + 21 ) );
    REQUIRE( writhing_stalker::observe_current_target( stalker ) == &you );
    CHECK( stalker.writhing_stalker_state().attempts_spent == spent );
    // Still-spent combat now chooses a fresh finite native recovery, rather
    // than refunding attempts or reviving the consumed old deadline.
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::retreating );
    CHECK( stalker.writhing_stalker_state().cooldown_until_turn > to_turn<int>( calendar::turn ) );
    CHECK( stalker.writhing_stalker_state().attempts_spent == spent );
    CHECK( stalker.provocative_sound == ( sound == 1 ) );
    clear_map_without_vision();
}

TEST_CASE( "stalker_bounded_search_completion_cannot_refund_native_combat",
           "[writhing_stalker][predator][pursuit][recovery]" )
{
    clear_map_without_vision();
    clear_avatar();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero );
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center + point::north_east );
    stalker.anger = 100;
    stalker.aggro_character = true;
    // Establish the genuine loaded birth before any observed search/attack
    // work. The later31minutes are earned active time, not pre-auth absence.
    REQUIRE( overmap_buffer.predator_activity_for( stalker,
             predator_activation::representation::local, stalker.pos_abs() ) );
    CHECK( overmap_buffer.predator_active_time( stalker ) == 0 );
    REQUIRE( writhing_stalker::observe_current_target( stalker ) == &you );
    stalker.writhing_stalker_state().begin_search();
    const auto search_start = stalker.writhing_stalker_state().search_started_active;
    stalker.attack_at( you.pos_bub() );
    const auto sequence = stalker.writhing_stalker_state().attempt_sequence;
    const auto spent = stalker.writhing_stalker_state().attempts_spent;
    REQUIRE( sequence > 0 );
    CHECK( stalker.writhing_stalker_state().search_started_active == search_start );
    you.setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    refresh_writhing_stalker_arena( here );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    set_time( calendar::turn + 31_minutes );
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    CHECK( stalker.writhing_stalker_state().attempts_spent == spent );
    CHECK( stalker.writhing_stalker_state().attempt_sequence == sequence );
    CHECK_FALSE( stalker.has_dest() );
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().attempts_spent == spent );
    clear_map_without_vision();
}

TEST_CASE( "stalker_distant_light_uses_reachable_native_steps_before_finite_arrival_search",
           "[writhing_stalker][predator][pursuit][light_transition][routing]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    const bool slow = GENERATE( false, true );
    here.ter_set( center + point::east * 4, ter_id( slow ? "t_dirtmound" : "t_wall" ) );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center );
    stalker.anger = 100;
    const auto light = here.get_abs( center + point::east * 24 );
    REQUIRE( stalker.get_pathfinding_settings().max_dist == 10 );
    INFO( "native max_length=" << stalker.get_pathfinding_settings().max_length << " slow=" << slow );
    REQUIRE( stalker.can_move_to( here.get_bub( light ) ) );
    // The full route refuses solely because regular pathfinding is bounded;
    // a real short route can still detour around this wall/slow tile.
    REQUIRE( here.route( stalker, pathfinding_target::point( here.get_bub( light ) ) ).empty() );
    REQUIRE_FALSE( here.route( stalker,
                              pathfinding_target::point( center + point( 8, 4 ) ) ).empty() );
    auto &state = stalker.writhing_stalker_state();
    const bool learned = GENERATE( false, true );
    if( learned ) {
        state.advance_to( 100 );
        REQUIRE( state.observe_target( 77, 1, light + point::north * 24, 100 ) );
        state.finish_search();
    }
    state.advance_to( 1000 );
    // Accepted physical-light adapter input, not native fire/sight proof.
    REQUIRE( state.observe_light_interest( light, "distant-native-light", 1000, 300 ) );
    stalker.plan();
    REQUIRE( stalker.has_dest() );
    CHECK( state.phase != writhing_stalker::lifecycle_phase::searching );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    for( int step = 0; step < 100 && stalker.pos_abs() != light; ++step ) {
        stalker.plan();
        REQUIRE( stalker.writhing_stalker_state().phase != writhing_stalker::lifecycle_phase::searching );
        stalker.set_moves( 100 );
        stalker.move();
        set_time( calendar::turn + 1_turns );
    }
    REQUIRE( stalker.pos_abs() == light );
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::searching );
    const int deadline = stalker.writhing_stalker_state().search_until_turn;
    REQUIRE( deadline == to_turn<int>( calendar::turn ) + 20 );
    stalker = round_trip_writhing_stalker( stalker );
    set_time( calendar::turn_zero + time_duration::from_turns( deadline ) );
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    CHECK_FALSE( stalker.has_dest() );
    CHECK( stalker.writhing_stalker_state().evidence_turn == ( learned ? 100 : -1 ) );
    clear_map_without_vision();
}

TEST_CASE( "stalker_replans_a_self_waypoint_toward_a_reachable_lamp",
           "[writhing_stalker][predator][light_transition][routing][native_regression]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms start( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, start );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    const auto lamp = start + point::east * 4;
    here.furn_set( lamp, furn_str_id( "f_exodii_lamp" ) );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( start );
    stalker.anger = 100;
    REQUIRE( stalker.can_move_to( lamp ) );
    INFO( "target cost=" << here.move_cost( lamp ) << " maxdist=" <<
          stalker.get_pathfinding_settings().max_dist << " maxlength=" <<
          stalker.get_pathfinding_settings().max_length );
    const auto lamp_route = here.route( stalker, pathfinding_target::point( lamp ) );
    here.furn_clear( lamp );
    refresh_writhing_stalker_arena( here );
    const auto clear_route = here.route( stalker, pathfinding_target::point( lamp ) );
    INFO( "lamp route=" << lamp_route.size() << " clear route=" << clear_route.size() );
    REQUIRE_FALSE( clear_route.empty() );
    here.furn_set( lamp, furn_str_id( "f_exodii_lamp" ) );
    refresh_writhing_stalker_arena( here );
    REQUIRE_FALSE( here.route( stalker, pathfinding_target::point( lamp ) ).empty() );
    auto &state = stalker.writhing_stalker_state();
    state.advance_to( 1000 );
    REQUIRE( state.observe_light_interest( here.get_abs( lamp ), "lamp-control", 1000, 186 ) );
    // Reproduce the retained native state without claiming this controlled map
    // reconstructs its dynamic occupancy or the path that led to the state.
    state.committed_waypoint = stalker.pos_abs();
    state.has_committed_waypoint = true;
    state.remaining_route_progress = 4;
    state.last_progress_turn = 997;
    stalker.plan();
    REQUIRE( stalker.has_dest() );
    CHECK( stalker.get_dest() != stalker.pos_abs() );
    CHECK( rl_dist( stalker.get_dest(), here.get_abs( lamp ) ) < 4 );
    clear_map_without_vision();
}

TEST_CASE( "stalker_due_or_started_familiar_visit_preserves_accepted_sound_then_finishes",
           "[writhing_stalker][predator][pursuit][scheduler][signal]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    const auto source = get_map().get_abs( tripoint_bub_ms( -36, 65, 0 ) );
    const auto sound = source + point::east * 5;
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    for( int x = -100; x <= 10; ++x ) {
        for( int y = -24; y <= 24; ++y ) {
            overmap_buffer.set_passable( source + point( x, y ), true );
        }
    }
    monster original = new_stalker_fixture_body();
    original.set_pos_abs_only( source );
    original.set_moves( 100 );
    auto &memory = original.writhing_stalker_state();
    memory.advance_to( 100 );
    REQUIRE( memory.observe_target( 77, 1, source + point::west * 48, 100 ) );
    memory.finish_search();
    const auto due = memory.familiar[0].revisit_at_active;
    memory.advance_to( 1000 );
    const auto actor = original.predator_state().actor_id;
    REQUIRE( om.hordes.spawn_entity( source, original ) );
    // Establish the actual native birth interval before waiting for a due
    // active-time visit. Unauthenticated absence cannot fund that visit.
    const auto birth_active = memory.active_turns;
    const auto birth_moves = original.get_moves();
    overmap_buffer.move_hordes();
    auto *born = overmap_buffer.entity_at( source );
    REQUIRE( born != nullptr );
    REQUIRE( born->monster_data );
    CHECK( born->monster_data->predator_state().activity_adopted_world == 1000 );
    CHECK( born->monster_data->writhing_stalker_state().active_turns == birth_active );
    CHECK( born->monster_data->get_moves() == birth_moves );
    CHECK( born->monster_data->pos_abs() == source );
    const auto find_owner = [&]() -> std::pair<tripoint_abs_ms, horde_entity *> {
        for( const auto &entry : om.horde_entities() ) {
            if( entry.second->monster_data &&
                entry.second->monster_data->predator_state().actor_id == actor ) {
                return { entry.first, entry.second };
            }
        }
        return { source, nullptr };
    };
    set_time( calendar::turn_zero + time_duration::from_turns( 100 + due + 1 ) );
    const bool started = GENERATE( false, true );
    if( started ) {
        overmap_buffer.move_hordes();
        REQUIRE( find_owner().second != nullptr );
        REQUIRE( find_owner().second->monster_data->writhing_stalker_state().revisiting_slot == 0 );
        set_time( calendar::turn + 1_turns );
    }
    const auto before = find_owner().first;
    om.hordes.signal_entities( sound, 20 );
    auto *owner = find_owner().second;
    REQUIRE( owner != nullptr );
    REQUIRE( owner->destination == sound );
    REQUIRE( owner->monster_data->provocative_sound );
    const auto search_start = owner->monster_data->writhing_stalker_state().search_started_active;
    const bool reload = GENERATE( false, true );
    if( reload ) {
        *owner->monster_data = round_trip_writhing_stalker( *owner->monster_data );
        om.hordes.refresh_entity_bucket( before );
    }
    overmap_buffer.move_hordes();
    owner = find_owner().second;
    REQUIRE( owner != nullptr );
    CHECK( owner->destination == sound );
    CHECK( owner->monster_data->wander_pos == sound );
    CHECK( owner->monster_data->provocative_sound );
    CHECK( rl_dist( find_owner().first, sound ) < rl_dist( before, sound ) );
    const auto &pending = owner->monster_data->writhing_stalker_state();
    CHECK( pending.search_started_active == search_start );
    CHECK( pending.familiar[0].revisit_at_active == ( started ? -1 : due ) );
    for( int turn = 0; turn < 12 && find_owner().first != sound; ++turn ) {
        set_time( calendar::turn + 1_turns );
        overmap_buffer.move_hordes();
        REQUIRE( find_owner().second != nullptr );
        REQUIRE( find_owner().second->destination == sound );
    }
    REQUIRE( find_owner().first == sound );
    REQUIRE( find_owner().second->tracking_intensity == 0 );
    CHECK_FALSE( find_owner().second->monster_data->provocative_sound );
    CHECK( find_owner().second->monster_data->wandf == 0 );
    if( reload ) {
        *find_owner().second->monster_data = round_trip_writhing_stalker(
                *find_owner().second->monster_data );
        om.hordes.refresh_entity_bucket( sound );
    }
    // A completed sound cannot be reselected after each step of the due
    // visit. Exercise more than its first resume, including arrival reload.
    int previous_distance = rl_dist( find_owner().first, source + point::west * 48 );
    for( int step = 0; step < 6; ++step ) {
        set_time( calendar::turn + 1_turns );
        overmap_buffer.move_hordes();
        owner = find_owner().second;
        REQUIRE( owner != nullptr );
        CHECK( owner->monster_data->writhing_stalker_state().revisiting_slot == 0 );
        CHECK( owner->destination != sound );
        const int distance = rl_dist( find_owner().first, source + point::west * 48 );
        CHECK( distance < previous_distance );
        previous_distance = distance;
        const auto position = find_owner().first;
        overmap_buffer.move_hordes();
        CHECK( find_owner().first == position );
    }
    set_time( calendar::turn + 11_minutes );
    overmap_buffer.move_hordes();
    owner = find_owner().second;
    REQUIRE( owner != nullptr );
    CHECK( owner->monster_data->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    CHECK( owner->monster_data->writhing_stalker_state().revisiting_slot == -1 );
    CHECK( owner->monster_data->writhing_stalker_state().familiar[0].revisit_at_active >
           owner->monster_data->writhing_stalker_state().active_turns );
    CHECK( owner->monster_data->writhing_stalker_state().evidence_turn == 100 );
    CHECK( owner->tracking_intensity == 0 );
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_distant_light_short_step_survives_transfer_then_expires_without_motion",
           "[writhing_stalker][predator][pursuit][light_transition][fallback]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    here.ter_set( center + point::east * 4, ter_id( "t_wall" ) );
    refresh_writhing_stalker_arena( here );
    monster *local = &spawn_new_stalker_fixture( center );
    local->anger = 100;
    const auto light = here.get_abs( center + point::east * 24 );
    REQUIRE( local->writhing_stalker_state().observe_light_interest(
                 light, "transferred-light-step", 1000, 30 ) );
    local->plan();
    REQUIRE( local->has_dest() );
    REQUIRE( local->get_dest() != light );
    REQUIRE( local->writhing_stalker_state().owns_waypoint( local->get_dest() ) );
    const auto actor = local->predator_state().actor_id;
    const auto epoch = local->predator_state().handoff_epoch;
    const bool transfer = GENERATE( false, true );
    if( transfer ) {
        const auto source = local->pos_abs();
        overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
        om.hordes.clear();
        g->despawn_monster( *local );
        local = nullptr;
        set_time( calendar::turn + 1_turns );
        overmap_buffer.move_hordes();
        for( monster &candidate : g->all_monsters() ) {
            if( candidate.predator_state().actor_id == actor ) {
                local = &candidate;
            }
        }
        REQUIRE( local != nullptr );
        CHECK( local->predator_state().handoff_epoch == epoch + 2 );
        CHECK( local->writhing_stalker_state().light_observed_position == light );
        CHECK( local->writhing_stalker_state().phase != writhing_stalker::lifecycle_phase::searching );
        local->plan();
    }
    REQUIRE( local->has_dest() );
    REQUIRE( local->get_dest() != light );
    REQUIRE( local->writhing_stalker_state().owns_waypoint( local->get_dest() ) );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        *local = round_trip_writhing_stalker( *local );
    }
    CHECK( local->writhing_stalker_state().light_observed_turn == 1000 );
    CHECK( local->writhing_stalker_state().light_expires_turn == 1030 );
    CHECK( local->writhing_stalker_state().light_sample_id == "transferred-light-step" );
    const auto position = local->pos_abs();
    const auto expiry = local->writhing_stalker_state().light_expires_turn;
    set_time( calendar::turn_zero + time_duration::from_turns( expiry ) );
    local->plan();
    CHECK( local->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    CHECK_FALSE( local->writhing_stalker_state().has_light_observed_position );
    CHECK_FALSE( local->has_dest() );
    CHECK( local->wandf == 0 );
    local->set_moves( 100 );
    // Compare the same authenticated body with a plain native idle action.
    // The expired light may not buy a route or another action, but ordinary
    // idle stumbling remains legitimate. Replay the identical engine state,
    // never select a seed or rewrite an existing actor's identity.
    std::ostringstream before_idle;
    { JsonOut json( before_idle ); local->serialize( json ); }
    const auto native_rng = rng_get_engine();
    const auto now = to_turn<int>( calendar::turn );
    const auto active_before = overmap_buffer.global_state.predator_activity.find( actor )->activity.active_at( now );
    auto &idle = local->writhing_stalker_state();
    idle.light_observed_turn = -1;
    idle.light_expires_turn = -1;
    idle.light_sample_id.clear();
    local->move();
    const auto idle_position = local->pos_abs();
    const auto idle_moves = local->get_moves();
    const auto idle_hp = local->get_hp();
    const auto idle_rng_after = rng_get_engine();
    CHECK( overmap_buffer.global_state.predator_activity.find( actor )->activity.active_at( now ) == active_before );
    // Use the actual movement owner to restore the probe's pose before the
    // native body reload. No direct ledger, token, epoch or clock assignment.
    local->setpos( here, here.get_bub( position ) );
    local->deserialize( json_loader::from_string( before_idle.str() ).get_object() );
    REQUIRE( overmap_buffer.global_state.predator_activity.owns(
                 actor, local->predator_state().handoff_epoch,
                 predator_activation::representation::local, position ) );
    rng_get_engine() = native_rng;
    local->move();
    CHECK( local->pos_abs() == idle_position );
    CHECK( local->get_moves() == idle_moves );
    CHECK( local->get_hp() == idle_hp );
    CHECK( rng_get_engine() == idle_rng_after );
    CHECK( overmap_buffer.global_state.predator_activity.find( actor )->activity.active_at( now ) == active_before );
    CHECK_FALSE( local->has_dest() );
    CHECK( local->wandf == 0 );
    CHECK( local->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    CHECK_FALSE( local->writhing_stalker_state().has_committed_waypoint );
    CHECK( horde_entity( *local ).tracking_intensity == 0 );
    CHECK( local->writhing_stalker_state().evidence_turn == -1 );
    clear_map_without_vision();
}

TEST_CASE( "stalker_abstract_sound_keeps_its_carrier_when_light_is_active_or_expires",
           "[writhing_stalker][predator][pursuit][light_transition][signal]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    here.ter_set( center + point::east * 4, ter_id( "t_wall" ) );
    refresh_writhing_stalker_arena( here );
    monster &original = spawn_new_stalker_fixture( center );
    original.anger = 100;
    const auto source = original.pos_abs();
    const auto sound = source + point::south * 6;
    REQUIRE( original.writhing_stalker_state().observe_light_interest(
                 source + point::east * 24, "sound-versus-light-step", 1000, 90 ) );
    original.plan();
    REQUIRE( original.has_dest() );
    const auto old_step = original.get_dest();
    const auto actor = original.predator_state().actor_id;
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    g->despawn_monster( original );
    om.hordes.signal_entities( sound, 200 );
    REQUIRE( overmap_buffer.entity_at( source ) != nullptr );
    REQUIRE( overmap_buffer.entity_at( source )->destination == sound );
    const bool expired = GENERATE( false, true );
    set_time( calendar::turn + ( expired ? 91_turns : 1_turns ) );
    overmap_buffer.move_hordes();
    monster *local = nullptr;
    for( monster &candidate : g->all_monsters() ) {
        if( candidate.predator_state().actor_id == actor ) {
            local = &candidate;
        }
    }
    REQUIRE( local != nullptr );
    CHECK( local->provocative_sound );
    CHECK( local->wander_pos == sound );
    CHECK( rl_dist( local->pos_abs(), sound ) < rl_dist( source, sound ) );
    CHECK_FALSE( ( local->has_dest() && local->get_dest() == old_step ) );
    if( expired ) {
        CHECK_FALSE( local->writhing_stalker_state().has_light_observed_position );
    }
    {
        const int separation = rl_dist( local->pos_abs(), sound );
        for( int turn = 0; turn < 6 && rl_dist( local->pos_abs(), sound ) >= separation; ++turn ) {
            set_time( calendar::turn + 1_turns );
            local->plan();
            local->set_moves( 100 );
            local->move();
        }
        CHECK( rl_dist( local->pos_abs(), sound ) < separation );
    }
    CHECK( local->writhing_stalker_state().evidence_turn == -1 );
    clear_map_without_vision();
}

TEST_CASE( "stalker_distant_remembered_goal_routes_local_steps_after_real_sound",
           "[writhing_stalker][predator][pursuit][routing][signal]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    monster &stalker = spawn_new_stalker_fixture( center );
    stalker.anger = 100;
    const auto remembered = stalker.pos_abs() + point::west * 180;
    REQUIRE_FALSE( here.inbounds( remembered ) );
    REQUIRE( scout_observation::site_sightline( stalker.pos_abs(), remembered ).empty() );
    // Authenticate this actual native birth before waiting for earned work.
    REQUIRE( overmap_buffer.predator_activity_for( stalker,
             predator_activation::representation::local, stalker.pos_abs() ) );
    CHECK( overmap_buffer.predator_active_time( stalker ) == 0 );
    auto &memory = stalker.writhing_stalker_state();
    memory.advance_to( 1000 );
    // Accepted observation fixture; navigation does not claim remote sensing.
    REQUIRE( memory.observe_target( 77, 1, remembered, 1000 ) );
    memory.finish_search();
    const auto due = memory.familiar[0].revisit_at_active;
    const bool followed_sound = GENERATE( false, true );
    if( followed_sound ) {
        const auto before = stalker.pos_abs();
        const auto sound = center + point::east * 8;
        stalker.hear_sound( sound, 2000, 8, true );
        REQUIRE( stalker.provocative_sound );
        for( int turn = 0; turn < 40 && stalker.pos_bub() != sound; ++turn ) {
            stalker.plan();
            stalker.set_moves( 100 );
            stalker.move();
            set_time( calendar::turn + 1_turns );
        }
        REQUIRE( stalker.pos_bub() == sound );
        REQUIRE( stalker.pos_abs() != before );
        REQUIRE( memory.familiar[0].revisit_at_active == due );
    }
    const bool light = GENERATE( false, true );
    if( light ) {
        // Recent direct evidence deliberately outranks a light observation.
        set_time( calendar::turn + 201_turns );
        // Consume the real owning interval before injecting this accepted
        // light-observation fixture; do not manually debit the same work.
        stalker.plan();
        REQUIRE( memory.observe_light_interest( remembered, "remembered-distant-source",
                 to_turn<int>( calendar::turn ), 300 ) );
    } else {
        set_time( calendar::turn_zero + time_duration::from_turns( 1000 + due + 1 ) );
    }
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    const auto before = stalker.pos_abs();
    const auto evidence = stalker.writhing_stalker_state().evidence_turn;
    stalker.plan();
    REQUIRE( stalker.has_dest() );
    const auto step = stalker.get_dest();
    CHECK( here.inbounds( step ) );
    CHECK( step != before );
    CHECK( step != remembered );
    CHECK( rl_dist( step, remembered ) < rl_dist( before, remembered ) );
    REQUIRE_FALSE( here.route( stalker, pathfinding_target::point( here.get_bub( step ) ) ).empty() );
    if( !light ) {
        CHECK( stalker.writhing_stalker_state().revisiting_slot == 0 );
        CHECK( stalker.writhing_stalker_state().searched_positions == 0 );
    }
    for( int turn = 0; turn < 6 && stalker.pos_abs() == before; ++turn ) {
        stalker.set_moves( 100 );
        stalker.move();
        set_time( calendar::turn + 1_turns );
        stalker.plan();
    }
    CHECK( rl_dist( stalker.pos_abs(), remembered ) < rl_dist( before, remembered ) );
    CHECK( stalker.writhing_stalker_state().evidence_turn == evidence );
    clear_map_without_vision();
}

TEST_CASE( "stalker_distant_remembered_navigation_keeps_loaded_and_blocked_limits",
           "[writhing_stalker][predator][pursuit][routing]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 2, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    const bool blocked = GENERATE( false, true );
    if( blocked ) {
        for( const auto &p : here.points_in_radius( center, 1 ) ) {
            if( p != center ) {
                here.ter_set( p, ter_id( "t_wall" ) );
            }
        }
        refresh_writhing_stalker_arena( here );
    }
    monster &stalker = spawn_new_stalker_fixture( center );
    // Authenticate this actual native birth before waiting for earned work.
    REQUIRE( overmap_buffer.predator_activity_for( stalker,
             predator_activation::representation::local, stalker.pos_abs() ) );
    CHECK( overmap_buffer.predator_active_time( stalker ) == 0 );
    auto &memory = stalker.writhing_stalker_state();
    memory.advance_to( 1000 );
    const auto remembered = stalker.pos_abs() + point::west * 180;
    REQUIRE( memory.observe_target( 77, 1, remembered, 1000 ) );
    memory.finish_search();
    set_time( calendar::turn_zero + time_duration::from_turns(
                  1000 + memory.familiar[0].revisit_at_active + 1 ) );
    stalker.plan();
    if( blocked ) {
        CHECK_FALSE( stalker.has_dest() );
        CHECK_FALSE( memory.has_committed_waypoint );
        CHECK( memory.searched_positions == 0 );
    } else {
        REQUIRE( stalker.has_dest() );
        CHECK( stalker.get_dest() != stalker.pos_abs() );
        CHECK( here.inbounds( stalker.get_dest() ) );
        CHECK( stalker.get_dest() != remembered );
        CHECK( rl_dist( stalker.get_dest(), remembered ) < rl_dist( stalker.pos_abs(), remembered ) );
        CHECK_FALSE( here.route( stalker,
                                pathfinding_target::point( here.get_bub( stalker.get_dest() ) ) ).empty() );
        CHECK( memory.revisiting_slot == 0 );
    }
    CHECK( memory.evidence_turn == 1000 );
    clear_map_without_vision();
}

TEST_CASE( "stalker_visible_stationary_shadow_continues_steps_then_holds_standoff",
           "[writhing_stalker][predator][pursuit][routing][shadow_continuation]" )
{
    clear_map_without_vision();
    clear_npcs();
    clear_avatar();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1_days + 12_hours );
    scoped_weather_override weather( weather_type_id( "clear" ) );
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    for( const auto &p : here.points_in_radius( center, 35 ) ) {
        here.ter_set( p, ter_id( "t_floor" ) );
        here.furn_clear( p );
        here.clear_fields( p );
    }
    // Native route permits long straight cheap paths before its bounded
    // detour search. A transparent slow tile exercises the short-step path
    // without obstructing the actual continuous sight under test.
    for( int dy = -1; dy <= 1; ++dy ) {
        here.ter_set( center + point( 28, dy ), ter_id( "t_dirtmound" ) );
    }
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center + point::east * 32 );
    stalker.anger = 100;
    stalker.aggro_character = true;
    // Actual same-target pressure keeps this native bright/focused approach
    // in Shadow; a lone exposed observer legitimately enters cooldown.
    for( const point &offset : { point::west, point::south, point::east, point::north } ) {
        monster &zombie = spawn_test_monster( mon_zombie.str(), center + offset );
        zombie.aggro_character = true;
        zombie.set_dest( you.pos_abs() );
    }
    const bool occupied_standoffs = GENERATE( false, true );
    if( occupied_standoffs ) {
        for( const auto &p : here.points_in_radius( center, 4 ) ) {
            const int distance = rl_dist( p, center );
            if( distance >= 2 && distance <= 4 ) {
                monster &zombie = spawn_test_monster( mon_zombie.str(), p );
                zombie.aggro_character = true;
                zombie.set_dest( you.pos_abs() );
            }
        }
    }
    REQUIRE( writhing_stalker::observed_zombie_pressure( stalker, here, you ) >= 4 );
    const auto target_position = you.pos_abs();
    const int target_hp = you.get_hp();
    REQUIRE( writhing_stalker::can_observe_actor( stalker, you ) );
    REQUIRE( stalker.get_pathfinding_settings().max_dist == 10 );
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::shadowing );
    REQUIRE( stalker.has_dest() );
    const auto first = stalker.get_dest();
    REQUIRE( first != stalker.pos_abs() );
    REQUIRE( rl_dist( first, target_position ) > 10 );
    // A native route may become unreachable after a partial move. Follow
    // its actual accepted replacement step, not an assumed fixed first goal.
    tripoint_abs_ms completed = first;
    bool reached = false;
    for( int turn = 0; turn < 40 && !reached; ++turn ) {
        REQUIRE( writhing_stalker::can_observe_actor( stalker, you ) );
        stalker.plan();
        REQUIRE( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::shadowing );
        REQUIRE( stalker.has_dest() );
        completed = stalker.get_dest();
        stalker.set_moves( 100 );
        stalker.move();
        reached = stalker.pos_abs() == completed;
        set_time( calendar::turn + 1_turns );
    }
    REQUIRE( reached );
    REQUIRE( rl_dist( completed, target_position ) > 10 );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    stalker.plan();
    REQUIRE( stalker.writhing_stalker_state().last_observed_position == target_position );
    REQUIRE( stalker.writhing_stalker_state().evidence_turn == to_turn<int>( calendar::turn ) );
    REQUIRE( stalker.has_dest() );
    REQUIRE( stalker.get_dest() != completed );
    for( int turn = 0; turn < 80 && rl_dist( stalker.pos_abs(), target_position ) > 8; ++turn ) {
        REQUIRE( writhing_stalker::can_observe_actor( stalker, you ) );
        stalker.plan();
        REQUIRE( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::shadowing );
        stalker.set_moves( 100 );
        stalker.move();
        set_time( calendar::turn + 1_turns );
    }
    REQUIRE( rl_dist( stalker.pos_abs(), target_position ) <= 8 );
    // A legitimate final shadow destination is allowed to remain occupied;
    // renewal of intermediate steps must not create perpetual movement.
    for( int turn = 0; turn < 20; ++turn ) {
        stalker.plan();
        stalker.set_moves( 100 );
        stalker.move();
        set_time( calendar::turn + 1_turns );
    }
    const auto settled = stalker.pos_abs();
    REQUIRE( rl_dist( settled, target_position ) >= 2 );
    REQUIRE( rl_dist( settled, target_position ) <= 8 );
    REQUIRE( stalker.writhing_stalker_state().has_committed_waypoint );
    const auto final_goal = stalker.writhing_stalker_state().committed_waypoint;
    if( occupied_standoffs ) {
        CHECK( rl_dist( final_goal, target_position ) >= 7 );
    } else {
        CHECK( rl_dist( final_goal, target_position ) <= 4 );
    }
    stalker = round_trip_writhing_stalker( stalker );
    for( int turn = 0; turn < 20; ++turn ) {
        stalker.plan();
        CHECK( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::shadowing );
        CHECK( stalker.writhing_stalker_state().committed_waypoint == final_goal );
        stalker.set_moves( 100 );
        stalker.move();
        set_time( calendar::turn + 1_turns );
        // Ordinary idle movement may stumble one square; it must not renew
        // the approach or walk a chain of new goals beside stationary prey.
        CHECK( rl_dist( stalker.pos_abs(), final_goal ) <= 1 );
    }
    CHECK( you.pos_abs() == target_position );
    CHECK( you.get_hp() == target_hp );
    CHECK( stalker.writhing_stalker_state().attempts_spent == 0 );
    const auto observed = stalker.writhing_stalker_state().last_observed_position;
    const int last_seen = stalker.writhing_stalker_state().evidence_turn;
    const auto wall_x = ( stalker.pos_bub().x() + you.pos_bub().x() ) / 2;
    for( int y = 30; y <= 100; ++y ) {
        here.ter_set( tripoint_bub_ms( wall_x, y, 0 ), ter_id( "t_wall" ) );
    }
    you.setpos( here, center + point::south );
    refresh_writhing_stalker_arena( here );
    REQUIRE_FALSE( writhing_stalker::can_observe_actor( stalker, you ) );
    set_time( calendar::turn + 1_turns );
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().evidence_turn == last_seen );
    CHECK( stalker.writhing_stalker_state().last_observed_position == observed );
    CHECK( stalker.writhing_stalker_state().heading == tripoint_rel_ms::zero );
    set_time( calendar::turn + 31_minutes );
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    CHECK_FALSE( stalker.has_dest() );
    clear_map_without_vision();
}

TEST_CASE( "stalker_expired_sound_flag_cannot_own_delayed_transferred_pursuit",
           "[writhing_stalker][predator][pursuit][scheduler][signal][fallback]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    monster &original = spawn_new_stalker_fixture( center );
    original.anger = 100;
    original.hear_sound( center + point::south * 3, 24, 3, true );
    REQUIRE( original.provocative_sound );
    REQUIRE( original.wandf > 0 );
    const int heard_budget = original.wandf;
    for( int turn = 0; turn <= heard_budget && original.wandf > 0; ++turn ) {
        original.set_moves( 100 );
        original.move();
        set_time( calendar::turn + 1_turns );
    }
    REQUIRE( original.wandf == 0 );
    REQUIRE( original.provocative_sound );
    original = round_trip_writhing_stalker( original );
    REQUIRE( original.wandf == 0 );
    REQUIRE( original.provocative_sound );
    auto &memory = original.writhing_stalker_state();
    const int seen = to_turn<int>( calendar::turn );
    memory.advance_to( seen );
    const bool learned = GENERATE( true, false );
    // Accepted observation fixtures feed the actual local planner; neither
    // this remembered sight nor the light packet claims native acquisition.
    if( learned ) {
        REQUIRE( memory.observe_target( 77, 1, original.pos_abs() + point::east * 24, seen ) );
    } else {
        REQUIRE( memory.observe_light_interest( original.pos_abs() + point::east * 24,
                 "expired-flag-native-light", seen, 120 ) );
    }
    set_time( calendar::turn + 61_turns );
    original.plan();
    REQUIRE( memory.phase == ( learned ? writhing_stalker::lifecycle_phase::searching :
                              writhing_stalker::lifecycle_phase::shadowing ) );
    REQUIRE( original.has_dest() );
    REQUIRE( memory.owns_waypoint( original.get_dest() ) );
    const auto old_route = original.get_dest();
    const auto source = original.pos_abs();
    const auto actor = original.predator_state().actor_id;
    const auto epoch = original.predator_state().handoff_epoch;
    // A later genuinely accepted sound remains independent even when it is
    // at exactly the carried route coordinate.
    const int sound = GENERATE( 0, 1, 2 );
    tripoint_abs_ms heard;
    if( sound ) {
        heard = sound == 2 ? old_route : source + point::south * 6;
        original.hear_sound( here.get_bub( heard ), 2000, 6, true );
        REQUIRE( original.wander_pos == heard );
        REQUIRE( original.wandf > 0 );
    }
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    for( int x = -30; x <= 30; ++x ) {
        for( int y = -30; y <= 30; ++y ) {
            overmap_buffer.set_passable( source + point( x, y ), true );
        }
    }
    g->despawn_monster( original );
    auto *owner = overmap_buffer.entity_at( source );
    REQUIRE( owner != nullptr );
    REQUIRE( owner->tracking_intensity == 1000 );
    REQUIRE( owner->monster_data->provocative_sound );
    REQUIRE( owner->monster_data->predator_state().handoff_epoch == epoch + 1 );
    const bool early_reentry = GENERATE( false, true );
    if( early_reentry ) {
        // The real load caller synchronizes a disposable handoff payload.
        // Exercise its provenance before a later unload and expiry too.
        overmap_buffer.spawn_monster( project_to<coords::sm>( source ) );
        monster *local = nullptr;
        for( monster &candidate : g->all_monsters() ) {
            if( candidate.is_caol_predator() && candidate.predator_state().actor_id == actor ) {
                REQUIRE( local == nullptr );
                local = &candidate;
            }
        }
        REQUIRE( local != nullptr );
        REQUIRE( local->predator_state().handoff_epoch == epoch + 2 );
        g->despawn_monster( *local );
        owner = overmap_buffer.entity_at( source );
        REQUIRE( owner != nullptr );
        REQUIRE( owner->monster_data->predator_state().handoff_epoch == epoch + 3 );
    }
    const bool reload = GENERATE( false, true );
    if( reload ) {
        *owner->monster_data = round_trip_writhing_stalker( *owner->monster_data );
        om.hordes.refresh_entity_bucket( source );
    }
    // Native placement is blocked only at the old location. An expired route
    // must not move elsewhere or turn itself into a fresh sound carrier.
    spawn_test_monster( mon_zombie.str(), here.get_bub( source ) );
    set_time( calendar::turn + 31_minutes );
    overmap_buffer.move_hordes();
    monster *payload = nullptr;
    tripoint_abs_ms position;
    int owners = 0;
    horde_entity *abstract = nullptr;
    for( const auto &entry : om.horde_entities() ) {
        if( entry.second->monster_data && entry.second->monster_data->is_caol_predator() &&
            entry.second->monster_data->predator_state().actor_id == actor ) {
            abstract = entry.second;
            payload = abstract->monster_data.get();
            position = entry.first;
            ++owners;
        }
    }
    for( monster &candidate : g->all_monsters() ) {
        if( candidate.is_caol_predator() && candidate.predator_state().actor_id == actor ) {
            payload = &candidate;
            position = candidate.pos_abs();
            ++owners;
        }
    }
    REQUIRE( payload != nullptr );
    REQUIRE( owners == 1 );
    CHECK( payload->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    CHECK_FALSE( payload->writhing_stalker_state().has_committed_waypoint );
    CHECK( payload->writhing_stalker_state().evidence_turn == ( learned ? seen : -1 ) );
    if( sound ) {
        CHECK( payload->provocative_sound );
        CHECK( payload->wandf > 0 );
        CHECK( payload->wander_pos == heard );
        CHECK( rl_dist( position, heard ) < rl_dist( source, heard ) );
    } else {
        CHECK( position == source );
        REQUIRE( abstract != nullptr );
        CHECK( abstract->tracking_intensity == 0 );
        CHECK( abstract->is_active() == learned );
        CHECK( payload->wandf == 0 );
        CHECK_FALSE( payload->has_dest() );
        CHECK_FALSE( payload->provocative_sound );
        const auto active = payload->writhing_stalker_state().active_turns;
        overmap_buffer.move_hordes();
        CHECK( payload->writhing_stalker_state().active_turns == active );
        CHECK( abstract->tracking_intensity == 0 );
    }
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_sound_stays_pending_beneath_current_sight_or_retreat_handoff",
           "[writhing_stalker][predator][pursuit][signal][fallback]" )
{
    clear_map_without_vision();
    clear_avatar();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1_days );
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    you.setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &original = spawn_new_stalker_fixture( center + point::east * 3 );
    original.anger = 100;
    original.aggro_character = true;
    const bool retreat = GENERATE( false, true );
    if( retreat ) {
        original.writhing_stalker_state().attempts_spent = 2;
    }
    original.plan();
    REQUIRE( original.writhing_stalker_state().phase == ( retreat ?
             writhing_stalker::lifecycle_phase::retreating : writhing_stalker::lifecycle_phase::shadowing ) );
    REQUIRE( original.has_dest() );
    const auto goal = original.get_dest();
    tripoint_abs_ms position = original.pos_abs();
    const bool same_coordinate = GENERATE( false, true );
    const auto sound = same_coordinate ? goal : position + point::south * 6;
    original.hear_sound( here.get_bub( sound ), 2000, 6, true );
    REQUIRE( original.wander_pos == sound );
    REQUIRE( original.wandf > 0 );
    const auto actor = original.predator_state().actor_id;
    const auto epoch = original.predator_state().handoff_epoch;
    overmap &om = overmap_buffer.get( project_to<coords::om>( position ).xy() );
    om.hordes.clear();
    for( int x = -12; x <= 12; ++x ) {
        for( int y = -12; y <= 12; ++y ) {
            overmap_buffer.set_passable( position + point( x, y ), true );
        }
    }
    g->despawn_monster( original );
    set_time( calendar::turn + 1_turns );
    overmap_buffer.move_hordes();
    monster *local = nullptr;
    for( monster &candidate : g->all_monsters() ) {
        if( candidate.is_caol_predator() && candidate.predator_state().actor_id == actor ) {
            REQUIRE( local == nullptr );
            local = &candidate;
        }
    }
    REQUIRE( local != nullptr );
    REQUIRE( local->predator_state().handoff_epoch == epoch + 2 );
    CHECK( local->provocative_sound );
    CHECK( local->wandf > 0 );
    CHECK( local->wander_pos == sound );
    position = local->pos_abs();
    // A local copy cannot claim the abstract representation. Keep this
    // refusal before exercising a second genuine native handoff.
    horde_entity refused( *local );
    std::ostringstream refused_before;
    { JsonOut json( refused_before ); refused.monster_data->serialize( json ); }
    CHECK_FALSE( overmap_buffer.predator_activity_for( *refused.monster_data,
                 predator_activation::representation::abstract, position ) );
    std::ostringstream refused_after;
    { JsonOut json( refused_after ); refused.monster_data->serialize( json ); }
    CHECK( refused_after.str() == refused_before.str() );
    REQUIRE( g->despawn_monster( *local ) );
    auto *accepted = overmap_buffer.entity_at( position );
    REQUIRE( accepted != nullptr );
    REQUIRE( accepted->monster_data );
    CHECK( accepted->monster_data->predator_state().handoff_epoch == epoch + 3 );
    on_out_of_scope release_native_fixture( [&] { om.release_monster_payload( position, actor ); } );
    horde_entity &owner = *accepted;
    REQUIRE( overmap_buffer.predator_activity_for( *owner.monster_data,
             predator_activation::representation::abstract, position ) );
    REQUIRE( owner.advance_predator_intent( position, to_turn<int>( calendar::turn ) ) );
    // Native fleeing does not acquire a fresh hostile watch observation.
    CHECK( owner.monster_data->writhing_stalker_state().evidence_turn ==
           to_turn<int>( calendar::turn ) - ( retreat ? 1 : 0 ) );
    if( retreat ) {
        CHECK( owner.destination == goal );
    } else {
        CHECK( owner.monster_data->writhing_stalker_state().owns_waypoint( owner.destination ) );
    }
    owner.synchronize_payload( position );
    CHECK( owner.monster_data->provocative_sound );
    CHECK( owner.monster_data->wandf > 0 );
    CHECK( owner.monster_data->wander_pos == sound );
    if( !same_coordinate ) {
        CHECK( owner.destination != sound );
        CHECK( owner.monster_data->get_dest() == owner.destination );
    }
    // Remove actual sight and let the existing recovery deadline settle.
    // The still-live sound can then own movement above remembered searching.
    you.setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    refresh_writhing_stalker_arena( here );
    set_time( calendar::turn + 31_minutes );
    int elapsed = 0;
    REQUIRE( overmap_buffer.predator_activity_for( *owner.monster_data,
             predator_activation::representation::abstract, position, &elapsed ) );
    REQUIRE( owner.advance_predator_intent( position, to_turn<int>( calendar::turn ), elapsed ) );
    CHECK( owner.destination == sound );
    CHECK( owner.tracking_intensity > 0 );
    CHECK( owner.monster_data->wander_pos == sound );
    CHECK_FALSE( owner.monster_data->has_dest() );
    clear_map_without_vision();
}

TEST_CASE( "stalker_selected_sound_budget_is_not_renewed_before_native_transport",
           "[writhing_stalker][predator][pursuit][scheduler][signal]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    monster &original = spawn_new_stalker_fixture( center );
    original.anger = 100;
    original.hear_sound( center + point::south * 6, 30, 6, true );
    REQUIRE( original.provocative_sound );
    REQUIRE( original.wandf > 3 );
    const auto sound = original.wander_pos;
    const int budget = original.wandf;
    const auto source = original.pos_abs();
    original.set_moves( -300 );
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    g->despawn_monster( original );
    for( int step = 1; step <= 2; ++step ) {
        set_time( calendar::turn + 1_turns );
        overmap_buffer.move_hordes();
        auto *owner = overmap_buffer.entity_at( source );
        REQUIRE( owner != nullptr );
        CHECK( owner->destination == sound );
        CHECK( owner->tracking_intensity == budget - step );
        CHECK( owner->moves <= 0 );
        // No transport has synchronized the payload yet. Repeated intent
        // service must not refill the entity from this older native budget.
        CHECK( owner->monster_data->wandf == budget );
    }
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_native_unlearned_goal_precedes_pending_sound_during_handoff",
           "[writhing_stalker][predator][pursuit][scheduler][signal][fallback]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    monster &original = spawn_new_stalker_fixture( center );
    original.anger = 100;
    const auto goal = here.get_abs( center + point::east * 6 );
    const auto sound = here.get_abs( center + point::south * 6 );
    original.set_dest( goal );
    original.hear_sound( here.get_bub( sound ), 2000, 6, true );
    REQUIRE( original.wander_pos == sound );
    original.set_moves( 100 );
    original.move();
    const auto source = original.pos_abs();
    REQUIRE( rl_dist( source, goal ) < rl_dist( here.get_abs( center ), goal ) );
    REQUIRE( original.has_dest() );
    REQUIRE_FALSE( original.writhing_stalker_state().has_pursuit_memory );
    const auto actor = original.predator_state().actor_id;
    original.set_moves( 100 );
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    for( int x = -12; x <= 12; ++x ) {
        for( int y = -12; y <= 12; ++y ) {
            overmap_buffer.set_passable( source + point( x, y ), true );
        }
    }
    g->despawn_monster( original );
    set_time( calendar::turn + 1_turns );
    overmap_buffer.move_hordes();
    monster *local = nullptr;
    for( monster &candidate : g->all_monsters() ) {
        if( candidate.is_caol_predator() && candidate.predator_state().actor_id == actor ) {
            REQUIRE( local == nullptr );
            local = &candidate;
        }
    }
    REQUIRE( local != nullptr );
    CHECK( rl_dist( local->pos_abs(), goal ) < rl_dist( source, goal ) );
    CHECK( local->has_dest() );
    CHECK( local->get_dest() == goal );
    CHECK( local->provocative_sound );
    CHECK( local->wander_pos == sound );
    CHECK_FALSE( local->writhing_stalker_state().has_pursuit_memory );
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_pending_sound_spends_native_budget_on_committed_route_steps",
           "[writhing_stalker][predator][pursuit][scheduler][signal][fallback]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    monster &heard_locally = spawn_new_stalker_fixture( center );
    heard_locally.anger = 100;
    heard_locally.hear_sound( center + point::south * 6, 30, 6, true );
    REQUIRE( heard_locally.provocative_sound );
    const int prior_budget = heard_locally.wandf;
    REQUIRE( prior_budget > 2 );
    const auto bytes = []( const auto &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    const auto removed_actor = heard_locally.predator_state().actor_id;
    auto removed = get_creature_tracker().find( heard_locally.pos_abs() );
    REQUIRE( removed.get() == &heard_locally );
    monster translated( heard_locally );
    g->remove_zombie( heard_locally );
    const auto removed_before = bytes( *removed );
    // Translation is NOT a handoff: the old LOCAL owner must refuse this
    // copied identity at another abstract pose, without changing either body.
    const auto source = here.get_abs( tripoint_bub_ms( -360, 65, 0 ) );
    REQUIRE_FALSE( here.inbounds( source ) );
    REQUIRE( rl_dist( project_to<coords::omt>( source ).xy(),
                     get_avatar().pos_abs_omt().xy() ) <= 40 );
    const auto sound = source + point::south * 6;
    translated.set_pos_abs_only( source );
    translated.wander_to( sound, prior_budget );
    translated.set_dest( source + point::east * ( prior_budget + 12 ) );
    translated.set_moves( 100 );
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    REQUIRE( om.hordes.spawn_entity( source, translated ) );
    auto *refused = overmap_buffer.entity_at( source );
    REQUIRE( refused != nullptr );
    const auto translated_before = bytes( *refused->monster_data );
    auto &world = overmap_buffer.global_state.predator_activity;
    REQUIRE( world.find( removed_actor ) );
    const auto authority_before = bytes( *world.find( removed_actor ) );
    CHECK_FALSE( overmap_buffer.predator_activity_at( *refused, source ) );
    CHECK_FALSE( overmap_buffer.predator_activity_for( *refused->monster_data,
                 predator_activation::representation::abstract, source ) );
    CHECK( bytes( *refused->monster_data ) == translated_before );
    CHECK( bytes( *removed ) == removed_before );
    CHECK( bytes( *world.find( removed_actor ) ) == authority_before );
    REQUIRE( om.release_monster_payload( source, removed_actor ) );

    // Separate genuine NEW abstract birth. No old identity or pose is
    // rewritten to gain this owner. The native signal is real abstract input,
    // not physical off-bubble hearing or a claimed remote observation.
    monster original = new_stalker_fixture_body();
    original.set_pos_abs_only( source );
    original.set_moves( 100 );
    const auto actor = original.predator_state().actor_id;
    REQUIRE( actor != removed_actor );
    REQUIRE( om.hordes.spawn_entity( source, original ) );
    om.hordes.signal_entities( sound, 5 );
    auto *accepted = overmap_buffer.entity_at( source );
    REQUIRE( accepted != nullptr );
    REQUIRE( accepted->monster_data );
    REQUIRE( accepted->monster_data->provocative_sound );
    const int budget = accepted->monster_data->wandf;
    REQUIRE( budget > 2 );
    const auto goal = source + point::east * ( budget + 12 );
    const auto epoch = accepted->monster_data->predator_state().handoff_epoch;
    REQUIRE( world.find( actor ) );
    const auto clock_before = bytes( *world.find( actor ) );
    const int active_delta_before = world.find( actor )->activity.active_delta;
    const bool confirmed_before = world.find( actor )->confirmed;
    point_abs_om ignored;
    tripoint_om_ms local;
    std::tie( ignored, local ) = project_remain<coords::om>( source );
    auto node = om.hordes.extract( om.hordes.find( local ) );
    auto payload = std::move( node.mapped().monster_data );
    payload->set_dest( goal );
    const auto route_body = bytes( *payload );
    // This value construction is route fixture preparation, not a new sensed
    // order. Keep the actual payload/UIDs and native node ownership intact.
    horde_entity route( *payload );
    route.monster_data = std::move( payload );
    node.mapped() = std::move( route );
    om.hordes.insert( std::move( node ) );
    accepted = overmap_buffer.entity_at( source );
    REQUIRE( accepted != nullptr );
    CHECK( bytes( *accepted->monster_data ) == route_body );
    CHECK( accepted->monster_data->pos_abs() == source );
    CHECK( accepted->monster_data->predator_state().actor_id == actor );
    CHECK( accepted->monster_data->predator_state().handoff_epoch == epoch );
    CHECK( bytes( *world.find( actor ) ) == clock_before );
    CHECK( world.find( actor )->activity.active_delta == active_delta_before );
    CHECK( world.find( actor )->confirmed == confirmed_before );
    CHECK( world.owns( actor, epoch, predator_activation::representation::abstract, source ) );
    for( int x = 0; x <= budget + 12; ++x ) {
        const auto cell = source + point::east * x;
        overmap_buffer.get( project_to<coords::om>( cell ).xy() );
        overmap_buffer.set_passable( cell, true );
    }
    for( int step = 1; step <= budget; ++step ) {
        set_time( calendar::turn + 1_turns );
        overmap_buffer.move_hordes();
        const auto position = source + point::east * step;
        REQUIRE_FALSE( here.inbounds( position ) );
        auto *owner = overmap_buffer.entity_at( position );
        REQUIRE( owner != nullptr );
        REQUIRE( owner->monster_data->predator_state().actor_id == actor );
        CHECK( owner->destination == goal );
        if( step < budget ) {
            CHECK( owner->monster_data->provocative_sound );
            CHECK( owner->monster_data->wander_pos == sound );
            CHECK( owner->monster_data->wandf == budget - step );
        } else {
            // The remaining route carrier is explicitly not new sound.
            CHECK_FALSE( owner->monster_data->provocative_sound );
            CHECK( owner->monster_data->wander_pos == goal );
        }
        const int remaining = owner->monster_data->wandf;
        overmap_buffer.move_hordes();
        CHECK( owner->monster_data->wandf == remaining );
    }
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_native_recovery_rejects_light_before_touching_pending_sound",
           "[writhing_stalker][predator][pursuit][scheduler][signal][light_transition]" )
{
    clear_map_without_vision();
    clear_avatar();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1_days );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &original = spawn_new_stalker_fixture( center + point::east * 3 );
    original.anger = 100;
    original.aggro_character = true;
    // Exhausted budget is a fixture; actual native planning owns withdrawal,
    // its destination and recovery deadline. No phase is assigned here.
    original.writhing_stalker_state().attempts_spent = 2;
    original.plan();
    REQUIRE( original.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::retreating );
    const auto goal = original.writhing_stalker_state().retreat_waypoint;
    const int deadline = original.writhing_stalker_state().cooldown_until_turn;
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    refresh_writhing_stalker_arena( here );
    const bool arrived = GENERATE( false, true );
    if( arrived ) {
        for( int turn = 0; turn < 40 && original.pos_abs() != goal; ++turn ) {
            original.plan();
            original.set_moves( 100 );
            original.move();
            set_time( calendar::turn + 1_turns );
        }
        REQUIRE( original.pos_abs() == goal );
        original.plan();
        REQUIRE( original.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::cooldown );
    }
    // Remove recent direct evidence as an alternative rejection cause, while
    // retaining the original native recovery deadline.
    set_time( calendar::turn + 201_turns );
    original.plan();
    REQUIRE_FALSE( original.writhing_stalker_state().has_direct_evidence( to_turn<int>( calendar::turn ) ) );
    REQUIRE( original.writhing_stalker_state().cooldown_active( to_turn<int>( calendar::turn ) ) );
    REQUIRE( original.writhing_stalker_state().cooldown_until_turn == deadline );
    const auto position = original.pos_abs();
    const auto actor = original.predator_state().actor_id;
    const auto epoch = original.predator_state().handoff_epoch;
    overmap &om = overmap_buffer.get( project_to<coords::om>( position ).xy() );
    om.hordes.clear();
    g->despawn_monster( original );
    auto *owner = overmap_buffer.entity_at( position );
    REQUIRE( owner != nullptr );
    REQUIRE( owner->monster_data->predator_state().handoff_epoch == epoch + 1 );
    if( arrived ) {
        REQUIRE( owner->tracking_intensity == 0 );
    }
    const bool pending_sound = GENERATE( false, true );
    const bool same_coordinate = GENERATE( false, true );
    const auto sound = position + point::south * 6;
    const auto light = same_coordinate ? sound : position + point::west * 6;
    if( pending_sound ) {
        // The accepted owner is abstract. Its local-hearing callback must
        // refuse without touching this body; genuine abstract sound is then
        // delivered through the native horde owner, above existing interest.
        std::ostringstream before;
        { JsonOut json( before ); owner->monster_data->serialize( json ); }
        CHECK_FALSE( overmap_buffer.predator_activity_for( *owner->monster_data,
                     predator_activation::representation::local, position ) );
        owner->monster_data->hear_sound( here.get_bub( sound ), 30, 6, true );
        std::ostringstream after;
        { JsonOut json( after ); owner->monster_data->serialize( json ); }
        CHECK( after.str() == before.str() );
        om.hordes.signal_entities( sound, 100 );
        owner = overmap_buffer.entity_at( position );
        REQUIRE( owner != nullptr );
        REQUIRE( owner->monster_data->provocative_sound );
        REQUIRE( owner->monster_data->wandf > 0 );
    }
    const bool reload = GENERATE( false, true );
    if( reload ) {
        *owner->monster_data = round_trip_writhing_stalker( *owner->monster_data );
        om.hordes.refresh_entity_bucket( position );
        owner = overmap_buffer.entity_at( position );
    }
    const auto before_destination = owner->destination;
    const int before_tracking = owner->tracking_intensity;
    const auto before_wander = owner->monster_data->wander_pos;
    const int before_budget = owner->monster_data->wandf;
    const bool before_sound = owner->monster_data->provocative_sound;
    const auto before_phase = owner->monster_data->writhing_stalker_state().phase;
    for( int packet = 0; packet < 2; ++packet ) {
        CHECK( om.hordes.attract_entities_to_light( light, 2000, 20,
                "recovery-light-" + std::to_string( packet ), calendar::turn,
                []( const tripoint_abs_ms & ) { return true; }, nullptr ) == 0 );
        owner = overmap_buffer.entity_at( position );
        REQUIRE( owner != nullptr );
        CHECK( owner->destination == before_destination );
        CHECK( owner->tracking_intensity == before_tracking );
        CHECK( owner->light_sample_id.empty() );
        CHECK( owner->monster_data->wander_pos == before_wander );
        CHECK( owner->monster_data->wandf == before_budget );
        CHECK( owner->monster_data->provocative_sound == before_sound );
        CHECK( owner->monster_data->writhing_stalker_state().phase == before_phase );
        CHECK_FALSE( owner->monster_data->writhing_stalker_state().has_light_observed_position );
        CHECK( owner->monster_data->predator_state().actor_id == actor );
    }
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_idle_light_admission_preserves_independent_native_sound",
           "[writhing_stalker][predator][pursuit][scheduler][signal][light_transition]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    monster &original = spawn_new_stalker_fixture( center );
    const auto source = original.pos_abs();
    const auto sound = source + point::south * 6;
    const bool pending = GENERATE( false, true );
    if( pending ) {
        original.hear_sound( here.get_bub( sound ), 30, 6, true );
        REQUIRE( original.provocative_sound );
    }
    const bool same_coordinate = GENERATE( false, true );
    const auto light = same_coordinate ? sound : source + point::west * 6;
    const auto actor = original.predator_state().actor_id;
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    g->despawn_monster( original );
    auto *owner = overmap_buffer.entity_at( source );
    REQUIRE( owner != nullptr );
    const auto prior_destination = owner->destination;
    const auto prior_tracking = owner->tracking_intensity;
    const int budget = owner->monster_data->wandf;
    CHECK( om.hordes.attract_entities_to_light( light, 2000, 20, "idle-light-admission",
            calendar::turn, []( const tripoint_abs_ms & ) { return true; }, nullptr ) ==
           ( pending ? 0 : 1 ) );
    owner = overmap_buffer.entity_at( source );
    REQUIRE( owner != nullptr );
    if( pending ) {
        CHECK( owner->destination == prior_destination );
        CHECK( owner->tracking_intensity == prior_tracking );
        CHECK( owner->monster_data->wander_pos == sound );
        CHECK( owner->monster_data->wandf == budget );
        CHECK( owner->monster_data->provocative_sound );
        CHECK_FALSE( owner->monster_data->writhing_stalker_state().has_light_observed_position );
        CHECK( owner->monster_data->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    } else {
        CHECK( owner->destination == light );
        CHECK_FALSE( owner->monster_data->provocative_sound );
        CHECK( owner->monster_data->writhing_stalker_state().light_observed_position == light );
    }
    set_time( calendar::turn + 1_turns );
    overmap_buffer.move_hordes();
    monster *local = nullptr;
    for( monster &candidate : g->all_monsters() ) {
        if( candidate.is_caol_predator() && candidate.predator_state().actor_id == actor ) {
            REQUIRE( local == nullptr );
            local = &candidate;
        }
    }
    REQUIRE( local != nullptr );
    CHECK( rl_dist( local->pos_abs(), pending ? sound : light ) < 6 );
    CHECK( local->provocative_sound == pending );
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_arrived_sound_settlement_respects_zero_active_time",
           "[writhing_stalker][predator][pursuit][signal]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const auto position = here.get_abs( tripoint_bub_ms( 65, 65, 0 ) );
    const auto bytes = []( const monster &body ) {
        std::ostringstream out;
        { JsonOut json( out ); body.serialize( json ); }
        return out.str();
    };
    monster unowned = new_stalker_fixture_body();
    unowned.set_pos_abs_only( position );
    const auto unowned_before = bytes( unowned );
    CHECK_FALSE( overmap_buffer.predator_activity_for( unowned,
                 predator_activation::representation::local, position ) );
    unowned.hear_sound( here.get_bub( position ), 30, 0, true );
    CHECK_FALSE( unowned.provocative_sound );
    CHECK( bytes( unowned ) == unowned_before );
    monster &original = spawn_new_stalker_fixture( here.get_bub( position ) );
    original.hear_sound( here.get_bub( position ), 30, 0, true );
    REQUIRE( original.provocative_sound );
    const int budget = original.wandf;
    REQUIRE( budget > 0 );
    horde_entity refused( original );
    const auto refused_before = bytes( *refused.monster_data );
    REQUIRE( refused.advance_predator_intent( position, 1000, 0 ) );
    CHECK( bytes( *refused.monster_data ) == refused_before );
    const auto actor = original.predator_state().actor_id;
    REQUIRE( g->despawn_monster( original ) );
    auto *accepted = overmap_buffer.entity_at( position );
    REQUIRE( accepted != nullptr );
    overmap &om = overmap_buffer.get( project_to<coords::om>( position ).xy() );
    on_out_of_scope release_native_fixture( [&] { om.release_monster_payload( position, actor ); } );
    horde_entity &owner = *accepted;
    // The transfer already owns this world turn. Zero may not duplicate its
    // opportunity; a newly adopted eligible callback is covered separately.
    REQUIRE( owner.advance_predator_intent( position, 1000, 0 ) );
    CHECK( owner.monster_data->provocative_sound );
    CHECK( owner.monster_data->wandf == budget );
    CHECK( owner.tracking_intensity == budget );
    set_time( calendar::turn + 1_turns );
    int elapsed = 0;
    REQUIRE( overmap_buffer.predator_activity_for( *owner.monster_data,
             predator_activation::representation::abstract, position, &elapsed ) );
    REQUIRE( elapsed == 1 );
    REQUIRE( owner.advance_predator_intent( position, 1001, elapsed ) );
    CHECK_FALSE( owner.monster_data->provocative_sound );
    CHECK( owner.monster_data->wandf == 0 );
    CHECK( owner.tracking_intensity == 0 );
    clear_map_without_vision();
}

TEST_CASE( "stalker_native_arrived_retreat_unload_keeps_pending_sound",
           "[writhing_stalker][predator][pursuit][scheduler][signal][recovery]" )
{
    clear_map_without_vision();
    clear_avatar();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1_days );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, center );
    refresh_writhing_stalker_arena( here );
    monster &original = spawn_new_stalker_fixture( center + point::east * 3 );
    original.anger = 100;
    original.aggro_character = true;
    // Exhausted budget is a fixture; actual native planning owns withdrawal,
    // its destination and recovery deadline. No phase is assigned here.
    original.writhing_stalker_state().attempts_spent = 2;
    original.plan();
    REQUIRE( original.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::retreating );
    const auto goal = original.writhing_stalker_state().retreat_waypoint;
    const int deadline = original.writhing_stalker_state().cooldown_until_turn;
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    refresh_writhing_stalker_arena( here );
    const bool arrived = true;
    if( arrived ) {
        for( int turn = 0; turn < 40 && original.pos_abs() != goal; ++turn ) {
            original.plan();
            original.set_moves( 100 );
            original.move();
            set_time( calendar::turn + 1_turns );
        }
        REQUIRE( original.pos_abs() == goal );
        original.plan();
        REQUIRE( original.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::cooldown );
    }
    // Remove recent direct evidence as an alternative rejection cause, while
    // retaining the original native recovery deadline.
    set_time( calendar::turn + 201_turns );
    original.plan();
    REQUIRE_FALSE( original.writhing_stalker_state().has_direct_evidence( to_turn<int>( calendar::turn ) ) );
    REQUIRE( original.writhing_stalker_state().cooldown_active( to_turn<int>( calendar::turn ) ) );
    REQUIRE( original.writhing_stalker_state().cooldown_until_turn == deadline );
    const auto position = original.pos_abs();
    const auto sound = position + point::south * 6;
    original.hear_sound( here.get_bub( sound ), 30, 6, true );
    REQUIRE( original.provocative_sound );
    const int budget = original.wandf;
    REQUIRE( budget > 3 );
    const auto actor = original.predator_state().actor_id;
    const auto epoch = original.predator_state().handoff_epoch;
    const bool reload = GENERATE( false, true );
    if( reload ) {
        original = round_trip_writhing_stalker( original );
    }
    overmap &om = overmap_buffer.get( project_to<coords::om>( position ).xy() );
    om.hordes.clear();
    g->despawn_monster( original );
    for( int tick = 0; tick < 3; ++tick ) {
        set_time( calendar::turn + 1_turns );
        overmap_buffer.move_hordes();
        auto *owner = overmap_buffer.entity_at( position );
        REQUIRE( owner != nullptr );
        CHECK( owner->monster_data->predator_state().actor_id == actor );
        CHECK( owner->monster_data->predator_state().handoff_epoch == epoch + 1 );
        CHECK( owner->tracking_intensity == 0 );
        CHECK_FALSE( owner->monster_data->has_dest() );
        CHECK( owner->monster_data->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::cooldown );
        CHECK( owner->monster_data->wander_pos == sound );
        CHECK( owner->monster_data->wandf == budget );
        CHECK( owner->monster_data->provocative_sound );
        if( reload ) {
            *owner->monster_data = round_trip_writhing_stalker( *owner->monster_data );
            om.hordes.refresh_entity_bucket( position );
        }
        overmap_buffer.move_hordes();
        CHECK( overmap_buffer.entity_at( position )->monster_data->wandf == budget );
    }
    set_time( calendar::turn_zero + time_duration::from_turns( deadline + 1 ) );
    overmap_buffer.move_hordes();
    monster *local = nullptr;
    for( monster &candidate : g->all_monsters() ) {
        if( candidate.is_caol_predator() && candidate.predator_state().actor_id == actor ) {
            REQUIRE( local == nullptr );
            local = &candidate;
        }
    }
    REQUIRE( local != nullptr );
    CHECK( local->predator_state().handoff_epoch == epoch + 2 );
    CHECK( rl_dist( local->pos_abs(), sound ) < rl_dist( position, sound ) );
    CHECK( local->provocative_sound );
    CHECK( local->wander_pos == sound );
    CHECK( local->writhing_stalker_state().cooldown_until_turn == -1 );
    CHECK( local->writhing_stalker_state().attempts_spent == 0 );
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_abstract_retreat_arrival_retires_only_its_own_carrier",
           "[writhing_stalker][predator][pursuit][scheduler][signal][recovery]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const auto source = here.get_abs( tripoint_bub_ms( -36, 65, 0 ) );
    const auto retreat = source + point::east;
    const int sound_kind = GENERATE( 0, 1, 2 );
    const auto sound = sound_kind == 2 ? retreat : source + point::south * 6;
    overmap &om = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    om.hordes.clear();
    for( int x = -12; x <= 12; ++x ) {
        for( int y = -12; y <= 12; ++y ) {
            overmap_buffer.set_passable( source + point( x, y ), true );
        }
    }
    monster original = new_stalker_fixture_body();
    original.set_pos_abs_only( source );
    original.set_moves( 100 );
    auto &state = original.writhing_stalker_state();
    state.advance_to( 100 );
    REQUIRE( state.observe_target( 77, 1, source + point::west * 3, 100 ) );
    state.advance_to( 1000 );
    // Accepted local retreat payload fixture; the native caller is exercised
    // separately. This case measures the real unloaded scheduler arrival.
    state.phase = writhing_stalker::lifecycle_phase::retreating;
    state.retreat_waypoint = retreat;
    state.has_retreat_waypoint = true;
    state.cooldown_until_turn = 1060;
    state.attempts_spent = 2;
    original.set_dest( retreat );
    const auto actor = original.predator_state().actor_id;
    REQUIRE( om.hordes.spawn_entity( source, original ) );
    if( sound_kind != 0 ) {
        // Deliver through the genuine accepted abstract owner. This is an
        // abstract sound order, not physical off-bubble local hearing proof.
        // Native set_dest starts this carrier at interest 1000; the signal
        // must actually exceed that interest rather than be silently refused.
        om.hordes.signal_entities( sound, 100 );
    }
    auto *prepared = overmap_buffer.entity_at( source );
    REQUIRE( prepared != nullptr );
    REQUIRE( prepared->monster_data );
    if( sound_kind != 0 ) {
        REQUIRE( prepared->monster_data->provocative_sound );
    }
    CHECK( prepared->monster_data->provocative_sound == ( sound_kind != 0 ) );
    const int budget = prepared->monster_data->wandf;
    overmap_buffer.move_hordes();
    auto *owner = overmap_buffer.entity_at( retreat );
    REQUIRE( owner != nullptr );
    REQUIRE( owner->tracking_intensity == 0 );
    const bool reload = GENERATE( false, true );
    for( int tick = 0; tick < 3; ++tick ) {
        if( reload ) {
            *owner->monster_data = round_trip_writhing_stalker( *owner->monster_data );
            om.hordes.refresh_entity_bucket( retreat );
        }
        set_time( calendar::turn + 1_turns );
        overmap_buffer.move_hordes();
        owner = overmap_buffer.entity_at( retreat );
        REQUIRE( owner != nullptr );
        CHECK( owner->monster_data->predator_state().actor_id == actor );
        CHECK( owner->monster_data->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::cooldown );
        CHECK( owner->tracking_intensity == 0 );
        CHECK_FALSE( owner->monster_data->has_dest() );
        CHECK( owner->monster_data->provocative_sound == ( sound_kind == 1 ) );
        CHECK( owner->monster_data->wandf == ( sound_kind == 1 ? budget - 1 : 0 ) );
        if( sound_kind == 1 ) {
            CHECK( owner->monster_data->wander_pos == sound );
        }
        overmap_buffer.move_hordes();
        CHECK( overmap_buffer.entity_at( retreat )->monster_data->wandf ==
               ( sound_kind == 1 ? budget - 1 : 0 ) );
    }
    if( sound_kind == 1 ) {
        set_time( calendar::turn_zero + 1061_turns );
        overmap_buffer.move_hordes();
        owner = nullptr;
        for( const auto &entry : om.horde_entities() ) {
            if( entry.second->monster_data &&
                entry.second->monster_data->predator_state().actor_id == actor ) {
                REQUIRE( owner == nullptr );
                owner = entry.second;
                CHECK( rl_dist( entry.first, sound ) < rl_dist( retreat, sound ) );
            }
        }
        REQUIRE( owner != nullptr );
        CHECK( owner->destination == sound );
        CHECK( owner->tracking_intensity > 0 );
        CHECK( owner->monster_data->provocative_sound );
        CHECK( owner->monster_data->writhing_stalker_state().attempts_spent == 0 );
    }
    om.hordes.clear();
    clear_map_without_vision();
}

TEST_CASE( "stalker_local_sound_arrival_cannot_reactivate_during_speculative_navigation",
           "[writhing_stalker][predator][pursuit][signal][routing][recovery]" )
{
    clear_map_and_put_player_underground();
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( calendar::turn_zero + 1000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_player_character().setpos( here, tripoint_bub_ms( 65, 65, -2 ) );
    get_scent().reset();
    refresh_writhing_stalker_arena( here );
    monster &stalker = spawn_new_stalker_fixture( center );
    stalker.anger = 100;
    const auto desired = stalker.pos_abs() + point::west * 24;
    const auto sound = stalker.pos_abs() + point::east * 6;
    const int mode = GENERATE( 0, 1, 2 );
    auto &state = stalker.writhing_stalker_state();
    state.advance_to( 1000 );
    if( mode < 2 ) {
        // Accepted sight fixture, not a new acquisition. Subsequent movement
        // has neither current native sight nor a scent route to the avatar.
        REQUIRE( state.observe_target( 77, 1, desired, 1000 ) );
        if( mode == 1 ) {
            state.finish_search();
        }
    } else {
        REQUIRE( state.observe_light_interest( desired, "local-sound-arrival-light", 1000, 300 ) );
    }
    REQUIRE_FALSE( writhing_stalker::can_observe_actor( stalker, get_player_character() ) );
    REQUIRE( stalker.scent_move().x() == -1 );
    stalker.hear_sound( here.get_bub( sound ), 2000, 6, true );
    REQUIRE( stalker.provocative_sound );
    for( int turn = 0; turn < 40 && stalker.pos_abs() != sound; ++turn ) {
        stalker.plan();
        stalker.set_moves( 100 );
        stalker.move();
        set_time( calendar::turn + 1_turns );
    }
    REQUIRE( stalker.pos_abs() == sound );
    REQUIRE( stalker.wandf > 0 );
    const bool reload = GENERATE( false, true );
    if( reload ) {
        stalker = round_trip_writhing_stalker( stalker );
    }
    if( mode == 1 ) {
        set_time( calendar::turn_zero + time_duration::from_turns(
                      1000 + stalker.writhing_stalker_state().familiar[0].revisit_at_active + 1 ) );
    }
    const auto evidence = stalker.writhing_stalker_state().evidence_turn;
    bool left_source = false;
    for( int turn = 0; turn < 12; ++turn ) {
        stalker.plan();
        CHECK_FALSE( stalker.provocative_sound );
        if( reload && turn == 6 ) {
            stalker = round_trip_writhing_stalker( stalker );
        }
        stalker.set_moves( 100 );
        stalker.move();
        set_time( calendar::turn + 1_turns );
        if( stalker.pos_abs() != sound ) {
            left_source = true;
        }
        if( left_source ) {
            CHECK( stalker.pos_abs() != sound );
        }
        CHECK( stalker.writhing_stalker_state().evidence_turn == evidence );
    }
    CHECK( rl_dist( stalker.pos_abs(), desired ) < rl_dist( sound, desired ) - 4 );
    // A genuinely new event may reuse the completed coordinate. It owns a
    // new native budget and must still be followed above speculative work.
    stalker.hear_sound( here.get_bub( sound ), 2000,
                        rl_dist( stalker.pos_abs(), sound ), true );
    REQUIRE( stalker.provocative_sound );
    for( int turn = 0; turn < 60 && stalker.pos_abs() != sound; ++turn ) {
        stalker.plan();
        REQUIRE( stalker.provocative_sound );
        stalker.set_moves( 100 );
        stalker.move();
        set_time( calendar::turn + 1_turns );
    }
    REQUIRE( stalker.pos_abs() == sound );
    stalker.plan();
    CHECK_FALSE( stalker.provocative_sound );
    if( mode == 2 ) {
        for( int turn = 0; turn < 100 && stalker.pos_abs() != desired; ++turn ) {
            stalker.plan();
            stalker.set_moves( 100 );
            stalker.move();
            set_time( calendar::turn + 1_turns );
        }
        REQUIRE( stalker.pos_abs() == desired );
        stalker.plan();
        REQUIRE( stalker.writhing_stalker_state().search_until_turn >= 0 );
        set_time( calendar::turn_zero + time_duration::from_turns(
                      stalker.writhing_stalker_state().search_until_turn + 1 ) );
    } else {
        set_time( calendar::turn + ( mode == 0 ? 31_minutes : 11_minutes ) );
    }
    stalker.plan();
    CHECK( stalker.writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
    CHECK_FALSE( stalker.has_dest() );
    CHECK_FALSE( stalker.provocative_sound );
    CHECK( stalker.wandf == 0 );
    CHECK( stalker.writhing_stalker_state().evidence_turn == evidence );
    clear_map_without_vision();
}

TEST_CASE( "stalker_materialized_expired_light_retains_then_settles_only_earned_route",
           "[writhing_stalker][predator_activation][native][light][intent_owner]" )
{
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    restore_on_out_of_scope restore_time( calendar::turn );
    const auto old_avatar = get_avatar().pos_abs();
    on_out_of_scope cleanup( [&] { clear_map_without_vision(); get_avatar().setpos( old_avatar, false ); } );
    clear_map_without_vision();
    set_time( calendar::turn_zero + 5000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_avatar().setpos( here, tripoint_bub_ms( center.xy(), -2 ) );
    monster &first = spawn_new_stalker_fixture( center );
    const auto position = first.pos_abs();
    const auto actor = first.predator_state().actor_id;
    const auto source = position + point::east * 6;
    REQUIRE( overmap_buffer.predator_activity_for( first, predator_activation::representation::local, position ) );
    auto &state = first.writhing_stalker_state();
    REQUIRE( state.observe_light_interest( source, "retained-materialized-light", 5000, 60 ) );
    overmap_buffer.retain_predator_light_work( first, "retained-materialized-light", 5000, 60 );
    const auto phase = state.phase;
    const bool arrival = GENERATE( false, true );
    REQUIRE( g->despawn_monster( first ) );
    overmap &owner = overmap_buffer.get( project_to<coords::om>( position ).xy() );
    on_out_of_scope release( [&] { owner.release_monster_payload( position, actor ); } );
    auto *abstract = overmap_buffer.entity_at( position );
    REQUIRE( abstract );
    get_avatar().setpos( project_to<coords::ms>( project_to<coords::omt>( position ) - point( 41, 0 ) ), false );
    CHECK_FALSE( overmap_buffer.predator_activity_for( *abstract->monster_data, predator_activation::representation::abstract, position ) );
    const auto bytes = []( const monster &body ) { std::ostringstream out; { JsonOut json( out ); body.serialize( json ); } return out.str(); };
    const auto frozen = bytes( *abstract->monster_data );
    calendar::turn += 1000_turns;
    CHECK_FALSE( overmap_buffer.predator_activity_for( *abstract->monster_data, predator_activation::representation::abstract, position ) );
    CHECK( bytes( *abstract->monster_data ) == frozen );
    *abstract->monster_data = round_trip_writhing_stalker( *abstract->monster_data );
    CHECK( bytes( *abstract->monster_data ) == frozen );
    get_avatar().setpos( here, tripoint_bub_ms( center.xy(), -2 ) );
    REQUIRE( overmap_buffer.predator_activity_for( *abstract->monster_data, predator_activation::representation::abstract, position ) );
    overmap_buffer.spawn_monster( project_to<coords::sm>( position ), false );
    REQUIRE( overmap_buffer.entity_at( position ) == nullptr );
    auto restored = get_creature_tracker().find( position );
    REQUIRE( restored );
    writhing_stalker::advance_active_work( *restored, 6000 );
    CHECK_FALSE( restored->writhing_stalker_state().light_interest_active( 6000 ) );
    CHECK_FALSE( restored->writhing_stalker_state().has_light_observed_position );
    CHECK( restored->writhing_stalker_state().has_committed_waypoint );
    CHECK( restored->writhing_stalker_state().phase == phase );
    CHECK( restored->writhing_stalker_state().light_observed_turn == 5000 );
    CHECK_FALSE( restored->writhing_stalker_state().observe_light_interest( source, "retained-materialized-light", 5000, 60 ) );
    if( arrival ) {
        // Real native relocation, no pose-only hop. Arrival is a remembered
        // place observation, not restored current optical evidence.
        restored->set_moves( 1000 );
        for( int step = 1; step <= 6; ++step ) {
            REQUIRE( restored->move_to( center + point::east * step, false ) );
        }
        CHECK( restored->pos_abs() == source );
        CHECK( restored->get_moves() < 1000 );
        CHECK( restored->writhing_stalker_state().arrive_at( restored->pos_abs() ) );
        CHECK( restored->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::searching );
        CHECK_FALSE( restored->writhing_stalker_state().has_committed_waypoint );
        const auto deadline = restored->writhing_stalker_state().search_until_turn;
        CHECK_FALSE( restored->writhing_stalker_state().arrive_at( restored->pos_abs() ) );
        CHECK( restored->writhing_stalker_state().search_until_turn == deadline );
    } else {
        calendar::turn += 60_turns;
        REQUIRE( overmap_buffer.predator_activity_for( *restored, predator_activation::representation::local, restored->pos_abs() ) );
        writhing_stalker::advance_active_work( *restored, 6060 );
        CHECK_FALSE( overmap_buffer.predator_light_work_alive( *restored ) );
        CHECK_FALSE( restored->writhing_stalker_state().has_committed_waypoint );
        CHECK( restored->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
        const auto settled = bytes( *restored );
        writhing_stalker::advance_active_work( *restored, 6060 );
        CHECK( bytes( *restored ) == settled );
    }
}

TEST_CASE( "stalker_capture_suspends_only_owned_progress_timeout",
           "[writhing_stalker][predator_activation][native][capture][progress]" )
{
    restore_on_out_of_scope restore_time( calendar::turn );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    clear_map_and_put_player_underground();
    on_out_of_scope cleanup( [] { clear_map_without_vision(); } );
    set_time( calendar::turn_zero + 810000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    get_avatar().setpos( here, tripoint_bub_ms( center.xy(), -2 ) );
    monster &first = spawn_new_stalker_fixture( center );
    const auto id = first.predator_state().actor_id;
    const auto source = first.pos_abs() + point::east * 6;
    REQUIRE( overmap_buffer.predator_activity_for( first, predator_activation::representation::local, first.pos_abs() ) );
    // Accepted typed-light setup, not an optical-acquisition claim.
    REQUIRE( first.writhing_stalker_state().observe_light_interest( source, "pause-progress", 810000, 5000 ) );
    overmap_buffer.retain_predator_light_work( first, "pause-progress", 810000, 5000 );
    first.plan();
    REQUIRE( first.has_dest() );
    const auto anchor = first.writhing_stalker_state().last_progress_turn;
    REQUIRE( anchor == 810000 );
    const auto observed = first.writhing_stalker_state().light_observed_turn;
    item carrier( itype_id( "pet_carrier" ), calendar::turn );
    carrier.contain_monster( center );
    REQUIRE( carrier.has_var( "contained_json" ) );
    calendar::turn += 1000_turns;
    REQUIRE( carrier.release_monster( center ) );
    auto restored = get_creature_tracker().find( here.get_abs( center ) );
    REQUIRE( restored );
    REQUIRE( restored->predator_state().actor_id == id );
    CHECK( restored->writhing_stalker_state().last_progress_turn == anchor + 1000 );
    restored->plan();
    CHECK( restored->writhing_stalker_state().phase != writhing_stalker::lifecycle_phase::searching );
    CHECK( restored->has_dest() );
    CHECK( restored->writhing_stalker_state().light_observed_turn == observed );
    calendar::turn += 51_turns; // real active no-progress time still expires
    restored->plan();
    CHECK( restored->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::searching );
    CHECK( restored->writhing_stalker_state().light_observed_turn == observed );
}

TEST_CASE( "stalker_retained_light_executes_successive_native_short_steps",
           "[writhing_stalker][predator_activation][native][capture][light][journey]" )
{
    restore_on_out_of_scope restore_time( calendar::turn );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    clear_map_and_put_player_underground();
    on_out_of_scope cleanup( [] { clear_map_without_vision(); } );
    set_time( calendar::turn_zero + 820000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_writhing_stalker_arena( here, center );
    here.ter_set( center + point::east * 4, ter_id( "t_wall" ) );
    refresh_writhing_stalker_arena( here );
    get_avatar().setpos( here, tripoint_bub_ms( center.xy(), -2 ) );
    get_scent().reset();
    monster &first = spawn_new_stalker_fixture( center );
    const auto id = first.predator_state().actor_id;
    const auto source = first.pos_abs() + point::east * 24;
    REQUIRE( overmap_buffer.predator_activity_for( first, predator_activation::representation::local, first.pos_abs() ) );
    REQUIRE( first.writhing_stalker_state().observe_light_interest( source, "short-step-journey", 820000, 40 ) );
    overmap_buffer.retain_predator_light_work( first, "short-step-journey", 820000, 40 );
    first.plan();
    REQUIRE( first.has_dest() );
    REQUIRE( first.get_dest() != source );
    const auto original_prefix = first.get_dest();
    item carrier( itype_id( "pet_carrier" ), calendar::turn );
    carrier.contain_monster( center );
    REQUIRE( carrier.has_var( "contained_json" ) );
    calendar::turn += 45_turns; // world evidence expired; below the independent progress timeout
    REQUIRE( carrier.release_monster( center ) );
    auto restored = get_creature_tracker().find( here.get_abs( center ) );
    REQUIRE( restored );
    REQUIRE( restored->predator_state().actor_id == id );
    bool advanced_beyond_prefix = false;
    for( int step = 0; step < 39 && restored->pos_abs() != source; ++step ) {
        calendar::turn += 1_turns;
        restored->process_turn();
        restored->plan();
        CHECK_FALSE( restored->writhing_stalker_state().light_interest_active( to_turn<int>( calendar::turn ) ) );
        CHECK( restored->writhing_stalker_state().light_observed_turn == 820000 );
        if( restored->get_moves() > 0 ) {
            restored->move();
        }
        advanced_beyond_prefix = advanced_beyond_prefix ||
                                 rl_dist( restored->pos_abs(), source ) < rl_dist( original_prefix, source );
    }
    CHECK( advanced_beyond_prefix );
    CHECK( restored->pos_abs() == source );
    restored->plan();
    CHECK( restored->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::searching );
    CHECK_FALSE( restored->writhing_stalker_state().has_light_observed_position );
    CHECK( restored->writhing_stalker_state().light_observed_turn == 820000 );
}

TEST_CASE( "stalker_abstract_expired_light_keeps_arrival_and_finite_search_scheduled",
           "[writhing_stalker][predator_activation][native][light][scheduler]" )
{
    restore_on_out_of_scope restore_time( calendar::turn );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    const auto old_avatar = get_avatar().pos_abs();
    clear_map_and_put_player_underground();
    set_time( calendar::turn_zero + 830000_turns );
    const auto start = get_map().get_abs( tripoint_bub_ms( -36, 65, 0 ) );
    const auto light = start + point::east * 3;
    const auto observer = get_avatar().pos_abs();
    overmap &om = overmap_buffer.get( project_to<coords::om>( start ).xy() );
    om.hordes.clear();
    on_out_of_scope cleanup( [&] {
        om.hordes.clear();
        clear_map_without_vision();
        get_avatar().setpos( old_avatar, false );
    } );
    for( int x = -5; x <= 8; ++x ) {
        for( int y = -5; y <= 5; ++y ) {
            overmap_buffer.set_passable( start + point( x, y ), true );
        }
    }
    monster born = new_stalker_fixture_body();
    born.set_pos_abs_only( start );
    born.set_moves( 0 );
    const auto actor = born.predator_state().actor_id;
    REQUIRE( om.hordes.spawn_entity( start, born ) );
    // An accepted typed-light packet is setup, not a physical ray claim.
    REQUIRE( om.hordes.attract_entities_to_light( light, 60, 20, "abstract-earned-arrival",
             calendar::turn, []( const tripoint_abs_ms & ) { return true; }, nullptr ) == 1 );
    overmap_buffer.move_hordes(); // actual zero-credit birth opportunity
    auto *owner = overmap_buffer.entity_at( start );
    REQUIRE( owner );
    REQUIRE_FALSE( owner->monster_data->writhing_stalker_state().has_pursuit_memory );
    CHECK( owner->moves == 0 );
    CHECK( overmap_buffer.predator_active_time( *owner->monster_data ) == 0 );
    const auto bytes = []( const monster &body ) {
        std::ostringstream out;
        { JsonOut json( out ); body.serialize( json ); }
        return out.str();
    };
    get_avatar().setpos( project_to<coords::ms>( project_to<coords::omt>( start ) + point( 41, 0 ) ), false );
    CHECK_FALSE( overmap_buffer.predator_activity_for( *owner->monster_data,
                 predator_activation::representation::abstract, start ) );
    const auto frozen = bytes( *owner->monster_data );
    calendar::turn += 100_turns; // source freshness ends, earned work does not
    overmap_buffer.move_hordes();
    REQUIRE( overmap_buffer.entity_at( start ) == owner );
    CHECK( bytes( *owner->monster_data ) == frozen );
    *owner->monster_data = round_trip_writhing_stalker( *owner->monster_data );
    CHECK( bytes( *owner->monster_data ) == frozen );
    CHECK( overmap_buffer.predator_active_time( *owner->monster_data ) == 0 );
    get_avatar().setpos( observer, false );
    overmap_buffer.move_hordes(); // resume settles pause, never funds a step
    REQUIRE( overmap_buffer.entity_at( start ) );
    CHECK( overmap_buffer.entity_at( start )->moves == 0 );
    CHECK( overmap_buffer.predator_active_time( *overmap_buffer.entity_at( start )->monster_data ) == 0 );
    tripoint_abs_ms current = start;
    for( int step = 0; step < 8 && current != light; ++step ) {
        calendar::turn += 1_turns;
        overmap_buffer.move_hordes();
        owner = nullptr;
        for( const auto &entry : om.horde_entities() ) {
            if( entry.second->monster_data && entry.second->monster_data->predator_state().actor_id == actor ) {
                current = entry.first;
                owner = entry.second;
            }
        }
        REQUIRE( owner );
        CHECK_FALSE( owner->monster_data->writhing_stalker_state().light_interest_active(
                         to_turn<int>( calendar::turn ) ) );
        CHECK( owner->monster_data->writhing_stalker_state().light_observed_turn == 830000 );
    }
    REQUIRE( current == light );
    REQUIRE( owner );
    CHECK( owner->tracking_intensity == 0 );
    CHECK( owner->is_active() ); // arrival still needs its next native callback
    const bool sound = GENERATE( false, true );
    if( sound ) {
        om.hordes.signal_entities( light + point::south * 3, 20 );
    }
    calendar::turn += 1_turns;
    overmap_buffer.move_hordes();
    owner = nullptr;
    for( const auto &entry : om.horde_entities() ) {
        if( entry.second->monster_data && entry.second->monster_data->predator_state().actor_id == actor ) {
            current = entry.first;
            owner = entry.second;
        }
    }
    REQUIRE( owner );
    CHECK( owner->monster_data->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::searching );
    CHECK_FALSE( owner->monster_data->writhing_stalker_state().has_light_observed_position );
    const auto deadline = owner->monster_data->writhing_stalker_state().search_until_turn;
    CHECK( deadline == to_turn<int>( calendar::turn ) + 20 );
    if( sound ) {
        CHECK( owner->destination == light + point::south * 3 );
        CHECK( current == light + point::south );
    } else {
        CHECK( current == light );
        CHECK( owner->tracking_intensity == 0 );
    }
    const auto settled = bytes( *owner->monster_data );
    const auto moves = owner->moves;
    overmap_buffer.move_hordes();
    CHECK( bytes( *owner->monster_data ) == settled );
    CHECK( owner->moves == moves );
    if( !sound ) {
        // Reload the actual arrived owner, keeping its finite deadline.
        *owner->monster_data = round_trip_writhing_stalker( *owner->monster_data );
        om.hordes.refresh_entity_bucket( current );
        calendar::turn += 21_turns;
        overmap_buffer.move_hordes();
        owner = overmap_buffer.entity_at( light );
        REQUIRE( owner );
        CHECK( owner->monster_data->writhing_stalker_state().phase == writhing_stalker::lifecycle_phase::idle );
        CHECK_FALSE( owner->monster_data->writhing_stalker_state().has_committed_waypoint );
        CHECK_FALSE( owner->is_active() );
        CHECK( owner->tracking_intensity == 0 );
        const auto retired = bytes( *owner->monster_data );
        calendar::turn += 100_turns;
        overmap_buffer.move_hordes();
        CHECK( bytes( *owner->monster_data ) == retired );
    }
}
