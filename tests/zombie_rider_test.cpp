#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "character.h"
#include "debug.h"
#include "effect.h"
#include "game.h"
#include "json.h"
#include "json_loader.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "mattack_actors.h"
#include "mongroup.h"
#include "monster.h"
#include "mtype.h"
#include "pathfinding.h"
#include "player_helpers.h"
#include "type_id.h"
#include "vehicle.h"
#include "zombie_rider_overmap_ai.h"

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
    for( const std::string &member : { "band_revision_cache", "band_reference",
                                      "ammo_initialization_version" } ) {
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

TEST_CASE( "zombie_rider_local_bow_shot_sets_cooldown_and_repositions", "[zombie_rider][monster][ai]" )
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

    for( int turn = 0; turn < 4; ++turn ) {
        rider.process_turn();
    }
    CHECK( rider.special_available( zombie_rider_bone_bow_shot ) );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_bow_pressure_marks_avatar_hostile_before_shooting", "[zombie_rider][monster][ai]" )
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

TEST_CASE( "zombie_rider_close_pressure_bunny_hops_without_point_blank_bow_shot", "[zombie_rider][monster][ai]" )
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

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
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

TEST_CASE( "zombie_rider_empty_bow_charges_instead_of_kiting_forever", "[zombie_rider][monster][ai]" )
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

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
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

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
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

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), center + point::east * 3 );
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

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), center + point::east * 3 );
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

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), center + point::north );
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

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), center + point::east * 2 );
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

TEST_CASE( "zombie_rider_close_indoor_pressure_repositions_instead_of_loitering", "[zombie_rider][monster][ai][map]" )
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

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
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

TEST_CASE( "zombie_rider_injured_continues_pursuit_instead_of_withdrawing", "[zombie_rider][monster][ai]" )
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

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
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

    monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
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
        CHECK( std::all_of( route.begin(), route.end(), [&rider]( const tripoint_bub_ms &step ) {
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
        monster &rider = spawn_test_monster( mon_zombie_rider.str(), rider_start );
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

    const plan_state enabled = run_plan( true );
    const plan_state disabled = run_plan( false );
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

    limitDebugLevel( DL_ALL );
    limitDebugClass( DC_ALL );
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
    CHECK( std::all_of( route.begin(), route.end(), [&rider]( const tripoint_bub_ms &step ) {
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
    monster reloaded = round_trip_zombie_rider( staged );
    calendar::turn = calendar::start_of_cataclysm + gate;
    reloaded.try_upgrade( false );
    CHECK( reloaded.type->id == mon_zombie_rider );
    CHECK( reloaded.predator_state().ammo_initialization_version == 1 );
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
    monster &rider = spawn_test_monster( mon_zombie_rider.str(), center + point::east * 6 );
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
    CHECK( bandit_mark_generation::horde_signal_power_from_light_projection( daylight_projection ) == 0 );

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
    CHECK_FALSE( zombie_rider_overmap_ai::refresh_light_memory( memory, interest, "lamp@old#110", 110 ) );
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
    CHECK( result.rider_ids == std::vector<std::string>{ "near_alpha", "near_beta" } );
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
    CHECK( result.rider_ids == std::vector<std::string>{ "lone_rider" } );
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
    CHECK( std::all_of( riders.begin(), riders.end(), []( const auto &rider ) {
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
    const std::optional<rider_band_observation> reverse_selected = reverse.observation_for( "rider", 50 );
    REQUIRE( reverse_selected );
    CHECK( reverse_selected->source_actor_id == "alpha" );
    CHECK( reverse_selected->position == direct.position );
}
