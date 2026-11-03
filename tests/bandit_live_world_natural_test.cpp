#include "bandit_live_world.h"
#include "bandit_live_world_probe.h"
#include "bandit_pursuit_handoff.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <iostream>
#include <iterator>
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
#include "item.h"
#include "json.h"
#include "json_loader.h"
#include "live_light.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "map_scale_constants.h"
#include "npc.h"
#include "options.h"
#include "path_info.h"
#include "filesystem.h"
#include "omdata.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "point.h"
#include "player_helpers.h"
#include "rng.h"
#include "regional_settings.h"
#include "worldfactory.h"

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
    const cata_path disk = PATH_INFO::current_dimension_save_path() / "overmaps/o.1.0.zzip";
    const cata_path plain = PATH_INFO::current_dimension_save_path() / "o.1.0";
    const bool region_present = get_options().has_option( "DEFAULT_REGION" );
    std::cout << "batch_worldgen_input catch_seed=" << Catch::getCurrentContext().getConfig()->rngSeed()
              << " game_seed=" << g->get_seed()
              << " absent_target_cardinals_asserted=1"
              << " disk_present=" << file_exist( disk )
              << " plain_present=" << file_exist( plain )
              << " compression=" << world_generator->active_world->has_compression_enabled()
              << " default_region_present=" << region_present
              << " default_region=" << ( region_present ?
                                         get_options().get_option( "DEFAULT_REGION" ).getValue() : "unavailable" )
              << '\n';
    std::cout << "batch_worldgen_lane"
#if defined(_MSC_VER)
              << " msvc=" << _MSC_VER
#else
              << " compiler=" << __VERSION__
#endif
#if defined(TILES)
              << " tiles=1"
#else
              << " tiles=0"
#endif
#if defined(_LIBCPP_VERSION)
              << " libcxx=" << _LIBCPP_VERSION
#elif defined(__GLIBCXX__)
              << " libstdcxx=" << __GLIBCXX__
#endif
              << '\n';
    bandit_live_world_probe::snapshot request_trace;
    overmap *requested = nullptr;
    {
        bandit_live_world_probe::session request_probe(
            bandit_live_world_probe::collection_mode::transition_events );
        requested = &overmap_buffer.get( point_abs_om( 1, 0 ) );
        request_trace = request_probe.result();
    }
    overmap &generated = *requested;
    for( const auto &event : request_trace.transition_events ) {
        if( event.transition == "overmap_request" ) {
            std::cout << "batch_worldgen_branch requested=(1,0) returned=" << generated.pos().to_string()
                      << " seed=" << g->get_seed() << " stage=" << event.outcome
                      << " " << event.reason << '\n';
        }
    }
    std::cout << "batch_worldgen_branch dropped=" << request_trace.dropped_transition_events << '\n';
    std::cout << "batch_worldgen_region id=" << generated.get_settings().id.str()
              << " city_size=" << generated.get_settings().get_settings_city().city_size
              << " city_spacing=" << generated.get_settings().get_settings_city().city_spacing << '\n';
    int scanned_tiles = 0;
    int bandit_tiles = 0;
    int cannibal_tiles = 0;
    std::map<std::string, int> terrain_counts;
    for( int x = 0; x < OMAPX; ++x ) {
        for( int y = 0; y < OMAPY; ++y ) {
            const tripoint_om_omt local( x, y, 0 );
            ++scanned_tiles;
            ++terrain_counts[generated.ter( local ).id().str()];
            const std::optional<overmap_special_id> special = generated.overmap_special_at( local );
            if( special && ( *special == overmap_special_id( "bandit_camp" ) ||
                             *special == overmap_special_id( "cannibal_camp" ) ) ) {
                bandit_tiles += *special == overmap_special_id( "bandit_camp" );
                cannibal_tiles += *special == overmap_special_id( "cannibal_camp" );
                found.emplace( special->str(), project_combine( generated.pos(), local ) );
            }
        }
    }
    std::cout << "batch_worldgen_camps scanned_tiles=" << scanned_tiles
              << " observed_bandit_tiles=" << bandit_tiles
              << " observed_cannibal_tiles=" << cannibal_tiles
              << " found_bandit=" << found.count( "bandit_camp" )
              << " found_cannibal=" << found.count( "cannibal_camp" )
              << " scan_complete=" << ( scanned_tiles == OMAPX * OMAPY ) << '\n';
    std::vector<std::pair<std::string, int>> top_terrain( terrain_counts.begin(),
                                          terrain_counts.end() );
    std::sort( top_terrain.begin(), top_terrain.end(), []( const auto & a, const auto & b ) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    } );
    std::cout << "batch_worldgen_content omt=" << generated.pos().to_string()
              << " z=0 tiles=" << scanned_tiles << " distinct_terrain=" << terrain_counts.size()
              << " cities=" << generated.cities.size() << " top=";
    for( std::size_t i = 0; i < std::min<std::size_t>( 8, top_terrain.size() ); ++i ) {
        std::cout << top_terrain[i].first << ':' << top_terrain[i].second << ',';
    }
    std::cout << '\n';
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

void check_native_camp_registrations( bandit_live_world::world_state &state,
                                      const std::map<std::string, tripoint_abs_omt> &camps )
{
    for( const auto &camp : camps ) {
        const bandit_live_world::abstract_bootstrap_result registered =
            bandit_live_world::register_abstract_sites_near( state, camp.second, 3,
                    existing_special_lookup );
        CHECK( registered.created_sites == 1 );
        CHECK( registered.recognized_tiles == 4 );
        const bandit_live_world::site_record *site = find_site_by_source( state, camp.first );
        REQUIRE( site != nullptr );
        CHECK( site->anchor == camp.second );
        CHECK( site->profile == ( camp.first == "cannibal_camp" ?
                                  bandit_live_world::hostile_site_profile::cannibal_camp :
                                  bandit_live_world::hostile_site_profile::camp_style ) );
        const bandit_live_world::abstract_bootstrap_result repeated =
            bandit_live_world::register_abstract_sites_near( state, camp.second, 3,
                    existing_special_lookup );
        CHECK( repeated.created_sites == 0 );
        CHECK( repeated.recognized_tiles == 4 );
    }
    CHECK( state.sites.size() == camps.size() );
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
        for( const character_id &id : generated_npc_ids )
        {
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
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    check_native_camp_registrations( state, camps );

    INFO( "ordinary default worldgen admitted " << camps.size()
          << " Native hostile camp special(s); zero admission is a valid outcome" );
    if( camps.size() != 2 ) {
        return;
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
    for( const int now_minutes : {
             0, 60, 120
         } ) {
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
    // The production candidate inventory is already capped before route analysis.
    const std::vector<bandit_live_world::structural_outing_plan> &production_candidates =
        candidates;
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
    CHECK( watch_path_budget >= 0 );
    CHECK( watch_path_budget <= 8 );
    CHECK( selected + rejected == static_cast<int>( production_candidates.size() ) );
    for( std::size_t index = 0; index < route_records.size(); ++index ) {
        const auto &record = route_records[index];
        CAPTURE( record );
        CHECK( record.find( "selector=non_frontier" ) != std::string::npos );
        const bool selected_record = record.find( " outcome=selected " ) != std::string::npos;
        const bool rejected_record = record.find( " outcome=rejected " ) != std::string::npos;
        REQUIRE( selected_record != rejected_record );
        // Generation admits camps, not an optical lookout. Consume the actual
        // reader result: unavailable native geometry must remain a refusal,
        // while an admitted route must satisfy the same canonical watch rules.
        auto candidate = production_candidates[index];
        const bool admitted = bandit_live_world::refine_structural_outing_route(
                                  oracle_site, 120, reads[index], candidate );
        REQUIRE( admitted == selected_record );
        if( admitted ) {
            auto selected_site = oracle_site;
            REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan(
                         selected_site, candidate, 120 ) );
            const auto &selected_outing = selected_site.active_outing;
            CHECK( selected_outing.selected_watch_kind != bandit_live_world::structural_watch_kind::none );
            CHECK( selected_outing.selected_watch_route_cost >= 0 );
            CHECK( selected_outing.selected_watch_route_cost <= 18 );
            CHECK( bandit_live_world::structural_watch_shared_route_is_canonical(
                       selected_outing.shared_route, oracle_site.anchor, selected_outing.selected_watch_omt,
                       selected_outing.target_footprint ) );
        } else {
            CHECK_FALSE( reads[index].summary.empty() );
        }
    }
    INFO( "natural feasibility oracle route records:" );
    for( const std::string &record : route_records ) {
        INFO( record );
    }
    CHECK( durable_state_before == serialize_state( state ) );
}

TEST_CASE( "deterministic Native camp special placement registers both camp factions",
           "[bandit][live_world][natural_worldgen][registration]" )
{
    const bandit_live_world::world_state old_world =
        overmap_buffer.global_state.bandit_live_world;
    const unsigned int old_seed = g->get_seed();
    on_out_of_scope restore_world( [old_world, old_seed]() {
        // This isolated custom region owns every member it materialized,
        // including partial mapgen if a REQUIRE unwinds the fixture.
        for( const auto &site : overmap_buffer.global_state.bandit_live_world.sites ) {
            for( const auto &member : site.members ) {
                auto actor = overmap_buffer.remove_npc( member.npc_id );
                if( actor && actor->get_faction() ) {
                    actor->get_faction()->remove_member( member.npc_id );
                }
            }
        }
        overmap_buffer.clear();
        overmap_buffer.global_state.bandit_live_world = old_world;
        g->set_seed( old_seed );
    } );
    overmap_buffer.clear();
    overmap_buffer.global_state.bandit_live_world.clear();
    constexpr std::uint32_t raw_seed = 830205018;
    g->set_seed( raw_seed );
    rng_set_engine_seed( raw_seed );
    // Generate ordinary base geography without the random special deck: a
    // naturally admitted OVERMAP_UNIQUE camp would correctly reject a second
    // explicit placement of that same special in this deterministic control.
    overmap_special_batch empty_batch( point_abs_om( 1, 0 ) );
    overmap_buffer.create_custom_overmap( point_abs_om( 1, 0 ), empty_batch );
    overmap &generated = overmap_buffer.get( point_abs_om( 1, 0 ) );
    bandit_live_world::world_state &state = overmap_buffer.global_state.bandit_live_world;
    const auto place_native_special = [&]( const overmap_special_id & special_id )
    -> std::optional<tripoint_abs_omt> {
        const overmap_special &special = special_id.obj();
        const city no_city;
        // Use a fixed, bounded set of ordinary placement attempts, separate
        // from the stochastic default-worldgen admission case.
        for( int attempt = 0; attempt < 100; ++attempt )
        {
            const tripoint_om_omt local( 10 + ( attempt % 10 ) * 16,
                                         10 + ( attempt / 10 ) * 16, 0 );
            if( !generated.can_place_special( special, local, om_direction::type::north, false ) ) {
                continue;
            }
            const std::vector<tripoint_om_omt> placed = generated.place_special(
                        special, local, om_direction::type::north, no_city, false, false );
            for( const tripoint_om_omt tile : placed ) {
                if( generated.overmap_special_at( tile ) == special_id ) {
                    const tripoint_om_omt origin = generated.overmap_special_origin_at( tile ).value_or( tile );
                    return project_combine( generated.pos(), origin );
                }
            }
        }
        return std::nullopt;
    };

    const std::map<std::string, tripoint_abs_omt> camps = {
        {
            "bandit_camp", place_native_special( overmap_special_id( "bandit_camp" ) ).value_or(
                tripoint_abs_omt::invalid )
        },
        {
            "cannibal_camp", place_native_special( overmap_special_id( "cannibal_camp" ) ).value_or(
                tripoint_abs_omt::invalid )
        }
    };
    for( const auto &camp : camps ) {
        REQUIRE( camp.second != tripoint_abs_omt::invalid );
    }
    check_native_camp_registrations( state, camps );
    REQUIRE( state.sites.size() == 2 );
    std::set<character_id> all_members;
    for( const auto &camp : camps ) {
        const auto *site = find_site_by_source( state, camp.first );
        REQUIRE( site );
        generate_camp_footprint( *site );
        site = find_site_by_source( state, camp.first );
        REQUIRE( site );
        REQUIRE( site->members.size() == 14 );
        CHECK( site->living_total == 14 );
        CHECK( site->roster().valid );
        for( const auto &member : site->members ) {
            REQUIRE( all_members.insert( member.npc_id ).second );
            const auto actor = overmap_buffer.find_npc( member.npc_id );
            REQUIRE( actor );
            CHECK( std::find( site->footprint.begin(), site->footprint.end(),
                              actor->pos_abs_omt() ) != site->footprint.end() );
        }
    }
    const auto loaded = round_trip( state );
    for( const auto &camp : camps ) {
        const auto *site = find_site_by_source( loaded, camp.first );
        REQUIRE( site );
        CHECK( site->members.size() == 14 );
        CHECK( site->roster().valid );
        const auto repeated = bandit_live_world::register_abstract_sites_near(
                                  state, camp.second, 3, existing_special_lookup );
        CHECK( repeated.created_sites == 0 );
        CHECK( repeated.recognized_tiles == 4 );
    }
    CHECK( all_members.size() == 28 );
}

TEST_CASE( "signal residents followed by native camp generation preserve ownership",
           "[bandit][live_world][natural_worldgen][signal_ownership]" )
{
    const bandit_live_world::world_state old_world = overmap_buffer.global_state.bandit_live_world;
    const unsigned int old_seed = g->get_seed();
    on_out_of_scope restore( [old_world, old_seed]() {
        for( const auto &site : overmap_buffer.global_state.bandit_live_world.sites ) {
            for( const auto &member : site.members ) {
                if( overmap_buffer.find_npc( member.npc_id ) ) {
                    auto actor = overmap_buffer.remove_npc( member.npc_id );
                    if( actor && actor->get_faction() ) {
                        actor->get_faction()->remove_member( member.npc_id );
                    }
                }
            }
        }
        overmap_buffer.clear();
        overmap_buffer.global_state.bandit_live_world = old_world;
        g->set_seed( old_seed );
    } );
    overmap_buffer.clear();
    auto &state = overmap_buffer.global_state.bandit_live_world;
    state.clear();
    constexpr std::uint32_t seed = 830205018;
    g->set_seed( seed );
    rng_set_engine_seed( seed );
    overmap_special_batch empty_batch( point_abs_om( 1, 0 ) );
    overmap_buffer.create_custom_overmap( point_abs_om( 1, 0 ), empty_batch );
    overmap &generated = overmap_buffer.get( point_abs_om( 1, 0 ) );
    const overmap_special_id special_id( "bandit_camp" );
    const overmap_special &special = special_id.obj();
    const city no_city;
    std::optional<tripoint_abs_omt> origin;
    for( int attempt = 0; attempt < 100 && !origin; ++attempt ) {
        const tripoint_om_omt local( 10 + ( attempt % 10 ) * 16,
                                     10 + ( attempt / 10 ) * 16, 0 );
        if( !generated.can_place_special( special, local, om_direction::type::north, false ) ) {
            continue;
        }
        const auto placed = generated.place_special( special, local, om_direction::type::north,
                            no_city, false, false );
        for( const tripoint_om_omt tile : placed ) {
            if( generated.overmap_special_at( tile ) == special_id ) {
                origin = project_combine( generated.pos(),
                                         generated.overmap_special_origin_at( tile ).value_or( tile ) );
                break;
            }
        }
    }
    REQUIRE( origin );
    bandit_live_world::register_abstract_sites_near( state, *origin, 3, existing_special_lookup );
    REQUIRE( state.sites.size() == 1 );
    auto &site = state.sites.front();
    REQUIRE( site.source_id == "bandit_camp" );
    const auto observer = resolve_live_bandit_staffed_observer_for_test( state, 0 );
    REQUIRE( observer.exclusion_reason.empty() );
    live_bandit_signal_observation smoke;
    smoke.mark.kind = "smoke";
    smoke.mark.mark_id = "ownership-control";
    smoke.source_omt = site.anchor;
    smoke.range_cap_omt = 4;
    observe_live_hostile_signal_sources_for_test( { smoke } );
    std::set<character_id> lazy_ids;
    for( const auto &member : site.members ) {
        lazy_ids.insert( member.npc_id );
    }
    REQUIRE( lazy_ids.size() > 1 );
    const tripoint_abs_omt anchor = site.anchor;
    {
        auto unrelated = round_trip( state );
        CHECK_FALSE( bandit_live_world::consume_pending_native_spawn( unrelated, "cannibal_camp",
                     anchor, "bandit" ) );
        CHECK_FALSE( bandit_live_world::consume_pending_native_spawn( unrelated, "bandit_camp",
                     anchor + point( 50, 0 ), "bandit" ) );
        CHECK_FALSE( bandit_live_world::consume_pending_native_spawn( unrelated, "bandit_camp",
                     anchor, "bandit_trader" ) );
        for( auto &member : unrelated.sites.front().members ) {
            REQUIRE( member.pending_native_spawn );
            member.pending_native_spawn = false; // Legacy provenance is unknown.
        }
        CHECK_FALSE( bandit_live_world::consume_pending_native_spawn( unrelated, "bandit_camp",
                     anchor, "bandit" ) );
        unrelated = round_trip( state );
        unrelated.sites.push_back( unrelated.sites.front() );
        CHECK_FALSE( bandit_live_world::consume_pending_native_spawn( unrelated, "bandit_camp",
                     anchor, "bandit" ) );
    }
    const character_id away_id = *lazy_ids.begin();
    const character_id dead_id = *std::next( lazy_ids.begin() );
    auto away = overmap_buffer.find_npc( away_id );
    REQUIRE( away );
    away->spawn_at_omt( anchor + point( 2, 0 ) );
    away->set_pain( 25 );
    away->set_part_hp_cur( bodypart_id( "arm_l" ), 20 );
    away->i_add( item( itype_id( "rock" ), calendar::turn ) );
    const tripoint_abs_ms away_position = away->pos_abs();
    const auto away_mission = away->mission;
    REQUIRE( bandit_live_world::update_member_state( site, dead_id,
             bandit_live_world::member_state::dead, "control casualty" ) );
    auto casualty = overmap_buffer.remove_npc( dead_id );
    REQUIRE( casualty );
    if( casualty->get_faction() ) {
        casualty->get_faction()->remove_member( dead_id );
    }
    // Persist both the overmap NPC bodies and the owning ledger before mapgen.
    const auto saved_before_mapgen = round_trip( state );
    overmap_buffer.save();
    overmap_buffer.reset();
    state = saved_before_mapgen;
    overmap_buffer.get( point_abs_om( 1, 0 ) );
    away = overmap_buffer.find_npc( away_id );
    REQUIRE( away );
    REQUIRE( away->pos_abs() == away_position );
    REQUIRE( away->get_pain() == 25 );
    REQUIRE( away->get_part_hp_cur( bodypart_id( "arm_l" ) ) == 20 );
    REQUIRE( away->has_amount( itype_id( "rock" ), 1 ) );
    CHECK( away->mission == away_mission );
    for( const tripoint_abs_omt &omt : state.sites.front().footprint ) {
        if( omt.z() == 0 ) {
            smallmap native_map;
            native_map.generate( omt, calendar::turn, true );
        }
    }
    const auto *after = find_site_by_source( state, "bandit_camp" );
    REQUIRE( after );
    const JsonArray definitions = json_loader::from_path( cata_path( cata_path::root_path::data,
                                  "json/mapgen/hells_raiders/bandit_camp.json" ) ).get_array();
    REQUIRE_FALSE( definitions.empty() );
    JsonObject definition = definitions.get_object( 0 );
    definition.allow_omitted_members();
    JsonObject map_object = definition.get_object( "object" );
    map_object.allow_omitted_members();
    const JsonArray placements = map_object.get_array( "place_npcs" );
    const std::size_t native_slots = placements.size();
    std::map<std::string, int> native_templates;
    for( JsonObject placement : placements ) {
        placement.allow_omitted_members();
        ++native_templates[placement.get_string( "class" )];
    }
    CHECK( after->members.size() == native_slots );
    CHECK( after->living_total == static_cast<int>( native_slots ) - 1 );
    std::set<character_id> ids;
    std::map<std::string, int> actual_templates;
    for( const auto &member : after->members ) {
        REQUIRE( ids.insert( member.npc_id ).second );
        ++actual_templates[member.npc_template_id];
        if( member.npc_id == dead_id ) {
            CHECK( member.state == bandit_live_world::member_state::dead );
            CHECK_FALSE( overmap_buffer.find_npc( member.npc_id ) );
        } else {
            REQUIRE( overmap_buffer.find_npc( member.npc_id ) );
        }
        CHECK_FALSE( member.pending_native_spawn );
    }
    CHECK( actual_templates == native_templates );
    for( const character_id id : lazy_ids ) {
        CHECK( ids.count( id ) == 1 );
    }
    CHECK( after->roster().valid );
    away = overmap_buffer.find_npc( away_id );
    REQUIRE( away );
    CHECK( away->pos_abs() == away_position );
    CHECK( away->get_pain() == 25 );
    CHECK( away->get_part_hp_cur( bodypart_id( "arm_l" ) ) == 20 );
    CHECK( away->has_amount( itype_id( "rock" ), 1 ) );
    CHECK( away->mission == away_mission );
    const auto saved = round_trip( state );
    REQUIRE( saved.sites.size() == 1 );
    CHECK( saved.sites.front().living_total == after->living_total );
    CHECK( saved.sites.front().members.size() == ids.size() );
    overmap_buffer.save();
    overmap_buffer.reset();
    state = saved;
    overmap_buffer.get( point_abs_om( 1, 0 ) );
    away = overmap_buffer.find_npc( away_id );
    REQUIRE( away );
    CHECK( away->pos_abs() == away_position );
    CHECK( away->get_part_hp_cur( bodypart_id( "arm_l" ) ) == 20 );
    CHECK( away->has_amount( itype_id( "rock" ), 1 ) );
    CHECK( away->mission == away_mission );
    CHECK_FALSE( overmap_buffer.find_npc( dead_id ) );
    bandit_live_world::register_abstract_sites_near( state, anchor, 3, existing_special_lookup );
    CHECK( state.sites.front().members.size() == ids.size() );
}

TEST_CASE( "pending native placement preserves an active local outing",
           "[bandit][live_world][signal_ownership]" )
{
    JsonObject document = json_loader::from_path( cata_path( cata_path::root_path::base,
                          "tests/data/r054_local_ownership_clock.json" ) ).get_object();
    document.allow_omitted_members();
    JsonObject fixture = document.get_array( "cases" ).get_object( 0 );
    fixture.allow_omitted_members();
    bandit_live_world::world_state world;
    world.deserialize( fixture.get_object( "world" ) );
    auto *site = world.find_site( "overmap_special:bandit_camp@129,149,0" );
    REQUIRE( site );
    REQUIRE( site->roster().valid );
    const auto *outing = site->active_external_outing();
    REQUIRE( outing );
    REQUIRE( outing->owner == bandit_live_world::simulation_owner::local );
    auto *member = site->find_member( character_id( 4 ) );
    REQUIRE( member );
    REQUIRE( member->state == bandit_live_world::member_state::local_contact );
    // The retained fixture predates provenance. Set the new marker explicitly
    // as a control; this does not infer it for that historical save.
    member->pending_native_spawn = true;
    const std::string before = serialize_state( world );
    REQUIRE( bandit_live_world::consume_pending_native_spawn( world, "bandit_camp",
             site->anchor, "bandit" ) );
    CHECK_FALSE( member->pending_native_spawn );
    member->pending_native_spawn = true;
    CHECK( serialize_state( world ) == before );
}

TEST_CASE( "natural hunting blind watch uses the production selector and exact pair handoff",
           "[bandit][live_world][natural_worldgen][structural_bounty][slow]" )
{
    const time_point saved_fixture_turn = calendar::turn;
    on_out_of_scope restore_fixture_turn( [saved_fixture_turn]() {
        set_time( saved_fixture_turn );
    } );
    map &main_map = get_map();
    const tripoint_abs_ms saved_player_position = get_avatar().pos_abs();
    const tripoint_abs_sm saved_map_origin = main_map.get_abs_sub();
    on_out_of_scope restore_location( [&main_map, saved_player_position, saved_map_origin]() {
        g->place_player_overmap( project_to<coords::omt>( saved_player_position ), false );
        main_map.load( saved_map_origin, false );
        get_avatar().setpos( saved_player_position, false );
    } );
    // The optical query uses native illumination at the same minute as the
    // maintenance transaction, rather than midnight before its later handoff.
    set_time( calendar::start_of_cataclysm + 1000_minutes );
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

    // Overmap placement alone does not create the physical target geometry
    // required by the production optical reader. Prepare it through ordinary
    // native map loading; the reader still decides visibility and route safety.
    {
        tinymap target_geometry;
        target_geometry.load( target, false );
    }

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

    const auto terrain_lookup = []( const tripoint_abs_omt & omt ) -> std::optional<std::string> {
        if( !overmap_buffer.ter_existing( omt ).is_valid() )
        {
            return std::nullopt;
        }
        return overmap_buffer.ter( omt ).id().str();
    };
    const auto route_lookup = []( const bandit_live_world::site_record & owned,
    const bandit_live_world::structural_outing_plan & plan ) {
        int budget = 64;
        return live_bandit_structural_route_read_for_test( owned, plan, budget );
    };
    bandit_live_world_probe::session optical_probe(
        bandit_live_world_probe::collection_mode::transition_events );
    const auto maintenance = bandit_live_world::advance_structural_bounty_maintenance(
                                 world, 1000, 1, 1, terrain_lookup,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read();
    }, route_lookup );
    const bandit_live_world::site_record &routed = world.sites.front();
    std::ostringstream optical_reads;
    const auto &optical_snapshot = optical_probe.result();
    for( const auto &event : optical_snapshot.transition_events ) {
        if( event.transition == "watch_geometry_read" ) {
            optical_reads << event.site_id << " generation=" << event.generation
                          << " " << event.reason << '\n';
        }
    }
    CAPTURE( optical_reads.str(), optical_snapshot.dropped_transition_events );
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

    const int dispatched_generation = routed.active_outing.generation;
    const std::string dispatched_activity_id = routed.active_outing.activity_id;
    const std::string dispatched_report_key = routed.active_outing.report_application_key;
    REQUIRE( dispatched_generation > 0 );
    REQUIRE_FALSE( dispatched_activity_id.empty() );
    REQUIRE_FALSE( dispatched_report_key.empty() );

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
    const auto report_staging_bodies = [&]( const char *stage ) {
        std::cout << "batch_blind_staging stage=" << stage << " seed=" <<
                  Catch::getCurrentContext().getConfig()->rngSeed() << " turn=" <<
                  to_turn<int>( calendar::turn ) << " owner=" <<
                  bandit_live_world::to_string( live_site.active_outing.owner ) <<
                  " epoch=" << live_site.active_outing.handoff_epoch << '\n';
        for( const auto id : generated_ids ) {
            const auto member = overmap_buffer.find_npc( id );
            const auto snapshot = std::find_if( live_site.active_outing.local_handoff.members.begin(),
                                                live_site.active_outing.local_handoff.members.end(),
            [id]( const auto & entry ) {
                return entry.npc_id == id;
            } );
            std::cout << "batch_blind_body id=" << id.get_value() << " present=" << bool( member );
            if( member ) {
                const auto &lease = member->get_bandit_live_world_projection_lease();
                std::cout << " pos=" << member->pos_abs().to_string() << " active=" << member->is_active()
                          << " dead=" << member->is_dead() << " moves=" << member->get_moves()
                          << " clock=" << to_turn<int>( member->get_last_updated() )
                          << " sleeping=" << member->in_sleep_state() << " incapable=" << member->duty_incapacitated()
                          << " goal=" << member->goal.to_string() << " path=" << member->path.size()
                          << " omt_path=" << member->omt_path.size()
                          << " attitude=" << static_cast<int>( member->get_attitude() )
                          << " alarm=" << member->faction_alarm.has_value()
                          << " target=" << ( member->current_target() != nullptr )
                          << " danger=" << member->get_ai_danger() << " lease=" << lease.present
                          << ':' << lease.generation << ':' << lease.handoff_epoch;
            }
            if( snapshot != live_site.active_outing.local_handoff.members.end() ) {
                std::cout << " staging=" << snapshot->staging_position.to_string();
            }
            std::cout << '\n';
        }
    };
    report_staging_bodies( "before" );
    std::map<std::string, bandit_live_world_probe::transition_event> motor_first_guards;
    std::uint64_t motor_probe_dropped = 0;
    bool motor_summary_full = false;
    const std::size_t movement_stop_bound = get_map().points_on_zlevel(
            production_route_edge.z() ).size();
    std::size_t movement_steps = 0;
    bool pair_reached_staging = false;
    while( movement_steps < movement_stop_bound && !pair_reached_staging ) {
        {
            // A fresh existing probe keeps the first refusal observable after many native steps.
            // Only the first instance of each actor/guard is retained, with a fixed output bound.
            bandit_live_world_probe::session motor_probe(
                bandit_live_world_probe::collection_mode::transition_events );
            process_monsters_and_npcs_turn_for_test();
            motor_probe_dropped += motor_probe.result().dropped_transition_events;
            for( const auto &event : motor_probe.result().transition_events ) {
                if( event.transition != "scout_ingress_motor" || event.actor_ids.empty() ) {
                    continue;
                }
                const auto actor = character_id( static_cast<int>( event.actor_ids.front() ) );
                if( std::find( generated_ids.begin(), generated_ids.end(), actor ) == generated_ids.end() ) {
                    continue;
                }
                const std::string key = std::to_string( event.actor_ids.front() ) + ':' + event.outcome;
                if( motor_first_guards.count( key ) == 0 ) {
                    if( motor_first_guards.size() < 16 ) {
                        motor_first_guards.emplace( key, event );
                    } else {
                        motor_summary_full = true;
                    }
                }
            }
        }
        pair_reached_staging = std::all_of( generated_ids.begin(), generated_ids.end(),
        [&live_site]( const character_id id ) {
            const shared_ptr_fast<npc> member = overmap_buffer.find_npc( id );
            const auto snapshot = std::find_if(
                                      live_site.active_outing.local_handoff.members.begin(),
                                      live_site.active_outing.local_handoff.members.end(),
            [id]( const bandit_live_world::local_handoff_member_snapshot & candidate ) {
                return candidate.npc_id == id;
            } );
            return member && snapshot != live_site.active_outing.local_handoff.members.end() &&
                   member->pos_abs() == snapshot->staging_position;
        } );
        movement_steps++;
    }
    report_staging_bodies( "after" );
    for( const auto &entry : motor_first_guards ) {
        const auto &event = entry.second;
        std::cout << "batch_blind_motor actor=" << event.actor_ids.front() << " turn=" << event.turn
                  << " guard=" << event.outcome << " " << event.reason << '\n';
    }
    std::cout << "batch_blind_staging steps=" << movement_steps << " bound=" << movement_stop_bound
              << " probe_dropped=" << motor_probe_dropped << " summary_full=" << motor_summary_full << '\n';
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
    std::ostringstream unload_inputs;
    const auto capture_unload_inputs = [&]( const char *stage ) {
        const auto &unloading = live_site.active_outing;
        unload_inputs << stage << " minute=" << to_minutes<int>( calendar::turn -
                      calendar::start_of_cataclysm )
                      << " owner=" << bandit_live_world::to_string( unloading.owner )
                      << " phase=" << bandit_live_world::to_string( unloading.phase )
                      << " epoch=" << unloading.handoff_epoch << " advanced=" << unloading.last_advanced_minutes
                      << " committed=" << unloading.local_handoff.committed_minutes
                      << " cursor=" << static_cast<bool>( bandit_live_world::current_external_simulation_cursor(
                                  live_site ) )
                      << " assembled=" << unloading.local_handoff.cohesion_assembled
                      << " abort=" << unloading.local_handoff.cohesion_abort_return
                      << " deadline=" << unloading.local_handoff.cohesion_deadline_minutes
                      << " reroutes=" << unloading.local_handoff.cohesion_reroutes_used
                      << " alternate=" << unloading.alternate_watch_reposition_pending
                      << " route=" << unloading.local_handoff.route_position.to_string()
                      << " watch=" << unloading.selected_watch_omt.to_string()
                      << " alternate_omt=" << unloading.alternate_watch_omt.to_string() << '\n';
        for( const character_id id : generated_ids ) {
            const auto member = overmap_buffer.find_npc( id );
            unload_inputs << "actor=" << id.get_value() << " found=" << static_cast<bool>( member );
            if( member ) {
                const auto &lease = member->get_bandit_live_world_projection_lease();
                unload_inputs << " active=" << member->is_active() << " dead=" << member->is_dead()
                              << " inbounds=" << get_map().inbounds( member->pos_abs() )
                              << " pos=" << member->pos_abs().to_string()
                              << " body_min=" << to_minutes<int>( member->get_last_updated() - calendar::start_of_cataclysm )
                              << " moves=" << member->get_moves()
                              << " mission=" << static_cast<int>( member->mission )
                              << " goal=" << member->goal.to_string()
                              << " omt_path=" << member->omt_path.size()
                              << " local_path=" << member->path.size()
                              << " lease=" << lease.present << ':' << lease.owner << ':' << lease.generation
                              << ':' << lease.handoff_epoch << ':' << lease.last_advanced_minutes;
            }
            unload_inputs << '\n';
        }
    };
    capture_unload_inputs( "before" );
    const auto interruption_before_unload = live_site.active_outing.assessment.interruption;
    const std::size_t unload_event_start = optical_probe.result().transition_events.size();
    bool dematerialized = dematerialize_live_bandit_structural_handoffs_for_test();
    std::optional<int> reposition_deadline;
    bool serviced_alternate_arrival = false;
    while( !dematerialized && live_site.active_outing.alternate_watch_reposition_pending ) {
        // The ordinary inactive owner must move the real pair to a selected
        // alternate before its transfer adapter can accept their arrival.
        calendar::turn += 5_minutes;
        process_overmap_npc_move_for_test();
        dematerialized = live_site.active_outing.owner == bandit_live_world::simulation_owner::abstract &&
                         live_site.active_outing.handoff_epoch == local_epoch_before + 1;
        if( dematerialized ) {
            break;
        }
        const int deadline = live_site.active_outing.local_handoff.cohesion_deadline_minutes;
        if( !reposition_deadline && deadline >= 0 ) {
            reposition_deadline = deadline;
        }
        // Cohesion runs before generic overmap travel. An actual arrival in
        // this pass therefore gets its next ordinary cohesion opportunity before
        // requiring a published rendezvous bound. A refusal is not retried forever.
        if( !reposition_deadline ) {
            const bool at_alternate = std::all_of( generated_ids.begin(), generated_ids.end(),
            [&live_site]( const character_id id ) {
                const auto member = overmap_buffer.find_npc( id );
                return member && !member->is_dead() &&
                       member->pos_abs_omt() == live_site.active_outing.alternate_watch_omt;
            } );
            if( at_alternate && !serviced_alternate_arrival ) {
                serviced_alternate_arrival = true;
                continue;
            }
            break;
        }
        // Keep the first actual deadline, without extending it on later passes.
        if( to_minutes<int>( calendar::turn - calendar::start_of_cataclysm ) >= *reposition_deadline ) {
            break;
        }
    }
    if( !dematerialized &&
        live_site.active_outing.phase == bandit_live_world::scout_phase::returning_home ) {
        // A real bounded alternate-route abort can leave Local custody owning
        // the first homeward reunion. Give that existing owner its ordinary
        // cadence, bounded by the persisted return allocation, before requiring
        // an Abstract transfer. Do not manufacture a coherent pose or receipt.
        const int homeward_bound = live_site.active_outing.expected_return_minutes + 60;
        CAPTURE( homeward_bound );
        while( !dematerialized && live_site.active_outing.is_active() &&
               live_site.active_outing.owner == bandit_live_world::simulation_owner::local &&
               to_minutes<int>( calendar::turn - calendar::start_of_cataclysm ) <= homeward_bound ) {
            REQUIRE( bandit_live_world::current_external_simulation_cursor( live_site ) );
            calendar::turn += 5_minutes;
            process_overmap_npc_move_for_test();
            dematerialized = live_site.active_outing.owner == bandit_live_world::simulation_owner::abstract &&
                             live_site.active_outing.handoff_epoch == local_epoch_before + 1;
        }
    }
    capture_unload_inputs( "after" );
    std::ostringstream unload_trace;
    const auto &unload_snapshot = optical_probe.result();
    for( std::size_t index = unload_event_start; index < unload_snapshot.transition_events.size();
         ++index ) {
        const auto &event = unload_snapshot.transition_events[index];
        if( event.transition == "scout_homeward_dematerialization" ||
            event.transition == "alternate_watch_cohesion" ) {
            unload_trace << event.site_id << " generation=" << event.generation << " epoch=" <<
                         event.handoff_epoch
                         << " phase=" << event.new_phase << " outcome=" << event.outcome
                         << " reason=" << event.reason << '\n';
        }
    }
    CAPTURE( unload_inputs.str(), unload_trace.str(), unload_snapshot.dropped_transition_events );
    REQUIRE( dematerialized );
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
    if( live_site.active_outing.phase == bandit_live_world::scout_phase::observing ) {
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
    } else {
        // Native placement can leave the pair unable to reach a coherent
        // alternate. The bounded route-refusal policy must then return the same
        // party honestly, rather than inventing assembly or observation time.
        REQUIRE( live_site.active_outing.phase == bandit_live_world::scout_phase::returning_home );
        REQUIRE( live_site.active_outing.local_handoff.cohesion_abort_return );
        REQUIRE( live_site.active_outing.local_handoff.cohesion_reroutes_used > 0 );
        REQUIRE_FALSE( live_site.active_outing.alternate_watch_reposition_pending );
        REQUIRE( live_site.active_outing.assessment.interruption );
        CHECK( live_site.active_outing.assessment.interruption->cause ==
               ( interruption_before_unload ? interruption_before_unload->cause :
                 "pair cohesion could not safely complete its route" ) );
        CHECK_FALSE( live_site.active_outing.assessment.readiness_latched );
        CHECK( live_site.applied_report_generation == applied_report_before );
        CHECK( live_site.applied_return_generation == applied_return_before );
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
    }
    ();
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
    overmap_buffer.find_npc( invalid_id )->setpos( project_to<coords::ms>( invalid_arrival_omt ),
            false );
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
    overmap_buffer.find_npc( invalid_id )->setpos( positions_before_generic_travel.at( invalid_id ),
            false );
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
    const int expected_return = live_site.active_outing.expected_return_minutes;
    REQUIRE( expected_return >= 0 );
    REQUIRE( expected_return < live_site.active_outing.missing_deadline_minutes );
    const int settlement_bound = std::max( current_minutes_before_report, expected_return ) + 60;
    // Physical receipts do not bypass the ordinary persisted return deadline.
    // Continue actual due service until that boundary instead of assuming the
    // very first hourly callback must close every generated route.
    while( live_site.active_outing.is_active() &&
           to_minutes<int>( calendar::turn - calendar::start_of_cataclysm ) <= settlement_bound ) {
        advance_abstract_structural_cadence();
    }
    CAPTURE( expected_return, settlement_bound );
    // Completing the exact physical return and its due service releases the
    // outing. Its durable report remains bound to the dispatched identity.
    const auto &report = live_site.current_scout_report;
    REQUIRE( report.is_present() );
    CHECK_FALSE( live_site.active_outing.is_active() );
    CHECK( report.source_generation == dispatched_generation );
    CHECK( report.source_activity_id == dispatched_activity_id );
    CHECK( report.application_key == dispatched_report_key );
    CHECK( report.revision == report_revision_before + 1 );
    CHECK( report.carrier_ids == generated_ids );
    CHECK_FALSE( report.provisional );
    CHECK( live_site.applied_report_generation == dispatched_generation );
    CHECK( live_site.applied_return_generation == dispatched_generation );

    // This natural terrain-only fixture has no completed sensing assessment or
    // capable named home reserve for a response. A delivered report does not
    // authorize an attack; the decision owner must retain the exact pending report.
    const auto effective_report = bandit_live_world::evaluate_scout_report_at(
                                      report, to_minutes<int>( calendar::turn - calendar::start_of_cataclysm ) );
    CHECK( effective_report.valid );
    CHECK_FALSE( effective_report.assessment_ready );
    CHECK( live_site.camp_decision.state ==
           bandit_live_world::camp_decision_state::report_awaiting_assessment );
    CHECK( live_site.camp_decision.source_report_generation == report.source_generation );
    CHECK( live_site.camp_decision.source_report_revision == report.revision );
    CHECK( live_site.camp_decision.source_report_activity_id == report.source_activity_id );
    CHECK( live_site.camp_decision.source_report_application_key == report.application_key );
    CHECK_FALSE( live_site.active_hostile_operation.is_active() );
    CHECK( std::count_if( live_site.acted_reports.begin(), live_site.acted_reports.end(),
    [&report]( const bandit_live_world::acted_report_summary & acted ) {
        return acted.source_generation == report.source_generation &&
               acted.report_revision == report.revision &&
               acted.acted_minutes == report.delivered_minutes &&
               acted.target_id == report.target_id && acted.target_omt == report.target_omt;
    } ) == 1 );

    const int delivered_revision = report.revision;
    const int delivered_minutes = report.delivered_minutes;
    const std::size_t accepted_report_count = live_site.acted_reports.size();
    process_overmap_npc_move_for_test();
    CHECK( live_site.current_scout_report.revision == delivered_revision );
    CHECK( live_site.current_scout_report.delivered_minutes == delivered_minutes );
    CHECK( live_site.current_scout_report.source_generation == dispatched_generation );
    CHECK( live_site.current_scout_report.application_key == dispatched_report_key );
    CHECK( live_site.applied_return_generation == dispatched_generation );
    CHECK( live_site.acted_reports.size() == accepted_report_count );
    CHECK_FALSE( live_site.active_hostile_operation.is_active() );
}
