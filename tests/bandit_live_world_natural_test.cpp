#include "bandit_live_world.h"
#include "bandit_pursuit_handoff.h"

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "avatar.h"
#include "clzones.h"
#include "coordinates.h"
#include "do_turn.h"
#include "faction.h"
#include "game.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "map_scale_constants.h"
#include "npc.h"
#include "omdata.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "point.h"
#include "player_helpers.h"
#include "rng.h"

bandit_live_world::structural_route_read live_bandit_structural_route_read_for_test(
    const bandit_live_world::site_record &, const bandit_live_world::structural_outing_plan &,
    int &watch_path_budget );

static const string_id<npc_template> npc_template_test_talker( "test_talker" );

namespace
{
TEST_CASE( "local reality safety preflight rejects thermal hostile contact", "[bandit][observer]" )
{
    clear_avatar();
    clear_map();
    map &here = get_map();
    avatar &observer = get_avatar();
    observer.setpos( here, tripoint_bub_ms( 60, 60, 0 ) );

    monster &kreck = spawn_test_monster( "mon_kreck", tripoint_bub_ms( 61, 60, 0 ) );
    const std::string unsafe = live_bandit_local_reality_safety_record_for_test( here, observer,
                               kreck );
    CHECK( unsafe == live_bandit_local_reality_safety_record_for_test( here, observer, kreck ) );
    CHECK( unsafe.find( "identity=mon_kreck@(61,60,0)" ) != std::string::npos );
    CHECK( unsafe.find( "type=mon_kreck" ) != std::string::npos );
    CHECK( unsafe.find( "senses=infrared:yes" ) != std::string::npos );
    CHECK( unsafe.find( "contact=yes" ) != std::string::npos );
    CHECK( unsafe.find( "verdict=unsafe" ) != std::string::npos );

    monster &deer = spawn_test_monster( "mon_deer", tripoint_bub_ms( 70, 60, 0 ) );
    const std::string safe = live_bandit_local_reality_safety_record_for_test( here, observer, deer );
    CHECK( safe.find( "type=mon_deer" ) != std::string::npos );
    CHECK( safe.find( "contact=no" ) != std::string::npos );
    CHECK( safe.find( "hostile=no" ) != std::string::npos );
    CHECK( safe.find( "same_z_possible_reach=" ) != std::string::npos );
    CHECK( safe.find( "verdict=safe" ) != std::string::npos );
}

std::optional<std::string> existing_special_lookup( const tripoint_abs_omt &omt )
{
    if( const std::optional<overmap_special_id> special =
            overmap_buffer.overmap_special_at_existing( omt ) ) {
        return special->str();
    }
    return std::nullopt;
}

std::map<std::string, tripoint_abs_omt> find_natural_hostile_camps()
{
    std::map<std::string, tripoint_abs_omt> found;
    // Match the hidden fresh-world route: generate (1,0) through the ordinary default route
    // while no cardinal neighbor is loaded.
    overmap &generated = overmap_buffer.get( point_abs_om( 1, 0 ) );
    for( int x = 0; x < OMAPX && found.size() < 2; ++x ) {
        for( int y = 0; y < OMAPY && found.size() < 2; ++y ) {
            const tripoint_om_omt local( x, y, 0 );
            const std::optional<overmap_special_id> special = generated.overmap_special_at( local );
            if( special && ( *special == overmap_special_id( "bandit_camp" ) ||
                             *special == overmap_special_id( "cannibal_camp" ) ) ) {
                found.emplace( special->str(), project_combine( generated.pos(), local ) );
            }
        }
    }
    return found;
}

const bandit_live_world::site_record *find_site_by_source(
    const bandit_live_world::world_state &state, const std::string &source_id )
{
    for( const bandit_live_world::site_record &site : state.sites ) {
        if( site.source_id == source_id ) {
            return &site;
        }
    }
    return nullptr;
}

bandit_live_world::world_state round_trip( const bandit_live_world::world_state &state )
{
    std::ostringstream buffer;
    JsonOut json( buffer, true );
    state.serialize( json );

    JsonValue json_in = json_loader::from_string( buffer.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( json_in.get_object() );
    return loaded;
}

std::string serialize_state( const bandit_live_world::world_state &state )
{
    std::ostringstream buffer;
    JsonOut json( buffer, true );
    state.serialize( json );
    return buffer.str();
}

void generate_camp_footprint( const bandit_live_world::site_record &site )
{
    for( const tripoint_abs_omt &omt : site.footprint ) {
        if( omt.z() != 0 ) {
            continue;
        }
        smallmap generated;
        generated.generate( omt, calendar::turn, false );
        generated.delete_unmerged_submaps();
    }
}

std::optional<std::string> natural_terrain_lookup( const tripoint_abs_omt &omt )
{
    if( !overmap_buffer.ter_existing( omt ).is_valid() ) {
        return std::nullopt;
    }
    return overmap_buffer.ter( omt ).id().str();
}
} // namespace

TEST_CASE( "naturally generated hostile camps register and reconcile their mapgen rosters",
           "[bandit][live_world][natural_worldgen][slow]" )
{
    std::vector<character_id> generated_npc_ids;
    zone_manager original_zones = zone_manager::get_manager();
    const unsigned int original_game_seed = g->get_seed();
    on_out_of_scope clear_generated_state( [&generated_npc_ids,
                                            original_zones = std::move( original_zones ),
                                            original_game_seed]() mutable {
        for( const character_id &id : generated_npc_ids ) {
            shared_ptr_fast<npc> generated_npc = overmap_buffer.remove_npc( id );
            if( generated_npc && generated_npc->get_faction() ) {
                generated_npc->get_faction()->remove_member( id );
            }
        }
        zone_manager::get_manager() = std::move( original_zones );
        g->set_seed( original_game_seed );
        overmap_buffer.clear();
    } );

    overmap_buffer.clear();
    REQUIRE_FALSE( overmap_buffer.has( point_abs_om( 1, 0 ) ) );
    REQUIRE_FALSE( overmap_buffer.has( point_abs_om( 1, -1 ) ) );
    REQUIRE_FALSE( overmap_buffer.has( point_abs_om( 1, 1 ) ) );
    REQUIRE_FALSE( overmap_buffer.has( point_abs_om( 0, 0 ) ) );
    REQUIRE_FALSE( overmap_buffer.has( point_abs_om( 2, 0 ) ) );
    constexpr std::uint32_t raw_seed = 830205018;
    g->set_seed( raw_seed );
    rng_set_engine_seed( raw_seed );
    const std::map<std::string, tripoint_abs_omt> camps = find_natural_hostile_camps();
    CAPTURE( raw_seed );
    REQUIRE( camps.count( "bandit_camp" ) == 1 );
    REQUIRE( camps.count( "cannibal_camp" ) == 1 );

    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    for( const std::pair<const std::string, tripoint_abs_omt> &camp : camps ) {
        const bandit_live_world::abstract_bootstrap_result result =
            bandit_live_world::register_abstract_sites_near( state, camp.second, 3,
                    existing_special_lookup );
        CHECK( result.created_sites == 1 );
        CHECK( result.recognized_tiles == 4 );
    }

    REQUIRE( state.sites.size() == 2 );
    const bandit_live_world::site_record *bandit_site = find_site_by_source( state, "bandit_camp" );
    const bandit_live_world::site_record *cannibal_site = find_site_by_source( state, "cannibal_camp" );
    REQUIRE( bandit_site != nullptr );
    REQUIRE( cannibal_site != nullptr );
    CHECK( bandit_site->profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( cannibal_site->profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    CHECK( bandit_site->footprint.size() == 8 );
    CHECK( cannibal_site->footprint.size() == 8 );
    CHECK( bandit_site->living_total == 6 );
    CHECK( cannibal_site->living_total == 5 );

    for( const std::pair<const std::string, tripoint_abs_omt> &camp : camps ) {
        const bandit_live_world::abstract_bootstrap_result repeat =
            bandit_live_world::register_abstract_sites_near( state, camp.second, 3,
                    existing_special_lookup );
        CHECK( repeat.created_sites == 0 );
        CHECK( repeat.recognized_tiles == 4 );
    }
    CHECK( state.sites.size() == 2 );

    const tripoint_abs_omt bandit_anchor = bandit_site->anchor;
    const tripoint_abs_omt cannibal_anchor = cannibal_site->anchor;
    generate_camp_footprint( *bandit_site );
    generate_camp_footprint( *cannibal_site );

    bandit_site = find_site_by_source( state, "bandit_camp" );
    cannibal_site = find_site_by_source( state, "cannibal_camp" );
    REQUIRE( bandit_site != nullptr );
    REQUIRE( cannibal_site != nullptr );
    CHECK( bandit_site->living_total == 14 );
    CHECK( cannibal_site->living_total == 14 );
    CHECK( bandit_site->members.size() == 14 );
    CHECK( cannibal_site->members.size() == 14 );
    for( const bandit_live_world::site_record &site : state.sites ) {
        for( const bandit_live_world::member_record &member : site.members ) {
            generated_npc_ids.push_back( member.npc_id );
        }
    }

    const bandit_live_world::world_state loaded = round_trip( state );
    CHECK( loaded.sites.size() == 2 );
    const bandit_live_world::site_record *loaded_bandit = find_site_by_source( loaded, "bandit_camp" );
    const bandit_live_world::site_record *loaded_cannibal = find_site_by_source( loaded,
            "cannibal_camp" );
    REQUIRE( loaded_bandit != nullptr );
    REQUIRE( loaded_cannibal != nullptr );
    CHECK( loaded_bandit->members.size() == 14 );
    CHECK( loaded_cannibal->members.size() == 14 );
    CHECK( loaded_bandit->anchor == bandit_anchor );
    CHECK( loaded_cannibal->anchor == cannibal_anchor );

    // Exercise the production near-ring scan and planner against the unchanged generated
    // geometry, but keep every mutation in an in-memory copy.  The live route reader owns the
    // actual see-cost, watch-ring, and normalized 18-OMT route checks used by the game path.
    const std::string durable_state_before = serialize_state( state );
    bandit_live_world::world_state oracle;
    oracle.sites.push_back( *bandit_site );
    int watch_path_budget = 8;
    std::vector<bandit_live_world::structural_bounty_scan_result> scans;
    for( const int now_minutes : { 0, 60, 120 } ) {
        scans.push_back( bandit_live_world::advance_structural_bounty_scan(
                             oracle, now_minutes, 4, natural_terrain_lookup ) );
    }

    const bandit_live_world::site_record &oracle_site = oracle.sites.front();
    int total_candidates = 0;
    int total_leads = 0;
    for( const bandit_live_world::structural_bounty_scan_result &scan : scans ) {
        total_candidates += scan.candidates_sampled;
        total_leads += scan.leads_seeded;
    }
    const std::vector<bandit_live_world::structural_outing_plan> candidates =
        bandit_live_world::plan_structural_bounty_outing_candidates( oracle_site, 120, false );
    REQUIRE( candidates.size() == 6 );
    // This is the production per-site cap owned by routine_candidate_full_route_solve_cap.
    constexpr std::size_t production_site_route_solve_cap = 2;
    const std::vector<bandit_live_world::structural_outing_plan> production_candidates(
        candidates.begin(), candidates.begin() + production_site_route_solve_cap );
    const std::vector<bandit_live_world::structural_route_read> reads =
        live_bandit_structural_route_analyzer_reads_for_test( oracle_site, production_candidates,
                watch_path_budget );
    REQUIRE( reads.size() == production_candidates.size() );
    std::vector<std::string> route_records;
    route_records.reserve( production_candidates.size() );
    int selected = 0;
    int rejected = 0;
    for( std::size_t index = 0; index < production_candidates.size(); ++index ) {
        const std::string record = live_bandit_structural_route_analyzer_record_for_test(
                                        oracle_site, production_candidates[index], "non_frontier", reads[index] );
        route_records.push_back( record );
        if( record.find( " outcome=selected " ) != std::string::npos ) {
            selected++;
        } else {
            rejected++;
        }
    }
    CAPTURE( oracle_site.anchor, oracle_site.footprint.size(), total_candidates, total_leads,
             oracle_site.intelligence_map.leads.size(), candidates.size(), reads.size(),
             watch_path_budget, selected, rejected );
    CHECK( total_candidates == 12 );
    CHECK( total_leads == 12 );
    CHECK( route_records.size() == production_candidates.size() );
    CHECK( watch_path_budget == 5 );
    CHECK( selected == 2 );
    CHECK( rejected == 0 );
    CAPTURE( route_records[0], route_records[1] );
    CHECK( route_records[0].find(
               "target=(217,30,0) selector=non_frontier outcome=selected "
               "watch=(218,27,0) route_cost=8" ) != std::string::npos );
    CHECK( route_records[1].find(
               "target=(214,25,0) selector=non_frontier outcome=selected "
               "watch=(217,27,0) route_cost=12" ) != std::string::npos );
    for( const std::string &record : route_records ) {
        CHECK( record.find( "selector=non_frontier" ) != std::string::npos );
        const bool selected_record = record.find( " outcome=selected " ) != std::string::npos;
        const bool rejected_record = record.find( " outcome=rejected " ) != std::string::npos;
        CHECK( selected_record != rejected_record );
    }
    INFO( "natural feasibility oracle route records:" );
    for( const std::string &record : route_records ) {
        INFO( record );
    }
    CHECK( durable_state_before == serialize_state( state ) );
}

TEST_CASE( "natural hunting blind watch uses the production selector and exact pair handoff",
           "[bandit][live_world][natural_worldgen][structural_bounty][slow]" )
{
    overmap_buffer.clear();
    on_out_of_scope clear_generated_world( []() {
        overmap_buffer.clear();
    } );

    const point_abs_om origin( 31, 31 );
    for( int dx = -1; dx <= 1; ++dx ) {
        for( int dy = -1; dy <= 1; ++dy ) {
            const point_abs_om address( origin.x() + dx, origin.y() + dy );
            overmap_special_batch custom_batch( address );
            overmap_buffer.create_custom_overmap( address, custom_batch );
        }
    }
    overmap &generated = overmap_buffer.get( origin );
    const overmap_special &blind = overmap_special_id( "Hunting Blind" ).obj();
    const city cit;
    std::optional<tripoint_abs_omt> blind_omt;
    for( int x = 8; x < OMAPX - 8 && !blind_omt; ++x ) {
        for( int y = 8; y < OMAPY - 8 && !blind_omt; ++y ) {
            const tripoint_om_omt local( x, y, 0 );
            for( int corridor_x = x - 6; corridor_x <= x + 2; ++corridor_x ) {
                for( int corridor_y = y - 3; corridor_y <= y + 4; ++corridor_y ) {
                    generated.ter_set( { corridor_x, corridor_y, 0 }, oter_id( "field" ) );
                }
            }
            generated.ter_set( { x, y + 1, 0 }, oter_id( "field" ) );
            generated.ter_set( { x, y + 2, 0 }, oter_id( "field" ) );
            generated.ter_set( { x, y + 3, 0 }, oter_id( "field" ) );
            if( generated.ter( { x, y + 1, 0 } ).id().str() != "field" ||
                generated.ter( { x, y + 2, 0 } ).id().str() != "field" ||
                generated.ter( { x, y + 3, 0 } ).id().str() != "field" ||
                !generated.can_place_special( blind, local, om_direction::type::north, false ) ) {
                continue;
            }
            const std::vector<tripoint_om_omt> placed = generated.place_special(
                    blind, local, om_direction::type::north, cit, false, false );
            if( placed.size() == 1 ) {
                blind_omt = project_combine( origin, placed.front() );
            }
        }
    }
    REQUIRE( blind_omt );
    const tripoint_abs_omt blind_pos = *blind_omt;
    const tripoint_abs_omt target( blind_pos.x(), blind_pos.y() + 3, blind_pos.z() );
    CHECK( overmap_buffer.ter( tripoint_abs_omt( blind_pos.x(), blind_pos.y() + 1,
             blind_pos.z() ) ).id().str() == "field" );
    CHECK( overmap_buffer.ter( tripoint_abs_omt( blind_pos.x(), blind_pos.y() + 2,
             blind_pos.z() ) ).id().str() == "field" );
    CHECK( overmap_buffer.ter( target ).id().str() == "field" );

    bandit_live_world::world_state world;
    bandit_live_world::site_record site;
    site.site_id = "overmap_special:bandit_camp@" +
                   std::to_string( blind_pos.x() + 1 ) + "," +
                   std::to_string( blind_pos.y() - 2 ) + ",0";
    site.source_kind = bandit_live_world::anchor_source_kind::overmap_special;
    site.site_kind = bandit_live_world::owned_site_kind::bandit_camp;
    site.profile = bandit_live_world::hostile_site_profile::camp_style;
    site.source_id = "bandit_camp";
    site.anchor = tripoint_abs_omt( blind_pos.x() + 1, blind_pos.y() - 2, 0 );
    site.footprint.push_back( site.anchor );
    site.routine_activated_minutes = 0;
    site.living_total = 3;
    site.supply_accounted_living_total = 3;
    const tripoint_abs_ms home = project_to<coords::ms>( site.anchor );
    for( int index = 0; index < 3; ++index ) {
        const tripoint_abs_ms tile( home.x() + index, home.y(), home.z() );
        site.members.push_back( { character_id( 710000 + index ), "bandit", tile,
                                  bandit_live_world::member_state::at_home, false, "" } );
        site.spawn_tiles.push_back( { tile, 1 } );
    }
    world.sites.push_back( site );
    world.sites.front().intelligence_map.terrain_scan_cursor = 6;

    const auto terrain_lookup = []( const tripoint_abs_omt &omt ) -> std::optional<std::string> {
        if( !overmap_buffer.ter_existing( omt ).is_valid() ) {
            return std::nullopt;
        }
        return overmap_buffer.ter( omt ).id().str();
    };
    const auto route_lookup = []( const bandit_live_world::site_record &owned,
                                  const bandit_live_world::structural_outing_plan &plan ) {
        int budget = 64;
        return live_bandit_structural_route_read_for_test( owned, plan, budget );
    };
    const auto maintenance = bandit_live_world::advance_structural_bounty_maintenance(
                                 world, 1000, 1, 1, terrain_lookup,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read();
    }, route_lookup );
    const bandit_live_world::site_record &routed = world.sites.front();
    CAPTURE( blind_pos, target, maintenance.scan.candidates_sampled, maintenance.scan.leads_seeded,
             maintenance.dispatches_planned, maintenance.dispatches_blocked,
             maintenance.scan.notes, maintenance.outing.notes,
             bandit_live_world::render_structural_bounty_maintenance_report( maintenance ) );
    REQUIRE( maintenance.scan.candidates_sampled == 1 );
    REQUIRE( maintenance.dispatches_planned == 1 );
    REQUIRE( maintenance.dispatches_applied == 1 );
    REQUIRE( routed.active_outing.is_active() );
    CHECK( routed.active_outing.selected_watch_kind != bandit_live_world::structural_watch_kind::none );
    CHECK( routed.active_outing.selected_watch_omt == blind_pos );
    CHECK( routed.active_outing.selected_watch_route_cost >= 0 );
    CHECK( routed.active_outing.selected_watch_route_cost <= 18 );
    CHECK( routed.active_outing.target_footprint.size() == 1 );
    CHECK( routed.active_outing.target_footprint.front() == target );
    CHECK( routed.active_outing.shared_route.front() == routed.anchor );
    const auto selected_watch_route_iter = std::find( routed.active_outing.shared_route.begin(),
            routed.active_outing.shared_route.end(), routed.active_outing.selected_watch_omt );
    REQUIRE( selected_watch_route_iter != routed.active_outing.shared_route.end() );
    CHECK( std::count( routed.active_outing.shared_route.begin(),
                       routed.active_outing.shared_route.end(),
                       routed.active_outing.selected_watch_omt ) == 1 );
    CHECK( bandit_live_world::structural_watch_shared_route_is_canonical(
               routed.active_outing.shared_route, routed.anchor,
               routed.active_outing.selected_watch_omt, routed.active_outing.target_footprint ) );
    CHECK( std::find( routed.active_outing.shared_route.begin(),
                      routed.active_outing.shared_route.end(), target ) ==
           routed.active_outing.shared_route.end() );

    bandit_live_world::site_record &mutable_site = world.sites.front();
    mutable_site.active_outing.waypoint_index = static_cast<int>( std::distance(
            mutable_site.active_outing.shared_route.cbegin(), selected_watch_route_iter ) );
    CHECK( mutable_site.active_outing.shared_route[static_cast<std::size_t>(
              mutable_site.active_outing.waypoint_index )] == mutable_site.active_outing.selected_watch_omt );
    mutable_site.active_outing.phase = bandit_live_world::scout_phase::observing;
    mutable_site.active_outing.last_advanced_minutes = 1000;
    REQUIRE( mutable_site.active_outing.member_ids.size() == 2 );

    clear_avatar();
    clear_map();
    clear_creatures();
    const bandit_live_world::world_state saved_owner =
        overmap_buffer.global_state.bandit_live_world;
    const time_point saved_turn = calendar::turn;
    calendar::turn += 1000_minutes;
    std::vector<character_id> generated_ids;
    on_out_of_scope restore_owner( [&saved_owner, &generated_ids, saved_turn]() {
        for( const character_id id : generated_ids ) {
            g->remove_npc( id );
            overmap_buffer.remove_npc( id );
        }
        clear_creatures();
        overmap_buffer.global_state.bandit_live_world = saved_owner;
        calendar::turn = saved_turn;
    } );
    const tripoint_abs_omt production_route_edge =
        mutable_site.active_outing.shared_route[static_cast<std::size_t>(
            mutable_site.active_outing.waypoint_index )];
    g->place_player_overmap( production_route_edge );
    const tripoint_abs_ms player_position =
        project_to<coords::ms>( production_route_edge ) + point( SEEX, SEEY );
    REQUIRE( get_map().inbounds( player_position ) );
    get_avatar().setpos( player_position, false );
    wipe_map_terrain();
    for( const character_id old_id : mutable_site.active_outing.member_ids ) {
        shared_ptr_fast<npc> member = make_shared_fast<npc>();
        member->normalize();
        member->load_npc_template( npc_template_test_talker );
        member->spawn_at_precise( mutable_site.find_member( old_id )->home_spawn_tile );
        overmap_buffer.insert_npc( member );
        generated_ids.push_back( member->getID() );
        mutable_site.find_member( old_id )->npc_id = member->getID();
    }
    mutable_site.active_outing.member_ids = generated_ids;
    mutable_site.active_outing.leader_id = generated_ids.front();
    mutable_site.active_outing.owner = bandit_live_world::simulation_owner::abstract;
    overmap_buffer.global_state.bandit_live_world = std::move( world );
    bandit_live_world::site_record &live_site =
        overmap_buffer.global_state.bandit_live_world.sites.front();
    const int applied_return_before = live_site.applied_return_generation;
    const int applied_report_before = live_site.applied_report_generation;

    REQUIRE( materialize_live_bandit_structural_handoffs_for_test() );
    const int local_epoch_before = live_site.active_outing.handoff_epoch;
    CHECK( live_site.active_outing.owner == bandit_live_world::simulation_owner::local );
    REQUIRE( live_site.active_outing.local_handoff.members.size() == generated_ids.size() );
    for( std::size_t index = 0; index < generated_ids.size(); ++index ) {
        const auto &snapshot = live_site.active_outing.local_handoff.members[index];
        CHECK( snapshot.npc_id == generated_ids[index] );
        CHECK( project_to<coords::omt>( snapshot.entry_position ) ==
               production_route_edge );
        CHECK( project_to<coords::omt>( snapshot.staging_position ) ==
               production_route_edge );
        CHECK( snapshot.entry_position != snapshot.staging_position );
        REQUIRE( overmap_buffer.find_npc( generated_ids[index] ) );
        CHECK( overmap_buffer.find_npc( generated_ids[index] )->is_active() );
    }
    const std::size_t movement_stop_bound = get_map().points_on_zlevel(
            production_route_edge.z() ).size();
    std::size_t movement_steps = 0;
    bool pair_reached_staging = false;
    while( movement_steps < movement_stop_bound && !pair_reached_staging ) {
        process_monsters_and_npcs_turn_for_test();
        pair_reached_staging = std::all_of( generated_ids.begin(), generated_ids.end(),
        [&live_site]( const character_id id ) {
            const shared_ptr_fast<npc> member = overmap_buffer.find_npc( id );
            const auto snapshot = std::find_if(
                                       live_site.active_outing.local_handoff.members.begin(),
                                       live_site.active_outing.local_handoff.members.end(),
            [id]( const bandit_live_world::local_handoff_member_snapshot &candidate ) {
                return candidate.npc_id == id;
            } );
            return member && snapshot != live_site.active_outing.local_handoff.members.end() &&
                   member->pos_abs() == snapshot->staging_position;
        } );
        movement_steps++;
    }
    REQUIRE( pair_reached_staging );
    process_monsters_and_npcs_turn_for_test();
    for( const character_id id : generated_ids ) {
        REQUIRE( overmap_buffer.find_npc( id ) );
        CHECK( overmap_buffer.find_npc( id )->is_active() );
    }

    g->place_player_overmap( live_site.anchor + point( 8, 8 ) );
    clear_map();
    clear_creatures();
    CHECK_FALSE( get_map().inbounds( project_to<coords::ms>( live_site.anchor ) +
                                     point( SEEX, SEEY ) ) );
    REQUIRE( dematerialize_live_bandit_structural_handoffs_for_test() );
    CHECK( live_site.active_outing.owner == bandit_live_world::simulation_owner::abstract );
    CHECK( live_site.active_outing.local_handoff.is_abstract_resume() );
    CHECK( live_site.active_outing.local_handoff.members.size() == generated_ids.size() );
    for( const character_id id : generated_ids ) {
        CHECK( overmap_buffer.find_npc( id ) );
        CHECK_FALSE( overmap_buffer.find_npc( id )->is_active() );
    }
    CHECK( live_site.active_outing.member_ids == generated_ids );
    CHECK( live_site.active_outing.handoff_epoch == local_epoch_before + 1 );
    CHECK( live_site.applied_return_generation == applied_return_before );
    CHECK( live_site.applied_report_generation == applied_report_before );
    CHECK_FALSE( materialize_live_bandit_structural_handoffs_for_test() );

    // Keep the pair under the abstract owner long enough for the ordinary structural
    // cadence to consume the observation and report boundary.  Re-materializing immediately
    // would leave a resumed local pair in observing, where the local motor is correctly not
    // allowed to manufacture return receipts.
    const auto advance_abstract_structural_cadence = [&]() {
        const int current_minutes = to_minutes<int>( calendar::turn - calendar::start_of_cataclysm );
        const int next_structural_cadence = current_minutes +
                                            ( 60 - current_minutes % 60 );
        calendar::turn += ( next_structural_cadence - current_minutes ) * 1_minutes;
        process_overmap_npc_move_for_test();
    };
    REQUIRE( live_site.active_outing.phase == bandit_live_world::scout_phase::observing );
    while( live_site.active_outing.phase == bandit_live_world::scout_phase::observing ) {
        advance_abstract_structural_cadence();
    }
    REQUIRE( live_site.active_outing.owner == bandit_live_world::simulation_owner::abstract );
    REQUIRE( live_site.active_outing.phase == bandit_live_world::scout_phase::returning_report );
    CHECK( live_site.active_outing.member_ids == generated_ids );
    CHECK( live_site.active_outing.local_handoff.is_abstract_resume() );
    while( live_site.active_outing.phase == bandit_live_world::scout_phase::returning_report ) {
        advance_abstract_structural_cadence();
    }
    REQUIRE( live_site.active_outing.owner == bandit_live_world::simulation_owner::abstract );
    REQUIRE( live_site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
    CHECK( live_site.active_outing.member_ids == generated_ids );
    CHECK( live_site.active_outing.local_handoff.is_abstract_resume() );

    const int persisted_resume_floor = std::max(
        live_site.active_outing.last_advanced_minutes,
        live_site.active_outing.local_handoff.committed_minutes );
    const int current_minutes = to_minutes<int>( calendar::turn - calendar::start_of_cataclysm );
    CAPTURE( persisted_resume_floor, current_minutes,
             live_site.active_outing.last_advanced_minutes,
             live_site.active_outing.local_handoff.committed_minutes );
    REQUIRE( persisted_resume_floor >= current_minutes );
    g->place_player_overmap( live_site.active_outing.local_handoff.route_position );
    CHECK( overmap_buffer.find_npc( generated_ids.front() ) );
    CHECK( overmap_buffer.find_npc( generated_ids.back() ) );
    calendar::turn += ( persisted_resume_floor - current_minutes + 1 ) * 1_minutes;
    REQUIRE( to_minutes<int>( calendar::turn - calendar::start_of_cataclysm ) >
             persisted_resume_floor );
    REQUIRE( materialize_live_bandit_structural_handoffs_for_test() );
    REQUIRE( live_site.active_outing.owner == bandit_live_world::simulation_owner::local );
    CHECK( live_site.active_outing.local_handoff.is_active() );
    CHECK( live_site.active_outing.local_handoff.committed_minutes >= persisted_resume_floor );

    const std::map<character_id, tripoint_abs_ms> positions_before_generic_travel = [&]() {
        std::map<character_id, tripoint_abs_ms> result;
        for( const character_id id : generated_ids ) {
            const shared_ptr_fast<npc> member = overmap_buffer.find_npc( id );
            REQUIRE( member );
            result.emplace( id, member->pos_abs() );
        }
        return result;
    }();
    process_overmap_npc_move_for_test();
    CHECK( live_site.active_outing.owner == bandit_live_world::simulation_owner::local );
    for( const auto &entry : positions_before_generic_travel ) {
        const shared_ptr_fast<npc> member = overmap_buffer.find_npc( entry.first );
        REQUIRE( member );
        CHECK( member->is_active() );
        CHECK( member->pos_abs() == entry.second );
    }

    // The ordinary turn hook must roll back the whole candidate when its second
    // identity-bound receipt is already present.  Both physical members remain
    // active and at camp; only the candidate-side duplicate makes the second
    // receipt reject after the first one has succeeded on the candidate copy.
    REQUIRE( live_site.active_outing.member_return_receipts.empty() );
    const character_id duplicate_id = generated_ids.back();
    const auto owner_before_duplicate_receipt = live_site.active_outing.owner;
    const auto cursor_before_duplicate_receipt = live_site.active_outing.local_handoff;
    for( const character_id id : generated_ids ) {
        const shared_ptr_fast<npc> member = overmap_buffer.find_npc( id );
        REQUIRE( member );
        member->setpos( project_to<coords::ms>( live_site.anchor ), false );
        member->set_moves( 0 );
    }
    live_site.active_outing.member_return_receipts.push_back( {
        duplicate_id, bandit_pursuit_handoff::make_operation_component_key(
            live_site.active_outing.activity_id, live_site.active_outing.generation,
            "return", std::to_string( duplicate_id.get_value() ) ),
        to_minutes<int>( calendar::turn - calendar::start_of_cataclysm )
    } );
    const std::size_t seeded_receipt_count = live_site.active_outing.member_return_receipts.size();
    process_overmap_npc_move_for_test();
    CHECK( live_site.active_outing.owner == owner_before_duplicate_receipt );
    CHECK( live_site.active_outing.member_return_receipts.size() == seeded_receipt_count );
    CHECK( live_site.active_outing.resolved_member_ids.empty() );
    CHECK( live_site.active_outing.local_handoff.route_position ==
           cursor_before_duplicate_receipt.route_position );
    for( const character_id id : generated_ids ) {
        const shared_ptr_fast<npc> member = overmap_buffer.find_npc( id );
        REQUIRE( member );
        CHECK( member->is_active() );
        CHECK( member->pos_abs_omt() == live_site.anchor );
    }
    live_site.active_outing.member_return_receipts.clear();
    for( const auto &entry : positions_before_generic_travel ) {
        const shared_ptr_fast<npc> member = overmap_buffer.find_npc( entry.first );
        REQUIRE( member );
        member->setpos( entry.second, false );
    }

    // The physical-arrival recorder is one transaction for the exact pair.  An invalid
    // unresolved member must prevent even the partner already at the camp OMT from being
    // unloaded or receiving a receipt; the ordinary turn hook must leave ownership and the
    // receipt-specific state untouched.
    const bandit_live_world::simulation_owner owner_before_invalid_arrival =
        live_site.active_outing.owner;
    const character_id invalid_id = generated_ids.front();
    const character_id camp_id = generated_ids.back();
    REQUIRE( overmap_buffer.find_npc( invalid_id ) );
    REQUIRE( overmap_buffer.find_npc( camp_id ) );
    const tripoint_abs_omt invalid_arrival_omt = live_site.anchor + point( 1, 0 );
    REQUIRE( invalid_arrival_omt != live_site.anchor );
    const int invalid_moves_before = overmap_buffer.find_npc( invalid_id )->get_moves();
    const int camp_moves_before = overmap_buffer.find_npc( camp_id )->get_moves();
    overmap_buffer.find_npc( invalid_id )->setpos( project_to<coords::ms>( invalid_arrival_omt ), false );
    overmap_buffer.find_npc( camp_id )->setpos( project_to<coords::ms>( live_site.anchor ), false );
    overmap_buffer.find_npc( invalid_id )->set_moves( 0 );
    overmap_buffer.find_npc( camp_id )->set_moves( 0 );
    process_monsters_and_npcs_turn_for_test();
    CHECK( live_site.active_outing.owner == owner_before_invalid_arrival );
    CHECK( live_site.active_outing.member_return_receipts.empty() );
    CHECK( live_site.active_outing.resolved_member_ids.empty() );
    REQUIRE( overmap_buffer.find_npc( invalid_id ) );
    CHECK( overmap_buffer.find_npc( invalid_id )->is_active() );
    CHECK( overmap_buffer.find_npc( invalid_id )->pos_abs_omt() != live_site.anchor );
    CHECK( overmap_buffer.find_npc( camp_id )->is_active() );
    overmap_buffer.find_npc( invalid_id )->setpos( positions_before_generic_travel.at( invalid_id ), false );
    overmap_buffer.find_npc( camp_id )->setpos( positions_before_generic_travel.at( camp_id ), false );
    overmap_buffer.find_npc( invalid_id )->set_moves( invalid_moves_before );
    overmap_buffer.find_npc( camp_id )->set_moves( camp_moves_before );

    const std::size_t homeward_turn_bound = get_map().points_on_zlevel(
        live_site.active_outing.local_handoff.route_position.z() ).size() *
        live_site.active_outing.shared_route.size();
    std::size_t homeward_turns = 0;
    while( live_site.active_outing.owner == bandit_live_world::simulation_owner::local &&
           homeward_turns < homeward_turn_bound ) {
        process_monsters_and_npcs_turn_for_test();
        homeward_turns++;
    }
    CAPTURE( homeward_turns, homeward_turn_bound,
             live_site.active_outing.member_return_receipts.size(),
             live_site.active_outing.local_handoff.route_position,
             live_site.active_outing.phase );
    REQUIRE( live_site.active_outing.owner == bandit_live_world::simulation_owner::abstract );
    REQUIRE( live_site.active_outing.member_return_receipts.size() == generated_ids.size() );
    CHECK( live_site.active_outing.resolved_member_ids == generated_ids );
    for( const character_id id : generated_ids ) {
        const auto receipt = std::find_if(
            live_site.active_outing.member_return_receipts.begin(),
            live_site.active_outing.member_return_receipts.end(),
        [id]( const bandit_live_world::structural_member_return_receipt & candidate ) {
            return candidate.member_id == id;
        } );
        REQUIRE( receipt != live_site.active_outing.member_return_receipts.end() );
        const shared_ptr_fast<npc> unloaded_member = overmap_buffer.find_npc( id );
        REQUIRE( unloaded_member );
        CHECK_FALSE( unloaded_member->is_active() );
        CHECK( unloaded_member->pos_abs_omt() == live_site.anchor );
    }

    const int report_revision_before = live_site.current_scout_report.revision;
    const int current_minutes_before_report = to_minutes<int>(
        calendar::turn - calendar::start_of_cataclysm );
    const int next_structural_cadence = current_minutes_before_report +
                                        ( 60 - current_minutes_before_report % 60 );
    calendar::turn += ( next_structural_cadence - current_minutes_before_report ) * 1_minutes;
    process_overmap_npc_move_for_test();
    CHECK( live_site.current_scout_report.is_present() );
    CHECK( live_site.current_scout_report.source_generation == live_site.active_outing.generation );
    CHECK( live_site.current_scout_report.revision > report_revision_before );
    CHECK( live_site.applied_report_generation == live_site.active_outing.generation );
    CHECK( live_site.camp_decision.state !=
           bandit_live_world::camp_decision_state::report_awaiting_assessment );
}
