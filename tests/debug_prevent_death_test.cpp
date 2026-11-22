#include "avatar.h"
#include "cata_catch.h"
#include "effect_on_condition.h"
#include "player_helpers.h"
#include "type_id.h"

TEST_CASE( "debug_prevent_death_handles_repeated_lethal_damage", "[eoc][death]" )
{
    clear_avatar();
    avatar &player = get_avatar();
    const bodypart_id torso( "torso" );
    const trait_id protection( "DEBUG_PREVENT_DEATH" );

    // An unprotected lethal injury remains lethal when the pre-death hooks run.
    player.apply_damage( nullptr, torso, player.get_part_hp_cur( torso ) + 1 );
    REQUIRE( player.is_dead_state() );
    effect_on_conditions::prevent_death();
    CHECK( player.is_dead_state() );

    clear_avatar();
    player.set_mutation( protection );
    for( int attempt = 0; attempt < 3; ++attempt ) {
        CAPTURE( attempt );
        player.apply_damage( nullptr, torso, player.get_part_hp_cur( torso ) + 1 );
        REQUIRE( player.is_dead_state() );
        effect_on_conditions::prevent_death();
        CHECK_FALSE( player.is_dead_state() );
        CHECK( player.get_part_hp_cur( torso ) == 1 );
    }
    clear_avatar();
}
