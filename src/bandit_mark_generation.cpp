#include "bandit_mark_generation.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>

namespace bandit_mark_generation
{
namespace
{
int clamp_nonnegative( int value )
{
    return std::max( 0, value );
}

std::string join_notes( const std::vector<std::string> &notes )
{
    if( notes.empty() ) {
        return "none";
    }

    std::ostringstream out;
    for( size_t i = 0; i < notes.size(); ++i ) {
        if( i > 0 ) {
            out << "; ";
        }
        out << notes[i];
    }
    return out.str();
}

std::string effective_mark_id( const signal_input &signal )
{
    if( !signal.id.empty() ) {
        return signal.id;
    }
    if( !signal.envelope_id.empty() ) {
        return signal.envelope_id;
    }
    return signal.region_id;
}

std::string effective_envelope_id( const signal_input &signal )
{
    if( !signal.envelope_id.empty() ) {
        return signal.envelope_id;
    }
    return effective_mark_id( signal );
}

std::string effective_region_id( const signal_input &signal )
{
    if( !signal.region_id.empty() ) {
        return signal.region_id;
    }
    return effective_envelope_id( signal );
}

bool same_mark_key( const typed_mark &mark, const signal_input &signal )
{
    return mark.family == signal.family && mark.envelope_id == effective_envelope_id( signal );
}

int strength_decay_for( cadence_tier tier )
{
    switch( tier ) {
        case cadence_tier::nearby_active:
            return 0;
        case cadence_tier::distant_inactive:
            return 1;
        case cadence_tier::daily_cleanup:
            return 1;
    }

    return 0;
}

int confidence_decay_for( cadence_tier tier )
{
    switch( tier ) {
        case cadence_tier::nearby_active:
            return 1;
        case cadence_tier::distant_inactive:
            return 1;
        case cadence_tier::daily_cleanup:
            return 1;
    }

    return 0;
}

int bounty_decay_for( cadence_tier tier )
{
    switch( tier ) {
        case cadence_tier::nearby_active:
            return 0;
        case cadence_tier::distant_inactive:
            return 1;
        case cadence_tier::daily_cleanup:
            return 1;
    }

    return 0;
}

int threat_decay_for_soft_marks( cadence_tier tier )
{
    switch( tier ) {
        case cadence_tier::nearby_active:
            return 0;
        case cadence_tier::distant_inactive:
            return 1;
        case cadence_tier::daily_cleanup:
            return 1;
    }

    return 0;
}

const heat_cell *find_heat( const ledger_state &state, const std::string &region_id )
{
    const auto it = std::find_if( state.heat.begin(), state.heat.end(), [&region_id]( const heat_cell &cell ) {
        return cell.region_id == region_id;
    } );
    return it == state.heat.end() ? nullptr : &*it;
}

void rebuild_heat( ledger_state &state )
{
    std::map<std::string, heat_cell> rebuilt;
    for( const typed_mark &mark : state.marks ) {
        heat_cell &cell = rebuilt[mark.region_id];
        cell.region_id = mark.region_id;
        cell.threat_heat += mark.threat_add + mark.monster_pressure_add;
        cell.bounty_heat += mark.bounty_add + mark.target_coherence_subtract;
        cell.mark_count++;
        if( mark.confirmed_threat ) {
            cell.confirmed_threat_marks++;
        }
    }

    state.heat.clear();
    state.heat.reserve( rebuilt.size() );
    for( const auto &entry : rebuilt ) {
        state.heat.push_back( entry.second );
    }
}

void sort_ledger( ledger_state &state )
{
    std::sort( state.marks.begin(), state.marks.end(), []( const typed_mark &lhs, const typed_mark &rhs ) {
        if( lhs.region_id != rhs.region_id ) {
            return lhs.region_id < rhs.region_id;
        }
        if( lhs.envelope_id != rhs.envelope_id ) {
            return lhs.envelope_id < rhs.envelope_id;
        }
        return lhs.id < rhs.id;
    } );
    std::sort( state.heat.begin(), state.heat.end(), []( const heat_cell &lhs, const heat_cell &rhs ) {
        return lhs.region_id < rhs.region_id;
    } );
}
} // namespace

std::string to_string( cadence_tier tier )
{
    switch( tier ) {
        case cadence_tier::nearby_active:
            return "nearby_active";
        case cadence_tier::distant_inactive:
            return "distant_inactive";
        case cadence_tier::daily_cleanup:
            return "daily_cleanup";
    }

    return "unknown";
}

update_result advance_state( ledger_state &state, int tick, cadence_tier tier,
                             const std::vector<signal_input> &signals )
{
    update_result result;
    state.last_tick = tick;

    for( typed_mark &mark : state.marks ) {
        mark.age_turns = std::max( 0, tick - mark.last_refresh_tick );
    }

    for( const signal_input &signal : signals ) {
        result.metrics.events_ingested++;

        const auto it = std::find_if( state.marks.begin(), state.marks.end(), [&signal]( const typed_mark &mark ) {
            return same_mark_key( mark, signal );
        } );

        if( it == state.marks.end() ) {
            typed_mark mark;
            mark.id = effective_mark_id( signal );
            mark.kind = signal.kind;
            mark.envelope_id = effective_envelope_id( signal );
            mark.region_id = effective_region_id( signal );
            mark.family = signal.family;
            mark.strength = clamp_nonnegative( signal.strength );
            mark.confidence = clamp_nonnegative( signal.confidence );
            mark.threat_add = clamp_nonnegative( signal.threat_add );
            mark.bounty_add = clamp_nonnegative( signal.bounty_add );
            mark.monster_pressure_add = clamp_nonnegative( signal.monster_pressure_add );
            mark.target_coherence_subtract = clamp_nonnegative( signal.target_coherence_subtract );
            mark.reward_profile_match = clamp_nonnegative( signal.reward_profile_match );
            mark.distance_multiplier = std::clamp( signal.distance_multiplier, 0.0, 1.0 );
            mark.threat_gate_result = signal.threat_gate_result;
            mark.hard_blocked_jobs = signal.hard_blocked_jobs;
            mark.confirmed_threat = signal.confirmed_threat;
            mark.soft_decay = signal.soft_decay;
            mark.last_refresh_tick = tick;
            mark.age_turns = 0;
            mark.notes = signal.notes;
            if( mark.notes.empty() ) {
                mark.notes.push_back( "created from " + mark.kind + " signal" );
            }
            state.marks.push_back( mark );
            result.metrics.marks_created++;
            continue;
        }

        typed_mark &mark = *it;
        mark.id = effective_mark_id( signal );
        if( !signal.kind.empty() ) {
            mark.kind = signal.kind;
        }
        mark.region_id = effective_region_id( signal );
        mark.strength = std::min( 6, std::max( mark.strength, clamp_nonnegative( signal.strength ) ) + 1 );
        mark.confidence = std::min( 6, std::max( mark.confidence, clamp_nonnegative( signal.confidence ) ) + 1 );
        mark.threat_add = std::max( mark.threat_add, clamp_nonnegative( signal.threat_add ) );
        mark.bounty_add = std::max( mark.bounty_add, clamp_nonnegative( signal.bounty_add ) );
        mark.monster_pressure_add = std::max( mark.monster_pressure_add,
                                              clamp_nonnegative( signal.monster_pressure_add ) );
        mark.target_coherence_subtract = std::max( mark.target_coherence_subtract,
                                                   clamp_nonnegative( signal.target_coherence_subtract ) );
        mark.reward_profile_match = std::max( mark.reward_profile_match,
                                              clamp_nonnegative( signal.reward_profile_match ) );
        mark.distance_multiplier = std::max( mark.distance_multiplier,
                                             std::clamp( signal.distance_multiplier, 0.0, 1.0 ) );
        mark.threat_gate_result = signal.threat_gate_result;
        mark.hard_blocked_jobs = signal.hard_blocked_jobs;
        mark.confirmed_threat = mark.confirmed_threat || signal.confirmed_threat;
        mark.soft_decay = mark.soft_decay && signal.soft_decay;
        mark.last_refresh_tick = tick;
        mark.age_turns = 0;
        if( !signal.notes.empty() ) {
            mark.notes = signal.notes;
        }
        result.metrics.marks_refreshed++;
    }

    for( typed_mark &mark : state.marks ) {
        if( mark.last_refresh_tick == tick ) {
            continue;
        }

        const int old_strength = mark.strength;
        const int old_confidence = mark.confidence;
        const int old_threat = mark.threat_add;
        const int old_bounty = mark.bounty_add;
        const int old_target_coherence = mark.target_coherence_subtract;

        if( mark.confirmed_threat ) {
            result.metrics.sticky_threat_preserved++;
            mark.bounty_add = clamp_nonnegative( mark.bounty_add - bounty_decay_for( tier ) );
            mark.target_coherence_subtract = clamp_nonnegative( mark.target_coherence_subtract - bounty_decay_for( tier ) );
            if( mark.soft_decay ) {
                mark.confidence = std::max( 1, mark.confidence - confidence_decay_for( tier ) );
                mark.strength = clamp_nonnegative( mark.strength - strength_decay_for( tier ) );
            }
        } else if( mark.soft_decay ) {
            mark.strength = clamp_nonnegative( mark.strength - strength_decay_for( tier ) );
            mark.confidence = clamp_nonnegative( mark.confidence - confidence_decay_for( tier ) );
            mark.bounty_add = clamp_nonnegative( mark.bounty_add - bounty_decay_for( tier ) );
            mark.target_coherence_subtract = clamp_nonnegative( mark.target_coherence_subtract - bounty_decay_for( tier ) );
            mark.threat_add = clamp_nonnegative( mark.threat_add - threat_decay_for_soft_marks( tier ) );
            mark.monster_pressure_add = clamp_nonnegative( mark.monster_pressure_add - threat_decay_for_soft_marks( tier ) );
        }

        if( mark.strength != old_strength || mark.confidence != old_confidence ||
            mark.threat_add != old_threat || mark.bounty_add != old_bounty ||
            mark.target_coherence_subtract != old_target_coherence ) {
            result.metrics.marks_cooled++;
        }
        mark.age_turns = std::max( 0, tick - mark.last_refresh_tick );
    }

    const size_t before_prune = state.marks.size();
    state.marks.erase( std::remove_if( state.marks.begin(), state.marks.end(), []( const typed_mark &mark ) {
        if( mark.confirmed_threat ) {
            return false;
        }
        return mark.strength == 0 && mark.confidence == 0 && mark.threat_add == 0 &&
               mark.bounty_add == 0 && mark.monster_pressure_add == 0 &&
               mark.target_coherence_subtract == 0;
    } ), state.marks.end() );
    result.metrics.marks_pruned = before_prune - state.marks.size();

    rebuild_heat( state );
    sort_ledger( state );
    result.leads = emit_leads( state, &result.metrics );
    return result;
}

std::vector<bandit_dry_run::lead_input> emit_leads( const ledger_state &state,
        ledger_metrics *metrics )
{
    std::vector<bandit_dry_run::lead_input> leads;
    leads.reserve( state.marks.size() );

    for( const typed_mark &mark : state.marks ) {
        if( mark.family == bandit_dry_run::lead_family::none ) {
            continue;
        }
        if( mark.strength == 0 && mark.confidence == 0 && mark.threat_add == 0 &&
            mark.bounty_add == 0 && mark.target_coherence_subtract == 0 ) {
            continue;
        }

        bandit_dry_run::lead_input lead;
        lead.id = mark.id;
        lead.envelope_id = mark.envelope_id;
        lead.family = mark.family;
        lead.distance_multiplier = mark.distance_multiplier;
        lead.lead_bounty_value = mark.strength + mark.bounty_add + mark.target_coherence_subtract;
        lead.lead_confidence_bonus = mark.confidence;
        lead.threat_penalty = mark.threat_add + mark.monster_pressure_add;
        lead.reward_profile_match = mark.reward_profile_match;
        lead.threat_gate_result = mark.threat_gate_result;
        lead.hard_blocked_jobs = mark.hard_blocked_jobs;
        lead.validity_notes = mark.notes;
        lead.validity_notes.push_back( "generated from " + mark.kind + " mark in " + mark.region_id );
        if( mark.confirmed_threat ) {
            lead.validity_notes.push_back( "confirmed threat stays sticky until a later real rewrite" );
        }
        if( const heat_cell *heat = find_heat( state, mark.region_id ) ) {
            lead.active_pressure_penalty = std::max( 0, heat->threat_heat - lead.threat_penalty );
            if( heat->bounty_heat >= lead.lead_bounty_value + 2 ) {
                lead.lead_confidence_bonus += 1;
            }
            lead.validity_notes.push_back( "regional heat threat=" + std::to_string( heat->threat_heat ) +
                                           ", bounty=" + std::to_string( heat->bounty_heat ) );
        }
        leads.push_back( lead );
    }

    std::sort( leads.begin(), leads.end(), []( const bandit_dry_run::lead_input &lhs,
    const bandit_dry_run::lead_input &rhs ) {
        if( lhs.envelope_id != rhs.envelope_id ) {
            return lhs.envelope_id < rhs.envelope_id;
        }
        return lhs.id < rhs.id;
    } );

    if( metrics != nullptr ) {
        metrics->leads_emitted = leads.size();
    }
    return leads;
}

std::string render_report( const ledger_state &state,
                           const std::vector<bandit_dry_run::lead_input> *leads )
{
    const std::vector<bandit_dry_run::lead_input> local_leads = leads == nullptr ? emit_leads( state ) :
            *leads;

    std::ostringstream out;
    out << "bandit mark-generation report\n";
    out << "last_tick=" << state.last_tick << "\n";
    out << "heat cells:\n";
    if( state.heat.empty() ) {
        out << "- none\n";
    } else {
        for( const heat_cell &cell : state.heat ) {
            out << "- " << cell.region_id << " threat_heat=" << cell.threat_heat
                << ", bounty_heat=" << cell.bounty_heat
                << ", mark_count=" << cell.mark_count
                << ", confirmed_threat_marks=" << cell.confirmed_threat_marks << "\n";
        }
    }

    out << "typed marks:\n";
    if( state.marks.empty() ) {
        out << "- none\n";
    } else {
        for( const typed_mark &mark : state.marks ) {
            out << "- " << mark.envelope_id << " [kind=" << mark.kind
                << ", family=" << bandit_dry_run::to_string( mark.family )
                << ", region=" << mark.region_id
                << ", strength=" << mark.strength
                << ", confidence=" << mark.confidence
                << ", threat_add=" << mark.threat_add
                << ", bounty_add=" << mark.bounty_add
                << ", monster_pressure_add=" << mark.monster_pressure_add
                << ", target_coherence_subtract=" << mark.target_coherence_subtract
                << ", confirmed_threat=" << ( mark.confirmed_threat ? "yes" : "no" )
                << ", soft_decay=" << ( mark.soft_decay ? "yes" : "no" )
                << ", age_turns=" << mark.age_turns
                << ", last_refresh_tick=" << mark.last_refresh_tick << "]\n";
            out << "  notes=" << join_notes( mark.notes ) << "\n";
        }
    }

    out << "generated leads:\n";
    if( local_leads.empty() ) {
        out << "- none\n";
    } else {
        for( const bandit_dry_run::lead_input &lead : local_leads ) {
            out << "- " << lead.envelope_id
                << " [family=" << bandit_dry_run::to_string( lead.family )
                << ", bounty=" << lead.lead_bounty_value
                << ", confidence=" << lead.lead_confidence_bonus
                << ", threat=" << lead.threat_penalty
                << ", active_pressure_penalty=" << lead.active_pressure_penalty
                << ", distance_multiplier=" << std::fixed << std::setprecision( 2 )
                << lead.distance_multiplier
                << ", threat_gate=" << bandit_dry_run::to_string( lead.threat_gate_result ) << "]\n";
            out << "  notes=" << join_notes( lead.validity_notes ) << "\n";
        }
    }

    return out.str();
}
} // namespace bandit_mark_generation
