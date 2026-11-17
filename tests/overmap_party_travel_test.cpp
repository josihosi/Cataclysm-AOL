#include "overmap_party_travel.h"

#include <sstream>
#include <vector>

#include "cata_catch.h"
#include "json.h"
#include "json_loader.h"

TEST_CASE( "shared overmap party work supports six members and preserves paid progress",
           "[overmap_party_travel][coarse]" )
{
    overmap_party_travel::work state;
    std::vector<tripoint_abs_omt> from;
    std::vector<tripoint_abs_omt> to;
    for( int i = 0; i < 6; ++i ) {
        from.emplace_back( 10, 10 + i, 0 );
        to.emplace_back( 11, 10 + i, 0 );
    }
    bool moved = false;
    REQUIRE( overmap_party_travel::service_set( state, from, to, 72, 100, 300000, moved ) );
    CHECK_FALSE( moved );
    CHECK( state.work_millis == 300000 );
    std::ostringstream serialized;
    JsonOut json( serialized );
    state.serialize( json );
    overmap_party_travel::work loaded;
    loaded.deserialize( json_loader::from_string( serialized.str() ).get_object() );
    CHECK( loaded.from == from );
    CHECK( loaded.to == to );
    CHECK( loaded.work_millis == state.work_millis );
    REQUIRE( overmap_party_travel::service_set( loaded, from, to, 72, 100, 300000, moved ) );
    CHECK_FALSE( moved );
    REQUIRE( overmap_party_travel::service_set( loaded, from, to, 72, 100, 300000, moved ) );
    CHECK( moved );
    // A changed edge cannot inherit old unfinished work.
    REQUIRE( overmap_party_travel::service_set( loaded, from, to, 72, 100, 300000, moved ) );
    to.back() = from.back() + point::south;
    REQUIRE( overmap_party_travel::service_set( loaded, from, to, 72, 100, 300000, moved ) );
    CHECK_FALSE( moved );
    CHECK( loaded.work_millis == 300000 );
}

TEST_CASE( "shared route retries failed searches and revalidates retained edges",
           "[overmap_party_travel][route]" )
{
    using namespace overmap_party_travel;
    route_state state;
    const tripoint_abs_omt a( 1, 1, 0 );
    const tripoint_abs_omt b( 2, 1, 0 );
    const tripoint_abs_omt c( 3, 1, 0 );
    int searches = 0;
    bool available = false;
    bool edge_open = true;
    const route_finder find = [&]( const tripoint_abs_omt &from, const tripoint_abs_omt &to ) {
        ++searches;
        return available ? std::vector<tripoint_abs_omt>{ to, b, from } :
               std::vector<tripoint_abs_omt>{};
    };
    const edge_validator valid = [&]( const tripoint_abs_omt &, const tripoint_abs_omt & ) {
        return edge_open;
    };
    CHECK( next_route_step( state, a, c, 10, 5, 8, find, valid ).status == route_status::unavailable );
    for( int now = 11; now <= 15; ++now ) {
        CHECK( next_route_step( state, a, c, now, 5, 8, find, valid ).status == route_status::retry_pending );
    }
    CHECK( searches == 1 );
    available = true;
    CHECK( next_route_step( state, a, c, 16, 5, 8, find, valid ).next == b );
    CHECK( searches == 2 );
    CHECK( next_route_step( state, a, c, 17, 5, 8, find, valid ).next == b );
    CHECK( searches == 2 );
    edge_open = false;
    CHECK( next_route_step( state, a, c, 18, 5, 8, find, valid ).status == route_status::unavailable );
    CHECK( searches == 3 );
    state.clear(); // A caller-observed policy or terrain change invalidates a negative result.
    edge_open = true;
    CHECK( next_route_step( state, a, c, 19, 5, 8, find, valid ).next == b );
    CHECK( searches == 4 );
    CHECK( next_route_step( state, c, c, 20, 5, 8, find, valid ).status == route_status::reached );
}

TEST_CASE( "shared transit binds paid work to actor and rejects local handoff",
           "[overmap_party_travel][transit]" )
{
    using namespace overmap_party_travel;
    route_state route;
    work state;
    const tripoint_abs_omt origin( 20, 20, 0 );
    const tripoint_abs_omt goal( 21, 20, 0 );
    actor_input actor;
    actor.actor_id = "party:first";
    actor.operation_generation = 7;
    actor.lease_epoch = 3;
    actor.origin = origin;
    actor.destination = goal;
    actor.route = &route;
    actor.find = []( const tripoint_abs_omt &from, const tripoint_abs_omt &to ) {
        return std::vector<tripoint_abs_omt>{ to, from };
    };
    actor.valid = []( const tripoint_abs_omt &, const tripoint_abs_omt & ) { return true; };
    actor.terrain_weight = []( const tripoint_abs_omt & ) { return 36; };
    actor.gate = []( const tripoint_abs_omt & ) { return edge_gate::distant; };
    actor.destination_cost = 36;
    transit_policy policy;
    policy.now = 100;
    policy.retry_delay = 60;
    policy.retained_edges = 8;
    policy.slowest_speed = 100;
    policy.active_millis = 60000;
    policy.fixed_edge_cost_millis = 300000;
    REQUIRE( prepare_transit( state, { actor }, policy ).status == transit_status::waiting );
    REQUIRE( state.work_millis == 60000 );
    std::ostringstream bytes;
    JsonOut writer( bytes );
    state.serialize( writer );
    work restored;
    restored.deserialize( json_loader::from_string( bytes.str() ).get_object() );
    CHECK( restored.actor_ids == std::vector<std::string>{ "party:first" } );
    CHECK( restored.actor_epochs == std::vector<std::uint64_t>{ 3 } );
    CHECK( restored.operation_generation == 7 );
    actor.actor_id = "party:replacement";
    REQUIRE( prepare_transit( restored, { actor }, policy ).status == transit_status::waiting );
    CHECK( restored.work_millis == 60000 ); // No inherited prior credit.
    CHECK( restored.actor_ids == std::vector<std::string>{ "party:replacement" } );
    const auto before = restored.work_millis;
    actor.gate = []( const tripoint_abs_omt & ) { return edge_gate::local_handoff; };
    CHECK( prepare_transit( restored, { actor }, policy ).status == transit_status::local_handoff );
    CHECK( restored.work_millis == before );
}

TEST_CASE( "cached NPC edge gate can veto a valid shared reroute",
           "[overmap_party_travel][route][recovery]" )
{
    using namespace overmap_party_travel;
    const tripoint_abs_omt origin( 20, 20, 0 );
    const tripoint_abs_omt old_edge( 21, 20, 0 );
    const tripoint_abs_omt detour( 20, 21, 0 );
    const tripoint_abs_omt goal( 22, 20, 0 );
    route_state route;
    work state;
    bool old_open = true;
    int searches = 0;
    actor_input actor;
    actor.actor_id = "pair:1";
    actor.operation_generation = 1;
    actor.lease_epoch = 1;
    actor.origin = origin;
    actor.destination = goal;
    actor.route = &route;
    actor.find = [&]( const tripoint_abs_omt &from, const tripoint_abs_omt &to ) {
        ++searches;
        return std::vector<tripoint_abs_omt>{ to, old_open ? old_edge : detour, from };
    };
    actor.valid = [&]( const tripoint_abs_omt &, const tripoint_abs_omt &to ) {
        return to != old_edge || old_open;
    };
    actor.terrain_weight = []( const tripoint_abs_omt & ) { return 24; };
    actor.gate = [&]( const tripoint_abs_omt &edge ) {
        return edge == old_edge ? edge_gate::distant : edge_gate::blocked;
    };
    actor.destination_cost = 24;
    transit_policy policy;
    policy.now = 10;
    policy.retry_delay = 2;
    policy.retained_edges = 8;
    policy.slowest_speed = 100;
    policy.active_millis = 60000;
    policy.fixed_edge_cost_millis = 300000;
    REQUIRE( prepare_transit( state, { actor }, policy ).status == transit_status::waiting );
    REQUIRE( state.work_millis == 60000 );
    old_open = false;
    ++policy.now;
    CHECK( prepare_transit( state, { actor }, policy ).status == transit_status::unavailable );
    CHECK( searches == 2 );
    CHECK( route.path.back() == detour );
    CHECK( state.work_millis == 60000 );
    ++policy.now;
    CHECK( prepare_transit( state, { actor }, policy ).status == transit_status::unavailable );
    CHECK( searches == 2 );
    actor.gate = []( const tripoint_abs_omt & ) { return edge_gate::distant; };
    CHECK( prepare_transit( state, { actor }, policy ).status == transit_status::waiting );
    CHECK( state.to == std::vector<tripoint_abs_omt>{ detour } );
    CHECK( state.work_millis == 60000 );
}
