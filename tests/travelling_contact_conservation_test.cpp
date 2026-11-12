#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "avatar.h"
#include "bandit_live_world.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "character_id.h"
#include "coordinates.h"
#include "do_turn.h"
#include "game.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "npc.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "player_helpers.h"

TEST_CASE( "ordinary loaded road contact callbacks conserve the NPC action budget",
           "[bandit][live_world][travelling_contact][conservation]" )
{
    clear_npcs();
    clear_map_without_vision();
    clear_avatar();
    set_time_to_day();
    map &here = get_map();
    avatar &player = get_avatar();
    player.setpos( here, tripoint_bub_ms( 50, 50, 0 ) );
    const tripoint_abs_omt road = player.pos_abs_omt();
    const oter_id old_terrain = overmap_buffer.ter( road );
    const auto old_state = overmap_buffer.global_state.bandit_live_world;
    on_out_of_scope restore( [&]() {
        overmap_buffer.global_state.bandit_live_world = old_state;
        overmap_buffer.ter_set( road, old_terrain );
        clear_npcs();
    } );
    overmap_buffer.ter_set( road, oter_id( "road_nesw" ) );
    npc &first = spawn_npc( point_bub_ms( 53, 50 ), "thug" );
    npc &second = spawn_npc( point_bub_ms( 54, 50 ), "thug" );
    const std::vector<character_id> ids = { first.getID(), second.getID() };
    bandit_live_world::site_record site;
    site.site_id = "ordinary-road-contact";
    site.anchor = road + point( -2, 0 );
    site.footprint = { site.anchor };
    site.profile = bandit_live_world::hostile_site_profile::camp_style;
    site.site_kind = bandit_live_world::owned_site_kind::bandit_camp;
    site.living_total = 2;
    for( npc *member : { &first, &second } ) {
        site.members.push_back( { member->getID(), "thug", member->pos_abs(),
                                  bandit_live_world::member_state::local_contact, false, "" } );
        member->set_attitude( NPCATT_NULL );
        member->set_moves( 150 );
    }
    auto &outing = site.active_outing;
    outing.kind = bandit_live_world::outing_kind::scout_sortie;
    outing.activity_id = "ordinary-road-contact#scout";
    outing.camp_id = site.site_id;
    outing.generation = 1;
    outing.owner = bandit_live_world::simulation_owner::local;
    outing.handoff_epoch = 1;
    outing.phase = bandit_live_world::scout_phase::observing;
    outing.job_type = "scout";
    outing.member_ids = ids;
    outing.leader_id = ids.front();
    outing.target_omt = road;
    outing.target_id = "road-player";
    overmap_buffer.global_state.bandit_live_world.sites = { site };
    REQUIRE( g->find_npc( ids.front() ) == &first );
    REQUIRE( g->find_npc( ids.back() ) == &second );
    REQUIRE( first.sees( here, player ) );
    const int player_hp = player.get_hp();
    std::map<character_id, tripoint_abs_ms> positions;
    std::map<character_id, int> health;
    for( npc *member : { &first, &second } ) {
        positions.emplace( member->getID(), member->pos_abs() );
        health.emplace( member->getID(), member->get_hp() );
    }
    for( int repeat = 0; repeat < 2; ++repeat ) {
        note_live_bandit_aftermath_for_test();
        note_live_bandit_local_turn_sight_avoid_for_test();
        CHECK( player.get_hp() == player_hp );
        for( const character_id id : ids ) {
            const npc *member = g->find_npc( id );
            REQUIRE( member );
            CHECK( member->get_moves() == 150 );
            CHECK( member->pos_abs() == positions.at( id ) );
            CHECK( member->get_hp() == health.at( id ) );
        }
    }
    // This is an eligible rolling-road ambush, not a silent no-contact case.
    CHECK( ( first.get_attitude() == NPCATT_KILL || second.get_attitude() == NPCATT_KILL ) );

    // Reload through the native local owner, then replace one body with its
    // serialized same-ID copy. Contact callbacks must still only set intent.
    g->reload_npcs();
    npc *loaded = g->find_npc( ids.front() );
    REQUIRE( loaded );
    const tripoint_abs_ms loaded_position = loaded->pos_abs();
    const int loaded_hp = loaded->get_hp();
    loaded->on_unload();
    std::ostringstream body_bytes;
    JsonOut body_json( body_bytes );
    loaded->serialize( body_json );
    g->remove_npc( ids.front() );
    auto old_body = overmap_buffer.remove_npc( ids.front() );
    REQUIRE( old_body );
    auto replacement = make_shared_fast<npc>();
    replacement->deserialize( json_loader::from_string( body_bytes.str() ).get_object() );
    REQUIRE( replacement->getID() == ids.front() );
    REQUIRE( replacement.get() != old_body.get() );
    overmap_buffer.insert_npc( replacement );
    g->load_npcs();
    REQUIRE( g->find_npc( ids.front() ) == replacement.get() );
    REQUIRE( replacement->pos_abs() == loaded_position );
    replacement->set_moves( 150 );
    const int player_hp_after_reload = player.get_hp();
    for( int repeat = 0; repeat < 2; ++repeat ) {
        note_live_bandit_aftermath_for_test();
        note_live_bandit_local_turn_sight_avoid_for_test();
        CHECK( replacement->get_moves() == 150 );
        CHECK( replacement->pos_abs() == loaded_position );
        CHECK( replacement->get_hp() == loaded_hp );
        CHECK( player.get_hp() == player_hp_after_reload );
    }
    process_monsters_and_npcs_turn_for_test();
    CHECK( replacement->get_moves() < 150 );
    CHECK( ( replacement->pos_abs() != loaded_position || player.get_hp() < player_hp_after_reload ) );
}
