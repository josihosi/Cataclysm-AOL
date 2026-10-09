#include <algorithm>
#include <functional>
#include <list>
#include <sstream>
#include <vector>
#include <utility>
#include <string>

#include "cata_catch.h"
#include "coordinates.h"
#include "debug.h"
#include "item.h"
#include "item_contents.h"
#include "item_location.h"
#include "item_pocket.h"
#include "json.h"
#include "json_loader.h"
#include "visitable.h"
#include "itype.h"
#include "map.h"
#include "map_helpers.h"
#include "map_selector.h"
#include "pocket_type.h"
#include "point.h"
#include "ret_val.h"
#include "type_id.h"
#include "units.h"

static const itype_id itype_crowbar_pocket_test( "crowbar_pocket_test" );
static const itype_id itype_hammer_pocket_test( "hammer_pocket_test" );
static const itype_id itype_jar_glass_sealed( "jar_glass_sealed" );
static const itype_id itype_log( "log" );
static const itype_id itype_pickle( "pickle" );
static const itype_id itype_purse( "purse" );
static const itype_id itype_test_tool_belt( "test_tool_belt" );
static const itype_id itype_tongs_pocket_test( "tongs_pocket_test" );
static const itype_id itype_wrench_pocket_test( "wrench_pocket_test" );

TEST_CASE( "item_contents" )
{
    map &here = get_map();

    clear_map_without_vision();
    item tool_belt( itype_test_tool_belt );

    const units::volume tool_belt_vol = tool_belt.volume();
    const units::mass tool_belt_weight = tool_belt.weight();

    //check empty weight is consistent
    CHECK( tool_belt.weight( true ) == tool_belt.type->weight );
    CHECK( tool_belt.weight( false ) == tool_belt.type->weight );

    item hammer( itype_hammer_pocket_test );
    item tongs( itype_tongs_pocket_test );
    item wrench( itype_wrench_pocket_test );
    item crowbar( itype_crowbar_pocket_test );

    ret_val<void> i1 = tool_belt.put_in( hammer, pocket_type::CONTAINER );
    ret_val<void> i2 = tool_belt.put_in( tongs, pocket_type::CONTAINER );
    ret_val<void> i3 = tool_belt.put_in( wrench, pocket_type::CONTAINER );
    ret_val<void> i4 = tool_belt.put_in( crowbar, pocket_type::CONTAINER );

    {
        CAPTURE( i1.str() );
        CHECK( i1.success() );
    }
    {
        CAPTURE( i2.str() );
        CHECK( i2.success() );
    }
    {
        CAPTURE( i3.str() );
        CHECK( i3.success() );
    }
    {
        CAPTURE( i4.str() );
        CHECK( i4.success() );
    }

    // check the items actually got added to the tool belt
    REQUIRE( tool_belt.num_item_stacks() == 4 );
    // tool belts are non-rigid
    CHECK( tool_belt.volume() == tool_belt_vol +
           hammer.volume() + tongs.volume() + wrench.volume() + crowbar.volume() );
    // check that the tool belt's weight adds up all its contents properly
    CHECK( tool_belt.weight() == tool_belt_weight +
           hammer.weight() + tongs.weight() + wrench.weight() + crowbar.weight() );
    // check that individual (not including contained items) weight is correct
    CHECK( tool_belt.weight( false ) == tool_belt.type->weight );
    // check that the tool belt is "full"
    CHECK( !tool_belt.can_contain( crowbar ).success() );

    tool_belt.force_insert_item( crowbar, pocket_type::CONTAINER );
    CHECK( tool_belt.num_item_stacks() == 5 );
    tool_belt.force_insert_item( crowbar, pocket_type::CONTAINER );
    tool_belt.overflow( here, tripoint_bub_ms::zero );
    CHECK( tool_belt.num_item_stacks() == 4 );
    tool_belt.overflow( here, tripoint_bub_ms::zero );
    // overflow should only spill items if they can't fit
    CHECK( tool_belt.num_item_stacks() == 4 );

    tool_belt.remove_items_with( []( const item & it ) {
        return it.typeId() == itype_crowbar_pocket_test;
    } );
    // check to see that removing an item works
    CHECK( tool_belt.num_item_stacks() == 3 );
    tool_belt.spill_contents( tripoint_bub_ms::zero );
    CHECK( tool_belt.empty() );
}

TEST_CASE( "overflow_on_combine", "[item]" )
{
    clear_map_without_vision();
    tripoint_bub_ms origin{ 60, 60, 0 };
    item purse( itype_purse );
    item log( itype_log );
    item_contents overfull_contents( purse.type->pockets );
    overfull_contents.force_insert_item( log, pocket_type::CONTAINER );
    capture_debugmsg_during( [&purse, &overfull_contents]() {
        purse.combine( overfull_contents );
    } );
    map &here = get_map();
    here.i_clear( origin );
    purse.overflow( here, origin );
    CHECK( here.i_at( origin ).size() == 1 );
}

TEST_CASE( "overflow_test", "[item]" )
{
    clear_map_without_vision();
    tripoint_bub_ms origin{ 60, 60, 0 };
    item purse( itype_purse );
    item log( itype_log );
    purse.force_insert_item( log, pocket_type::MIGRATION );
    map &here = get_map();
    purse.overflow( here, origin );
    CHECK( here.i_at( origin ).size() == 1 );
}

TEST_CASE( "overflow_test_into_parent_item", "[item]" )
{
    map &here = get_map();

    clear_map_without_vision();
    tripoint_bub_ms origin{ 60, 60, 0 };
    item jar( itype_jar_glass_sealed );
    item pickle( itype_pickle );
    pickle.force_insert_item( pickle, pocket_type::MIGRATION );
    jar.put_in( pickle, pocket_type::CONTAINER );
    int contents_pre = 0;
    for( item *it : jar.all_items_top() ) {
        contents_pre += it->count();
    }
    REQUIRE( contents_pre == 1 );

    item_location jar_loc( map_cursor( origin ), &jar );
    jar_loc.overflow( here );
    CHECK( here.i_at( origin ).empty() );

    int contents_count = 0;
    for( item *it : jar.all_items_top() ) {
        contents_count += it->count();
    }
    CHECK( contents_count == 2 );
}

TEST_CASE( "native nested hydration preserves saved identity while copies and splits remain distinct",
           "[item_contents][item_identity_reload_067]" )
{
    item original( itype_id( "backpack" ) );
    item inner( itype_id( "box_small" ) );
    REQUIRE( inner.put_in( item( itype_id( "coin_gold" ) ), pocket_type::CONTAINER ).success() );
    REQUIRE( original.put_in( inner, pocket_type::CONTAINER ).success() );
    const auto identities = []( const item &root ) {
        std::vector<std::int64_t> result;
        root.visit_items( [&]( const item *it, const item * ) {
            result.push_back( it->uid().get_value() ); return VisitResponse::NEXT;
        } );
        return result;
    };
    const auto saved_ids = identities( original ); REQUIRE( saved_ids.size() == 3 );
    item copied = original;
    const auto copy_ids = identities( copied ); REQUIRE( copy_ids.size() == saved_ids.size() );
    for( size_t i = 0; i < saved_ids.size(); ++i ) { CHECK( saved_ids[i] != copy_ids[i] ); }
    CHECK( original.stacks_with( copied ) );
    for( int repeat = 0; repeat < 3; ++repeat ) {
        std::ostringstream stream; JsonOut out( stream ); original.serialize( out );
        item loaded; loaded.deserialize( json_loader::from_string( stream.str() ).get_object() );
        CHECK( identities( loaded ) == saved_ids );
        CHECK( loaded.stacks_with( original ) );
        original = std::move( loaded ); CHECK( identities( original ) == saved_ids );
    }
    item ammo( itype_id( "9mm" ), calendar::turn, 40 );
    REQUIRE( ammo.count_by_charges() );
    const auto ammo_uid = ammo.uid().get_value();
    item split = ammo.split( 10 );
    CHECK( ammo.uid().get_value() == ammo_uid ); CHECK( split.uid().get_value() != ammo_uid );
    CHECK( ammo.charges == 30 ); CHECK( split.charges == 10 );
    item ammo_copy = ammo; CHECK( ammo_copy.uid() != ammo.uid() );
    const auto split_uid = split.uid().get_value();
    item transferred = std::move( split ); CHECK( transferred.uid().get_value() == split_uid );
}

TEST_CASE( "hydration keeps mod attached and overflow property identities",
           "[item_contents][item_identity_reload_067]" )
{
    const std::string mode = GENERATE( std::string( "mod" ), std::string( "attached" ),
                                      std::string( "overflow" ) );
    CAPTURE( mode );
    item original( itype_id( mode == "mod" ? "debug_modular_m4_carbine" :
                            mode == "attached" ? "backpack" : "purse" ) );
    if( mode == "mod" ) {
        REQUIRE( original.put_in( item( itype_id( "shoulder_strap" ) ), pocket_type::MOD ).success() );
    } else if( mode == "attached" ) {
        const item pocket( itype_id( "purse" ) );
        original.get_contents().add_pocket( pocket );
        REQUIRE( original.get_contents().get_added_pockets().size() == 1 );
        CHECK( original.get_contents().get_added_pockets().front()->uid() != pocket.uid() );
        const auto pockets = original.get_contents().get_container_pockets();
        REQUIRE_FALSE( pockets.empty() );
        REQUIRE( pockets.back()->insert_item( item( itype_id( "coin_gold" ) ), true, false ).success() );
    } else {
        original.force_insert_item( item( itype_id( "log" ) ), pocket_type::CONTAINER );
    }
    const auto identities = []( const item &root ) {
        std::vector<std::pair<std::int64_t, std::string>> result;
        // Generic inventory visitation intentionally excludes MOD/MIGRATION.
        // Inspect every actual pocket for hydration identity, including those.
        result.emplace_back( root.uid().get_value(), root.typeId().str() );
        for( const item *it : root.all_items_ptr() ) {
            result.emplace_back( it->uid().get_value(), it->typeId().str() );
        }
        for( const item *pocket : root.get_contents().get_added_pockets() ) {
            result.emplace_back( pocket->uid().get_value(), pocket->typeId().str() );
        }
        // Native mod hydration precedes other pockets and may change traversal
        // order. Require the same physical UID/type association, not that order.
        std::sort( result.begin(), result.end() );
        return result;
    };
    const auto saved = identities( original );
    if( mode == "mod" ) {
        // This native gun also supplies its default magazine. Preserve all
        // identities, and establish the actual mod rather than a guessed count.
        REQUIRE( saved.size() >= 2 );
        const auto mods = original.mods();
        REQUIRE( std::any_of( mods.begin(), mods.end(), []( const item *it ) {
            return it->typeId() == itype_id( "shoulder_strap" );
        } ) );
    } else { REQUIRE( saved.size() == ( mode == "attached" ? 3 : 2 ) ); }
    for( int repeat = 0; repeat < 2; ++repeat ) {
        std::ostringstream stream; JsonOut out( stream ); original.serialize( out );
        item loaded;
        capture_debugmsg_during( [&]() {
            loaded.deserialize( json_loader::from_string( stream.str() ).get_object() );
        } );
        CHECK( identities( loaded ) == saved );
        if( mode == "overflow" ) { CHECK( loaded.get_contents().num_item_stacks() == 1 ); }
        original = std::move( loaded );
    }
    item purse( itype_id( "purse" ) ), too_large( itype_id( "log" ) );
    const auto source_uid = too_large.uid().get_value();
    CHECK_FALSE( purse.get_contents().insert_item( std::move( too_large ),
                 pocket_type::CONTAINER, false, false, false ).success() );
    CHECK( too_large.uid().get_value() == source_uid );
    CHECK( too_large.typeId() == itype_id( "log" ) );
    CHECK( purse.empty() );
}
