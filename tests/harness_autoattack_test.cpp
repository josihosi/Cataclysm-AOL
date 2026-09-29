#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "action.h"
#include "avatar.h"
#include "avatar_action.h"
#include "calendar.h"
#include "cata_catch.h"
#include "effect.h"
#include "game.h"
#include "input_context.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "monster.h"
#include "npc.h"
#include "player_helpers.h"
#include "point.h"
#include "type_id.h"

TEST_CASE( "semantic World advertises the registered native Tab autoattack",
           "[semantic_surface][harness][autoattack]" )
{
    const input_context context = get_default_mode_input_context();
    REQUIRE( context.is_registered_action( "autoattack" ) );
    CHECK( look_up_action( "autoattack" ) == ACTION_AUTOATTACK );
    const std::vector<std::pair<std::string, std::string>> actions =
        openclaw_harness_world_actions( context );
    const auto offered = std::find_if( actions.begin(), actions.end(),
    []( const std::pair<std::string, std::string> &action ) {
        return action.first == "world.autoattack";
    } );
    REQUIRE( offered != actions.end() );
    CHECK( offered->second == "autoattack" );
}

TEST_CASE( "native autoattack passes a turn without a hostile and spends moves against one",
           "[semantic_surface][harness][autoattack]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    you.set_moves( 1000 );
    avatar_action::autoattack( you, here );
    CHECK( you.get_moves() == 0 );

    you.set_moves( 1000 );
    g->set_safe_mode( SAFE_MODE_STOP );
    avatar_action::autoattack( you, here );
    CHECK( you.get_moves() == 1000 );
    g->set_safe_mode( SAFE_MODE_OFF );

    you.set_moves( 1000 );
    spawn_test_monster( "mon_zombie", you.pos_bub( here ) + tripoint::east );
    avatar_action::autoattack( you, here );
    CHECK( you.get_moves() < 1000 );
}

TEST_CASE( "native autoattack excludes a non-enemy NPC from target selection",
           "[semantic_surface][harness][autoattack]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const character_id id = here.place_npc( you.pos_bub( here ).xy() + point( 1, 0 ),
                            string_id<npc_template>( "test_talker" ) );
    g->load_npcs();
    npc *neighbor = g->find_npc( id );
    REQUIRE( neighbor != nullptr );
    REQUIRE_FALSE( neighbor->is_enemy() );
    const int hp = neighbor->get_hp();
    you.set_moves( 1000 );
    avatar_action::autoattack( you, here );
    CHECK( you.get_moves() == 0 );
    CHECK( neighbor->get_hp() == hp );
}

TEST_CASE( "native autoattack refuses a character that cannot attack",
           "[semantic_surface][harness][autoattack]" )
{
    load_effect_type( json_loader::from_string(
                          R"({"id":"harness_autoattack_cannot_attack","name":["Blocked"],"desc":[""],"flags":["CANNOT_ATTACK"]})" ).get_object(),
                      "dda" );

    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    you.add_effect( efftype_id( "harness_autoattack_cannot_attack" ), 1_minutes );
    REQUIRE( you.has_flag( json_character_flag( "CANNOT_ATTACK" ) ) );
    you.set_moves( 1000 );
    avatar_action::autoattack( you, get_map() );
    CHECK( you.get_moves() == 1000 );
}
