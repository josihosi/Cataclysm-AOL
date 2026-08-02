#include "bandit_live_world.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "cata_catch.h"
#include "json.h"
#include "json_loader.h"

namespace
{
std::optional<std::string> special_lookup( const tripoint_abs_omt &omt )
{
    if( omt.z() != 0 ) {
        return std::nullopt;
    }

    if( omt.x() >= 10 && omt.x() <= 11 && omt.y() >= 20 && omt.y() <= 21 ) {
        return std::string( "bandit_camp" );
    }

    if( omt.x() >= 40 && omt.x() <= 42 && omt.y() >= 50 && omt.y() <= 52 ) {
        return std::string( "bandit_work_camp" );
    }

    if( omt.x() >= 70 && omt.x() <= 71 && omt.y() >= 80 && omt.y() <= 81 ) {
        return std::string( "cannibal_camp" );
    }

    return std::nullopt;
}

std::optional<std::string> multi_z_special_lookup( const tripoint_abs_omt &omt )
{
    const bool tracked_z = omt.z() == 0 || omt.z() == 1 || omt.z() == 5;
    if( tracked_z && omt.x() >= 10 && omt.x() <= 11 && omt.y() >= 20 && omt.y() <= 21 ) {
        return std::string( "bandit_camp" );
    }

    return std::nullopt;
}
void add_bandit_camp_member( bandit_live_world::world_state &world, int index, int id_base = 11000 )
{
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( id_base + index ),
             tripoint_abs_ms( 240 + index, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
}

void add_bandit_work_camp_member( bandit_live_world::world_state &world, int index,
                                  int id_base = 12000 )
{
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( id_base + index ),
             tripoint_abs_ms( 984 + index, 1224, 0 ), std::string( "bandit_work_camp" ),
             std::nullopt, special_lookup ) );
}

void add_cannibal_camp_member( bandit_live_world::world_state &world, int index,
                               int id_base = 12500 )
{
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter",
             character_id( id_base + index ), tripoint_abs_ms( 1680 + index, 1920, 0 ),
             std::string( "cannibal_camp" ), std::nullopt, special_lookup ) );
}

std::optional<std::string> lookup_test_terrain(
    const std::vector<std::pair<tripoint_abs_omt, std::string>> &terrain,
    const tripoint_abs_omt &omt )
{
    for( const std::pair<tripoint_abs_omt, std::string> &entry : terrain ) {
        if( entry.first == omt ) {
            return entry.second;
        }
    }
    return std::nullopt;
}

bandit_live_world::world_state round_trip_world( const bandit_live_world::world_state &world )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );
    return loaded;
}

std::string serialize_world( const bandit_live_world::world_state &world )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );
    return out.str();
}

void set_test_active_outing( bandit_live_world::site_record &site, const std::string &activity_id,
                             bandit_live_world::outing_kind kind =
                                 bandit_live_world::outing_kind::scout_sortie )
{
    site.active_outing.kind = kind;
    site.active_outing.activity_id = activity_id;
    site.active_outing.camp_id = site.site_id;
    site.active_outing.generation = site.next_outing_generation++;
    if( kind == bandit_live_world::outing_kind::scout_sortie ) {
        site.active_outing.job_type = "scout";
    }
    site.active_outing.return_application_key = activity_id + ":return:" +
                                                   std::to_string( site.active_outing.generation );
    site.active_outing.report_application_key = activity_id + ":report:" +
                                                std::to_string( site.active_outing.generation );
    site.active_outing.cargo_application_key = activity_id + ":cargo:" +
                                               std::to_string( site.active_outing.generation );
}

bandit_live_world::simulation_advance_cursor require_current_simulation_cursor(
    const bandit_live_world::site_record &site )
{
    const std::optional<bandit_live_world::simulation_advance_cursor> cursor =
        bandit_live_world::current_external_simulation_cursor( site );
    REQUIRE( cursor.has_value() );
    return *cursor;
}

void prepare_hostile_follow_on( bandit_live_world::site_record &site, const int report_revision,
                                const int scout_generation, const std::string &target_id,
                                const tripoint_abs_omt &target_omt, const int delivered_minutes )
{
    site.current_scout_report.revision = report_revision;
    site.current_scout_report.source_activity_id = site.site_id + "#scout:" +
            std::to_string( scout_generation );
    site.current_scout_report.source_generation = scout_generation;
    site.current_scout_report.source_job_type = "scout";
    site.current_scout_report.target_id = target_id;
    site.current_scout_report.target_omt = target_omt;
    site.current_scout_report.target_lead_revision = 3;
    site.current_scout_report.application_key =
        site.current_scout_report.source_activity_id + ":report:" +
        std::to_string( scout_generation );
    site.current_scout_report.delivered_minutes = delivered_minutes;
    site.current_scout_report.provisional = false;
    site.next_outing_generation = std::max( site.next_outing_generation, scout_generation + 1 );
    REQUIRE( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
             bandit_live_world::camp_decision_transition_result::applied );
    REQUIRE( bandit_live_world::transition_camp_decision_state(
                 site, bandit_live_world::camp_decision_state::report_awaiting_assessment,
                 bandit_live_world::camp_decision_state::preparing_follow_on,
                 report_revision, scout_generation, delivered_minutes + 1, -1,
                 "test follow-on approved" ) ==
             bandit_live_world::camp_decision_transition_result::applied );
}

void apply_test_hostile_dispatch( bandit_live_world::site_record &site,
                                  const bandit_live_world::dispatch_plan &dispatch,
                                  const int current_minutes )
{
    const bandit_live_world::hostile_operation_kind operation_kind =
        site.profile == bandit_live_world::hostile_site_profile::cannibal_camp ?
        bandit_live_world::hostile_operation_kind::raid :
        bandit_live_world::hostile_operation_kind::shakedown;
    const bandit_live_world::hostile_operation_plan operation =
        bandit_live_world::plan_hostile_operation(
            site, operation_kind, dispatch.member_ids,
            { site.anchor, dispatch.target_omt }, site.anchor, current_minutes );
    REQUIRE( operation.valid );
    REQUIRE( bandit_live_world::apply_hostile_operation_plan( site, operation ) );
    REQUIRE( bandit_live_world::transition_hostile_operation_phase(
                 site, require_current_simulation_cursor( site ),
                 bandit_live_world::hostile_operation_phase::assembling,
                 bandit_live_world::hostile_operation_phase::outbound,
                 current_minutes + 1, "test hostile party departed" ) ==
             bandit_live_world::hostile_operation_transition_result::applied );
}

bandit_live_world::hostile_operation_transition_result transition_test_hostile_operation(
    bandit_live_world::site_record &site,
    const bandit_live_world::hostile_operation_phase expected_phase,
    const bandit_live_world::hostile_operation_phase next_phase,
    const int current_minutes, const std::string &reason )
{
    return bandit_live_world::transition_hostile_operation_phase(
               site, require_current_simulation_cursor( site ), expected_phase, next_phase,
               current_minutes, reason );
}
} // namespace

TEST_CASE( "bandit_live_world_claims_one_bounded_special_backed_site_ledger", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 101 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 102 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 103 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    REQUIRE( world.owner_id == "hells_raiders_live_owner_v0" );
    REQUIRE( world.sites.size() == 1 );
    const bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.site_id == "overmap_special:bandit_camp@10,20,0" );
    CHECK( site.source_kind == bandit_live_world::anchor_source_kind::overmap_special );
    CHECK( site.site_kind == bandit_live_world::owned_site_kind::bandit_camp );
    CHECK( site.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( site.anchor == tripoint_abs_omt( 10, 20, 0 ) );
    CHECK( site.headcount == 3 );
    REQUIRE( site.footprint.size() == 4 );
    CHECK( site.footprint.front() == tripoint_abs_omt( 10, 20, 0 ) );
    CHECK( site.footprint.back() == tripoint_abs_omt( 11, 21, 0 ) );
    REQUIRE( site.members.size() == 3 );
    CHECK( site.members.front().npc_template_id == "bandit" );
    REQUIRE( site.spawn_tiles.size() == 3 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 240, 480, 0 ) )->headcount == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 241, 480, 0 ) )->headcount == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 264, 504, 0 ) )->headcount == 1 );
}

TEST_CASE( "bandit_live_world_collapses_multi_z_special_camp_into_one_site",
           "[bandit][live_world][multi_z]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 151 ),
             tripoint_abs_ms( 240, 480, 1 ), std::string( "bandit_camp" ), std::nullopt,
             multi_z_special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 152 ),
             tripoint_abs_ms( 241, 480, 5 ), std::string( "bandit_camp" ), std::nullopt,
             multi_z_special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 153 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             multi_z_special_lookup ) );

    REQUIRE( world.sites.size() == 1 );
    const bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.site_id == "overmap_special:bandit_camp@10,20,0" );
    CHECK( site.anchor == tripoint_abs_omt( 10, 20, 0 ) );
    CHECK( site.headcount == 3 );
    REQUIRE( site.footprint.size() == 12 );
    CHECK( site.footprint.front() == tripoint_abs_omt( 10, 20, 0 ) );
    CHECK( site.footprint.back() == tripoint_abs_omt( 11, 21, 5 ) );
    CHECK( std::any_of( site.footprint.begin(), site.footprint.end(), []( const tripoint_abs_omt &omt ) {
        return omt == tripoint_abs_omt( 10, 20, 1 );
    } ) );
    CHECK( std::any_of( site.footprint.begin(), site.footprint.end(), []( const tripoint_abs_omt &omt ) {
        return omt == tripoint_abs_omt( 10, 20, 5 );
    } ) );
    REQUIRE( site.members.size() == 3 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 240, 480, 1 ) )->headcount == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 241, 480, 5 ) )->headcount == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 264, 504, 0 ) )->headcount == 1 );
}

TEST_CASE( "bandit_live_world_keeps_map_extra_hostile_spawns_as_micro_sites", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 201 ),
             tripoint_abs_ms( 120, 96, 0 ), std::nullopt, std::string( "mx_looters" ),
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 202 ),
             tripoint_abs_ms( 168, 120, 0 ), std::nullopt, std::string( "mx_bandits_block" ),
             special_lookup ) );

    REQUIRE( world.sites.size() == 2 );
    const bandit_live_world::site_record *looters = world.find_site( "map_extra:mx_looters@5,4,0" );
    REQUIRE( looters != nullptr );
    CHECK( looters->site_kind == bandit_live_world::owned_site_kind::looters );
    CHECK( looters->profile == bandit_live_world::hostile_site_profile::small_hostile_site );
    CHECK( looters->headcount == 1 );
    REQUIRE( looters->footprint.size() == 1 );
    CHECK( looters->footprint.front() == tripoint_abs_omt( 5, 4, 0 ) );

    const bandit_live_world::site_record *roadblock = world.find_site( "map_extra:mx_bandits_block@7,5,0" );
    REQUIRE( roadblock != nullptr );
    CHECK( roadblock->site_kind == bandit_live_world::owned_site_kind::bandits_block );
    CHECK( roadblock->profile == bandit_live_world::hostile_site_profile::small_hostile_site );
    CHECK( roadblock->headcount == 1 );
    REQUIRE( roadblock->footprint.size() == 1 );
    CHECK( roadblock->footprint.front() == tripoint_abs_omt( 7, 5, 0 ) );
}

TEST_CASE( "bandit_live_world_registers_abstract_special_before_npc_materialization",
           "[bandit][live_world][abstract_bootstrap]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::register_abstract_site( world,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_work_camp",
             tripoint_abs_omt( 41, 51, 0 ), special_lookup,
             bandit_live_world::abstract_roster_seed_for_site_kind(
                 bandit_live_world::owned_site_kind::bandit_work_camp ) ) );

    REQUIRE( world.sites.size() == 1 );
    const bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.site_id == "overmap_special:bandit_work_camp@40,50,0" );
    CHECK( site.source_kind == bandit_live_world::anchor_source_kind::overmap_special );
    CHECK( site.site_kind == bandit_live_world::owned_site_kind::bandit_work_camp );
    CHECK( site.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( site.anchor == tripoint_abs_omt( 40, 50, 0 ) );
    CHECK( site.headcount == 6 );
    CHECK( site.members.empty() );
    REQUIRE( site.footprint.size() == 9 );
    CHECK( site.footprint.front() == tripoint_abs_omt( 40, 50, 0 ) );
    CHECK( site.footprint.back() == tripoint_abs_omt( 42, 52, 0 ) );

    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );

    REQUIRE( loaded.sites.size() == 1 );
    const bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.site_id == site.site_id );
    CHECK( loaded_site.headcount == 6 );
    CHECK( loaded_site.members.empty() );
    REQUIRE( loaded_site.footprint.size() == 9 );
}

TEST_CASE( "bandit_live_world_reconciles_materialized_spawns_with_abstract_specials",
           "[bandit][live_world][abstract_bootstrap]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::register_abstract_site( world,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_camp",
             tripoint_abs_omt( 11, 21, 0 ), special_lookup,
             bandit_live_world::abstract_roster_seed_for_site_kind(
                 bandit_live_world::owned_site_kind::bandit_camp ) ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 203 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 204 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    REQUIRE( world.sites.size() == 1 );
    const bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.site_id == "overmap_special:bandit_camp@10,20,0" );
    CHECK( site.headcount == 6 );
    REQUIRE( site.members.size() == 2 );
    CHECK( site.members.front().npc_id == character_id( 203 ) );
    CHECK( site.members.back().npc_id == character_id( 204 ) );
    REQUIRE( site.spawn_tiles.size() == 2 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 240, 480, 0 ) )->headcount == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 264, 504, 0 ) )->headcount == 1 );
}

TEST_CASE( "bandit_live_world_records_bounded_live_signal_marks_on_owned_sites",
           "[bandit][live_world][live_signal]" )
{
    bandit_live_world::world_state world;

    REQUIRE( bandit_live_world::register_abstract_site( world,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_camp",
             tripoint_abs_omt( 11, 21, 0 ), special_lookup,
             bandit_live_world::abstract_roster_seed_for_site_kind(
                 bandit_live_world::owned_site_kind::bandit_camp ) ) );
    REQUIRE( world.sites.size() == 1 );
    bandit_live_world::site_record &site = world.sites.front();

    bandit_live_world::live_signal_mark smoke_mark;
    smoke_mark.mark_id = "live_smoke@18,20,0";
    smoke_mark.kind = "smoke";
    smoke_mark.source_omt = tripoint_abs_omt( 18, 20, 0 );
    smoke_mark.observed_range_omt = 0;
    smoke_mark.range_cap_omt = 15;
    smoke_mark.strength = 1;
    smoke_mark.confidence = 1;
    smoke_mark.bounty_add = 1;
    smoke_mark.threat_add = 0;
    smoke_mark.notes.push_back( "live source hook: fd_fire=2, fd_smoke=1" );

    REQUIRE( bandit_live_world::record_live_signal_mark( site, smoke_mark ) );
    CHECK( site.remembered_target_or_mark == "live_smoke@18,20,0" );
    CHECK( site.remembered_bounty_estimate == 1 );
    CHECK( site.remembered_threat_estimate == 0 );
    REQUIRE( site.known_recent_marks.size() == 1 );
    CHECK( site.known_recent_marks.front() == "live_smoke@18,20,0" );
    REQUIRE( site.intelligence_map.leads.size() == 1 );
    CHECK( site.intelligence_map.leads.front().kind == bandit_live_world::camp_lead_kind::smoke_signal );
    CHECK( site.intelligence_map.leads.front().source_summary.find( "obscured/uncertain" ) !=
           std::string::npos );

    CHECK_FALSE( bandit_live_world::record_live_signal_mark( site, smoke_mark ) );
    CHECK( site.known_recent_marks.size() == 1 );

    for( int i = 0; i < 9; ++i ) {
        bandit_live_world::live_signal_mark extra_mark = smoke_mark;
        extra_mark.mark_id = "live_smoke@" + std::to_string( 30 + i ) + ",20,0";
        CHECK( bandit_live_world::record_live_signal_mark( site, extra_mark ) );
    }
    CHECK( site.known_recent_marks.size() == 8 );
    CHECK( site.known_recent_marks.front() == "live_smoke@31,20,0" );
    CHECK( site.known_recent_marks.back() == "live_smoke@38,20,0" );
}

TEST_CASE( "bandit_live_world_survives_a_save_style_round_trip", "[bandit][live_world]" )
{
    bandit_live_world::world_state original;
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit_quartermaster", character_id( 301 ),
             tripoint_abs_ms( 960, 1200, 0 ), std::string( "bandit_work_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit", character_id( 302 ),
             tripoint_abs_ms( 984, 1224, 0 ), std::string( "bandit_work_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &original_site = original.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( original_site, tripoint_abs_omt( 48, 50, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    CHECK( plan.group.handoff_epoch == 0 );
    CHECK( plan.entry.handoff_epoch == 0 );
    const std::string before_epoch_rejection = serialize_world( original );
    bandit_live_world::dispatch_plan mismatched_epoch = plan;
    mismatched_epoch.entry.handoff_epoch = 1;
    CHECK_FALSE( bandit_live_world::apply_dispatch_plan( original_site, mismatched_epoch ) );
    CHECK( serialize_world( original ) == before_epoch_rejection );
    bandit_live_world::dispatch_plan noninitial_epoch = plan;
    noninitial_epoch.group.handoff_epoch = 2;
    noninitial_epoch.entry.handoff_epoch = 2;
    CHECK_FALSE( bandit_live_world::apply_dispatch_plan( original_site, noninitial_epoch ) );
    CHECK( serialize_world( original ) == before_epoch_rejection );
    REQUIRE( bandit_live_world::apply_dispatch_plan( original_site, plan ) );

    std::ostringstream out;
    JsonOut jsout( out, true );
    original.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );

    CHECK( loaded.owner_id == original.owner_id );
    REQUIRE( loaded.sites.size() == 1 );
    const bandit_live_world::site_record &site = loaded.sites.front();
    CHECK( site.site_id == "overmap_special:bandit_work_camp@40,50,0" );
    CHECK( site.site_kind == bandit_live_world::owned_site_kind::bandit_work_camp );
    CHECK( site.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( site.headcount == 2 );
    REQUIRE( site.footprint.size() == 9 );
    REQUIRE( site.members.size() == 2 );
    CHECK( site.members.front().npc_id == character_id( 301 ) );
    CHECK( site.members.front().npc_template_id == "bandit_quartermaster" );
    CHECK( site.members.back().npc_id == character_id( 302 ) );
    REQUIRE( site.spawn_tiles.size() == 2 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 960, 1200, 0 ) )->headcount == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 984, 1224, 0 ) )->headcount == 1 );
    CHECK( site.active_outing.activity_id == "overmap_special:bandit_work_camp@40,50,0#dispatch" );
    CHECK( site.active_outing.kind == bandit_live_world::outing_kind::scout_sortie );
    CHECK( site.active_outing.generation == 1 );
    CHECK( site.next_outing_generation == 2 );
    CHECK( site.active_outing.return_application_key ==
           "overmap_special:bandit_work_camp@40,50,0#dispatch:return:1" );
    CHECK( site.active_outing.target_id == "player_basecamp_nearby" );
    REQUIRE( site.active_outing.member_ids.size() == 1 );
    CHECK( site.active_outing.member_ids.front() == character_id( 301 ) );
}

TEST_CASE( "bandit_live_world_deserialize_commits_only_after_the_packet_is_valid",
           "[bandit][live_world][save]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 303 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    const std::string before = serialize_world( world );

    JsonValue malformed = json_loader::from_string(
                              R"({"owner_id":"replacement","sites":"not-an-array"})" );
    CHECK_THROWS( world.deserialize( malformed.get_object() ) );
    CHECK( serialize_world( world ) == before );
}

TEST_CASE( "bandit_live_world_migrates_and_normalizes_legacy_active_outing_identity",
           "[bandit][live_world][migration]" )
{
    SECTION( "valid legacy active group gains stable identity and generation" ) {
        JsonValue legacy = json_loader::from_string( R"({
            "site_id": "legacy_camp",
            "members": [ { "npc_id": 42, "state": "outbound" } ],
            "active_group_id": "legacy_camp#dispatch",
            "active_target_id": "legacy_target",
            "active_job_type": "scout",
            "active_member_ids": [ 42 ],
            "active_sortie_started_minutes": 120
        })" );
        bandit_live_world::site_record site;
        site.deserialize( legacy.get_object() );

        CHECK( site.schema_version == 5 );
        CHECK( site.active_outing.is_active() );
        CHECK( site.active_outing.kind == bandit_live_world::outing_kind::scout_sortie );
        CHECK( site.active_outing.activity_id == "legacy_camp#dispatch" );
        CHECK( site.active_outing.camp_id == "legacy_camp" );
        CHECK( site.active_outing.generation == 1 );
        CHECK( site.active_outing.last_advanced_minutes == 120 );
        CHECK( site.active_outing.return_application_key == "legacy_camp#dispatch:return:1" );
        CHECK( site.next_outing_generation == 2 );

        bandit_live_world::world_state world;
        world.sites.push_back( site );
        const bandit_live_world::world_state loaded = round_trip_world( world );
        REQUIRE( loaded.sites.size() == 1 );
        CHECK( loaded.sites.front().active_outing.activity_id == "legacy_camp#dispatch" );
        CHECK( loaded.sites.front().active_outing.generation == 1 );
        const std::string modern_json = serialize_world( loaded );
        CHECK( modern_json.find( "active_group_id" ) == std::string::npos );
        CHECK( modern_json.find( "active_target_id" ) == std::string::npos );
        CHECK( modern_json.find( "active_target_omt" ) == std::string::npos );
        CHECK( modern_json.find( "active_job_type" ) == std::string::npos );
        CHECK( modern_json.find( "active_member_ids" ) == std::string::npos );
        CHECK( modern_json.find( "active_sortie_started_minutes" ) == std::string::npos );
        CHECK( modern_json.find( "active_sortie_local_contact_minutes" ) == std::string::npos );
    }

    SECTION( "duplicate legacy reservations close safely instead of wedging the camp" ) {
        JsonValue malformed = json_loader::from_string( R"({
            "site_id": "legacy_camp",
            "members": [ { "npc_id": 43, "state": "outbound" } ],
            "active_group_id": "legacy_camp#dispatch",
            "active_target_id": "legacy_target",
            "active_job_type": "scout",
            "active_member_ids": [ 43, 43 ]
        })" );
        bandit_live_world::site_record site;
        site.deserialize( malformed.get_object() );

        CHECK_FALSE( site.active_outing.is_active() );
        CHECK( site.active_outing.member_ids.empty() );
        CHECK( site.active_outing.target_id.empty() );
        REQUIRE( site.find_member( character_id( 43 ) ) != nullptr );
        CHECK( site.find_member( character_id( 43 ) )->state ==
               bandit_live_world::member_state::at_home );
        CHECK( site.next_outing_generation == 2 );
    }

    SECTION( "legacy scalar local contact migrates to local ownership" ) {
        JsonValue legacy = json_loader::from_string( R"({
            "site_id": "legacy_contact_camp",
            "members": [ { "npc_id": 47, "state": "local_contact" } ],
            "active_group_id": "legacy_contact_camp#dispatch",
            "active_target_id": "legacy_contact_target",
            "active_job_type": "scout",
            "active_member_ids": [ 47 ],
            "active_sortie_started_minutes": 120,
            "active_sortie_local_contact_minutes": 150
        })" );
        bandit_live_world::site_record site;
        site.deserialize( legacy.get_object() );

        REQUIRE( site.active_outing.is_active() );
        CHECK( site.active_outing.owner == bandit_live_world::simulation_owner::local );
        CHECK( site.active_outing.handoff_epoch == 1 );
        CHECK( site.active_outing.last_advanced_minutes == 150 );
        CHECK( bandit_live_world::current_external_simulation_cursor( site ).has_value() );
    }

    SECTION( "transitional nested identity imports its former site payload" ) {
        JsonValue transitional = json_loader::from_string( R"({
            "schema_version": 2,
            "site_id": "transitional_camp",
            "members": [ { "npc_id": 44, "state": "local_contact" } ],
            "active_outing": {
                "schema_version": 1,
                "kind": "scout_sortie",
                "activity_id": "transitional_camp#dispatch",
                "camp_id": "transitional_camp",
                "generation": 4,
                "simulation_owner": "local",
                "handoff_epoch": 1,
                "return_application_key": "transitional_camp#dispatch:return:4"
            },
            "active_target_id": "legacy_nested_target",
            "active_target_omt": [ 9, 10, 0 ],
            "active_job_type": "scout",
            "active_member_ids": [ 44 ],
            "active_sortie_started_minutes": 200,
            "active_sortie_local_contact_minutes": 240
        })" );
        bandit_live_world::site_record site;
        site.deserialize( transitional.get_object() );

        REQUIRE( site.active_outing.is_active() );
        CHECK( site.schema_version == 5 );
        CHECK( site.active_outing.schema_version == 4 );
        CHECK( site.active_outing.member_ids == std::vector<character_id> { character_id( 44 ) } );
        CHECK( site.active_outing.leader_id == character_id( 44 ) );
        CHECK( site.active_outing.target_id == "legacy_nested_target" );
        CHECK( site.active_outing.target_omt == tripoint_abs_omt( 9, 10, 0 ) );
        CHECK( site.active_outing.job_type == "scout" );
        CHECK( site.active_outing.phase == bandit_live_world::scout_phase::observing );
        CHECK( site.active_outing.started_minutes == 200 );
        CHECK( site.active_outing.local_contact_minutes == 240 );
        CHECK( site.active_outing.last_progress_minutes == 240 );
        CHECK( site.active_outing.last_advanced_minutes == 240 );
        CHECK( site.active_outing.owner == bandit_live_world::simulation_owner::local );
        CHECK( site.active_outing.handoff_epoch == 1 );
        CHECK( site.active_outing.expected_return_minutes == 1080 );
        CHECK( site.active_outing.missing_deadline_minutes == 2520 );
        CHECK( site.active_outing.report_application_key ==
               "transitional_camp#dispatch:report:4" );
        CHECK( site.active_outing.cargo_application_key ==
               "transitional_camp#dispatch:cargo:4" );
        CHECK( site.next_outing_generation == 5 );
    }

    SECTION( "schema-three local owner repairs its legacy even handoff epoch" ) {
        JsonValue transitional = json_loader::from_string( R"({
            "schema_version": 4,
            "site_id": "owner_epoch_camp",
            "members": [ { "npc_id": 46, "state": "local_contact" } ],
            "active_outing": {
                "schema_version": 3,
                "kind": "scout_sortie",
                "activity_id": "owner_epoch_camp#dispatch",
                "camp_id": "owner_epoch_camp",
                "generation": 2,
                "member_ids": [ 46 ],
                "leader_id": 46,
                "target_id": "legacy_contact",
                "job_type": "scout",
                "phase": "observing",
                "simulation_owner": "local",
                "handoff_epoch": 2,
                "started_minutes": 100,
                "local_contact_minutes": 150,
                "last_progress_minutes": 150,
                "last_advanced_minutes": 150,
                "return_application_key": "owner_epoch_camp#dispatch:return:2",
                "report_application_key": "owner_epoch_camp#dispatch:report:2",
                "cargo_application_key": "owner_epoch_camp#dispatch:cargo:2"
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( transitional.get_object() );

        REQUIRE( site.active_outing.is_active() );
        CHECK( site.active_outing.owner == bandit_live_world::simulation_owner::local );
        CHECK( site.active_outing.handoff_epoch == 3 );
        CHECK( site.active_outing.last_advanced_minutes == 150 );
        CHECK( bandit_live_world::current_external_simulation_cursor( site ).has_value() );
    }

    SECTION( "schema-four torn advancement cursor fails closed" ) {
        JsonValue malformed = json_loader::from_string( R"({
            "schema_version": 5,
            "site_id": "torn_cursor_camp",
            "members": [ { "npc_id": 48, "state": "outbound" } ],
            "active_outing": {
                "schema_version": 4,
                "kind": "scout_sortie",
                "activity_id": "torn_cursor_camp#dispatch",
                "camp_id": "torn_cursor_camp",
                "generation": 1,
                "member_ids": [ 48 ],
                "leader_id": 48,
                "target_id": "torn_target",
                "job_type": "scout",
                "phase": "searching",
                "simulation_owner": "abstract",
                "handoff_epoch": 0,
                "started_minutes": 100,
                "last_progress_minutes": 150,
                "last_advanced_minutes": 140,
                "return_application_key": "torn_cursor_camp#dispatch:return:1",
                "report_application_key": "torn_cursor_camp#dispatch:report:1",
                "cargo_application_key": "torn_cursor_camp#dispatch:cargo:1"
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( malformed.get_object() );

        CHECK_FALSE( site.active_outing.is_active() );
        CHECK( site.active_outing.member_ids.empty() );
        REQUIRE( site.find_member( character_id( 48 ) ) != nullptr );
        CHECK( site.find_member( character_id( 48 ) )->state ==
               bandit_live_world::member_state::at_home );
    }

    SECTION( "incomplete schema-four cursor never falls back to legacy scalars" ) {
        JsonValue malformed = json_loader::from_string( R"({
            "schema_version": 5,
            "site_id": "incomplete_cursor_camp",
            "members": [ { "npc_id": 49, "state": "local_contact" } ],
            "active_outing": {
                "schema_version": 4,
                "kind": "scout_sortie",
                "activity_id": "incomplete_cursor_camp#dispatch",
                "camp_id": "incomplete_cursor_camp",
                "generation": 1,
                "simulation_owner": "local",
                "handoff_epoch": 0,
                "last_advanced_minutes": 140
            },
            "active_hostile_operation": {},
            "active_target_id": "legacy_fallback_target",
            "active_job_type": "scout",
            "active_member_ids": [ 49 ],
            "active_sortie_started_minutes": 100,
            "active_sortie_local_contact_minutes": 150
        })" );
        bandit_live_world::site_record site;
        site.deserialize( malformed.get_object() );

        CHECK_FALSE( site.active_outing.is_active() );
        CHECK( site.active_outing.member_ids.empty() );
        REQUIRE( site.find_member( character_id( 49 ) ) != nullptr );
        CHECK( site.find_member( character_id( 49 ) )->state ==
               bandit_live_world::member_state::at_home );
        CHECK_FALSE( bandit_live_world::current_external_simulation_cursor( site ).has_value() );
    }

    SECTION( "invalid modern identity still releases its embedded reservations" ) {
        JsonValue malformed = json_loader::from_string( R"({
            "schema_version": 3,
            "site_id": "malformed_modern_camp",
            "members": [ { "npc_id": 45, "state": "outbound" } ],
            "active_outing": {
                "schema_version": 2,
                "kind": "none",
                "activity_id": "malformed_modern_camp#dispatch",
                "camp_id": "malformed_modern_camp",
                "generation": 5,
                "member_ids": [ 45 ]
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( malformed.get_object() );

        CHECK_FALSE( site.active_outing.is_active() );
        CHECK( site.active_outing.member_ids.empty() );
        REQUIRE( site.find_member( character_id( 45 ) ) != nullptr );
        CHECK( site.find_member( character_id( 45 ) )->state ==
               bandit_live_world::member_state::at_home );
    }
}

TEST_CASE( "bandit_live_world_migrates_or_closes_persisted_hostile_operation_owners",
           "[bandit][live_world][hostile_operation][migration][save]" )
{
    using bandit_live_world::hostile_operation_kind;
    using bandit_live_world::hostile_operation_phase;

    SECTION( "unpinned legacy hostile outing retains its party only for withdrawal" ) {
        JsonValue legacy = json_loader::from_string( R"({
            "schema_version":4,
            "site_id":"legacy_hostile_camp",
            "members":[{"npc_id":54,"state":"outbound"}],
            "current_scout_report":{
                "revision":2,
                "source_activity_id":"legacy_hostile_camp#scout:3",
                "source_generation":3,
                "source_job_type":"scout",
                "target_id":"legacy_target",
                "target_omt":[3,0,0],
                "target_lead_revision":0,
                "application_key":"legacy_hostile_camp#scout:3:report:3",
                "delivered_minutes":90,
                "provisional":false
            },
            "camp_decision":{
                "state":"preparing_follow_on",
                "source_report_revision":2,
                "source_report_generation":3,
                "source_report_activity_id":"legacy_hostile_camp#scout:3",
                "source_report_application_key":"legacy_hostile_camp#scout:3:report:3",
                "target_id":"legacy_target",
                "target_omt":[3,0,0],
                "target_lead_revision":0,
                "last_transition_minutes":95
            },
            "active_outing":{
                "schema_version":3,
                "kind":"hostile_operation",
                "activity_id":"legacy_hostile_camp#dispatch",
                "camp_id":"legacy_hostile_camp",
                "generation":4,
                "member_ids":[54],
                "leader_id":54,
                "shared_route":[[0,0,0],[3,0,0]],
                "waypoint_index":0,
                "target_id":"legacy_target",
                "target_omt":[3,0,0],
                "job_type":"toll",
                "phase":"outbound",
                "started_minutes":100,
                "last_progress_minutes":120,
                "last_advanced_minutes":120,
                "return_application_key":"legacy_hostile_camp#dispatch:return:4",
                "cargo_application_key":"legacy_hostile_camp#dispatch:cargo:4"
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( legacy.get_object() );

        CHECK( site.schema_version == 5 );
        CHECK_FALSE( site.active_outing.is_active() );
        REQUIRE( site.active_hostile_operation.is_active() );
        CHECK( site.active_hostile_operation.operation_kind ==
               hostile_operation_kind::shakedown );
        CHECK( site.active_hostile_operation.phase ==
               hostile_operation_phase::returning_home );
        CHECK( site.active_hostile_operation.legacy_unpinned );
        CHECK( site.active_hostile_operation.has_rally );
        CHECK( site.active_hostile_operation.reservation.activity_id ==
               "legacy_hostile_camp#dispatch" );
        CHECK( site.active_hostile_operation.reservation.generation == 4 );
        CHECK( site.active_hostile_operation.reservation.member_ids ==
               std::vector<character_id> { character_id( 54 ) } );
        CHECK( site.find_member( character_id( 54 ) )->state ==
               bandit_live_world::member_state::outbound );
        CHECK( site.next_outing_generation == 5 );
        const std::string before_escalation = serialize_world(
                                                  bandit_live_world::world_state { 3,
                                                      "hells_raiders_live_owner_v0", { site } } );
        CHECK( transition_test_hostile_operation(
                   site, hostile_operation_phase::returning_home,
                   hostile_operation_phase::approaching, 130, "unsafe legacy escalation" ) ==
               bandit_live_world::hostile_operation_transition_result::rejected );
        CHECK( serialize_world( bandit_live_world::world_state { 3,
                                "hells_raiders_live_owner_v0", { site } } ) == before_escalation );
        CHECK( transition_test_hostile_operation(
                   site, hostile_operation_phase::returning_home,
                   hostile_operation_phase::lost, 130, "legacy party lost" ) ==
               bandit_live_world::hostile_operation_transition_result::applied );
        CHECK( serialize_world( bandit_live_world::world_state { 3,
                                "hells_raiders_live_owner_v0", { site } } ) != before_escalation );
    }

    SECTION( "malformed new operation releases every known in-flight reservation" ) {
        JsonValue malformed = json_loader::from_string( R"({
            "schema_version":5,
            "site_id":"malformed_hostile_camp",
            "members":[{"npc_id":55,"state":"outbound"}],
            "active_hostile_operation":{
                "operation_kind":"shakedown",
                "phase":"returning_home",
                "legacy_unpinned":true,
                "reservation":{
                    "kind":"hostile_operation",
                    "activity_id":"malformed_hostile_camp#hostile:5",
                    "camp_id":"malformed_hostile_camp",
                    "generation":5,
                    "member_ids":[55,999],
                    "leader_id":55,
                    "job_type":"toll",
                    "phase":"returning_home",
                    "return_application_key":"malformed_hostile_camp#hostile:5:return:5",
                    "cargo_application_key":"malformed_hostile_camp#hostile:5:cargo:5"
                }
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( malformed.get_object() );

        CHECK_FALSE( site.active_hostile_operation.is_active() );
        CHECK( site.active_hostile_operation.reservation.member_ids.empty() );
        REQUIRE( site.find_member( character_id( 55 ) ) != nullptr );
        CHECK( site.find_member( character_id( 55 ) )->state ==
               bandit_live_world::member_state::at_home );
    }

    SECTION( "a valid scout owner wins over a duplicate hostile owner" ) {
        JsonValue duplicate = json_loader::from_string( R"({
            "schema_version":5,
            "site_id":"duplicate_owner_camp",
            "members":[
                {"npc_id":60,"state":"outbound"},
                {"npc_id":61,"state":"at_home"}
            ],
            "active_outing":{
                "kind":"scout_sortie",
                "activity_id":"duplicate_owner_camp#scout:2",
                "camp_id":"duplicate_owner_camp",
                "generation":2,
                "member_ids":[60],
                "leader_id":60,
                "job_type":"scout",
                "phase":"outbound",
                "return_application_key":"duplicate_owner_camp#scout:2:return:2",
                "report_application_key":"duplicate_owner_camp#scout:2:report:2",
                "cargo_application_key":"duplicate_owner_camp#scout:2:cargo:2"
            },
            "active_hostile_operation":{
                "operation_kind":"shakedown",
                "phase":"assembling",
                "has_rally":true,
                "rally_omt":[1,0,0],
                "legacy_unpinned":true,
                "reservation":{
                    "kind":"hostile_operation",
                    "activity_id":"duplicate_owner_camp#hostile:3",
                    "camp_id":"duplicate_owner_camp",
                    "generation":3,
                    "member_ids":[61],
                    "leader_id":61,
                    "shared_route":[[0,0,0],[1,0,0],[2,0,0]],
                    "target_omt":[2,0,0],
                    "job_type":"toll",
                    "phase":"assembling"
                }
            }
        })" );
        bandit_live_world::site_record site;
        site.deserialize( duplicate.get_object() );

        REQUIRE( site.active_outing.is_active() );
        CHECK( site.active_outing.kind == bandit_live_world::outing_kind::scout_sortie );
        CHECK_FALSE( site.active_hostile_operation.is_active() );
        CHECK( site.active_hostile_operation.reservation.member_ids.empty() );
        CHECK( site.find_member( character_id( 60 ) )->state ==
               bandit_live_world::member_state::outbound );
        CHECK( site.find_member( character_id( 61 ) )->state ==
               bandit_live_world::member_state::at_home );
    }
}

TEST_CASE( "bandit_live_world_round_trips_bounded_authoritative_scout_state",
           "[bandit][live_world][scout_state][save]" )
{
    bandit_live_world::world_state original;
    add_bandit_camp_member( original, 0, 45000 );
    add_bandit_camp_member( original, 1, 45000 );
    bandit_live_world::site_record &site = original.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45000 ),
             bandit_live_world::member_state::local_contact, "test scout active" ) );
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45001 ),
             bandit_live_world::member_state::dead, "test recorded casualty" ) );
    set_test_active_outing( site, site.site_id + "#scout:full" );
    site.active_outing.member_ids = { character_id( 45000 ), character_id( 45001 ) };
    site.active_outing.leader_id = character_id( 45000 );
    site.active_outing.shared_route = { site.anchor, tripoint_abs_omt( 12, 20, 0 ),
                                       tripoint_abs_omt( 14, 20, 0 ) };
    site.active_outing.waypoint_index = 1;
    site.active_outing.target_id = "resource-lead-7";
    site.active_outing.target_omt = tripoint_abs_omt( 14, 20, 0 );
    site.active_outing.job_type = "scout";
    site.active_outing.target_lead_revision = 7;
    site.active_outing.phase = bandit_live_world::scout_phase::burned_withdrawal;
    site.active_outing.observations.push_back( { "burn@14,20,0", "two observers exposed", 83,
                                                 330, true } );
    site.active_outing.cargo.supply_units = 3;
    site.active_outing.cargo.trade_value = 90;
    site.active_outing.casualty_ids = { character_id( 45001 ) };
    site.active_outing.started_minutes = 100;
    site.active_outing.local_contact_minutes = 300;
    site.active_outing.last_progress_minutes = 330;
    site.active_outing.expected_return_minutes = 940;
    site.active_outing.missing_deadline_minutes = 2380;
    site.active_outing.owner = bandit_live_world::simulation_owner::local;
    site.active_outing.handoff_epoch = 1;
    site.active_outing.last_advanced_minutes = 331;

    const std::vector<bandit_live_world::scout_phase> phases = {
        bandit_live_world::scout_phase::assembling,
        bandit_live_world::scout_phase::outbound,
        bandit_live_world::scout_phase::searching,
        bandit_live_world::scout_phase::observing,
        bandit_live_world::scout_phase::harvesting,
        bandit_live_world::scout_phase::burned_withdrawal,
        bandit_live_world::scout_phase::returning_exposed,
        bandit_live_world::scout_phase::returning_report,
        bandit_live_world::scout_phase::returning_home,
        bandit_live_world::scout_phase::lost,
    };
    for( const bandit_live_world::scout_phase phase : phases ) {
        original.sites.front().active_outing.phase = phase;
        const bandit_live_world::world_state loaded = round_trip_world( original );
        REQUIRE( loaded.schema_version == 3 );
        REQUIRE( loaded.sites.size() == 1 );
        const bandit_live_world::active_outing_state &outing = loaded.sites.front().active_outing;
        CHECK( outing.schema_version == 4 );
        CHECK( outing.phase == phase );
        CHECK( outing.member_ids == original.sites.front().active_outing.member_ids );
        CHECK( outing.leader_id == character_id( 45000 ) );
        CHECK( outing.shared_route == original.sites.front().active_outing.shared_route );
        CHECK( outing.waypoint_index == 1 );
        CHECK( outing.target_id == "resource-lead-7" );
        CHECK( outing.target_omt == tripoint_abs_omt( 14, 20, 0 ) );
        CHECK( outing.target_lead_revision == 7 );
        REQUIRE( outing.observations.size() == 1 );
        CHECK( outing.observations.front().fact_key == "burn@14,20,0" );
        CHECK( outing.observations.front().confidence == 83 );
        CHECK( outing.observations.front().critical );
        CHECK( outing.cargo.supply_units == 3 );
        CHECK( outing.cargo.trade_value == 90 );
        CHECK( outing.casualty_ids == std::vector<character_id> { character_id( 45001 ) } );
        CHECK( outing.started_minutes == 100 );
        CHECK( outing.local_contact_minutes == 300 );
        CHECK( outing.last_progress_minutes == 330 );
        CHECK( outing.expected_return_minutes == 940 );
        CHECK( outing.missing_deadline_minutes == 2380 );
        CHECK( outing.owner == bandit_live_world::simulation_owner::local );
        CHECK( outing.handoff_epoch == 1 );
        CHECK( outing.last_advanced_minutes == 331 );
        CHECK_FALSE( outing.return_application_key.empty() );
        CHECK_FALSE( outing.report_application_key.empty() );
        CHECK_FALSE( outing.cargo_application_key.empty() );
    }
}

TEST_CASE( "bandit_live_world_simulation_owner_handoff_is_shared_and_token_checked",
           "[bandit][live_world][handoff][owner]" )
{
    using bandit_live_world::simulation_owner;
    using bandit_live_world::simulation_owner_transition_result;

    bandit_live_world::world_state scout_world;
    add_bandit_camp_member( scout_world, 0, 45500 );
    add_bandit_camp_member( scout_world, 1, 45500 );
    bandit_live_world::site_record &scout_site = scout_world.sites.front();
    REQUIRE( bandit_live_world::update_member_state(
                 scout_site, character_id( 45500 ), bandit_live_world::member_state::outbound,
                 "test scout departed" ) );
    set_test_active_outing( scout_site, scout_site.site_id + "#scout:owner" );
    scout_site.active_outing.member_ids = { character_id( 45500 ) };
    scout_site.active_outing.leader_id = character_id( 45500 );
    scout_site.active_outing.phase = bandit_live_world::scout_phase::outbound;
    scout_site.active_outing.started_minutes = 10;
    scout_site.active_outing.last_progress_minutes = 10;
    scout_site.active_outing.last_advanced_minutes = 10;
    const std::string scout_id = scout_site.active_outing.activity_id;
    const int scout_generation = scout_site.active_outing.generation;
    const std::string initial_scout = serialize_world( scout_world );

    CHECK( bandit_live_world::advance_external_simulation(
               scout_site, scout_id, scout_generation, simulation_owner::abstract, 0, 9, 11 ) ==
           simulation_owner_transition_result::rejected );
    CHECK( bandit_live_world::transition_external_simulation_owner(
               scout_site, scout_id + ":stale", scout_generation,
               simulation_owner::abstract, simulation_owner::local, 0, 10, 11 ) ==
           simulation_owner_transition_result::rejected );
    CHECK( serialize_world( scout_world ) == initial_scout );

    REQUIRE( bandit_live_world::advance_external_simulation(
                 scout_site, scout_id, scout_generation, simulation_owner::abstract, 0, 10, 11 ) ==
             simulation_owner_transition_result::applied );
    CHECK( bandit_live_world::advance_external_simulation(
               scout_site, scout_id, scout_generation, simulation_owner::abstract, 0, 10, 12 ) ==
           simulation_owner_transition_result::rejected );
    const std::string after_advance = serialize_world( scout_world );
    CHECK( bandit_live_world::advance_external_simulation(
               scout_site, scout_id, scout_generation, simulation_owner::abstract, 0, 11, 11 ) ==
           simulation_owner_transition_result::rejected );
    CHECK( serialize_world( scout_world ) == after_advance );

    REQUIRE( bandit_live_world::transition_external_simulation_owner(
                 scout_site, scout_id, scout_generation, simulation_owner::abstract,
                 simulation_owner::local, 0, 11, 12 ) ==
             simulation_owner_transition_result::applied );
    CHECK( scout_site.active_outing.owner == simulation_owner::local );
    CHECK( scout_site.active_outing.handoff_epoch == 1 );
    CHECK( scout_site.active_outing.last_advanced_minutes == 12 );
    const std::string local_scout = serialize_world( scout_world );
    CHECK( bandit_live_world::transition_external_simulation_owner(
               scout_site, scout_id, scout_generation, simulation_owner::abstract,
               simulation_owner::local, 0, 11, 12 ) ==
           simulation_owner_transition_result::rejected );
    CHECK( bandit_live_world::advance_external_simulation(
               scout_site, scout_id, scout_generation, simulation_owner::abstract, 0, 11, 13 ) ==
           simulation_owner_transition_result::rejected );
    CHECK( serialize_world( scout_world ) == local_scout );

    REQUIRE( bandit_live_world::advance_external_simulation(
                 scout_site, scout_id, scout_generation, simulation_owner::local, 1, 12, 13 ) ==
             simulation_owner_transition_result::applied );
    REQUIRE( bandit_live_world::transition_external_simulation_owner(
                 scout_site, scout_id, scout_generation, simulation_owner::local,
                 simulation_owner::abstract, 1, 13, 14 ) ==
             simulation_owner_transition_result::applied );
    const bandit_live_world::world_state loaded_scout = round_trip_world( scout_world );
    REQUIRE( loaded_scout.sites.size() == 1 );
    CHECK( loaded_scout.sites.front().active_outing.owner == simulation_owner::abstract );
    CHECK( loaded_scout.sites.front().active_outing.handoff_epoch == 2 );
    CHECK( loaded_scout.sites.front().active_outing.last_advanced_minutes == 14 );

    bandit_live_world::world_state hostile_world;
    for( int index = 0; index < 6; ++index ) {
        add_bandit_camp_member( hostile_world, index, 45600 );
    }
    bandit_live_world::site_record &hostile_site = hostile_world.sites.front();
    const tripoint_abs_omt rally( 14, 20, 0 );
    const tripoint_abs_omt target( 18, 20, 0 );
    prepare_hostile_follow_on( hostile_site, 5, 4, "owner-target", target, 700 );
    const bandit_live_world::hostile_operation_plan hostile_plan =
        bandit_live_world::plan_hostile_operation(
            hostile_site, bandit_live_world::hostile_operation_kind::shakedown,
            { character_id( 45600 ), character_id( 45601 ) },
            { hostile_site.anchor, rally, target }, rally, 702 );
    REQUIRE( hostile_plan.valid );
    REQUIRE( bandit_live_world::apply_hostile_operation_plan( hostile_site, hostile_plan ) );
    REQUIRE( transition_test_hostile_operation(
                 hostile_site, bandit_live_world::hostile_operation_phase::assembling,
                 bandit_live_world::hostile_operation_phase::outbound, 703,
                 "test hostile departed" ) ==
             bandit_live_world::hostile_operation_transition_result::applied );
    bandit_live_world::active_outing_state &hostile_reservation =
        hostile_site.active_hostile_operation.reservation;
    REQUIRE( bandit_live_world::transition_external_simulation_owner(
                 hostile_site, hostile_reservation.activity_id,
                 hostile_reservation.generation, simulation_owner::abstract,
                 simulation_owner::local, 0, 703, 704 ) ==
             simulation_owner_transition_result::applied );
    const bandit_live_world::world_state loaded_hostile = round_trip_world( hostile_world );
    REQUIRE( loaded_hostile.sites.size() == 1 );
    REQUIRE( loaded_hostile.sites.front().active_hostile_operation.is_active() );
    CHECK( loaded_hostile.sites.front().active_hostile_operation.reservation.owner ==
           simulation_owner::local );
    CHECK( loaded_hostile.sites.front().active_hostile_operation.reservation.handoff_epoch == 1 );
    CHECK( loaded_hostile.sites.front().active_hostile_operation.reservation.last_advanced_minutes == 704 );

    bandit_live_world::world_state repaired_hostile = hostile_world;
    repaired_hostile.sites.front().active_hostile_operation.reservation.owner =
        simulation_owner::local;
    repaired_hostile.sites.front().active_hostile_operation.reservation.handoff_epoch = 0;
    repaired_hostile.sites.front().active_hostile_operation.reservation.last_advanced_minutes = -1;
    repaired_hostile = round_trip_world( repaired_hostile );
    CHECK_FALSE( repaired_hostile.sites.front().active_hostile_operation.is_active() );
    CHECK( repaired_hostile.sites.front().active_hostile_operation.reservation.member_ids.empty() );
}

TEST_CASE( "bandit_live_world_scout_phase_transitions_are_one_way_and_atomic",
           "[bandit][live_world][scout_state][phase]" )
{
    using bandit_live_world::scout_phase;
    using bandit_live_world::scout_phase_transition_result;

    const std::vector<scout_phase> phases = {
        scout_phase::assembling,
        scout_phase::outbound,
        scout_phase::searching,
        scout_phase::observing,
        scout_phase::harvesting,
        scout_phase::burned_withdrawal,
        scout_phase::returning_exposed,
        scout_phase::returning_report,
        scout_phase::returning_home,
        scout_phase::lost,
    };
    const std::vector<std::pair<scout_phase, scout_phase>> forward_edges = {
        { scout_phase::assembling, scout_phase::outbound },
        { scout_phase::outbound, scout_phase::searching },
        { scout_phase::outbound, scout_phase::observing },
        { scout_phase::outbound, scout_phase::returning_report },
        { scout_phase::outbound, scout_phase::returning_home },
        { scout_phase::searching, scout_phase::observing },
        { scout_phase::searching, scout_phase::returning_report },
        { scout_phase::searching, scout_phase::returning_home },
        { scout_phase::observing, scout_phase::harvesting },
        { scout_phase::observing, scout_phase::burned_withdrawal },
        { scout_phase::observing, scout_phase::returning_report },
        { scout_phase::observing, scout_phase::returning_home },
        { scout_phase::harvesting, scout_phase::returning_report },
        { scout_phase::harvesting, scout_phase::returning_home },
        { scout_phase::burned_withdrawal, scout_phase::returning_exposed },
        { scout_phase::burned_withdrawal, scout_phase::returning_report },
        { scout_phase::burned_withdrawal, scout_phase::returning_home },
        { scout_phase::returning_exposed, scout_phase::returning_report },
        { scout_phase::returning_exposed, scout_phase::returning_home },
        { scout_phase::returning_report, scout_phase::returning_home },
    };
    for( const scout_phase previous : phases ) {
        for( const scout_phase next : phases ) {
            const bool same_phase = previous == next;
            const bool terminal_loss = next == scout_phase::lost &&
                                       previous != scout_phase::lost;
            const bool forward = std::find( forward_edges.begin(), forward_edges.end(),
                                            std::make_pair( previous, next ) ) !=
                                 forward_edges.end();
            CHECK( bandit_live_world::is_valid_scout_phase_transition( previous, next ) ==
                   ( same_phase || terminal_loss || forward ) );
        }
    }

    CHECK( bandit_live_world::scout_phase_after_burned_evacuation( true ) ==
           scout_phase::returning_report );
    CHECK( bandit_live_world::scout_phase_after_burned_evacuation( false ) ==
           scout_phase::returning_exposed );
    CHECK( bandit_live_world::scout_phase_requires_homeward_only(
               scout_phase::burned_withdrawal ) );
    CHECK( bandit_live_world::scout_phase_requires_homeward_only(
               scout_phase::returning_exposed ) );
    CHECK_FALSE( bandit_live_world::scout_phase_requires_homeward_only(
                     scout_phase::observing ) );

    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45100 );
    bandit_live_world::site_record &site = world.sites.front();
    set_test_active_outing( site, site.site_id + "#scout:phase" );
    site.active_outing.member_ids = { character_id( 45100 ) };
    site.active_outing.leader_id = character_id( 45100 );
    site.active_outing.job_type = "scout";
    site.active_outing.phase = scout_phase::outbound;
    site.active_outing.started_minutes = 100;
    site.active_outing.last_progress_minutes = 100;
    site.active_outing.last_advanced_minutes = 100;

    const bandit_live_world::simulation_advance_cursor initial_cursor =
        require_current_simulation_cursor( site );
    const std::string before_stale = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::assembling, scout_phase::searching, 110 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == before_stale );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::outbound, scout_phase::searching, 99 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == before_stale );

    site.active_outing.kind = bandit_live_world::outing_kind::hostile_operation;
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::outbound, scout_phase::searching, 110 ) ==
           scout_phase_transition_result::rejected );
    site.active_outing.kind = bandit_live_world::outing_kind::scout_sortie;
    site.active_outing.job_type = "raid";
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::outbound, scout_phase::searching, 110 ) ==
           scout_phase_transition_result::rejected );
    site.active_outing.job_type = "scout";
    CHECK( serialize_world( world ) == before_stale );

    bandit_live_world::site_record legacy_scavenge = site;
    legacy_scavenge.active_outing.job_type = "scavenge";
    CHECK( bandit_live_world::transition_active_scout_phase(
               legacy_scavenge, require_current_simulation_cursor( legacy_scavenge ),
               scout_phase::outbound, scout_phase::searching, 110 ) ==
           scout_phase_transition_result::applied );

    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::outbound, scout_phase::searching, 110 ) ==
           scout_phase_transition_result::applied );
    CHECK( site.active_outing.phase == scout_phase::searching );
    CHECK( site.active_outing.last_progress_minutes == 110 );
    CHECK( site.active_outing.last_advanced_minutes == 110 );
    const std::string after_first_advance = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, initial_cursor, scout_phase::searching, scout_phase::observing, 120 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == after_first_advance );
    const std::string before_same = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::searching,
               scout_phase::searching, 120 ) ==
           scout_phase_transition_result::unchanged );
    CHECK( serialize_world( world ) == before_same );

    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::searching,
               scout_phase::observing, 120 ) ==
           scout_phase_transition_result::applied );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::observing,
               scout_phase::burned_withdrawal, 130 ) ==
           scout_phase_transition_result::applied );
    const std::string burned_state = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::burned_withdrawal,
               scout_phase::observing, 140 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == burned_state );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::burned_withdrawal,
               scout_phase::returning_exposed, 140 ) ==
           scout_phase_transition_result::applied );
    const std::string exposed_state = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::returning_exposed,
               scout_phase::harvesting, 150 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == exposed_state );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::returning_exposed,
               scout_phase::returning_report, 150 ) ==
           scout_phase_transition_result::applied );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::returning_report,
               scout_phase::returning_home, 160 ) ==
           scout_phase_transition_result::applied );
    const std::string home_state = serialize_world( world );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::returning_home,
               scout_phase::outbound, 170 ) ==
           scout_phase_transition_result::rejected );
    CHECK( serialize_world( world ) == home_state );
    CHECK( bandit_live_world::transition_active_scout_phase(
               site, require_current_simulation_cursor( site ), scout_phase::returning_home,
               scout_phase::lost, 170 ) ==
           scout_phase_transition_result::applied );

    bandit_live_world::world_state returning_world;
    add_bandit_camp_member( returning_world, 0, 45110 );
    bandit_live_world::site_record &returning_site = returning_world.sites.front();
    set_test_active_outing( returning_site, returning_site.site_id + "#scout:returning" );
    returning_site.active_outing.member_ids = { character_id( 45110 ) };
    returning_site.active_outing.leader_id = character_id( 45110 );
    returning_site.active_outing.job_type = "scout";
    returning_site.active_outing.phase = scout_phase::returning_exposed;
    returning_site.active_outing.started_minutes = 100;
    returning_site.active_outing.last_progress_minutes = 200;
    returning_site.active_outing.last_advanced_minutes = 200;
    const std::string before_contact = serialize_world( returning_world );
    CHECK_FALSE( bandit_live_world::note_active_sortie_local_contact(
                     returning_site, require_current_simulation_cursor( returning_site ),
                     character_id( 45110 ), 210 ) );
    CHECK( serialize_world( returning_world ) == before_contact );

    returning_site.active_outing.started_minutes = -1;
    returning_site.active_outing.phase = scout_phase::burned_withdrawal;
    const std::string before_restart = serialize_world( returning_world );
    CHECK_FALSE( bandit_live_world::note_active_sortie_started(
                     returning_site, require_current_simulation_cursor( returning_site ), 220 ) );
    CHECK( serialize_world( returning_world ) == before_restart );

    JsonValue future_phase_json = json_loader::from_string(
        R"({"kind":"scout_sortie","activity_id":"future-phase","generation":1,"job_type":"scout","phase":"future_phase"})" );
    bandit_live_world::active_outing_state future_phase;
    future_phase.deserialize( future_phase_json.get_object() );
    CHECK( future_phase.phase == scout_phase::lost );

    JsonValue legacy_phase_json = json_loader::from_string(
        R"({"kind":"scout_sortie","activity_id":"legacy-phase","generation":1,"job_type":"scout"})" );
    bandit_live_world::active_outing_state legacy_phase;
    legacy_phase.deserialize( legacy_phase_json.get_object() );
    CHECK( legacy_phase.phase == scout_phase::assembling );

    bandit_live_world::world_state malformed_job_world;
    add_bandit_camp_member( malformed_job_world, 0, 45120 );
    bandit_live_world::site_record &malformed_job_site = malformed_job_world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( malformed_job_site, character_id( 45120 ),
             bandit_live_world::member_state::outbound, "malformed scout reservation" ) );
    set_test_active_outing( malformed_job_site,
                            malformed_job_site.site_id + "#scout:malformed-job" );
    malformed_job_site.active_outing.member_ids = { character_id( 45120 ) };
    malformed_job_site.active_outing.leader_id = character_id( 45120 );
    malformed_job_site.active_outing.job_type = "raid";
    const bandit_live_world::world_state repaired_job = round_trip_world( malformed_job_world );
    REQUIRE( repaired_job.sites.size() == 1 );
    CHECK_FALSE( repaired_job.sites.front().active_outing.is_active() );
    CHECK( repaired_job.sites.front().find_member( character_id( 45120 ) )->state ==
           bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_live_world_camp_decision_pins_only_final_scout_reports_and_transitions_once",
           "[bandit][live_world][camp_decision][save][replay]" )
{
    using bandit_live_world::camp_decision_state;
    using bandit_live_world::camp_decision_transition_result;

    const std::vector<camp_decision_state> states = {
        camp_decision_state::idle,
        camp_decision_state::report_awaiting_assessment,
        camp_decision_state::preparing_follow_on,
        camp_decision_state::cooldown,
        camp_decision_state::abandoned,
    };
    const std::vector<std::pair<camp_decision_state, camp_decision_state>> forward_edges = {
        { camp_decision_state::idle, camp_decision_state::report_awaiting_assessment },
        { camp_decision_state::report_awaiting_assessment,
          camp_decision_state::preparing_follow_on },
        { camp_decision_state::report_awaiting_assessment, camp_decision_state::cooldown },
        { camp_decision_state::preparing_follow_on, camp_decision_state::cooldown },
        { camp_decision_state::cooldown, camp_decision_state::idle },
        { camp_decision_state::cooldown,
          camp_decision_state::report_awaiting_assessment },
        { camp_decision_state::abandoned,
          camp_decision_state::report_awaiting_assessment },
    };
    for( const camp_decision_state previous : states ) {
        for( const camp_decision_state next : states ) {
            const bool same_state = previous == next;
            const bool fail_closed = next == camp_decision_state::abandoned &&
                                     previous != camp_decision_state::abandoned;
            const bool forward = std::find( forward_edges.begin(), forward_edges.end(),
                                            std::make_pair( previous, next ) ) !=
                                 forward_edges.end();
            CHECK( bandit_live_world::is_valid_camp_decision_transition( previous, next ) ==
                   ( same_state || fail_closed || forward ) );
        }
    }

    bandit_live_world::world_state world;
    for( int index = 0; index < 3; ++index ) {
        add_bandit_camp_member( world, index, 45130 );
    }
    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan stale_idle_plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ),
                "decision-target" );
    REQUIRE( stale_idle_plan.valid );
    site.current_scout_report.revision = 7;
    site.current_scout_report.source_activity_id = site.site_id + "#scout:decision";
    site.current_scout_report.source_generation = 4;
    site.current_scout_report.source_job_type = "scout";
    site.current_scout_report.target_id = "decision-target";
    site.current_scout_report.target_omt = tripoint_abs_omt( 18, 20, 0 );
    site.current_scout_report.target_lead_revision = 3;
    site.current_scout_report.application_key =
        site.current_scout_report.source_activity_id + ":report:4";
    site.current_scout_report.delivered_minutes = 600;
    site.current_scout_report.provisional = true;

    const std::string before_provisional = serialize_world( world );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::rejected );
    CHECK( serialize_world( world ) == before_provisional );
    site.current_scout_report.provisional = false;
    site.current_scout_report.source_job_type = "scavenge";
    const std::string before_scavenge = serialize_world( world );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::rejected );
    CHECK( serialize_world( world ) == before_scavenge );
    site.current_scout_report.source_job_type = "scout";
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::applied );
    CHECK( site.camp_decision.state == camp_decision_state::report_awaiting_assessment );
    CHECK( site.camp_decision.source_report_revision == 7 );
    CHECK( site.camp_decision.source_report_generation == 4 );
    CHECK( site.camp_decision.source_report_activity_id ==
           site.current_scout_report.source_activity_id );
    CHECK( site.camp_decision.source_report_application_key ==
           site.current_scout_report.application_key );
    CHECK_FALSE( bandit_live_world::plan_site_dispatch(
                     site, tripoint_abs_omt( 18, 20, 0 ), "decision-target" ).valid );
    CHECK_FALSE( bandit_live_world::apply_dispatch_plan( site, stale_idle_plan ) );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::unchanged );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    CHECK( loaded.sites.front().camp_decision.state ==
           camp_decision_state::report_awaiting_assessment );
    CHECK( loaded.sites.front().camp_decision.source_report_generation == 4 );
    CHECK( loaded.sites.front().camp_decision.target_id == "decision-target" );

    const std::string before_stale = serialize_world( world );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::preparing_follow_on, 6, 4, 700, -1, "wrong report" ) ==
           camp_decision_transition_result::rejected );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::preparing_follow_on, 7, 3, 700, -1, "wrong generation" ) ==
           camp_decision_transition_result::rejected );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::preparing_follow_on, 7, 4, 599, -1, "stale time" ) ==
           camp_decision_transition_result::rejected );
    CHECK( serialize_world( world ) == before_stale );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::report_awaiting_assessment, 7, 4, 700, -1, "same" ) ==
           camp_decision_transition_result::unchanged );
    CHECK( serialize_world( world ) == before_stale );

    site.active_outing.kind = bandit_live_world::outing_kind::structural_sortie;
    site.active_outing.activity_id = site.site_id + "#structural";
    site.active_outing.generation = 8;
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::preparing_follow_on, 7, 4, 700, -1, "slot busy" ) ==
           camp_decision_transition_result::rejected );
    site.active_outing.clear();
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::preparing_follow_on, 7, 4, 700, -1, "assessment passed" ) ==
           camp_decision_transition_result::applied );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::preparing_follow_on,
               camp_decision_state::cooldown, 7, 4, 800, 799, "invalid cooldown" ) ==
           camp_decision_transition_result::rejected );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::preparing_follow_on,
               camp_decision_state::cooldown, 7, 4, 800, 900, "bounded cooldown" ) ==
           camp_decision_transition_result::applied );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::cooldown, camp_decision_state::idle,
               7, 4, 899, -1, "too early" ) == camp_decision_transition_result::rejected );
    CHECK( bandit_live_world::transition_camp_decision_state(
               site, camp_decision_state::cooldown, camp_decision_state::idle,
               7, 4, 900, -1, "cooldown elapsed" ) == camp_decision_transition_result::applied );
    CHECK( site.camp_decision.state == camp_decision_state::idle );
    CHECK( site.camp_decision.source_report_revision == 7 );
    CHECK( site.camp_decision.source_report_generation == 4 );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( site ) ==
           camp_decision_transition_result::rejected );

    bandit_live_world::world_state abandoned_world = loaded;
    bandit_live_world::site_record &abandoned_site = abandoned_world.sites.front();
    CHECK( bandit_live_world::transition_camp_decision_state(
               abandoned_site, camp_decision_state::report_awaiting_assessment,
               camp_decision_state::abandoned, 7, 4, 700, -1, "assessment rejected" ) ==
           camp_decision_transition_result::applied );
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( abandoned_site ) ==
           camp_decision_transition_result::rejected );
    abandoned_site.current_scout_report.revision = 8;
    abandoned_site.current_scout_report.source_generation = 5;
    abandoned_site.current_scout_report.source_activity_id = abandoned_site.site_id +
            "#scout:new-decision";
    abandoned_site.current_scout_report.application_key =
        abandoned_site.current_scout_report.source_activity_id + ":report:5";
    abandoned_site.current_scout_report.delivered_minutes = 800;
    CHECK( bandit_live_world::accept_current_scout_report_for_assessment( abandoned_site ) ==
           camp_decision_transition_result::applied );

    JsonValue unknown_state_json = json_loader::from_string(
                                       R"({"state":"future_state"})" );
    bandit_live_world::camp_decision_record unknown_state;
    unknown_state.deserialize( unknown_state_json.get_object() );
    CHECK( unknown_state.state == camp_decision_state::abandoned );
    CHECK( unknown_state.transition_reason == "unknown persisted camp decision state" );

    JsonValue missing_state_json = json_loader::from_string( R"({"schema_version":0})" );
    bandit_live_world::camp_decision_record missing_state;
    missing_state.deserialize( missing_state_json.get_object() );
    CHECK( missing_state.state == camp_decision_state::idle );

    JsonValue legacy_final_report_json = json_loader::from_string( R"({
        "site_id":"legacy-final-report",
        "current_scout_report":{
            "revision":2,
            "source_activity_id":"legacy-final-report#scout:2",
            "source_generation":2,
            "source_job_type":"scout",
            "target_id":"legacy-target",
            "target_omt":[4,5,0],
            "target_lead_revision":1,
            "application_key":"legacy-final-report#scout:2:report:2",
            "delivered_minutes":500,
            "provisional":false
        }
    })" );
    bandit_live_world::site_record migrated_final_report;
    migrated_final_report.deserialize( legacy_final_report_json.get_object() );
    CHECK( migrated_final_report.camp_decision.state ==
           camp_decision_state::report_awaiting_assessment );
    CHECK( migrated_final_report.camp_decision.source_report_generation == 2 );
    CHECK( migrated_final_report.camp_decision.target_id == "legacy-target" );

    bandit_live_world::world_state malformed_world = loaded;
    malformed_world.sites.front().camp_decision.source_report_application_key = "forged-key";
    const bandit_live_world::world_state repaired = round_trip_world( malformed_world );
    REQUIRE( repaired.sites.size() == 1 );
    CHECK( repaired.sites.front().camp_decision.state == camp_decision_state::abandoned );
    CHECK( repaired.sites.front().camp_decision.transition_reason ==
           "repaired inconsistent persisted camp decision" );
}

TEST_CASE( "bandit_live_world_plans_and_applies_a_fresh_report_pinned_hostile_operation",
           "[bandit][live_world][hostile_operation][plan][save][replay]" )
{
    using bandit_live_world::hostile_operation_kind;
    using bandit_live_world::hostile_operation_phase;

    bandit_live_world::world_state world;
    for( int index = 0; index < 7; ++index ) {
        add_bandit_camp_member( world, index, 45200 );
    }
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt rally( 14, 20, 0 );
    const tripoint_abs_omt target( 18, 20, 0 );
    const std::vector<tripoint_abs_omt> route = { site.anchor, rally, target };
    const std::vector<character_id> members = {
        character_id( 45200 ), character_id( 45201 ), character_id( 45202 )
    };
    prepare_hostile_follow_on( site, 9, 4, "report-target", target, 600 );

    const std::string before_rejections = serialize_world( world );
    CHECK_FALSE( bandit_live_world::plan_hostile_operation(
                     site, hostile_operation_kind::raid, members, route, rally, 602 ).valid );
    CHECK_FALSE( bandit_live_world::plan_hostile_operation(
                     site, hostile_operation_kind::shakedown, members,
                     { site.anchor, target }, rally, 602 ).valid );
    site.find_member( members.back() )->wounded_or_unready = true;
    CHECK_FALSE( bandit_live_world::plan_hostile_operation(
                     site, hostile_operation_kind::shakedown, members, route, rally, 602 ).valid );
    site.find_member( members.back() )->wounded_or_unready = false;
    CHECK( serialize_world( world ) == before_rejections );

    const bandit_live_world::hostile_operation_plan plan =
        bandit_live_world::plan_hostile_operation(
            site, hostile_operation_kind::shakedown, members, route, rally, 602 );
    REQUIRE( plan.valid );
    CHECK( plan.operation.operation_kind == hostile_operation_kind::shakedown );
    CHECK( plan.operation.phase == hostile_operation_phase::assembling );
    CHECK( plan.operation.source_report_revision == 9 );
    CHECK( plan.operation.source_report_generation == 4 );
    CHECK( plan.operation.source_report_activity_id ==
           site.current_scout_report.source_activity_id );
    CHECK( plan.operation.source_report_application_key ==
           site.current_scout_report.application_key );
    CHECK( plan.operation.has_rally );
    CHECK( plan.operation.rally_omt == rally );
    CHECK_FALSE( plan.operation.legacy_unpinned );
    CHECK( plan.operation.reservation.activity_id == site.site_id + "#hostile:5" );
    CHECK( plan.operation.reservation.generation == 5 );
    CHECK( plan.operation.reservation.kind ==
           bandit_live_world::outing_kind::hostile_operation );
    CHECK( plan.operation.reservation.member_ids == members );
    CHECK( plan.operation.reservation.shared_route == route );
    CHECK( plan.operation.reservation.target_id == "report-target" );
    CHECK( plan.operation.reservation.job_type == "toll" );

    REQUIRE( bandit_live_world::apply_hostile_operation_plan( site, plan ) );
    CHECK_FALSE( site.active_outing.is_active() );
    CHECK( site.active_hostile_operation.is_active() );
    CHECK( site.active_external_outing() == &site.active_hostile_operation.reservation );
    CHECK( site.next_outing_generation == 6 );
    for( const character_id &member_id : members ) {
        REQUIRE( site.find_member( member_id ) != nullptr );
        CHECK( site.find_member( member_id )->state ==
               bandit_live_world::member_state::at_home );
    }
    const std::string after_apply = serialize_world( world );
    CHECK( after_apply.find( "\"has_rally\": true" ) != std::string::npos );
    CHECK_FALSE( bandit_live_world::apply_hostile_operation_plan( site, plan ) );
    CHECK( serialize_world( world ) == after_apply );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    const bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.schema_version == 5 );
    CHECK( loaded_site.active_hostile_operation.is_active() );
    CHECK( loaded_site.active_hostile_operation.operation_kind ==
           hostile_operation_kind::shakedown );
    CHECK( loaded_site.active_hostile_operation.phase ==
           hostile_operation_phase::assembling );
    CHECK( loaded_site.active_hostile_operation.has_rally );
    CHECK( loaded_site.active_hostile_operation.rally_omt == rally );
    CHECK( loaded_site.active_hostile_operation.reservation.member_ids == members );
    CHECK( loaded_site.active_hostile_operation.reservation.shared_route == route );
    CHECK( loaded_site.active_hostile_operation.reservation.owner ==
           bandit_live_world::simulation_owner::abstract );
    CHECK( loaded_site.active_external_outing() ==
           &loaded_site.active_hostile_operation.reservation );

    bandit_live_world::world_state malformed_party = world;
    malformed_party.sites.front().active_hostile_operation.reservation.member_ids.resize( 1 );
    malformed_party.sites.front().active_hostile_operation.reservation.leader_id =
        malformed_party.sites.front().active_hostile_operation.reservation.member_ids.front();
    const bandit_live_world::world_state repaired_party = round_trip_world( malformed_party );
    REQUIRE( repaired_party.sites.size() == 1 );
    CHECK_FALSE( repaired_party.sites.front().active_hostile_operation.is_active() );
    CHECK( repaired_party.sites.front().active_hostile_operation.reservation.member_ids.empty() );

    bandit_live_world::world_state broken_reserve = world;
    for( int member_id = 45203; member_id <= 45206; ++member_id ) {
        broken_reserve.sites.front().find_member( character_id( member_id ) )->wounded_or_unready = true;
    }
    const bandit_live_world::world_state repaired_reserve = round_trip_world( broken_reserve );
    REQUIRE( repaired_reserve.sites.size() == 1 );
    CHECK_FALSE( repaired_reserve.sites.front().active_hostile_operation.is_active() );
    CHECK( repaired_reserve.sites.front().active_hostile_operation.reservation.member_ids.empty() );

    bandit_live_world::world_state cannibal_world;
    for( int index = 0; index < 5; ++index ) {
        add_cannibal_camp_member( cannibal_world, index, 45300 );
    }
    bandit_live_world::site_record &cannibal_site = cannibal_world.sites.front();
    const tripoint_abs_omt cannibal_rally( 72, 80, 0 );
    const tripoint_abs_omt cannibal_target( 75, 80, 0 );
    const std::vector<character_id> cannibal_members = {
        character_id( 45300 ), character_id( 45301 )
    };
    prepare_hostile_follow_on( cannibal_site, 3, 2, "night-target",
                               cannibal_target, 700 );
    CHECK_FALSE( bandit_live_world::plan_hostile_operation(
                     cannibal_site, hostile_operation_kind::shakedown, cannibal_members,
                     { cannibal_site.anchor, cannibal_rally, cannibal_target },
                     cannibal_rally, 702 ).valid );
    const bandit_live_world::hostile_operation_plan raid_plan =
        bandit_live_world::plan_hostile_operation(
            cannibal_site, hostile_operation_kind::raid, cannibal_members,
            { cannibal_site.anchor, cannibal_rally, cannibal_target },
            cannibal_rally, 702 );
    REQUIRE( raid_plan.valid );
    CHECK( raid_plan.operation.reservation.job_type == "raid" );
}

TEST_CASE( "bandit_live_world_hostile_operation_phases_are_one_way_and_atomic",
           "[bandit][live_world][hostile_operation][phase][cas]" )
{
    using bandit_live_world::hostile_operation_phase;
    using bandit_live_world::hostile_operation_transition_result;

    const std::vector<hostile_operation_phase> phases = {
        hostile_operation_phase::assembling,
        hostile_operation_phase::outbound,
        hostile_operation_phase::rallying,
        hostile_operation_phase::waiting_night,
        hostile_operation_phase::approaching,
        hostile_operation_phase::committed_contact,
        hostile_operation_phase::returning_home,
        hostile_operation_phase::lost,
    };
    const std::vector<std::pair<hostile_operation_phase, hostile_operation_phase>> forward_edges = {
        { hostile_operation_phase::assembling, hostile_operation_phase::outbound },
        { hostile_operation_phase::outbound, hostile_operation_phase::rallying },
        { hostile_operation_phase::outbound, hostile_operation_phase::returning_home },
        { hostile_operation_phase::rallying, hostile_operation_phase::waiting_night },
        { hostile_operation_phase::rallying, hostile_operation_phase::approaching },
        { hostile_operation_phase::rallying, hostile_operation_phase::returning_home },
        { hostile_operation_phase::waiting_night, hostile_operation_phase::approaching },
        { hostile_operation_phase::waiting_night, hostile_operation_phase::returning_home },
        { hostile_operation_phase::approaching, hostile_operation_phase::committed_contact },
        { hostile_operation_phase::approaching, hostile_operation_phase::returning_home },
        { hostile_operation_phase::committed_contact, hostile_operation_phase::returning_home },
    };
    for( const hostile_operation_phase previous : phases ) {
        for( const hostile_operation_phase next : phases ) {
            const bool same = previous == next;
            const bool lost = next == hostile_operation_phase::lost &&
                              previous != hostile_operation_phase::lost;
            const bool forward = std::find( forward_edges.begin(), forward_edges.end(),
                                            std::make_pair( previous, next ) ) !=
                                 forward_edges.end();
            CHECK( bandit_live_world::is_valid_hostile_operation_phase_transition(
                       previous, next ) == ( same || lost || forward ) );
        }
    }

    for( const hostile_operation_phase phase : phases ) {
        bandit_live_world::hostile_operation_state saved;
        saved.operation_kind = bandit_live_world::hostile_operation_kind::shakedown;
        saved.phase = phase;
        saved.reservation.kind = bandit_live_world::outing_kind::hostile_operation;
        saved.reservation.activity_id = "phase-round-trip";
        saved.reservation.generation = 1;
        std::ostringstream out;
        JsonOut jsout( out, true );
        saved.serialize( jsout );
        JsonValue jsin = json_loader::from_string( out.str() );
        bandit_live_world::hostile_operation_state loaded;
        loaded.deserialize( jsin.get_object() );
        CHECK( loaded.phase == phase );
    }

    bandit_live_world::world_state world;
    for( int index = 0; index < 6; ++index ) {
        add_bandit_camp_member( world, index, 45400 );
    }
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt rally( 14, 20, 0 );
    const tripoint_abs_omt target( 18, 20, 0 );
    const std::vector<character_id> members = {
        character_id( 45400 ), character_id( 45401 )
    };
    prepare_hostile_follow_on( site, 5, 4, "phase-target", target, 800 );
    const bandit_live_world::hostile_operation_plan plan =
        bandit_live_world::plan_hostile_operation(
            site, bandit_live_world::hostile_operation_kind::shakedown, members,
            { site.anchor, rally, target }, rally, 802 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_hostile_operation_plan( site, plan ) );

    const std::string assembled = serialize_world( world );
    const bandit_live_world::simulation_advance_cursor assembled_cursor =
        require_current_simulation_cursor( site );
    bandit_live_world::simulation_advance_cursor wrong_identity = assembled_cursor;
    wrong_identity.activity_id += ":stale";
    CHECK( bandit_live_world::transition_hostile_operation_phase(
               site, wrong_identity,
               hostile_operation_phase::assembling, hostile_operation_phase::outbound,
               803, "wrong identity" ) == hostile_operation_transition_result::rejected );
    bandit_live_world::simulation_advance_cursor wrong_generation = assembled_cursor;
    wrong_generation.generation++;
    CHECK( bandit_live_world::transition_hostile_operation_phase(
               site, wrong_generation,
               hostile_operation_phase::assembling, hostile_operation_phase::outbound,
               803, "wrong generation" ) == hostile_operation_transition_result::rejected );
    for( int member_id = 45402; member_id <= 45404; ++member_id ) {
        site.find_member( character_id( member_id ) )->wounded_or_unready = true;
    }
    const std::string insufficient_reserve = serialize_world( world );
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::assembling, hostile_operation_phase::outbound,
               803, "reserve vanished" ) == hostile_operation_transition_result::rejected );
    CHECK( serialize_world( world ) == insufficient_reserve );
    for( int member_id = 45402; member_id <= 45404; ++member_id ) {
        site.find_member( character_id( member_id ) )->wounded_or_unready = false;
    }
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::outbound, hostile_operation_phase::rallying,
               803, "wrong expected" ) == hostile_operation_transition_result::rejected );
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::assembling, hostile_operation_phase::outbound,
               801, "stale time" ) == hostile_operation_transition_result::rejected );
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::assembling, hostile_operation_phase::assembling,
               803, "same" ) == hostile_operation_transition_result::unchanged );
    CHECK( serialize_world( world ) == assembled );

    bandit_live_world::world_state lost_before_departure = world;
    bandit_live_world::site_record &lost_site = lost_before_departure.sites.front();
    REQUIRE( transition_test_hostile_operation(
                 lost_site, hostile_operation_phase::assembling,
                 hostile_operation_phase::lost, 803, "lost before departure" ) ==
             hostile_operation_transition_result::applied );
    const bandit_live_world::world_state loaded_lost = round_trip_world( lost_before_departure );
    REQUIRE( loaded_lost.sites.size() == 1 );
    REQUIRE( loaded_lost.sites.front().active_hostile_operation.is_active() );
    CHECK( loaded_lost.sites.front().active_hostile_operation.phase ==
           hostile_operation_phase::lost );
    CHECK( loaded_lost.sites.front().active_hostile_operation.reservation.resolved_member_ids ==
           members );
    for( const character_id &member_id : members ) {
        CHECK( loaded_lost.sites.front().find_member( member_id )->state ==
               bandit_live_world::member_state::at_home );
    }

    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::assembling, hostile_operation_phase::outbound,
                 803, "party departed" ) == hostile_operation_transition_result::applied );
    for( const character_id &member_id : members ) {
        CHECK( site.find_member( member_id )->state ==
               bandit_live_world::member_state::outbound );
    }
    CHECK( site.active_hostile_operation.reservation.last_progress_minutes == 803 );
    CHECK( site.active_hostile_operation.reservation.last_advanced_minutes == 803 );
    CHECK( site.active_hostile_operation.last_transition_reason == "party departed" );
    const std::string departed = serialize_world( world );
    CHECK( bandit_live_world::transition_hostile_operation_phase(
               site, assembled_cursor, hostile_operation_phase::outbound,
               hostile_operation_phase::rallying, 810, "stale pre-departure cursor" ) ==
           hostile_operation_transition_result::rejected );
    CHECK( serialize_world( world ) == departed );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::outbound, hostile_operation_phase::rallying,
                 810, "rally reached" ) == hostile_operation_transition_result::applied );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::rallying, hostile_operation_phase::waiting_night,
                 820, "waiting for night" ) == hostile_operation_transition_result::applied );
    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    CHECK( loaded.sites.front().active_hostile_operation.phase ==
           hostile_operation_phase::waiting_night );
    CHECK( loaded.sites.front().active_hostile_operation.reservation.owner ==
           bandit_live_world::simulation_owner::abstract );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::waiting_night, hostile_operation_phase::approaching,
                 830, "night approach" ) == hostile_operation_transition_result::applied );
    const bandit_live_world::simulation_advance_cursor pre_contact_cursor =
        require_current_simulation_cursor( site );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::approaching,
                 hostile_operation_phase::committed_contact, 840, "contact committed" ) ==
             hostile_operation_transition_result::applied );
    CHECK( site.active_hostile_operation.reservation.owner ==
           bandit_live_world::simulation_owner::local );
    CHECK( site.active_hostile_operation.reservation.handoff_epoch == 1 );
    for( const character_id &member_id : members ) {
        CHECK( site.find_member( member_id )->state ==
               bandit_live_world::member_state::local_contact );
    }
    const std::string committed = serialize_world( world );
    CHECK( bandit_live_world::transition_hostile_operation_phase(
               site, pre_contact_cursor, hostile_operation_phase::committed_contact,
               hostile_operation_phase::returning_home, 850, "stale pre-contact owner" ) ==
           hostile_operation_transition_result::rejected );
    CHECK( serialize_world( world ) == committed );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::committed_contact,
                 hostile_operation_phase::returning_home, 850, "withdraw" ) ==
             hostile_operation_transition_result::applied );
    CHECK( site.active_hostile_operation.reservation.owner ==
           bandit_live_world::simulation_owner::abstract );
    CHECK( site.active_hostile_operation.reservation.handoff_epoch == 2 );
    for( const character_id &member_id : members ) {
        CHECK( site.find_member( member_id )->state ==
               bandit_live_world::member_state::outbound );
    }
    const std::string returning = serialize_world( world );
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::returning_home,
               hostile_operation_phase::approaching, 860, "illegal reversal" ) ==
           hostile_operation_transition_result::rejected );
    CHECK( serialize_world( world ) == returning );
    REQUIRE( transition_test_hostile_operation(
                 site, hostile_operation_phase::returning_home,
                 hostile_operation_phase::lost, 860, "party lost" ) ==
             hostile_operation_transition_result::applied );
    CHECK( transition_test_hostile_operation(
               site, hostile_operation_phase::lost, hostile_operation_phase::lost,
               870, "same terminal" ) == hostile_operation_transition_result::unchanged );

    JsonValue future_phase_json = json_loader::from_string(
        R"({"operation_kind":"shakedown","phase":"future_phase","reservation":{"kind":"hostile_operation","activity_id":"future-op","generation":1}})" );
    bandit_live_world::hostile_operation_state future_phase;
    future_phase.deserialize( future_phase_json.get_object() );
    CHECK( future_phase.phase == hostile_operation_phase::lost );
}

TEST_CASE( "bandit_live_world_caps_scout_routes_observations_and_reservations_on_load",
           "[bandit][live_world][scout_state][save]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45100 );
    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45100 ),
             bandit_live_world::member_state::outbound, "test scout active" ) );
    set_test_active_outing( site, site.site_id + "#scout:caps" );
    site.active_outing.member_ids = { character_id( 45100 ) };
    site.active_outing.leader_id = character_id( 45100 );
    site.active_outing.waypoint_index = 280;
    for( int index = 0; index < 300; ++index ) {
        site.active_outing.shared_route.emplace_back( index, 20, 0 );
    }
    for( int index = 0; index < 20; ++index ) {
        site.active_outing.observations.push_back( { "fact-" + std::to_string( index ),
                                                     "bounded observation", 50, index, false } );
    }
    site.active_outing.observations.push_back( { "critical-after-cap", "burned withdrawal",
                                                 90, 21, true } );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    REQUIRE( loaded.sites.front().active_outing.shared_route.size() == 20 );
    CHECK( loaded.sites.front().active_outing.shared_route.front() == tripoint_abs_omt( 280, 20, 0 ) );
    CHECK( loaded.sites.front().active_outing.shared_route.back() == tripoint_abs_omt( 299, 20, 0 ) );
    CHECK( loaded.sites.front().active_outing.waypoint_index == 0 );
    CHECK( loaded.sites.front().active_outing.observations.size() == 16 );
    CHECK( std::any_of( loaded.sites.front().active_outing.observations.begin(),
                        loaded.sites.front().active_outing.observations.end(),
    []( const bandit_live_world::sortie_observation & observation ) {
        return observation.fact_key == "critical-after-cap" && observation.critical;
    } ) );

    world.sites.front().active_outing.waypoint_index = 0;
    const bandit_live_world::world_state capped_from_start = round_trip_world( world );
    REQUIRE( capped_from_start.sites.front().active_outing.shared_route.size() == 256 );
    CHECK( capped_from_start.sites.front().active_outing.shared_route.front() ==
           tripoint_abs_omt( 0, 20, 0 ) );
    CHECK( capped_from_start.sites.front().active_outing.shared_route.back() ==
           tripoint_abs_omt( 299, 20, 0 ) );
    CHECK( capped_from_start.sites.front().active_outing.waypoint_index == 0 );

    bandit_live_world::world_state oversized;
    REQUIRE( bandit_live_world::register_abstract_site( oversized,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_camp",
             tripoint_abs_omt( 11, 21, 0 ), special_lookup, 17 ) );
    bandit_live_world::site_record &oversized_site = oversized.sites.front();
    set_test_active_outing( oversized_site, oversized_site.site_id + "#scout:oversized" );
    for( int index = 0; index < 17; ++index ) {
        const character_id member_id( 45200 + index );
        oversized_site.members.push_back( { member_id, "bandit", tripoint_abs_ms(),
                                            bandit_live_world::member_state::outbound, false, "" } );
        oversized_site.active_outing.member_ids.push_back( member_id );
    }
    oversized_site.active_outing.leader_id = oversized_site.active_outing.member_ids.front();
    const bandit_live_world::world_state closed = round_trip_world( oversized );
    REQUIRE( closed.sites.size() == 1 );
    CHECK_FALSE( closed.sites.front().active_outing.is_active() );
    CHECK( closed.sites.front().active_outing.member_ids.empty() );
    CHECK( closed.sites.front().count_members_in_state( bandit_live_world::member_state::outbound ) == 0 );
}

TEST_CASE( "bandit_live_world_persists_partial_scout_casualties_without_closing_the_partner",
           "[bandit][live_world][scout_state][save]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45300 );
    add_bandit_camp_member( world, 1, 45300 );
    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45300 ),
             bandit_live_world::member_state::local_contact, "scout pair active" ) );
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45301 ),
             bandit_live_world::member_state::local_contact, "scout pair active" ) );
    set_test_active_outing( site, site.site_id + "#scout:partial-casualty" );
    site.active_outing.member_ids = { character_id( 45300 ), character_id( 45301 ) };
    site.active_outing.leader_id = character_id( 45300 );
    site.active_outing.job_type = "scout";
    REQUIRE( bandit_live_world::note_active_sortie_started(
                 site, require_current_simulation_cursor( site ), 100 ) );
    REQUIRE( bandit_live_world::note_active_sortie_local_contact(
                 site, require_current_simulation_cursor( site ), character_id( 45300 ), 100 ) );
    CHECK( site.active_outing.expected_return_minutes == 940 );
    CHECK( site.active_outing.missing_deadline_minutes == 2380 );

    REQUIRE( bandit_live_world::record_active_outing_casualty(
                 site, require_current_simulation_cursor( site ), character_id( 45301 ),
                 bandit_live_world::member_state::dead, 400, "observer killed" ) );
    CHECK( site.active_outing.is_active() );
    CHECK( site.active_outing.casualty_ids ==
           std::vector<character_id> { character_id( 45301 ) } );
    CHECK( site.active_outing.resolved_member_ids ==
           std::vector<character_id> { character_id( 45301 ) } );
    bandit_pursuit_handoff::return_packet stale_pre_casualty_packet;
    stale_pre_casualty_packet.valid = true;
    stale_pre_casualty_packet.group_id = site.active_outing.activity_id;
    stale_pre_casualty_packet.source_camp_id = site.site_id;
    stale_pre_casualty_packet.activity_generation = site.active_outing.generation;
    stale_pre_casualty_packet.handoff_epoch = site.active_outing.handoff_epoch;
    stale_pre_casualty_packet.return_application_key = site.active_outing.return_application_key;
    stale_pre_casualty_packet.job_type = bandit_dry_run::job_template::scout;
    stale_pre_casualty_packet.survivors_remaining = 2;
    const std::string before_stale_return = serialize_world( world );
    CHECK_FALSE( bandit_live_world::apply_return_packet( site, stale_pre_casualty_packet ) );
    CHECK( serialize_world( world ) == before_stale_return );
    bandit_pursuit_handoff::return_packet contradictory_casualty_packet =
        stale_pre_casualty_packet;
    contradictory_casualty_packet.anchored_identity_updates = { { "45301", "missing" } };
    contradictory_casualty_packet.survivors_remaining = 1;
    CHECK_FALSE( bandit_live_world::apply_return_packet( site, contradictory_casualty_packet ) );
    CHECK( serialize_world( world ) == before_stale_return );
    bandit_live_world::local_gate_input gate_input;
    gate_input.local_threat = 1;
    gate_input.local_opportunity = 2;
    const bandit_live_world::local_gate_decision gate =
        bandit_live_world::choose_local_gate_posture( site, gate_input );
    CHECK( gate.dispatch_strength == 1 );

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    const bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.active_outing.is_active() );
    CHECK( loaded_site.active_outing.member_ids.size() == 2 );
    CHECK( loaded_site.active_outing.casualty_ids ==
           std::vector<character_id> { character_id( 45301 ) } );
    CHECK( loaded_site.active_outing.resolved_member_ids ==
           std::vector<character_id> { character_id( 45301 ) } );
    CHECK( loaded_site.active_outing.expected_return_minutes == 940 );
    CHECK( loaded_site.active_outing.missing_deadline_minutes == 2380 );
    REQUIRE( loaded_site.find_member( character_id( 45300 ) ) != nullptr );
    REQUIRE( loaded_site.find_member( character_id( 45301 ) ) != nullptr );
    CHECK( loaded_site.find_member( character_id( 45300 ) )->state ==
           bandit_live_world::member_state::local_contact );
    CHECK( loaded_site.find_member( character_id( 45301 ) )->state ==
           bandit_live_world::member_state::dead );

    bandit_live_world::world_state inconsistent = world;
    REQUIRE( inconsistent.sites.front().find_member( character_id( 45301 ) ) != nullptr );
    inconsistent.sites.front().find_member( character_id( 45301 ) )->state =
        bandit_live_world::member_state::local_contact;
    const bandit_live_world::world_state safely_closed = round_trip_world( inconsistent );
    REQUIRE( safely_closed.sites.size() == 1 );
    CHECK_FALSE( safely_closed.sites.front().active_outing.is_active() );
    CHECK( safely_closed.sites.front().active_outing.casualty_ids.empty() );
    CHECK( safely_closed.sites.front().find_member( character_id( 45301 ) )->state ==
           bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_live_world_first_scout_survivor_applies_provisional_receipts_once",
           "[bandit][live_world][scout_state][split_return][replay]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45500 );
    add_bandit_camp_member( world, 1, 45500 );
    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45500 ),
             bandit_live_world::member_state::local_contact, "paired scout active" ) );
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45501 ),
             bandit_live_world::member_state::local_contact, "paired scout active" ) );
    set_test_active_outing( site, site.site_id + "#scout:split" );
    site.active_outing.member_ids = { character_id( 45500 ), character_id( 45501 ) };
    site.active_outing.leader_id = character_id( 45500 );
    site.active_outing.job_type = "scout";
    site.active_outing.target_id = "split-target";
    site.active_outing.target_omt = tripoint_abs_omt( 18, 20, 0 );
    site.active_outing.target_lead_revision = 4;
    site.active_outing.observations = {
        { "shared-visual", "one visible defender", 75, 490, false }
    };
    site.active_outing.cargo = { 3, 60 };
    site.active_outing.started_minutes = 100;
    site.active_outing.owner = bandit_live_world::simulation_owner::local;
    site.active_outing.handoff_epoch = 1;
    site.active_outing.last_advanced_minutes = 100;

    const std::vector<bandit_live_world::active_member_observation> first_arrival = {
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::home,
          "first survivor returned" },
        { character_id( 45501 ), bandit_live_world::active_member_observation_state::returning_home,
          "partner still returning" }
    };
    const bandit_live_world::scout_resolution_effect first_effect =
        bandit_live_world::apply_active_scout_observations(
            site, require_current_simulation_cursor( site ), first_arrival, 500 );
    CHECK( first_effect.valid );
    CHECK( first_effect.changed );
    CHECK_FALSE( first_effect.completed );
    CHECK( first_effect.newly_resolved == 1 );
    CHECK( first_effect.newly_returned == 1 );
    CHECK( first_effect.provisional_report_applied );
    CHECK( first_effect.cargo_credited );
    CHECK( site.active_outing.is_active() );
    CHECK( site.active_outing.member_ids.size() == 2 );
    CHECK( site.active_outing.resolved_member_ids ==
           std::vector<character_id> { character_id( 45500 ) } );
    CHECK( site.find_member( character_id( 45500 ) )->state ==
           bandit_live_world::member_state::at_home );
    CHECK( site.find_member( character_id( 45501 ) )->state ==
           bandit_live_world::member_state::local_contact );
    REQUIRE( site.current_scout_report.is_present() );
    CHECK( site.current_scout_report.provisional );
    CHECK( site.current_scout_report.revision == 1 );
    CHECK( site.current_scout_report.source_generation == site.active_outing.generation );
    CHECK( site.current_scout_report.application_key.find( ":members:45500" ) !=
           std::string::npos );
    CHECK( site.returned_cargo_stock.supply_units == 3 );
    CHECK( site.returned_cargo_stock.trade_value == 60 );
    CHECK( site.active_outing.cargo.supply_units == 0 );
    CHECK( site.active_outing.cargo.trade_value == 0 );
    CHECK( site.applied_return_generation == 0 );
    CHECK( site.applied_report_generation == 0 );
    CHECK( site.applied_cargo_generation == 0 );
    CHECK( site.camp_decision.state == bandit_live_world::camp_decision_state::idle );
    CHECK_FALSE( site.camp_decision.has_pinned_report() );

    const bandit_live_world::dispatch_plan blocked_dispatch =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 19, 20, 0 ),
                "new-target-before-partner-resolves" );
    CHECK_FALSE( blocked_dispatch.valid );

    const std::string before_replay = serialize_world( world );
    const bandit_live_world::scout_resolution_effect replay_effect =
        bandit_live_world::apply_active_scout_observations(
            site, require_current_simulation_cursor( site ), first_arrival, 510 );
    CHECK( replay_effect.valid );
    CHECK_FALSE( replay_effect.changed );
    CHECK( serialize_world( world ) == before_replay );

    const std::vector<bandit_live_world::active_member_observation> contradictory_replay = {
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::dead,
          "contradictory stale state" },
        { character_id( 45501 ), bandit_live_world::active_member_observation_state::returning_home,
          "partner still returning" }
    };
    CHECK_FALSE( bandit_live_world::apply_active_scout_observations(
                     site, require_current_simulation_cursor( site ),
                     contradictory_replay, 520 ).valid );
    CHECK( serialize_world( world ) == before_replay );

    const std::vector<bandit_live_world::active_member_observation> duplicate_member = {
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::home,
          "first duplicate observation" },
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::home,
          "second duplicate observation" }
    };
    const bandit_live_world::scout_resolution_effect duplicate_effect =
        bandit_live_world::apply_active_scout_observations(
            site, require_current_simulation_cursor( site ), duplicate_member, 520 );
    CHECK_FALSE( duplicate_effect.valid );
    CHECK( duplicate_effect.newly_resolved == 0 );
    CHECK( serialize_world( world ) == before_replay );

    const std::vector<bandit_live_world::active_member_observation> unknown_member = {
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::home,
          "first survivor already home" },
        { character_id( 49999 ), bandit_live_world::active_member_observation_state::home,
          "unknown member" }
    };
    const bandit_live_world::scout_resolution_effect unknown_effect =
        bandit_live_world::apply_active_scout_observations(
            site, require_current_simulation_cursor( site ), unknown_member, 520 );
    CHECK_FALSE( unknown_effect.valid );
    CHECK( unknown_effect.newly_resolved == 0 );
    CHECK( serialize_world( world ) == before_replay );

    bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.active_outing.is_active() );
    CHECK( loaded_site.active_outing.resolved_member_ids ==
           std::vector<character_id> { character_id( 45500 ) } );
    REQUIRE( loaded_site.current_scout_report.is_present() );
    CHECK( loaded_site.current_scout_report.schema_version == 3 );
    CHECK( loaded_site.current_scout_report.provisional );
    CHECK( loaded_site.applied_report_generation == 0 );
    CHECK( loaded_site.returned_cargo_stock.supply_units == 3 );
    CHECK( loaded_site.camp_decision.state == bandit_live_world::camp_decision_state::idle );
    const std::string loaded_before_replay = serialize_world( loaded );
    CHECK_FALSE( bandit_live_world::apply_active_scout_observations(
                     loaded_site, require_current_simulation_cursor( loaded_site ),
                     first_arrival, 530 ).changed );
    CHECK( serialize_world( loaded ) == loaded_before_replay );

    loaded_site.active_outing.cargo = { 1, 20 };
    const std::vector<bandit_live_world::active_member_observation> final_arrival = {
        { character_id( 45500 ), bandit_live_world::active_member_observation_state::home,
          "first survivor already home" },
        { character_id( 45501 ), bandit_live_world::active_member_observation_state::home,
          "second survivor returned" }
    };
    const bandit_live_world::scout_resolution_effect final_effect =
        bandit_live_world::apply_active_scout_observations(
            loaded_site, require_current_simulation_cursor( loaded_site ), final_arrival, 600 );
    CHECK( final_effect.valid );
    CHECK( final_effect.changed );
    CHECK( final_effect.completed );
    CHECK( final_effect.newly_resolved == 1 );
    CHECK_FALSE( loaded_site.active_outing.is_active() );
    REQUIRE( loaded_site.current_scout_report.is_present() );
    CHECK_FALSE( loaded_site.current_scout_report.provisional );
    CHECK( loaded_site.current_scout_report.revision == 2 );
    CHECK( loaded_site.current_scout_report.application_key.find( ":members" ) ==
           std::string::npos );
    CHECK( loaded_site.returned_cargo_stock.supply_units == 4 );
    CHECK( loaded_site.returned_cargo_stock.trade_value == 80 );
    CHECK( loaded_site.applied_return_generation == 1 );
    CHECK( loaded_site.applied_report_generation == 1 );
    CHECK( loaded_site.applied_cargo_generation == 1 );
    CHECK( loaded_site.camp_decision.state ==
           bandit_live_world::camp_decision_state::report_awaiting_assessment );
    CHECK( loaded_site.camp_decision.source_report_revision == 2 );
    CHECK( loaded_site.camp_decision.source_report_application_key ==
           loaded_site.current_scout_report.application_key );
    CHECK( loaded_site.camp_decision.target_id == "split-target" );
    CHECK( loaded_site.camp_decision.target_lead_revision == 4 );
    CHECK( loaded_site.camp_decision.last_transition_minutes == 600 );
    CHECK( loaded_site.find_member( character_id( 45500 ) )->state ==
           bandit_live_world::member_state::at_home );
    CHECK( loaded_site.find_member( character_id( 45501 ) )->state ==
           bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_live_world_split_scout_repairs_and_terminal_loss_paths_are_bounded",
           "[bandit][live_world][scout_state][split_return][save]" )
{
    bandit_live_world::world_state partial_world;
    add_bandit_camp_member( partial_world, 0, 45600 );
    add_bandit_camp_member( partial_world, 1, 45600 );
    bandit_live_world::site_record &partial_site = partial_world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( partial_site, character_id( 45600 ),
             bandit_live_world::member_state::local_contact, "paired scout active" ) );
    REQUIRE( bandit_live_world::update_member_state( partial_site, character_id( 45601 ),
             bandit_live_world::member_state::local_contact, "paired scout active" ) );
    set_test_active_outing( partial_site, partial_site.site_id + "#scout:deadline" );
    partial_site.active_outing.member_ids = { character_id( 45600 ), character_id( 45601 ) };
    partial_site.active_outing.leader_id = character_id( 45600 );
    partial_site.active_outing.job_type = "scout";
    partial_site.active_outing.target_id = "deadline-target";
    partial_site.active_outing.target_omt = tripoint_abs_omt( 18, 20, 0 );
    partial_site.active_outing.cargo = { 2, 40 };
    partial_site.active_outing.expected_return_minutes = 960;
    partial_site.active_outing.missing_deadline_minutes = 2400;
    partial_site.active_outing.owner = bandit_live_world::simulation_owner::local;
    partial_site.active_outing.handoff_epoch = 1;

    const std::vector<bandit_live_world::active_member_observation> first_arrival = {
        { character_id( 45600 ), bandit_live_world::active_member_observation_state::home,
          "first survivor returned" },
        { character_id( 45601 ), bandit_live_world::active_member_observation_state::unresolved,
          "partner not loaded before deadline" }
    };
    REQUIRE( bandit_live_world::apply_active_scout_observations(
                 partial_site, require_current_simulation_cursor( partial_site ),
                 first_arrival, 500 ).provisional_report_applied );

    const std::vector<bandit_live_world::active_member_observation> premature_missing = {
        { character_id( 45600 ), bandit_live_world::active_member_observation_state::home,
          "first survivor already home" },
        { character_id( 45601 ), bandit_live_world::active_member_observation_state::missing,
          "premature missing declaration" }
    };
    const std::string before_premature_missing = serialize_world( partial_world );
    const bandit_live_world::scout_resolution_effect premature_effect =
        bandit_live_world::apply_active_scout_observations(
            partial_site, require_current_simulation_cursor( partial_site ),
            premature_missing, 2399 );
    CHECK_FALSE( premature_effect.valid );
    CHECK_FALSE( premature_effect.changed );
    CHECK( serialize_world( partial_world ) == before_premature_missing );

    SECTION( "malformed provisional report is cleared without losing the active reservation" ) {
        bandit_live_world::world_state malformed = partial_world;
        malformed.sites.front().current_scout_report.target_id = "forged-target";
        const bandit_live_world::world_state repaired = round_trip_world( malformed );
        REQUIRE( repaired.sites.size() == 1 );
        CHECK( repaired.sites.front().active_outing.is_active() );
        CHECK( repaired.sites.front().active_outing.resolved_member_ids ==
               std::vector<character_id> { character_id( 45600 ) } );
        CHECK_FALSE( repaired.sites.front().current_scout_report.is_present() );
        CHECK( repaired.sites.front().find_member( character_id( 45600 ) )->state ==
               bandit_live_world::member_state::at_home );
        CHECK( repaired.sites.front().find_member( character_id( 45601 ) )->state ==
               bandit_live_world::member_state::local_contact );
    }

    SECTION( "partner declared missing at the fixed deadline finalizes the scout" ) {
        const std::vector<bandit_live_world::active_member_observation> deadline = {
            { character_id( 45600 ), bandit_live_world::active_member_observation_state::home,
              "first survivor already home" },
            { character_id( 45601 ), bandit_live_world::active_member_observation_state::missing,
              "partner unresolved beyond fixed grace" }
        };
        const bandit_live_world::scout_resolution_effect effect =
            bandit_live_world::apply_active_scout_observations(
                partial_site, require_current_simulation_cursor( partial_site ), deadline, 2400 );
        CHECK( effect.valid );
        CHECK( effect.completed );
        CHECK_FALSE( partial_site.active_outing.is_active() );
        CHECK( partial_site.find_member( character_id( 45600 ) )->state ==
               bandit_live_world::member_state::at_home );
        CHECK( partial_site.find_member( character_id( 45601 ) )->state ==
               bandit_live_world::member_state::missing );
        REQUIRE( partial_site.current_scout_report.is_present() );
        CHECK_FALSE( partial_site.current_scout_report.provisional );
        CHECK( partial_site.current_scout_report.casualty_ids ==
               std::vector<character_id> { character_id( 45601 ) } );
        CHECK( partial_site.returned_cargo_stock.supply_units == 2 );
    }

    SECTION( "all casualties close lost without creating a physical report or cargo credit" ) {
        bandit_live_world::world_state lost_world;
        add_bandit_camp_member( lost_world, 0, 45610 );
        add_bandit_camp_member( lost_world, 1, 45610 );
        bandit_live_world::site_record &lost_site = lost_world.sites.front();
        REQUIRE( bandit_live_world::update_member_state( lost_site, character_id( 45610 ),
                 bandit_live_world::member_state::local_contact, "paired scout active" ) );
        REQUIRE( bandit_live_world::update_member_state( lost_site, character_id( 45611 ),
                 bandit_live_world::member_state::local_contact, "paired scout active" ) );
        set_test_active_outing( lost_site, lost_site.site_id + "#scout:lost" );
        lost_site.active_outing.member_ids = { character_id( 45610 ), character_id( 45611 ) };
        lost_site.active_outing.leader_id = character_id( 45610 );
        lost_site.active_outing.job_type = "scout";
        lost_site.active_outing.target_id = "lost-target";
        lost_site.active_outing.cargo = { 5, 100 };
        lost_site.active_outing.expected_return_minutes = 960;
        lost_site.active_outing.missing_deadline_minutes = 2400;
        lost_site.active_outing.owner = bandit_live_world::simulation_owner::local;
        lost_site.active_outing.handoff_epoch = 1;
        const std::vector<bandit_live_world::active_member_observation> lost = {
            { character_id( 45610 ), bandit_live_world::active_member_observation_state::dead,
              "lead scout killed" },
            { character_id( 45611 ), bandit_live_world::active_member_observation_state::missing,
              "escort never returned" }
        };
        const bandit_live_world::scout_resolution_effect effect =
            bandit_live_world::apply_active_scout_observations(
                lost_site, require_current_simulation_cursor( lost_site ), lost, 2400 );
        CHECK( effect.valid );
        CHECK( effect.completed );
        CHECK_FALSE( lost_site.active_outing.is_active() );
        CHECK_FALSE( lost_site.current_scout_report.is_present() );
        CHECK( lost_site.returned_cargo_stock.supply_units == 0 );
        CHECK( lost_site.returned_cargo_stock.trade_value == 0 );
        CHECK( lost_site.applied_return_generation == 1 );
        CHECK( lost_site.applied_report_generation == 1 );
        CHECK( lost_site.applied_cargo_generation == 1 );
        CHECK( lost_site.camp_decision.state ==
               bandit_live_world::camp_decision_state::idle );
        CHECK_FALSE( lost_site.camp_decision.has_pinned_report() );
    }
}

TEST_CASE( "bandit_live_world_atomically_rehomes_scout_report_and_cargo_before_closing",
           "[bandit][live_world][scout_state][handoff][replay]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45400 );
    add_bandit_camp_member( world, 1, 45400 );
    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan = bandit_live_world::plan_site_dispatch(
                site, tripoint_abs_omt( 18, 20, 0 ), "target-for-return-receipt" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    REQUIRE( site.active_outing.member_ids.size() == 1 );
    const character_id scout_id = site.active_outing.member_ids.front();
    site.active_outing.observations.push_back( { "visual-window-1", "one visible defender",
                                                 70, 500, false } );
    site.active_outing.cargo.supply_units = 4;
    site.active_outing.cargo.trade_value = 120;
    site.active_outing.started_minutes = 450;
    site.active_outing.last_progress_minutes = 500;
    site.active_outing.last_advanced_minutes = 510;

    const std::vector<bandit_live_world::active_member_observation> home = {
        { scout_id, bandit_live_world::active_member_observation_state::home, "returned home" }
    };
    const std::optional<bandit_pursuit_handoff::return_packet> packet =
        bandit_live_world::resolve_active_group_aftermath( site, home );
    REQUIRE( packet.has_value() );
    REQUIRE( bandit_live_world::apply_return_packet( site, *packet ) );
    CHECK_FALSE( site.active_outing.is_active() );
    CHECK( site.applied_return_generation == 1 );
    CHECK( site.applied_report_generation == 1 );
    CHECK( site.applied_cargo_generation == 1 );
    REQUIRE( site.current_scout_report.is_present() );
    CHECK( site.current_scout_report.revision == 1 );
    CHECK( site.current_scout_report.source_generation == 1 );
    CHECK( site.current_scout_report.target_id == "target-for-return-receipt" );
    CHECK( site.current_scout_report.target_omt == tripoint_abs_omt( 18, 20, 0 ) );
    CHECK( site.current_scout_report.target_lead_revision == 0 );
    REQUIRE( site.current_scout_report.observations.size() == 1 );
    CHECK( site.current_scout_report.observations.front().fact_key == "visual-window-1" );
    CHECK( site.current_scout_report.delivered_minutes == 510 );
    CHECK( site.returned_cargo_stock.supply_units == 4 );
    CHECK( site.returned_cargo_stock.trade_value == 120 );
    CHECK_FALSE( site.last_cargo_application_key.empty() );

    bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    bandit_live_world::site_record &loaded_site = loaded.sites.front();
    REQUIRE( loaded_site.current_scout_report.is_present() );
    CHECK( loaded_site.current_scout_report.observations.front().fact_key == "visual-window-1" );
    CHECK( loaded_site.current_scout_report.target_id == "target-for-return-receipt" );
    CHECK( loaded_site.returned_cargo_stock.supply_units == 4 );
    CHECK( loaded_site.returned_cargo_stock.trade_value == 120 );
    const std::string before_replay = serialize_world( loaded );
    CHECK_FALSE( bandit_live_world::apply_return_packet( loaded_site, *packet ) );
    CHECK( serialize_world( loaded ) == before_replay );
}

TEST_CASE( "bandit_live_world_keeps_several_hostile_sites_independent_across_save_and_writeback",
           "[bandit][live_world][multi_site]" )
{
    bandit_live_world::world_state original;
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit", character_id( 1001 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "thug", character_id( 1002 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit_quartermaster", character_id( 2001 ),
             tripoint_abs_ms( 960, 1200, 0 ), std::string( "bandit_work_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit", character_id( 2002 ),
             tripoint_abs_ms( 984, 1224, 0 ), std::string( "bandit_work_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( original, "bandit", character_id( 3001 ),
             tripoint_abs_ms( 168, 120, 0 ), std::nullopt, std::string( "mx_bandits_block" ),
             special_lookup ) );

    REQUIRE( original.sites.size() == 3 );
    bandit_live_world::site_record &camp =
        *original.find_site( "overmap_special:bandit_camp@10,20,0" );
    bandit_live_world::site_record &work_camp =
        *original.find_site( "overmap_special:bandit_work_camp@40,50,0" );
    bandit_live_world::site_record &roadblock =
        *original.find_site( "map_extra:mx_bandits_block@7,5,0" );

    const bandit_live_world::dispatch_plan camp_plan =
        bandit_live_world::plan_site_dispatch( camp, tripoint_abs_omt( 18, 20, 0 ),
                                               "player@18,20,0" );
    REQUIRE( camp_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( camp, camp_plan ) );
    camp.remembered_threat_estimate = 7;
    camp.remembered_bounty_estimate = 11;
    camp.remembered_retreat_bias = 2;
    camp.remembered_return_clock = 30;
    camp.remembered_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::ample;
    camp.known_recent_marks = { "west-basecamp-pressure" };

    const bandit_live_world::dispatch_plan work_camp_plan =
        bandit_live_world::plan_site_dispatch( work_camp, tripoint_abs_omt( 48, 50, 0 ),
                                               "player@48,50,0" );
    REQUIRE( work_camp_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( work_camp, work_camp_plan ) );
    work_camp.remembered_threat_estimate = 3;
    work_camp.remembered_bounty_estimate = 5;
    work_camp.remembered_retreat_bias = 1;
    work_camp.remembered_return_clock = 90;
    work_camp.remembered_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::tight;
    work_camp.known_recent_marks = { "east-workcamp-pressure" };

    const bandit_live_world::dispatch_plan roadblock_plan =
        bandit_live_world::plan_site_dispatch( roadblock, tripoint_abs_omt( 8, 5, 0 ),
                                               "player@8,5,0" );
    REQUIRE( roadblock_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( roadblock, roadblock_plan ) );
    roadblock.remembered_threat_estimate = 1;
    roadblock.known_recent_marks = { "roadblock-probe" };

    std::ostringstream out;
    JsonOut jsout( out, true );
    original.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );

    REQUIRE( loaded.sites.size() == 3 );
    bandit_live_world::site_record &loaded_camp =
        *loaded.find_site( "overmap_special:bandit_camp@10,20,0" );
    bandit_live_world::site_record &loaded_work_camp =
        *loaded.find_site( "overmap_special:bandit_work_camp@40,50,0" );
    bandit_live_world::site_record &loaded_roadblock =
        *loaded.find_site( "map_extra:mx_bandits_block@7,5,0" );

    CHECK( loaded_camp.anchor == tripoint_abs_omt( 10, 20, 0 ) );
    CHECK( loaded_camp.headcount == 2 );
    CHECK( loaded_camp.active_outing.activity_id == "overmap_special:bandit_camp@10,20,0#dispatch" );
    CHECK( loaded_camp.active_outing.target_id == "player@18,20,0" );
    REQUIRE( loaded_camp.active_outing.member_ids == std::vector<character_id>( { character_id( 1001 ) } ) );
    CHECK( loaded_camp.find_member( character_id( 1001 ) )->state ==
           bandit_live_world::member_state::outbound );
    CHECK( loaded_camp.find_member( character_id( 1002 ) )->state ==
           bandit_live_world::member_state::at_home );
    CHECK( loaded_camp.dispatchable_member_capacity() == 0 );
    CHECK( loaded_camp.remembered_threat_estimate == 7 );
    CHECK( loaded_camp.remembered_bounty_estimate == 11 );
    REQUIRE( loaded_camp.known_recent_marks.size() == 1 );
    CHECK( loaded_camp.known_recent_marks.front() == "west-basecamp-pressure" );

    CHECK( loaded_work_camp.anchor == tripoint_abs_omt( 40, 50, 0 ) );
    CHECK( loaded_work_camp.headcount == 2 );
    CHECK( loaded_work_camp.active_outing.activity_id == "overmap_special:bandit_work_camp@40,50,0#dispatch" );
    CHECK( loaded_work_camp.active_outing.target_id == "player@48,50,0" );
    REQUIRE( loaded_work_camp.active_outing.member_ids == std::vector<character_id>( { character_id( 2001 ) } ) );
    CHECK( loaded_work_camp.find_member( character_id( 2001 ) )->state ==
           bandit_live_world::member_state::outbound );
    CHECK( loaded_work_camp.find_member( character_id( 2002 ) )->state ==
           bandit_live_world::member_state::at_home );
    CHECK( loaded_work_camp.dispatchable_member_capacity() == 0 );
    CHECK( loaded_work_camp.remembered_threat_estimate == 3 );
    CHECK( loaded_work_camp.remembered_bounty_estimate == 5 );
    CHECK( loaded_work_camp.remembered_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::tight );
    REQUIRE( loaded_work_camp.known_recent_marks.size() == 1 );
    CHECK( loaded_work_camp.known_recent_marks.front() == "east-workcamp-pressure" );

    CHECK( loaded_roadblock.anchor == tripoint_abs_omt( 7, 5, 0 ) );
    CHECK( loaded_roadblock.headcount == 1 );
    CHECK( loaded_roadblock.active_outing.activity_id == "map_extra:mx_bandits_block@7,5,0#dispatch" );
    CHECK( loaded_roadblock.active_outing.target_id == "player@8,5,0" );
    REQUIRE( loaded_roadblock.active_outing.member_ids == std::vector<character_id>( { character_id( 3001 ) } ) );
    CHECK( loaded_roadblock.remembered_threat_estimate == 1 );
    REQUIRE( loaded_roadblock.known_recent_marks.size() == 1 );
    CHECK( loaded_roadblock.known_recent_marks.front() == "roadblock-probe" );

    bandit_pursuit_handoff::local_outcome camp_loss;
    camp_loss.survivors_remaining = 0;
    camp_loss.anchored_identity_updates = { { "1001", "missing" } };
    camp_loss.result = bandit_pursuit_handoff::mission_result::broken;
    camp_loss.resolution = bandit_pursuit_handoff::lead_resolution::target_lost;
    camp_loss.posture = bandit_pursuit_handoff::return_posture::broken_flee;
    camp_loss.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
    const bandit_pursuit_handoff::return_packet camp_packet =
        bandit_pursuit_handoff::build_return_packet( camp_plan.entry, camp_loss );

    REQUIRE( bandit_live_world::apply_return_packet( loaded_camp, camp_packet ) );
    CHECK( loaded_camp.headcount == 1 );
    CHECK( loaded_camp.active_outing.activity_id.empty() );
    CHECK( loaded_camp.active_outing.target_id.empty() );
    CHECK( loaded_camp.active_outing.member_ids.empty() );
    CHECK( loaded_camp.find_member( character_id( 1001 ) )->state ==
           bandit_live_world::member_state::missing );
    CHECK( loaded_camp.find_spawn_tile( tripoint_abs_ms( 240, 480, 0 ) )->headcount == 0 );
    CHECK( loaded_camp.dispatchable_member_capacity() == 0 );
    CHECK_FALSE( bandit_live_world::plan_site_dispatch( loaded_camp, tripoint_abs_omt( 18, 20, 0 ),
                 "player@18,20,0" ).valid );

    CHECK( loaded_work_camp.headcount == 2 );
    CHECK( loaded_work_camp.active_outing.activity_id == "overmap_special:bandit_work_camp@40,50,0#dispatch" );
    REQUIRE( loaded_work_camp.active_outing.member_ids == std::vector<character_id>( { character_id( 2001 ) } ) );
    CHECK( loaded_work_camp.find_member( character_id( 2001 ) )->state ==
           bandit_live_world::member_state::outbound );
    CHECK( loaded_work_camp.find_member( character_id( 2002 ) )->state ==
           bandit_live_world::member_state::at_home );
    REQUIRE( loaded_work_camp.known_recent_marks.size() == 1 );
    CHECK( loaded_work_camp.known_recent_marks.front() == "east-workcamp-pressure" );

    CHECK( loaded_roadblock.headcount == 1 );
    CHECK( loaded_roadblock.active_outing.activity_id == "map_extra:mx_bandits_block@7,5,0#dispatch" );
    REQUIRE( loaded_roadblock.active_outing.member_ids == std::vector<character_id>( { character_id( 3001 ) } ) );
}

TEST_CASE( "bandit_live_world_builds_a_bounded_scout_dispatch_plan_from_owned_members", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 401 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 402 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 403 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    const bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );

    REQUIRE( plan.valid );
    CHECK( plan.site_id == site.site_id );
    CHECK( plan.target_id == "player_basecamp_nearby" );
    REQUIRE( plan.evaluation.candidates.size() > 1 );
    CHECK( plan.evaluation.candidates[plan.evaluation.winner_index].job ==
           bandit_dry_run::job_template::scout );
    CHECK( plan.entry.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::scout );
    CHECK( plan.entry.group_id == site.site_id + "#dispatch" );
    CHECK( plan.entry.current_target_or_mark == "player_basecamp_nearby" );
    REQUIRE( plan.member_ids.size() == 1 );
    CHECK( plan.group.group_strength == 1 );
    REQUIRE( plan.group.anchored_identities.size() == 1 );
    CHECK( plan.group.anchored_identities.front().id == "401" );
}

TEST_CASE( "bandit_live_world_applies_a_dispatch_plan_by_marking_the_selected_member_outbound", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 501 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 502 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    REQUIRE( site.find_member( character_id( 501 ) ) != nullptr );
    CHECK( site.find_member( character_id( 501 ) )->state == bandit_live_world::member_state::outbound );
    CHECK( site.find_member( character_id( 501 ) )->last_writeback_summary ==
           "dispatch scout toward player_basecamp_nearby" );
    CHECK( site.find_member( character_id( 502 ) )->state == bandit_live_world::member_state::at_home );
    CHECK( site.count_members_in_state( bandit_live_world::member_state::outbound ) == 1 );

    const bandit_live_world::dispatch_plan second_plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    CHECK_FALSE( second_plan.valid );
    REQUIRE_FALSE( second_plan.notes.empty() );
    CHECK( second_plan.notes.back().find( "active outside group/contact" ) != std::string::npos );
}

TEST_CASE( "bandit_live_world_persists_independent_camp_intelligence_maps",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state original;
    REQUIRE( bandit_live_world::register_abstract_site( original,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_camp",
             tripoint_abs_omt( 11, 21, 0 ), special_lookup,
             bandit_live_world::abstract_roster_seed_for_site_kind(
                 bandit_live_world::owned_site_kind::bandit_camp ) ) );
    REQUIRE( bandit_live_world::register_abstract_site( original,
             bandit_live_world::anchor_source_kind::overmap_special, "bandit_work_camp",
             tripoint_abs_omt( 41, 51, 0 ), special_lookup,
             bandit_live_world::abstract_roster_seed_for_site_kind(
                 bandit_live_world::owned_site_kind::bandit_work_camp ) ) );

    bandit_live_world::site_record &camp =
        *original.find_site( "overmap_special:bandit_camp@10,20,0" );
    bandit_live_world::camp_map_lead camp_lead;
    camp_lead.lead_id = camp.site_id + "#lead:basecamp_activity:west";
    camp_lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    camp_lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    camp_lead.target_id = "player@18,20,0";
    camp_lead.omt = tripoint_abs_omt( 18, 20, 0 );
    camp_lead.source_key = "scout-west";
    camp_lead.source_summary = "scout confirmed west target";
    camp_lead.bounty = 9;
    camp_lead.threat = 2;
    camp_lead.confidence = 4;
    camp_lead.target_alert = true;
    camp.intelligence_map.leads.push_back( camp_lead );

    bandit_live_world::site_record &work_camp =
        *original.find_site( "overmap_special:bandit_work_camp@40,50,0" );
    bandit_live_world::camp_map_lead work_lead;
    work_lead.lead_id = work_camp.site_id + "#lead:human_activity:east";
    work_lead.kind = bandit_live_world::camp_lead_kind::human_activity;
    work_lead.status = bandit_live_world::camp_lead_status::suspected;
    work_lead.target_id = "traveler@48,50,0";
    work_lead.omt = tripoint_abs_omt( 48, 50, 0 );
    work_lead.source_key = "smoke-east";
    work_lead.source_summary = "smoke mark suspected east target";
    work_lead.bounty = 3;
    work_lead.threat = 6;
    work_lead.confidence = 1;
    work_lead.scout_seen = false;
    work_camp.intelligence_map.leads.push_back( work_lead );

    std::ostringstream out;
    JsonOut jsout( out, true );
    original.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );

    const bandit_live_world::site_record &loaded_camp =
        *loaded.find_site( "overmap_special:bandit_camp@10,20,0" );
    const bandit_live_world::site_record &loaded_work_camp =
        *loaded.find_site( "overmap_special:bandit_work_camp@40,50,0" );

    REQUIRE( loaded_camp.intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead &loaded_camp_lead =
        loaded_camp.intelligence_map.leads.front();
    CHECK( loaded_camp_lead.lead_id == camp_lead.lead_id );
    CHECK( loaded_camp_lead.kind == bandit_live_world::camp_lead_kind::basecamp_activity );
    CHECK( loaded_camp_lead.status == bandit_live_world::camp_lead_status::scout_confirmed );
    CHECK( loaded_camp_lead.target_id == "player@18,20,0" );
    CHECK( loaded_camp_lead.omt == tripoint_abs_omt( 18, 20, 0 ) );
    CHECK( loaded_camp_lead.bounty == 9 );
    CHECK( loaded_camp_lead.threat == 2 );
    CHECK( loaded_camp_lead.confidence == 4 );
    CHECK( loaded_camp_lead.target_alert );

    REQUIRE( loaded_work_camp.intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead &loaded_work_lead =
        loaded_work_camp.intelligence_map.leads.front();
    CHECK( loaded_work_lead.lead_id == work_lead.lead_id );
    CHECK( loaded_work_lead.kind == bandit_live_world::camp_lead_kind::human_activity );
    CHECK( loaded_work_lead.status == bandit_live_world::camp_lead_status::suspected );
    CHECK( loaded_work_lead.target_id == "traveler@48,50,0" );
    CHECK( loaded_work_lead.omt == tripoint_abs_omt( 48, 50, 0 ) );
    CHECK( loaded_work_lead.bounty == 3 );
    CHECK( loaded_work_lead.threat == 6 );
    CHECK( loaded_work_lead.confidence == 1 );
    CHECK_FALSE( loaded_work_lead.scout_seen );
}

TEST_CASE( "bandit_live_world_scout_return_writes_a_persistent_camp_map_lead",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 551 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 552 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt target_omt( 18, 20, 0 );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, target_omt, "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    site.active_outing.started_minutes = 120;
    site.active_outing.local_contact_minutes = 180;

    bandit_pursuit_handoff::local_outcome outcome;
    outcome.survivors_remaining = 1;
    outcome.result = bandit_pursuit_handoff::mission_result::scouted;
    outcome.resolution = bandit_pursuit_handoff::lead_resolution::still_valid;
    outcome.posture = bandit_pursuit_handoff::return_posture::escape_home;
    outcome.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::ample;
    const bandit_pursuit_handoff::return_packet packet =
        bandit_pursuit_handoff::build_return_packet( plan.entry, outcome );

    REQUIRE( bandit_live_world::apply_return_packet( site, packet ) );

    REQUIRE( site.intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead &lead = site.intelligence_map.leads.front();
    CHECK( lead.kind == bandit_live_world::camp_lead_kind::basecamp_activity );
    CHECK( lead.status == bandit_live_world::camp_lead_status::scout_confirmed );
    CHECK( lead.target_id == "player_basecamp_nearby" );
    CHECK( lead.omt == target_omt );
    CHECK( lead.source_key == plan.entry.group_id );
    CHECK( lead.bounty >= 2 );
    CHECK( lead.threat >= 1 );
    CHECK( lead.confidence >= 2 );
    CHECK( lead.threat_confirmed );
    CHECK_FALSE( lead.target_alert );
    CHECK_FALSE( lead.scout_seen );
    CHECK( lead.last_scouted_minutes == 120 );
    CHECK( lead.last_checked_minutes == 180 );
    CHECK( lead.last_outcome == "scouted" );
    CHECK( site.active_outing.activity_id.empty() );
}

TEST_CASE( "bandit_live_world_migrates_legacy_scalar_memory_only_as_fallback",
           "[bandit][live_world][camp_map]" )
{
    {
        const std::string legacy_json = R"({
            "site_id": "legacy_camp",
            "active_target_id": "legacy-active",
            "remembered_target_or_mark": "legacy-mark",
            "remembered_threat_estimate": 5,
            "remembered_bounty_estimate": 7
        })";
        JsonValue legacy_value = json_loader::from_string( legacy_json );
        bandit_live_world::site_record legacy_site;
        legacy_site.deserialize( legacy_value.get_object() );

        REQUIRE( legacy_site.intelligence_map.leads.size() == 1 );
        const bandit_live_world::camp_map_lead &lead = legacy_site.intelligence_map.leads.front();
        CHECK( lead.kind == bandit_live_world::camp_lead_kind::human_activity );
        CHECK( lead.status == bandit_live_world::camp_lead_status::suspected );
        CHECK( lead.target_id == "legacy-mark" );
        CHECK( lead.bounty == 7 );
        CHECK( lead.threat == 5 );
        CHECK( lead.confidence == 2 );
        CHECK( lead.threat_confirmed );
        CHECK_FALSE( lead.scout_seen );
        CHECK_FALSE( lead.target_alert );
        CHECK( lead.last_outcome == "legacy_memory" );
        CHECK( lead.source_summary == "migrated from legacy remembered_* site memory" );
    }

    {
        const std::string explicit_empty_map_json = R"({
            "site_id": "empty_modern_camp",
            "remembered_target_or_mark": "legacy-mark",
            "remembered_threat_estimate": 9,
            "remembered_bounty_estimate": 9,
            "intelligence_map": {
                "schema_version": 1,
                "leads": []
            }
        })";
        JsonValue explicit_empty_map_value = json_loader::from_string( explicit_empty_map_json );
        bandit_live_world::site_record explicit_empty_map_site;
        explicit_empty_map_site.deserialize( explicit_empty_map_value.get_object() );

        CHECK( explicit_empty_map_site.intelligence_map.leads.empty() );
    }

    {
        const std::string modern_json = R"({
            "site_id": "modern_camp",
            "remembered_target_or_mark": "legacy-mark",
            "remembered_threat_estimate": 9,
            "remembered_bounty_estimate": 9,
            "intelligence_map": {
                "schema_version": 1,
                "leads": [ {
                    "lead_id": "modern_camp#lead:basecamp_activity:confirmed",
                    "kind": "basecamp_activity",
                    "status": "scout_confirmed",
                    "target_id": "confirmed-mark",
                    "bounty": 4,
                    "threat": 1,
                    "confidence": 3,
                    "source_summary": "existing camp map proof"
                } ]
            }
        })";
        JsonValue modern_value = json_loader::from_string( modern_json );
        bandit_live_world::site_record modern_site;
        modern_site.deserialize( modern_value.get_object() );

        REQUIRE( modern_site.intelligence_map.leads.size() == 1 );
        const bandit_live_world::camp_map_lead &lead = modern_site.intelligence_map.leads.front();
        CHECK( lead.lead_id == "modern_camp#lead:basecamp_activity:confirmed" );
        CHECK( lead.kind == bandit_live_world::camp_lead_kind::basecamp_activity );
        CHECK( lead.status == bandit_live_world::camp_lead_status::scout_confirmed );
        CHECK( lead.target_id == "confirmed-mark" );
        CHECK( lead.bounty == 4 );
        CHECK( lead.threat == 1 );
        CHECK( lead.confidence == 3 );
        CHECK( lead.source_summary == "existing camp map proof" );
    }
}

TEST_CASE( "bandit_structural_bounty_classifies_coarse_terrain",
           "[bandit][live_world][structural_bounty]" )
{
    const bandit_live_world::structural_bounty_read forest =
        bandit_live_world::classify_structural_bounty_terrain( "forest_thick" );
    CHECK( forest.eligible );
    CHECK( forest.terrain_class == "forest" );
    CHECK( forest.bounty == 1 );
    CHECK( forest.confidence == 1 );

    const bandit_live_world::structural_bounty_read town =
        bandit_live_world::classify_structural_bounty_terrain( "house_base_north" );
    CHECK( town.eligible );
    CHECK( town.terrain_class == "town" );
    CHECK( town.bounty == 2 );
    CHECK( town.confidence == 1 );
    CHECK( town.latent_threat == 1 );

    const bandit_live_world::structural_bounty_read city =
        bandit_live_world::classify_structural_bounty_terrain( "city_downtown" );
    CHECK( city.eligible );
    CHECK( city.terrain_class == "town" );
    CHECK( city.bounty == 3 );
    CHECK( city.latent_threat == 2 );

    const bandit_live_world::structural_bounty_read open =
        bandit_live_world::classify_structural_bounty_terrain( "field_road" );
    CHECK_FALSE( open.eligible );
    CHECK( open.bounty == 0 );
    CHECK( open.terrain_class == "open" );
}

TEST_CASE( "bandit_structural_bounty_lead_upsert_respects_debounce",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13000 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt forest_omt( 13, 20, 0 );
    const bandit_live_world::structural_bounty_read forest =
        bandit_live_world::classify_structural_bounty_terrain( "forest_water" );
    REQUIRE( forest.eligible );

    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest, 100 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, forest.terrain_class );
    bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    CHECK( lead->kind == bandit_live_world::camp_lead_kind::structural_bounty );
    CHECK( lead->status == bandit_live_world::camp_lead_status::suspected );
    CHECK( lead->bounty == 1 );
    CHECK( lead->threat == 0 );
    CHECK_FALSE( lead->threat_confirmed );

    lead->status = bandit_live_world::camp_lead_status::harvested;
    lead->bounty = 0;
    lead->times_harvested = 1;
    CHECK( bandit_live_world::structural_bounty_memory_suppresses_refresh( site.intelligence_map,
            forest_omt, forest.terrain_class ) );
    CHECK_FALSE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest, 200 ) );
    CHECK( site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::harvested );
    CHECK( site.intelligence_map.find_lead( lead_id )->bounty == 0 );
    CHECK( site.intelligence_map.find_lead( lead_id )->times_harvested == 1 );

    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const bandit_live_world::structural_bounty_read town =
        bandit_live_world::classify_structural_bounty_terrain( "house_base" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town, 300 ) );
    const std::string town_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                              town_omt, town.terrain_class );
    site.intelligence_map.find_lead( town_id )->status = bandit_live_world::camp_lead_status::dangerous;
    CHECK( bandit_live_world::structural_bounty_memory_suppresses_refresh( site.intelligence_map,
            town_omt, town.terrain_class ) );
    CHECK_FALSE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town, 400 ) );
}

TEST_CASE( "bandit_structural_scan_seeds_sparse_leads",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13200 );
    bandit_live_world::site_record &site = world.sites.front();

    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { tripoint_abs_omt( 6, 20, 0 ), "forest_thick" },
        { tripoint_abs_omt( 14, 20, 0 ), "house_base_north" },
        { tripoint_abs_omt( 10, 16, 0 ), "field_road" },
        { tripoint_abs_omt( 10, 24, 0 ), "forest_water" },
    };

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 4,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    } );

    CHECK( result.sites_considered == 1 );
    CHECK( result.candidates_sampled == 4 );
    CHECK( result.budget_used == 4 );
    CHECK( result.leads_seeded == 3 );
    CHECK( site.intelligence_map.known_radius_omt == 8 );
    CHECK( site.intelligence_map.next_near_tick_minutes == 60 );
    CHECK( site.intelligence_map.leads.size() == 3 );

    const std::string forest_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                  tripoint_abs_omt( 6, 20, 0 ), "forest" );
    const bandit_live_world::camp_map_lead *forest = site.intelligence_map.find_lead( forest_id );
    REQUIRE( forest != nullptr );
    CHECK( forest->kind == bandit_live_world::camp_lead_kind::structural_bounty );
    CHECK( forest->status == bandit_live_world::camp_lead_status::suspected );
    CHECK( forest->target_id == "forest" );
    CHECK( forest->bounty == 1 );
    CHECK( forest->threat == 0 );
    CHECK( forest->confidence == 1 );
    CHECK_FALSE( forest->generated_by_this_camp_routine );

    const std::string town_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                tripoint_abs_omt( 14, 20, 0 ), "town" );
    const bandit_live_world::camp_map_lead *town = site.intelligence_map.find_lead( town_id );
    REQUIRE( town != nullptr );
    CHECK( town->bounty == 2 );
    CHECK( town->threat == 0 );
    CHECK( town->source_key == "structural_bounty:town" );
}

TEST_CASE( "bandit_structural_scan_respects_global_budget_cap",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13300 );
    add_bandit_work_camp_member( world, 0, 13400 );

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 5,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );

    REQUIRE( world.sites.size() == 2 );
    CHECK( result.sites_considered == 2 );
    CHECK( result.candidates_sampled == 5 );
    CHECK( result.budget_used == 5 );
    CHECK( result.budget_exhausted );
    CHECK( result.leads_seeded == 5 );
    CHECK( world.sites.front().intelligence_map.leads.size() == 4 );
    CHECK( world.sites.back().intelligence_map.leads.size() == 1 );
}

TEST_CASE( "bandit_structural_scan_respects_per_camp_near_cadence",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13500 );
    bandit_live_world::site_record &site = world.sites.front();

    const bandit_live_world::structural_bounty_scan_result first =
        bandit_live_world::advance_structural_bounty_scan( world, 10, 4,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );
    CHECK( first.candidates_sampled == 4 );
    CHECK( site.intelligence_map.next_near_tick_minutes == 70 );

    const bandit_live_world::structural_bounty_scan_result second =
        bandit_live_world::advance_structural_bounty_scan( world, 20, 4,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );
    CHECK( second.sites_considered == 1 );
    CHECK( second.sites_deferred_by_cadence == 1 );
    CHECK( second.candidates_sampled == 0 );
    CHECK( second.leads_seeded == 0 );
    CHECK( site.intelligence_map.next_near_tick_minutes == 70 );
}

TEST_CASE( "bandit_structural_scan_skips_non_camp_profiles",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_cannibal_camp_member( world, 0, 13600 );
    bandit_live_world::site_record &site = world.sites.front();

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 4,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );

    CHECK( result.sites_considered == 1 );
    CHECK( result.sites_skipped_not_camp == 1 );
    CHECK( result.candidates_sampled == 0 );
    CHECK( site.intelligence_map.leads.empty() );
}

TEST_CASE( "bandit_structural_scan_skips_active_outside_or_no_ready_home_sites",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13700 );
    add_bandit_work_camp_member( world, 0, 13800 );
    bandit_live_world::site_record &active_outside_site = world.sites.front();
    bandit_live_world::site_record &empty_home_site = world.sites.back();
    active_outside_site.active_outing.member_ids.push_back( character_id( 13700 ) );
    empty_home_site.members.clear();
    empty_home_site.headcount = 0;
    empty_home_site.spawn_tiles.clear();

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 8,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );

    CHECK( result.sites_considered == 2 );
    CHECK( result.sites_skipped_active_outside == 1 );
    CHECK( result.sites_skipped_no_ready_home == 1 );
    CHECK( result.candidates_sampled == 0 );
    CHECK( active_outside_site.intelligence_map.leads.empty() );
    CHECK( empty_home_site.intelligence_map.leads.empty() );
}

TEST_CASE( "bandit_structural_scan_skips_retired_empty_sites",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13900 );
    bandit_live_world::site_record &site = world.sites.front();
    site.retired_empty_site = true;

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 4,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );

    CHECK( result.sites_considered == 1 );
    CHECK( result.sites_skipped_retired == 1 );
    CHECK( result.candidates_sampled == 0 );
    CHECK( result.leads_seeded == 0 );
    CHECK( site.intelligence_map.leads.empty() );
    CHECK( site.intelligence_map.next_near_tick_minutes == -1 );
}

TEST_CASE( "bandit_structural_scan_does_not_refresh_harvested_or_dangerous_bounty",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14000 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 10 ) );
    const std::string forest_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                  forest_omt, "forest" );
    site.intelligence_map.find_lead( forest_id )->status =
        bandit_live_world::camp_lead_status::harvested;
    site.intelligence_map.find_lead( forest_id )->bounty = 0;
    site.intelligence_map.find_lead( forest_id )->times_harvested = 1;

    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const bandit_live_world::structural_bounty_read town_read =
        bandit_live_world::classify_structural_bounty_terrain( "house_base" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town_read, 10 ) );
    const std::string town_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                town_omt, "town" );
    site.intelligence_map.find_lead( town_id )->status =
        bandit_live_world::camp_lead_status::dangerous;
    site.intelligence_map.find_lead( town_id )->threat = 5;

    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { forest_omt, "forest" },
        { town_omt, "house_base" },
    };
    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 2,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    } );

    CHECK( result.candidates_sampled == 2 );
    CHECK( result.leads_seeded == 0 );
    CHECK( result.leads_suppressed_by_memory == 2 );
    REQUIRE( site.intelligence_map.find_lead( forest_id ) != nullptr );
    CHECK( site.intelligence_map.find_lead( forest_id )->status ==
           bandit_live_world::camp_lead_status::harvested );
    CHECK( site.intelligence_map.find_lead( forest_id )->bounty == 0 );
    CHECK( site.intelligence_map.find_lead( forest_id )->times_harvested == 1 );
    REQUIRE( site.intelligence_map.find_lead( town_id ) != nullptr );
    CHECK( site.intelligence_map.find_lead( town_id )->status ==
           bandit_live_world::camp_lead_status::dangerous );
    CHECK( site.intelligence_map.find_lead( town_id )->threat == 5 );
}

TEST_CASE( "bandit_structural_scan_suppresses_recently_checked_structural_lead",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14100 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    site.intelligence_map.find_lead( lead_id )->last_checked_minutes = 20;
    site.intelligence_map.find_lead( lead_id )->last_outcome = "recently_checked_low_interest";

    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 30, 1,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::string( "forest" );
    } );

    CHECK( result.candidates_sampled == 1 );
    CHECK( result.leads_seeded == 0 );
    CHECK( result.leads_suppressed_by_memory == 1 );
    REQUIRE( site.intelligence_map.find_lead( lead_id ) != nullptr );
    CHECK( site.intelligence_map.find_lead( lead_id )->last_checked_minutes == 20 );
    CHECK( site.intelligence_map.find_lead( lead_id )->last_outcome == "recently_checked_low_interest" );
}

TEST_CASE( "bandit_structural_scan_does_not_convert_mobile_actor_to_ground_bounty",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14200 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt player_omt( 6, 20, 0 );

    bandit_live_world::camp_map_lead player;
    player.lead_id = "player_seen_in_open_ground";
    player.kind = bandit_live_world::camp_lead_kind::moving_actor;
    player.status = bandit_live_world::camp_lead_status::scout_confirmed;
    player.target_id = "player@6,20,0";
    player.omt = player_omt;
    player.bounty = 9;
    player.confidence = 3;
    site.intelligence_map.leads.push_back( player );

    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { player_omt, "field_road" },
    };
    const bandit_live_world::structural_bounty_scan_result result =
        bandit_live_world::advance_structural_bounty_scan( world, 0, 1,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    } );

    CHECK( result.candidates_sampled == 1 );
    CHECK( result.leads_seeded == 0 );
    REQUIRE( site.intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead *mobile =
        site.intelligence_map.find_lead( "player_seen_in_open_ground" );
    REQUIRE( mobile != nullptr );
    CHECK( mobile->kind == bandit_live_world::camp_lead_kind::moving_actor );
    CHECK( mobile->target_id == "player@6,20,0" );
    CHECK( mobile->bounty == 9 );
}

TEST_CASE( "bandit_structural_bounty_keeps_mobile_actor_separate_from_ground_bounty",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 13100 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt forest_omt( 13, 21, 0 );

    bandit_live_world::camp_map_lead player;
    player.lead_id = "player_seen_in_forest";
    player.kind = bandit_live_world::camp_lead_kind::moving_actor;
    player.status = bandit_live_world::camp_lead_status::scout_confirmed;
    player.target_id = "player@13,21,0";
    player.omt = forest_omt;
    player.bounty = 9;
    player.confidence = 3;
    site.intelligence_map.leads.push_back( player );

    const bandit_live_world::structural_bounty_read forest =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest, 500 ) );

    const std::string structural_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                       forest_omt, forest.terrain_class );
    const bandit_live_world::camp_map_lead *structural =
        site.intelligence_map.find_lead( structural_id );
    REQUIRE( structural != nullptr );
    CHECK( structural->kind == bandit_live_world::camp_lead_kind::structural_bounty );
    CHECK( structural->target_id == "forest" );
    CHECK( structural->bounty == 1 );
    CHECK( structural->confidence == 1 );

    const bandit_live_world::camp_map_lead *mobile =
        site.intelligence_map.find_lead( "player_seen_in_forest" );
    REQUIRE( mobile != nullptr );
    CHECK( mobile->kind == bandit_live_world::camp_lead_kind::moving_actor );
    CHECK( mobile->target_id == "player@13,21,0" );
    CHECK( mobile->bounty == 9 );
    CHECK( site.intelligence_map.leads.size() == 2 );
}

TEST_CASE( "bandit_structural_outing_planner_selects_forest_and_town_jobs",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14300 );
    add_bandit_camp_member( world, 1, 14300 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string forest_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                  forest_omt, "forest" );
    const bandit_live_world::camp_map_lead *forest = site.intelligence_map.find_lead( forest_id );
    REQUIRE( forest != nullptr );

    const bandit_live_world::structural_outing_plan forest_plan =
        bandit_live_world::plan_structural_bounty_outing( site, *forest, 100 );
    REQUIRE( forest_plan.valid );
    CHECK( forest_plan.job == bandit_dry_run::job_template::scavenge );
    CHECK( forest_plan.member_ids.size() == 1 );
    CHECK( forest_plan.effective_interest == 2 );

    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const bandit_live_world::structural_bounty_read town_read =
        bandit_live_world::classify_structural_bounty_terrain( "house_base" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town_read, 0 ) );

    const bandit_live_world::structural_outing_plan best_plan =
        bandit_live_world::plan_structural_bounty_outing( site, 100 );
    REQUIRE( best_plan.valid );
    CHECK( best_plan.target_omt == town_omt );
    CHECK( best_plan.job == bandit_dry_run::job_template::scout );
    CHECK( best_plan.effective_interest == 3 );
}

TEST_CASE( "bandit_structural_outing_planner_blocks_active_outside_pressure",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14400 );
    add_bandit_camp_member( world, 1, 14400 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );

    const bandit_live_world::structural_outing_plan open_plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( open_plan.valid );

    set_test_active_outing( site, site.site_id + "#dispatch" );
    site.active_outing.member_ids.push_back( character_id( 14400 ) );

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    CHECK_FALSE( plan.valid );
    CHECK_FALSE( bandit_live_world::apply_structural_bounty_outing_plan( site, open_plan, 100 ) );
}

TEST_CASE( "bandit_structural_dispatch_holds_high_known_threat_low_reward",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14800 );
    add_bandit_camp_member( world, 1, 14800 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    lead->threat = 3;
    lead->threat_confirmed = true;

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    CHECK_FALSE( plan.valid );
    CHECK( plan.known_threat == 3 );
    CHECK( plan.effective_interest <= 0 );
}

TEST_CASE( "bandit_structural_outing_reveals_threat_and_turns_back_before_arrival",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14500 );
    add_bandit_camp_member( world, 1, 14500 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const bandit_live_world::structural_bounty_read town_read =
        bandit_live_world::classify_structural_bounty_terrain( "house_base" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                town_omt, "town" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    CHECK( lead->threat == 0 );
    CHECK_FALSE( lead->threat_confirmed );

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );
    CHECK( site.active_outing.target_id == lead_id );
    REQUIRE( site.find_member( character_id( 14500 ) ) != nullptr );
    CHECK( site.find_member( character_id( 14500 ) )->state == bandit_live_world::member_state::outbound );

    const bandit_live_world::structural_outing_result result =
        bandit_live_world::advance_structural_bounty_outings( world, 160,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 4, true, "test threat at stalking distance" };
    } );

    CHECK( result.stalking_checks_processed == 1 );
    CHECK( result.lost_interest_returns == 1 );
    CHECK( result.arrivals_processed == 0 );
    CHECK( result.members_returned == 1 );
    const bandit_live_world::camp_map_lead *updated = site.intelligence_map.find_lead( lead_id );
    REQUIRE( updated != nullptr );
    CHECK( updated->status == bandit_live_world::camp_lead_status::dangerous );
    CHECK( updated->threat == 4 );
    CHECK( updated->threat_confirmed );
    CHECK( updated->bounty == 2 );
    CHECK( updated->last_checked_minutes == 160 );
    CHECK( updated->last_outcome == "threat_revealed_lost_interest" );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.active_outing.target_id.empty() );
    CHECK( site.active_outing.member_ids.empty() );
    CHECK( site.active_outing.started_minutes == -1 );
    CHECK( site.active_outing.local_contact_minutes == -1 );
    CHECK( site.find_member( character_id( 14500 ) )->state == bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_structural_outing_consumes_bounty_on_arrival_after_interest_survives",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14600 );
    add_bandit_camp_member( world, 1, 14600 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );

    const bandit_live_world::structural_outing_result stalk =
        bandit_live_world::advance_structural_bounty_outings( world, 160,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "quiet forest edge" };
    } );
    CHECK( stalk.stalking_checks_processed == 1 );
    CHECK( stalk.arrivals_processed == 0 );
    CHECK( site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::scout_confirmed );
    CHECK( site.intelligence_map.find_lead( lead_id )->bounty == 1 );

    const bandit_live_world::structural_outing_result arrived =
        bandit_live_world::advance_structural_bounty_outings( world, 200,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( arrived.arrivals_processed == 1 );
    CHECK( arrived.members_returned == 1 );
    const bandit_live_world::camp_map_lead *updated = site.intelligence_map.find_lead( lead_id );
    REQUIRE( updated != nullptr );
    CHECK( updated->status == bandit_live_world::camp_lead_status::harvested );
    CHECK( updated->bounty == 0 );
    CHECK( updated->times_harvested == 1 );
    CHECK( updated->last_checked_minutes == 200 );
    CHECK( updated->last_outcome == "harvested_structural_bounty" );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.active_outing.target_id.empty() );
    CHECK( site.active_outing.member_ids.empty() );
    CHECK( site.active_outing.started_minutes == -1 );
    CHECK( site.active_outing.local_contact_minutes == -1 );
    CHECK( site.find_member( character_id( 14600 ) )->state == bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_structural_outing_arrival_is_once_only_after_reload",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14900 );
    add_bandit_camp_member( world, 1, 14900 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );

    bandit_live_world::world_state before_stalk = round_trip_world( world );
    bandit_live_world::site_record &before_site = before_stalk.sites.front();
    CHECK( before_site.active_outing.activity_id == before_site.site_id + "#structural" );
    CHECK( before_site.active_outing.target_id == lead_id );
    CHECK( before_site.active_outing.target_omt == forest_omt );
    CHECK( before_site.active_outing.job_type == "scavenge" );
    CHECK( before_site.active_outing.started_minutes == 100 );
    CHECK( before_site.active_outing.local_contact_minutes == -1 );
    REQUIRE( before_site.find_member( character_id( 14900 ) ) != nullptr );
    CHECK( before_site.find_member( character_id( 14900 ) )->state ==
           bandit_live_world::member_state::outbound );
    REQUIRE( before_site.intelligence_map.find_lead( lead_id ) != nullptr );
    CHECK( before_site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::active );
    CHECK_FALSE( before_site.intelligence_map.find_lead( lead_id )->threat_confirmed );

    const bandit_live_world::structural_outing_result too_early =
        bandit_live_world::advance_structural_bounty_outings( before_stalk, 159,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "should not run before stalking" };
    } );
    CHECK( too_early.stalking_checks_processed == 0 );
    CHECK( too_early.arrivals_processed == 0 );
    CHECK( before_site.active_outing.local_contact_minutes == -1 );

    const bandit_live_world::structural_outing_result stalk =
        bandit_live_world::advance_structural_bounty_outings( before_stalk, 160,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "quiet forest edge after reload" };
    } );
    CHECK( stalk.stalking_checks_processed == 1 );
    CHECK( stalk.arrivals_processed == 0 );
    REQUIRE( before_site.intelligence_map.find_lead( lead_id ) != nullptr );
    CHECK( before_site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::scout_confirmed );
    CHECK( before_site.intelligence_map.find_lead( lead_id )->bounty == 1 );
    CHECK( before_site.intelligence_map.find_lead( lead_id )->times_harvested == 0 );
    CHECK( before_site.active_outing.local_contact_minutes == 160 );

    bandit_live_world::world_state after_stalk = round_trip_world( before_stalk );
    bandit_live_world::site_record &after_site = after_stalk.sites.front();
    REQUIRE( after_site.intelligence_map.find_lead( lead_id ) != nullptr );
    CHECK( after_site.intelligence_map.find_lead( lead_id )->threat_confirmed );
    CHECK( after_site.intelligence_map.find_lead( lead_id )->bounty == 1 );
    CHECK( after_site.active_outing.activity_id == after_site.site_id + "#structural" );
    CHECK( after_site.active_outing.local_contact_minutes == 160 );

    const bandit_live_world::structural_outing_result arrived =
        bandit_live_world::advance_structural_bounty_outings( after_stalk, 200,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( arrived.arrivals_processed == 1 );
    CHECK( arrived.members_returned == 1 );
    const bandit_live_world::camp_map_lead *harvested = after_site.intelligence_map.find_lead( lead_id );
    REQUIRE( harvested != nullptr );
    CHECK( harvested->status == bandit_live_world::camp_lead_status::harvested );
    CHECK( harvested->bounty == 0 );
    CHECK( harvested->times_harvested == 1 );
    CHECK( harvested->last_outcome == "harvested_structural_bounty" );
    CHECK( after_site.active_outing.activity_id.empty() );
    CHECK( after_site.active_outing.member_ids.empty() );
    CHECK( after_site.find_member( character_id( 14900 ) )->state ==
           bandit_live_world::member_state::at_home );

    const bandit_live_world::structural_outing_result repeat =
        bandit_live_world::advance_structural_bounty_outings( after_stalk, 260,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{};
    } );
    CHECK( repeat.active_outings_considered == 0 );
    CHECK( repeat.arrivals_processed == 0 );
    CHECK( after_site.intelligence_map.find_lead( lead_id )->times_harvested == 1 );

    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { forest_omt, "forest" },
    };
    const bandit_live_world::structural_bounty_scan_result scan =
        bandit_live_world::advance_structural_bounty_scan( after_stalk, 0, 1,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    } );
    CHECK( scan.candidates_sampled == 1 );
    CHECK( scan.leads_seeded == 0 );
    CHECK( scan.leads_suppressed_by_memory == 1 );
    CHECK( after_site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::harvested );
    CHECK( after_site.intelligence_map.find_lead( lead_id )->bounty == 0 );
    CHECK( after_site.intelligence_map.find_lead( lead_id )->times_harvested == 1 );
}

TEST_CASE( "bandit_structural_outing_dangerous_turnback_survives_reload_and_blocks_reselection",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 15000 );
    add_bandit_camp_member( world, 1, 15000 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const bandit_live_world::structural_bounty_read town_read =
        bandit_live_world::classify_structural_bounty_terrain( "house_base" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, town_omt, town_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                town_omt, "town" );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 100 );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, 100 ) );

    bandit_live_world::world_state loaded = round_trip_world( world );
    bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.active_outing.local_contact_minutes == -1 );
    CHECK( loaded_site.find_member( character_id( 15000 ) )->state ==
           bandit_live_world::member_state::outbound );

    const bandit_live_world::structural_outing_result turned_back =
        bandit_live_world::advance_structural_bounty_outings( loaded, 160,
    []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 4, true, "dangerous town edge after reload" };
    } );
    CHECK( turned_back.stalking_checks_processed == 1 );
    CHECK( turned_back.lost_interest_returns == 1 );
    CHECK( turned_back.arrivals_processed == 0 );
    CHECK( turned_back.members_returned == 1 );

    bandit_live_world::world_state reloaded_danger = round_trip_world( loaded );
    bandit_live_world::site_record &danger_site = reloaded_danger.sites.front();
    const bandit_live_world::camp_map_lead *danger = danger_site.intelligence_map.find_lead( lead_id );
    REQUIRE( danger != nullptr );
    CHECK( danger->status == bandit_live_world::camp_lead_status::dangerous );
    CHECK( danger->bounty == 2 );
    CHECK( danger->threat == 4 );
    CHECK( danger->threat_confirmed );
    CHECK( danger->last_outcome == "threat_revealed_lost_interest" );
    CHECK( danger_site.active_outing.activity_id.empty() );
    CHECK( danger_site.active_outing.member_ids.empty() );
    CHECK( danger_site.find_member( character_id( 15000 ) )->state ==
           bandit_live_world::member_state::at_home );

    const bandit_live_world::structural_outing_plan blocked =
        bandit_live_world::plan_structural_bounty_outing( danger_site, *danger, 220 );
    CHECK_FALSE( blocked.valid );

    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { town_omt, "house_base" },
    };
    const bandit_live_world::structural_bounty_scan_result scan =
        bandit_live_world::advance_structural_bounty_scan( reloaded_danger, 0, 2,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    } );
    CHECK( scan.candidates_sampled == 2 );
    CHECK( scan.leads_seeded == 0 );
    CHECK( scan.leads_suppressed_by_memory == 1 );
    CHECK( danger_site.intelligence_map.find_lead( lead_id )->status ==
           bandit_live_world::camp_lead_status::dangerous );
    CHECK( danger_site.intelligence_map.find_lead( lead_id )->threat == 4 );
}

TEST_CASE( "bandit_playback_structural_forest_town_progression_500",
           "[bandit][live_world][structural_bounty][playback]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 15100 );
    add_bandit_camp_member( world, 1, 15100 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const tripoint_abs_omt town_omt( 14, 20, 0 );
    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { forest_omt, "forest" },
        { town_omt, "house_base" },
    };

    int scan_candidates = 0;
    int leads_seeded = 0;
    int leads_suppressed = 0;
    int dispatches_planned = 0;
    int stalking_checks = 0;
    int lost_interest_returns = 0;
    int arrivals = 0;
    int members_returned = 0;
    std::vector<std::string> dispatched_leads;

    for( int minute = 0; minute <= 500; ++minute ) {
        const bandit_live_world::structural_outing_result outing =
            bandit_live_world::advance_structural_bounty_outings( world, minute,
        [&town_omt]( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & lead ) {
            if( lead.omt == town_omt ) {
                return bandit_live_world::structural_threat_read{ 4, true, "deterministic town danger" };
            }
            return bandit_live_world::structural_threat_read{ 0, true, "deterministic quiet forest" };
        } );
        stalking_checks += outing.stalking_checks_processed;
        lost_interest_returns += outing.lost_interest_returns;
        arrivals += outing.arrivals_processed;
        members_returned += outing.members_returned;

        if( !site.has_active_outside_pressure() ) {
            const bandit_live_world::structural_outing_plan plan =
                bandit_live_world::plan_structural_bounty_outing( site, minute );
            if( plan.valid ) {
                REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, minute ) );
                dispatches_planned++;
                dispatched_leads.push_back( plan.lead_id );
            }
        }

        const bandit_live_world::structural_bounty_scan_result scan =
            bandit_live_world::advance_structural_bounty_scan( world, minute, 4,
        [&terrain]( const tripoint_abs_omt & omt ) {
            return lookup_test_terrain( terrain, omt );
        } );
        scan_candidates += scan.candidates_sampled;
        leads_seeded += scan.leads_seeded;
        leads_suppressed += scan.leads_suppressed_by_memory;
    }

    const std::string forest_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                  forest_omt, "forest" );
    const std::string town_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                town_omt, "town" );
    const bandit_live_world::camp_map_lead *forest = site.intelligence_map.find_lead( forest_id );
    const bandit_live_world::camp_map_lead *town = site.intelligence_map.find_lead( town_id );
    REQUIRE( forest != nullptr );
    REQUIRE( town != nullptr );

    CHECK( leads_seeded == 2 );
    CHECK( dispatches_planned == 2 );
    CHECK( stalking_checks == 2 );
    CHECK( lost_interest_returns == 1 );
    CHECK( arrivals == 1 );
    CHECK( members_returned == 2 );
    CHECK( forest->status == bandit_live_world::camp_lead_status::harvested );
    CHECK( forest->bounty == 0 );
    CHECK( forest->times_harvested == 1 );
    CHECK( town->status == bandit_live_world::camp_lead_status::dangerous );
    CHECK( town->bounty == 2 );
    CHECK( town->threat == 4 );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.active_outing.member_ids.empty() );
    CHECK( site.find_member( character_id( 15100 ) )->state ==
           bandit_live_world::member_state::at_home );
    CHECK( scan_candidates <= 40 );
    CHECK( leads_suppressed >= 1 );
    REQUIRE( dispatched_leads.size() == 2 );
    CHECK( std::count( dispatched_leads.begin(), dispatched_leads.end(), forest_id ) == 1 );
    CHECK( std::count( dispatched_leads.begin(), dispatched_leads.end(), town_id ) == 1 );
}

TEST_CASE( "bandit_playback_structural_multi_camp_budget_stays_bounded",
           "[bandit][live_world][structural_bounty][playback]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 15200 );
    add_bandit_camp_member( world, 1, 15200 );
    add_bandit_work_camp_member( world, 0, 15300 );
    add_bandit_work_camp_member( world, 1, 15300 );

    int scan_candidates = 0;
    int scan_budget_hits = 0;
    int leads_seeded = 0;
    int leads_suppressed = 0;
    int dispatches_planned = 0;
    int stalking_checks = 0;
    int lost_interest_returns = 0;
    int arrivals = 0;
    int members_returned = 0;
    int max_active_outings = 0;
    std::vector<std::string> dispatched_leads;

    for( int minute = 0; minute <= 500; ++minute ) {
        const bandit_live_world::structural_outing_result outing =
            bandit_live_world::advance_structural_bounty_outings( world, minute,
        []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
            return bandit_live_world::structural_threat_read{ 0, true, "deterministic quiet structural target" };
        } );
        stalking_checks += outing.stalking_checks_processed;
        lost_interest_returns += outing.lost_interest_returns;
        arrivals += outing.arrivals_processed;
        members_returned += outing.members_returned;

        int active_now = 0;
        for( bandit_live_world::site_record &site : world.sites ) {
            if( site.has_active_outside_pressure() ) {
                active_now++;
                continue;
            }
            const bandit_live_world::structural_outing_plan plan =
                bandit_live_world::plan_structural_bounty_outing( site, minute );
            if( plan.valid && minute <= 320 ) {
                REQUIRE( bandit_live_world::apply_structural_bounty_outing_plan( site, plan, minute ) );
                dispatches_planned++;
                active_now++;
                dispatched_leads.push_back( plan.lead_id );
            }
        }
        max_active_outings = std::max( max_active_outings, active_now );

        const bandit_live_world::structural_bounty_scan_result scan =
            bandit_live_world::advance_structural_bounty_scan( world, minute, 8,
        []( const tripoint_abs_omt &omt ) -> std::optional<std::string> {
            if( omt.z() != 0 ) {
                return std::nullopt;
            }
            return std::string( "forest" );
        } );
        scan_candidates += scan.candidates_sampled;
        scan_budget_hits += scan.budget_exhausted ? 1 : 0;
        leads_seeded += scan.leads_seeded;
        leads_suppressed += scan.leads_suppressed_by_memory;
    }

    REQUIRE( world.sites.size() == 2 );
    CHECK( leads_seeded == 16 );
    CHECK( dispatches_planned == 8 );
    CHECK( stalking_checks == 8 );
    CHECK( lost_interest_returns == 0 );
    CHECK( arrivals == 8 );
    CHECK( members_returned == 8 );
    CHECK( max_active_outings == 2 );
    CHECK( scan_budget_hits == 3 );
    CHECK( scan_candidates <= 32 );
    CHECK( leads_suppressed >= 8 );
    REQUIRE( dispatched_leads.size() == 8 );
    for( const std::string &lead_id : dispatched_leads ) {
        CHECK( std::count( dispatched_leads.begin(), dispatched_leads.end(), lead_id ) == 1 );
    }

    for( const bandit_live_world::site_record &site : world.sites ) {
        CHECK( site.active_outing.activity_id.empty() );
        CHECK( site.active_outing.member_ids.empty() );
        int harvested = 0;
        for( const bandit_live_world::camp_map_lead &lead : site.intelligence_map.leads ) {
            if( lead.status == bandit_live_world::camp_lead_status::harvested ) {
                harvested++;
                CHECK( lead.bounty == 0 );
                CHECK( lead.times_harvested == 1 );
                CHECK( lead.last_outcome == "harvested_structural_bounty" );
            }
        }
        CHECK( harvested == 4 );
    }
}

TEST_CASE( "bandit_structural_live_maintenance_seeds_dispatches_and_advances",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 15400 );
    add_bandit_camp_member( world, 1, 15400 );
    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const std::vector<std::pair<tripoint_abs_omt, std::string>> terrain = {
        { forest_omt, "forest" },
    };

    const bandit_live_world::structural_bounty_maintenance_result seeded =
        bandit_live_world::advance_structural_bounty_maintenance( world, 0, 4, 1,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    }, []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "quiet live-maintenance structural target" };
    } );
    CHECK( seeded.scan.candidates_sampled == 4 );
    CHECK( seeded.scan.leads_seeded == 1 );
    CHECK( seeded.dispatch_cap == 1 );
    CHECK( seeded.dispatches_planned == 1 );
    CHECK( seeded.dispatches_applied == 1 );
    CHECK( seeded.outing.active_outings_considered == 0 );
    CHECK( site.active_outing.activity_id == site.site_id + "#structural" );
    CHECK( site.active_outing.target_omt == forest_omt );
    CHECK( site.find_member( character_id( 15400 ) )->state ==
           bandit_live_world::member_state::outbound );
    const std::string seeded_report = bandit_live_world::render_structural_bounty_maintenance_report(
                                          seeded );
    CHECK( seeded_report.find( "leads_seeded=1" ) != std::string::npos );
    CHECK( seeded_report.find( "dispatches_applied=1" ) != std::string::npos );

    const bandit_live_world::structural_bounty_maintenance_result stalked =
        bandit_live_world::advance_structural_bounty_maintenance( world, 60, 4, 1,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    }, []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "quiet live-maintenance structural target" };
    } );
    CHECK( stalked.outing.active_outings_considered == 1 );
    CHECK( stalked.outing.stalking_checks_processed == 1 );
    CHECK( stalked.scan.sites_skipped_active_outside == 1 );
    CHECK( stalked.dispatches_applied == 0 );
    const bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead(
                site.active_outing.target_id );
    REQUIRE( lead != nullptr );
    CHECK( lead->status == bandit_live_world::camp_lead_status::scout_confirmed );
    CHECK( site.active_outing.local_contact_minutes == 60 );

    const std::string lead_id = site.active_outing.target_id;
    const bandit_live_world::structural_bounty_maintenance_result arrived =
        bandit_live_world::advance_structural_bounty_maintenance( world, 100, 4, 1,
    [&terrain]( const tripoint_abs_omt & omt ) {
        return lookup_test_terrain( terrain, omt );
    }, []( const bandit_live_world::site_record &, const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "quiet live-maintenance structural target" };
    } );
    CHECK( arrived.outing.arrivals_processed == 1 );
    CHECK( arrived.outing.members_returned == 1 );
    CHECK( arrived.dispatches_applied == 0 );
    const bandit_live_world::camp_map_lead *harvested = site.intelligence_map.find_lead( lead_id );
    REQUIRE( harvested != nullptr );
    CHECK( harvested->status == bandit_live_world::camp_lead_status::harvested );
    CHECK( harvested->bounty == 0 );
    CHECK( harvested->times_harvested == 1 );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.find_member( character_id( 15400 ) )->state ==
           bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_structural_outing_recent_check_debounce_blocks_reselection",
           "[bandit][live_world][structural_bounty]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 14700 );
    add_bandit_camp_member( world, 1, 14700 );
    bandit_live_world::site_record &site = world.sites.front();

    const tripoint_abs_omt forest_omt( 6, 20, 0 );
    const bandit_live_world::structural_bounty_read forest_read =
        bandit_live_world::classify_structural_bounty_terrain( "forest" );
    REQUIRE( bandit_live_world::upsert_structural_bounty_lead( site, forest_omt, forest_read, 0 ) );
    const std::string lead_id = bandit_live_world::make_structural_bounty_lead_id( site.site_id,
                                forest_omt, "forest" );
    bandit_live_world::camp_map_lead *lead = site.intelligence_map.find_lead( lead_id );
    REQUIRE( lead != nullptr );
    lead->last_checked_minutes = 140;
    lead->last_outcome = "recently_checked_low_interest";

    const bandit_live_world::structural_outing_plan plan =
        bandit_live_world::plan_structural_bounty_outing( site, *lead, 160 );
    CHECK_FALSE( plan.valid );
}

TEST_CASE( "bandit_live_world_keeps_a_home_reserve_for_site_backed_camps", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 601 );

    bandit_live_world::site_record &camp = world.sites.front();
    CHECK( camp.count_live_members() == 1 );
    CHECK( camp.dispatchable_member_capacity() == 0 );

    const bandit_live_world::dispatch_plan blocked_plan =
        bandit_live_world::plan_site_dispatch( camp, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    CHECK_FALSE( blocked_plan.valid );
    REQUIRE_FALSE( blocked_plan.notes.empty() );
    CHECK( blocked_plan.notes.back().find( "home reserve" ) != std::string::npos );

    bandit_live_world::world_state roadside_world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( roadside_world, "bandit", character_id( 602 ),
             tripoint_abs_ms( 120, 96, 0 ), std::nullopt, std::string( "mx_looters" ),
             special_lookup ) );
    const bandit_live_world::site_record &roadside_site = roadside_world.sites.front();
    CHECK( roadside_site.dispatchable_member_capacity() == 1 );
}

TEST_CASE( "bandit_live_world_bandit_camp_reserve_scales_with_living_roster",
           "[bandit][live_world][camp_map]" )
{
    const std::vector<std::pair<int, int>> expected_dispatchable = {
        { 2, 1 },
        { 4, 3 },
        { 5, 3 },
        { 7, 5 },
        { 10, 6 },
    };

    for( const std::pair<int, int> &row : expected_dispatchable ) {
        bandit_live_world::world_state world;
        for( int i = 0; i < row.first; ++i ) {
            add_bandit_camp_member( world, i, 11000 + row.first * 100 );
        }
        REQUIRE( world.sites.size() == 1 );
        const bandit_live_world::site_record &site = world.sites.front();
        CHECK( site.count_live_members() == row.first );
        CHECK( site.dispatchable_member_capacity() == row.second );
    }
}

TEST_CASE( "bandit_live_world_two_bandit_camp_waits_after_scout_confirmation_to_keep_a_reserve",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 2; ++i ) {
        add_bandit_camp_member( world, i, 11950 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.dispatchable_member_capacity() == 1 );

    bandit_live_world::camp_map_lead uncertain;
    uncertain.status = bandit_live_world::camp_lead_status::stale;
    uncertain.bounty = 8;
    uncertain.threat = 1;
    uncertain.confidence = 1;
    const bandit_live_world::camp_map_dispatch_decision uncertain_decision =
        bandit_live_world::choose_camp_map_dispatch( site, uncertain );
    CHECK( uncertain_decision.intent == bandit_dry_run::job_template::scout );
    CHECK( uncertain_decision.hard_home_reserve == 1 );
    CHECK( uncertain_decision.dispatchable == 1 );
    CHECK( uncertain_decision.selected_member_count == 1 );

    bandit_live_world::camp_map_lead confirmed;
    confirmed.lead_id = "confirmed_basecamp@18,20,0";
    confirmed.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    confirmed.status = bandit_live_world::camp_lead_status::scout_confirmed;
    confirmed.target_id = "player@18,20,0";
    confirmed.omt = tripoint_abs_omt( 18, 20, 0 );
    confirmed.bounty = 8;
    confirmed.threat = 1;
    confirmed.confidence = 3;

    const bandit_live_world::camp_map_dispatch_decision confirmed_decision =
        bandit_live_world::choose_camp_map_dispatch( site, confirmed );
    CHECK( confirmed_decision.intent == bandit_dry_run::job_template::stalk );
    CHECK( confirmed_decision.hard_home_reserve == 0 );
    CHECK( confirmed_decision.dispatchable == 2 );
    CHECK( confirmed_decision.selected_member_count == 2 );

    prepare_hostile_follow_on( site, 2, 1, confirmed.target_id, confirmed.omt, 100 );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, confirmed );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::stalk );
    CHECK( plan.member_ids.size() == 2 );

    const bandit_live_world::hostile_operation_plan operation =
        bandit_live_world::plan_hostile_operation(
            site, bandit_live_world::hostile_operation_kind::shakedown, plan.member_ids,
            { site.anchor, confirmed.omt }, site.anchor, 102 );
    CHECK_FALSE( operation.valid );
    CHECK_FALSE( site.active_hostile_operation.is_active() );
    CHECK( site.count_members_in_state( bandit_live_world::member_state::at_home ) == 2 );
}

TEST_CASE( "bandit_live_world_scout_confirmed_basecamp_keeps_small_roster_to_stalk",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 5; ++i ) {
        add_bandit_camp_member( world, i, 11970 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.dispatchable_member_capacity() == 3 );
    site.remembered_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;

    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "confirmed_basecamp@18,20,0";
    lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.target_id = "player@18,20,0";
    lead.omt = tripoint_abs_omt( 18, 20, 0 );
    lead.bounty = 8;
    lead.threat = 1;
    lead.confidence = 3;

    const bandit_live_world::camp_map_dispatch_decision decision =
        bandit_live_world::choose_camp_map_dispatch( site, lead );
    CHECK( decision.intent == bandit_dry_run::job_template::stalk );
    CHECK( decision.hard_home_reserve == 2 );
    CHECK( decision.dispatchable == 3 );
    CHECK( decision.selected_member_count == 2 );
}

TEST_CASE( "bandit_live_world_scout_confirmed_basecamp_promotes_to_toll_party",
           "[bandit][live_world][camp_map][shakedown]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 7; ++i ) {
        add_bandit_camp_member( world, i, 11980 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.dispatchable_member_capacity() == 5 );
    site.remembered_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;

    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "confirmed_basecamp@18,20,0";
    lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.target_id = "player@18,20,0";
    lead.omt = tripoint_abs_omt( 18, 20, 0 );
    lead.bounty = 8;
    lead.threat = 1;
    lead.confidence = 3;

    const bandit_live_world::camp_map_dispatch_decision decision =
        bandit_live_world::choose_camp_map_dispatch( site, lead );
    CHECK( decision.intent == bandit_dry_run::job_template::toll );
    CHECK( decision.hard_home_reserve == 2 );
    CHECK( decision.dispatchable == 5 );
    CHECK( decision.selected_member_count == 3 );

    prepare_hostile_follow_on( site, 2, 1, lead.target_id, lead.omt, 100 );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, lead );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::toll );
    CHECK( plan.member_ids.size() == 3 );

    apply_test_hostile_dispatch( site, plan, 102 );
    CHECK( site.active_hostile_operation.reservation.job_type == "toll" );
    CHECK( site.active_hostile_operation.reservation.member_ids.size() == 3 );
    CHECK( site.count_members_in_state( bandit_live_world::member_state::at_home ) == 4 );

    bandit_live_world::local_gate_input input;
    input.local_threat = 1;
    input.local_opportunity = 2;
    input.local_contact_established = true;
    const bandit_live_world::local_gate_decision gate =
        bandit_live_world::choose_local_gate_posture( site, input );
    CHECK( gate.valid );
    CHECK( gate.posture == bandit_live_world::local_gate_posture::open_shakedown );
    CHECK( gate.shakedown_capable );
    CHECK( gate.opens_shakedown_surface );
    CHECK_FALSE( gate.combat_forward );
}

TEST_CASE( "bandit_live_world_cannibal_scout_confirmation_promotes_to_attack_pack",
           "[bandit][live_world][camp_map][cannibal]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 6; ++i ) {
        add_cannibal_camp_member( world, i, 11990 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    REQUIRE( site.dispatchable_member_capacity() == 4 );

    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "confirmed_basecamp@72,80,0";
    lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.target_id = "player@72,80,0";
    lead.omt = tripoint_abs_omt( 72, 80, 0 );
    lead.bounty = 9;
    lead.threat = 1;
    lead.confidence = 3;

    const bandit_live_world::camp_map_dispatch_decision decision =
        bandit_live_world::choose_camp_map_dispatch( site, lead );
    CHECK( decision.intent == bandit_dry_run::job_template::raid );
    CHECK( decision.hard_home_reserve == 2 );
    CHECK( decision.dispatchable == 4 );
    CHECK( decision.selected_member_count == 4 );

    prepare_hostile_follow_on( site, 2, 1, lead.target_id, lead.omt, 100 );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, lead );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::raid );
    CHECK( plan.member_ids.size() == 4 );

    apply_test_hostile_dispatch( site, plan, 102 );
    CHECK( site.active_hostile_operation.operation_kind ==
           bandit_live_world::hostile_operation_kind::raid );
    CHECK( site.active_hostile_operation.reservation.job_type == "raid" );
    CHECK( site.active_hostile_operation.reservation.member_ids.size() == 4 );
    CHECK( site.count_members_in_state( bandit_live_world::member_state::at_home ) == 2 );

    bandit_live_world::local_gate_input input;
    input.local_threat = 2;
    input.local_opportunity = 1;
    input.local_contact_established = true;
    input.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision gate =
        bandit_live_world::choose_local_gate_posture( site, input );
    CHECK( gate.valid );
    CHECK( gate.posture == bandit_live_world::local_gate_posture::attack_now );
    CHECK( gate.combat_forward );
    CHECK_FALSE( gate.opens_shakedown_surface );
}

TEST_CASE( "bandit_live_world_active_outside_group_blocks_parallel_dispatch",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 5; ++i ) {
        add_bandit_camp_member( world, i, 12000 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.dispatchable_member_capacity() == 3 );
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 12000 ),
             bandit_live_world::member_state::outbound, "already outside" ) );
    site.active_outing.member_ids = { character_id( 12000 ) };
    set_test_active_outing( site, site.site_id + "#already_outside" );

    bandit_live_world::camp_map_lead lead;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.bounty = 8;
    lead.threat = 1;
    lead.confidence = 3;
    const bandit_live_world::camp_map_dispatch_decision decision =
        bandit_live_world::choose_camp_map_dispatch( site, lead );
    CHECK( decision.intent == bandit_dry_run::job_template::hold_chill );
    CHECK( decision.active_outside == 1 );

    const bandit_live_world::dispatch_plan blocked_plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ),
                                               "player_basecamp_nearby" );
    CHECK_FALSE( blocked_plan.valid );
    REQUIRE_FALSE( blocked_plan.notes.empty() );
    CHECK( blocked_plan.notes.front().find( "active outside" ) != std::string::npos );
}

TEST_CASE( "bandit_live_world_wounded_and_killed_members_shrink_camp_map_dispatch",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 7; ++i ) {
        add_bandit_camp_member( world, i, 12100 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.dispatchable_member_capacity() == 5 );
    REQUIRE( site.find_member( character_id( 12101 ) ) != nullptr );
    site.find_member( character_id( 12101 ) )->wounded_or_unready = true;
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 12102 ),
             bandit_live_world::member_state::dead, "killed before the next pressure beat" ) );

    CHECK( site.count_live_members() == 6 );
    CHECK( site.dispatchable_member_capacity() == 3 );

    bandit_live_world::camp_map_lead lead;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.bounty = 6;
    lead.threat = 2;
    lead.confidence = 3;

    const bandit_live_world::camp_map_dispatch_decision decision =
        bandit_live_world::choose_camp_map_dispatch( site, lead );
    CHECK( decision.ready_at_home == 5 );
    CHECK( decision.wounded_or_unready == 1 );
    CHECK( decision.hard_home_reserve == 2 );
    CHECK( decision.dispatchable == 3 );
    CHECK( decision.intent == bandit_dry_run::job_template::stalk );
    CHECK( decision.selected_member_count == 2 );
}

TEST_CASE( "bandit_live_world_camp_map_risk_reward_handles_pressure_and_cooling",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 5; ++i ) {
        add_bandit_camp_member( world, i, 12200 );
    }
    bandit_live_world::site_record &site = world.sites.front();

    bandit_live_world::camp_map_lead dangerous;
    dangerous.status = bandit_live_world::camp_lead_status::scout_confirmed;
    dangerous.bounty = 1;
    dangerous.threat = 6;
    dangerous.confidence = 2;
    const bandit_live_world::camp_map_dispatch_decision dangerous_decision =
        bandit_live_world::choose_camp_map_dispatch( site, dangerous );
    CHECK( dangerous_decision.intent == bandit_dry_run::job_template::hold_chill );
    CHECK( dangerous_decision.selected_member_count == 0 );

    bandit_live_world::camp_map_lead hungry;
    hungry.status = bandit_live_world::camp_lead_status::scout_confirmed;
    hungry.bounty = 2;
    hungry.threat = 2;
    hungry.confidence = 2;
    bandit_live_world::camp_map_dispatch_pressure hungry_pressure;
    hungry_pressure.stockpile_pressure = 3;
    const bandit_live_world::camp_map_dispatch_decision hungry_decision =
        bandit_live_world::choose_camp_map_dispatch( site, hungry, hungry_pressure );
    CHECK( hungry_decision.intent == bandit_dry_run::job_template::stalk );
    CHECK( hungry_decision.hard_home_reserve == 2 );
    CHECK( hungry_decision.dispatchable == 3 );
    CHECK( hungry_decision.selected_member_count == 2 );

    bandit_live_world::camp_map_lead defender_loss;
    defender_loss.status = bandit_live_world::camp_lead_status::scout_confirmed;
    defender_loss.bounty = 1;
    defender_loss.threat = 4;
    defender_loss.confidence = 2;
    const bandit_live_world::camp_map_dispatch_decision before_defender_loss =
        bandit_live_world::choose_camp_map_dispatch( site, defender_loss );
    defender_loss.prior_defender_losses = 2;
    const bandit_live_world::camp_map_dispatch_decision after_defender_loss =
        bandit_live_world::choose_camp_map_dispatch( site, defender_loss );
    CHECK( before_defender_loss.intent == bandit_dry_run::job_template::hold_chill );
    CHECK( after_defender_loss.intent == bandit_dry_run::job_template::stalk );
    CHECK( after_defender_loss.selected_member_count == 2 );

    bandit_live_world::camp_map_lead no_opening;
    no_opening.status = bandit_live_world::camp_lead_status::active;
    no_opening.bounty = 7;
    no_opening.threat = 1;
    no_opening.confidence = 3;
    bandit_live_world::camp_map_dispatch_pressure no_opening_pressure;
    no_opening_pressure.opening_available = false;
    const bandit_live_world::camp_map_dispatch_decision no_opening_decision =
        bandit_live_world::choose_camp_map_dispatch( site, no_opening, no_opening_pressure );
    CHECK( no_opening_decision.intent == bandit_dry_run::job_template::hold_chill );
    CHECK( no_opening_decision.selected_member_count == 0 );
}

TEST_CASE( "bandit_live_world_prior_bandit_losses_cool_large_camp_pressure",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 10; ++i ) {
        add_bandit_camp_member( world, i, 12300 );
    }
    bandit_live_world::site_record &site = world.sites.front();

    bandit_live_world::camp_map_lead lead;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.bounty = 8;
    lead.threat = 2;
    lead.confidence = 3;
    const bandit_live_world::camp_map_dispatch_decision clean_pressure =
        bandit_live_world::choose_camp_map_dispatch( site, lead );

    lead.prior_bandit_losses = 2;
    const bandit_live_world::camp_map_dispatch_decision cooled_pressure =
        bandit_live_world::choose_camp_map_dispatch( site, lead );

    CHECK( clean_pressure.intent == bandit_dry_run::job_template::stalk );
    CHECK( clean_pressure.hard_home_reserve == 4 );
    CHECK( clean_pressure.dispatchable == 6 );
    CHECK( clean_pressure.selected_member_count == 3 );
    CHECK( cooled_pressure.intent == bandit_dry_run::job_template::stalk );
    CHECK( cooled_pressure.hard_home_reserve == 5 );
    CHECK( cooled_pressure.dispatchable == 5 );
    CHECK( cooled_pressure.selected_member_count == 2 );
    CHECK( cooled_pressure.risk_score > clean_pressure.risk_score );
}

TEST_CASE( "bandit_live_world_dispatch_rules_are_driven_by_hostile_site_profile", "[bandit][live_world][profile]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 650 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 651 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 652 ),
             tripoint_abs_ms( 168, 120, 0 ), std::nullopt, std::string( "mx_bandits_block" ),
             special_lookup ) );

    bandit_live_world::site_record &camp =
        *world.find_site( "overmap_special:bandit_camp@10,20,0" );
    bandit_live_world::site_record &roadblock =
        *world.find_site( "map_extra:mx_bandits_block@7,5,0" );

    CHECK( camp.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( roadblock.profile == bandit_live_world::hostile_site_profile::small_hostile_site );
    CHECK( camp.dispatchable_member_capacity() == 1 );
    CHECK( roadblock.dispatchable_member_capacity() == 1 );

    const bandit_live_world::dispatch_plan camp_plan =
        bandit_live_world::plan_site_dispatch( camp, tripoint_abs_omt( 18, 20, 0 ),
                                               "player@18,20,0" );
    REQUIRE( camp_plan.valid );
    CHECK( camp_plan.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( camp_plan.group.retreat_bias == 1 );
    CHECK( camp_plan.group.return_clock == 2 );
    CHECK( camp_plan.group.remaining_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::ample );
    REQUIRE( camp_plan.notes.size() >= 2 );
    CHECK( camp_plan.notes[camp_plan.notes.size() - 2].find( "persistent camp pressure" ) !=
           std::string::npos );

    const bandit_live_world::dispatch_plan roadblock_plan =
        bandit_live_world::plan_site_dispatch( roadblock, tripoint_abs_omt( 8, 5, 0 ),
                                               "player@8,5,0" );
    REQUIRE( roadblock_plan.valid );
    CHECK( roadblock_plan.profile == bandit_live_world::hostile_site_profile::small_hostile_site );
    CHECK( roadblock_plan.group.retreat_bias == 2 );
    CHECK( roadblock_plan.group.return_clock == 1 );
    CHECK( roadblock_plan.group.remaining_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::tight );
    REQUIRE( roadblock_plan.notes.size() >= 2 );
    CHECK( roadblock_plan.notes[roadblock_plan.notes.size() - 2].find( "brittle local pressure" ) !=
           std::string::npos );
}

TEST_CASE( "bandit_live_world_keeps_cannibal_camp_separate_from_bandit_camp_ownership", "[bandit][live_world][profile][cannibal]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 660 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 661 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 760 ),
             tripoint_abs_ms( 1680, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_butcher", character_id( 761 ),
             tripoint_abs_ms( 1681, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_camp_leader", character_id( 762 ),
             tripoint_abs_ms( 1704, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 763 ),
             tripoint_abs_ms( 1705, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );

    REQUIRE( world.sites.size() == 2 );
    bandit_live_world::site_record &bandit_camp =
        *world.find_site( "overmap_special:bandit_camp@10,20,0" );
    bandit_live_world::site_record &cannibal_camp =
        *world.find_site( "overmap_special:cannibal_camp@70,80,0" );

    CHECK( bandit_camp.site_kind == bandit_live_world::owned_site_kind::bandit_camp );
    CHECK( bandit_camp.profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( cannibal_camp.site_kind == bandit_live_world::owned_site_kind::cannibal_camp );
    CHECK( cannibal_camp.profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    CHECK( bandit_camp.dispatchable_member_capacity() == 1 );
    CHECK( cannibal_camp.dispatchable_member_capacity() == 2 );
    REQUIRE( cannibal_camp.footprint.size() == 4 );
    CHECK( cannibal_camp.footprint.front() == tripoint_abs_omt( 70, 80, 0 ) );
    CHECK( cannibal_camp.footprint.back() == tripoint_abs_omt( 71, 81, 0 ) );

    const bandit_live_world::dispatch_plan bandit_plan =
        bandit_live_world::plan_site_dispatch( bandit_camp, tripoint_abs_omt( 18, 20, 0 ),
                                               "player@18,20,0" );
    REQUIRE( bandit_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( bandit_camp, bandit_plan ) );

    prepare_hostile_follow_on( cannibal_camp, 2, 1, "player@72,80,0",
                               tripoint_abs_omt( 72, 80, 0 ), 100 );
    const bandit_live_world::dispatch_plan cannibal_plan =
        bandit_live_world::plan_site_dispatch( cannibal_camp, tripoint_abs_omt( 72, 80, 0 ),
                                               "player@72,80,0" );
    REQUIRE( cannibal_plan.valid );
    CHECK( cannibal_plan.profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    CHECK( cannibal_plan.entry.job_type == bandit_dry_run::job_template::stalk );
    CHECK( cannibal_plan.group.retreat_bias == 3 );
    CHECK( cannibal_plan.group.return_clock == 3 );
    CHECK( cannibal_plan.group.remaining_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::tight );
    REQUIRE( cannibal_plan.notes.size() >= 3 );
    std::string cannibal_plan_notes;
    for( const std::string &note : cannibal_plan.notes ) {
        cannibal_plan_notes += note + "\n";
    }
    CHECK( cannibal_plan_notes.find( "hungry camp pressure" ) != std::string::npos );
    CHECK( cannibal_plan_notes.find( "pack_size 2" ) != std::string::npos );
    apply_test_hostile_dispatch( cannibal_camp, cannibal_plan, 102 );

    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );

    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );

    REQUIRE( loaded.sites.size() == 2 );
    const bandit_live_world::site_record *loaded_bandit =
        loaded.find_site( "overmap_special:bandit_camp@10,20,0" );
    const bandit_live_world::site_record *loaded_cannibal =
        loaded.find_site( "overmap_special:cannibal_camp@70,80,0" );
    REQUIRE( loaded_bandit != nullptr );
    REQUIRE( loaded_cannibal != nullptr );

    CHECK( loaded_bandit->profile == bandit_live_world::hostile_site_profile::camp_style );
    CHECK( loaded_bandit->active_outing.activity_id == "overmap_special:bandit_camp@10,20,0#dispatch" );
    CHECK( loaded_bandit->active_outing.target_id == "player@18,20,0" );
    REQUIRE( loaded_bandit->active_outing.member_ids == std::vector<character_id>( { character_id( 660 ) } ) );
    CHECK( loaded_bandit->find_member( character_id( 661 ) )->state ==
           bandit_live_world::member_state::at_home );

    CHECK( loaded_cannibal->profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    CHECK( loaded_cannibal->active_hostile_operation.reservation.activity_id ==
           "overmap_special:cannibal_camp@70,80,0#hostile:2" );
    CHECK( loaded_cannibal->active_hostile_operation.reservation.target_id == "player@72,80,0" );
    REQUIRE( loaded_cannibal->active_hostile_operation.reservation.member_ids ==
             std::vector<character_id>( { character_id( 760 ), character_id( 761 ) } ) );
    CHECK( loaded_cannibal->find_member( character_id( 762 ) )->state ==
           bandit_live_world::member_state::at_home );
    CHECK( loaded_cannibal->find_member( character_id( 763 ) )->state ==
           bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_live_world_blocks_lone_cannibal_pack_pressure", "[bandit][live_world][profile][cannibal]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 770 ),
             tripoint_abs_ms( 1680, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_butcher", character_id( 771 ),
             tripoint_abs_ms( 1681, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_camp_leader", character_id( 772 ),
             tripoint_abs_ms( 1704, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    CHECK( site.dispatchable_member_capacity() == 1 );

    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 72, 80, 0 ),
                "player@72,80,0" );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::scout );
    REQUIRE( plan.member_ids == std::vector<character_id>( { character_id( 770 ) } ) );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    CHECK( site.active_outing.job_type == "scout" );

    bandit_live_world::local_gate_input input;
    input.local_threat = 1;
    input.local_opportunity = 3;
    input.local_contact_established = true;
    input.darkness_or_concealment = true;
    const bandit_live_world::local_gate_decision decision =
        bandit_live_world::choose_local_gate_posture( site, input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::probe );
    CHECK_FALSE( decision.combat_forward );

    site.active_outing.job_type = "raid";
    const bandit_live_world::local_gate_decision disguised_attack_decision =
        bandit_live_world::choose_local_gate_posture( site, input );
    CHECK( disguised_attack_decision.valid );
    CHECK( disguised_attack_decision.posture == bandit_live_world::local_gate_posture::probe );
    CHECK_FALSE( disguised_attack_decision.combat_forward );
}

TEST_CASE( "bandit_live_world_darkness_can_turn_cannibal_pack_contact_into_attack", "[bandit][live_world][profile][cannibal]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 780 ),
             tripoint_abs_ms( 1680, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_butcher", character_id( 781 ),
             tripoint_abs_ms( 1681, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_camp_leader", character_id( 782 ),
             tripoint_abs_ms( 1704, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 783 ),
             tripoint_abs_ms( 1705, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    prepare_hostile_follow_on( site, 2, 1, "player@72,80,0",
                               tripoint_abs_omt( 72, 80, 0 ), 100 );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 72, 80, 0 ),
                "player@72,80,0" );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::stalk );
    apply_test_hostile_dispatch( site, plan, 102 );
    REQUIRE( site.active_hostile_operation.reservation.member_ids.size() == 2 );

    bandit_live_world::local_gate_input daylight;
    daylight.local_threat = 3;
    daylight.local_opportunity = 1;
    daylight.local_contact_established = true;
    daylight.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision daylight_decision =
        bandit_live_world::choose_local_gate_posture( site, daylight );
    CHECK( daylight_decision.valid );
    CHECK( daylight_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( daylight_decision.combat_forward );

    bandit_live_world::local_gate_input dark = daylight;
    dark.darkness_or_concealment = true;
    const bandit_live_world::local_gate_decision dark_decision =
        bandit_live_world::choose_local_gate_posture( site, dark );
    CHECK( dark_decision.valid );
    CHECK( dark_decision.posture == bandit_live_world::local_gate_posture::attack_now );
    CHECK( dark_decision.combat_forward );
    const std::string dark_report = bandit_live_world::render_local_gate_report( site, dark,
                                    dark_decision );
    CHECK( dark_report.find( "profile=cannibal_camp" ) != std::string::npos );
    CHECK( dark_report.find( "pack_size=2" ) != std::string::npos );
    CHECK( dark_report.find( "darkness_or_concealment=yes" ) != std::string::npos );
    CHECK( dark_report.find( "posture=attack_now" ) != std::string::npos );

    bandit_live_world::local_gate_input high_threat = dark;
    high_threat.local_threat = 8;
    high_threat.local_opportunity = 0;
    const bandit_live_world::local_gate_decision high_threat_decision =
        bandit_live_world::choose_local_gate_posture( site, high_threat );
    CHECK_FALSE( high_threat_decision.valid );
    CHECK( high_threat_decision.posture == bandit_live_world::local_gate_posture::abort );
}

TEST_CASE( "bandit_live_world_writeback_shrinks_headcount_and_future_dispatch_capacity", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 701 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 702 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 703 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    CHECK( site.headcount == 3 );
    CHECK( site.count_live_members() == 3 );
    CHECK( site.dispatchable_member_capacity() == 2 );
    REQUIRE( site.find_spawn_tile( tripoint_abs_ms( 241, 480, 0 ) ) != nullptr );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 241, 480, 0 ) )->headcount == 1 );

    REQUIRE( bandit_live_world::update_member_state( site, character_id( 702 ),
             bandit_live_world::member_state::dead, "killed during local contact" ) );
    CHECK( site.headcount == 2 );
    CHECK( site.count_live_members() == 2 );
    CHECK( site.dispatchable_member_capacity() == 1 );
    CHECK( site.find_member( character_id( 702 ) )->last_writeback_summary ==
           "killed during local contact" );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 241, 480, 0 ) )->headcount == 0 );

    REQUIRE( bandit_live_world::update_member_state( site, character_id( 703 ),
             bandit_live_world::member_state::missing, "never returned from scout" ) );
    CHECK( site.headcount == 1 );
    CHECK( site.count_live_members() == 1 );
    CHECK( site.dispatchable_member_capacity() == 0 );

    const bandit_live_world::dispatch_plan blocked_plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    CHECK_FALSE( blocked_plan.valid );
    REQUIRE_FALSE( blocked_plan.notes.empty() );
    CHECK( blocked_plan.notes.back().find( "home reserve" ) != std::string::npos );
}

TEST_CASE( "bandit_live_world_chooses_reviewer_readable_local_approach_gate_posture", "[bandit][live_world][approach_gate]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 901 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 902 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 903 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    bandit_live_world::local_gate_input stalk_input;
    stalk_input.local_threat = 2;
    stalk_input.local_opportunity = 0;
    bandit_live_world::local_gate_decision decision =
        bandit_live_world::choose_local_gate_posture( site, stalk_input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::stalk );
    CHECK_FALSE( decision.opens_shakedown_surface );
    CHECK_FALSE( decision.combat_forward );

    bandit_live_world::local_gate_input camp_input;
    camp_input.local_threat = 3;
    camp_input.local_opportunity = 2;
    camp_input.standoff_distance = 10;
    camp_input.basecamp_or_camp_scene = true;
    camp_input.recent_exposure = true;
    decision = bandit_live_world::choose_local_gate_posture( site, camp_input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( decision.opens_shakedown_surface );
    CHECK_FALSE( decision.combat_forward );
    const std::string camp_report = bandit_live_world::render_local_gate_report( site, camp_input,
                                    decision );
    CHECK( camp_report.find( "posture=hold_off" ) != std::string::npos );
    CHECK( camp_report.find( "strength=1" ) != std::string::npos );
    CHECK( camp_report.find( "threat=3" ) != std::string::npos );
    CHECK( bandit_live_world::hot_defended_doorstep_blocks_pickup( site, camp_input, decision,
            character_id( 901 ) ) );

    bandit_live_world::local_gate_input distant_watch_input = camp_input;
    distant_watch_input.current_exposure = false;
    distant_watch_input.recent_exposure = false;
    distant_watch_input.standoff_distance = bandit_live_world::minimum_hold_off_standoff_omt();
    const bandit_live_world::local_gate_decision distant_watch_decision =
        bandit_live_world::choose_local_gate_posture( site, distant_watch_input );
    CHECK_FALSE( bandit_live_world::hot_defended_doorstep_blocks_pickup( site, distant_watch_input,
                 distant_watch_decision, character_id( 901 ) ) );

    bandit_live_world::local_gate_input smoked_watcher_input = camp_input;
    smoked_watcher_input.current_exposure = false;
    smoked_watcher_input.recent_exposure = false;
    smoked_watcher_input.smoke_obscured_lead = true;
    smoked_watcher_input.smoke_on_watcher_tile = true;
    const bandit_live_world::local_gate_decision smoked_watcher_decision =
        bandit_live_world::choose_local_gate_posture( site, smoked_watcher_input );
    CHECK( smoked_watcher_decision.valid );
    CHECK( smoked_watcher_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( smoked_watcher_decision.opens_shakedown_surface );
    CHECK_FALSE( smoked_watcher_decision.combat_forward );
    const std::string smoke_report = bandit_live_world::render_local_gate_report( site,
                                      smoked_watcher_input, smoked_watcher_decision );
    CHECK( smoke_report.find( "smoke_obscured=yes" ) != std::string::npos );
    CHECK( smoke_report.find( "smoke_on_watcher=yes" ) != std::string::npos );
    CHECK( smoke_report.find( "camping the smoked tile" ) != std::string::npos );
    CHECK( bandit_live_world::hot_defended_doorstep_blocks_pickup( site, smoked_watcher_input,
            smoked_watcher_decision, character_id( 901 ) ) );

    bandit_live_world::local_gate_input smoked_sightline_contact_input = camp_input;
    smoked_sightline_contact_input.current_exposure = false;
    smoked_sightline_contact_input.recent_exposure = false;
    smoked_sightline_contact_input.local_contact_established = true;
    smoked_sightline_contact_input.smoke_obscured_lead = true;
    smoked_sightline_contact_input.smoke_between_watcher_and_camp = true;
    const bandit_live_world::local_gate_decision smoked_sightline_contact_decision =
        bandit_live_world::choose_local_gate_posture( site, smoked_sightline_contact_input );
    CHECK( smoked_sightline_contact_decision.valid );
    CHECK( smoked_sightline_contact_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( smoked_sightline_contact_decision.opens_shakedown_surface );
    CHECK_FALSE( smoked_sightline_contact_decision.combat_forward );
    const std::string smoked_sightline_contact_report = bandit_live_world::render_local_gate_report(
                site, smoked_sightline_contact_input, smoked_sightline_contact_decision );
    CHECK( smoked_sightline_contact_report.find( "smoke_obscured=yes" ) != std::string::npos );
    CHECK( smoked_sightline_contact_report.find( "smoke_sightline=yes" ) != std::string::npos );
    CHECK( smoked_sightline_contact_report.find( "shakedown=no" ) != std::string::npos );
    CHECK( smoked_sightline_contact_report.find( "backs off/waits" ) != std::string::npos );

    bandit_live_world::local_gate_input probe_input;
    probe_input.local_threat = 1;
    probe_input.local_opportunity = 1;
    decision = bandit_live_world::choose_local_gate_posture( site, probe_input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::probe );
    CHECK_FALSE( decision.opens_shakedown_surface );
    CHECK_FALSE( decision.combat_forward );

    REQUIRE( bandit_live_world::update_member_state( site, character_id( 902 ),
             bandit_live_world::member_state::outbound, "joins local gate proof group" ) );
    site.active_outing.member_ids.push_back( character_id( 902 ) );

    bandit_live_world::local_gate_input shakedown_input;
    shakedown_input.local_threat = 1;
    shakedown_input.local_opportunity = 3;
    shakedown_input.local_contact_established = true;
    decision = bandit_live_world::choose_local_gate_posture( site, shakedown_input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::open_shakedown );
    CHECK( decision.opens_shakedown_surface );
    CHECK_FALSE( decision.combat_forward );

    bandit_live_world::local_gate_input rolling_input;
    rolling_input.local_threat = 2;
    rolling_input.local_opportunity = 1;
    rolling_input.rolling_travel_scene = true;
    decision = bandit_live_world::choose_local_gate_posture( site, rolling_input );
    CHECK( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::attack_now );
    CHECK_FALSE( decision.opens_shakedown_surface );
    CHECK( decision.combat_forward );
    const std::string rolling_report = bandit_live_world::render_local_gate_report( site, rolling_input,
                                       decision );
    CHECK( rolling_report.find( "posture=attack_now" ) != std::string::npos );
    CHECK( rolling_report.find( "rolling_travel=yes" ) != std::string::npos );
    CHECK( rolling_report.find( "combat_forward=yes" ) != std::string::npos );

    bandit_live_world::local_gate_input hopeless_input;
    hopeless_input.local_threat = 8;
    hopeless_input.local_opportunity = 0;
    decision = bandit_live_world::choose_local_gate_posture( site, hopeless_input );
    CHECK_FALSE( decision.valid );
    CHECK( decision.posture == bandit_live_world::local_gate_posture::abort );
    CHECK_FALSE( decision.opens_shakedown_surface );
    CHECK_FALSE( decision.combat_forward );
}

TEST_CASE( "bandit_live_world_vertical_targets_route_via_reachable_ground_dispatch_target",
           "[bandit][live_world][multi_z][dispatch]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 154 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 155 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 156 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const tripoint_abs_omt roof_target( 18, 20, 5 );
    const tripoint_abs_omt ground_target( 18, 20, 0 );
    CHECK( bandit_live_world::reachable_ground_dispatch_target( site, roof_target ) == ground_target );

    const bandit_live_world::dispatch_plan direct_plan =
        bandit_live_world::plan_site_dispatch( site, roof_target, "player@18,20,5" );
    REQUIRE( direct_plan.valid );
    CHECK( direct_plan.target_id == "player@18,20,5" );
    CHECK( direct_plan.target_omt == ground_target );
    CHECK( std::any_of( direct_plan.notes.begin(), direct_plan.notes.end(), []( const std::string &note ) {
        return note.find( "vertical dispatch fallback" ) != std::string::npos &&
               note.find( "18,20,5" ) != std::string::npos &&
               note.find( "18,20,0" ) != std::string::npos;
    } ) );

    bandit_live_world::camp_map_lead elevated_lead;
    elevated_lead.lead_id = "elevated_basecamp@18,20,5";
    elevated_lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    elevated_lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    elevated_lead.target_id = "player@18,20,5";
    elevated_lead.omt = roof_target;
    elevated_lead.radius_omt = 2;
    elevated_lead.bounty = 8;
    elevated_lead.threat = 1;
    elevated_lead.confidence = 3;
    site.intelligence_map.leads.push_back( elevated_lead );

    const bandit_live_world::camp_map_lead *matched_lead =
        bandit_live_world::find_camp_map_dispatch_lead_for_target( site, roof_target, "" );
    REQUIRE( matched_lead != nullptr );
    CHECK( matched_lead->lead_id == elevated_lead.lead_id );

    const bandit_live_world::camp_map_lead *ground_matched_lead =
        bandit_live_world::find_camp_map_dispatch_lead_for_target( site, ground_target,
                "player@18,20,0" );
    REQUIRE( ground_matched_lead != nullptr );
    CHECK( ground_matched_lead->lead_id == elevated_lead.lead_id );

    prepare_hostile_follow_on( site, 2, 1, elevated_lead.target_id, ground_target, 100 );
    const bandit_live_world::dispatch_plan camp_map_plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, *matched_lead );
    REQUIRE( camp_map_plan.valid );
    CHECK( camp_map_plan.target_id == "player@18,20,5" );
    CHECK( camp_map_plan.target_omt == ground_target );
    CHECK( std::any_of( camp_map_plan.notes.begin(), camp_map_plan.notes.end(), []( const std::string &note ) {
        return note.find( "vertical dispatch fallback" ) != std::string::npos &&
               note.find( "18,20,5" ) != std::string::npos &&
               note.find( "18,20,0" ) != std::string::npos;
    } ) );

    apply_test_hostile_dispatch( site, camp_map_plan, 102 );
    CHECK( site.active_hostile_operation.reservation.target_id == "player@18,20,5" );
    CHECK( site.active_hostile_operation.reservation.target_omt == ground_target );
}

TEST_CASE( "bandit_live_world_hold_off_goal_keeps_visible_standoff",
           "[bandit][live_world][local_gate]" )
{
    const tripoint_abs_omt player( 140, 41, 0 );
    const tripoint_abs_omt camp_anchor( 140, 51, 0 );
    const tripoint_abs_omt goal = bandit_live_world::choose_hold_off_standoff_goal(
                                      camp_anchor, player, 2 );

    CHECK( bandit_live_world::ordinary_scout_watch_standoff_omt() == 5 );
    CHECK( bandit_live_world::minimum_hold_off_standoff_omt() == 5 );
    CHECK( rl_dist( goal, player ) >= bandit_live_world::minimum_hold_off_standoff_omt() );
    CHECK( goal == tripoint_abs_omt( 140, 46, 0 ) );

    const tripoint_abs_omt diagonal_goal = bandit_live_world::choose_hold_off_standoff_goal(
            tripoint_abs_omt( 150, 51, 0 ), player, 2 );
    CHECK( diagonal_goal == tripoint_abs_omt( 145, 46, 0 ) );

    CHECK( bandit_live_world::choose_hold_off_standoff_goal( player, player, 2 ) == player );
}

TEST_CASE( "bandit_live_world_sight_avoid_uses_only_bounded_local_reposition_candidates",
           "[bandit][live_world][sight_avoid]" )
{
    const tripoint_abs_ms current( 100, 100, 0 );
    const std::vector<bandit_live_world::sight_avoid_candidate> candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), true, true, true, 0 },
        { tripoint_abs_ms( 99, 100, 0 ), true, false, true, 1 },
        { tripoint_abs_ms( 110, 100, 0 ), true, false, false, 99 },
        { tripoint_abs_ms( 100, 99, 0 ), false, false, false, 20 },
    };

    const bandit_live_world::sight_avoid_decision quiet =
        bandit_live_world::choose_sight_avoid_reposition( current, false, false, candidates );
    CHECK( quiet.valid );
    CHECK_FALSE( quiet.repositions );
    CHECK( quiet.reason == "still stalking" );

    const bandit_live_world::sight_avoid_decision exposed =
        bandit_live_world::choose_sight_avoid_reposition( current, true, false, candidates );
    CHECK( exposed.valid );
    REQUIRE( exposed.repositions );
    CHECK( exposed.destination == tripoint_abs_ms( 99, 100, 0 ) );
    CHECK( exposed.reason == "repositioning because exposed" );
    CHECK( rl_dist( exposed.destination, current ) == 1 );

    const std::vector<bandit_live_world::sight_avoid_candidate> hollow_candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), true, true, true, 0 },
        { tripoint_abs_ms( 100, 101, 0 ), false, false, false, 10 },
    };
    const bandit_live_world::sight_avoid_decision no_good_step =
        bandit_live_world::choose_sight_avoid_reposition( current, true, true, hollow_candidates );
    CHECK( no_good_step.valid );
    CHECK_FALSE( no_good_step.repositions );

    const std::vector<bandit_live_world::sight_avoid_candidate> smoke_blocked_candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), false, true, true, 0, true },
        { tripoint_abs_ms( 99, 100, 0 ), false, true, true, 0, true },
    };
    const bandit_live_world::sight_avoid_decision smoke_blocked =
        bandit_live_world::choose_sight_avoid_reposition( current, false, false,
                smoke_blocked_candidates, true );
    CHECK( smoke_blocked.valid );
    CHECK_FALSE( smoke_blocked.repositions );
    CHECK( smoke_blocked.reason == "blocked: smoke-obscured no adjacent passable reposition candidate" );

    const std::vector<bandit_live_world::sight_avoid_candidate> smoked_candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), true, true, true, 0, false },
        { tripoint_abs_ms( 99, 100, 0 ), true, true, true, 0, true },
    };
    const bandit_live_world::sight_avoid_decision smoked_tile =
        bandit_live_world::choose_sight_avoid_reposition( current, false, false, smoked_candidates,
                true );
    CHECK( smoked_tile.valid );
    REQUIRE( smoked_tile.repositions );
    CHECK( smoked_tile.destination == tripoint_abs_ms( 101, 100, 0 ) );
    CHECK( smoked_tile.reason == "repositioning because smoke obscures lead" );

    const std::vector<bandit_live_world::sight_avoid_candidate> smoky_no_clear_candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), true, true, true, 0, true },
        { tripoint_abs_ms( 99, 100, 0 ), true, true, true, 0, true },
    };
    const bandit_live_world::sight_avoid_decision smoky_no_clear =
        bandit_live_world::choose_sight_avoid_reposition( current, false, false,
                smoky_no_clear_candidates, true );
    CHECK( smoky_no_clear.valid );
    REQUIRE( smoky_no_clear.repositions );
    CHECK( rl_dist( smoky_no_clear.destination, current ) == 1 );
    CHECK( smoky_no_clear.reason == "repositioning because smoke obscures lead" );
}

TEST_CASE( "bandit_live_world_scout_sortie_has_finite_return_home_clock",
           "[bandit][live_world][scout_return]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 921 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 922 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ),
                "player@18,20,0" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    CHECK( site.active_outing.job_type == "scout" );
    CHECK( site.active_outing.member_ids.size() == 1 );

    REQUIRE( bandit_live_world::note_active_sortie_started(
                 site, require_current_simulation_cursor( site ), 100 ) );
    CHECK_FALSE( bandit_live_world::note_active_sortie_started(
                     site, require_current_simulation_cursor( site ), 105 ) );
    CHECK( bandit_live_world::ordinary_scout_sortie_limit_minutes() == 720 );
    CHECK_FALSE( bandit_live_world::scout_sortie_should_return_home( site, 819,
                 bandit_live_world::ordinary_scout_sortie_limit_minutes() ) );
    CHECK( bandit_live_world::scout_sortie_should_return_home( site, 820,
            bandit_live_world::ordinary_scout_sortie_limit_minutes() ) );
    REQUIRE( bandit_live_world::note_active_sortie_local_contact(
                 site, require_current_simulation_cursor( site ),
                 site.active_outing.member_ids.front(), 130 ) );
    CHECK( site.active_outing.expected_return_minutes == 970 );
    CHECK( site.active_outing.missing_deadline_minutes == 2410 );
    CHECK_FALSE( bandit_live_world::scout_sortie_should_return_home( site, 849,
                 bandit_live_world::ordinary_scout_sortie_limit_minutes() ) );
    CHECK( bandit_live_world::scout_sortie_should_return_home( site, 850,
            bandit_live_world::ordinary_scout_sortie_limit_minutes() ) );

    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );
    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );
    REQUIRE( loaded.sites.size() == 1 );
    const bandit_live_world::site_record &loaded_site = loaded.sites.front();
    CHECK( loaded_site.active_outing.job_type == "scout" );
    CHECK( loaded_site.active_outing.started_minutes == 100 );
    CHECK( loaded_site.active_outing.local_contact_minutes == 130 );
    CHECK( loaded_site.active_outing.expected_return_minutes == 970 );
    CHECK( loaded_site.active_outing.missing_deadline_minutes == 2410 );

    REQUIRE( bandit_live_world::update_member_state( site, site.active_outing.member_ids.front(),
             bandit_live_world::member_state::local_contact, "watched long enough near player@18,20,0" ) );
    const std::vector<bandit_live_world::active_member_observation> still_watching = {
        { site.active_outing.member_ids.front(),
          bandit_live_world::active_member_observation_state::returning_home,
          "scout sortie limit reached; returning home" }
    };
    CHECK_FALSE( bandit_live_world::resolve_active_group_aftermath( site, still_watching ).has_value() );

    const std::vector<bandit_live_world::active_member_observation> home = {
        { site.active_outing.member_ids.front(), bandit_live_world::active_member_observation_state::home,
          "npc back on home footprint" }
    };
    const std::optional<bandit_pursuit_handoff::return_packet> packet =
        bandit_live_world::resolve_active_group_aftermath( site, home );
    REQUIRE( packet.has_value() );
    CHECK( packet->result == bandit_pursuit_handoff::mission_result::withdrawn );
    REQUIRE( bandit_live_world::apply_return_packet( site, *packet ) );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.active_outing.job_type.empty() );
    CHECK( site.active_outing.started_minutes == -1 );
    CHECK( site.find_member( character_id( 921 ) )->state == bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit_live_world_makes_cannibal_camp_attack_instead_of_extort", "[bandit][live_world][cannibal][shakedown]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 911 ),
             tripoint_abs_ms( 1680, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_butcher", character_id( 912 ),
             tripoint_abs_ms( 1681, 1920, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_camp_leader", character_id( 913 ),
             tripoint_abs_ms( 1704, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "cannibal_hunter", character_id( 914 ),
             tripoint_abs_ms( 1705, 1944, 0 ), std::string( "cannibal_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( site.profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    prepare_hostile_follow_on( site, 2, 1, "player@72,80,0",
                               tripoint_abs_omt( 72, 80, 0 ), 100 );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 72, 80, 0 ),
                "player@72,80,0" );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::stalk );
    apply_test_hostile_dispatch( site, plan, 102 );

    bandit_live_world::local_gate_input favorable_input;
    favorable_input.local_threat = 1;
    favorable_input.local_opportunity = 3;
    favorable_input.local_contact_established = true;
    favorable_input.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision favorable_decision =
        bandit_live_world::choose_local_gate_posture( site, favorable_input );
    CHECK( favorable_decision.valid );
    CHECK( favorable_decision.posture == bandit_live_world::local_gate_posture::attack_now );
    CHECK_FALSE( favorable_decision.opens_shakedown_surface );
    CHECK( favorable_decision.combat_forward );
    const std::string favorable_report =
        bandit_live_world::render_local_gate_report( site, favorable_input, favorable_decision );
    CHECK( favorable_report.find( "profile=cannibal_camp" ) != std::string::npos );
    CHECK( favorable_report.find( "posture=attack_now" ) != std::string::npos );
    CHECK( favorable_report.find( "pack_size=2" ) != std::string::npos );
    CHECK( favorable_report.find( "darkness_or_concealment=no" ) != std::string::npos );
    CHECK( favorable_report.find( "current_exposure=no" ) != std::string::npos );
    CHECK( favorable_report.find( "sight_exposure=none" ) != std::string::npos );
    CHECK( favorable_report.find( "shakedown=no" ) != std::string::npos );
    CHECK( favorable_report.find( "attack-to-kill" ) != std::string::npos );

    bandit_live_world::shakedown_goods_pool pool;
    pool.player_carried_value = 100;
    pool.companion_carried_value = 50;
    pool.reachable_basecamp_value = 850;
    pool.basecamp_or_camp_scene = true;
    const bandit_live_world::shakedown_surface blocked_surface =
        bandit_live_world::build_shakedown_surface( site, favorable_input, favorable_decision, pool );
    CHECK_FALSE( blocked_surface.valid );
    REQUIRE_FALSE( blocked_surface.notes.empty() );
    CHECK( blocked_surface.notes.front().find( "local gate did not open" ) != std::string::npos );

    bandit_live_world::local_gate_input cautious_input = favorable_input;
    cautious_input.local_contact_established = false;
    cautious_input.local_threat = 2;
    cautious_input.local_opportunity = 1;
    cautious_input.basecamp_or_camp_scene = false;
    const bandit_live_world::local_gate_decision cautious_decision =
        bandit_live_world::choose_local_gate_posture( site, cautious_input );
    CHECK( cautious_decision.valid );
    CHECK( cautious_decision.posture == bandit_live_world::local_gate_posture::probe );
    CHECK_FALSE( cautious_decision.opens_shakedown_surface );
    CHECK_FALSE( cautious_decision.combat_forward );

    bandit_live_world::local_gate_input exposed_input = favorable_input;
    exposed_input.local_threat = 3;
    exposed_input.local_opportunity = 2;
    exposed_input.darkness_or_concealment = true;
    exposed_input.current_exposure = true;
    const bandit_live_world::local_gate_decision exposed_decision =
        bandit_live_world::choose_local_gate_posture( site, exposed_input );
    CHECK( exposed_decision.valid );
    CHECK( exposed_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( exposed_decision.opens_shakedown_surface );
    CHECK_FALSE( exposed_decision.combat_forward );
    const std::string exposed_report =
        bandit_live_world::render_local_gate_report( site, exposed_input, exposed_decision );
    CHECK( exposed_report.find( "current_exposure=yes" ) != std::string::npos );
    CHECK( exposed_report.find( "sight_exposure=current" ) != std::string::npos );
    CHECK( exposed_report.find( "visible beeline" ) != std::string::npos );

    bandit_live_world::local_gate_input smoked_cannibal_input = favorable_input;
    smoked_cannibal_input.local_contact_established = false;
    smoked_cannibal_input.smoke_obscured_lead = true;
    smoked_cannibal_input.smoke_on_watcher_tile = true;
    const bandit_live_world::local_gate_decision smoked_cannibal_decision =
        bandit_live_world::choose_local_gate_posture( site, smoked_cannibal_input );
    CHECK( smoked_cannibal_decision.valid );
    CHECK( smoked_cannibal_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( smoked_cannibal_decision.opens_shakedown_surface );
    CHECK_FALSE( smoked_cannibal_decision.combat_forward );
    const std::string smoked_cannibal_report =
        bandit_live_world::render_local_gate_report( site, smoked_cannibal_input,
                smoked_cannibal_decision );
    CHECK( smoked_cannibal_report.find( "profile=cannibal_camp" ) != std::string::npos );
    CHECK( smoked_cannibal_report.find( "smoke_obscured=yes" ) != std::string::npos );
    CHECK( smoked_cannibal_report.find( "smoke_on_watcher=yes" ) != std::string::npos );
    CHECK( smoked_cannibal_report.find( "shakedown=no" ) != std::string::npos );
    CHECK( smoked_cannibal_report.find( "camping the smoked tile" ) != std::string::npos );

    bandit_live_world::local_gate_input smoked_contact_cannibal_input = favorable_input;
    smoked_contact_cannibal_input.smoke_obscured_lead = true;
    smoked_contact_cannibal_input.smoke_between_watcher_and_camp = true;
    const bandit_live_world::local_gate_decision smoked_contact_cannibal_decision =
        bandit_live_world::choose_local_gate_posture( site, smoked_contact_cannibal_input );
    CHECK( smoked_contact_cannibal_decision.valid );
    CHECK( smoked_contact_cannibal_decision.posture == bandit_live_world::local_gate_posture::hold_off );
    CHECK_FALSE( smoked_contact_cannibal_decision.opens_shakedown_surface );
    CHECK_FALSE( smoked_contact_cannibal_decision.combat_forward );
    const std::string smoked_contact_cannibal_report =
        bandit_live_world::render_local_gate_report( site, smoked_contact_cannibal_input,
                smoked_contact_cannibal_decision );
    CHECK( smoked_contact_cannibal_report.find( "profile=cannibal_camp" ) != std::string::npos );
    CHECK( smoked_contact_cannibal_report.find( "smoke_sightline=yes" ) != std::string::npos );
    CHECK( smoked_contact_cannibal_report.find( "shakedown=no" ) != std::string::npos );
    CHECK( smoked_contact_cannibal_report.find( "camping the smoked tile" ) != std::string::npos );

    const tripoint_abs_ms current_tile( 100, 100, 0 );
    const std::vector<bandit_live_world::sight_avoid_candidate> exposed_candidates = {
        { tripoint_abs_ms( 101, 100, 0 ), true, true, true, 0 },
        { tripoint_abs_ms( 99, 100, 0 ), true, false, true, 2 },
    };
    const bandit_live_world::sight_avoid_decision reposition =
        bandit_live_world::choose_sight_avoid_reposition( current_tile, true, false,
                exposed_candidates );
    REQUIRE( reposition.repositions );
    CHECK( rl_dist( reposition.destination, current_tile ) == 1 );

    bandit_live_world::local_gate_input hopeless_input;
    hopeless_input.local_threat = 8;
    hopeless_input.local_opportunity = 0;
    const bandit_live_world::local_gate_decision hopeless_decision =
        bandit_live_world::choose_local_gate_posture( site, hopeless_input );
    CHECK_FALSE( hopeless_decision.valid );
    CHECK( hopeless_decision.posture == bandit_live_world::local_gate_posture::abort );
    CHECK_FALSE( hopeless_decision.opens_shakedown_surface );
}

TEST_CASE( "bandit_live_world_builds_a_bounded_pay_or_fight_shakedown_surface", "[bandit][live_world][shakedown]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 951 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 952 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 953 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player@18,20,0" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    bandit_live_world::local_gate_input single_bandit_gate_input;
    single_bandit_gate_input.local_threat = 1;
    single_bandit_gate_input.local_opportunity = 3;
    single_bandit_gate_input.local_contact_established = true;
    single_bandit_gate_input.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision single_bandit_gate_decision =
        bandit_live_world::choose_local_gate_posture( site, single_bandit_gate_input );
    CHECK( single_bandit_gate_decision.posture ==
           bandit_live_world::local_gate_posture::open_shakedown );
    CHECK( single_bandit_gate_decision.opens_shakedown_surface );

    REQUIRE( bandit_live_world::update_member_state( site, character_id( 952 ),
             bandit_live_world::member_state::outbound, "joins shakedown proof group" ) );
    site.active_outing.member_ids.push_back( character_id( 952 ) );

    bandit_live_world::local_gate_input gate_input;
    gate_input.local_threat = 1;
    gate_input.local_opportunity = 3;
    gate_input.local_contact_established = true;
    gate_input.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision gate_decision =
        bandit_live_world::choose_local_gate_posture( site, gate_input );
    REQUIRE( gate_decision.valid );
    REQUIRE( gate_decision.posture == bandit_live_world::local_gate_posture::open_shakedown );
    REQUIRE( gate_decision.opens_shakedown_surface );

    bandit_live_world::shakedown_goods_pool basecamp_pool;
    basecamp_pool.player_carried_value = 100;
    basecamp_pool.companion_carried_value = 50;
    basecamp_pool.vehicle_carried_value = 10000;
    basecamp_pool.reachable_basecamp_value = 850;
    basecamp_pool.basecamp_or_camp_scene = true;
    const bandit_live_world::shakedown_surface basecamp_surface =
        bandit_live_world::build_shakedown_surface( site, gate_input, gate_decision,
                basecamp_pool );
    CHECK( basecamp_surface.valid );
    CHECK( basecamp_surface.pay_available );
    CHECK( basecamp_surface.fight_available );
    CHECK( basecamp_surface.includes_basecamp_inventory );
    CHECK_FALSE( basecamp_surface.includes_vehicle_inventory );
    CHECK( basecamp_surface.reachable_goods_value == 1000 );
    CHECK( basecamp_surface.demanded_value == 350 );
    CHECK( basecamp_surface.opening_id == "basecamp_pressure" );
    const std::string basecamp_report =
        bandit_live_world::render_shakedown_surface_report( site, basecamp_surface );
    CHECK( basecamp_report.find( "pay_option=yes" ) != std::string::npos );
    CHECK( basecamp_report.find( "fight_option=yes" ) != std::string::npos );
    CHECK( basecamp_report.find( "visible_responses=pay/fight" ) != std::string::npos );
    CHECK( basecamp_report.find( "payment_surface=npc_trade_ui" ) != std::string::npos );
    CHECK( basecamp_report.find( "pay/fight/refuse" ) == std::string::npos );
    CHECK( basecamp_report.find( "basecamp_inventory=yes" ) != std::string::npos );
    CHECK( basecamp_report.find( "vehicle_inventory=no" ) != std::string::npos );
    CHECK( basecamp_report.find( "opening=basecamp_pressure" ) != std::string::npos );
    CHECK( basecamp_report.find( "demanded_toll=350" ) != std::string::npos );

    bandit_live_world::local_gate_input offbase_gate_input = gate_input;
    offbase_gate_input.basecamp_or_camp_scene = false;
    bandit_live_world::shakedown_goods_pool offbase_pool;
    offbase_pool.player_carried_value = 200;
    offbase_pool.companion_carried_value = 100;
    offbase_pool.vehicle_carried_value = 600;
    offbase_pool.reachable_basecamp_value = 5000;
    const bandit_live_world::shakedown_surface offbase_surface =
        bandit_live_world::build_shakedown_surface( site, offbase_gate_input, gate_decision,
                offbase_pool );
    CHECK( offbase_surface.valid );
    CHECK_FALSE( offbase_surface.includes_basecamp_inventory );
    CHECK( offbase_surface.includes_vehicle_inventory );
    CHECK( offbase_surface.reachable_goods_value == 900 );
    CHECK( offbase_surface.demanded_value == 315 );
    CHECK( offbase_surface.opening_id == "weakness_read" );

    bandit_live_world::local_gate_input cover_gate_input = offbase_gate_input;
    cover_gate_input.standoff_distance = 2;
    const bandit_live_world::shakedown_surface cover_surface =
        bandit_live_world::build_shakedown_surface( site, cover_gate_input, gate_decision,
                offbase_pool );
    CHECK( cover_surface.valid );
    CHECK( cover_surface.opening_id == "warning_from_cover" );

    bandit_live_world::local_gate_input roadblock_gate_input = offbase_gate_input;
    roadblock_gate_input.local_threat = 3;
    const bandit_live_world::local_gate_decision roadblock_gate_decision =
        bandit_live_world::choose_local_gate_posture( site, roadblock_gate_input );
    REQUIRE( roadblock_gate_decision.opens_shakedown_surface );
    const bandit_live_world::shakedown_surface roadblock_surface =
        bandit_live_world::build_shakedown_surface( site, roadblock_gate_input,
                roadblock_gate_decision, offbase_pool );
    CHECK( roadblock_surface.valid );
    CHECK( roadblock_surface.opening_id == "roadblock_toll" );

    bandit_live_world::local_gate_input rolling_gate_input = gate_input;
    rolling_gate_input.rolling_travel_scene = true;
    const bandit_live_world::shakedown_surface rolling_surface =
        bandit_live_world::build_shakedown_surface( site, rolling_gate_input, gate_decision,
                offbase_pool );
    CHECK_FALSE( rolling_surface.valid );
    REQUIRE_FALSE( rolling_surface.notes.empty() );
    CHECK( rolling_surface.notes.front().find( "direct-ambush" ) != std::string::npos );
}


TEST_CASE( "bandit_live_world_records_shakedown_aftermath_for_renegotiation_pressure", "[bandit][live_world][shakedown]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 971 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 972 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit_trader", character_id( 973 ),
             tripoint_abs_ms( 264, 504, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player@18,20,0" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    site.remembered_threat_estimate = 4;

    bandit_live_world::local_gate_input gate_input;
    gate_input.local_threat = 1;
    gate_input.local_opportunity = 3;
    gate_input.local_contact_established = true;
    gate_input.basecamp_or_camp_scene = true;
    const bandit_live_world::local_gate_decision gate_decision =
        bandit_live_world::choose_local_gate_posture( site, gate_input );
    REQUIRE( gate_decision.opens_shakedown_surface );

    bandit_live_world::shakedown_goods_pool pool;
    pool.player_carried_value = 100;
    pool.companion_carried_value = 50;
    pool.reachable_basecamp_value = 850;
    pool.basecamp_or_camp_scene = true;

    const bandit_live_world::shakedown_surface first_surface =
        bandit_live_world::build_shakedown_surface( site, gate_input, gate_decision, pool );
    REQUIRE( first_surface.valid );
    REQUIRE( first_surface.demanded_value == 350 );

    bandit_live_world::shakedown_outcome fight_opened;
    fight_opened.fought = true;
    fight_opened.basecamp_or_camp_scene = true;
    fight_opened.demanded_value = first_surface.demanded_value;
    fight_opened.reachable_goods_value = first_surface.reachable_goods_value;
    REQUIRE( bandit_live_world::apply_shakedown_outcome( site, fight_opened ).valid );
    bandit_live_world::begin_shakedown_basecamp_defender_observation( site, 2 );
    CHECK( site.shakedown_basecamp_defender_observation_pending );
    CHECK_FALSE( bandit_live_world::apply_shakedown_basecamp_defender_observation( site, 2 ).valid );
    CHECK( site.shakedown_basecamp_defender_observation_pending );
    const bandit_live_world::shakedown_aftermath_effect harsh_effect =
        bandit_live_world::apply_shakedown_basecamp_defender_observation( site, 1 );
    CHECK( harsh_effect.valid );
    CHECK( harsh_effect.stronger_reopen );
    CHECK_FALSE( harsh_effect.cools_later_pressure );
    CHECK( harsh_effect.demand_modifier_percent == 140 );
    CHECK( site.last_shakedown_outcome == "fight_defender_loss" );
    CHECK( site.shakedown_defender_losses == 1 );
    CHECK_FALSE( site.shakedown_basecamp_defender_observation_pending );
    CHECK( site.shakedown_reopen_available );
    CHECK_FALSE( site.shakedown_reopen_used );
    CHECK( site.remembered_threat_estimate == 3 );

    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );
    JsonValue jsin = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( jsin.get_object() );
    REQUIRE( loaded.sites.size() == 1 );
    CHECK( loaded.sites.front().shakedown_reopen_available );
    CHECK( loaded.sites.front().shakedown_defender_losses == 1 );
    CHECK_FALSE( loaded.sites.front().shakedown_basecamp_defender_observation_pending );
    CHECK( loaded.sites.front().last_shakedown_outcome == "fight_defender_loss" );

    const bandit_live_world::shakedown_surface reopened_surface =
        bandit_live_world::build_shakedown_surface( site, gate_input, gate_decision, pool );
    CHECK( reopened_surface.valid );
    CHECK( reopened_surface.pay_available );
    CHECK( reopened_surface.fight_available );
    CHECK( reopened_surface.opening_id == "reopened_demand" );
    CHECK( reopened_surface.demanded_value == 490 );
    const std::string reopened_report =
        bandit_live_world::render_shakedown_surface_report( site, reopened_surface );
    CHECK( reopened_report.find( "renegotiation reopen" ) != std::string::npos );
    CHECK( reopened_report.find( "demanded_toll=490" ) != std::string::npos );

    REQUIRE( bandit_live_world::mark_shakedown_reopen_used( site ) );
    const bandit_live_world::shakedown_surface spent_reopen_surface =
        bandit_live_world::build_shakedown_surface( site, gate_input, gate_decision, pool );
    CHECK( spent_reopen_surface.demanded_value == 350 );

    bandit_live_world::shakedown_outcome bandit_loss;
    bandit_loss.fought = true;
    bandit_loss.demanded_value = first_surface.demanded_value;
    bandit_loss.reachable_goods_value = first_surface.reachable_goods_value;
    bandit_loss.bandit_losses = 1;
    bandit_loss.extraction_failed = true;
    const bandit_live_world::shakedown_aftermath_effect cool_effect =
        bandit_live_world::apply_shakedown_outcome( site, bandit_loss );
    CHECK( cool_effect.valid );
    CHECK_FALSE( cool_effect.stronger_reopen );
    CHECK( cool_effect.cools_later_pressure );
    CHECK( cool_effect.demand_modifier_percent == 75 );
    CHECK( site.shakedown_bandit_losses == 1 );
    CHECK( site.shakedown_caution == 1 );
    CHECK( site.remembered_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::collapsed );

    const bandit_live_world::shakedown_surface cooled_surface =
        bandit_live_world::build_shakedown_surface( site, gate_input, gate_decision, pool );
    CHECK( cooled_surface.demanded_value == 263 );
    const std::string cooled_report =
        bandit_live_world::render_shakedown_surface_report( site, cooled_surface );
    CHECK( cooled_report.find( "aftermath caution" ) != std::string::npos );
}

TEST_CASE( "bandit_live_world_cools_shakedown_pressure_when_the_active_bandit_is_lost", "[bandit][live_world][shakedown]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 981 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 982 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    bandit_live_world::shakedown_outcome fight_opened;
    fight_opened.fought = true;
    fight_opened.basecamp_or_camp_scene = true;
    fight_opened.demanded_value = 350;
    fight_opened.reachable_goods_value = 1000;
    REQUIRE( bandit_live_world::apply_shakedown_outcome( site, fight_opened ).valid );
    CHECK( site.last_shakedown_outcome == "fight_unresolved" );

    bandit_pursuit_handoff::local_outcome outcome;
    outcome.survivors_remaining = 0;
    outcome.anchored_identity_updates = { { "981", "missing" } };
    outcome.result = bandit_pursuit_handoff::mission_result::broken;
    outcome.resolution = bandit_pursuit_handoff::lead_resolution::target_lost;
    outcome.posture = bandit_pursuit_handoff::return_posture::escape_safe;
    outcome.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
    const bandit_pursuit_handoff::return_packet packet =
        bandit_pursuit_handoff::build_return_packet( plan.entry, outcome );

    REQUIRE( bandit_live_world::apply_return_packet( site, packet ) );
    CHECK( site.find_member( character_id( 981 ) )->state == bandit_live_world::member_state::missing );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.last_shakedown_outcome == "fight_bandit_loss" );
    CHECK( site.shakedown_bandit_losses == 1 );
    CHECK( site.shakedown_caution == 1 );
    CHECK( site.remembered_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::collapsed );
}

TEST_CASE( "bandit_live_world_applies_a_return_packet_onto_the_active_owned_outing", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 801 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 802 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    REQUIRE( site.active_outing.member_ids == std::vector<character_id>( { character_id( 801 ) } ) );

    bandit_pursuit_handoff::local_outcome outcome;
    outcome.survivors_remaining = 0;
    outcome.anchored_identity_updates = { { "801", "dead" } };
    outcome.result = bandit_pursuit_handoff::mission_result::repelled;
    outcome.resolution = bandit_pursuit_handoff::lead_resolution::target_lost;
    outcome.posture = bandit_pursuit_handoff::return_posture::broken_flee;
    outcome.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
    const bandit_pursuit_handoff::return_packet packet =
        bandit_pursuit_handoff::build_return_packet( plan.entry, outcome );

    REQUIRE( bandit_live_world::apply_return_packet( site, packet ) );
    CHECK( site.find_member( character_id( 801 ) )->state == bandit_live_world::member_state::dead );
    CHECK( site.find_member( character_id( 801 ) )->last_writeback_summary ==
           "return repelled from player_basecamp_nearby (dead)" );
    CHECK( site.find_member( character_id( 802 ) )->state == bandit_live_world::member_state::at_home );
    CHECK( site.headcount == 1 );
    CHECK( site.dispatchable_member_capacity() == 0 );
    CHECK( site.active_outing.activity_id.empty() );
    CHECK( site.active_outing.target_id.empty() );
    CHECK( site.active_outing.member_ids.empty() );
}

TEST_CASE( "bandit_live_world_rejects_malformed_return_packets_atomically",
           "[bandit][live_world][handoff]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 811 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 812 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ),
                "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    bandit_pursuit_handoff::local_outcome outcome;
    outcome.survivors_remaining = 0;
    outcome.anchored_identity_updates = { { "811", "dead" } };
    outcome.result = bandit_pursuit_handoff::mission_result::repelled;
    outcome.resolution = bandit_pursuit_handoff::lead_resolution::target_lost;
    const bandit_pursuit_handoff::return_packet valid_packet =
        bandit_pursuit_handoff::build_return_packet( plan.entry, outcome );

    SECTION( "survivor count mismatch" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.survivors_remaining = 1;
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "duplicate member resolution" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.anchored_identity_updates.push_back( { "811", "missing" } );
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "unknown member resolution" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.anchored_identity_updates = { { "999999", "dead" } };
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "unknown resolution state" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.anchored_identity_updates = { { "811", "teleported" } };
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "stale generation" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        ++malformed.activity_generation;
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "stale handoff epoch" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        ++malformed.handoff_epoch;
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "wrong application key" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.return_application_key += ":wrong";
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }

    SECTION( "wrong job type" ) {
        bandit_pursuit_handoff::return_packet malformed = valid_packet;
        malformed.job_type = bandit_dry_run::job_template::toll;
        const std::string before = serialize_world( world );
        CHECK_FALSE( bandit_live_world::apply_return_packet( site, malformed ) );
        CHECK( serialize_world( world ) == before );
    }
}

TEST_CASE( "bandit_live_world_report_watermark_does_not_close_a_legacy_hostile_operation",
           "[bandit][live_world][handoff][replay]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 45700 );
    bandit_live_world::site_record &site = world.sites.front();
    REQUIRE( bandit_live_world::update_member_state( site, character_id( 45700 ),
             bandit_live_world::member_state::outbound, "test stale hostile outing" ) );
    set_test_active_outing( site, site.site_id + "#toll:stale",
                            bandit_live_world::outing_kind::hostile_operation );
    site.active_outing.member_ids = { character_id( 45700 ) };
    site.active_outing.leader_id = character_id( 45700 );
    site.active_outing.job_type = "toll";
    site.applied_report_generation = site.active_outing.generation;

    const bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    CHECK_FALSE( loaded.sites.front().active_outing.is_active() );
    CHECK( loaded.sites.front().active_outing.member_ids.empty() );
    REQUIRE( loaded.sites.front().active_hostile_operation.is_active() );
    CHECK( loaded.sites.front().active_hostile_operation.legacy_unpinned );
    CHECK( loaded.sites.front().active_hostile_operation.phase ==
           bandit_live_world::hostile_operation_phase::returning_home );
    CHECK( loaded.sites.front().active_hostile_operation.reservation.generation ==
           loaded.sites.front().applied_report_generation );
    REQUIRE( loaded.sites.front().find_member( character_id( 45700 ) ) != nullptr );
    CHECK( loaded.sites.front().find_member( character_id( 45700 ) )->state ==
           bandit_live_world::member_state::outbound );
}

TEST_CASE( "bandit_live_world_rejects_a_completed_return_after_reload_and_redispatch",
           "[bandit][live_world][handoff][replay]" )
{
    bandit_live_world::world_state world;
    for( int index = 0; index < 3; ++index ) {
        REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit",
                 character_id( 820 + index ), tripoint_abs_ms( 240 + index, 480, 0 ),
                 std::string( "bandit_camp" ), std::nullopt, special_lookup ) );
    }

    bandit_live_world::site_record &first_site = world.sites.front();
    const bandit_live_world::dispatch_plan first_plan =
        bandit_live_world::plan_site_dispatch( first_site, tripoint_abs_omt( 18, 20, 0 ),
                "player_basecamp_nearby" );
    REQUIRE( first_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( first_site, first_plan ) );

    bandit_pursuit_handoff::local_outcome first_outcome;
    first_outcome.survivors_remaining = 1;
    first_outcome.result = bandit_pursuit_handoff::mission_result::scouted;
    first_outcome.resolution = bandit_pursuit_handoff::lead_resolution::still_valid;
    const bandit_pursuit_handoff::return_packet first_packet =
        bandit_pursuit_handoff::build_return_packet( first_plan.entry, first_outcome );
    REQUIRE( bandit_live_world::apply_return_packet( first_site, first_packet ) );
    REQUIRE( first_site.applied_return_generation == 1 );
    REQUIRE( first_site.camp_decision.state ==
             bandit_live_world::camp_decision_state::report_awaiting_assessment );
    const int first_report_revision = first_site.camp_decision.source_report_revision;
    const int first_report_generation = first_site.camp_decision.source_report_generation;
    REQUIRE( bandit_live_world::transition_camp_decision_state(
                 first_site,
                 bandit_live_world::camp_decision_state::report_awaiting_assessment,
                 bandit_live_world::camp_decision_state::cooldown,
                 first_report_revision, first_report_generation, 0, 0,
                 "first report assessed without follow-on" ) ==
             bandit_live_world::camp_decision_transition_result::applied );
    REQUIRE( bandit_live_world::transition_camp_decision_state(
                 first_site, bandit_live_world::camp_decision_state::cooldown,
                 bandit_live_world::camp_decision_state::idle,
                 first_report_revision, first_report_generation, 0, -1,
                 "zero-duration test cooldown elapsed" ) ==
             bandit_live_world::camp_decision_transition_result::applied );

    const bandit_live_world::dispatch_plan second_plan =
        bandit_live_world::plan_site_dispatch( first_site, tripoint_abs_omt( 19, 20, 0 ),
                "player_basecamp_moved" );
    REQUIRE( second_plan.valid );
    REQUIRE( second_plan.entry.activity_generation == 2 );
    REQUIRE( bandit_live_world::apply_dispatch_plan( first_site, second_plan ) );

    bandit_live_world::world_state loaded = round_trip_world( world );
    REQUIRE( loaded.sites.size() == 1 );
    bandit_live_world::site_record &loaded_site = loaded.sites.front();
    REQUIRE( loaded_site.active_outing.generation == 2 );
    REQUIRE( loaded_site.applied_return_generation == 1 );
    const std::string before_replay = serialize_world( loaded );
    CHECK_FALSE( bandit_live_world::apply_return_packet( loaded_site, first_packet ) );
    CHECK( serialize_world( loaded ) == before_replay );
}

TEST_CASE( "bandit_live_world_scout_persistence_stays_within_a_coarse_save_budget",
           "[bandit][live_world][save_size]" )
{
    bandit_live_world::world_state empty_world;
    const std::size_t empty_bytes = serialize_world( empty_world ).size();

    bandit_live_world::world_state normal_world;
    for( int index = 0; index < 4; ++index ) {
        add_bandit_camp_member( normal_world, index, 11800 );
    }
    bandit_live_world::site_record &normal_site = normal_world.sites.front();
    const bandit_live_world::dispatch_plan normal_plan =
        bandit_live_world::plan_site_dispatch( normal_site, tripoint_abs_omt( 18, 20, 0 ),
                "player@18,20,0" );
    REQUIRE( normal_plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( normal_site, normal_plan ) );
    normal_site.active_outing.shared_route = {
        tripoint_abs_omt( 11, 20, 0 ), tripoint_abs_omt( 13, 20, 0 ),
        tripoint_abs_omt( 15, 20, 0 ), tripoint_abs_omt( 18, 20, 0 )
    };
    normal_site.active_outing.observations = {
        { "smoke@18,20,0", "thin smoke over the target", 45, 120, false },
        { "defenders@18,20,0", "two visible defenders", 70, 125, true }
    };
    normal_site.active_outing.cargo = { 2, 60 };
    normal_site.active_outing.started_minutes = 100;
    normal_site.active_outing.last_advanced_minutes = 100;
    normal_site.active_outing.expected_return_minutes = 940;
    normal_site.active_outing.missing_deadline_minutes = 2380;
    const std::size_t normal_bytes = serialize_world( normal_world ).size();

    bandit_live_world::world_state saturated_world = normal_world;
    bandit_live_world::site_record &saturated_site = saturated_world.sites.front();
    saturated_site.active_outing.generation = 2;
    saturated_site.next_outing_generation = 3;
    saturated_site.active_outing.return_application_key =
        saturated_site.active_outing.activity_id + ":return:2";
    saturated_site.active_outing.report_application_key =
        saturated_site.active_outing.activity_id + ":report:2";
    saturated_site.active_outing.cargo_application_key =
        saturated_site.active_outing.activity_id + ":cargo:2";
    saturated_site.active_outing.shared_route.clear();
    for( int index = 0; index < 256; ++index ) {
        saturated_site.active_outing.shared_route.emplace_back( 10 + index, 20 + index, 0 );
    }
    saturated_site.active_outing.observations.clear();
    saturated_site.current_scout_report.revision = 1;
    saturated_site.current_scout_report.source_activity_id = saturated_site.site_id + "#scout:1";
    saturated_site.current_scout_report.source_generation = 1;
    saturated_site.applied_report_generation = 1;
    saturated_site.current_scout_report.target_id = saturated_site.active_outing.target_id;
    saturated_site.current_scout_report.target_omt = saturated_site.active_outing.target_omt;
    saturated_site.current_scout_report.application_key =
        saturated_site.current_scout_report.source_activity_id + ":report:1";
    for( int index = 0; index < 16; ++index ) {
        const bandit_live_world::sortie_observation observation = {
            "bounded-fact-" + std::to_string( index ), std::string( 512, 'x' ),
            100, 1000 + index, index % 2 == 0
        };
        saturated_site.active_outing.observations.push_back( observation );
        saturated_site.current_scout_report.observations.push_back( observation );
    }
    saturated_site.active_outing.cargo = { 999999, 999999 };
    saturated_site.returned_cargo_stock = { 999999, 999999 };
    const std::size_t saturated_bytes = serialize_world( saturated_world ).size();

    CAPTURE( empty_bytes, normal_bytes, saturated_bytes );
    CHECK( empty_bytes < normal_bytes );
    CHECK( normal_bytes < saturated_bytes );
    CHECK( saturated_bytes < 64 * 1024 );
    CHECK( serialize_world( round_trip_world( saturated_world ) ).size() == saturated_bytes );
}

TEST_CASE( "bandit_live_world_empty_site_retirement_requires_both_home_and_active_sides_empty",
           "[bandit][live_world][retirement]" )
{
    SECTION( "home members remain without dispatch keeps the site active" ) {
        bandit_live_world::world_state world;
        REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 10001 ),
                 tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
                 special_lookup ) );

        std::vector<std::string> reports;
        CHECK( bandit_live_world::retire_empty_hostile_sites( world, &reports ) == 0 );
        REQUIRE( world.sites.size() == 1 );
        CHECK_FALSE( world.sites.front().retired_empty_site );
        CHECK( reports.empty() );
        CHECK( world.sites.front().count_members_in_state( bandit_live_world::member_state::at_home ) == 1 );
        CHECK( world.sites.front().count_home_side_signals() > 0 );
    }

    SECTION( "active dispatch without at-home members keeps the site active" ) {
        bandit_live_world::world_state world;
        REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 10002 ),
                 tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
                 special_lookup ) );

        bandit_live_world::site_record &site = world.sites.front();
        set_test_active_outing( site, site.site_id + "#dispatch" );
        site.active_outing.target_id = "player_basecamp_nearby";
        site.active_outing.member_ids = { character_id( 10002 ) };
        REQUIRE( bandit_live_world::update_member_state( site, character_id( 10002 ),
                 bandit_live_world::member_state::outbound, "test outbound dispatch" ) );

        std::vector<std::string> reports;
        CHECK( bandit_live_world::retire_empty_hostile_sites( world, &reports ) == 0 );
        CHECK_FALSE( site.retired_empty_site );
        CHECK( reports.empty() );
        CHECK( site.count_members_in_state( bandit_live_world::member_state::at_home ) == 0 );
        CHECK( site.has_active_outside_pressure() );
    }

    SECTION( "unresolved returning-home active aftermath keeps the site active until resolved" ) {
        bandit_live_world::world_state world;
        REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 10003 ),
                 tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
                 special_lookup ) );

        bandit_live_world::site_record &site = world.sites.front();
        set_test_active_outing( site, site.site_id + "#dispatch" );
        site.active_outing.target_id = "player_basecamp_nearby";
        site.active_outing.member_ids = { character_id( 10003 ) };
        REQUIRE( bandit_live_world::update_member_state( site, character_id( 10003 ),
                 bandit_live_world::member_state::local_contact, "test unresolved contact" ) );

        const std::vector<bandit_live_world::active_member_observation> returning_home = {
            { character_id( 10003 ), bandit_live_world::active_member_observation_state::returning_home,
              "still returning home" }
        };
        CHECK_FALSE( bandit_live_world::resolve_active_group_aftermath( site, returning_home ).has_value() );

        std::vector<std::string> reports;
        CHECK( bandit_live_world::retire_empty_hostile_sites( world, &reports ) == 0 );
        CHECK_FALSE( site.retired_empty_site );
        CHECK( reports.empty() );
        CHECK( site.has_active_outside_pressure() );
    }

    SECTION( "no live home side and no active outside pressure retires the site from AI surfaces" ) {
        bandit_live_world::world_state world;
        REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 10004 ),
                 tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
                 special_lookup ) );

        bandit_live_world::site_record &site = world.sites.front();
        REQUIRE( bandit_live_world::update_member_state( site, character_id( 10004 ),
                 bandit_live_world::member_state::dead, "test camp cleared" ) );
        REQUIRE( site.count_home_side_signals() == 0 );
        REQUIRE_FALSE( site.has_active_outside_pressure() );

        std::vector<std::string> reports;
        CHECK( bandit_live_world::retire_empty_hostile_sites( world, &reports ) == 1 );
        CHECK( site.retired_empty_site );
        REQUIRE( reports.size() == 1 );
        CHECK( reports.front().find( "retired_empty_site" ) != std::string::npos );
        CHECK( reports.front().find( "home_side_signals=0" ) != std::string::npos );
        CHECK( reports.front().find( "active_outside=no" ) != std::string::npos );

        const bandit_live_world::dispatch_plan retired_plan =
            bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ),
                    "player_basecamp_nearby" );
        CHECK_FALSE( retired_plan.valid );
        REQUIRE_FALSE( retired_plan.notes.empty() );
        CHECK( retired_plan.notes.front().find( "retired_empty_site" ) != std::string::npos );

        bandit_live_world::live_signal_mark smoke_mark;
        smoke_mark.mark_id = "live_smoke@18,20,0";
        smoke_mark.kind = "smoke";
        smoke_mark.source_omt = tripoint_abs_omt( 18, 20, 0 );
        smoke_mark.range_cap_omt = 15;
        CHECK_FALSE( bandit_live_world::record_live_signal_mark( site, smoke_mark ) );
    }
}

TEST_CASE( "bandit_live_world_resolves_bounded_live_aftermath_observations", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 901 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "thug", character_id( 902 ),
             tripoint_abs_ms( 241, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

    bandit_live_world::site_record &site = world.sites.front();
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 18, 20, 0 ), "player_basecamp_nearby" );
    REQUIRE( plan.valid );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

    const std::vector<bandit_live_world::active_member_observation> in_contact = {
        { character_id( 901 ), bandit_live_world::active_member_observation_state::local_contact,
          "still in local contact" }
    };
    CHECK_FALSE( bandit_live_world::resolve_active_group_aftermath( site, in_contact ).has_value() );

    SECTION( "dead member resolves to a broken return packet" ) {
        const std::vector<bandit_live_world::active_member_observation> wiped = {
            { character_id( 901 ), bandit_live_world::active_member_observation_state::dead,
              "killed near player target" }
        };
        const std::optional<bandit_pursuit_handoff::return_packet> wiped_packet =
            bandit_live_world::resolve_active_group_aftermath( site, wiped );
        REQUIRE( wiped_packet.has_value() );
        CHECK( wiped_packet->valid );
        CHECK( wiped_packet->group_id == site.active_outing.activity_id );
        CHECK( wiped_packet->source_camp_id == site.site_id );
        CHECK( wiped_packet->survivors_remaining == 0 );
        CHECK( wiped_packet->result == bandit_pursuit_handoff::mission_result::broken );
        REQUIRE( wiped_packet->anchored_identity_updates.size() == 1 );
        CHECK( wiped_packet->anchored_identity_updates.front().id == "901" );
        CHECK( wiped_packet->anchored_identity_updates.front().status == "dead" );
        REQUIRE( bandit_live_world::apply_return_packet( site, *wiped_packet ) );
        CHECK( site.find_member( character_id( 901 ) )->state == bandit_live_world::member_state::dead );
        CHECK( site.headcount == 1 );
    }

    SECTION( "missing member resolves to a broken return packet" ) {
        const std::vector<bandit_live_world::active_member_observation> lost = {
            { character_id( 901 ), bandit_live_world::active_member_observation_state::missing,
              "vanished during live contact" }
        };
        const std::optional<bandit_pursuit_handoff::return_packet> lost_packet =
            bandit_live_world::resolve_active_group_aftermath( site, lost );
        REQUIRE( lost_packet.has_value() );
        CHECK( lost_packet->valid );
        CHECK( lost_packet->group_id == site.active_outing.activity_id );
        CHECK( lost_packet->source_camp_id == site.site_id );
        CHECK( lost_packet->survivors_remaining == 0 );
        CHECK( lost_packet->result == bandit_pursuit_handoff::mission_result::broken );
        REQUIRE( lost_packet->anchored_identity_updates.size() == 1 );
        CHECK( lost_packet->anchored_identity_updates.front().id == "901" );
        CHECK( lost_packet->anchored_identity_updates.front().status == "missing" );
        REQUIRE( bandit_live_world::apply_return_packet( site, *lost_packet ) );
        CHECK( site.find_member( character_id( 901 ) )->state == bandit_live_world::member_state::missing );
        CHECK( site.headcount == 1 );
    }
}

TEST_CASE( "bandit_live_world_plans_live_dispatch_from_remembered_camp_map_lead",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 7; ++i ) {
        add_bandit_camp_member( world, i, 12400 );
    }

    bandit_live_world::site_record &site = world.sites.front();
    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "remembered_basecamp@18,20,0";
    lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    lead.status = bandit_live_world::camp_lead_status::scout_confirmed;
    lead.target_id = "player@18,20,0";
    lead.omt = tripoint_abs_omt( 18, 20, 0 );
    lead.radius_omt = 2;
    lead.bounty = 7;
    lead.threat = 1;
    lead.confidence = 3;
    site.intelligence_map.leads.push_back( lead );

    const bandit_live_world::camp_map_lead *matched_lead =
        bandit_live_world::find_camp_map_dispatch_lead_for_target( site,
                tripoint_abs_omt( 19, 20, 0 ), "" );
    REQUIRE( matched_lead != nullptr );
    CHECK( matched_lead->lead_id == lead.lead_id );

    prepare_hostile_follow_on( site, 2, 1, lead.target_id, lead.omt, 100 );
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, *matched_lead );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::toll );
    CHECK( plan.member_ids.size() == 3 );
    CHECK( plan.target_omt == lead.omt );
    REQUIRE_FALSE( plan.notes.empty() );
    CHECK( plan.notes.back().find( "profile camp_style" ) != std::string::npos );

    apply_test_hostile_dispatch( site, plan, 102 );
    CHECK( site.active_hostile_operation.reservation.job_type == "toll" );
    CHECK( site.active_hostile_operation.reservation.target_id == lead.target_id );
    CHECK( site.active_hostile_operation.reservation.target_omt == lead.omt );
    CHECK( site.remembered_bounty_estimate == lead.bounty );
    CHECK( site.active_hostile_operation.reservation.member_ids.size() == 3 );
}

TEST_CASE( "bandit_live_world_remembered_camp_map_lead_can_hold_when_no_opening",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    for( int i = 0; i < 5; ++i ) {
        add_bandit_camp_member( world, i, 12500 );
    }

    const bandit_live_world::site_record &site = world.sites.front();
    bandit_live_world::camp_map_lead lead;
    lead.lead_id = "active_basecamp@18,20,0";
    lead.kind = bandit_live_world::camp_lead_kind::basecamp_activity;
    lead.status = bandit_live_world::camp_lead_status::active;
    lead.target_id = "player@18,20,0";
    lead.omt = tripoint_abs_omt( 18, 20, 0 );
    lead.bounty = 8;
    lead.threat = 1;
    lead.confidence = 3;

    bandit_live_world::camp_map_dispatch_pressure pressure;
    pressure.opening_available = false;
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, lead, pressure );
    CHECK_FALSE( plan.valid );
    REQUIRE_FALSE( plan.notes.empty() );
    CHECK( plan.notes.back().find( "held pressure" ) != std::string::npos );
}

TEST_CASE( "bandit_live_world_live_signal_marks_write_camp_map_signal_leads",
           "[bandit][live_world][camp_map]" )
{
    bandit_live_world::world_state world;
    add_bandit_camp_member( world, 0, 12600 );
    bandit_live_world::site_record &site = world.sites.front();

    bandit_live_world::live_signal_mark smoke;
    smoke.mark_id = "live_smoke@18,20,0";
    smoke.kind = "smoke";
    smoke.source_omt = tripoint_abs_omt( 18, 20, 0 );
    smoke.range_cap_omt = 15;
    smoke.bounty_add = 2;
    smoke.threat_add = 1;
    smoke.confidence = 2;

    CHECK( bandit_live_world::record_live_signal_mark( site, smoke ) );
    REQUIRE( site.intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead &lead = site.intelligence_map.leads.front();
    CHECK( lead.kind == bandit_live_world::camp_lead_kind::smoke_signal );
    CHECK( lead.status == bandit_live_world::camp_lead_status::suspected );
    CHECK( lead.target_id == smoke.mark_id );
    CHECK( lead.omt == smoke.source_omt );
    CHECK( lead.radius_omt == smoke.range_cap_omt );
    CHECK( lead.bounty == smoke.bounty_add );
    CHECK( lead.threat == smoke.threat_add );
    CHECK( lead.confidence == smoke.confidence );
}
