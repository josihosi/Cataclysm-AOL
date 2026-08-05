#include "overmap_ui.h"

#include <string>

#include "avatar.h"
#include "cata_catch.h"
#include "game.h"
#include "json_loader.h"
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

    CHECK( bandit_camp.first == "B" );
    CHECK( cannibal_camp.first == "C" );
    CHECK( bandit_dispatch.first == "b" );
    CHECK( cannibal_dispatch.first == "c" );
    CHECK( bandit_camp.second == c_light_red );
    CHECK( cannibal_camp.second == c_red );
    CHECK( bandit_dispatch.second == c_yellow );
    CHECK( cannibal_dispatch.second == c_pink );
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
    CHECK_FALSE( dispatches.camps );
    CHECK( dispatches.dispatches );
    CHECK_FALSE( loaded_cannibals.bandits );
    CHECK( loaded_cannibals.cannibals );
    CHECK( loaded_cannibals.loaded_only );
    CHECK( overmap_ui::ecology_filter_labels( overmap_ui::ecology_filter_mode::camps ) ==
           std::vector<std::string> { "camps", "bandits", "cannibals", "loaded+unloaded" } );
    CHECK( overmap_ui::ecology_filter_labels( overmap_ui::ecology_filter_mode::dispatches ) ==
           std::vector<std::string> { "dispatches", "bandits", "cannibals", "loaded+unloaded" } );
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
    bool camps_enabled, bool dispatches_enabled, const std::string &primary_label ) {
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
        REQUIRE( labels.size() == 4 );
        CHECK( labels.next_string() == primary_label );
        CHECK( labels.next_string() == "bandits" );
        CHECK( labels.next_string() == "cannibals" );
        CHECK( labels.next_string() == "loaded+unloaded" );
    };

    check_export( overmap_ui::ecology_filter_mode::camps, true, false, "camps" );
    check_export( overmap_ui::ecology_filter_mode::dispatches, false, true, "dispatches" );

    overmap_ui::remember_ecology_observer_controls( saved_controls );
    player_character.unset_mutation( trait_DEBUG_CLAIRVOYANCE );
}
