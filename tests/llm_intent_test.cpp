#include <map>
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
#include "monster.h"
#include "npc.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "point.h"

namespace llm_intent
{
std::string build_snapshot_for_test( npc &listener, const std::string &player_utterance,
                                     const std::string &request_id );
std::string build_action_prompt_for_test( const std::string &npc_name,
        const std::string &player_utterance,
        const std::string &snapshot );
size_t look_around_selection_limit_for_test();
std::vector<std::string> parse_look_around_response_for_test( const std::string &text,
        const std::vector<std::string> &allowed_names );
bool parse_move_field_for_test( const std::string &field, point &delta,
                                std::string &terminal_state,
                                std::string &error );
tripoint_abs_ms resolve_move_target_for_test( const tripoint_abs_ms &origin,
        const point &delta );
} // namespace llm_intent

static const faction_id faction_your_followers( "your_followers" );
static const mtype_id mon_zombie( "mon_zombie" );
static const string_id<npc_template> npc_template_test_talker( "test_talker" );

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

    CHECK_FALSE( llm_intent::parse_move_field_for_test( "move: E E N hold_position", delta,
                 terminal_state, error ) );
    CHECK( error == "Move field must use move=<dx>,<dy> <state>." );

    CHECK_FALSE( llm_intent::parse_move_field_for_test( "move E E N hold_position", delta,
                 terminal_state, error ) );
    CHECK( error == "Move field must use move=<dx>,<dy> <state>." );

    CHECK_FALSE( llm_intent::parse_move_field_for_test( "move=4 east hold_position", delta,
                 terminal_state, error ) );
    CHECK_FALSE( error.empty() );
}

TEST_CASE( "llm_intent_look_around_supports_four_selected_items", "[llm_intent]" )
{
    CHECK( llm_intent::look_around_selection_limit_for_test() == 4 );

    const std::vector<std::string> selected = llm_intent::parse_look_around_response_for_test(
                "item_1, item_2, item_3, item_4",
                { "adhesive bandage", "9x19mm JHP, reloaded",
                    "Glock 9x19mm 15-round magazine", "small plastic bag" } );
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

TEST_CASE( "llm_intent_can_resolve_lettered_neutral_targets", "[llm_intent]" )
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

    listener.set_llm_intent_legend_map( "req-target", {{ 'b', g->shared_from( neutral_npc ) }} );
    listener.set_llm_intent_actions( {}, "req-target", "b" );
    listener.move();

    CHECK( listener.current_target() == &neutral_npc );
    CHECK( listener.attitude_to( neutral_npc ) == Creature::Attitude::HOSTILE );
}
