#include "ecology_debug_incident.h"

#include <optional>
#include <string>
#include <vector>

#include "cata_catch.h"
#include "json_loader.h"

namespace
{

ecology_debug::selected_projection make_projection()
{
    ecology_debug::selected_projection result;
    result.marker.id = "dispatch/camp/outing/1";
    result.marker.alias = "BD-TEST";
    result.marker.kind = ecology_debug::entity_kind::bandit_dispatch;
    result.marker.faction = ecology_debug::entity_faction::bandit;
    result.marker.omt = tripoint_abs_omt( 10, 20, -1 );
    result.marker.owner = "abstract";
    result.marker.state = "returning";
    result.marker.generation = 3;
    result.marker.provenance = ecology_debug::entity_provenance::natural;
    result.token.world_identity = "world/test";
    result.token.canonical_id = result.marker.id;
    result.token.generation = result.marker.generation;
    result.token.owner = result.marker.owner;
    result.token.authority_token = "site/4:outing/1";

    ecology_debug::selected_detail detail;
    detail.entity_id = result.marker.id;
    detail.source_camp_id = "camp/bandit-a";
    detail.phase = "returning";
    detail.last_transition_minutes = 90;
    detail.last_transition_reason = "casualty_recorded";
    detail.evidence_id = "casualty@10,20,-1";
    detail.evidence_kind = "casualty";
    detail.evidence_state = "confirmed";
    detail.evidence_reason = "physical_signal_return";
    detail.evidence_observed_minutes = 78;
    detail.evidence_age_minutes = 12;
    detail.next_deadline_minutes = 150;
    detail.destination = tripoint_abs_omt( 2, 3, -1 );
    detail.route = { tripoint_abs_omt( 10, 20, -1 ), tripoint_abs_omt( 2, 3, -1 ) };
    detail.members.push_back( { character_id( 43 ), "Bravo", 64, "returning", false } );
    detail.members.push_back( { character_id( 42 ), "Alpha", 100, "returning", false } );
    result.detail = detail;
    return result;
}

ecology_debug::incident_identity make_identity()
{
    ecology_debug::incident_identity result;
    result.turn = 1234;
    result.timestamp = "2026-08-05T12:00:00Z";
    result.player_omt = tripoint_abs_omt( 1, 2, -1 );
    result.scenario = "phase4-observer";
    result.commit = "abc123";
    result.binary = "cataclysm-tiles";
    result.run_identity = "run/20260805-120000";
    return result;
}

ecology_debug::delta_ring make_delta_ring( const ecology_debug::selected_projection &projection )
{
    ecology_debug::delta_ring result;
    ecology_debug::delta_observation_context context;
    context.turn = 1200;
    context.timestamp = "2026-08-05T11:55:00Z";
    REQUIRE( result.observe( std::nullopt, projection, context ).emitted == 1 );
    return result;
}

ecology_debug::incident_intervention make_intervention( uint64_t sequence )
{
    ecology_debug::incident_intervention result;
    result.sequence = sequence;
    result.turn = 1230 + static_cast<int>( sequence );
    result.timestamp = "2026-08-05T11:59:00Z";
    result.entity_id = "dispatch/camp/outing/1";
    result.action = "wound_member";
    result.before_summary = "npc 42 hp=100";
    result.after_summary = "npc 42 hp=64";
    result.debug_intervention = true;
    return result;
}

} // namespace

TEST_CASE( "ecology_debug_incident_is_deterministic_and_preserves_provenance",
           "[ecology_debug][incident][phase4]" )
{
    const ecology_debug::selected_projection selected = make_projection();
    const ecology_debug::delta_ring deltas = make_delta_ring( selected );
    const ecology_debug::incident_identity identity = make_identity();
    const std::vector<ecology_debug::incident_intervention> interventions = {
        make_intervention( 2 ), make_intervention( 1 )
    };
    const std::string deltas_before = deltas.serialize_compact_json();
    ecology_debug::incident_watch_state watch;
    watch.spec.preset = ecology_debug::watch_preset::casualty;
    watch.spec.disposition = ecology_debug::trigger_disposition::pause;
    watch.spec.absolute_deadline_minutes = 160;
    watch.input.prior = selected;
    watch.input.current = selected;
    watch.input.now_minutes = 120;
    watch.input.armed_minutes = 100;
    watch.input.last_progress_minutes = 120;
    watch.result.status = ecology_debug::watch_status::triggered;
    watch.result.disposition = ecology_debug::trigger_disposition::pause;
    watch.result.newly_triggered = true;
    watch.result.meaningful_progress = true;
    watch.result.reason = "casualty";

    const ecology_debug::incident_bundle_result first = ecology_debug::serialize_incident_bundle(
                identity, selected, deltas, std::string( "Scout casualty" ), interventions, watch );
    const ecology_debug::incident_bundle_result second = ecology_debug::serialize_incident_bundle(
                identity, selected, deltas, std::string( "Scout casualty" ), interventions, watch );

    REQUIRE( first.valid );
    CHECK( first.payload == second.payload );
    CHECK( first.payload_bytes == first.payload.size() );
    CHECK( deltas.serialize_compact_json() == deltas_before );
    CHECK( selected.detail->members.front().npc_id == character_id( 43 ) );
    CHECK( interventions.front().sequence == 2 );

    const JsonObject root = json_loader::from_string( first.payload ).get_object();
    root.allow_omitted_members();
    CHECK( root.get_string( "schema" ) == "c-aol.ecology.incident" );
    CHECK( root.get_int( "version" ) == 2 );
    const JsonObject metadata = root.get_object( "metadata" );
    metadata.allow_omitted_members();
    CHECK( metadata.get_int( "payload_bytes" ) == static_cast<int>( first.payload.size() ) );
    const JsonObject serialized_selected = root.get_object( "selected" );
    serialized_selected.allow_omitted_members();
    CHECK( serialized_selected.get_string( "provenance" ) == "natural" );
    const JsonObject token = serialized_selected.get_object( "token" );
    token.allow_omitted_members();
    CHECK( token.get_string( "authority_token" ) == "site/4:outing/1" );
    const JsonObject detail = serialized_selected.get_object( "details" );
    detail.allow_omitted_members();
    CHECK( detail.get_string( "evidence_kind" ) == "casualty" );
    CHECK( detail.get_int( "evidence_observed_minutes" ) == 78 );
    const JsonObject delta_block = root.get_object( "deltas" );
    delta_block.allow_omitted_members();
    const JsonArray records = delta_block.get_array( "records" );
    REQUIRE( records.size() == 1 );
    const JsonObject delta = records.get_object( 0 );
    delta.allow_omitted_members();
    CHECK( delta.get_string( "provenance" ) == "natural" );
    const JsonObject serialized_watch = root.get_object( "watch" );
    serialized_watch.allow_omitted_members();
    CHECK( serialized_watch.get_string( "preset" ) == "casualty" );
    CHECK( serialized_watch.get_string( "status" ) == "triggered" );
    CHECK( serialized_watch.get_string( "reason" ) == "casualty" );
    const JsonArray serialized_interventions = root.get_array( "interventions" );
    REQUIRE( serialized_interventions.size() == 2 );
    JsonObject intervention = serialized_interventions.get_object( 0 );
    intervention.allow_omitted_members();
    CHECK( intervention.get_int( "sequence" ) == 1 );
    CHECK( intervention.get_bool( "debug_intervention" ) );
}

TEST_CASE( "ecology_debug_incident_applies_exact_note_and_intervention_caps",
           "[ecology_debug][incident][phase4]" )
{
    const ecology_debug::selected_projection selected = make_projection();
    const ecology_debug::delta_ring deltas = make_delta_ring( selected );
    const std::string note( ecology_debug::ecology_incident_note_byte_cap + 17, 'n' );
    std::vector<ecology_debug::incident_intervention> interventions;
    for( size_t index = 0; index < ecology_debug::ecology_incident_intervention_cap + 3; ++index ) {
        interventions.push_back( make_intervention( index + 1 ) );
    }

    const ecology_debug::incident_bundle_result result = ecology_debug::serialize_incident_bundle(
                make_identity(), selected, deltas, note, interventions );

    REQUIRE( result.valid );
    CHECK( result.note_truncated );
    CHECK( result.intervention_dropped_count == 3 );
    const JsonObject root = json_loader::from_string( result.payload ).get_object();
    root.allow_omitted_members();
    CHECK( root.get_string( "human_note" ).size() == ecology_debug::ecology_incident_note_byte_cap );
    const JsonObject metadata = root.get_object( "metadata" );
    metadata.allow_omitted_members();
    CHECK( metadata.get_int( "note_byte_limit" ) ==
           static_cast<int>( ecology_debug::ecology_incident_note_byte_cap ) );
    CHECK( metadata.get_int( "note_input_bytes" ) == static_cast<int>( note.size() ) );
    CHECK( metadata.get_bool( "note_truncated" ) );
    CHECK( metadata.get_int( "intervention_limit" ) ==
           static_cast<int>( ecology_debug::ecology_incident_intervention_cap ) );
    CHECK( metadata.get_int( "intervention_retained_count" ) ==
           static_cast<int>( ecology_debug::ecology_incident_intervention_cap ) );
    CHECK( metadata.get_int( "intervention_dropped_count" ) == 3 );
    const JsonArray serialized = root.get_array( "interventions" );
    REQUIRE( serialized.size() == ecology_debug::ecology_incident_intervention_cap );
    JsonObject first = serialized.get_object( 0 );
    first.allow_omitted_members();
    CHECK( first.get_int( "sequence" ) == 4 );
}

TEST_CASE( "ecology_debug_incident_retains_deadline_watch_without_a_delta",
           "[ecology_debug][incident][phase4]" )
{
    const ecology_debug::selected_projection selected = make_projection();
    ecology_debug::incident_watch_state watch;
    watch.spec.preset = ecology_debug::watch_preset::no_progress_by_deadline;
    watch.spec.absolute_deadline_minutes = 460;
    watch.input.prior = selected;
    watch.input.current = selected;
    watch.input.now_minutes = 460;
    watch.input.armed_minutes = 100;
    watch.input.last_progress_minutes = 100;
    watch.result.status = ecology_debug::watch_status::timed_out;
    watch.result.newly_triggered = true;
    watch.result.reason = "no_progress_deadline_reached";

    const ecology_debug::delta_ring deltas;
    const ecology_debug::incident_bundle_result result =
        ecology_debug::serialize_incident_bundle( make_identity(), selected, deltas,
                std::nullopt, {}, watch );

    REQUIRE( result.valid );
    const JsonObject root = json_loader::from_string( result.payload ).get_object();
    root.allow_omitted_members();
    const JsonObject serialized_watch = root.get_object( "watch" );
    serialized_watch.allow_omitted_members();
    CHECK( serialized_watch.get_string( "status" ) == "timed_out" );
    CHECK( serialized_watch.get_string( "reason" ) == "no_progress_deadline_reached" );
    const JsonObject serialized_deltas = root.get_object( "deltas" );
    serialized_deltas.allow_omitted_members();
    CHECK( serialized_deltas.get_array( "records" ).size() == 0 );
}

TEST_CASE( "ecology_debug_incident_rejects_invalid_identity_selection_and_provenance",
           "[ecology_debug][incident][phase4]" )
{
    ecology_debug::selected_projection selected = make_projection();
    ecology_debug::delta_ring deltas = make_delta_ring( selected );

    ecology_debug::incident_identity identity = make_identity();
    identity.run_identity.clear();
    ecology_debug::incident_bundle_result result = ecology_debug::serialize_incident_bundle(
                identity, selected, deltas, std::nullopt, {} );
    CHECK_FALSE( result.valid );
    CHECK( result.error == "incident_identity_invalid" );
    CHECK( result.payload.empty() );

    result = ecology_debug::serialize_incident_bundle( make_identity(), std::nullopt, deltas,
             std::nullopt, {} );
    CHECK_FALSE( result.valid );
    CHECK( result.error == "incident_selection_missing" );

    selected.token.authority_token = "replacement";
    selected.token.canonical_id = "replacement-id";
    result = ecology_debug::serialize_incident_bundle( make_identity(), selected, deltas,
             std::nullopt, {} );
    CHECK_FALSE( result.valid );
    CHECK( result.error == "incident_selection_token_invalid" );

    selected = make_projection();
    selected.detail->entity_id = "stale-selection";
    result = ecology_debug::serialize_incident_bundle( make_identity(), selected, deltas,
             std::nullopt, {} );
    CHECK_FALSE( result.valid );
    CHECK( result.error == "incident_selection_token_invalid" );

    selected = make_projection();
    ecology_debug::incident_intervention invalid = make_intervention( 1 );
    invalid.debug_intervention = false;
    result = ecology_debug::serialize_incident_bundle( make_identity(), selected, deltas,
             std::nullopt, { invalid } );
    CHECK_FALSE( result.valid );
    CHECK( result.error == "incident_intervention_provenance_invalid" );
}

TEST_CASE( "ecology_debug_incident_rejects_stale_delta_token_mismatch",
           "[ecology_debug][incident][phase4]" )
{
    const ecology_debug::selected_projection selected = make_projection();
    ecology_debug::selected_projection other = selected;
    other.marker.id = "dispatch/camp/outing/other";
    other.token.canonical_id = other.marker.id;
    ecology_debug::delta_ring deltas = make_delta_ring( other );

    const ecology_debug::incident_bundle_result result = ecology_debug::serialize_incident_bundle(
                make_identity(), selected, deltas, std::nullopt, {} );

    CHECK_FALSE( result.valid );
    CHECK( result.error == "incident_delta_token_mismatch" );
    CHECK( result.payload.empty() );
}

TEST_CASE( "ecology_debug_incident_rejects_unbounded_caller_text_and_default_order",
           "[ecology_debug][incident][phase4]" )
{
    const ecology_debug::selected_projection selected = make_projection();
    const ecology_debug::delta_ring deltas = make_delta_ring( selected );
    const std::string oversized( ecology_debug::ecology_incident_caller_text_byte_cap + 1, 'x' );

    const std::vector<std::string ecology_debug::incident_identity::*> identity_fields = {
        &ecology_debug::incident_identity::timestamp,
        &ecology_debug::incident_identity::scenario,
        &ecology_debug::incident_identity::commit,
        &ecology_debug::incident_identity::binary,
        &ecology_debug::incident_identity::run_identity,
    };
    for( std::string ecology_debug::incident_identity::*field : identity_fields ) {
        ecology_debug::incident_identity identity = make_identity();
        identity.*field = oversized;
        const ecology_debug::incident_bundle_result result =
            ecology_debug::serialize_incident_bundle( identity, selected, deltas, std::nullopt, {} );
        CHECK_FALSE( result.valid );
        CHECK( result.error == "incident_identity_text_too_long" );
    }

    const std::vector<std::string ecology_debug::incident_intervention::*> intervention_fields = {
        &ecology_debug::incident_intervention::timestamp,
        &ecology_debug::incident_intervention::entity_id,
        &ecology_debug::incident_intervention::action,
        &ecology_debug::incident_intervention::before_summary,
        &ecology_debug::incident_intervention::after_summary,
    };
    for( std::string ecology_debug::incident_intervention::*field : intervention_fields ) {
        ecology_debug::incident_intervention intervention = make_intervention( 1 );
        intervention.*field = oversized;
        const ecology_debug::incident_bundle_result result =
            ecology_debug::serialize_incident_bundle( make_identity(), selected, deltas,
                    std::nullopt, { intervention } );
        CHECK_FALSE( result.valid );
        CHECK( result.error == "incident_intervention_text_too_long" );
    }

    ecology_debug::incident_intervention intervention = make_intervention( 1 );
    intervention.sequence = 0;
    ecology_debug::incident_bundle_result result = ecology_debug::serialize_incident_bundle(
                make_identity(), selected, deltas, std::nullopt, { intervention } );
    CHECK_FALSE( result.valid );
    CHECK( result.error == "incident_intervention_order_invalid" );

    intervention = make_intervention( 1 );
    intervention.turn = -1;
    result = ecology_debug::serialize_incident_bundle( make_identity(), selected, deltas,
             std::nullopt, { intervention } );
    CHECK_FALSE( result.valid );
    CHECK( result.error == "incident_intervention_order_invalid" );
}
