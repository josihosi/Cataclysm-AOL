#include "ecology_debug_delta.h"

#include <algorithm>
#include <sstream>
#include <tuple>
#include <utility>

#include "json.h"

namespace ecology_debug
{
namespace
{

delta_entity_summary make_summary( const selected_projection &projection )
{
    delta_entity_summary result;
    result.id = projection.marker.id;
    result.alias = projection.marker.alias;
    result.kind = projection.marker.kind;
    result.faction = projection.marker.faction;
    result.omt = projection.marker.omt;
    result.owner = projection.marker.owner;
    result.loaded = projection.marker.loaded;
    result.phase = projection.marker.state;
    result.generation = projection.marker.generation;
    if( !projection.detail ) {
        return result;
    }

    const selected_detail &detail = *projection.detail;
    if( !detail.phase.empty() ) {
        result.phase = detail.phase;
    }
    result.population = detail.population;
    result.interest = detail.interest;
    result.target = detail.target;
    result.hp_percent = detail.hp_percent;
    result.members.reserve( detail.members.size() );
    for( const member_detail &member : detail.members ) {
        result.members.push_back( { member.npc_id.get_value(), member.hp_percent, member.status } );
    }
    std::sort( result.members.begin(), result.members.end(),
    []( const delta_member_summary & lhs, const delta_member_summary & rhs ) {
        return std::tie( lhs.npc_id, lhs.status, lhs.hp_percent ) <
               std::tie( rhs.npc_id, rhs.status, rhs.hp_percent );
    } );
    return result;
}

bool token_self_consistent( const selected_projection &projection )
{
    return !projection.token.world_identity.empty() &&
           !projection.token.canonical_id.empty() &&
           !projection.token.authority_token.empty() &&
           projection.token.canonical_id == projection.marker.id &&
           projection.token.generation == projection.marker.generation &&
           projection.token.owner == projection.marker.owner;
}

bool same_token( const immutable_entity_token &lhs, const immutable_entity_token &rhs )
{
    return lhs.world_identity == rhs.world_identity &&
           lhs.canonical_id == rhs.canonical_id &&
           lhs.generation == rhs.generation &&
           lhs.owner == rhs.owner &&
           lhs.authority_token == rhs.authority_token;
}

bool hp_equal( const delta_entity_summary &lhs, const delta_entity_summary &rhs )
{
    if( lhs.hp_percent && rhs.hp_percent && lhs.hp_percent != rhs.hp_percent ) {
        return false;
    }
    size_t left_index = 0;
    size_t right_index = 0;
    while( left_index < lhs.members.size() && right_index < rhs.members.size() ) {
        const delta_member_summary &left = lhs.members[left_index];
        const delta_member_summary &right = rhs.members[right_index];
        if( left.npc_id < right.npc_id ) {
            ++left_index;
            continue;
        }
        if( right.npc_id < left.npc_id ) {
            ++right_index;
            continue;
        }
        if( left.hp_percent && right.hp_percent && left.hp_percent != right.hp_percent ) {
            return false;
        }
        ++left_index;
        ++right_index;
    }
    return true;
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

void write_summary( JsonOut &json, const delta_entity_summary &summary )
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
    json.member( "target" );
    if( summary.target ) {
        write_omt( json, *summary.target );
    } else {
        json.write_null();
    }
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

std::string serialize_with_size( const std::deque<delta_record> &records,
                                 size_t dropped_count, size_t trace_bytes )
{
    std::ostringstream output;
    JsonOut json( output, false );
    json.start_object();
    json.member( "schema", "c-aol.ecology.delta" );
    json.member( "version", 1 );
    json.member( "metadata" );
    json.start_object();
    json.member( "retained_count", records.size() );
    json.member( "dropped_count", dropped_count );
    json.member( "record_limit", ecology_delta_record_cap );
    json.member( "truncated", dropped_count > 0 );
    json.member( "trace_bytes", trace_bytes );
    json.end_object();
    json.member( "records" );
    json.start_array();
    for( const delta_record &record : records ) {
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
            write_summary( json, *record.before );
        } else {
            json.write_null();
        }
        json.member( "after" );
        if( record.after ) {
            write_summary( json, *record.after );
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
    return output.str();
}

} // namespace

delta_observation_result delta_ring::observe(
    const std::optional<selected_projection> &prior,
    const std::optional<selected_projection> &current,
    const delta_observation_context &context )
{
    delta_observation_result result;
    if( !prior && !current ) {
        return result;
    }

    const auto append_anomaly = [this, &result, &context](
                                    const std::optional<selected_projection> &before_projection,
                                    const std::optional<selected_projection> &after_projection,
                                    const std::string &reason ) {
        delta_record record;
        record.kind = delta_kind::anomaly;
        record.turn = context.turn;
        record.timestamp = context.timestamp;
        record.provenance = after_projection ? after_projection->marker.provenance :
                            before_projection->marker.provenance;
        record.disposition = trigger_disposition::fail;
        record.anomaly_reason = reason;
        if( before_projection ) {
            record.entity_id = before_projection->marker.id;
            record.before = make_summary( *before_projection );
        }
        if( after_projection ) {
            if( record.entity_id.empty() ) {
                record.entity_id = after_projection->marker.id;
            }
            record.after = make_summary( *after_projection );
        }
        append( std::move( record ) );
        result.emitted = 1;
        result.disposition = trigger_disposition::fail;
        result.anomaly = true;
    };

    if( prior && !token_self_consistent( *prior ) ) {
        append_anomaly( prior, current, "prior_token_invalid" );
        return result;
    }
    if( current && !token_self_consistent( *current ) ) {
        append_anomaly( prior, current, "current_token_invalid" );
        return result;
    }
    if( prior && !current ) {
        append_anomaly( prior, current, "selected_entity_missing" );
        return result;
    }
    if( prior && current && !same_token( prior->token, current->token ) ) {
        append_anomaly( prior, current, "entity_token_mismatch" );
        return result;
    }

    const auto append_change = [this, &result, &context, &current]( delta_kind kind,
                               const std::string &entity_id,
                               const std::optional<delta_entity_summary> &before,
                               const std::optional<delta_entity_summary> &after ) {
        delta_record record;
        record.kind = kind;
        record.turn = context.turn;
        record.timestamp = context.timestamp;
        record.entity_id = entity_id;
        record.provenance = current->marker.provenance;
        record.disposition = context.disposition;
        record.before = before;
        record.after = after;
        append( std::move( record ) );
        ++result.emitted;
        result.disposition = context.disposition;
    };

    if( !prior ) {
        const delta_entity_summary after = make_summary( *current );
        append_change( delta_kind::appeared, current->marker.id, std::nullopt, after );
        return result;
    }

    const delta_entity_summary before = make_summary( *prior );
    const delta_entity_summary after = make_summary( *current );
    if( before.omt != after.omt ) {
        append_change( delta_kind::moved, after.id, before, after );
    }
    if( before.phase != after.phase ) {
        append_change( delta_kind::phase_changed, after.id, before, after );
    }
    if( !hp_equal( before, after ) ) {
        append_change( delta_kind::hp_changed, after.id, before, after );
    }
    return result;
}

const std::deque<delta_record> &delta_ring::records() const
{
    return records_;
}

delta_ring_metadata delta_ring::metadata() const
{
    delta_ring_metadata result;
    result.retained_count = records_.size();
    result.dropped_count = dropped_count_;
    result.truncated = dropped_count_ > 0;
    result.trace_bytes = serialize_compact_json().size();
    return result;
}

std::string delta_ring::serialize_compact_json() const
{
    size_t trace_bytes = 0;
    std::string output;
    for( int attempt = 0; attempt < 8; ++attempt ) {
        output = serialize_with_size( records_, dropped_count_, trace_bytes );
        if( output.size() == trace_bytes ) {
            return output;
        }
        trace_bytes = output.size();
    }
    return serialize_with_size( records_, dropped_count_, trace_bytes );
}

void delta_ring::clear()
{
    records_.clear();
    next_sequence_ = 1;
    dropped_count_ = 0;
}

void delta_ring::append( delta_record record )
{
    record.sequence = next_sequence_++;
    records_.push_back( std::move( record ) );
    while( records_.size() > ecology_delta_record_cap ) {
        records_.pop_front();
        ++dropped_count_;
    }
}

std::string to_string( delta_kind kind )
{
    switch( kind ) {
        case delta_kind::appeared:
            return "appeared";
        case delta_kind::moved:
            return "moved";
        case delta_kind::phase_changed:
            return "phase_changed";
        case delta_kind::hp_changed:
            return "hp_changed";
        case delta_kind::anomaly:
            return "anomaly";
    }
    return "anomaly";
}

std::string to_string( trigger_disposition disposition )
{
    switch( disposition ) {
        case trigger_disposition::continue_capture:
            return "continue_capture";
        case trigger_disposition::pause:
            return "pause";
        case trigger_disposition::fail:
            return "fail";
    }
    return "fail";
}

} // namespace ecology_debug
