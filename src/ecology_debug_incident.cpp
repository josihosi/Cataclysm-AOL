#include "ecology_debug_incident.h"

#include <algorithm>
#include <sstream>
#include <tuple>
#include <utility>

#include "json.h"

namespace ecology_debug
{
namespace
{

bool token_self_consistent( const selected_projection &projection )
{
    const bool detail_matches = !projection.detail ||
                                projection.detail->entity_id == projection.marker.id;
    return detail_matches &&
           !projection.token.world_identity.empty() &&
           !projection.token.canonical_id.empty() &&
           !projection.token.owner.empty() &&
           !projection.token.authority_token.empty() &&
           projection.token.canonical_id == projection.marker.id &&
           projection.token.generation == projection.marker.generation &&
           projection.token.owner == projection.marker.owner;
}

void write_omt( JsonOut &json, const tripoint_abs_omt &omt )
{
    json.start_array();
    json.write( omt.x() );
    json.write( omt.y() );
    json.write( omt.z() );
    json.end_array();
}

void write_ms( JsonOut &json, const tripoint_abs_ms &ms )
{
    json.start_array();
    json.write( ms.x() );
    json.write( ms.y() );
    json.write( ms.z() );
    json.end_array();
}

void write_optional_int( JsonOut &json, std::string_view name, const std::optional<int> &value )
{
    if( value ) {
        json.member( name, *value );
    } else {
        json.null_member( name );
    }
}

void write_optional_size( JsonOut &json, std::string_view name,
                          const std::optional<size_t> &value )
{
    if( value ) {
        json.member( name, *value );
    } else {
        json.null_member( name );
    }
}

void write_optional_omt( JsonOut &json, std::string_view name,
                         const std::optional<tripoint_abs_omt> &value )
{
    json.member( name );
    if( value ) {
        write_omt( json, *value );
    } else {
        json.write_null();
    }
}

void write_token( JsonOut &json, const immutable_entity_token &token )
{
    json.start_object();
    json.member( "world_identity", token.world_identity );
    json.member( "canonical_id", token.canonical_id );
    json.member( "generation", token.generation );
    json.member( "owner", token.owner );
    json.member( "authority_token", token.authority_token );
    json.end_object();
}

void write_selected( JsonOut &json, const selected_projection &projection )
{
    json.start_object();
    json.member( "token" );
    write_token( json, projection.token );
    json.member( "id", projection.marker.id );
    json.member( "alias", projection.marker.alias );
    json.member( "kind", to_string( projection.marker.kind ) );
    json.member( "faction", to_string( projection.marker.faction ) );
    json.member( "omt" );
    write_omt( json, projection.marker.omt );
    json.member( "owner", projection.marker.owner );
    json.member( "loaded", projection.marker.loaded );
    json.member( "phase", projection.marker.state );
    json.member( "provenance", to_string( projection.marker.provenance ) );
    json.member( "generation", projection.marker.generation );
    json.member( "details" );
    if( !projection.detail ) {
        json.write_null();
        json.end_object();
        return;
    }

    const selected_detail &detail = *projection.detail;
    json.start_object();
    json.member( "source_camp_id", detail.source_camp_id );
    json.member( "phase", detail.phase );
    json.member( "last_transition_minutes", detail.last_transition_minutes );
    json.member( "last_transition_reason", detail.last_transition_reason );
    json.member( "blocked_reason", detail.blocked_reason );
    json.member( "evidence_id", detail.evidence_id );
    json.member( "evidence_kind", detail.evidence_kind );
    json.member( "evidence_state", detail.evidence_state );
    json.member( "evidence_reason", detail.evidence_reason );
    json.member( "evidence_observed_minutes", detail.evidence_observed_minutes );
    json.member( "evidence_age_minutes", detail.evidence_age_minutes );
    json.member( "next_deadline_minutes", detail.next_deadline_minutes );
    json.member( "destination" );
    write_omt( json, detail.destination );
    json.member( "route" );
    json.start_array();
    for( const tripoint_abs_omt &omt : detail.route ) {
        write_omt( json, omt );
    }
    json.end_array();

    std::vector<member_detail> members = detail.members;
    std::sort( members.begin(), members.end(),
    []( const member_detail & lhs, const member_detail & rhs ) {
        return std::tie( lhs.npc_id, lhs.name, lhs.status ) <
               std::tie( rhs.npc_id, rhs.name, rhs.status );
    } );
    json.member( "members" );
    json.start_array();
    for( const member_detail &member : members ) {
        json.start_object();
        json.member( "npc_id", member.npc_id.get_value() );
        json.member( "name", member.name );
        write_optional_int( json, "hp_percent", member.hp_percent );
        json.member( "status", member.status );
        json.member( "loaded", member.loaded );
        json.member( "position_ms" );
        if( member.loaded && member.position_ms ) {
            write_ms( json, *member.position_ms );
        } else {
            json.write_null();
        }
        write_optional_int( json, "moves", member.loaded ? member.moves : std::nullopt );
        json.member( "goal_omt" );
        if( member.loaded && member.goal_omt ) {
            write_omt( json, *member.goal_omt );
        } else {
            json.write_null();
        }
        write_optional_size( json, "local_path_size",
                             member.loaded ? member.local_path_size : std::nullopt );
        write_optional_size( json, "omt_path_size",
                             member.loaded ? member.omt_path_size : std::nullopt );
        json.member( "movement_state", member.loaded ? member.movement_state : "" );
        json.member( "movement_blocker", member.loaded ? member.movement_blocker : "" );
        json.end_object();
    }
    json.end_array();
    write_optional_int( json, "population", detail.population );
    write_optional_int( json, "interest", detail.interest );
    write_optional_omt( json, "target", detail.target );
    write_optional_int( json, "hp_percent", detail.hp_percent );
    json.end_object();
    json.end_object();
}

void write_delta_summary( JsonOut &json, const delta_entity_summary &summary )
{
    json.start_object();
    json.member( "id", summary.id );
    json.member( "alias", summary.alias );
    json.member( "kind", to_string( summary.kind ) );
    json.member( "faction", to_string( summary.faction ) );
    json.member( "omt" );
    write_omt( json, summary.omt );
    json.member( "owner", summary.owner );
    json.member( "loaded", summary.loaded );
    json.member( "phase", summary.phase );
    json.member( "generation", summary.generation );
    write_optional_int( json, "population", summary.population );
    write_optional_int( json, "interest", summary.interest );
    write_optional_omt( json, "target", summary.target );
    write_optional_int( json, "hp_percent", summary.hp_percent );
    json.member( "members" );
    json.start_array();
    for( const delta_member_summary &member : summary.members ) {
        json.start_object();
        json.member( "npc_id", member.npc_id );
        write_optional_int( json, "hp_percent", member.hp_percent );
        json.member( "status", member.status );
        json.end_object();
    }
    json.end_array();
    json.end_object();
}

void write_deltas( JsonOut &json, const delta_ring &deltas )
{
    const delta_ring_metadata metadata = deltas.metadata();
    json.start_object();
    json.member( "schema", "c-aol.ecology.delta" );
    json.member( "version", 1 );
    json.member( "metadata" );
    json.start_object();
    json.member( "retained_count", metadata.retained_count );
    json.member( "dropped_count", metadata.dropped_count );
    json.member( "record_limit", metadata.record_limit );
    json.member( "truncated", metadata.truncated );
    json.member( "trace_bytes", metadata.trace_bytes );
    json.end_object();
    json.member( "records" );
    json.start_array();
    for( const delta_record &record : deltas.records() ) {
        json.start_object();
        json.member( "sequence", record.sequence );
        json.member( "type", to_string( record.kind ) );
        json.member( "turn", record.turn );
        json.member( "timestamp", record.timestamp );
        json.member( "entity_id", record.entity_id );
        json.member( "provenance", to_string( record.provenance ) );
        json.member( "disposition", to_string( record.disposition ) );
        json.member( "before" );
        if( record.before ) {
            write_delta_summary( json, *record.before );
        } else {
            json.write_null();
        }
        json.member( "after" );
        if( record.after ) {
            write_delta_summary( json, *record.after );
        } else {
            json.write_null();
        }
        if( record.anomaly_reason.empty() ) {
            json.null_member( "anomaly_reason" );
        } else {
            json.member( "anomaly_reason", record.anomaly_reason );
        }
        json.end_object();
    }
    json.end_array();
    json.end_object();
}

std::string truncate_utf8( const std::string &input, size_t limit )
{
    if( input.size() <= limit ) {
        return input;
    }
    size_t length = limit;
    while( length > 0 && ( static_cast<unsigned char>( input[length] ) & 0xc0 ) == 0x80 ) {
        --length;
    }
    return input.substr( 0, length );
}

std::string build_payload( const incident_identity &identity,
                           const selected_projection &selected,
                           const delta_ring &deltas,
                           const std::optional<std::string> &note,
                           const std::vector<incident_intervention> &interventions,
                           const std::optional<incident_watch_state> &watch,
                           size_t intervention_input_count,
                           size_t payload_bytes )
{
    std::ostringstream output;
    JsonOut json( output, false );
    json.start_object();
    json.member( "schema", "c-aol.ecology.incident" );
    json.member( "version", 2 );
    json.member( "identity" );
    json.start_object();
    json.member( "turn", identity.turn );
    json.member( "timestamp", identity.timestamp );
    json.member( "player_omt" );
    write_omt( json, identity.player_omt );
    json.member( "scenario", identity.scenario );
    json.member( "commit", identity.commit );
    json.member( "binary", identity.binary );
    json.member( "run_identity", identity.run_identity );
    json.end_object();
    json.member( "metadata" );
    json.start_object();
    json.member( "payload_bytes", payload_bytes );
    json.member( "note_byte_limit", ecology_incident_note_byte_cap );
    json.member( "note_input_bytes", note ? note->size() : 0 );
    json.member( "note_truncated", note && note->size() > ecology_incident_note_byte_cap );
    json.member( "intervention_limit", ecology_incident_intervention_cap );
    json.member( "intervention_input_count", intervention_input_count );
    json.member( "intervention_retained_count", interventions.size() );
    json.member( "intervention_dropped_count", intervention_input_count - interventions.size() );
    json.member( "intervention_truncated", intervention_input_count > interventions.size() );
    json.end_object();
    json.member( "human_note" );
    if( note ) {
        json.write( truncate_utf8( *note, ecology_incident_note_byte_cap ) );
    } else {
        json.write_null();
    }
    json.member( "selected" );
    write_selected( json, selected );
    json.member( "deltas" );
    write_deltas( json, deltas );
    json.member( "watch" );
    if( watch ) {
        write_watch_json( json, watch->spec, watch->input, watch->result );
    } else {
        json.write_null();
    }
    json.member( "interventions" );
    json.start_array();
    for( const incident_intervention &intervention : interventions ) {
        json.start_object();
        json.member( "sequence", intervention.sequence );
        json.member( "turn", intervention.turn );
        json.member( "timestamp", intervention.timestamp );
        json.member( "entity_id", intervention.entity_id );
        json.member( "action", intervention.action );
        json.member( "before", intervention.before_summary );
        json.member( "after", intervention.after_summary );
        json.member( "debug_intervention", true );
        json.end_object();
    }
    json.end_array();
    json.end_object();
    return output.str();
}

std::string validate( const incident_identity &identity,
                      const std::optional<selected_projection> &selected,
                      const delta_ring &deltas,
                      const std::vector<incident_intervention> &interventions )
{
    if( identity.turn < 0 || identity.timestamp.empty() || identity.scenario.empty() ||
        identity.commit.empty() || identity.binary.empty() || identity.run_identity.empty() ) {
        return "incident_identity_invalid";
    }
    if( identity.timestamp.size() > ecology_incident_caller_text_byte_cap ||
        identity.scenario.size() > ecology_incident_caller_text_byte_cap ||
        identity.commit.size() > ecology_incident_caller_text_byte_cap ||
        identity.binary.size() > ecology_incident_caller_text_byte_cap ||
        identity.run_identity.size() > ecology_incident_caller_text_byte_cap ) {
        return "incident_identity_text_too_long";
    }
    if( !selected ) {
        return "incident_selection_missing";
    }
    if( !token_self_consistent( *selected ) ) {
        return "incident_selection_token_invalid";
    }
    for( const delta_record &record : deltas.records() ) {
        if( !record.entity_id.empty() && record.entity_id != selected->token.canonical_id ) {
            return "incident_delta_token_mismatch";
        }
    }
    for( const incident_intervention &intervention : interventions ) {
        if( !intervention.debug_intervention ) {
            return "incident_intervention_provenance_invalid";
        }
        if( intervention.timestamp.size() > ecology_incident_caller_text_byte_cap ||
            intervention.entity_id.size() > ecology_incident_caller_text_byte_cap ||
            intervention.action.size() > ecology_incident_caller_text_byte_cap ||
            intervention.before_summary.size() > ecology_incident_caller_text_byte_cap ||
            intervention.after_summary.size() > ecology_incident_caller_text_byte_cap ) {
            return "incident_intervention_text_too_long";
        }
        if( intervention.sequence == 0 || intervention.turn < 0 ) {
            return "incident_intervention_order_invalid";
        }
        if( intervention.entity_id != selected->token.canonical_id ) {
            return "incident_intervention_token_mismatch";
        }
        if( intervention.timestamp.empty() || intervention.action.empty() ) {
            return "incident_intervention_invalid";
        }
    }
    return {};
}

} // namespace

incident_bundle_result serialize_incident_bundle(
    const incident_identity &identity,
    const std::optional<selected_projection> &selected,
    const delta_ring &deltas,
    const std::optional<std::string> &human_note,
    const std::vector<incident_intervention> &interventions,
    const std::optional<incident_watch_state> &watch )
{
    incident_bundle_result result;
    result.error = validate( identity, selected, deltas, interventions );
    if( !result.error.empty() ) {
        return result;
    }

    std::vector<incident_intervention> retained = interventions;
    std::sort( retained.begin(), retained.end(),
    []( const incident_intervention & lhs, const incident_intervention & rhs ) {
        return std::tie( lhs.sequence, lhs.turn, lhs.timestamp, lhs.entity_id, lhs.action,
                         lhs.before_summary, lhs.after_summary ) <
               std::tie( rhs.sequence, rhs.turn, rhs.timestamp, rhs.entity_id, rhs.action,
                         rhs.before_summary, rhs.after_summary );
    } );
    if( retained.size() > ecology_incident_intervention_cap ) {
        retained.erase( retained.begin(), retained.end() - ecology_incident_intervention_cap );
    }

    size_t payload_bytes = 0;
    for( int attempt = 0; attempt < 8; ++attempt ) {
        result.payload = build_payload( identity, *selected, deltas, human_note, retained, watch,
                                        interventions.size(), payload_bytes );
        if( result.payload.size() == payload_bytes ) {
            break;
        }
        payload_bytes = result.payload.size();
    }
    result.payload_bytes = result.payload.size();
    result.valid = true;
    result.note_truncated = human_note && human_note->size() > ecology_incident_note_byte_cap;
    result.intervention_dropped_count = interventions.size() - retained.size();
    return result;
}

} // namespace ecology_debug
