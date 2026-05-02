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
#include "zombie_rider_overmap_ai.h"

static const efftype_id effect_run( "run" );
static const itype_id zombie_rider_tainted_bone_arrow( "zombie_rider_tainted_bone_arrow" );
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
        "Up on a six-legged horse — or is it a spider? — a towering figure with eyes the color of blood holds a gory bow of wet bones and sinews.\n"
        "It moves ferociously, tumbling feet hastening across the terrain.\n"
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
    CHECK( rider.starting_ammo.count( zombie_rider_tainted_bone_arrow ) == 1 );
    CHECK( rider.starting_ammo.at( zombie_rider_tainted_bone_arrow ) >= 12 );
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

    const int ammo_before = rider.ammo[zombie_rider_tainted_bone_arrow];
    REQUIRE( rider.special_available( zombie_rider_bone_bow_shot ) );
    rider.move();

    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before - 1 );
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
    const int distance_before = rl_dist( rider.pos_bub(), you.pos_bub() );
    rider.plan();
    const tripoint_bub_ms planned_dest = here.get_bub( rider.get_dest() );
    const int destination_distance = rl_dist( planned_dest, you.pos_bub() );
    CHECK( destination_distance >= 4 );
    CHECK( destination_distance <= 6 );
    CHECK( planned_dest.y() != rider_start.y() );

    const int ammo_before = rider.ammo[zombie_rider_tainted_bone_arrow];
    for( int moves = 0; moves < 2 && rider.pos_bub() == rider_start; ++moves ) {
        rider.set_moves( 100 );
        rider.move();
    }

    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before );
    CHECK( rider.pos_bub() != rider_start );
    CHECK( rl_dist( here.get_bub( rider.get_dest() ), you.pos_bub() ) > distance_before );
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
    rider.plan();

    CHECK( rider.get_dest() == you.pos_abs() );
    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == 0 );
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
    CHECK( planned_dest != you.pos_bub() );
    CHECK( rl_dist( planned_dest, you.pos_bub() ) >= 4 );
    CHECK( rl_dist( planned_dest, you.pos_bub() ) <= 6 );
    CHECK( planned_dest.y() != rider_start.y() );

    for( int moves = 0; moves < 2 && rider.pos_bub() == rider_start; ++moves ) {
        rider.set_moves( 100 );
        rider.move();
    }

    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before );
    CHECK( rider.pos_bub() != rider_start );
    CHECK( rl_dist( rider.pos_bub(), you.pos_bub() ) >= 4 );
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

    const int ammo_before = rider.ammo[zombie_rider_tainted_bone_arrow];
    rider.move();
    if( rl_dist( rider.pos_bub(), you.pos_bub() ) == distance_before ) {
        rider.set_moves( 100 );
        rider.move();
    }

    CHECK( rider.ammo[zombie_rider_tainted_bone_arrow] == ammo_before );
    CHECK( rider.pos_bub() != rider_start );
    CHECK( rl_dist( here.get_bub( rider.get_dest() ), you.pos_bub() ) > distance_before );
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
                zombie_rider_overmap_ai::mature_world_gate_days - 1, 3 );
    CHECK_FALSE( early.should_investigate );
    CHECK( early.reason == "early_world_gate" );
    CHECK( early.max_riders_drawn == 0 );

    const zombie_rider_overmap_ai::rider_light_interest mature =
        zombie_rider_overmap_ai::evaluate_light_attraction( projection,
                zombie_rider_overmap_ai::mature_world_gate_days + 30, 3 );
    CHECK( mature.should_investigate );
    CHECK( mature.reason == "exposed_bright_light" );
    CHECK( mature.interest_score > 0 );
    CHECK( mature.memory_turns >= 90 );
    CHECK( mature.memory_turns <= 300 );
    CHECK( mature.max_riders_drawn == 1 );
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
                zombie_rider_overmap_ai::mature_world_gate_days + 30, 2 );
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
                zombie_rider_overmap_ai::mature_world_gate_days + 30, 2 );
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
                zombie_rider_overmap_ai::mature_world_gate_days + 30, 2 );
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
                zombie_rider_overmap_ai::mature_world_gate_days + 30, 9 );
    REQUIRE( interest.should_investigate );
    CHECK( interest.reason == "elevated_bright_light" );
    CHECK( interest.interest_score == 6 );
    CHECK( interest.memory_turns == 300 );
    CHECK( interest.max_riders_drawn == zombie_rider_overmap_ai::max_riders_drawn_by_light );

    zombie_rider_overmap_ai::rider_light_memory memory;
    zombie_rider_overmap_ai::refresh_light_memory( memory, interest );
    REQUIRE( memory.active() );
    CHECK( memory.max_riders_drawn == zombie_rider_overmap_ai::max_riders_drawn_by_light );

    zombie_rider_overmap_ai::advance_light_memory( memory, 60 );
    CHECK( memory.active() );
    CHECK( memory.interest_score == 5 );
    CHECK( memory.turns_remaining == 240 );
    CHECK( memory.max_riders_drawn == zombie_rider_overmap_ai::max_riders_drawn_by_light );

    zombie_rider_overmap_ai::advance_light_memory( memory, 1000 );
    CHECK_FALSE( memory.active() );
    CHECK( memory.reason == "decayed_after_light_off" );
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

TEST_CASE( "zombie_rider_overmap_convergence_forms_capped_band_from_active_light_memory",
           "[zombie_rider][overmap][ai]" )
{
    const tripoint_abs_omt light_omt( 100, 100, 0 );
    const zombie_rider_overmap_ai::rider_light_memory memory = strong_rider_light_memory();
    const std::vector<zombie_rider_overmap_ai::rider_overmap_agent> riders = {
        { "near_beta", tripoint_abs_omt( 100, 94, 0 ) },
        { "near_alpha", tripoint_abs_omt( 106, 100, 0 ) },
        { "third_capped", tripoint_abs_omt( 109, 101, 0 ) },
        { "far_ignored", tripoint_abs_omt( 170, 100, 0 ) },
        { "cooldown_ignored", tripoint_abs_omt( 99, 100, 0 ), true, false, 30 },
        { "banded_ignored", tripoint_abs_omt( 98, 100, 0 ), true, true, 0 },
    };

    const zombie_rider_overmap_ai::rider_convergence_result result =
        zombie_rider_overmap_ai::evaluate_rider_convergence( memory, light_omt, riders );

    REQUIRE( result.should_converge );
    CHECK( result.cap == zombie_rider_overmap_ai::max_riders_drawn_by_light );
    CHECK( result.selected_riders == zombie_rider_overmap_ai::max_riders_drawn_by_light );
    CHECK( result.band_formed );
    CHECK( result.band_size == 2 );
    CHECK( result.posture == "rider_band_harass" );
    CHECK( result.reason == "riders_converged_to_light_band" );
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
}
