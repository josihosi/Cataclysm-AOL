#include <array>
#include <map>
#include <sstream>
#include <string>

#include "avatar.h"
#include "cata_catch.h"
#include "character_id.h"
#include "coordinates.h"
#include "creature.h"
#include "faction.h"
#include "game.h"
#include "llm_intent.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "monster.h"
#include "npc.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "point.h"
#include "type_id.h"

static const faction_id faction_your_followers( "your_followers" );
static const furn_str_id furn_f_table( "f_table" );
static const mtype_id mon_zombie( "mon_zombie" );
static const string_id<npc_template> npc_template_test_talker( "test_talker" );
static const ter_str_id ter_t_rock_wall( "t_rock_wall" );

namespace
{
static npc &spawn_test_npc_at( const point_bub_ms &pos, const std::string &name )
{
    map &here = get_map();
    const character_id model_id = here.place_npc( pos, npc_template_test_talker );
    g->load_npcs();

    npc *guy = g->find_npc( model_id );
    REQUIRE( guy != nullptr );
    clear_character( *guy );
    guy->setpos( here, tripoint_bub_ms( pos, 0 ) );
    guy->name = name;
    return *guy;
}

static void setup_snapshot_test_scene()
{
    clear_avatar();
    clear_npcs();
    clear_map_with_vision();
    clear_vehicles();
    set_time_to_day();
}

static void run_npc_turns( npc &guy, int turns )
{
    for( int turn = 0; turn < turns; ++turn ) {
        guy.set_moves( 100 );
        guy.move();
    }
}
}

TEST_CASE( "llm_intent_snapshot_includes_attitude_and_lettered_targets", "[llm_intent]" )
{
    setup_snapshot_test_scene();
    map &here = get_map();
    avatar &player_character = get_avatar();
    player_character.name = "Test Player";
    player_character.setpos( here, tripoint_bub_ms( 48, 50, 0 ) );

    npc &listener = spawn_test_npc_at( point_bub_ms( 50, 50 ), "Listener NPC" );
    listener.set_fac( faction_your_followers );

    npc &neutral_npc = spawn_test_npc_at( point_bub_ms( 52, 50 ), "Neutral NPC" );
    monster &zombie = spawn_test_monster( mon_zombie.str(), tripoint_bub_ms( 54, 50, 0 ) );
    CAPTURE( neutral_npc.disp_name() );
    CAPTURE( zombie.disp_name() );

    const std::string snapshot = llm_intent::build_snapshot_for_test( listener, "Hold there.", "req-snapshot" );

    CHECK( snapshot.find( "creature legend with attitude and threat level:" ) != std::string::npos );
    CHECK( snapshot.find( "map axes: +x east/right, -x west/left, +y north/up, -y south/down" ) != std::string::npos );
    CHECK( snapshot.find( "-20" ) != std::string::npos );
    CHECK( snapshot.find( "+20" ) != std::string::npos );
    CHECK( snapshot.find( "|.........|.........|.........|.........|" ) != std::string::npos );
    CHECK( snapshot.find( "dy=+20 " ) != std::string::npos );
    CHECK( snapshot.find( "dy=+00 " ) != std::string::npos );
    CHECK( snapshot.find( "dy=-20 " ) != std::string::npos );
    CHECK( snapshot.find( "a ... player friendly threat=" ) != std::string::npos );
    CHECK( snapshot.find( "Neutral NPC" ) != std::string::npos );
    CHECK( snapshot.find( "neutral threat=" ) != std::string::npos );
    CHECK( snapshot.find( "zombie hostile threat=" ) != std::string::npos );
}

TEST_CASE( "llm_intent_prompt_explicitly_allows_lettered_targets", "[llm_intent]" )
{
    const std::string prompt = llm_intent::build_action_prompt_for_test( "Listener NPC",
                               "Do it.", "snapshot" );

    CHECK( prompt.find( "Any creature with a map letter is a valid explicit target handle, including the player, friendlies, neutrals, and hostiles." ) != std::string::npos );
    CHECK( prompt.find( "attack=a" ) != std::string::npos );
    CHECK( prompt.find( "attack=b" ) != std::string::npos );
}

TEST_CASE( "llm_intent_prompt_uses_delta_move_contract", "[llm_intent]" )
{
    const std::string prompt = llm_intent::build_action_prompt_for_test( "Listener NPC",
                               "Move there.", "snapshot" );

    CHECK( prompt.find( "move=<dx>,<dy> <state>" ) != std::string::npos );
    CHECK( prompt.find( "Positive x is east/right, negative x is west/left, positive y is north/up, and negative y is south/down." ) != std::string::npos );
    CHECK( prompt.find( "dx column markers and dy row labels" ) != std::string::npos );
    CHECK( prompt.find( "move=0,-5 hold_position" ) != std::string::npos );
    CHECK( prompt.find( "move: S S S S S hold_position" ) == std::string::npos );
}

TEST_CASE( "llm_intent_can_parse_delta_move_fields", "[llm_intent]" )
{
    point delta;
    std::string terminal_state;
    std::string error;

    CHECK( llm_intent::parse_move_field_for_test( "move=4,-2 hold_position", delta,
           terminal_state, error ) );
    CHECK( delta == point( 4, -2 ) );
    CHECK( terminal_state == "hold_position" );

    CHECK( llm_intent::parse_move_field_for_test( "move=-1,3 wait_here", delta,
           terminal_state, error ) );
    CHECK( delta == point( -1, 3 ) );
    CHECK( terminal_state == "wait_here" );

    CHECK( llm_intent::parse_move_field_for_test( "MOVE= -20,+20 WAIT_HERE", delta,
           terminal_state, error ) );
    CHECK( delta == point( -20, 20 ) );
    CHECK( terminal_state == "wait_here" );

    CHECK_FALSE( llm_intent::parse_move_field_for_test( "move: E E N hold_position", delta,
                 terminal_state, error ) );
    CHECK( error == "Move field must use move=<dx>,<dy> <state>." );

    CHECK_FALSE( llm_intent::parse_move_field_for_test( "move E E N hold_position", delta,
                 terminal_state, error ) );
    CHECK( error == "Move field must use move=<dx>,<dy> <state>." );

    CHECK_FALSE( llm_intent::parse_move_field_for_test( "move=4 east hold_position", delta,
                 terminal_state, error ) );
    CHECK_FALSE( error.empty() );

    CHECK_FALSE( llm_intent::parse_move_field_for_test( "move=21,0 hold_position", delta,
                 terminal_state, error ) );
    CHECK( error == "Move field delta must stay within the snapshot map (-20..20)." );

    CHECK( llm_intent::parse_move_field_for_test( "move=1,0 wait_here", delta,
           terminal_state, error ) );
    CHECK( error.empty() );

    CHECK_FALSE( llm_intent::parse_move_field_for_test( "move=-2147483648,0 wait_here", delta,
                 terminal_state, error ) );
    CHECK( error == "Move field delta must stay within the snapshot map (-20..20)." );
}

TEST_CASE( "llm_intent_action_csv_applies_move_and_attack_contract", "[llm_intent]" )
{
    std::vector<std::string> actions;
    std::string attack_target;
    std::optional<point> move_delta;
    std::string terminal_state;
    std::string error;

    CHECK( llm_intent::parse_action_csv_for_test(
               "On it|move=4,-2 wait_here|equip_gun", actions, attack_target,
               move_delta, terminal_state, error ) );
    CHECK( actions == std::vector<std::string>{ "move=4,-2 wait_here", "equip_gun" } );
    CHECK( attack_target.empty() );
    CHECK( move_delta == point( 4, -2 ) );
    CHECK( terminal_state == "wait_here" );

    CHECK( llm_intent::parse_action_csv_for_test(
               "Engaging|attack=a|equip_gun|follow_close panic_off", actions, attack_target,
               move_delta, terminal_state, error ) );
    CHECK( actions == std::vector<std::string>{ "equip_gun", "follow_close", "panic_off" } );
    CHECK( attack_target == "a" );
    CHECK_FALSE( move_delta.has_value() );

    CHECK_FALSE( llm_intent::parse_action_csv_for_test(
                     "No|move=1,0 wait_here|move=2,0 hold_position", actions, attack_target,
                     move_delta, terminal_state, error ) );
    CHECK( error == "CSV move field repeated." );

    CHECK_FALSE( llm_intent::parse_action_csv_for_test(
                     "No|move=21,0 wait_here", actions, attack_target,
                     move_delta, terminal_state, error ) );
    CHECK( error == "Move field delta must stay within the snapshot map (-20..20)." );

    CHECK( llm_intent::parse_action_csv_for_test(
               "Moving|move=1,0 wait_here", actions, attack_target,
               move_delta, terminal_state, error ) );
    CHECK( error.empty() );
    CHECK( move_delta == point( 1, 0 ) );

    CHECK_FALSE( llm_intent::parse_action_csv_for_test(
                     "No|bogus follow_close", actions, attack_target,
                     move_delta, terminal_state, error ) );
    CHECK( error == "CSV action token is invalid." );

    const std::string normalized = llm_intent::normalize_csv_separators_for_test(
                                       "Moving+move=-20,+20 wait_here" );
    CHECK( normalized == "Moving|move=-20,+20 wait_here" );
    CHECK( llm_intent::parse_action_csv_for_test(
               normalized, actions, attack_target, move_delta, terminal_state, error ) );
    CHECK( move_delta == point( -20, 20 ) );
}

TEST_CASE( "llm_intent_machine_event_log_preserves_utf8_json_punctuation", "[llm_intent]" )
{
    const std::string curly_quotes = "\xE2\x80\x9Cquoted\xE2\x80\x9D";
    const std::string payload = "[CAOL_EVENT] action_status npc=\"Ada " + curly_quotes +
                                "\" kind=\"look_inventory\"";

    const std::string prepared = llm_intent::prepare_event_log_payload_for_test( payload );

    CHECK( prepared == payload + "\n\n" );
    CHECK( prepared.find( curly_quotes ) != std::string::npos );
    CHECK( prepared.find( "npc=\"Ada \"quoted\"\"" ) == std::string::npos );
}

TEST_CASE( "llm_intent_look_around_filters_and_caps_selected_items", "[llm_intent]" )
{
    CHECK( llm_intent::look_around_selection_limit_for_test() == 4 );

    const std::vector<std::string> selected = llm_intent::parse_look_around_response_for_test(
                "item_1:2, unknown, item_2, item_3, item_4, item_5, item_1",
                { "adhesive bandage", "9x19mm JHP, reloaded",
                    "Glock 9x19mm 15-round magazine", "small plastic bag", "combat knife" } );
    REQUIRE( selected.size() == 4 );
    CHECK( selected[0] == "adhesive bandage" );
    CHECK( selected[1] == "9x19mm JHP, reloaded" );
    CHECK( selected[2] == "Glock 9x19mm 15-round magazine" );
    CHECK( selected[3] == "small plastic bag" );
}

TEST_CASE( "llm_intent_resolves_move_deltas_to_snapshot_targets", "[llm_intent]" )
{
    const tripoint_abs_ms origin( 100, 200, 7 );
    CHECK( llm_intent::resolve_move_target_for_test( origin, point( 4, -2 ) ) ==
           tripoint_abs_ms( 104, 202, 7 ) );
    CHECK( llm_intent::resolve_move_target_for_test( origin, point( -1, 3 ) ) ==
           tripoint_abs_ms( 99, 197, 7 ) );
}

TEST_CASE( "llm_intent_move_targets_reuse_existing_tile_pathing", "[llm_intent]" )
{
    override_option opt_llm_intent( "LLM_INTENT_ENABLE", "true" );
    setup_snapshot_test_scene();

    map &here = get_map();
    avatar &player_character = get_avatar();
    player_character.setpos( here, tripoint_bub_ms( 46, 50, 0 ) );

    npc &listener = spawn_test_npc_at( point_bub_ms( 50, 50 ), "Listener NPC" );
    listener.set_fac( faction_your_followers );

    const tripoint_abs_ms target = llm_intent::resolve_move_target_for_test( listener.pos_abs(),
                                   point( 2, 1 ) );
    listener.set_llm_intent_move_target( target, llm_intent_action::wait_here );
    run_npc_turns( listener, 6 );

    CHECK( listener.pos_abs() == target );
    CHECK_FALSE( listener.goto_to_this_pos.has_value() );
    CHECK( listener.mission == NPC_MISSION_GUARD_ALLY );
    CHECK( listener.get_attitude() == NPCATT_NULL );

    clear_npcs();
}

TEST_CASE( "llm_intent_unreachable_move_target_clears_stale_path", "[llm_intent]" )
{
    override_option opt_llm_intent( "LLM_INTENT_ENABLE", "true" );
    setup_snapshot_test_scene();

    map &here = get_map();
    avatar &player_character = get_avatar();
    player_character.setpos( here, tripoint_bub_ms( 44, 50, 0 ) );

    npc &listener = spawn_test_npc_at( point_bub_ms( 50, 50 ), "Listener NPC" );
    listener.set_fac( faction_your_followers );
    listener.set_attitude( NPCATT_FOLLOW );

    const tripoint_bub_ms stale_path_target( 54, 50, 0 );
    REQUIRE( listener.update_path( stale_path_target ) );
    REQUIRE_FALSE( listener.path.empty() );

    const tripoint_bub_ms unreachable_target( 50, 49, 0 );
    here.ter_set( unreachable_target, ter_t_rock_wall );
    REQUIRE_FALSE( listener.update_path( unreachable_target, false, false ) );
    REQUIRE_FALSE( listener.path.empty() );

    const tripoint_abs_ms ordered_target = here.get_abs( unreachable_target );
    const tripoint_abs_ms initial_position = listener.pos_abs();
    listener.set_llm_intent_move_target( ordered_target, llm_intent_action::wait_here );
    run_npc_turns( listener, 1 );

    CHECK( listener.pos_abs() == initial_position );
    CHECK_FALSE( listener.goto_to_this_pos.has_value() );
    CHECK( listener.path.empty() );

    const int initial_player_distance = rl_dist( listener.pos_bub(), player_character.pos_bub() );
    run_npc_turns( listener, 1 );
    CHECK( rl_dist( listener.pos_bub(), player_character.pos_bub() ) < initial_player_distance );

    clear_npcs();
}

TEST_CASE( "llm_intent_hold_position_releases_when_player_gets_far", "[llm_intent]" )
{
    override_option opt_llm_intent( "LLM_INTENT_ENABLE", "true" );
    setup_snapshot_test_scene();

    map &here = get_map();
    avatar &player_character = get_avatar();
    player_character.setpos( here, tripoint_bub_ms( 48, 50, 0 ) );

    npc &listener = spawn_test_npc_at( point_bub_ms( 50, 50 ), "Listener NPC" );
    listener.set_fac( faction_your_followers );

    listener.set_llm_intent_move_target( listener.pos_abs(), llm_intent_action::hold_position );
    run_npc_turns( listener, 1 );

    REQUIRE( listener.mission == NPC_MISSION_GUARD_ALLY );
    REQUIRE( listener.get_attitude() == NPCATT_NULL );

    player_character.setpos( here, tripoint_bub_ms( 80, 80, 0 ) );
    run_npc_turns( listener, 1 );

    CHECK( listener.get_attitude() == NPCATT_FOLLOW );
    CHECK( listener.mission == NPC_MISSION_NULL );
    CHECK_FALSE( listener.guard_pos.has_value() );

    clear_npcs();
}

TEST_CASE( "llm_intent_wait_here_stays_guarded_when_player_gets_far", "[llm_intent]" )
{
    override_option opt_llm_intent( "LLM_INTENT_ENABLE", "true" );
    setup_snapshot_test_scene();

    map &here = get_map();
    avatar &player_character = get_avatar();
    player_character.setpos( here, tripoint_bub_ms( 48, 50, 0 ) );

    npc &listener = spawn_test_npc_at( point_bub_ms( 50, 50 ), "Listener NPC" );
    listener.set_fac( faction_your_followers );

    listener.set_llm_intent_move_target( listener.pos_abs(), llm_intent_action::wait_here );
    run_npc_turns( listener, 1 );

    REQUIRE( listener.mission == NPC_MISSION_GUARD_ALLY );
    REQUIRE( listener.get_attitude() == NPCATT_NULL );

    player_character.setpos( here, tripoint_bub_ms( 80, 80, 0 ) );
    run_npc_turns( listener, 1 );

    CHECK( listener.get_attitude() == NPCATT_NULL );
    CHECK( listener.mission == NPC_MISSION_GUARD_ALLY );
    CHECK( listener.guard_pos == listener.pos_abs() );

    clear_npcs();
}

TEST_CASE( "llm_intent_snapshot_request_resolves_lettered_neutral_targets", "[llm_intent]" )
{
    override_option opt_llm_intent( "LLM_INTENT_ENABLE", "true" );
    setup_snapshot_test_scene();

    map &here = get_map();
    avatar &player_character = get_avatar();
    player_character.setpos( here, tripoint_bub_ms( 46, 50, 0 ) );

    npc &listener = spawn_test_npc_at( point_bub_ms( 50, 50 ), "Listener NPC" );
    listener.set_fac( faction_your_followers );

    npc &neutral_npc = spawn_test_npc_at( point_bub_ms( 54, 50 ), "Neutral NPC" );
    REQUIRE( listener.attitude_to( neutral_npc ) == Creature::Attitude::NEUTRAL );
    REQUIRE( here.furn_set( neutral_npc.pos_bub(), furn_f_table ) );

    const std::string snapshot = llm_intent::build_snapshot_for_test(
                                     listener, "Attack the neutral target.", "req-target" );
    REQUIRE( snapshot.find( "B ... Neutral NPC neutral threat=" ) != std::string::npos );

    std::vector<std::string> actions;
    std::string attack_target;
    std::optional<point> move_delta;
    std::string terminal_state;
    std::string error;
    REQUIRE( llm_intent::parse_action_csv_for_test(
                 "Engaging|attack=B", actions, attack_target,
                 move_delta, terminal_state, error ) );
    REQUIRE( attack_target == "b" );

    listener.set_llm_intent_actions( {}, "stale-request", attack_target );
    listener.move();
    CHECK( listener.attitude_to( neutral_npc ) == Creature::Attitude::NEUTRAL );

    listener.set_llm_intent_actions( {}, "req-target", attack_target );
    listener.move();

    CHECK( listener.current_target() == &neutral_npc );
    CHECK( listener.attitude_to( neutral_npc ) == Creature::Attitude::HOSTILE );
}

TEST_CASE( "llm_intent_snapshot_does_not_reuse_target_handles_after_z", "[llm_intent]" )
{
    setup_snapshot_test_scene();

    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 5, 5, 0 ) );
    npc &listener = spawn_test_npc_at( point_bub_ms( 50, 50 ), "Listener NPC" );

    int spawned = 0;
    for( int y = 45; y <= 55 && spawned < 27; ++y ) {
        for( int x = 45; x <= 55 && spawned < 27; ++x ) {
            const tripoint_bub_ms pos( x, y, 0 );
            if( pos == listener.pos_bub() ) {
                continue;
            }
            spawn_test_monster( mon_zombie.str(), pos );
            ++spawned;
        }
    }
    REQUIRE( spawned == 27 );

    const std::string snapshot = llm_intent::build_snapshot_for_test(
                                     listener, "Watch the horde.", "req-dense" );
    const size_t map_start = snapshot.find( "map:\n" );
    REQUIRE( map_start != std::string::npos );

    std::istringstream map_stream( snapshot.substr( map_start + 5 ) );
    std::string row;
    REQUIRE( std::getline( map_stream, row ) );
    REQUIRE( std::getline( map_stream, row ) );

    std::array<int, 26> handle_counts{};
    int unlettered_creatures = 0;
    int map_rows = 0;
    while( std::getline( map_stream, row ) && row.rfind( "dy=", 0 ) == 0 ) {
        const size_t grid_start = row.find( ' ' );
        REQUIRE( grid_start != std::string::npos );
        const std::string grid = row.substr( grid_start + 1 );
        REQUIRE( grid.size() == 41 );
        ++map_rows;
        for( char glyph : grid ) {
            if( glyph >= 'a' && glyph <= 'z' ) {
                ++handle_counts[static_cast<size_t>( glyph - 'a' )];
            } else if( glyph >= 'A' && glyph <= 'Z' ) {
                ++handle_counts[static_cast<size_t>( glyph - 'A' )];
            } else if( glyph == '?' ) {
                ++unlettered_creatures;
            }
        }
    }

    CHECK( map_rows == 41 );
    CHECK( unlettered_creatures == 1 );
    for( int count : handle_counts ) {
        CHECK( count == 1 );
    }

    const size_t creature_legend_start = snapshot.find(
            "creature legend with attitude and threat level:\n" );
    const size_t map_axes_start = snapshot.find( "map axes:", creature_legend_start );
    REQUIRE( creature_legend_start != std::string::npos );
    REQUIRE( map_axes_start != std::string::npos );
    const std::string creature_legend = snapshot.substr(
                                            creature_legend_start, map_axes_start - creature_legend_start );
    CHECK( creature_legend.find( "? ... zombie" ) == std::string::npos );

    clear_creatures();
}
