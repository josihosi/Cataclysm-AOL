#include "ecology_debug_view.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "bandit_live_world.h"
#include "cata_catch.h"
#include "json.h"
#include "json_loader.h"

namespace
{

bandit_live_world::site_record make_site( int index, bool cannibal, int z = 0 )
{
    bandit_live_world::site_record site;
    site.site_id = "observer-site-" + std::to_string( index );
    site.source_kind = bandit_live_world::anchor_source_kind::overmap_special;
    site.site_kind = cannibal ? bandit_live_world::owned_site_kind::cannibal_camp :
                     bandit_live_world::owned_site_kind::bandit_camp;
    site.profile = cannibal ? bandit_live_world::hostile_site_profile::cannibal_camp :
                   bandit_live_world::hostile_site_profile::camp_style;
    site.source_id = cannibal ? "cannibal_camp" : "bandit_camp";
    site.anchor = tripoint_abs_omt( index, index % 7, z );
    site.footprint.push_back( site.anchor );
    site.living_total = 1;
    const character_id member_id( 500000 + index );
    site.members.push_back( { member_id, cannibal ? "cannibal_hunter" : "bandit",
                              project_to<coords::ms>( site.anchor ),
                              bandit_live_world::member_state::at_home, false, "" } );
    site.spawn_tiles.push_back( { project_to<coords::ms>( site.anchor ), 1 } );
    return site;
}

void add_active_outing( bandit_live_world::site_record &site, int generation = 1 )
{
    bandit_live_world::active_outing_state &outing = site.active_outing;
    outing.kind = bandit_live_world::outing_kind::scout_sortie;
    outing.activity_id = "observer-outing-" + std::to_string( generation ) + "-" + site.site_id;
    outing.camp_id = site.site_id;
    outing.generation = generation;
    outing.member_ids.push_back( site.members.front().npc_id );
    outing.leader_id = site.members.front().npc_id;
    outing.shared_route = { site.anchor,
                            tripoint_abs_omt( site.anchor.x() + 1, site.anchor.y(), site.anchor.z() ),
                            tripoint_abs_omt( site.anchor.x() + 2, site.anchor.y(), site.anchor.z() ) };
    outing.waypoint_index = 1;
    outing.target_id = "observer-target";
    outing.target_omt = outing.shared_route.back();
    outing.job_type = "scout";
    outing.phase = bandit_live_world::scout_phase::outbound;
    outing.started_minutes = 100;
    outing.last_progress_minutes = 120;
    outing.last_advanced_minutes = 120;
    outing.expected_return_minutes = 300;
    outing.missing_deadline_minutes = 420;
    outing.return_application_key = bandit_pursuit_handoff::make_operation_component_key(
                                         outing.activity_id, outing.generation, "return" );
    outing.report_application_key = bandit_pursuit_handoff::make_operation_component_key(
                                         outing.activity_id, outing.generation, "report" );
    outing.cargo_application_key = bandit_pursuit_handoff::make_operation_component_key(
                                        outing.activity_id, outing.generation, "cargo" );
    site.members.front().state = bandit_live_world::member_state::outbound;
}

std::string dispatch_id( const bandit_live_world::site_record &site )
{
    return "dispatch/" + site.site_id + "/" + site.active_outing.activity_id + "/" +
           std::to_string( site.active_outing.generation );
}

bandit_live_world::world_state round_trip( const bandit_live_world::world_state &world )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );
    JsonValue value = json_loader::from_string( out.str() );
    bandit_live_world::world_state loaded;
    loaded.deserialize( value.get_object() );
    return loaded;
}

std::string serialize_world( const bandit_live_world::world_state &world )
{
    std::ostringstream out;
    JsonOut jsout( out, true );
    world.serialize( jsout );
    return out.str();
}

bool entity_ordered( const ecology_debug::entity_marker &lhs,
                     const ecology_debug::entity_marker &rhs )
{
    return std::make_tuple( lhs.omt.z(), lhs.omt.y(), lhs.omt.x(),
                            static_cast<int>( lhs.kind ), lhs.id ) <
           std::make_tuple( rhs.omt.z(), rhs.omt.y(), rhs.omt.x(),
                            static_cast<int>( rhs.kind ), rhs.id );
}

} // namespace

TEST_CASE( "ecology_debug_view_is_gate_closed_and_side_effect_free",
           "[ecology_debug][observer][phase4]" )
{
    bandit_live_world::world_state world;
    world.sites.push_back( make_site( 1, false ) );
    add_active_outing( world.sites.front() );
    const std::string original_activity = world.sites.front().active_outing.activity_id;
    const std::string original_save = serialize_world( world );
    int loaded_reads = 0;
    int detail_reads = 0;
    int mobile_reads = 0;
    ecology_debug::query_request request;
    request.member_is_loaded = [&loaded_reads]( character_id ) {
        loaded_reads++;
        return true;
    };
    request.read_selected_member = [&detail_reads]( character_id ) {
        detail_reads++;
        return std::optional<ecology_debug::runtime_member_read>();
    };
    request.read_mobile_entities = [&mobile_reads]( const ecology_debug::query_region &,
    std::string_view, size_t ) {
        mobile_reads++;
        return std::vector<ecology_debug::mobile_entity_read>();
    };

    const ecology_debug::view_snapshot closed = ecology_debug::query_bandit_ecology( world,
            request );

    CHECK( closed.entities.empty() );
    CHECK_FALSE( closed.selected );
    CHECK( closed.metadata.candidate_count == 0 );
    CHECK( closed.metadata.query_microseconds == 0 );
    CHECK( loaded_reads == 0 );
    CHECK( detail_reads == 0 );
    CHECK( mobile_reads == 0 );
    CHECK( world.sites.front().active_outing.activity_id == original_activity );
    CHECK( world.sites.front().members.front().state ==
           bandit_live_world::member_state::outbound );
    CHECK( serialize_world( world ) == original_save );
}

TEST_CASE( "ecology_debug_view_merges_filtered_mobile_entities_deterministically",
           "[ecology_debug][observer][phase4]" )
{
    ecology_debug::mobile_entity_read lower_horde;
    lower_horde.id = "horde/lower";
    lower_horde.kind = ecology_debug::entity_kind::zombie_horde;
    lower_horde.faction = ecology_debug::entity_faction::zombie;
    lower_horde.omt = tripoint_abs_omt( 4, 5, -1 );
    lower_horde.owner = ecology_debug::entity_owner::abstract;
    lower_horde.state = "roaming";
    lower_horde.generation = 3;
    lower_horde.population = 24;
    lower_horde.interest = 61;
    lower_horde.target = tripoint_abs_omt( 8, 9, -1 );
    lower_horde.hp_percent = 9;

    ecology_debug::mobile_entity_read lower_stalker;
    lower_stalker.id = "stalker/lower";
    lower_stalker.kind = ecology_debug::entity_kind::writhing_stalker;
    lower_stalker.faction = ecology_debug::entity_faction::zombie;
    lower_stalker.omt = lower_horde.omt;
    lower_stalker.owner = ecology_debug::entity_owner::concrete;
    lower_stalker.loaded = true;
    lower_stalker.state = "tracking";
    lower_stalker.generation = 7;
    lower_stalker.population = 99;
    lower_stalker.interest = 88;
    lower_stalker.target = tripoint_abs_omt( 7, 7, -1 );
    lower_stalker.hp_percent = 42;

    ecology_debug::mobile_entity_read upper_horde = lower_horde;
    upper_horde.id = "horde/upper";
    upper_horde.omt = tripoint_abs_omt( 4, 5, 1 );
    upper_horde.target = tripoint_abs_omt( 8, 9, 1 );

    ecology_debug::mobile_entity_read missing_id = lower_horde;
    missing_id.id.clear();
    ecology_debug::mobile_entity_read mismatched_faction = lower_horde;
    mismatched_faction.id = "horde/not-a-zombie";
    mismatched_faction.faction = ecology_debug::entity_faction::bandit;
    ecology_debug::mobile_entity_read unsupported_kind = lower_horde;
    unsupported_kind.id = "camp/not-mobile";
    unsupported_kind.kind = ecology_debug::entity_kind::bandit_camp;
    unsupported_kind.faction = ecology_debug::entity_faction::bandit;

    const std::vector<ecology_debug::mobile_entity_read> rows = {
        upper_horde, lower_stalker, missing_id, lower_horde, mismatched_faction, unsupported_kind
    };
    int mobile_reads = 0;
    ecology_debug::query_region callback_region;
    std::string callback_selected_id;
    size_t callback_cap = 0;
    ecology_debug::query_request request;
    request.enabled = true;
    request.filters.hordes = true;
    request.filters.stalkers = true;
    request.region.enabled = true;
    request.region.minimum = tripoint_abs_omt( -10, -10, -2 );
    request.region.maximum = tripoint_abs_omt( 10, 10, 2 );
    request.selected_id = lower_horde.id;
    request.read_mobile_entities = [&rows, &mobile_reads, &callback_region,
    &callback_selected_id, &callback_cap]( const ecology_debug::query_region & region,
    std::string_view selected_id, size_t cap ) {
        mobile_reads++;
        callback_region = region;
        callback_selected_id = selected_id;
        callback_cap = cap;
        return rows;
    };

    const bandit_live_world::world_state world;
    const ecology_debug::view_snapshot view = ecology_debug::query_bandit_ecology( world,
            request );

    CHECK( mobile_reads == 1 );
    CHECK( callback_region.enabled );
    CHECK( callback_region.minimum == request.region.minimum );
    CHECK( callback_region.maximum == request.region.maximum );
    CHECK( callback_selected_id == request.selected_id );
    CHECK( callback_cap == ecology_debug::candidate_cap );
    REQUIRE( view.entities.size() == 3 );
    CHECK( view.metadata.candidate_count == 3 );
    CHECK( std::is_sorted( view.entities.begin(), view.entities.end(), entity_ordered ) );
    CHECK( view.entities[0].id == lower_horde.id );
    CHECK( view.entities[1].id == lower_stalker.id );
    CHECK( view.entities[2].id == upper_horde.id );
    CHECK( view.entities[0].owner == "abstract" );
    CHECK( view.entities[1].owner == "concrete" );
    CHECK( view.entities[1].loaded );
    REQUIRE( view.selected );
    CHECK( view.selected->phase == "roaming" );
    CHECK( view.selected->population == 24 );
    CHECK( view.selected->interest == 61 );
    REQUIRE( view.selected->target );
    CHECK( *view.selected->target == tripoint_abs_omt( 8, 9, -1 ) );
    CHECK_FALSE( view.selected->hp_percent );

    request.filters.hordes = false;
    request.selected_id = lower_stalker.id;
    const ecology_debug::view_snapshot stalkers = ecology_debug::query_bandit_ecology( world,
            request );
    REQUIRE( stalkers.entities.size() == 1 );
    CHECK( stalkers.entities.front().kind == ecology_debug::entity_kind::writhing_stalker );
    REQUIRE( stalkers.selected );
    CHECK( stalkers.selected->hp_percent == 42 );
    CHECK_FALSE( stalkers.selected->population );
    CHECK_FALSE( stalkers.selected->interest );
    CHECK_FALSE( stalkers.selected->target );
    CHECK( stalkers.selected->destination == lower_stalker.omt );

    request.filters.hordes = true;
    request.filters.stalkers = false;
    request.selected_id.clear();
    const ecology_debug::view_snapshot hordes = ecology_debug::query_bandit_ecology( world,
            request );
    REQUIRE( hordes.entities.size() == 2 );
    CHECK( std::all_of( hordes.entities.begin(), hordes.entities.end(), []( const auto & marker ) {
        return marker.kind == ecology_debug::entity_kind::zombie_horde;
    } ) );

    request.filters.loaded_only = true;
    const ecology_debug::view_snapshot loaded_hordes = ecology_debug::query_bandit_ecology( world,
            request );
    CHECK( loaded_hordes.entities.empty() );
}

TEST_CASE( "ecology_debug_view_rejects_ambiguous_canonical_ids",
           "[ecology_debug][observer][phase4]" )
{
    bandit_live_world::world_state world;
    world.sites.push_back( make_site( 1, false ) );

    ecology_debug::mobile_entity_read first;
    first.id = "mobile/duplicate";
    first.kind = ecology_debug::entity_kind::zombie_horde;
    first.faction = ecology_debug::entity_faction::zombie;
    first.omt = tripoint_abs_omt( 2, 2, 0 );
    ecology_debug::mobile_entity_read second = first;
    second.omt = tripoint_abs_omt( 3, 3, 0 );
    ecology_debug::mobile_entity_read camp_collision = first;
    camp_collision.id = "camp/observer-site-1";
    camp_collision.omt = world.sites.front().anchor;
    ecology_debug::mobile_entity_read unique = first;
    unique.id = "mobile/unique";
    unique.omt = tripoint_abs_omt( 4, 4, 0 );

    ecology_debug::query_request request;
    request.enabled = true;
    request.filters.hordes = true;
    request.filters.stalkers = true;
    request.selected_id = first.id;
    request.read_mobile_entities = [first, second, camp_collision, unique](
    const ecology_debug::query_region &, std::string_view, size_t ) {
        return std::vector<ecology_debug::mobile_entity_read> {
            first, second, camp_collision, unique
        };
    };

    const ecology_debug::view_snapshot view = ecology_debug::query_bandit_ecology( world,
            request );

    REQUIRE( view.entities.size() == 1 );
    CHECK( view.entities.front().id == unique.id );
    CHECK_FALSE( view.selected );
    CHECK( view.metadata.candidate_count == 1 );
    CHECK_FALSE( view.metadata.truncated );
}

TEST_CASE( "ecology_debug_view_sorts_co_located_z_levels_and_reads_selected_detail_only",
           "[ecology_debug][observer][phase4]" )
{
    bandit_live_world::world_state world;
    world.sites.push_back( make_site( 4, true, 1 ) );
    world.sites.push_back( make_site( 4, false, 0 ) );
    world.sites.back().site_id += "-bandit";
    for( bandit_live_world::site_record &site : world.sites ) {
        add_active_outing( site, site.profile ==
                           bandit_live_world::hostile_site_profile::cannibal_camp ? 2 : 1 );
        site.active_outing.shared_route[1] = site.anchor;
    }
    bandit_live_world::active_outing_state &selected_outing = world.sites.front().active_outing;
    selected_outing.owner = bandit_live_world::simulation_owner::local;
    selected_outing.handoff_epoch = 1;
    selected_outing.local_contact_minutes = 130;
    selected_outing.last_advanced_minutes = 130;
    selected_outing.local_handoff.activity_id = selected_outing.activity_id;
    selected_outing.local_handoff.activity_generation = selected_outing.generation;
    selected_outing.local_handoff.handoff_epoch = selected_outing.handoff_epoch;
    selected_outing.local_handoff.waypoint_index = selected_outing.waypoint_index;
    selected_outing.local_handoff.phase = selected_outing.phase;
    selected_outing.local_handoff.route_position = world.sites.front().anchor;
    selected_outing.local_handoff.committed_minutes = 130;
    selected_outing.local_handoff.members.push_back( {
        world.sites.front().members.front().npc_id, tripoint_abs_ms(), tripoint_abs_ms(),
        tripoint_abs_ms(), tripoint_abs_ms(), 55, false
    } );
    selected_outing.observations.emplace_back(
        "burn@140,51,1", "route burned", 80, 140, true,
        bandit_live_world::sortie_observation_kind::burn, "burned" );
    const std::string selected_id = dispatch_id( world.sites.front() );
    int detail_reads = 0;
    ecology_debug::query_request request;
    request.enabled = true;
    request.now_minutes = 150;
    request.selected_id = selected_id;
    request.member_is_loaded = [&world]( character_id member_id ) {
        return member_id == world.sites.front().members.front().npc_id;
    };
    request.read_selected_member = [&detail_reads]( character_id ) {
        detail_reads++;
        return ecology_debug::runtime_member_read { "Selected cannibal", 73, true };
    };

    const ecology_debug::view_snapshot view = ecology_debug::query_bandit_ecology( world, request );

    REQUIRE( view.entities.size() == 4 );
    CHECK( std::is_sorted( view.entities.begin(), view.entities.end(), entity_ordered ) );
    CHECK( view.entities[0].omt.z() == 0 );
    CHECK( view.entities[2].omt.z() == 1 );
    CHECK( view.entities[2].omt == view.entities[3].omt );
    CHECK( view.entities[2].id != view.entities[3].id );
    CHECK( view.entities[3].owner == "local" );
    REQUIRE( view.selected );
    CHECK( view.selected->entity_id == selected_id );
    CHECK( view.selected->source_camp_id == "camp/observer-site-4" );
    CHECK( view.selected->phase == "outbound" );
    CHECK( view.selected->last_transition_minutes == 120 );
    CHECK( view.selected->next_deadline_minutes == 300 );
    CHECK( view.selected->blocked_reason == "awaiting_local_cohesion" );
    CHECK( view.selected->evidence_id == "burn@140,51,1" );
    CHECK( view.selected->evidence_kind == "burn" );
    CHECK( view.selected->evidence_state == "burned" );
    CHECK( view.selected->evidence_observed_minutes == 140 );
    CHECK( view.selected->evidence_age_minutes == 10 );
    REQUIRE( view.selected->members.size() == 1 );
    CHECK( view.selected->members.front().name == "Selected cannibal" );
    CHECK( view.selected->members.front().hp_percent == 73 );
    CHECK( detail_reads == 1 );
    CHECK( view.entities[2].alias != view.entities[3].alias );

    request.filters.loaded_only = true;
    const ecology_debug::view_snapshot loaded_only = ecology_debug::query_bandit_ecology( world,
            request );
    CHECK( loaded_only.metadata.candidate_count == 2 );
    CHECK( loaded_only.entities.size() == 2 );
    CHECK_FALSE( loaded_only.metadata.truncated );
}

TEST_CASE( "ecology_debug_view_enforces_exact_caps_and_forces_selection",
           "[ecology_debug][observer][phase4]" )
{
    bandit_live_world::world_state world;
    world.sites.reserve( 1100 );
    for( int index = 0; index < 1100; ++index ) {
        world.sites.push_back( make_site( index, index % 2 != 0 ) );
        add_active_outing( world.sites.back(), index + 1 );
    }
    const std::string selected_id = dispatch_id( world.sites.back() );
    ecology_debug::query_request request;
    request.enabled = true;
    request.selected_id = selected_id;

    const ecology_debug::view_snapshot view = ecology_debug::query_bandit_ecology( world, request );

    CHECK( ecology_debug::candidate_cap == 2048 );
    CHECK( ecology_debug::marker_cap == 256 );
    CHECK( ecology_debug::delta_cap == 128 );
    CHECK( view.metadata.candidate_count == 2200 );
    CHECK( view.metadata.considered_count == 2048 );
    CHECK( view.metadata.emitted_count == 256 );
    CHECK( view.metadata.dropped_count == 1944 );
    CHECK( view.metadata.truncated );
    CHECK( std::is_sorted( view.entities.begin(), view.entities.end(), entity_ordered ) );
    CHECK( std::any_of( view.entities.begin(), view.entities.end(), [&selected_id](
    const ecology_debug::entity_marker & marker ) {
        return marker.id == selected_id;
    } ) );
    REQUIRE( view.selected );
    CHECK( view.selected->entity_id == selected_id );
}

TEST_CASE( "ecology_debug_view_keeps_selected_entity_outside_query_region",
           "[ecology_debug][observer][phase4]" )
{
    bandit_live_world::world_state world;
    world.sites.push_back( make_site( 80, false ) );
    add_active_outing( world.sites.front() );
    ecology_debug::query_request request;
    request.enabled = true;
    request.region.enabled = true;
    request.region.minimum = tripoint_abs_omt( -5, -5, 0 );
    request.region.maximum = tripoint_abs_omt( 5, 5, 0 );
    request.selected_id = dispatch_id( world.sites.front() );

    const ecology_debug::view_snapshot view = ecology_debug::query_bandit_ecology( world, request );

    REQUIRE( view.entities.size() == 1 );
    CHECK( view.entities.front().id == request.selected_id );
    REQUIRE( view.selected );
    CHECK( view.selected->entity_id == request.selected_id );
}

TEST_CASE( "ecology_debug_view_resolves_selected_entity_by_authority_index",
           "[ecology_debug][observer][phase4]" )
{
    bandit_live_world::world_state world;
    for( int index = 0; index < 90; ++index ) {
        world.sites.push_back( make_site( index, false ) );
    }
    add_active_outing( world.sites[80] );
    ecology_debug::query_request request;
    request.enabled = true;
    request.selected_id = dispatch_id( world.sites[80] );

    const ecology_debug::view_snapshot selected =
        ecology_debug::query_selected_bandit_ecology( world, request, 80 );

    REQUIRE( selected.entities.size() == 1 );
    CHECK( selected.entities.front().id == request.selected_id );
    CHECK( selected.entities.front().authority_index == 80 );
    CHECK( selected.metadata.candidate_count == 1 );
    CHECK( selected.metadata.considered_count == 1 );
    REQUIRE( selected.selected );
    CHECK( selected.selected->entity_id == request.selected_id );

    const ecology_debug::view_snapshot stale =
        ecology_debug::query_selected_bandit_ecology( world, request, 79 );
    CHECK( stale.entities.empty() );
    CHECK_FALSE( stale.selected );
}

TEST_CASE( "ecology_debug_view_uses_authoritative_camp_and_active_dispatch_members",
           "[ecology_debug][observer][phase4]" )
{
    SECTION( "unmaterialized camps appear and small hostile sites do not" ) {
        bandit_live_world::world_state world;
        bandit_live_world::site_record abstract_camp = make_site( 12, false );
        abstract_camp.living_total = 3;
        abstract_camp.members.clear();
        abstract_camp.spawn_tiles.clear();
        world.sites.push_back( std::move( abstract_camp ) );
        bandit_live_world::site_record looters = make_site( 13, false );
        looters.site_kind = bandit_live_world::owned_site_kind::looters;
        looters.profile = bandit_live_world::hostile_site_profile::small_hostile_site;
        world.sites.push_back( std::move( looters ) );
        ecology_debug::query_request request;
        request.enabled = true;
        request.filters.dispatches = false;

        const ecology_debug::view_snapshot view = ecology_debug::query_bandit_ecology( world,
                request );

        REQUIRE( view.entities.size() == 1 );
        CHECK( view.entities.front().id == "camp/observer-site-12" );
        CHECK( view.entities.front().owner == "abstract" );
    }

    SECTION( "resolved home member cannot make a remote dispatch loaded" ) {
        bandit_live_world::world_state world;
        world.sites.push_back( make_site( 14, false ) );
        add_active_outing( world.sites.front(), 4 );
        bandit_live_world::site_record &site = world.sites.front();
        const character_id resolved_id = site.members.front().npc_id;
        const character_id active_id( 600014 );
        site.members.push_back( { active_id, "bandit", project_to<coords::ms>( site.anchor ),
                                  bandit_live_world::member_state::outbound, false, "" } );
        site.living_total = 2;
        site.active_outing.member_ids.push_back( active_id );
        site.active_outing.resolved_member_ids.push_back( resolved_id );
        site.members.front().state = bandit_live_world::member_state::at_home;
        ecology_debug::query_request request;
        request.enabled = true;
        request.filters.camps = false;
        request.filters.loaded_only = true;
        request.member_is_loaded = [resolved_id]( character_id member_id ) {
            return member_id == resolved_id;
        };

        const ecology_debug::view_snapshot view = ecology_debug::query_bandit_ecology( world,
                request );

        CHECK( view.metadata.candidate_count == 0 );
        CHECK( view.entities.empty() );
        CHECK_FALSE( view.metadata.truncated );
    }

    SECTION( "lost outing with a live unresolved member has no marker" ) {
        bandit_live_world::world_state world;
        world.sites.push_back( make_site( 15, true ) );
        add_active_outing( world.sites.front(), 5 );
        world.sites.front().active_outing.phase = bandit_live_world::scout_phase::lost;
        ecology_debug::query_request request;
        request.enabled = true;
        request.filters.camps = false;

        const ecology_debug::view_snapshot view = ecology_debug::query_bandit_ecology( world,
                request );

        CHECK( view.metadata.candidate_count == 0 );
        CHECK( view.entities.empty() );
    }
}

TEST_CASE( "ecology_debug_view_removes_terminal_entities_and_survives_owner_save_load",
           "[ecology_debug][observer][phase4][save]" )
{
    bandit_live_world::world_state world;
    world.sites.push_back( make_site( 8, false, -1 ) );
    add_active_outing( world.sites.front(), 3 );
    const std::string selected_id = dispatch_id( world.sites.front() );
    ecology_debug::query_request request;
    request.enabled = true;
    request.selected_id = selected_id;

    const ecology_debug::view_snapshot before = ecology_debug::query_bandit_ecology( world,
            request );
    REQUIRE( before.entities.size() == 2 );
    const bandit_live_world::world_state loaded = round_trip( world );
    const ecology_debug::view_snapshot after_load = ecology_debug::query_bandit_ecology( loaded,
            request );
    REQUIRE( after_load.entities.size() == before.entities.size() );
    CHECK( after_load.entities[0].id == before.entities[0].id );
    CHECK( after_load.entities[1].id == before.entities[1].id );
    CHECK( after_load.entities[0].omt.z() == -1 );

    world.sites.front().active_outing.casualty_ids.push_back(
        world.sites.front().members.front().npc_id );
    world.sites.front().members.front().state = bandit_live_world::member_state::dead;
    world.sites.front().living_total = 0;
    const ecology_debug::view_snapshot terminal = ecology_debug::query_bandit_ecology( world,
            request );
    CHECK( terminal.entities.empty() );
    CHECK_FALSE( terminal.selected );

    world.sites.front().active_outing.clear();
    world.sites.front().members.front().state = bandit_live_world::member_state::at_home;
    world.sites.front().living_total = 1;
    const ecology_debug::view_snapshot returned = ecology_debug::query_bandit_ecology( world,
            request );
    REQUIRE( returned.entities.size() == 1 );
    CHECK( returned.entities.front().id == "camp/observer-site-8" );
    CHECK_FALSE( returned.selected );
}
