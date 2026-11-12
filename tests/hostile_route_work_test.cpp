#include <iostream>

#include "avatar.h"
#include "bandit_live_world.h"
#include "bandit_live_world_probe.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "do_turn.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "npc.h"
#include "overmapbuffer.h"
#include "player_helpers.h"
#include "type_id.h"

TEST_CASE( "hostile search route work across paid blocked turns and opening",
           "[bandit][hostile_route_work]" )
{
    clear_npcs();
    clear_map_without_vision();
    clear_avatar();
    set_time_to_day();
    const time_point old_turn = calendar::turn;
    auto old_state = overmap_buffer.global_state.bandit_live_world;
    on_out_of_scope restore( [&]() {
        overmap_buffer.global_state.bandit_live_world = old_state;
        calendar::turn = old_turn;
        clear_npcs();
    } );
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 5, 5, 0 ) );
    get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
    on_out_of_scope uncloak( []() {
        get_avatar().unset_mutation( trait_id( "DEBUG_CLOAK" ) );
    } );
    npc &actor = spawn_npc( point_bub_ms( 50, 50 ), "thug" );
    actor.set_attitude( NPCATT_KILL );
    actor.set_hunger( 0 );
    actor.set_thirst( 0 );
    actor.set_sleepiness( 0 );
    actor.set_mutation( trait_id( "NO_BASH" ) );
    const auto start = actor.pos_abs();
    for( int x = 49; x <= 51; ++x ) {
        for( int y = 49; y <= 51; ++y ) {
            if( x != 50 || y != 50 ) {
                here.ter_set( tripoint_bub_ms( x, y, 0 ), ter_str_id( "t_concrete_wall" ) );
            }
        }
    }
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0, true );
    bandit_live_world::site_record site;
    const bool cannibal = GENERATE( false, true );
    site.site_id = cannibal ? "route-cannibal" : "route-bandit";
    site.site_kind = cannibal ? bandit_live_world::owned_site_kind::cannibal_camp :
                     bandit_live_world::owned_site_kind::bandit_camp;
    site.profile = cannibal ? bandit_live_world::hostile_site_profile::cannibal_camp :
                   bandit_live_world::hostile_site_profile::camp_style;
    site.anchor = actor.pos_abs_omt() + point( -2, 0 );
    site.members.push_back( { actor.getID(), "thug", start,
                              bandit_live_world::member_state::local_contact, false, "" } );
    auto &operation = site.active_hostile_operation;
    operation.operation_kind = cannibal ? bandit_live_world::hostile_operation_kind::raid :
                               bandit_live_world::hostile_operation_kind::shakedown;
    if( !cannibal ) {
        operation.shakedown_pending_branch = "fight";
        site.last_shakedown_outcome = "fight_unresolved";
    }
    operation.phase = bandit_live_world::hostile_operation_phase::committed_contact;
    auto &outing = operation.reservation;
    outing.kind = bandit_live_world::outing_kind::hostile_operation;
    outing.activity_id = site.site_id + "#raid";
    outing.camp_id = site.site_id;
    outing.generation = 1;
    outing.owner = bandit_live_world::simulation_owner::local;
    outing.handoff_epoch = 1;
    outing.phase = bandit_live_world::scout_phase::observing;
    outing.job_type = cannibal ? "raid" : "toll";
    outing.target_omt = actor.pos_abs_omt();
    outing.member_ids = { actor.getID() };
    outing.leader_id = actor.getID();
    operation.sweep.begin( outing.target_omt, outing.generation );
    const auto goal = start + point( 5, 0 );
    operation.sweep.candidates = {{ goal }};
    overmap_buffer.global_state.bandit_live_world.sites = { site };
    auto &live = overmap_buffer.global_state.bandit_live_world.sites.front();
    REQUIRE( bandit_live_world::active_local_assault_site_for(
                 overmap_buffer.global_state.bandit_live_world, actor.getID() ) == &live );
    if( !cannibal ) {
        REQUIRE( bandit_live_world::hostile_shakedown_combat_is_ongoing( live ) );
    }
    using namespace bandit_live_world_probe;
    session probe( collection_mode::timings );
    const auto get = []( const snapshot & s, counter c ) {
        return s.counters[static_cast<std::size_t>( c )];
    };
    int paid = 0;
    for( int turn = 0; turn < 4; ++turn ) {
        calendar::turn += 1_turns;
        actor.set_moves( 1 - actor.get_speed() );
        refresh_live_bandit_activation();
        const auto before_callback = get( probe.result(), counter::hostile_search_route_solves );
        note_live_bandit_aftermath_for_test();
        const auto after_callback = get( probe.result(), counter::hostile_search_route_solves );
        process_monsters_and_npcs_turn_for_test();
        note_live_bandit_local_turn_sight_avoid_for_test();
        const auto after_motor = get( probe.result(), counter::hostile_search_route_solves );
        std::cout << "HOSTILE_TURN profile=" << site.site_id << " turn=" << turn + 1
                  << " callback_solves=" << after_callback - before_callback
                  << " motor_post_solves=" << after_motor - after_callback << '\n';
        if( !cannibal ) {
            CHECK( bandit_live_world::hostile_shakedown_combat_is_ongoing( live ) );
        }
        paid += actor.get_moves() < 1;
        CHECK( actor.pos_abs() == start );
    }
    const auto blocked = probe.result();
    const auto normal = get( blocked, counter::hostile_search_route_solves );
    const auto fire = get( blocked, counter::hostile_search_fire_comparisons );
    const auto relaxed = get( blocked, counter::hostile_search_relaxed_diagnostics );
    std::cout << "HOSTILE_BLOCKED profile=" << site.site_id << " paid=" << paid
              << " normal=" << normal << " fire=" << fire << " relaxed=" << relaxed
              << " normal_ns=" << blocked.sections[static_cast<std::size_t>
                      ( section::hostile_search_route_solve )].inclusive.total_ns
              << " fire_ns=" << blocked.sections[static_cast<std::size_t>
                      ( section::hostile_search_fire_comparison )].inclusive.total_ns << '\n';
    CHECK( paid == 4 );
    REQUIRE( normal > 0 );
    CHECK( relaxed == 0 );
    const int blocked_turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    const int retry_turn = live.active_hostile_operation.site_search_retry_after_turn;
    here.ter_set( tripoint_bub_ms( 51, 50, 0 ), ter_str_id( "t_floor" ) );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0, true );
    int open_turns = 0;
    while( actor.pos_abs() == start && open_turns < 65 ) {
        ++open_turns;
        calendar::turn += 1_turns;
        actor.set_moves( 1 - actor.get_speed() );
        refresh_live_bandit_activation();
        note_live_bandit_aftermath_for_test();
        process_monsters_and_npcs_turn_for_test();
        note_live_bandit_local_turn_sight_avoid_for_test();
    }
    const auto opened = probe.result();
    std::cout << "HOSTILE_OPEN profile=" << site.site_id
              << " paid=" << ( actor.get_moves() < 1 )
              << " moved=" << ( actor.pos_abs() != start )
              << " turns=" << open_turns
              << " retry_delay=" << retry_turn - blocked_turn
              << " normal_delta=" << get( opened, counter::hostile_search_route_solves ) - normal
              << " fire_delta=" << get( opened, counter::hostile_search_fire_comparisons ) - fire << '\n';
    CHECK( actor.get_moves() < 1 );
    CHECK( actor.pos_abs() != start );
    if( !cannibal ) {
        CHECK( retry_turn > blocked_turn );
        CHECK( open_turns == retry_turn - blocked_turn );
    }
}
