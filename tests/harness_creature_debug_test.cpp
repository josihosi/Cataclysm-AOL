#include <algorithm>
#include <string>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "creature_tracker.h"
#include "game.h"
#include "harness_creature_debug.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "monster.h"
#include "npc.h"
#include "player_helpers.h"

TEST_CASE( "debug creature removal binds the selected identity and preserves its neighbour",
           "[semantic_surface][harness_creature_debug]" )
{
    clear_avatar();
    clear_map();
    avatar &viewer = get_avatar();
    const bool target_is_npc = GENERATE( false, true );
    const tripoint_bub_ms target_pos = viewer.pos_bub() + tripoint::east;
    monster &neighbour = spawn_test_monster( "mon_test_zombie",
                         viewer.pos_bub() + tripoint::west );
    Creature *target = target_is_npc ?
                       static_cast<Creature *>( &spawn_npc( target_pos.xy(), "test_talker" ) ) :
                       static_cast<Creature *>( &spawn_test_monster( "mon_test_zombie", target_pos ) );
    const std::string identity = harness_creature_identity( *target );
    const int neighbour_hp = neighbour.get_hp();
    const int player_hp = viewer.get_hp();
    const time_point before = calendar::turn;
    get_map().build_map_cache( viewer.pos_bub().z() );
    const auto actions = harness_creature_debug_actions( viewer );
    REQUIRE( std::any_of( actions.begin(), actions.end(), [&identity]( const auto & action ) {
        return action.id == "world.debug_kill_creature" && action.stable_id == identity;
    } ) );
    CHECK( harness_debug_kill_creature( viewer, "missing", "bad-request", "test-run" ).empty() );
    CHECK( harness_debug_kill_creature( viewer, harness_creature_identity( viewer ),
                                        "player-request", "test-run" ).empty() );
    const std::string receipt = harness_debug_kill_creature( viewer, identity, "exact-request",
                                "test-run" );
    REQUIRE_FALSE( receipt.empty() );
    const JsonObject result = json_loader::from_string( receipt );
    result.allow_omitted_members();
    CHECK( result.get_string( "identity" ) == identity );
    CHECK( result.get_string( "request_id" ) == "exact-request" );
    CHECK( harness_last_debug_creature_intervention( "test-run" ) == receipt );
    CHECK( harness_last_debug_creature_intervention( "different-run" ) == "none_this_run" );
    CHECK_FALSE( result.get_bool( "gameplay_credit" ) );
    const JsonObject old_state = result.get_object( "before" );
    old_state.allow_omitted_members();
    CHECK_FALSE( old_state.get_bool( "dead" ) );
    const JsonObject new_state = result.get_object( "after" );
    new_state.allow_omitted_members();
    CHECK( new_state.get_bool( "dead" ) );
    CHECK( get_creature_tracker().creature_at<Creature>( target_pos ) == nullptr );
    CHECK( neighbour.get_hp() == neighbour_hp );
    CHECK_FALSE( neighbour.is_dead_state() );
    CHECK( viewer.get_hp() == player_hp );
    CHECK( calendar::turn == before );
    CHECK( harness_debug_kill_creature( viewer, identity, "stale-request", "test-run" ).empty() );
}
