#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <clocale>
#include <cstdlib>
#include <map>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "active_time.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_path.h"
#include "path_info.h"
#include "pathfinding.h"
#include "string_formatter.h"
#include "worldfactory.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "creature_tracker.h"
#include "do_turn.h"
#include "debug.h"
#include "field_type.h"
#include "game.h"
#include "horde_entity.h"
#include "json.h"
#include "json_loader.h"
#include "live_light.h"
#include "line.h"
#include "level_cache.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "mapbuffer.h"
#include "migo_population_native.h"
#include "migo_tower_ai.h"
#include "migo_tower_capability.h"
#include "scout_observation.h"
#include "mtype.h"
#include "item.h"
#include "item_group.h"
#include "migo_tower_native.h"
#include "monster.h"
#include "npc.h"
#include "omdata.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "player_helpers.h"
#include "physical_navigation.h"
#include "physical_light.h"
#include "rng.h"
#include "imgui/imgui.h"
#include "ui_manager.h"
#include "output.h"
#include "options_helpers.h"
#include "input.h"
#include "cached_options.h"
#include "cursesdef.h"
#include "submap.h"

TEST_CASE( "shared active arithmetic preserves separate source and operation clocks",
           "[migo_tower][raid_sweep][active_movement_budget][serialization]" )
{
    raid_sweep::progress sweep;
    sweep.begin( tripoint_abs_omt( 8, 2, 0 ), 1 );
    raid_sweep::source_memory source;
    migo_tower::site_state tower;
    tower.site_id = "separate-clock";
    tower.anchor = tripoint_abs_omt( 8, 2, 0 );
    migo_tower::site_state frozen_home = tower;
    const auto advance = [&]( const int world, const int active ) {
        sweep.advance( world, active );
        source.advance( world, active );
        tower.advance( world, active );
        frozen_home.advance( world, 0 );
    };
    advance( 100, 500 ); // Old/absent anchors adopt with no invented backlog.
    CHECK( sweep.active_elapsed == 0 );
    CHECK( source.active_elapsed == 0 );
    CHECK( tower.active_elapsed == 0 );
    advance( 105, 3 );
    advance( 105, 500 ); // Same callback cannot mint another active interval.
    advance( 104, 500 );
    advance( 10000, 0 );
    CHECK( sweep.active_elapsed == 3 );
    CHECK( source.active_elapsed == 3 );
    CHECK( tower.active_elapsed == 3 );
    CHECK( frozen_home.active_elapsed == 0 );
    advance( 10001, 500 ); // Credit is bounded by this actual one-turn interval.
    advance( 10002, -5 );
    CHECK( sweep.active_elapsed == 4 );
    CHECK( source.active_elapsed == 4 );
    CHECK( tower.active_elapsed == 4 );
    const auto roundtrip = []( const auto & value ) {
        std::ostringstream out;
        {
            JsonOut json( out );
            value.serialize( json );
        }
        auto restored = value;
        restored.deserialize( json_loader::from_string( out.str() ) );
        return restored;
    };
    auto loaded_sweep = roundtrip( sweep );
    auto loaded_source = roundtrip( source );
    auto loaded_tower = roundtrip( tower );
    CHECK( sweep.last_world == 10002 );
    CHECK( source.last_world == 10002 );
    CHECK( tower.last_service_world == 10002 );
    CHECK( sweep.active_elapsed == 4 ); // Serialization itself spends nothing.
    for( int replay = 0; replay < 2; ++replay ) {
        loaded_sweep.advance( 10003, 1 );
        loaded_source.advance( 10003, 1 );
        loaded_tower.advance( 10003, 1 );
    }
    CHECK( loaded_sweep.active_elapsed == 5 );
    CHECK( loaded_source.active_elapsed == 5 );
    CHECK( loaded_tower.active_elapsed == 5 );
    CHECK( frozen_home.active_elapsed == 0 );
    frozen_home.advance( 10003, 1 );
    CHECK( frozen_home.active_elapsed == 1 );
}

TEST_CASE( "shared native movement funding retains species speed debt and adapter policy",
           "[migo_tower][active_movement_budget]" )
{
    npc human;
    human.normalize();
    human.set_speed_base( 83 );
    human.set_speed_bonus( 0 );
    monster migo( mtype_id( "mon_mi_go" ) );
    const int human_speed = human.get_speed();
    const int migo_speed = migo.get_speed();
    REQUIRE( human_speed > 0 );
    REQUIRE( migo_speed > 0 );
    REQUIRE( human_speed != migo_speed );
    for( const int speed : { human_speed, migo_speed } ) {
        const int debt = -150;
        int each_turn = debt;
        for( int tick = 0; tick < 60; ++tick ) {
            each_turn = active_time::funded_moves( each_turn, speed, 1 );
        }
        CHECK( active_time::funded_moves( debt, speed, 60 ) == each_turn );
        CHECK( each_turn == debt + 60 * speed );
        CHECK( active_time::funded_moves( debt, speed, 0 ) == debt );
        CHECK( active_time::funded_moves( debt, speed, -1 ) == debt - speed );
        CHECK( active_time::funded_moves( 13, speed, 0 ) == 13 );
        CHECK( active_time::funded_moves( std::numeric_limits<int>::max() - 1, speed, 1 ) ==
               std::numeric_limits<int>::max() );
        CHECK( active_time::funded_moves( std::numeric_limits<int>::min(), speed, 1 ) ==
               std::numeric_limits<int>::min() + speed );
    }
    // These are still adapter inputs, not a new clock or speed normalization.
    CHECK( active_time::funded_moves( 0, human_speed, 60 ) !=
           active_time::funded_moves( 0, migo_speed, 60 ) );
}

TEST_CASE( "migo_signal_location_and_observer_persist_without_actor_knowledge",
           "[migo_tower][serialization]" )
{
    migo_tower::world_state world;
    world.last_signal_scan = 450;
    migo_tower::site_state site;
    site.site_id = "ordinary-site";
    site.anchor = tripoint_abs_omt( 8, 2, 0 );
    site.last_sense_turn = 450;
    site.signal = migo_tower::signal_record{ "physical-sample", "smoke",
        tripoint_abs_omt( 2, 2, 0 ), std::nullopt,
        "real-member", tripoint_abs_ms( 200, 60, 3 ), 450 };
    migo_tower::operation_record operation;
    operation.generation = 9;
    operation.source = *site.signal;
    operation.members = { {"real-member"}, {"second-member", true, true} };
    operation.communicated_losses = {"witnessed-member"};
    operation.advance( 450, 600 );
    operation.advance( 451, 1 );
    operation.advance( 451, 600 );
    site.operation = operation;
    site.next_generation = 10;
    world.sites.push_back( site );
    std::ostringstream output;
    {
        JsonOut json( output );
        world.serialize( json );
    }
    migo_tower::world_state loaded;
    loaded.deserialize( json_loader::from_string( output.str() ) );
    REQUIRE( loaded.find( site.site_id ) );
    const auto &signal = loaded.find( site.site_id )->signal;
    REQUIRE( signal );
    CHECK_FALSE( signal->source_ms );
    CHECK( signal->source_omt == site.signal->source_omt );
    CHECK( signal->observer_member == "real-member" );
    CHECK( signal->observer_position == site.signal->observer_position );
    CHECK( signal->observed_turn == 450 );
    CHECK( loaded.last_signal_scan == 450 );
    REQUIRE( loaded.find( site.site_id )->operation );
    const auto &restored_operation = *loaded.find( site.site_id )->operation;
    CHECK( restored_operation.generation == 9 );
    CHECK( restored_operation.contains( "real-member" ) );
    CHECK_FALSE( restored_operation.contains( "unselected-member" ) );
    CHECK( restored_operation.members[1].settled );
    CHECK( restored_operation.members[1].returned );
    CHECK( restored_operation.sweep.active_elapsed == 1 );
    CHECK( restored_operation.source.observed_turn == 450 );
    CHECK( restored_operation.communicated_losses == std::vector<std::string>{"witnessed-member"} );
    CHECK( loaded.find( site.site_id )->next_generation == 10 );
    CHECK( loaded.find( site.site_id )->last_sense_turn == 450 );
    loaded.deserialize( json_loader::from_string( "{}" ) );
    CHECK( loaded.sites.empty() );
    CHECK( loaded.last_signal_scan == -1 );
    overmap_global_state new_world;
    new_world.migo_towers = world;
    new_world.clear();
    CHECK( new_world.migo_towers.sites.empty() );
    CHECK( new_world.migo_towers.last_signal_scan == -1 );
}

TEST_CASE( "migo_real_signal_receiver_and_map_spawn_share_saved_residents",
           "[migo_tower][migo_population][physical_light][native][raid_sweep]" )
{
    const bool load_first = GENERATE( false, true );
    clear_avatar();
    clear_map_without_vision();
    const auto old_turn = calendar::turn;
    on_out_of_scope restore_time( [&]() {
        set_time( old_turn );
    } );
    set_time( calendar::turn_zero + 1200_turns );
    reset_live_light_sample_cache();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_bandits = overmap_buffer.global_state.bandit_live_world;
    std::set<character_id> old_bandit_ids;
    for( const auto &site : old_bandits.sites ) {
        for( const auto &member : site.members ) {
            old_bandit_ids.insert( member.npc_id );
        }
    }
    map &here = get_map();
    const auto old_origin = here.get_abs_sub();
    const auto source_bub = get_avatar().pos_bub();
    const auto source = get_avatar().pos_abs();
    const auto source_omt = project_to<coords::omt>( source );
    // This is the new-content manifest control. A retagged existing forest
    // column is a legacy site and must not acquire fresh biological slots.
    // Find a genuinely unused nearby column through native existing-only
    // reads; never replace saved maps or override that production safeguard.
    std::optional<tripoint_abs_omt> fresh_anchor;
    for( const int dy : {0, -1, 1, -2, 2} ) {
        const tripoint_abs_omt candidate( source_omt.xy() + point( 4, dy ), 0 );
        if( std::any_of( old_populations.sites.begin(), old_populations.sites.end(),
        [&]( const auto &site ) { return site.anchor.xy() == candidate.xy(); } ) ) {
            continue;
        }
        bool existing = false;
        for( int z = 0; z < 4; ++z ) {
            for( int x = 0; x < 2; ++x ) {
                for( int y = 0; y < 2; ++y ) {
                    existing |= MAPBUFFER.lookup_submap_existing( project_to<coords::sm>(
                                    tripoint_abs_omt( candidate.xy(), z ) ) + point( x, y ) ) != nullptr;
                }
            }
        }
        if( !existing ) {
            fresh_anchor = candidate;
            break;
        }
    }
    REQUIRE( fresh_anchor );
    const tripoint_abs_omt anchor = *fresh_anchor;
    REQUIRE_FALSE( here.inbounds( project_to<coords::ms>( anchor ) ) );
    std::vector<oter_id> old_terrain;
    std::set<character_id> old_npcs;
    for( int z = 0; z < 4; ++z ) {
        const tripoint_abs_omt p( anchor.xy(), z );
        old_terrain.push_back( overmap_buffer.ter( p ) );
        for( const auto &n : overmap_buffer.get_npcs_near_omt( p, 0 ) ) {
            old_npcs.insert( n->getID() );
        }
        overmap_buffer.ter_set( p, oter_str_id( "mi-go_scout_tower_" +
                                                std::to_string( z + 1 ) + "_north" ) );
    }
    const auto geometry = migo_population::geometry_at( anchor );
    on_out_of_scope restore( [&]() {
        auto &om = overmap_buffer.get( project_to<coords::om>( anchor ).xy() );
        std::vector<std::pair<tripoint_abs_ms, std::string>> residents;
        for( const auto &entry : om.horde_entities() ) {
            if( geometry && entry.second->monster_data &&
                entry.second->monster_data->population_identity().site_id == geometry->id ) {
                residents.emplace_back( entry.first,
                                        entry.second->monster_data->population_identity().member_id );
            }
        }
        for( const auto &entry : residents ) {
            CHECK( om.release_monster_payload( entry.first, entry.second ) );
        }
        clear_creatures();
        for( const auto &site : overmap_buffer.global_state.bandit_live_world.sites ) {
            for( const auto &member : site.members ) {
                if( old_bandit_ids.count( member.npc_id ) == 0 ) {
                    overmap_buffer.remove_npc( member.npc_id );
                }
            }
        }
        for( int z = 0; z < 4; ++z ) {
            const tripoint_abs_omt p( anchor.xy(), z );
            for( const auto &n : overmap_buffer.get_npcs_near_omt( p, 0 ) ) {
                if( old_npcs.count( n->getID() ) == 0 ) {
                    overmap_buffer.remove_npc( n->getID() );
                }
            }
            overmap_buffer.ter_set( p, old_terrain[z] );
        }
        here.remove_field( here.get_bub( source ), field_type_id( "fd_smoke" ) );
        here.load( old_origin, false );
        get_avatar().setpos( here, here.get_bub( source ) );
        MAPBUFFER.clear_outside_reality_bubble();
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.global_state.bandit_live_world = old_bandits;
        reset_live_light_sample_cache();
    } );
    // Real mapgen prepares the saved manifest, without entering the tower or
    // calling discover. Both receivers must consume these exact native slots.
    REQUIRE( geometry );
    overmap_buffer.global_state.migo_towers.clear();
    tinymap prepared;
    prepared.load( anchor, false );
    std::set<std::string> saved_ids;
    int native_spawn_slots = 0;
    for( int z = 0; z < 4; ++z ) {
        for( int x = 0; x < 2; ++x ) {
            for( int y = 0; y < 2; ++y ) {
                submap *sm = MAPBUFFER.lookup_submap_existing( project_to<coords::sm>(
                        tripoint_abs_omt( anchor.xy(), z ) ) + point( x, y ) );
                REQUIRE( sm );
                for( const auto &slot : sm->spawns ) {
                    ++native_spawn_slots;
                    if( slot.population.site_id == geometry->id ) {
                        CHECK( saved_ids.insert( slot.population.member_id ).second );
                    }
                }
            }
        }
    }
    const auto *seed_state = overmap_buffer.global_state.migo_populations.find( geometry->id );
    CAPTURE( anchor, source_omt, geometry->id, native_spawn_slots );
    INFO( "existing native seed committed=" << ( seed_state && seed_state->seed_committed ) );
    INFO( "post-load new_content=" << ( seed_state && seed_state->new_content ) );
    REQUIRE( saved_ids.size() == 8 );
    auto population = [&]() {
        return overmap_buffer.global_state.migo_populations.find( geometry->id );
    };
    REQUIRE( population() );
    REQUIRE_FALSE( population()->seed_committed );
    here.add_field( source_bub, field_type_id( "fd_smoke" ), 3 );
    const auto enter_tower = [&]() {
        // Keep the same source inside the new bubble while the actual main
        // map enters the tower. tinymap deliberately does not expose this
        // spawn API: local coordinates belong to the game's current map.
        here.load( project_to<coords::sm>( anchor ) - point( 8, 4 ), false );
        get_avatar().setpos( here, here.get_bub( source ) );
        here.spawn_monsters( true );
        for( int z = 0; z < 4; ++z ) {
            here.build_map_cache( z );
        }
    };
    if( load_first ) {
        enter_tower();
        REQUIRE( population()->seed_committed );
    }
    // This invokes the production sampler and delivery owner, not a synthetic
    // packet, direct discover call or future receiver stub.
    run_live_light_delivery_for_test();
    const auto &samples = live_light::samples_for_turn( calendar::turn );
    REQUIRE( std::any_of( samples.begin(), samples.end(), []( const auto & sample ) {
        return sample.mark.kind == "smoke";
    } ) );
    REQUIRE( population()->seed_committed );
    REQUIRE( population()->members.size() == 8 );
    const auto expected_source = project_to<coords::ms>( anchor ) + tripoint( 12, 9, 0 );
    CHECK( population()->source == expected_source );
    std::set<std::string> current_ids;
    for( const auto &member : population()->members ) {
        current_ids.insert( member.token.member_id );
    }
    CHECK( current_ids == saved_ids );
    const auto *tower = overmap_buffer.global_state.migo_towers.find( geometry->id );
    REQUIRE( tower );
    REQUIRE( tower->signal );
    CHECK( tower->signal->source_omt == source_omt );
    CHECK( tower->signal->source_ms == source );
    CHECK( tower->signal->observer_position.z() > 0 );
    CHECK( saved_ids.count( tower->signal->observer_member ) == 1 );
    CHECK( tower->signal->observed_turn == to_turn<int>( calendar::turn ) );
    CHECK( tower->recognized_enemies == 0 ); // The distant night emitter is not an occupant read.
    CHECK( tower->decision.members.size() == 3 );
    CHECK( tower->decision.reason == "uncertain_signal_ordinary_party" );
    REQUIRE( tower->find_source( source_omt ) );
    CHECK( tower->find_source( source_omt )->opposition.empty() );
    CHECK( tower->find_source( source_omt )->completed_generation == 0 );
    CHECK( std::count_if( population()->members.begin(),
    population()->members.end(), []( const auto &m ) {
        return m.assignment == migo_population::duty::dispatched;
    } ) == 3 ); // The actual delivery owner committed the response, without a direct API call.
    REQUIRE( tower->operation );
    CHECK( tower->operation->generation == 1 );
    CHECK( tower->next_generation == 2 );
    CHECK( tower->operation->members.size() == 3 );
    CHECK( tower->operation->party_at_departure.size() == 3 );
    CHECK( tower->operation->sweep.candidates.size() >= 45 );
    CHECK( tower->operation->sweep.candidates.size() <= 46 );
    std::set<tripoint_abs_ms> candidate_positions;
    for( const auto &candidate : tower->operation->sweep.candidates ) {
        CHECK( candidate_positions.insert( candidate.position ).second );
        const auto p = project_to<coords::omt>( candidate.position );
        CHECK( std::abs( p.x() - source_omt.x() ) <= 1 );
        CHECK( std::abs( p.y() - source_omt.y() ) <= 1 );
    }
    CHECK( tower->operation->sweep.pending() == source );
    CHECK_FALSE( migo_tower::begin_response( geometry->id, to_turn<int>( calendar::turn ) ) );
    // A missing cross-file policy record cannot lease a parallel copy of
    // existing away members. Retain their actual payloads/epochs and restore
    // this simulated save-gap operand before any movement.
    auto *mutable_tower = overmap_buffer.global_state.migo_towers.find( geometry->id );
    const auto saved_operation = mutable_tower->operation;
    mutable_tower->operation.reset();
    CHECK_FALSE( migo_tower::begin_response( geometry->id, to_turn<int>( calendar::turn ) ) );
    CHECK( mutable_tower->next_generation == 2 );
    mutable_tower->operation = saved_operation;
    int leased = 0;
    int defenders = 0;
    for( const auto &member : population()->members ) {
        if( member.assignment == migo_population::duty::dispatched ) {
            ++leased;
            REQUIRE( member.current_position );
            CHECK( tower->operation->contains( member.token.member_id ) );
            if( !load_first ) {
                auto &owner = overmap_buffer.get( project_to<coords::om>( member.home ).xy() );
                bool found = false;
                for( const auto &entry : owner.horde_entities() ) {
                    if( entry.second->monster_data && entry.second->monster_data->population_identity().member_id ==
                        member.token.member_id ) {
                        found = true;
                        CHECK( member.current_position == entry.first );
                        CHECK( entry.second->is_active() );
                        CHECK( entry.second->tracking_intensity == 0 );
                        CHECK( migo_tower::owns_intent( *entry.second->monster_data,
                                                       migo_population::representation::abstract ) );
                    }
                }
                CHECK( found );
            }
        } else {
            ++defenders;
        }
    }
    CHECK( leased == 3 );
    CHECK( defenders == 5 );
    const auto first_observation = tower->last_sense_turn;
    run_live_light_delivery_for_test();
    if( !load_first ) {
        // The actual scheduler must admit these native non-zombie payloads
        // without manufacturing tracking, and must not service them twice.
        auto &owner = overmap_buffer.get( project_to<coords::om>( anchor ).xy() );
        // Recreate the actual native unload constructor for an unselected
        // home resident with an inherited goal. Its immobile admission must
        // survive servicing somebody else's operation.
        std::optional<tripoint_abs_ms> defender_position;
        std::string defender_id;
        time_point defender_processed;
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data &&
                entry.second->monster_data->population_identity().site_id == geometry->id &&
                !tower->operation->contains( entry.second->monster_data->population_identity().member_id ) ) {
                monster defender( *entry.second->monster_data );
                defender_position = entry.first;
                defender_id = defender.population_identity().member_id;
                defender.set_dest( entry.first + point::east );
                REQUIRE( owner.release_monster_payload( entry.first, defender_id ) );
                auto *retained = owner.retain_monster_payload( entry.first, defender );
                REQUIRE( retained );
                REQUIRE( retained->tracking_intensity == 1000 );
                defender_processed = retained->last_processed;
                break;
            }
        }
        REQUIRE( defender_position );
        calendar::turn += 1_turns;
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        overmap_buffer.move_hordes();
        bool defender_found = false;
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data &&
                entry.second->monster_data->population_identity().member_id == defender_id ) {
                defender_found = true;
                CHECK( entry.first == *defender_position );
                CHECK( entry.second->last_processed == defender_processed );
                CHECK( population()->find( defender_id )->assignment == migo_population::duty::available );
            }
        }
        CHECK( defender_found );
        std::vector<std::string> before;
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data && tower->operation->contains(
                    entry.second->monster_data->population_identity().member_id ) ) {
                std::ostringstream saved;
                { JsonOut json( saved ); entry.second->monster_data->serialize( json ); }
                before.push_back( saved.str() );
            }
        }
        REQUIRE( before.size() == 3 );
        overmap_buffer.move_hordes();
        std::vector<std::string> after;
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data && tower->operation->contains(
                    entry.second->monster_data->population_identity().member_id ) ) {
                std::ostringstream saved;
                { JsonOut json( saved ); entry.second->monster_data->serialize( json ); }
                after.push_back( saved.str() );
            }
        }
        std::sort( before.begin(), before.end() );
        std::sort( after.begin(), after.end() );
        CHECK( before == after );
    }
    enter_tower();
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    calendar::turn += 1_turns;
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    for( monster &body : g->all_monsters() ) {
        if( body.population_identity().site_id == geometry->id &&
            tower->operation->contains( body.population_identity().member_id ) ) {
            REQUIRE( migo_tower::owns_intent( body, migo_population::representation::local ) );
            const int before = body.get_moves();
            const bool already_credited = body.migo_intent()->last_energy_world == to_turn<int>( calendar::turn );
            body.process_turn();
            CHECK( body.get_moves() == before + ( already_credited ? 0 : body.get_speed() ) );
            const int after = body.get_moves();
            body.process_turn();
            CHECK( body.get_moves() == after );
            CHECK( body.migo_intent()->last_native_process_world == to_turn<int>( calendar::turn ) );
        }
    }
    CHECK( population()->members.size() == 8 );
    CHECK( population()->source == expected_source );
    CHECK( overmap_buffer.global_state.migo_towers.find( geometry->id )->last_sense_turn ==
           first_observation );
    for( const auto &member : population()->members ) {
        CHECK( saved_ids.count( member.token.member_id ) == 1 );
    }
    // Lower-level movement proof uses the actual generated body, stairs and
    // resin portals. It is not a selected assault or a live native journey.
    monster *climber = nullptr;
    for( monster &body : g->all_monsters() ) {
        if( body.population_identity().site_id == geometry->id &&
            ( !climber || body.pos_abs().z() > climber->pos_abs().z() ) ) {
            climber = &body;
        }
    }
    REQUIRE( climber );
    REQUIRE( climber->pos_abs().z() == 3 );
    int descents = 0;
    int doors_opened = 0;
    for( int steps = 0; steps < 200 && ( climber->pos_abs().z() != 0 ||
                                         project_to<coords::omt>( climber->pos_abs() ).xy() == anchor.xy() ); ++steps ) {
        calendar::turn += 1_turns;
        climber->process_turn();
        const auto before = climber->pos_abs();
        const auto next = physical_navigation::read_step( *climber, source );
        INFO( next.reason );
        CAPTURE( before, next.position, steps );
        REQUIRE( next.known );
        REQUIRE( next.next != physical_navigation::action::none );
        if( next.next == physical_navigation::action::open_door ) {
            REQUIRE( physical_navigation::commit_door( *climber, next.position ) );
            climber->mod_moves( -next.move_cost );
            ++doors_opened;
            CHECK( climber->pos_abs() == before );
        } else {
            REQUIRE( here.inbounds( next.position ) );
            REQUIRE( climber->move_to( here.get_bub( next.position ) ) );
            CHECK( climber->pos_abs() == next.position );
            descents += climber->pos_abs().z() < before.z() ? 1 : 0;
        }
        CHECK( population()->owns( climber->population_identity(),
                                   migo_population::representation::local ) );
    }
    CHECK( descents == 3 );
    CHECK( doors_opened > 0 );
    CHECK( climber->pos_abs().z() == 0 );
    CHECK( project_to<coords::omt>( climber->pos_abs() ).xy() != anchor.xy() );
    // Supplemental refusal controls reuse the genuinely received packet. The
    // synthetic freshness below earns no new positive emission/receipt proof.
    auto refused_samples = samples;
    calendar::turn += 5_minutes;
    for( auto &sample : refused_samples ) {
        sample.observed_turn = to_turn<int>( calendar::turn );
        sample.source_ms = source + point( 24, 0 ); // Contradicts its original OMT.
    }
    CHECK( migo_tower::receive_signals( refused_samples,
                                        to_turn<int>( calendar::turn ) ).received == 0 );
    CHECK( overmap_buffer.global_state.migo_towers.find( geometry->id )->last_sense_turn ==
           first_observation );
    for( monster &body : g->all_monsters() ) {
        if( body.population_identity().site_id == geometry->id ) {
            body.add_effect( efftype_id( "blind" ), 1_hours );
            REQUIRE_FALSE( body.can_see() );
        }
    }
    calendar::turn += 5_minutes;
    refused_samples = samples;
    for( auto &sample : refused_samples ) {
        sample.observed_turn = to_turn<int>( calendar::turn );
    }
    CHECK( migo_tower::receive_signals( refused_samples,
                                        to_turn<int>( calendar::turn ) ).received == 0 );
    CHECK( overmap_buffer.global_state.migo_towers.find( geometry->id )->last_sense_turn ==
           first_observation );
}


TEST_CASE( "migo_receiver_retains_material_access_and_orders_actual_extinction_before_relight",
           "[migo_tower][raid_sweep][physical_light][native][serialization]" )
{
    // Controlled committed residents and an open balcony isolate observation.
    // Real mapgen/seed conservation is covered by the preceding receiver test;
    // this is no live native story, production interval or floor-plan credit.
    clear_avatar();
    clear_map_with_vision( -1, 4, false );
    const auto old_turn = calendar::turn;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_bandits = overmap_buffer.global_state.bandit_live_world;
    overmap_buffer.global_state.migo_populations.clear();
    overmap_buffer.global_state.migo_towers.clear();
    overmap_buffer.global_state.bandit_live_world = {};
    reset_live_light_sample_cache();
    set_time_to_day();
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
    get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
    const auto anchor = project_to<coords::omt>( here.get_abs( tripoint_bub_ms( 60, 60, 0 ) ) );
    std::vector<oter_id> old_terrain;
    for( int z = 0; z < 4; ++z ) {
        const tripoint_abs_omt p( anchor.xy(), z );
        old_terrain.push_back( overmap_buffer.ter( p ) );
        overmap_buffer.ter_set( p, oter_str_id( "mi-go_scout_tower_" + std::to_string(
                z + 1 ) + "_north" ) );
    }
    const auto source = project_to<coords::ms>( anchor ) + point( 36, 12 );
    const auto source_area = project_to<coords::omt>( source );
    const auto access = project_to<coords::ms>( source_area ) + point( 6, 12 );
    const auto old_source_terrain = overmap_buffer.ter( source_area );
    overmap_buffer.ter_set( source_area, oter_str_id( "field" ) );
    on_out_of_scope restore( [&]() {
        clear_map_without_vision( -1, 4 );
        clear_creatures();
        for( int z = 0; z < 4; ++z ) {
            overmap_buffer.ter_set( tripoint_abs_omt( anchor.xy(), z ), old_terrain[z] );
        }
        overmap_buffer.ter_set( source_area, old_source_terrain );
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.global_state.bandit_live_world = old_bandits;
        get_avatar().unset_mutation( trait_id( "DEBUG_CLOAK" ) );
        reset_live_light_sample_cache();
        set_time( old_turn );
    } );
    const auto geometry = migo_population::geometry_at( anchor );
    REQUIRE( geometry );
    migo_population::site_record population;
    population.id = geometry->id;
    population.anchor = anchor;
    std::vector<migo_population::member_record> residents;
    for( int i = 0; i < 6; ++i ) {
        const auto position = project_to<coords::ms>( anchor ) + tripoint( 6 + i, 12, i == 5 ? 1 : 0 );
        if( i == 5 ) {
            here.ter_set( here.get_bub( position ), ter_str_id( "t_floor" ) );
        }
        const mtype_id type( "mon_mi_go" );
        monster *body = g->place_critter_at( type, here.get_bub( position ) );
        REQUIRE( body );
        migo_population::member_record member;
        member.token = {population.id, "resident-" + std::to_string( i ), 1, anchor};
        member.type = type;
        member.home = position;
        member.owner = migo_population::representation::local;
        member.hp = body->get_hp();
        body->population_identity() = member.token;
        residents.push_back( member );
    }
    REQUIRE( population.commit_seed( residents, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    // Keep the avatar off the fire. Exercise the actual discovery phases,
    // then the actual receiver cadence, without treating clock setup as
    // production/cooldown or whole native-story elapsed-time credit.
    get_avatar().setpos( here, here.get_bub( source ) + point( 0, -24 ), false );
    here.add_field( here.get_bub( source ), field_type_id( "fd_fire" ), 3 );
    for( int z = 0; z < 4; ++z ) {
        here.build_map_cache( z );
    }
    for( int tick = 0; tick < 2 * physical_light::loaded_source_sampler::discovery_interval_turns &&
         !overmap_buffer.global_state.migo_towers.find( population.id ); ++tick ) {
        calendar::turn += 1_turns;
        run_live_light_delivery_for_test();
    }
    auto *site = overmap_buffer.global_state.migo_towers.find( population.id );
    std::ostringstream diagnosis;
    diagnosis << "scan=" << overmap_buffer.global_state.migo_towers.last_signal_scan
              << " source=" << source << " outside=" << here.is_outside( here.get_bub( source ) );
    for( const auto &sample : live_light::samples_for_turn( calendar::turn ) ) {
        diagnosis << " sample=" << sample.mark.kind << ':' << sample.source_omt
                  << " range=" << sample.range_cap_omt << " support=" << sample.smoke_support_reason;
    }
    for( const auto &registered : overmap_buffer.global_state.migo_populations.sites ) {
        diagnosis << " population=" << registered.id << " seeded=" << registered.seed_committed
                  << " count=" << registered.members.size();
    }
    for( const monster &body : g->all_monsters() ) {
        diagnosis << " body=" << body.population_identity().member_id << ':' << body.pos_abs()
                  << " sees=" << body.can_see() << " hp=" << body.get_hp();
    }
    INFO( diagnosis.str() );
    REQUIRE( site );
    REQUIRE( site->signal );
    CHECK( site->signal->source_ms == source );
    auto *history = site->find_source( source_area );
    REQUIRE( history );
    REQUIRE( history->observed_access.count( access.to_string_writable() ) == 1 );
    const auto first_access = history->observed_access.at( access.to_string_writable() );
    const auto first_evidence = history->latest_evidence;
    // A cosmetic ground change with the same physical access is no new raid.
    here.ter_set( here.get_bub( access ), ter_str_id( "t_dirt" ) );
    calendar::turn += 5_minutes;
    run_live_light_delivery_for_test();
    CHECK( history->observed_access.at( access.to_string_writable() ) == first_access );
    CHECK( history->latest_evidence == first_evidence );
    here.ter_set( here.get_bub( access ), ter_str_id( "t_wall" ) );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    here.invalidate_visibility_cache();
    calendar::turn += 5_minutes;
    run_live_light_delivery_for_test();
    CHECK( history->observed_access.at( access.to_string_writable() ) != first_access );
    CHECK( history->latest_evidence != first_evidence );
    const auto episode = history->episode;
    here.ter_set( here.get_bub( access ), ter_str_id( "t_dirt" ) );
    here.remove_field( here.get_bub( source ), field_type_id( "fd_fire" ) );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    here.invalidate_visibility_cache();
    calendar::turn += 5_minutes;
    run_live_light_delivery_for_test();
    std::ostringstream extinction;
    extinction << "now=" << to_turn<int>( calendar::turn ) << " scan="
               << overmap_buffer.global_state.migo_towers.last_signal_scan
               << " signal=" << site->signal->kind << ':' << *site->signal->source_ms
               << " present=" << live_light::source_present_for_turn( calendar::turn,
                       *site->signal->source_ms, site->signal->kind ).value_or( true );
    scout_observation::site_reader extinction_reader;
    for( const monster &body : g->all_monsters() ) {
        if( body.pos_abs().z() == 1 ) {
            const auto square = here.get_bub( source );
            extinction << " observer=" << body.population_identity().member_id
                       << " range=" << body.sight_range( here.ambient_light_at( square ) )
                       << " optics=" << here.sees_with_optics( body.pos_bub(), square,
                               MAX_VIEW_DISTANCE * 2, 2.0f )
                       << " ray=" << extinction_reader.read_ray( body.pos_abs(), source ).visible;
        }
    }
    INFO( extinction.str() );
    CHECK_FALSE( history->observed_active );
    CHECK( history->extinct_world == to_turn<int>( calendar::turn ) );
    const int extinguished_at = history->extinct_world;
    here.add_field( here.get_bub( source ), field_type_id( "fd_fire" ), 3 );
    for( int tick = 0; tick < 2 * physical_light::loaded_source_sampler::discovery_interval_turns &&
         !history->observed_active; ++tick ) {
        calendar::turn += 1_turns;
        run_live_light_delivery_for_test();
    }
    CHECK( history->observed_active );
    CHECK( history->extinct_world == extinguished_at ); // sparse discovery is not another absence
    CHECK( history->episode == episode + 1 );
    const auto retained = history->observed_access;
    for( monster &body : g->all_monsters() ) {
        body.add_effect( efftype_id( "blind" ), 1_hours );
    }
    calendar::turn += 5_minutes;
    run_live_light_delivery_for_test();
    CHECK( history->observed_access == retained ); // lost visibility never erases known access
    std::ostringstream bytes;
    {
        JsonOut out( bytes );
        site->serialize( out );
    }
    migo_tower::site_state loaded;
    loaded.deserialize( json_loader::from_string( bytes.str() ) );
    REQUIRE( loaded.find_source( source_area ) );
    CHECK( loaded.find_source( source_area )->episode == episode + 1 );
    CHECK( loaded.find_source( source_area )->observed_access == retained );
}

TEST_CASE( "migo_capability_admission_uses_damage_survival_and_real_reserve",
           "[migo_tower]" )
{
    using namespace migo_tower;
    combat_profile ordinary;
    ordinary.known = true;
    ordinary.hp = 100;
    ordinary.speed = 100;
    ordinary.hit = 5;
    ordinary.dodge = 5;
    ordinary.melee.add_damage( damage_type_id( "bash" ), 20 );
    std::vector<candidate> available;
    for( int i = 0; i < 8; ++i ) {
        available.push_back( {"body-" + std::to_string( i ),
                              i < 6 ? "mon_mi_go" : "mon_mi_go_scout", ordinary} );
    }
    const auto baseline = select_party( available, {}, 20, false );
    REQUIRE( baseline.members.size() == 3 );
    CHECK( baseline.members == std::vector<std::string> {"body-0", "body-1", "body-6"} );
    combat_profile enemy = ordinary;
    enemy.hp = 620;
    // Three bodies lose two before inflicting620 damage; four lose only one.
    const auto larger = select_party( available, {enemy}, 1, false );
    REQUIRE( larger.members.size() == 4 );
    CHECK( larger.projection.known );
    CHECK( larger.projection.sufficient );
    CHECK( larger.projection.casualties == 1 );
    CHECK( larger.projection.party_damage_per_second > 0 );
    const auto cautious = select_party( available, {enemy}, 1, true );
    // Six cannot meet the cautious criterion with the preferred reserve;
    // the legitimate larger response may consume it without relaxing loss.
    CHECK( cautious.members.size() > 6 );
    CHECK( cautious.required_home_reserve == 0 );
    CHECK( cautious.projection.sufficient );
    CHECK( cautious.projection.casualties == 0 );
    enemy.known = false;
    CHECK( select_party( available, {enemy}, 1, false ).reason == "observed_capability_unknown" );
    enemy.known = true;
    enemy.ranged_range = 10;
    enemy.ranged.add_damage( damage_type_id( "bullet" ), 200 );
    CHECK( select_party( available, {enemy}, 20, false ).members.empty() );
    CHECK( select_party( available, {enemy}, 5, false ).members.empty() );
    // Armor is a damage-channel fact, not a size or generic power rating.
    enemy.ranged = damage_instance();
    enemy.armor.set_resist( damage_type_id( "bash" ), 100 );
    CHECK( select_party( available, {enemy}, 1, false ).members.empty() );
    available.resize( 4 );
    const auto four = select_party( available, {}, 1, false );
    CHECK( four.members.size() == 3 );
    CHECK( four.required_home_reserve == 0 );
    available.push_back( {"substitute", "mon_mi_go", ordinary} );
    CHECK( select_party( available, {}, 1, false ).members.size() == 3 );
    available.back().capability.hp = 0;
    const auto surviving = select_party( available, {}, 1, false );
    CHECK( surviving.members.size() == 3 );
    CHECK( surviving.required_home_reserve == 0 );
    CHECK( std::find( surviving.members.begin(), surviving.members.end(), "substitute" ) ==
           surviving.members.end() );
    available.resize( 2 );
    CHECK( select_party( available, {}, 1, false ).members.empty() );
}


TEST_CASE( "migo_native_capabilities_require_perception_and_ignore_hidden_human_contents",
           "[migo_tower][physical_light][native]" )
{
    clear_avatar();
    clear_map_with_vision( -1, 4, false );
    const auto old_turn = calendar::turn;
    on_out_of_scope restore_scene( [&]() {
        clear_avatar();
        clear_map_without_vision( -1, 4 );
        set_time( old_turn );
    } );
    set_time_to_day();
    map &here = get_map();
    monster observer( mtype_id( "mon_mi_go" ) );
    observer.set_pos_abs_only( here.get_abs( tripoint_bub_ms( 12, 66, 0 ) ) );
    auto &human = get_avatar();
    human.setpos( here, tripoint_bub_ms( 30, 66, 0 ), false );
    REQUIRE( human.wear_item( item( itype_id( "backpack" ) ), false ) );
    here.build_map_cache( 0 );
    scout_observation::site_reader first_read;
    const auto unarmed = migo_tower::observe_enemy( observer, human, first_read );
    REQUIRE( unarmed );
    REQUIRE( unarmed->known );
    CHECK( unarmed->hp > 0 );
    human.set_part_hp_cur( bodypart_id( "torso" ), 1 );
    human.i_add( item( itype_id( "rock" ) ) );
    REQUIRE_FALSE( human.get_wielded_item() );
    scout_observation::site_reader hidden_change;
    const auto same = migo_tower::observe_enemy( observer, human, hidden_change );
    REQUIRE( same );
    CHECK( same->hp == unarmed->hp );
    CHECK( same->melee.total_damage() == unarmed->melee.total_damage() );
    CHECK( same->ranged.total_damage() == unarmed->ranged.total_damage() );
    REQUIRE( itype_id( "glock_19" ).is_valid() );
    item gun( itype_id( "glock_19" ) );
    human.set_wielded_item( gun );
    scout_observation::site_reader armed_read;
    const auto armed = migo_tower::observe_enemy( observer, human, armed_read );
    REQUIRE( armed );
    CHECK( armed->ranged.total_damage() > 0 );
    CHECK( armed->ranged_range > 0 );
    gun.ammo_set( gun.ammo_default() );
    human.set_wielded_item( gun );
    scout_observation::site_reader loaded_read;
    const auto loaded = migo_tower::observe_enemy( observer, human, loaded_read );
    REQUIRE( loaded );
    CHECK( loaded->ranged.total_damage() == armed->ranged.total_damage() );
    CHECK( loaded->ranged_range == armed->ranged_range );
    observer.add_effect( efftype_id( "blind" ), 1_hours );
    scout_observation::site_reader blind_read;
    CHECK_FALSE( migo_tower::observe_enemy( observer, human, blind_read ) );
    observer.remove_effect( efftype_id( "blind" ) );
    for( int y = 0; y < 132; ++y ) {
        here.ter_set( tripoint_bub_ms( 20, y, 0 ), ter_str_id( "t_wall" ) );
    }
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    scout_observation::site_reader occluded_read;
    CHECK_FALSE( migo_tower::observe_enemy( observer, human, occluded_read ) );
    const auto own_before = migo_tower::own_capability( observer );
    observer.set_hp( 50 );
    const auto own_after = migo_tower::own_capability( observer );
    REQUIRE( own_before.known );
    CHECK( own_after.hp == 50 );
    CHECK( own_after.hp < own_before.hp );
    monster scout( mtype_id( "mon_mi_go_scout" ) );
    const auto scout_profile = migo_tower::own_capability( scout );
    REQUIRE( scout_profile.known );
    CHECK( scout_profile.ranged.total_damage() > 0 );
    CHECK( scout_profile.ranged_range == 30 );
    CHECK( scout_profile.ranged_cost >= 5 * scout.get_speed() );
}

TEST_CASE( "migo_observed_modular_gun_does_not_invent_hidden_ammunition",
           "[migo_tower][migo_modular_weapon][native]" )
{
    const bool npc_target = GENERATE( false, true );
    CAPTURE( npc_target );
    clear_avatar();
    clear_map_with_vision( -1, 4, false );
    const auto old_turn = calendar::turn;
    on_out_of_scope restore_scene( [&]() {
        clear_avatar();
        clear_map_without_vision( -1, 4 );
        set_time( old_turn );
    } );
    set_time_to_day();
    map &here = get_map();
    monster observer( mtype_id( "mon_mi_go" ) );
    observer.set_pos_abs_only( here.get_abs( tripoint_bub_ms( 12, 66, 0 ) ) );
    npc resident;
    resident.normalize();
    Character &human = npc_target ? static_cast<Character &>( resident ) : get_avatar();
    const tripoint_bub_ms target( 30, 66, 0 );
    if( npc_target ) {
        // Initial pose of an unregistered observation subject, not a transfer
        // between native overmap owners. The actual avatar keeps setpos.
        resident.set_pos_abs_only( here.get_abs( target ) );
    } else {
        human.setpos( here, target, false );
    }
    here.build_map_cache( 0 );
    const auto bytes = []( const item &value ) {
        std::ostringstream output;
        JsonOut json( output );
        value.serialize( json );
        return output.str();
    };
    const auto turn = calendar::turn;
    const auto human_position = human.pos_abs();
    const auto observer_position = observer.pos_abs();
    const int human_hp = human.get_part_hp_cur( bodypart_id( "torso" ) );
    const int observer_hp = observer.get_hp();
    const auto ground = here.ter( here.get_bub( human_position ) );
    std::vector<migo_tower::candidate> available;
    for( int i = 0; i < 7; ++i ) {
        available.push_back( {"resident-" + std::to_string( i ), "mon_mi_go",
                              migo_tower::own_capability( observer )} );
    }
    // Production item groups supply real receivers. Neither a changed hidden
    // caliber nor the magazine's actual load may become observed knowledge.
    for( const std::string group : {"modular_ar15", "ar15_300"} ) {
        for( const int rounds : {0, 28} ) {
            CAPTURE( group, rounds );
            item rifle = item_group::item_from( item_group_id( group ) );
            REQUIRE( rifle.typeId() == itype_id( "modular_ar15" ) );
            REQUIRE_FALSE( rifle.ammo_default().is_null() );
            rifle.ammo_set( rifle.ammo_default(), rounds );
            REQUIRE( rifle.ammo_remaining() == rounds );
            item bare_type( rifle.typeId() );
            REQUIRE( bare_type.ammo_default().is_null() );
            human.set_wielded_item( rifle );
            const std::string original = bytes( *human.get_wielded_item() );
            // Real item save/load retains the functional receiver and ammo.
            item restored;
            restored.deserialize( json_loader::from_string( original ) );
            REQUIRE( bytes( restored ) == original );
            human.set_wielded_item( restored );
            // The setup setter copies items; bind conservation after that copy.
            const std::string observed_before = bytes( *human.get_wielded_item() );
            for( int repeat = 0; repeat < 2; ++repeat ) {
                std::optional<migo_tower::combat_profile> observed;
                scout_observation::site_reader reader;
                const std::string diagnostic = capture_debugmsg_during( [&]() {
                    observed = migo_tower::observe_enemy( observer, human, reader );
                } );
                REQUIRE( observed );
                INFO( diagnostic );
                CHECK( diagnostic.empty() );
                CHECK_FALSE( observed->known );
                const auto admission = migo_tower::select_party( available, {*observed}, 18, false );
                CHECK( admission.members.empty() );
                CHECK( admission.reason == "observed_capability_unknown" );
                CHECK( bytes( *human.get_wielded_item() ) == observed_before );
            }
        }
    }
    // A fixed-caliber definition remains a useful, known armed estimate even
    // when the actual magazine is empty. No hidden ammo peek is needed.
    human.set_wielded_item( item( itype_id( "glock_19" ) ) );
    const std::string fixed_before = bytes( *human.get_wielded_item() );
    scout_observation::site_reader fixed_reader;
    std::optional<migo_tower::combat_profile> fixed;
    CHECK( capture_debugmsg_during( [&]() {
        fixed = migo_tower::observe_enemy( observer, human, fixed_reader );
    } ).empty() );
    REQUIRE( fixed );
    CHECK( fixed->known );
    CHECK( fixed->ranged.total_damage() > 0 );
    CHECK( fixed->ranged_range > 0 );
    CHECK( bytes( *human.get_wielded_item() ) == fixed_before );
    CHECK( calendar::turn == turn );
    CHECK( human.pos_abs() == human_position );
    CHECK( observer.pos_abs() == observer_position );
    CHECK( human.get_part_hp_cur( bodypart_id( "torso" ) ) == human_hp );
    CHECK( observer.get_hp() == observer_hp );
    CHECK( here.ter( here.get_bub( human_position ) ) == ground );
    // Invalid API input still diagnoses rather than silently accepting NULL.
    item bare_type( itype_id( "modular_ar15" ) );
    const std::string bare_before = bytes( bare_type );
    const std::string invalid = capture_debugmsg_during( [&]() {
        bare_type.ammo_set( itype_id::NULL_ID() );
    } );
    CHECK_THAT( invalid, Catch::Matchers::Contains( "can't set ammo null in AR-15 as it is not an ammo" ) );
    CHECK( bytes( bare_type ) == bare_before );
}

TEST_CASE( "migo_extended_melee_is_exposure_and_saved_threat_memory_is_conservative",
           "[migo_tower][serialization]" )
{
    using namespace migo_tower;
    combat_profile ordinary;
    ordinary.known = true;
    ordinary.hp = 100;
    ordinary.speed = 100;
    ordinary.hit = 5;
    ordinary.dodge = 5;
    ordinary.melee.add_damage( damage_type_id( "bash" ), 20 );
    combat_profile enemy = ordinary;
    enemy.melee.clear();
    melee_option reach;
    reach.damage.add_damage( damage_type_id( "bash" ), 200 );
    reach.hit = 5;
    reach.range = 4;
    reach.no_adjacent = true;
    enemy.melee_options.push_back( reach );
    const std::vector<combat_profile> party( 3, ordinary );
    CHECK( project( party, {enemy}, 1, true ).sufficient );
    CHECK_FALSE( project( party, {enemy}, 5, true ).sufficient );
    threat_scenarios memory;
    remember_observation( memory, {enemy} );
    remember_observation( memory, {} );
    remember_observation( memory, {enemy} );
    REQUIRE( memory.size() == 1 );
    site_state site;
    site.advance( 100, 9000 );
    site.advance( 101, 1 );
    site.advance( 101, 9000 );
    REQUIRE( site.active_elapsed == 1 );
    auto *history = site.remember_source( tripoint_abs_omt( 2, 3, 2 ) );
    REQUIRE( history );
    history->opposition = memory;
    history->latest_evidence = "physically-seen-weapon";
    REQUIRE( history->finish( 1, site.active_elapsed, "returned", {"witnessed-loss"}, false, party ) );
    CHECK_FALSE( history->finish( 1, 99, "returned", {}, false, party ) );
    CHECK( history->caution_level() == 1 );
    CHECK( history->requires_stronger() );
    admission_decision sufficient;
    sufficient.projection.known = true;
    sufficient.projection.sufficient = true;
    CHECK_FALSE( history->retry_ready( 86400, party, sufficient, true ) );
    CHECK_FALSE( history->retry_ready( 86401, party, sufficient, true ) );
    history->latest_evidence = "physically-seen-changed-weapon";
    CHECK( history->retry_ready( 86401, party, sufficient, true ) );
    std::ostringstream output;
    { JsonOut json( output ); site.serialize( json ); }
    site_state loaded;
    loaded.deserialize( json_loader::from_string( output.str() ) );
    const auto *restored = loaded.find_source( tripoint_abs_omt( 2, 3, 0 ) );
    REQUIRE( restored );
    CHECK( restored->caution_level() == 1 );
    CHECK( restored->retry_after_active == 86401 );
    REQUIRE( restored->opposition.size() == 1 );
    CHECK( capability_key( restored->opposition[0] ) == capability_key( memory[0] ) );
    CHECK( restored->opposition[0][0].melee_options[0].no_adjacent );
    CHECK( restored->opposition[0][0].melee_options[0].range == 4 );
    CHECK( loaded.active_elapsed == 1 );
    loaded.advance( 100000, 0 );
    CHECK( loaded.active_elapsed == 1 );
    loaded.advance( 100001, 1 );
    CHECK( loaded.active_elapsed == 2 );
    history = loaded.find_source( tripoint_abs_omt( 2, 3, -1 ) );
    REQUIRE( history );
    REQUIRE( history->finish( 2, 86401, "returned", {"second-loss"}, false, party ) );
    CHECK( history->caution_level() == 2 );
    CHECK( history->retry_after_active == 86401 + 7 * 86400 );
}

TEST_CASE( "migo_member_intent_follows_the_native_payload_and_reload",
           "[migo_tower][serialization][ownership]" )
{
    monster original( mtype_id( "mon_mi_go" ) );
    CHECK_FALSE( original.migo_intent() );
    original.population_identity() = { "site", "resident", 7, tripoint_abs_omt( 2, 2, 0 ) };
    migo_tower::member_intent intent;
    intent.generation = 4;
    intent.home = tripoint_abs_ms( 60, 60, 3 );
    intent.waypoint = tripoint_abs_ms( 72, 60, 0 );
    intent.last_energy_world = 101;
    intent.last_native_process_world = 100;
    original.set_migo_intent( intent );
    horde_entity prepared( original );
    REQUIRE( prepared.monster_data );
    REQUIRE( prepared.monster_data->migo_intent() );
    prepared.monster_data->migo_intent()->retreat();
    prepared.monster_data->migo_intent()->witnessed_losses.push_back( "witnessed-member" );
    // A prepared receiving candidate is a private payload, not a second
    // global motor or a mutation of the retained source on refusal.
    CHECK( original.migo_intent()->phase == migo_tower::member_phase::outbound );
    CHECK( original.migo_intent()->waypoint == intent.waypoint );
    CHECK( original.migo_intent()->witnessed_losses.empty() );
    CHECK( prepared.monster_data->population_identity().epoch == 7 );
    CHECK_FALSE( prepared.monster_data->migo_intent()->waypoint );
    prepared.monster_data->migo_intent()->waypoint = intent.home;
    prepared.monster_data->migo_intent()->retreat();
    CHECK( prepared.monster_data->migo_intent()->waypoint == intent.home );

    std::ostringstream saved;
    { JsonOut json( saved ); prepared.monster_data->serialize( json ); }
    monster loaded;
    loaded.deserialize( json_loader::from_string( saved.str() ) );
    REQUIRE( loaded.migo_intent() );
    CHECK( loaded.population_identity().member_id == "resident" );
    CHECK( loaded.population_identity().epoch == 7 );
    CHECK( loaded.migo_intent()->generation == 4 );
    CHECK( loaded.migo_intent()->phase == migo_tower::member_phase::returning );
    CHECK( loaded.migo_intent()->last_energy_world == 101 );
    CHECK( loaded.migo_intent()->last_native_process_world == 100 );
    CHECK( loaded.migo_intent()->witnessed_losses == std::vector<std::string>{"witnessed-member"} );
    original.clear_migo_intent();
    std::ostringstream old_saved;
    { JsonOut json( old_saved ); original.serialize( json ); }
    loaded.deserialize( json_loader::from_string( old_saved.str() ) );
    CHECK_FALSE( loaded.migo_intent() );
    CHECK( loaded.population_identity().epoch == 7 );
}

TEST_CASE( "migo_native_settlement_does_not_turn_roster_death_into_loss_knowledge",
           "[migo_tower][ownership][native][serialization]" )
{
    const bool still_away = GENERATE( false, true );
    // Component setup supplies leases/death truth, not a dispatched journey or
    // witnessed native casualty. The actual service caller must keep those
    // facts private until communication or its legitimate overdue allowance.
    clear_avatar();
    clear_map_with_vision( -1, 4, false );
    const auto old_turn = calendar::turn;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    on_out_of_scope restore( [&]() {
        clear_map_without_vision( -1, 4 );
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        set_time( old_turn );
    } );
    set_time( calendar::turn_zero + 1500_turns );
    get_avatar().setpos( get_map(), tripoint_bub_ms( 1, 1, 0 ), false );
    const auto home = get_map().get_abs( tripoint_bub_ms( 60, 60, 0 ) );
    const auto anchor = project_to<coords::omt>( home );
    const std::string id = "settlement-knowledge-component";
    migo_population::site_record population;
    population.id = id;
    population.anchor = anchor;
    std::vector<migo_population::member_record> members;
    for( const auto &name : {"returnee", "unseen-one", "unseen-two"} ) {
        migo_population::member_record member;
        member.token = {id, name, 1, anchor};
        member.type = mtype_id( "mon_mi_go" );
        member.home = home;
        member.hp = member.type->hp;
        member.assignment = migo_population::duty::dispatched;
        member.owner = name == std::string( "returnee" ) ?
                       migo_population::representation::local : migo_population::representation::abstract;
        members.push_back( member );
    }
    REQUIRE( population.commit_seed( members, std::nullopt, "", 0, true, false ) );
    REQUIRE( population.record_death( members[1].token ) );
    REQUIRE( population.record_death( members[2].token ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = anchor;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = anchor + point( 2, 0 );
    operation.members = {{"returnee"}, {"unseen-one"}, {"unseen-two"}};
    operation.sweep.candidates = {{get_map().get_abs( tripoint_bub_ms( 110, 60, 0 ) )}};
    operation.overdue_after_active = 10;
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    site.operation = operation;
    REQUIRE( site.remember_source( operation.source.source_omt ) );
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster *body = g->place_critter_at( mtype_id( "mon_mi_go" ),
                                       tripoint_bub_ms( still_away ? 100 : 60, 60, 0 ) );
    REQUIRE( body );
    body->population_identity() = members[0].token;
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.phase = still_away ? migo_tower::member_phase::outbound : migo_tower::member_phase::returning;
    intent.home = home;
    body->set_migo_intent( intent );
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 0 );
    calendar::turn += 1_turns;
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    auto *current = overmap_buffer.global_state.migo_towers.find( id );
    REQUIRE( current );
    REQUIRE( current->operation );
    CHECK( current->operation->members[0].returned == !still_away );
    CHECK( current->operation->members[1].settled );
    CHECK( current->operation->members[2].settled );
    CHECK( current->operation->communicated_losses.empty() );
    CHECK_FALSE( current->operation->resolved );
    CHECK( current->find_source( operation.source.source_omt )->completed_generation == 0 );
    calendar::turn += 9_turns;
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 9 );
    CHECK( current->operation->resolved == !still_away );
    CHECK_FALSE( body->is_dead_state() );
    const auto *history = current->find_source( operation.source.source_omt );
    REQUIRE( history );
    CHECK( history->outcome == "overdue_missing" );
    CHECK( history->casualty_attempts == 0 );
    CHECK( history->overdue_attempts == 1 );
    CHECK( history->last_witnessed_losses.empty() );
    std::ostringstream saved;
    { JsonOut json( saved ); current->serialize( json ); }
    migo_tower::site_state loaded;
    loaded.deserialize( json_loader::from_string( saved.str() ) );
    CHECK( loaded.operation->resolved == !still_away );
    CHECK( loaded.find_source( operation.source.source_omt )->overdue_attempts == 1 );
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 9 );
    CHECK( current->find_source( operation.source.source_omt )->overdue_attempts == 1 );
}

TEST_CASE( "migo_terminal_home_interval_never_activates_an_away_or_unauthenticated_survivor",
           "[migo_tower][migo_activation][native][serialization]" )
{
    const std::string mode = GENERATE( "active", "frozen", "away", "missing", "divergent" );
    CAPTURE( mode );
    clear_avatar();
    clear_map_without_vision();
    const auto old_turn = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    overmap_buffer.global_state.migo_populations.clear();
    overmap_buffer.global_state.migo_towers.clear();
    set_time( calendar::turn_zero + 15000_turns );
    const auto position = get_map().get_abs( tripoint_bub_ms( 60, 60, 0 ) );
    const auto anchor = project_to<coords::omt>( position );
    const auto away = project_to<coords::ms>( anchor + point( 41, 0 ) ) + point( 12, 12 );
    const std::string id = "terminal-home-interval";
    overmap *away_owner = nullptr;
    on_out_of_scope restore( [&]() {
        if( away_owner ) {
            away_owner->release_monster_payload( away, "peer" );
        }
        clear_map_without_vision();
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        get_avatar().setpos( old_avatar, false );
        set_time( old_turn );
    } );
    get_avatar().setpos( position - point( 3, 0 ), false );
    migo_population::site_record population;
    population.id = id;
    population.anchor = anchor;
    std::vector<migo_population::member_record> members;
    for( const auto &name : {
             "returnee", "peer", "unseen-loss"
         } ) {
        migo_population::member_record member;
        member.token = {id, name, 1, anchor};
        member.type = mtype_id( "mon_mi_go" );
        member.home = position;
        member.hp = member.type->hp;
        member.owner = migo_population::representation::local;
        member.assignment = migo_population::duty::available;
        if( member.token.member_id == "peer" && mode == "away" ) {
            member.owner = migo_population::representation::abstract;
            member.assignment = migo_population::duty::returning;
        }
        members.push_back( member );
    }
    REQUIRE( population.commit_seed( members, std::nullopt, "", 0, true, false ) );
    REQUIRE( population.record_death( members[2].token ) );
    if( mode == "active" || mode == "frozen" ) {
        REQUIRE( population.record_death( members[1].token ) );
    }
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = anchor;
    site.operation.emplace();
    auto &operation = *site.operation;
    operation.generation = 1;
    operation.source.source_omt = anchor + point( 2, 0 );
    operation.members = {{"returnee", true, true, position},
        {"peer", mode != "away", mode != "away"}, {"unseen-loss", true}
    };
    operation.sweep.candidates = {{position + point( 3, 0 )}};
    operation.overdue_after_active = 10;
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    REQUIRE( site.remember_source( operation.source.source_omt ) );
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster *returned = g->place_critter_at( members[0].type, get_map().get_bub( position ) );
    REQUIRE( returned );
    returned->population_identity() = members[0].token;
    returned->set_moves( 137 );
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.phase = migo_tower::member_phase::settled;
    intent.home = position;
    returned->set_migo_intent( intent );
    if( mode == "away" ) {
        monster peer( members[1].type );
        peer.population_identity() = members[1].token;
        peer.set_pos_abs_only( away );
        peer.set_moves( 0 );
        intent.phase = migo_tower::member_phase::returning;
        peer.set_migo_intent( intent );
        away_owner = &overmap_buffer.get( project_to<coords::om>( away ).xy() );
        REQUIRE( away_owner->retain_monster_payload( away, peer ) );
    } else if( mode == "divergent" ) {
        monster *peer = g->place_critter_at( members[1].type, get_map().get_bub( position + point::east ) );
        REQUIRE( peer );
        peer->population_identity() = members[1].token;
        ++peer->population_identity().epoch; // stale saved ledger cannot authenticate this body
        peer->set_migo_intent( intent );
    }
    if( mode == "frozen" ) {
        get_avatar().setpos( project_to<coords::ms>( anchor - point( 41, 0 ) ), false );
    }
    const auto body_state = [&]() {
        std::ostringstream out;
        {
            JsonOut json( out );
            returned->serialize( json );
        }
        return out.str();
    };
    const auto before = body_state();
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    auto &current = *overmap_buffer.global_state.migo_towers.find( id )->operation;
    CHECK( current.activity.active == ( mode == "active" ) );
    CHECK( current.sweep.active_elapsed == 0 );
    CHECK_FALSE( migo_tower::owns_intent( *returned, migo_population::representation::local ) );
    CHECK( migo_tower::prepare_step( *returned, migo_population::representation::local ).next ==
           physical_navigation::action::none );
    calendar::turn += 10_turns;
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 10 );
    overmap_buffer.move_hordes();
    CHECK( current.sweep.active_elapsed == ( mode == "active" ? 10 : 0 ) );
    CHECK( current.resolved == ( mode == "active" ) );
    CHECK( current.communicated_losses.empty() );
    CHECK( current.sweep.pending() == position + point( 3, 0 ) );
    CHECK( body_state() == before );
    // Even an inactive home does not disable ordinary available local life.
    CHECK( migo_tower::activity_for( *returned, migo_population::representation::local,
                                     to_turn<int>( calendar::turn ) ) );
    if( away_owner ) {
        const auto *entry = away_owner->entity_at( project_remain<coords::om>( away ).remainder_tripoint );
        REQUIRE( entry );
        CHECK( entry->monster_data->pos_abs() == away );
        CHECK( entry->monster_data->migo_intent()->last_energy_world == -1 );
        CHECK( entry->moves == 0 );
    }
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 10 );
    CHECK( current.sweep.active_elapsed == ( mode == "active" ? 10 : 0 ) );
}

TEST_CASE( "migo_source_history_eviction_cannot_restore_a_blind_retry",
           "[migo_tower][serialization]" )
{
    using namespace migo_tower;
    site_state site;
    for( int i = 0; i < 4; ++i ) {
        auto *history = site.remember_source( tripoint_abs_omt( i, 0, 0 ) );
        REQUIRE( history );
        REQUIRE( history->finish( i + 1, 0, "searched_returned", {}, false, {} ) );
    }
    CHECK_FALSE( site.remember_source( tripoint_abs_omt( 4, 0, 0 ) ) );
    site.active_elapsed = 86400; // explicit component elapsed, no native time proof
    REQUIRE( site.remember_source( tripoint_abs_omt( 4, 0, 0 ) ) );
    CHECK_FALSE( site.find_source( tripoint_abs_omt( 0, 0, 0 ) ) );
    auto *recalled = site.remember_source( tripoint_abs_omt( 0, 0, 1 ) );
    REQUIRE( recalled );
    CHECK( recalled->prior_outcome_unavailable );
    CHECK( recalled->caution_level() == 2 );
    CHECK( recalled->retry_after_active == 8 * 86400 );
    admission_decision sufficient;
    sufficient.projection.known = true;
    sufficient.projection.sufficient = true;
    CHECK_FALSE( recalled->retry_ready( 9 * 86400, {}, sufficient, false ) );
    std::ostringstream saved;
    { JsonOut json( saved ); site.serialize( json ); }
    site_state loaded;
    loaded.deserialize( json_loader::from_string( saved.str() ) );
    REQUIRE( loaded.find_source( tripoint_abs_omt( 0, 0, 0 ) ) );
    CHECK( loaded.find_source( tripoint_abs_omt( 0, 0, 0 ) )->prior_outcome_unavailable );
    CHECK( loaded.prior_sources == site.prior_sources );
}

TEST_CASE( "migo_late_knowledge_resolves_one_overdue_attempt_without_double_loss",
           "[migo_tower][serialization]" )
{
    migo_tower::source_history history;
    history.latest_evidence = "actual-source-facts";
    REQUIRE( history.finish( 1, 100, "overdue_missing", {}, true, {} ) );
    CHECK( history.overdue_attempts == 1 );
    CHECK( history.casualty_attempts == 0 );
    REQUIRE( history.finish( 1, 200, "returned", {"actually-witnessed"}, false, {} ) );
    CHECK( history.overdue_attempts == 0 );
    CHECK( history.casualty_attempts == 1 );
    CHECK( history.caution_level() == 1 );
    const auto deadline = history.retry_after_active;
    CHECK_FALSE( history.finish( 1, 300, "returned", {"actually-witnessed"}, false, {} ) );
    CHECK( history.retry_after_active == deadline );
    REQUIRE( history.finish( 2, 400, "overdue_missing", {}, true, {} ) );
    CHECK( history.caution_level() == 2 );
    REQUIRE( history.finish( 2, 500, "searched_returned", {}, false, {} ) );
    CHECK( history.caution_level() == 1 ); // this late party actually returned, with no new casualty
    CHECK( history.casualty_attempts == 1 );
    CHECK( history.overdue_attempts == 0 );
    CHECK_FALSE( history.finish( 2, 600, "searched_returned", {}, false, {} ) );
}

TEST_CASE( "migo_actual_death_needs_physical_witness_then_home_communication",
           "[migo_tower][native][ownership][serialization]" )
{
    const bool unseen = GENERATE( false, true );
    clear_avatar();
    clear_map_with_vision( -1, 4, false );
    const auto old_turn = calendar::turn;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    on_out_of_scope restore( [&]() {
        clear_map_without_vision( -1, 4 );
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        set_time( old_turn );
    } );
    set_time_to_day();
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
    const auto home = here.get_abs( tripoint_bub_ms( 60, 60, 0 ) );
    const auto casualty_position = here.get_abs( tripoint_bub_ms( 90, 60, 0 ) );
    const auto anchor = project_to<coords::omt>( home );
    const std::string id = "witness-native-component";
    migo_population::site_record population;
    population.id = id;
    population.anchor = anchor;
    std::vector<migo_population::member_record> members;
    for( const auto &name : {"returnee", "casualty"} ) {
        migo_population::member_record member;
        member.token = {id, name, 1, anchor};
        member.type = mtype_id( "mon_mi_go" );
        member.home = home;
        member.hp = member.type->hp;
        member.owner = migo_population::representation::local;
        member.assignment = migo_population::duty::dispatched;
        members.push_back( member );
    }
    REQUIRE( population.commit_seed( members, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = anchor;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = project_to<coords::omt>( casualty_position );
    operation.sweep.candidates = {{casualty_position}};
    operation.members = {{"returnee"}, {"casualty"}};
    operation.overdue_after_active = 100000;
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    site.operation = operation;
    REQUIRE( site.remember_source( operation.source.source_omt ) );
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster *carrier = g->place_critter_at( members[0].type, tripoint_bub_ms( 89, 60, 0 ) );
    monster *casualty = g->place_critter_at( members[1].type, tripoint_bub_ms( 90, 60, 0 ) );
    REQUIRE( carrier );
    REQUIRE( casualty );
    carrier->population_identity() = members[0].token;
    casualty->population_identity() = members[1].token;
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.home = home;
    carrier->set_migo_intent( intent );
    casualty->set_migo_intent( intent );
    if( unseen ) {
        carrier->add_effect( efftype_id( "blind" ), 1_hours );
    }
    if( unseen ) {
        REQUIRE_FALSE( carrier->can_see() ); // native adjacent awareness is not a visual witness
    } else {
        REQUIRE( carrier->sees_without_clairvoyance_physical( here, *casualty ) );
    }
    // Explicit native fatal damage/death is component setup, not a claimed
    // combat victory. The real death hook owns terminal roster/witness facts.
    casualty->set_hp( 0 );
    casualty->die( &here, nullptr );
    casualty->die( &here, nullptr );
    auto *current = overmap_buffer.global_state.migo_towers.find( id );
    REQUIRE( current );
    CHECK( current->operation->communicated_losses.empty() );
    CHECK( carrier->migo_intent()->witnessed_losses.size() == ( unseen ? 0 : 1 ) );
    CHECK( overmap_buffer.global_state.migo_populations.find( id )->find( "casualty" )->owner ==
           migo_population::representation::dead );
    std::ostringstream saved_carrier;
    { JsonOut json( saved_carrier ); carrier->serialize( json ); }
    monster reloaded;
    reloaded.deserialize( json_loader::from_string( saved_carrier.str() ) );
    CHECK( reloaded.migo_intent()->witnessed_losses == carrier->migo_intent()->witnessed_losses );
    carrier->remove_effect( efftype_id( "blind" ) ); // restore sight after the unseen death; no new knowledge
    for( int turns = 0; turns < 100 && !current->operation->members[0].returned; ++turns ) {
        calendar::turn += 1_turns;
        process_monsters_and_npcs_turn_for_test();
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    }
    CHECK( current->operation->members[0].returned );
    CHECK( current->operation->resolved == !unseen );
    CHECK( current->operation->communicated_losses.size() == ( unseen ? 0 : 1 ) );
    CHECK( current->find_source( operation.source.source_omt )->casualty_attempts == ( unseen ? 0 : 1 ) );
}

TEST_CASE( "migo_actual_search_motor_excludes_fire_reopens_once_and_visits_connected_floor",
           "[migo_tower][raid_sweep][native][ownership]" )
{
    const bool contained = GENERATE( false, true );
    clear_avatar();
    clear_map_with_vision( -1, 4, false );
    const auto old_turn = calendar::turn;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    on_out_of_scope restore( [&]() {
        clear_map_without_vision( -1, 4 );
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        set_time( old_turn );
    } );
    set_time_to_day();
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
    const tripoint_bub_ms start( 60, 60, 0 );
    const tripoint_bub_ms first( 64, 60, 0 );
    const tripoint_bub_ms second( 66, 60, 0 );
    const tripoint_bub_ms fire( 64, 61, 0 );
    const tripoint_bub_ms stairs( 67, 60, 0 );
    const auto landing = stairs + tripoint_rel_ms( 0, 0, 1 );
    for( const auto &p : here.points_in_rectangle( tripoint_bub_ms( 64, 58, 1 ),
            tripoint_bub_ms( 70, 62, 1 ) ) ) {
        here.ter_set( p, ter_str_id( "t_floor" ) );
    }
    here.ter_set( stairs, ter_str_id( "t_stairs_up" ) );
    here.ter_set( landing, ter_str_id( "t_stairs_down" ) );
    if( contained ) {
        here.furn_set( fire, furn_str_id( "f_brazier" ) );
    }
    here.add_field( fire, field_type_id( "fd_fire" ), 1 );
    here.invalidate_map_cache( 0 );
    here.invalidate_map_cache( 1 );
    here.build_map_cache( 0 );
    here.build_map_cache( 1 );
    const auto anchor = project_to<coords::omt>( here.get_abs( start ) );
    const std::string id = "search-motor-component";
    migo_population::member_record member;
    member.token = {id, "real-body", 1, anchor};
    member.type = mtype_id( "mon_mi_go" );
    member.home = here.get_abs( start );
    member.owner = migo_population::representation::local;
    member.assignment = migo_population::duty::dispatched;
    member.hp = member.type->hp;
    migo_population::site_record population;
    population.id = id;
    population.anchor = anchor;
    REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = anchor;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.kind = "smoke";
    operation.source.source_omt = anchor;
    operation.source.source_ms = here.get_abs( fire );
    operation.members = {{member.token.member_id, false, false, here.get_abs( start )}};
    operation.sweep.source = anchor;
    operation.sweep.generation = operation.generation;
    operation.sweep.candidates = {{here.get_abs( first )}, {here.get_abs( second )}};
    operation.overdue_after_active = 100000;
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    site.operation = operation;
    REQUIRE( site.remember_source( anchor ) );
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster *body = g->place_critter_at( member.type, start );
    REQUIRE( body );
    body->population_identity() = member.token;
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.home = member.home;
    body->set_migo_intent( intent );
    // Lease/intent is explicit component setup; movement, hazard reads and
    // floor observation below use actual native callers and physical tiles.
    auto *current = overmap_buffer.global_state.migo_towers.find( id );
    REQUIRE( current );
    REQUIRE( migo_tower::owns_intent( *body, migo_population::representation::local ) );
    const auto approach = physical_navigation::read_step( *body, here.get_abs( first ) );
    INFO( approach.reason );
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 0 );
    calendar::turn += 1_turns;
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    if( contained ) {
        CHECK( approach.next != physical_navigation::action::none );
        CHECK( current->operation->sweep.candidates[0].state == raid_sweep::disposition::pending );
    } else {
        CHECK( approach.reason == "approach_fire_hazard" );
        CHECK( approach.hazard_revision != 0 );
        CHECK( current->operation->sweep.candidates[0].state == raid_sweep::disposition::hazardous );
        CHECK( current->operation->sweep.last_progress == 0 );
        here.remove_field( fire, field_type_id( "fd_fire" ) ); // declared physical reopening setup
        calendar::turn += 1_turns;
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        CHECK( current->operation->sweep.pending() == here.get_abs( first ) );
        CHECK( current->operation->sweep.candidates[0].reconsidered );
        here.add_field( fire, field_type_id( "fd_fire" ), 1 );
        calendar::turn += 1_turns;
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        CHECK( current->operation->sweep.candidates[0].state == raid_sweep::disposition::hazardous );
        here.remove_field( fire, field_type_id( "fd_fire" ) );
        calendar::turn += 1_turns;
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        CHECK( current->operation->sweep.pending() == here.get_abs( second ) );
    }
    bool arrived_upstairs = false;
    for( int turns = 0; turns < 40 && !arrived_upstairs; ++turns ) {
        calendar::turn += 1_turns;
        process_monsters_and_npcs_turn_for_test();
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        REQUIRE_FALSE( body->is_dead_state() );
        arrived_upstairs = std::any_of( current->operation->sweep.candidates.begin(),
        current->operation->sweep.candidates.end(), [&]( const auto &candidate ) {
            return candidate.position == here.get_abs( landing ) &&
                   candidate.state == raid_sweep::disposition::searched;
        } );
    }
    CHECK( arrived_upstairs );
    CHECK( body->pos_abs().z() == 1 );
    CHECK( current->operation->sweep.candidates.size() > 2 );
    CHECK( current->find_source( anchor )->observed_active == contained );
    std::ostringstream saved;
    { JsonOut json( saved ); current->serialize( json ); }
    migo_tower::site_state restored;
    restored.deserialize( json_loader::from_string( saved.str() ) );
    CHECK( restored.operation->sweep.cursor == current->operation->sweep.cursor );
    CHECK( restored.operation->sweep.candidates[0].reconsidered == !contained );
    CHECK( restored.find_source( anchor )->observed_active == contained );
}

TEST_CASE( "migo issued sweep approach responds to newly burning intermediate ground",
           "[migo_tower][raid_sweep][native][ownership]" )
{
    clear_avatar();
    clear_creatures();
    clear_map_with_vision( -1, 1, false );
    const auto old_turn = calendar::turn;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    on_out_of_scope restore( [&]() {
        clear_creatures();
        clear_map_without_vision( -1, 1 );
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        set_time( old_turn );
    } );
    set_time_to_day();
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
    const tripoint_bub_ms start( 55, 60, 0 );
    const tripoint_bub_ms gap( 61, 60, 0 );
    const tripoint_bub_ms goal( 68, 60, 0 );
    for( int y = 0; y < here.getmapsize() * SEEY; ++y ) {
        here.ter_set( tripoint_bub_ms( gap.x(), y, 0 ), ter_str_id( "t_wall" ) );
    }
    here.ter_set( gap, ter_str_id( "t_floor" ) );
    const auto anchor = project_to<coords::omt>( here.get_abs( start ) );
    migo_population::member_record member;
    member.token = {"new-approach-fire", "member", 1, anchor};
    member.type = mtype_id( "mon_mi_go" );
    member.home = here.get_abs( start );
    member.owner = migo_population::representation::local;
    member.assignment = migo_population::duty::dispatched;
    member.hp = member.type->hp;
    migo_population::site_record population;
    population.id = member.token.site_id;
    population.anchor = anchor;
    REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = population.id;
    site.anchor = anchor;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = anchor;
    operation.members = {{member.token.member_id, false, false, member.home}};
    operation.sweep.source = anchor;
    operation.sweep.generation = 1;
    operation.sweep.candidates = {{here.get_abs( goal )}, {here.get_abs( start + point::south * 5 )}};
    operation.overdue_after_active = 100000;
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    site.operation = operation;
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster *body = g->place_critter_at( member.type, start );
    REQUIRE( body );
    body->population_identity() = member.token;
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.home = member.home;
    body->set_migo_intent( intent );
    auto *current = overmap_buffer.global_state.migo_towers.find( population.id );
    REQUIRE( current );
    REQUIRE( migo_tower::owns_intent( *body, migo_population::representation::local ) );
    const auto initial = migo_tower::prepare_step( *body, migo_population::representation::local );
    REQUIRE( initial.next == physical_navigation::action::walk );
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 0 );
    const auto progress = current->operation->sweep.last_progress;
    const auto original_position = body->pos_abs();
    here.add_field( gap, field_type_id( "fd_fire" ), 3 );
    calendar::turn += 1_turns;
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    CHECK( here.get_field_intensity( goal, field_type_id( "fd_fire" ) ) == 0 );
    CHECK( current->operation->sweep.candidates.front().state == raid_sweep::disposition::hazardous );
    CHECK( current->operation->sweep.candidates.front().geometry_revision != 0 );
    CHECK( current->operation->sweep.last_progress == progress );
    CHECK( body->pos_abs() == original_position );
    const auto cursor = current->operation->sweep.cursor;
    const auto retry_deadline = current->operation->sweep.retry_after_active;
    calendar::turn += 1_turns;
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    CHECK( current->operation->sweep.cursor == cursor );
    CHECK( current->operation->sweep.candidates.front().state == raid_sweep::disposition::hazardous );
    CHECK( current->operation->sweep.last_progress == progress );
    CHECK( current->operation->sweep.retry_after_active == retry_deadline );
    here.remove_field( gap, field_type_id( "fd_fire" ) );
    // Removal marks the field dead; native field cleanup then erases it.
    here.delete_field( gap, field_type_id( "fd_fire" ) );
    here.set_pathfinding_cache_dirty( gap );
    const auto reopened = physical_navigation::read_step( *body, here.get_abs( goal ) );
    INFO( reopened.reason );
    REQUIRE( reopened.next == physical_navigation::action::walk );
    calendar::turn += 1_turns;
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    CHECK( current->operation->sweep.pending() == here.get_abs( goal ) );
    CHECK( current->operation->sweep.candidates.front().reconsidered );
}

TEST_CASE( "migo_queued_native_migrations_preserve_rejected_source_owners",
           "[migo_tower][native][ownership]" )
{
    const bool stairs = GENERATE( false, true );
    const int permutation = GENERATE( 0, 1, 2, 3, 4, 5 );
    CAPTURE( stairs, permutation );
    const auto old_turn = calendar::turn;
    const auto activation_avatar = get_avatar().pos_abs();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto remote_om = project_to<coords::om>( get_avatar().pos_abs() ).xy() + point( 2, 0 );
    overmap &owner = overmap_buffer.get( remote_om );
    const tripoint_abs_omt cell( project_to<coords::omt>( remote_om ) + point( 2, 2 ), 0 );
    const auto start = project_to<coords::ms>( cell ) + point( 12, 12 );
    const auto goal = start + point::east;
    const std::string id = "native-queue-conservation";
    std::vector<oter_id> old_terrain;
    for( int z = 0; z <= 1; ++z ) {
        const tripoint_abs_omt p( cell.xy(), z );
        old_terrain.push_back( overmap_buffer.ter_existing( p ) );
        overmap_buffer.ter_set( p, oter_str_id( "field" ) );
        tinymap terrain;
        terrain.load( p, false );
        for( const auto &tile : terrain.points_in_rectangle( tripoint_omt_ms( 0, 0, z ),
                tripoint_omt_ms( 23, 23, z ) ) ) {
            terrain.ter_set( tile, ter_str_id( "t_floor" ) );
            terrain.furn_set( tile, furn_str_id::NULL_ID() );
        }
        if( stairs ) {
            terrain.ter_set( tripoint_omt_ms( 12, 12, z ),
                             ter_str_id( z == 0 ? "t_stairs_up" : "t_stairs_down" ) );
        }
    }
    on_out_of_scope restore( [&]() {
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data &&
                entry.second->monster_data->population_identity().site_id == id ) {
                CHECK( owner.release_monster_payload( entry.first,
                       entry.second->monster_data->population_identity().member_id ) );
            }
        }
        for( int z = 0; z <= 1; ++z ) {
            overmap_buffer.ter_set( tripoint_abs_omt( cell.xy(), z ), old_terrain[z] );
        }
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        MAPBUFFER.clear_outside_reality_bubble();
        get_avatar().setpos( activation_avatar, false );
        set_time( old_turn );
    } );
    // This is an off-bubble motor control, not an inactive-party control.
    // Keep the real geometry remote while admitting the entire short journey
    // within40; the separate activation controls prove the outside boundary.
    get_avatar().setpos( project_to<coords::ms>( project_to<coords::omt>( start ) - point( 30, 0 ) ),
                         false );

    // Leases are component setup. The real scheduler prepares overlapping
    // physical moves; a nonparticipating resident blocks the queue's head.
    const std::vector<tripoint_abs_ms> positions = {
        start,
        stairs ? start + tripoint( 0, 0, 1 ) : start + point::west,
        stairs ? start + tripoint( -1, 0, 1 ) : start + point( -2, 0 ),
        goal,
        start + point( 3, 3 )
    };
    migo_population::site_record population;
    population.id = id;
    population.anchor = cell;
    std::vector<migo_population::member_record> members;
    for( size_t i = 0; i < positions.size(); ++i ) {
        migo_population::member_record member;
        member.token = {id, "queue-" + std::to_string( i ), 1, cell};
        member.type = mtype_id( "mon_mi_go" );
        member.home = positions[i];
        member.owner = migo_population::representation::abstract;
        member.assignment = i < 3 ? migo_population::duty::dispatched :
                            migo_population::duty::available;
        member.hp = member.type->hp;
        members.push_back( member );
    }
    REQUIRE( population.commit_seed( members, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = cell;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = cell;
    operation.sweep.candidates = {{goal}};
    operation.overdue_after_active = 100000;
    for( int i = 0; i < 3; ++i ) {
        operation.members.push_back( {members[i].token.member_id, false, false, positions[i]} );
    }
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    site.operation = operation;
    REQUIRE( site.remember_source( cell ) );
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    std::vector<int> insertion_order = {0, 1, 2};
    for( int i = 0; i < permutation; ++i ) {
        std::next_permutation( insertion_order.begin(), insertion_order.end() );
    }
    insertion_order.push_back( 3 );
    insertion_order.push_back( 4 );
    for( const int i : insertion_order ) {
        monster body( members[i].type );
        body.population_identity() = members[i].token;
        body.set_pos_abs_only( positions[i] );
        body.set_moves( 0 );
        if( i < 3 ) {
            migo_tower::member_intent intent;
            intent.generation = 1;
            intent.home = positions[i];
            body.set_migo_intent( intent );
        }
        REQUIRE( owner.retain_monster_payload( positions[i], body ) );
    }
    auto check_conservation = [&]() {
        std::multiset<std::string> ids;
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data &&
                entry.second->monster_data->population_identity().site_id == id ) {
                ids.insert( entry.second->monster_data->population_identity().member_id );
                CHECK( entry.second->monster_data->population_identity().epoch == 1 );
                CHECK( entry.second->monster_data->get_hp() == members[0].hp );
            }
        }
        REQUIRE( ids.size() == members.size() );
        for( const auto &member : members ) {
            REQUIRE( ids.count( member.token.member_id ) == 1 );
        }
    };
    for( int turn = 0; turn < 3; ++turn ) {
        calendar::turn += 1_turns;
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        overmap_buffer.move_hordes();
        check_conservation();
    }
    for( const auto &entry : owner.horde_entities() ) {
        if( entry.second->monster_data &&
            entry.second->monster_data->population_identity().site_id == id ) {
            const auto &body = *entry.second->monster_data;
            const auto member = std::find_if( members.begin(), members.end(), [&]( const auto &m ) {
                return m.token.member_id == body.population_identity().member_id;
            } );
            REQUIRE( member != members.end() );
            CHECK( entry.first == member->home );
            CHECK( entry.second->moves == 0 );
            if( body.migo_intent() ) {
                CHECK( body.migo_intent()->last_energy_world == -1 );
                CHECK( entry.second->last_processed == calendar::turn );
            }
        }
    }
    // Use the actual overmap writer/reader, including the scheduler guard and
    // exact body budgets. Reloading cannot spend another same-world interval.
    std::ostringstream saved;
    owner.serialize( saved );
    owner.hordes.clear();
    std::istringstream input( saved.str() );
    owner.unserialize( input );
    check_conservation();
    overmap_buffer.move_hordes();
    check_conservation();
    for( const auto &entry : owner.horde_entities() ) {
        if( entry.second->monster_data &&
            entry.second->monster_data->population_identity().site_id == id ) {
            CHECK( entry.second->moves == 0 );
        }
    }
    // Move the controlled blocker, retaining its same native token. Once the
    // physical destination is free the conserved queue must make progress.
    monster blocker( members[3].type );
    blocker.population_identity() = members[3].token;
    blocker.set_pos_abs_only( goal + point( 2, 2 ) );
    blocker.set_moves( 0 );
    REQUIRE( owner.release_monster_payload( goal, members[3].token.member_id ) );
    REQUIRE( owner.retain_monster_payload( blocker.pos_abs(), blocker ) );
    bool advanced = false;
    for( int turn = 0; turn < 20; ++turn ) {
        calendar::turn += 1_turns;
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        overmap_buffer.move_hordes();
        check_conservation();
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data &&
                entry.second->monster_data->population_identity().site_id == id &&
                entry.second->monster_data->population_identity().member_id == members[0].token.member_id ) {
                advanced |= entry.first != start;
            }
        }
    }
    CHECK( advanced );
}

TEST_CASE( "migo_operation_scheduler_crosses_remote_overmaps_without_servicing_other_hordes",
           "[migo_tower][native][ownership][migo_movement_acquisition]" )
{
    const bool deferred_geometry = GENERATE( false, true );
    // A leased native payload is component setup. The public scheduler owns
    // every physical step, cross-overmap insertion and actual home settlement.
    const auto old_turn = calendar::turn;
    const auto activation_avatar = get_avatar().pos_abs();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto first_om = project_to<coords::om>( get_avatar().pos_abs() ).xy() + point( 2, 0 );
    const auto second_om = first_om + point::east;
    overmap &first_owner = overmap_buffer.get( first_om );
    overmap &second_owner = overmap_buffer.get( second_om );
    const tripoint_abs_omt first_cell( project_to<coords::omt>( first_om ) +
                                     point( OMAPX - 1, deferred_geometry ? 6 : 2 ), 0 );
    const auto second_cell = first_cell + point::east;
    const auto old_first = overmap_buffer.ter_existing( first_cell );
    const auto old_second = overmap_buffer.ter_existing( second_cell );
    const auto start = project_to<coords::ms>( first_cell ) + point( 12, 12 );
    const auto goal = start + point( 18, 0 );
    // This body has its own authenticated predator scheduler. Keep it in
    // the same resident container but outside its independent 40-OMT range;
    // the Mi-go-only pass must not wake it along with the leased party.
    const auto unrelated_position = start + point( 0, 41 * 2 * SEEY );
    const std::string id = "remote-operation-component";
    std::string unrelated_id;
    on_out_of_scope restore( [&]() {
        for( auto *owner : {&first_owner, &second_owner} ) {
            for( const auto &entry : owner->horde_entities() ) {
                if( entry.second->monster_data &&
                    ( entry.second->monster_data->population_identity().site_id == id ||
                      ( !unrelated_id.empty() &&
                        entry.second->monster_data->handoff_actor_id() == unrelated_id ) ) ) {
                    owner->release_monster_payload( entry.first,
                                                    entry.second->monster_data->handoff_actor_id() );
                }
            }
        }
        overmap_buffer.ter_set( first_cell, old_first );
        overmap_buffer.ter_set( second_cell, old_second );
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        MAPBUFFER.clear_outside_reality_bubble();
        get_avatar().setpos( activation_avatar, false );
        set_time( old_turn );
    } );
    // This is an off-bubble motor control, not an inactive-party control.
    // Keep the real geometry remote while admitting the entire short journey
    // within40; the separate activation controls prove the outside boundary.
    // This start lies at the EAST edge of its overmap. Put the avatar
    // across that boundary, so the first owner remains fallback-only while
    // its actual party is within40. A west-side avatar in the same overmap
    // would admit unrelated hordes through the ordinary nearby owner pass.
    get_avatar().setpos( project_to<coords::ms>( project_to<coords::omt>( start ) + point( 30, 0 ) ),
                         false );
    REQUIRE( project_to<coords::om>( unrelated_position ).xy() == first_om );
    REQUIRE( rl_dist( get_avatar().pos_abs_omt().xy(),
                      project_to<coords::omt>( unrelated_position ).xy() ) > 40 );

    const auto prepare_cell = [&]( const tripoint_abs_omt &cell ) {
        tinymap terrain;
        terrain.load( cell, false );
        for( const auto &p : terrain.points_in_rectangle( tripoint_omt_ms( 0, 0, 0 ),
                tripoint_omt_ms( 23, 23, 0 ) ) ) {
            terrain.ter_set( p, ter_str_id( "t_grass" ) );
            terrain.furn_set( p, furn_str_id::NULL_ID() );
        }
    };
    for( const auto &cell : {first_cell, second_cell} ) {
        overmap_buffer.ter_set( cell, oter_str_id( "field" ) );
        if( cell != second_cell || !deferred_geometry ) {
            prepare_cell( cell );
        }
    }
    migo_population::site_record population;
    population.id = id;
    population.anchor = first_cell;
    migo_population::member_record member;
    member.token = {id, "one-owner", 1, first_cell};
    member.type = mtype_id( "mon_mi_go" );
    member.home = start;
    member.owner = migo_population::representation::abstract;
    member.assignment = migo_population::duty::dispatched;
    member.hp = member.type->hp;
    REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = first_cell;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = second_cell;
    operation.sweep.candidates = {{goal}};
    operation.members = {{member.token.member_id, false, false, start}};
    operation.overdue_after_active = 100000;
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    site.operation = operation;
    REQUIRE( site.remember_source( second_cell ) );
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster body( member.type );
    body.population_identity() = member.token;
    body.set_pos_abs_only( start );
    body.set_moves( 0 );
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.home = start;
    body.set_migo_intent( intent );
    REQUIRE( first_owner.retain_monster_payload( start, body ) );
    monster unrelated( mtype_id( "mon_writhing_stalker" ) );
    unrelated.ensure_predator_state();
    unrelated_id = unrelated.handoff_actor_id();
    REQUIRE_FALSE( unrelated_id.empty() );
    unrelated.set_pos_abs_only( unrelated_position );
    unrelated.set_dest( unrelated_position + point::east );
    auto *retained = first_owner.retain_monster_payload( unrelated_position, unrelated );
    REQUIRE( retained );
    REQUIRE( retained->is_active() );
    const auto unrelated_processed = retained->last_processed;
    const auto *predator_owner = overmap_buffer.global_state.predator_activity.find( unrelated_id );
    INFO( "unrelated predator confirmed=" << ( predator_owner && predator_owner->confirmed )
          << " distance_omt=" << rl_dist( get_avatar().pos_abs_omt().xy(),
                                        project_to<coords::omt>( unrelated_position ).xy() ) );
    auto *current = overmap_buffer.global_state.migo_towers.find( id );
    REQUIRE( current );
    if( deferred_geometry ) {
        // A read still cannot manufacture physical facts. The actual active
        // scheduler must obtain its own adjacent geometry through native
        // mapgen instead of requiring a player visit or fixture map fill.
        const auto unknown = physical_navigation::read_step( body, goal );
        REQUIRE_FALSE( unknown.known );
        CHECK( unknown.reason == "segment_geometry_unknown" );
        CHECK_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( goal ) ) );
    }
    bool crossed = false;
    bool moved = false;
    for( int turns = 0; turns < 80 && !current->operation->resolved; ++turns ) {
        calendar::turn += 1_turns;
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        overmap_buffer.move_hordes();
        int owners = 0;
        for( auto *owner : {&first_owner, &second_owner} ) {
            for( const auto &entry : owner->horde_entities() ) {
                if( !entry.second->monster_data ) {
                    continue;
                }
                const monster &payload = *entry.second->monster_data;
                if( payload.population_identity().site_id == id ) {
                    ++owners;
                    moved |= entry.first != start;
                    crossed |= project_to<coords::om>( entry.first ).xy() == second_om;
                    CHECK( payload.population_identity().epoch == 1 );
                    CHECK( payload.get_hp() == member.hp );
                    const auto position = entry.first;
                    const auto budget = entry.second->moves;
                    overmap_buffer.move_hordes();
                    CHECK( entry.first == position );
                    CHECK( entry.second->moves == budget );
                }
                if( payload.handoff_actor_id() == unrelated_id ) {
                    CHECK( entry.first == unrelated_position );
                    CHECK( entry.second->last_processed == unrelated_processed );
                }
            }
        }
        CHECK( owners == 1 );
    }
    CHECK( moved );
    CHECK( crossed );
    CHECK( current->operation->resolved );
    CHECK( current->operation->members[0].returned );
    const auto *returned = overmap_buffer.global_state.migo_populations.find( id )->find( "one-owner" );
    REQUIRE( returned );
    CHECK( returned->assignment == migo_population::duty::available );
    CHECK( returned->owner == migo_population::representation::abstract );
}

TEST_CASE( "migo_remote_scheduler_reacquires_members_after_native_site_generation",
           "[migo_tower][native][ownership][migo_movement_owner_lifetime]" )
{
    const std::string kind = GENERATE( "operation", "rally", "operation_then_rally" );
    CAPTURE( kind );
    const auto old_turn = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto first_om = project_to<coords::om>( old_avatar ).xy() + point( 2, 2 );
    const tripoint_abs_omt first_cell( project_to<coords::omt>( first_om ) +
                                     point( OMAPX - 1, OMAPY - 12 ), 0 );
    const auto tower_cell = first_cell + point::east;
    const auto second_cell = first_cell + point( 0, 24 );
    const std::array<tripoint_abs_ms, 2> starts = {
        project_to<coords::ms>( first_cell ) + point( 23, 12 ),
        project_to<coords::ms>( second_cell ) + point( 12, 12 )
    };
    const std::array<tripoint_abs_ms, 2> goals = {
        starts[0] + point( 48, 0 ), starts[1] + point( 6, 0 )
    };
    const std::string id = "native-generation-outer-owner";
    std::vector<std::pair<tripoint_abs_omt, oter_id>> terrain;
    std::set<character_id> old_npcs;
    for( int z = 0; z < 4; ++z ) {
        for( const auto &npc : overmap_buffer.get_npcs_near_omt(
                 tripoint_abs_omt( tower_cell.xy(), z ), 0 ) ) {
            old_npcs.insert( npc->getID() );
        }
    }
    on_out_of_scope restore( [&]() {
        for( const auto &cell : {first_cell, second_cell, tower_cell} ) {
            if( auto *owner = overmap_buffer.find_loaded_overmap( project_to<coords::om>( cell ).xy() ) ) {
                for( const auto &entry : owner->horde_entities() ) {
                    if( entry.second->monster_data &&
                        entry.second->monster_data->population_identity().site_id == id ) {
                        owner->release_monster_payload( entry.first,
                                                       entry.second->monster_data->population_identity().member_id );
                    }
                }
            }
        }
        for( int z = 0; z < 4; ++z ) {
            for( const auto &npc : overmap_buffer.get_npcs_near_omt(
                     tripoint_abs_omt( tower_cell.xy(), z ), 0 ) ) {
                if( !old_npcs.count( npc->getID() ) ) {
                    overmap_buffer.remove_npc( npc->getID() );
                }
            }
        }
        for( const auto &entry : terrain ) {
            overmap_buffer.ter_set( entry.first, entry.second );
        }
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        MAPBUFFER.clear_outside_reality_bubble();
        get_avatar().setpos( old_avatar, false );
        set_time( old_turn );
    } );
    for( int z = 0; z < 4; ++z ) {
        for( const auto &cell : {first_cell, second_cell, tower_cell} ) {
            const tripoint_abs_omt layer( cell.xy(), z );
            terrain.emplace_back( layer, overmap_buffer.ter( layer ) );
            for( int x = 0; x < 2; ++x ) {
                for( int y = 0; y < 2; ++y ) {
                    REQUIRE_FALSE( MAPBUFFER.lookup_submap_existing( project_to<coords::sm>( layer ) + point( x, y ) ) );
                }
            }
            overmap_buffer.ter_set( layer, cell == tower_cell ?
                                   oter_str_id( "mi-go_scout_tower_" + std::to_string( z + 1 ) + "_north" ) :
                                   oter_str_id( z == 0 ? "field" : "open_air" ) );
        }
    }
    const auto generated_geometry = migo_population::geometry_at( tower_cell );
    REQUIRE( generated_geometry );
    // Both source containers are outside the ordinary nearby owner pass,
    // while every real body is inside its existing 40-OMT activation bound.
    get_avatar().setpos( project_to<coords::ms>( first_cell + point( 25, 20 ) ), false );
    for( const auto &cell : {first_cell, second_cell} ) {
        REQUIRE( rl_dist( get_avatar().pos_abs_omt().xy(), cell.xy() ) < 40 );
        REQUIRE( project_to<coords::om>( cell ).xy() != project_to<coords::om>( get_avatar().pos_abs() ).xy() );
        tinymap source;
        source.load( cell, false );
        for( const auto &p : source.points_in_rectangle( tripoint_omt_ms( 0, 0, 0 ),
                tripoint_omt_ms( 23, 23, 0 ) ) ) {
            source.ter_set( p, ter_str_id( "t_grass" ) );
            source.furn_set( p, furn_str_id::NULL_ID() );
        }
    }
    migo_population::site_record population;
    population.id = id;
    population.anchor = first_cell;
    std::vector<migo_population::member_record> members;
    for( int i = 0; i < 2; ++i ) {
        migo_population::member_record member;
        member.token = {id, "outer-member-" + std::to_string( i ), 1, first_cell};
        member.type = mtype_id( "mon_mi_go" );
        member.home = goals[i];
        member.owner = migo_population::representation::abstract;
        member.assignment = kind == "rally" || ( kind == "operation_then_rally" && i == 1 ) ?
                            migo_population::duty::returning : migo_population::duty::dispatched;
        member.hp = member.type->hp;
        members.push_back( member );
    }
    REQUIRE( population.commit_seed( members, std::nullopt, "", 0, true, false ) );
    // A genuine one-home fixture ensures the next real native registration
    // must grow its owner vector. No stale pointer is probed by this control.
    migo_population::world_state initial;
    initial.sites.push_back( population );
    overmap_buffer.global_state.migo_populations = std::move( initial );
    auto &world = overmap_buffer.global_state.migo_populations;
    REQUIRE( world.sites.size() == world.sites.capacity() );
    const auto initial_capacity = world.sites.capacity();
    overmap_buffer.global_state.migo_towers.clear();
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = first_cell;
    if( kind != "rally" ) {
        site.operation.emplace();
        site.operation->generation = 1;
        site.operation->source.source_omt = tower_cell;
        site.operation->sweep.candidates = {{goals[0]}};
        site.operation->overdue_after_active = 100000;
    }
    if( kind != "operation" ) {
        site.response_home_rally.emplace();
        site.response_home_rally->source = tower_cell;
        site.response_home_rally->episode = 1;
        site.response_home_rally->generation = 2;
    }
    for( const auto &member : members ) {
        if( member.assignment == migo_population::duty::returning ) {
            raid_sweep::home_rally_member recalled;
            recalled.member_id = member.token.member_id;
            recalled.allowance_turns = 100000;
            recalled.no_progress_turns = 100000;
            REQUIRE( site.response_home_rally->add( recalled ) );
        } else {
            site.operation->members.push_back( {member.token.member_id} );
        }
    }
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    std::array<std::int64_t, 2> uids;
    for( int i = 0; i < 2; ++i ) {
        monster body( members[i].type );
        body.population_identity() = members[i].token;
        body.set_pos_abs_only( starts[i] );
        body.set_moves( 0 );
        body.add_item( item( itype_id( "rock" ), calendar::turn ) );
        migo_tower::member_intent intent;
        intent.generation = members[i].assignment == migo_population::duty::returning ? 2 : 1;
        intent.phase = members[i].assignment == migo_population::duty::returning ?
                       migo_tower::member_phase::returning : migo_tower::member_phase::outbound;
        intent.home = members[i].home;
        body.set_migo_intent( intent );
        auto &owner = overmap_buffer.get( project_to<coords::om>( starts[i] ).xy() );
        auto *entry = owner.retain_monster_payload( starts[i], body );
        REQUIRE( entry );
        uids[i] = entry->monster_data->inv.front().uid().get_value();
    }
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    overmap_buffer.move_hordes(); // real zero-credit adoption
    REQUIRE_FALSE( world.find( generated_geometry->id ) );
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( tower_cell ) ) );
    calendar::turn += 1_turns;
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    overmap_buffer.move_hordes(); // first member generates the real adjacent tower
    REQUIRE( world.sites.capacity() > initial_capacity );
    const auto *generated = world.find( generated_geometry->id );
    REQUIRE( generated );
    CHECK( generated->new_content );
    CHECK_FALSE( generated->seed_committed );
    CHECK( generated->members.empty() );
    int native_slots = 0;
    for( int z = 0; z < 4; ++z ) {
        for( int x = 0; x < 2; ++x ) {
            for( int y = 0; y < 2; ++y ) {
                const auto *sm = MAPBUFFER.lookup_submap_cached( project_to<coords::sm>(
                                     tripoint_abs_omt( tower_cell.xy(), z ) ) + point( x, y ) );
                REQUIRE( sm );
                for( const auto &slot : sm->spawns ) {
                    native_slots += slot.population.site_id == generated_geometry->id;
                }
            }
        }
    }
    CHECK( native_slots == 8 );
    std::array<std::string, 2> once;
    const auto serialized = []( const monster &body ) {
        std::ostringstream bytes;
        JsonOut out( bytes );
        body.serialize( out );
        return bytes.str();
    };
    const auto actual_body = [&]( int i ) -> const monster * {
        const auto *home = world.find( id );
        const auto *member = home ? home->find( members[i].token.member_id ) : nullptr;
        if( !member || !member->current_position ) {
            return nullptr;
        }
        auto *owner = overmap_buffer.find_loaded_overmap( project_to<coords::om>( *member->current_position ).xy() );
        const auto *entry = owner ? owner->entity_at( project_remain<coords::om>( *member->current_position ).remainder_tripoint ) : nullptr;
        return entry ? entry->monster_data.get() : nullptr;
    };
    for( int i = 0; i < 2; ++i ) {
        const auto *body = actual_body( i );
        REQUIRE( body );
        CHECK( body->get_hp() == members[i].hp );
        CHECK( body->inv.front().uid().get_value() == uids[i] );
        CHECK( body->population_identity().epoch == 1 );
        if( i == 1 ) {
            CHECK( body->pos_abs() != starts[i] );
            CHECK( body->migo_intent()->last_energy_world == to_turn<int>( calendar::turn ) );
        }
        once[i] = serialized( *body );
    }
    overmap_buffer.move_hordes();
    CHECK( world.sites.size() == 2 );
    for( int i = 0; i < 2; ++i ) {
        REQUIRE( actual_body( i ) );
        CHECK( serialized( *actual_body( i ) ) == once[i] );
    }
}

TEST_CASE( "migo_canonical_movement_acquires_only_its_immediate_native_frontier",
           "[migo_tower][native][ownership][migo_movement_acquisition]" )
{
    const std::string mode = GENERATE( "active", "reload", "service", "copy", "foreign",
                                      "frozen", "loaded_duplicate", "blocked", "vehicle", "load_only" );
    CAPTURE( mode );
    const auto old_turn = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const tripoint_abs_omt cell( get_avatar().pos_abs_omt().xy() + point( 20, 10 ), 0 );
    const auto adjacent = cell + point::east;
    const auto distant = cell + point( 2, 0 );
    const auto old_cell = overmap_buffer.ter_existing( cell );
    const auto old_adjacent = overmap_buffer.ter_existing( adjacent );
    const auto old_distant = overmap_buffer.ter_existing( distant );
    const auto origin = project_to<coords::ms>( cell ) + point( 23, 12 );
    const auto goal = origin + point( 48, 0 );
    const auto edge = origin + point::east;
    const std::string id = "native-frontier-component";
    overmap &owner = overmap_buffer.get( project_to<coords::om>( origin ).xy() );
    monster *duplicate = nullptr;
    on_out_of_scope restore( [&]() {
        if( duplicate ) {
            g->remove_zombie( *duplicate );
        }
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data &&
                entry.second->monster_data->population_identity().site_id == id ) {
                owner.release_monster_payload( entry.first,
                                               entry.second->monster_data->population_identity().member_id );
            }
        }
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.ter_set( cell, old_cell );
        overmap_buffer.ter_set( adjacent, old_adjacent );
        overmap_buffer.ter_set( distant, old_distant );
        MAPBUFFER.clear_outside_reality_bubble();
        get_avatar().setpos( old_avatar, false );
        set_time( old_turn );
    } );
    for( const auto &p : {cell, adjacent, distant} ) {
        overmap_buffer.ter_set( p, oter_str_id( "field" ) );
    }
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_existing( project_to<coords::sm>( edge ) ) );
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_existing( project_to<coords::sm>( goal ) ) );
    tinymap source;
    source.load( cell, true );
    for( const auto &p : source.points_in_rectangle( tripoint_omt_ms( 0, 0, cell.z() ),
            tripoint_omt_ms( 23, 23, cell.z() ) ) ) {
        source.ter_set( p, ter_str_id( "t_grass" ) );
        source.furn_set( p, furn_str_id::NULL_ID() );
    }
    if( mode == "vehicle" ) {
        REQUIRE( source.add_vehicle( vproto_id( "bicycle" ), tripoint_omt_ms( 15, 15, 0 ),
                                     0_degrees, 0, veh_spawn_status::UNDAMAGED ) );
    }
    // A full existing upper layer must survive generation of the missing
    // ground layer. This is physical saved/resident override, not fake air.
    std::vector<std::pair<tripoint_abs_sm, submap *>> upper;
    for( int x = 0; x < 2; ++x ) {
        for( int y = 0; y < 2; ++y ) {
            const auto p = project_to<coords::sm>( adjacent ) + tripoint( x, y, 1 );
            REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( p ) );
            auto sm = std::make_unique<submap>();
            sm->set_all_ter( ter_str_id( "t_floor" ) );
            sm->last_touched = calendar::turn - 7_turns;
            upper.emplace_back( p, sm.get() );
            REQUIRE( MAPBUFFER.add_submap( p, sm ) );
        }
    }
    migo_population::site_record population;
    population.id = id;
    population.anchor = cell;
    migo_population::member_record member;
    member.token = {id, "frontier-owner", 1, cell};
    member.type = mtype_id( "mon_mi_go" );
    member.home = origin;
    member.owner = migo_population::representation::abstract;
    member.assignment = migo_population::duty::dispatched;
    member.hp = member.type->hp;
    REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = cell;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = distant;
    operation.sweep.candidates = {{goal}};
    operation.members = {{member.token.member_id, false, false, origin}};
    operation.overdue_after_active = 100000;
    site.operation = operation;
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster payload( member.type );
    payload.population_identity() = member.token;
    payload.set_pos_abs_only( origin );
    payload.set_moves( 0 );
    payload.add_item( item( itype_id( "rock" ), calendar::turn ) );
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.home = origin;
    payload.set_migo_intent( intent );
    REQUIRE( owner.retain_monster_payload( origin, payload ) );
    const auto retained = [&]() -> monster * {
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data &&
                entry.second->monster_data->population_identity().member_id == member.token.member_id ) {
                return entry.second->monster_data.get();
            }
        }
        return nullptr;
    };
    auto *body = retained();
    REQUIRE( body );
    const auto uid = body->inv.front().uid().get_value();
    const auto hp = body->get_hp();
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    // Public zero-credit adoption is a real owner opportunity, not a clock
    // injection. It cannot generate, fund, or advance a physical step.
    overmap_buffer.move_hordes();
    body = retained();
    REQUIRE( body );
    CHECK( body->pos_abs() == origin );
    CHECK( body->migo_intent()->last_energy_world == -1 );
    CHECK_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( edge ) ) );
    calendar::turn += 1_turns;
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    CHECK_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( edge ) ) );
    const auto serialize_body = []( const monster &value ) {
        std::ostringstream bytes;
        JsonOut out( bytes );
        value.serialize( out );
        return bytes.str();
    };
    const auto before = serialize_body( *body );
    if( mode == "copy" || mode == "service" ) {
        monster candidate( *body );
        const auto proposal = mode == "copy" ?
                              migo_tower::prepare_step( candidate, migo_population::representation::abstract ) :
                              physical_navigation::read_step( candidate, goal );
        CHECK( proposal.next == physical_navigation::action::none );
        CHECK_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( edge ) ) );
        CHECK( serialize_body( *body ) == before );
        return;
    }
    if( mode == "load_only" ) {
        // Supplemental acquisition-owner conservation: obtaining real facts
        // is not an action, body settlement, arrival or energy grant.
        REQUIRE( migo_tower::owns_intent( *body, migo_population::representation::abstract ) );
        monster candidate( *body );
        auto proposal = migo_tower::prepare_step( candidate, migo_population::representation::abstract );
        REQUIRE( proposal.missing_geometry );
        const auto request = *proposal.missing_geometry;
        REQUIRE( rl_dist( request, origin ) <= 1 );
        REQUIRE( physical_navigation::acquire_movement_geometry( origin, proposal ) );
        CHECK( serialize_body( *body ) == before );
        CHECK_FALSE( physical_navigation::acquire_movement_geometry( origin, proposal ) );
        proposal.missing_geometry = goal;
        CHECK_FALSE( physical_navigation::acquire_movement_geometry( origin, proposal ) );
        CHECK_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( goal ) ) );
        CHECK( overmap_buffer.global_state.migo_towers.find( id )->operation->sweep.cursor == 0 );
        return;
    }
    if( mode == "foreign" ) {
        ++body->population_identity().epoch;
    } else if( mode == "frozen" ) {
        get_avatar().setpos( origin + point( 41 * 24, 0 ), false );
        migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    } else if( mode == "loaded_duplicate" ) {
        duplicate = g->place_critter_around( make_shared_fast<monster>( *body ),
                                            get_player_character().pos_bub() + point( 2, 2 ), 1 );
        REQUIRE( duplicate );
    } else if( mode == "blocked" ) {
        tinymap wall;
        wall.load( adjacent, true );
        for( const auto &p : wall.points_in_rectangle( tripoint_omt_ms( 0, 0, cell.z() ),
                tripoint_omt_ms( 23, 23, cell.z() ) ) ) {
            wall.ter_set( p, ter_str_id( "t_wall" ) );
            wall.furn_set( p, furn_str_id::NULL_ID() );
        }
    }
    std::set<tripoint_abs_sm> buffered;
    for( const auto &entry : MAPBUFFER ) {
        buffered.insert( entry.first );
    }
    const auto refused_source = serialize_body( *body );
    overmap_buffer.move_hordes();
    body = retained();
    REQUIRE( body );
    const bool positive = mode == "active" || mode == "reload";
    CHECK( body->get_hp() == hp );
    CHECK( body->inv.front().uid().get_value() == uid );
    CHECK( ( body->pos_abs() != origin ) == positive );
    CHECK( body->migo_intent()->last_energy_world ==
           ( positive ? to_turn<int>( calendar::turn ) : -1 ) );
    CHECK_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( goal ) ) );
    if( positive ) {
        REQUIRE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( edge ) ) );
        for( const auto &entry : MAPBUFFER ) {
            if( !buffered.count( entry.first ) ) {
                CHECK( project_to<coords::omt>( entry.first ).xy() == adjacent.xy() );
            }
        }
        for( const auto &entry : upper ) {
            CHECK( MAPBUFFER.lookup_submap_cached( entry.first ) == entry.second );
            CHECK( entry.second->get_ter( point_sm_ms( 0, 0 ) ) == ter_str_id( "t_floor" ) );
            CHECK( entry.second->last_touched == old_turn - 7_turns );
        }
        if( mode == "reload" ) {
            const auto saved = serialize_body( *body );
            body->deserialize( json_loader::from_string( saved ) );
            CHECK( serialize_body( *body ) == saved );
            CHECK( body->inv.front().uid().get_value() == uid );
        }
        const auto after = serialize_body( *body );
        overmap_buffer.move_hordes();
        CHECK( serialize_body( *retained() ) == after );
    } else if( mode != "blocked" ) {
        CHECK_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( edge ) ) );
        if( mode != "vehicle" ) {
            CHECK( serialize_body( *body ) == refused_source );
        }
    }
}

TEST_CASE( "migo_local_movement_acquires_at_the_real_bubble_edge_without_copy_authority",
           "[migo_tower][native][ownership][migo_movement_acquisition]" )
{
    const bool copied = GENERATE( false, true );
    CAPTURE( copied );
    map &here = get_map();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_turn = calendar::turn;
    auto origin = here.get_abs( tripoint_bub_ms( here.getmapsize() * SEEX - 1, 60, 0 ) );
    point direction = point::east;
    if( project_to<coords::omt>( origin ) == project_to<coords::omt>( origin + direction ) ) {
        origin = here.get_abs( tripoint_bub_ms( 0, 60, 0 ) );
        direction = point::west;
    }
    const auto edge = origin + direction;
    const auto cell = project_to<coords::omt>( origin );
    const auto next = project_to<coords::omt>( edge );
    const auto old_next = overmap_buffer.ter_existing( next );
    const auto old_ter = here.ter( here.get_bub( origin ) );
    const auto old_furn = here.furn( here.get_bub( origin ) );
    const auto goal = origin + direction * 18;
    const std::string id = "loaded-frontier-component";
    overmap_buffer.get( project_to<coords::om>( origin ).xy() );
    shared_ptr_fast<monster> body;
    monster *local = nullptr;
    on_out_of_scope restore( [&]() {
        if( local && get_creature_tracker().find( local->pos_abs() ).get() == local ) {
            g->remove_zombie( *local );
        }
        for( const auto &where : {origin, edge} ) {
            if( overmap *owner = overmap_buffer.find_loaded_overmap( project_to<coords::om>( where ).xy() ) ) {
                for( const auto &entry : owner->horde_entities() ) {
                    if( entry.second->monster_data &&
                        entry.second->monster_data->population_identity().site_id == id ) {
                        owner->release_monster_payload( entry.first,
                                                       entry.second->monster_data->population_identity().member_id );
                    }
                }
            }
        }
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.ter_set( next, old_next );
        here.ter_set( here.get_bub( origin ), old_ter );
        here.furn_set( here.get_bub( origin ), old_furn );
        MAPBUFFER.clear_outside_reality_bubble();
        set_time( old_turn );
    } );
    REQUIRE( cell != next );
    REQUIRE_FALSE( here.inbounds( edge ) );
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_existing( project_to<coords::sm>( edge ) ) );
    overmap_buffer.ter_set( next, oter_str_id( "field" ) );
    here.ter_set( here.get_bub( origin ), ter_str_id( "t_grass" ) );
    here.furn_set( here.get_bub( origin ), furn_str_id::NULL_ID() );
    migo_population::site_record population;
    population.id = id;
    population.anchor = cell;
    migo_population::member_record member;
    member.token = {id, "local-frontier-owner", 1, cell};
    member.type = mtype_id( "mon_mi_go" );
    member.home = origin;
    member.owner = migo_population::representation::local;
    member.assignment = migo_population::duty::dispatched;
    member.hp = member.type->hp;
    REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = cell;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = next;
    operation.sweep.candidates = {{goal}};
    operation.members = {{member.token.member_id, false, false, origin}};
    operation.overdue_after_active = 100000;
    site.operation = operation;
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    body = make_shared_fast<monster>( member.type );
    body->population_identity() = member.token;
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.home = origin;
    body->set_migo_intent( intent );
    local = g->place_critter_at( body, here.get_bub( origin ) );
    REQUIRE( local );
    local->add_item( item( itype_id( "rock" ), calendar::turn ) );
    const auto uid = local->inv.front().uid().get_value();
    const auto hp = local->get_hp();
    calendar::turn += 1_turns;
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    local->process_turn();
    REQUIRE( local->get_moves() > 0 );
    REQUIRE( migo_tower::plan_local( *local, false, nullptr ) );
    CHECK_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( edge ) ) );
    const auto energy = local->migo_intent()->last_energy_world;
    if( copied ) {
        monster candidate( *local );
        REQUIRE( migo_tower::move_local( candidate ) );
        CHECK_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( edge ) ) );
        CHECK( local->pos_abs() == origin );
        CHECK( local->inv.front().uid().get_value() == uid );
        CHECK( local->migo_intent()->last_energy_world == energy );
        return;
    }
    REQUIRE( migo_tower::move_local( *local ) );
    REQUIRE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( edge ) ) );
    const auto *lease = overmap_buffer.global_state.migo_populations.find( id )->find(
                            member.token.member_id );
    REQUIRE( lease );
    CAPTURE( origin, edge, goal, local->pos_abs(), local->get_hp(), local->get_moves(),
             local->population_identity().epoch, static_cast<int>( lease->owner ), lease->current_position );
    overmap *destination = overmap_buffer.find_loaded_overmap( project_to<coords::om>( edge ).xy() );
    REQUIRE( destination );
    const horde_entity *entry = nullptr;
    for( const auto &candidate : destination->horde_entities() ) {
        if( candidate.second->monster_data &&
            candidate.second->monster_data->population_identity().member_id == member.token.member_id ) {
            REQUIRE_FALSE( entry );
            entry = candidate.second;
        }
    }
    REQUIRE( entry );
    REQUIRE( entry->monster_data );
    CHECK( project_to<coords::omt>( entry->monster_data->pos_abs() ) == next );
    CHECK( rl_dist( entry->monster_data->pos_abs(), origin ) == 1 );
    CHECK( entry->monster_data->get_hp() == hp );
    CHECK( entry->monster_data->population_identity().epoch == member.token.epoch + 1 );
    CHECK( entry->monster_data->migo_intent()->last_energy_world == energy );
    CHECK( entry->monster_data->inv.front().uid().get_value() == uid );
    CHECK_FALSE( get_creature_tracker().find( origin ) );
}

TEST_CASE( "raid_sweep_builds_bounded_source_region_and_preserves_exclusions",
           "[raid_sweep][serialization]" )
{
    const tripoint_abs_omt source( -3, 4, 2 );
    const tripoint_abs_ms exact = project_to<coords::ms>( source ) + point( 7, 8 );
    raid_sweep::progress sweep;
    sweep.begin( source, 9, exact );
    REQUIRE( sweep.pending() == exact );
    CHECK( sweep.candidates.size() == 91 );
    std::set<tripoint_abs_omt> areas;
    for( const auto &candidate : sweep.candidates ) {
        const auto area = project_to<coords::omt>( candidate.position );
        CHECK( std::abs( area.x() - source.x() ) <= 1 );
        CHECK( std::abs( area.y() - source.y() ) <= 1 );
        CHECK( ( area.z() == 0 || area.z() == 2 ) );
        areas.insert( area );
    }
    CHECK( areas.size() == 18 );
    sweep.add( exact );
    sweep.add_floor( source );
    sweep.add_floor( source + point( 2, 0 ) );
    CHECK( sweep.candidates.size() == 91 );
    sweep.begin( source, 10, exact + point( 48, 0 ) );
    CHECK( sweep.candidates.size() == 90 ); // foreign exact MS grants no fallback
    CHECK( sweep.pending() != exact + point( 48, 0 ) );
    while( sweep.pending() ) {
        REQUIRE( sweep.exclude( raid_sweep::disposition::hazardous, 17 ) );
    }
    CHECK( sweep.outcome() == "hazard_blocked" );
    const auto reopened = sweep.candidates.front().position;
    REQUIRE( sweep.reconsider( reopened, 18 ) );
    CHECK( sweep.outcome() == "pending" );
    REQUIRE_FALSE( sweep.arrive( reopened, false ) );
    REQUIRE( sweep.arrive( reopened, true ) );
    CHECK( sweep.outcome() == "incomplete_sweep" );
    std::ostringstream bytes;
    {
        JsonOut out( bytes );
        sweep.serialize( out );
    }
    raid_sweep::progress loaded;
    loaded.deserialize( json_loader::from_string( bytes.str() ) );
    CHECK( loaded.generation == 10 );
    CHECK( loaded.source == source );
    CHECK( loaded.outcome() == "incomplete_sweep" );
    CHECK( loaded.candidates.front().reconsidered );
    CHECK_FALSE( loaded.arrive( reopened, true ) );
}

TEST_CASE( "raid_sweep_does_not_extend_refusal_by_oscillation_or_reload",
           "[raid_sweep][serialization]" )
{
    raid_sweep::progress sweep;
    const tripoint_abs_ms goal( 30, 30, 0 );
    sweep.candidates = { {goal} };
    sweep.advance( 100, 0 );
    sweep.approach( goal + point( -10, 0 ) );
    for( int turn = 1; turn <= 1801; ++turn ) {
        sweep.advance( 100 + turn, 1 );
        sweep.approach( goal + point( turn % 2 ? -11 : -10, 0 ) );
    }
    CHECK( sweep.last_progress == 1 ); // first real detour step; repeats earn nothing
    CHECK( sweep.obstructed() );
    std::ostringstream bytes;
    {
        JsonOut out( bytes );
        sweep.serialize( out );
    }
    raid_sweep::progress loaded;
    loaded.deserialize( json_loader::from_string( bytes.str() ) );
    CHECK( loaded.obstructed() );
    loaded.advance( 5000, 0 );
    CHECK( loaded.active_elapsed == 1801 );
    loaded.approach( goal + point( -10, 0 ) );
    CHECK( loaded.obstructed() ); // persisted visited cells reject replayed loops
    loaded.approach( goal + point( -9, 0 ) );
    CHECK_FALSE( loaded.obstructed() );
    CHECK( loaded.retry_after_active == 3601 );
}

TEST_CASE( "raid_sweep_tracks_member_detours_without_stationary_peer_credit",
           "[raid_sweep][serialization]" )
{
    raid_sweep::progress sweep;
    const tripoint_abs_ms goal( 30, 30, 0 );
    sweep.candidates = {{goal}, {goal + point( 1, 0 )}};
    sweep.advance( 100, 0 );
    sweep.approach( goal + point( -10, 0 ), "a" );
    sweep.approach( goal + point( -2, 0 ), "b" );
    sweep.advance( 101, 1 );
    sweep.approach( goal + point( -10, 0 ), "a" );
    sweep.approach( goal + point( -2, 0 ), "b" );
    CHECK( sweep.last_progress == 0 );
    sweep.approach( goal + point( -11, 0 ), "a" );
    CHECK( sweep.last_progress == 1 ); // real off-axis route may move away
    sweep.advance( 102, 1 );
    sweep.approach( goal + point( -10, 0 ), "a" );
    CHECK( sweep.last_progress == 1 );
    REQUIRE( sweep.arrive( goal, true ) );
    CHECK( sweep.movement.empty() );
    sweep.advance( 103, 1 );
    sweep.approach( goal + point( -12, 0 ), "a" );
    CHECK( sweep.last_progress == 2 ); // a new candidate starts a fresh baseline
}

TEST_CASE( "raid_source_memory_rejects_packets_and_eviction_loops_but_accepts_changed_facts",
           "[raid_sweep][serialization]" )
{
    raid_sweep::source_memory memory;
    const tripoint_abs_omt area( 2, 3, 0 );
    memory.advance( 100, 9000 );
    REQUIRE( memory.ready( area, "same_smoke" ) );
    REQUIRE( memory.finish( area, 1, "same_smoke", "hazard_blocked" ) );
    CHECK_FALSE( memory.ready( area, "same_smoke" ) );
    CHECK_FALSE( memory.ready( area, "observed_open_route" ) );
    CHECK_FALSE( memory.finish( area, 1, "same_smoke", "hazard_blocked" ) );
    const auto deadline = memory.outcomes.front().retry_after_active;
    memory.advance( 86500, 86400 );
    CHECK( memory.active_elapsed == 86400 );
    CHECK( memory.outcomes.front().retry_after_active == deadline );
    CHECK_FALSE( memory.ready( tripoint_abs_omt( area.xy(), 3 ), "same_smoke" ) );
    CHECK( memory.ready( area, "observed_open_route" ) );
    for( int i = 1; i <= 3; ++i ) {
        REQUIRE( memory.finish( area + point( i, 0 ), i + 1, "same_smoke", "empty" ) );
    }
    REQUIRE( memory.finish( area + point( 4, 0 ), 5, "same_smoke", "empty" ) );
    CHECK( memory.outcomes.size() == 4 );
    CHECK_FALSE( memory.ready( area, "same_smoke" ) );
    CHECK_FALSE( memory.ready( area, "observed_open_route" ) );
    CHECK_FALSE( memory.ready( area + point( 5, 0 ), "different_area" ) );
    memory.advance( 172900, 86400 );
    CHECK_FALSE( memory.ready( area, "same_smoke" ) );
    CHECK( memory.ready( area, "observed_open_route" ) );
    std::ostringstream bytes;
    {
        JsonOut out( bytes );
        memory.serialize( out );
    }
    raid_sweep::source_memory loaded;
    loaded.deserialize( json_loader::from_string( bytes.str() ) );
    CHECK_FALSE( loaded.ready( area, "same_smoke" ) );
    CHECK( loaded.ready( area, "observed_open_route" ) );
    REQUIRE( loaded.finish( area, 6, "observed_open_route", "searched" ) );
    CHECK_FALSE( loaded.ready( area, "observed_open_route" ) );
    CHECK_FALSE( loaded.finish( area, 6, "observed_open_route", "searched" ) );
    CHECK( raid_sweep::fingerprint( {"a", "b", "a"} ) == raid_sweep::fingerprint( {"b", "a"} ) );
    CHECK( raid_sweep::fingerprint( {"ab", "c"} ) != raid_sweep::fingerprint( {"a", "bc"} ) );
}

TEST_CASE( "raid_source_episode_requires_observed_extinction_and_a_later_receipt",
           "[raid_sweep][serialization]" )
{
    raid_sweep::source_memory memory;
    const tripoint_abs_omt area( 2, 3, 0 );
    REQUIRE( memory.reserve( area ) );
    CHECK( memory.next_episode( area, 100 ) == 1 );
    memory.observe_absence( area, 200 );
    CHECK( memory.next_episode( area, 100 ) == 1 );
    CHECK( memory.next_episode( area, 200 ) == 1 );
    CHECK( memory.next_episode( area, 201 ) == 2 );
    memory.adopt_signal( area, 100 );
    CHECK_FALSE( memory.outcomes.front().source_active );
    memory.adopt_signal( area, 201 );
    CHECK( memory.outcomes.front().source_active );
    CHECK( memory.outcomes.front().episode == 2 );
    memory.adopt_signal( area, 201 );
    CHECK( memory.outcomes.front().episode == 2 );
    std::ostringstream bytes;
    {
        JsonOut out( bytes );
        memory.serialize( out );
    }
    raid_sweep::source_memory loaded;
    loaded.deserialize( json_loader::from_string( bytes.str() ) );
    CHECK( loaded.next_episode( area, 201 ) == 2 );
    loaded.observe_absence( area, 300 );
    CHECK( loaded.next_episode( area, 201 ) == 2 );
    CHECK( loaded.next_episode( area, 301 ) == 3 );
}

TEST_CASE( "raid_deferred_approach_witness_survives_reload_without_minting_progress",
           "[raid_sweep][serialization]" )
{
    raid_sweep::progress sweep;
    const tripoint_abs_ms goal( 30, 30, 0 );
    sweep.candidates = {{goal}};
    REQUIRE( sweep.exclude( raid_sweep::disposition::unreachable, 0, {"member-a", "member-b"} ) );
    CHECK_FALSE( sweep.reconsider( goal, 0, "member-a" ) );
    std::ostringstream bytes;
    {
        JsonOut out( bytes );
        sweep.serialize( out );
    }
    raid_sweep::progress loaded;
    loaded.deserialize( json_loader::from_string( bytes.str() ) );
    CHECK_FALSE( loaded.reconsider( goal, 0, "member-b" ) );
    REQUIRE( loaded.reconsider( goal, 0, "member-c" ) ); // previously blocked, now physically available
    CHECK( loaded.last_progress == 0 );
    REQUIRE( loaded.exclude( raid_sweep::disposition::unreachable, 0, {"member-c"} ) );
    CHECK_FALSE( loaded.reconsider( goal, 0, "member-d" ) );
    // A legacy exclusion has no comparison witness; it stays honestly unsearched.
    raid_sweep::candidate legacy;
    legacy.deserialize( json_loader::from_string( "{\"state\":3}" ) );
    CHECK( legacy.state == raid_sweep::disposition::unreachable );
    CHECK( legacy.reconsidered );
}

TEST_CASE( "physical_sweep_does_not_search_a_burning_arrival_square",
           "[migo_tower][raid_sweep][native]" )
{
    clear_avatar();
    clear_map();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_turn = calendar::turn;
    on_out_of_scope restore( [&]() {
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        set_time( old_turn );
        clear_map();
    } );
    set_time_to_day();
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
    const tripoint_bub_ms start( 60, 60, 0 );
    monster *body = g->place_critter_at( mtype_id( "mon_mi_go" ), start );
    REQUIRE( body );
    const auto home = body->pos_abs();
    const auto anchor = project_to<coords::omt>( home );
    migo_population::member_record member;
    member.token = {"burning-arrival", "member", 1, anchor};
    member.type = body->type->id;
    member.home = home;
    member.owner = migo_population::representation::local;
    member.assignment = migo_population::duty::dispatched;
    member.hp = body->get_hp();
    migo_population::site_record population;
    population.id = member.token.site_id;
    population.anchor = anchor;
    REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = population.id;
    site.anchor = anchor;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = anchor;
    operation.members = {{member.token.member_id, false, false, home}};
    operation.sweep.source = anchor;
    operation.sweep.generation = 1;
    operation.sweep.candidates = {{home}};
    operation.overdue_after_active = 100000;
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    site.operation = operation;
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    body->population_identity() = member.token;
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.home = home;
    body->set_migo_intent( intent );
    here.add_field( start, field_type_id( "fd_fire" ), 3 );
    const auto step = physical_navigation::read_step( *body, home );
    CHECK( step.reason == "approach_fire_hazard" );
    CHECK( step.hazard_revision != 0 );
    calendar::turn += 1_turns;
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    const auto *current = overmap_buffer.global_state.migo_towers.find( population.id );
    REQUIRE( current );
    CHECK( current->operation->sweep.candidates.front().state == raid_sweep::disposition::hazardous );
    CHECK( current->operation->sweep.outcome() == "hazard_blocked" );
    CHECK( current->operation->sweep.last_progress == 0 );
    CHECK( body->pos_abs() == home ); // no artificial withdrawal/movement
}

TEST_CASE( "raid_sweep_requires_arrival_and_observation_and_bounds_obstruction",
           "[migo_tower][raid_sweep][serialization]" )
{
    raid_sweep::progress sweep;
    sweep.source = tripoint_abs_omt( 3, 2, 0 );
    sweep.generation = 4;
    const tripoint_abs_ms first( 84, 60, 0 );
    const tripoint_abs_ms second( 85, 60, 1 );
    sweep.candidates = { {first}, {second} };
    sweep.advance( 100, 9000 );
    CHECK( sweep.active_elapsed == 0 );
    sweep.advance( 10000, 0 );
    CHECK( sweep.active_elapsed == 0 );
    sweep.advance( 11799, 1799 );
    CHECK_FALSE( sweep.obstructed() );
    CHECK_FALSE( sweep.arrive( first, false ) );
    CHECK_FALSE( sweep.arrive( second, true ) );
    sweep.advance( 11800, 1 );
    CHECK( sweep.obstructed() );
    REQUIRE( sweep.exclude( raid_sweep::disposition::hazardous, 7 ) );
    CHECK( sweep.last_progress == 0 );
    CHECK_FALSE( sweep.obstructed() ); // a different candidate gets its bounded attempt
    CHECK_FALSE( sweep.reconsider( first, 7 ) );
    REQUIRE( sweep.reconsider( first, 8 ) );
    CHECK( sweep.pending() == first );
    CHECK_FALSE( sweep.obstructed() ); // one material-change retry has a bounded allowance
    CHECK( sweep.last_progress == 0 ); // it still is not physical search progress
    sweep.made_progress(); // caller has actually moved or engaged
    CHECK_FALSE( sweep.obstructed() );
    REQUIRE( sweep.arrive( first, true ) );
    CHECK( sweep.candidates[0].state == raid_sweep::disposition::searched );
    CHECK( sweep.pending() == second );
    CHECK_FALSE( sweep.reconsider( first, 9 ) );
    REQUIRE( sweep.exclude( raid_sweep::disposition::unreachable, 11 ) );
    CHECK_FALSE( sweep.pending() );
    CHECK_FALSE( sweep.arrive( second, true ) );
    REQUIRE( sweep.reconsider( second, 12 ) );
    REQUIRE( sweep.exclude( raid_sweep::disposition::unreachable, 12 ) );
    CHECK_FALSE( sweep.reconsider( second, 13 ) );
    std::ostringstream saved;
    { JsonOut json( saved ); sweep.serialize( json ); }
    raid_sweep::progress restored;
    restored.deserialize( json_loader::from_string( saved.str() ) );
    CHECK_FALSE( restored.pending() );
    CHECK( restored.generation == 4 );
    CHECK( restored.candidates[0].reconsidered );
    CHECK( restored.candidates[0].geometry_revision == 8 );
    CHECK( restored.candidates[1].state == raid_sweep::disposition::unreachable );
    CHECK( restored.active_elapsed == 1800 );
    restored.advance( 11800, 1800 );
    CHECK( restored.active_elapsed == 1800 );
    restored.advance( 11801, 1800 );
    CHECK( restored.active_elapsed == 1801 ); // never admit more than the real interval

    raid_sweep::progress changed;
    changed.candidates = { {first} };
    REQUIRE( changed.exclude( raid_sweep::disposition::hazardous, 7 ) );
    REQUIRE( changed.reconsider( first, 8 ) );
    REQUIRE( changed.exclude( raid_sweep::disposition::hazardous, 8 ) );
    CHECK_FALSE( changed.reconsider( first, 9 ) );
    CHECK_FALSE( changed.pending() );
}

TEST_CASE( "migo_loaded_operation_frozen_scheduler_preserves_budget_fields_and_body",
           "[migo_tower][migo_activation][native]" )
{
    clear_avatar();
    clear_map_without_vision();
    const auto previous_time = calendar::turn;
    const auto previous_avatar = get_avatar().pos_abs();
    const auto previous_population = overmap_buffer.global_state.migo_populations;
    const auto previous_towers = overmap_buffer.global_state.migo_towers;
    const auto previous_bandits = overmap_buffer.global_state.bandit_live_world;
    overmap_buffer.global_state.migo_populations.clear();
    overmap_buffer.global_state.migo_towers.clear();
    overmap_buffer.global_state.bandit_live_world = {};
    set_time( calendar::turn_zero + 100000_turns );
    map &here = get_map();
    const tripoint_bub_ms position( 63, 63, 0 );
    const auto absolute = here.get_abs( position );
    const auto body_omt = project_to<coords::omt>( absolute );
    const auto home = body_omt + point( 45, 0 );
    const std::string id = "loaded-frozen-native-owner";
    migo_population::site_record population;
    population.id = id;
    population.anchor = home;
    migo_population::member_record member;
    member.token = { id, "native-local", 1, home };
    member.type = mtype_id( "mon_mi_go" );
    member.owner = migo_population::representation::local;
    member.assignment = migo_population::duty::dispatched;
    member.home = project_to<coords::ms>( home );
    member.hp = member.type->hp;
    REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = home;
    site.operation.emplace();
    site.operation->generation = 1;
    site.operation->members.push_back( { member.token.member_id } );
    site.operation->sweep.begin( body_omt, 1, absolute + point::east );
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster *body = g->place_critter_at( member.type, position );
    REQUIRE( body );
    body->population_identity() = member.token;
    body->set_moves( 150 );
    body->wandf = 10;
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.home = member.home;
    intent.waypoint = absolute + point::east;
    body->set_migo_intent( intent );
    body->set_dest( *intent.waypoint );
    on_out_of_scope restore( [&]() {
        std::vector<monster *> remaining;
        for( monster &candidate : g->all_monsters() ) {
            if( candidate.population_identity().site_id == id ) {
                remaining.push_back( &candidate );
            }
        }
        for( const auto *candidate : remaining ) {
            g->remove_zombie( *candidate );
        }
        clear_fields( 0 );
        overmap_buffer.global_state.migo_populations = previous_population;
        overmap_buffer.global_state.migo_towers = previous_towers;
        overmap_buffer.global_state.bandit_live_world = previous_bandits;
        get_avatar().setpos( previous_avatar, false );
        set_time( previous_time );
    } );
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    get_avatar().setpos( project_to<coords::ms>( body_omt - point( 41, 0 ) ), false );
    set_time( calendar::turn + 2_days );
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    REQUIRE_FALSE( overmap_buffer.global_state.migo_towers.find( id )->operation->activity.active );
    here.add_field( position, field_type_id( "fd_fire" ), 1 );
    const auto serialize = []( const monster & value ) {
        std::ostringstream out;
        {
            JsonOut json( out );
            value.serialize( json );
        }
        return out.str();
    };
    const auto frozen = serialize( *body );
    process_monsters_and_npcs_turn_for_test(); // actual plan/move/field/daily-biological owner
    monster *retained = nullptr;
    for( monster &candidate : g->all_monsters() ) {
        if( candidate.population_identity().site_id == id ) {
            retained = &candidate;
        }
    }
    REQUIRE( retained );
    CHECK( serialize( *retained ) == frozen );
    CHECK( retained->get_moves() == 150 );
    CHECK( retained->wandf == 10 );
    CHECK( retained->pos_abs() == absolute );
    CHECK( retained->get_hp() == member.hp );
}

TEST_CASE( "migo_whole_party_interval_gates_real_abstract_owner_and_frozen_reload",
           "[migo_tower][migo_activation][native]" )
{
    using migo_population::representation;
    clear_avatar();
    clear_map_without_vision();
    const auto old_time = calendar::turn;
    const auto old_avatar = get_avatar().pos_abs();
    const auto old_population = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    overmap_buffer.global_state.migo_populations.clear();
    overmap_buffer.global_state.migo_towers.clear();
    set_time( calendar::turn_zero + 100000_turns );
    const int inner_distance = GENERATE( 25, 39 );
    const auto player = project_to<coords::omt>( old_avatar );
    const tripoint_abs_omt anchor( player.xy() + point( 45, 0 ), 0 );
    const std::string id = "activation-real-owner";
    const std::array<tripoint_abs_ms, 2> positions = {
        project_to<coords::ms>( tripoint_abs_omt( player.xy() + point( inner_distance, 0 ), 0 ) ) + point( 12, 12 ),
        project_to<coords::ms>( tripoint_abs_omt( player.xy() + point( 41, 0 ), 1 ) ) + point( 12, 12 )
    };
    std::vector<migo_population::member_record> members;
    for( int i = 0; i < 2; ++i ) {
        migo_population::member_record member;
        member.token = { id, "member-" + std::to_string( i ), 1, anchor };
        member.type = mtype_id( "mon_mi_go" );
        member.owner = representation::abstract;
        member.assignment = migo_population::duty::dispatched;
        member.home = project_to<coords::ms>( tripoint_abs_omt( anchor.xy(), i ) ) + point( 12, 12 );
        member.hp = member.type->hp - 7;
        members.push_back( member );
    }
    migo_population::site_record population;
    population.id = id;
    population.anchor = anchor;
    population.production_elapsed = 17;
    REQUIRE( population.commit_seed( members, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = anchor;
    site.operation.emplace();
    site.operation->generation = 1;
    site.operation->source.source_omt = project_to<coords::omt>( positions[0] );
    site.operation->sweep.begin( site.operation->source.source_omt, 1, positions[0] + point( 3, 0 ) );
    site.operation->advance( to_turn<int>( calendar::turn ), 0 );
    for( const auto &member : members ) {
        site.operation->members.push_back( { member.token.member_id } );
    }
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    std::array<overmap *, 2> owners;
    std::array<monster *, 2> bodies;
    on_out_of_scope restore( [&]() {
        for( int i = 0; i < 2; ++i ) {
            if( auto *owner = overmap_buffer.find_loaded_overmap( project_to<coords::om>
                ( positions[i] ).xy() ) ) {
                owner->release_monster_payload( positions[i], members[i].token.member_id );
            }
        }
        overmap_buffer.global_state.migo_populations = old_population;
        overmap_buffer.global_state.migo_towers = old_towers;
        get_avatar().setpos( old_avatar, false );
        set_time( old_time );
    } );
    const auto serialize = []( const auto & value ) {
        std::ostringstream out;
        {
            JsonOut json( out );
            value.serialize( json );
        }
        return out.str();
    };
    for( int i = 0; i < 2; ++i ) {
        // This case needs a stationary physical refusal while it exercises
        // party intervals. Unknown terrain is no longer that prerequisite:
        // the authenticated movement owner may acquire it and take a step.
        // Supply actual blocked footing, without changing any clock/budget.
        tinymap footing;
        footing.load( project_to<coords::omt>( positions[i] ), true );
        for( int x = -1; x <= 1; ++x ) {
            for( int y = -1; y <= 1; ++y ) {
                const auto p = project_remain<coords::omt>( positions[i] + point( x, y ) ).remainder_tripoint;
                footing.ter_set( p, ter_str_id( x == 0 && y == 0 ? "t_floor" : "t_wall" ) );
                footing.furn_set( p, furn_str_id::NULL_ID() );
            }
        }
        monster body( members[i].type );
        body.set_hp( members[i].hp );
        body.population_identity() = members[i].token;
        body.set_pos_abs_only( positions[i] );
        body.set_moves( 0 );
        migo_tower::member_intent intent;
        intent.generation = 1;
        intent.home = members[i].home;
        body.set_migo_intent( intent );
        owners[i] = &overmap_buffer.get( project_to<coords::om>( positions[i] ).xy() );
        auto *entity = owners[i]->retain_monster_payload( positions[i], body );
        REQUIRE( entity );
        REQUIRE( entity->monster_data );
        bodies[i] = entity->monster_data.get();
        const auto refusal = physical_navigation::read_step( *bodies[i], positions[i] + point( 3, 0 ) );
        REQUIRE( refusal.next == physical_navigation::action::none );
        REQUIRE_FALSE( refusal.missing_geometry );
    }
    auto home = [&]() -> auto & { return *overmap_buffer.global_state.migo_populations.find( id ); };
    auto operation = [&]() -> auto & { return *overmap_buffer.global_state.migo_towers.find( id )->operation; };
    auto service = [&]() {
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        overmap_buffer.move_hordes();
    };
    service(); // legacy interval and native container authentication adopt now
    REQUIRE( operation().activity.active );
    CHECK_FALSE( home().activity.active );
    CHECK( operation().sweep.active_elapsed == 0 );
    for( int i = 0; i < 2; ++i ) {
        CHECK( home().members[i].current_position == positions[i] );
    }
    set_time( calendar::turn + 1_turns );
    service();
    CHECK( operation().sweep.active_elapsed == 1 );
    CHECK( home().production_elapsed == 17 );
    for( int i = 0; i < 2; ++i ) {
        // The actual blocked physical route never banks movement.
        auto *entity = owners[i]->entity_at( project_remain<coords::om>
                                             ( positions[i] ).remainder_tripoint );
        REQUIRE( entity );
        CHECK( entity->moves == 0 );
        bodies[i] = entity->monster_data.get();
    }
    const std::array<std::string, 2> frozen_body = { serialize( *bodies[0] ), serialize( *bodies[1] ) };
    const auto frozen_sweep = serialize( operation().sweep );
    const auto outside = project_to<coords::ms>( tripoint_abs_omt(
            player.xy() + point( inner_distance - 41, 0 ), player.z() ) );
    get_avatar().setpos( outside, false );
    set_time( calendar::turn + 1_turns );
    service();
    REQUIRE_FALSE( operation().activity.active );
    set_time( calendar::turn + 2_days );
    service();
    CHECK( serialize( operation().sweep ) == frozen_sweep );
    CHECK( serialize( *bodies[0] ) == frozen_body[0] );
    CHECK( serialize( *bodies[1] ) == frozen_body[1] );
    CHECK( home().production_elapsed == 17 );
    const auto saved_population = serialize( overmap_buffer.global_state.migo_populations );
    const auto saved_towers = serialize( overmap_buffer.global_state.migo_towers );
    overmap_buffer.global_state.migo_populations.deserialize( json_loader::from_string(
                saved_population ) );
    overmap_buffer.global_state.migo_towers.deserialize( json_loader::from_string( saved_towers ) );
    service();
    CHECK_FALSE( operation().activity.active );
    CHECK( serialize( *bodies[0] ) == frozen_body[0] );
    CHECK( serialize( *bodies[1] ) == frozen_body[1] );
    get_avatar().setpos( outside + point( 24, 0 ), false ); // exactly40 for the inner living member
    service();
    REQUIRE( operation().activity.active );
    CHECK( operation().sweep.active_elapsed == 1 );
    CHECK( operation().activity.active_delta_turns == 0 );
    set_time( calendar::turn + 1_turns );
    service();
    CHECK( operation().sweep.active_elapsed == 2 );
    service();
    CHECK( operation().sweep.active_elapsed == 2 );
    CHECK_FALSE( home().activity.active );
    // A corpse inside cannot activate its still-outside peer. Native payload
    // authentication records actual health; no body is manufactured by lookup.
    auto *first = owners[0]->entity_at( project_remain<coords::om>( positions[0] ).remainder_tripoint );
    REQUIRE( first );
    first->monster_data->set_hp( 0 );
    CHECK_FALSE( migo_population::commit_position( *first->monster_data, representation::abstract,
            positions[0] ) );
    set_time( calendar::turn + 1_turns );
    service();
    CHECK_FALSE( operation().activity.active );
    CHECK_FALSE( operation().resolved );
    auto *last = owners[1]->entity_at( project_remain<coords::om>( positions[1] ).remainder_tripoint );
    REQUIRE( last );
    last->monster_data->set_hp( 0 );
    CHECK_FALSE( migo_population::commit_position( *last->monster_data, representation::abstract,
            positions[1] ) );
    service();
    CHECK( operation().resolved );
    CHECK( operation().communicated_losses.empty() );
    CHECK( operation().sweep.active_elapsed == 2 );
}

TEST_CASE( "migo_native_loop_boundary_handoff_yields_released_handle_and_preserves_refusal",
           "[migo_tower][ownership][native][migo_movement_acquisition]" )
{
    // This isolates the real movement/placement caller. Seeded leases and
    // intent are setup, not signal, admission, full journey or live proof.
    const bool occupied_receiver = GENERATE( false, true );
    clear_avatar();
    clear_map_without_vision();
    const auto old_turn = calendar::turn;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_bandits = overmap_buffer.global_state.bandit_live_world;
    overmap_buffer.global_state.bandit_live_world = {};
    set_time( calendar::turn_zero + 1400_turns );
    map &here = get_map();
    const auto old_origin = here.get_abs_sub();
    const tripoint_bub_ms edge( here.getmapsize() * SEEX - 1, 60, 0 );
    const auto start = here.get_abs( edge );
    const auto outside = start + point::east;
    const auto home = start + point( 26, 0 );
    const auto anchor = project_to<coords::omt>( home );
    const auto near_cell = project_to<coords::omt>( outside );
    const oter_id old_near_terrain = overmap_buffer.ter_existing( near_cell );
    const oter_id old_home_terrain = overmap_buffer.ter_existing( anchor );
    overmap_buffer.ter_set( near_cell, oter_str_id( "field" ) );
    overmap_buffer.ter_set( anchor, oter_str_id( "field" ) );
    const std::string id = "motor-fixture@" + anchor.to_string_writable();
    const std::string member_id = id + ":returnee";
    const std::string blocker_id = id + ":blocker";
    overmap &owner = overmap_buffer.get( project_to<coords::om>( outside ).xy() );
    on_out_of_scope restore( [&]() {
        clear_avatar();
        clear_map_without_vision();
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data && entry.second->monster_data->population_identity().site_id == id ) {
                owner.release_monster_payload( entry.first,
                                               entry.second->monster_data->population_identity().member_id );
            }
        }
        here.load( old_origin, false );
        get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ) );
        overmap_buffer.ter_set( near_cell, old_near_terrain );
        overmap_buffer.ter_set( anchor, old_home_terrain );
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.global_state.bandit_live_world = old_bandits;
        MAPBUFFER.clear_outside_reality_bubble();
        set_time( old_turn );
    } );
    for( const auto &cell : {near_cell, anchor} ) {
        tinymap terrain;
        terrain.load( cell, false );
        for( const auto &p : terrain.points_in_rectangle( tripoint_omt_ms( 0, 0, 0 ),
                tripoint_omt_ms( 2 * SEEX - 1, 2 * SEEY - 1, 0 ) ) ) {
            terrain.ter_set( p, ter_str_id( "t_grass" ) );
            terrain.furn_set( p, furn_str_id::NULL_ID() );
        }
    }
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ) );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    migo_population::site_record population;
    population.id = id;
    population.anchor = anchor;
    migo_population::member_record resident;
    resident.token = {id, member_id, 1, anchor};
    resident.type = mtype_id( "mon_mi_go" );
    resident.owner = migo_population::representation::local;
    resident.assignment = migo_population::duty::returning;
    resident.home = home;
    resident.hp = resident.type->hp;
    auto blocker = resident;
    blocker.token.member_id = blocker_id;
    blocker.owner = migo_population::representation::abstract;
    blocker.assignment = migo_population::duty::available;
    REQUIRE( population.commit_seed( {resident, blocker}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state tower;
    tower.site_id = id;
    tower.anchor = anchor;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = project_to<coords::omt>( start );
    operation.members = { {member_id} };
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    tower.operation = operation;
    tower.next_generation = 2;
    overmap_buffer.global_state.migo_towers.sites.push_back( tower );
    monster *body = g->place_critter_at( mtype_id( "mon_mi_go" ), edge );
    REQUIRE( body );
    body->add_item( item( itype_id( "rock" ), calendar::turn ) );
    const auto inventory_uid = body->inv.front().uid().get_value();
    body->population_identity() = resident.token;
    body->set_moves( 0 );
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.phase = migo_tower::member_phase::returning;
    intent.home = home;
    body->set_migo_intent( intent );
    // Native contact is an action; simply retaining a visible enemy is not
    // search progress. The returning member must resume home outside its
    // actual immediate melee reach, rather than start chasing that enemy.
    get_avatar().set_str_base( 30 );
    get_avatar().recalc_hp();
    get_avatar().set_all_parts_hp_to_max(); // durable fixture target, no victory assertion
    get_avatar().setpos( here, edge - point::east );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    REQUIRE( body->sees( here, get_avatar() ) );
    REQUIRE( body->attitude( &get_avatar() ) == MATT_ATTACK );
    body->set_moves( 100 );
    body->plan();
    CHECK( body->migo_intent()->last_combat_world < to_turn<int>( calendar::turn ) );
    body->move();
    CHECK( body->migo_intent()->last_combat_world == to_turn<int>( calendar::turn ) );
    REQUIRE_FALSE( get_avatar().is_dead_state() );
    CHECK( body->pos_abs() == start );
    get_avatar().setpos( here, edge - point( 5, 0 ) );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    REQUIRE( body->sees( here, get_avatar() ) );
    REQUIRE_FALSE( body->is_adjacent( &get_avatar(), true ) );
    REQUIRE( migo_tower::own_capability( *body ).ranged_range == 0 );
    body->set_moves( 100 );
    body->plan();
    CHECK( body->migo_intent()->phase == migo_tower::member_phase::returning );
    CHECK( body->get_dest() == outside );
    // Reset only this transfer fixture's initial budget/target setup. The
    // actual near/far native decisions above retain their own scope.
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ) );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    body->set_moves( 0 );
    body->unset_dest();
    monster blocking( mtype_id( "mon_mi_go" ) );
    blocking.population_identity() = blocker.token;
    const auto blocker_position = occupied_receiver ? outside : outside + point( 0, 2 );
    blocking.set_pos_abs_only( blocker_position );
    REQUIRE( owner.retain_monster_payload( blocker_position, blocking ) );
    // Planning adopted the current interval at zero. Earn one actual native
    // tick before asking the scheduler to fund this transfer.
    calendar::turn += 1_turns;
    // Actual acid at the departure would affect a released HP0 handle again
    // if monmove continued native field/death processing after the handoff.
    here.add_field( edge, field_type_id( "fd_acid" ), 1 );
    process_monsters_and_npcs_turn_for_test();
    auto *ledger = overmap_buffer.global_state.migo_populations.find( id );
    REQUIRE( ledger );
    auto *record = ledger->find( member_id );
    REQUIRE( record );
    if( occupied_receiver ) {
        CHECK( body->inv.front().uid().get_value() == inventory_uid );
        CHECK( record->owner == migo_population::representation::local );
        CHECK( record->token.epoch == 1 );
        CHECK( body->pos_abs() == start );
        CHECK( body->get_moves() == body->get_speed() );
        CHECK( body->migo_intent()->last_refused_world == to_turn<int>( calendar::turn ) );
        const int budget = body->get_moves();
        calendar::turn += 1_turns;
        body->process_turn();
        CHECK( body->get_moves() == budget ); // no accumulated refused-step backlog
        REQUIRE( owner.release_monster_payload( outside, blocker_id ) );
        // Setup removes the receiving obstruction; this is not proof of the
        // blocker's autonomous travel, death or population loss.
        blocking.set_pos_abs_only( outside + point( 0, 2 ) );
        REQUIRE( owner.retain_monster_payload( blocking.pos_abs(), blocking ) );
        here.remove_field( edge, field_type_id( "fd_acid" ) );
        process_monsters_and_npcs_turn_for_test();
    }
    CHECK( record->owner == migo_population::representation::abstract );
    CHECK( record->token.epoch == 2 );
    CHECK( record->hp > 0 );
    CHECK( ledger->living_count() == 2 );
    CHECK( std::none_of( here.i_at( edge ).begin(), here.i_at( edge ).end(), []( const item &i ) {
        return i.is_corpse();
    } ) );
    monster *payload = nullptr;
    int copies = 0;
    for( const auto &entry : owner.horde_entities() ) {
        if( entry.second->monster_data && entry.second->monster_data->population_identity().member_id == member_id ) {
            ++copies;
            CHECK( entry.first == outside );
            payload = entry.second->monster_data.get();
        }
    }
    REQUIRE( copies == 1 );
    REQUIRE( payload );
    CHECK( payload->get_hp() > 0 );
    REQUIRE_FALSE( payload->inv.empty() );
    CHECK( payload->inv.front().uid().get_value() == inventory_uid );
    const int budget = payload->get_moves();
    const int hp = payload->get_hp();
    here.load( old_origin + point( 1, 0 ), false );
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ) );
    overmap_buffer.spawn_monster( project_to<coords::sm>( outside ) );
    monster *reentered = nullptr;
    for( monster &loaded : g->all_monsters() ) {
        if( loaded.population_identity().member_id == member_id ) {
            REQUIRE_FALSE( reentered );
            reentered = &loaded;
        }
    }
    REQUIRE( reentered );
    REQUIRE_FALSE( reentered->inv.empty() );
    CHECK( reentered->inv.front().uid().get_value() == inventory_uid );
    CHECK( record->owner == migo_population::representation::local );
    CHECK( reentered->population_identity().epoch == 3 );
    CHECK( reentered->pos_abs() == outside );
    reentered->process_turn();
    CHECK( reentered->get_moves() == budget );
    CHECK( reentered->get_hp() == hp );
    calendar::turn += 1_turns;
    reentered->process_turn();
    CHECK( reentered->get_moves() == budget + reentered->get_speed() );
}

TEST_CASE( "migo_native_return_resolves_distant_floors_and_occupied_approaches",
           "[migo_tower][native][ownership]" )
{
    const int scenario = GENERATE( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 );
    const bool remote = scenario == 1 || scenario == 2 || scenario == 3 ||
                        scenario == 5 || scenario == 7;
    const bool occupied_approach = scenario == 2;
    const bool missing_stairs = scenario == 3;
    const bool walled_approach = scenario == 4 || scenario == 5;
    const bool native_floorplan = scenario >= 6;
    const bool native_atmosphere = scenario >= 8;
    const bool real_fire = scenario == 9;
    CAPTURE( remote, occupied_approach, missing_stairs, walled_approach, native_floorplan,
             native_atmosphere, real_fire );
    clear_avatar();
    clear_map_without_vision();
    get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
    const auto old_turn = calendar::turn;
    const auto activation_avatar = get_avatar().pos_abs();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_bandits = overmap_buffer.global_state.bandit_live_world;
    overmap_buffer.global_state.bandit_live_world = {};
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ) );
    const auto offset = native_floorplan ? tripoint( 55, 108, 0 ) :
                        tripoint( walled_approach ? 44 : 36, 60, 0 );
    const auto start = remote ?
                       project_to<coords::ms>( project_to<coords::om>( get_avatar().pos_abs() ).xy() +
                                              point( 2, 0 ) ) + offset :
                       here.get_abs( native_floorplan ? tripoint_bub_ms( 55, 108, 0 ) :
                                     tripoint_bub_ms( 20, 60, 0 ) );
    const auto home = start + ( native_floorplan ? tripoint( 5, -48, 2 ) :
                               tripoint( occupied_approach ? 12 : 85, 0, occupied_approach ? 0 : 2 ) );
    const auto anchor = project_to<coords::omt>( home );
    overmap &owner = overmap_buffer.get( project_to<coords::om>( start ).xy() );
    const std::string id = "return-route-" + std::to_string( scenario );
    std::vector<std::pair<tripoint_abs_omt, oter_id>> old_terrain;
    std::set<character_id> old_npcs;
    for( int z = 0; z < 4; ++z ) {
        for( const auto &npc : overmap_buffer.get_npcs_near_omt( tripoint_abs_omt( anchor.xy(), z ), 1 ) ) {
            old_npcs.insert( npc->getID() );
        }
    }
    on_out_of_scope restore( [&]() {
        clear_creatures();
        clear_avatar();
        std::vector<std::pair<tripoint_abs_ms, std::string>> retained;
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data &&
                entry.second->monster_data->population_identity().site_id == id ) {
                retained.emplace_back( entry.first,
                                       entry.second->monster_data->population_identity().member_id );
            }
        }
        for( const auto &entry : retained ) {
            CHECK( owner.release_monster_payload( entry.first, entry.second ) );
        }
        for( const auto &entry : old_terrain ) {
            overmap_buffer.ter_set( entry.first, entry.second );
        }
        for( const auto &npc : overmap_buffer.get_npcs_near_omt( tripoint_abs_omt( anchor.xy(), 0 ), 1 ) ) {
            if( old_npcs.count( npc->getID() ) == 0 ) {
                overmap_buffer.remove_npc( npc->getID() );
            }
        }
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.global_state.bandit_live_world = old_bandits;
        clear_map_without_vision();
        MAPBUFFER.clear_outside_reality_bubble();
        get_avatar().setpos( activation_avatar, false );
        set_time( old_turn );
    } );
    // This is an off-bubble motor control, not an inactive-party control.
    // Keep the real geometry remote while admitting the entire short journey
    // within40; the separate activation controls prove the outside boundary.
    if( remote ) {
        get_avatar().setpos( project_to<coords::ms>( project_to<coords::omt>( start ) - point( 30, 0 ) ),
                             false );
    }

    const auto first = project_to<coords::omt>( start );
    if( native_floorplan ) {
        // Use the production north-facing tower, including its irregular
        // footprint, off-axis resin portals, emitters and real floor pairs.
        // The return starts only48 MS from home's XY; that projection is
        // still an approach on the wrong floor, not the physical home.
        for( int z = 0; z < 4; ++z ) {
            const tripoint_abs_omt cell( anchor.xy(), z );
            old_terrain.emplace_back( cell, overmap_buffer.ter_existing( cell ) );
            overmap_buffer.ter_set( cell, oter_str_id( "mi-go_scout_tower_" +
                                   std::to_string( z + 1 ) + "_north" ) );
        }
        smallmap generated;
        generated.generate( tripoint_abs_omt( anchor.xy(), 0 ), calendar::turn, false );
        REQUIRE( generated.ter( tripoint_omt_ms( 12, 12, 2 ) ) == ter_str_id( "t_floor_resin" ) );
        REQUIRE( generated.ter( tripoint_omt_ms( 5, 8, 0 ) ) == ter_str_id( "t_slope_up" ) );
        REQUIRE( generated.ter( tripoint_omt_ms( 5, 8, 1 ) ) == ter_str_id( "t_slope_down" ) );
        REQUIRE( generated.ter( tripoint_omt_ms( 5, 9, 1 ) ) == ter_str_id( "t_slope_up" ) );
        REQUIRE( generated.ter( tripoint_omt_ms( 5, 9, 2 ) ) == ter_str_id( "t_slope_down" ) );
        for( int z = 0; z < 3; ++z ) {
            for( int y = anchor.y(); y <= ( z == 0 ? project_to<coords::omt>( start ).y() :
                                          anchor.y() ); ++y ) {
                const tripoint_abs_omt cell( anchor.x(), y, z );
                if( y != anchor.y() ) {
                    old_terrain.emplace_back( cell, overmap_buffer.ter_existing( cell ) );
                    overmap_buffer.ter_set( cell, oter_str_id( "field" ) );
                }
                tinymap terrain;
                terrain.load( cell, false );
                for( const auto &tile : terrain.points_in_rectangle( tripoint_omt_ms( 0, 0, z ),
                        tripoint_omt_ms( 23, 23, z ) ) ) {
                    terrain.ter_set( tile, y == anchor.y() ? generated.ter( tile ) :
                                     ter_str_id( "t_grass" ).id() );
                    terrain.furn_set( tile, y == anchor.y() ? generated.furn( tile ) :
                                      furn_str_id::NULL_ID().id() );
                }
            }
        }
        generated.delete_unmerged_submaps();
        // Isolate navigation from generated captives' legitimate native
        // combat, and refresh the main map after editing shared submaps
        // through tinymaps. Neither is an assertion waiver or product change.
        for( int z = 0; z < 4; ++z ) {
            for( const auto &npc : overmap_buffer.get_npcs_near_omt( tripoint_abs_omt( anchor.xy(), z ), 1 ) ) {
                if( old_npcs.count( npc->getID() ) == 0 ) {
                    overmap_buffer.remove_npc( npc->getID() );
                }
            }
        }
        // Reload the owning map once after shared-submap edits, including
        // routing and emitter-location caches, rather than partial refreshes.
        here.load( here.get_abs_sub(), false );
        for( int z = 0; z < 4; ++z ) {
            here.build_map_cache( z, true );
        }
        g->reload_npcs();
        clear_creatures();
        if( native_atmosphere ) {
            REQUIRE_FALSE( here.get_furn_field_locations().empty() );
            for( int turn = 0; turn < 40; ++turn ) {
                calendar::turn += 1_turns;
                here.furniture_terrain_emit_fields();
                here.process_fields();
            }
            bool emitted = false;
            for( const auto &tile : here.points_on_zlevel( 1 ) ) {
                emitted |= here.get_field_intensity( tile, field_type_id( "fd_migo_atmosphere" ) ) > 0;
            }
            REQUIRE( emitted );
        }
    } else {
        // A real ground corridor reaches the building. For the upper-home cases,
        // matching stairs exist only at home, beyond the native short path.
        for( int z = 0; z <= home.z(); ++z ) {
            for( int x = z == 0 ? first.x() : anchor.x(); x <= anchor.x(); ++x ) {
                const tripoint_abs_omt cell( x, anchor.y(), z );
                old_terrain.emplace_back( cell, overmap_buffer.ter_existing( cell ) );
                overmap_buffer.ter_set( cell, oter_str_id( "field" ) );
                tinymap terrain;
                terrain.load( cell, false );
                for( const auto &tile : terrain.points_in_rectangle( tripoint_omt_ms( 0, 0, z ),
                        tripoint_omt_ms( 23, 23, z ) ) ) {
                    terrain.ter_set( tile, ter_str_id( "t_floor" ) );
                    terrain.furn_set( tile, furn_str_id::NULL_ID() );
                }
                if( walled_approach && cell.x() == anchor.x() ) {
                    // The bounded projection lands on the real building wall
                    // before its off-axis entrance/stairs fit the native route.
                    const int wall_x = home.x() - cell.x() * 24 - 5;
                    const int entry_y = home.y() - cell.y() * 24 + 3;
                    REQUIRE( wall_x >= 0 );
                    REQUIRE( wall_x < 24 );
                    REQUIRE( entry_y >= 0 );
                    REQUIRE( entry_y < 24 );
                    for( int y = 0; y < 24; ++y ) {
                        terrain.ter_set( tripoint_omt_ms( wall_x, y, z ),
                                         ter_str_id( y == entry_y ? "t_resin_hole_c" : "t_wall_resin" ) );
                    }
                }
                // Keep both members of the real stair pair inside this OMT.
                // A home near the east edge still needs an off-axis approach,
                // not a silently refused placement at x=25.
                const int stair_base = walled_approach ?
                                       std::clamp( home.x() - cell.x() * 24 + 4, 0, 2 * SEEX - 2 ) : 6;
                const tripoint_omt_ms stairs( stair_base + ( z == 2 ),
                                             home.y() - cell.y() * 24, z );
                if( home.z() > 0 && !missing_stairs && cell.x() == anchor.x() ) {
                    if( z == 0 ) {
                        const bool stairs_inside = stairs.x() >= 0 && stairs.x() < 2 * SEEX &&
                                                   stairs.y() >= 0 && stairs.y() < 2 * SEEY;
                        CAPTURE( scenario, cell, home, stairs, stairs_inside );
                        terrain.ter_set( stairs, ter_str_id( "t_stairs_up" ) );
                        REQUIRE( terrain.ter( stairs ) == ter_str_id( "t_stairs_up" ) );
                    } else if( z == 1 ) {
                        terrain.ter_set( stairs, ter_str_id( "t_stairs_down" ) );
                        terrain.ter_set( stairs + point::east, ter_str_id( "t_stairs_up" ) );
                    } else {
                        terrain.ter_set( stairs, ter_str_id( "t_stairs_down" ) );
                    }
                }
            }
        }
        // Tinymaps edit shared submaps but not the owning map's route caches.
        // As in the native-floorplan branch, refresh after constructing the
        // synthetic stair geometry and before creating the returning actor.
        here.load( here.get_abs_sub(), false );
        for( int z = 0; z <= home.z(); ++z ) {
            here.build_map_cache( z, true );
        }
    }
    migo_population::site_record population;
    population.id = id;
    population.anchor = anchor;
    migo_population::member_record member;
    member.token = {id, "returnee", 1, anchor};
    member.type = mtype_id( "mon_mi_go_scout" );
    member.home = home;
    member.owner = remote ? migo_population::representation::abstract :
                   migo_population::representation::local;
    member.assignment = migo_population::duty::returning;
    member.hp = member.type->hp;
    std::vector<migo_population::member_record> residents{member};
    if( occupied_approach ) {
        auto blocker = member;
        blocker.token.member_id = "home-defender";
        blocker.assignment = migo_population::duty::available;
        blocker.home = start + point::east;
        residents.push_back( blocker );
    }
    REQUIRE( population.commit_seed( residents, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = anchor;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = first;
    operation.members = {{member.token.member_id}};
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    site.operation = operation;
    site.remember_source( first );
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster original( member.type );
    original.set_pos_abs_only( start );
    original.population_identity() = member.token;
    original.set_moves( 0 );
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.phase = migo_tower::member_phase::returning;
    intent.home = home;
    original.set_migo_intent( intent );
    if( native_atmosphere ) {
        REQUIRE( original.is_immune_field( field_type_id( "fd_migo_atmosphere" ) ) );
    }
    const auto fire_stair = project_to<coords::ms>( tripoint_abs_omt( anchor.xy(), 1 ) ) +
                            tripoint( 5, 9, 0 );
    if( real_fire ) {
        here.furn_set( here.get_bub( fire_stair ), furn_str_id( "f_brazier" ) );
        here.add_field( here.get_bub( fire_stair ), field_type_id( "fd_fire" ), 1 );
    }
    if( remote ) {
        REQUIRE( owner.retain_monster_payload( start, original ) );
    } else {
        REQUIRE( g->place_critter_at( make_shared_fast<monster>( original ), here.get_bub( start ) ) );
    }
    if( occupied_approach ) {
        monster blocker( member.type );
        blocker.population_identity() = residents[1].token;
        blocker.set_pos_abs_only( residents[1].home );
        REQUIRE( owner.retain_monster_payload( blocker.pos_abs(), blocker ) );
    }
    const auto first_step = physical_navigation::read_step( original, home );
    INFO( first_step.reason );
    CHECK( first_step.next == physical_navigation::action::walk );
    CHECK( first_step.position.z() == start.z() );
    if( native_floorplan ) {
        CHECK( first_step.position.y() < start.y() );
    } else {
        CHECK( first_step.position.x() > start.x() );
    }
    bool reloaded = false;
    bool refused_missing_connection = false;
    std::string final_position;
    std::string final_reason;
    std::string final_geometry;
    for( int turn = 0; turn < 250; ++turn ) {
        calendar::turn += 1_turns;
        if( native_atmosphere ) {
            here.furniture_terrain_emit_fields();
            here.process_fields();
        }
        if( real_fire && turn == 80 ) {
            for( int z = 0; z < 3; ++z ) {
                for( const auto &tile : here.points_on_zlevel( z ) ) {
                    here.remove_field( tile, field_type_id( "fd_fire" ) );
                }
            }
            here.furn_set( here.get_bub( fire_stair ), furn_str_id::NULL_ID() );
        }
        if( remote ) {
            overmap_buffer.move_hordes();
        } else {
            process_monsters_and_npcs_turn_for_test();
        }
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        monster *current = nullptr;
        int copies = 0;
        int blockers = 0;
        auto inspect = [&]( monster &body ) {
            if( body.population_identity().site_id == id ) {
                if( body.population_identity().member_id == "returnee" ) {
                    current = &body;
                    ++copies;
                } else {
                    ++blockers;
                    CHECK( body.pos_abs() == residents[1].home );
                    CHECK( body.get_hp() == residents[1].hp );
                }
            }
        };
        for( monster &body : g->all_monsters() ) {
            inspect( body );
        }
        for( const auto &entry : owner.horde_entities() ) {
            if( entry.second->monster_data ) {
                inspect( *entry.second->monster_data );
            }
        }
        REQUIRE( copies == 1 );
        CHECK( blockers == ( occupied_approach ? 1 : 0 ) );
        REQUIRE( current );
        const auto *record = overmap_buffer.global_state.migo_populations.find( id )->find( "returnee" );
        REQUIRE( record );
        CHECK( current->get_hp() == member.hp );
        CHECK( record->token.epoch == current->population_identity().epoch );
        CHECK( record->token.site_id == current->population_identity().site_id );
        if( turn == 3 ) {
            std::ostringstream saved;
            { JsonOut json( saved ); current->serialize( json ); }
            monster loaded;
            loaded.deserialize( json_loader::from_string( saved.str() ) );
            *current = loaded;
            reloaded = true;
            CHECK( current->migo_intent()->phase == migo_tower::member_phase::returning );
        }
        if( real_fire && turn < 80 ) {
            CHECK( current->posz() < home.z() );
            CHECK( record->assignment == migo_population::duty::returning );
            CHECK_FALSE( overmap_buffer.global_state.migo_towers.find( id )->operation->members[0].returned );
        }
        if( record->assignment == migo_population::duty::available ) {
            CHECK( project_to<coords::omt>( current->pos_abs() ) == anchor );
            CHECK( current->pos_abs().z() == home.z() );
            CHECK( overmap_buffer.global_state.migo_towers.find( id )->operation->members[0].returned );
            break;
        }
        if( missing_stairs && current->pos_abs().xy() == home.xy() ) {
            // The existing ground route is usable, but sharing home's XY
            // is not arrival upstairs and cannot create a floor connection.
            const auto refused = physical_navigation::read_step( *current, home );
            CHECK( refused.next == physical_navigation::action::none );
            CHECK( current->pos_abs().z() == 0 );
            CHECK( current->migo_intent()->phase == migo_tower::member_phase::returning );
            refused_missing_connection = true;
            break;
        }
        final_position = current->pos_abs().to_string_writable();
        if( turn == 249 ) {
            const auto final_read = physical_navigation::read_step( *current, home );
            final_reason = final_read.reason;
            if( !native_floorplan && !remote ) {
                std::ostringstream details;
                details << "loaded origin=" << here.inbounds( current->pos_abs() )
                        << " goal=" << here.inbounds( home )
                        << " map=" << here.get_abs_sub().to_string()
                        << " read action=" << static_cast<int>( final_read.next )
                        << " known=" << final_read.known
                        << " next=" << final_read.position.to_string_writable()
                        << " cost=" << final_read.move_cost;
                const auto next = here.get_bub( final_read.position );
                const Creature *occupant = get_creature_tracker().creature_at( next );
                details << " native_reach=" << current->can_reach_to( next )
                        << " native_will=" << current->will_move_to( &here, next )
                        << " native_can=" << current->can_move_to( next )
                        << " native_cost=" << current->calc_movecost( here, current->pos_bub(), next )
                        << " moves=" << current->get_moves()
                        << " occupant=" << ( occupant != nullptr );
                if( const auto *npc = dynamic_cast<const class npc *>( occupant ) ) {
                    details << " npc=" << npc->getID().get_value();
                }
                for( int z = current->posz(); z <= home.z(); ++z ) {
                    const auto p = here.get_bub( tripoint_abs_ms( current->pos_abs().xy(), z ) );
                    details << " " << p.to_string() << ":" << here.ter( p ).id().str()
                            << ":floor=" << here.has_floor_or_water( p )
                            << ":move=" << current->will_move_to( &here, p );
                }
                final_geometry = details.str();
            }
            if( native_floorplan && !remote ) {
                std::ostringstream details;
                details << "native max_dist=" << current->get_pathfinding_settings().max_dist
                        << " max_length=" << current->get_pathfinding_settings().max_length;
                for( int z = current->posz(); z <= home.z(); ++z ) {
                    for( const auto &tile : here.points_in_radius( here.get_bub(
                             tripoint_abs_ms( current->pos_abs().xy(), z ) ), 2 ) ) {
                        details << " " << here.get_abs( tile ).to_string_writable()
                                << ":" << here.ter( tile ).id().str()
                                << "/" << here.furn( tile ).id().str()
                                << ":move=" << current->will_move_to( &here, tile )
                                << ":safe=" << current->know_danger_at( &here, tile );
                        for( const auto &f : here.field_at( tile ) ) {
                            details << ":field=" << f.first.id().str();
                        }
                    }
                }
                final_geometry = details.str();
            }
        }
    }
    CHECK( reloaded );
    CAPTURE( final_position, final_reason, final_geometry );
    const auto &finished = *overmap_buffer.global_state.migo_towers.find( id )->operation;
    const auto assignment = overmap_buffer.global_state.migo_populations.find( id )->find(
                                "returnee" )->assignment;
    const auto *final_member = overmap_buffer.global_state.migo_populations.find( id )->find( "returnee" );
    INFO( "scenario=" << scenario << " home=" << final_member->home.to_string_writable()
          << " owner=" << static_cast<int>( final_member->owner )
          << " epoch=" << final_member->token.epoch
          << " current=" << ( final_member->current_position ?
                              final_member->current_position->to_string_writable() : "absent" ) );
    if( missing_stairs ) {
        CHECK( refused_missing_connection );
        CHECK( assignment == migo_population::duty::returning );
        CHECK_FALSE( finished.members[0].returned );
        CHECK_FALSE( finished.resolved );
    } else {
        CHECK( assignment == migo_population::duty::available );
    }
}

TEST_CASE( "migo_native_arrival_keeps_motor_owned_until_service_commits",
           "[migo_tower][home_rally_native][arrival_commit]" )
{
    const auto old_turn = calendar::turn;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_bandits = overmap_buffer.global_state.bandit_live_world;
    clear_avatar();
    clear_map_without_vision( -1, 2 );
    on_out_of_scope restore( [&]() {
        clear_creatures();
        clear_avatar();
        clear_map_without_vision( -1, 2 );
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.global_state.bandit_live_world = old_bandits;
        set_time( old_turn );
    } );
    overmap_buffer.global_state.bandit_live_world = {};
    set_time( calendar::turn_zero + 100000_turns );
    map &here = get_map();
    get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
    const tripoint_bub_ms lower( 50, 50, 0 );
    const tripoint_bub_ms upper( 50, 50, 1 );
    here.ter_set( lower, ter_str_id( "t_stairs_up" ) );
    here.ter_set( upper, ter_str_id( "t_stairs_down" ) );
    const auto home = here.get_abs( upper );
    const auto anchor = project_to<coords::omt>( home );
    const std::string id = "native-arrival-commit";
    migo_population::member_record member;
    member.token = {id, "returnee", 1, anchor};
    member.type = mtype_id( "mon_mi_go_scout" );
    member.home = home;
    member.owner = migo_population::representation::local;
    member.assignment = migo_population::duty::returning;
    member.hp = member.type->hp;
    migo_population::site_record population;
    population.id = id;
    population.anchor = anchor;
    REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = anchor;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = anchor;
    operation.members = {{member.token.member_id}};
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    site.operation = operation;
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster *body = g->place_critter_at( member.type, lower );
    REQUIRE( body );
    body->population_identity() = member.token;
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.phase = migo_tower::member_phase::returning;
    intent.home = home;
    body->set_migo_intent( intent );
    body->set_moves( 0 ); // Initial fixture credit; the native next turn earns movement.
    process_monsters_and_npcs_turn_for_test();
    CHECK( body->pos_abs() == here.get_abs( lower ) );
    CHECK( body->get_moves() == 0 ); // First authenticated adoption grants no backlog.
    calendar::turn += 1_turns;
    process_monsters_and_npcs_turn_for_test();
    // Native120 credit pays the real100-cost stair. The remaining action
    // opportunity must not surrender the return motor to ordinary idle work.
    CHECK( body->pos_abs() == home );
    CHECK( body->migo_intent()->last_energy_world == to_turn<int>( calendar::turn ) );
    CHECK( body->migo_intent()->phase == migo_tower::member_phase::returning );
    CHECK( migo_tower::owns_intent( *body, migo_population::representation::local ) );
    auto *current = overmap_buffer.global_state.migo_populations.find( id );
    REQUIRE( current );
    CHECK( current->find( "returnee" )->assignment == migo_population::duty::returning );
    CHECK_FALSE( overmap_buffer.global_state.migo_towers.find( id )->operation->members[0].returned );
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    CHECK( current->find( "returnee" )->assignment == migo_population::duty::available );
    CHECK( body->migo_intent()->phase == migo_tower::member_phase::settled );
    CHECK( overmap_buffer.global_state.migo_towers.find( id )->operation->members[0].returned );
    const auto settled_pose = body->pos_abs();
    const int settled_moves = body->get_moves();
    const auto settled_token = body->population_identity();
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    CHECK( body->pos_abs() == settled_pose );
    CHECK( body->get_moves() == settled_moves );
    CHECK( body->population_identity().epoch == settled_token.epoch );
    CHECK( body->get_hp() == member.hp );
}

TEST_CASE( "migo_final_sweep_disposition_preserves_a_peers_actual_native_engagement",
           "[migo_tower][raid_sweep][native][ownership]" )
{
    const std::string completion = GENERATE( "arrival", "fire", "obstruction" );
    const bool fire_deferral = completion == "fire";
    const bool obstruction = completion == "obstruction";
    clear_avatar();
    clear_map_with_vision( -1, 1, false );
    const auto old_turn = calendar::turn;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    on_out_of_scope restore( [&]() {
        clear_creatures();
        clear_map_without_vision( -1, 1 );
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        set_time( old_turn );
    } );
    set_time_to_day();
    map &here = get_map();
    const auto home = here.get_abs( tripoint_bub_ms( 30, 60, 0 ) );
    const auto quiet_position = here.get_abs( tripoint_bub_ms( 60, 60, 0 ) );
    const auto engaged_position = here.get_abs( tripoint_bub_ms( 84, 60, 0 ) );
    const auto goal = quiet_position + point( completion == "arrival" ? 0 : 2, 0 );
    const auto anchor = project_to<coords::omt>( home );
    const std::string id = "last-candidate-combat-component";
    for( int y = 0; y < here.getmapsize() * SEEY; ++y ) {
        here.ter_set( tripoint_bub_ms( 72, y, 0 ), ter_str_id( "t_concrete_wall" ) );
    }
    if( fire_deferral ) {
        here.add_field( here.get_bub( goal ), field_type_id( "fd_fire" ), 3 );
    }
    if( obstruction ) {
        here.ter_set( here.get_bub( goal ), ter_str_id( "t_concrete_wall" ) );
    }
    get_avatar().setpos( here, tripoint_bub_ms( 94, 60, 0 ), false );
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    migo_population::site_record population;
    population.id = id;
    population.anchor = anchor;
    std::vector<migo_population::member_record> members;
    for( const auto &name : {"quiet", "engaged"} ) {
        migo_population::member_record member;
        member.token = {id, name, 1, anchor};
        member.type = mtype_id( "mon_mi_go" );
        member.home = home;
        member.hp = member.type->hp;
        member.owner = migo_population::representation::local;
        member.assignment = migo_population::duty::dispatched;
        members.push_back( member );
    }
    REQUIRE( population.commit_seed( members, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = id;
    site.anchor = anchor;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = project_to<coords::omt>( goal );
    operation.sweep.source = operation.source.source_omt;
    operation.sweep.generation = 1;
    operation.sweep.candidates = {{goal}};
    operation.members = {{"quiet"}, {"engaged"}};
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    site.operation = operation;
    REQUIRE( site.remember_source( operation.source.source_omt ) );
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    monster *quiet = g->place_critter_at( members[0].type, here.get_bub( quiet_position ) );
    monster *engaged = g->place_critter_at( members[1].type, here.get_bub( engaged_position ) );
    REQUIRE( quiet );
    REQUIRE( engaged );
    for( int i = 0; i < 2; ++i ) {
        monster &body = i == 0 ? *quiet : *engaged;
        body.population_identity() = members[i].token;
        migo_tower::member_intent intent;
        intent.generation = 1;
        intent.phase = migo_tower::member_phase::searching;
        intent.home = home;
        body.set_migo_intent( intent );
    }
    REQUIRE_FALSE( quiet->sees( here, get_avatar() ) );
    REQUIRE( engaged->sees( here, get_avatar() ) );
    REQUIRE( engaged->attitude( &get_avatar() ) == MATT_ATTACK );
    REQUIRE_FALSE( engaged->is_adjacent( &get_avatar(), true ) );
    REQUIRE( migo_tower::own_capability( *engaged ).ranged_range == 0 );
    // Population leases and corridor/fire are component setup. The actual
    // native planner chooses the visible hostile target; none is injected.
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    calendar::turn += 1_turns;
    engaged->plan();
    REQUIRE( engaged->get_dest() == get_avatar().pos_abs() );
    REQUIRE( engaged->migo_intent()->last_observed_enemy_world == to_turn<int>( calendar::turn ) );
    quiet->plan();
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    if( obstruction ) {
        // Hold executors while native plans keep the real visible enemy.
        // Each service receives one actual caller tick, not a clock jump or
        // fabricated progress; this is no whole-game elapsed-time proof.
        for( int tick = 1; tick < 1800; ++tick ) {
            calendar::turn += 1_turns;
            engaged->plan();
            quiet->plan();
            migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        }
    }
    auto *current = overmap_buffer.global_state.migo_towers.find( id );
    REQUIRE( current );
    REQUIRE_FALSE( current->operation->sweep.pending() );
    CHECK( current->operation->sweep.candidates.front().state ==
           ( fire_deferral ? raid_sweep::disposition::hazardous :
             obstruction ? raid_sweep::disposition::unreachable : raid_sweep::disposition::searched ) );
    engaged->plan();
    quiet->plan();
    CHECK( engaged->migo_intent()->phase == migo_tower::member_phase::searching );
    CHECK( engaged->get_dest() == get_avatar().pos_abs() );
    CHECK( quiet->migo_intent()->phase == migo_tower::member_phase::searching );
    CHECK_FALSE( current->operation->resolved );
    const int distance = rl_dist( engaged->pos_abs(), get_avatar().pos_abs() );
    engaged->set_moves( 100 );
    engaged->move();
    CHECK( rl_dist( engaged->pos_abs(), get_avatar().pos_abs() ) < distance );
    CHECK( engaged->migo_intent()->phase == migo_tower::member_phase::searching );
    // Native fear is a controlled condition, not a combat-victory claim.
    // Genuine flight latches return; recovery must not restart a far chase.
    engaged->morale = -100;
    REQUIRE( engaged->attitude( &get_avatar() ) == MATT_FLEE );
    engaged->plan();
    CHECK( engaged->migo_intent()->phase == migo_tower::member_phase::returning );
    engaged->morale = 30;
    engaged->plan();
    CHECK( engaged->migo_intent()->phase == migo_tower::member_phase::returning );
    CHECK( engaged->get_dest() != get_avatar().pos_abs() );
    quiet->plan();
    CHECK( quiet->migo_intent()->phase == migo_tower::member_phase::returning );
    for( int i = 0; i < 2; ++i ) {
        const auto &token = i == 0 ? quiet->population_identity() : engaged->population_identity();
        CHECK( token.site_id == members[i].token.site_id );
        CHECK( token.member_id == members[i].token.member_id );
        CHECK( token.epoch == members[i].token.epoch );
    }
}

TEST_CASE( "migo_native_sweep_crosses_a_slow_diagonal_with_native_distance_limits",
           "[migo_tower][raid_sweep][native][sweep_diagonal]" )
{
    const bool circular = GENERATE( false, true );
    const bool old_trigdist = trigdist;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_bandits = overmap_buffer.global_state.bandit_live_world;
    const auto old_turn = calendar::turn;
    clear_avatar();
    clear_map();
    trigdist = circular;
    overmap_buffer.global_state.migo_populations = {};
    overmap_buffer.global_state.migo_towers = {};
    overmap_buffer.global_state.bandit_live_world = {};
    on_out_of_scope restore( [&]() {
        clear_creatures();
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.global_state.bandit_live_world = old_bandits;
        trigdist = old_trigdist;
        set_time( old_turn );
        clear_map();
    } );
    set_time_to_day();
    get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
    const tripoint_bub_ms start( 100, 40, 0 );
    const tripoint_bub_ms destination( 52, 84, 0 );
    // The native scene stalled on this 48-by-44 diagonal with long grass.
    // Pure normal ground can use the native straight-line shortcut, masking
    // its regular pathfinder's circular max_dist guard.
    here.ter_set( tripoint_bub_ms( 76, 62, 0 ), ter_str_id( "t_grass_long" ) );
    monster *body = g->place_critter_at( mtype_id( "mon_mi_go" ), start );
    REQUIRE( body );
    const auto home = body->pos_abs();
    const auto goal = here.get_abs( destination );
    const auto anchor = project_to<coords::omt>( home );
    CAPTURE( circular, rl_dist( home, goal ), body->get_pathfinding_settings().max_dist );
    CHECK( rl_dist( home, goal ) == ( circular ? 65 : 48 ) );
    REQUIRE( body->get_pathfinding_settings().max_dist == 50 );
    migo_population::member_record member;
    member.token = {"slow-diagonal", "member", 1, anchor};
    member.type = body->type->id;
    member.home = home;
    member.owner = migo_population::representation::local;
    member.assignment = migo_population::duty::dispatched;
    member.hp = body->get_hp();
    migo_population::site_record population;
    population.id = member.token.site_id;
    population.anchor = anchor;
    REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = population.id;
    site.anchor = anchor;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = project_to<coords::omt>( goal );
    operation.members = {{member.token.member_id, false, false, home}};
    operation.sweep.source = operation.source.source_omt;
    operation.sweep.generation = 1;
    operation.sweep.candidates = {{goal}};
    operation.overdue_after_active = 100000;
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    site.operation = operation;
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    body->population_identity() = member.token;
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.home = home;
    body->set_migo_intent( intent );
    const auto first = migo_tower::prepare_step( *body, migo_population::representation::local );
    INFO( first.reason );
    REQUIRE( first.next == physical_navigation::action::walk );
    bool reloaded = false;
    for( int turn = 0; turn < 120; ++turn ) {
        calendar::turn += 1_turns;
        process_monsters_and_npcs_turn_for_test();
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        REQUIRE( body->get_hp() == member.hp );
        REQUIRE( body->population_identity().member_id == member.token.member_id );
        if( turn == 3 ) {
            std::ostringstream saved;
            { JsonOut json( saved ); body->serialize( json ); }
            monster loaded;
            loaded.deserialize( json_loader::from_string( saved.str() ) );
            *body = loaded;
            reloaded = true;
        }
        const auto &sweep = overmap_buffer.global_state.migo_towers.find(
                                population.id )->operation->sweep;
        if( sweep.candidates.front().state == raid_sweep::disposition::searched ) {
            CHECK( body->pos_abs() == goal );
            break;
        }
    }
    CHECK( reloaded );
    const auto &sweep = overmap_buffer.global_state.migo_towers.find(
                            population.id )->operation->sweep;
    CHECK( sweep.candidates.front().state == raid_sweep::disposition::searched );
    CHECK( sweep.cursor == 1 );
    CHECK( sweep.outcome() == "searched" );
}

TEST_CASE( "migo_queued_party_records_a_burning_destination_as_hazardous",
           "[migo_tower][raid_sweep][native][sweep_fire_queue]" )
{
    const bool contained = GENERATE( false, true );
    const bool occupied_queue = GENERATE( false, true );
    const bool old_trigdist = trigdist;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_bandits = overmap_buffer.global_state.bandit_live_world;
    const auto old_turn = calendar::turn;
    clear_avatar();
    clear_map();
    trigdist = true;
    overmap_buffer.global_state.migo_populations = {};
    overmap_buffer.global_state.migo_towers = {};
    overmap_buffer.global_state.bandit_live_world = {};
    on_out_of_scope restore( [&]() {
        clear_creatures();
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.global_state.bandit_live_world = old_bandits;
        trigdist = old_trigdist;
        set_time( old_turn );
        clear_map();
    } );
    set_time_to_day();
    get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
    const tripoint_bub_ms first( 60, 60, 0 );
    const tripoint_bub_ms destination( 108, 61, 0 );
    const auto goal = here.get_abs( destination );
    const auto anchor = project_to<coords::omt>( here.get_abs( first ) );
    migo_population::site_record population;
    population.id = "queued-burning-source";
    population.anchor = anchor;
    std::vector<migo_population::member_record> members;
    std::vector<monster *> bodies;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = project_to<coords::omt>( goal );
    operation.sweep.source = operation.source.source_omt;
    operation.sweep.generation = 1;
    operation.sweep.candidates = {{goal}, {goal + point( 0, 6 )}};
    operation.overdue_after_active = 100000;
    operation.advance( to_turn<int>( calendar::turn ), 0 );
    for( int i = 0; i < 3; ++i ) {
        monster *body = g->place_critter_at( mtype_id( "mon_mi_go" ),
                                           first + point( -i, occupied_queue ? 0 : 2 * i ) );
        REQUIRE( body );
        migo_population::member_record member;
        member.token = {population.id, "member" + std::to_string( i ), 1, anchor};
        member.type = body->type->id;
        member.home = body->pos_abs();
        member.owner = migo_population::representation::local;
        member.assignment = migo_population::duty::dispatched;
        member.hp = body->get_hp();
        members.push_back( member );
        bodies.push_back( body );
        operation.members.push_back( {member.token.member_id, false, false, member.home} );
        body->population_identity() = member.token;
        migo_tower::member_intent intent;
        intent.generation = 1;
        intent.home = member.home;
        body->set_migo_intent( intent );
    }
    REQUIRE( population.commit_seed( members, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state site;
    site.site_id = population.id;
    site.anchor = anchor;
    site.operation = operation;
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    if( contained ) {
        here.furn_set( destination, furn_str_id( "f_brazier" ) );
    }
    here.add_field( destination, field_type_id( "fd_fire" ), 3 );
    const auto leading = physical_navigation::read_step( *bodies.front(), goal );
    CAPTURE( contained, occupied_queue, leading.reason );
    CHECK( leading.next == physical_navigation::action::none );
    CHECK( leading.reason == "approach_fire_hazard" );
    // A truly available intermediate approach can still advance. A follower
    // whose next square is occupied cannot mask the leader's known fire refusal.
    calendar::turn += 1_turns;
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    CHECK( overmap_buffer.global_state.migo_towers.find( population.id )->operation->
           sweep.candidates.front().state == ( occupied_queue ? raid_sweep::disposition::hazardous :
                   raid_sweep::disposition::pending ) );
    bool reloaded = false;
    for( int turn = 0; turn < 1810; ++turn ) {
        calendar::turn += 1_turns;
        process_monsters_and_npcs_turn_for_test();
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        if( turn == 3 ) {
            std::ostringstream saved;
            { JsonOut json( saved ); bodies.back()->serialize( json ); }
            monster loaded;
            loaded.deserialize( json_loader::from_string( saved.str() ) );
            *bodies.back() = loaded;
            reloaded = true;
        }
        const auto &sweep = overmap_buffer.global_state.migo_towers.find(
                                population.id )->operation->sweep;
        if( turn >= 4 && sweep.candidates.front().state != raid_sweep::disposition::pending ) {
            break;
        }
    }
    const auto &sweep = overmap_buffer.global_state.migo_towers.find(
                            population.id )->operation->sweep;
    CHECK( reloaded );
    CHECK( sweep.candidates.front().state == raid_sweep::disposition::hazardous );
    CHECK( sweep.candidates.front().geometry_revision != 0 );
    for( int i = 0; i < 3; ++i ) {
        CHECK( bodies[i]->get_hp() == members[i].hp );
        CHECK( bodies[i]->population_identity().member_id == members[i].token.member_id );
        CHECK( bodies[i]->pos_abs() != goal );
    }
}

TEST_CASE( "service_preparation_keeps_current_reads_and_rejects_changed_bindings",
           "[migo_tower][navigation_service_preparation]" )
{
    clear_avatar();
    clear_map();
    const auto saved_turn = calendar::turn;
    const tripoint_abs_sm cell( 4000, 4000, 0 );
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( cell ) );
    on_out_of_scope cleanup( [&]() {
        MAPBUFFER.clear_outside_reality_bubble();
        CHECK( calendar::turn == saved_turn );
    } );
    auto terrain = std::make_unique<submap>();
    terrain->set_all_ter( ter_str_id( "t_floor" ) );
    REQUIRE( MAPBUFFER.add_submap( cell, terrain ) );
    // Pre-existing distant geometry allows a different actor/window to be
    // compared without changing any bindings of the first small window.
    for( int y = 1; y <= 3; ++y ) {
        auto bridge = std::make_unique<submap>();
        bridge->set_all_ter( ter_str_id( "t_floor" ) );
        REQUIRE( MAPBUFFER.add_submap( cell + point( 0, y ), bridge ) );
    }
    const auto count_before_reads = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto origin = project_to<coords::ms>( cell ) + point( 2, 6 );
    const auto goal = origin + point::east * 4;
    monster body( mtype_id( "mon_mi_go" ) );
    body.set_pos_abs_only( origin );
    body.set_moves( 100 );
    const auto body_bytes = [&]() {
        std::ostringstream out;
        JsonOut json( out );
        body.serialize( json );
        return out.str();
    };
    const auto before = body_bytes();
    const auto compare = [&]( const physical_navigation::step_read &actual,
                              const physical_navigation::step_read &expected ) {
        CHECK( actual.next == expected.next );
        CHECK( actual.position == expected.position );
        CHECK( actual.known == expected.known );
        CHECK( actual.reason == expected.reason );
        CHECK( actual.hazard_revision == expected.hazard_revision );
        CHECK( actual.move_cost == expected.move_cost );
    };
    {
        physical_navigation::preparation prepared;
        const auto first = physical_navigation::read_step( body, goal, {}, &prepared );
        REQUIRE( first.next == physical_navigation::action::walk );
        compare( physical_navigation::read_step( body, goal, {}, &prepared ), first );
        monster distant( body );
        distant.set_pos_abs_only( origin + point::south * 36 );
        const auto farther = physical_navigation::read_step( distant, goal );
        REQUIRE( farther.next == physical_navigation::action::walk );
        compare( physical_navigation::read_step( distant, goal, {}, &prepared ), farther );
        compare( physical_navigation::read_step( body, goal, {}, &prepared ), first );
        const auto occupied = [&]( const tripoint_abs_ms &p ) { return p == goal; };
        const auto refused = physical_navigation::read_step( body, goal, occupied, &prepared );
        REQUIRE( refused.next == physical_navigation::action::none );
        compare( refused, physical_navigation::read_step( body, goal, occupied ) );
        // No failed result is retained. Changed current occupancy reopens it.
        compare( physical_navigation::read_step( body, goal, {}, &prepared ), first );
        // Same window but a different full goal must not retain the old slot.
        compare( physical_navigation::read_step( body, goal + point::south, {}, &prepared ),
                 physical_navigation::read_step( body, goal + point::south ) );
        const auto unknown_goal = origin + point::east * 12;
        const auto unknown = physical_navigation::read_step( body, unknown_goal, {}, &prepared );
        REQUIRE_FALSE( unknown.known );
        CHECK( unknown.next == physical_navigation::action::none );
        const auto next_cell = cell + point::east;
        REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( next_cell ) );
        // Adversarial resident admission: the cached unknown binding must be
        // rejected, even though the original known resident pointer is stable.
        auto added = std::make_unique<submap>();
        added->set_all_ter( ter_str_id( "t_floor" ) );
        REQUIRE( MAPBUFFER.add_submap( next_cell, added ) );
        const auto reopened = physical_navigation::read_step( body, unknown_goal, {}, &prepared );
        REQUIRE( reopened.next == physical_navigation::action::walk );
        compare( reopened, physical_navigation::read_step( body, unknown_goal ) );
        monster different( mtype_id( "mon_mi_go_scout" ) );
        different.set_pos_abs_only( origin + point::south );
        compare( physical_navigation::read_step( different, goal, {}, &prepared ),
                 physical_navigation::read_step( different, goal ) );
    }
    CHECK( body_bytes() == before );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == count_before_reads + 1 );
    submap *current = MAPBUFFER.lookup_submap_cached( cell );
    REQUIRE( current );
    // Geometry/actions delimit preparation scopes. No workspace crosses these
    // real field/door edits or the later serialization/representation boundary.
    current->set_all_ter( ter_str_id( "t_wall" ) );
    for( int x = 2; x <= 6; ++x ) {
        current->set_ter( point_sm_ms( x, 6 ), ter_str_id( "t_floor" ) );
    }
    current->set_ter( point_sm_ms( 4, 6 ), ter_str_id( "t_door_c" ) );
    {
        physical_navigation::preparation prepared;
        REQUIRE( physical_navigation::read_step( body, goal, {}, &prepared ).next ==
                 physical_navigation::action::walk );
        monster cannot_open( mtype_id( "mon_bear_mutant_3headed" ) );
        REQUIRE( cannot_open.get_pathfinding_settings().max_dist >= 4 );
        REQUIRE_FALSE( cannot_open.has_flag( mon_flag_CAN_OPEN_DOORS ) );
        cannot_open.set_pos_abs_only( origin );
        const auto refused = physical_navigation::read_step( cannot_open, goal, {}, &prepared );
        CHECK( refused.next == physical_navigation::action::none );
        compare( refused, physical_navigation::read_step( cannot_open, goal ) );
    }
    // The actual native door commit occurs after the old workspace closes.
    monster at_door( body );
    at_door.set_pos_abs_only( origin + point::east );
    REQUIRE( physical_navigation::commit_door( at_door, origin + point::east * 2 ) );
    {
        physical_navigation::preparation fresh;
        compare( physical_navigation::read_step( body, goal, {}, &fresh ),
                 physical_navigation::read_step( body, goal ) );
    }
    current->get_field( point_sm_ms( 6, 6 ) ).add_field( field_type_id( "fd_fire" ), 1, 0_turns );
    ++current->field_count;
    {
        physical_navigation::preparation fresh;
        const auto fire = physical_navigation::read_step( body, goal, {}, &fresh );
        CHECK( fire.next == physical_navigation::action::none );
        CHECK( fire.reason == "approach_fire_hazard" );
        CHECK( fire.hazard_revision != 0 );
        compare( fire, physical_navigation::read_step( body, goal ) );
    }
    current->get_field( point_sm_ms( 6, 6 ) ).clear();
    current->field_count = 0;
    current->set_ter( point_sm_ms( 6, 7 ), ter_str_id( "t_floor" ) );
    for( const bool contained : {false, true} ) {
        CAPTURE( contained );
        current->set_furn( point_sm_ms( 6, 7 ), contained ? furn_str_id( "f_brazier" ) :
                           furn_str_id::NULL_ID() );
        current->get_field( point_sm_ms( 6, 7 ) ).add_field( field_type_id( "fd_fire" ), 1, 0_turns );
        current->field_count = 1;
        {
            physical_navigation::preparation fresh;
            const auto fire = physical_navigation::read_step( body, goal, {}, &fresh );
            CHECK( ( fire.next == physical_navigation::action::walk ) == contained );
            CHECK( ( fire.hazard_revision == 0 ) == contained );
            compare( fire, physical_navigation::read_step( body, goal ) );
        }
        current->get_field( point_sm_ms( 6, 7 ) ).clear();
        current->field_count = 0;
        current->set_furn( point_sm_ms( 6, 7 ), furn_str_id::NULL_ID() );
    }
    current->set_ter( point_sm_ms( 4, 6 ), ter_str_id( "t_stairs_up" ) );
    auto upper = std::make_unique<submap>();
    upper->set_all_ter( ter_str_id( "t_floor" ) );
    upper->set_ter( point_sm_ms( 4, 6 ), ter_str_id( "t_stairs_down" ) );
    REQUIRE( MAPBUFFER.add_submap( cell + tripoint_rel_sm( 0, 0, 1 ), upper ) );
    {
        physical_navigation::preparation fresh;
        const auto up_goal = goal + tripoint_rel_ms( 0, 0, 1 );
        const auto approach = physical_navigation::read_step( body, up_goal, {}, &fresh );
        CHECK( approach.next == physical_navigation::action::walk );
        CHECK( approach.position.z() == origin.z() );
        monster at_stair( body );
        at_stair.set_pos_abs_only( origin + point::east * 2 );
        const auto crossing = physical_navigation::read_step( at_stair, up_goal, {}, &fresh );
        CHECK( crossing.next == physical_navigation::action::walk );
        CHECK( crossing.position == at_stair.pos_abs() + tripoint_rel_ms( 0, 0, 1 ) );
        compare( crossing, physical_navigation::read_step( at_stair, up_goal ) );
    }
    MAPBUFFER.lookup_submap_cached( cell + tripoint_rel_sm( 0, 0, 1 ) )->set_ter(
        point_sm_ms( 4, 6 ), ter_str_id( "t_floor" ) );
    {
        monster below_goal( body );
        below_goal.set_pos_abs_only( goal );
        physical_navigation::preparation fresh;
        const auto missing = physical_navigation::read_step( below_goal,
                             goal + tripoint_rel_ms( 0, 0, 1 ), {}, &fresh );
        CHECK( missing.next == physical_navigation::action::none );
        compare( missing, physical_navigation::read_step( below_goal,
                 goal + tripoint_rel_ms( 0, 0, 1 ) ) );
    }
    std::ostringstream saved;
    { JsonOut json( saved ); body.serialize( json ); }
    monster reloaded;
    reloaded.deserialize( json_loader::from_string( saved.str() ) );
    {
        physical_navigation::preparation fresh;
        const auto after = physical_navigation::read_step( reloaded, goal, {}, &fresh );
        CHECK( after.next == physical_navigation::action::walk );
        compare( after, physical_navigation::read_step( reloaded, goal ) );
    }
    CHECK( body_bytes() == before );
}

TEST_CASE( "remote region service keeps physical refusal and reopening owners",
           "[migo_tower][navigation_service_preparation][navigation_region]" )
{
    clear_avatar();
    clear_map();
    const auto saved_turn = calendar::turn;
    const tripoint_abs_sm cell( 4400, 4400, 0 );
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( cell ) );
    on_out_of_scope cleanup( [&]() {
        MAPBUFFER.clear_outside_reality_bubble();
        CHECK( calendar::turn == saved_turn );
    } );
    auto terrain = std::make_unique<submap>();
    terrain->set_all_ter( ter_str_id( "t_floor" ) );
    for( int y = 0; y < SEEY; ++y ) {
        terrain->set_ter( point_sm_ms( 5, y ), ter_str_id( "t_wall_metal" ) );
    }
    REQUIRE( MAPBUFFER.add_submap( cell, terrain ) );
    submap *resident = MAPBUFFER.lookup_submap_cached( cell );
    REQUIRE( resident );
    const auto count = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto base = project_to<coords::ms>( cell );
    const auto origin = base + point( 2, 6 );
    const auto goal = base + point( 8, 6 );
    monster body( mtype_id( "mon_mi_go" ) );
    body.set_pos_abs_only( origin );
    body.set_moves( 100 );
    const auto serialize = [&]() {
        std::ostringstream out;
        JsonOut json( out );
        body.serialize( json );
        return out.str();
    };
    const auto before = serialize();
    const auto same = [&]( const physical_navigation::step_read &actual,
                           const physical_navigation::step_read &expected ) {
        CHECK( actual.next == expected.next );
        CHECK( actual.position == expected.position );
        CHECK( actual.reason == expected.reason );
        CHECK( actual.known == expected.known );
        CHECK( actual.move_cost == expected.move_cost );
        CHECK( actual.hazard_revision == expected.hazard_revision );
    };
    {
        physical_navigation::preparation service;
        for( int y : {6, 7, 6} ) {
            monster member( body );
            member.set_pos_abs_only( base + point( 2, y ) );
            const auto read = physical_navigation::read_step( member, goal, {}, &service );
            CHECK( read.next == physical_navigation::action::none );
            CHECK( read.known );
            CHECK( read.reason == "no_existing_safe_native_route" );
            same( read, physical_navigation::read_step( member, goal ) );
        }
        const auto unknown = physical_navigation::read_step( body, goal + point::east * SEEX,
                             {}, &service );
        CHECK_FALSE( unknown.known );
        CHECK( unknown.next == physical_navigation::action::none );
    }
    CHECK( serialize() == before );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == count );
    // Geometry edits end the old service lifetime. The certificate never owns
    // a retry clock or a failed result, so a new scope immediately reopens it.
    resident->set_ter( point_sm_ms( 5, 6 ), ter_str_id( "t_floor" ) );
    {
        physical_navigation::preparation reopened;
        const auto read = physical_navigation::read_step( body, goal, {}, &reopened );
        REQUIRE( read.next == physical_navigation::action::walk );
        const auto occupied = [&]( const auto &p ) { return p == goal; };
        CHECK( physical_navigation::read_step( body, goal, occupied, &reopened ).next ==
               physical_navigation::action::none );
        same( physical_navigation::read_step( body, goal, {}, &reopened ), read );
    }
    resident->get_field( point_sm_ms( 5, 6 ) ).add_field( field_type_id( "fd_fire" ), 1, 0_turns );
    resident->field_count = 1;
    {
        physical_navigation::preparation fire;
        const auto read = physical_navigation::read_step( body, goal, {}, &fire );
        CHECK( read.next == physical_navigation::action::none );
        CHECK( read.reason == "approach_fire_hazard" );
        CHECK( read.hazard_revision != 0 );
        same( read, physical_navigation::read_step( body, goal ) );
    }
    resident->get_field( point_sm_ms( 5, 6 ) ).clear();
    resident->field_count = 0;
    monster reloaded;
    reloaded.deserialize( json_loader::from_string( before ) );
    {
        physical_navigation::preparation reload;
        const auto read = physical_navigation::read_step( reloaded, goal, {}, &reload );
        CHECK( read.next == physical_navigation::action::walk );
        same( read, physical_navigation::read_step( body, goal ) );
    }
    CHECK( serialize() == before );
    CHECK( calendar::turn == saved_turn );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == count );
}

TEST_CASE( "service_preparation_rejects_vehicle_metadata_from_an_existing_only_load",
           "[migo_tower][navigation_service_preparation]" )
{
    clear_avatar();
    clear_map();
    REQUIRE( world_generator->active_world );
    const auto saved_turn = calendar::turn;
    const tripoint_abs_sm cell( 4200, 4200, 0 );
    const auto vehicle_owner = cell + point( 80, 0 );
    const auto segment = project_to<coords::seg>( project_to<coords::omt>( vehicle_owner ) );
    const auto directory = PATH_INFO::current_dimension_save_path() / "maps" /
                           string_format( "%d.%d.%d", segment.x(), segment.y(), segment.z() );
    REQUIRE_FALSE( std::filesystem::exists( directory.get_unrelative_path() ) );
    REQUIRE_FALSE( std::filesystem::exists( ( directory + ".zzip" ).get_unrelative_path() ) );
    on_out_of_scope cleanup( [&]() {
        MAPBUFFER.clear_outside_reality_bubble();
        std::filesystem::remove_all( directory.get_unrelative_path() );
        std::filesystem::remove( ( directory + ".zzip" ).get_unrelative_path() );
        CHECK( calendar::turn == saved_turn );
    } );
    auto terrain = std::make_unique<submap>();
    terrain->set_all_ter( ter_str_id( "t_floor" ) );
    REQUIRE( MAPBUFFER.add_submap( cell, terrain ) );
    {
        // Authoritative writer produces the actual compressed/plain quad;
        // its separate buffer is closed before the tested preparation starts.
        mapbuffer writer;
        auto remote = std::make_unique<submap>();
        remote->set_all_ter( ter_str_id( "t_floor" ) );
        auto machine = std::make_unique<vehicle>( vproto_id( "vehicle_camera_test" ) );
        REQUIRE( machine->part_count() > 0 );
        machine->sm_pos = vehicle_owner;
        machine->part( 0 ).mount = point_rel_ms( -80 * SEEX + 6, 6 );
        machine->refresh();
        const auto target = project_to<coords::ms>( vehicle_owner ) +
                            ( machine->abs_part_pos( machine->part( 0 ) ) - project_to<coords::ms>( machine->sm_pos ) );
        REQUIRE( project_to<coords::sm>( target ) == cell );
        remote->vehicles.push_back( std::move( machine ) );
        REQUIRE( writer.add_submap( vehicle_owner, remote ) );
        writer.save();
    }
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( vehicle_owner ) );
    REQUIRE( MAPBUFFER.vehicle_parts_at( cell ).empty() );
    monster body( mtype_id( "mon_mi_go" ) );
    const auto origin = project_to<coords::ms>( cell ) + point( 2, 6 );
    const auto goal = origin + point::east * 4;
    body.set_pos_abs_only( origin );
    physical_navigation::preparation prepared;
    REQUIRE( physical_navigation::read_step( body, goal, {}, &prepared ).next ==
             physical_navigation::action::walk );
    submap *stable = MAPBUFFER.lookup_submap_cached( cell );
    REQUIRE( MAPBUFFER.lookup_submap_existing( vehicle_owner ) );
    REQUIRE_FALSE( MAPBUFFER.vehicle_parts_at( cell ).empty() );
    CHECK( MAPBUFFER.lookup_submap_cached( cell ) == stable );
    const auto refused = physical_navigation::read_step( body, goal, {}, &prepared );
    CHECK( refused.next == physical_navigation::action::none );
    CHECK_FALSE( refused.known );
    CHECK( refused.reason == "origin_geometry_unknown" );
    const auto original = physical_navigation::read_step( body, goal );
    CHECK( refused.reason == original.reason );
    CHECK( refused.position == original.position );
}

TEST_CASE( "positive remote segments revalidate current paths without spending motion",
           "[migo_tower][navigation_segment]" )
{
    clear_avatar();
    clear_map();
    const auto saved_turn = calendar::turn;
    const tripoint_abs_sm cell( 4600, 4600, 0 );
    REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( cell ) );
    on_out_of_scope cleanup( [&]() {
        MAPBUFFER.clear_outside_reality_bubble();
        CHECK( calendar::turn == saved_turn );
    } );
    auto floor = std::make_unique<submap>();
    floor->set_all_ter( ter_str_id( "t_floor" ) );
    submap *terrain = floor.get();
    REQUIRE( MAPBUFFER.add_submap( cell, floor ) );
    const auto origin = project_to<coords::ms>( cell ) + point( 2, 6 );
    const auto goal = origin + point::east * 7;
    monster body( mtype_id( "mon_mi_go" ) );
    body.set_pos_abs_only( origin );
    body.set_moves( 100 );
    const auto serialize = [&]( const monster &actor ) {
        std::ostringstream out;
        JsonOut json( out );
        actor.serialize( json );
        return out.str();
    };
    const auto unchanged = serialize( body );
    physical_navigation::segment retained;
    const auto read = [&]( const tripoint_abs_ms &destination,
                          const std::function<bool( const tripoint_abs_ms & )> &occupied = {} ) {
        return physical_navigation::read_step( body, destination, occupied, nullptr, &retained );
    };
    const auto first = read( goal );
    REQUIRE( first.next == physical_navigation::action::walk );
    CHECK( retained.size() == 7 );
    CHECK( read( goal ).position == first.position );
    CHECK( retained.size() == 7 );
    CHECK( serialize( body ) == unchanged );
    monster failed_candidate( body );
    failed_candidate.set_pos_abs_only( first.position );
    physical_navigation::segment failed_copy( retained );
    CHECK( physical_navigation::read_step( failed_candidate, goal, {}, nullptr,
                                         &failed_copy ).next == physical_navigation::action::walk );
    CHECK( retained.size() == 7 );
    CHECK( read( goal ).position == first.position );
    // Current committed position, not a read result, consumes exactly one point.
    body.set_pos_abs_only( first.position );
    CHECK( read( goal ).position == origin + point::east * 2 );
    CHECK( retained.size() == 6 );
    const auto block = [&]( const tripoint_abs_ms &p ) { return p == goal; };
    CHECK( read( goal, block ).next == physical_navigation::action::none );
    CHECK( retained.size() == 0 ); // no retained refusal
    CHECK( read( goal ).next == physical_navigation::action::walk );
    CHECK( retained.size() > 0 );
    const auto alternate = origin + point::north * 2;
    CHECK( read( alternate ).next == physical_navigation::action::walk );
    CHECK( read( goal ).next == physical_navigation::action::walk );
    body.set_pos_abs_only( origin + point::south ); // off-segment => cold route
    const auto cold = physical_navigation::read_step( body, goal );
    CHECK( read( goal ).position == cold.position );
    CHECK( body.get_moves() == 100 );
    // No service view survives these real field/door/terrain mutations.
    body.set_pos_abs_only( origin );
    retained.clear();
    terrain->set_all_ter( ter_str_id( "t_wall" ) );
    for( int x = 0; x < SEEX; ++x ) {
        terrain->set_ter( point_sm_ms( x, 6 ), ter_str_id( "t_floor" ) );
    }
    terrain->set_ter( point_sm_ms( 5, 6 ), ter_str_id( "t_door_c" ) );
    const auto closed = read( goal );
    REQUIRE( closed.next == physical_navigation::action::walk );
    monster no_doors( mtype_id( "mon_bear_mutant_3headed" ) );
    no_doors.set_pos_abs_only( origin );
    const auto incapable = physical_navigation::read_step( no_doors, goal, {}, nullptr, &retained );
    CHECK( incapable.next == physical_navigation::action::none );
    CHECK( retained.size() == 0 );
    REQUIRE( read( goal ).next == physical_navigation::action::walk );
    monster opener( body );
    opener.set_pos_abs_only( origin + point::east * 2 );
    REQUIRE( physical_navigation::commit_door( opener, origin + point::east * 3 ) );
    CHECK( read( goal ).next == physical_navigation::action::walk );
    const point_sm_ms burning( 5, 6 );
    terrain->get_field( burning ).add_field( fd_fire, 1, 0_turns );
    ++terrain->field_count;
    const auto fire = read( goal );
    CHECK( fire.next == physical_navigation::action::none );
    CHECK( fire.reason == "approach_fire_hazard" );
    CHECK( retained.size() == 0 );
    terrain->get_field( burning ).remove_field( fd_fire );
    --terrain->field_count;
    CHECK( read( goal ).next == physical_navigation::action::walk );
    CHECK( body.pos_abs() == origin );
    CHECK( body.get_moves() == 100 );
}

TEST_CASE( "retained positive segments complete actual outward and home journeys",
           "[migo_tower][navigation_segment][native]" )
{
    const std::string layout = GENERATE( "open", "wall", "reopened", "same_turn_reopened",
                                         "alternate_shortcut", "adverse" );
    const int repetition = GENERATE( 0, 1, 2 );
    const bool detour = layout != "open";
    const bool reload = GENERATE( false, true );
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_bandits = overmap_buffer.global_state.bandit_live_world;
    const auto old_turn = calendar::turn;
    const bool old_trigdist = trigdist;
    on_out_of_scope restore( [&]() {
        clear_creatures();
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.global_state.bandit_live_world = old_bandits;
        set_time( old_turn );
        trigdist = old_trigdist;
        clear_map();
    } );
    struct journey_result {
        int cost = 0;
        int steps = 0;
        int outward_cost = 0;
        int outward_steps = 0;
        int peak_segment_points = 0;
        int revisited_squares = 0;
        int native_speed = 0;
        double cpu_seconds = 0;
        double wall_seconds = 0;
    };
    const auto journey = [&]( bool warm ) {
        CAPTURE( layout, reload, warm, repetition );
        clear_avatar();
        clear_map();
        clear_creatures();
        set_time_to_day();
        // Exercise circular native bounds; compare actual charged movement.
        trigdist = true;
        overmap_buffer.global_state.migo_populations = {};
        overmap_buffer.global_state.migo_towers = {};
        overmap_buffer.global_state.bandit_live_world = {};
        get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
        map &here = get_map();
        get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
        const tripoint_bub_ms start( 30, 50, 0 );
        const tripoint_bub_ms destination( 88, 50, 0 );
        for( const auto &p : here.points_in_rectangle( tripoint_bub_ms( 15, 15, 0 ),
                tripoint_bub_ms( 110, 90, 0 ) ) ) {
            here.ter_set( p, ter_str_id( "t_floor" ) );
            here.furn_set( p, furn_str_id::NULL_ID() );
        }
        if( detour ) {
            for( int y = 25; y <= 65; ++y ) {
                here.ter_set( tripoint_bub_ms( 35, y, 0 ), ter_str_id( "t_wall" ) );
            }
        }
        if( layout == "adverse" ) {
            for( int x = 20; x < 90; ++x ) {
                for( int y = 45; y <= 55; ++y ) {
                    if( x != 35 ) {
                        here.ter_set( tripoint_bub_ms( x, y, 0 ), ter_str_id( "t_dirtmound" ) );
                    }
                }
            }
        }
        monster *body = g->place_critter_at( mtype_id( "mon_mi_go" ), start );
        REQUIRE( body );
        const auto home = body->pos_abs();
        const auto goal = here.get_abs( destination );
        const auto anchor = project_to<coords::omt>( home );
        migo_population::member_record member;
        member.token = {"segment-journey", "member", 1, anchor};
        member.type = body->type->id;
        member.home = home;
        member.owner = migo_population::representation::local;
        member.assignment = migo_population::duty::dispatched;
        member.hp = body->get_hp();
        migo_population::site_record population;
        population.id = member.token.site_id;
        population.anchor = anchor;
        REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
        overmap_buffer.global_state.migo_populations.sites.push_back( population );
        migo_tower::site_state site;
        site.site_id = population.id;
        site.anchor = anchor;
        migo_tower::operation_record operation;
        operation.generation = 1;
        operation.source.source_omt = project_to<coords::omt>( goal );
        operation.members = {{member.token.member_id, false, false, home}};
        operation.sweep.source = operation.source.source_omt;
        operation.sweep.generation = 1;
        operation.sweep.candidates = {{goal}};
        operation.overdue_after_active = 100000;
        operation.advance( to_turn<int>( calendar::turn ), 0 );
        site.operation = operation;
        overmap_buffer.global_state.migo_towers.sites.push_back( site );
        body->population_identity() = member.token;
        migo_tower::member_intent intent;
        intent.generation = 1;
        intent.home = home;
        body->set_migo_intent( intent );
        journey_result result;
        result.native_speed = body->get_speed();
        bool reached = false;
        bool loaded = false;
        bool tested_positive = false;
        std::set<std::pair<int, tripoint_abs_ms>> visited;
        // Component planning + execution + settlement, excluding world setup,
        // output and input. Process CPU is not a native owner-stage timer.
        const auto wall_started = std::chrono::steady_clock::now();
        const auto cpu_started = std::clock();
        for( int turn = 0; turn < 240; ++turn ) {
            calendar::turn += 1_turns;
            if( ( layout == "reopened" || layout == "alternate_shortcut" ) && turn == 5 ) {
                // A new off-path shortcut must be assessed as a journey-cost
                // tradeoff; validating the old path does not discover it.
                here.ter_set( tripoint_bub_ms( 35, 50, 0 ), ter_str_id( "t_floor" ) );
                visited.clear();
            }
            if( layout == "alternate_shortcut" && turn == 12 ) {
                // A different off-path opening must not build unbounded route
                // history or mask the actual target after the first change.
                here.ter_set( tripoint_bub_ms( 35, 40, 0 ), ter_str_id( "t_floor" ) );
                visited.clear();
            }
            if( !warm ) {
                body->migo_intent()->navigation.clear();
            }
            const auto before = body->pos_abs();
            body->set_moves( 1000 );
            const auto prepared = migo_tower::prepare_step( *body,
                                  migo_population::representation::local );
            if( !tested_positive ) {
                REQUIRE( prepared.next == physical_navigation::action::walk );
                if( warm ) {
                    CHECK( body->migo_intent()->navigation.size() > 0 );
                    CHECK( body->migo_intent()->navigation.size() <= 60 );
                }
                tested_positive = true;
            }
            CHECK( body->pos_abs() == before );
            CHECK( body->get_moves() == 1000 );
            result.peak_segment_points = std::max( result.peak_segment_points,
                                         static_cast<int>( body->migo_intent()->navigation.size() ) );
            CHECK( body->migo_intent()->navigation.size() <= 60 );
            if( layout == "same_turn_reopened" && turn == 5 ) {
                // Real geometry owner changes an off-path connection after
                // preparation, with no clock advance before the next read.
                here.ter_set( tripoint_bub_ms( 35, 50, 0 ), ter_str_id( "t_floor" ) );
                visited.clear();
            }
            if( body->migo_intent()->phase == migo_tower::member_phase::settled ) {
                break;
            }
            if( !warm ) {
                body->migo_intent()->navigation.clear();
            }
            REQUIRE( migo_tower::plan_local( *body, false, nullptr ) );
            if( !warm ) {
                body->migo_intent()->navigation.clear();
            }
            const int phase = static_cast<int>( body->migo_intent()->phase );
            REQUIRE( migo_tower::move_local( *body ) );
            REQUIRE( body->pos_abs() != before );
            // Count backtracking rather than equating any revisit with an
            // endless loop. Opening a shortcut or a cold reload can select a
            // different finite path. The full journey must still settle in
            // the unchanged bounded loop below, including all charged steps.
            if( !visited.emplace( phase, body->pos_abs() ).second ) {
                ++result.revisited_squares;
            }
            const auto physical = here.get_bub( body->pos_abs() );
            CHECK( here.ter( physical ) != ter_str_id( "t_wall" ) );
            if( detour && physical.x() == 35 ) {
                CHECK( ( physical.y() < 25 || physical.y() > 65 ||
                         ( ( layout == "reopened" || layout == "same_turn_reopened" ||
                             layout == "alternate_shortcut" ) && turn >= 5 && physical.y() == 50 ) ||
                         ( layout == "alternate_shortcut" && turn >= 12 && physical.y() == 40 ) ) );
            }
            CHECK( body->get_hp() == member.hp );
            CHECK( body->population_identity().epoch == member.token.epoch );
            result.cost += 1000 - body->get_moves();
            ++result.steps;
            if( reload && turn == 8 ) {
                std::ostringstream saved;
                { JsonOut json( saved ); body->serialize( json ); }
                monster restored;
                restored.deserialize( json_loader::from_string( saved.str() ) );
                CHECK( restored.migo_intent()->navigation.size() == 0 );
                CHECK( restored.pos_abs() == body->pos_abs() );
                *body = restored;
                loaded = true;
            }
            if( !reached && body->pos_abs() == goal ) {
                reached = true;
                result.outward_cost = result.cost;
                result.outward_steps = result.steps;
            }
            if( reached ) {
                migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
            }
        }
        REQUIRE( reached );
        CHECK( loaded == reload );
        CHECK( body->migo_intent()->phase == migo_tower::member_phase::settled );
        CHECK( body->migo_intent()->navigation.size() == 0 );
        CHECK( project_to<coords::omt>( body->pos_abs() ) == anchor );
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 0 );
        const auto *settled = overmap_buffer.global_state.migo_populations.find( population.id );
        REQUIRE( settled );
        CHECK( settled->find( member.token.member_id )->owner == migo_population::representation::local );
        CHECK( settled->find( member.token.member_id )->assignment == migo_population::duty::available );
        CHECK( overmap_buffer.global_state.migo_towers.find( population.id )->operation->members.front().returned );
        result.cpu_seconds = static_cast<double>( std::clock() - cpu_started ) / CLOCKS_PER_SEC;
        result.wall_seconds = std::chrono::duration<double>( std::chrono::steady_clock::now() -
                              wall_started ).count();
        return result;
    };
    journey_result cold;
    journey_result warm;
    bool paired = false;
    SECTION( "cold_only" ) {
        cold = journey( false );
    }
    SECTION( "paired" ) {
        paired = true;
        if( repetition % 2 == 0 ) {
            cold = journey( false );
            warm = journey( true );
        } else {
            warm = journey( true );
            cold = journey( false );
        }
    }
    std::ofstream output( std::filesystem::path( PATH_INFO::user_dir() ) / "segment-journeys.jsonl",
                          std::ios::app );
    JsonOut json( output );
    json.start_object();
    json.member( "layout", layout );
    json.member( "reload", reload );
    json.member( "repetition", repetition );
    json.member( "paired", paired );
    json.member( "cold_cost", cold.cost );
    json.member( "warm_cost", warm.cost );
    json.member( "cold_steps", cold.steps );
    json.member( "warm_steps", warm.steps );
    json.member( "cold_outward_cost", cold.outward_cost );
    json.member( "warm_outward_cost", warm.outward_cost );
    json.member( "cold_outward_steps", cold.outward_steps );
    json.member( "warm_outward_steps", warm.outward_steps );
    json.member( "cold_cpu_seconds", cold.cpu_seconds );
    json.member( "warm_cpu_seconds", warm.cpu_seconds );
    json.member( "cold_wall_seconds", cold.wall_seconds );
    json.member( "warm_wall_seconds", warm.wall_seconds );
    json.member( "peak_segment_points", warm.peak_segment_points );
    json.member( "cold_native_speed", cold.native_speed );
    json.member( "warm_native_speed", warm.native_speed );
    json.member( "cold_revisited_squares", cold.revisited_squares );
    json.member( "warm_revisited_squares", warm.revisited_squares );
    json.member( "segment_value_bytes", sizeof( physical_navigation::segment ) );
    json.member( "logical_point_cost_payload_ceiling_bytes", 60 *
                 ( sizeof( tripoint_abs_ms ) + sizeof( int ) ) );
    json.end_object();
    output << '\n';
    CAPTURE( layout, reload, cold.cost, warm.cost, cold.steps, warm.steps );
    // Route-quality deltas are measured above, not an invented universal
    // exact-shortest-cost contract. Hard reachability, safety, bounded state,
    // actual settlement and loop negatives remain inside both real journeys.
    CHECK( cold.cost > 0 );
    CHECK( cold.steps < 240 );
    if( paired ) {
        CHECK( warm.cost > 0 );
        CHECK( warm.steps < 240 );
    }
}

TEST_CASE( "receiver observation records only a genuine bounded native scan",
           "[migo_tower][receiver_observation][physical_light][native]" )
{
    clear_avatar();
    clear_map_with_vision( -1, 4, false );
    clear_creatures();
    const auto old_turn = calendar::turn;
    const auto old_map_origin = get_map().get_abs_sub();
    const auto old_avatar_position = get_avatar().pos_abs();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    overmap_buffer.global_state.migo_populations.clear();
    overmap_buffer.global_state.migo_towers.clear();
    set_time_to_day();
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
    get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
    const auto anchor = project_to<coords::omt>( here.get_abs( tripoint_bub_ms( 60, 60, 0 ) ) );
    const auto source = project_to<coords::ms>( anchor ) + tripoint( 36, 12, 0 );
    const auto source_omt = project_to<coords::omt>( source );
    std::vector<std::pair<tripoint_abs_omt, oter_id>> terrain;
    on_out_of_scope restore( [&] {
        clear_creatures();
        for( const auto &entry : terrain ) {
            overmap_buffer.ter_set( entry.first, entry.second );
        }
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        get_avatar().unset_mutation( trait_id( "DEBUG_CLOAK" ) );
        get_map().load( old_map_origin, false, false );
        get_avatar().setpos( old_avatar_position, false );
        reset_live_light_sample_cache();
        set_time( old_turn );
    } );
    // Registry reset alone does not isolate this one-observer control:
    // existing native map markers/carriers remain discoverable. Temporarily
    // isolate other candidate ground markers in this scan's actual region;
    // restore all terrain, and leave native actors and receiver work alone.
    // Valid sections use source_omt or anchor, each with at most40 reach.
    for( int dx = -41; dx <= 41; ++dx ) {
        for( int dy = -41; dy <= 41; ++dy ) {
            const tripoint_abs_omt p( anchor.xy() + point( dx, dy ), 0 );
            if( p == anchor || p == source_omt ) {
                continue;
            }
            const auto old = overmap_buffer.ter_existing( p );
            const std::string type = old->get_type_id().str();
            if( type == "mi-go_scout_tower_1" || type == "mi-go_camp1" ) {
                terrain.emplace_back( p, old );
                overmap_buffer.ter_set( p, oter_str_id( "field" ) );
            }
        }
    }
    for( int z = 0; z < 4; ++z ) {
        const tripoint_abs_omt p( anchor.xy(), z );
        terrain.emplace_back( p, overmap_buffer.ter( p ) );
        overmap_buffer.ter_set( p, oter_str_id( "mi-go_scout_tower_" + std::to_string(
                z + 1 ) + "_north" ) );
        const tripoint_abs_omt q( source_omt.xy(), z );
        terrain.emplace_back( q, overmap_buffer.ter( q ) );
        overmap_buffer.ter_set( q, oter_str_id( "field" ) );
    }

    const auto geometry = migo_population::geometry_at( anchor );
    REQUIRE( geometry );
    migo_population::site_record population;
    population.id = geometry->id;
    population.anchor = anchor;
    const auto position = project_to<coords::ms>( anchor ) + tripoint( 6, 12, 1 );
    here.ter_set( here.get_bub( position ), ter_str_id( "t_floor" ) );
    monster *body = g->place_critter_at( mtype_id( "mon_mi_go" ), here.get_bub( position ) );
    REQUIRE( body );
    migo_population::member_record member;
    member.token = {population.id, "observed-resident", 1, anchor};
    member.type = body->type->id;
    member.home = position;
    member.owner = migo_population::representation::local;
    member.hp = body->get_hp();
    body->population_identity() = member.token;
    REQUIRE( population.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    for( int z = 0; z < 4; ++z ) {
        here.build_map_cache( z );
    }
    live_bandit_signal_observation sample;
    sample.sample_id = "exact-current-source";
    sample.mark.kind = "smoke";
    sample.source_ms = source;
    sample.source_omt = source_omt;
    sample.smoke_observable_ms = source + tripoint( 0, 0, 1 );
    sample.smoke_support_reason = "actual-test-plume";
    sample.range_cap_omt = 40;
    const int now = to_turn<int>( calendar::turn );
    sample.observed_turn = now;
    bool matched_contexts = false;
    std::string expected = "received";
    std::vector<live_bandit_signal_observation> samples = {sample, sample};
    samples[1].sample_id = "not-reached-after-success";
    SECTION( "actual emitted packet retains receiver across player and saved geometry" ) {
        const auto original_towers = overmap_buffer.global_state.migo_towers;
        const auto original_populations = overmap_buffer.global_state.migo_populations;
        here.add_field( here.get_bub( source ), field_type_id( "fd_smoke" ), 1 );
        get_avatar().setpos( source, false ); // Carrier source is genuinely sampled now.
        reset_live_light_sample_cache();
        run_live_light_delivery_for_test();
        const auto &actual = live_light::samples_for_turn( calendar::turn );
        const auto found = std::find_if( actual.begin(), actual.end(), [&]( const auto &packet ) {
            return packet.mark.kind == "smoke" && packet.source_ms == source;
        } );
        REQUIRE( found != actual.end() );
        REQUIRE( found->observed_turn == now );
        samples = { *found };
        overmap_buffer.global_state.migo_towers = original_towers;
        overmap_buffer.global_state.migo_populations = original_populations;
        matched_contexts = true;
    }
    SECTION( "positive and early success" ) {}
    SECTION( "full native token epoch remains exact" ) {
        auto &resident = overmap_buffer.global_state.migo_populations.sites.back().members.front();
        resident.token.epoch = std::numeric_limits<std::uint64_t>::max();
        body->population_identity() = resident.token;
    }
    SECTION( "first invalid input gates" ) {
        const std::string gate = GENERATE( "kind", "id", "turn", "range0", "range41", "projection", "smoke",
                                           "light" );
        if( gate == "kind" ) {
            samples[0].mark.kind = "noise";
            expected = "unsupported_kind";
        }
        if( gate == "id" ) {
            samples[0].sample_id.clear();
            expected = "empty_id";
        }
        if( gate == "turn" ) {
            --samples[0].observed_turn;
            expected = "observed_turn";
        }
        if( gate == "range0" ) {
            samples[0].range_cap_omt = 0;
            expected = "range_nonpositive";
        }
        if( gate == "range41" ) {
            samples[0].range_cap_omt = 41;
            expected = "range_above40";
        }
        if( gate == "projection" ) {
            samples[0].source_omt += point( 1, 0 );
            expected = "source_projection";
        }
        if( gate == "smoke" ) {
            samples[0].smoke_observable_ms.reset();
            expected = "smoke_support";
        }
        if( gate == "light" ) {
            samples[0].mark.kind = "light";
            expected = "light_support";
        }
        samples.resize( 1 );
    }
    SECTION( "actual resident first rejects" ) {
        const std::string gate = GENERATE( "unavailable", "saved_dead", "owner", "missing_body",
                                           "cannot_see", "friendly", "not_upper", "narcosis", "stunned", "downed", "onfire", "fleeing",
                                           "outside", "dead_body" );
        auto &resident = overmap_buffer.global_state.migo_populations.sites.back().members.front();
        if( gate == "unavailable" ) {
            resident.assignment = migo_population::duty::dispatched;
        }
        if( gate == "saved_dead" ) {
            resident.hp = 0;
        }
        if( gate == "owner" ) {
            resident.owner = migo_population::representation::dead;
        }
        if( gate == "missing_body" ) {
            body->population_identity().member_id = "different-owner";
        }
        if( gate == "cannot_see" ) {
            body->add_effect( efftype_id( "blind" ), 1_hours );
        }
        if( gate == "friendly" ) {
            body->friendly = 1;
        }
        if( gate == "not_upper" ) {
            body->setpos( here, here.get_bub( position + tripoint( 0, 0, -1 ) ) );
        }
        for( const std::string effect : {
                 "narcosis", "stunned", "downed", "onfire"
             } ) {
            if( gate == effect ) {
                body->add_effect( efftype_id( effect ), 1_hours );
            }
        }
        if( gate == "fleeing" ) {
            body->add_effect( efftype_id( "run" ), 1_hours );
            body->morale = -200;
        }
        if( gate == "outside" ) {
            const auto outside = position + tripoint( 48, 0, 0 );
            here.ter_set( here.get_bub( outside ), ter_str_id( "t_floor" ) );
            body->setpos( here, here.get_bub( outside ) );
        }
        if( gate == "dead_body" ) {
            body->set_hp( 0 );
        }
        // all_monsters is the native non-dead range: a dead local body is
        // absent from this lookup, rather than reaching the defensive dead check.
        expected = gate == "dead_body" ? "missing_body" : gate;
    }
    SECTION( "native blocked exit ray" ) {
        here.ter_set( here.get_bub( position + tripoint( 1, 0, 0 ) ), ter_str_id( "t_wall" ) );
        here.build_map_cache( 1 );
        expected = "exit_ray_refused";
    }
    SECTION( "contained light is not exposed" ) {
        samples.resize( 1 );
        samples[0].mark.kind = "light";
        samples[0].has_light_projection = true;
        samples[0].light_projection.packet.exposure =
            bandit_mark_generation::light_exposure_band::contained;
        expected = "light_support";
    }
    SECTION( "actual overmap reach refusal" ) {
        samples.resize( 1 );
        samples[0].range_cap_omt = 1;
        // Zero remaining brightness is visible. A real positive native see
        // cost above that budget supplies a refusal, rather than assuming >0.
        overmap_buffer.ter_set( tripoint_abs_omt( source_omt.xy(), 1 ), oter_str_id( "forest_thick" ) );
        expected = "detection_refused";
    }
    SECTION( "unknown existing corridor is not a ray failure" ) {
        overmap_buffer.ter_set( tripoint_abs_omt( source_omt.xy(), 1 ), oter_str_id::NULL_ID() );
        expected = "unknown_corridor";
    }
    SECTION( "source inside is skipped before optical work" ) {
        samples[0].source_ms = position + tripoint( 0, 0, -1 );
        samples[0].source_omt = anchor;
        expected = "source_inside";
    }
    SECTION( "remembered site no longer has native geometry" ) {
        // geometry_at is anchored by ground type, not by every upper floor.
        // An actual retained signal keeps the old anchor in the candidate set.
        migo_tower::site_state former;
        former.site_id = population.id;
        former.anchor = anchor;
        former.signal = migo_tower::signal_record{ sample.sample_id, sample.mark.kind,
            sample.source_omt, sample.source_ms, member.token.member_id, position, now - 300 };
        overmap_buffer.global_state.migo_towers.sites.push_back( former );
        overmap_buffer.ter_set( anchor, oter_str_id( "field" ) );
        expected = "no_geometry";
    }
    SECTION( "escaped output bound is enforced before copying" ) {
        samples.assign( 3, sample );
        for( auto &item : samples ) {
            item.mark.kind = "noise";
            item.sample_id.assign( 8000, '\x01' );
        }
        expected = "truncated";
    }
    SECTION( "bounded long identities are omitted whole" ) {
        samples[0].sample_id.assign( 100000, '\\' );
        expected = "truncated";
    }
    SECTION( "long support reason is omitted whole without changing receipt" ) {
        samples[0].smoke_support_reason.assign( 100000, 'q' );
        expected = "truncated";
    }
    SECTION( "sample count overflow does not stop receiver" ) {
        samples.assign( 100, sample );
        for( auto &item : samples ) {
            item.observed_turn = now - 1;
            item.source_ms.reset();
            item.smoke_observable_ms.reset();
        }
        expected = "truncated";
    }
    const auto before_towers = overmap_buffer.global_state.migo_towers;
    const auto before_populations = overmap_buffer.global_state.migo_populations;
    const auto snapshot = [&] {
        std::ostringstream out;
        JsonOut js( out );
        overmap_buffer.global_state.migo_towers.serialize( js );
        return out.str();
    };
    const auto body_snapshot = [&] {
        std::ostringstream out;
        JsonOut js( out );
        body->serialize( js );
        return out.str();
    };
    const auto original_body = body_snapshot();
    const auto original_position = get_avatar().pos_abs();
    const auto original_time = calendar::turn;
    const auto plain = migo_tower::receive_signals( samples, now );
    const auto plain_state = snapshot();
    const auto plain_towers = overmap_buffer.global_state.migo_towers;
    overmap_buffer.global_state.migo_towers = before_towers;
    overmap_buffer.global_state.migo_populations = before_populations;
    migo_tower::receiver_observation observed;
    const auto measured = migo_tower::receive_signals( samples, now, &observed );
    REQUIRE( observed.entered() );
    CHECK( plain.received == measured.received );
    CHECK( plain.eligible_observers == measured.eligible_observers );
    CHECK( plain.actual_sites == measured.actual_sites );
    CHECK( plain.committed_populations == measured.committed_populations );
    CHECK( snapshot() == plain_state );
    CHECK( body_snapshot() == original_body );
    CHECK( get_avatar().pos_abs() == original_position );
    CHECK( calendar::turn == original_time );
    std::ostringstream output;
    {
        JsonOut js( output );
        observed.serialize( js );
    }
    const auto report = output.str();
    INFO( "expected=" << expected << " report=" << report );
    CHECK( report.size() <= 65536 );
    std::istringstream in( report );
    TextJsonIn jsin( in );
    const auto object = jsin.get_object();
    object.allow_omitted_members();
    CHECK( object.get_int( "turn" ) == now );
    CHECK( object.get_int( "retained_storage_bytes" ) <= 65536 );
    CHECK( object.get_int( "capture_memory_bound" ) <= 65536 );
    CHECK_FALSE( object.get_bool( "storage_unavailable" ) );
    CHECK( object.get_bool( "result_available" ) );
    CHECK( report.size() <= static_cast<std::size_t>( object.get_int( "byte_bound" ) ) );
    bool saw_expected = expected == "truncated" && object.get_bool( "truncated" );
    for( const auto value : object.get_array( "records" ) ) {
        const auto record = value.get_object();
        record.allow_omitted_members();
        const std::string kind = record.get_string( "kind" );
        if( kind == "sample" ) {
            const auto index = static_cast<std::size_t>( record.get_int( "sample" ) );
            REQUIRE( index < samples.size() );
            CHECK( record.get_string( "id" ) == samples[index].sample_id );
            CHECK( record.get_string( "smoke_support" ) == samples[index].smoke_support_reason );
        }
        if( kind == "member" ) {
            const auto &resident = before_populations.sites.back().members.front();
            CHECK( record.get_string( "site" ) == population.id );
            CHECK( record.get_string( "member" ) == resident.token.member_id );
            CHECK( record.get_int( "member_owner" ) == static_cast<int>( resident.owner ) );
            CHECK( record.get_int( "member_duty" ) == static_cast<int>( resident.assignment ) );
            CHECK( record.get_int( "member_hp" ) == resident.hp );
            std::uint64_t epoch = 0;
            REQUIRE( record.read( "member_epoch", epoch ) );
            CHECK( epoch == resident.token.epoch );
            const std::string rejection = record.get_string( "first_rejection", "" );
            const bool matched = rejection != "unavailable" && rejection != "saved_dead" &&
                                 rejection != "owner" && rejection != "missing_body";
            CHECK( record.has_member( "matched_site" ) == matched );
            CHECK( record.has_member( "matched_member" ) == matched );
            CHECK( record.has_member( "matched_epoch" ) == matched );
            if( matched ) {
                const auto &token = body->population_identity();
                CHECK( record.get_string( "matched_site" ) == token.site_id );
                CHECK( record.get_string( "matched_member" ) == token.member_id );
                REQUIRE( record.read( "matched_epoch", epoch ) );
                CHECK( epoch == token.epoch );
                CHECK( record.get_int( "matched_anchor_x" ) == token.site_anchor.x() );
                CHECK( record.get_int( "matched_anchor_y" ) == token.site_anchor.y() );
                CHECK( record.get_int( "matched_anchor_z" ) == token.site_anchor.z() );
            }
            const bool reached_position = matched && rejection != "dead_body" &&
                                          rejection != "cannot_see" && rejection != "friendly";
            CHECK( record.has_member( "body_x" ) == reached_position );
            CHECK( record.has_member( "body_y" ) == reached_position );
            CHECK( record.has_member( "body_z" ) == reached_position );
            if( reached_position ) {
                CHECK( record.get_int( "body_x" ) == body->pos_abs().x() );
                CHECK( record.get_int( "body_y" ) == body->pos_abs().y() );
                CHECK( record.get_int( "body_z" ) == body->pos_abs().z() );
            }
        }
        if( record.get_string( "outcome" ) == expected ) { saw_expected = true; }
        if( kind == "member" && record.get_string( "first_rejection", "" ) == expected ) {
            saw_expected = true;
            CHECK( record.get_int( expected ) == 1 );
            CHECK( measured.eligible_observers == 0 );
            CHECK( measured.received == 0 );
        }
        if( kind == "attempt" && record.get_string( "outcome" ) == expected ) {
            if( expected == "exit_ray_refused" ) { CHECK( record.get_int( "ray_visible" ) == 0 ); }
            if( expected == "detection_refused" ) {
                CHECK( record.get_int( "detect_visible" ) == 0 );
                CHECK_FALSE( record.has_member( "ray_visible" ) );
            }
            if( expected == "source_inside" || expected == "unknown_corridor" ) {
                CHECK_FALSE( record.has_member( "ray_visible" ) );
                CHECK_FALSE( record.has_member( "detect_visible" ) );
            }
        }
    }
    CHECK( saw_expected );
    if( expected == "received" ) {
        REQUIRE( measured.received == 1 );
        int attempts = 0;
        for( const auto value : object.get_array( "records" ) ) {
            const auto record = value.get_object();
            record.allow_omitted_members();
            if( record.get_string( "kind" ) == "attempt" ) {
                ++attempts;
                CHECK( record.get_int( "sample" ) == 0 );
            }
        }
        CHECK( attempts == 1 );
    }
    if( expected == "truncated" ) {
        CHECK( object.get_bool( "truncated" ) );
    }
    migo_tower::receiver_observation skipped;
    CHECK( migo_tower::receive_signals( samples, now, &skipped ).received == 0 );
    CHECK_FALSE( skipped.entered() );
    CHECK( snapshot() == plain_state );
    // Reusing a populated sink cannot rewrite its source turn or identity.
    migo_tower::receive_signals( samples, now + 300, &observed );
    std::ostringstream again;
    {
        JsonOut js( again );
        observed.serialize( js );
    }
    CHECK( again.str() == report );
    observed = migo_tower::receiver_observation();
    CHECK_FALSE( observed.entered() );
    const auto original_samples = samples;
    samples[0].observed_turn = now + 600;
    migo_tower::receive_signals( samples, now + 600, &observed );
    REQUIRE( observed.entered() );
    std::ostringstream reset;
    {
        JsonOut js( reset );
        observed.serialize( js );
    }
    std::istringstream reset_in( reset.str() );
    TextJsonIn reset_json( reset_in );
    const auto reset_object = reset_json.get_object();
    reset_object.allow_omitted_members();
    CHECK( reset_object.get_int( "turn" ) == now + 600 );
    if( matched_contexts ) {
        const auto fixed_body = body_snapshot();
        const auto fixed_time = calendar::turn;
        const auto original_origin = here.get_abs_sub();
        const std::string old_savedir = PATH_INFO::savedir();
        const auto dictionary = ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path();
        const auto fixture_savedir = std::filesystem::path( old_savedir ) / "optical-migo-saved";
        REQUIRE_FALSE( std::filesystem::exists( fixture_savedir ) );
        PATH_INFO::set_savedir( fixture_savedir.string() + "/" );
        on_out_of_scope restore_savedir( [&]() {
            PATH_INFO::set_savedir( old_savedir );
            std::filesystem::remove_all( fixture_savedir );
        } );
        std::filesystem::create_directories( PATH_INFO::world_base_save_path().get_unrelative_path() );
        if( std::filesystem::exists( dictionary ) ) {
            std::filesystem::copy_file( dictionary,
                                       ( PATH_INFO::world_base_save_path() / "maps.dict" ).get_unrelative_path() );
        }
        const auto compare = [&]( const char *context ) {
            overmap_buffer.global_state.migo_towers = before_towers;
            overmap_buffer.global_state.migo_populations = before_populations;
            migo_tower::receiver_observation record;
            const auto result = migo_tower::receive_signals( original_samples, now, &record );
            INFO( "MiGo physical receiver context=" << context );
            CHECK( result.received == plain.received );
            CHECK( result.received > 0 );
            CHECK( snapshot() == plain_state );
            auto completed = plain_towers.sites.front().sources.front();
            REQUIRE( completed.finish( 1, 0, "empty", {}, false, {} ) );
            const auto &remote = overmap_buffer.global_state.migo_towers.sites.front();
            CHECK( remote.sources.front().latest_evidence == completed.completed_evidence );
            completed.latest_evidence = remote.sources.front().latest_evidence;
            CHECK_FALSE( completed.retry_ready( completed.retry_after_active, {}, remote.decision, false ) );
            CHECK( body_snapshot() == fixed_body );
            CHECK( calendar::turn == fixed_time );
            std::ostringstream serialized;
            { JsonOut json( serialized ); record.serialize( json ); }
            const auto report = json_loader::from_string( serialized.str() ).get_object();
            report.allow_omitted_members();
            bool received = false;
            for( const JsonObject &entry : report.get_array( "records" ) ) {
                entry.allow_omitted_members();
                if( entry.get_string( "kind" ) == "attempt" &&
                    entry.get_string( "outcome", "" ) == "received" ) {
                    CHECK( entry.get_int( "ray_known" ) == 1 );
                    CHECK( entry.get_int( "ray_visible" ) == 1 );
                    received = true;
                }
            }
            CHECK( received );
        };
        compare( "near resident" );
        here.load( original_origin + point( 20, 20 ), false, false );
        get_avatar().setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
        REQUIRE_FALSE( here.inbounds( source ) );
        REQUIRE_FALSE( here.inbounds( body->pos_abs() ) );
        compare( "far resident" );
        MAPBUFFER.save();
        MAPBUFFER.clear_outside_reality_bubble();
        REQUIRE_FALSE( MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( position ) ) );
        compare( "far saved" );
        here.load( original_origin, false, false );
        compare( "near reloaded" );
    }

}

TEST_CASE( "migo_response_reserve_fallback_uses_only_present_capable_members",
           "[migo_tower][home_rally]" )
{
    using namespace migo_tower;
    combat_profile ordinary;
    ordinary.known = true;
    ordinary.hp = 100;
    ordinary.speed = 100;
    ordinary.hit = 5;
    ordinary.dodge = 5;
    ordinary.melee.add_damage( damage_type_id( "bash" ), 20 );
    std::vector<candidate> present;
    for( int i = 0; i < 4; ++i ) {
        present.push_back( { "present-" + std::to_string( i ),
                             i == 2 ? "mon_mi_go_scout" : "mon_mi_go", ordinary } );
    }
    const auto selected = select_party( present, {}, 1, false );
    REQUIRE( selected.members.size() == 3 );
    CHECK( selected.projection.sufficient );
    present.resize( 3 );
    CHECK( select_party( present, {}, 1, false ).members.size() == 3 );
    present.back().capability.hp = 0;
    CHECK( select_party( present, {}, 1, false ).members.empty() );
    present.back().capability = ordinary;
    combat_profile overwhelming = ordinary;
    overwhelming.armor.set_resist( damage_type_id( "bash" ), 100 );
    CHECK( select_party( present, { overwhelming }, 1, false ).members.empty() );
    overwhelming.known = false;
    CHECK( select_party( present, { overwhelming }, 1, false ).reason ==
           "observed_capability_unknown" );
}

TEST_CASE( "native_migo_response_recalls_an_available_away_body_before_readiness",
           "[migo_tower][home_rally_native]" )
{
    const bool upper_home = GENERATE( false, true );
    const bool sufficient_present = GENERATE( false, true );
    CAPTURE( upper_home, sufficient_present );
    clear_avatar();
    clear_map_with_vision( -1, 4, false );
    clear_creatures();
    const auto old_turn = calendar::turn;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    overmap_buffer.global_state.migo_populations.clear();
    overmap_buffer.global_state.migo_towers.clear();
    set_time_to_day();
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
    get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
    const auto anchor = project_to<coords::omt>( here.get_abs( tripoint_bub_ms( 60, 60, 0 ) ) );
    std::vector<std::pair<tripoint_abs_omt, oter_id>> terrain;
    on_out_of_scope restore( [&]() {
        clear_creatures();
        for( const auto &entry : terrain ) {
            overmap_buffer.ter_set( entry.first, entry.second );
        }
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        clear_avatar();
        clear_map_without_vision( -1, 4 );
        set_time( old_turn );
    } );
    for( int z = 0; z < 4; ++z ) {
        const tripoint_abs_omt p( anchor.xy(), z );
        terrain.emplace_back( p, overmap_buffer.ter( p ) );
        overmap_buffer.ter_set( p, oter_str_id( "mi-go_scout_tower_" +
                                            std::to_string( z + 1 ) + "_north" ) );
    }
    const auto geometry = migo_population::geometry_at( anchor );
    REQUIRE( geometry );
    migo_population::site_record population;
    population.id = geometry->id;
    population.anchor = anchor;
    std::vector<migo_population::member_record> members;
    std::vector<monster *> bodies;
    const int member_count = sufficient_present ? 5 : !upper_home ? 4 : 3;
    for( int i = 0; i < member_count; ++i ) {
        const int x = i == member_count - 1 ? 85 :
                      !sufficient_present && i == 2 ? 109 : 60 + i;
        const tripoint_bub_ms position( x, 60, i == 0 ? 1 : 0 );
        here.ter_set( position, ter_str_id( "t_floor" ) );
        monster *body = g->place_critter_at( mtype_id( "mon_mi_go" ), position );
        REQUIRE( body );
        migo_population::member_record member;
        member.token = { geometry->id, "native-rally-" + std::to_string( i ), 1, anchor };
        member.type = body->type->id;
        member.home = here.get_abs( tripoint_bub_ms( 60 + i, 60,
                                   upper_home && i == member_count - 1 ? 3 : 0 ) );
        member.hp = body->get_hp();
        member.owner = migo_population::representation::local;
        member.current_position = body->pos_abs();
        body->population_identity() = member.token;
        members.push_back( member );
        bodies.push_back( body );
    }
    REQUIRE( population.commit_seed( members, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state state;
    state.site_id = geometry->id;
    state.anchor = anchor;
    const int now = to_turn<int>( calendar::turn );
    state.signal = migo_tower::signal_record{ "native-rally-signal", "smoke",
                   anchor + point( 2, 0 ), std::nullopt, members.front().token.member_id,
                   bodies.front()->pos_abs(), now };
    overmap_buffer.global_state.migo_towers.sites.push_back( state );
    for( int z : { 0, 1 } ) {
        here.build_map_cache( z );
    }
    const bool dispatched = migo_tower::begin_response( geometry->id, now );
    if( sufficient_present ) {
        REQUIRE( dispatched );
        const auto *actual = overmap_buffer.global_state.migo_towers.find( geometry->id );
        REQUIRE( actual->operation );
        REQUIRE( actual->response_home_rally );
        REQUIRE( actual->response_home_rally->find( members.back().token.member_id ) );
        CHECK( actual->operation->members.size() == 3 );
        CHECK_FALSE( actual->operation->contains( members.back().token.member_id ) );
        CHECK( actual->response_home_rally->find( members.back().token.member_id )->working() );
        CHECK( actual->response_home_rally->generation != actual->operation->generation );
        CHECK( actual->decision.required_home_reserve == 0 );
        CHECK( migo_tower::plan_local( *bodies.back(), false, nullptr ) );
        CHECK( bodies.back()->pos_abs() == *members.back().current_position );
        const auto generation = actual->operation->generation;
        CHECK_FALSE( migo_tower::begin_response( geometry->id, now ) );
        CHECK( actual->operation->generation == generation );
        if( !upper_home ) {
            overmap *owner = overmap_buffer.find_loaded_overmap( project_to<coords::om>( anchor ).xy() );
            REQUIRE( owner );
            on_out_of_scope release( [&]() {
                for( const auto &entry : owner->horde_entities() ) {
                    if( entry.second->monster_data &&
                        entry.second->monster_data->population_identity().site_id == geometry->id &&
                        std::any_of( members.begin(), members.end(), [&]( const auto &created ) {
                            return created.token.member_id == entry.second->monster_data->population_identity().member_id;
                        } ) ) {
                        owner->release_monster_payload( entry.first,
                                                       entry.second->monster_data->handoff_actor_id() );
                    }
                }
            } );
            const auto recalled_id = members.back().token.member_id;
            const auto ordinary = std::find_if( bodies.begin(), bodies.end(), [&]( const monster *body ) {
                return body != bodies.back() &&
                       !actual->operation->contains( body->population_identity().member_id );
            } );
            REQUIRE( ordinary != bodies.end() );
            const auto ordinary_id = ( *ordinary )->population_identity().member_id;
            const auto ordinary_position = ( *ordinary )->pos_abs();
            const auto operation_body = std::find_if( bodies.begin(), bodies.end(), [&]( const monster *body ) {
                return actual->operation->contains( body->population_identity().member_id );
            } );
            REQUIRE( operation_body != bodies.end() );
            const auto recalled_position = bodies.back()->pos_abs();
            REQUIRE( g->despawn_monster( **operation_body ) );
            REQUIRE( g->despawn_monster( **ordinary ) );
            REQUIRE( g->despawn_monster( *bodies.back() ) );
            // A distinct abstract resident can share the recalled actor's
            // transfer key. Rally must authenticate the Mi-go, not whichever
            // owner the coordinate lookup happens to encounter first.
            const auto collision_local = project_remain<coords::om>( recalled_position ).remainder_tripoint;
            auto collision = owner->hordes.spawn_entity( recalled_position, mtype_id( "mon_zombie" ), true );
            REQUIRE( collision );
            horde_entity *const other = &( *collision )->second;
            other->tracking_intensity = 1;
            other->destination = recalled_position;
            owner->hordes.refresh_entity_bucket( recalled_position, other );
            on_out_of_scope release_other( [&]() {
                auto found = owner->hordes.find( collision_local, other );
                if( found != owner->hordes.end() ) {
                    owner->hordes.erase( found );
                }
            } );
            const auto body_bytes = []( const monster &body ) {
                std::ostringstream out;
                { JsonOut json( out ); body.serialize( json ); }
                return out.str();
            };
            auto actual_recall = [&]() -> monster & {
                const auto *member = overmap_buffer.global_state.migo_populations.find(
                                         geometry->id )->find( recalled_id );
                REQUIRE( member->current_position );
                if( member->owner == migo_population::representation::abstract ) {
                    for( auto *entry : owner->entities_at( project_remain<coords::om>(
                                 *member->current_position ).remainder_tripoint ) ) {
                        if( entry->monster_data &&
                            entry->monster_data->population_identity().member_id == recalled_id ) {
                            return *entry->monster_data;
                        }
                    }
                    FAIL( "recalled abstract owner was not found" );
                }
                monster *local = nullptr;
                for( monster &body : g->all_monsters() ) {
                    if( body.population_identity().member_id == recalled_id ) {
                        local = &body;
                    }
                }
                REQUIRE( local );
                return *local;
            };
            auto *ordinary_entry = owner->entity_at(
                                       project_remain<coords::om>( ordinary_position ).remainder_tripoint );
            REQUIRE( ordinary_entry );
            REQUIRE( ordinary_entry->monster_data );
            const auto ordinary_before = body_bytes( *ordinary_entry->monster_data );
            const auto actor_omt = project_to<coords::omt>( recalled_position );
            get_avatar().setpos( project_to<coords::ms>( actor_omt - point( 40, 0 ) ) + point( 12, 12 ), false );
            set_time( calendar::turn + 1_turns );
            migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
            REQUIRE( actual->operation->activity.active );
            REQUIRE( actual->response_home_rally->find( recalled_id )->active );
            overmap_buffer.move_hordes(); // Operation and recall name this same non-nearby container.
            CHECK( actual_recall().pos_abs() != recalled_position );
            CHECK( actual_recall().migo_intent()->last_energy_world == to_turn<int>( calendar::turn ) );
            const auto once = body_bytes( actual_recall() );
            overmap_buffer.move_hordes();
            CHECK( body_bytes( actual_recall() ) == once );
            CHECK( body_bytes( *ordinary_entry->monster_data ) == ordinary_before );
            CHECK( overmap_buffer.global_state.migo_populations.find( geometry->id )->find(
                       ordinary_id )->assignment == migo_population::duty::available );
        }
        return;
    }
    REQUIRE_FALSE( dispatched );
    REQUIRE_FALSE( overmap_buffer.global_state.migo_towers.find( geometry->id )->operation );
    REQUIRE( project_to<coords::omt>( bodies.back()->pos_abs() ).xy() != anchor.xy() );
    const auto before = bodies.back()->pos_abs();
    const auto identity = bodies.back()->population_identity();
    REQUIRE( migo_tower::plan_local( *bodies.back(), false, nullptr ) );
    REQUIRE( bodies.back()->has_dest() );
    REQUIRE( bodies.back()->get_dest() != before );
    CHECK( bodies.back()->population_identity().site_id == identity.site_id );
    CHECK( bodies.back()->population_identity().member_id == identity.member_id );
    CHECK( bodies.back()->population_identity().epoch == identity.epoch );
    CHECK( bodies.back()->population_identity().site_anchor == identity.site_anchor );
    CHECK( bodies.back()->pos_abs() == before ); // Planning is not arrival/movement.
    CHECK_FALSE( overmap_buffer.global_state.migo_towers.find( geometry->id )->operation );
    const auto member_id = members.back().token.member_id;
    const auto site_id = geometry->id;
    auto population_now = [&]() -> migo_population::site_record & {
        return *overmap_buffer.global_state.migo_populations.find( site_id );
    };
    auto site_now = [&]() -> migo_tower::site_state & {
        return *overmap_buffer.global_state.migo_towers.find( site_id );
    };
    auto recall_now = [&]() -> raid_sweep::home_rally_member & {
        return *site_now().response_home_rally->find( member_id );
    };
    REQUIRE( site_now().response_home_rally );
    REQUIRE( site_now().response_home_rally->find( member_id ) );
    CHECK( recall_now().active_turns == 0 );
    const auto allowance = recall_now().allowance_turns;
    const auto signal_turn = site_now().signal->observed_turn;
    SECTION( "real native movement and arrival precede atomic admission" ) {
        monster &returnee = *bodies.back();
        int native_steps = 0;
        for( int turn = 0; turn < 80 && population_now().find( member_id )->assignment ==
             migo_population::duty::returning; ++turn ) {
            set_time( calendar::turn + 1_turns );
            returnee.process_turn();
            REQUIRE( migo_tower::plan_local( returnee, false, nullptr ) );
            for( int action = 0; action < 16 && returnee.get_moves() > 0 && returnee.has_dest(); ++action ) {
                const auto from = returnee.pos_abs();
                const auto moves_before = returnee.get_moves();
                REQUIRE( migo_tower::move_local( returnee ) );
                native_steps += returnee.pos_abs() != from;
                REQUIRE( ( returnee.pos_abs() != from || returnee.get_moves() < moves_before || !returnee.has_dest() ) );
            }
            migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        }
        CHECK( native_steps > 0 );
        CHECK( returnee.pos_abs() != before );
        REQUIRE( population_now().find( member_id )->assignment == migo_population::duty::available );
        CHECK( project_to<coords::omt>( returnee.pos_abs() ).xy() == anchor.xy() );
        CHECK( recall_now().outcome == raid_sweep::home_rally_outcome::arrived );
        CHECK( recall_now().allowance_turns == allowance );
        CHECK( site_now().signal->observed_turn == signal_turn );
        REQUIRE( migo_tower::begin_response( site_id, to_turn<int>( calendar::turn ) ) );
        REQUIRE( site_now().operation );
        CHECK( site_now().decision.required_home_reserve == 0 );
        CHECK( site_now().operation->contains( member_id ) );
        CHECK( recall_now().outcome == raid_sweep::home_rally_outcome::transferred );
        CHECK( returnee.migo_intent()->generation == site_now().operation->generation );
        const auto once_moves = returnee.get_moves();
        const auto once_hp = returnee.get_hp();
        returnee.process_turn(); // Same native world interval cannot fund again after transfer.
        CHECK( returnee.get_moves() == once_moves );
        CHECK( returnee.get_hp() == once_hp );
    }
    SECTION( "independent frozen body and episode survive native reload without banked work" ) {
        monster &returnee = *bodies.back();
        returnee.add_effect( efftype_id( "stunned" ), 30_minutes );
        const auto frozen_position = returnee.pos_abs();
        const auto actor_omt = project_to<coords::omt>( frozen_position );
        const tripoint_abs_omt far( actor_omt.xy() - point( 41, 0 ), actor_omt.z() );
        const auto bytes = []( const auto &value ) {
            std::ostringstream out;
            { JsonOut json( out ); value.serialize( json ); }
            return out.str();
        };
        get_avatar().setpos( project_to<coords::ms>( far ) + point( 12, 12 ), false );
        set_time( calendar::turn + 1_turns );
        migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
        REQUIRE( population_now().activity.active );
        REQUIRE_FALSE( recall_now().active );
        const auto frozen_body = bytes( returnee );
        const auto active_before = recall_now().active_turns;
        set_time( calendar::turn + 600_turns );
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 600 );
        returnee.process_turn();
        returnee.on_unload();
        returnee.on_load();
        CHECK( bytes( returnee ) == frozen_body );
        CHECK( recall_now().active_turns == active_before );
        const auto population_bytes = bytes( overmap_buffer.global_state.migo_populations );
        const auto policy_bytes = bytes( overmap_buffer.global_state.migo_towers );
        overmap_buffer.global_state.migo_populations.deserialize( json_loader::from_string( population_bytes ) );
        overmap_buffer.global_state.migo_towers.deserialize( json_loader::from_string( policy_bytes ) );
        returnee.deserialize( json_loader::from_string( frozen_body ) );
        returnee.on_load();
        CHECK( bytes( returnee ) == frozen_body );
        get_avatar().setpos( project_to<coords::ms>( far + point( 1, 0 ) ) + point( 12, 12 ), false );
        migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
        REQUIRE( recall_now().active );
        CHECK( recall_now().active_turns == active_before );
        REQUIRE( migo_tower::activity_for( returnee, migo_population::representation::local,
                 to_turn<int>( calendar::turn ), migo_tower::activity_boundary::load ) );
        const auto resumed = bytes( returnee );
        REQUIRE( migo_tower::activity_for( returnee, migo_population::representation::local,
                 to_turn<int>( calendar::turn ), migo_tower::activity_boundary::load ) );
        CHECK( bytes( returnee ) == resumed );
        CHECK( returnee.pos_abs() == frozen_position );
        CHECK( recall_now().allowance_turns == allowance );
        CHECK( site_now().signal->observed_turn == signal_turn );
        CHECK_FALSE( migo_tower::begin_response( site_id, to_turn<int>( calendar::turn ) ) );
        CHECK( recall_now().working() ); // Stale source cannot erase the genuine homeward trip.
        set_time( calendar::turn + 1_turns );
        migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
        returnee.process_turn();
        CHECK( recall_now().active_turns == active_before + 1 );
    }
    SECTION( "native priority refusal conserves owner and finite same-episode work" ) {
        monster &returnee = *bodies.back();
        returnee.add_effect( efftype_id( "stunned" ), 1_hours );
        const auto before_position = returnee.pos_abs();
        const auto token = returnee.population_identity();
        set_time( calendar::turn + 1_turns );
        const auto refused = migo_tower::prepare_step( returnee, migo_population::representation::local );
        CHECK( refused.next == physical_navigation::action::none );
        CHECK( refused.reason == "home_rally_native_priority" );
        CHECK( returnee.pos_abs() == before_position );
        CHECK( returnee.population_identity().epoch == token.epoch );
        CHECK( recall_now().working() );
        CHECK( recall_now().allowance_turns == allowance );
        returnee.remove_effect( efftype_id( "stunned" ) );
        REQUIRE( migo_tower::plan_local( returnee, false, nullptr ) );
        CHECK( returnee.get_dest() != before_position );
        const auto earned = recall_now().active_turns;
        const auto no_progress = recall_now().no_progress_turns;
        set_time( calendar::turn + time_duration::from_turns( no_progress ) );
        migo_tower::service_operations( to_turn<int>( calendar::turn ), no_progress );
        CHECK( returnee.pos_abs() == before_position );
        CHECK( recall_now().active_turns == earned + no_progress );
        CHECK( recall_now().outcome == raid_sweep::home_rally_outcome::expired );
        CHECK( population_now().find( member_id )->assignment == migo_population::duty::available );
        CHECK_FALSE( returnee.has_dest() );
        CHECK_FALSE( migo_tower::begin_response( site_id, to_turn<int>( calendar::turn ) ) );
        CHECK( recall_now().outcome == raid_sweep::home_rally_outcome::expired );
        CHECK( recall_now().allowance_turns == allowance );
    }
    SECTION( "switching response sources cannot replenish a spent native recall episode" ) {
        const bool empty_other = GENERATE( false, true );
        CAPTURE( empty_other );
        const auto serialize = []( const auto &value ) {
            std::ostringstream out;
            { JsonOut json( out ); value.serialize( json ); }
            return out.str();
        };
        const auto original_source = site_now().signal->source_omt;
        const auto original_episode = site_now().response_home_rally->episode;
        const int allowance = static_cast<int>( recall_now().no_progress_turns );
        for( monster *body : bodies ) {
            const auto *participant = site_now().response_home_rally->find(
                                          body->population_identity().member_id );
            if( participant && participant->working() ) {
                REQUIRE( migo_tower::plan_local( *body, false, nullptr ) );
                CHECK( participant->active );
                CHECK( participant->active_turns == 0 );
            }
        }
        set_time( calendar::turn + time_duration::from_turns( allowance ) );
        migo_tower::service_operations( to_turn<int>( calendar::turn ), allowance );
        REQUIRE( recall_now().outcome == raid_sweep::home_rally_outcome::expired );
        REQUIRE_FALSE( site_now().response_home_rally->pending() );
        if( empty_other ) {
            for( monster *body : bodies ) {
                if( project_to<coords::omt>( body->pos_abs() ).xy() != anchor.xy() ) {
                    body->add_effect( efftype_id( "stunned" ), 24_hours );
                }
            }
        }
        site_now().signal->source_omt += point( 0, 2 );
        site_now().signal->sample_id = "different-rally-source";
        site_now().signal->observed_turn = to_turn<int>( calendar::turn );
        CHECK_FALSE( migo_tower::begin_response( site_id, to_turn<int>( calendar::turn ) ) );
        if( empty_other ) {
            REQUIRE( site_now().response_home_rally );
            CHECK( site_now().response_home_rally->source == original_source );
            for( monster *body : bodies ) {
                body->remove_effect( efftype_id( "stunned" ) );
            }
        } else {
            REQUIRE( site_now().response_home_rally );
            REQUIRE( site_now().response_home_rally->pending() );
            for( monster *body : bodies ) {
                const auto *participant = site_now().response_home_rally->find(
                                              body->population_identity().member_id );
                if( participant && participant->working() ) {
                    REQUIRE( migo_tower::plan_local( *body, false, nullptr ) );
                    CHECK( participant->active );
                    CHECK( participant->active_turns == 0 );
                }
            }
            set_time( calendar::turn + time_duration::from_turns( allowance ) );
            migo_tower::service_operations( to_turn<int>( calendar::turn ), allowance );
            REQUIRE_FALSE( site_now().response_home_rally->pending() );
        }
        // Use the real source/population serializers. Reload cannot turn a
        // spent A episode into a new trip merely because B was most recent.
        const auto saved_towers = serialize( overmap_buffer.global_state.migo_towers );
        const auto saved_population = serialize( overmap_buffer.global_state.migo_populations );
        overmap_buffer.global_state.migo_towers.deserialize(
            json_loader::from_string( saved_towers ).get_object() );
        overmap_buffer.global_state.migo_populations.deserialize(
            json_loader::from_string( saved_population ).get_object() );
        site_now().signal->source_omt = original_source;
        site_now().signal->sample_id = "same-original-rally-source";
        site_now().signal->observed_turn = to_turn<int>( calendar::turn );
        CHECK_FALSE( migo_tower::begin_response( site_id, to_turn<int>( calendar::turn ) ) );
        REQUIRE( site_now().find_source( original_source ) );
        CHECK( site_now().find_source( original_source )->episode == original_episode );
        const auto *again = site_now().response_home_rally ?
                            site_now().response_home_rally->find( member_id ) : nullptr;
        CHECK_FALSE( ( again && again->working() ) );
    }
    SECTION( "only a genuinely reopened source can renew a native Mi-go recall" ) {
        const int interval = static_cast<int>( recall_now().no_progress_turns );
        for( monster *body : bodies ) {
            const auto *participant = site_now().response_home_rally->find(
                                          body->population_identity().member_id );
            if( participant && participant->working() ) {
                REQUIRE( migo_tower::plan_local( *body, false, nullptr ) );
            }
        }
        set_time( calendar::turn + time_duration::from_turns( interval ) );
        migo_tower::service_operations( to_turn<int>( calendar::turn ), interval );
        REQUIRE_FALSE( site_now().response_home_rally->pending() );
        const auto episode = site_now().response_home_rally->episode;
        const auto source = site_now().signal->source_omt;
        auto *history = site_now().find_source( source );
        REQUIRE( history );
        // Declared genuine source-owner extinction/receipt inputs. This
        // exercises native episode admission, not physical sensing itself.
        history->observed_active = false;
        history->extinct_world = to_turn<int>( calendar::turn );
        site_now().signal->observed_turn = to_turn<int>( calendar::turn );
        CHECK_FALSE( migo_tower::begin_response( site_id, to_turn<int>( calendar::turn ) ) );
        CHECK_FALSE( site_now().response_home_rally->pending() );
        CHECK( site_now().find_source( source )->episode == episode );
        set_time( calendar::turn + 1_turns );
        site_now().signal->observed_turn = to_turn<int>( calendar::turn );
        const auto position = bodies.back()->pos_abs();
        CHECK_FALSE( migo_tower::begin_response( site_id, to_turn<int>( calendar::turn ) ) );
        REQUIRE( site_now().response_home_rally->pending() );
        CHECK( site_now().response_home_rally->episode == episode + 1 );
        CHECK( recall_now().active_turns == 0 );
        CHECK( bodies.back()->pos_abs() == position );
    }
    SECTION( "an unseen authenticated member may join a spent episode only once" ) {
        const int interval = static_cast<int>( recall_now().no_progress_turns );
        for( monster *body : bodies ) {
            const auto *participant = site_now().response_home_rally->find(
                                          body->population_identity().member_id );
            if( participant && participant->working() ) {
                REQUIRE( migo_tower::plan_local( *body, false, nullptr ) );
            }
        }
        set_time( calendar::turn + time_duration::from_turns( interval ) );
        migo_tower::service_operations( to_turn<int>( calendar::turn ), interval );
        REQUIRE_FALSE( site_now().response_home_rally->pending() );
        monster &newly_away = *bodies[1]; // Existing genuine member, not a new population.
        const auto id = newly_away.population_identity().member_id;
        REQUIRE_FALSE( site_now().response_home_rally->find( id ) );
        const tripoint_bub_ms position( 86, 61, 0 );
        here.ter_set( position, ter_str_id( "t_floor" ) );
        newly_away.setpos( here, position );
        REQUIRE( migo_tower::activity_for( newly_away, migo_population::representation::local,
                                         to_turn<int>( calendar::turn ), migo_tower::activity_boundary::load ) );
        site_now().signal->observed_turn = to_turn<int>( calendar::turn );
        CHECK_FALSE( migo_tower::begin_response( site_id, to_turn<int>( calendar::turn ) ) );
        const auto *participant = site_now().response_home_rally->find( id );
        REQUIRE( participant );
        CHECK( participant->working() );
        CHECK( participant->active_turns == 0 );
        CHECK_FALSE( recall_now().working() );
        const auto allowance = participant->allowance_turns;
        const auto generation = site_now().response_home_rally->generation;
        CHECK_FALSE( migo_tower::begin_response( site_id, to_turn<int>( calendar::turn ) ) );
        CHECK( site_now().response_home_rally->find( id )->allowance_turns == allowance );
        CHECK( site_now().response_home_rally->generation == generation );
    }
    SECTION( "real native death ends recall without an invented arrival or loss report" ) {
        monster &returnee = *bodies.back();
        returnee.set_hp( 0 );
        returnee.check_dead_state( &here );
        REQUIRE( returnee.is_dead() );
        REQUIRE( population_now().find( member_id )->owner == migo_population::representation::dead );
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        CHECK( recall_now().outcome == raid_sweep::home_rally_outcome::unavailable );
        CHECK_FALSE( site_now().operation );
        CHECK( site_now().signal->observed_turn == signal_turn );
        const auto active = recall_now().active_turns;
        set_time( calendar::turn + 5_minutes );
        migo_tower::service_operations( to_turn<int>( calendar::turn ), 300 );
        CHECK( recall_now().active_turns == active );
        CHECK( recall_now().outcome == raid_sweep::home_rally_outcome::unavailable );
    }
    if( !upper_home ) {
        SECTION( "actual native handoff preserves the recall through abstract freeze and return" ) {
            const std::string fault = GENERATE( "none", "missing", "foreign" );
            CAPTURE( fault );
            const auto position = bodies.back()->pos_abs();
            overmap *owner = overmap_buffer.find_loaded_overmap( project_to<coords::om>( position ).xy() );
            REQUIRE( owner );
            on_out_of_scope release( [&]() {
                for( const auto &entry : owner->horde_entities() ) {
                    if( entry.second->monster_data &&
                        entry.second->monster_data->population_identity().site_id == site_id &&
                        std::any_of( members.begin(), members.end(), [&]( const auto &created ) {
                            return created.token.member_id == entry.second->monster_data->population_identity().member_id;
                        } ) ) {
                        owner->release_monster_payload( entry.first,
                                                       entry.second->monster_data->handoff_actor_id() );
                    }
                }
            } );
            const auto epoch = bodies.back()->population_identity().epoch;
            const auto peer_id = members[2].token.member_id;
            REQUIRE( peer_id != member_id );
            REQUIRE( g->despawn_monster( *bodies[2] ) );
            REQUIRE( g->despawn_monster( *bodies.back() ) );
            REQUIRE( population_now().find( member_id )->owner == migo_population::representation::abstract );
            CHECK( population_now().find( member_id )->token.epoch == epoch + 1 );
            auto payload_for = [&]( const std::string &id ) -> monster & {
                const auto *member = population_now().find( id );
                REQUIRE( member->current_position );
                if( member->owner == migo_population::representation::local ) {
                    monster *local = nullptr;
                    for( monster &candidate : g->all_monsters() ) {
                        if( candidate.population_identity().member_id == id &&
                            candidate.pos_abs() == *member->current_position ) {
                            local = &candidate;
                        }
                    }
                    REQUIRE( local );
                    REQUIRE( population_now().owns( local->population_identity(), member->owner ) );
                    return *local;
                }
                REQUIRE( member->owner == migo_population::representation::abstract );
                const auto *entry = owner->entity_at(
                                        project_remain<coords::om>( *member->current_position ).remainder_tripoint );
                REQUIRE( entry );
                REQUIRE( entry->monster_data );
                return *entry->monster_data;
            };
            auto payload = [&]() -> monster & { return payload_for( member_id ); };
            const auto native_callback = [&]() {
                overmap_buffer.move_hordes();
                // This fixture's geometry is loaded. A real abstract step
                // may materialize; follow that accepted native owner rather
                // than requiring the departed container to retain its copy.
                if( population_now().find( member_id )->owner == migo_population::representation::local &&
                    recall_now().working() ) {
                    monster &local = payload();
                    local.process_turn();
                    REQUIRE( migo_tower::plan_local( local, false, nullptr ) );
                    for( int action = 0; action < 16 && local.get_moves() > 0 && local.has_dest(); ++action ) {
                        REQUIRE( migo_tower::move_local( local ) );
                    }
                }
            };
            const auto bytes = []( const monster &body ) {
                std::ostringstream out;
                { JsonOut json( out ); body.serialize( json ); }
                return out.str();
            };
            REQUIRE( migo_tower::owns_intent( payload(), migo_population::representation::abstract ) );
            const auto actor_omt = project_to<coords::omt>( position );
            const tripoint_abs_omt far( actor_omt.xy() - point( 41, 0 ), actor_omt.z() );
            get_avatar().setpos( project_to<coords::ms>( far ) + point( 12, 12 ), false );
            set_time( calendar::turn + 1_turns );
            migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
            REQUIRE_FALSE( recall_now().active );
            const auto frozen = bytes( payload() );
            const auto frozen_peer = bytes( payload_for( peer_id ) );
            const auto peer_active = site_now().response_home_rally->find( peer_id )->active_turns;
            const auto active = recall_now().active_turns;
            set_time( calendar::turn + 600_turns );
            migo_tower::service_operations( to_turn<int>( calendar::turn ), 600 );
            overmap_buffer.move_hordes();
            CHECK( bytes( payload() ) == frozen );
            CHECK( recall_now().active_turns == active );
            get_avatar().setpos( project_to<coords::ms>( far + point( 1, 0 ) ) + point( 12, 12 ), false );
            migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
            REQUIRE( recall_now().active );
            REQUIRE_FALSE( site_now().response_home_rally->find( peer_id )->active );
            CHECK( rl_dist( get_avatar().pos_abs_omt().xy(),
                            project_to<coords::omt>( payload_for( peer_id ).pos_abs() ).xy() ) == 41 );
            CHECK( recall_now().active_turns == active );
            if( fault != "none" ) {
                const auto recorded_epoch = population_now().find( member_id )->token.epoch;
                const auto current = payload().pos_abs();
                std::string refused_body;
                if( fault == "missing" ) {
                    // The retained metadata alone must not substitute for an
                    // absent native owner. This is a deliberate absence control.
                    REQUIRE( owner->release_monster_payload( current, payload().handoff_actor_id() ) );
                } else {
                    // A declared stale/foreign saved payload, never a way to
                    // bind a positive actor or rewrite its canonical lease.
                    ++payload().population_identity().epoch;
                    refused_body = bytes( payload() );
                }
                set_time( calendar::turn + 1_turns );
                migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
                overmap_buffer.move_hordes();
                CHECK_FALSE( recall_now().active );
                CHECK( recall_now().active_turns == active );
                CHECK( recall_now().working() );
                CHECK( population_now().find( member_id )->assignment == migo_population::duty::returning );
                CHECK( population_now().find( member_id )->token.epoch == recorded_epoch );
                CHECK( bytes( payload_for( peer_id ) ) == frozen_peer );
                if( fault == "missing" ) {
                    CHECK_FALSE( owner->entity_at( project_remain<coords::om>( current ).remainder_tripoint ) );
                } else {
                    CHECK( bytes( payload() ) == refused_body );
                }
                return;
            }
            int steps = 0;
            for( int turn = 0; turn < 80 && recall_now().working(); ++turn ) {
                const auto from = payload().pos_abs();
                set_time( calendar::turn + 1_turns );
                migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
                native_callback();
                steps += payload().pos_abs() != from;
            }
            CHECK( steps > 0 );
            CHECK( recall_now().outcome == raid_sweep::home_rally_outcome::arrived );
            CHECK( population_now().find( member_id )->assignment == migo_population::duty::available );
            CHECK( project_to<coords::omt>( payload().pos_abs() ).xy() == anchor.xy() );
            CHECK( payload().population_identity().epoch == epoch + 2 ); // Accepted native materialization.
            CHECK( recall_now().allowance_turns == allowance );
            CHECK( bytes( payload_for( peer_id ) ) == frozen_peer );
            CHECK( site_now().response_home_rally->find( peer_id )->active_turns == peer_active );
            CHECK( site_now().response_home_rally->find( peer_id )->working() );
            const auto once = bytes( payload() );
            overmap_buffer.move_hordes();
            overmap_buffer.move_hordes();
            CHECK( bytes( payload() ) == once );
            CHECK( bytes( payload_for( peer_id ) ) == frozen_peer );

        }
    }
}

TEST_CASE( "response_dispatch_settles_the_previous_native_body_interval",
           "[migo_tower][home_rally_dispatch_settlement]" )
{
    const bool abstract = GENERATE( false, true );
    const bool legacy = GENERATE( false, true );
    CAPTURE( abstract, legacy );
    clear_avatar();
    clear_map_with_vision( -1, 4, false );
    clear_creatures();
    const auto old_turn = calendar::turn;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    overmap_buffer.global_state.migo_populations.clear();
    overmap_buffer.global_state.migo_towers.clear();
    set_time( calendar::turn_zero + 100000_turns );
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
    const auto player_position = get_avatar().pos_abs();
    const auto anchor = project_to<coords::omt>( here.get_abs( tripoint_bub_ms( 60, 60, 0 ) ) );
    std::vector<std::pair<tripoint_abs_omt, oter_id>> terrain;
    overmap *owner = nullptr;
    std::vector<migo_population::member_record> members;
    on_out_of_scope restore( [&]() {
        clear_creatures();
        if( owner ) {
            for( const auto &member : members ) {
                owner->release_monster_payload( *member.current_position, member.token.member_id );
            }
        }
        for( const auto &entry : terrain ) {
            overmap_buffer.ter_set( entry.first, entry.second );
        }
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        clear_avatar();
        clear_map_without_vision( -1, 4 );
        set_time( old_turn );
    } );
    for( int z = 0; z < 4; ++z ) {
        const tripoint_abs_omt p( anchor.xy(), z );
        terrain.emplace_back( p, overmap_buffer.ter( p ) );
        overmap_buffer.ter_set( p, oter_str_id( "mi-go_scout_tower_" +
                                            std::to_string( z + 1 ) + "_north" ) );
    }
    const auto geometry = migo_population::geometry_at( anchor );
    REQUIRE( geometry );
    migo_population::site_record population;
    population.id = geometry->id;
    population.anchor = anchor;
    std::array<monster *, 3> bodies;
    for( int i = 0; i < 3; ++i ) {
        const tripoint_bub_ms p( 60 + i, 60, i == 0 ? 1 : 0 );
        here.ter_set( p, ter_str_id( "t_floor" ) );
        migo_population::member_record member;
        member.token = {geometry->id, "dispatch-pause-" + std::to_string( i ), 1, anchor};
        member.type = mtype_id( "mon_mi_go" );
        member.home = here.get_abs( p );
        member.current_position = member.home;
        member.hp = member.type->hp;
        member.owner = abstract && i > 0 ? migo_population::representation::abstract :
                       migo_population::representation::local;
        members.push_back( member );
        if( member.owner == migo_population::representation::local ) {
            bodies[i] = g->place_critter_at( member.type, p );
            REQUIRE( bodies[i] );
            bodies[i]->population_identity() = member.token;
        }
    }
    REQUIRE( population.commit_seed( members, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    owner = &overmap_buffer.get( project_to<coords::om>( anchor ).xy() );
    if( abstract ) {
        for( int i : {1, 2} ) {
            // Genuine legacy ABSTRACT fixture births, not transformed LOCAL
            // copies. The existing native container owns their first payload.
            monster body( members[i].type );
            body.population_identity() = members[i].token;
            body.set_pos_abs_only( members[i].home );
            auto *entry = owner->retain_monster_payload( members[i].home, body );
            REQUIRE( entry );
            REQUIRE( entry->monster_data );
            bodies[i] = entry->monster_data.get();
        }
    }
    migo_tower::site_state site;
    site.site_id = geometry->id;
    site.anchor = anchor;
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    auto home = [&]() -> auto & {
        return *overmap_buffer.global_state.migo_populations.find( geometry->id );
    };
    auto state = [&]() -> auto & {
        return *overmap_buffer.global_state.migo_towers.find( geometry->id );
    };
    const auto serialize = []( const monster &body ) {
        std::ostringstream out;
        { JsonOut json( out ); body.serialize( json ); }
        return out.str();
    };
    const auto suspension = [&]( const monster &body ) {
        const auto document = json_loader::from_string( serialize( body ) );
        const auto object = document.get_object();
        const auto consume = []( const auto &self, const JsonValue &value ) -> void {
            if( value.test_object() ) {
                for( const JsonMember child : value.get_object() ) {
                    self( self, child );
                }
            } else if( value.test_array() ) {
                for( const JsonValue child : value.get_array() ) {
                    self( self, child );
                }
            } else if( value.test_string() ) {
                ( void )value.get_string();
            } else if( value.test_bool() ) {
                ( void )value.get_bool();
            } else if( value.test_number() ) {
                ( void )value.get_float();
            } else {
                REQUIRE( value.test_null() );
            }
        };
        for( const JsonMember member : object ) {
            // Consume each actual complete native value; no blanket omission
            // setting can hide a missing field or change in this body record.
            consume( consume, member );
        }
        time_duration value;
        REQUIRE( object.read( "suspended_evolution_time", value ) );
        return value;
    };
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    if( !legacy ) {
        REQUIRE( migo_tower::activity_for( *bodies[1], members[1].owner,
                                         to_turn<int>( calendar::turn ), migo_tower::activity_boundary::load ) );
    }
    const auto before = suspension( *bodies[1] );
    const auto frozen_body = serialize( *bodies[1] );
    get_avatar().setpos( project_to<coords::ms>( anchor + point( 41, 0 ) ), false );
    set_time( calendar::turn + 1_turns );
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    REQUIRE_FALSE( home().activity.active );
    set_time( calendar::turn + 600_turns );
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    CHECK( serialize( *bodies[1] ) == frozen_body );
    get_avatar().setpos( player_position, false );
    set_time( calendar::turn + 1_turns );
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    state().signal = migo_tower::signal_record{"dispatch-pause", "smoke", anchor + point( 2, 0 ),
                     std::nullopt, members.front().token.member_id, bodies.front()->pos_abs(),
                     to_turn<int>( calendar::turn )};
    for( int z : {0, 1} ) {
        here.build_map_cache( z );
    }
    REQUIRE( migo_tower::begin_response( geometry->id, to_turn<int>( calendar::turn ) ) );
    REQUIRE( state().operation );
    CHECK( state().operation->contains( members[1].token.member_id ) );
    // The new operation owns only its new interval. Earlier legacy absence or
    // frozen home debt must already be settled in the real selected body.
    CHECK( suspension( *bodies[1] ) > before );
    if( !legacy ) {
        CHECK( suspension( *bodies[1] ) == before +
               time_duration::from_turns( home().activity.suspended_turns ) );
    }
    const auto committed = serialize( *bodies[1] );
    CHECK_FALSE( migo_tower::begin_response( geometry->id, to_turn<int>( calendar::turn ) ) );
    CHECK( serialize( *bodies[1] ) == committed );
    REQUIRE( migo_tower::activity_for( *bodies[1], members[1].owner,
                                     to_turn<int>( calendar::turn ), migo_tower::activity_boundary::load ) );
    CHECK( serialize( *bodies[1] ) == committed );
}

TEST_CASE( "compact_rally_source_facts_are_bounded_and_eviction_cannot_grant_a_trip",
           "[migo_tower][home_rally][serialization]" )
{
    const auto serialize = []( const auto &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    raid_sweep::source_outcome human;
    migo_tower::source_history migo;
    const auto check = [&]( auto original ) {
        CHECK( serialize( original ).find( "rally_episode" ) == std::string::npos );
        original.rally_episode = original.episode;
        for( int outcome = 0; outcome <= static_cast<int>( raid_sweep::home_rally_outcome::expired );
             ++outcome ) {
            original.rally_outcomes["member-" + std::to_string( outcome )] = outcome;
        }
        auto loaded = original;
        loaded.deserialize( json_loader::from_string( serialize( original ) ).get_object() );
        CHECK( serialize( loaded ) == serialize( original ) );
        for( int malformed = 0; malformed < 6; ++malformed ) {
            auto bad = original;
            if( malformed == 0 ) {
                bad.rally_episode = 0;
            } else if( malformed == 1 ) {
                bad.rally_episode = bad.episode + 1;
            } else if( malformed == 2 ) {
                bad.rally_outcomes[""] = 1;
            } else if( malformed == 3 ) {
                bad.rally_outcomes[std::string( 129, 'x' )] = 1;
            } else if( malformed == 4 ) {
                bad.rally_outcomes["bad-outcome"] = 5;
            } else {
                for( int i = 0; i < 17; ++i ) {
                    bad.rally_outcomes["oversize-" + std::to_string( i )] = 1;
                }
            }
            auto refused = original;
            CHECK_THROWS( refused.deserialize( json_loader::from_string( serialize( bad ) ).get_object() ) );
        }
    };
    check( human );
    check( migo );

    const tripoint_abs_omt source( 10, 20, 0 );
    raid_sweep::source_memory memory;
    REQUIRE( memory.reserve( source ) );
    memory.find( source )->rally_episode = 1;
    memory.find( source )->rally_outcomes["spent"] = static_cast<int>( raid_sweep::home_rally_outcome::expired );
    for( int i = 1; i <= 4; ++i ) {
        REQUIRE( memory.reserve( source + point( i, 0 ) ) );
    }
    CHECK_FALSE( memory.find( source ) );
    CHECK_FALSE( memory.rally_history_known( source, 1 ) );
    const auto before_refusal = serialize( memory );
    CHECK_FALSE( memory.ready( source, "unknown-previous-outcome" ) );
    CHECK( serialize( memory ) == before_refusal );
    // Rally refusal must not reserve a generation-zero slot and bypass the
    // existing absent-area dispatch caution. Genuine resident evidence is a
    // separate control, not a fabricated extinction of the evicted area.
    raid_sweep::source_memory resident;
    REQUIRE( resident.reserve( source ) );
    CHECK( resident.find( source )->generation == 0 );
    resident.observe_absence( source, 1000 );
    CHECK( resident.next_episode( source, 1000 ) == 1 );
    CHECK( resident.next_episode( source, 1001 ) == 2 );
    CHECK( resident.rally_history_known( source, 2 ) );
    resident.adopt_signal( source, 1001 );
    CHECK( resident.find( source )->episode == 2 );

    migo_tower::site_state site;
    auto *first = site.remember_source( source );
    REQUIRE( first );
    first->rally_episode = 1;
    first->rally_outcomes["spent"] = static_cast<int>( raid_sweep::home_rally_outcome::transferred );
    for( int i = 1; i <= 4; ++i ) {
        REQUIRE( site.remember_source( source + point( i, 0 ) ) );
    }
    CHECK_FALSE( site.find_source( source ) );
    const auto *unknown = site.remember_source( source );
    REQUIRE( unknown );
    CHECK( unknown->prior_outcome_unavailable );
    CHECK( unknown->rally_outcomes.empty() );
    CHECK( unknown->episode == unknown->episode_at_outcome );
}

TEST_CASE( "individual_home_rally_interval_is_finite_and_reload_cannot_replenish_it",
           "[home_rally][migo_tower]" )
{
    using namespace raid_sweep;
    const auto bytes = []( const auto &value ) {
        std::ostringstream out;
        { JsonOut json( out ); value.serialize( json ); }
        return out.str();
    };
    home_rally rally;
    rally.source = tripoint_abs_omt( 2, 3, 0 );
    rally.episode = 1;
    home_rally_member member;
    member.member_id = "genuine-existing-member";
    member.allowance_turns = 100;
    member.no_progress_turns = 40;
    member.evaluate( 1000, true, 900 );
    member.observe( tripoint_abs_ms( 30, 40, 0 ), false );
    CHECK( member.active_turns == 0 );
    CHECK( member.active_delta_turns == 0 );
    CHECK( member.suspended_body_turns( 900 ) == 100 );
    CHECK( member.claim_motor_turns() == 0 );
    member.settle_body( 1000 );
    member.evaluate( 1005, true, 1000 );
    CHECK( member.active_turns == 5 );
    CHECK( member.claim_motor_turns() == 5 );
    CHECK( member.claim_motor_turns() == 0 );
    member.evaluate( 1006, false, 1000 );
    CHECK( member.active_turns == 5 );
    member.evaluate( 5000, false, 1000 );
    CHECK( member.active_turns == 5 );
    CHECK( member.suspended_body_turns( 1000 ) == 3995 );
    CHECK_FALSE( member.exhausted() );
    REQUIRE( rally.add( member ) );
    CHECK_FALSE( rally.add( member ) );
    const auto frozen = bytes( rally );
    home_rally loaded;
    loaded.deserialize( json_loader::from_string( frozen ) );
    CHECK( bytes( rally ) == frozen ); // Serialization is not an owner callback.
    REQUIRE( loaded.find( member.member_id ) );
    auto &resumed = *loaded.find( member.member_id );
    CHECK( resumed.active_delta_turns == 0 );
    resumed.evaluate( 6000, true, 1000 );
    CHECK( resumed.active_turns == 5 );
    CHECK( resumed.claim_motor_turns() == 0 );
    CHECK( resumed.suspended_body_turns( 1000 ) == 4995 );
    resumed.settle_body( 6000 );
    resumed.evaluate( 6001, true, 6000 );
    CHECK( resumed.active_turns == 6 );
    CHECK( resumed.claim_motor_turns() == 1 );
    resumed.evaluate( 6001, true, 6000 );
    CHECK( resumed.claim_motor_turns() == 0 );
    const auto current = bytes( loaded );
    resumed.evaluate( 5999, true, 6000 );
    CHECK( bytes( loaded ) == current );
    resumed.observe( tripoint_abs_ms( 29, 40, 0 ), false );
    CHECK( resumed.last_progress == 6 );
    resumed.evaluate( 6041, true, 6000 );
    CHECK( resumed.exhausted() ); // Real active no-progress, not the frozen absence.
    CHECK( resumed.active_turns == 46 );
    resumed.observe( tripoint_abs_ms( 28, 40, 0 ), true );
    CHECK( resumed.outcome == home_rally_outcome::arrived );
    CHECK_FALSE( loaded.pending() );
    const auto arrived = bytes( loaded );
    resumed.observe( tripoint_abs_ms( 27, 40, 0 ), true );
    CHECK( bytes( loaded ) == arrived );
    for( int i = 0; i < 15; ++i ) {
        member.member_id = "other-" + std::to_string( i );
        REQUIRE( loaded.add( member ) );
    }
    member.member_id = "overflow";
    CHECK_FALSE( loaded.add( member ) );
    CHECK( loaded.members.size() == 16 );
}

// Separate the native pre-hook work from the species hook without changing a
// plan's physical inputs, PRNG stream or canonical body. Cache prewarming is
// outside the measured call and is not a gameplay scheduling proposal.
TEST_CASE( "native_migo_plan_reachability_cold_and_warm_conserve_the_same_decision",
           "[.][migo_tower][migo_plan_attribution]" )
{
    const std::string type = GENERATE( std::string( "mon_mi_go" ),
                                       std::string( "mon_mi_go_scout" ) );
    clear_map( -2, 2 );
    map &here = get_map();
    const auto old_rng = rng_get_engine();
    const auto old_position = get_avatar().pos_abs();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    on_out_of_scope restore( [&]() {
        clear_creatures();
        overmap_buffer.global_state.migo_populations = old_populations;
        get_avatar().setpos( old_position, false );
        rng_get_engine() = old_rng;
    } );
    get_avatar().setpos( here.get_abs( tripoint_bub_ms( 110, 110, -2 ) ), false );
    monster &body = spawn_test_monster( type, tripoint_bub_ms( 60, 60, 0 ) );
    monster &peer = spawn_test_monster( "mon_zombie_hulk", tripoint_bub_ms( 66, 66, 0 ) );
    const auto anchor = body.pos_abs_omt();
    migo_population::site_record home;
    home.id = "native-plan-caller";
    home.anchor = anchor;
    migo_population::member_record member;
    member.token = {home.id, "native-plan-member", 1, anchor};
    member.type = body.type->id;
    member.home = body.pos_abs();
    member.owner = migo_population::representation::local;
    member.assignment = migo_population::duty::available;
    member.hp = body.get_hp();
    REQUIRE( home.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    body.population_identity() = member.token;
    body.set_migo_intent( migo_tower::member_intent() );
    overmap_buffer.global_state.migo_populations.sites.push_back( home );
    REQUIRE( overmap_buffer.global_state.migo_populations.find( home.id )->owns(
                 member.token, migo_population::representation::local ) );
    REQUIRE( get_creature_tracker().find( body.pos_abs() ).get() == &body );
    REQUIRE_FALSE( migo_tower::owns_intent( body, migo_population::representation::local ) );
    // The original packet contains both active generation1 and ordinary
    // generation0 intents. This first control covers the latter, not a raid.
    for( int z = -2; z <= OVERMAP_HEIGHT; ++z ) {
        here.build_map_cache( z );
    }
    const auto bytes = []( const auto &value ) {
        std::ostringstream out;
        { JsonOut js( out ); value.serialize( js ); }
        return out.str();
    };
    const auto initial_peer = bytes( peer );
    const auto initial_world = bytes( overmap_buffer.global_state.migo_populations );
    const auto initial_time = calendar::turn;
    const monster initial_body( body );
    const auto seed = rng_get_engine();
    double cold = 0.0;
    double warm = 0.0;
    double prewarm = 0.0;
    double direct_hook = 0.0;
    int visited = 0;
    const auto timed = []( const auto &call ) {
        const auto started = std::chrono::steady_clock::now();
        call();
        return std::chrono::duration<double>( std::chrono::steady_clock::now() - started ).count();
    };
    for( int repeat = 0; repeat < 3; ++repeat ) {
        body = initial_body;
        rng_get_engine() = seed;
        get_creature_tracker().invalidate_reachability_cache();
        cold += timed( [&]() { body.plan(); } );
        const auto expected_body = bytes( body );
        const auto expected_rng = rng_get_engine();
        body = initial_body;
        rng_get_engine() = seed;
        get_creature_tracker().invalidate_reachability_cache();
        prewarm += timed( [&]() {
            get_creature_tracker().for_each_reachable( body, [&]( Creature * ) { ++visited; } );
        } );
        CHECK( rng_get_engine() == seed );
        warm += timed( [&]() { body.plan(); } );
        CHECK( bytes( body ) == expected_body );
        CHECK( rng_get_engine() == expected_rng );
        CHECK( bytes( peer ) == initial_peer );
        CHECK( bytes( overmap_buffer.global_state.migo_populations ) == initial_world );
        CHECK( calendar::turn == initial_time );
        body = initial_body;
        rng_get_engine() = seed;
        bool applied = true;
        direct_hook += timed( [&]() { applied = migo_tower::plan_local( body, false, nullptr ); } );
        CHECK_FALSE( applied );
        CHECK( bytes( body ) == bytes( initial_body ) );
        CHECK( rng_get_engine() == seed );
    }
    std::ostringstream out;
    {
        JsonOut js( out );
        js.start_object();
        js.member( "type", type );
        js.member( "repetitions", 3 );
        js.member( "cold_full_plan_seconds", cold );
        js.member( "warm_full_plan_seconds", warm );
        js.member( "native_reachability_prewarm_seconds", prewarm );
        js.member( "generation0_direct_hook_seconds", direct_hook );
        js.member( "reachable_visits", visited );
        js.end_object();
    }
    std::cout << "MIGO_PLAN_COMPARISON " << out.str() << '\n';
}

#if !defined(TILES)
extern "C" int ungetch( int );
#endif

TEST_CASE( "active_native_migo_plan_separates_hostile_recognition_from_targeting",
           "[.][migo_tower][migo_plan_species_attribution]" )
{
#if !defined(TILES)
    // The test world starts with loaded data, not a running game mode. Use the
    // established save/load owner before exercising actual advancing turns.
    REQUIRE( world_generator->active_world );
    const std::string world_name = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world_name ) );
#endif
    clear_map( -2, 2 );
    map &here = get_map();
    const auto old_rng = rng_get_engine();
    const auto old_position = get_avatar().pos_abs();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_turn = calendar::turn;
    on_out_of_scope restore( [&]() {
        clear_creatures();
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        get_avatar().setpos( old_position, false );
        set_time( old_turn );
        rng_get_engine() = old_rng;
    } );
    // A full game turn expects the avatar near the native map center, inside
    // the fixed scent-array footprint. A real opaque room keeps it from
    // replacing the deliberately visible hostile target in either comparison.
    get_avatar().setpos( here.get_abs( tripoint_bub_ms( 66, 60, 0 ) ), false );
    for( int x = 65; x <= 67; ++x ) {
        for( int y = 59; y <= 61; ++y ) {
            if( x != 66 || y != 60 ) {
                here.ter_set( tripoint_bub_ms( x, y, 0 ), ter_str_id( "t_wall" ) );
            }
        }
    }
    set_time( calendar::turn_zero + 12_hours );
    monster &body = spawn_test_monster( "mon_mi_go", tripoint_bub_ms( 60, 60, 0 ) );
    monster &enemy = spawn_test_monster( "mon_zombie_hulk", tripoint_bub_ms( 66, 66, 0 ) );
    const auto anchor = body.pos_abs_omt();
    migo_population::site_record home;
    home.id = "active-plan-caller";
    home.anchor = anchor;
    migo_population::member_record member;
    member.token = {home.id, "active-plan-member", 1, anchor};
    member.type = body.type->id;
    member.home = body.pos_abs();
    member.owner = migo_population::representation::local;
    member.assignment = migo_population::duty::dispatched;
    member.hp = body.get_hp();
    REQUIRE( home.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    body.population_identity() = member.token;
    migo_tower::member_intent intent;
    intent.generation = 1;
    intent.home = member.home;
    body.set_migo_intent( intent );
    overmap_buffer.global_state.migo_populations.sites.push_back( home );
    migo_tower::site_state site;
    site.site_id = home.id;
    site.anchor = anchor;
    migo_tower::operation_record operation;
    operation.generation = 1;
    operation.source.source_omt = anchor + point( 1, 0 );
    operation.sweep.candidates = {{body.pos_abs() + point( 18, 0 )}};
    operation.members = {{member.token.member_id, false, false, member.home}};
    operation.overdue_after_active = 100000;
    site.operation = operation;
    overmap_buffer.global_state.migo_towers.sites.push_back( site );
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    REQUIRE( migo_tower::owns_intent( body, migo_population::representation::local ) );
    REQUIRE( migo_tower::activity_for( body, migo_population::representation::local,
                                     to_turn<int>( calendar::turn ) ) );
    REQUIRE( body.attitude_to( enemy ) == Creature::Attitude::HOSTILE );
    for( int z = -2; z <= OVERMAP_HEIGHT; ++z ) {
        here.build_map_cache( z );
    }
    REQUIRE( body.sees( here, enemy ) );
    REQUIRE_FALSE( body.sees( here, get_avatar() ) );
    REQUIRE( here.inbounds( get_avatar().pos_bub() + point( 41, 41 ) ) );
    REQUIRE( here.inbounds( get_avatar().pos_bub() - point( 41, 41 ) ) );
    const auto bytes = []( const auto &value ) {
        std::ostringstream out;
        { JsonOut js( out ); value.serialize( js ); }
        return out.str();
    };
    const auto world = bytes( overmap_buffer.global_state.migo_populations );
    const auto towers = bytes( overmap_buffer.global_state.migo_towers );
    const auto peer = bytes( enemy );
    const auto now = calendar::turn;
    const monster initial( body );
    const auto seed = rng_get_engine();
    const auto reset = [&]() { body = initial; rng_get_engine() = seed; };
    const auto timed = []( const auto &call ) {
        const auto started = std::chrono::steady_clock::now();
        call();
        return std::chrono::duration<double>( std::chrono::steady_clock::now() - started ).count();
    };
    double full = 0.0;
    double hook = 0.0;
    double optical = 0.0;
    double native_visibility = 0.0;
    std::string full_endpoint;
    std::string hook_endpoint;
    cata_default_random_engine full_rng;
    cata_default_random_engine hook_rng;
    for( int repeat = 0; repeat < 3; ++repeat ) {
        reset();
        get_creature_tracker().invalidate_reachability_cache();
        get_creature_tracker().for_each_reachable( body, []( Creature * ) {} );
        full += timed( [&]() { body.plan(); } );
        if( repeat == 0 ) {
            full_endpoint = bytes( body );
            full_rng = rng_get_engine();
        } else {
            CHECK( bytes( body ) == full_endpoint );
            CHECK( rng_get_engine() == full_rng );
        }
        reset();
        bool applied = true;
        hook += timed( [&]() { applied = migo_tower::plan_local( body, false, &enemy ); } );
        CHECK_FALSE( applied ); // Native combat retains its normal priority.
        CHECK( body.migo_intent()->last_observed_enemy_world == to_turn<int>( calendar::turn ) );
        REQUIRE_FALSE( body.migo_intent()->observed_threats.empty() );
        if( repeat == 0 ) {
            hook_endpoint = bytes( body );
            hook_rng = rng_get_engine();
        } else {
            CHECK( bytes( body ) == hook_endpoint );
            CHECK( rng_get_engine() == hook_rng );
        }
        reset();
        scout_observation::site_reader reader;
        bool recognized = false;
        optical += timed( [&]() {
            recognized = migo_tower::observe_enemy( body, enemy, reader ).has_value();
        } );
        CHECK( recognized );
        CHECK( bytes( body ) == bytes( initial ) );
        CHECK( rng_get_engine() == seed );
        bool visible = false;
        native_visibility += timed( [&]() { visible = body.sees( here, enemy ); } );
        CHECK( visible );
        CHECK( bytes( body ) == bytes( initial ) );
        CHECK( rng_get_engine() == seed );
        CHECK( bytes( enemy ) == peer );
        CHECK( bytes( overmap_buffer.global_state.migo_populations ) == world );
        CHECK( bytes( overmap_buffer.global_state.migo_towers ) == towers );
        CHECK( calendar::turn == now );
    }
    std::ostringstream out;
    {
        JsonOut js( out );
        js.start_object();
        js.member( "repetitions", 3 );
        js.member( "warm_full_plan_seconds", full );
        js.member( "active_hostile_hook_seconds", hook );
        js.member( "isolated_observe_enemy_seconds", optical );
        js.member( "native_sees_seconds", native_visibility );
        js.end_object();
    }
    std::cout << "MIGO_PLAN_COMPARISON " << out.str() << '\n';

    // One immutable physical scene, three fresh decisions, each followed by a
    // second read within that same synchronous decision. This is a cache-path
    // discriminator, not permission to retain geometry between game turns.
    reset();
    const auto main_lm = here.get_cache_ref( 0 ).lm;
    const auto main_sm = here.get_cache_ref( 0 ).sm;
    const auto resident_count = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    // These native physical planes are genuinely resident before observation;
    // this control does not label cache eviction as cold disk I/O.
    const auto first_light_sm = project_to<coords::sm>( enemy.pos_abs() ) -
                                point( MAPSIZE / 2, MAPSIZE / 2 );
    for( int z = 0; z <= 2; ++z ) {
        for( int x = 0; x < MAPSIZE; ++x ) {
            for( int y = 0; y < MAPSIZE; ++y ) {
                REQUIRE( MAPBUFFER.lookup_submap_cached( tripoint_abs_sm(
                        first_light_sm.x() + x, first_light_sm.y() + y, z ) ) );
            }
        }
    }
    for( int repeat = 0; repeat < 3; ++repeat ) {
        double first = 0.0;
        double reused = 0.0;
        double retire = 0.0;
        const auto cpu = std::clock();
        auto reader = std::make_unique<scout_observation::site_reader>();
        scout_observation::visibility_read a;
        scout_observation::visibility_read b;
        first = timed( [&]() {
            a = reader->read_actor( body, enemy, {MAX_VIEW_DISTANCE * 2, 2.0f} );
        } );
        const auto views = reader->cached_view_count();
        const auto physical = reader->cached_lighting_submap_count();
        const auto summaries = reader->cached_sunlight_summary_count();
        const auto paths = reader->cached_lighting_path_group_count();
        reused = timed( [&]() {
            b = reader->read_actor( body, enemy, {MAX_VIEW_DISTANCE * 2, 2.0f} );
        } );
        CHECK( a.known );
        CHECK( a.visible );
        CHECK( a.known == b.known );
        CHECK( a.visible == b.visible );
        CHECK( a.reason == b.reason );
        CHECK( views == reader->cached_view_count() );
        CHECK( physical == reader->cached_lighting_submap_count() );
        CHECK( summaries == reader->cached_sunlight_summary_count() );
        CHECK( paths == reader->cached_lighting_path_group_count() );
        retire = timed( [&]() {
            reader.reset();
        } );
        const double cpu_seconds = double( std::clock() - cpu ) / CLOCKS_PER_SEC;
        CHECK( bytes( body ) == bytes( initial ) );
        CHECK( bytes( enemy ) == peer );
        CHECK( rng_get_engine() == seed );
        CHECK( bytes( overmap_buffer.global_state.migo_populations ) == world );
        CHECK( bytes( overmap_buffer.global_state.migo_towers ) == towers );
        CHECK( calendar::turn == now );
        CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == resident_count );
        std::ostringstream sample;
        {
            JsonOut js( sample );
            js.start_object();
            js.member( "repeat", repeat );
            js.member( "fresh_seconds", first );
            js.member( "reused_seconds", reused );
            js.member( "retire_seconds", retire );
            js.member( "whole_pair_process_cpu_seconds", cpu_seconds );
            js.member( "views", views );
            js.member( "physical_submaps", physical );
            js.member( "solar_summaries", summaries );
            js.member( "path_groups", paths );
            js.end_object();
        }
        std::cout << "MIGO_OPTICAL_COMPARISON " << sample.str() << '\n';
    }
    for( int x = 0; x < MAPSIZE_X; ++x ) {
        for( int y = 0; y < MAPSIZE_Y; ++y ) {
            REQUIRE( here.get_cache_ref( 0 ).lm[x][y].values == main_lm[x][y].values );
            REQUIRE( here.get_cache_ref( 0 ).sm[x][y] == main_sm[x][y] );
        }
    }

#if !defined(TILES)
    // Exercise the real advancing turn producer and its normal input flush;
    // no diagnostic test-row API or additional event stream is introduced.
    override_option diagnostic( "RAID_PERFORMANCE_DIAGNOSTIC", "quiet_signal" );
    override_option autosave( "AUTOSAVE", "false" );
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    const auto trace = std::filesystem::path( PATH_INFO::user_dir() ) / "migo-plan-turn.jsonl";
    REQUIRE_FALSE( std::filesystem::exists( trace ) );
    const auto environment = []( const char *key, const char *value ) {
#if defined(_WIN32)
        return _putenv_s( key, value ? value : "" );
#else
        return value ? setenv( key, value, 1 ) : unsetenv( key );
#endif
    };
    const std::map<std::string, std::string> settings = {
        { "OPENCLAW_HARNESS_RUN_ID", "migo-plan-component" },
        { "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", "migo-plan-component" },
        { "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", trace.string() }
    };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &entry : settings ) {
        const char *value = std::getenv( entry.first.c_str() );
        previous[entry.first] = value ? std::optional<std::string>( value ) : std::nullopt;
        REQUIRE( environment( entry.first.c_str(), entry.second.c_str() ) == 0 );
    }
    on_out_of_scope restore_environment( [&]() {
        for( const auto &entry : previous ) {
            environment( entry.first.c_str(), entry.second ? entry.second->c_str() : nullptr );
        }
        std::filesystem::remove( trace );
    } );
    const std::string old_ctype = std::setlocale( LC_CTYPE, nullptr );
    on_out_of_scope restore_locale( [&]() { std::setlocale( LC_CTYPE, old_ctype.c_str() ); } );
    // The standalone test runner deliberately starts in the C locale; native
    // curses needs the host's Unicode locale just as the ordinary main does.
    REQUIRE( std::setlocale( LC_CTYPE, "" ) != nullptr );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&]() { if( owns_terminal ) { catacurses::endwin(); } } );
    const bool owns_imgui = ImGui::GetCurrentContext() == nullptr;
    if( owns_imgui ) {
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.DisplaySize = ImVec2( 800.0f, 600.0f );
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->AddFontDefault();
        io.Fonts->Build();
        ImGui::NewFrame();
    }
    on_out_of_scope close_imgui( [&]() {
        if( owns_imgui ) { ImGui::EndFrame(); ImGui::DestroyContext(); }
    } );
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    restore_on_out_of_scope<bool> restore_new_game( g->new_game );
    restore_on_out_of_scope<safe_mode_type> restore_safe_mode( g->safe_mode );
    g->new_game = false;
    g->set_safe_mode( SAFE_MODE_OFF );
    test_mode = false;
    ui_adaptor root;
    root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw();
    reset();
    // Give the isolated player actual usable footing; no plan/target input changes.
    here.ter_set( get_avatar().pos_bub(), ter_str_id( "t_rock_floor" ) );
    const int measured_turn = to_turn<int>( calendar::turn ) + 1;
    get_avatar().set_moves( 0 );
    REQUIRE_FALSE( g->do_turn() );
    const auto pause = get_default_mode_input_context().first_keyboard_character_for_action( "pause" );
    REQUIRE( pause );
    REQUIRE( pause->size() == 1 );
    REQUIRE( ungetch( pause->front() ) != -1 );
    get_avatar().set_moves( 100 );
    REQUIRE_FALSE( g->do_turn() );
    bool row_seen = false;
    std::ifstream input( trace );
    std::string line;
    while( std::getline( input, line ) ) {
        const auto start = line.find( '{' );
        if( start == std::string::npos ) { continue; }
        const auto event = json_loader::from_string( line.substr( start ) ).get_object();
        event.allow_omitted_members();
        if( event.get_string( "event", "" ) != "raid_performance_interval" ) { continue; }
        const auto names = event.get_array( "stage_names" );
        std::map<std::string, int> indexes;
        int index = 0;
        for( const auto name : names ) { indexes[name.get_string()] = index++; }
        for( const auto value : event.get_array( "rows" ) ) {
            const auto row = value.get_object();
            row.allow_omitted_members();
            if( row.get_int( "turn" ) != measured_turn ) { continue; }
            row_seen = true;
            const auto durations = row.get_array( "stages_seconds" );
            const auto calls = row.get_array( "stage_calls" );
            const auto seconds = [&]( const std::string &name ) {
                REQUIRE( indexes.count( name ) == 1 );
                return durations.get_float( indexes.at( name ) );
            };
            for( const std::string &name : { "migo_local_plan", "migo_plan_native_targeting",
                                            "migo_plan_species_hook", "migo_plan_hostile_recognition",
                     "scout_actor_read", "scout_view_build", "scout_view_cache",
                     "scout_light_read", "scout_light_dependencies", "scout_light_native",
                     "scout_actor_predicate", "scout_reader_retire"
                 } ) {
                REQUIRE( indexes.count( name ) == 1 );
                CHECK( calls.get_int( indexes.at( name ) ) > 0 );
            }
            CHECK( seconds( "migo_local_plan" ) + 0.00002 >=
                   seconds( "migo_plan_native_targeting" ) + seconds( "migo_plan_species_hook" ) );
            CHECK( seconds( "migo_plan_species_hook" ) + 0.00002 >=
                   seconds( "migo_plan_hostile_recognition" ) );
            CHECK( seconds( "scout_actor_read" ) >= seconds( "scout_light_read" ) );
            CHECK( seconds( "scout_light_read" ) >= seconds( "scout_light_native" ) );
            CHECK( seconds( "migo_plan_hostile_recognition" ) >= seconds( "scout_actor_read" ) );
            std::cout << "MIGO_PLAN_ROW " << row.str() << '\n';
        }
    }
    CHECK( row_seen );
#endif
}

TEST_CASE( "migo same tower operation and recall use independent actual body intervals",
           "[migo_tower][migo_activation][independent_native_owners]" )
{
    clear_avatar();
    clear_map_with_vision( -1, 4, false );
    clear_creatures();
    const auto old_turn = calendar::turn;
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    overmap_buffer.global_state.migo_populations.clear();
    overmap_buffer.global_state.migo_towers.clear();
    set_time_to_day();
    map &here = get_map();
    get_avatar().setpos( here, tripoint_bub_ms( 1, 1, 0 ), false );
    get_avatar().set_mutation( trait_id( "DEBUG_CLOAK" ) );
    const auto anchor = project_to<coords::omt>( here.get_abs( tripoint_bub_ms( 60, 60, 0 ) ) );
    std::vector<std::pair<tripoint_abs_omt, oter_id>> terrain;
    on_out_of_scope restore( [&]() {
        clear_creatures();
        for( const auto &entry : terrain ) {
            overmap_buffer.ter_set( entry.first, entry.second );
        }
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        clear_avatar();
        clear_map_without_vision( -1, 4 );
        set_time( old_turn );
    } );
    for( int z = 0; z < 4; ++z ) {
        const tripoint_abs_omt p( anchor.xy(), z );
        terrain.emplace_back( p, overmap_buffer.ter( p ) );
        overmap_buffer.ter_set( p, oter_str_id( "mi-go_scout_tower_" +
                                                std::to_string( z + 1 ) + "_north" ) );
    }
    const auto geometry = migo_population::geometry_at( anchor );
    REQUIRE( geometry );
    migo_population::site_record population;
    population.id = geometry->id;
    population.anchor = anchor;
    std::vector<migo_population::member_record> members;
    std::vector<monster *> bodies;
    for( int i = 0; i < 5; ++i ) {
        const tripoint_bub_ms position( i == 4 ? 85 : 60 + i, 60, i == 0 ? 1 : 0 );
        here.ter_set( position, ter_str_id( "t_floor" ) );
        monster *body = g->place_critter_at( mtype_id( "mon_mi_go" ), position );
        REQUIRE( body );
        migo_population::member_record member;
        member.token = { geometry->id, "independent-owner-" + std::to_string( i ), 1, anchor };
        member.type = body->type->id;
        member.home = here.get_abs( tripoint_bub_ms( 60 + i, 60, 0 ) );
        member.hp = body->get_hp();
        member.owner = migo_population::representation::local;
        member.current_position = body->pos_abs();
        body->population_identity() = member.token;
        members.push_back( member );
        bodies.push_back( body );
    }
    REQUIRE( population.commit_seed( members, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( population );
    migo_tower::site_state state;
    state.site_id = geometry->id;
    state.anchor = anchor;
    const int now = to_turn<int>( calendar::turn );
    state.signal = migo_tower::signal_record{ "independent-owner-signal", "smoke",
                   anchor + point( 2, 0 ), std::nullopt, members.front().token.member_id,
                   bodies.front()->pos_abs(), now };
    overmap_buffer.global_state.migo_towers.sites.push_back( state );
    for( int z : { 0, 1 } ) {
        here.build_map_cache( z );
    }
    REQUIRE( migo_tower::begin_response( geometry->id, now ) );
    auto site_now = [&]() -> migo_tower::site_state & {
        return *overmap_buffer.global_state.migo_towers.find( geometry->id );
    };
    REQUIRE( site_now().operation );
    REQUIRE( site_now().response_home_rally );
    const auto recalled_id = members.back().token.member_id;
    REQUIRE( site_now().response_home_rally->find( recalled_id ) );
    REQUIRE_FALSE( site_now().operation->contains( recalled_id ) );
    REQUIRE( site_now().operation->generation != site_now().response_home_rally->generation );
    const auto recalled_omt = project_to<coords::omt>( bodies.back()->pos_abs() );
    REQUIRE( recalled_omt.xy() == anchor.xy() + point( 1, 0 ) );
    monster *operation_body = nullptr;
    for( monster *body : bodies ) {
        if( site_now().operation->contains( body->population_identity().member_id ) ) {
            operation_body = body;
            break;
        }
    }
    REQUIRE( operation_body );
    REQUIRE( project_to<coords::omt>( operation_body->pos_abs() ).xy() == anchor.xy() );
    // This is an isolated local-adapter test, not a reality-map unload or
    // local-to-abstract transfer: raw avatar setpos leaves these bodies in the
    // tracker. Both owners were legitimately admitted by begin_response, and
    // each retains its authenticated native lease. Real transfer/reentry is
    // qualified separately by native and representation-handoff evidence.
    get_avatar().setpos( project_to<coords::ms>( anchor - point( 40, 0 ) ), false );
    migo_tower::refresh_activation( now );
    REQUIRE( site_now().operation->activity.active );
    REQUIRE_FALSE( site_now().response_home_rally->find( recalled_id )->active );
    const auto frozen = site_now().response_home_rally->find( recalled_id )->active_turns;
    set_time( calendar::turn + 1_turns );
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    CHECK( site_now().operation->activity.active_delta_turns == 1 );
    CHECK( site_now().response_home_rally->find( recalled_id )->active_turns == frozen );
    CHECK_FALSE( migo_tower::activity_for( *bodies.back(), migo_population::representation::local,
                 to_turn<int>( calendar::turn ), migo_tower::activity_boundary::process ) );
    get_avatar().setpos( project_to<coords::ms>( recalled_omt + point( 40, 0 ) ), false );
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    REQUIRE_FALSE( site_now().operation->activity.active );
    REQUIRE( site_now().response_home_rally->find( recalled_id )->active );
    set_time( calendar::turn + 1_turns );
    migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
    CHECK( site_now().response_home_rally->find( recalled_id )->active_turns == frozen + 1 );
    CHECK( site_now().operation->activity.active_delta_turns == 0 );
    CHECK_FALSE( migo_tower::activity_for( *operation_body, migo_population::representation::local,
                 to_turn<int>( calendar::turn ), migo_tower::activity_boundary::process ) );
}
