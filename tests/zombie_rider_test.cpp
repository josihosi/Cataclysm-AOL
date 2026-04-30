#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "character.h"
#include "effect.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "mongroup.h"
#include "monster.h"
#include "mtype.h"
#include "pathfinding.h"
#include "type_id.h"

static const efftype_id effect_run( "run" );
static const itype_id arrow_wood( "arrow_wood" );
static const mongroup_id GROUP_ZOMBIE( "GROUP_ZOMBIE" );
static const mongroup_id GROUP_ZOMBIE_UPGRADE( "GROUP_ZOMBIE_UPGRADE" );
static const mtype_id mon_zombie( "mon_zombie" );
static const mtype_id mon_zombie_hunter( "mon_zombie_hunter" );
static const mtype_id mon_zombie_predator( "mon_zombie_predator" );
static const mtype_id mon_zombie_rider( "mon_zombie_rider" );
static const species_id species_HUMAN( "HUMAN" );
static const species_id species_ZOMBIE( "ZOMBIE" );
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
    const ter_id t_floor( "t_floor" );
    for( const tripoint_bub_ms &p : here.points_in_radius( center, 14 ) ) {
        here.ter_set( p, t_floor );
        here.furn_clear( p );
        here.clear_fields( p );
    }
    refresh_pathing_cache( here );
}

TEST_CASE( "zombie_rider_monster_footing", "[zombie_rider][monster]" )
{
    const mtype &rider = *mon_zombie_rider;
    const std::string exact_description =
        "Up on, what can only be described as, a six legged horse - or is it a spider? - a towering figure, with eyes the color of blood, holds a gory bow of wet bones and sinews.\n"
        "It's movement ferocious, as the tumbling feet hasten across the terrain.\n"
        "Running is out of the question.";

    CHECK( rider.nname() == "zombie rider" );
    CHECK( rider.get_description() == exact_description );
    CHECK( rider.in_species( species_ZOMBIE ) );
    CHECK_FALSE( rider.in_species( species_HUMAN ) );
    CHECK( rider.size > creature_size::medium );
    CHECK( rider.speed >= 150 );
    CHECK( rider.speed <= 200 );
    CHECK( rider.hp >= 200 );
    CHECK( rider.has_special_attack( "zombie_rider_bone_bow_shot" ) );
    CHECK( rider.starting_ammo.count( arrow_wood ) == 1 );
    CHECK( rider.starting_ammo.at( arrow_wood ) >= 12 );
    CHECK( rider.has_flag( mon_flag_RANGED_ATTACKER ) );
    CHECK( rider.has_flag( mon_flag_PATH_AVOID_DANGER ) );
    CHECK( rider.has_flag( mon_flag_HARDTOSHOOT ) );
    CHECK( rider.has_flag( mon_flag_HIT_AND_RUN ) );
    CHECK_FALSE( rider.has_flag( mon_flag_BASHES ) );
    CHECK_FALSE( rider.has_flag( mon_flag_GROUP_BASH ) );
    CHECK_FALSE( rider.has_flag( mon_flag_UNBREAKABLE_MORALE ) );
    CHECK_FALSE( rider.upgrades );
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

    const int ammo_before = rider.ammo[arrow_wood];
    REQUIRE( rider.special_available( zombie_rider_bone_bow_shot ) );
    rider.move();

    CHECK( rider.ammo[arrow_wood] == ammo_before - 1 );
    CHECK( rider.has_effect( effect_run ) );
    CHECK_FALSE( rider.special_available( zombie_rider_bone_bow_shot ) );
    CHECK( rl_dist( here.get_bub( rider.get_dest() ), you.pos_bub() ) > distance_before );
    CHECK( rl_dist( rider.pos_bub(), you.pos_bub() ) >= distance_before );

    for( int turn = 0; turn < 4; ++turn ) {
        rider.process_turn();
    }
    CHECK( rider.special_available( zombie_rider_bone_bow_shot ) );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_close_pressure_withdraws_without_point_blank_bow_shot", "[zombie_rider][monster][ai]" )
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
    rider.set_moves( 100 );

    REQUIRE( rider.sees( here, you ) );
    const int distance_before = rl_dist( rider.pos_bub(), you.pos_bub() );
    rider.plan();
    CHECK( rl_dist( here.get_bub( rider.get_dest() ), you.pos_bub() ) > distance_before );

    const int ammo_before = rider.ammo[arrow_wood];
    rider.move();

    CHECK( rider.ammo[arrow_wood] == ammo_before );
    CHECK( rl_dist( rider.pos_bub(), you.pos_bub() ) > distance_before );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_injured_withdraws_instead_of_holding_bow_range", "[zombie_rider][monster][ai]" )
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
    CHECK( rl_dist( here.get_bub( rider.get_dest() ), you.pos_bub() ) > distance_before );

    const int ammo_before = rider.ammo[arrow_wood];
    rider.move();

    CHECK( rider.ammo[arrow_wood] == ammo_before );
    CHECK( rl_dist( rider.pos_bub(), you.pos_bub() ) > distance_before );
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
    const int ammo_before = rider.ammo[arrow_wood];
    rider.move();

    CHECK( rider.ammo[arrow_wood] == ammo_before );
    CHECK( rider.special_available( zombie_rider_bone_bow_shot ) );
    clear_map_without_vision();
}

TEST_CASE( "zombie_rider_endpoint_spawn_and_evolution_gate", "[zombie_rider][monster][mongroup]" )
{
    constexpr time_duration mature_world_gate = 730_days;
    const mtype &hunter = *mon_zombie_hunter;
    const mtype &predator = *mon_zombie_predator;

    REQUIRE( hunter.upgrades );
    CHECK( hunter.upgrade_into == mon_zombie_predator );
    CHECK_FALSE( predator.upgrades );
    CHECK( predator.upgrade_into != mon_zombie_rider );
    CHECK_FALSE( GROUP_ZOMBIE_UPGRADE->IsMonsterInGroup( mon_zombie_rider ) );

    const MonsterGroup &zombie_group = GROUP_ZOMBIE.obj();
    std::optional<MonsterGroupEntry> rider_entry;
    int direct_entries = 0;
    for( const MonsterGroupEntry &entry : zombie_group.monsters ) {
        if( entry.is_group() ) {
            continue;
        }
        if( entry.mtype == mon_zombie_rider ) {
            rider_entry = entry;
            direct_entries++;
        }
    }

    REQUIRE( direct_entries == 1 );
    REQUIRE( rider_entry.has_value() );
    CHECK( rider_entry->frequency == 1 );
    CHECK( rider_entry->cost_multiplier >= 80 );
    CHECK( rider_entry->pack_minimum == 1 );
    CHECK( rider_entry->pack_maximum == 1 );
    CHECK( rider_entry->starts >= mature_world_gate );

    int all_direct_entries = 0;
    for( const auto &group_pair : MonsterGroupManager::Get_all_Groups() ) {
        for( const MonsterGroupEntry &entry : group_pair.second.monsters ) {
            if( entry.is_group() || entry.mtype != mon_zombie_rider ) {
                continue;
            }
            all_direct_entries++;
            CHECK( entry.starts >= mature_world_gate );
            CHECK( entry.pack_minimum == 1 );
            CHECK( entry.pack_maximum == 1 );
        }
    }
    CHECK( all_direct_entries == 1 );
}
