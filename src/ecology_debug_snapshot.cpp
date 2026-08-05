#include "ecology_debug_snapshot.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "json.h"

namespace ecology_debug
{
namespace
{

struct bounded_text {
    std::string value;
    bool truncated = false;
    bool sanitized = false;
};

struct bounded_context {
    bounded_text schema;
    std::optional<bounded_text> commit;
    std::optional<bounded_text> binary;
    std::optional<bounded_text> scenario;
    bounded_text calendar_turn;
    bounded_text timestamp;
    bounded_text region_label;
    std::vector<bounded_text> filter_labels;
    bounded_text selected_id;
    std::vector<std::string> truncated_fields;
    std::vector<std::string> sanitized_fields;
};

size_t utf8_sequence_length( unsigned char first )
{
    if( first < 0x80 ) {
        return 1;
    }
    if( first >= 0xC2 && first <= 0xDF ) {
        return 2;
    }
    if( first >= 0xE0 && first <= 0xEF ) {
        return 3;
    }
    if( first >= 0xF0 && first <= 0xF4 ) {
        return 4;
    }
    return 0;
}

bool valid_utf8_sequence( std::string_view input, size_t offset, size_t length )
{
    if( length <= 1 || offset + length > input.size() ) {
        return length == 1;
    }
    for( size_t index = 1; index < length; ++index ) {
        const unsigned char continuation = static_cast<unsigned char>( input[offset + index] );
        if( continuation < 0x80 || continuation > 0xBF ) {
            return false;
        }
    }
    const unsigned char first = static_cast<unsigned char>( input[offset] );
    const unsigned char second = static_cast<unsigned char>( input[offset + 1] );
    if( first == 0xE0 && second < 0xA0 ) {
        return false;
    }
    if( first == 0xED && second > 0x9F ) {
        return false;
    }
    if( first == 0xF0 && second < 0x90 ) {
        return false;
    }
    if( first == 0xF4 && second > 0x8F ) {
        return false;
    }
    return true;
}

bounded_text bound_identity( std::string_view input )
{
    bounded_text result;
    result.value.reserve( std::min( input.size(), snapshot_identity_limit ) );
    size_t offset = 0;
    while( offset < input.size() ) {
        const unsigned char first = static_cast<unsigned char>( input[offset] );
        if( first < 0x20 || first == 0x7F ) {
            if( result.value.size() >= snapshot_identity_limit ) {
                result.truncated = true;
                break;
            }
            result.value.push_back( '?' );
            result.sanitized = true;
            ++offset;
            continue;
        }
        const size_t length = utf8_sequence_length( first );
        if( length == 0 || !valid_utf8_sequence( input, offset, length ) ) {
            if( result.value.size() >= snapshot_identity_limit ) {
                result.truncated = true;
                break;
            }
            result.value.push_back( '?' );
            result.sanitized = true;
            ++offset;
            continue;
        }
        if( result.value.size() + length > snapshot_identity_limit ) {
            result.truncated = true;
            break;
        }
        result.value.append( input.substr( offset, length ) );
        offset += length;
    }
    result.truncated = result.truncated || offset < input.size();
    return result;
}

void record_identity_status( bounded_context &bounded, const bounded_text &text,
                             const std::string &field )
{
    if( text.truncated ) {
        bounded.truncated_fields.push_back( field );
    }
    if( text.sanitized ) {
        bounded.sanitized_fields.push_back( field );
    }
}

bounded_context make_bounded_context( const snapshot_context &context, bool include_volatile )
{
    bounded_context result;
    result.schema = bound_identity( context.schema );
    record_identity_status( result, result.schema, "schema" );
    const auto copy_optional = [&result]( const std::optional<std::string> &input,
    const std::string &field ) -> std::optional<bounded_text> {
        if( !input ) {
            return std::nullopt;
        }
        bounded_text text = bound_identity( *input );
        record_identity_status( result, text, field );
        return text;
    };
    result.commit = copy_optional( context.commit, "commit" );
    result.binary = copy_optional( context.binary, "binary" );
    result.scenario = copy_optional( context.scenario, "scenario" );
    if( include_volatile ) {
        result.calendar_turn = bound_identity( context.calendar_turn );
        record_identity_status( result, result.calendar_turn, "calendar_turn" );
        result.timestamp = bound_identity( context.timestamp );
        record_identity_status( result, result.timestamp, "timestamp" );
    }
    result.region_label = bound_identity( context.region.enabled ? "bounded" : "all" );
    result.filter_labels.reserve( context.filter_labels.size() );
    for( size_t index = 0; index < context.filter_labels.size(); ++index ) {
        bounded_text label = bound_identity( context.filter_labels[index] );
        record_identity_status( result, label, "filter_labels[" + std::to_string( index ) + "]" );
        result.filter_labels.push_back( std::move( label ) );
    }
    result.selected_id = bound_identity( context.selected_id );
    record_identity_status( result, result.selected_id, "selected_id" );
    return result;
}

void write_optional_string( JsonOut &json, std::string_view name,
                            const std::optional<bounded_text> &value )
{
    if( value ) {
        json.member( name, value->value );
    } else {
        json.null_member( name );
    }
}

void write_omt( JsonOut &json, const tripoint_abs_omt &omt )
{
    json.start_array();
    json.write( omt.x() );
    json.write( omt.y() );
    json.write( omt.z() );
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

void write_context( JsonOut &json, const snapshot_context &context,
                    const bounded_context &bounded, bool include_volatile )
{
    json.member( "context" );
    json.start_object();
    write_optional_string( json, "commit", bounded.commit );
    write_optional_string( json, "binary", bounded.binary );
    write_optional_string( json, "scenario", bounded.scenario );
    if( include_volatile ) {
        json.member( "calendar_turn", bounded.calendar_turn.value );
        json.member( "timestamp", bounded.timestamp.value );
    }
    if( include_volatile ) {
        json.member( "player_omt" );
        write_omt( json, context.player_omt );
        json.member( "region" );
        json.start_object();
        json.member( "label", bounded.region_label.value );
        json.member( "enabled", context.region.enabled );
        json.member( "minimum" );
        write_omt( json, context.region.minimum );
        json.member( "maximum" );
        write_omt( json, context.region.maximum );
        json.member( "selected_outside_region_included",
                     context.selected_outside_region_included );
        json.end_object();
    }
    json.member( "filters" );
    json.start_object();
    json.member( "camps", context.filters.camps );
    json.member( "dispatches", context.filters.dispatches );
    json.member( "bandits", context.filters.bandits );
    json.member( "cannibals", context.filters.cannibals );
    json.member( "loaded_only", context.filters.loaded_only );
    json.member( "labels" );
    json.start_array();
    for( const bounded_text &label : bounded.filter_labels ) {
        json.write( label.value );
    }
    json.end_array();
    json.end_object();
    json.member( "selected_id", bounded.selected_id.value );
    json.end_object();
}

void write_metadata( JsonOut &json, const query_metadata &metadata,
                     const bounded_context &bounded, bool include_volatile )
{
    json.member( "metadata" );
    json.start_object();
    json.member( "candidate_count", metadata.candidate_count );
    json.member( "considered_count", metadata.considered_count );
    json.member( "emitted_count", metadata.emitted_count );
    json.member( "dropped_count", metadata.dropped_count );
    json.member( "candidate_cap", metadata.candidate_limit );
    json.member( "marker_cap", metadata.marker_limit );
    json.member( "event_cap", metadata.event_limit );
    json.member( "truncated", metadata.truncated );
    if( include_volatile ) {
        json.member( "query_us", metadata.query_microseconds );
        json.member( "render_us", metadata.render_microseconds );
    }
    if( include_volatile ) {
        json.member( "trace_bytes", metadata.trace_bytes );
    }
    json.member( "identity_limit_bytes", snapshot_identity_limit );
    json.member( "identity_truncated", !bounded.truncated_fields.empty() );
    json.member( "identity_truncated_fields", bounded.truncated_fields );
    json.member( "identity_sanitized", !bounded.sanitized_fields.empty() );
    json.member( "identity_sanitized_fields", bounded.sanitized_fields );
    json.end_object();
}

void write_entity( JsonOut &json, const entity_marker &entity )
{
    json.start_object();
    json.member( "id", entity.id );
    json.member( "alias", entity.alias );
    json.member( "kind", to_string( entity.kind ) );
    json.member( "faction", to_string( entity.faction ) );
    json.member( "omt" );
    write_omt( json, entity.omt );
    json.member( "owner", entity.owner );
    json.member( "loaded", entity.loaded );
    json.member( "state", entity.state );
    json.member( "provenance", to_string( entity.provenance ) );
    json.member( "generation", entity.generation );
    json.end_object();
}

void write_selected( JsonOut &json, const std::optional<selected_detail> &selected,
                     bool include_volatile )
{
    json.member( "selected" );
    if( !selected ) {
        json.write_null();
        return;
    }
    json.start_object();
    json.member( "entity_id", selected->entity_id );
    json.member( "source_camp_id", selected->source_camp_id );
    json.member( "phase", selected->phase );
    json.member( "last_transition_minutes", selected->last_transition_minutes );
    json.member( "last_transition_reason", selected->last_transition_reason );
    json.member( "blocked_reason", selected->blocked_reason );
    json.member( "evidence_reason", selected->evidence_reason );
    if( include_volatile ) {
        json.member( "evidence_age_minutes", selected->evidence_age_minutes );
    }
    json.member( "next_deadline_minutes", selected->next_deadline_minutes );
    json.member( "destination" );
    write_omt( json, selected->destination );
    json.member( "route" );
    json.start_array();
    for( const tripoint_abs_omt &omt : selected->route ) {
        write_omt( json, omt );
    }
    json.end_array();
    json.member( "members" );
    json.start_array();
    for( const member_detail &member : selected->members ) {
        json.start_object();
        json.member( "npc_id", member.npc_id.get_value() );
        json.member( "name", member.name );
        write_optional_int( json, "hp_percent", member.hp_percent );
        json.member( "status", member.status );
        json.member( "loaded", member.loaded );
        json.end_object();
    }
    json.end_array();
    json.end_object();
}

void write_snapshot_impl( JsonOut &json, const view_snapshot &view,
                          const snapshot_context &context, bool include_volatile )
{
    const bounded_context bounded = make_bounded_context( context, include_volatile );
    json.start_object();
    json.member( "schema", bounded.schema.value );
    json.member( "version", context.version );
    write_context( json, context, bounded, include_volatile );
    write_metadata( json, view.metadata, bounded, include_volatile );
    json.member( "entities" );
    json.start_array();
    for( const entity_marker &entity : view.entities ) {
        write_entity( json, entity );
    }
    json.end_array();
    write_selected( json, view.selected, include_volatile );
    json.end_object();
}

} // namespace

void write_snapshot( JsonOut &json, const view_snapshot &view,
                     const snapshot_context &context )
{
    write_snapshot_impl( json, view, context, true );
}

std::string serialize_snapshot( const view_snapshot &view,
                                const snapshot_context &context )
{
    std::ostringstream output;
    JsonOut json( output, false );
    write_snapshot( json, view, context );
    return output.str();
}

std::string serialize_sized_snapshot( const view_snapshot &view,
                                      const snapshot_context &context )
{
    view_snapshot sized = view;
    sized.metadata.render_microseconds = 0;
    sized.metadata.trace_bytes = 0;
    const auto render_started = std::chrono::steady_clock::now();
    const std::string base = serialize_snapshot( sized, context );
    const auto replace_number = []( std::string & payload, std::string_view field,
    const std::string & value ) {
        const size_t field_pos = payload.find( field );
        if( field_pos == std::string::npos ) {
            return;
        }
        const size_t value_begin = field_pos + field.size();
        size_t value_end = value_begin;
        while( value_end < payload.size() && payload[value_end] >= '0' &&
               payload[value_end] <= '9' ) {
            ++value_end;
        }
        payload.replace( value_begin, value_end - value_begin, value );
    };
    const auto build_payload = [&base, &replace_number]( long long render_microseconds ) {
        const std::string render_value = std::to_string( render_microseconds );
        size_t payload_bytes = base.size();
        for( int attempt = 0; attempt < 4; ++attempt ) {
            const size_t next = base.size() - 2 + render_value.size() +
                                std::to_string( payload_bytes ).size();
            if( next == payload_bytes ) {
                break;
            }
            payload_bytes = next;
        }
        std::string output = base;
        replace_number( output, "\"trace_bytes\":", std::to_string( payload_bytes ) );
        replace_number( output, "\"render_us\":", render_value );
        return output;
    };

    long long render_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
                                       std::chrono::steady_clock::now() - render_started ).count();
    std::string output = build_payload( render_microseconds );
    for( int attempt = 0; attempt < 4; ++attempt ) {
        const long long total_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
                                                std::chrono::steady_clock::now() - render_started ).count();
        if( std::to_string( total_microseconds ).size() ==
            std::to_string( render_microseconds ).size() ) {
            replace_number( output, "\"render_us\":", std::to_string( total_microseconds ) );
            return output;
        }
        render_microseconds = total_microseconds;
        output = build_payload( render_microseconds );
    }
    return output;
}

view_snapshot selected_monitor_projection( const view_snapshot &view,
        std::string_view selected_id )
{
    view_snapshot result;
    result.metadata.candidate_limit = view.metadata.candidate_limit;
    result.metadata.marker_limit = view.metadata.marker_limit;
    result.metadata.event_limit = view.metadata.event_limit;

    const auto selected_marker = std::find_if( view.entities.begin(), view.entities.end(),
    [selected_id]( const entity_marker & marker ) {
        return marker.id == selected_id;
    } );
    if( selected_marker == view.entities.end() ) {
        return result;
    }

    result.entities.push_back( *selected_marker );
    result.metadata.candidate_count = 1;
    result.metadata.considered_count = 1;
    result.metadata.emitted_count = 1;
    if( view.selected && view.selected->entity_id == selected_id ) {
        result.selected = view.selected;
    }
    return result;
}

std::string serialize_monitor_snapshot( const view_snapshot &view,
                                        const snapshot_context &context )
{
    std::ostringstream output;
    JsonOut json( output, false );
    write_snapshot_impl( json, view, context, false );
    return output.str();
}

} // namespace ecology_debug
