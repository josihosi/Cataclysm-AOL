#include "bandit_live_world.h"

#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "clzones.h"
#include "coordinates.h"
#include "do_turn.h"
#include "faction.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_scale_constants.h"
#include "npc.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "point.h"
#include "rng.h"

namespace
{
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
    // The test runner initializes overmap (0,0) with an empty custom batch.  Generate one adjacent
    // overmap through the ordinary default route so mandatory-special spill remains bounded.
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
    on_out_of_scope clear_generated_state( [&generated_npc_ids,
                                            original_zones = std::move( original_zones )]() mutable {
        for( const character_id &id : generated_npc_ids ) {
            shared_ptr_fast<npc> generated_npc = overmap_buffer.remove_npc( id );
            if( generated_npc && generated_npc->get_faction() ) {
                generated_npc->get_faction()->remove_member( id );
            }
        }
        zone_manager::get_manager() = std::move( original_zones );
        overmap_buffer.clear();
    } );

    overmap_buffer.clear();
    REQUIRE_FALSE( overmap_buffer.has( point_abs_om( 1, 0 ) ) );
    rng_set_engine_seed( 830204914 );
    const std::map<std::string, tripoint_abs_omt> camps = find_natural_hostile_camps();
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
    CHECK( watch_path_budget == 0 );
    CHECK( selected == 2 );
    CHECK( rejected == 0 );
    CHECK( route_records[0].find(
               "target=(207,31,0) selector=non_frontier outcome=selected "
               "watch=(210,29,0) route_cost=10" ) != std::string::npos );
    CHECK( route_records[1].find(
               "target=(211,27,0) selector=non_frontier outcome=selected "
               "watch=(209,30,0) route_cost=10" ) != std::string::npos );
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
