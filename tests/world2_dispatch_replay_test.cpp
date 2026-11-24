#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "avatar.h"
#include "bandit_live_world.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "do_turn.h"
#include "game.h"
#include "home_admission_timing.h"
#include "json.h"
#include "map.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "options.h"
#include "options_helpers.h"
#include "overmapbuffer.h"
#include "path_info.h"
#include "worldfactory.h"

namespace
{
constexpr int saved_turn = 5218254;
constexpr int dispatch_turn = 5220000;
constexpr int movement_turn = 5221200;

struct world2_camp {
    std::string site_id;
    std::array<int, 3> residents;
    std::array<tripoint_abs_omt, 3> homes;
    std::array<int, 3> body_turns;
    std::vector<character_id> party;
    tripoint_abs_omt historical_watch;
};

const std::array<world2_camp, 2> camps = {{
        { "overmap_special:bandit_camp@129,149,0", { 10, 12, 13 },
            { tripoint_abs_omt( 129, 149, 0 ), tripoint_abs_omt( 129, 149, 0 ),
                tripoint_abs_omt( 130, 149, 0 ) }, { 5216400, 5217432, 5217432 },
            { character_id( 10 ), character_id( 12 ) }, tripoint_abs_omt( 141, 148, 0 ) },
        { "overmap_special:cannibal_camp@165,149,0", { 11, 14, 15 },
            { tripoint_abs_omt( 165, 149, 0 ), tripoint_abs_omt( 165, 149, 0 ),
                tripoint_abs_omt( 166, 149, 0 ) }, { 5216400, 5217432, 5217432 },
            { character_id( 11 ), character_id( 14 ) }, tripoint_abs_omt( 149, 149, 0 ) }
    }};

const bandit_live_world::site_record &world2_site( const world2_camp &camp )
{
    const auto &sites = overmap_buffer.global_state.bandit_live_world.sites;
    const auto found = std::find_if( sites.begin(), sites.end(), [&]( const auto &site ) {
        return site.site_id == camp.site_id;
    } );
    CAPTURE( camp.site_id );
    REQUIRE( found != sites.end() );
    return *found;
}

struct clock_sample {
    double wall_ms;
    double cpu_ms;
};

template<typename Work>
clock_sample measure( Work &&work )
{
    const auto cpu = std::clock();
    const auto wall = std::chrono::steady_clock::now();
    const bool ok = work();
    const auto wall_ms = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - wall ).count();
    const double cpu_ms = 1000.0 * ( std::clock() - cpu ) / CLOCKS_PER_SEC;
    REQUIRE( ok );
    return { wall_ms, cpu_ms };
}

void write_clock( JsonOut &out, const char *name, const clock_sample &sample )
{
    out.member( name );
    out.start_object();
    out.member( "wall_ms", sample.wall_ms );
    out.member( "process_cpu_ms", sample.cpu_ms );
    out.end_object();
}

void world2_turn()
{
    const int before = to_turn<int>( calendar::turn );
    // Match the native headless component driver: no GUI input or changed
    // avatar position. Only do_turn can advance the calendar and scheduler.
    get_avatar().set_moves( 0 );
    REQUIRE_FALSE( g->do_turn() );
    REQUIRE( to_turn<int>( calendar::turn ) == before + 1 );
}
} // namespace

TEST_CASE( "World 2 saved dispatch replay at turn 5220000", "[.][world2_dispatch_replay]" )
{
    namespace fs = std::filesystem;
    using namespace bandit_live_world;
    const char *root = std::getenv( "CAOL_WORLD2_DISPATCH_USERDIR" );
    REQUIRE( root );
    REQUIRE( fs::equivalent( PATH_INFO::user_dir(), root ) );
    REQUIRE( fs::is_regular_file( fs::path( root ) / ".world2-dispatch-disposable" ) );
    REQUIRE( fs::equivalent( PATH_INFO::savedir(), fs::path( root ) / "save" ) );
    REQUIRE( std::getenv( "CAOL_HOME_ADMISSION_TRACE" ) == nullptr );
    REQUIRE( std::getenv( "CAOL_HOME_DETAIL_TRACE" ) == nullptr );
    REQUIRE( std::getenv( "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH" ) == nullptr );
    const char *mode_arg = std::getenv( "CAOL_WORLD2_DISPATCH_MODE" );
    const char *arm_arg = std::getenv( "CAOL_WORLD2_DISPATCH_ARM" );
    REQUIRE( mode_arg );
    REQUIRE( arm_arg );
    const std::string mode( mode_arg );
    const std::string arm( arm_arg );
    REQUIRE( ( mode == "qualify" || mode == "turn" || mode == "body" ) );
    REQUIRE( ( arm == "legacy" || arm == "map_free" ) );
    const bool probe = arm == "map_free";

    REQUIRE( world_generator->active_world == nullptr );
    REQUIRE( world_generator->has_world( "TestSetup00" ) );
    const auto loading = measure( []() { return g->load( "TestSetup00" ); } );
    REQUIRE( to_turn<int>( calendar::turn ) == saved_turn );
    REQUIRE( world_generator->active_world->world_name == "TestSetup00" );
    const std::vector<mod_id> mods = { mod_id( "dda" ), mod_id( "no_npc_food" ),
                                      mod_id( "personal_portal_storms" ) };
    REQUIRE( world_generator->active_world->active_mod_order == mods );
    REQUIRE( get_option<bool>( "NO_NPC_FOOD" ) );
    override_option autosave( "AUTOSAVE", "false" );
    override_option diagnostic( "RAID_PERFORMANCE_DIAGNOSTIC", "off" );
    REQUIRE_FALSE( get_option<bool>( "AUTOSAVE" ) );
    REQUIRE( get_option<std::string>( "RAID_PERFORMANCE_DIAGNOSTIC" ) == "off" );
    const auto origin = get_avatar().pos_abs();

    get_avatar().set_moves( 0 );
    const auto initial_turn = measure( []() { return !g->do_turn(); } );
    const int initial_after = to_turn<int>( calendar::turn );
    REQUIRE( initial_after == saved_turn );
    get_avatar().set_moves( 0 );
    const auto first_advancing = measure( []() { return !g->do_turn(); } );
    REQUIRE( to_turn<int>( calendar::turn ) == saved_turn + 1 );
    while( to_turn<int>( calendar::turn ) < dispatch_turn - 1 ) {
        world2_turn();
    }
    REQUIRE( get_avatar().pos_abs() == origin );

    std::ostringstream record;
    JsonOut out( record );
    out.start_object();
    out.member( "arm", arm );
    out.member( "mode", mode );
    out.member( "loaded_turn", saved_turn );
    out.member( "initial_after", initial_after );
    out.member( "avatar", origin );
    out.member( "activity", get_avatar().activity.id().str() );
    std::vector<std::string> local_actors;
    for( const npc &body : g->all_npcs() ) {
        local_actors.push_back( std::to_string( body.getID().get_value() ) + ":" +
                                body.pos_abs().to_string() );
    }
    for( const monster &body : g->all_monsters() ) {
        local_actors.push_back( body.type->id.str() + ":" + body.pos_abs().to_string() );
    }
    std::sort( local_actors.begin(), local_actors.end() );
    out.member( "local_actors", local_actors );
    out.member( "home_bodies" );
    out.start_array();
    for( const world2_camp &camp : camps ) {
        const auto &site = world2_site( camp );
        REQUIRE_FALSE( site.active_outing.is_active() );
        REQUIRE( site.home_activation.active );
        for( std::size_t i = 0; i < camp.residents.size(); ++i ) {
            const character_id id( camp.residents[i] );
            CAPTURE( camp.site_id, id );
            const auto *member = site.find_member( id );
            REQUIRE( member );
            REQUIRE( member->state == member_state::at_home );
            const auto body = overmap_buffer.find_npc( id );
            REQUIRE( body );
            REQUIRE( body->pos_abs_omt() == camp.homes[i] );
            REQUIRE_FALSE( body->is_active() );
            REQUIRE_FALSE( get_map().inbounds( body->pos_abs() ) );
            REQUIRE( to_turn<int>( body->get_last_updated() ) == camp.body_turns[i] );
            REQUIRE_FALSE( body->pending_home_death );
            const auto mark = site.home_activation.bodies.find( id );
            REQUIRE( mark != site.home_activation.bodies.end() );
            REQUIRE( mark->second.world_turn == to_turns<int>( body->get_last_updated() -
                     calendar::start_of_cataclysm ) );
            out.start_object();
            out.member( "id", id.get_value() );
            out.member( "position", body->pos_abs() );
            out.member( "turn", camp.body_turns[i] );
            out.member( "suspended", mark->second.suspended_turns );
            out.member( "health", body->hp_percentage() );
            out.member( "effects", body->get_effects().size() );
            out.end_object();
        }
    }
    out.end_array();

    detailed_map_entry_counts entries;
    home_admission_body_timing body_timing;
    clock_sample dispatch{};
    const auto due = [&]() { return !g->do_turn(); };
    {
        set_ordinary_home_probe_for_test( true );
        set_ordinary_home_probe_for_test( probe );
        set_home_admission_body_timing_for_test( mode == "body" ? &body_timing : nullptr );
        on_out_of_scope reset_probe( []() {
            set_ordinary_home_probe_for_test( false );
            set_home_admission_body_timing_for_test( nullptr );
        } );
        get_avatar().set_moves( 0 );
        if( mode == "qualify" ) {
            scoped_detailed_map_entry_counter count( entries );
            REQUIRE( due() );
        } else if( mode == "turn" ) {
            dispatch = measure( due );
        } else {
            REQUIRE( due() );
        }
    }
    REQUIRE( to_turn<int>( calendar::turn ) == dispatch_turn );
    REQUIRE( ordinary_home_probe_successes_for_test() == ( probe ? 6 : 0 ) );
    if( mode == "qualify" ) {
        CAPTURE( entries.regions_entered, entries.region_loads, entries.region_saves );
        REQUIRE( entries.regions_entered == 2 );
        REQUIRE( entries.region_loads == ( probe ? 0 : 4 ) );
        REQUIRE( entries.region_saves == ( probe ? 0 : 4 ) );
    } else if( mode == "body" ) {
        REQUIRE( body_timing.calls == 6 );
    }
    std::map<character_id, tripoint_abs_ms> party_start;
    out.member( "dispatch" );
    out.start_array();
    for( const world2_camp &camp : camps ) {
        const auto &site = world2_site( camp );
        const auto &outing = site.active_outing;
        CAPTURE( camp.site_id, outing.target_lead_id, outing.target_lead_revision,
                 outing.started_minutes, outing.selected_watch_omt );
        REQUIRE( outing.kind == outing_kind::structural_sortie );
        REQUIRE( outing.started_minutes == 7800 );
        REQUIRE( outing.member_ids == camp.party );
        REQUIRE( outing.owner == simulation_owner::abstract );
        REQUIRE( outing.phase == scout_phase::outbound );
        REQUIRE( outing.target_lead_id == camp.site_id +
                 "#lead:smoke_signal:camp-smoke@(145,149,1)@145,149,1" );
        for( int number : camp.residents ) {
            const auto body = overmap_buffer.find_npc( character_id( number ) );
            REQUIRE( body );
            REQUIRE( to_turn<int>( body->get_last_updated() ) == dispatch_turn );
            REQUIRE_FALSE( body->pending_home_death );
        }
        for( character_id id : camp.party ) {
            party_start.emplace( id, overmap_buffer.find_npc( id )->pos_abs() );
        }
        out.start_object();
        out.member( "site", camp.site_id );
        out.member( "lead", outing.target_lead_id );
        out.member( "revision", outing.target_lead_revision );
        out.member( "watch", outing.selected_watch_omt );
        out.member( "historical_watch", camp.historical_watch );
        out.member( "historical_revision", 9 );
        out.member( "route", outing.shared_route );
        out.member( "party", outing.member_ids );
        out.end_object();
    }
    out.end_array();
    std::map<character_id, tripoint_abs_omt> previous_omt;
    for( const auto &[id, start] : party_start ) {
        previous_omt.emplace( id, project_to<coords::omt>( start ) );
    }
    out.member( "movement_profile" );
    out.start_array();
    while( to_turn<int>( calendar::turn ) < movement_turn ) {
        world2_turn();
        for( auto &[id, previous] : previous_omt ) {
            const auto body = overmap_buffer.find_npc( id );
            REQUIRE( body );
            const auto current = body->pos_abs_omt();
            if( current == previous ) {
                continue;
            }
            REQUIRE_FALSE( body->is_active() );
            out.start_object();
            out.member( "turn", to_turn<int>( calendar::turn ) );
            out.member( "id", id.get_value() );
            out.member( "from", previous );
            out.member( "to", current );
            out.member( "goal", body->goal );
            out.member( "remaining_route", body->omt_path );
            out.end_object();
            previous = current;
        }
    }
    out.end_array();
    REQUIRE( get_avatar().pos_abs() == origin );
    out.member( "movement" );
    out.start_array();
    for( const world2_camp &camp : camps ) {
        REQUIRE( world2_site( camp ).active_outing.member_ids == camp.party );
        for( character_id id : camp.party ) {
            const auto body = overmap_buffer.find_npc( id );
            REQUIRE( body );
            REQUIRE( body->pos_abs_omt() != project_to<coords::omt>( party_start.at( id ) ) );
            REQUIRE_FALSE( body->is_active() );
            out.start_object();
            out.member( "id", id.get_value() );
            out.member( "before", party_start.at( id ) );
            out.member( "after", body->pos_abs() );
            out.end_object();
        }
    }
    out.end_array();
    if( mode == "qualify" ) {
        out.member( "admission_regions", entries.regions_entered );
        out.member( "admission_loads", entries.region_loads );
        out.member( "admission_saves", entries.region_saves );
    } else if( mode == "turn" ) {
        write_clock( out, "load", loading );
        write_clock( out, "initial_do_turn", initial_turn );
        write_clock( out, "first_advancing_turn", first_advancing );
        write_clock( out, "dispatch_time", dispatch );
    } else {
        out.member( "body_calls", body_timing.calls );
        out.member( "body_wall_ms", std::chrono::duration<double, std::milli>( body_timing.elapsed ).count() );
    }
    out.end_object();
    std::cout << "WORLD2_DISPATCH_REPLAY " << record.str() << '\n';
}
