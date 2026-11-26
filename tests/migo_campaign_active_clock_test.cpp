#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "do_turn.h"
#include "game.h"
#include "horde_entity.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "mapbuffer.h"
#include "migo_population.h"
#include "migo_population_native.h"
#include "migo_tower_native.h"
#include "monster.h"
#include "omdata.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "player_helpers.h"
#include "rng.h"
#include "submap.h"
#include "worldfactory.h"

TEST_CASE( "seed campaign migo loaded unloaded frozen active clock", "[.][seed_campaign][migo_clock]" )
{
    using namespace migo_population;
    clear_map();
    const auto before_world = overmap_buffer.global_state.migo_populations;
    const auto before_time = calendar::turn;
    const auto before_player = get_avatar().pos_abs();
    calendar::turn += 1_days;
    map &here = get_map();
    const tripoint_abs_ms pod = here.get_abs( tripoint_bub_ms( 65, 65, 0 ) );
    const auto anchor = project_to<coords::omt>( pod );
    const oter_id before_terrain = overmap_buffer.ter( anchor );
    overmap_buffer.ter_set( anchor, oter_str_id( "mi-go_scout_tower_1_north" ) );
    const auto geometry = geometry_at( anchor );
    REQUIRE( geometry );
    const std::string id = geometry->id;
    on_out_of_scope restore( [&]() {
        std::vector<monster *> local;
        for( monster &body : g->all_monsters() ) {
            if( body.population_identity().site_id == id ) {
                local.push_back( &body );
            }
        }
        for( monster *body : local ) {
            g->remove_zombie( *body );
        }
        overmap &owner = overmap_buffer.get( project_to<coords::om>( anchor ).xy() );
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data && entry.second->monster_data->population_identity().site_id == id ) {
                owner.release_monster_payload( entry.first, entry.second->monster_data->population_identity().member_id );
                break;
            }
        }
        overmap_buffer.global_state.migo_populations = before_world;
        overmap_buffer.ter_set( anchor, before_terrain );
        calendar::turn = before_time;
        g->place_player_overmap( project_to<coords::omt>( before_player ), false );
        get_avatar().setpos( before_player, false );
    } );
    here.furn_set( here.get_bub( pod ), furn_str_id( "f_migo_brood_pod" ) );
    site_record seed;
    seed.id = id;
    seed.anchor = anchor;
    seed.tower = true;
    member_record member;
    member.token = { id, "campaign-original-1", 0, anchor };
    member.type = mtype_id( "mon_mi_go" );
    member.home = pod + point( 2, 0 );
    member.hp = 100;
    member.owner = representation::local;
    REQUIRE( seed.commit_seed( { member }, pod, "f_migo_brood_pod", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites = { seed };
    monster payload( member.type );
    payload.population_identity() = member.token;
    payload.set_hp( member.hp );
    const auto body_position = member.home;
    monster *body = g->place_critter_at( make_shared_fast<monster>( payload ), here.get_bub( body_position ) );
    REQUIRE( body );
    const auto owner = [&]() -> site_record & {
        return *overmap_buffer.global_state.migo_populations.find( id );
    };
    const auto sample = [&]( const std::string &phase, int step ) {
        const auto &site = owner();
        const auto *resident = site.find( member.token.member_id );
        REQUIRE( resident );
        std::cout << "MIGO_CLOCK phase=" << phase << " step=" << step
                  << " site=" << site.id << " source=" << pod << " anchor=" << anchor
                  << " world=" << to_turn<int>( calendar::turn )
                  << " active=" << site.activity.active
                  << " delta=" << site.activity.active_delta_turns
                  << " suspended=" << site.activity.suspended_turns
                  << " elapsed=" << site.production_elapsed
                  << " last_service=" << site.last_service_world
                  << " source_loaded=" << get_map().inbounds( pod )
                  << " source_not_retired=" << !site.source_destroyed
                  << " player=" << get_avatar().pos_abs_omt()
                  << " occupancy=" << site.occupancy_verified
                  << " roster=" << site.members.size()
                  << " member=" << resident->token.member_id
                  << " epoch=" << resident->token.epoch
                  << " owner=" << static_cast<int>( resident->owner ) << std::endl;
    };
    const auto relocate = [&]( const tripoint_abs_omt &destination ) {
        REQUIRE( g->place_player_overmap( destination, false ) );
        get_avatar().setpos( project_to<coords::ms>( destination ) + point( 12, 12 ), false );
        REQUIRE( get_avatar().pos_abs_omt() == destination );
    };
    service_loaded();
    sample( "loaded_adopt", 0 );
    for( int i = 1; i <= 3; ++i ) {
        calendar::turn += 1_turns;
        service_loaded();
        sample( "loaded", i );
    }
    const auto loaded_elapsed = owner().production_elapsed;
    REQUIRE( loaded_elapsed == 3 );
    relocate( anchor + point( 10, 0 ) );
    service_loaded();
    sample( "unloaded_adopt", 0 );
    REQUIRE_FALSE( get_map().inbounds( pod ) );
    for( int i = 1; i <= 3; ++i ) {
        calendar::turn += 1_turns;
        submap_load_counts counts;
        {
            scoped_submap_load_counter counter( counts );
            service_loaded();
        }
        sample( "active_unloaded", i );
        std::cout << "MIGO_COST step=" << i << " lookups=" << counts.lookup_calls
                  << " unserialize=" << counts.unserialize_calls
                  << " deserialize=" << counts.deserialize_calls << std::endl;
        REQUIRE( owner().activity.active );
        CHECK( owner().production_elapsed == loaded_elapsed + i );
    }
    const bool earned_cycle = std::getenv( "CAOL_MIGO_EARN_CYCLE" ) != nullptr;
    if( earned_cycle ) {
        const auto interval = to_turns<std::int64_t>( 7_days );
        int supplied = 0;
        while( owner().production_elapsed < interval && supplied < interval ) {
            calendar::turn += 1_turns;
            service_loaded();
            ++supplied;
        }
        REQUIRE( owner().production_elapsed == interval );
        REQUIRE( owner().activity.active );
        REQUIRE( owner().members.size() == 1 );
        CHECK( owner().birth_sequence == 0 );
        const auto rng_at_due = rng_get_engine();
        for( int repeat = 0; repeat < 5; ++repeat ) {
            calendar::turn += 1_turns;
            service_loaded();
        }
        CHECK( rng_get_engine() == rng_at_due );
        CHECK( owner().production_elapsed == interval );
        CHECK( owner().birth_sequence == 0 );
        std::cout << "MIGO_CYCLE due_rng_stable=1 repeated=5" << std::endl;
        std::cout << "MIGO_CYCLE earned=" << owner().production_elapsed
                  << " birth_sequence=" << owner().birth_sequence
                  << " world=" << to_turn<int>( calendar::turn ) << std::endl;
        if( std::getenv( "CAOL_MIGO_WORLD_RELOAD" ) ) {
            REQUIRE_FALSE( get_map().inbounds( pod ) );
            REQUIRE( owner().activity.active );
            REQUIRE( owner().source == pod );
            const auto resident = *owner().find( member.token.member_id );
            REQUIRE( resident.owner == representation::abstract );
            const auto original_body = [&]() -> const monster * {
                const monster *found = nullptr;
                overmap &native_map = overmap_buffer.get( project_to<coords::om>( anchor ).xy() );
                for( const auto &entry : native_map.horde_entities() ) {
                    if( entry.second->monster_data &&
                        entry.second->monster_data->population_identity().site_id == id &&
                        entry.second->monster_data->population_identity().member_id == member.token.member_id ) {
                        REQUIRE_FALSE( found );
                        found = entry.second->monster_data.get();
                    }
                }
                return found;
            };
            REQUIRE( original_body() );
            const auto original_position = original_body()->pos_abs();
            const int original_hp = original_body()->get_hp();
            const auto original_token = original_body()->population_identity();
            REQUIRE( owner().owns( original_token, representation::abstract ) );
            REQUIRE( world_generator->active_world );
            const std::string world_name = world_generator->active_world->world_name;
            REQUIRE( g->save() );
            const auto clock = owner().activity;
            const auto saved_world = calendar::turn;
            const auto saved_service = owner().last_service_world;
            REQUIRE( turn_handler::cleanup_at_end() );
            world_generator->set_active_world( nullptr );
            REQUIRE( g->load( world_name ) );
            REQUIRE_FALSE( get_map().inbounds( pod ) );
            REQUIRE( calendar::turn == saved_world );
            REQUIRE( owner().source == pod );
            REQUIRE( owner().source_furniture == "f_migo_brood_pod" );
            REQUIRE_FALSE( owner().source_destroyed );
            REQUIRE( owner().production_elapsed == interval );
            REQUIRE( owner().birth_sequence == 0 );
            REQUIRE( owner().members.size() == 1 );
            REQUIRE( owner().activity.active );
            CHECK( owner().activity.adopted_world_turn == clock.adopted_world_turn );
            CHECK( owner().activity.last_world_turn == clock.last_world_turn );
            CHECK( owner().activity.suspended_turns == clock.suspended_turns );
            CHECK( owner().last_service_world == saved_service );
            const auto *after = owner().find( member.token.member_id );
            REQUIRE( after );
            CHECK( after->token.site_id == resident.token.site_id );
            CHECK( after->token.member_id == resident.token.member_id );
            CHECK( after->token.epoch == resident.token.epoch );
            CHECK( after->owner == resident.owner );
            REQUIRE( original_body() );
            CHECK( original_body()->population_identity().epoch == original_token.epoch );
            CHECK( original_body()->pos_abs() == original_position );
            CHECK( original_body()->get_hp() == original_hp );
            REQUIRE( owner().owns( original_body()->population_identity(), representation::abstract ) );
            const auto rng_after_reload = rng_get_engine();
            service_loaded();
            service_loaded();
            CHECK( owner().production_elapsed == interval );
            CHECK( owner().birth_sequence == 0 );
            CHECK( owner().members.size() == 1 );
            CHECK( rng_get_engine() == rng_after_reload );
            std::cout << "MIGO_RELOAD pending=604800 source=" << pod << " resident="
                      << resident.token.member_id << " epoch=" << resident.token.epoch
                      << " active=" << owner().activity.active << " world=" << to_turn<int>( calendar::turn )
                      << " last_service=" << owner().last_service_world << " body=" << original_position
                      << " hp=" << original_hp << " repeat_no_birth=1" << std::endl;
        }
    }
    const auto before_frozen = owner().production_elapsed;
    relocate( anchor + point( 50, 0 ) );
    service_loaded();
    sample( "frozen_adopt", 0 );
    for( int i = 1; i <= 3; ++i ) {
        calendar::turn += 1_turns;
        service_loaded();
        sample( "frozen", i );
        REQUIRE_FALSE( owner().activity.active );
        CHECK( owner().production_elapsed == before_frozen );
    }
    relocate( anchor );
    service_loaded();
    sample( "return_adopt", 0 );
    REQUIRE( get_map().inbounds( pod ) );
    CHECK( get_map().furn( get_map().get_bub( pod ) ) == furn_str_id( "f_migo_brood_pod" ) );
    CHECK( owner().source == pod );
    CHECK( owner().members.size() == 1 );
    CHECK( owner().birth_sequence == 0 );
    CHECK( owner().production_elapsed == before_frozen );
    calendar::turn += 1_turns;
    service_loaded();
    sample( "loaded_again", 1 );
    if( earned_cycle ) {
        REQUIRE( owner().birth_sequence == 1 );
        REQUIRE( owner().members.size() == 2 );
        CHECK( owner().production_elapsed == 0 );
        const auto born = owner().members.back().token;
        const auto count_birth_bodies = [&]() {
            int count = 0;
            for( const monster &candidate : g->all_monsters() ) {
                if( candidate.population_identity().site_id == born.site_id &&
                    candidate.population_identity().member_id == born.member_id ) {
                    ++count;
                    CHECK( candidate.population_identity().epoch == born.epoch );
                }
            }
            overmap &native_map = overmap_buffer.get( project_to<coords::om>( anchor ).xy() );
            for( const auto &entry : native_map.horde_entities() ) {
                if( entry.second->monster_data &&
                    entry.second->monster_data->population_identity().site_id == born.site_id &&
                    entry.second->monster_data->population_identity().member_id == born.member_id ) {
                    ++count;
                    CHECK( entry.second->monster_data->population_identity().epoch == born.epoch );
                }
            }
            return count;
        };
        const int native_bodies = count_birth_bodies();
        REQUIRE( native_bodies == 1 );
        service_loaded();
        calendar::turn += 1_turns;
        service_loaded();
        CHECK( owner().birth_sequence == 1 );
        CHECK( owner().members.size() == 2 );
        CHECK( count_birth_bodies() == 1 );
        std::cout << "MIGO_CYCLE placed=" << owner().birth_sequence
                  << " roster=" << owner().members.size() << " native_bodies=" << native_bodies
                  << " new_member=" << born.member_id << " epoch=" << born.epoch << std::endl;
    } else {
        CHECK( owner().production_elapsed == before_frozen + 1 );
    }
}
