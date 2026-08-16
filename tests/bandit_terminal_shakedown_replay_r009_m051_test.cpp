#include "bandit_live_world.h"

#include <sstream>
#include <string>

#include "cata_catch.h"
#include "json.h"
#include "json_loader.h"

namespace
{

std::string serialize_terminal_replay_world( const bandit_live_world::world_state &world )
{
    std::ostringstream out;
    JsonOut json( out, true );
    world.serialize( json );
    return out.str();
}

bandit_live_world::world_state reload_terminal_replay_world(
    const bandit_live_world::world_state &world )
{
    JsonValue saved = json_loader::from_string( serialize_terminal_replay_world( world ) );
    bandit_live_world::world_state reloaded;
    reloaded.deserialize( saved.get_object() );
    return reloaded;
}

} // namespace

TEST_CASE( "R009-M051 observes released terminal shakedown replays without mutation",
           "[bandit][live_world][R009-M051][terminal_shakedown]" )
{
    const tripoint_abs_omt target( 41, 52, 0 );
    const bandit_live_world::terminal_hostile_shakedown_replay_identity identity = {
        "shared-target", target, 3, "camp-a#hostile:4", "camp-a#scout:3:report:4", 4
    };

    bandit_live_world::world_state settled;
    bandit_live_world::site_record &winner = settled.sites.emplace_back();
    winner.site_id = "camp-a";
    winner.last_hostile_shakedown_aftermath_key = identity.operation_id + "|" +
            identity.report_key + "|" + std::to_string( identity.generation );
    winner.last_hostile_shakedown_operation_id = identity.operation_id;
    winner.last_hostile_shakedown_report_key = identity.report_key;
    winner.last_hostile_shakedown_generation = identity.generation;
    settled.hostile_target_opportunities.push_back( { identity.target_id, identity.target_omt,
            75, 4, 2, identity.target_revision, identity.operation_id, identity.report_key,
            identity.generation } );

    REQUIRE_FALSE( winner.active_hostile_operation.is_active() );
    const bandit_live_world::world_state reloaded = reload_terminal_replay_world( settled );
    REQUIRE( reloaded.sites.size() == 1 );
    const bandit_live_world::site_record &released_winner = reloaded.sites.front();
    REQUIRE_FALSE( released_winner.active_hostile_operation.is_active() );
    const std::string saved = serialize_terminal_replay_world( reloaded );

    CHECK( bandit_live_world::observe_terminal_hostile_shakedown_replay( reloaded,
           released_winner, identity ) ==
           bandit_live_world::terminal_hostile_shakedown_replay_disposition::exact_duplicate );
    CHECK( serialize_terminal_replay_world( reloaded ) == saved );

    bandit_live_world::terminal_hostile_shakedown_replay_identity stale = identity;
    stale.generation++;
    CHECK( bandit_live_world::observe_terminal_hostile_shakedown_replay( reloaded,
           released_winner, stale ) ==
           bandit_live_world::terminal_hostile_shakedown_replay_disposition::stale_replay );
    CHECK( serialize_terminal_replay_world( reloaded ) == saved );
}
