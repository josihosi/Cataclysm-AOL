#include "bandit_live_world.h"

#include <string>

#include "cata_catch.h"

namespace
{

bandit_live_world::site_record make_routine_camp( const std::string &site_id,
        const tripoint_abs_omt &anchor, const int member_base )
{
    bandit_live_world::site_record site;
    site.site_id = site_id;
    site.source_kind = bandit_live_world::anchor_source_kind::overmap_special;
    site.site_kind = bandit_live_world::owned_site_kind::bandit_camp;
    site.profile = bandit_live_world::hostile_site_profile::camp_style;
    site.source_id = "bandit_camp";
    site.anchor = anchor;
    site.footprint.push_back( anchor );
    site.living_total = 3;
    for( int offset = 0; offset < 3; ++offset ) {
        const tripoint_abs_ms home = project_to<coords::ms>( anchor ) + point( offset, 0 );
        site.members.push_back( { character_id( member_base + offset ), "bandit", home,
                                  bandit_live_world::member_state::at_home, false, "" } );
        site.spawn_tiles.push_back( { home, 1 } );
    }
    return site;
}

} // namespace

TEST_CASE( "authoritative player opportunity adopts once then dispatches the routine scout pair",
           "[bandit][r009_m055]" )
{
    bandit_live_world::world_state world;
    // Deliberately insert reverse lexical order: adoption must select the stable camp identity,
    // not insertion order, and must not need a fixture-authored lead.
    world.sites.push_back( make_routine_camp( "z-camp", tripoint_abs_omt( 12, 0, 0 ), 1000 ) );
    world.sites.push_back( make_routine_camp( "a-camp", tripoint_abs_omt( 0, 0, 0 ), 2000 ) );
    const std::string target_id = "player@2,0,0";
    const tripoint_abs_omt target_omt( 2, 0, 0 );
    REQUIRE( bandit_live_world::observe_authoritative_hostile_target_opportunity( world,
             target_id, target_omt, { 80, 1, 1 } ) );
    REQUIRE( world.sites[0].intelligence_map.leads.empty() );
    REQUIRE( world.sites[1].intelligence_map.leads.empty() );

    const int adopted = bandit_live_world::adopt_observed_hostile_player_opportunities( world,
                        60, []( const bandit_live_world::site_record &,
    const bandit_live_world::hostile_target_opportunity_record & ) {
        return true;
    } );
    REQUIRE( adopted == 1 );
    REQUIRE( world.sites[0].intelligence_map.leads.empty() );
    REQUIRE( world.sites[1].intelligence_map.leads.size() == 1 );
    const bandit_live_world::camp_map_lead &adopted_lead =
        world.sites[1].intelligence_map.leads.front();
    CHECK( adopted_lead.target_id == target_id );
    CHECK( adopted_lead.omt == target_omt );
    CHECK( adopted_lead.revision == 1 );
    CHECK( adopted_lead.kind == bandit_live_world::camp_lead_kind::terrain_opportunity );
    CHECK( adopted_lead.generated_by_this_camp_routine );
    CHECK( adopted_lead.last_outcome == "authoritative_player_opportunity_adopted" );
    const std::string adopted_lead_id = adopted_lead.lead_id;

    CHECK( bandit_live_world::adopt_observed_hostile_player_opportunities( world, 60,
    []( const bandit_live_world::site_record &,
    const bandit_live_world::hostile_target_opportunity_record & ) {
        return true;
    } ) == 0 );
    CHECK( world.sites[1].intelligence_map.leads.size() == 1 );

    const bandit_live_world::structural_bounty_maintenance_result maintenance =
        bandit_live_world::advance_structural_bounty_maintenance( world, 60, 0, 1,
    []( const tripoint_abs_omt & ) -> std::optional<std::string> {
        return std::nullopt;
    }, []( const bandit_live_world::site_record &,
    const bandit_live_world::camp_map_lead & ) {
        return bandit_live_world::structural_threat_read{ 0, true, "test-clear" };
    }, []( const bandit_live_world::site_record &,
    const bandit_live_world::structural_outing_plan & ) {
        return bandit_live_world::structural_route_read{ true, 4, 0, "test-routable" };
    } );

    REQUIRE( maintenance.dispatches_applied == 1 );
    const bandit_live_world::site_record &dispatched = world.sites[1];
    CHECK( dispatched.active_outing.kind == bandit_live_world::outing_kind::structural_sortie );
    CHECK( dispatched.active_outing.target_id == adopted_lead_id );
    REQUIRE( world.find_hostile_target_opportunity( target_id, target_omt ) != nullptr );
    CHECK( world.find_hostile_target_opportunity( target_id, target_omt )->revision == 1 );
    CHECK( dispatched.active_outing.member_ids == std::vector<character_id>{
               character_id( 2000 ), character_id( 2001 ) } );
}

TEST_CASE( "consumed player opportunity is never adopted", "[bandit][r009_m055]" )
{
    bandit_live_world::world_state world;
    world.sites.push_back( make_routine_camp( "a-camp", tripoint_abs_omt( 0, 0, 0 ), 3000 ) );
    world.hostile_target_opportunities.push_back( { "player@2,0,0", tripoint_abs_omt( 2, 0, 0 ),
            80, 1, 1, 1, "operation", "report", 1 } );

    CHECK( bandit_live_world::adopt_observed_hostile_player_opportunities( world, 60,
    []( const bandit_live_world::site_record &,
    const bandit_live_world::hostile_target_opportunity_record & ) {
        return true;
    } ) == 0 );
    CHECK( world.sites.front().intelligence_map.leads.empty() );

    world.hostile_target_opportunities.clear();
    world.hostile_target_opportunities.push_back( { "player@2,0,0", tripoint_abs_omt( 2, 0, 0 ),
            80, 1, 1, 1, "", "", 0 } );
    CHECK( bandit_live_world::adopt_observed_hostile_player_opportunities( world, 60,
    []( const bandit_live_world::site_record &,
    const bandit_live_world::hostile_target_opportunity_record & ) {
        return false;
    } ) == 0 );
    CHECK( world.sites.front().intelligence_map.leads.empty() );

    world.hostile_target_opportunities.front().revision = 0;
    CHECK( bandit_live_world::adopt_observed_hostile_player_opportunities( world, 60,
    []( const bandit_live_world::site_record &,
    const bandit_live_world::hostile_target_opportunity_record & ) {
        return true;
    } ) == 0 );
    CHECK( world.sites.front().intelligence_map.leads.empty() );
}
