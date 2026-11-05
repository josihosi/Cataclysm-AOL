#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "creature_tracker.h"
#include "do_turn.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "character.h"
#include "city.h"
#include "debug.h"
#include "effect.h"
#include "game.h"
#include "json.h"
#include "json_loader.h"
#include "item.h"
#include "live_light.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "mattack_actors.h"
#include "mongroup.h"
#include "migo_tower_native.h"
#include "monster.h"
#include "mtype.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "path_info.h"
#include "pathfinding.h"
#include "player_helpers.h"
#include "rng.h"
#include "trap.h"
#include "type_id.h"
#include "vehicle.h"
#include "worldfactory.h"
#include "zombie_rider_overmap_ai.h"

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

static const efftype_id effect_run( "run" );
static const efftype_id effect_downed( "downed" );
static const itype_id zombie_rider_tainted_bone_arrow( "zombie_rider_tainted_bone_arrow" );
static const mongroup_id GROUP_DEBUG_ZOMBIE_RIDER( "GROUP_DEBUG_ZOMBIE_RIDER" );
static const mongroup_id GROUP_ZOMBIE( "GROUP_ZOMBIE" );
static const mongroup_id GROUP_ZOMBIE_PREDATOR_UPGRADE( "GROUP_ZOMBIE_PREDATOR_UPGRADE" );
static const mongroup_id GROUP_ZOMBIE_UPGRADE( "GROUP_ZOMBIE_UPGRADE" );
static const mtype_id mon_zombie( "mon_zombie" );
static const mtype_id mon_zombie_hunter( "mon_zombie_hunter" );
static const mtype_id mon_zombie_predator( "mon_zombie_predator" );
static const mtype_id mon_zombie_rider( "mon_zombie_rider" );
static const species_id species_HUMAN( "HUMAN" );
static const species_id species_ZOMBIE( "ZOMBIE" );
static const skill_id skill_dodge( "dodge" );
static const damage_type_id damage_bash( "bash" );
static const vproto_id vehicle_prototype_none( "none" );
static const vproto_id vehicle_prototype_test_shopping_cart( "test_shopping_cart" );
static const std::string zombie_rider_bone_bow_shot( "zombie_rider_bone_bow_shot" );


static void refresh_pathing_cache( map &here )
{
    g->reset_light_level();
    here.invalidate_visibility_cache();
    here.update_visibility_cache( 0 );
    here.invalidate_map_cache( 0 );
    here.set_transparency_cache_dirty( 0 );
    here.set_pathfinding_cache_dirty( 0 );
    here.build_map_cache( 0 );
}

static void prepare_zombie_rider_local_arena( map &here, const tripoint_bub_ms &center )
{
    clear_avatar();
    const ter_id t_floor( "t_floor" );
    for( const tripoint_bub_ms &p : here.points_in_radius( center, 14 ) ) {
        here.ter_set( p, t_floor );
        here.furn_clear( p );
        here.clear_fields( p );
    }
    refresh_pathing_cache( here );
}

static monster round_trip_zombie_rider( const monster &rider )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    rider.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    monster loaded;
    loaded.deserialize( jsin.get_object() );
    return loaded;
}

// The lifecycle additions are optional continuity metadata.  Retarget the
// three keys to unknown legacy diagnostics so the loader sees each expected
// member as omitted while every ordinary monster and pursuit member remains
// valid serialized JSON.
static monster round_trip_zombie_rider_without_neutral_predator_metadata( const monster &rider )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    rider.serialize( jsout );

    std::string legacy = out.str();
    for( const std::string member : {
             "band_revision_cache", "band_reference",
             "ammo_initialization_version"
         } ) {
        const std::size_t member_pos = legacy.find( "\"" + member + "\"" );
        REQUIRE( member_pos != std::string::npos );
        legacy.replace( member_pos + 1, member.size(), "legacy_" + member );
    }

    JsonValue jsin = json_loader::from_string( legacy );
    monster loaded;
    loaded.deserialize( jsin.get_object() );
    return loaded;
}

TEST_CASE( "zombie_rider_monster_footing", "[zombie_rider][monster]" )
{
    const mtype &rider = *mon_zombie_rider;
    const std::string exact_description =
        "A towering corpse rides a six-legged, horse-sized tangle of muscle and chitin.  Blood-red "
        "eyes peer over a bow of wet bone and sinew as the mount's feet hammer across the ground.";

    CHECK( rider.nname() == "zombie rider" );
    CHECK( rider.get_description() == exact_description );
    CHECK( rider.in_species( species_ZOMBIE ) );
    CHECK_FALSE( rider.in_species( species_HUMAN ) );
    CHECK( rider.size > creature_size::medium );
    CHECK( rider.speed >= 150 );
    CHECK( rider.speed <= 200 );
    CHECK( rider.hp >= 200 );
    CHECK( rider.has_special_attack( "zombie_rider_bone_bow_shot" ) );
    CHECK( rider.starting_ammo.count( zombie_rider_tainted_bone_arrow ) == 1 );
    CHECK( rider.starting_ammo.at( zombie_rider_tainted_bone_arrow ) >= 12 );
    CHECK( rider.has_flag( mon_flag_RANGED_ATTACKER ) );
    CHECK( rider.has_flag( mon_flag_PATH_AVOID_DANGER ) );
    CHECK( rider.path_settings.avoid_dangerous_fields );
    CHECK( rider.has_flag( mon_flag_HARDTOSHOOT ) );
    CHECK_FALSE( rider.has_flag( mon_flag_HIT_AND_RUN ) );
    CHECK_FALSE( rider.has_flag( mon_flag_BASHES ) );
    CHECK_FALSE( rider.has_flag( mon_flag_GROUP_BASH ) );
    CHECK_FALSE( rider.has_flag( mon_flag_UNBREAKABLE_MORALE ) );
    CHECK_FALSE( rider.upgrades );
}

TEST_CASE( "zombie_rider_poly_initializes_the_durable_predator_identity",
           "[zombie_rider][monster][persistence]" )
{
    monster predator( mon_zombie_predator );
    REQUIRE_FALSE( predator.is_caol_predator() );

    predator.poly( mon_zombie_rider );

    REQUIRE( predator.is_caol_predator() );
    CHECK_FALSE( predator.predator_state().actor_id.empty() );
}

TEST_CASE( "zombie_rider_ammunition_is_initialized_once_at_creation_or_transition",
           "[zombie_rider][monster][ammo][evolution]" )
{
    monster direct( mon_zombie_rider );
    REQUIRE( direct.predator_state().ammo_initialization_version == 1 );
    REQUIRE( direct.ammo[zombie_rider_tainted_bone_arrow] ==
             direct.type->starting_ammo.at( zombie_rider_tainted_bone_arrow ) );

    direct.ammo[zombie_rider_tainted_bone_arrow] = 0;
    direct.poly( mon_zombie_rider );
    CHECK( direct.ammo[zombie_rider_tainted_bone_arrow] == 0 );

    monster transition( mon_zombie_predator );
    transition.poly( mon_zombie_rider );
    const int configured = transition.type->starting_ammo.at( zombie_rider_tainted_bone_arrow );
    CHECK( transition.predator_state().ammo_initialization_version == 1 );
    CHECK( transition.ammo[zombie_rider_tainted_bone_arrow] == configured );

    transition.ammo[zombie_rider_tainted_bone_arrow] = 3;
    transition.poly( mon_zombie_rider );
    CHECK( transition.ammo[zombie_rider_tainted_bone_arrow] == 3 );
}

TEST_CASE( "zombie_rider_large_body_small_passage_pathing", "[zombie_rider][monster][map]" )
{
    clear_map_without_vision();
    map &here = get_map();
    const ter_id t_floor( "t_floor" );
    const ter_id t_wall( "t_wall" );
    const ter_id t_window_empty( "t_window_empty" );
    const tripoint_bub_ms top_left{ 4, 4, 0 };
    const tripoint_bub_ms bottom_right{ 11, 8, 0 };
    const tripoint_bub_ms rider_start{ 5, 5, 0 };
    const tripoint_bub_ms window_passage{ 6, 5, 0 };
    const tripoint_bub_ms normal_floor{ 5, 6, 0 };
    const tripoint_bub_ms target{ 10, 5, 0 };

    for( const tripoint_bub_ms &p : here.points_in_rectangle( top_left, bottom_right ) ) {
        here.ter_set( p, t_wall );
        here.furn_clear( p );
        here.clear_fields( p );
    }
    const std::vector<tripoint_bub_ms> open_route = {
        rider_start, window_passage, { 7, 5, 0 }, { 8, 5, 0 }, { 9, 5, 0 }, target,
        normal_floor, { 5, 7, 0 }, { 6, 7, 0 }, { 7, 7, 0 }, { 8, 7, 0 }, { 9, 7, 0 },
        { 10, 7, 0 }, { 10, 6, 0 }
    };
    for( const tripoint_bub_ms &p : open_route ) {
        here.ter_set( p, t_floor );
    }
    here.ter_set( window_passage, t_window_empty );
    refresh_pathing_cache( here );

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
    monster ordinary_zombie( mon_zombie, rider_start );
    Character &you = get_player_character();
    you.setpos( here, target );
    rider.zombie_rider_pursuit_state().observe( you.get_identity(), you.pos_abs(),
            to_turn<int>( calendar::turn ) );
    rider.set_moves( 1000 );
    CHECK_FALSE( rider.move_to( window_passage, false ) );
    CHECK_FALSE( rider.zombie_rider_pursuit_state().impact_ready );

    REQUIRE( here.passable( window_passage ) );
    REQUIRE( here.has_flag_ter( ter_furn_flag::TFLAG_SMALL_PASSAGE, window_passage ) );
    REQUIRE( rider.get_size() > creature_size::medium );
    CHECK( ordinary_zombie.will_move_to( window_passage ) );
    CHECK_FALSE( rider.will_move_to( window_passage ) );
    CHECK( rider.will_move_to( normal_floor ) );

    pathfinding_settings normal_sized_settings( {}, 20, 100, 0, false, false, false, true,
            false, false, ordinary_zombie.get_size() );
    const std::vector<tripoint_bub_ms> normal_path = here.route( rider_start,
    pathfinding_target::point( target ), normal_sized_settings, []( const tripoint_bub_ms & ) {
        return false;
    } );
    REQUIRE_FALSE( normal_path.empty() );
    CHECK( normal_path.back() == target );
    CHECK( std::find( normal_path.begin(), normal_path.end(), window_passage ) != normal_path.end() );

    pathfinding_settings rider_sized_settings( {}, 20, 100, 0, false, false, false, true,
            false, false, rider.get_size() );
    const std::vector<tripoint_bub_ms> rider_path = here.route( rider_start,
    pathfinding_target::point( target ), rider_sized_settings, []( const tripoint_bub_ms & ) {
        return false;
    } );
    REQUIRE_FALSE( rider_path.empty() );
    CHECK( rider_path.back() == target );
    CHECK( std::find( rider_path.begin(), rider_path.end(), window_passage ) == rider_path.end() );
    CHECK( std::any_of( rider_path.begin(), rider_path.end(), []( const tripoint_bub_ms & p ) {
        return p.y() != 5;
    } ) );
    for( const tripoint_bub_ms &step : rider_path ) {
        CHECK( rider.will_move_to( step ) );
    }
    clear_map_without_vision();
}

// Bounded read-only attribution at actual callers. Do not settle an interval
// merely to observe it, and never ask predator_state on an ordinary predator.
static std::string rider_native_causal_snapshot( const monster &body )
{
    std::ostringstream out;
    out << "world=" << to_turn<int>( calendar::turn ) << " type=" << body.type->id.str()
        << " pose=" << body.pos_abs().to_string() << " tracker="
        << ( get_creature_tracker().find( body.pos_abs() ).get() == &body )
        << " can_upgrade=" << body.can_upgrade() << " upgrade=" << body.get_upgrade_time()
        << " moves=" << body.get_moves();
    std::ostringstream encoded;
    { JsonOut json( encoded ); body.serialize( json ); }
    const auto value = json_loader::from_string( encoded.str() );
    const auto json = value.get_object();
    json.allow_omitted_members();
    time_duration suspended = 0_turns;
    json.read( "suspended_evolution_time", suspended );
    out << " suspended_evolution=" << to_turns<int>( suspended );
    if( json.has_object( "special_attacks" ) ) {
        auto specials = json.get_object( "special_attacks" );
        specials.allow_omitted_members();
        if( specials.has_object( "zombie_rider_bone_bow_shot" ) ) {
            auto bow = specials.get_object( "zombie_rider_bone_bow_shot" );
            bow.allow_omitted_members();
            out << " bow_cooldown=" << bow.get_int( "cooldown" );
        }
    }
    if( body.is_caol_predator() ) {
        const auto &life = body.predator_state();
        out << " actor=" << life.actor_id << " epoch=" << life.handoff_epoch
            << " native_process=" << life.last_native_process_world;
        if( const auto *record = overmap_buffer.global_state.predator_activity.find( life.actor_id ) ) {
            out << " owner=" << static_cast<int>( record->owner )
                << " record_epoch=" << record->epoch
                << " confirmed=" << record->confirmed << " prior_pose="
                << ( record->position ? record->position->to_string() : "absent" )
                << " adopted=" << record->activity.adopted_world
                << " last_world=" << record->activity.last_world
                << " active=" << record->activity.active
                << " delta=" << record->activity.active_delta;
        } else { out << " record=absent"; }
    }
    return out.str();
}

static monster new_scheduler_predator( const mtype_id &type );

// Only genuinely NEW local fixtures use this adapter. Canonical records are
// retained across tracker cleanup, so a repeated constructor token is not a
// new birth. Bounded native retries consume real RNG, never rewrite identity.
static monster &spawn_new_local_rider_fixture( const tripoint_bub_ms &start,
        const bool death_drops = true )
{
    REQUIRE( get_creature_tracker().creature_at( start ) == nullptr );
    monster probe( mon_zombie_rider );
    map &here = get_map();
    CAPTURE( here.ter( start ), here.furn( start ), here.tr_at( start ), here.move_cost( start ) );
    REQUIRE( probe.will_move_to( start ) );
    REQUIRE( probe.know_danger_at( start ) );
    auto candidate = make_shared_fast<monster>( new_scheduler_predator( mon_zombie_rider ) );
    candidate->ammo = candidate->type->starting_ammo;
    monster *placed = g->place_critter_at( candidate, start );
    REQUIRE( placed );
    REQUIRE( placed == candidate.get() );
    placed->death_drops = death_drops;
    return *placed;
}

TEST_CASE( "zombie_rider_local_bow_shot_sets_cooldown_and_repositions",
           "[zombie_rider][monster][ai]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms rider_start = center + point::east * 6;
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );
    you.set_all_parts_hp_to_max();

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_special( zombie_rider_bone_bow_shot, 0 );
    rider.set_moves( 100 );

    REQUIRE( rider.sees( here, you ) );
    const int distance_before = rl_dist( rider.pos_bub(), you.pos_bub() );
    rider.plan();
    CHECK( rider.get_dest() == you.pos_abs() );

    const int ammo_before = rider.ammo[zombie_rider_tainted_bone_arrow];
    REQUIRE( rider.special_available( zombie_rider_bone_bow_shot ) );
    rider.move();

    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before - 1 );
    CHECK_FALSE( rider.has_effect( effect_run ) );
    CHECK_FALSE( rider.special_available( zombie_rider_bone_bow_shot ) );
    CHECK( rider.get_dest() == you.pos_abs() );
    CHECK( rl_dist( rider.pos_bub(), you.pos_bub() ) <= distance_before );

    const auto native_body = [&]() {
        std::ostringstream out;
        { JsonOut json( out ); rider.serialize( json ); }
        return out.str();
    };
    const auto before_cooldown = native_body();
    for( int turn = 0; turn < 4; ++turn ) {
        rider.process_turn();
        CHECK( native_body() == before_cooldown );
    }
    CHECK_FALSE( rider.special_available( zombie_rider_bone_bow_shot ) );
    for( int turn = 0; turn < 4; ++turn ) {
        calendar::turn += 1_turns;
        rider.process_turn();
        CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before - 1 );
        CHECK( rider.special_available( zombie_rider_bone_bow_shot ) == ( turn == 3 ) );
        const auto once = native_body();
        rider.process_turn();
        CHECK( native_body() == once );
    }
    CHECK( rider.special_available( zombie_rider_bone_bow_shot ) );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_bow_pressure_marks_avatar_hostile_before_shooting",
           "[zombie_rider][monster][ai]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms rider_start = center + point::east * 6;
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );
    you.set_all_parts_hp_to_max();

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
    rider.anger = 100;
    rider.aggro_character = false;
    rider.set_special( zombie_rider_bone_bow_shot, 0 );
    rider.set_moves( 100 );

    REQUIRE( rider.sees( here, you ) );
    rider.plan();
    CHECK( rider.get_dest() == you.pos_abs() );
    CHECK( rider.aggro_character );

    const int ammo_before = rider.ammo[zombie_rider_tainted_bone_arrow];
    REQUIRE( rider.special_available( zombie_rider_bone_bow_shot ) );
    rider.move();

    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before - 1 );
    CHECK_FALSE( rider.special_available( zombie_rider_bone_bow_shot ) );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_close_pressure_bunny_hops_without_point_blank_bow_shot",
           "[zombie_rider][monster][ai]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms rider_start = center + point::east * 3;
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );

    monster &rider = spawn_new_local_rider_fixture( rider_start );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_special( zombie_rider_bone_bow_shot, 0 );
    rider.set_special( "bite", 10 );
    rider.set_moves( 100 );

    REQUIRE( rider.sees( here, you ) );
    rider.plan();
    const tripoint_bub_ms planned_dest = here.get_bub( rider.get_dest() );
    const int destination_distance = rl_dist( planned_dest, you.pos_bub() );
    CHECK( rider.get_dest() == you.pos_abs() );
    CHECK( destination_distance == 0 );

    const int ammo_before = rider.ammo[zombie_rider_tainted_bone_arrow];
    for( int moves = 0; moves < 2 && rider.pos_bub() == rider_start; ++moves ) {
        rider.set_moves( 100 );
        rider.move();
    }

    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before );
    CHECK( rider.pos_bub() != rider_start );
    CHECK( rl_dist( here.get_bub( rider.get_dest() ), you.pos_bub() ) == 0 );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_empty_bow_charges_instead_of_kiting_forever",
           "[zombie_rider][monster][ai]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms rider_start = center + point::east * 6;
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );

    monster &rider = spawn_new_local_rider_fixture( rider_start );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.ammo[zombie_rider_tainted_bone_arrow] = 0;
    rider.set_special( zombie_rider_bone_bow_shot, 0 );
    rider.set_moves( 100 );

    REQUIRE( rider.sees( here, you ) );
    const int distance_before = rl_dist( rider.pos_bub(), you.pos_bub() );
    rider.plan();

    CHECK( rider.get_dest() == you.pos_abs() );
    for( int moves = 0; moves < 2 && rider.pos_bub() == rider_start; ++moves ) {
        rider.set_moves( 100 );
        rider.move();
    }

    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == 0 );
    CHECK( rider.pos_bub() != rider_start );
    CHECK( rl_dist( rider.pos_bub(), you.pos_bub() ) < distance_before );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_failed_pressure_annulus_keeps_pursuing_visible_target",
           "[zombie_rider][monster][ai][map]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms rider_start = center + point::east * 8;
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );

    // The pressure planner may only choose an unoccupied square 3..6 tiles
    // from the target and at most 6 tiles from the rider. Occupy every square
    // in that annulus with ordinary zombies. Creatures do not obstruct LOS,
    // so the rider still has a valid visible target.
    for( const tripoint_bub_ms &p : here.points_in_radius( center, 6 ) ) {
        const int target_distance = rl_dist( p, center );
        if( target_distance >= 3 && target_distance <= 6 && p != rider_start ) {
            spawn_test_monster( mon_zombie.str(), p );
        }
    }
    refresh_pathing_cache( here );

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_special( zombie_rider_bone_bow_shot, 10 );
    rider.set_moves( 100 );

    REQUIRE( rider.sees( here, you ) );
    const int distance_before = rl_dist( rider.pos_bub(), you.pos_bub() );
    rider.plan();

    CHECK( rider.get_dest() == you.pos_abs() );
    CHECK( rl_dist( here.get_bub( rider.get_dest() ), you.pos_bub() ) == 0 );
    rider.move();
    CHECK( rl_dist( rider.pos_bub(), you.pos_bub() ) < distance_before );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_remembers_last_observation_routes_then_expires_search",
           "[zombie_rider][monster][ai][pursuit]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms rider_start = center + point::east * 8;
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    // Keep the target on the prepared floor and use same-level occlusion to
    // exercise remembered pursuit without triggering falling movement.
    here.ter_set( center + point::east * 4, ter_id( "t_wall" ) );
    refresh_pathing_cache( here );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );

    monster &rider = spawn_new_local_rider_fixture( rider_start );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_moves( 100 );
    auto &state = rider.zombie_rider_pursuit_state();
    state.observe( you.get_identity(), here.get_abs( center ), to_turn<int>( calendar::turn ) );
    // Move the player into a small sealed, same-level enclosure well away
    // from the observed tile.  The center and its approach remain traversable.
    const tripoint_bub_ms enclosure{ 57, 57, 0 };
    const ter_id t_wall( "t_wall" );
    for( int dx = -1; dx <= 1; ++dx ) {
        for( int dy = -1; dy <= 1; ++dy ) {
            if( std::abs( dx ) == 1 || std::abs( dy ) == 1 ) {
                here.ter_set( enclosure + point{ dx, dy }, t_wall );
            }
        }
    }
    you.setpos( here, enclosure );
    refresh_pathing_cache( here );

    REQUIRE_FALSE( rider.sees( here, you ) );
    rider.plan();
    CHECK( here.get_bub( rider.get_dest() ) == center );
    for( int steps = 0; steps < 12 && rl_dist( rider.pos_bub(), center ) > 1; ++steps ) {
        rider.set_moves( 100 );
        rider.move();
    }
    CHECK( rl_dist( rider.pos_bub(), center ) <= 2 );
    rider.plan();
    CHECK( state.phase == zombie_rider_overmap_ai::pursuit_phase::searching );

    calendar::turn += 21_turns;
    rider.plan();
    CHECK( state.phase == zombie_rider_overmap_ai::pursuit_phase::idle );
    CHECK_FALSE( state.evidence_valid( to_turn<int>( calendar::turn ) ) );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_pursuit_state_round_trips_typed_observation_and_waypoint",
           "[zombie_rider][monster][save][pursuit]" )
{
    monster rider( mon_zombie_rider );
    const tripoint_abs_ms observed{ 120, 130, 0 };
    const tripoint_abs_ms waypoint{ 118, 130, 0 };
    rider.zombie_rider_pursuit_state().observe( 42, observed, 77 );
    rider.zombie_rider_pursuit_state().evidence_source_actor_id = "direct-rider";
    rider.zombie_rider_pursuit_state().evidence_provenance = "direct_rider_sight";
    rider.zombie_rider_pursuit_state().movement_waypoint = waypoint;
    rider.zombie_rider_pursuit_state().impact_ready = true;
    rider.zombie_rider_pursuit_state().impact_ready_turn = 78;
    rider.set_special( "zombie_rider_impact", 7 );
    rider.predator_state().ammo_initialization_version = 3;
    rider.predator_state().band_reference = "rider-band:opaque-17";
    rider.predator_state().band_revision_cache = 9;
    rider.ammo[zombie_rider_tainted_bone_arrow] = 0;

    const monster loaded = round_trip_zombie_rider( rider );
    const auto &state = loaded.zombie_rider_pursuit_state();
    CHECK( state.phase == zombie_rider_overmap_ai::pursuit_phase::pursuing );
    CHECK( state.target_identity == 42 );
    CHECK( state.last_observed_position == observed );
    CHECK( state.last_observed_turn == 77 );
    CHECK( state.evidence_source_actor_id == "direct-rider" );
    CHECK( state.evidence_provenance == "direct_rider_sight" );
    CHECK( state.movement_waypoint == waypoint );
    CHECK( state.has_movement_waypoint );
    CHECK( state.impact_ready );
    CHECK( state.impact_ready_turn == 78 );
    CHECK( loaded.predator_state().ammo_initialization_version == 3 );
    CHECK( loaded.predator_state().band_reference == "rider-band:opaque-17" );
    CHECK( loaded.predator_state().band_revision_cache == 9 );
    const auto loaded_ammo = loaded.ammo.find( zombie_rider_tainted_bone_arrow );
    REQUIRE( loaded_ammo != loaded.ammo.end() );
    CHECK( loaded_ammo->second == 0 );
    CHECK_FALSE( loaded.special_available( "zombie_rider_impact" ) );

    const monster legacy = round_trip_zombie_rider_without_neutral_predator_metadata( rider );
    CHECK( legacy.predator_state().ammo_initialization_version == 0 );
    CHECK( legacy.predator_state().band_reference.empty() );
    CHECK( legacy.predator_state().band_revision_cache == 0 );
}

TEST_CASE( "zombie_rider_pursuit_state_expires_stale_active_evidence",
           "[zombie_rider][save][pursuit]" )
{
    zombie_rider_overmap_ai::rider_pursuit_state state;
    state.observe( 42, tripoint_abs_ms{ 120, 130, 0 }, 77 );
    state.advance_to( 278 );
    CHECK( state.phase == zombie_rider_overmap_ai::pursuit_phase::idle );
    CHECK_FALSE( state.evidence_valid( 278 ) );
    CHECK_FALSE( state.has_last_observed_position );
    CHECK_FALSE( state.has_movement_waypoint );
}

TEST_CASE( "zombie_rider_relay_preserves_source_timestamp_and_bounded_evidence",
           "[zombie_rider][overmap][band][pursuit]" )
{
    using namespace zombie_rider_overmap_ai;
    rider_band_observation shared;
    shared.target_identity = 42;
    shared.position = tripoint_abs_ms( 120, 140, 0 );
    shared.observed_turn = 30;
    shared.expires_at_turn = 230;
    shared.source_actor_id = "direct-rider";
    shared.provenance = "relayed_direct_rider_sight";
    rider_pursuit_state recipient;
    recipient.relay( shared );
    CHECK( recipient.phase == pursuit_phase::pursuing );
    CHECK( recipient.target_identity == shared.target_identity );
    CHECK( recipient.last_observed_position == shared.position );
    CHECK( recipient.last_observed_turn == shared.observed_turn );
    CHECK( recipient.evidence_source_actor_id == "direct-rider" );
    CHECK( recipient.evidence_provenance == "relayed_direct_rider_sight" );
    recipient.advance_to( 231 );
    CHECK_FALSE( recipient.has_last_observed_position );
}

TEST_CASE( "zombie_rider_impact_requires_closing_step_and_consumes_once",
           "[zombie_rider][monster][ai][impact]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );

    monster &rider = spawn_new_local_rider_fixture( center + point::east * 3 );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_special( "zombie_rider_impact", 0 );
    rider.zombie_rider_pursuit_state().observe( you.get_identity(), you.pos_abs(),
            to_turn<int>( calendar::turn ) );
    rider.set_dest( you.pos_abs() );
    you.set_dodges_left( 0 );
    you.set_free_dodges_left( 0 );
    you.set_stamina( 0 );

    CHECK_FALSE( rider.zombie_rider_pursuit_state().impact_ready );
    for( int steps = 0; steps < 3 && !rider.zombie_rider_pursuit_state().impact_ready; ++steps ) {
        rider.set_moves( 100 );
        rider.move();
    }
    CHECK( rider.pos_bub() != center + point::east * 3 );
    CHECK( rider.zombie_rider_pursuit_state().impact_ready );

    const int hp_before = you.get_hp();
    rider.set_moves( 100 );
    rider.move();
    CHECK( you.get_hp() <= hp_before );
    CHECK_FALSE( rider.zombie_rider_pursuit_state().impact_ready );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_impact_focused_control_matrix",
           "[zombie_rider][monster][impact][controls]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );

    monster &rider = spawn_new_local_rider_fixture( center + point::east * 3 );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_dest( you.pos_abs() );
    auto &state = rider.zombie_rider_pursuit_state();
    state.observe( you.get_identity(), you.pos_abs(), to_turn<int>( calendar::turn ) );

    // Setup/teleport, stationary adjacency, and a sideways step do not arm.
    rider.setpos( here, center + point::east );
    CHECK_FALSE( state.impact_ready );
    rider.setpos( here, center + point::north );
    CHECK_FALSE( state.impact_ready );
    rider.setpos( here, center + point::north * 2 );
    CHECK_FALSE( state.impact_ready );

    // Only the real one-tile closing step arms the token.
    rider.set_moves( 1000 );
    CHECK( rider.move_to( center + point::north, false ) );
    CHECK( state.impact_ready );

    const mtype_special_attack &impact = rider.type->special_attacks.at( "zombie_rider_impact" );
    const int moves_before = rider.get_moves();
    state.target_identity = you.get_identity() + 1;
    CHECK_FALSE( impact->call( rider ) );
    CHECK( state.impact_ready );
    CHECK( rider.get_moves() == moves_before );

    state.target_identity = you.get_identity();
    rider.anger = 0;
    CHECK_FALSE( impact->call( rider ) );
    CHECK( state.impact_ready );
    CHECK( rider.get_moves() == moves_before );

    rider.anger = 100;
    rider.setpos( here, center + point::north );
    state.impact_ready = true;
    state.impact_ready_turn = to_turn<int>( calendar::turn );
    const int level_moves_before = rider.get_moves();
    rider.setpos( here, tripoint_bub_ms{ center.x(), center.y(), 1 } );
    CHECK_FALSE( impact->call( rider ) );
    CHECK( state.impact_ready );
    CHECK( rider.get_moves() == level_moves_before );

    // Restore a visible, hostile, same-level contact.  A valid miss still
    // consumes the token and pays native melee move cost.
    rider.setpos( here, center + point::north );
    state.impact_ready = true;
    state.impact_ready_turn = to_turn<int>( calendar::turn );
    you.set_dodges_left( 0 );
    you.set_free_dodges_left( 0 );
    you.set_stamina( 0 );
    rider.set_moves( 100 );
    CHECK( impact->call( rider ) );
    CHECK_FALSE( state.impact_ready );
    CHECK( rider.get_moves() == -10 );
    CHECK_FALSE( impact->call( rider ) );

    // An already-downed target remains damage-eligible, but its existing
    // downed effect is not refreshed by the impact.
    rider.set_moves( 100 );
    state.impact_ready = true;
    state.impact_ready_turn = to_turn<int>( calendar::turn );
    you.add_effect( effect_downed, 1_turns, true );
    CHECK( impact->call( rider ) );
    CHECK_FALSE( state.impact_ready );
    CHECK( you.has_effect( effect_downed ) );
    CHECK( rider.get_moves() == -10 );

    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_impact_blocked_physical_step_does_not_arm",
           "[zombie_rider][monster][impact][controls][blocked]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    here.ter_set( center + point::east, ter_id( "t_wall" ) );
    refresh_pathing_cache( here );

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), center + point::east * 2 );
    auto &state = rider.zombie_rider_pursuit_state();
    state.observe( you.get_identity(), you.pos_abs(), to_turn<int>( calendar::turn ) );
    rider.set_moves( 1000 );
    CHECK_FALSE( rider.move_to( center + point::east, false ) );
    CHECK( rider.pos_bub() == center + point::east * 2 );
    CHECK_FALSE( state.impact_ready );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_impact_vehicle_teleport_does_not_arm",
           "[zombie_rider][monster][impact][controls][vehicle]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );

    const tripoint_bub_ms vehicle_pos = center + point::east;
    vehicle *const cart = here.add_vehicle( vehicle_prototype_none,
                                            vehicle_pos, 0_degrees, 0, veh_spawn_status::UNDAMAGED );
    REQUIRE( cart != nullptr );

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), center + point::east * 2 );
    auto &state = rider.zombie_rider_pursuit_state();
    state.observe( you.get_identity(), you.pos_abs(), to_turn<int>( calendar::turn ) );
    rider.set_moves( 1000 );
    // Vehicle occupancy is not a substitute for a physical approach: a
    // forced relocation onto the occupied tile must not arm impact.
    CHECK( rider.move_to( vehicle_pos, true ) );
    CHECK( rider.pos_bub() == vehicle_pos );
    CHECK_FALSE( state.impact_ready );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_impact_scheduler_has_one_native_attack_and_cooldown",
           "[zombie_rider][monster][impact][scheduler]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    you.set_dodges_left( 0 );
    you.set_free_dodges_left( 0 );
    you.set_stamina( 0 );

    monster &rider = spawn_new_local_rider_fixture( center + point::north );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_dest( you.pos_abs() );
    rider.zombie_rider_pursuit_state().target_identity = you.get_identity();
    rider.zombie_rider_pursuit_state().impact_ready = true;
    rider.zombie_rider_pursuit_state().impact_ready_turn = to_turn<int>( calendar::turn );
    rider.set_special( "zombie_rider_impact", 0 );
    // Both contact attacks can be ready in live play.  The named impact must
    // arbitrate this closing turn instead of losing it to the generic map's
    // earlier bite key.
    rider.set_special( "bite", 0 );
    rider.set_moves( 100 );

    // Exercise monster::move's scheduler, not the actor directly.  The
    // impact consumes exactly its 110 moves and returns before bite/ordinary
    // strike scheduling can run in the same invocation.
    rider.move();
    CHECK_FALSE( rider.zombie_rider_pursuit_state().impact_ready );
    CHECK( rider.zombie_rider_pursuit_state().impact_recovery_until_turn ==
           to_turn<int>( calendar::turn ) + 1 );
    CHECK( rider.get_moves() == -10 );
    CHECK_FALSE( rider.special_available( "zombie_rider_impact" ) );
    CHECK( rider.special_available( "bite" ) );

    // Standing adjacent during cooldown does not re-arm or strike again.
    rider.set_moves( 100 );
    rider.move();
    CHECK_FALSE( rider.zombie_rider_pursuit_state().impact_ready );
    CHECK( rider.get_moves() > -100 );

    // Cooldown expiry alone is insufficient: a fresh physical approach is
    // required before the next impact can arm.
    rider.set_special( "zombie_rider_impact", 0 );
    rider.set_moves( 1000 );
    CHECK( rider.move_to( center + point::east * 2, false ) );
    CHECK_FALSE( rider.zombie_rider_pursuit_state().impact_ready );
    rider.set_moves( 1000 );
    CHECK( rider.move_to( center + point::east, false ) );
    CHECK( rider.zombie_rider_pursuit_state().impact_ready );

    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_impact_readiness_survives_planner_observation_refresh",
           "[zombie_rider][monster][impact][scheduler]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    you.set_dodges_left( 0 );
    you.set_free_dodges_left( 0 );
    you.set_stamina( 0 );

    monster &rider = spawn_new_local_rider_fixture( center + point::east * 2 );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_dest( you.pos_abs() );
    rider.set_special( "zombie_rider_impact", 0 );
    auto &state = rider.zombie_rider_pursuit_state();
    state.observe( you.get_identity(), you.pos_abs(), to_turn<int>( calendar::turn ) );

    rider.set_moves( 1000 );
    REQUIRE( rider.move_to( center + point::east, false ) );
    REQUIRE( state.impact_ready );

    // monster::plan() refreshes the same target observation before the next
    // scheduler invocation; that refresh must not erase the physical token.
    rider.plan();
    CHECK( state.impact_ready );
    rider.set_moves( 100 );
    rider.move();
    CHECK_FALSE( state.impact_ready );
    CHECK( rider.get_moves() == -10 );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_impact_native_miss_consumes_readiness_and_cost",
           "[zombie_rider][monster][impact][miss]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    you.set_skill_level( skill_dodge, 100 );
    you.set_dodges_left( 1 );
    you.set_free_dodges_left( 0 );

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), center + point::north );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_dest( you.pos_abs() );
    auto &state = rider.zombie_rider_pursuit_state();
    state.target_identity = you.get_identity();
    state.impact_ready = true;
    const auto &special = rider.type->special_attacks.at( "zombie_rider_impact" );
    auto &actor = const_cast<melee_actor &>( dynamic_cast<const melee_actor &>( *special ) );
    const int old_accuracy = actor.accuracy;
    // A strongly negative special accuracy makes this a deterministic native
    // dodge branch instead of depending on a single random hit roll.
    actor.accuracy = -100;
    const int hp_before = you.get_hp();
    rider.set_moves( 100 );
    CHECK( special->call( rider ) );
    CHECK( you.get_hp() == hp_before );
    CHECK_FALSE( state.impact_ready );
    CHECK( rider.get_moves() == -10 );
    CHECK( state.impact_recovery_until_turn == to_turn<int>( calendar::turn ) + 1 );
    actor.accuracy = old_accuracy;
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_impact_native_armor_absorption_is_no_damage",
           "[zombie_rider][monster][impact][armor]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    item jacket( itype_id( "jacket_eod" ) );
    jacket.put_in( item( itype_id( "heavy_steel_ballistic_plate" ) ), pocket_type::CONTAINER );
    you.wear_item( jacket, false );
    you.set_skill_level( skill_dodge, 0 );
    you.set_dodges_left( 1 );
    you.set_free_dodges_left( 0 );

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), center + point::north );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_dest( you.pos_abs() );
    auto &state = rider.zombie_rider_pursuit_state();
    state.target_identity = you.get_identity();
    state.impact_ready = true;
    const auto &special = rider.type->special_attacks.at( "zombie_rider_impact" );
    auto &actor = const_cast<melee_actor &>( dynamic_cast<const melee_actor &>( *special ) );
    const damage_instance old_damage = actor.damage_max_instance;
    const float old_min = actor.min_mul;
    const float old_max = actor.max_mul;
    const int old_accuracy = actor.accuracy;
    actor.damage_max_instance = damage_instance( damage_bash, 0 );
    actor.min_mul = actor.max_mul = 1.0f;
    actor.accuracy = 100;
    const int hp_before = you.get_hp();
    CHECK( you.get_armor_type( damage_bash, bodypart_id( "torso" ) ) > 0 );
    rider.set_moves( 100 );
    CHECK( special->call( rider ) );
    CHECK( you.get_hp() == hp_before );
    CHECK_FALSE( state.impact_ready );
    actor.damage_max_instance = old_damage;
    actor.min_mul = old_min;
    actor.max_mul = old_max;
    actor.accuracy = old_accuracy;
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_close_indoor_pressure_repositions_instead_of_loitering",
           "[zombie_rider][monster][ai][map]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const ter_id t_floor( "t_floor" );
    const ter_id t_wall( "t_wall" );
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms rider_start = center + point::east * 3;

    prepare_zombie_rider_local_arena( here, center );
    for( const tripoint_bub_ms &p : here.points_in_rectangle( center + point( -6, -4 ),
            center + point( 6, 4 ) ) ) {
        here.ter_set( p, t_wall );
    }
    for( const tripoint_bub_ms &p : here.points_in_rectangle( center + point( -5, -3 ),
            center + point( 5, 3 ) ) ) {
        here.ter_set( p, t_floor );
    }
    refresh_pathing_cache( here );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );

    monster &rider = spawn_new_local_rider_fixture( rider_start );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_special( zombie_rider_bone_bow_shot, 0 );
    rider.set_special( "bite", 10 );
    rider.set_moves( 100 );

    REQUIRE( rider.sees( here, you ) );
    const int ammo_before = rider.ammo[zombie_rider_tainted_bone_arrow];
    rider.plan();
    const tripoint_bub_ms planned_dest = here.get_bub( rider.get_dest() );

    CHECK( planned_dest != rider_start );
    CHECK( rider.get_dest() == you.pos_abs() );
    CHECK( planned_dest == you.pos_bub() );

    for( int moves = 0; moves < 2 && rider.pos_bub() == rider_start; ++moves ) {
        rider.set_moves( 100 );
        rider.move();
    }

    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before );
    CHECK( rider.pos_bub() != rider_start );
    CHECK( rl_dist( rider.pos_bub(), you.pos_bub() ) < 3 );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_injured_continues_pursuit_instead_of_withdrawing",
           "[zombie_rider][monster][ai]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms rider_start = center + point::east * 8;
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );

    monster &rider = spawn_new_local_rider_fixture( rider_start );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_hp( rider.get_hp_max() / 2 );
    rider.set_special( zombie_rider_bone_bow_shot, 0 );
    rider.set_moves( 100 );

    REQUIRE( rider.sees( here, you ) );
    const int distance_before = rl_dist( rider.pos_bub(), you.pos_bub() );
    rider.plan();
    CHECK( rider.get_dest() == you.pos_abs() );

    const int ammo_before = rider.ammo[zombie_rider_tainted_bone_arrow];
    rider.move();
    if( rl_dist( rider.pos_bub(), you.pos_bub() ) == distance_before ) {
        rider.set_moves( 100 );
        rider.move();
    }

    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before - 1 );
    CHECK( rider.pos_bub() != rider_start );
    CHECK( rl_dist( rider.pos_bub(), you.pos_bub() ) < distance_before );
    CHECK( rl_dist( here.get_bub( rider.get_dest() ), you.pos_bub() ) == 0 );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_camp_pressure_intent_drives_live_planning",
           "[zombie_rider][monster][ai][camp_light]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms rider_start = center + point::east * 8;
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );

    monster &rider = spawn_new_local_rider_fixture( rider_start );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_moves( 100 );
    const tripoint_abs_ms camp_source = here.get_abs( center );

    SECTION( "circle_harass_survives_visible_target_planning_and_moves_laterally" ) {
        rider.set_special( zombie_rider_bone_bow_shot, 10 );
        zombie_rider_overmap_ai::set_camp_pressure_intent(
            rider, zombie_rider_overmap_ai::rider_camp_pressure_posture::circle_harass,
            camp_source, 30, 0 );

        rider.plan();
        const tripoint_bub_ms planned_dest = here.get_bub( rider.get_dest() );
        const point rider_relative( rider_start.x() - center.x(), rider_start.y() - center.y() );
        const point destination_relative( planned_dest.x() - center.x(),
                                          planned_dest.y() - center.y() );
        CHECK( rider.get_dest() != camp_source );
        CHECK( std::abs( rider_relative.x * destination_relative.y -
                         rider_relative.y * destination_relative.x ) > 0 );

        const int ammo_before = rider.ammo[zombie_rider_tainted_bone_arrow];
        for( int moves = 0; moves < 2 && rider.pos_bub() == rider_start; ++moves ) {
            rider.set_moves( 100 );
            rider.move();
        }
        CHECK( rider.pos_bub() != rider_start );
        CHECK( rider.pos_bub().y() != rider_start.y() );
        CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before );
        CHECK( zombie_rider_overmap_ai::get_camp_pressure_intent( rider ).has_value() );
    }

    SECTION( "circle_harass_with_an_empty_bow_does_not_charge_the_camp" ) {
        rider.ammo[zombie_rider_tainted_bone_arrow] = 0;
        rider.set_special( zombie_rider_bone_bow_shot, 0 );
        zombie_rider_overmap_ai::set_camp_pressure_intent(
            rider, zombie_rider_overmap_ai::rider_camp_pressure_posture::circle_harass,
            camp_source, 30, 0 );

        rider.plan();
        const tripoint_bub_ms planned_dest = here.get_bub( rider.get_dest() );
        const point rider_relative( rider_start.x() - center.x(), rider_start.y() - center.y() );
        const point destination_relative( planned_dest.x() - center.x(),
                                          planned_dest.y() - center.y() );
        CHECK( rider.get_dest() != you.pos_abs() );
        CHECK( std::abs( rider_relative.x * destination_relative.y -
                         rider_relative.y * destination_relative.x ) > 0 );

        for( int moves = 0; moves < 2 && rider.pos_bub() == rider_start; ++moves ) {
            rider.set_moves( 100 );
            rider.move();
        }
        CHECK( rider.pos_bub() != rider_start );
        CHECK( rider.pos_bub().y() != rider_start.y() );
        CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == 0 );
    }

    SECTION( "circle_harass_rejects_a_locally_open_but_sealed_orbit" ) {
        const ter_id t_wall( "t_wall" );
        const tripoint_bub_ms sealed_orbit = center + point::south * 8;
        for( const tripoint_bub_ms &p : here.points_in_rectangle(
                 sealed_orbit + point( -5, -5 ), sealed_orbit + point( 5, 5 ) ) ) {
            if( std::abs( p.x() - sealed_orbit.x() ) == 5 ||
                std::abs( p.y() - sealed_orbit.y() ) == 5 ) {
                here.ter_set( p, t_wall );
            }
        }
        refresh_pathing_cache( here );
        rider.set_special( zombie_rider_bone_bow_shot, 10 );
        zombie_rider_overmap_ai::set_camp_pressure_intent(
            rider, zombie_rider_overmap_ai::rider_camp_pressure_posture::circle_harass,
            camp_source, 30, 0 );

        REQUIRE( rider.sees( here, you ) );
        rider.plan();
        const tripoint_bub_ms planned_dest = here.get_bub( rider.get_dest() );
        CHECK( planned_dest.y() < center.y() );
        const std::vector<tripoint_bub_ms> route = here.route(
                    rider, pathfinding_target::point( planned_dest ) );
        REQUIRE_FALSE( route.empty() );
        CHECK( route.back() == planned_dest );
    }

    SECTION( "direct_attack_closes_on_a_visible_defender_during_bow_cooldown" ) {
        rider.set_special( zombie_rider_bone_bow_shot, 10 );
        rider.set_dest( here.get_abs( rider_start + point::north * 8 ) );
        zombie_rider_overmap_ai::set_camp_pressure_intent(
            rider, zombie_rider_overmap_ai::rider_camp_pressure_posture::direct_attack,
            camp_source, 30, 0 );

        const int distance_before = rl_dist( rider.pos_bub(), you.pos_bub() );
        rider.plan();
        CHECK( rider.get_dest() == you.pos_abs() );
        for( int moves = 0; moves < 2 && rider.pos_bub() == rider_start; ++moves ) {
            rider.set_moves( 100 );
            rider.move();
        }
        CHECK( rl_dist( rider.pos_bub(), you.pos_bub() ) < distance_before );
    }

    SECTION( "investigate_stops_short_of_an_unseen_fire_source" ) {
        const tripoint_bub_ms investigate_start = center + point::east * 14;
        const tripoint_bub_ms naive_stand_off = center + point::east * 8;
        rider.setpos( here, investigate_start );
        you.setpos( here, tripoint_bub_ms( center.xy(), 1 ) );
        REQUIRE( here.add_field( center, fd_fire, 3 ) );
        REQUIRE( here.add_field( naive_stand_off, fd_fire, 3 ) );
        refresh_pathing_cache( here );
        zombie_rider_overmap_ai::set_camp_pressure_intent(
            rider, zombie_rider_overmap_ai::rider_camp_pressure_posture::investigate,
            camp_source, 30, 0 );

        REQUIRE_FALSE( rider.sees( here, you ) );
        rider.plan();
        const tripoint_bub_ms planned_dest = here.get_bub( rider.get_dest() );
        CHECK( planned_dest != center );
        CHECK( rl_dist( planned_dest, center ) >= 4 );
        CHECK( here.get_field_intensity( planned_dest, fd_fire ) == 0 );
        CHECK( rider.know_danger_at( planned_dest ) );
        const std::vector<tripoint_bub_ms> route = here.route(
                    rider, pathfinding_target::point( planned_dest ) );
        REQUIRE_FALSE( route.empty() );
        CHECK( route.back() == planned_dest );
        CHECK( std::all_of( route.begin(), route.end(), [&rider]( const tripoint_bub_ms & step ) {
            return rider.know_danger_at( step );
        } ) );

        const int distance_before = rl_dist( rider.pos_bub(), center );
        for( int moves = 0; moves < 2 && rider.pos_bub() == investigate_start; ++moves ) {
            rider.set_moves( 100 );
            rider.move();
        }
        CHECK( rider.pos_bub() != investigate_start );
        CHECK( rider.pos_bub() != center );
        CHECK( rl_dist( rider.pos_bub(), center ) < distance_before );
        CHECK( rl_dist( rider.pos_bub(), center ) >= 4 );
        CHECK( rider.know_danger_at( rider.pos_bub() ) );
        CHECK( here.get_field_intensity( rider.pos_bub(), fd_fire ) == 0 );
    }

    SECTION( "investigate_with_an_empty_bow_does_not_charge_a_visible_defender" ) {
        rider.ammo[zombie_rider_tainted_bone_arrow] = 0;
        rider.set_special( zombie_rider_bone_bow_shot, 0 );
        zombie_rider_overmap_ai::set_camp_pressure_intent(
            rider, zombie_rider_overmap_ai::rider_camp_pressure_posture::investigate,
            camp_source, 30, 0 );

        REQUIRE( rider.sees( here, you ) );
        rider.plan();
        const tripoint_bub_ms planned_dest = here.get_bub( rider.get_dest() );
        CHECK( rider.get_dest() != you.pos_abs() );
        CHECK( planned_dest != center );
        CHECK( rl_dist( planned_dest, center ) >= 4 );

        for( int moves = 0; moves < 2 && rider.pos_bub() == rider_start; ++moves ) {
            rider.set_moves( 100 );
            rider.move();
        }
        CHECK( rider.pos_bub() != rider_start );
        CHECK( rider.pos_bub() != center );
        CHECK( rl_dist( rider.pos_bub(), center ) >= 4 );
        CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == 0 );
    }

    SECTION( "withdraw_overrides_a_ready_final_shot_and_moves_away" ) {
        rider.set_special( zombie_rider_bone_bow_shot, 0 );
        zombie_rider_overmap_ai::set_camp_pressure_intent(
            rider, zombie_rider_overmap_ai::rider_camp_pressure_posture::withdraw,
            camp_source, 30, 0 );

        const int distance_before = rl_dist( rider.pos_bub(), you.pos_bub() );
        const int ammo_before = rider.ammo[zombie_rider_tainted_bone_arrow];
        rider.plan();
        CHECK( rl_dist( here.get_bub( rider.get_dest() ), you.pos_bub() ) > distance_before );
        for( int moves = 0; moves < 2 &&
             rl_dist( rider.pos_bub(), you.pos_bub() ) <= distance_before; ++moves ) {
            rider.set_moves( 100 );
            rider.move();
        }
        CHECK( rl_dist( rider.pos_bub(), you.pos_bub() ) > distance_before );
        CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before );
    }

    SECTION( "intent_survives_save_round_trip_then_expires" ) {
        zombie_rider_overmap_ai::set_camp_pressure_intent(
            rider, zombie_rider_overmap_ai::rider_camp_pressure_posture::circle_harass,
            camp_source, 20, 1 );
        monster loaded = round_trip_zombie_rider( rider );

        const std::optional<zombie_rider_overmap_ai::rider_camp_pressure_intent> loaded_intent =
            zombie_rider_overmap_ai::get_camp_pressure_intent( loaded );
        REQUIRE( loaded_intent.has_value() );
        CHECK( loaded_intent->posture ==
               zombie_rider_overmap_ai::rider_camp_pressure_posture::circle_harass );
        CHECK( loaded_intent->source == camp_source );
        CHECK( loaded_intent->formation_slot == 1 );
        CHECK( loaded_intent->turns_remaining == 20 );

        calendar::turn += 21_turns;
        CHECK_FALSE( zombie_rider_overmap_ai::get_camp_pressure_intent( loaded ).has_value() );
    }

    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_live_plan_is_independent_of_diagnostic_filter",
           "[zombie_rider][monster][map][debug]" )
{
    struct plan_state {
        tripoint_abs_ms destination;
        tripoint_abs_ms wander_destination;
        int wander_range;
        int moves;
        int anger;
        bool aggro_character;
        int ammo;
        bool bow_ready;
        time_point turn;
        bool owns_camp_intent;
        zombie_rider_overmap_ai::rider_camp_pressure_posture camp_posture;
        tripoint_abs_ms camp_source;
        int camp_slot;
        int camp_turns_remaining;
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
        const tripoint_bub_ms rider_start = center + point::east * 8;
        restore_on_out_of_scope restore_calendar_turn( calendar::turn );
        set_time( daylight_time( calendar::turn ) + 2_hours );
        prepare_zombie_rider_local_arena( here, center );
        you.setpos( here, center );

        limitDebugLevel( diagnostics_enabled ? DL_ALL : 0 );
        limitDebugClass( D_GAME );
        monster &rider = spawn_new_local_rider_fixture( rider_start );
        rider.anger = 100;
        rider.aggro_character = true;
        rider.set_moves( 100 );
        rider.set_special( zombie_rider_bone_bow_shot, 10 );
        const tripoint_abs_ms camp_source = here.get_abs( center );
        zombie_rider_overmap_ai::set_camp_pressure_intent(
            rider, zombie_rider_overmap_ai::rider_camp_pressure_posture::circle_harass,
            camp_source, 30, 0 );
        REQUIRE( rider.sees( here, you ) );
        rider.plan();

        const auto intent = zombie_rider_overmap_ai::get_camp_pressure_intent( rider );
        return plan_state{ rider.get_dest(), rider.wander_pos, rider.wandf, rider.get_moves(),
                           rider.anger, rider.aggro_character,
                           rider.ammo[zombie_rider_tainted_bone_arrow],
                           rider.special_available( zombie_rider_bone_bow_shot ), calendar::turn,
                           intent.has_value(),
                           intent ? intent->posture : zombie_rider_overmap_ai::rider_camp_pressure_posture::none,
                           intent ? intent->source : tripoint_abs_ms::zero,
                           intent ? intent->formation_slot : 0,
                           intent ? intent->turns_remaining : 0 };
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
    CHECK( enabled.ammo == disabled.ammo );
    CHECK( enabled.bow_ready == disabled.bow_ready );
    CHECK( enabled.turn == disabled.turn );
    CHECK( enabled.owns_camp_intent == disabled.owns_camp_intent );
    CHECK( enabled.camp_posture == disabled.camp_posture );
    CHECK( enabled.camp_source == disabled.camp_source );
    CHECK( enabled.camp_slot == disabled.camp_slot );
    CHECK( enabled.camp_turns_remaining == disabled.camp_turns_remaining );

    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_distant_camp_investigation_uses_a_reachable_waypoint",
           "[zombie_rider][monster][ai][camp_light]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const ter_id t_floor( "t_floor" );
    const ter_id t_wall( "t_wall" );
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms rider_start = center + point::east * 55;
    const tripoint_bub_ms final_stand_off = center + point::east * 8;

    clear_avatar();
    for( const tripoint_bub_ms &p : here.points_in_rectangle(
             center + point( -2, -5 ), rider_start + point( 2, 5 ) ) ) {
        here.ter_set( p, t_floor );
        here.furn_clear( p );
        here.clear_fields( p );
    }
    for( int y = center.y() - 3; y <= center.y() + 3; ++y ) {
        here.ter_set( tripoint_bub_ms( rider_start.x() - 10, y, 0 ), t_wall );
    }
    refresh_pathing_cache( here );
    you.setpos( here, tripoint_bub_ms( center.xy(), 1 ) );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_moves( 100 );
    zombie_rider_overmap_ai::set_camp_pressure_intent(
        rider, zombie_rider_overmap_ai::rider_camp_pressure_posture::investigate,
        here.get_abs( center ), 30, 0 );

    REQUIRE_FALSE( rider.sees( here, you ) );
    CHECK( rl_dist( rider_start, final_stand_off ) > rider.get_pathfinding_settings().max_dist );
    CHECK( here.route( rider, pathfinding_target::point( final_stand_off ) ).empty() );
    rider.plan();
    const tripoint_bub_ms planned_dest = here.get_bub( rider.get_dest() );
    CHECK( planned_dest != rider_start );
    CHECK( rl_dist( rider_start, planned_dest ) <= rider.get_pathfinding_settings().max_dist );
    CHECK( rl_dist( planned_dest, center ) < rl_dist( rider_start, center ) );
    const std::vector<tripoint_bub_ms> route = here.route(
                rider, pathfinding_target::point( planned_dest ) );
    REQUIRE_FALSE( route.empty() );
    CHECK( route.back() == planned_dest );
    CHECK( std::all_of( route.begin(), route.end(), [&rider]( const tripoint_bub_ms & step ) {
        return rider.know_danger_at( step );
    } ) );

    for( int moves = 0; moves < 2 && rider.pos_bub() == rider_start; ++moves ) {
        rider.set_moves( 100 );
        rider.move();
    }
    CHECK( rider.pos_bub() != rider_start );
    CHECK( rl_dist( rider.pos_bub(), center ) < rl_dist( rider_start, center ) );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_blocked_los_prevents_bow_shot", "[zombie_rider][monster][ai][map]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const ter_id t_wall( "t_wall" );
    const tripoint_bub_ms center{ 65, 65, 0 };
    const tripoint_bub_ms rider_start = center + point::east * 6;
    prepare_zombie_rider_local_arena( here, center );
    for( int x = 1; x < 6; ++x ) {
        here.ter_set( center + point::east * x, t_wall );
    }
    refresh_pathing_cache( here );
    you.setpos( here, center );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    set_time( daylight_time( calendar::turn ) + 2_hours );

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_dest( you.pos_abs() );
    rider.set_special( zombie_rider_bone_bow_shot, 0 );
    rider.set_moves( 100 );

    REQUIRE_FALSE( rider.sees( here, you ) );
    const int ammo_before = rider.ammo[zombie_rider_tainted_bone_arrow];
    rider.move();

    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before );
    CHECK( rider.special_available( zombie_rider_bone_bow_shot ) );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_endpoint_uses_mature_predator_evolution",
           "[zombie_rider][monster][mongroup]" )
{
    const mtype &hunter = *mon_zombie_hunter;
    const mtype &predator = *mon_zombie_predator;

    REQUIRE( hunter.upgrades );
    CHECK( hunter.upgrade_into == mon_zombie_predator );
    REQUIRE( predator.upgrades );
    CHECK( predator.half_life == 168 );
    CHECK( predator.upgrade_group == GROUP_ZOMBIE_PREDATOR_UPGRADE );
    CHECK( predator.upgrade_world_age_gate_seasons ==
           zombie_rider_overmap_ai::mature_world_gate_seasons );
    CHECK( GROUP_ZOMBIE_PREDATOR_UPGRADE->defaultMonster == mon_zombie_predator );
    CHECK( GROUP_ZOMBIE_PREDATOR_UPGRADE->freq_total == 1000 );
    CHECK( GROUP_ZOMBIE_PREDATOR_UPGRADE->IsMonsterInGroup( mon_zombie_rider ) );
    CHECK_FALSE( GROUP_ZOMBIE_UPGRADE->IsMonsterInGroup( mon_zombie_rider ) );

    const MonsterGroup &zombie_group = GROUP_ZOMBIE.obj();
    int direct_entries = 0;
    for( const MonsterGroupEntry &entry : zombie_group.monsters ) {
        if( entry.is_group() ) {
            continue;
        }
        if( entry.mtype == mon_zombie_rider ) {
            direct_entries++;
        }
    }

    CHECK( direct_entries == 0 );

    int natural_direct_entries = 0;
    int debug_direct_entries = 0;
    for( const auto &group_pair : MonsterGroupManager::Get_all_Groups() ) {
        for( const MonsterGroupEntry &entry : group_pair.second.monsters ) {
            if( entry.is_group() || entry.mtype != mon_zombie_rider ) {
                continue;
            }
            if( group_pair.first == GROUP_DEBUG_ZOMBIE_RIDER ) {
                debug_direct_entries++;
                CHECK( entry.pack_minimum == 1 );
                CHECK( entry.pack_maximum == 1 );
                continue;
            }
            // The predator upgrade group is the deliberate late-world origin,
            // not an ordinary area-generation spawn table.
            if( group_pair.first == GROUP_ZOMBIE_PREDATOR_UPGRADE ) {
                continue;
            }
            natural_direct_entries++;
        }
    }
    CHECK( natural_direct_entries == 0 );
    CHECK( debug_direct_entries == 1 );
}

TEST_CASE( "zombie_rider_predator_evolution_gate_scales_with_configured_seasons",
           "[zombie_rider][monster][calendar]" )
{
    const int season_length_days = GENERATE( 14, 91, 127 );
    CAPTURE( season_length_days );
    restore_on_out_of_scope restore_start_of_cataclysm( calendar::start_of_cataclysm );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    on_out_of_scope restore_season_length( []() {
        calendar::set_season_length( 91 );
    } );
    calendar::set_season_length( season_length_days );
    calendar::start_of_cataclysm = calendar::turn_zero;

    monster predator( mon_zombie_predator );
    REQUIRE( predator.can_upgrade() );
    predator.allow_upgrade();
    const time_duration gate = zombie_rider_overmap_ai::mature_world_gate_seasons *
                               calendar::season_length();
    CHECK( zombie_rider_overmap_ai::mature_world_gate_days() == to_days<int>( gate ) );

    calendar::turn = calendar::start_of_cataclysm + gate - 1_turns;
    predator.try_upgrade( false );
    CHECK( predator.type->id == mon_zombie_predator );
    CHECK( predator.get_upgrade_time() == 0 );

    calendar::turn = calendar::start_of_cataclysm + gate;
    predator.try_upgrade( false );
    CHECK( ( predator.type->id == mon_zombie_rider ||
             predator.get_upgrade_time() > to_days<int>( calendar::turn - calendar::turn_zero ) ||
             !predator.can_upgrade() ) );
}

TEST_CASE( "zombie_rider_zz_disabled_blacklisted_and_multistage_catchup_matrix",
           "[zombie_rider][monster][evolution][blacklist][save]" )
{
    const MonsterGroupManager::blacklist_state blacklist_before =
        MonsterGroupManager::snapshot_blacklist_state();
    on_out_of_scope restore_blacklist( [blacklist_before]() {
        MonsterGroupManager::restore_blacklist_state( blacklist_before );
    } );
    restore_on_out_of_scope restore_start_of_cataclysm( calendar::start_of_cataclysm );
    restore_on_out_of_scope restore_calendar_turn( calendar::turn );
    calendar::start_of_cataclysm = calendar::turn_zero;
    calendar::set_season_length( 91 );
    const time_duration gate = zombie_rider_overmap_ai::mature_world_gate_seasons *
                               calendar::season_length();

    // Disabled scheduling: without the explicit production allow-upgrade
    // admission, a mature predator remains unchanged.
    calendar::turn = calendar::start_of_cataclysm + gate + 1_days;
    monster disabled( mon_zombie_predator );
    disabled.try_upgrade( false );
    CHECK( disabled.type->id == mon_zombie_predator );

    // Blacklist admission is enforced by the native MonsterGroupManager, not
    // by a rider-specific test seam.
    const JsonValue blacklist_json = json_loader::from_string(
                                         R"({"monsters":["mon_zombie_rider"]})" );
    MonsterGroupManager::LoadMonsterBlacklist( blacklist_json.get_object() );
    CHECK( MonsterGroupManager::monster_is_blacklisted( mon_zombie_rider ) );

    calendar::turn = calendar::start_of_cataclysm + gate;
    monster blacklisted( mon_zombie_predator );
    blacklisted.allow_upgrade();
    blacklisted.try_upgrade( false );
    CHECK( blacklisted.type->id == mon_zombie_predator );
    CHECK_FALSE( blacklisted.type->id == mon_zombie_rider );

    MonsterGroupManager::restore_blacklist_state( blacklist_before );
    CHECK_FALSE( MonsterGroupManager::monster_is_blacklisted( mon_zombie_rider ) );

    // A failed early schedule does not consume the later mature catch-up.
    calendar::turn = calendar::start_of_cataclysm + gate - 1_days;
    monster staged( mon_zombie_predator );
    staged.allow_upgrade();
    staged.try_upgrade( false );
    CHECK( staged.type->id == mon_zombie_predator );
    CHECK( staged.get_upgrade_time() == 0 );

    // Save/load before the mature stage, then let the ordinary upgrade path
    // choose the endpoint once the world is eligible.
    const auto before_reload = rider_native_causal_snapshot( staged );
    monster reloaded = round_trip_zombie_rider( staged );
    const auto after_reload = rider_native_causal_snapshot( reloaded );
    CHECK( after_reload == before_reload );
    calendar::turn = calendar::start_of_cataclysm + gate;
    const auto before_upgrade = rider_native_causal_snapshot( reloaded );
    reloaded.try_upgrade( false );
    INFO( "before_reload: " << before_reload );
    INFO( "after_reload: " << after_reload );
    INFO( "before_upgrade: " << before_upgrade );
    INFO( "after_upgrade: " << rider_native_causal_snapshot( reloaded ) );
    // The native upgrade group has a weighted default/reschedule outcome.
    // A separate real native-owner seeded control covers a positive rider
    // endpoint; this ordinary reload may not manufacture that outcome.
    if( reloaded.type->id == mon_zombie_rider ) {
        CHECK( reloaded.predator_state().ammo_initialization_version == 1 );
    } else {
        CHECK( reloaded.type->id == mon_zombie_predator );
        CHECK( ( !reloaded.can_upgrade() || reloaded.get_upgrade_time() >
                 to_days<int>( calendar::turn - calendar::turn_zero ) ) );
    }
}

TEST_CASE( "zombie_rider_shot_ammo_survives_unload_reload_and_repeated_serialization",
           "[zombie_rider][monster][ammo][save][evolution]" )
{
    clear_map_without_vision();
    map &here = get_map();
    Character &you = get_player_character();
    const tripoint_bub_ms center{ 65, 65, 0 };
    prepare_zombie_rider_local_arena( here, center );
    you.setpos( here, center );
    monster &rider = spawn_new_local_rider_fixture( center + point::east * 6 );
    rider.anger = 100;
    rider.aggro_character = true;
    rider.set_special( zombie_rider_bone_bow_shot, 0 );
    rider.set_moves( 100 );
    const int initial = rider.ammo[zombie_rider_tainted_bone_arrow];
    REQUIRE( rider.sees( here, you ) );
    rider.plan();
    rider.move();
    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == initial - 1 );

    monster unloaded = round_trip_zombie_rider( rider );
    CHECK( unloaded.ammo[zombie_rider_tainted_bone_arrow] == initial - 1 );
    monster repeated = round_trip_zombie_rider( unloaded );
    CHECK( repeated.ammo[zombie_rider_tainted_bone_arrow] == initial - 1 );
    repeated.ammo[zombie_rider_tainted_bone_arrow] = 0;
    monster empty = round_trip_zombie_rider( repeated );
    CHECK( empty.ammo[zombie_rider_tainted_bone_arrow] == 0 );
    clear_map_without_vision();
}

TEST_CASE( "live_unbanded_rider_receives_one_light_investigation",
           "[zombie_rider][native][light][unbanded_regression]" )
{
    restore_on_out_of_scope restore_time( calendar::turn );
    restore_on_out_of_scope restore_bands( overmap_buffer.global_state.zombie_rider_bands );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    restore_on_out_of_scope restore_memory( overmap_buffer.global_state.zombie_rider_light_memory );
    restore_on_out_of_scope restore_memory_turn( overmap_buffer.global_state.zombie_rider_light_memory_last_turn );
    clear_map_without_vision();
    on_out_of_scope cleanup( [] { clear_map_without_vision(); } );
    set_time( calendar::turn_zero + 80000000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_zombie_rider_local_arena( here, center );
    get_avatar().setpos( here, tripoint_bub_ms( center.xy(), -2 ) );
    overmap_buffer.global_state.zombie_rider_bands.clear();
    overmap_buffer.global_state.zombie_rider_light_memory.clear();
    monster &rider = spawn_new_local_rider_fixture( center + point::east * 4 );
    REQUIRE( overmap_buffer.predator_activity_for( rider, predator_activation::representation::local,
             rider.pos_abs() ) );
    REQUIRE( overmap_buffer.global_state.zombie_rider_bands.living_members(
                 rider.predator_state().actor_id ) == 0 );
    REQUIRE_FALSE( zombie_rider_overmap_ai::get_camp_pressure_intent( rider ) );

    bandit_mark_generation::light_packet packet;
    packet.id = "unbanded-lamp";
    packet.envelope_id = "unbanded-lamp-envelope";
    packet.region_id = "unbanded-lamp-region";
    packet.observed_range_omt = 8;
    packet.source_strength = 4;
    packet.persistence = 3;
    packet.time = bandit_mark_generation::light_time_band::night;
    packet.weather = bandit_mark_generation::light_weather_band::clear;
    packet.exposure = bandit_mark_generation::light_exposure_band::exposed;
    packet.source = bandit_mark_generation::light_source_band::ordinary;
    packet.terrain = bandit_mark_generation::light_terrain_band::open;
    live_bandit_signal_observation signal;
    signal.light_projection = bandit_mark_generation::adapt_light_packet( packet );
    signal.has_light_projection = true;
    signal.horde_signal_power = bandit_mark_generation::horde_signal_power_from_light_projection(
                                    signal.light_projection );
    REQUIRE( signal.light_projection.viable );
    REQUIRE( signal.horde_signal_power > 0 );
    signal.source_ms = here.get_abs( center );
    signal.source_omt = project_to<coords::omt>( *signal.source_ms );
    signal.sample_id = "unbanded-lamp#80000000";
    signal.observed_turn = to_turn<int>( calendar::turn );
    CHECK( signal_live_zombie_riders_from_light_observations_for_test( {} ) == 0 );
    REQUIRE( signal_live_zombie_riders_from_light_observations_for_test( { signal } ) == 1 );
    const auto intent = zombie_rider_overmap_ai::get_camp_pressure_intent( rider );
    REQUIRE( intent );
    CHECK( intent->posture == zombie_rider_overmap_ai::rider_camp_pressure_posture::investigate );
    CHECK( intent->source == *signal.source_ms );
    CHECK( intent->sample_id == signal.sample_id );
    CHECK( overmap_buffer.global_state.zombie_rider_bands.living_members(
               rider.predator_state().actor_id ) == 0 );
    CHECK( signal_live_zombie_riders_from_light_observations_for_test( { signal } ) == 0 );
    CHECK( zombie_rider_overmap_ai::get_camp_pressure_intent( rider )->observed_world ==
           signal.observed_turn );
}

TEST_CASE( "zombie_rider_overmap_light_attraction_is_late_game_and_bounded",
           "[zombie_rider][overmap][ai]" )
{
    bandit_mark_generation::light_packet camp_light;
    camp_light.id = "late_game_exposed_camp_light";
    camp_light.envelope_id = "camp_light_cluster";
    camp_light.region_id = "camp_region";
    camp_light.observed_range_omt = 8;
    camp_light.source_strength = 4;
    camp_light.persistence = 3;
    camp_light.time = bandit_mark_generation::light_time_band::night;
    camp_light.weather = bandit_mark_generation::light_weather_band::clear;
    camp_light.exposure = bandit_mark_generation::light_exposure_band::exposed;
    camp_light.source = bandit_mark_generation::light_source_band::ordinary;
    camp_light.terrain = bandit_mark_generation::light_terrain_band::open;

    const bandit_mark_generation::light_projection projection =
        bandit_mark_generation::adapt_light_packet( camp_light );
    REQUIRE( projection.viable );
    REQUIRE( bandit_mark_generation::horde_signal_power_from_light_projection( projection ) > 0 );

    const zombie_rider_overmap_ai::rider_light_interest early =
        zombie_rider_overmap_ai::evaluate_light_attraction( projection,
                zombie_rider_overmap_ai::mature_world_gate_days() - 1, 3 );
    CHECK_FALSE( early.should_investigate );
    CHECK( early.reason == "early_world_gate" );
    CHECK( early.max_riders_drawn == 0 );

    const zombie_rider_overmap_ai::rider_light_interest mature =
        zombie_rider_overmap_ai::evaluate_light_attraction( projection,
                zombie_rider_overmap_ai::mature_world_gate_days() + 30, 3 );
    CHECK( mature.should_investigate );
    CHECK( mature.reason == "exposed_bright_light" );
    CHECK( mature.interest_score > 0 );
    CHECK( mature.memory_turns >= 90 );
    CHECK( mature.memory_turns <= 300 );
    CHECK( mature.max_riders_drawn == 1 );

    const zombie_rider_overmap_ai::rider_light_interest no_riders =
        zombie_rider_overmap_ai::evaluate_light_attraction( projection,
                zombie_rider_overmap_ai::mature_world_gate_days() + 30, 0 );
    CHECK_FALSE( no_riders.should_investigate );
    CHECK( no_riders.reason == "no_riders_available" );
    CHECK( no_riders.max_riders_drawn == 0 );
}

TEST_CASE( "zombie_rider_overmap_light_negative_controls_do_not_call_riders",
           "[zombie_rider][overmap][ai]" )
{
    bandit_mark_generation::light_packet no_light;
    no_light.id = "dark_camp";
    no_light.envelope_id = "dark_camp";
    no_light.region_id = "dark_region";
    no_light.observed_range_omt = 4;
    no_light.source_strength = 0;
    no_light.persistence = 0;
    no_light.time = bandit_mark_generation::light_time_band::night;
    no_light.weather = bandit_mark_generation::light_weather_band::clear;
    no_light.exposure = bandit_mark_generation::light_exposure_band::contained;
    no_light.source = bandit_mark_generation::light_source_band::ordinary;

    const bandit_mark_generation::light_projection dark_projection =
        bandit_mark_generation::adapt_light_packet( no_light );
    const zombie_rider_overmap_ai::rider_light_interest dark_interest =
        zombie_rider_overmap_ai::evaluate_light_attraction( dark_projection,
                zombie_rider_overmap_ai::mature_world_gate_days() + 30, 2 );
    CHECK_FALSE( dark_interest.should_investigate );
    CHECK( dark_interest.reason == "no_viable_light_signal" );

    bandit_mark_generation::light_packet weak_screened_light = no_light;
    weak_screened_light.id = "weak_screened_light";
    weak_screened_light.source_strength = 1;
    weak_screened_light.exposure = bandit_mark_generation::light_exposure_band::screened;
    weak_screened_light.observed_range_omt = 2;

    const bandit_mark_generation::light_projection weak_projection =
        bandit_mark_generation::adapt_light_packet( weak_screened_light );
    REQUIRE( weak_projection.viable );
    CHECK( bandit_mark_generation::horde_signal_power_from_light_projection( weak_projection ) == 0 );

    const zombie_rider_overmap_ai::rider_light_interest weak_interest =
        zombie_rider_overmap_ai::evaluate_light_attraction( weak_projection,
                zombie_rider_overmap_ai::mature_world_gate_days() + 30, 2 );
    CHECK_FALSE( weak_interest.should_investigate );
    CHECK( weak_interest.reason == "below_rider_light_threshold" );

    bandit_mark_generation::light_packet daylight_searchlight = weak_screened_light;
    daylight_searchlight.id = "daylight_searchlight";
    daylight_searchlight.source_strength = 8;
    daylight_searchlight.persistence = 8;
    daylight_searchlight.observed_range_omt = 12;
    daylight_searchlight.time = bandit_mark_generation::light_time_band::daylight;
    daylight_searchlight.exposure = bandit_mark_generation::light_exposure_band::exposed;
    daylight_searchlight.source = bandit_mark_generation::light_source_band::searchlight;

    const bandit_mark_generation::light_projection daylight_projection =
        bandit_mark_generation::adapt_light_packet( daylight_searchlight );
    REQUIRE( daylight_projection.viable );
    CHECK( bandit_mark_generation::horde_signal_power_from_light_projection(
               daylight_projection ) == 0 );

    const zombie_rider_overmap_ai::rider_light_interest daylight_interest =
        zombie_rider_overmap_ai::evaluate_light_attraction( daylight_projection,
                zombie_rider_overmap_ai::mature_world_gate_days() + 30, 2 );
    CHECK_FALSE( daylight_interest.should_investigate );
    CHECK( daylight_interest.reason == "below_rider_light_threshold" );
}

TEST_CASE( "zombie_rider_overmap_light_memory_decays_and_caps_accumulation",
           "[zombie_rider][overmap][ai]" )
{
    bandit_mark_generation::light_packet elevated_light;
    elevated_light.id = "elevated_searchlight";
    elevated_light.envelope_id = "elevated_camp_light";
    elevated_light.region_id = "camp_region";
    elevated_light.observed_range_omt = 12;
    elevated_light.source_strength = 8;
    elevated_light.persistence = 8;
    elevated_light.side_leakage = 2;
    elevated_light.time = bandit_mark_generation::light_time_band::night;
    elevated_light.weather = bandit_mark_generation::light_weather_band::clear;
    elevated_light.exposure = bandit_mark_generation::light_exposure_band::exposed;
    elevated_light.source = bandit_mark_generation::light_source_band::searchlight;
    elevated_light.terrain = bandit_mark_generation::light_terrain_band::open;
    elevated_light.vertical_offset = 2;
    elevated_light.vertical_sightline = true;
    elevated_light.elevation_bonus = 3;

    const bandit_mark_generation::light_projection projection =
        bandit_mark_generation::adapt_light_packet( elevated_light );
    REQUIRE( projection.viable );

    const zombie_rider_overmap_ai::rider_light_interest interest =
        zombie_rider_overmap_ai::evaluate_light_attraction( projection,
                zombie_rider_overmap_ai::mature_world_gate_days() + 30, 9 );
    REQUIRE( interest.should_investigate );
    CHECK( interest.reason == "elevated_bright_light" );
    CHECK( interest.interest_score == 6 );
    CHECK( interest.memory_turns == 300 );
    CHECK( interest.max_riders_drawn == zombie_rider_overmap_ai::max_riders_drawn_by_light );

    zombie_rider_overmap_ai::rider_light_memory memory;
    zombie_rider_overmap_ai::refresh_light_memory( memory, interest );
    REQUIRE( memory.active() );
    CHECK( memory.max_riders_drawn == zombie_rider_overmap_ai::max_riders_drawn_by_light );

    zombie_rider_overmap_ai::rider_light_memory single_advance = memory;
    zombie_rider_overmap_ai::rider_light_memory partitioned_advance = memory;
    zombie_rider_overmap_ai::advance_light_memory( single_advance, 60 );
    for( int turn = 0; turn < 60; ++turn ) {
        zombie_rider_overmap_ai::advance_light_memory( partitioned_advance, 1 );
    }

    CHECK( partitioned_advance.interest_score == single_advance.interest_score );
    CHECK( partitioned_advance.turns_remaining == single_advance.turns_remaining );
    CHECK( partitioned_advance.max_riders_drawn == single_advance.max_riders_drawn );
    CHECK( partitioned_advance.decay_turn_remainder == single_advance.decay_turn_remainder );
    CHECK( partitioned_advance.reason == single_advance.reason );
    CHECK( single_advance.active() );
    CHECK( single_advance.interest_score == 5 );
    CHECK( single_advance.turns_remaining == 240 );
    CHECK( single_advance.max_riders_drawn == zombie_rider_overmap_ai::max_riders_drawn_by_light );

    zombie_rider_overmap_ai::advance_light_memory( single_advance, 1000 );
    CHECK_FALSE( single_advance.active() );
    CHECK( single_advance.reason == "decayed_after_light_off" );
}

TEST_CASE( "zombie_rider_light_observation_is_timestamped_and_cached_delivery_does_not_refresh",
           "[zombie_rider][overmap][ai][continuity]" )
{
    zombie_rider_overmap_ai::rider_light_interest interest;
    interest.should_investigate = true;
    interest.interest_score = 5;
    interest.memory_turns = 90;
    interest.max_riders_drawn = 1;
    interest.reason = "brief_exposed_light";
    zombie_rider_overmap_ai::rider_light_memory memory;

    REQUIRE( zombie_rider_overmap_ai::refresh_light_memory( memory, interest, "lamp@1#100", 100 ) );
    CHECK( memory.observed_at_turn == 100 );
    CHECK( memory.expires_at_turn == 190 );
    CHECK( memory.sample_id == "lamp@1#100" );
    zombie_rider_overmap_ai::advance_light_memory( memory, 10 );
    CHECK( memory.turns_remaining == 80 );
    CHECK_FALSE( zombie_rider_overmap_ai::refresh_light_memory( memory, interest,
                 "lamp@1#100", 100 ) );
    CHECK( memory.observed_at_turn == 100 );
    CHECK( memory.expires_at_turn == 190 );
    CHECK( memory.turns_remaining == 80 );

    REQUIRE( zombie_rider_overmap_ai::refresh_light_memory( memory, interest, "lamp@2#125", 125 ) );
    CHECK( memory.observed_at_turn == 125 );
    CHECK( memory.expires_at_turn == 215 );
    CHECK_FALSE( zombie_rider_overmap_ai::refresh_light_memory( memory, interest, "lamp@old#110",
                 110 ) );
    CHECK( memory.observed_at_turn == 125 );
    CHECK( memory.expires_at_turn == 215 );
    // Light evidence is an area lead only; it carries no prey/camp identity.
    CHECK( memory.sample_id.find( "target" ) == std::string::npos );
}

static zombie_rider_overmap_ai::rider_light_memory strong_rider_light_memory()
{
    zombie_rider_overmap_ai::rider_light_memory memory;
    memory.interest_score = 6;
    memory.turns_remaining = 240;
    memory.max_riders_drawn = zombie_rider_overmap_ai::max_riders_drawn_by_light;
    memory.reason = "elevated_bright_light";
    return memory;
}

TEST_CASE( "zombie_rider_overmap_convergence_is_capped_dispatch_not_band_membership",
           "[zombie_rider][overmap][ai]" )
{
    const tripoint_abs_omt light_omt( 100, 100, 0 );
    const zombie_rider_overmap_ai::rider_light_memory memory = strong_rider_light_memory();
    const std::vector<zombie_rider_overmap_ai::rider_overmap_agent> riders = {
        { "near_beta", tripoint_abs_omt( 100, 94, 0 ) },
        { "near_alpha", tripoint_abs_omt( 106, 100, 0 ) },
        { "third_capped", tripoint_abs_omt( 109, 101, 0 ) },
        { "far_ignored", tripoint_abs_omt( 170, 100, 0 ) },
        { "wrong_z_ignored", tripoint_abs_omt( 100, 100, -1 ) },
        { "cooldown_ignored", tripoint_abs_omt( 99, 100, 0 ), true, false, 30 },
        { "banded_ignored", tripoint_abs_omt( 98, 100, 0 ), true, true, 0 },
    };

    const zombie_rider_overmap_ai::rider_convergence_result result =
        zombie_rider_overmap_ai::evaluate_rider_convergence( memory, light_omt, riders );

    REQUIRE( result.should_converge );
    CHECK( result.cap == zombie_rider_overmap_ai::max_riders_drawn_by_light );
    CHECK( result.selected_riders == zombie_rider_overmap_ai::max_riders_drawn_by_light );
    CHECK_FALSE( result.band_formed );
    CHECK( result.band_size == 0 );
    CHECK( result.posture == "lone_rider_harass" );
    CHECK( result.reason == "rider_converges_to_light_interest" );
    CHECK( result.rider_ids == std::vector<std::string> { "near_alpha", "near_beta" } );
}

TEST_CASE( "zombie_rider_overmap_convergence_keeps_lone_rider_below_band_minimum",
           "[zombie_rider][overmap][ai]" )
{
    const tripoint_abs_omt light_omt( 100, 100, 0 );
    zombie_rider_overmap_ai::rider_light_memory memory = strong_rider_light_memory();
    memory.max_riders_drawn = 1;
    const std::vector<zombie_rider_overmap_ai::rider_overmap_agent> riders = {
        { "lone_rider", tripoint_abs_omt( 101, 100, 0 ) },
        { "outside_response", tripoint_abs_omt( 150, 100, 0 ) },
    };

    const zombie_rider_overmap_ai::rider_convergence_result result =
        zombie_rider_overmap_ai::evaluate_rider_convergence( memory, light_omt, riders );

    REQUIRE( result.should_converge );
    CHECK_FALSE( result.band_formed );
    CHECK( result.band_size == 0 );
    CHECK( result.selected_riders == 1 );
    CHECK( result.posture == "lone_rider_harass" );
    CHECK( result.reason == "rider_converges_to_light_interest" );
    CHECK( result.rider_ids == std::vector<std::string> { "lone_rider" } );
}

TEST_CASE( "zombie_rider_overmap_convergence_reserves_riders_between_light_clusters",
           "[zombie_rider][overmap][ai]" )
{
    const zombie_rider_overmap_ai::rider_light_memory memory = strong_rider_light_memory();
    std::vector<zombie_rider_overmap_ai::rider_overmap_agent> riders = {
        { "rider_alpha", tripoint_abs_omt( 100, 98, 0 ) },
        { "rider_beta", tripoint_abs_omt( 102, 100, 0 ) },
    };
    const zombie_rider_overmap_ai::rider_convergence_result first =
        zombie_rider_overmap_ai::evaluate_rider_convergence(
            memory, tripoint_abs_omt( 100, 100, 0 ), riders );
    REQUIRE( first.selected_riders == 2 );

    zombie_rider_overmap_ai::reserve_rider_convergence( riders, first );
    CHECK( std::all_of( riders.begin(), riders.end(), []( const auto & rider ) {
        return !rider.available && !rider.already_in_band;
    } ) );

    const zombie_rider_overmap_ai::rider_convergence_result second =
        zombie_rider_overmap_ai::evaluate_rider_convergence(
            memory, tripoint_abs_omt( 110, 100, 0 ), riders );
    CHECK_FALSE( second.should_converge );
    CHECK( second.selected_riders == 0 );
    CHECK( second.reason == "no_eligible_riders_in_response_radius" );
}

TEST_CASE( "zombie_rider_overmap_convergence_stops_after_light_memory_decay",
           "[zombie_rider][overmap][ai]" )
{
    const tripoint_abs_omt light_omt( 100, 100, 0 );
    zombie_rider_overmap_ai::rider_light_memory memory = strong_rider_light_memory();
    zombie_rider_overmap_ai::advance_light_memory( memory, 1000 );
    REQUIRE_FALSE( memory.active() );

    const std::vector<zombie_rider_overmap_ai::rider_overmap_agent> riders = {
        { "near_north", tripoint_abs_omt( 100, 94, 0 ) },
        { "near_east", tripoint_abs_omt( 107, 100, 0 ) },
    };
    const zombie_rider_overmap_ai::rider_convergence_result result =
        zombie_rider_overmap_ai::evaluate_rider_convergence( memory, light_omt, riders );

    CHECK_FALSE( result.should_converge );
    CHECK_FALSE( result.band_formed );
    CHECK( result.selected_riders == 0 );
    CHECK( result.reason == "light_memory_inactive" );
}

TEST_CASE( "zombie_rider_overmap_band_pressure_circles_instead_of_wall_suicide",
           "[zombie_rider][overmap][ai]" )
{
    zombie_rider_overmap_ai::rider_camp_pressure_input defended_camp;
    defended_camp.light_memory_active = true;
    defended_camp.rider_count = 2;
    defended_camp.band_formed = true;
    defended_camp.defender_strength = 4;
    defended_camp.breach_or_opening = false;

    const zombie_rider_overmap_ai::rider_camp_pressure_result pressure =
        zombie_rider_overmap_ai::choose_camp_pressure_posture( defended_camp );
    CHECK( pressure.posture == zombie_rider_overmap_ai::rider_camp_pressure_posture::circle_harass );
    CHECK( zombie_rider_overmap_ai::to_string( pressure.posture ) == "circle_harass" );
    CHECK( pressure.reason == "band_without_breach_circles_and_harasses" );

    defended_camp.breach_or_opening = true;
    defended_camp.defender_strength = 2;
    const zombie_rider_overmap_ai::rider_camp_pressure_result breach =
        zombie_rider_overmap_ai::choose_camp_pressure_posture( defended_camp );
    CHECK( breach.posture == zombie_rider_overmap_ai::rider_camp_pressure_posture::direct_attack );
    CHECK( zombie_rider_overmap_ai::to_string( breach.posture ) == "direct_attack" );
    CHECK( breach.reason == "breach_or_opening_with_advantage" );

    defended_camp.rider_wounded = true;
    const zombie_rider_overmap_ai::rider_camp_pressure_result wounded =
        zombie_rider_overmap_ai::choose_camp_pressure_posture( defended_camp );
    CHECK( wounded.posture == zombie_rider_overmap_ai::rider_camp_pressure_posture::withdraw );
    CHECK( zombie_rider_overmap_ai::to_string( wounded.posture ) == "withdraw" );
    CHECK( wounded.reason == "wounded_rider_disengages" );

    zombie_rider_overmap_ai::rider_camp_pressure_input quiet_camp;
    const zombie_rider_overmap_ai::rider_camp_pressure_result quiet =
        zombie_rider_overmap_ai::choose_camp_pressure_posture( quiet_camp );
    CHECK( quiet.posture == zombie_rider_overmap_ai::rider_camp_pressure_posture::none );
    CHECK( zombie_rider_overmap_ai::to_string( quiet.posture ) == "none" );
    CHECK( quiet.reason == "no_active_light_pressure" );

    zombie_rider_overmap_ai::rider_camp_pressure_input lone_probe;
    lone_probe.light_memory_active = true;
    lone_probe.rider_count = 1;
    const zombie_rider_overmap_ai::rider_camp_pressure_result investigate =
        zombie_rider_overmap_ai::choose_camp_pressure_posture( lone_probe );
    CHECK( investigate.posture == zombie_rider_overmap_ai::rider_camp_pressure_posture::investigate );
    CHECK( zombie_rider_overmap_ai::to_string( investigate.posture ) == "investigate" );
    CHECK( investigate.reason == "lone_rider_investigates_light" );

    zombie_rider_overmap_ai::rider_camp_pressure_input mixed_band = defended_camp;
    mixed_band.rider_count = 1;
    mixed_band.band_formed = false;
    mixed_band.defender_strength = 2;
    mixed_band.breach_or_opening = true;
    mixed_band.rider_wounded = false;
    const zombie_rider_overmap_ai::rider_camp_pressure_result mixed_healthy_rider =
        zombie_rider_overmap_ai::choose_camp_pressure_posture( mixed_band );
    CHECK( mixed_healthy_rider.posture ==
           zombie_rider_overmap_ai::rider_camp_pressure_posture::investigate );
    CHECK( mixed_healthy_rider.reason == "lone_rider_investigates_light" );
}

TEST_CASE( "zombie_rider_bands_require_encounter_and_preserve_survivors",
           "[zombie_rider][overmap][band]" )
{
    using namespace zombie_rider_overmap_ai;
    rider_band_registry bands;
    // Positive admission represents the local/native LOS + route branch.
    // These two negatives are the abstract fail-closed controls: endpoint
    // proximity without reciprocal perception, and a sealed/unknown route.
    const rider_band_encounter unknown_geometry { "alpha", "beta", 10, false, true };
    const rider_band_encounter sealed_route { "alpha", "beta", 10, true, false };
    CHECK_FALSE( bands.register_encounter( unknown_geometry ) );
    CHECK_FALSE( bands.register_encounter( sealed_route ) );

    REQUIRE( bands.register_encounter( { "beta", "alpha", 11, true, true } ) );
    CHECK( bands.band_for( "alpha" ) == "alpha" );
    CHECK( bands.band_for( "beta" ) == "alpha" );
    CHECK( bands.living_members( "alpha" ) == 2 );
    CHECK_FALSE( bands.register_encounter( { "alpha", "beta", 11, true, true } ) );

    REQUIRE( bands.register_encounter( { "gamma", "delta", 12, true, true } ) );
    REQUIRE( bands.register_encounter( { "delta", "alpha", 13, true, true } ) );
    CHECK( bands.band_for( "gamma" ) == "alpha" );
    CHECK( bands.living_members( "beta" ) == 4 );

    predator_lifecycle_state stale_cache;
    stale_cache.band_reference = "obsolete";
    stale_cache.band_revision_cache = 0;
    bands.reconcile_cache( "gamma", stale_cache );
    CHECK( stale_cache.band_reference == "alpha" );
    CHECK( stale_cache.band_revision_cache == bands.revision_for( "gamma" ) );

    std::ostringstream serialized;
    JsonOut json( serialized, true );
    bands.serialize( json );
    rider_band_registry reloaded;
    reloaded.deserialize( json_loader::from_string( serialized.str() ).get_object() );
    CHECK( reloaded.band_for( "delta" ) == "alpha" );
    CHECK( reloaded.living_members( "alpha" ) == 4 );

    REQUIRE( bands.record_casualty( "beta", 14 ) );
    CHECK( bands.living_members( "alpha" ) == 3 );
    CHECK( bands.band_for( "beta" ).empty() );
    CHECK( bands.band_for( "gamma" ) == "alpha" );
}

TEST_CASE( "zombie_rider_band_survivors_keep_a_dead_canonical_root_across_save_and_merge",
           "[zombie_rider][overmap][band][save]" )
{
    using namespace zombie_rider_overmap_ai;
    rider_band_registry bands;
    REQUIRE( bands.register_encounter( { "alpha", "beta", 10, true, true } ) );
    REQUIRE( bands.register_encounter( { "beta", "gamma", 11, true, true } ) );
    const std::uint64_t revision_before_casualty = bands.revision_for( "beta" );

    REQUIRE( bands.record_casualty( "alpha", 12 ) );
    CHECK( bands.band_for( "alpha" ).empty() );
    CHECK( bands.band_for( "beta" ) == "alpha" );
    CHECK( bands.band_for( "gamma" ) == "alpha" );
    CHECK( bands.in_same_band( "beta", "gamma" ) );
    CHECK( bands.living_members( "beta" ) == 2 );
    CHECK( bands.revision_for( "beta" ) == revision_before_casualty + 1 );

    predator_lifecycle_state cached_beta;
    bands.reconcile_cache( "beta", cached_beta );
    CHECK( cached_beta.band_reference == "alpha" );
    CHECK( cached_beta.band_revision_cache == bands.revision_for( "beta" ) );

    std::ostringstream serialized;
    JsonOut json( serialized, true );
    bands.serialize( json );
    rider_band_registry reloaded;
    reloaded.deserialize( json_loader::from_string( serialized.str() ).get_object() );
    CHECK( reloaded.band_for( "beta" ) == "alpha" );
    CHECK( reloaded.band_for( "gamma" ) == "alpha" );
    CHECK( reloaded.living_members( "gamma" ) == 2 );
    CHECK( reloaded.revision_for( "gamma" ) == bands.revision_for( "beta" ) );

    REQUIRE( reloaded.register_encounter( { "delta", "gamma", 13, true, true } ) );
    CHECK( reloaded.band_for( "delta" ) == "alpha" );
    CHECK( reloaded.in_same_band( "beta", "delta" ) );
    CHECK( reloaded.living_members( "beta" ) == 3 );
}

TEST_CASE( "zombie_rider_band_malformed_references_are_diagnostic_and_fail_closed",
           "[zombie_rider][overmap][band][save]" )
{
    using namespace zombie_rider_overmap_ai;
    rider_band_registry bands;
    const std::string malformed = R"({
        "members": [
            { "actor_id": "beta", "parent": "missing", "alive": true, "casualty_turn": -1 },
            { "actor_id": "cycle_a", "parent": "cycle_b", "alive": true, "casualty_turn": -1 },
            { "actor_id": "cycle_b", "parent": "cycle_a", "alive": true, "casualty_turn": -1 }
        ],
        "revisions": {}, "encounter_turns": {}, "observations": []
    })";
    bands.deserialize( json_loader::from_string( malformed ).get_object() );

    CHECK( bands.band_for( "beta" ).empty() );
    CHECK( bands.band_for( "cycle_a" ).empty() );
    CHECK( bands.living_members( "beta" ) == 0 );
    const std::string diagnostic = bands.diagnostic_json();
    CHECK( diagnostic.find( "beta:missing_parent:missing" ) != std::string::npos );
    CHECK( diagnostic.find( "cycle_a:parent_cycle:" ) != std::string::npos );
    CHECK( diagnostic.find( "cycle_b:parent_cycle:" ) != std::string::npos );
    CHECK( diagnostic.find( "\"actor_id\":\"missing\"" ) == std::string::npos );
}

TEST_CASE( "zombie_rider_band_observations_are_encounter_bounded_and_not_refreshed",
           "[zombie_rider][overmap][band]" )
{
    using namespace zombie_rider_overmap_ai;
    rider_band_registry bands;
    REQUIRE( bands.register_encounter( { "alpha", "beta", 30, true, true } ) );
    rider_band_observation observed;
    observed.target_identity = 42;
    observed.position = tripoint_abs_ms( 120, 140, 0 );
    observed.observed_turn = 29;
    observed.expires_at_turn = 40;
    observed.source_actor_id = "alpha";
    observed.provenance = "direct_sight";
    observed.uncertainty = 3;
    bands.observe( "alpha", observed );
    REQUIRE( bands.share_observation( "alpha", "beta", 30 ) );
    const std::optional<rider_band_observation> shared = bands.observation_for( "beta", 30 );
    REQUIRE( shared );
    CHECK( shared->position == observed.position );
    CHECK( shared->observed_turn == 29 );
    CHECK( shared->expires_at_turn == 40 );
    CHECK( shared->source_actor_id == "alpha" );
    CHECK( shared->provenance == "relayed_direct_rider_sight" );
    CHECK_FALSE( bands.share_observation( "alpha", "beta", 31 ) );
    CHECK_FALSE( bands.observation_for( "beta", 41 ) );
}

TEST_CASE( "zombie_rider_band_equal_turn_observations_choose_identified_direct_evidence_deterministically",
           "[zombie_rider][overmap][band]" )
{
    using namespace zombie_rider_overmap_ai;
    rider_band_registry bands;
    rider_band_observation light;
    light.position = tripoint_abs_ms( 10, 10, 0 );
    light.observed_turn = 50;
    light.expires_at_turn = 90;
    light.source_actor_id = "zeta";
    light.provenance = "light";
    light.uncertainty = 0;
    rider_band_observation direct = light;
    direct.target_identity = 42;
    direct.position = tripoint_abs_ms( 12, 10, 0 );
    direct.source_actor_id = "alpha";
    direct.provenance = "direct_rider_sight";
    direct.uncertainty = 2;

    bands.observe( "rider", light );
    bands.observe( "rider", direct );
    const std::optional<rider_band_observation> selected = bands.observation_for( "rider", 50 );
    REQUIRE( selected );
    CHECK( selected->target_identity == 42 );
    CHECK( selected->source_actor_id == "alpha" );
    CHECK( selected->position == direct.position );

    rider_band_registry reverse;
    reverse.observe( "rider", direct );
    reverse.observe( "rider", light );
    const std::optional<rider_band_observation> reverse_selected = reverse.observation_for( "rider",
            50 );
    REQUIRE( reverse_selected );
    CHECK( reverse_selected->source_actor_id == "alpha" );
    CHECK( reverse_selected->position == direct.position );
}

TEST_CASE( "rider encounter preparation leaves resident abstract history untouched",
           "[zombie_rider][overmap][band][resident_history]" )
{
    // The optional measurement is confined to this component fixture. Cold
    // native file construction/loading is never part of repeated owner work.
    const bool measure = std::getenv( "CAOL_RESIDENT_HISTORY_MEASURE" ) != nullptr;
    for( const int history : {
             0, 16, 64
         } ) {
        CAPTURE( history );
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
        restore_on_out_of_scope<time_point> restore_clock( calendar::turn );
        set_time_to_day();
        map &here = get_map();
        get_player_character().setpos( here, tripoint_bub_ms( 100, 100, 0 ) );
        monster &first = spawn_test_monster( mon_zombie_rider.str(), { 60, 60, 0 } );
        monster &second = spawn_test_monster( mon_zombie_rider.str(), { 64, 60, 0 } );
        refresh_pathing_cache( here );
        REQUIRE( first.sees( here, second ) );
        REQUIRE( second.sees( here, first ) );
        REQUIRE_FALSE( here.route( first,
                                   pathfinding_target::point( second.pos_bub() ) ).empty() );
        const std::string first_id = first.predator_state().actor_id;
        const std::string second_id = second.predator_state().actor_id;
        const int now = to_turn<int>( calendar::turn );
        first.zombie_rider_pursuit_state().observe( get_player_character().get_identity(),
                get_player_character().pos_abs(), now );
        const auto first_pos = first.pos_abs();
        const auto second_pos = second.pos_abs();
        const int first_moves = first.get_moves();
        const int second_moves = second.get_moves();
        const int first_hp = first.get_hp();
        const int second_hp = second.get_hp();
        const auto first_ammo = first.ammo;
        const auto second_ammo = second.ammo;
        const auto original_time = calendar::turn;
        const auto make_resident = [&]( const point_abs_om & address ) -> overmap & {
            if( overmap *existing = overmap_buffer.get_existing( address ) ) {
                existing->hordes.clear(); // RAM-only fixture; existing files are not rewritten.
                return *existing;
            }
            const auto terrain = world_generator->active_world->has_compression_enabled() ?
                                 PATH_INFO::current_dimension_save_path() / zzip_overmap_directory /
                                 overmapbuffer::terrain_filename( address ) + zzip_suffix :
                                 PATH_INFO::current_dimension_save_path() /
                                 overmapbuffer::terrain_filename( address );
            created_files.push_back( terrain.get_unrelative_path() );
            created_files.push_back( overmapbuffer::player_filename( address ).get_unrelative_path() );
            // Native serialization/load, no world generation. get_existing
            // cached the prior absence; get opens the exact file just saved.
            auto empty = std::make_unique<overmap>( address );
            empty->save();
            return overmap_buffer.get( address );
        };
        const auto serialize_owner = []( const overmap & owner ) {
            std::ostringstream out;
            owner.serialize( out );
            return out.str();
        };
        const auto report = [&]( const char *phase, const auto & work, const int calls ) {
            const auto wall = std::chrono::steady_clock::now();
            const auto cpu = std::clock();
            work();
            const double cpu_s = static_cast<double>( std::clock() - cpu ) / CLOCKS_PER_SEC;
            const double wall_s = std::chrono::duration<double>(
                                      std::chrono::steady_clock::now() - wall ).count();
            if( measure ) {
                std::cout << "resident_history_measurement maps=" << history << " phase=" << phase
                          << " calls=" << calls << " wall_s=" << wall_s;
#if defined(_WIN32)
                std::cout << " crt_elapsed_s=" << cpu_s;
#else
                std::cout << " process_cpu_s=" << cpu_s;
#endif
#if defined(__unix__) || defined(__APPLE__)
                rusage usage {};
                if( getrusage( RUSAGE_SELF, &usage ) == 0 ) {
                    // ru_maxrss is a process lifetime high-water value, not an
                    // allocation delta or the memory of this owner alone.
                    std::cout << " process_maxrss_native=" << usage.ru_maxrss;
                }
#endif
                std::cout << '\n';
            }
        };
        std::vector<overmap *> distant;
        overmap *near = nullptr;
        const auto near_address = coords::project_to<coords::om>( first.pos_abs() ).xy();
        report( "cold_native_fixture", [&] {
            near = &make_resident( near_address );
            for( int i = 0; i < history; ++i )
            {
                overmap &owner = make_resident( point_abs_om( 1000 + i, 1000 ) );
                const auto p = coords::project_to<coords::ms>( tripoint_abs_om( owner.pos(), 0 ) ) +
                tripoint_rel_ms( 24, 24, 0 );
                if( i % 2 == 0 ) {
                    REQUIRE( owner.hordes.spawn_entity( p, mon_zombie_rider ) );
                } else {
                    monster body( mon_zombie_rider );
                    body.predator_state().band_reference = "old_projection";
                    body.predator_state().band_revision_cache = 77;
                    REQUIRE( owner.hordes.spawn_entity( p, body ) );
                }
                distant.push_back( &owner );
            }
        }, history + 1 );
        REQUIRE( near != nullptr );
        // A genuine existing band peer is abstract, even though its actual
        // position is in the loaded map. Encounter preparation cannot update
        // its payload. Native transfer below owns its later local adoption.
        const auto peer_pos = here.get_abs( tripoint_bub_ms( 72, 72, 0 ) );
        monster peer( mon_zombie_rider );
        const std::string peer_id = peer.predator_state().actor_id;
        REQUIRE( near->hordes.spawn_entity( peer_pos, peer ) );
        auto &bands = overmap_buffer.global_state.zombie_rider_bands;
        REQUIRE( bands.register_encounter( { first_id, peer_id, now - 1, true, true } ) );
        std::vector<std::string> before;
        for( const overmap *owner : distant ) {
            before.push_back( serialize_owner( *owner ) );
        }
        const std::string near_before = serialize_owner( *near );
        const int map_count = overmap_buffer.global_state.overmap_count;
        report( "first_encounter", [&] {
            overmap_buffer.reconcile_rider_band_encounters();
        }, 1 );
        REQUIRE( bands.in_same_band( first_id, second_id ) );
        CHECK( bands.in_same_band( first_id, peer_id ) );
        CHECK( bands.living_members( first_id ) == 3 );
        REQUIRE( bands.observation_for( second_id, now ) );
        CHECK( bands.observation_for( second_id, now )->observed_turn == now );
        CHECK( bands.observation_for( second_id, now )->source_actor_id == first_id );
        const std::string registry = bands.diagnostic_json();
        report( "repeated_encounter", [&] {
            for( int i = 0; i < 100; ++i )
            {
                overmap_buffer.reconcile_rider_band_encounters();
            }
        }, 100 );
        CHECK( bands.diagnostic_json() == registry );
        report( "resident_query", [&] {
            for( int i = 0; i < 2000; ++i )
            {
                const auto found = overmap_buffer.get_loaded_overmaps_near(
                    coords::project_to<coords::sm>( first_pos ).xy(), 4 );
                REQUIRE( found.size() == 1 );
                REQUIRE( found.front() == near );
            }
        }, 2000 );
        CHECK( serialize_owner( *near ) == near_before );
        for( std::size_t i = 0; i < distant.size(); ++i ) {
            CHECK( serialize_owner( *distant[i] ) == before[i] );
        }
        CHECK( overmap_buffer.global_state.overmap_count == map_count );
        CHECK( calendar::turn == original_time );
        CHECK( first.pos_abs() == first_pos );
        CHECK( second.pos_abs() == second_pos );
        CHECK( first.get_moves() == first_moves );
        CHECK( second.get_moves() == second_moves );
        CHECK( first.get_hp() == first_hp );
        CHECK( second.get_hp() == second_hp );
        CHECK( first.ammo == first_ammo );
        CHECK( second.ammo == second_ammo );

        zombie_rider_overmap_ai::rider_band_registry reloaded;
        reloaded.deserialize( json_loader::from_string( registry ).get_object() );
        CHECK( reloaded.in_same_band( second_id, peer_id ) );
        bands = std::move( reloaded );
        overmap_buffer.spawn_monster( coords::project_to<coords::sm>( peer_pos ) );
        monster *returned = nullptr;
        for( monster &body : g->all_monsters() ) {
            if( body.predator_state().actor_id == peer_id ) {
                returned = &body;
            }
        }
        REQUIRE( returned != nullptr );
        CHECK( overmap_buffer.entity_at( peer_pos ) == nullptr );
        overmap_buffer.reconcile_rider_band_encounters();
        CHECK( returned->predator_state().band_reference == bands.band_for( peer_id ) );
        CHECK( returned->predator_state().band_revision_cache == bands.revision_for( peer_id ) );
        CHECK( bands.living_members( peer_id ) == 3 );
        returned->die( &here, &first );
        CHECK( bands.living_members( first_id ) == 2 );
        CHECK( bands.band_for( peer_id ).empty() );
        const std::string after_death = bands.diagnostic_json();
        overmap_buffer.reconcile_rider_band_encounters();
        CHECK( bands.diagnostic_json() == after_death );
    }
}

TEST_CASE( "native rider encounters require reciprocal sight",
           "[zombie_rider][overmap][band][resident_history]" )
{
    const bool blind = GENERATE( false, true );
    CAPTURE( blind );
    clear_avatar();
    clear_map_without_vision();
    overmap_buffer.clear();
    on_out_of_scope cleanup( [] {
        clear_map_without_vision();
        overmap_buffer.clear();
    } );
    restore_on_out_of_scope<time_point> restore_clock( calendar::turn );
    set_time_to_day();
    map &here = get_map();
    get_player_character().setpos( here, tripoint_bub_ms( 100, 100, 0 ) );
    monster &first = spawn_test_monster( mon_zombie_rider.str(), { 60, 60, 0 } );
    monster &second = spawn_test_monster( mon_zombie_rider.str(), { 64, 60, 0 } );
    if( blind ) {
        second.add_effect( efftype_id( "blind" ), 1_hours );
    } else {
        for( int dx = -1; dx <= 1; ++dx ) {
            for( int dy = -1; dy <= 1; ++dy ) {
                if( dx != 0 || dy != 0 ) {
                    here.ter_set( second.pos_bub() + tripoint( dx, dy, 0 ), ter_id( "t_rock" ) );
                }
            }
        }
    }
    refresh_pathing_cache( here );
    CHECK( first.sees( here, second ) == blind );
    REQUIRE_FALSE( second.sees( here, first ) );
    auto &bands = overmap_buffer.global_state.zombie_rider_bands;
    const auto before = bands.diagnostic_json();
    overmap_buffer.reconcile_rider_band_encounters();
    CHECK_FALSE( bands.in_same_band( first.predator_state().actor_id,
                                     second.predator_state().actor_id ) );
    CHECK( bands.diagnostic_json() == before );
}


TEST_CASE( "predator_native_loaded_freeze_keeps_body_and_earned_intent",
           "[zombie_rider][predator_activation][native]" )
{
    clear_avatar();
    clear_map_without_vision();
    const auto old_time = calendar::turn;
    const auto old_position = get_avatar().pos_abs();
    const auto old_world = overmap_buffer.global_state;
    overmap_buffer.global_state = {};
    set_time( calendar::turn_zero + 100000_turns );
    map &here = get_map();
    const tripoint_bub_ms local( 63, 63, 0 );
    prepare_zombie_rider_local_arena( here, local );
    const auto position = here.get_abs( local );
    const auto omt = project_to<coords::omt>( position );
    get_avatar().setpos( here, local + point( 3, 0 ) );
    monster *body = g->place_critter_at( mon_zombie_rider, local );
    REQUIRE( body );
    on_out_of_scope restore( [&]() {
        g->clear_zombies();
        here.clear_fields( local );
        overmap_buffer.global_state = old_world;
        get_avatar().setpos( old_position, false );
        set_time( old_time );
    } );
    body->on_load();
    body->anger = 100;
    body->aggro_character = true;
    REQUIRE( body->sees( here, get_avatar() ) );
    body->plan(); // Genuine native observation before the frozen absence.
    zombie_rider_overmap_ai::set_camp_pressure_intent(
        *body, zombie_rider_overmap_ai::rider_camp_pressure_posture::investigate,
        position + point( 10, 0 ), 90, 0 );
    REQUIRE( body->zombie_rider_pursuit_state().last_observed_turn == to_turn<int>( calendar::turn ) );
    body->set_moves( 150 );
    body->wandf = 10;
    get_avatar().setpos( project_to<coords::ms>( omt - point( 41, 0 ) ), false );
    body->plan(); // actual current native range boundary before the absence
    const auto intent = zombie_rider_overmap_ai::get_camp_pressure_intent( *body );
    REQUIRE( intent );
    here.add_field( local, field_type_id( "fd_fire" ), 1 );
    const auto bytes = []( const monster &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    const auto frozen = bytes( *body );
    // This detached-avatar fixture advances world time without asking the
    // local visibility helper to index a position outside its resident map.
    calendar::turn += 2_days;
    process_monsters_and_npcs_turn_for_test();
    REQUIRE( get_creature_tracker().find( position ) );
    body = get_creature_tracker().find( position ).get();
    CHECK( bytes( *body ) == frozen );
    CHECK( body->get_moves() == 150 );
    CHECK( body->wandf == 10 );
    const auto retained = zombie_rider_overmap_ai::get_camp_pressure_intent( *body );
    REQUIRE( retained );
    CHECK( retained->turns_remaining == intent->turns_remaining );
    here.clear_fields( local );
    get_avatar().setpos( project_to<coords::ms>( omt - point( 40, 0 ) ), false );
    body->on_load();
    const auto resumed = zombie_rider_overmap_ai::get_camp_pressure_intent( *body );
    REQUIRE( resumed );
    CHECK( resumed->turns_remaining == intent->turns_remaining );
    const auto settled = bytes( *body );
    body->on_load();
    CHECK( bytes( *body ) == settled );
    calendar::turn += 1_turns;
    body->process_turn();
    CHECK( body->get_moves() > 150 );
    const auto once = bytes( *body );
    body->process_turn();
    CHECK( bytes( *body ) == once );
}


TEST_CASE( "predator_native_group_placement_retains_refused_snapshots",
           "[zombie_rider][predator_activation][native][placement]" )
{
    clear_avatar();
    clear_map_without_vision();
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    overmap_buffer.clear();
    set_time( calendar::turn_zero + 100000_turns );
    map &here = get_map();
    const auto corner = here.get_bub( project_to<coords::ms>(
                            project_to<coords::omt>( here.get_abs( tripoint_bub_ms( 63, 63, 0 ) ) ) ) );
    const auto source = project_to<coords::sm>( here.get_abs( corner ) );
    const auto om = project_to<coords::om>( source ).xy();
    overmap &owner = overmap_buffer.get( om );
    owner.clear_mon_groups();
    owner.hordes.clear();
    get_avatar().setpos( here, corner + point( 15, 0 ) );
    on_out_of_scope restore( [&]() {
        clear_map_without_vision();
        overmap_buffer.clear();
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
    } );
    mongroup group( GROUP_DEBUG_ZOMBIE_RIDER, source, 0 );
    std::set<std::string> ids;
    for( int i = 0; i < 2; ++i ) {
        monster body( mon_zombie_rider );
        body.ensure_predator_state();
        body.set_hp( body.get_hp() - 7 - i );
        body.add_item( item( itype_id( "rock" ), calendar::turn ) );
        ids.insert( body.predator_state().actor_id );
        group.monsters.push_back( body );
    }
    const auto native_groups_json = [&]( const mongroup &value ) {
        std::ostringstream out;
        {
            JsonOut json( out );
            json.start_object();
            json.member( "monster_groups" );
            json.start_array();
            json.start_array();
            json.write( value );
            json.write( std::vector<tripoint_om_sm>{project_remain<coords::om>( source ).remainder_tripoint} );
            json.end_array();
            json.end_array();
            json.end_object();
        }
        return out.str();
    };
    owner.unserialize( json_loader::from_string( native_groups_json( group ) ).get_object() );
    const auto retained_group = [&]() -> mongroup * {
        for( mongroup *candidate : overmap_buffer.groups_at( source ) ) {
            if( !candidate->monsters.empty() && candidate->monsters.front().is_caol_predator() &&
                ids.count( candidate->monsters.front().predator_state().actor_id ) ) {
                return candidate;
            }
        }
        return nullptr;
    };
    REQUIRE( retained_group() );
    const auto bytes = []( const monster &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    std::vector<std::string> originals;
    std::map<std::string, std::int64_t> item_ids;
    for( const monster &body : retained_group()->monsters ) {
        originals.push_back( bytes( body ) );
        item_ids.emplace( body.predator_state().actor_id, body.inv.front().uid().get_value() );
    }
    for( int y = 0; y < SEEY; ++y ) {
        for( int x = 0; x < SEEX; ++x ) {
            here.ter_set( corner + point( x, y ), ter_id( "t_wall" ) );
            here.furn_clear( corner + point( x, y ) );
        }
    }
    here.spawn_monsters( true ); // Actual public map -> submap -> group owner.
    REQUIRE( retained_group() );
    REQUIRE( retained_group()->monsters.size() == 2 );
    CHECK( bytes( retained_group()->monsters[0] ) == originals[0] );
    CHECK( bytes( retained_group()->monsters[1] ) == originals[1] );
    here.ter_set( corner, ter_id( "t_grass" ) );
    here.spawn_monsters( true );
    REQUIRE( retained_group() );
    REQUIRE( retained_group()->monsters.size() == 1 );
    CHECK( retained_group()->population == 0 );
    std::set<std::string> placed;
    for( monster &body : g->all_monsters() ) {
        if( body.is_caol_predator() && ids.count( body.predator_state().actor_id ) ) {
            placed.insert( body.predator_state().actor_id );
            CHECK( body.predator_state().handoff_epoch == 1 );
            REQUIRE( body.inv.size() == 1 );
            CHECK( body.inv.front().uid().get_value() == item_ids.at( body.predator_state().actor_id ) );
            CHECK( body.pos_abs() == here.get_abs( corner ) );
        }
    }
    CHECK( placed.size() == 1 );
    mongroup *const source_slot = retained_group();
    const auto refused = bytes( source_slot->monsters.front() );
    const auto saved_group = native_groups_json( *source_slot );
    // A real reload releases its prior owner before decoding the saved body.
    // Item UIDs must survive native serialization and move-based installation.
    source_slot->clear();
    auto reloaded = std::make_unique<overmap>( om );
    reloaded->unserialize( json_loader::from_string( saved_group ).get_object() );
    auto omt = project_to<coords::omt>( source );
    const auto loaded_groups = reloaded->debug_unsafe_get_groups_at( omt );
    REQUIRE( loaded_groups.size() == 1 );
    CHECK( bytes( loaded_groups.front().get().monsters.front() ) == refused );
    *source_slot = std::move( loaded_groups.front().get() );
    // A second usable cell is a genuine new placement opportunity.
    here.ter_set( corner + point::east, ter_id( "t_grass" ) );
    here.spawn_monsters( true );
    CHECK_FALSE( retained_group() );
    for( monster &body : g->all_monsters() ) {
        if( body.is_caol_predator() && ids.count( body.predator_state().actor_id ) ) {
            placed.insert( body.predator_state().actor_id );
            CHECK( body.predator_state().handoff_epoch == 1 );
            REQUIRE( body.inv.size() == 1 );
            CHECK( body.inv.front().uid().get_value() == item_ids.at( body.predator_state().actor_id ) );
        }
    }
    CHECK( placed == ids );
    here.spawn_monsters( true );
    std::size_t count = 0;
    for( monster &body : g->all_monsters() ) {
        count += body.is_caol_predator() && ids.count( body.predator_state().actor_id );
    }
    CHECK( count == ids.size() );
}



TEST_CASE( "predator_mixed_native_nemesis_entries_freeze_without_starving_peers",
           "[zombie_rider][predator_activation][native][nemesis]" )
{
    clear_avatar();
    clear_map_without_vision();
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    overmap_buffer.clear();
    set_time( calendar::turn_zero + 100000_turns );
    const point_abs_om address( 3, 4 );
    overmap &owner = overmap_buffer.get( address );
    owner.clear_mon_groups();
    owner.hordes.clear();
    on_out_of_scope cleanup( [&]() {
        clear_map_without_vision();
        overmap_buffer.clear();
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
    } );
    const auto origin = project_to<coords::sm>( tripoint_abs_om( address, 0 ) );
    const auto source = origin + tripoint_rel_sm( 8, 8, 0 );
    const auto other = source + point( 0, 2 );
    REQUIRE( oter_str_id( "road_nesw" ).is_valid() );
    for( int y = 3; y < 7; ++y ) {
        for( int x = 3; x < 9; ++x ) {
            owner.ter_set( tripoint_om_omt( x, y, 0 ), oter_str_id( "road_nesw" ).id() );
        }
    }
    const auto make_group = [&]( const tripoint_abs_sm &position, const std::string &name ) {
        mongroup group( GROUP_DEBUG_ZOMBIE_RIDER, position, 0 );
        group.horde = true;
        group.behaviour = mongroup::horde_behaviour::nemesis;
        group.nemesis_target = position.xy() + point( 5, 0 );
        monster ordinary( mtype_id( "mon_hound_tindalos" ) );
        ordinary.set_value( "fixture_identity", name );
        group.monsters.push_back( std::move( ordinary ) );
        return group;
    };
    auto mixed = make_group( source, "ordinary-alpha" );
    monster predator( mon_zombie_rider );
    predator.ensure_predator_state();
    predator.set_hp( predator.get_hp() - 13 );
    predator.add_item( item( itype_id( "rock" ), calendar::turn ) );
    const auto predator_id = predator.predator_state().actor_id;
    mixed.monsters.push_back( std::move( predator ) );
    owner.debug_force_add_group( mixed );
    owner.debug_force_add_group( make_group( other, "ordinary-beta" ) );
    const auto source_omt = project_to<coords::omt>( source );
    get_avatar().setpos( project_to<coords::ms>( source_omt - point( 41, 0 ) ), false );
    const auto bytes = []( const monster &body ) {
        std::ostringstream out;
        { JsonOut json( out ); body.serialize( json ); }
        return out.str();
    };
    const auto census = [&]() {
        std::map<std::string, std::pair<tripoint_abs_sm, std::string>> found;
        for( int y : { 8, 10 } ) {
            for( int x = 7; x <= 14; ++x ) {
                const auto cell = origin + tripoint_rel_sm( x, y, 0 );
                for( const mongroup *group : overmap_buffer.groups_at( cell ) ) {
                    for( const monster &body : group->monsters ) {
                        const auto id = body.is_caol_predator() ? body.predator_state().actor_id :
                                        body.get_value( "fixture_identity" ).str();
                        REQUIRE( found.emplace( id, std::make_pair( group->abs_pos, bytes( body ) ) ).second );
                    }
                }
            }
        }
        return found;
    };
    const auto initial = census();
    REQUIRE( initial.size() == 3 );
    auto previous = initial;
    rng_set_engine_seed( 20261024 );
    for( int step = 0; step < 12; ++step ) {
        calendar::turn += 150_turns; // genuine native cadence; no backlog injection
        overmap_buffer.move_nemesis();
        const auto current = census();
        REQUIRE( current.size() == 3 );
        CHECK( current.at( predator_id ) == initial.at( predator_id ) );
        for( const auto &id : { "ordinary-alpha", "ordinary-beta" } ) {
            CHECK( rl_dist( current.at( id ).first, previous.at( id ).first ) <= 1 );
        }
        previous = current;
        if( step == 5 ) {
            // Actual modern native save/load must keep genuine nemesis
            // groups in their native group scheduler, not migrate them into
            // abstract hordes and claim that as a nemesis reload.
            std::ostringstream saved;
            owner.serialize( saved );
            owner.clear_mon_groups();
            std::istringstream input( saved.str() );
            owner.unserialize( input );
            CHECK( census() == previous );
            CHECK( owner.horde_entities().empty() );
        }
    }
    CHECK( previous.at( "ordinary-alpha" ).first.x() > source.x() );
    CHECK( previous.at( "ordinary-beta" ).first.x() > other.x() );
    // Both existing full-body fragments get a native opportunity when the
    // predator resumes; newly split entries never move twice in one callback.
    get_avatar().setpos( project_to<coords::ms>( source_omt - point( 40, 0 ) ), false );
    for( int step = 0; step < 12; ++step ) {
        calendar::turn += 150_turns;
        overmap_buffer.move_nemesis();
        const auto current = census();
        REQUIRE( current.size() == 3 );
        CHECK( rl_dist( current.at( predator_id ).first, previous.at( predator_id ).first ) <= 1 );
        previous = current;
    }
    CHECK( previous.at( predator_id ).first.x() > source.x() );
}

TEST_CASE( "predator_fullbody_native_save_preserves_items_and_ordinary_group_shape",
           "[zombie_rider][predator_activation][native][serialization]" )
{
    clear_avatar();
    clear_map_without_vision();
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    overmap_buffer.clear();
    set_time( calendar::turn_zero + 100000_turns );
    const point_abs_om address( 3, 4 );
    overmap &owner = overmap_buffer.get( address );
    owner.clear_mon_groups();
    owner.hordes.clear();
    on_out_of_scope cleanup( [&]() {
        clear_map_without_vision();
        overmap_buffer.clear();
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
    } );
    const auto source = project_to<coords::sm>( tripoint_abs_om( address, 0 ) ) +
                        tripoint_rel_sm( 8, 8, 0 );
    mongroup group( GROUP_DEBUG_ZOMBIE_RIDER, source, 0 );
    monster body( mon_zombie_rider );
    body.ensure_predator_state();
    body.set_pos_abs_only( project_to<coords::ms>( source ) );
    body.set_hp( body.get_hp() - 13 );
    body.ammo[itype_id( "9mm" )] = 7;
    body.inv.emplace_back( itype_id( "rock" ), calendar::turn );
    body.inv.emplace_back( itype_id( "backpack" ), calendar::turn );
    REQUIRE( body.inv.back().put_in( item( itype_id( "9mm" ), calendar::turn, 7 ),
                                   pocket_type::CONTAINER ).success() );
    body.dissectable_inv.emplace_back( itype_id( "rock" ), calendar::turn );
    body.tied_item = cata::make_value<item>( itype_id( "rock" ), calendar::turn );
    body.tack_item = cata::make_value<item>( itype_id( "rock" ), calendar::turn );
    body.armor_item = cata::make_value<item>( itype_id( "rock" ), calendar::turn );
    body.storage_item = cata::make_value<item>( itype_id( "rock" ), calendar::turn );
    body.battery_item = cata::make_value<item>( itype_id( "battery" ), calendar::turn );
    group.monsters.push_back( std::move( body ) );
    owner.debug_force_add_group( group );
    auto groups = overmap_buffer.groups_at( source );
    REQUIRE( groups.size() == 1 );
    const auto bytes = []( const monster &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    const auto expected = bytes( groups.front()->monsters.front() );
    const auto original_uid = groups.front()->monsters.front().inv.front().uid().get_value();
    std::ostringstream saved;
    owner.serialize( saved ); // Actual native save owner, including fullbody bin handling.
    CHECK( bytes( groups.front()->monsters.front() ) == expected );
    groups.front()->clear();
    auto reloaded = std::make_unique<overmap>( address );
    std::istringstream input( saved.str() );
    reloaded->unserialize( input );
    auto omt = project_to<coords::omt>( source );
    auto current = reloaded->debug_unsafe_get_groups_at( omt );
    REQUIRE( current.size() == 1 );
    REQUIRE( current.front().get().monsters.size() == 1 );
    CHECK( bytes( current.front().get().monsters.front() ) == expected );
    CHECK( current.front().get().monsters.front().inv.front().uid().get_value() == original_uid );

    // The legacy single-position reader must install the same genuine body
    // by move too, without expanding its nominal population into new actors.
    std::ostringstream legacy;
    {
        JsonOut json( legacy );
        json.start_object();
        json.member( "mongroups" );
        json.start_array();
        json.write( current.front().get() );
        json.end_array();
        json.end_object();
    }
    current.front().get().clear();
    auto legacy_owner = std::make_unique<overmap>( address );
    legacy_owner->unserialize( json_loader::from_string( legacy.str() ).get_object() );
    auto old = legacy_owner->debug_unsafe_get_groups_at( omt );
    REQUIRE( old.size() == 1 );
    REQUIRE( old.front().get().monsters.size() == 1 );
    CHECK( bytes( old.front().get().monsters.front() ) == expected );

    // Full horde snapshots also pass through the native per-entry reader.
    // Release the prior group owner before constructing this distinct codec
    // input, and compare against the accepted native horde body.
    monster horde_body = std::move( old.front().get().monsters.front() );
    old.front().get().monsters.clear();
    auto accepted = legacy_owner->hordes.spawn_entity( horde_body.pos_abs(), horde_body );
    REQUIRE( accepted );
    horde_body = monster();
    const auto horde_expected = bytes( *( *accepted )->second.monster_data );
    std::ostringstream horde_saved;
    legacy_owner->serialize( horde_saved );
    legacy_owner->hordes.clear();
    std::istringstream horde_input( horde_saved.str() );
    legacy_owner->unserialize( horde_input );
    const auto entries = legacy_owner->horde_entities();
    REQUIRE( entries.size() == 1 );
    REQUIRE( entries.front().second->monster_data );
    CHECK( bytes( *entries.front().second->monster_data ) == horde_expected );

    // Old monster_map coordinates migrate through the same real body reader
    // and accepted native horde owner, with no item-identity cloning.
    std::ostringstream old_map;
    {
        JsonOut json( old_map );
        json.start_object();
        json.member( "monster_map" );
        json.start_array();
        json.write( project_remain<coords::om>( source ).remainder_tripoint );
        json.write( *entries.front().second->monster_data );
        json.end_array();
        json.end_object();
    }
    legacy_owner->hordes.clear();
    legacy_owner->unserialize( json_loader::from_string( old_map.str() ).get_object() );
    const auto migrated = legacy_owner->horde_entities();
    REQUIRE( migrated.size() == 1 );
    REQUIRE( migrated.front().second->monster_data );
    CHECK( bytes( *migrated.front().second->monster_data ) == horde_expected );

    // Default serializer still writes the group's position. Anonymous recipe
    // groups keep native binning, separate coordinates and their population.
    std::ostringstream ordinary;
    {
        JsonOut json( ordinary );
        mongroup recipe( GROUP_DEBUG_ZOMBIE_RIDER, source, 4 );
        recipe.serialize( json );
    }
    const auto recipe_json = json_loader::from_string( ordinary.str() ).get_object();
    mongroup decoded_recipe;
    decoded_recipe.deserialize( recipe_json ); // Consume every field through the native reader.
    CHECK( recipe_json.has_array( "abs_pos" ) );
    CHECK( decoded_recipe.abs_pos == source );
    CHECK( decoded_recipe.population == 4 );
    CHECK( decoded_recipe.monsters.empty() );
}


TEST_CASE( "predator_native_nemesis_cross_owner_keeps_fullbody_item_identity",
           "[zombie_rider][predator_activation][native][nemesis]" )
{
    clear_avatar();
    clear_map_without_vision();
    const bool guarded = GENERATE( false, true );
    const int start_offset = GENERATE( 1, 3 );
    CAPTURE( guarded, start_offset );
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    overmap_buffer.clear();
    set_time( calendar::turn_zero + 100000_turns );
    const point_abs_om address( 3, 4 );
    overmap &source_owner = overmap_buffer.get( address );
    overmap &destination_owner = overmap_buffer.get( address + point::east );
    source_owner.clear_mon_groups();
    source_owner.hordes.clear();
    destination_owner.clear_mon_groups();
    destination_owner.hordes.clear();
    on_out_of_scope cleanup( [&]() {
        clear_map_without_vision();
        overmap_buffer.clear();
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
    } );
    if( guarded ) {
        const auto &special = overmap_special_id( "supermarket" ).obj();
        REQUIRE( special.has_flag( "SAFE_AT_WORLDGEN" ) );
        const auto placed = destination_owner.place_special( special,
                            tripoint_om_omt( 0, 4, 0 ), om_direction::type::north,
                            city(), false, true );
        REQUIRE( std::find( placed.begin(), placed.end(), tripoint_om_omt( 0, 4, 0 ) ) != placed.end() );
    }
    const auto source = project_to<coords::sm>( tripoint_abs_om( address, 0 ) ) +
                        tripoint_rel_sm( 2 * OMAPX - start_offset, 8, 0 );
    const auto boundary = source + point( start_offset - 1, 0 );
    source_owner.ter_set( tripoint_om_omt( OMAPX - 1, 4, 0 ), oter_str_id( "road_nesw" ).id() );
    source_owner.ter_set( tripoint_om_omt( OMAPX - 2, 4, 0 ), oter_str_id( "road_nesw" ).id() );
    for( int y = 1; y <= 8; ++y ) {
        for( int x = 0; x < 5; ++x ) {
            destination_owner.ter_set( tripoint_om_omt( x, y, 0 ), oter_str_id( "road_nesw" ).id() );
        }
    }
    if( guarded ) {
        // The native special placement above installed the worldgen guard;
        // no fixture-private cache access or substitute predicate is used.
        REQUIRE( overmap_buffer.groups_at( boundary + point::east ).empty() );
    }
    mongroup group( GROUP_DEBUG_ZOMBIE_RIDER, source, 0 );
    group.horde = true;
    group.behaviour = mongroup::horde_behaviour::nemesis;
    group.nemesis_target = source.xy() + point( 5, 0 );
    monster body( mon_zombie_rider );
    body.ensure_predator_state();
    body.inv.emplace_back( itype_id( "rock" ), calendar::turn );
    const auto actor = body.predator_state().actor_id;
    group.monsters.push_back( std::move( body ) );
    source_owner.debug_force_add_group( group );
    auto initial = overmap_buffer.groups_at( source );
    REQUIRE( initial.size() == 1 );
    const auto uid = initial.front()->monsters.front().inv.front().uid().get_value();
    get_avatar().setpos( project_to<coords::ms>( project_to<coords::omt>( source ) ), false );
    const auto bytes = []( const monster &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    const auto original_body = bytes( initial.front()->monsters.front() );
    if( guarded ) {
        auto progress = source;
        for( int lead = 0; progress != boundary && lead < 12; ++lead ) {
            calendar::turn += 150_turns;
            overmap_buffer.move_nemesis();
            std::size_t count = 0;
            for( int x = 0; x < start_offset; ++x ) {
                for( const mongroup *candidate : overmap_buffer.groups_at( source + point( x, 0 ) ) ) {
                    if( !candidate->monsters.empty() &&
                        candidate->monsters.front().is_caol_predator() &&
                        candidate->monsters.front().predator_state().actor_id == actor ) {
                        ++count;
                        CHECK( rl_dist( candidate->abs_pos, progress ) <= 1 );
                        progress = candidate->abs_pos;
                    }
                }
            }
            REQUIRE( count == 1 );
        }
        REQUIRE( progress == boundary );
        for( int attempt = 0; attempt < 6; ++attempt ) {
            calendar::turn += 150_turns;
            const auto now = calendar::turn;
            overmap_buffer.move_nemesis();
            CHECK( calendar::turn == now );
            const auto retained = overmap_buffer.groups_at( boundary );
            REQUIRE( retained.size() == 1 );
            REQUIRE( retained.front()->monsters.size() == 1 );
            CHECK( retained.front()->abs_pos == boundary );
            CHECK( bytes( retained.front()->monsters.front() ) == original_body );
        }
        std::ostringstream saved;
        source_owner.serialize( saved );
        source_owner.clear_mon_groups();
        std::istringstream input( saved.str() );
        source_owner.unserialize( input );
        const auto retained = overmap_buffer.groups_at( boundary );
        REQUIRE( retained.size() == 1 );
        CHECK( bytes( retained.front()->monsters.front() ) == original_body );
        // Real native retargeting chooses a different crossing outside the
        // worldgen-safe footprint; no actor/lease/clock injection is involved.
        overmap_buffer.signal_nemesis( boundary + point( 5, -5 ) );
    }
    bool crossed = false;
    auto previous = guarded ? boundary : source;
    for( int step = 0; step < 12; ++step ) {
        calendar::turn += 150_turns;
        overmap_buffer.move_nemesis();
        std::size_t count = 0;
        for( int y = -5; y <= 0; ++y ) {
            for( int x = 0; x <= start_offset + 4; ++x ) {
                for( const mongroup *candidate : overmap_buffer.groups_at( source + point( x, y ) ) ) {
                    for( const monster &current : candidate->monsters ) {
                        if( current.is_caol_predator() && current.predator_state().actor_id == actor ) {
                            ++count;
                            REQUIRE( current.inv.size() == 1 );
                            CHECK( current.inv.front().uid().get_value() == uid );
                            CHECK( current.predator_state().handoff_epoch == 0 );
                            CHECK( bytes( current ) == original_body );
                            CHECK( rl_dist( candidate->abs_pos, previous ) <= 1 );
                            previous = candidate->abs_pos;
                            crossed = crossed || project_to<coords::om>( candidate->abs_pos ).xy() != address;
                        }
                    }
                }
            }
        }
        CHECK( count == 1 );
    }
    CHECK( crossed );
}


TEST_CASE( "predator_native_nemesis_new_destination_gets_no_second_opportunity",
           "[zombie_rider][predator_activation][native][nemesis][legacy]" )
{
    clear_avatar();
    clear_map_without_vision();
    overmap_buffer.clear();
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    set_time( calendar::turn_zero + 100000_turns );
    const point_abs_om address( 7351, -8153 );
    const auto destination = address + point::east;
    std::vector<std::filesystem::path> created;
    on_out_of_scope cleanup( [&]() {
        clear_map_without_vision();
        overmap_buffer.clear();
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
        for( const auto &path : created ) {
            std::filesystem::remove( path );
        }
    } );
    for( const auto &location : { address, destination } ) {
        const auto path = world_generator->active_world->has_compression_enabled() ?
                          PATH_INFO::current_dimension_save_path() / zzip_overmap_directory /
                          overmapbuffer::terrain_filename( location ) + zzip_suffix :
                          PATH_INFO::current_dimension_save_path() / overmapbuffer::terrain_filename( location );
        REQUIRE_FALSE( std::filesystem::exists( path.get_unrelative_path() ) );
        created.push_back( path.get_unrelative_path() );
        created.push_back( overmapbuffer::player_filename( location ).get_unrelative_path() );
    }
    const auto source = project_to<coords::sm>( tripoint_abs_om( address, 0 ) ) +
                        tripoint_rel_sm( 2 * OMAPX - 2, 8, 0 );
    std::string actor;
    std::int64_t uid = 0;
    {
        auto input = std::make_unique<overmap>( address );
        auto empty_destination = std::make_unique<overmap>( destination );
        input->ter_set( tripoint_om_omt( OMAPX - 1, 4, 0 ), oter_str_id( "road_nesw" ).id() );
        empty_destination->ter_set( tripoint_om_omt( 0, 4, 0 ), oter_str_id( "road_nesw" ).id() );
        mongroup group( GROUP_DEBUG_ZOMBIE_RIDER, source, 0 );
        group.horde = true;
        group.behaviour = mongroup::horde_behaviour::nemesis;
        group.nemesis_target = source.xy() + point( 5, 0 );
        monster body( mon_zombie_rider );
        body.ensure_predator_state();
        body.set_pos_abs_only( project_to<coords::ms>( source ) );
        body.inv.emplace_back( itype_id( "rock" ), calendar::turn );
        actor = body.predator_state().actor_id;
        group.monsters.push_back( std::move( body ) );
        input->debug_force_add_group( group );
        auto omt = project_to<coords::omt>( source );
        auto initial = input->debug_unsafe_get_groups_at( omt );
        REQUIRE( initial.size() == 1 );
        uid = initial.front().get().monsters.front().inv.front().uid().get_value();
        input->save();
        empty_destination->save();
    }
    // Only the source is admitted. Cold discovery may decode compact facts,
    // but it must not publish the other native file as a resident owner.
    overmap_buffer.get( address );
    REQUIRE_FALSE( overmap_buffer.find_loaded_overmap( destination ) );
    get_avatar().setpos( project_to<coords::ms>( project_to<coords::omt>( source ) ), false );
    bool crossed = false;
    auto previous = source;
    for( int attempt = 0; attempt < 12 && !crossed; ++attempt ) {
        calendar::turn += 150_turns;
        overmap_buffer.move_nemesis();
        if( !overmap_buffer.find_loaded_overmap( destination ) ) {
            std::size_t count = 0;
            for( int x = 0; x < 2; ++x ) {
                for( const mongroup *remaining : overmap_buffer.groups_at( source + point( x, 0 ) ) ) {
                    ++count;
                    CHECK( rl_dist( remaining->abs_pos, previous ) <= 1 );
                    previous = remaining->abs_pos;
                }
            }
            REQUIRE( count == 1 );
            continue;
        }
        const auto arrived = overmap_buffer.groups_at( source + point( 2, 0 ) );
        REQUIRE( arrived.size() == 1 );
        REQUIRE( arrived.front()->monsters.size() == 1 );
        const monster &body = arrived.front()->monsters.front();
        CHECK( arrived.front()->abs_pos == source + point( 2, 0 ) );
        CHECK( rl_dist( arrived.front()->abs_pos, previous ) == 1 ); // one crossing opportunity
        CHECK( body.predator_state().actor_id == actor );
        CHECK( body.predator_state().handoff_epoch == 0 );
        REQUIRE( body.inv.size() == 1 );
        CHECK( body.inv.front().uid().get_value() == uid );
        crossed = true;
    }
    CHECK( crossed );
}


TEST_CASE( "predator_native_heavy_evolution_preserves_items_through_motion_and_blockage",
           "[zombie_rider][predator_activation][native][evolution]" )
{
    const bool blocked = GENERATE( false, true );
    CAPTURE( blocked );
    clear_avatar();
    clear_map_without_vision();
    const auto old_time = calendar::turn;
    const auto old_start = calendar::start_of_cataclysm;
    const auto old_avatar = get_avatar().pos_abs();
    overmap_buffer.clear();
    calendar::start_of_cataclysm = calendar::turn_zero;
    calendar::turn = calendar::start_of_cataclysm +
                     zombie_rider_overmap_ai::mature_world_gate_seasons * calendar::season_length();
    on_out_of_scope cleanup( [&]() {
        clear_map_without_vision();
        overmap_buffer.clear();
        get_avatar().setpos( old_avatar, false );
        calendar::start_of_cataclysm = old_start;
        set_time( old_time );
    } );
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ) );
    const auto source = here.get_abs( tripoint_bub_ms( 60, 60, 0 ) ) + point( 400, 0 );
    const auto destination = source + point::east;
    REQUIRE_FALSE( here.inbounds( source ) );
    overmap &owner = overmap_buffer.get( project_to<coords::om>( source ).xy() );
    owner.clear_mon_groups();
    owner.hordes.clear();
    std::bitset<24 * 24> known_passable;
    known_passable.set();
    overmap_buffer.set_passable( project_to<coords::omt>( destination ), known_passable );
    monster original( mon_zombie_predator );
    REQUIRE( original.can_upgrade() );
    original.allow_upgrade();
    original.set_pos_abs_only( source );
    original.set_moves( 100 ); // genuine retained native budget, not a catch-up grant
    original.inv.emplace_back( itype_id( "rock" ), calendar::turn );
    auto inserted = owner.hordes.spawn_entity( source, original );
    REQUIRE( inserted );
    const auto uid = ( *inserted )->second.monster_data->inv.front().uid().get_value();
    const auto bytes = []( const monster &body ) {
        std::ostringstream out;
        { JsonOut json( out ); body.serialize( json ); }
        return out.str();
    };
    const auto item_bytes = []( const item &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    const auto original_item = item_bytes( ( *inserted )->second.monster_data->inv.front() );
    overmap_buffer.alert_entity( source, source + point( 10, 0 ), 5 );
    const std::vector<tripoint_abs_ms> approaches = {
        destination, destination + point( 0, 1 ), destination + point( 0, -1 )
    };
    if( blocked ) {
        for( const auto &position : approaches ) {
            REQUIRE( owner.hordes.spawn_entity( position, mtype_id( "mon_zombie" ) ) );
            CHECK_FALSE( overmap_buffer.passable( position ) );
        }
    }
    // Select a reproducible native weighted roll, not a forced type or a
    // substituted evolution result. The real scheduler still owns poly and
    // generated inventory after the group makes this choice.
    unsigned seed = 0;
    for( ; seed < 2048; ++seed ) {
        rng_set_engine_seed( seed );
        if( MonsterGroupManager::GetRandomMonsterFromGroup( GROUP_ZOMBIE_PREDATOR_UPGRADE ) ==
            mon_zombie_rider ) {
            break;
        }
    }
    REQUIRE( seed < 2048 );
    rng_set_engine_seed( seed );
    overmap_buffer.move_hordes();
    const auto relative_source = project_remain<coords::om>( source ).remainder_tripoint;
    const auto relative_destination = project_remain<coords::om>( destination ).remainder_tripoint;
    if( blocked ) {
        const horde_entity *retained = owner.entity_at( relative_source );
        REQUIRE( retained );
        REQUIRE( retained->monster_data );
        // Native ordinary evolution is independent of route viability. Do
        // not turn this terrain refusal into cancellation of a real upgrade.
        REQUIRE( retained->get_type()->id == mon_zombie_rider );
        REQUIRE_FALSE( retained->monster_data->inv.empty() );
        CHECK( retained->monster_data->inv.front().uid().get_value() == uid );
        CHECK( item_bytes( retained->monster_data->inv.front() ) == original_item );
        CHECK( retained->monster_data->pos_abs() == source );
        // Ordinary blocked pursuit still receives its one native speed credit.
        CHECK( retained->moves == 100 + mon_zombie_rider.obj().speed );
        for( const auto &position : approaches ) {
            const auto occupant = owner.hordes.find( project_remain<coords::om>( position ).remainder_tripoint );
            REQUIRE( occupant != owner.hordes.end() );
            owner.hordes.erase( occupant );
        }
        calendar::turn += 1_turns;
        rng_set_engine_seed( seed );
        overmap_buffer.move_hordes();
    }
    const horde_entity *arrived = owner.entity_at( relative_destination );
    REQUIRE( arrived );
    REQUIRE( arrived->monster_data );
    REQUIRE( arrived->get_type()->id == mon_zombie_rider );
    REQUIRE_FALSE( arrived->monster_data->inv.empty() );
    CHECK( arrived->monster_data->inv.front().uid().get_value() == uid );
    CHECK( item_bytes( arrived->monster_data->inv.front() ) == original_item );
    CHECK( arrived->monster_data->predator_state().handoff_epoch == 0 );
    REQUIRE( arrived->monster_data->ammo.count( itype_id( "zombie_rider_tainted_bone_arrow" ) ) == 1 );
    CHECK( arrived->monster_data->ammo.at( itype_id( "zombie_rider_tainted_bone_arrow" ) ) == 18 );
    CHECK_FALSE( owner.entity_at( relative_source ) );
    const int carrier_moves = arrived->moves;
    const int body_moves = arrived->monster_data->get_moves();
    overmap_buffer.move_hordes(); // settle first same-world body binding, never another transport
    REQUIRE( owner.entity_at( relative_destination ) );
    CHECK( owner.entity_at( relative_destination )->moves == carrier_moves );
    CHECK( owner.entity_at( relative_destination )->monster_data->get_moves() == body_moves );
    const auto once = bytes( *owner.entity_at( relative_destination )->monster_data );
    overmap_buffer.move_hordes();
    REQUIRE( owner.entity_at( relative_destination ) );
    CHECK( bytes( *owner.entity_at( relative_destination )->monster_data ) == once );
}

TEST_CASE( "predator_commit_keeps_original_items_and_native_generated_suffix",
           "[zombie_rider][predator_activation][native][ownership]" )
{
    monster source( mtype_id( "mon_zombie" ) );
    source.inv.emplace_back( itype_id( "rock" ), calendar::turn );
    source.dissectable_inv.emplace_back( itype_id( "rock" ), calendar::turn );
    source.tied_item = cata::make_value<item>( itype_id( "rock" ), calendar::turn );
    source.tack_item = cata::make_value<item>( itype_id( "rock" ), calendar::turn );
    source.armor_item = cata::make_value<item>( itype_id( "rock" ), calendar::turn );
    source.storage_item = cata::make_value<item>( itype_id( "rock" ), calendar::turn );
    source.battery_item = cata::make_value<item>( itype_id( "battery" ), calendar::turn );
    const auto item_bytes = []( const item &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    const auto original_item = item_bytes( source.inv.front() );
    const auto uid = source.inv.front().uid().get_value();
    const auto dissection_uid = source.dissectable_inv.front().uid().get_value();
    const auto tied_uid = source.tied_item->uid().get_value();
    const auto tack_uid = source.tack_item->uid().get_value();
    const auto armor_uid = source.armor_item->uid().get_value();
    const auto storage_uid = source.storage_item->uid().get_value();
    const auto battery_uid = source.battery_item->uid().get_value();
    monster candidate( source );
    CHECK( candidate.inv.front().uid().get_value() != uid ); // ordinary copies still clone
    candidate.poly( mon_zombie_rider ); // actual native evolution preparation
    REQUIRE( candidate.inv.size() >= source.inv.size() );
    CHECK( source.inv.front().uid().get_value() == uid ); // preparation/refusal did not release owner
    // A deterministic appended item supplements any native randomized drops;
    // every generated suffix identity must survive the accepted commit.
    candidate.inv.emplace_back( itype_id( "rock" ), calendar::turn );
    std::vector<std::int64_t> generated;
    for( std::size_t i = source.inv.size(); i < candidate.inv.size(); ++i ) {
        generated.push_back( candidate.inv[i].uid().get_value() );
    }
    const auto hp = candidate.get_hp();
    const auto ammo = candidate.ammo;
    const auto actor = candidate.predator_state().actor_id;
    source.transfer_owned_items_to( candidate );
    CHECK( source.inv.empty() );
    CHECK( source.dissectable_inv.empty() );
    CHECK_FALSE( source.tied_item );
    CHECK_FALSE( source.battery_item );
    REQUIRE( candidate.inv.size() == generated.size() + 1 );
    CHECK( candidate.inv.front().uid().get_value() == uid );
    CHECK( item_bytes( candidate.inv.front() ) == original_item );
    for( std::size_t i = 0; i < generated.size(); ++i ) {
        CHECK( candidate.inv[i + 1].uid().get_value() == generated[i] );
    }
    CHECK( candidate.dissectable_inv.front().uid().get_value() == dissection_uid );
    CHECK( candidate.tied_item->uid().get_value() == tied_uid );
    CHECK( candidate.tack_item->uid().get_value() == tack_uid );
    CHECK( candidate.armor_item->uid().get_value() == armor_uid );
    CHECK( candidate.storage_item->uid().get_value() == storage_uid );
    CHECK( candidate.battery_item->uid().get_value() == battery_uid );
    CHECK( candidate.get_hp() == hp );
    CHECK( candidate.ammo == ammo );
    CHECK( candidate.predator_state().actor_id == actor );
}

TEST_CASE( "predator_interval_preserves_suspended_debt_and_zero_credit_reload",
           "[zombie_rider][predator_activation][clock]" )
{
    predator_activation::activity_interval interval;
    interval.evaluate( 100, true );
    CHECK( interval.active_at( 100 ) == 0 );
    interval.evaluate( 105, true );
    CHECK( interval.active_delta == 5 );
    CHECK( interval.active_at( 105 ) == 5 );
    interval.evaluate( 105, false );
    CHECK( interval.active_delta == 0 );
    CHECK( interval.suspension_at( 205 ) == 100 );
    CHECK( interval.active_at( 205 ) == 5 );
    const auto bytes = []( const predator_activation::activity_interval &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    const auto frozen = bytes( interval );
    predator_activation::activity_interval reloaded;
    reloaded.deserialize( json_loader::from_string( frozen ).get_object() );
    CHECK( bytes( interval ) == frozen ); // serialization itself settles nothing
    CHECK( bytes( reloaded ) == frozen );
    reloaded.evaluate( 205, false );
    CHECK( reloaded.active_delta == 0 );
    CHECK( reloaded.active_at( 205 ) == 5 );
    CHECK( reloaded.suspended_turns == 100 );
    reloaded.evaluate( 205, true );
    CHECK( reloaded.active_delta == 0 );
    reloaded.evaluate( 206, true );
    CHECK( reloaded.active_delta == 1 );
    CHECK( reloaded.active_at( 206 ) == 6 );
    const auto once = bytes( reloaded );
    reloaded.evaluate( 206, true );
    CHECK( bytes( reloaded ) == once );
    reloaded.evaluate( 200, false );
    CHECK( bytes( reloaded ) == once ); // an older callback cannot rewind ownership
}

TEST_CASE( "predator_compact_range_interval_uses_whole_authenticated_living_band",
           "[zombie_rider][predator_activation][clock]" )
{
    using namespace predator_activation;
    world_state world;
    zombie_rider_overmap_ai::rider_band_registry bands;
    const point_abs_omt center( 500, 500 );
    const auto near = project_to<coords::ms>( tripoint_abs_omt( center + point( 39, 0 ), 4 ) );
    const auto far = project_to<coords::ms>( tripoint_abs_omt( center + point( 41, 0 ), -3 ) );
    CHECK( world.confirm( "near", "mon_zombie_rider", 7, representation::abstract, near, 100 ) );
    CHECK( world.confirm( "far", "mon_zombie_rider", 9, representation::abstract, far, 100 ) );
    CHECK( bands.register_encounter( {"near", "far", 100, true, true} ) );
    world.refresh( center, 100, bands );
    CHECK( world.find( "near" )->activity.active );
    CHECK( world.find( "far" )->activity.active );
    world.refresh( center, 105, bands );
    CHECK( world.find( "far" )->activity.active_at( 105 ) == 5 );
    CHECK_FALSE( world.confirm( "near", "mon_zombie_rider", 7,
                               representation::abstract, near + point::east, 105 ) );
    CHECK_FALSE( world.confirm( "near", "mon_zombie_rider", 6,
                               representation::abstract, near, 105 ) );
    CHECK( world.owns( "near", 7, representation::abstract, near ) );
    CHECK_FALSE( world.transfer( "near", 8, representation::abstract,
                                representation::local, near, 105 ) );
    CHECK( world.owns( "near", 7, representation::abstract, near ) );
    CHECK( world.transfer( "near", 7, representation::abstract,
                          representation::local, near, 105 ) );
    CHECK_FALSE( world.transfer( "near", 7, representation::abstract,
                                representation::local, near, 105 ) );
    CHECK( world.find( "near" )->epoch == 8 );
    // No real tracker body authenticated this local destination. Metadata is
    // never sufficient as a new surviving local range witness.
    world.refresh( center, 105, bands );
    CHECK_FALSE( world.find( "near" )->activity.active );
    CHECK_FALSE( world.find( "far" )->activity.active );
    world.record_death( "near", 8, 105 );
    CHECK_FALSE( world.confirm( "near", "mon_zombie_rider", 8,
                               representation::local, near, 105 ) );
    world.refresh( center, 205, bands );
    CHECK( world.find( "far" )->activity.active_at( 205 ) == 5 );
    const auto exactly40 = project_to<coords::omt>( far ).xy() - point( 40, 0 );
    world.refresh( exactly40, 205, bands );
    CHECK( world.find( "far" )->activity.active );
    CHECK( world.find( "far" )->activity.active_delta == 0 );
    world.refresh( exactly40, 206, bands );
    CHECK( world.find( "far" )->activity.active_delta == 1 );
    CHECK( world.find( "far" )->activity.active_at( 206 ) == 6 );
    world.refresh( center, 206, bands ); // genuine frozen boundary before save/reload
    std::ostringstream out;
    { JsonOut json( out ); world.serialize( json ); }
    world_state loaded;
    loaded.deserialize( json_loader::from_string( out.str() ).get_object() );
    CHECK_FALSE( loaded.owns( "far", 9, representation::abstract, far ) );
    CHECK( loaded.confirm( "far", "mon_zombie_rider", 9, representation::abstract, far, 306 ) );
    loaded.refresh( center, 306, bands );
    CHECK_FALSE( loaded.find( "far" )->activity.active );
    CHECK( loaded.find( "far" )->activity.active_at( 306 ) == 6 );
}

TEST_CASE( "predator_same_turn_native_remove_and_clear_recheck_band_witness",
           "[zombie_rider][predator_activation][native]" )
{
    clear_avatar();
    clear_map_without_vision();
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    const auto old_world = overmap_buffer.global_state;
    overmap_buffer.global_state = {};
    set_time_to_day();
    map &here = get_map();
    const tripoint_bub_ms near( 47, 60, 0 );
    const tripoint_bub_ms far( 92, 60, 0 );
    for( int y = 46; y < 75; ++y ) {
        for( int x = 34; x < 111; ++x ) {
            here.ter_set( tripoint_bub_ms( x, y, 0 ), ter_id( "t_grass" ) );
            here.furn_clear( tripoint_bub_ms( x, y, 0 ) );
        }
    }
    refresh_pathing_cache( here );
    get_avatar().setpos( here, tripoint_bub_ms( 72, 68, 0 ) );
    monster *first = g->place_critter_at( mon_zombie_rider, near );
    monster *second = g->place_critter_at( mon_zombie_rider, far );
    REQUIRE( first );
    REQUIRE( second );
    on_out_of_scope restore( [&]() {
        g->clear_zombies();
        overmap_buffer.global_state = old_world;
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
    } );
    first->on_load();
    second->on_load();
    REQUIRE( first->sees( here, *second ) );
    REQUIRE( second->sees( here, *first ) );
    REQUIRE_FALSE( here.route( *first, pathfinding_target::point( far ) ).empty() );
    overmap_buffer.reconcile_rider_band_encounters();
    const auto first_id = first->predator_state().actor_id;
    const auto second_id = second->predator_state().actor_id;
    auto &bands = overmap_buffer.global_state.zombie_rider_bands;
    REQUIRE( bands.in_same_band( first_id, second_id ) );
    const auto omt = project_to<coords::omt>( first->pos_abs() );
    get_avatar().setpos( project_to<coords::ms>( omt - point( 39, 0 ) ), false );
    auto &world = overmap_buffer.global_state.predator_activity;
    const auto query = [&]() {
        world.refresh( get_avatar().pos_abs_omt().xy(), to_turn<int>( calendar::turn ), bands );
    };
    query();
    CHECK( world.find( first_id )->activity.active );
    CHECK( world.find( second_id )->activity.active ); // genuine surviving peer at41
    g->remove_zombie( *first );
    query(); // unchanged world turn and avatar; no manual invalidation
    CHECK_FALSE( world.find( first_id )->activity.active );
    CHECK_FALSE( world.find( second_id )->activity.active );
    CHECK( world.find( first_id )->owner == predator_activation::representation::local );
    CHECK( bands.living_members( second_id ) == 2 ); // absence is not casualty
    get_avatar().setpos( project_to<coords::ms>( omt - point( 38, 0 ) ), false );
    query(); // the remaining survivor is now exactly40
    CHECK( world.find( second_id )->activity.active );
    g->clear_zombies();
    query();
    CHECK_FALSE( world.find( second_id )->activity.active );
    CHECK( bands.living_members( second_id ) == 2 );
    // Actual death is a separate native owner transition. Keep the avatar in
    // its real local map while querying the last survivor's frozen boundary.
    get_avatar().setpos( here, tripoint_bub_ms( 72, 68, 0 ) );
    monster *last = g->place_critter_at( mon_zombie_rider, near );
    REQUIRE( last );
    last->on_load();
    const auto last_id = last->predator_state().actor_id;
    const auto frozen_center = last->pos_abs_omt().xy() - point( 41, 0 );
    world.refresh( frozen_center, to_turn<int>( calendar::turn ), bands );
    CHECK_FALSE( world.find( last_id )->activity.active );
    last->die( &here, nullptr );
    CHECK( last->is_dead() );
    world.refresh( frozen_center, to_turn<int>( calendar::turn ), bands );
    REQUIRE( world.find( last_id ) );
    CHECK( world.find( last_id )->owner == predator_activation::representation::dead );
    CHECK_FALSE( world.find( last_id )->position );
    CHECK_FALSE( world.find( last_id )->activity.active );
}

TEST_CASE( "predator_cold_bootstrap_authenticates_existing_native_bodies_only",
           "[zombie_rider][predator_activation][legacy][native]" )
{
    clear_avatar();
    clear_map_without_vision();
    overmap_buffer.clear();
    restore_on_out_of_scope<time_point> restore_time( calendar::turn );
    set_time( calendar::turn_zero + 100000_turns );
    // Previous real save/load callers may have left genuine native actors.
    // Authenticate their baseline through the same cold reader; do not filter
    // them out or delete their files to force a three-actor expectation.
    overmap_buffer.bootstrap_predator_positions();
    std::ostringstream prior_ledger;
    { JsonOut json( prior_ledger ); overmap_buffer.global_state.predator_activity.serialize( json ); }
    const auto prior_state = json_loader::from_string( prior_ledger.str() ).get_object();
    prior_state.allow_omitted_members();
    std::map<std::string, std::string> prior_records;
    std::map<std::string, bool> prior_confirmed;
    std::map<std::string, std::string> prior_native_owner;
    std::set<point_abs_om> resident_prior_owners;
    for( const JsonObject actor : prior_state.get_array( "actors" ) ) {
        actor.allow_omitted_members();
        const auto id = actor.get_string( "actor_id" );
        std::ostringstream out;
        { JsonOut json( out ); overmap_buffer.global_state.predator_activity.find( id )->serialize( json ); }
        prior_records.emplace( id, out.str() );
        prior_confirmed.emplace( id,
                                 overmap_buffer.global_state.predator_activity.find( id )->confirmed );
        const auto *record = overmap_buffer.global_state.predator_activity.find( id );
        if( record->position ) {
            const auto address = project_to<coords::om>( *record->position ).xy();
            if( overmap_buffer.find_loaded_overmap( address ) ) {
                resident_prior_owners.insert( address );
            }
            const auto path = world_generator->active_world->has_compression_enabled() ?
                              PATH_INFO::current_dimension_save_path() / zzip_overmap_directory /
                              overmapbuffer::terrain_filename( address ) + zzip_suffix :
                              PATH_INFO::current_dimension_save_path() / overmapbuffer::terrain_filename( address );
            prior_native_owner.emplace( id, std::string( "resident=" ) +
                                        ( overmap_buffer.find_loaded_overmap( address ) ? "yes" : "no" ) +
                                        " file=" + ( std::filesystem::exists( path.get_unrelative_path() ) ? "yes" : "no" ) );
        }
    }
    // Some preceding fixtures leave genuine resident bodies without saving
    // their containers. A cold disk reader cannot authenticate discarded RAM.
    // Persist the actual native owners, not a fabricated actor projection.
    std::vector<std::filesystem::path> created_prior_files;
    on_out_of_scope cleanup_prior_files( [&]() {
        clear_map_without_vision();
        overmap_buffer.clear();
        for( const auto &path : created_prior_files ) {
            std::filesystem::remove( path );
        }
    } );
    for( const auto &address : resident_prior_owners ) {
        overmap *owner = overmap_buffer.find_loaded_overmap( address );
        REQUIRE( owner );
        const auto terrain = world_generator->active_world->has_compression_enabled() ?
                             PATH_INFO::current_dimension_save_path() / zzip_overmap_directory /
                             overmapbuffer::terrain_filename( address ) + zzip_suffix :
                             PATH_INFO::current_dimension_save_path() / overmapbuffer::terrain_filename( address );
        for( const auto &path : { terrain, overmapbuffer::player_filename( address ) } ) {
            if( !std::filesystem::exists( path.get_unrelative_path() ) ) {
                created_prior_files.push_back( path.get_unrelative_path() );
            }
        }
        owner->save();
    }
    overmap_buffer.clear();
    // Native clock reload resets confirmation, preserves stable legacy aliases,
    // and requires the measured pass to authenticate every real owner again.
    overmap_buffer.global_state.predator_activity.deserialize(
        json_loader::from_string( prior_ledger.str() ).get_object() );
    const point_abs_om address( 7311, -8123 );
    REQUIRE_FALSE( overmap_buffer.get_existing( address ) );
    const auto path = world_generator->active_world->has_compression_enabled() ?
                      PATH_INFO::current_dimension_save_path() / zzip_overmap_directory /
                      overmapbuffer::terrain_filename( address ) + zzip_suffix :
                      PATH_INFO::current_dimension_save_path() / overmapbuffer::terrain_filename( address );
    const auto player_path = overmapbuffer::player_filename( address );
    REQUIRE_FALSE( std::filesystem::exists( path.get_unrelative_path() ) );
    REQUIRE_FALSE( std::filesystem::exists( player_path.get_unrelative_path() ) );
    on_out_of_scope cleanup( [&]() {
        clear_map_without_vision();
        overmap_buffer.clear();
        std::filesystem::remove( path.get_unrelative_path() );
        std::filesystem::remove( player_path.get_unrelative_path() );
    } );
    const auto position = project_to<coords::ms>( tripoint_abs_om( address, 0 ) ) +
                          tripoint_rel_ms( 24, 24, 0 );
    auto source = std::make_unique<overmap>( address );
    monster heavy( mon_zombie_rider );
    heavy.set_hp( heavy.get_hp() - 11 );
    heavy.add_item( item( itype_id( "rock" ), calendar::turn ) );
    heavy.ensure_predator_state();
    const auto id = heavy.predator_state().actor_id;
    REQUIRE( source->hordes.spawn_entity( position, heavy ) );
    REQUIRE( source->hordes.spawn_entity( position + point::east, mon_zombie_rider ) );
    // Native group reader retains a real body but never expands its nominal
    // population in the actor-only cold discovery mode.
    mongroup group( GROUP_DEBUG_ZOMBIE_RIDER, project_to<coords::sm>( position ), 99 );
    monster grouped( mon_zombie_rider );
    grouped.ensure_predator_state();
    const auto grouped_id = grouped.predator_state().actor_id;
    group.monsters.push_back( grouped );
    std::ostringstream encoded;
    {
        JsonOut json( encoded );
        json.start_object();
        json.member( "monster_groups" );
        json.start_array();
        json.start_array();
        json.write( group );
        json.write( std::vector<tripoint_om_sm>{project_remain<coords::om>( group.abs_pos ).remainder_tripoint} );
        json.end_array();
        json.end_array();
        json.end_object();
    }
    source->unserialize( json_loader::from_string( encoded.str() ).get_object() );
    source->save();
    source.reset();
    const auto size = std::filesystem::file_size( path.get_unrelative_path() );
    const bool compressed = world_generator->active_world->has_compression_enabled();
    const auto input_root = compressed ? PATH_INFO::current_dimension_save_path() / zzip_overmap_directory :
                            PATH_INFO::current_dimension_save_path();
    std::size_t input_files = 0;
    std::uintmax_t input_bytes = 0;
    for( const auto &entry : std::filesystem::directory_iterator( input_root.get_unrelative_path() ) ) {
        const auto filename = entry.path().filename().u8string();
        int x = 0;
        int y = 0;
        char prefix = 0;
        char dot_x = 0;
        char dot_y = 0;
        std::string suffix;
        std::istringstream name( filename );
        if( entry.is_regular_file() && name >> prefix >> dot_x >> x >> dot_y >> y &&
            prefix == 'o' && dot_x == '.' && dot_y == '.' ) {
            std::getline( name, suffix );
            if( suffix == ( compressed ? zzip_suffix : "" ) ) {
                ++input_files;
                input_bytes += entry.file_size();
            }
        }
    }
    REQUIRE( input_files >= 1 );
    const auto wall = std::chrono::steady_clock::now();
    const auto cpu = std::clock();
    overmap_buffer.bootstrap_predator_positions();
    const auto elapsed_cpu = static_cast<double>( std::clock() - cpu ) / CLOCKS_PER_SEC;
    const auto elapsed_wall = std::chrono::duration<double>(
                                  std::chrono::steady_clock::now() - wall ).count();
    REQUIRE_FALSE( overmap_buffer.find_loaded_overmap( address ) );
    auto &world = overmap_buffer.global_state.predator_activity;
    const auto *record = world.find( id );
    REQUIRE( record );
    CHECK( record->confirmed );
    CHECK( record->position == position );
    CHECK( record->owner == predator_activation::representation::abstract );
    CHECK( record->activity.adopted_world == 100000 );
    CHECK( record->activity.active_at( 100000 ) == 0 );
    const auto *group_record = world.find( grouped_id );
    REQUIRE( group_record );
    CHECK( group_record->owner == predator_activation::representation::group );
    CHECK( group_record->position == project_to<coords::ms>( group.abs_pos ) );
    std::ostringstream ledger;
    { JsonOut json( ledger ); world.serialize( json ); }
    const JsonObject state = json_loader::from_string( ledger.str() ).get_object();
    state.allow_omitted_members(); // explicit ledger projection; full reader covered separately
    CHECK( state.get_array( "actors" ).size() == prior_records.size() + 3 );
    for( const auto &prior : prior_records ) {
        const auto *record = world.find( prior.first );
        REQUIRE( record );
        CAPTURE( prior.first, static_cast<int>( record->owner ), prior_confirmed.at( prior.first ),
                 record->position ? record->position->to_string() : "absent",
                 prior_native_owner.count( prior.first ) ? prior_native_owner.at( prior.first ) : "absent" );
        CHECK( record->confirmed );
        std::ostringstream out;
        { JsonOut json( out ); record->serialize( json ); }
        CHECK( out.str() == prior.second );
    }
    CHECK( std::filesystem::file_size( path.get_unrelative_path() ) == size );
    overmap_buffer.bootstrap_predator_positions();
    CHECK_FALSE( overmap_buffer.find_loaded_overmap( address ) );
    if( std::getenv( "CAOL_RESIDENT_HISTORY_MEASURE" ) ) {
        std::cout << "predator_cold_native_bootstrap declared_fixture_files=1 fixture_bytes=" << size
                  << " actual_input_files=" << input_files << " actual_input_bytes=" << input_bytes
                  << " fixture_actors=3 prior_actors=" << prior_records.size()
                  << " saved_prior_resident_owners=" << resident_prior_owners.size()
                  << " anonymous_population=99 native_owner_static_bytes=" << sizeof( overmap )
                  << " wall_s=" << elapsed_wall;
#if defined(_WIN32)
        std::cout << " crt_elapsed_s=" << elapsed_cpu;
#else
        std::cout << " process_cpu_s=" << elapsed_cpu;
#endif
        std::cout << '\n';
    }
    // The actual normal owner can subsequently load and place the same body;
    // the cold scratch owner was never published or retained.
    overmap &loaded = overmap_buffer.get( address );
    auto entry = loaded.entity_at( project_remain<coords::om>( position ).remainder_tripoint );
    REQUIRE( entry );
    REQUIRE( entry->monster_data );
    CHECK( entry->monster_data->predator_state().actor_id == id );
    CHECK( entry->monster_data->get_hp() == heavy.get_hp() );
    CHECK( entry->monster_data->predator_state().handoff_epoch == 0 );
}

TEST_CASE( "predator_native_body_pause_keeps_fractional_anchors_and_once_only_funding",
           "[predator_activation][native][biology]" )
{
    const auto type = GENERATE( values<mtype_id>( {
        mon_zombie_rider, mtype_id( "mon_writhing_stalker" )
    } ) );
    CAPTURE( type );
    clear_avatar();
    clear_map_without_vision();
    const auto old_world = overmap_buffer.global_state;
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    overmap_buffer.global_state = {};
    set_time( calendar::turn_zero + 100013_turns ); // retain a non-day-aligned phase
    map &here = get_map();
    const tripoint_bub_ms local( 63, 63, 0 );
    here.ter_set( local, ter_id( "t_grass" ) );
    get_avatar().setpos( here, local + point( 5, 0 ) );
    on_out_of_scope cleanup( [&]() {
        g->clear_zombies();
        overmap_buffer.global_state = old_world;
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
    } );
    monster *body = g->place_critter_at( type, local );
    REQUIRE( body );
    body->on_load();
    body->set_baby_timer( calendar::turn + 61_minutes );
    body->set_moves( 37 );
    const auto position = body->pos_abs();
    const auto where = body->pos_abs_omt();
    const auto id = body->predator_state().actor_id;
    const auto bytes = []( const monster &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    const auto anchor = [&]( const monster &value, const std::string &key ) {
        const auto projection = json_loader::from_string( bytes( value ) ).get_object();
        projection.allow_omitted_members(); // explicit read-only projection, full reader tested separately
        return projection.get_int64( key );
    };
    const auto baby_before = anchor( *body, "baby_timer" );
    const auto update_before = anchor( *body, "last_updated" );
    const auto evolution_before = anchor( *body, "suspended_evolution_time" );
    get_avatar().setpos( project_to<coords::ms>( where - point( 41, 0 ) ), false );
    body->on_unload(); // actual frozen boundary, not a manually injected clock
    const auto frozen = bytes( *body );
    // Native body and compact-clock readers both participate in reload. The
    // same tracked identity must reauthenticate without spending suspended debt.
    std::ostringstream saved_clock;
    { JsonOut json( saved_clock ); overmap_buffer.global_state.predator_activity.serialize( json ); }
    body->deserialize( json_loader::from_string( frozen ).get_object() );
    overmap_buffer.global_state.predator_activity.deserialize(
        json_loader::from_string( saved_clock.str() ).get_object() );
    CHECK( bytes( *body ) == frozen );
    calendar::turn += 2_days + 17_turns;
    body->on_load();
    body->process_turn();
    CHECK( bytes( *body ) == frozen );
    CHECK( body->get_moves() == 37 );
    const auto pending = overmap_buffer.predator_pending_pause( *body );
    CHECK( pending == to_turns<int>( 2_days + 17_turns ) );
    get_avatar().setpos( project_to<coords::ms>( where - point( 40, 0 ) ), false );
    body->on_load();
    CHECK( body->predator_state().actor_id == id );
    CHECK( body->pos_abs() == position );
    CHECK( anchor( *body, "baby_timer" ) == baby_before + pending );
    CHECK( anchor( *body, "last_updated" ) == update_before + pending );
    CHECK( anchor( *body, "suspended_evolution_time" ) == evolution_before + pending );
    const auto resumed = bytes( *body );
    body->on_load();
    CHECK( bytes( *body ) == resumed );
    calendar::turn += 1_turns;
    const int speed = body->get_speed();
    body->process_turn();
    CHECK( body->get_moves() == 37 + speed );
    const auto once = bytes( *body );
    body->process_turn();
    CHECK( bytes( *body ) == once );
}

TEST_CASE( "predator_native_light_expiry_uses_earned_actor_work_not_source_freshness",
           "[zombie_rider][predator_activation][native][light]" )
{
    const auto type = GENERATE( values<mtype_id>( {
        mon_zombie_rider, mtype_id( "mon_writhing_stalker" )
    } ) );
    const bool legacy = GENERATE( false, true );
    CAPTURE( type, legacy );
    clear_avatar();
    clear_map_without_vision();
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    overmap_buffer.clear();
    set_time( calendar::turn_zero + 100000_turns );
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ) );
    const auto position = get_avatar().pos_abs() + point( 400, 0 );
    const auto source = position + point( 24, 0 );
    overmap &owner = overmap_buffer.get( project_to<coords::om>( position ).xy() );
    owner.clear_mon_groups();
    owner.hordes.clear();
    on_out_of_scope cleanup( [&]() {
        overmap_buffer.clear();
        clear_map_without_vision();
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
    } );
    REQUIRE( owner.hordes.spawn_entity( position, type ) );
    int rays = 0;
    const auto visible = [&]( const tripoint_abs_ms &seen ) {
        CHECK( seen == position );
        ++rays;
        return true;
    };
    const auto observed = calendar::turn;
    REQUIRE( owner.hordes.attract_entities_to_light( source, 60, 10, "earned-native-light", observed, visible ) == 1 );
    CHECK( rays == 1 );
    const auto relative = project_remain<coords::om>( position ).remainder_tripoint;
    const auto entity = [&]() -> horde_entity & {
        auto *current = owner.entity_at( relative );
        REQUIRE( current );
        REQUIRE( current->monster_data );
        return *current;
    };
    const auto bytes = []( const monster &body ) {
        std::ostringstream out;
        { JsonOut json( out ); body.serialize( json ); }
        return out.str();
    };
    const auto facts = [&]() {
        const auto &current = entity();
        return std::make_tuple( current.light_sample_id, current.light_observed,
                                current.light_expires, current.light_source );
    };
    const auto original_facts = facts();
    const auto goal = entity().destination;
    const auto strength = entity().tracking_intensity;
    REQUIRE( overmap_buffer.predator_light_work_alive( *entity().monster_data ) );
    bool expired_at_adoption = false;
    if( legacy ) {
        expired_at_adoption = GENERATE( false, true );
        CAPTURE( expired_at_adoption );
        // Produce an old valid body by omitting only the new scalar activation
        // tail from this exact native serializer's caol_predator object. The
        // original identity, native light directive and acquisition facts stay.
        std::string old_body = bytes( *entity().monster_data );
        const auto owner_start = old_body.find( "\"caol_predator\":" );
        const auto tail = old_body.find( ",\"activity_bound\":", owner_start );
        const auto owner_end = old_body.find( '}', tail );
        REQUIRE( owner_start != std::string::npos );
        REQUIRE( tail != std::string::npos );
        REQUIRE( owner_end != std::string::npos );
        const auto schema = old_body.find( "\"schema\":2", owner_start );
        REQUIRE( schema < tail );
        old_body.replace( schema, std::string( "\"schema\":2" ).size(), "\"schema\":1" );
        old_body.erase( tail, owner_end - tail );
        auto decoded = std::make_unique<monster>();
        decoded->deserialize( json_loader::from_string( old_body ).get_object() );
        entity().monster_data = std::move( decoded ); // native reload installs a new body
        REQUIRE_FALSE( entity().monster_data->predator_state().activity_bound );
        REQUIRE( entity().monster_data->predator_state().light_work_sample.empty() );
        overmap_buffer.global_state.predator_activity.clear(); // genuine cold adoption
        if( expired_at_adoption ) {
            calendar::turn += 61_turns;
        }
    }
    get_avatar().setpos( project_to<coords::ms>( project_to<coords::omt>( position ) - point( 41, 0 ) ), false );
    owner.hordes.expire_light_interest( calendar::turn );
    const auto frozen = bytes( *entity().monster_data );
    calendar::turn += 1000_turns;
    owner.hordes.expire_light_interest( calendar::turn );
    CHECK( bytes( *entity().monster_data ) == frozen );
    CHECK( entity().tracking_intensity == strength );
    CHECK( facts() == original_facts );
    CHECK( entity().light_expires < calendar::turn ); // global evidence genuinely expired
    CHECK( entity().destination == goal );
    get_avatar().setpos( project_to<coords::ms>( project_to<coords::omt>( position ) - point( 40, 0 ) ), false );
    owner.hordes.expire_light_interest( calendar::turn );
    if( expired_at_adoption ) {
        CHECK( entity().tracking_intensity == 0 );
        CHECK_FALSE( overmap_buffer.predator_light_work_alive( *entity().monster_data ) );
        CHECK( entity().monster_data->predator_state().light_work_sample.empty() );
        CHECK( facts() == original_facts );
        return; // Stale at adoption never starts a new investigation.
    }
    CHECK( entity().tracking_intensity == strength );
    REQUIRE( overmap_buffer.predator_light_work_alive( *entity().monster_data ) );
    const auto resumed = bytes( *entity().monster_data );
    owner.hordes.expire_light_interest( calendar::turn );
    CHECK( bytes( *entity().monster_data ) == resumed );
    // Re-delivery of the same packet cannot renew an earned budget, and does
    // not rewrite the original observation/expiry into present evidence.
    CHECK( owner.hordes.attract_entities_to_light( source, 60, 10, "earned-native-light", observed, visible ) == 0 );
    CHECK( facts() == original_facts );
    calendar::turn += 59_turns;
    owner.hordes.expire_light_interest( calendar::turn );
    CHECK( entity().tracking_intensity == strength );
    calendar::turn += 1_turns;
    owner.hordes.expire_light_interest( calendar::turn );
    CHECK_FALSE( overmap_buffer.predator_light_work_alive( *entity().monster_data ) );
    CHECK( entity().tracking_intensity == 0 );
    CHECK( entity().light_interest_strength == 0 );
    CHECK( facts() == original_facts );
    const auto exhausted = bytes( *entity().monster_data );
    owner.hordes.expire_light_interest( calendar::turn );
    CHECK( bytes( *entity().monster_data ) == exhausted );
}

TEST_CASE( "predator_native_legacy_adoption_keeps_already_current_work",
           "[predator_activation][native][legacy][ownership]" )
{
    const auto type = GENERATE( values<mtype_id>( {
        mon_zombie_rider, mtype_id( "mon_writhing_stalker" )
    } ) );
    CAPTURE( type );
    clear_avatar();
    clear_map_without_vision();
    overmap_buffer.clear();
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    set_time( calendar::turn_zero + 600_turns );
    map &here = get_map();
    const tripoint_bub_ms local( 63, 63, 0 );
    here.ter_set( local, ter_id( "t_grass" ) );
    get_avatar().setpos( here, local + point( 5, 0 ) );
    on_out_of_scope cleanup( [&]() {
        clear_map_without_vision();
        overmap_buffer.clear();
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
    } );
    monster *body = g->place_critter_at( type, local );
    REQUIRE( body );
    const auto position = body->pos_abs();
    const auto id = body->predator_state().actor_id;
    body->set_hp( body->get_hp() - 11 );
    const int hp = body->get_hp();
    body->set_moves( 47 );
    // A valid older native body can carry current earned work before its
    // first activation binding. Native body absence and this current work's
    // already settled world boundary are different facts.
    if( type == mon_zombie_rider ) {
        zombie_rider_overmap_ai::set_camp_pressure_intent( *body,
                zombie_rider_overmap_ai::rider_camp_pressure_posture::investigate,
                position + point::east, 45, 0 );
    } else {
        auto &state = body->writhing_stalker_state();
        state.advance_to( 580 );
        state.observe_target( 42, 1, position + point::east, 580 );
        state.advance_to( 600 );
        state.phase = writhing_stalker::lifecycle_phase::cooldown;
        state.cooldown_until_turn = 620;
    }
    g->despawn_monster( *body );
    REQUIRE( overmap_buffer.entity_at( position ) );
    overmap_buffer.spawn_monster( project_to<coords::sm>( position ), false );
    monster *restored = get_creature_tracker().creature_at<monster>( position );
    REQUIRE( restored );
    CHECK( restored->predator_state().actor_id == id );
    CHECK( restored->get_hp() == hp );
    CHECK( restored->get_moves() == 47 );
    CHECK( restored->predator_state().handoff_epoch == 2 );
    if( type == mon_zombie_rider ) {
        const auto intent = zombie_rider_overmap_ai::get_camp_pressure_intent( *restored );
        REQUIRE( intent );
        CHECK( intent->turns_remaining == 45 );
    } else {
        CHECK( restored->writhing_stalker_state().cooldown_until_turn == 620 );
        CHECK( restored->writhing_stalker_state().active_turns == 20 );
    }
}


TEST_CASE( "rider_native_cooldown_requires_four_real_active_turns_not_repeated_callbacks",
           "[zombie_rider][predator_activation][native][cooldown_control]" )
{
    restore_on_out_of_scope restore_time( calendar::turn );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    on_out_of_scope cleanup( [] { clear_map_without_vision(); } );
    clear_avatar();
    clear_map_without_vision();
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_zombie_rider_local_arena( here, center );
    get_avatar().setpos( here, center );
    set_time( daylight_time( calendar::turn ) + 2_hours );
    shared_ptr_fast<monster> candidate;
    for( int n = 0; n < 256; ++n ) {
        candidate = make_shared_fast<monster>( mon_zombie_rider );
        const auto &id = candidate->predator_state().actor_id;
        bool occupied = overmap_buffer.global_state.predator_activity.find( id ) != nullptr;
        for( const monster &other : g->all_monsters() ) {
            occupied = occupied || ( other.is_caol_predator() && other.predator_state().actor_id == id );
        }
        if( !occupied ) {
            break;
        }
        candidate.reset();
    }
    REQUIRE( candidate );
    candidate->ammo = candidate->type->starting_ammo;
    monster *rider = g->place_critter_at( candidate, center + point::east * 6 );
    REQUIRE( rider );
    rider->anger = 100;
    rider->aggro_character = true;
    rider->set_special( zombie_rider_bone_bow_shot, 0 );
    rider->set_moves( 100 );
    REQUIRE( rider->sees( here, get_avatar() ) );
    rider->plan();
    const int ammo = rider->ammo[zombie_rider_tainted_bone_arrow];
    rider->move();
    REQUIRE( rider->ammo[zombie_rider_tainted_bone_arrow] == ammo - 1 );
    const auto bytes = [&]() {
        std::ostringstream out;
        { JsonOut json( out ); rider->serialize( json ); }
        return out.str();
    };
    const auto cooldown = [&]() {
        const auto json = json_loader::from_string( bytes() ).get_object();
        json.allow_omitted_members();
        const auto specials = json.get_object( "special_attacks" );
        specials.allow_omitted_members();
        const auto bow = specials.get_object( zombie_rider_bone_bow_shot );
        bow.allow_omitted_members();
        return bow.get_int( "cooldown" );
    };
    REQUIRE( cooldown() == 4 );
    const auto shot = bytes();
    for( int n = 0; n < 4; ++n ) {
        rider->process_turn();
        CHECK( bytes() == shot );
    }
    CHECK_FALSE( rider->special_available( zombie_rider_bone_bow_shot ) );
    for( int n = 1; n <= 4; ++n ) {
        calendar::turn += 1_turns;
        rider->process_turn();
        CHECK( cooldown() == 4 - n );
        CHECK( rider->ammo[zombie_rider_tainted_bone_arrow] == ammo - 1 );
        const auto once = bytes();
        rider->process_turn();
        CHECK( bytes() == once );
    }
    CHECK( rider->special_available( zombie_rider_bone_bow_shot ) );
}

TEST_CASE( "predator_evolution_reload_matches_real_native_owner_default_and_positive_rolls",
           "[zombie_rider][predator_activation][native][evolution_control]" )
{
    restore_on_out_of_scope restore_time( calendar::turn );
    restore_on_out_of_scope restore_start( calendar::start_of_cataclysm );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    const auto rng_before = rng_get_engine();
    on_out_of_scope restore_rng( [&] { rng_get_engine() = rng_before; } );
    on_out_of_scope cleanup( [] { clear_map_without_vision(); } );
    clear_avatar();
    clear_map_without_vision();
    map &here = get_map();
    const tripoint_bub_ms p( 65, 65, 0 );
    here.ter_set( p, ter_id( "t_floor" ) );
    get_avatar().setpos( here, p + point::west * 3 );
    calendar::start_of_cataclysm = calendar::turn_zero;
    const time_duration gate = zombie_rider_overmap_ai::mature_world_gate_seasons * calendar::season_length();
    calendar::turn = calendar::start_of_cataclysm + gate - 1_days;
    monster staged( mon_zombie_predator );
    staged.allow_upgrade();
    staged.try_upgrade( false );
    REQUIRE( staged.type->id == mon_zombie_predator );
    REQUIRE( staged.get_upgrade_time() == 0 );
    std::ostringstream encoded;
    { JsonOut json( encoded ); staged.serialize( json ); }
    bool saw_default = false;
    bool saw_positive = false;
    for( unsigned int seed = 1; seed <= 256 && !( saw_default && saw_positive ); ++seed ) {
        calendar::turn = calendar::start_of_cataclysm + gate - 1_days;
        auto native = make_shared_fast<monster>();
        native->deserialize( json_loader::from_string( encoded.str() ).get_object() );
        REQUIRE( g->place_critter_at( native, p ) );
        native->on_load(); // actual native ordinary owner, before the world gate
        REQUIRE( get_creature_tracker().find( native->pos_abs() ).get() == native.get() );
        REQUIRE( native->type->id == mon_zombie_predator );
        REQUIRE( native->get_upgrade_time() == 0 );
        monster standalone = round_trip_zombie_rider( *native );
        CHECK( standalone.can_upgrade() == native->can_upgrade() );
        CHECK( standalone.get_upgrade_time() == native->get_upgrade_time() );
        calendar::turn = calendar::start_of_cataclysm + gate;
        // No diagnostic group draw: both actual callers receive equal RNG.
        rng_set_engine_seed( seed );
        standalone.try_upgrade( false );
        rng_set_engine_seed( seed );
        native->try_upgrade( false );
        CAPTURE( seed );
        CHECK( standalone.type->id == native->type->id );
        CHECK( standalone.get_upgrade_time() == native->get_upgrade_time() );
        CHECK( standalone.get_hp() == native->get_hp() );
        CHECK( standalone.ammo == native->ammo );
        if( native->type->id == mon_zombie_predator ) {
            saw_default = true;
            CHECK( ( !native->can_upgrade() || native->get_upgrade_time() >
                     to_days<int>( calendar::turn - calendar::turn_zero ) ) );
        } else {
            REQUIRE( native->type->id == mon_zombie_rider );
            saw_positive = true;
            CHECK( native->predator_state().ammo_initialization_version == 1 );
            CHECK( native->ammo[zombie_rider_tainted_bone_arrow] ==
                   mon_zombie_rider->starting_ammo.at( zombie_rider_tainted_bone_arrow ) );
        }
        g->remove_zombie( *native );
    }
    CHECK( saw_default );
    CHECK( saw_positive );
}

// Constructor-only NEW fixtures. Existing copied/reloaded identities are never
// rewritten, and canonical owners remain authoritative across world reloads.
static monster new_scheduler_predator( const mtype_id &type )
{
    for( int attempt = 0; attempt < 256; ++attempt ) {
        monster body( type );
        const auto &id = body.predator_state().actor_id;
        bool used = overmap_buffer.global_state.predator_activity.find( id ) != nullptr;
        for( const monster &other : g->all_monsters() ) {
            used = used || ( other.is_caol_predator() && other.predator_state().actor_id == id );
        }
        if( !used ) {
            return body;
        }
    }
    FAIL( "no unused native constructor identity" );
    std::abort();
}

TEST_CASE( "rider_new_local_fixture_preserves_foreign_and_stale_owner_refusal",
           "[zombie_rider][predator_activation][native][fixture_ownership]" )
{
    restore_on_out_of_scope restore_time( calendar::turn );
    clear_map_without_vision();
    on_out_of_scope cleanup( [] { clear_map_without_vision(); } );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_zombie_rider_local_arena( here, center );
    get_avatar().setpos( here, center );
    set_time( daylight_time( calendar::turn ) + 2_hours );
    monster &source = spawn_new_local_rider_fixture( center + point::east * 6 );
    source.anger = 100;
    source.aggro_character = true;
    source.set_special( zombie_rider_bone_bow_shot, 0 );
    REQUIRE( source.sees( here, get_avatar() ) );
    source.plan(); // the real owner authenticates its actual birth
    REQUIRE( source.get_dest() == get_avatar().pos_abs() );
    const auto id = source.predator_state().actor_id;
    const auto epoch = source.predator_state().handoff_epoch;
    REQUIRE( overmap_buffer.global_state.predator_activity.owns( id, epoch,
             predator_activation::representation::local, source.pos_abs() ) );
    // Intentionally copied identity is NOT a new fixture birth.
    auto foreign = make_shared_fast<monster>( source );
    const bool stale = GENERATE( false, true );
    item carrier( itype_id( "pet_carrier" ), calendar::turn );
    if( stale ) {
        carrier.contain_monster( source.pos_bub() );
        REQUIRE( carrier.has_var( "contained_json" ) );
        const auto *record = overmap_buffer.global_state.predator_activity.find( id );
        REQUIRE( record );
        REQUIRE( record->owner == predator_activation::representation::captured );
        REQUIRE( record->epoch == epoch + 1 );
    }
    const auto canonical = [&]() {
        const auto *record = overmap_buffer.global_state.predator_activity.find( id );
        REQUIRE( record );
        std::ostringstream out;
        { JsonOut json( out ); record->serialize( json ); }
        return out.str();
    };
    const auto body_bytes = []( const monster &body ) {
        std::ostringstream out;
        { JsonOut json( out ); body.serialize( json ); }
        return out.str();
    };
    const auto authority = canonical();
    const auto item_state = carrier.get_var( "contained_json" );
    REQUIRE( g->place_critter_at( foreign, center + point::west * 8 ) == foreign.get() );
    REQUIRE( foreign->predator_state().actor_id == id );
    REQUIRE( foreign->predator_state().handoff_epoch == epoch );
    const auto refused = body_bytes( *foreign );
    foreign->plan();
    foreign->process_turn();
    foreign->move();
    CHECK_FALSE( foreign->move_to( center + point::west * 7, false ) );
    CHECK( body_bytes( *foreign ) == refused );
    CHECK( canonical() == authority );
    CHECK( carrier.get_var( "contained_json" ) == item_state );
    monster &fresh = spawn_new_local_rider_fixture( center + point::south * 8 );
    CHECK( fresh.predator_state().actor_id != id );
    CHECK( overmap_buffer.global_state.predator_activity.find( fresh.predator_state().actor_id ) == nullptr );
    fresh.anger = 100;
    fresh.aggro_character = true;
    fresh.set_special( zombie_rider_bone_bow_shot, 0 );
    REQUIRE( fresh.sees( here, get_avatar() ) );
    fresh.plan();
    CHECK( fresh.get_dest() == get_avatar().pos_abs() );
    CHECK( overmap_buffer.global_state.predator_activity.owns( fresh.predator_state().actor_id,
            fresh.predator_state().handoff_epoch, predator_activation::representation::local,
            fresh.pos_abs() ) );
    CHECK( canonical() == authority );
}

TEST_CASE( "predator_scheduler_services_mixed_owner_after_migo_only_pass_once",
           "[predator_activation][native][scheduler]" )
{
    clear_avatar();
    clear_map_without_vision();
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    overmap_buffer.clear();
    on_out_of_scope cleanup( [&] {
        clear_map_without_vision();
        overmap_buffer.clear();
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
    } );
    set_time( calendar::turn_zero + 200000_turns );
    const point_abs_om address( 3, 4 );
    overmap &owner = overmap_buffer.get( address );
    owner.hordes.clear();
    owner.clear_mon_groups();
    const auto position = project_to<coords::ms>( tripoint_abs_om( address, 0 ) ) +
                          tripoint_rel_ms( 48, 48, 0 );
    const auto where = project_to<coords::omt>( position );
    // Across an OM seam, within40 but outside the ordinary11OMT envelope.
    get_avatar().setpos( project_to<coords::ms>( where - point( 35, 0 ) ), false );
    const auto ordinary_max = project_to<coords::om>( get_avatar().pos_abs_sm() +
                              point( MAPSIZE * 2, MAPSIZE * 2 ) );
    REQUIRE( ordinary_max.x() < address.x() );
    const std::string site_id = "mixed-owner-native-service";
    migo_population::site_record population;
    population.id = site_id;
    population.anchor = where;
    migo_population::member_record member;
    member.token = {site_id, "surviving-committed-member", 1, where};
    member.type = mtype_id( "mon_mi_go" );
    member.home = position;
    member.hp = member.type->hp;
    member.owner = migo_population::representation::abstract;
    member.assignment = migo_population::duty::dispatched;
    REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = site_id;
    site.anchor = where;
    site.operation.emplace();
    site.operation->generation = 1;
    site.operation->members = {{member.token.member_id}};
    site.operation->source.source_omt = where + point::east;
    site.operation->sweep.candidates = {{position + point::east * 3}};
    site.operation->overdue_after_active = 100000;
    site.operation->advance( to_turn<int>( calendar::turn ), 0 );
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster migo( member.type );
    migo.population_identity() = member.token;
    migo.set_pos_abs_only( position );
    migo.set_moves( 0 );
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.phase = migo_tower::member_phase::outbound;
    intent.home = position;
    migo.set_migo_intent( intent );
    auto *migo_owner = owner.retain_monster_payload( position, migo );
    REQUIRE( migo_owner );
    // A genuine native signal makes the real abstract body a due carrier.
    monster predator = new_scheduler_predator( mon_zombie_rider );
    const auto predator_position = position + point::south * 3;
    predator.set_pos_abs_only( predator_position );
    predator.set_moves( 0 );
    auto *predator_owner = owner.retain_monster_payload( predator_position, predator );
    REQUIRE( predator_owner );
    owner.spawn_monster( position + point::south * 6, mon_zombie );
    owner.hordes.signal_entities( position + point::east * 3, 100 );
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    REQUIRE( overmap_buffer.global_state.migo_populations.active_sites.count( site_id ) == 1 );
    REQUIRE( overmap_buffer.global_state.migo_towers.find( site_id )->operation->activity.active );
    REQUIRE( migo_tower::owns_intent( *migo_owner->monster_data, migo_population::representation::abstract ) );
    const auto ordinary_position = position + point::south * 6;
    auto *ordinary = overmap_buffer.entity_at( ordinary_position );
    REQUIRE( ordinary );
    const auto ordinary_before = ordinary->last_processed;
    calendar::turn += 1_turns;
    overmap_buffer.move_hordes();
    const auto *committed = overmap_buffer.global_state.predator_activity.find(
                                predator.predator_state().actor_id );
    REQUIRE( committed );
    REQUIRE( committed->position );
    const auto committed_position = *committed->position;
    CHECK( rl_dist( committed_position, predator_position ) <= 1 );
    predator_owner = overmap_buffer.entity_at( committed_position );
    REQUIRE( predator_owner );
    CHECK( predator_owner->monster_data->predator_state().actor_id == predator.predator_state().actor_id );
    CHECK( predator_owner->last_processed == calendar::turn );
    CHECK( predator_owner->monster_data->predator_state().last_advanced_turn == to_turn<int>( calendar::turn ) );
    CHECK( overmap_buffer.entity_at( ordinary_position )->last_processed == ordinary_before );
    const auto bytes = []( const horde_entity &entity ) {
        std::ostringstream out;
        {
            JsonOut json( out );
            json.start_object();
            json.member( "destination", entity.destination );
            json.member( "tracking", entity.tracking_intensity );
            json.member( "processed", entity.last_processed );
            json.member( "moves", entity.moves );
            json.member( "body", *entity.monster_data );
            json.end_object();
        }
        return out.str();
    };
    // Both real service paths remain once-only at unchanged native time.
    const auto predator_once = bytes( *predator_owner );
    migo_owner = overmap_buffer.entity_at( position );
    REQUIRE( migo_owner );
    CHECK( migo_owner->last_processed == calendar::turn );
    const auto migo_once = bytes( *migo_owner );
    overmap_buffer.move_hordes();
    CHECK( bytes( *overmap_buffer.entity_at( committed_position ) ) == predator_once );
    CHECK( bytes( *overmap_buffer.entity_at( position ) ) == migo_once );
    CHECK( overmap_buffer.entity_at( ordinary_position )->last_processed == ordinary_before );
}

TEST_CASE( "predator_cold_group_scheduler_admits_only_eligible_existing_native_owner",
           "[predator_activation][native][nemesis][legacy]" )
{
    clear_avatar();
    clear_map_without_vision();
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    overmap_buffer.clear();
    set_time( calendar::turn_zero + 300000_turns );
    const int distance = GENERATE( 40, 41 );
    const point_abs_om address( 7361, -8163 );
    const auto path = world_generator->active_world->has_compression_enabled() ?
                      PATH_INFO::current_dimension_save_path() / zzip_overmap_directory /
                      overmapbuffer::terrain_filename( address ) + zzip_suffix :
                      PATH_INFO::current_dimension_save_path() / overmapbuffer::terrain_filename( address );
    const auto player_path = overmapbuffer::player_filename( address );
    REQUIRE_FALSE( std::filesystem::exists( path.get_unrelative_path() ) );
    REQUIRE_FALSE( std::filesystem::exists( player_path.get_unrelative_path() ) );
    on_out_of_scope cleanup( [&] {
        clear_map_without_vision();
        overmap_buffer.clear();
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
        std::filesystem::remove( path.get_unrelative_path() );
        std::filesystem::remove( player_path.get_unrelative_path() );
    } );
    overmap_buffer.bootstrap_predator_positions(); // authenticate existing fixture history too
    auto input = std::make_unique<overmap>( address );
    const auto position = project_to<coords::sm>( tripoint_abs_om( address, 0 ) ) +
                          tripoint_rel_sm( 8, 8, 0 );
    input->ter_set( tripoint_om_omt( 4, 4, 0 ), oter_str_id( "road_nesw" ).id() );
    input->ter_set( tripoint_om_omt( 3, 4, 0 ), oter_str_id( "road_nesw" ).id() );
    mongroup group( GROUP_DEBUG_ZOMBIE_RIDER, position, 0 );
    group.horde = true;
    group.behaviour = mongroup::horde_behaviour::nemesis;
    group.nemesis_target = position.xy() - point( 5, 0 );
    monster body = new_scheduler_predator( mon_zombie_rider );
    body.set_pos_abs_only( project_to<coords::ms>( position ) );
    body.inv.emplace_back( itype_id( "rock" ), calendar::turn );
    const auto id = body.predator_state().actor_id;
    group.monsters.push_back( std::move( body ) );
    input->debug_force_add_group( group );
    auto initial_omt = project_to<coords::omt>( position );
    const auto initial = input->debug_unsafe_get_groups_at( initial_omt );
    REQUIRE( initial.size() == 1 );
    const auto uid = initial.front().get().monsters.front().inv.front().uid().get_value();
    // Metadata authentication must not publish this owner's terrain or turn
    // its anonymous recipe into discovery actors. Once real GROUP work is
    // admitted, the unchanged normal loader owns same-file recipe migration.
    const auto recipe_position = position + point( 4, 4 );
    input->ter_set( tripoint_om_omt( 6, 6, 0 ), oter_str_id( "road_nesw" ).id() );
    mongroup recipe( mongroup_id( "GROUP_DEBUG_EXACTLY_ONE" ), recipe_position, 3 );
    recipe.horde = true;
    input->debug_force_add_group( recipe );
    input->save();
    input.reset();
    overmap_buffer.clear(); // genuine normal world reload, not token reset
    get_avatar().setpos( project_to<coords::ms>( project_to<coords::omt>( position ) - point( distance, 0 ) ), false );
    overmap_buffer.bootstrap_predator_positions();
    REQUIRE_FALSE( overmap_buffer.find_loaded_overmap( address ) );
    const auto *record = overmap_buffer.global_state.predator_activity.find( id );
    REQUIRE( record );
    REQUIRE( record->confirmed );
    REQUIRE( record->owner == predator_activation::representation::group );
    const auto original = *record->position;
    bool moved = false;
    for( int attempt = 0; attempt < 8 && !moved; ++attempt ) {
        calendar::turn += 1_turns;
        overmap_buffer.move_nemesis();
        if( distance == 41 ) {
            CHECK_FALSE( overmap_buffer.find_loaded_overmap( address ) );
            CHECK( overmap_buffer.global_state.predator_activity.find( id )->position == original );
            if( attempt == 3 ) {
                std::ostringstream clock;
                { JsonOut json( clock ); overmap_buffer.global_state.predator_activity.serialize( json ); }
                overmap_buffer.clear(); // native world reload, no actor preloading
                overmap_buffer.global_state.predator_activity.deserialize(
                    json_loader::from_string( clock.str() ).get_object() );
                overmap_buffer.bootstrap_predator_positions();
                CHECK_FALSE( overmap_buffer.find_loaded_overmap( address ) );
                CHECK( overmap_buffer.global_state.predator_activity.find( id )->position == original );
            }
        } else {
            REQUIRE( overmap_buffer.find_loaded_overmap( address ) );
            const auto current = *overmap_buffer.global_state.predator_activity.find( id )->position;
            CHECK( rl_dist( project_to<coords::sm>( current ), position ) <= 1 );
            moved = current != original;
        }
    }
    CHECK( moved == ( distance == 40 ) );
    // Explicit existing-owner read is after proving the scheduler did not
    // load a frozen owner merely to decide range.
    overmap *loaded = overmap_buffer.get_existing( address );
    REQUIRE( loaded );
    // The ordinary same-file recipe still migrates exactly once. Neither
    // the cold probe nor repeated scheduler admission expands it again.
    REQUIRE( loaded->horde_entities().size() == 3 );
    CHECK( overmap_buffer.get_existing( address ) == loaded );
    CHECK( loaded->horde_entities().size() == 3 );
    for( const auto &entry : loaded->horde_entities() ) {
        CHECK( entry.second->get_type()->id == mon_zombie );
    }
    const auto current = *overmap_buffer.global_state.predator_activity.find( id )->position;
    const auto groups = overmap_buffer.groups_at( project_to<coords::sm>( current ) );
    REQUIRE( groups.size() == 1 );
    REQUIRE( groups.front()->monsters.size() == 1 );
    CHECK( groups.front()->monsters.front().inv.front().uid().get_value() == uid );
    CHECK( groups.front()->monsters.front().predator_state().actor_id == id );
    CHECK( groups.front()->monsters.front().predator_state().handoff_epoch == 0 );
}

TEST_CASE( "rider_native_retained_location_survives_freeze_transport_without_new_sight",
           "[predator_activation][native][pursuit][capture]" )
{
    restore_on_out_of_scope restore_time( calendar::turn );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    on_out_of_scope cleanup( [] { clear_map_without_vision(); } );
    clear_avatar();
    clear_map_without_vision();
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_zombie_rider_local_arena( here, center );
    get_avatar().setpos( here, center );
    set_time( daylight_time( calendar::turn ) + 2_hours );
    auto candidate = make_shared_fast<monster>( new_scheduler_predator( mon_zombie_rider ) );
    candidate->ammo = candidate->type->starting_ammo;
    monster *rider = g->place_critter_at( candidate, center + point::east * 8 );
    REQUIRE( rider );
    rider->anger = 100;
    rider->aggro_character = true;
    rider->set_moves( 0 );
    REQUIRE( rider->sees( here, get_avatar() ) );
    rider->plan(); // genuine current native sight, not a synthetic observation
    REQUIRE( rider->zombie_rider_pursuit_state().has_last_observed_position );
    const auto observed = rider->zombie_rider_pursuit_state().last_observed_position;
    const auto acquired = rider->zombie_rider_pursuit_state().last_observed_turn;
    REQUIRE( acquired == to_turn<int>( calendar::turn ) );
    REQUIRE( observed == get_avatar().pos_abs() );
    get_avatar().setpos( here, tripoint_bub_ms( center.xy(), -2 ), false );
    for( int step = 0; step < 16 &&
         rider->zombie_rider_pursuit_state().phase != zombie_rider_overmap_ai::pursuit_phase::searching; ++step ) {
        calendar::turn += 1_turns;
        rider->process_turn();
        rider->plan();
        if( rider->get_moves() > 0 ) {
            rider->move();
        }
    }
    REQUIRE( rider->zombie_rider_pursuit_state().phase == zombie_rider_overmap_ai::pursuit_phase::searching );
    REQUIRE( rider->zombie_rider_pursuit_state().search_until_turn > to_turn<int>( calendar::turn ) );
    const auto position = rider->pos_abs();
    const auto where = rider->pos_abs_omt();
    const auto id = rider->predator_state().actor_id;
    const bool captured = GENERATE( false, true );
    const bool expire = GENERATE( false, true );
    CAPTURE( captured, expire );
    item container( itype_id( "pet_carrier" ), calendar::turn );
    overmap *owner = nullptr;
    if( captured ) {
        container.contain_monster( rider->pos_bub() );
        REQUIRE( container.has_var( "contained_json" ) );
        rider = nullptr;
    } else {
        owner = &overmap_buffer.get( project_to<coords::om>( position ).xy() );
        g->despawn_monster( *rider );
        rider = nullptr;
        REQUIRE( overmap_buffer.entity_at( position ) );
    }
    get_avatar().setpos( project_to<coords::ms>( where - point( 41, 0 ) ), false );
    if( !captured ) {
        CHECK_FALSE( overmap_buffer.predator_activity_at( *overmap_buffer.entity_at( position ), position ) );
    }
    std::ostringstream saved_clock;
    { JsonOut json( saved_clock ); overmap_buffer.global_state.predator_activity.serialize( json ); }
    if( captured ) {
        const auto encoded_item = serialize( container );
        deserialize_from_string( container, encoded_item );
    } else {
        auto *entity = overmap_buffer.entity_at( position );
        *entity->monster_data = round_trip_zombie_rider( *entity->monster_data );
        owner->hordes.refresh_entity_bucket( position );
    }
    overmap_buffer.global_state.predator_activity.deserialize(
        json_loader::from_string( saved_clock.str() ).get_object() );
    calendar::turn += 1000_turns;
    if( !captured ) {
        auto *entity = overmap_buffer.entity_at( position );
        const auto before = serialize( *entity->monster_data );
        const auto active_before = overmap_buffer.global_state.predator_activity.find( id )->activity.active_at(
                                       to_turn<int>( calendar::turn ) );
        CHECK_FALSE( overmap_buffer.predator_activity_at( *entity, position ) );
        CHECK( serialize( *entity->monster_data ) == before );
        CHECK( overmap_buffer.global_state.predator_activity.find( id )->activity.active_at(
                   to_turn<int>( calendar::turn ) ) == active_before );
    }
    get_avatar().setpos( project_to<coords::ms>( where - point( 40, 0 ) ), false );
    if( captured ) {
        REQUIRE( container.release_monster( here.get_bub( position ), 0 ) );
    } else {
        overmap_buffer.spawn_monster( project_to<coords::sm>( position ), false );
    }
    for( monster &body : g->all_monsters() ) {
        if( body.is_caol_predator() && body.predator_state().actor_id == id ) {
            REQUIRE( rider == nullptr );
            rider = &body;
        }
    }
    REQUIRE( rider );
    auto &state = rider->zombie_rider_pursuit_state();
    REQUIRE( state.last_observed_turn == acquired );
    CHECK_FALSE( state.evidence_valid( to_turn<int>( calendar::turn ) ) );
    REQUIRE( state.retained_observation_work );
    REQUIRE( state.search_until_turn > to_turn<int>( calendar::turn ) );
    const auto deadline = state.search_until_turn;
    // Native spawn has cleared the old destination. Only the actual local
    // planner can restore this earned location search, without new sight.
    CHECK_FALSE( rider->has_dest() );
    if( expire ) {
        calendar::turn = calendar::turn_zero + time_duration::from_turns( deadline + 1 );
    }
    rider->plan();
    CHECK_FALSE( state.evidence_valid( to_turn<int>( calendar::turn ) ) );
    if( expire ) {
        CHECK( state.phase == zombie_rider_overmap_ai::pursuit_phase::idle );
        CHECK_FALSE( state.has_movement_waypoint );
        CHECK_FALSE( state.retained_observation_work );
    } else {
        CHECK( rider->has_dest() );
        CHECK( state.phase == zombie_rider_overmap_ai::pursuit_phase::searching );
        CHECK( state.last_observed_turn == acquired );
        CHECK( state.last_observed_position == observed );
        CHECK( state.search_until_turn == deadline );
        rider->plan();
        CHECK( state.search_until_turn == deadline );
        const auto before_move = rider->pos_abs();
        calendar::turn += 1_turns;
        rider->process_turn();
        rider->plan();
        rider->move();
        CHECK( rider->pos_abs() != before_move );
        CHECK( state.last_observed_turn == acquired );
    }
}

TEST_CASE( "predator_materialization_does_not_fund_local_energy_twice",
           "[predator_activation][native][handoff][energy]" )
{
    restore_on_out_of_scope restore_time( calendar::turn );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    clear_map_without_vision();
    on_out_of_scope cleanup( [] { clear_map_without_vision(); } );
    set_time( calendar::turn_zero + 800000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_zombie_rider_local_arena( here, center );
    get_avatar().setpos( here, tripoint_bub_ms( center.xy(), -2 ) );
    const auto type = mtype_id( GENERATE( "mon_zombie_rider", "mon_writhing_stalker" ) );
    auto fresh = make_shared_fast<monster>( new_scheduler_predator( type ) );
    fresh->ammo = fresh->type->starting_ammo;
    monster *body = g->place_critter_at( fresh, center );
    REQUIRE( body );
    REQUIRE( body->has_special( "bite" ) );
    body->set_special( "bite", 8 );
    body->set_moves( 0 );
    const auto id = body->predator_state().actor_id;
    const auto position = body->pos_abs();
    REQUIRE( overmap_buffer.predator_activity_for( *body, predator_activation::representation::local, position ) );
    REQUIRE( g->despawn_monster( *body ) );
    overmap &owner = overmap_buffer.get( project_to<coords::om>( position ).xy() );
    owner.hordes.signal_entities( position + point::east * 4, 100 );
    overmap_buffer.move_hordes(); // actual native zero-credit birth opportunity
    REQUIRE( overmap_buffer.entity_at( position ) );
    const bool blocked = GENERATE( false, true );
    std::vector<monster *> blockers;
    if( blocked ) {
        for( const auto &offset : { point( 1, -1 ), point( 1, 0 ), point( 1, 1 ) } ) {
            monster *blocker = g->place_critter_at( mon_zombie, center + offset );
            REQUIRE( blocker );
            blockers.push_back( blocker );
        }
    }
    calendar::turn += 1_turns;
    if( blocked ) {
        auto *source = overmap_buffer.entity_at( position );
        REQUIRE( source );
        std::string expected = serialize( *source->monster_data );
        const auto prior_active = source->monster_data->predator_state().activity_active_turns;
        // This public owner runs before transport preparation in the real
        // scheduler. It may settle the rightful current body interval, but
        // must not fund movement or mutate pose/epoch/intent/body/items.
        owner.hordes.expire_light_interest( calendar::turn );
        const auto settled_active = source->monster_data->predator_state().activity_active_turns;
        const auto *record = overmap_buffer.global_state.predator_activity.find( id );
        REQUIRE( record );
        CHECK( settled_active == ( type == mon_zombie_rider ?
                                  record->activity.active_at( to_turn<int>( calendar::turn ) ) : prior_active ) );
        if( settled_active != prior_active ) {
            // Explicitly compare every native field except this one proven
            // interval-binding value; never modify the actual body to match.
            const std::string field = "\"activity_active_turns\":" + std::to_string( prior_active ) + ",";
            const auto at = expected.find( field );
            REQUIRE( at != std::string::npos );
            REQUIRE( expected.find( field, at + field.size() ) == std::string::npos );
            expected.replace( at, field.size(), "\"activity_active_turns\":" +
                              std::to_string( settled_active ) + "," );
        }
        CHECK( serialize( *source->monster_data ) == expected );
        CHECK( source->moves == 0 );
        CHECK( source->monster_data->get_moves() == 0 );
        CHECK( source->monster_data->predator_state().last_energy_world == -1 );
        const auto before = serialize( *source->monster_data );
        overmap_buffer.move_hordes();
        auto *refused = overmap_buffer.entity_at( position );
        REQUIRE( refused );
        CHECK( refused->moves == 0 );
        CHECK( refused->monster_data->predator_state().last_energy_world == -1 );
        CHECK( serialize( *refused->monster_data ) == before );
        overmap_buffer.move_hordes();
        CHECK( serialize( *overmap_buffer.entity_at( position )->monster_data ) == before );
        for( monster *blocker : blockers ) {
            g->remove_zombie( *blocker );
        }
        calendar::turn += 1_turns;
    }
    overmap_buffer.move_hordes();
    body = nullptr;
    for( monster &candidate : g->all_monsters() ) {
        if( candidate.predator_state().actor_id == id ) {
            REQUIRE_FALSE( body );
            body = &candidate;
        }
    }
    REQUIRE( body );
    CHECK( rl_dist( body->pos_abs(), position ) == 1 );
    const auto after_step = body->get_moves();
    CHECK( after_step == body->get_speed() - 100 );
    const auto bytes = [&]() {
        std::ostringstream out;
        { JsonOut json( out ); body->serialize( json ); }
        return out.str();
    };
    const auto cooldown = [&]() {
        const auto json = json_loader::from_string( bytes() ).get_object();
        json.allow_omitted_members();
        const auto specials = json.get_object( "special_attacks" );
        specials.allow_omitted_members();
        const auto bite = specials.get_object( "bite" );
        bite.allow_omitted_members();
        return bite.get_int( "cooldown" );
    };
    const int before_cooldown = cooldown();
    REQUIRE( before_cooldown > 0 );
    body->process_turn(); // rightful physiology, but this interval already funded the native step
    CHECK( cooldown() == before_cooldown - 1 );
    CHECK( body->get_moves() == after_step );
    const auto once = bytes();
    body->process_turn();
    CHECK( bytes() == once );
    CHECK( body->predator_state().last_energy_world == to_turn<int>( calendar::turn ) );
    // The opposite handoff direction carries already-funded native energy.
    calendar::turn += 1_turns;
    body->process_turn();
    const auto local_funded = body->get_moves();
    CHECK( local_funded == after_step + body->get_speed() );
    const auto second_position = body->pos_abs();
    REQUIRE( g->despawn_monster( *body ) );
    owner.hordes.signal_entities( second_position + point::east * 4, 100 );
    overmap_buffer.move_hordes();
    body = nullptr;
    for( monster &candidate : g->all_monsters() ) {
        if( candidate.predator_state().actor_id == id ) {
            REQUIRE_FALSE( body );
            body = &candidate;
        }
    }
    REQUIRE( body );
    CHECK( body->get_moves() == local_funded - 100 );
    const auto returned = bytes();
    body->process_turn();
    CHECK( bytes() == returned );
    // Native item/body/ledger reload preserves this world fact; contained
    // time cannot shift its stamp or create a delayed movement budget.
    const auto consumed_world = body->predator_state().last_energy_world;
    const auto carried_moves = body->get_moves();
    const auto captured_position = body->pos_bub();
    item carrier( itype_id( "pet_carrier" ), calendar::turn );
    carrier.contain_monster( captured_position );
    REQUIRE( carrier.has_var( "contained_json" ) );
    const auto carrier_uid = carrier.uid().get_value();
    const auto saved_item = serialize( carrier );
    deserialize_from_string( carrier, saved_item );
    CHECK( carrier.uid().get_value() == carrier_uid );
    std::ostringstream ledger;
    { JsonOut json( ledger ); overmap_buffer.global_state.predator_activity.serialize( json ); }
    overmap_buffer.global_state.predator_activity.deserialize(
        json_loader::from_string( ledger.str() ).get_object() );
    calendar::turn += 1000_turns;
    REQUIRE( carrier.release_monster( captured_position ) );
    body = get_creature_tracker().find( here.get_abs( captured_position ) ).get();
    REQUIRE( body );
    REQUIRE( body->predator_state().actor_id == id );
    CHECK( body->predator_state().last_energy_world == consumed_world );
    CHECK( body->get_moves() == carried_moves );
    body->process_turn(); // resumed at zero elapsed, no credit
    CHECK( body->get_moves() == carried_moves );
    calendar::turn += 1_turns;
    body->process_turn();
    CHECK( body->get_moves() == carried_moves + body->get_speed() );
    const auto resumed = bytes();
    body->process_turn();
    CHECK( bytes() == resumed );
    // The existing native parser accepts an old valid body without the
    // optional stamp; it must default -1, not inherit another payload's fact.
    std::string legacy = bytes();
    const auto key = legacy.find( "\"last_energy_world\"" );
    REQUIRE( key != std::string::npos );
    const auto comma = legacy.find( ',', key );
    REQUIRE( comma != std::string::npos );
    legacy.erase( key, comma - key + 1 );
    monster legacy_body( type );
    legacy_body.predator_state().last_energy_world = 77;
    legacy_body.deserialize( json_loader::from_string( legacy ).get_object() );
    CHECK( legacy_body.predator_state().last_energy_world == -1 );
    CHECK( legacy_body.predator_state().actor_id == id );
}

TEST_CASE( "predator_unfunded_materialization_keeps_rightful_local_credit",
           "[predator_activation][native][handoff][energy]" )
{
    restore_on_out_of_scope restore_time( calendar::turn );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    clear_map_without_vision();
    on_out_of_scope cleanup( [] { clear_map_without_vision(); } );
    set_time( calendar::turn_zero + 830000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_zombie_rider_local_arena( here, center );
    get_avatar().setpos( here, tripoint_bub_ms( center.xy(), -2 ) );
    const auto type = mtype_id( GENERATE( "mon_zombie_rider", "mon_writhing_stalker" ) );
    auto fresh = make_shared_fast<monster>( new_scheduler_predator( type ) );
    monster *body = g->place_critter_at( fresh, center );
    REQUIRE( body );
    body->set_moves( 0 );
    const auto position = body->pos_abs();
    const auto id = body->predator_state().actor_id;
    REQUIRE( overmap_buffer.predator_activity_for( *body, predator_activation::representation::local, position ) );
    body->process_turn();
    CHECK( body->get_moves() == 0 );
    CHECK( body->predator_state().last_energy_world == -1 );
    REQUIRE( g->despawn_monster( *body ) );
    calendar::turn += 1_turns;
    overmap_buffer.spawn_monster( project_to<coords::sm>( position ), false );
    body = get_creature_tracker().find( position ).get();
    REQUIRE( body );
    REQUIRE( body->predator_state().actor_id == id );
    REQUIRE( body->predator_state().last_energy_world == -1 );
    body->process_turn();
    CHECK( body->get_moves() == body->get_speed() );
    CHECK( body->predator_state().last_energy_world == to_turn<int>( calendar::turn ) );
    const auto moves = body->get_moves();
    body->process_turn();
    CHECK( body->get_moves() == moves );
}

TEST_CASE( "predator_future_energy_fact_refuses_activity_and_capture_without_mutation",
           "[predator_activation][native][energy][capture][negative]" )
{
    restore_on_out_of_scope restore_time( calendar::turn );
    restore_on_out_of_scope restore_activity( overmap_buffer.global_state.predator_activity );
    clear_map_without_vision();
    on_out_of_scope cleanup( [] { clear_map_without_vision(); } );
    set_time( calendar::turn_zero + 840000_turns );
    map &here = get_map();
    const tripoint_bub_ms center( 65, 65, 0 );
    prepare_zombie_rider_local_arena( here, center );
    get_avatar().setpos( here, tripoint_bub_ms( center.xy(), -2 ) );
    const auto type = mtype_id( GENERATE( "mon_zombie_rider", "mon_writhing_stalker" ) );
    auto fresh = make_shared_fast<monster>( new_scheduler_predator( type ) );
    monster *body = g->place_critter_at( fresh, center );
    REQUIRE( body );
    REQUIRE( overmap_buffer.predator_activity_for( *body, predator_activation::representation::local, body->pos_abs() ) );
    const bool release = GENERATE( false, true );
    if( release ) {
        item carrier( itype_id( "pet_carrier" ), calendar::turn );
        carrier.contain_monster( center );
        REQUIRE( carrier.has_var( "contained_json" ) );
        REQUIRE_FALSE( get_creature_tracker().find( here.get_abs( center ) ) );
        monster snapshot( type );
        snapshot.deserialize( json_loader::from_string( carrier.get_var( "contained_json" ) ).get_object() );
        snapshot.predator_state().last_energy_world = to_turn<int>( calendar::turn ) + 1; // invalid saved fact
        carrier.set_var( "contained_json", serialize( snapshot ) );
        const auto item_before = serialize( carrier );
        std::ostringstream clock;
        { JsonOut json( clock ); overmap_buffer.global_state.predator_activity.serialize( json ); }
        CHECK_FALSE( carrier.release_monster( center ) );
        CHECK_FALSE( get_creature_tracker().find( here.get_abs( center ) ) );
        CHECK( serialize( carrier ) == item_before );
        std::ostringstream after;
        { JsonOut json( after ); overmap_buffer.global_state.predator_activity.serialize( json ); }
        CHECK( after.str() == clock.str() );
        return;
    }
    body->predator_state().last_energy_world = to_turn<int>( calendar::turn ) + 1; // invalid input fixture
    const auto before = serialize( *body );
    CHECK_FALSE( overmap_buffer.predator_activity_for( *body, predator_activation::representation::local, body->pos_abs() ) );
    body->process_turn();
    item carrier( itype_id( "pet_carrier" ), calendar::turn );
    carrier.contain_monster( center );
    CHECK_FALSE( carrier.has_var( "contained_json" ) );
    CHECK( get_creature_tracker().find( here.get_abs( center ) ).get() == body );
    CHECK( serialize( *body ) == before );
}
