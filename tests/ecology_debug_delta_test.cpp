#include "ecology_debug_delta.h"

#include <optional>
#include <string>

#include "cata_catch.h"
#include "json.h"
#include "json_loader.h"

namespace
{

ecology_debug::selected_projection make_projection( std::string id = "dispatch/camp/outing/1" )
{
    ecology_debug::selected_projection result;
    result.marker.id = std::move( id );
    result.marker.alias = "BD-TEST";
    result.marker.kind = ecology_debug::entity_kind::bandit_dispatch;
    result.marker.faction = ecology_debug::entity_faction::bandit;
    result.marker.omt = tripoint_abs_omt( 10, 20, 0 );
    result.marker.owner = "abstract";
    result.marker.state = "outbound";
    result.marker.generation = 1;
    result.marker.provenance = ecology_debug::entity_provenance::natural;
    result.token.world_identity = "world/test";
    result.token.canonical_id = result.marker.id;
    result.token.generation = result.marker.generation;
    result.token.owner = result.marker.owner;
    result.token.authority_token = "site/4:outing/1";

    ecology_debug::selected_detail detail;
    detail.entity_id = result.marker.id;
    detail.phase = "outbound";
    ecology_debug::member_detail member;
    member.npc_id = character_id( 42 );
    member.name = "Scout";
    member.hp_percent = 100;
    member.status = "outbound";
    detail.members.push_back( member );
    result.detail = detail;
    return result;
}

ecology_debug::delta_observation_context context()
{
    ecology_debug::delta_observation_context result;
    result.turn = 1234;
    result.timestamp = "2026-08-05T12:00:00Z";
    return result;
}

} // namespace

TEST_CASE( "ecology_debug_delta_records_only_selected_transitions",
           "[ecology_debug][delta][phase4]" )
{
    ecology_debug::delta_ring ring;
    ecology_debug::selected_projection before = make_projection();

    const ecology_debug::delta_observation_result appeared = ring.observe(
                std::nullopt, before, context() );
    REQUIRE( appeared.emitted == 1 );
    CHECK( ring.records().back().kind == ecology_debug::delta_kind::appeared );
    CHECK( ring.records().back().provenance == ecology_debug::entity_provenance::natural );
    CHECK_FALSE( ring.records().back().before );
    REQUIRE( ring.records().back().after );
    CHECK( ring.records().back().after->omt == tripoint_abs_omt( 10, 20, 0 ) );

    const size_t prior_size = ring.records().size();
    const ecology_debug::delta_observation_result unchanged = ring.observe( before, before,
            context() );
    CHECK( unchanged.emitted == 0 );
    CHECK( ring.records().size() == prior_size );

    ecology_debug::selected_projection status_only = before;
    status_only.detail->members.front().status = "missing";
    ecology_debug::member_detail newcomer;
    newcomer.npc_id = character_id( 99 );
    newcomer.hp_percent = 22;
    newcomer.status = "joined";
    status_only.detail->members.push_back( newcomer );
    const ecology_debug::delta_observation_result non_hp_change = ring.observe( before,
            status_only, context() );
    CHECK( non_hp_change.emitted == 0 );
    CHECK( ring.records().size() == prior_size );

    ecology_debug::selected_projection unloaded = before;
    unloaded.marker.loaded = false;
    unloaded.detail->hp_percent.reset();
    unloaded.detail->members.front().hp_percent.reset();
    const ecology_debug::delta_observation_result hp_hidden = ring.observe( before, unloaded,
            context() );
    CHECK( hp_hidden.emitted == 0 );
    const ecology_debug::delta_observation_result hp_visible = ring.observe( unloaded, before,
            context() );
    CHECK( hp_visible.emitted == 0 );
    CHECK( ring.records().size() == prior_size );

    ecology_debug::selected_projection after = before;
    after.marker.omt = tripoint_abs_omt( 11, 21, -1 );
    after.marker.state = "returning";
    after.marker.provenance = ecology_debug::entity_provenance::debug_intervention;
    after.detail->phase = "returning";
    after.detail->members.front().hp_percent = 61;
    ecology_debug::delta_observation_context changed_context = context();
    changed_context.disposition = ecology_debug::trigger_disposition::pause;
    const ecology_debug::delta_observation_result changed = ring.observe( before, after,
            changed_context );

    REQUIRE( changed.emitted == 3 );
    CHECK( changed.disposition == ecology_debug::trigger_disposition::pause );
    REQUIRE( ring.records().size() == prior_size + 3 );
    CHECK( ring.records()[prior_size].kind == ecology_debug::delta_kind::moved );
    CHECK( ring.records()[prior_size + 1].kind == ecology_debug::delta_kind::phase_changed );
    CHECK( ring.records()[prior_size + 2].kind == ecology_debug::delta_kind::hp_changed );
    for( size_t index = prior_size; index < ring.records().size(); ++index ) {
        CHECK( ring.records()[index].provenance ==
               ecology_debug::entity_provenance::debug_intervention );
        CHECK( ring.records()[index].turn == 1234 );
        CHECK( ring.records()[index].timestamp == "2026-08-05T12:00:00Z" );
        REQUIRE( ring.records()[index].before );
        REQUIRE( ring.records()[index].after );
    }
    CHECK( ring.records()[prior_size + 2].before->members.front().hp_percent == 100 );
    CHECK( ring.records()[prior_size + 2].after->members.front().hp_percent == 61 );
}

TEST_CASE( "ecology_debug_delta_fails_closed_on_missing_or_changed_authority",
           "[ecology_debug][delta][phase4]" )
{
    ecology_debug::delta_ring ring;
    ecology_debug::selected_projection before = make_projection();

    const ecology_debug::delta_observation_result missing = ring.observe( before, std::nullopt,
            context() );
    REQUIRE( missing.emitted == 1 );
    CHECK( missing.anomaly );
    CHECK( missing.disposition == ecology_debug::trigger_disposition::fail );
    CHECK( ring.records().back().kind == ecology_debug::delta_kind::anomaly );
    CHECK( ring.records().back().anomaly_reason == "selected_entity_missing" );

    ecology_debug::selected_projection replacement = before;
    replacement.token.authority_token = "site/9:outing/8";
    const ecology_debug::delta_observation_result mismatch = ring.observe( before, replacement,
            context() );
    REQUIRE( mismatch.emitted == 1 );
    CHECK( mismatch.anomaly );
    CHECK( ring.records().back().anomaly_reason == "entity_token_mismatch" );

    ecology_debug::selected_projection invalid = before;
    invalid.token.canonical_id.clear();
    const ecology_debug::delta_observation_result invalid_result = ring.observe( std::nullopt,
            invalid, context() );
    REQUIRE( invalid_result.emitted == 1 );
    CHECK( invalid_result.anomaly );
    CHECK( ring.records().back().anomaly_reason == "current_token_invalid" );
    CHECK_FALSE( ring.records().back().before );
    REQUIRE( ring.records().back().after );

    ecology_debug::selected_projection local = before;
    local.marker.owner = "local";
    local.token.owner = "local";
    local.token.authority_token = "site/4:local/42";
    const ecology_debug::delta_observation_result local_result = ring.observe( std::nullopt,
            local, context() );
    REQUIRE( local_result.emitted == 1 );
    CHECK_FALSE( local_result.anomaly );
    CHECK( ring.records().back().kind == ecology_debug::delta_kind::appeared );

    ecology_debug::selected_projection owner_changed = before;
    owner_changed.marker.owner = "local";
    owner_changed.token.owner = "local";
    const ecology_debug::delta_observation_result owner_mismatch = ring.observe( before,
            owner_changed, context() );
    REQUIRE( owner_mismatch.emitted == 1 );
    CHECK( owner_mismatch.anomaly );
    CHECK( ring.records().back().anomaly_reason == "entity_token_mismatch" );
}

TEST_CASE( "ecology_debug_delta_cap_and_json_are_exact_and_deterministic",
           "[ecology_debug][delta][phase4]" )
{
    ecology_debug::delta_ring left;
    ecology_debug::delta_ring right;
    ecology_debug::selected_projection projection = make_projection();
    for( int index = 0; index < 130; ++index ) {
        ecology_debug::delta_observation_context event_context = context();
        projection.marker.provenance = index % 2 == 0 ?
                                       ecology_debug::entity_provenance::natural :
                                       ecology_debug::entity_provenance::debug_intervention;
        event_context.turn = index;
        event_context.timestamp = "tick-" + std::to_string( index );
        REQUIRE( left.observe( std::nullopt, projection, event_context ).emitted == 1 );
        REQUIRE( right.observe( std::nullopt, projection, event_context ).emitted == 1 );
    }

    REQUIRE( left.records().size() == ecology_debug::ecology_delta_record_cap );
    CHECK( left.records().front().sequence == 3 );
    CHECK( left.records().back().sequence == 130 );
    const ecology_debug::delta_ring_metadata metadata = left.metadata();
    CHECK( metadata.retained_count == 128 );
    CHECK( metadata.dropped_count == 2 );
    CHECK( metadata.truncated );

    const std::string left_json = left.serialize_compact_json();
    const std::string right_json = right.serialize_compact_json();
    CHECK( left_json == right_json );
    CHECK( metadata.trace_bytes == left_json.size() );
    const JsonObject parsed = json_loader::from_string( left_json ).get_object();
    parsed.allow_omitted_members();
    const JsonObject parsed_metadata = parsed.get_object( "metadata" );
    parsed_metadata.allow_omitted_members();
    CHECK( parsed_metadata.get_int( "retained_count" ) == 128 );
    CHECK( parsed_metadata.get_int( "dropped_count" ) == 2 );
    CHECK( parsed_metadata.get_bool( "truncated" ) );
    CHECK( static_cast<size_t>( parsed_metadata.get_int( "trace_bytes" ) ) == left_json.size() );
    const JsonArray records = parsed.get_array( "records" );
    CHECK( records.size() == 128 );
    const JsonObject first_record = records.get_object( 0 );
    first_record.allow_omitted_members();
    const JsonObject last_record = records.get_object( 127 );
    last_record.allow_omitted_members();
    CHECK( first_record.get_int( "sequence" ) == 3 );
    CHECK( last_record.get_int( "sequence" ) == 130 );

    left.clear();
    CHECK( left.records().empty() );
    CHECK_FALSE( left.metadata().truncated );
    CHECK( left.observe( std::nullopt, projection, context() ).emitted == 1 );
    CHECK( left.records().front().sequence == 1 );
}
