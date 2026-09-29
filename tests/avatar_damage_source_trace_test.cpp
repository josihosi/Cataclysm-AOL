#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include "bodypart.h"
#include "cata_catch.h"
#include "character.h"
#include "debug.h"
#include "map.h"
#include "map_helpers.h"
#include "npc.h"
#include "player_helpers.h"
#include "projectile.h"
#include "uistate.h"

namespace
{
class scoped_damage_trace_environment
{
    public:
        scoped_damage_trace_environment() {
            if( const char *value = std::getenv( "OPENCLAW_HARNESS_UI_TRACE" ) ) {
                old_trace = value;
            }
            if( const char *value = std::getenv( "OPENCLAW_HARNESS_RUN_ID" ) ) {
                old_run = value;
            }
        }
        ~scoped_damage_trace_environment() {
            restore( "OPENCLAW_HARNESS_UI_TRACE", old_trace );
            restore( "OPENCLAW_HARNESS_RUN_ID", old_run );
        }
    private:
        static void restore( const char *name, const std::optional<std::string> &value ) {
            if( value ) {
                setenv( name, value->c_str(), 1 );
            } else {
                unsetenv( name );
            }
        }
        std::optional<std::string> old_trace;
        std::optional<std::string> old_run;
};

class scoped_stderr_capture
{
    public:
        scoped_stderr_capture() : old( std::cerr.rdbuf( output.rdbuf() ) ) {}
        ~scoped_stderr_capture() {
            std::cerr.rdbuf( old );
        }
        std::string str() const {
            return output.str();
        }
    private:
        std::ostringstream output;
        std::streambuf *old;
};
} // namespace

TEST_CASE( "avatar_damage_source_trace_records_real_harm_only_in_bound_harness",
           "[character][harness][damage]" )
{
    scoped_damage_trace_environment saved_environment;
    clear_avatar();
    clear_npcs();
    clear_map_without_vision();
    map &here = get_map();
    Character &avatar = get_player_character();
    avatar.setpos( here, tripoint_bub_ms{ 50, 50, 0 } );
    npc &shooter = spawn_npc( point_bub_ms{ 50, 46 }, "mi-go_prisoner" );
    const bool old_distraction_attack = uistate.distraction_attack;
    uistate.distraction_attack = false;
    const bodypart_id torso( "torso" );
    const int initial_hp = avatar.get_part_hp_cur( torso );

    setenv( "OPENCLAW_HARNESS_RUN_ID", "damage-source-test-run", 1 );
    unsetenv( "OPENCLAW_HARNESS_UI_TRACE" );
    std::string ordinary_log;
    {
        scoped_stderr_capture capture;
        avatar.apply_damage( &shooter, torso, 1 );
        ordinary_log = capture.str();
    }
    CHECK( ordinary_log.find( "component=avatar_damage_source" ) == std::string::npos );
    CHECK( avatar.get_part_hp_cur( torso ) == initial_hp - 1 );

    setenv( "OPENCLAW_HARNESS_UI_TRACE", "1", 1 );
    std::string bound_log;
    {
        scoped_stderr_capture capture;
        avatar.apply_damage( &shooter, torso, 2 );
        bound_log = capture.str();
    }
    CHECK( bound_log.find( "component=avatar_damage_source" ) != std::string::npos );
    CHECK( bound_log.find( "run_id=damage-source-test-run" ) != std::string::npos );
    CHECK( bound_log.find( "source_kind=character" ) != std::string::npos );
    CHECK( bound_log.find( "source_character_id=" + std::to_string(
                               shooter.getID().get_value() ) ) != std::string::npos );
    CHECK( bound_log.find( "target_character_id=" + std::to_string(
                               avatar.getID().get_value() ) ) != std::string::npos );
    CHECK( bound_log.find( "body_part=torso damage=2" ) != std::string::npos );
    CHECK( bound_log.find( "hp_before=" + std::to_string( initial_hp - 1 ) ) != std::string::npos );
    CHECK( bound_log.find( "hp_after=" + std::to_string( initial_hp - 3 ) ) != std::string::npos );
    CHECK( avatar.get_part_hp_cur( torso ) == initial_hp - 3 );

    dealt_projectile_attack shot;
    shot.last_hit_critter = nullptr;
    shot.proj.speed = 1000;
    shot.proj.impact = damage_instance( damage_type_id( "bullet" ), 20 );
    shot.proj.range = 30;
    shot.proj.critical_multiplier = 1;
    std::string projectile_log;
    {
        scoped_stderr_capture capture;
        avatar.deal_projectile_attack( &here, &shooter, shot, 0.0, false );
        projectile_log = capture.str();
    }
    REQUIRE( shot.last_hit_critter == &avatar );
    REQUIRE( shot.dealt_dam.total_damage() > 0 );
    CHECK( projectile_log.find( "component=avatar_damage_source" ) != std::string::npos );
    CHECK( projectile_log.find( "source_character_id=" + std::to_string(
                                    shooter.getID().get_value() ) ) != std::string::npos );

    std::string unknown_source_log;
    {
        scoped_stderr_capture capture;
        avatar.apply_damage( nullptr, torso, 1 );
        shooter.apply_damage( &avatar, torso, 1 );
        unknown_source_log = capture.str();
    }
    CHECK( unknown_source_log.find( "source_kind=none source_character_id=-1" ) != std::string::npos );
    CHECK( unknown_source_log.find( "target_character_id=" + std::to_string(
                                        shooter.getID().get_value() ) ) == std::string::npos );
    uistate.distraction_attack = old_distraction_attack;
}
