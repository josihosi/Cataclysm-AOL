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
    CHECK( snapshot.find( "a ... player friendly threat=" ) != std::string::npos );
    CHECK( snapshot.find( "b ... Neutral NPC neutral threat=" ) != std::string::npos );
    CHECK( snapshot.find( "c ... zombie hostile threat=" ) != std::string::npos );
}

TEST_CASE( "llm_intent_prompt_explicitly_allows_lettered_targets", "[llm_intent]" )
{
    const std::string prompt = llm_intent::build_action_prompt_for_test( "Listener NPC",
                               "Do it.", "snapshot" );

    CHECK( prompt.find( "Any creature with a map letter is a valid explicit target handle, including the player, friendlies, neutrals, and hostiles." ) != std::string::npos );
    CHECK( prompt.find( "attack=a" ) != std::string::npos );
    CHECK( prompt.find( "attack=b" ) != std::string::npos );
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
