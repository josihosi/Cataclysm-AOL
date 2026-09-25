#include <filesystem>
#include <fstream>
#include <string>

#include "calendar.h"
#include "cata_catch.h"
#include "game.h"
#include "worldfactory.h"

namespace
{
class blocked_world_path
{
    public:
        explicit blocked_world_path( const std::filesystem::path &world_path ) : original_( world_path ),
            moved_( world_path.string() + ".blocked_save_receipt_test" ) {
            std::filesystem::rename( original_, moved_ );
            std::ofstream blocker( original_ );
        }

        ~blocked_world_path() {
            std::error_code error;
            std::filesystem::remove( original_, error );
            std::filesystem::rename( moved_, original_, error );
        }

    private:
        std::filesystem::path original_;
        std::filesystem::path moved_;
};
} // namespace

TEST_CASE( "quicksave_reports_confirmed_turn_and_recovers_after_failed_write",
           "[savegame][quicksave][regression]" )
{
    REQUIRE( g != nullptr );
    REQUIRE( world_generator != nullptr );
    REQUIRE( world_generator->active_world != nullptr );
    g->save_is_dirty = false;
    const std::string world_name = world_generator->active_world->world_name;
    const int initial_turn = to_turns<int>( calendar::turn - calendar::turn_zero );

    REQUIRE( g->save() );
    CHECK( g->last_save_result() == "saved" );
    REQUIRE( g->last_confirmed_save_turn() );
    CHECK( *g->last_confirmed_save_turn() == initial_turn );

    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world_name ) );
    REQUIRE( g->last_confirmed_save_turn() );
    CHECK( *g->last_confirmed_save_turn() == initial_turn );
    REQUIRE( g->quicksave() );
    CHECK( g->last_save_result() == "not_needed" );

    // Elapsed simulation time alone does not increment the player action
    // counter.  The no-op must retain the older disk turn.
    calendar::turn += 15_minutes;
    REQUIRE( g->quicksave() );
    CHECK( g->last_save_result() == "not_needed" );
    REQUIRE( g->last_confirmed_save_turn() );
    CHECK( *g->last_confirmed_save_turn() == initial_turn );

    REQUIRE( g->save() );
    const int advanced_turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    CHECK( g->last_save_result() == "saved" );
    REQUIRE( g->last_confirmed_save_turn() );
    CHECK( *g->last_confirmed_save_turn() == advanced_turn );

    const std::filesystem::path world_path =
        world_generator->active_world->folder_path().get_unrelative_path();
    {
        blocked_world_path blocker( world_path );
        CHECK_FALSE( g->save() );
        CHECK( g->last_save_result() == "failed_exception" );
        CHECK_FALSE( g->last_confirmed_save_turn() );
    }
    // No counted player action occurred, but the failed write cannot be
    // treated as an unchanged completed save.
    REQUIRE( g->quicksave() );
    CHECK( g->last_save_result() == "saved" );
    REQUIRE( g->last_confirmed_save_turn() );
    CHECK( *g->last_confirmed_save_turn() == advanced_turn );
}
