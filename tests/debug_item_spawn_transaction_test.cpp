#include <algorithm>

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
debug_menu::debug_item_spawn_request make_request( const tripoint_bub_ms &destination,
        const std::string &transaction_id )
{
    return { itype_test_apple, 3, 0, 0, get_avatar().get_faction()->id, destination, transaction_id };
}
} // namespace

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
