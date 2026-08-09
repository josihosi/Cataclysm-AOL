#include "ecology_debug_watch.h"

#include <algorithm>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "json.h"

namespace ecology_debug
{
namespace
{

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
           lhs.authority_token == rhs.authority_token;
}

std::string phase_of( const selected_projection &projection )
{
    if( projection.detail && !projection.detail->phase.empty() ) {
        return projection.detail->phase;
    }
    return projection.marker.state;
}

struct evidence_signature {
    std::string id;
    std::string kind;
    std::string state;
    std::string reason;
    int observed_minutes = -1;

    bool empty() const {
        return id.empty() && kind.empty() && state.empty() && reason.empty();
    }

    bool operator==( const evidence_signature &other ) const {
        return id == other.id && kind == other.kind && state == other.state &&
               reason == other.reason && observed_minutes == other.observed_minutes;
    }
};

evidence_signature evidence_of( const selected_projection &projection )
{
    if( !projection.detail ) {
        return {};
    }
    return { projection.detail->evidence_id, projection.detail->evidence_kind,
             projection.detail->evidence_state, projection.detail->evidence_reason,
             projection.detail->evidence_observed_minutes };
}

bool is_exposure_phase( std::string_view phase )
{
    return phase == "burned_withdrawal" || phase == "returning_exposed";
}

bool is_returning_phase( std::string_view phase )
{
    return phase == "returning_report" || phase == "returning_home" ||
           phase == "returning_exposed" || phase == "burned_withdrawal";
}

bool member_terminal( const member_detail &member )
{
    return member.status == "dead" || member.status == "missing" ||
           ( member.hp_percent && *member.hp_percent <= 0 );
}

bool member_dead( const member_detail &member )
{
    return member.status == "dead" || ( member.hp_percent && *member.hp_percent <= 0 );
}

bool hp_changed_between( const selected_projection &prior,
                         const selected_projection &current )
{
    if( !prior.detail || !current.detail ) {
        return false;
    }
    if( prior.detail->hp_percent && current.detail->hp_percent &&
        prior.detail->hp_percent != current.detail->hp_percent ) {
        return true;
    }
    std::unordered_map<int, std::optional<int>> prior_hp;
    for( const member_detail &member : prior.detail->members ) {
        prior_hp.emplace( member.npc_id.get_value(), member.hp_percent );
    }
    for( const member_detail &member : current.detail->members ) {
        const auto found = prior_hp.find( member.npc_id.get_value() );
        if( found != prior_hp.end() && found->second && member.hp_percent &&
            found->second != member.hp_percent ) {
            return true;
        }
    }
    return false;
}

bool all_members_dead( const selected_projection &projection )
{
    if( !projection.detail || projection.detail->members.empty() ) {
        return false;
    }
    return std::all_of( projection.detail->members.begin(), projection.detail->members.end(),
    []( const member_detail & member ) {
        return member_dead( member );
    } );
}

bool casualty_between( const selected_projection &prior, const selected_projection &current )
{
    if( !prior.detail || !current.detail ) {
        return false;
    }

    std::unordered_map<int, const member_detail *> current_members;
    size_t prior_live = 0;
    size_t current_live = 0;
    for( const member_detail &member : current.detail->members ) {
        current_members.emplace( member.npc_id.get_value(), &member );
        if( !member_terminal( member ) ) {
            ++current_live;
        }
    }
    for( const member_detail &member : prior.detail->members ) {
        if( member_terminal( member ) ) {
            continue;
        }
        ++prior_live;
        const auto found = current_members.find( member.npc_id.get_value() );
        if( found != current_members.end() && member_terminal( *found->second ) ) {
            return true;
        }
    }
    return current_live < prior_live;
}

struct transition_summary {
    bool moved = false;
    bool hp_changed = false;
    bool phase_changed = false;
    bool evidence_acquired = false;
    bool exposure_or_burn = false;
    bool casualty = false;
    bool return_started = false;
};

transition_summary compare( const selected_projection &prior,
                            const selected_projection &current )
{
    transition_summary result;
    const std::string prior_phase = phase_of( prior );
    const std::string current_phase = phase_of( current );
    result.moved = prior.marker.omt != current.marker.omt;
    result.hp_changed = hp_changed_between( prior, current );
    result.phase_changed = prior_phase != current_phase;

    const evidence_signature prior_evidence = evidence_of( prior );
    const evidence_signature current_evidence = evidence_of( current );
    const bool evidence_changed = !current_evidence.empty() &&
                                  !( prior_evidence == current_evidence );
    result.evidence_acquired = evidence_changed;
    result.exposure_or_burn = ( prior_phase != current_phase &&
                                is_exposure_phase( current_phase ) ) ||
                              ( evidence_changed && ( current_evidence.kind == "burn" ||
                                      current_evidence.state == "burned" ||
                                      current_evidence.state == "exposed" ) );
    result.casualty = casualty_between( prior, current ) ||
                      ( evidence_changed && current_evidence.kind == "casualty" );
    result.return_started = !is_returning_phase( prior_phase ) &&
                            is_returning_phase( current_phase );
    return result;
}

bool any_meaningful_progress( const transition_summary &summary )
{
    return summary.moved || summary.hp_changed || summary.phase_changed ||
           summary.evidence_acquired || summary.exposure_or_burn || summary.casualty ||
           summary.return_started;
}

watch_result invalid_result( const std::string &reason )
{
    watch_result result;
    result.status = watch_status::invalid;
    result.disposition = trigger_disposition::fail;
    result.reason = reason;
    return result;
}

watch_result anomaly_result( const std::string &reason )
{
    watch_result result;
    result.status = watch_status::anomaly;
    result.disposition = trigger_disposition::fail;
    result.newly_triggered = true;
    result.reason = reason;
    return result;
}

watch_result triggered_result( const watch_spec &spec, const std::string &reason,
                               bool meaningful_progress )
{
    watch_result result;
    result.status = watch_status::triggered;
    result.disposition = spec.disposition;
    result.newly_triggered = true;
    result.meaningful_progress = meaningful_progress;
    result.reason = reason;
    return result;
}

watch_result timed_out_result( const watch_spec &spec, const std::string &reason,
                               bool meaningful_progress )
{
    watch_result result;
    result.status = watch_status::timed_out;
    result.disposition = spec.disposition;
    result.newly_triggered = true;
    result.meaningful_progress = meaningful_progress;
    result.reason = reason;
    return result;
}

} // namespace

watch_result evaluate_watch( const watch_spec &spec, const watch_input &input )
{
    if( input.prior && !token_self_consistent( *input.prior ) ) {
        return anomaly_result( "prior_token_invalid" );
    }
    if( input.current && !token_self_consistent( *input.current ) ) {
        return anomaly_result( "current_token_invalid" );
    }
    if( input.prior && input.current && !same_token( input.prior->token, input.current->token ) ) {
        return anomaly_result( "entity_token_mismatch" );
    }
    if( !input.prior && !input.current ) {
        return invalid_result( "selected_entity_required" );
    }

    if( input.prior && !input.current ) {
        watch_result result;
        result.newly_triggered = !input.already_triggered;
        if( all_members_dead( *input.prior ) ) {
            result.status = watch_status::died;
            result.disposition = input.already_triggered ? trigger_disposition::continue_capture :
                                 spec.disposition;
            result.meaningful_progress = true;
            result.reason = "selected_entity_died";
            return result;
        }
        if( is_returning_phase( phase_of( *input.prior ) ) ) {
            result.status = watch_status::completed;
            result.disposition = input.already_triggered ? trigger_disposition::continue_capture :
                                 spec.disposition;
            result.meaningful_progress = true;
            result.reason = "selected_entity_completed";
            return result;
        }
        return anomaly_result( "selected_entity_missing" );
    }

    transition_summary transition;
    if( input.prior && input.current ) {
        transition = compare( *input.prior, *input.current );
    }
    const bool meaningful_progress = any_meaningful_progress( transition );

    if( input.already_triggered ) {
        watch_result result;
        result.status = watch_status::consumed;
        result.meaningful_progress = meaningful_progress;
        result.reason = "already_triggered";
        return result;
    }

    if( spec.absolute_deadline_minutes &&
        ( input.now_minutes < 0 || input.armed_minutes < 0 ||
          *spec.absolute_deadline_minutes < input.armed_minutes ) ) {
        return invalid_result( "invalid_watch_clock" );
    }

    switch( spec.preset ) {
        case watch_preset::selected_phase_change:
            if( transition.phase_changed ) {
                return triggered_result( spec, "selected_phase_changed", true );
            }
            break;
        case watch_preset::evidence_acquired:
            if( transition.evidence_acquired ) {
                return triggered_result( spec, "evidence_acquired", true );
            }
            break;
        case watch_preset::exposure_or_burn:
            if( transition.exposure_or_burn ) {
                return triggered_result( spec, "exposure_or_burn", true );
            }
            break;
        case watch_preset::casualty:
            if( transition.casualty ) {
                return triggered_result( spec, "casualty", true );
            }
            break;
        case watch_preset::return_or_completion:
            if( transition.return_started ) {
                return triggered_result( spec, "return_started", true );
            }
            break;
        case watch_preset::no_progress_by_deadline:
            if( !spec.absolute_deadline_minutes ) {
                return invalid_result( "deadline_required" );
            }
            if( input.last_progress_minutes < input.armed_minutes ||
                input.last_progress_minutes > input.now_minutes ||
                input.last_progress_minutes < 0 ) {
                return invalid_result( "invalid_watch_clock" );
            }
            if( meaningful_progress ) {
                break;
            }
            if( input.now_minutes >= *spec.absolute_deadline_minutes ) {
                return timed_out_result( spec, "no_progress_deadline_reached", false );
            }
            break;
    }


    if( spec.preset != watch_preset::no_progress_by_deadline &&
        spec.absolute_deadline_minutes && input.now_minutes >= *spec.absolute_deadline_minutes ) {
        return timed_out_result( spec, "deadline_reached", meaningful_progress );
    }

    watch_result result;
    result.meaningful_progress = meaningful_progress;
    result.reason = "watching";
    return result;
}

std::string serialize_watch_json( const watch_spec &spec, const watch_input &input,
                                  const watch_result &result )
{
    std::ostringstream output;
    JsonOut json( output, false );
    write_watch_json( json, spec, input, result );
    return output.str();
}

void write_watch_json( JsonOut &json, const watch_spec &spec, const watch_input &input,
                       const watch_result &result )
{
    json.start_object();
    json.member( "schema", "c-aol.ecology.watch" );
    json.member( "version", 1 );
    json.member( "preset", to_string( spec.preset ) );
    json.member( "requested_disposition", to_string( spec.disposition ) );
    if( spec.absolute_deadline_minutes ) {
        json.member( "deadline_minutes", *spec.absolute_deadline_minutes );
    } else {
        json.null_member( "deadline_minutes" );
    }
    json.member( "now_minutes", input.now_minutes );
    json.member( "armed_minutes", input.armed_minutes );
    json.member( "last_progress_minutes", input.last_progress_minutes );
    json.member( "status", to_string( result.status ) );
    json.member( "disposition", to_string( result.disposition ) );
    json.member( "newly_triggered", result.newly_triggered );
    json.member( "meaningful_progress", result.meaningful_progress );
    json.member( "reason", result.reason );
    json.end_object();
}

std::string to_string( watch_preset preset )
{
    switch( preset ) {
        case watch_preset::selected_phase_change:
            return "selected_phase_change";
        case watch_preset::evidence_acquired:
            return "evidence_acquired";
        case watch_preset::exposure_or_burn:
            return "exposure_or_burn";
        case watch_preset::casualty:
            return "casualty";
        case watch_preset::return_or_completion:
            return "return_or_completion";
        case watch_preset::no_progress_by_deadline:
            return "no_progress_by_deadline";
    }
    return "selected_phase_change";
}

std::optional<watch_preset> watch_preset_from_string( std::string_view value )
{
    if( value == "selected_phase_change" ) {
        return watch_preset::selected_phase_change;
    }
    if( value == "evidence_acquired" ) {
        return watch_preset::evidence_acquired;
    }
    if( value == "exposure_or_burn" ) {
        return watch_preset::exposure_or_burn;
    }
    if( value == "casualty" ) {
        return watch_preset::casualty;
    }
    if( value == "return_or_completion" ) {
        return watch_preset::return_or_completion;
    }
    if( value == "no_progress_by_deadline" ) {
        return watch_preset::no_progress_by_deadline;
    }
    return std::nullopt;
}

std::string to_string( watch_status status )
{
    switch( status ) {
        case watch_status::watching:
            return "watching";
        case watch_status::triggered:
            return "triggered";
        case watch_status::timed_out:
            return "timed_out";
        case watch_status::consumed:
            return "consumed";
        case watch_status::completed:
            return "completed";
        case watch_status::died:
            return "died";
        case watch_status::anomaly:
            return "anomaly";
        case watch_status::invalid:
            return "invalid";
    }
    return "invalid";
}

} // namespace ecology_debug
