#include "overmap_ui.h"

#include <algorithm>
#include <string>

#include "avatar.h"
#include "bandit_live_world.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "creature_tracker.h"
#include "game.h"
#include "json_loader.h"
#include "map_helpers.h"
#include "migo_population.h"
#include "migo_tower_ai.h"
#include "npc.h"
#include "overmapbuffer.h"
#include "player_helpers.h"
#include "type_id.h"

static const trait_id trait_DEBUG_CLAIRVOYANCE( "DEBUG_CLAIRVOYANCE" );

TEST_CASE( "ecology_observer_marker_legend_is_stable_and_distinct",
           "[ecology_debug][observer_ui][phase4]" )
{
    const std::pair<std::string, nc_color> bandit_camp =
        overmap_ui::ecology_marker_display( ecology_debug::entity_kind::bandit_camp );
    const std::pair<std::string, nc_color> cannibal_camp =
        overmap_ui::ecology_marker_display( ecology_debug::entity_kind::cannibal_camp );
    const std::pair<std::string, nc_color> bandit_dispatch =
        overmap_ui::ecology_marker_display( ecology_debug::entity_kind::bandit_dispatch );
    const std::pair<std::string, nc_color> cannibal_dispatch =
        overmap_ui::ecology_marker_display( ecology_debug::entity_kind::cannibal_dispatch );
    const std::pair<std::string, nc_color> zombie_horde =
        overmap_ui::ecology_marker_display( ecology_debug::entity_kind::zombie_horde );
    const std::pair<std::string, nc_color> writhing_stalker =
        overmap_ui::ecology_marker_display( ecology_debug::entity_kind::writhing_stalker );

    CHECK( bandit_camp.first == "B" );
    CHECK( cannibal_camp.first == "C" );
    CHECK( bandit_dispatch.first == "b" );
    CHECK( cannibal_dispatch.first == "c" );
    CHECK( zombie_horde.first == "H" );
    CHECK( writhing_stalker.first == "S" );
    CHECK( overmap_ui::ecology_marker_display( ecology_debug::entity_kind::migo_tower ).first == "M" );
    CHECK( overmap_ui::ecology_marker_display( ecology_debug::entity_kind::migo_dispatch ).first ==
           "m" );
    CHECK( bandit_camp.second == c_light_red );
    CHECK( cannibal_camp.second == c_red );
    CHECK( bandit_dispatch.second == c_yellow );
    CHECK( cannibal_dispatch.second == c_pink );
    CHECK( zombie_horde.second == c_light_green );
    CHECK( writhing_stalker.second == c_dark_gray );
    ecology_debug::entity_marker marker;
    marker.kind = ecology_debug::entity_kind::bandit_dispatch;
    marker.position_basis = "checkpoint";
    CHECK( overmap_ui::ecology_marker_display( marker ).first == "?" );
    marker.position_basis = "destination";
    CHECK( overmap_ui::ecology_marker_display( marker ).first == "?" );
    marker.position_basis = "current_member";
    CHECK( overmap_ui::ecology_marker_display( marker ).first == "b" );
}

TEST_CASE( "ecology_observer_viewport_counts_match_tiles_renderer_formula",
           "[ecology_debug][observer_ui][phase4]" )
{
    CHECK( overmap_ui::ecology_viewport_tile_counts( point( 800, 600 ), point( 20, 20 ),
            false ) == point( 40, 30 ) );
    CHECK( overmap_ui::ecology_viewport_tile_counts( point( 800, 600 ), point( 20, 20 ),
            true ) == point( 81, 121 ) );
}

TEST_CASE( "ecology_observer_co_located_marker_prefers_selection",
           "[ecology_debug][observer_ui][phase4]" )
{
    overmap_ui::overmap_draw_data_t data;
    const tripoint_abs_omt shared_omt( 15, 22, -1 );
    ecology_debug::entity_marker camp;
    camp.id = "camp/shared";
    camp.kind = ecology_debug::entity_kind::bandit_camp;
    camp.omt = shared_omt;
    ecology_debug::entity_marker dispatch;
    dispatch.id = "dispatch/shared/1";
    dispatch.kind = ecology_debug::entity_kind::bandit_dispatch;
    dispatch.omt = shared_omt;
    data.ecology_view.entities = { camp, dispatch };

    const ecology_debug::entity_marker *first = overmap_ui::ecology_marker_at( data, shared_omt );
    REQUIRE( first != nullptr );
    CHECK( first->id == camp.id );

    data.ecology_selected_id = dispatch.id;
    const ecology_debug::entity_marker *selected = overmap_ui::ecology_marker_at( data, shared_omt );
    REQUIRE( selected != nullptr );
    CHECK( selected->id == dispatch.id );
    CHECK( overmap_ui::ecology_marker_at( data, tripoint_abs_omt( 15, 22, 0 ) ) == nullptr );
}

TEST_CASE( "ecology_observer_selection_follows_only_when_pinned",
           "[ecology_debug][observer_ui][phase4]" )
{
    overmap_ui::overmap_draw_data_t data;
    data.cursor_pos = tripoint_abs_omt( 15, 22, 0 );
    ecology_debug::entity_marker dispatch;
    dispatch.id = "dispatch/moving/1";
    dispatch.kind = ecology_debug::entity_kind::bandit_dispatch;
    dispatch.omt = tripoint_abs_omt( 16, 22, 0 );
    data.ecology_view.entities = { dispatch };
    data.ecology_selected_id = dispatch.id;

    overmap_ui::reconcile_ecology_selection( data );
    CHECK( data.ecology_selected_id.empty() );
    CHECK( data.cursor_pos == tripoint_abs_omt( 15, 22, 0 ) );

    data.ecology_selected_id = dispatch.id;
    data.ecology_pinned = true;
    overmap_ui::reconcile_ecology_selection( data );
    CHECK( data.ecology_selected_id == dispatch.id );
    CHECK( data.cursor_pos == dispatch.omt );
}

TEST_CASE( "ecology_observer_cache_tracks_exact_region_and_follow_center",
           "[ecology_debug][observer_ui][phase4]" )
{
    overmap_ui::overmap_draw_data_t data;
    data.cursor_pos = tripoint_abs_omt( 15, 22, 0 );
    data.ecology_last_query_cursor = data.cursor_pos;
    data.ecology_last_query_turn = 17;
    data.ecology_filter = overmap_ui::ecology_filter_mode::dispatches;
    data.ecology_last_query_filter = data.ecology_filter;
    data.ecology_faction_filter = overmap_ui::ecology_faction_filter_mode::bandits;
    data.ecology_last_query_faction_filter = data.ecology_faction_filter;
    data.ecology_selected_id = "dispatch/moving/1";
    data.ecology_last_query_selected_id = data.ecology_selected_id;

    ecology_debug::query_region original_region;
    original_region.enabled = true;
    original_region.minimum = tripoint_abs_omt( 5, 12, 0 );
    original_region.maximum = tripoint_abs_omt( 25, 32, 0 );
    data.ecology_last_query_region = original_region;
    CHECK( data.ecology_cache_matches( 17, data.cursor_pos, original_region ) );

    ecology_debug::query_region expanded_region = original_region;
    expanded_region.minimum = tripoint_abs_omt( 4, 11, 0 );
    expanded_region.maximum = tripoint_abs_omt( 26, 33, 0 );
    CHECK_FALSE( data.ecology_cache_matches( 17, data.cursor_pos, expanded_region ) );

    data.ecology_loaded_only = true;
    CHECK_FALSE( data.ecology_cache_matches( 17, data.cursor_pos, original_region ) );
    data.ecology_loaded_only = false;

    ecology_debug::entity_marker dispatch;
    dispatch.id = data.ecology_selected_id;
    dispatch.kind = ecology_debug::entity_kind::bandit_dispatch;
    dispatch.omt = tripoint_abs_omt( 16, 22, 0 );
    data.ecology_view.entities = { dispatch };
    data.ecology_pinned = true;
    overmap_ui::reconcile_ecology_selection( data );

    REQUIRE( data.cursor_pos == dispatch.omt );
    CHECK_FALSE( data.ecology_cache_matches( 17, data.cursor_pos, original_region ) );
}

TEST_CASE( "ecology_observer_filter_contract_matches_human_and_json_views",
           "[ecology_debug][observer_ui][phase4]" )
{
    const ecology_debug::query_filters camps = overmap_ui::ecology_query_filters(
                overmap_ui::ecology_filter_mode::camps );
    const ecology_debug::query_filters dispatches = overmap_ui::ecology_query_filters(
                overmap_ui::ecology_filter_mode::dispatches );
    const ecology_debug::query_filters loaded_cannibals = overmap_ui::ecology_query_filters(
                overmap_ui::ecology_filter_mode::all,
                overmap_ui::ecology_faction_filter_mode::cannibals, true );

    CHECK( camps.camps );
    CHECK_FALSE( camps.dispatches );
    CHECK_FALSE( camps.hordes );
    CHECK_FALSE( camps.stalkers );
    CHECK_FALSE( dispatches.camps );
    CHECK( dispatches.dispatches );
    CHECK_FALSE( dispatches.hordes );
    CHECK_FALSE( dispatches.stalkers );
    CHECK_FALSE( loaded_cannibals.bandits );
    CHECK( loaded_cannibals.cannibals );
    CHECK_FALSE( loaded_cannibals.migos );
    const auto migos = overmap_ui::ecology_query_filters( overmap_ui::ecology_filter_mode::all,
                       overmap_ui::ecology_faction_filter_mode::migos );
    CHECK( migos.migos );
    CHECK_FALSE( migos.bandits );
    CHECK_FALSE( migos.cannibals );
    CHECK( loaded_cannibals.loaded_only );
    CHECK( overmap_ui::ecology_filter_labels( overmap_ui::ecology_filter_mode::camps ) ==
           std::vector<std::string> { "camps", "bandits", "cannibals", "Mi-go", "loaded+unloaded" } );
    CHECK( overmap_ui::ecology_filter_labels( overmap_ui::ecology_filter_mode::dispatches ) ==
           std::vector<std::string> { "dispatches", "bandits", "cannibals", "Mi-go", "loaded+unloaded" } );
    CHECK( overmap_ui::ecology_filter_labels( overmap_ui::ecology_filter_mode::all,
            overmap_ui::ecology_faction_filter_mode::bandits, true ) ==
           std::vector<std::string> { "camps", "dispatches", "bandits", "loaded" } );
}

TEST_CASE( "ecology_observer_binary_name_matches_supported_executable_variants",
           "[ecology_debug][observer_ui][phase4]" )
{
    CHECK( overmap_ui::ecology_observer_binary_name( "cataclysm-dda.mo", false ) ==
           "cataclysm" );
    CHECK( overmap_ui::ecology_observer_binary_name( "cataclysm-dda.mo", true ) ==
           "cataclysm-tiles" );
    CHECK( overmap_ui::ecology_observer_binary_name( "cataclysm-tlg.mo", false ) ==
           "cataclysm-tlg" );
    CHECK( overmap_ui::ecology_observer_binary_name( "cataclysm-tlg.mo", true ) ==
           "cataclysm-tlg-tiles" );
}

TEST_CASE( "ecology_observer_controls_survive_transient_overmap_draw_data",
           "[ecology_debug][observer_ui][phase4]" )
{
    overmap_ui::overmap_draw_data_t saved_controls;
    overmap_ui::restore_ecology_observer_controls( saved_controls );

    overmap_ui::overmap_draw_data_t first_open;
    first_open.ecology_filter = overmap_ui::ecology_filter_mode::dispatches;
    first_open.ecology_faction_filter = overmap_ui::ecology_faction_filter_mode::cannibals;
    first_open.ecology_loaded_only = true;
    first_open.ecology_selected_id = "dispatch/persisted/7";
    first_open.ecology_pinned = true;
    ecology_debug::entity_marker selected_marker;
    selected_marker.id = first_open.ecology_selected_id;
    selected_marker.authority_index = 42;
    first_open.ecology_view.entities.push_back( selected_marker );
    overmap_ui::remember_ecology_observer_controls( first_open );

    overmap_ui::overmap_draw_data_t second_open;
    overmap_ui::restore_ecology_observer_controls( second_open );
    CHECK( second_open.ecology_filter == overmap_ui::ecology_filter_mode::dispatches );
    CHECK( second_open.ecology_faction_filter ==
           overmap_ui::ecology_faction_filter_mode::cannibals );
    CHECK( second_open.ecology_loaded_only );
    CHECK( second_open.ecology_selected_id == first_open.ecology_selected_id );
    CHECK( second_open.ecology_selected_authority_index == 42 );
    CHECK( second_open.ecology_pinned );

    overmap_ui::remember_ecology_observer_controls( saved_controls );
}

TEST_CASE( "ecology_observer_export_uses_the_process_local_overmap_filter",
           "[ecology_debug][observer_ui][phase4]" )
{
    clear_avatar();
    avatar &player_character = get_avatar();
    player_character.set_mutation( trait_DEBUG_CLAIRVOYANCE );
    overmap_ui::overmap_draw_data_t saved_controls;
    overmap_ui::restore_ecology_observer_controls( saved_controls );

    const auto check_export = []( overmap_ui::ecology_filter_mode filter,
    bool camps_enabled, bool dispatches_enabled, const std::string & primary_label ) {
        overmap_ui::overmap_draw_data_t controls;
        controls.ecology_filter = filter;
        overmap_ui::remember_ecology_observer_controls( controls );
        const JsonObject root = json_loader::from_string(
                                    overmap_ui::ecology_observer_snapshot_json() ).get_object();
        root.allow_omitted_members();
        const JsonObject context = root.get_object( "context" );
        context.allow_omitted_members();
        const JsonObject filters = context.get_object( "filters" );
        filters.allow_omitted_members();
        CHECK( filters.get_bool( "camps" ) == camps_enabled );
        CHECK( filters.get_bool( "dispatches" ) == dispatches_enabled );
        JsonArray labels = filters.get_array( "labels" );
        REQUIRE( labels.size() == 5 );
        CHECK( labels.next_string() == primary_label );
        CHECK( labels.next_string() == "bandits" );
        CHECK( labels.next_string() == "cannibals" );
        CHECK( labels.next_string() == "Mi-go" );
        CHECK( labels.next_string() == "loaded+unloaded" );
    };

    check_export( overmap_ui::ecology_filter_mode::camps, true, false, "camps" );
    check_export( overmap_ui::ecology_filter_mode::dispatches, false, true, "dispatches" );

    overmap_ui::remember_ecology_observer_controls( saved_controls );
    player_character.unset_mutation( trait_DEBUG_CLAIRVOYANCE );
}

TEST_CASE( "ecology observer exports current resident dispatch position",
           "[ecology_debug][observer_ui][dispatch_position]" )
{
    clear_avatar();
    clear_map();
    clear_npcs();
    avatar &player = get_avatar();
    player.set_mutation( trait_DEBUG_CLAIRVOYANCE );
    overmap_ui::overmap_draw_data_t old_controls;
    overmap_ui::restore_ecology_observer_controls( old_controls );
    auto old_world = overmap_buffer.global_state.bandit_live_world;
    on_out_of_scope restore( [&] {
        overmap_buffer.global_state.bandit_live_world = std::move( old_world );
        overmap_ui::remember_ecology_observer_controls( old_controls );
        clear_npcs();
    } );
    npc &member = spawn_npc( player.pos_bub().xy() + point::east, "test_talker" );
    const auto actual_ms = player.pos_abs() + tripoint_rel_ms( 3 * SEEX * 2, 0, 0 );
    member.set_pos_abs_only( actual_ms );
    get_creature_tracker().clear_npcs();
    REQUIRE_FALSE( member.is_active() );
    const auto player_omt = player.pos_abs_omt();
    bandit_live_world::site_record site;
    site.site_id = "observer-resident-integration";
    site.source_kind = bandit_live_world::anchor_source_kind::overmap_special;
    site.site_kind = bandit_live_world::owned_site_kind::bandit_camp;
    site.profile = bandit_live_world::hostile_site_profile::camp_style;
    site.source_id = "bandit_camp";
    site.anchor = player_omt + point::south;
    site.footprint.push_back( site.anchor );
    site.living_total = 1;
    site.members.push_back( { member.getID(), "test_talker", project_to<coords::ms>( site.anchor ),
                              bandit_live_world::member_state::outbound, false, "" } );
    site.spawn_tiles.push_back( { project_to<coords::ms>( site.anchor ), 1 } );
    auto &outing = site.active_outing;
    outing.kind = bandit_live_world::outing_kind::scout_sortie;
    outing.activity_id = "observer-resident-outing";
    outing.camp_id = site.site_id;
    outing.generation = 1;
    outing.member_ids.push_back( member.getID() );
    outing.leader_id = member.getID();
    outing.shared_route = { site.anchor, player_omt + point::west };
    outing.waypoint_index = 1;
    outing.target_omt = outing.shared_route.back();
    outing.phase = bandit_live_world::scout_phase::outbound;
    overmap_buffer.global_state.bandit_live_world = {};
    overmap_buffer.global_state.bandit_live_world.sites.push_back( site );
    const std::string id = "dispatch/" + site.site_id + "/" + outing.activity_id + "/1";
    overmap_ui::overmap_draw_data_t controls;
    controls.ecology_filter = overmap_ui::ecology_filter_mode::dispatches;
    controls.ecology_selected_id = id;
    overmap_ui::remember_ecology_observer_controls( controls );
    JsonObject root = json_loader::from_string( overmap_ui::ecology_observer_snapshot_json() ).get_object();
    root.allow_omitted_members();
    const JsonArray entities = root.get_array( "entities" );
    bool found = false;
    for( const JsonValue value : entities ) {
        JsonObject entity = value.get_object();
        entity.allow_omitted_members();
        if( entity.get_string( "id" ) != id ) {
            continue;
        }
        found = true;
        CHECK( entity.get_string( "position_basis" ) == "current_member" );
        CHECK( entity.get_int( "representative_npc_id" ) == member.getID().get_value() );
        const auto omt = project_to<coords::omt>( actual_ms );
        CHECK( entity.get_array( "omt" ).get_int( 0 ) == omt.x() );
        CHECK( entity.get_array( "omt" ).get_int( 1 ) == omt.y() );
    }
    CHECK( found );
    JsonObject selected = root.get_object( "selected" );
    selected.allow_omitted_members();
    JsonObject selected_member = selected.get_array( "members" ).get_object( 0 );
    selected_member.allow_omitted_members();
    CHECK( selected_member.get_array( "position_ms" ).get_int( 0 ) == actual_ms.x() );
    CHECK( selected_member.get_array( "position_ms" ).get_int( 1 ) == actual_ms.y() );
}

TEST_CASE( "ecology overmap facts follow gated canonical migo projection",
           "[ecology_debug][observer_ui][migo]" )
{
    bandit_live_world::world_state humans;
    migo_population::world_state population;
    migo_tower::world_state towers;
    population.sites.emplace_back();
    auto &site = population.sites.back();
    site.id = "ui-tower";
    site.tower = true;
    site.anchor = tripoint_abs_omt( 12, 15, 0 );
    site.members.emplace_back();
    auto &member = site.members.back();
    member.token.site_id = site.id;
    member.token.member_id = "ui-member";
    member.owner = migo_population::representation::abstract;
    member.assignment = migo_population::duty::dispatched;
    member.hp = 200;
    member.current_position = project_to<coords::ms>( tripoint_abs_omt( 13, 15, 0 ) );
    towers.sites.emplace_back();
    auto &tower = towers.sites.back();
    tower.site_id = site.id;
    tower.operation.emplace();
    tower.operation->generation = 2;
    tower.operation->source.source_omt = tripoint_abs_omt( 20, 15, 0 );
    tower.operation->sweep.begin( tower.operation->source.source_omt, 2 );
    tower.operation->members.emplace_back();
    tower.operation->members.back().member_id = member.token.member_id;
    ecology_debug::query_request request;
    request.enabled = true;
    request.migo_population = &population;
    request.migo_towers = &towers;
    auto view = ecology_debug::query_bandit_ecology( humans, request );
    REQUIRE( view.entities.size() == 2 );
    const auto dispatch = std::find_if( view.entities.begin(),
    view.entities.end(), []( const auto & row ) {
        return row.kind == ecology_debug::entity_kind::migo_dispatch;
    } );
    REQUIRE( dispatch != view.entities.end() );
    request.selected_id = dispatch->id;
    overmap_ui::overmap_draw_data_t data;
    data.cursor_pos = dispatch->omt;
    data.ecology_selected_id = dispatch->id;
    data.ecology_view = ecology_debug::query_bandit_ecology( humans, request );
    const auto closed = overmap_ui::ecology_semantic_facts( data, data.cursor_pos );
    CHECK( closed.at( "ecology_enabled" ) == "false" );
    CHECK( closed.count( "ecology_cursor_id" ) == 0 );
    data.ecology_enabled = true;
    const auto facts = overmap_ui::ecology_semantic_facts( data, data.cursor_pos );
    CHECK( facts.at( "ecology_cursor_id" ) == request.selected_id );
    CHECK( facts.at( "ecology_cursor_kind" ) == "migo_dispatch_member" );
    CHECK( facts.count( "ecology_cursor_position_basis" ) == 0 );
    CHECK( facts.count( "ecology_cursor_representative_npc_id" ) == 0 );
    const auto cursor_marker_it = std::find_if( data.ecology_view.entities.begin(),
    data.ecology_view.entities.end(), [&data]( const auto &row ) {
        return row.omt == data.cursor_pos && row.id == data.ecology_selected_id;
    } );
    REQUIRE( cursor_marker_it != data.ecology_view.entities.end() );
    auto &cursor_marker = *cursor_marker_it;
    cursor_marker.position_basis = "current_member";
    cursor_marker.representative_npc_id = 42;
    const auto current = overmap_ui::ecology_semantic_facts( data, data.cursor_pos );
    CHECK( current.at( "ecology_cursor_position_basis" ) == "current_member" );
    CHECK( current.at( "ecology_cursor_representative_npc_id" ) == "42" );
    data.ecology_enabled = false;
    const auto private_facts = overmap_ui::ecology_semantic_facts( data, data.cursor_pos );
    CHECK( private_facts.count( "ecology_cursor_position_basis" ) == 0 );
    CHECK( private_facts.count( "ecology_cursor_representative_npc_id" ) == 0 );
    data.ecology_enabled = true;
    cursor_marker.position_basis = "checkpoint";
    cursor_marker.representative_npc_id.reset();
    const auto fallback = overmap_ui::ecology_semantic_facts( data, data.cursor_pos );
    CHECK( fallback.at( "ecology_cursor_position_basis" ) == "checkpoint" );
    CHECK( fallback.count( "ecology_cursor_representative_npc_id" ) == 0 );
    CHECK( facts.at( "ecology_selected_id" ) == request.selected_id );
    CHECK( facts.at( "ecology_population" ) == "1" );
    CHECK( facts.at( "ecology_signal_source" ) == tower.operation->source.source_omt.to_string() );
    CHECK( overmap_ui::ecology_marker_display( dispatch->kind ).first == "m" );
    request.filters.migos = false;
    data.ecology_view = ecology_debug::query_bandit_ecology( humans, request );
    const auto filtered = overmap_ui::ecology_semantic_facts( data, data.cursor_pos );
    CHECK( filtered.at( "ecology_count" ) == "0" );
    CHECK( filtered.count( "ecology_cursor_id" ) == 0 );
}
