#include "bandit_live_world.h"

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
    CHECK( site.active_group_id == "overmap_special:bandit_work_camp@40,50,0#dispatch" );
    CHECK( site.active_target_id == "player_basecamp_nearby" );
    REQUIRE( site.active_member_ids.size() == 1 );
    CHECK( site.active_member_ids.front() == character_id( 301 ) );
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
    CHECK( loaded_camp.active_group_id == "overmap_special:bandit_camp@10,20,0#dispatch" );
    CHECK( loaded_camp.active_target_id == "player@18,20,0" );
    REQUIRE( loaded_camp.active_member_ids == std::vector<character_id>( { character_id( 1001 ) } ) );
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
    CHECK( loaded_work_camp.active_group_id == "overmap_special:bandit_work_camp@40,50,0#dispatch" );
    CHECK( loaded_work_camp.active_target_id == "player@48,50,0" );
    REQUIRE( loaded_work_camp.active_member_ids == std::vector<character_id>( { character_id( 2001 ) } ) );
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
    CHECK( loaded_roadblock.active_group_id == "map_extra:mx_bandits_block@7,5,0#dispatch" );
    CHECK( loaded_roadblock.active_target_id == "player@8,5,0" );
    REQUIRE( loaded_roadblock.active_member_ids == std::vector<character_id>( { character_id( 3001 ) } ) );
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
    CHECK( loaded_camp.active_group_id.empty() );
    CHECK( loaded_camp.active_target_id.empty() );
    CHECK( loaded_camp.active_member_ids.empty() );
    CHECK( loaded_camp.find_member( character_id( 1001 ) )->state ==
           bandit_live_world::member_state::missing );
    CHECK( loaded_camp.find_spawn_tile( tripoint_abs_ms( 240, 480, 0 ) )->headcount == 0 );
    CHECK( loaded_camp.dispatchable_member_capacity() == 0 );
    CHECK_FALSE( bandit_live_world::plan_site_dispatch( loaded_camp, tripoint_abs_omt( 18, 20, 0 ),
                 "player@18,20,0" ).valid );

    CHECK( loaded_work_camp.headcount == 2 );
    CHECK( loaded_work_camp.active_group_id == "overmap_special:bandit_work_camp@40,50,0#dispatch" );
    REQUIRE( loaded_work_camp.active_member_ids == std::vector<character_id>( { character_id( 2001 ) } ) );
    CHECK( loaded_work_camp.find_member( character_id( 2001 ) )->state ==
           bandit_live_world::member_state::outbound );
    CHECK( loaded_work_camp.find_member( character_id( 2002 ) )->state ==
           bandit_live_world::member_state::at_home );
    REQUIRE( loaded_work_camp.known_recent_marks.size() == 1 );
    CHECK( loaded_work_camp.known_recent_marks.front() == "east-workcamp-pressure" );

    CHECK( loaded_roadblock.headcount == 1 );
    CHECK( loaded_roadblock.active_group_id == "map_extra:mx_bandits_block@7,5,0#dispatch" );
    REQUIRE( loaded_roadblock.active_member_ids == std::vector<character_id>( { character_id( 3001 ) } ) );
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
    site.active_sortie_started_minutes = 120;
    site.active_sortie_local_contact_minutes = 180;

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
    CHECK( site.active_group_id.empty() );
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
    active_outside_site.active_member_ids.push_back( character_id( 13700 ) );
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

    site.active_group_id = site.site_id + "#dispatch";
    site.active_member_ids.push_back( character_id( 14400 ) );

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
    CHECK( site.active_target_id == lead_id );
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
    CHECK( site.active_group_id.empty() );
    CHECK( site.active_target_id.empty() );
    CHECK( site.active_member_ids.empty() );
    CHECK( site.active_sortie_started_minutes == -1 );
    CHECK( site.active_sortie_local_contact_minutes == -1 );
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
    CHECK( site.active_group_id.empty() );
    CHECK( site.active_target_id.empty() );
    CHECK( site.active_member_ids.empty() );
    CHECK( site.active_sortie_started_minutes == -1 );
    CHECK( site.active_sortie_local_contact_minutes == -1 );
    CHECK( site.find_member( character_id( 14600 ) )->state == bandit_live_world::member_state::at_home );
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

TEST_CASE( "bandit_live_world_two_bandit_camp_commits_buddy_pair_after_scout_confirmation",
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

    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, confirmed );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::stalk );
    CHECK( plan.member_ids.size() == 2 );

    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    CHECK( site.active_job_type == "stalk" );
    CHECK( site.active_member_ids.size() == 2 );
    CHECK( site.count_members_in_state( bandit_live_world::member_state::at_home ) == 0 );
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
    site.active_member_ids = { character_id( 12000 ) };
    site.active_group_id = site.site_id + "#already_outside";

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
    REQUIRE( bandit_live_world::apply_dispatch_plan( cannibal_camp, cannibal_plan ) );

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
    CHECK( loaded_bandit->active_group_id == "overmap_special:bandit_camp@10,20,0#dispatch" );
    CHECK( loaded_bandit->active_target_id == "player@18,20,0" );
    REQUIRE( loaded_bandit->active_member_ids == std::vector<character_id>( { character_id( 660 ) } ) );
    CHECK( loaded_bandit->find_member( character_id( 661 ) )->state ==
           bandit_live_world::member_state::at_home );

    CHECK( loaded_cannibal->profile == bandit_live_world::hostile_site_profile::cannibal_camp );
    CHECK( loaded_cannibal->active_group_id == "overmap_special:cannibal_camp@70,80,0#dispatch" );
    CHECK( loaded_cannibal->active_target_id == "player@72,80,0" );
    REQUIRE( loaded_cannibal->active_member_ids == std::vector<character_id>( { character_id( 760 ),
             character_id( 761 ) } ) );
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
    CHECK( site.active_job_type == "scout" );

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

    site.active_job_type = "raid";
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
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 72, 80, 0 ),
                "player@72,80,0" );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::stalk );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    REQUIRE( site.active_member_ids.size() == 2 );

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
    site.active_member_ids.push_back( character_id( 902 ) );

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

TEST_CASE( "bandit_live_world_hold_off_goal_keeps_visible_standoff",
           "[bandit][live_world][local_gate]" )
{
    const tripoint_abs_omt player( 140, 41, 0 );
    const tripoint_abs_omt camp_anchor( 140, 51, 0 );
    const tripoint_abs_omt goal = bandit_live_world::choose_hold_off_standoff_goal(
                                      camp_anchor, player, 2 );

    CHECK( bandit_live_world::ordinary_scout_watch_standoff_omt() == 2 );
    CHECK( bandit_live_world::minimum_hold_off_standoff_omt() == 2 );
    CHECK( rl_dist( goal, player ) >= bandit_live_world::minimum_hold_off_standoff_omt() );
    CHECK( goal == tripoint_abs_omt( 140, 43, 0 ) );

    const tripoint_abs_omt diagonal_goal = bandit_live_world::choose_hold_off_standoff_goal(
            tripoint_abs_omt( 150, 51, 0 ), player, 2 );
    CHECK( diagonal_goal == tripoint_abs_omt( 142, 43, 0 ) );

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
    CHECK( site.active_job_type == "scout" );
    CHECK( site.active_member_ids.size() == 1 );

    REQUIRE( bandit_live_world::note_active_sortie_started( site, 100 ) );
    CHECK_FALSE( bandit_live_world::note_active_sortie_started( site, 105 ) );
    CHECK( bandit_live_world::ordinary_scout_sortie_limit_minutes() == 720 );
    CHECK_FALSE( bandit_live_world::scout_sortie_should_return_home( site, 819,
                 bandit_live_world::ordinary_scout_sortie_limit_minutes() ) );
    CHECK( bandit_live_world::scout_sortie_should_return_home( site, 820,
            bandit_live_world::ordinary_scout_sortie_limit_minutes() ) );
    REQUIRE( bandit_live_world::note_active_sortie_local_contact( site, 130 ) );
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
    CHECK( loaded_site.active_job_type == "scout" );
    CHECK( loaded_site.active_sortie_started_minutes == 100 );
    CHECK( loaded_site.active_sortie_local_contact_minutes == 130 );

    REQUIRE( bandit_live_world::update_member_state( site, site.active_member_ids.front(),
             bandit_live_world::member_state::local_contact, "watched long enough near player@18,20,0" ) );
    const std::vector<bandit_live_world::active_member_observation> still_watching = {
        { site.active_member_ids.front(),
          bandit_live_world::active_member_observation_state::returning_home,
          "scout sortie limit reached; returning home" }
    };
    CHECK_FALSE( bandit_live_world::resolve_active_group_aftermath( site, still_watching ).has_value() );

    const std::vector<bandit_live_world::active_member_observation> home = {
        { site.active_member_ids.front(), bandit_live_world::active_member_observation_state::home,
          "npc back on home footprint" }
    };
    const std::optional<bandit_pursuit_handoff::return_packet> packet =
        bandit_live_world::resolve_active_group_aftermath( site, home );
    REQUIRE( packet.has_value() );
    CHECK( packet->result == bandit_pursuit_handoff::mission_result::withdrawn );
    REQUIRE( bandit_live_world::apply_return_packet( site, *packet ) );
    CHECK( site.active_group_id.empty() );
    CHECK( site.active_job_type.empty() );
    CHECK( site.active_sortie_started_minutes == -1 );
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
    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch( site, tripoint_abs_omt( 72, 80, 0 ),
                "player@72,80,0" );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::stalk );
    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );

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
    site.active_member_ids.push_back( character_id( 952 ) );

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
    const std::string basecamp_report =
        bandit_live_world::render_shakedown_surface_report( site, basecamp_surface );
    CHECK( basecamp_report.find( "pay_option=yes" ) != std::string::npos );
    CHECK( basecamp_report.find( "fight_option=yes" ) != std::string::npos );
    CHECK( basecamp_report.find( "basecamp_inventory=yes" ) != std::string::npos );
    CHECK( basecamp_report.find( "vehicle_inventory=no" ) != std::string::npos );
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
    CHECK( site.active_group_id.empty() );
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
    REQUIRE( site.active_member_ids == std::vector<character_id>( { character_id( 801 ) } ) );

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
    CHECK( site.active_group_id.empty() );
    CHECK( site.active_target_id.empty() );
    CHECK( site.active_member_ids.empty() );
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
        site.active_group_id = site.site_id + "#dispatch";
        site.active_target_id = "player_basecamp_nearby";
        site.active_member_ids = { character_id( 10002 ) };
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
        site.active_group_id = site.site_id + "#dispatch";
        site.active_target_id = "player_basecamp_nearby";
        site.active_member_ids = { character_id( 10003 ) };
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
        CHECK( wiped_packet->group_id == site.active_group_id );
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
        CHECK( lost_packet->group_id == site.active_group_id );
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

    const bandit_live_world::dispatch_plan plan =
        bandit_live_world::plan_site_dispatch_from_camp_map_lead( site, *matched_lead );
    REQUIRE( plan.valid );
    CHECK( plan.entry.job_type == bandit_dry_run::job_template::stalk );
    CHECK( plan.member_ids.size() == 2 );
    CHECK( plan.target_omt == lead.omt );
    REQUIRE_FALSE( plan.notes.empty() );
    CHECK( plan.notes.back().find( "profile camp_style" ) != std::string::npos );

    REQUIRE( bandit_live_world::apply_dispatch_plan( site, plan ) );
    CHECK( site.active_job_type == "stalk" );
    CHECK( site.active_target_id == lead.target_id );
    CHECK( site.active_target_omt == lead.omt );
    CHECK( site.remembered_bounty_estimate == lead.bounty );
    CHECK( site.active_member_ids.size() == 2 );
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
