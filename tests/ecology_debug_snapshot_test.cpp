#include "ecology_debug_snapshot.h"

#include <string>

#include "cata_catch.h"
#include "json_loader.h"

namespace
{

ecology_debug::view_snapshot make_snapshot()
{
    ecology_debug::view_snapshot view;
    view.entities.push_back( { "camp/bandit-a", "BC-AAAAAA",
                               ecology_debug::entity_kind::bandit_camp,
                               ecology_debug::entity_faction::bandit,
                               tripoint_abs_omt( 4, 5, -1 ), "abstract", false, "active",
                               ecology_debug::entity_provenance::natural, 0, std::nullopt } );
    view.entities.push_back( { "dispatch/cannibal-b", "CD-BBBBBB",
                               ecology_debug::entity_kind::cannibal_dispatch,
                               ecology_debug::entity_faction::cannibal,
                               tripoint_abs_omt( 4, 5, -1 ), "local", true, "returning",
                               ecology_debug::entity_provenance::debug_intervention, 7, std::nullopt } );
    ecology_debug::selected_detail selected;
    selected.entity_id = "dispatch/cannibal-b";
    selected.source_camp_id = "camp/cannibal-b";
    selected.phase = "returning";
    selected.last_transition_minutes = 120;
    selected.last_transition_reason = "casualty_recorded";
    selected.blocked_reason = "awaiting_local_cohesion";
    selected.evidence_reason = "physical_signal_return";
    selected.evidence_age_minutes = 12;
    selected.next_deadline_minutes = 180;
    selected.destination = tripoint_abs_omt( 2, 3, -1 );
    selected.route = { tripoint_abs_omt( 4, 5, -1 ), tripoint_abs_omt( 2, 3, -1 ) };
    selected.members.push_back( { character_id( 42 ), "Alpha", 55, "returning", true } );
    selected.members.push_back( { character_id( 43 ), "Bravo", std::nullopt, "dead", false } );
    view.selected = selected;
    view.metadata.candidate_count = 300;
    view.metadata.considered_count = 256;
    view.metadata.emitted_count = 2;
    view.metadata.dropped_count = 298;
    view.metadata.candidate_limit = 256;
    view.metadata.marker_limit = 2;
    view.metadata.event_limit = 9;
    view.metadata.truncated = true;
    view.metadata.query_microseconds = 37;
    view.metadata.render_microseconds = 11;
    view.metadata.trace_bytes = 4096;
    return view;
}

ecology_debug::snapshot_context make_context()
{
    ecology_debug::snapshot_context context;
    context.commit = "abc123";
    context.binary = "cataclysm-tiles";
    context.scenario = "phase4-observer";
    context.calendar_turn = "turn 12345";
    context.timestamp = "2026-08-05T12:34:56+02:00";
    context.player_omt = tripoint_abs_omt( 1, 2, -1 );
    context.region.enabled = true;
    context.region.minimum = tripoint_abs_omt( -10, -20, -2 );
    context.region.maximum = tripoint_abs_omt( 10, 20, 1 );
    context.filters.loaded_only = true;
    context.filter_labels = { "bandits", "cannibals", "loaded" };
    context.selected_id = "dispatch/cannibal-b";
    return context;
}

} // namespace

TEST_CASE( "ecology_debug_snapshot_is_byte_deterministic_and_preserves_order",
           "[ecology_debug][snapshot][phase4]" )
{
    const ecology_debug::view_snapshot view = make_snapshot();
    const ecology_debug::snapshot_context context = make_context();

    const std::string first = ecology_debug::serialize_snapshot( view, context );
    const std::string second = ecology_debug::serialize_snapshot( view, context );

    CHECK( first == second );
    CHECK( first.find( "\"provenance\":\"natural\"" ) != std::string::npos );
    CHECK( first.find( "\"provenance\":\"debug_intervention\"" ) != std::string::npos );
    CHECK( first.find( "\"omt\":[4,5,-1]" ) != std::string::npos );
    CHECK( first.find( "\"hp_percent\":null" ) != std::string::npos );
    CHECK( first.find( "\"candidate_cap\":256" ) != std::string::npos );
    CHECK( first.find( "\"marker_cap\":2" ) != std::string::npos );
    CHECK( first.find( "\"event_cap\":9" ) != std::string::npos );
    CHECK( first.find( "\"truncated\":true" ) != std::string::npos );
    CHECK( first.find( "\"cannibals\":true" ) != std::string::npos );
    CHECK( first.find( "\"id\":\"camp/bandit-a\"" ) <
           first.find( "\"id\":\"dispatch/cannibal-b\"" ) );

    const JsonObject root = json_loader::from_string( first ).get_object();
    root.allow_omitted_members();
    CHECK( root.get_string( "schema" ) == "c-aol.ecology.observer" );
    CHECK( root.get_int( "version" ) == 1 );
    const JsonObject metadata = root.get_object( "metadata" );
    metadata.allow_omitted_members();
    CHECK( metadata.get_int( "query_us" ) == 37 );
    CHECK( metadata.get_int( "render_us" ) == 11 );
    CHECK( metadata.get_int( "trace_bytes" ) == 4096 );
    CHECK_FALSE( metadata.get_bool( "identity_truncated" ) );
}

TEST_CASE( "ecology_debug_snapshot_serializes_no_selection_and_bounds_context_identity",
           "[ecology_debug][snapshot][phase4]" )
{
    ecology_debug::view_snapshot view;
    ecology_debug::snapshot_context context;
    context.commit = std::string( ecology_debug::snapshot_identity_limit + 7, 'x' );
    context.binary = std::string( "tiles\nunsafe" );
    context.filter_labels = { std::string( ecology_debug::snapshot_identity_limit + 1, 'f' ) };

    const std::string output = ecology_debug::serialize_snapshot( view, context );

    CHECK( output.find( "\"selected\":null" ) != std::string::npos );
    CHECK( output.find( "tiles?unsafe" ) != std::string::npos );
    CHECK( output.find( std::string( ecology_debug::snapshot_identity_limit + 1, 'x' ) ) ==
           std::string::npos );
    const JsonObject root = json_loader::from_string( output ).get_object();
    root.allow_omitted_members();
    const JsonObject metadata = root.get_object( "metadata" );
    metadata.allow_omitted_members();
    CHECK( metadata.get_bool( "identity_truncated" ) );
    CHECK( metadata.get_bool( "identity_sanitized" ) );
    CHECK( metadata.get_int( "identity_limit_bytes" ) ==
           static_cast<int>( ecology_debug::snapshot_identity_limit ) );
}

TEST_CASE( "ecology_debug_snapshot_reports_its_exact_payload_bytes",
           "[ecology_debug][snapshot][phase4]" )
{
    ecology_debug::snapshot_context context = make_context();
    context.region.minimum = tripoint_abs_omt( -1, -1, -1 );
    context.region.maximum = tripoint_abs_omt( 1, 1, -1 );
    context.selected_outside_region_included = true;
    const std::string output = ecology_debug::serialize_sized_snapshot( make_snapshot(), context );
    const JsonObject root = json_loader::from_string( output ).get_object();
    root.allow_omitted_members();
    const JsonObject metadata = root.get_object( "metadata" );
    metadata.allow_omitted_members();

    CHECK( metadata.get_int( "trace_bytes" ) == static_cast<int>( output.size() ) );
    CHECK( metadata.get_int( "render_us" ) >= 0 );
    const JsonObject serialized_context = root.get_object( "context" );
    serialized_context.allow_omitted_members();
    const JsonObject region = serialized_context.get_object( "region" );
    region.allow_omitted_members();
    CHECK( region.get_bool( "selected_outside_region_included" ) );
}

TEST_CASE( "ecology_debug_monitor_snapshot_ignores_artifact_only_volatility",
           "[ecology_debug][snapshot][phase4]" )
{
    ecology_debug::view_snapshot first_view = make_snapshot();
    ecology_debug::snapshot_context first_context = make_context();
    ecology_debug::view_snapshot second_view = first_view;
    ecology_debug::snapshot_context second_context = first_context;
    second_context.calendar_turn = "turn 12346";
    second_context.timestamp = "2026-08-05T12:35:02+02:00";
    second_context.player_omt = tripoint_abs_omt( 9, 8, 0 );
    second_context.region.minimum = tripoint_abs_omt( -1, -1, 0 );
    second_context.region.maximum = tripoint_abs_omt( 19, 18, 0 );
    second_view.metadata.query_microseconds = 99;
    second_view.metadata.render_microseconds = 23;
    second_view.metadata.trace_bytes = 8192;
    second_view.selected->evidence_age_minutes = 13;

    const ecology_debug::view_snapshot first_selected =
        ecology_debug::selected_monitor_projection( first_view, first_context.selected_id );
    second_view.entities.front().state = "unrelated camp changed";
    const ecology_debug::view_snapshot second_selected =
        ecology_debug::selected_monitor_projection( second_view, second_context.selected_id );

    const std::string first_monitor = ecology_debug::serialize_monitor_snapshot(
                                          first_selected, first_context );
    const std::string second_monitor = ecology_debug::serialize_monitor_snapshot(
                                           second_selected, second_context );
    CHECK( first_monitor == second_monitor );
    CHECK( first_monitor.find( "calendar_turn" ) == std::string::npos );
    CHECK( first_monitor.find( "timestamp" ) == std::string::npos );
    CHECK( first_monitor.find( "query_us" ) == std::string::npos );
    CHECK( first_monitor.find( "render_us" ) == std::string::npos );
    CHECK( first_monitor.find( "trace_bytes" ) == std::string::npos );
    CHECK( first_monitor.find( "player_omt" ) == std::string::npos );
    CHECK( first_monitor.find( "\"region\"" ) == std::string::npos );
    CHECK( first_monitor.find( "evidence_age_minutes" ) == std::string::npos );
    CHECK( first_selected.entities.size() == 1 );
    CHECK( first_selected.entities.front().id == first_context.selected_id );

    const std::string artifact = ecology_debug::serialize_snapshot( second_view, second_context );
    CHECK( artifact.find( "\"calendar_turn\":\"turn 12346\"" ) != std::string::npos );
    CHECK( artifact.find( "\"timestamp\":\"2026-08-05T12:35:02+02:00\"" ) !=
           std::string::npos );
    CHECK( artifact.find( "\"query_us\":99" ) != std::string::npos );
    CHECK( artifact.find( "\"render_us\":23" ) != std::string::npos );
    CHECK( artifact.find( "\"trace_bytes\":8192" ) != std::string::npos );
    CHECK( artifact.find( "\"player_omt\":[9,8,0]" ) != std::string::npos );
    CHECK( artifact.find( "\"evidence_age_minutes\":13" ) != std::string::npos );
}
