#include <algorithm>
#include <cstdlib>

#include "avatar.h"
#include "cata_catch.h"
#include "debug_menu.h"
#include "faction.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "player_helpers.h"

static const itype_id itype_test_apple( "test_apple" );

namespace
{
class scoped_r022_environment
{
    public:
        scoped_r022_environment()
        {
            save( "OPENCLAW_HARNESS_RUN_ID", run_id_, had_run_id_ );
            save( "OPENCLAW_HARNESS_R022_TRANSACTION_ID", transaction_id_, had_transaction_id_ );
        }

        ~scoped_r022_environment()
        {
            restore( "OPENCLAW_HARNESS_RUN_ID", run_id_, had_run_id_ );
            restore( "OPENCLAW_HARNESS_R022_TRANSACTION_ID", transaction_id_, had_transaction_id_ );
            debug_menu::reset_harness_item_setup();
        }

        void set( const std::string &run_id, const std::string &transaction_id ) const {
#if defined( _WIN32 )
            _putenv_s( "OPENCLAW_HARNESS_RUN_ID", run_id.c_str() );
            _putenv_s( "OPENCLAW_HARNESS_R022_TRANSACTION_ID", transaction_id.c_str() );
#else
            setenv( "OPENCLAW_HARNESS_RUN_ID", run_id.c_str(), 1 );
            setenv( "OPENCLAW_HARNESS_R022_TRANSACTION_ID", transaction_id.c_str(), 1 );
#endif
        }

        void clear() const {
#if defined( _WIN32 )
            _putenv_s( "OPENCLAW_HARNESS_RUN_ID", "" );
            _putenv_s( "OPENCLAW_HARNESS_R022_TRANSACTION_ID", "" );
#else
            unsetenv( "OPENCLAW_HARNESS_RUN_ID" );
            unsetenv( "OPENCLAW_HARNESS_R022_TRANSACTION_ID" );
#endif
        }

    private:
        static void save( const char *name, std::string &value, bool &had_value ) {
            const char *const environment_value = std::getenv( name );
            had_value = environment_value != nullptr;
            value = had_value ? environment_value : "";
        }

        static void restore( const char *name, const std::string &value, bool had_value ) {
#if defined( _WIN32 )
            _putenv_s( name, had_value ? value.c_str() : "" );
#else
            if( had_value ) {
                setenv( name, value.c_str(), 1 );
            } else {
                unsetenv( name );
            }
#endif
        }

        std::string run_id_;
        std::string transaction_id_;
        bool had_run_id_ = false;
        bool had_transaction_id_ = false;
};

debug_menu::debug_item_spawn_request make_request( const tripoint_bub_ms &destination,
        const std::string &transaction_id )
{
    return { itype_test_apple, 3, 0, 0, get_avatar().get_faction()->id, destination, transaction_id };
}
} // namespace

TEST_CASE( "r022_harness_item_setup_is_run_scoped_and_receipted",
           "[debug][item_spawn_transaction]" )
{
    clear_avatar();
    clear_map_without_vision();
    map &here = get_map();
    const tripoint_bub_ms destination = get_avatar().pos_bub() + tripoint( 4, 0, 0 );
    scoped_r022_environment environment;
    debug_menu::reset_harness_item_setup();

    environment.clear();
    CHECK_FALSE( debug_menu::process_harness_item_setup( get_avatar().pos_bub() ) );

    environment.set( "r022-run-one", "r022-cleanup-failure" );
    const tripoint_bub_ms out_of_bounds_player_pos = get_avatar().pos_bub() + tripoint( 10000, 0, 0 );
    const auto cleanup_failure = debug_menu::process_harness_item_setup( out_of_bounds_player_pos );
    REQUIRE( cleanup_failure );
    CHECK_FALSE( cleanup_failure->transaction.accepted );
    CHECK_FALSE( cleanup_failure->cleanup.accepted );
    CHECK( cleanup_failure->transaction.failure == "declared destination is outside the loaded map" );
    CHECK( cleanup_failure->cleanup.failure == "declared destination is outside the loaded map" );

    const auto retained_cleanup_failure = debug_menu::process_harness_item_setup(
                                            get_avatar().pos_bub() );
    REQUIRE( retained_cleanup_failure );
    CHECK_FALSE( retained_cleanup_failure->transaction.accepted );
    CHECK_FALSE( retained_cleanup_failure->cleanup.accepted );
    CHECK( retained_cleanup_failure->transaction.failure == cleanup_failure->transaction.failure );
    CHECK( retained_cleanup_failure->cleanup.failure == cleanup_failure->cleanup.failure );
    CHECK( here.i_at( destination ).empty() );

    environment.set( "r022-run-one", "r022-accepted" );
    const auto first = debug_menu::process_harness_item_setup( get_avatar().pos_bub() );
    REQUIRE( first );
    CHECK( first->run_id == "r022-run-one" );
    CHECK( first->transaction.accepted );
    CHECK( first->transaction.audit_passed );
    CHECK( first->transaction.identities.size() == 3 );
    CHECK( first->cleanup.accepted );
    CHECK( first->cleanup.removed == 3 );
    CHECK( here.i_at( destination ).empty() );

    const auto repeated = debug_menu::process_harness_item_setup( get_avatar().pos_bub() );
    REQUIRE( repeated );
    CHECK( repeated->transaction.transaction_id == first->transaction.transaction_id );
    CHECK( repeated->transaction.identities.size() == first->transaction.identities.size() );
    CHECK( repeated->cleanup.removed == first->cleanup.removed );
    CHECK( here.i_at( destination ).empty() );

    environment.set( "r022-run-two", "r022-accepted" );
    const auto next_run = debug_menu::process_harness_item_setup( get_avatar().pos_bub() );
    REQUIRE( next_run );
    CHECK( next_run->run_id == "r022-run-two" );
    CHECK( next_run->transaction.accepted );
    CHECK( next_run->cleanup.removed == 3 );

    here.add_item( destination, item( itype_test_apple, calendar::turn, 0 ) );
    environment.set( "r022-run-two", "r022-rejected" );
    const auto rejected = debug_menu::process_harness_item_setup( get_avatar().pos_bub() );
    REQUIRE( rejected );
    CHECK_FALSE( rejected->transaction.accepted );
    CHECK( rejected->cleanup.accepted );
    CHECK( rejected->cleanup.retained_untagged == 1 );
    REQUIRE( here.i_at( destination ).size() == 1 );
    here.i_clear( destination );
    const auto retained_rejection = debug_menu::process_harness_item_setup( get_avatar().pos_bub() );
    REQUIRE( retained_rejection );
    CHECK_FALSE( retained_rejection->transaction.accepted );

    environment.set( "r022-run-two", "r022-supported-retry" );
    const auto retry = debug_menu::process_harness_item_setup( get_avatar().pos_bub() );
    REQUIRE( retry );
    CHECK( retry->transaction.accepted );

    here.add_item( destination, item( itype_test_apple, calendar::turn, 0 ) );
    environment.set( "r022-run-two", "r022-lifecycle-reentry" );
    const auto before_reset = debug_menu::process_harness_item_setup( get_avatar().pos_bub() );
    REQUIRE( before_reset );
    CHECK_FALSE( before_reset->transaction.accepted );
    here.i_clear( destination );
    debug_menu::reset_harness_item_setup();
    const auto after_reset = debug_menu::process_harness_item_setup( get_avatar().pos_bub() );
    REQUIRE( after_reset );
    CHECK( after_reset->transaction.accepted );
    CHECK( after_reset->cleanup.removed == 3 );
}

TEST_CASE( "debug_item_spawn_transaction_preserves_exact_tagged_map_identities",
           "[debug][item_spawn_transaction]" )
{
    clear_avatar();
    clear_map_without_vision();
    map &here = get_map();
    const tripoint_bub_ms destination = get_avatar().pos_bub() + tripoint::east;
    const debug_menu::debug_item_spawn_request request = make_request( destination, "r022-positive" );

    const debug_menu::debug_item_spawn_receipt receipt = debug_menu::debug_item_spawn_transaction(
                request );

    REQUIRE( receipt.accepted );
    REQUIRE( receipt.audit_passed );
    CHECK( receipt.zero_credit );
    REQUIRE( receipt.identities.size() == 3 );
    const map_stack items = here.i_at( destination );
    REQUIRE( items.size() == 3 );
    for( int ordinal = 0; ordinal < request.quantity; ++ordinal ) {
        const auto found = std::find_if( items.begin(), items.end(), [&]( const item & candidate ) {
            return candidate.get_var( "debug_item_spawn_transaction" ) == request.transaction_id &&
                   candidate.get_var( "debug_item_spawn_ordinal", -1 ) == ordinal;
        } );
        REQUIRE( found != items.end() );
        CHECK( found->typeId() == request.type );
        CHECK( found->charges == request.charges );
        CHECK( found->damage() == request.damage );
        CHECK( found->get_owner() == request.owner );
    }

    here.add_item( destination, item( itype_test_apple, calendar::turn, 0 ) );
    const debug_menu::debug_item_spawn_cleanup_receipt cleanup =
        debug_menu::debug_item_spawn_transaction_cleanup( request );

    CHECK( cleanup.accepted );
    CHECK( cleanup.audit_passed );
    CHECK( cleanup.zero_credit );
    CHECK( cleanup.removed == request.quantity );
    CHECK( cleanup.retained_untagged == 1 );
    REQUIRE( here.i_at( destination ).size() == 1 );
    CHECK( here.i_at( destination ).only_item().get_var( "debug_item_spawn_transaction" ).empty() );
}

TEST_CASE( "debug_item_spawn_transaction_rejects_frozen_negative_controls",
           "[debug][item_spawn_transaction]" )
{
    clear_avatar();
    clear_map_without_vision();
    map &here = get_map();
    const tripoint_bub_ms destination = get_avatar().pos_bub() + tripoint::east;

    here.add_item( destination, item( itype_test_apple, calendar::turn, 0 ) );
    debug_menu::debug_item_spawn_request occupied = make_request( destination, "r022-occupied" );
    const debug_menu::debug_item_spawn_receipt occupied_receipt =
        debug_menu::debug_item_spawn_transaction( occupied );
    CHECK_FALSE( occupied_receipt.accepted );
    CHECK( here.i_at( destination ).size() == 1 );

    const tripoint_bub_ms empty_destination = destination + tripoint::east;
    debug_menu::debug_item_spawn_request invalid_type = make_request( empty_destination, "r022-invalid" );
    invalid_type.type = itype_id( "r022_missing_item_type" );
    const debug_menu::debug_item_spawn_receipt invalid_type_receipt =
        debug_menu::debug_item_spawn_transaction( invalid_type );
    CHECK_FALSE( invalid_type_receipt.accepted );
    CHECK( here.i_at( empty_destination ).empty() );

    debug_menu::debug_item_spawn_request invalid_owner = make_request( empty_destination,
            "r022-invalid-owner" );
    invalid_owner.owner = faction_id( "r022_missing_faction" );
    const debug_menu::debug_item_spawn_receipt invalid_owner_receipt =
        debug_menu::debug_item_spawn_transaction( invalid_owner );
    CHECK_FALSE( invalid_owner_receipt.accepted );
    CHECK( here.i_at( empty_destination ).empty() );

    debug_menu::debug_item_spawn_request invalid_condition = make_request( empty_destination,
            "r022-invalid-condition" );
    invalid_condition.damage = 4001;
    const debug_menu::debug_item_spawn_receipt invalid_condition_receipt =
        debug_menu::debug_item_spawn_transaction( invalid_condition );
    CHECK_FALSE( invalid_condition_receipt.accepted );
    CHECK( here.i_at( empty_destination ).empty() );

}
