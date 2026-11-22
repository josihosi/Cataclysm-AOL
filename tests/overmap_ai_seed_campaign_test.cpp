#include <cstdlib>
#include <iostream>

#include "bandit_live_world.h"
#include "cata_catch.h"
#include "rng.h"

// A deliberately small production-connected pilot.  The fixture applies one
// resolved setup parameter and reports only decisions actually evaluated.
TEST_CASE( "seed campaign first sight pilot", "[.][seed_campaign][nogame]" )
{
    const char *value = std::getenv( "CAOL_CAMPAIGN_ROLLING" );
    REQUIRE( value != nullptr );
    const std::string rolling_string( value );
    REQUIRE( ( rolling_string == "0" || rolling_string == "1" ) );
    const bool rolling = rolling_string == "1";
    const bool player = rng( 0, 1 ) != 0;
    const bool follower = rng( 0, 1 ) != 0;
    const bool reached = player || follower;
    bool parley = false;
    if( reached ) {
        parley = bandit_live_world::normal_shakedown_first_sight_requires_parley( player, follower,
                 rolling );
        CHECK( parley == !rolling );
    }
    std::cout << "CAMPAIGN_EVENT {\"player\":" << player << ",\"follower\":" << follower
              << ",\"rolling\":" << rolling << ",\"reached\":" << reached
              << ",\"parley\":" << parley << "}\n";
}
