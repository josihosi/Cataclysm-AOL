#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>

#include "bandit_live_world_probe.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "game.h"
#include "json.h"
#include "json_loader.h"
#include "worldfactory.h"

namespace
{
class scoped_receipt_environment
{
    public:
        void set( const std::string &name, const std::optional<std::string> &value ) {
            const char *previous = std::getenv( name.c_str() );
            saved_.emplace( name, previous ? std::optional<std::string>( previous ) : std::nullopt );
            REQUIRE( assign( name, value ) == 0 );
        }

        ~scoped_receipt_environment() {
            for( const auto &entry : saved_ ) {
                assign( entry.first, entry.second );
            }
        }

    private:
        static int assign( const std::string &name, const std::optional<std::string> &value ) {
#if defined(_WIN32)
            return _putenv_s( name.c_str(), value ? value->c_str() : "" );
#else
            return value ? setenv( name.c_str(), value->c_str(), 1 ) : unsetenv( name.c_str() );
#endif
        }
        std::map<std::string, std::optional<std::string>> saved_;
};

class blocked_world_path
{
    public:
        explicit blocked_world_path( const std::filesystem::path &world_path ) : original_( world_path ),
            moved_( world_path ) {
            moved_ += ".blocked_save_receipt_test"; // NOLINT(cata-u8-path)
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

TEST_CASE( "unicode world saves with unicode receipt tree entries", "[savegame][unicode]" )
{
    REQUIRE( g != nullptr );
    REQUIRE( world_generator != nullptr );
    REQUIRE( world_generator->active_world != nullptr );
    const std::filesystem::path world_path =
        world_generator->active_world->folder_path().get_unrelative_path();
    REQUIRE( world_path.u8string().find( "测试世界" ) != std::string::npos );
    const std::filesystem::path entry = world_path / std::filesystem::u8path( "receipt-测试.txt" );
    const std::filesystem::path events = world_path.parent_path() / "caol-unicode-receipt-test.jsonl";
    REQUIRE_FALSE( std::filesystem::exists( entry ) );
    REQUIRE_FALSE( std::filesystem::exists( events ) );
    on_out_of_scope remove_entry( [&]() {
        std::filesystem::remove( entry );
        std::filesystem::remove( events );
    } );
    scoped_receipt_environment environment;
    for( const char *name : {
             "OPENCLAW_CERTIFICATION_ROUND_ID", "OPENCLAW_CERTIFICATION_LEASE_ID",
             "OPENCLAW_CERTIFICATION_SAVE_CAPABILITY", "OPENCLAW_CERTIFICATION_PREVIOUS_WORLD_TREE_SHA256",
             "OPENCLAW_CERTIFICATION_PREVIOUS_WORLD_SAVE_SHA256", "OPENCLAW_CERTIFICATION_SAVE_SEQUENCE",
             "OPENCLAW_HARNESS_TRANSITION_EVENT_PATH", "OPENCLAW_HARNESS_RUN_ID"
         } ) {
        environment.set( name, std::nullopt );
    }
    {
        std::ofstream output( entry, std::ios::binary );
        REQUIRE( output.good() );
        output << "receipt tree control\n";
        REQUIRE( output.good() );
    }
    g->save_is_dirty = false;
    REQUIRE( g->save() );
    CHECK( g->last_save_result() == "saved" );
    CHECK( g->last_confirmed_save_turn() );
    CHECK( std::filesystem::exists( entry ) );

    environment.set( "OPENCLAW_HARNESS_TRANSITION_EVENT_PATH", events.u8string() );
    environment.set( "OPENCLAW_HARNESS_RUN_ID", "unicode-receipt-control" );
    // Without certification prerequisites, even an invalid UTF-8 path is not read.
    REQUIRE_NOTHROW( bandit_live_world_probe::record_certification_save_receipt( 1,
                     std::string( 1, static_cast<char>( 0xff ) ) ) );
    CHECK_FALSE( std::filesystem::exists( events ) );
    environment.set( "OPENCLAW_CERTIFICATION_ROUND_ID", "test-round" );
    environment.set( "OPENCLAW_CERTIFICATION_LEASE_ID", "test-lease" );
    environment.set( "OPENCLAW_CERTIFICATION_SAVE_CAPABILITY", "synthetic-test-capability" );
    environment.set( "OPENCLAW_CERTIFICATION_PREVIOUS_WORLD_TREE_SHA256", std::string( 64, 'a' ) );
    environment.set( "OPENCLAW_CERTIFICATION_PREVIOUS_WORLD_SAVE_SHA256", std::string( 64, 'b' ) );
    REQUIRE_NOTHROW( bandit_live_world_probe::record_certification_save_receipt( 1,
                     std::string( 1, static_cast<char>( 0xff ) ) ) );
    CHECK_FALSE( std::filesystem::exists( events ) );
    environment.set( "OPENCLAW_CERTIFICATION_SAVE_SEQUENCE", "1" );
    bandit_live_world_probe::record_certification_save_receipt( 1, world_path.u8string() );
    {
        std::ofstream output( entry, std::ios::binary | std::ios::trunc );
        REQUIRE( output.good() );
        output << "changed Unicode entry\n";
        REQUIRE( output.good() );
    }
    environment.set( "OPENCLAW_CERTIFICATION_SAVE_SEQUENCE", "2" );
    bandit_live_world_probe::record_certification_save_receipt( 2, world_path.u8string() );
    std::ifstream input( events );
    REQUIRE( input.good() );
    std::string first_line;
    std::string second_line;
    REQUIRE( std::getline( input, first_line ) );
    REQUIRE( std::getline( input, second_line ) );
    const JsonObject first = json_loader::from_string( first_line ).get_object();
    const JsonObject second = json_loader::from_string( second_line ).get_object();
    first.allow_omitted_members();
    second.allow_omitted_members();
    const JsonObject first_receipt = first.get_object( "certification_save_receipt" );
    const JsonObject second_receipt = second.get_object( "certification_save_receipt" );
    first_receipt.allow_omitted_members();
    second_receipt.allow_omitted_members();
    const std::string first_digest = first_receipt.get_string( "current_world_tree_sha256" );
    const std::string second_digest = second_receipt.get_string( "current_world_tree_sha256" );
    CHECK( first_receipt.get_string( "round_id" ) == "test-round" );
    CHECK( first_digest.size() == 64 );
    CHECK( second_digest.size() == 64 );
    CHECK( first_digest != second_digest );
    CHECK( second_receipt.get_string( "current_world_save_sha256" ) == second_digest );
    CHECK( second_receipt.get_string( "proof" ).size() == 64 );
}

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
