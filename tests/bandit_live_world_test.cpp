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
    CHECK( looters->headcount == 1 );
    REQUIRE( looters->footprint.size() == 1 );
    CHECK( looters->footprint.front() == tripoint_abs_omt( 5, 4, 0 ) );

    const bandit_live_world::site_record *roadblock = world.find_site( "map_extra:mx_bandits_block@7,5,0" );
    REQUIRE( roadblock != nullptr );
    CHECK( roadblock->site_kind == bandit_live_world::owned_site_kind::bandits_block );
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
    CHECK( site.headcount == 2 );
    REQUIRE( site.footprint.size() == 9 );
    REQUIRE( site.members.size() == 2 );
    CHECK( site.members.front().npc_id == character_id( 301 ) );
    CHECK( site.members.front().npc_template_id == "bandit_quartermaster" );
    CHECK( site.members.back().npc_id == character_id( 302 ) );
    REQUIRE( site.spawn_tiles.size() == 2 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 960, 1200, 0 ) )->headcount == 1 );
    CHECK( site.find_spawn_tile( tripoint_abs_ms( 984, 1224, 0 ) )->headcount == 1 );
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
