#include "cata_catch.h"

#include "bandit_live_world.h"
#include "calendar.h"
#include "do_turn.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "mapbuffer.h"
#include "npc.h"
#include "overmapbuffer.h"
#include "physical_navigation.h"
#include "player_helpers.h"
#include "cata_scope_helpers.h"
#include "type_id.h"

TEST_CASE( "native hostile boundary step respects blockage and charges movement",
           "[overmap_party_travel][hostile_boundary]" )
{
    clear_npcs();
    clear_map();
    on_out_of_scope cleanup( []() { clear_npcs(); } );
    map &here = get_map();
    npc &traveler = spawn_npc( point_bub_ms( 60, 60 ), "test_talker" );
    const tripoint_abs_ms origin = traveler.pos_abs();
    const tripoint_abs_ms destination = origin + point::east;
    const tripoint_bub_ms tile = here.get_bub( destination );
    here.ter_set( tile, ter_str_id( "t_wall" ) );
    traveler.set_moves( 100 );
    CHECK( physical_navigation::step_npc_boundary( traveler, destination ) ==
           physical_navigation::boundary_action::refused );
    CHECK( traveler.pos_abs() == origin );
    CHECK( traveler.get_moves() == 100 );

    here.ter_set( tile, ter_str_id( "t_grass" ) );
    npc &blocker = spawn_npc( tile.xy(), "test_talker" );
    const character_id blocker_id = blocker.getID();
    CHECK( physical_navigation::step_npc_boundary( traveler, destination ) ==
           physical_navigation::boundary_action::refused );
    CHECK( traveler.pos_abs() == origin );
    CHECK( traveler.get_moves() == 100 );
    g->remove_npc( blocker_id );
    overmap_buffer.remove_npc( blocker_id );

    CHECK( physical_navigation::step_npc_boundary( traveler, destination ) ==
           physical_navigation::boundary_action::moved );
    CHECK( traveler.pos_abs() == destination );
    CHECK( traveler.get_moves() < 100 );
}

TEST_CASE( "hostile boundary retries a changed physical edge without repeated reads",
           "[overmap_party_travel][hostile_boundary]" )
{
    clear_npcs();
    clear_map();
    const auto previous_world = overmap_buffer.global_state.bandit_live_world;
    const auto previous_turn = calendar::turn;
    on_out_of_scope cleanup( [&]() {
        overmap_buffer.global_state.bandit_live_world = previous_world;
        calendar::turn = previous_turn;
        clear_npcs();
    } );
    map &here = get_map();
    npc &traveler = spawn_npc( point_bub_ms( 60, 60 ), "test_talker" );
    const auto original = traveler.pos_abs();
    const tripoint_bub_ms edge( here.getmapsize() * SEEX - 1, 60, traveler.posz() );
    traveler.spawn_at_precise( here.get_abs( edge ) );
    const auto origin = traveler.pos_abs();
    const auto destination = origin + point::east;
    const auto origin_omt = project_to<coords::omt>( origin );
    const auto next_omt = origin_omt + point::east;
    const auto goal = origin_omt + point( 3, 0 );
    traveler.goal = goal;
    traveler.omt_path = { goal, next_omt, origin_omt };
    traveler.set_mission( NPC_MISSION_TRAVELLING );
    traveler.set_moves( 100 );
    bandit_live_world::site_record site;
    site.site_id = "hostile-boundary-test";
    site.anchor = origin_omt + point( -2, 0 );
    bandit_live_world::member_record member;
    member.npc_id = traveler.getID();
    member.state = bandit_live_world::member_state::outbound;
    site.members.push_back( member );
    auto &operation = site.active_hostile_operation;
    operation.operation_kind = bandit_live_world::hostile_operation_kind::raid;
    operation.phase = bandit_live_world::hostile_operation_phase::approaching;
    auto &outing = operation.reservation;
    outing.kind = bandit_live_world::outing_kind::hostile_operation;
    outing.activity_id = "hostile-boundary-test:1";
    outing.camp_id = site.site_id;
    outing.generation = 1;
    outing.member_ids = { traveler.getID() };
    outing.shared_route = { site.anchor, goal };
    outing.phase = bandit_live_world::scout_phase::outbound;
    outing.owner = bandit_live_world::simulation_owner::abstract;
    outing.last_advanced_minutes = 0;
    overmap_buffer.global_state.bandit_live_world.sites = { site };
    REQUIRE( bandit_live_world::active_operation_duty_site_for(
                 overmap_buffer.global_state.bandit_live_world, traveler.getID() ) );

    physical_navigation::step_read request;
    request.position = origin;
    request.missing_geometry = destination;
    if( !MAPBUFFER.lookup_submap_existing( project_to<coords::sm>( destination ) ) ) {
        REQUIRE( physical_navigation::acquire_movement_geometry( origin, request ) );
    }
    const auto tile_omt = project_to<coords::omt>( destination );
    const auto tile_local = project_remain<coords::omt>( destination ).remainder_tripoint;
    tinymap outside;
    outside.load( tile_omt, false );
    const auto old_terrain = outside.ter( tile_local );
    on_out_of_scope restore_tile( [&]() {
        outside.ter_set( tile_local, old_terrain );
        traveler.spawn_at_precise( original );
    } );
    outside.ter_set( tile_local, ter_str_id( "t_wall" ) );
    submap_load_counts counts;
    scoped_submap_load_counter counted( counts );
    CHECK_FALSE( live_bandit_hostile_boundary_step( traveler, here ) );
    REQUIRE( counts.lookup_calls > 0 );
    const auto first_reads = counts.lookup_calls;
    for( int tick = 0; tick < 2; ++tick ) {
        calendar::turn += 1_turns;
        CHECK_FALSE( live_bandit_hostile_boundary_step( traveler, here ) );
    }
    CHECK( counts.lookup_calls == first_reads );
    CHECK( traveler.pos_abs() == origin );
    CHECK( traveler.get_moves() == 100 );
    outside.ter_set( tile_local, ter_str_id( "t_grass" ) );
    calendar::turn += 11_turns;
    CHECK( live_bandit_hostile_boundary_step( traveler, here ) );
    CHECK( counts.lookup_calls > first_reads );
    CHECK( traveler.pos_abs() == destination );
    CHECK( traveler.get_moves() < 100 );
}

TEST_CASE( "incoming native movement stops when ownership changes between tiles",
           "[overmap_party_travel][hostile_boundary][incoming_ownership]" )
{
    const bool invalidate_on_entry = GENERATE( false, true );
    clear_npcs();
    clear_map();
    on_out_of_scope cleanup( []() { clear_npcs(); } );
    map &here = get_map();
    npc &traveler = spawn_npc( point_bub_ms( 60, 60 ), "test_talker" );
    const character_id id = traveler.getID();
    on_out_of_scope remove_body( [&]() {
        if( overmap_buffer.find_npc( id ) ) {
            g->remove_npc( id );
            overmap_buffer.remove_npc( id );
        }
    } );
    const auto body = overmap_buffer.find_npc( id );
    REQUIRE( body );
    g->remove_npc( id );
    int boundary_y = 24;
    while( project_to<coords::omt>( here.get_abs( tripoint_bub_ms( 0, boundary_y - 1, traveler.posz() ) ) ) !=
           project_to<coords::omt>( here.get_abs( tripoint_bub_ms( 0, boundary_y + 1, traveler.posz() ) ) ) ) {
        ++boundary_y;
    }
    const auto edge = here.get_abs( tripoint_bub_ms( 0, boundary_y, traveler.posz() ) );
    const auto outside = edge + point::west * ( invalidate_on_entry ? 1 : 3 );
    physical_navigation::step_read needed;
    needed.position = edge;
    needed.missing_geometry = edge + point::west;
    if( !MAPBUFFER.lookup_submap_existing( project_to<coords::sm>( *needed.missing_geometry ) ) ) {
        REQUIRE( physical_navigation::acquire_movement_geometry( edge, needed ) );
    }
    tinymap remote;
    remote.load( project_to<coords::omt>( outside ), false );
    struct original_tile {
        tripoint_omt_ms position;
        ter_id terrain;
        furn_id furniture;
    };
    std::vector<original_tile> original;
    for( int x = -3; x <= -1; ++x ) {
        for( int y = -1; y <= 1; ++y ) {
            const auto absolute = edge + point( x, y );
            REQUIRE( project_to<coords::omt>( absolute ) == project_to<coords::omt>( outside ) );
            const auto local = project_remain<coords::omt>( absolute ).remainder_tripoint;
            original.push_back( { local, remote.ter( local ), remote.furn( local ) } );
            remote.ter_set( local, ter_str_id( "t_grass" ) );
            remote.furn_set( local, furn_str_id::NULL_ID() );
        }
    }
    on_out_of_scope restore_tiles( [&]() {
        for( const auto &tile : original ) {
            remote.ter_set( tile.position, tile.terrain );
            remote.furn_set( tile.position, tile.furniture );
        }
    } );
    traveler.travel_overmap( outside );
    REQUIRE_FALSE( traveler.is_active() );
    int checks = 0;
    const auto result = physical_navigation::move_npc_into_loaded_area(
                            traveler, here, project_to<coords::omt>( edge ), 10000,
    [&]() {
        ++checks;
        return invalidate_on_entry ? !here.inbounds( traveler.pos_abs() ) : checks <= 2;
    } );
    CHECK( result.interrupted );
    CHECK( result.moved );
    CHECK( result.entered == invalidate_on_entry );
    CHECK( here.inbounds( traveler.pos_abs() ) == invalidate_on_entry );
    CHECK( rl_dist( traveler.pos_abs(), outside ) == 1 );
    CHECK( checks >= 3 );
}
