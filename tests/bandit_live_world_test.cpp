#include "bandit_live_world.h"

#include <optional>
#include <sstream>
#include <string>

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
} // namespace

TEST_CASE( "bandit live world claims one bounded special-backed site ledger", "[bandit][live_world]" )
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

TEST_CASE( "bandit live world keeps map-extra hostile spawns as micro-sites", "[bandit][live_world]" )
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

TEST_CASE( "bandit live world survives a save-style round trip", "[bandit][live_world]" )
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

TEST_CASE( "bandit live world keeps several hostile sites independent across save and writeback",
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

TEST_CASE( "bandit live world builds a bounded scout dispatch plan from owned members", "[bandit][live_world]" )
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

TEST_CASE( "bandit live world applies a dispatch plan by marking the selected member outbound", "[bandit][live_world]" )
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
    CHECK( second_plan.notes.back().find( "active outbound/contact group" ) != std::string::npos );
}

TEST_CASE( "bandit live world keeps a home reserve for site-backed camps", "[bandit][live_world]" )
{
    bandit_live_world::world_state world;
    REQUIRE( bandit_live_world::claim_tracked_spawn( world, "bandit", character_id( 601 ),
             tripoint_abs_ms( 240, 480, 0 ), std::string( "bandit_camp" ), std::nullopt,
             special_lookup ) );

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

TEST_CASE( "bandit live world dispatch rules are driven by hostile site profile", "[bandit][live_world][profile]" )
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

TEST_CASE( "bandit live world keeps cannibal camp separate from bandit camp ownership", "[bandit][live_world][profile][cannibal]" )
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
    CHECK( cannibal_camp.dispatchable_member_capacity() == 1 );
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
    CHECK( cannibal_plan.group.retreat_bias == 3 );
    CHECK( cannibal_plan.group.return_clock == 3 );
    CHECK( cannibal_plan.group.remaining_pressure ==
           bandit_pursuit_handoff::remaining_return_pressure_state::tight );
    REQUIRE( cannibal_plan.notes.size() >= 2 );
    CHECK( cannibal_plan.notes[cannibal_plan.notes.size() - 2].find( "hungry camp pressure" ) !=
           std::string::npos );
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
    REQUIRE( loaded_cannibal->active_member_ids == std::vector<character_id>( { character_id( 760 ) } ) );
    CHECK( loaded_cannibal->find_member( character_id( 761 ) )->state ==
           bandit_live_world::member_state::at_home );
    CHECK( loaded_cannibal->find_member( character_id( 762 ) )->state ==
           bandit_live_world::member_state::at_home );
}

TEST_CASE( "bandit live world writeback shrinks headcount and future dispatch capacity", "[bandit][live_world]" )
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

TEST_CASE( "bandit live world chooses reviewer-readable local approach gate posture", "[bandit][live_world][approach_gate]" )
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

TEST_CASE( "bandit live world builds a bounded pay-or-fight shakedown surface", "[bandit][live_world][shakedown]" )
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


TEST_CASE( "bandit live world records shakedown aftermath for renegotiation pressure", "[bandit][live_world][shakedown]" )
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

TEST_CASE( "bandit live world cools shakedown pressure when the active bandit is lost", "[bandit][live_world][shakedown]" )
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

TEST_CASE( "bandit live world applies a return packet onto the active owned outing", "[bandit][live_world]" )
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

TEST_CASE( "bandit live world resolves bounded live aftermath observations", "[bandit][live_world]" )
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
