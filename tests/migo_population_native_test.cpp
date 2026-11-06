#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "debug.h"
#include "creature_tracker.h"
#include "field_type.h"
#include "field.h"
#include "filesystem.h"
#include "game.h"
#include "horde_entity.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "mapbuffer.h"
#include "migo_population.h"
#include "migo_population_native.h"
#include "migo_tower_ai.h"
#include "migo_tower_native.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "omdata.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "submap.h"
#include "path_info.h"
#include "save_continuity.h"
#include "worldfactory.h"

namespace
{
std::vector<std::pair<tripoint_abs_ms, monster *>> abstract_members( const std::string &site_id,
        const tripoint_abs_omt &anchor )
{
    std::vector<std::pair<tripoint_abs_ms, monster *>> result;
    for( const auto &entry : overmap_buffer.get( project_to<coords::om>( anchor ).xy() ).horde_entities() ) {
        if( entry.second->monster_data &&
            entry.second->monster_data->population_identity().site_id == site_id ) {
            result.emplace_back( entry.first, entry.second->monster_data.get() );
        }
    }
    return result;
}

monster *local_member( const std::string &id )
{
    for( monster &m : g->all_monsters() ) {
        if( m.population_identity().member_id == id ) {
            return &m;
        }
    }
    return nullptr;
}

void release_fixture( const std::string &site_id, const tripoint_abs_omt &anchor )
{
    for( const auto &entry : abstract_members( site_id, anchor ) ) {
        const std::string id = entry.second->population_identity().member_id;
        CHECK( overmap_buffer.get( project_to<coords::om>( entry.first ).xy() ).release_monster_payload(
                   entry.first, id ) );
    }
    std::vector<monster *> local;
    for( monster &m : g->all_monsters() ) {
        if( m.population_identity().site_id == site_id ) {
            local.push_back( &m );
        }
    }
    for( monster *m : local ) {
        g->remove_zombie( *m );
    }
}
} // namespace

TEST_CASE( "migo_cold_native_archive_authenticates_legacy_and_saved_projection_without_generation",
           "[migo_population][migo_activation][savegame]" )
{
    using namespace migo_population;
    clear_map();
    const auto previous_population = overmap_buffer.global_state.migo_populations;
    const auto previous_towers = overmap_buffer.global_state.migo_towers;
    const auto previous_time = calendar::turn;
    const auto previous_avatar = get_avatar().pos_abs();
    overmap_buffer.global_state.migo_populations.clear();
    overmap_buffer.global_state.migo_towers.clear();
    calendar::turn = calendar::turn_zero + 8_days;
    const bool saved_projection = GENERATE( false, true );
    const bool terminal_home = GENERATE( false, true );
    const point_abs_om owner_key( 271, 273 );
    const tripoint_abs_omt actual_omt = project_to<coords::omt>( tripoint_abs_om( owner_key,
        1 ) ) + point( OMAPX - 1, 9 );
    const tripoint_abs_ms actual = project_to<coords::ms>( actual_omt ) + point( 12, 12 );
    const tripoint_abs_omt stale_home = actual_omt + point::east;
    const std::string id = "cold-native-authentication";
    const bool compressed = world_generator->active_world->has_compression_enabled();
    const cata_path root = compressed ? PATH_INFO::current_dimension_save_path() /
                           zzip_overmap_directory : PATH_INFO::current_dimension_save_path();
    const cata_path archive = root / std::filesystem::u8path( overmapbuffer::terrain_filename(
            owner_key ) ) +
                              ( compressed ? zzip_suffix : "" );
    const cata_path view = overmapbuffer::player_filename( owner_key );
    REQUIRE_FALSE( file_exist( archive ) );
    REQUIRE_FALSE( file_exist( view ) );
    on_out_of_scope restore( [&]() {
        overmap_buffer.clear();
        overmap_buffer.global_state.migo_populations = previous_population;
        overmap_buffer.global_state.migo_towers = previous_towers;
        get_avatar().setpos( previous_avatar, false );
        calendar::turn = previous_time;
        if( file_exist( archive ) ) {
            CHECK( remove_file( archive ) );
        }
        if( file_exist( view ) ) {
            CHECK( remove_file( view ) );
        }
    } );
    site_record site;
    site.id = id;
    site.anchor = stale_home; // locator only; deliberately the wrong native owner
    member_record member;
    member.token = { id, "carrier", 7, stale_home };
    member.type = mtype_id( "mon_mi_go" );
    member.owner = representation::abstract;
    member.assignment = terminal_home ? duty::available : duty::returning;
    member.home = terminal_home ? actual : project_to<coords::ms>( stale_home );
    member.hp = member.type->hp;
    if( saved_projection ) {
        member.current_position = actual;
    }
    REQUIRE( site.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( site );
    migo_tower::site_state tower;
    tower.site_id = id;
    tower.anchor = stale_home;
    tower.operation.emplace();
    tower.operation->generation = 3;
    tower.operation->members.push_back( { member.token.member_id, terminal_home, terminal_home, member.home } );
    overmap_buffer.global_state.migo_towers.sites.push_back( tower );
    monster body( member.type );
    body.population_identity() = member.token;
    body.set_pos_abs_only( actual );
    migo_tower::member_intent intent;
    intent.generation = 3;
    intent.phase = terminal_home ? migo_tower::member_phase::settled :
                   migo_tower::member_phase::returning;
    intent.home = member.home;
    body.set_migo_intent( intent );
    {
        // Native empty/unknown terrain is sufficient for body authentication.
        // Generating a distant scene would exercise unrelated required-special
        // placement and grant knowledge that this migration does not require.
        // Match the native buffer's heap ownership of this large grid object.
        auto owner = std::make_unique<overmap>( owner_key );
        REQUIRE( owner->retain_monster_payload( actual, body ) );
        owner->save(); // genuine native writer, not an alternate archive decoder
    }
    REQUIRE( file_exist( archive ) );
    const auto archive_bytes = std::filesystem::file_size( archive.get_unrelative_path() );
    const auto serialize = []( const auto & value ) {
        std::ostringstream out;
        {
            JsonOut json( out );
            value.serialize( json );
        }
        return out.str();
    };
    const auto saved_population = serialize( overmap_buffer.global_state.migo_populations );
    const auto saved_towers = serialize( overmap_buffer.global_state.migo_towers );
    const auto saved_body = serialize( body );
    overmap_buffer.clear(); // actual owner is now absent, not merely hidden from a test helper
    overmap_buffer.global_state.migo_populations.deserialize( json_loader::from_string(
                saved_population ) );
    overmap_buffer.global_state.migo_towers.deserialize( json_loader::from_string( saved_towers ) );
    REQUIRE_FALSE( overmap_buffer.find_loaded_overmap( owner_key ) );
    get_avatar().setpos( project_to<coords::ms>( ( terminal_home ? stale_home : actual_omt ) - point(
                40, 0 ) ), false );
    const auto submaps_before = std::distance( MAPBUFFER.begin(), MAPBUFFER.end() );
    const auto begin = std::chrono::steady_clock::now();
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    const auto cold_us = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - begin ).count();
    auto *loaded_owner = overmap_buffer.find_loaded_overmap( owner_key );
    REQUIRE( loaded_owner );
    auto *loaded = loaded_owner->entity_at( project_remain<coords::om>( actual ).remainder_tripoint );
    REQUIRE( loaded );
    REQUIRE( loaded->monster_data );
    CHECK( serialize( *loaded->monster_data ) == saved_body );
    auto *home = overmap_buffer.global_state.migo_populations.find( id );
    REQUIRE( home );
    CHECK( home->members.front().current_position == actual );
    CHECK( home->activity.active == terminal_home );
    CHECK( migo_tower::owns_intent( *loaded->monster_data,
                                    representation::abstract ) == !terminal_home );
    auto &operation = *overmap_buffer.global_state.migo_towers.find( id )->operation;
    CHECK( operation.activity.active );
    CHECK( operation.activity.active_delta_turns == 0 );
    CHECK( operation.sweep.active_elapsed == 0 );
    CHECK( std::distance( MAPBUFFER.begin(), MAPBUFFER.end() ) == submaps_before );
    CHECK_FALSE( overmap_buffer.find_loaded_overmap( project_to<coords::om>( stale_home ).xy() ) );
    // The now-resident owner remains authoritative without another file read.
    REQUIRE( remove_file( archive ) );
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    CHECK( overmap_buffer.find_loaded_overmap( owner_key ) == loaded_owner );
    calendar::turn += 1_turns;
    migo_tower::refresh_activation( to_turn<int>( calendar::turn ) );
    CHECK( operation.activity.active_delta_turns == 1 );
    CHECK( serialize( *loaded->monster_data ) == saved_body );
    std::cout << "migo40 cold native owner auth: saved_projection=" << saved_projection <<
                 " terminal_home=" << terminal_home << " archive_bytes=" << archive_bytes << " cold_wall_us=" <<
              cold_us <<
    " native_owner_object_bytes=" << sizeof( overmap ) <<
    " submap_delta=0 authenticated_owner=1; component/Mac only\n";
}

TEST_CASE( "migo_unowned_native_lifecycle_does_not_mutate_a_divergent_payload",
           "[migo_population][migo_activation][native]" )
{
    using namespace migo_population;
    clear_map();
    const auto previous_population = overmap_buffer.global_state.migo_populations;
    const auto previous_time = calendar::turn;
    overmap_buffer.global_state.migo_populations.clear();
    calendar::turn = calendar::turn_zero + 8_days;
    site_record home;
    home.id = "divergent-lifecycle";
    home.anchor = project_to<coords::omt>( get_avatar().pos_abs() );
    member_record member;
    member.token = {home.id, "resident", 7, home.anchor};
    member.type = mtype_id( "mon_mi_go" );
    member.owner = representation::local;
    member.hp = member.type->hp;
    REQUIRE( home.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( home );
    monster *body = g->place_critter_at( member.type, tripoint_bub_ms( 63, 63, 0 ) );
    REQUIRE( body );
    on_out_of_scope restore( [&]() {
        g->remove_zombie( *body );
        overmap_buffer.global_state.migo_populations = previous_population;
        calendar::turn = previous_time;
    } );
    body->population_identity() = member.token;
    ++body->population_identity().epoch;
    const auto serialize = [&]() {
        std::ostringstream out;
        {
            JsonOut json( out );
            body->serialize( json );
        }
        return out.str();
    };
    const auto before = serialize();
    CHECK_FALSE( can_transfer( *body, representation::local ) );
    body->process_turn();
    CHECK( serialize() == before );
    body->on_load();
    CHECK( serialize() == before );
    body->on_unload();
    CHECK( serialize() == before );
}

TEST_CASE( "migo_native_pause_settlement_preserves_fractional_day_and_local_residents",
           "[migo_population][migo_activation][native]" )
{
    using namespace migo_population;
    clear_map();
    const auto previous_population = overmap_buffer.global_state.migo_populations;
    const auto previous_towers = overmap_buffer.global_state.migo_towers;
    const auto previous_time = calendar::turn;
    overmap_buffer.global_state.migo_populations.clear();
    overmap_buffer.global_state.migo_towers.clear();
    calendar::turn = calendar::turn_zero + 4_days + 23_hours + 30_minutes;
    const auto anchor = project_to<coords::omt>( get_avatar().pos_abs() ) + point( 45, 0 );
    site_record site;
    site.id = "native-pause";
    site.anchor = anchor;
    member_record member;
    member.token = { site.id, "resident", 1, anchor };
    member.type = mtype_id( "mon_mi_go" );
    member.hp = member.type->hp;
    member.owner = representation::local;
    member.home = project_to<coords::ms>( anchor );
    REQUIRE( site.commit_seed( {member}, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( site );
    monster *body = g->place_critter_at( member.type, tripoint_bub_ms( 63, 63, 0 ) );
    REQUIRE( body );
    body->population_identity() = member.token;
    on_out_of_scope restore( [&]() {
        g->remove_zombie( *body );
        overmap_buffer.global_state.migo_populations = previous_population;
        overmap_buffer.global_state.migo_towers = previous_towers;
        calendar::turn = previous_time;
    } );
    const auto snapshot = []( const monster & value ) {
        std::ostringstream out;
        {
            JsonOut json( out );
            value.serialize( json );
        }
        auto result = json_loader::from_string( out.str() ).get_object();
        result.allow_omitted_members(); // this helper projects selected native fields
        return result;
    };
    body->set_baby_timer( calendar::turn + 3_days );
    body->on_unload(); // establish a valid native snapshot rather than a fresh-constructor timestamp
    body->clear_migo_intent(); // old valid payload predates the new binding fields
    calendar::turn += 1_days;
    body->on_load(); // zero-credit legacy adoption, ordinary local processing remains enabled
    auto &home = overmap_buffer.global_state.migo_populations.sites.front();
    CHECK_FALSE( home.activity.active );
    REQUIRE( body->migo_intent() );
    CHECK( body->migo_intent()->activity_bound );
    body->on_unload(); // exact local departure boundary, no fictitious home work
    const auto before = snapshot( *body );
    time_duration adopted = 0_turns;
    before.read( "suspended_evolution_time", adopted );
    CHECK( adopted == 1_days );
    calendar::turn += 2_hours;
    // The native unload/load boundary settles its home pause once. Actual
    // cross-representation transactions have separate ownership controls.
    body->on_load();
    const auto after = snapshot( *body );
    time_duration paused = 0_turns;
    after.read( "suspended_evolution_time", paused );
    CHECK( paused == adopted + 2_hours );
    std::optional<time_point> baby_before;
    std::optional<time_point> baby_after;
    before.read( "baby_timer", baby_before );
    after.read( "baby_timer", baby_after );
    REQUIRE( baby_before );
    CHECK( baby_after == *baby_before + 2_hours );
    CHECK( body->get_hp() == member.hp );
    body->on_load();
    time_duration repeated = 0_turns;
    snapshot( *body ).read( "suspended_evolution_time", repeated );
    CHECK( repeated == paused );
    calendar::turn += 1_turns;
    body->set_moves( 0 );
    body->process_turn();
    CHECK( body->get_moves() > 0 ); // frozen home cannot disable ordinary loaded life
    time_duration local = 0_turns;
    snapshot( *body ).read( "suspended_evolution_time", local );
    CHECK( local == paused );
    // Direct native helper preserves sub-day precision across a day boundary,
    // and serialization/reload never applies the same pause a second time.
    calendar::turn += 17_minutes;
    body->suspend_offscreen_time( 17_minutes );
    time_duration fractional = 0_turns;
    snapshot( *body ).read( "suspended_evolution_time", fractional );
    CHECK( fractional == adopted + 2_hours + 17_minutes );
    std::ostringstream saved;
    {
        JsonOut json( saved );
        body->serialize( json );
    }
    monster loaded;
    loaded.deserialize( json_loader::from_string( saved.str() ) );
    time_duration retained = 0_turns;
    snapshot( loaded ).read( "suspended_evolution_time", retained );
    CHECK( retained == fractional );
    loaded.suspend_offscreen_time( 0_turns );
    loaded.suspend_offscreen_time( -1_turns );
    snapshot( loaded ).read( "suspended_evolution_time", retained );
    CHECK( retained == fractional );
}

TEST_CASE( "migo_native_evolution_pause_keeps_fractional_day_on_reload",
           "[migo_population][migo_activation][native]" )
{
    const auto previous_time = calendar::turn;
    on_out_of_scope restore( [&]() {
        calendar::turn = previous_time;
    } );
    override_option evolution( "EVOLUTION_INVERSE_MULTIPLIER", "1.00" );
    calendar::turn = calendar::turn_zero + 4_days + 23_hours + 30_minutes;
    const mtype_id initial( "mon_feral_human_pipe_fungal_infected" );
    const mtype_id grown( "mon_feral_human_pipe_fungal_corpse" );
    monster body( initial );
    body.set_pos_abs_only( get_avatar().pos_abs() );
    REQUIRE( body.can_upgrade() );
    body.try_upgrade( true ); // native deterministic four-day growth schedule
    const int due = body.get_upgrade_time();
    REQUIRE( due > 4 );
    calendar::turn = calendar::turn_zero + due * 1_days - 30_minutes;
    body.on_unload();
    monster unpaused( body );
    calendar::turn += 2_hours;
    unpaused.try_upgrade( false );
    CHECK( unpaused.type->id == grown ); // same native rule really is due without a pause
    body.suspend_offscreen_time( 2_hours );
    body.try_upgrade( false );
    CHECK( body.type->id == initial );
    CHECK( body.get_upgrade_time() == due );
    std::ostringstream saved;
    {
        JsonOut json( saved );
        body.serialize( json );
    }
    monster loaded;
    loaded.deserialize( json_loader::from_string( saved.str() ) );
    loaded.try_upgrade( false );
    CHECK( loaded.type->id == initial );
    CHECK( loaded.get_upgrade_time() == due );
    calendar::turn += 31_minutes; // effective active date now crosses the native due day
    body.try_upgrade( false );
    loaded.try_upgrade( false );
    CHECK( body.type->id == grown );
    CHECK( loaded.type->id == grown );
}

TEST_CASE( "migo_real_tower_mapgen_and_repeated_discovery_share_eight_member_slots",
           "[migo_population][mapgen]" )
{
    using namespace migo_population;
    const world_state before = overmap_buffer.global_state.migo_populations;
    const tripoint_abs_omt anchor( 71, 73, 0 );
    const auto nearby_npcs = [&]() {
        std::set<character_id> ids;
        for( int z = 0; z < 4; ++z ) {
            for( const auto &n : overmap_buffer.get_npcs_near_omt( tripoint_abs_omt( anchor.xy(), z ), 0 ) ) {
                ids.insert( n->getID() );
            }
        }
        return ids;
    };
    const auto original_npcs = nearby_npcs();
    std::vector<oter_id> old_terrain;
    for( int z = 0; z < 4; ++z ) {
        old_terrain.push_back( overmap_buffer.ter( tripoint_abs_omt( anchor.xy(), z ) ) );
        overmap_buffer.ter_set( tripoint_abs_omt( anchor.xy(), z ),
                               oter_str_id( "mi-go_scout_tower_" + std::to_string( z + 1 ) + "_north" ) );
    }
    const auto geometry = geometry_at( anchor );
    REQUIRE( geometry );
    on_out_of_scope restore( [&]() {
        release_fixture( geometry->id, anchor );
        for( const character_id &id : nearby_npcs() ) {
            if( original_npcs.count( id ) == 0 ) {
                overmap_buffer.remove_npc( id );
            }
        }
        for( int z = 0; z < 4; ++z ) {
            overmap_buffer.ter_set( tripoint_abs_omt( anchor.xy(), z ), old_terrain[z] );
        }
        MAPBUFFER.clear_outside_reality_bubble();
        overmap_buffer.global_state.migo_populations = before;
    } );
    const bool initially_blocked = GENERATE( false, true );
    std::set<std::string> original_manifest;
    if( initially_blocked ) {
        tinymap generated;
        REQUIRE( overmap_buffer.global_state.migo_populations.find( geometry->id ) == nullptr );
        generated.load( anchor, false );
        REQUIRE( overmap_buffer.global_state.migo_populations.find( geometry->id ) );
        REQUIRE_FALSE( overmap_buffer.global_state.migo_populations.find( geometry->id )->seed_committed );
        REQUIRE( abstract_members( geometry->id, anchor ).empty() );
        std::map<tripoint_abs_ms, ter_id> terrain_before;
        for( int z = 0; z < 4; ++z ) {
            for( int x = 0; x < 2; ++x ) {
                for( int y = 0; y < 2; ++y ) {
                    const tripoint_abs_sm pos = project_to<coords::sm>( tripoint_abs_omt( anchor.xy(), z ) ) + point( x, y );
                    submap *sm = MAPBUFFER.lookup_submap_existing( pos );
                    REQUIRE( sm );
                    for( const spawn_point &slot : sm->spawns ) {
                        if( slot.population.empty() ) {
                            continue;
                        }
                        original_manifest.insert( slot.population.member_id );
                        const tripoint_abs_ms home = project_to<coords::ms>( pos ) + point_rel_ms( slot.pos.raw() );
                        for( int dx = -3; dx <= 3; ++dx ) {
                            for( int dy = -3; dy <= 3; ++dy ) {
                                const tripoint_abs_ms cell = home + point( dx, dy );
                                const tripoint_abs_sm cell_sm = project_to<coords::sm>( cell );
                                submap *target = MAPBUFFER.lookup_submap_existing( cell_sm );
                                REQUIRE( target );
                                const point_sm_ms local( cell.x() - cell_sm.x() * SEEX, cell.y() - cell_sm.y() * SEEY );
                                terrain_before.emplace( cell, target->get_ter( local ) );
                                target->set_ter( local, ter_str_id( "t_wall" ) );
                            }
                        }
                    }
                }
            }
        }
        REQUIRE( original_manifest.size() == 8 );
        for( const auto &entry : terrain_before ) {
            const tripoint_abs_sm pos = project_to<coords::sm>( entry.first );
            const submap *sm = MAPBUFFER.lookup_submap_existing( pos );
            CHECK( sm->get_ter( point_sm_ms( entry.first.x() - pos.x() * SEEX,
                                           entry.first.y() - pos.y() * SEEY ) )->movecost == 0 );
        }
        site_record *pending = discover( anchor );
        REQUIRE( pending );
        CHECK( pending->members.size() == 8 );
        CHECK( pending->reserve( 1, []( const member_record & ) { return true; } ).empty() );
        CHECK( abstract_members( geometry->id, anchor ).empty() );
        for( const member_record &member : pending->members ) {
            CHECK( member.owner == representation::slot );
            CHECK( original_manifest.count( member.token.member_id ) == 1 );
        }
        REQUIRE( discover( anchor ) );
        CHECK( abstract_members( geometry->id, anchor ).empty() );
        for( const auto &entry : terrain_before ) {
            const tripoint_abs_sm pos = project_to<coords::sm>( entry.first );
            submap *sm = MAPBUFFER.lookup_submap_existing( pos );
            sm->set_ter( point_sm_ms( entry.first.x() - pos.x() * SEEX,
                                    entry.first.y() - pos.y() * SEEY ), entry.second );
        }
    }
    site_record *site = discover( anchor );
    REQUIRE( site );
    REQUIRE( site->seed_committed );
    REQUIRE( site->members.size() == 8 );
    CHECK( site->ceiling == 8 );
    CHECK( site->defensive_reserve == 2 );
    CHECK( site->new_content );
    REQUIRE( site->source );
    CHECK( site->source_furniture == "f_migo_brood_pod" );
    CHECK_FALSE( site->occupancy_verified ); // Abstract discovery is not a full interior census.
    std::set<std::string> ids;
    int ordinary = 0;
    int scouts = 0;
    for( const member_record &m : site->members ) {
        CHECK( ids.insert( m.token.member_id ).second );
        if( initially_blocked ) {
            CHECK( original_manifest.count( m.token.member_id ) == 1 );
        }
        ordinary += m.type == mtype_id( "mon_mi_go" );
        scouts += m.type == mtype_id( "mon_mi_go_scout" );
    }
    CHECK( ordinary == 6 );
    CHECK( scouts == 2 );
    const auto generated_npcs = nearby_npcs();
    for( int i = 0; i < 3; ++i ) {
        site = discover( anchor );
        REQUIRE( site );
        CHECK( site->members.size() == 8 );
        for( const member_record &m : site->members ) {
            CHECK( ids.count( m.token.member_id ) == 1 );
        }
        CHECK( nearby_npcs() == generated_npcs );
    }
    const tripoint_abs_sm source_sm = project_to<coords::sm>( *site->source );
    submap *source_map = MAPBUFFER.lookup_submap_existing( source_sm );
    REQUIRE( source_map );
    const point_sm_ms source_cell( site->source->x() - source_sm.x() * SEEX,
                                  site->source->y() - source_sm.y() * SEEY );
    CHECK( source_map->get_furn( source_cell ).id().str() == "f_migo_brood_pod" );
    // Copying the native payload must retain its allocated member UUID, even
    // though Creature's runtime identity is copy-local.
    const auto payloads = abstract_members( site->id, anchor );
    REQUIRE( payloads.size() == 8 );
    monster copy( *payloads.front().second );
    CHECK_FALSE( copy.is_caol_predator() );
    CHECK( copy.population_identity().member_id == payloads.front().second->population_identity().member_id );
    // Explicit payloads can recover membership after missing global metadata;
    // they cannot prove which decorative pod owned the former factory. Never
    // invent a different source while recovering their actual native owner.
    const tripoint_abs_ms original_source = *site->source;
    tinymap damaged;
    damaged.load( project_to<coords::omt>( original_source ), false );
    map &source_here = *damaged.cast_to_map();
    source_here.bash( source_here.get_bub( original_source ), 999, false, true );
    REQUIRE( site->source_destroyed );
    auto &sites = overmap_buffer.global_state.migo_populations.sites;
    sites.erase( std::remove_if( sites.begin(), sites.end(), [&]( const site_record &s ) {
        return s.id == geometry->id;
    } ), sites.end() );
    REQUIRE( reconcile_payload( *payloads.front().second, representation::abstract ) );
    site = overmap_buffer.global_state.migo_populations.find( geometry->id );
    REQUIRE( site );
    CHECK( site->members.size() == 8 );
    CHECK_FALSE( site->source );
    for( const member_record &m : site->members ) {
        CHECK( ids.count( m.token.member_id ) == 1 );
    }
}

TEST_CASE( "migo_native_local_abstract_refusal_reload_and_casualty_preserve_one_owner",
           "[migo_population][ownership]" )
{
    using namespace migo_population;
    clear_map();
    const world_state before = overmap_buffer.global_state.migo_populations;
    map &here = get_map();
    const tripoint_bub_ms position( 65, 65, 0 );
    const tripoint_abs_ms absolute = here.get_abs( position );
    const tripoint_abs_omt anchor = project_to<coords::omt>( absolute );
    const std::string id = "native-member-fixture";
    on_out_of_scope restore( [&]() {
        release_fixture( id, anchor );
        overmap_buffer.global_state.migo_populations = before;
    } );
    site_record site;
    site.id = id;
    site.anchor = anchor;
    member_record member;
    member.token = { id, "one-real-member", 0 };
    member.home = absolute;
    member.type = mtype_id( "mon_mi_go" );
    member.hp = 113;
    member.owner = representation::local;
    REQUIRE( site.commit_seed( { member }, std::nullopt, "", 0, true, false ) );
    overmap_buffer.global_state.migo_populations.sites.push_back( site );
    monster payload( member.type );
    payload.population_identity() = member.token;
    payload.set_hp( 113 );
    monster *loaded = g->place_critter_at( make_shared_fast<monster>( payload ), position );
    REQUIRE( loaded );
    // A different native owner at the same key must survive rollback of the
    // prepared Mi-go handoff, even though position-only lookup finds it first.
    overmap &collision_owner = overmap_buffer.get( project_to<coords::om>( absolute ).xy() );
    REQUIRE( collision_owner.hordes.spawn_entity( absolute, mtype_id( "mon_zombie" ) ) );
    const tripoint_om_ms collision_local = project_remain<coords::om>( absolute ).remainder_tripoint;
    // Force the checked lease's real post-insertion refusal. Its receiver
    // must be removed from the actual immobile native bucket before return.
    const auto saturated_epoch = std::numeric_limits<std::uint64_t>::max();
    auto &record = overmap_buffer.global_state.migo_populations.find( id )->members.front();
    record.token.epoch = saturated_epoch;
    loaded->population_identity().epoch = saturated_epoch;
    bool accepted = true;
    const std::string refusal = capture_debugmsg_during( [&]() {
        accepted = g->despawn_monster( *loaded );
    } );
    CHECK_FALSE( accepted );
    CHECK( refusal.find( "Refusing to delete local monster after failed horde handoff" ) != std::string::npos );
    CHECK( local_member( member.token.member_id ) == loaded );
    CHECK( loaded->get_hp() == 113 );
    CHECK( record.owner == representation::local );
    CHECK( record.token.epoch == saturated_epoch );
    CHECK( abstract_members( id, anchor ).empty() );
    const auto occupants = collision_owner.entities_at( collision_local );
    REQUIRE( occupants.size() == 1 );
    CHECK( occupants.front()->get_type()->id == mtype_id( "mon_zombie" ) );
    const auto zombie_owner = collision_owner.hordes.find( collision_local, occupants.front() );
    REQUIRE( zombie_owner != collision_owner.hordes.end() );
    collision_owner.hordes.erase( zombie_owner );
    record.token.epoch = 0;
    loaded->population_identity().epoch = 0;
    REQUIRE( g->despawn_monster( *loaded ) );
    CHECK( local_member( member.token.member_id ) == nullptr );
    auto abstracts = abstract_members( id, anchor );
    REQUIRE( abstracts.size() == 1 );
    CHECK( abstracts.front().second->population_identity().epoch == 1 );
    CHECK( abstracts.front().second->get_hp() == 113 );
    here.ter_set( position, ter_str_id( "t_wall" ) );
    overmap_buffer.spawn_monster( project_to<coords::sm>( absolute ), false );
    CHECK( local_member( member.token.member_id ) == nullptr );
    REQUIRE( abstract_members( id, anchor ).size() == 1 );
    CHECK( abstract_members( id, anchor ).front().second->population_identity().epoch == 1 );
    here.ter_set( position, ter_str_id( "t_floor" ) );
    overmap_buffer.spawn_monster( project_to<coords::sm>( absolute ), false );
    loaded = local_member( member.token.member_id );
    REQUIRE( loaded );
    CHECK( loaded->get_hp() == 113 );
    CHECK( loaded->population_identity().epoch == 2 );
    CHECK( abstract_members( id, anchor ).empty() );
    std::ostringstream out;
    { JsonOut json( out ); loaded->serialize( json ); }
    monster restored( member.type );
    restored.deserialize( json_loader::from_string( out.str() ) );
    CHECK( restored.population_identity().member_id == member.token.member_id );
    CHECK( restored.population_identity().epoch == 2 );
    CHECK( restored.get_hp() == 113 );
    // An already-returning member at its authoritative home settles through
    // the native service even with no brood source and no admitted time.
    record.assignment = duty::returning;
    service_loaded();
    CHECK( record.assignment == duty::available );
    service_loaded();
    CHECK( record.assignment == duty::available );
    CHECK( overmap_buffer.global_state.migo_populations.find( id )->birth_sequence == 0 );
    loaded->die( &here, nullptr );
    g->remove_zombie( *loaded );
    site_record *owner = overmap_buffer.global_state.migo_populations.find( id );
    REQUIRE( owner );
    CHECK( owner->living_count() == 0 );
    // Reconcile a stale saved transport copy through the real receiving caller.
    restored.set_pos_abs_only( absolute );
    REQUIRE( overmap_buffer.get( project_to<coords::om>( absolute ).xy() ).retain_monster_payload(
                 absolute, restored ) );
    overmap_buffer.spawn_monster( project_to<coords::sm>( absolute ), false );
    CHECK( local_member( member.token.member_id ) == nullptr );
    CHECK( abstract_members( id, anchor ).empty() );
    CHECK( owner->living_count() == 0 );
}

TEST_CASE( "migo_native_brood_service_placement_and_destroyed_source_are_once_only",
           "[migo_population][production]" )
{
    using namespace migo_population;
    clear_map();
    const world_state before = overmap_buffer.global_state.migo_populations;
    const time_point before_time = calendar::turn;
    calendar::turn += 1_days;
    map &here = get_map();
    const tripoint_bub_ms pod( 65, 65, 0 );
    const tripoint_abs_ms absolute = here.get_abs( pod );
    const tripoint_abs_omt anchor = project_to<coords::omt>( absolute );
    const oter_id old_terrain = overmap_buffer.ter( anchor );
    overmap_buffer.ter_set( anchor, oter_str_id( "mi-go_scout_tower_1_north" ) );
    const auto geometry = geometry_at( anchor );
    REQUIRE( geometry );
    on_out_of_scope restore( [&]() {
        release_fixture( geometry->id, anchor );
        overmap_buffer.global_state.migo_populations = before;
        overmap_buffer.ter_set( anchor, old_terrain );
        calendar::turn = before_time;
        here.furn_set( pod, furn_str_id::NULL_ID() );
        clear_fields( 0 );
    } );
    here.furn_set( pod, furn_str_id( "f_migo_brood_pod" ) );
    site_record seed;
    seed.id = geometry->id;
    seed.anchor = anchor;
    seed.tower = true;
    REQUIRE( seed.commit_seed( {}, absolute, "f_migo_brood_pod", 0, true, false ) );
    // Domain controls prove admitted seven-day accounting. Here a persisted
    // nearly-due event challenges the actual production/placement callers.
    seed.production_elapsed = to_turns<int>( 7_days ) - 1;
    seed.last_service_world = to_turn<int>( calendar::turn ) - 1;
    overmap_buffer.global_state.migo_populations.sites = { seed };
    auto owner = [&]() -> site_record & {
        return *overmap_buffer.global_state.migo_populations.find( geometry->id );
    };
    // A persisted nearly-due event does not establish an old active range.
    // Adopt at zero credit first, then earn the final native interval.
    service_loaded();
    CHECK( owner().members.empty() );
    CHECK( owner().production_elapsed == to_turns<int>( 7_days ) - 1 );
    calendar::turn += 1_turns;
    SECTION( "safe_birth_and_reload_do_not_repeat_or_catch_up" ) {
        service_loaded();
        REQUIRE( owner().members.size() == 1 );
        const member_record born = owner().members.front();
        CHECK( owner().birth_sequence == 1 );
        CHECK( owner().production_elapsed == 0 );
        REQUIRE( local_member( born.token.member_id ) );
        CHECK( local_member( born.token.member_id )->population_identity().epoch == 1 );
        CHECK( abstract_members( geometry->id, anchor ).empty() );
        std::ostringstream out;
        { JsonOut json( out ); owner().serialize( json ); }
        owner().deserialize( json_loader::from_string( out.str() ) );
        service_loaded();
        CHECK( owner().members.size() == 1 );
        calendar::turn += 30_days;
        service_loaded();
        CHECK( owner().members.size() == 1 );
        CHECK( owner().production_elapsed == 0 );
        calendar::turn += 1_turns;
        service_loaded();
        CHECK( owner().production_elapsed == 1 );
    }
    SECTION( "blocked_birth_keeps_one_event_until_real_space_exists" ) {
        for( const tripoint_bub_ms &p : here.points_in_radius( pod, 3, 0 ) ) {
            here.ter_set( p, ter_str_id( "t_wall" ) );
        }
        service_loaded();
        CHECK( owner().members.empty() );
        CHECK( owner().production_elapsed == to_turns<int>( 7_days ) );
        for( int i = 0; i < 3; ++i ) {
            calendar::turn += 10_days;
            service_loaded();
            CHECK( owner().members.empty() );
        }
        here.ter_set( pod + point( 1, 0 ), ter_str_id( "t_floor" ) );
        calendar::turn += 1_turns;
        service_loaded();
        CHECK( owner().members.size() == 1 );
        CHECK( owner().birth_sequence == 1 );
        CHECK( owner().production_elapsed == 0 );
        calendar::turn += 1_turns;
        service_loaded();
        CHECK( owner().members.size() == 1 );
    }
    SECTION( "hazardous_home_has_no_safe_birth_space" ) {
        for( const tripoint_bub_ms &p : here.points_in_radius( pod, 3, 0 ) ) {
            REQUIRE( here.add_field( p, field_type_str_id( "fd_fire" ), 1 ) );
        }
        service_loaded();
        CHECK( owner().members.empty() );
        CHECK( owner().production_elapsed == to_turns<int>( 7_days ) );
        CHECK( abstract_members( geometry->id, anchor ).empty() );
    }
    SECTION( "native_bash_retires_source_and_restored_decoration_cannot_restart_it" ) {
        here.bash( pod, 999, false, true );
        CHECK( here.furn( pod ) != furn_str_id( "f_migo_brood_pod" ) );
        CHECK( owner().source_destroyed );
        CHECK( owner().production_elapsed == 0 );
        here.furn_set( pod, furn_str_id( "f_migo_brood_pod" ) );
        service_loaded();
        calendar::turn += 14_days;
        service_loaded();
        CHECK( owner().members.empty() );
        CHECK( owner().source_destroyed );
    }
    SECTION( "actual_fire_processing_destroys_dedicated_pod" ) {
        REQUIRE( here.add_field( pod, field_type_str_id( "fd_fire" ), 3 ) );
        int turns = 0;
        while( here.furn( pod ) == furn_str_id( "f_migo_brood_pod" ) && turns++ < 1000 ) {
            here.process_fields();
            calendar::turn += 1_turns;
        }
        CAPTURE( turns );
        CHECK( here.furn( pod ) != furn_str_id( "f_migo_brood_pod" ) );
        CHECK( owner().source_destroyed );
        service_loaded();
        CHECK( owner().members.empty() );
    }
}

TEST_CASE( "migo_legacy_saved_slots_preserve_capable_overcapacity_and_do_not_recruit_ambient",
           "[migo_population][mapgen][legacy]" )
{
    using namespace migo_population;
    const world_state before = overmap_buffer.global_state.migo_populations;
    const tripoint_abs_omt anchor( 81, 83, 0 );
    std::vector<oter_id> old_terrain;
    for( int z = 0; z < 4; ++z ) {
        old_terrain.push_back( overmap_buffer.ter( tripoint_abs_omt( anchor.xy(), z ) ) );
        overmap_buffer.ter_set( tripoint_abs_omt( anchor.xy(), z ),
                               oter_str_id( "mi-go_scout_tower_" + std::to_string( z + 1 ) + "_north" ) );
    }
    const auto geometry = geometry_at( anchor );
    REQUIRE( geometry );
    on_out_of_scope restore( [&]() {
        release_fixture( geometry->id, anchor );
        for( const auto &entry : overmap_buffer.get( project_to<coords::om>( anchor ).xy() ).horde_entities() ) {
            if( entry.second->monster_data && entry.second->monster_data->unique_name == "ambient-legacy-control" ) {
                // Ambient has no site lease: erase only the fixture's exact node.
                // Ordinary native insertion stores a payload with no member ID.
                entry.second->monster_data->population_identity() = { "fixture-cleanup", "ambient", 0 };
                overmap_buffer.get( project_to<coords::om>( entry.first ).xy() ).release_monster_payload(
                    entry.first, "ambient" );
            }
        }
        for( int z = 0; z < 4; ++z ) {
            for( const auto &n : overmap_buffer.get_npcs_near_omt( tripoint_abs_omt( anchor.xy(), z ), 0 ) ) {
                overmap_buffer.remove_npc( n->getID() );
            }
            overmap_buffer.ter_set( tripoint_abs_omt( anchor.xy(), z ), old_terrain[z] );
        }
        MAPBUFFER.clear_outside_reality_bubble();
        overmap_buffer.global_state.migo_populations = before;
    } );
    tinymap generated;
    generated.load( anchor, false );
    // Emulate the actual old eight-field saved spawn format, before any live
    // materialization. Its saved slots are provenance, live proximity is not.
    int original_slots = 0;
    std::vector<tripoint_abs_ms> pods;
    for( int z = 0; z < 4; ++z ) {
        for( int x = 0; x < 2; ++x ) {
            for( int y = 0; y < 2; ++y ) {
                const tripoint_abs_sm pos = project_to<coords::sm>( tripoint_abs_omt( anchor.xy(), z ) ) + point( x, y );
                submap *sm = MAPBUFFER.lookup_submap_existing( pos );
                REQUIRE( sm );
                for( spawn_point &slot : sm->spawns ) {
                    if( !slot.population.empty() ) {
                        slot.population = {};
                        if( original_slots == 0 ) {
                            slot.type = mtype_id( "mon_mi_go_guard" );
                            slot.count = 3; // Legitimate old over-capacity residents survive.
                        } else if( original_slots == 1 ) {
                            slot.type = mtype_id( "mon_mi_go_surgeon" );
                        }
                        ++original_slots;
                    }
                }
                for( int sx = 0; sx < SEEX; ++sx ) {
                    for( int sy = 0; sy < SEEY; ++sy ) {
                        const point_sm_ms cell( sx, sy );
                        const std::string type = sm->get_furn( cell ).id().str();
                        if( type == "f_migo_brood_pod" ) {
                            sm->set_furn( cell, furn_str_id( "f_alien_pod" ) );
                        }
                        if( sm->get_furn( cell ) == furn_str_id( "f_alien_pod" ) ) {
                            pods.push_back( project_to<coords::ms>( pos ) + point_rel_ms( sx, sy ) );
                        }
                    }
                }
            }
        }
    }
    REQUIRE( original_slots == 8 );
    REQUIRE_FALSE( pods.empty() );
    std::sort( pods.begin(), pods.end() );
    auto &sites = overmap_buffer.global_state.migo_populations.sites;
    sites.erase( std::remove_if( sites.begin(), sites.end(), [&]( const site_record &s ) {
        return s.id == geometry->id;
    } ), sites.end() );
    monster ambient( mtype_id( "mon_mi_go" ) );
    ambient.unique_name = "ambient-legacy-control";
    const tripoint_abs_ms ambient_pos = project_to<coords::ms>( anchor ) + point( 1, 1 );
    ambient.set_pos_abs_only( ambient_pos );
    REQUIRE( overmap_buffer.get( project_to<coords::om>( anchor ).xy() ).retain_monster_payload(
                 ambient_pos, ambient ) );
    site_record *site = discover( anchor );
    REQUIRE( site );
    CHECK_FALSE( site->new_content );
    REQUIRE( site->source );
    CHECK( *site->source == pods.front() );
    CHECK( site->source_furniture == "f_alien_pod" );
    CHECK( site->members.size() == 10 );
    CHECK( site->living_count() == 10 );
    CHECK( site->ceiling == 8 ); // Tower ceiling does not cull legitimate over-capacity residents.
    CHECK( site->ambient_occupancy == 1 );
    CHECK_FALSE( site->occupancy_verified );
    int guards = 0;
    int surgeons = 0;
    std::set<std::string> ids;
    for( const member_record &m : site->members ) {
        guards += m.type == mtype_id( "mon_mi_go_guard" );
        surgeons += m.type == mtype_id( "mon_mi_go_surgeon" );
        CHECK( ids.insert( m.token.member_id ).second );
    }
    CHECK( guards == 3 );
    CHECK( surgeons == 1 );
    REQUIRE( discover( anchor ) );
    CHECK( site->members.size() == 10 );
    CHECK( site->ambient_occupancy == 1 );
    CHECK( overmap_buffer.entity_at( ambient_pos )->monster_data->population_identity().empty() );
    // Actual legacy native bash chooses no successor among surviving pods.
    const tripoint_abs_ms chosen = *site->source;
    tinymap source_map;
    source_map.load( project_to<coords::omt>( chosen ), false );
    map &source_here = *source_map.cast_to_map();
    source_here.bash( source_here.get_bub( chosen ), 999, false, true );
    CHECK( site->source_destroyed );
    REQUIRE( discover( anchor ) );
    CHECK( site->source == chosen );
    CHECK( site->source_destroyed );
    source_here.furn_set( source_here.get_bub( chosen ), furn_str_id( "f_alien_pod" ) );
    REQUIRE( discover( anchor ) );
    CHECK( site->source_destroyed );
}

TEST_CASE( "migo_real_encampment_uses_actual_saved_residents_and_one_pod",
           "[migo_population][mapgen][encampment]" )
{
    using namespace migo_population;
    const world_state before = overmap_buffer.global_state.migo_populations;
    const tripoint_abs_omt anchor( 91, 93, 0 );
    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { anchor, "mi-go_camp1_north" },
        { anchor + point( 1, 0 ), "mi-go_camp2_north" },
        { anchor + tripoint( 0, 0, 1 ), "mi-go_camp1-1_north" },
        { anchor + tripoint( 1, 0, 1 ), "mi-go_camp2-1_north" },
        { anchor + tripoint( 1, 0, 2 ), "mi-go_camp2-2_north" },
        { anchor + tripoint( 1, 0, 3 ), "mi-go_camp2-3_north" }
    };
    std::vector<oter_id> old;
    std::set<character_id> npcs;
    for( const auto &entry : terrain ) {
        old.push_back( overmap_buffer.ter( entry.first ) );
        for( const auto &n : overmap_buffer.get_npcs_near_omt( entry.first, 0 ) ) {
            npcs.insert( n->getID() );
        }
        overmap_buffer.ter_set( entry.first, oter_str_id( entry.second ) );
    }
    const auto geometry = geometry_at( anchor + point( 1, 0 ) );
    REQUIRE( geometry );
    REQUIRE( geometry->parts.size() == 2 );
    on_out_of_scope restore( [&]() {
        release_fixture( geometry->id, anchor );
        for( size_t i = 0; i < terrain.size(); ++i ) {
            for( const auto &n : overmap_buffer.get_npcs_near_omt( terrain[i].first, 0 ) ) {
                if( npcs.count( n->getID() ) == 0 ) {
                    overmap_buffer.remove_npc( n->getID() );
                }
            }
            overmap_buffer.ter_set( terrain[i].first, old[i] );
        }
        MAPBUFFER.clear_outside_reality_bubble();
        overmap_buffer.global_state.migo_populations = before;
    } );
    // Generate actual content first without discovery/materialization. Capture
    // its real sampled slots, including capable types, then consume that set.
    for( const tripoint_abs_omt &part : geometry->parts ) {
        tinymap generated;
        generated.load( part, false );
    }
    std::map<std::string, mtype_id> slots;
    std::vector<tripoint_abs_ms> sources;
    for( const auto &entry : terrain ) {
        for( int x = 0; x < 2; ++x ) {
            for( int y = 0; y < 2; ++y ) {
                const tripoint_abs_sm pos = project_to<coords::sm>( entry.first ) + point( x, y );
                submap *sm = MAPBUFFER.lookup_submap_existing( pos );
                REQUIRE( sm );
                for( const spawn_point &slot : sm->spawns ) {
                    if( slot.population.site_id == geometry->id ) {
                        REQUIRE( slot.count == 1 );
                        CHECK( slots.emplace( slot.population.member_id, slot.type ).second );
                    }
                }
                for( int sx = 0; sx < SEEX; ++sx ) {
                    for( int sy = 0; sy < SEEY; ++sy ) {
                        if( sm->get_furn( point_sm_ms( sx, sy ) ) == furn_str_id( "f_migo_brood_pod" ) ) {
                            sources.push_back( project_to<coords::ms>( pos ) + point_rel_ms( sx, sy ) );
                        }
                    }
                }
            }
        }
    }
    REQUIRE_FALSE( slots.empty() );
    REQUIRE( sources.size() == 1 );
    site_record *site = discover( anchor );
    REQUIRE( site );
    CHECK( site->new_content );
    CHECK( site->ceiling == std::max<int>( 8, slots.size() ) );
    CHECK( site->source == sources.front() );
    CHECK( site->members.size() == slots.size() );
    for( const member_record &member : site->members ) {
        CHECK( slots.at( member.token.member_id ) == member.type );
    }
    REQUIRE( discover( anchor + point( 1, 0 ) ) );
    CHECK( site->members.size() == slots.size() );
    CHECK( site->source == sources.front() );
}

TEST_CASE( "migo_load_first_save_failure_retry_reload_preserves_seed_and_terminal_casualty",
           "[migo_population][savegame][ownership]" )
{
    using namespace migo_population;
    clear_map();
    const world_state before = overmap_buffer.global_state.migo_populations;
    const tripoint_abs_sm old_origin = get_map().get_abs_sub();
    const tripoint_abs_ms old_avatar = get_avatar().pos_abs();
    const tripoint_abs_omt anchor( 101, 103, 0 );
    std::vector<oter_id> old_terrain;
    for( int z = 0; z < 4; ++z ) {
        old_terrain.push_back( overmap_buffer.ter( tripoint_abs_omt( anchor.xy(), z ) ) );
        overmap_buffer.ter_set( tripoint_abs_omt( anchor.xy(), z ),
                               oter_str_id( "mi-go_scout_tower_" + std::to_string( z + 1 ) + "_north" ) );
    }
    const auto geometry = geometry_at( anchor );
    REQUIRE( geometry );
    std::set<character_id> original_npcs;
    for( int z = 0; z < 4; ++z ) {
        for( const auto &n : overmap_buffer.get_npcs_near_omt( tripoint_abs_omt( anchor.xy(), z ), 0 ) ) {
            original_npcs.insert( n->getID() );
        }
    }
    on_out_of_scope restore( [&]() {
        release_fixture( geometry->id, anchor );
        for( int z = 0; z < 4; ++z ) {
            for( const auto &n : overmap_buffer.get_npcs_near_omt( tripoint_abs_omt( anchor.xy(), z ), 0 ) ) {
                if( original_npcs.count( n->getID() ) == 0 ) {
                    overmap_buffer.remove_npc( n->getID() );
                }
            }
            overmap_buffer.ter_set( tripoint_abs_omt( anchor.xy(), z ), old_terrain[z] );
        }
        get_map().load( old_origin, false );
        get_avatar().setpos( get_map(), get_map().get_bub( old_avatar ) );
        MAPBUFFER.clear_outside_reality_bubble();
        overmap_buffer.global_state.migo_populations = before;
    } );
    // Main-map entry is the first discovery/materialization caller: no manual
    // discover, member injection or second group draw precedes it.
    get_map().load( project_to<coords::sm>( anchor ) - point( 4, 4 ), false );
    get_avatar().setpos( get_map(), tripoint_bub_ms( 48, 48, 0 ) );
    get_map().spawn_monsters( true );
    auto owner = [&]() -> site_record * {
        return overmap_buffer.global_state.migo_populations.find( geometry->id );
    };
    REQUIRE( owner() );
    REQUIRE( owner()->seed_committed );
    REQUIRE( owner()->members.size() == 8 );
    const auto source = owner()->source;
    REQUIRE( source );
    std::set<std::string> ids;
    for( const member_record &member : owner()->members ) {
        CHECK( ids.insert( member.token.member_id ).second );
    }
    auto physical_count = [&]() {
        int count = abstract_members( geometry->id, anchor ).size();
        for( const monster &m : g->all_monsters() ) {
            count += m.population_identity().site_id == geometry->id;
        }
        return count;
    };
    CHECK( physical_count() == 8 );
    g->save_is_dirty = false;
    REQUIRE( g->save() );
    // Refuse an actual map write after player/dimension publication starts.
    // Existing save continuity owns partial-world recovery, not this ledger.
    const std::filesystem::path maps =
        ( PATH_INFO::current_dimension_save_path() / "maps" ).get_unrelative_path();
    const std::filesystem::path held = maps.string() + ".migo-save-control";
    REQUIRE( std::filesystem::is_directory( maps ) );
    std::filesystem::rename( maps, held );
    {
        std::ofstream blocker( maps );
    }
    {
        on_out_of_scope unblock( [&]() {
            std::filesystem::remove( maps );
            std::filesystem::rename( held, maps );
        } );
        CHECK_FALSE( g->save() );
        CHECK_FALSE( g->last_confirmed_save_turn() );
        CHECK( save_continuity::incomplete(
                   world_generator->active_world->folder_path().get_unrelative_path() ) );
        CHECK( owner()->members.size() == 8 );
        CHECK( physical_count() == 8 );
        CHECK( owner()->source == source );
    }
    REQUIRE( g->save() );
    const std::string world = world_generator->active_world->world_name;
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world ) );
    REQUIRE( owner() );
    CHECK( owner()->members.size() == 8 );
    CHECK( owner()->source == source );
    CHECK( physical_count() == 8 );
    for( const member_record &member : owner()->members ) {
        CHECK( ids.count( member.token.member_id ) == 1 );
    }
    monster *victim = nullptr;
    for( monster &m : g->all_monsters() ) {
        if( m.population_identity().site_id == geometry->id ) {
            victim = &m;
            break;
        }
    }
    REQUIRE( victim );
    const std::string dead = victim->population_identity().member_id;
    victim->die( &get_map(), nullptr );
    g->remove_zombie( *victim );
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world ) );
    REQUIRE( owner() );
    CHECK( owner()->living_count() == 7 );
    CHECK( owner()->members.size() == 8 );
    CHECK( owner()->find( dead )->owner == representation::dead );
    CHECK( local_member( dead ) == nullptr );
    CHECK( physical_count() == 7 );
    get_map().spawn_monsters( true );
    CHECK( owner()->living_count() == 7 );
    CHECK( physical_count() == 7 );
}

TEST_CASE( "migo_empty_legacy_and_barry_content_never_gain_a_new_population_or_factory",
           "[migo_population][mapgen][legacy]" )
{
    using namespace migo_population;
    const bool story = GENERATE( false, true );
    const world_state before = overmap_buffer.global_state.migo_populations;
    const tripoint_abs_omt anchor( 111, 113, 0 );
    std::vector<oter_id> old;
    std::set<character_id> npcs;
    for( int z = 0; z < 4; ++z ) {
        const tripoint_abs_omt p( anchor.xy(), z );
        old.push_back( overmap_buffer.ter( p ) );
        for( const auto &n : overmap_buffer.get_npcs_near_omt( p, 0 ) ) {
            npcs.insert( n->getID() );
        }
        overmap_buffer.ter_set( p, oter_str_id( std::string( story ? "barry_" : "" ) +
                               "mi-go_scout_tower_" + std::to_string( z + 1 ) + "_north" ) );
    }
    const std::string site_id = "mi-go-tower:" + anchor.to_string_writable();
    on_out_of_scope restore( [&]() {
        release_fixture( site_id, anchor );
        for( int z = 0; z < 4; ++z ) {
            const tripoint_abs_omt p( anchor.xy(), z );
            for( const auto &n : overmap_buffer.get_npcs_near_omt( p, 0 ) ) {
                if( npcs.count( n->getID() ) == 0 ) {
                    overmap_buffer.remove_npc( n->getID() );
                }
            }
            overmap_buffer.ter_set( p, old[z] );
        }
        MAPBUFFER.clear_outside_reality_bubble();
        overmap_buffer.global_state.migo_populations = before;
    } );
    tinymap generated;
    generated.load( anchor, false );
    int story_slots = 0;
    int brood_pods = 0;
    for( int z = 0; z < 4; ++z ) {
        for( int x = 0; x < 2; ++x ) {
            for( int y = 0; y < 2; ++y ) {
                submap *sm = MAPBUFFER.lookup_submap_existing(
                                 project_to<coords::sm>( tripoint_abs_omt( anchor.xy(), z ) ) + point( x, y ) );
                REQUIRE( sm );
                if( story ) {
                    for( const spawn_point &slot : sm->spawns ) {
                        CHECK( slot.population.empty() );
                        story_slots += slot.count;
                    }
                } else {
                    sm->spawns.clear(); // An already-cleared old interior.
                }
                for( int sx = 0; sx < SEEX; ++sx ) {
                    for( int sy = 0; sy < SEEY; ++sy ) {
                        const point_sm_ms cell( sx, sy );
                        brood_pods += sm->get_furn( cell ) == furn_str_id( "f_migo_brood_pod" );
                        if( !story && ( sm->get_furn( cell ) == furn_str_id( "f_alien_pod" ) ||
                                        sm->get_furn( cell ) == furn_str_id( "f_migo_brood_pod" ) ) ) {
                            sm->set_furn( cell, furn_str_id::NULL_ID() );
                        }
                    }
                }
            }
        }
    }
    auto &sites = overmap_buffer.global_state.migo_populations.sites;
    sites.erase( std::remove_if( sites.begin(), sites.end(), [&]( const site_record &s ) {
        return s.id == site_id;
    } ), sites.end() );
    if( story ) {
        CHECK( brood_pods == 0 );
        CHECK( story_slots > 0 );
        CHECK_FALSE( geometry_at( anchor ) );
        CHECK( discover( anchor ) == nullptr );
        CHECK( overmap_buffer.global_state.migo_populations.find( site_id ) == nullptr );
    } else {
        site_record *site = discover( anchor );
        REQUIRE( site );
        CHECK_FALSE( site->new_content );
        CHECK( site->members.empty() );
        CHECK_FALSE( site->source );
        REQUIRE( discover( anchor ) );
        CHECK( site->members.empty() );
        CHECK_FALSE( site->source );
        CHECK( abstract_members( site_id, anchor ).empty() );
    }
}

TEST_CASE( "migo_saved_slot_manifest_preserves_payload_inputs_and_reads_old_spawns",
           "[migo_population][savegame][submap]" )
{
    submap original;
    original.set_all_ter( ter_str_id( "t_floor" ) );
    spawn_point claimed( mtype_id( "mon_mi_go_guard" ), 1, point_sm_ms( 4, 5 ), -1, -1, false, "resident" );
    claimed.population = { "site", "stable-member", 0, tripoint_abs_omt( 2, 3, 0 ) };
    claimed.data.ammo.emplace( itype_id( "9mm" ), jmapgen_int( 4, 9 ) );
    claimed.data.patrol_points_rel_ms = { point_rel_ms( 7, 8 ) };
    original.spawns.push_back( claimed );
    original.spawns.emplace_back( mtype_id( "mon_mi_go" ), 2, point_sm_ms( 1, 2 ) );
    std::ostringstream out;
    { JsonOut json( out ); json.start_object(); original.store( json ); json.end_object(); }
    submap loaded;
    for( JsonMember member : json_loader::from_string( out.str() ).get_object() ) {
        loaded.load( member, member.name(), savegame_version );
    }
    REQUIRE( loaded.spawns.size() == 2 );
    const spawn_point &resident = loaded.spawns[0];
    CHECK( resident.population.member_id == claimed.population.member_id );
    CHECK( resident.population.site_anchor == claimed.population.site_anchor );
    CHECK( resident.type == claimed.type );
    CHECK( resident.name == claimed.name );
    CHECK( resident.data.ammo.at( itype_id( "9mm" ) ).val == 4 );
    CHECK( resident.data.ammo.at( itype_id( "9mm" ) ).valmax == 9 );
    CHECK( resident.data.patrol_points_rel_ms == claimed.data.patrol_points_rel_ms );
    CHECK( loaded.spawns[1].population.empty() );
    CHECK( loaded.spawns[1].count == 2 );
}

TEST_CASE( "migo_partial_legacy_generation_cannot_refill_residents_or_reconstruct_a_factory",
           "[migo_population][mapgen][legacy][regression]" )
{
    using namespace migo_population;
    const world_state before = overmap_buffer.global_state.migo_populations;
    const tripoint_abs_omt anchor( 121, 123, 0 );
    std::vector<oter_id> old;
    std::set<character_id> npcs;
    for( int z = 0; z < 4; ++z ) {
        const tripoint_abs_omt p( anchor.xy(), z );
        old.push_back( overmap_buffer.ter( p ) );
        for( const auto &n : overmap_buffer.get_npcs_near_omt( p, 0 ) ) {
            npcs.insert( n->getID() );
        }
        overmap_buffer.ter_set( p, oter_str_id( "mi-go_scout_tower_" + std::to_string( z + 1 ) + "_north" ) );
    }
    const auto geometry = geometry_at( anchor );
    REQUIRE( geometry );
    on_out_of_scope restore( [&]() {
        release_fixture( geometry->id, anchor );
        for( int z = 0; z < 4; ++z ) {
            const tripoint_abs_omt p( anchor.xy(), z );
            for( const auto &n : overmap_buffer.get_npcs_near_omt( p, 0 ) ) {
                if( npcs.count( n->getID() ) == 0 ) {
                    overmap_buffer.remove_npc( n->getID() );
                }
            }
            overmap_buffer.ter_set( p, old[z] );
        }
        MAPBUFFER.clear_outside_reality_bubble();
        overmap_buffer.global_state.migo_populations = before;
    } );
    // A real old lower-floor quad survives; upper maps are absent. Only its
    // two saved residents are proven. No surviving pod exists in that quad.
    for( int x = 0; x < 2; ++x ) {
        for( int y = 0; y < 2; ++y ) {
            auto sm = std::make_unique<submap>();
            sm->set_all_ter( ter_str_id( "t_floor" ) );
            if( x == 0 && y == 0 ) {
                sm->spawns.emplace_back( mtype_id( "mon_mi_go_guard" ), 2, point_sm_ms( 4, 5 ) );
            }
            REQUIRE( MAPBUFFER.add_submap( project_to<coords::sm>( anchor ) + point( x, y ), sm ) );
        }
    }
    site_record *site = discover( anchor );
    CHECK( site != nullptr );
    // Exercise the actual missing-level mapgen caller too. Regenerated
    // resident draws/decorations are not historical population provenance.
    tinymap upper;
    upper.load( anchor + tripoint::above, false );
    site = discover( anchor );
    REQUIRE( site );
    CHECK_FALSE( site->new_content );
    CHECK( site->members.size() == 2 );
    CHECK( site->living_count() == 2 );
    CHECK_FALSE( site->source );
    for( const member_record &member : site->members ) {
        CHECK( member.type == mtype_id( "mon_mi_go_guard" ) );
    }
    REQUIRE( discover( anchor ) );
    CHECK( site->members.size() == 2 );
    CHECK( abstract_members( geometry->id, anchor ).size() == 2 );
}

TEST_CASE( "ordinary_colocated_horde_payloads_load_without_occupied_tracker_warning",
           "[horde][handoff][collision]" )
{
    clear_map();
    map &here = get_map();
    const tripoint_bub_ms position( 65, 65, 0 );
    const tripoint_abs_ms absolute = here.get_abs( position );
    overmap &owner = overmap_buffer.get( project_to<coords::om>( absolute ).xy() );
    const tripoint_om_ms local = project_remain<coords::om>( absolute ).remainder_tripoint;
    for( const tripoint_bub_ms &p : here.points_in_radius( position, 1 ) ) {
        here.ter_set( p, ter_str_id( "t_floor" ) );
    }
    monster first( mtype_id( "mon_zombie" ) );
    monster second( mtype_id( "mon_zombie_fat" ) );
    first.set_hp( 43 );
    second.set_hp( 67 );
    REQUIRE( owner.hordes.spawn_entity( absolute, first, true ) );
    REQUIRE( owner.hordes.spawn_entity( absolute, second, true ) );
    const std::string warning = capture_debugmsg_during( [&]() {
        overmap_buffer.spawn_monster( project_to<coords::sm>( absolute ), false );
    } );
    CHECK( warning.find( "there's already a monster" ) == std::string::npos );
    CHECK( owner.entities_at( local ).empty() );
    std::set<int> hp;
    for( const monster &m : g->all_monsters() ) {
        if( m.type->id == mtype_id( "mon_zombie" ) || m.type->id == mtype_id( "mon_zombie_fat" ) ) {
            hp.insert( m.get_hp() );
        }
    }
    const std::set<int> expected_hp{ 43, 67 };
    CHECK( hp == expected_hp );
}

TEST_CASE( "ordinary_horde_collision_keeps_payload_when_nearby_tiles_blocked",
           "[horde][handoff][collision]" )
{
    clear_map();
    map &here = get_map();
    const tripoint_bub_ms position( 65, 65, 0 );
    const tripoint_abs_ms absolute = here.get_abs( position );
    overmap &owner = overmap_buffer.get( project_to<coords::om>( absolute ).xy() );
    const tripoint_om_ms local = project_remain<coords::om>( absolute ).remainder_tripoint;
    here.ter_set( position, ter_str_id( "t_floor" ) );
    monster *occupant = g->place_critter_at( mtype_id( "mon_zombie" ), position );
    REQUIRE( occupant );
    for( const tripoint_bub_ms &p : here.points_in_radius( position, 1 ) ) {
        if( p != position ) {
            here.ter_set( p, ter_str_id( "t_wall" ) );
        }
    }
    monster payload( mtype_id( "mon_zombie_fat" ) );
    payload.set_hp( 67 );
    REQUIRE( owner.hordes.spawn_entity( absolute, payload, true ) );
    const std::string warning = capture_debugmsg_during( [&]() {
        overmap_buffer.spawn_monster( project_to<coords::sm>( absolute ), false );
    } );
    CHECK( warning.find( "there's already a monster" ) == std::string::npos );
    const auto remaining = owner.entities_at( local );
    REQUIRE( remaining.size() == 1 );
    REQUIRE( remaining.front()->monster_data );
    CHECK( remaining.front()->monster_data->get_hp() == 67 );
    CHECK( get_creature_tracker().find( absolute ).get() == occupant );
    here.ter_set( position + tripoint::east, ter_str_id( "t_floor" ) );
    overmap_buffer.spawn_monster( project_to<coords::sm>( absolute ), false );
    CHECK( owner.entities_at( local ).empty() );
    const monster *loaded = get_creature_tracker().find( here.get_abs( position + tripoint::east ) ).get();
    REQUIRE( loaded );
    CHECK( loaded->get_hp() == 67 );
}
