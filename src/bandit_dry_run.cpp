#include "bandit_dry_run.h"

#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>

namespace bandit_dry_run
{
namespace
{
int lookup_or_zero( const std::map<job_template, int> &values, job_template job )
{
    const auto it = values.find( job );
    return it == values.end() ? 0 : it->second;
}

bool contains_job( const std::vector<job_template> &jobs, job_template needle )
{
    return std::find( jobs.begin(), jobs.end(), needle ) != jobs.end();
}

int required_min_manpower( job_template job )
{
    switch( job ) {
        case job_template::hold_chill:
            return 0;
        case job_template::scout:
        case job_template::scavenge:
        case job_template::stalk:
        case job_template::steal:
            return 1;
        case job_template::toll:
        case job_template::raid:
        case job_template::reinforce:
            return 2;
    }

    return 99;
}

std::vector<job_template> compatible_jobs_for( lead_family family )
{
    switch( family ) {
        case lead_family::none:
            return {};
        case lead_family::site:
            return { job_template::scout, job_template::scavenge, job_template::steal, job_template::raid };
        case lead_family::corridor:
            return { job_template::scout, job_template::toll, job_template::stalk };
        case lead_family::moving_carrier:
            return { job_template::stalk, job_template::toll, job_template::steal };
        case lead_family::friendly_pressure:
            return { job_template::reinforce, job_template::scout };
    }

    return {};
}

int job_lead_fit_bonus( job_template job, lead_family family )
{
    switch( family ) {
        case lead_family::site:
            switch( job ) {
                case job_template::scout:
                    return 1;
                case job_template::scavenge:
                    return 2;
                case job_template::steal:
                    return 1;
                case job_template::raid:
                    return 0;
                default:
                    return -2;
            }
        case lead_family::corridor:
            switch( job ) {
                case job_template::scout:
                    return 0;
                case job_template::toll:
                    return 3;
                case job_template::stalk:
                    return 2;
                default:
                    return -2;
            }
        case lead_family::moving_carrier:
            switch( job ) {
                case job_template::stalk:
                    return 3;
                case job_template::toll:
                    return 1;
                case job_template::steal:
                    return 2;
                default:
                    return -2;
            }
        case lead_family::friendly_pressure:
            switch( job ) {
                case job_template::reinforce:
                    return 3;
                case job_template::scout:
                    return 1;
                default:
                    return -2;
            }
        case lead_family::none:
            return -2;
    }

    return -2;
}

int ordinary_need_alignment( job_template job, int reward_profile_match )
{
    if( reward_profile_match <= 0 ) {
        return 0;
    }

    switch( job ) {
        case job_template::scavenge:
        case job_template::toll:
        case job_template::steal:
        case job_template::raid:
            return 1;
        default:
            return 0;
    }
}

int shortage_band_bonus( shortage_band shortage )
{
    switch( shortage ) {
        case shortage_band::stable:
            return 0;
        case shortage_band::low:
            return 1;
        case shortage_band::critical:
            return 2;
    }

    return 0;
}

bool is_threat_compatible( job_template job )
{
    switch( job ) {
        case job_template::scout:
        case job_template::toll:
        case job_template::stalk:
        case job_template::raid:
        case job_template::reinforce:
            return true;
        case job_template::hold_chill:
        case job_template::scavenge:
        case job_template::steal:
            return false;
    }

    return false;
}

std::string describe_valid_lead( const lead_input &lead )
{
    return "valid " + to_string( lead.family ) + " lead envelope";
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
} // namespace

std::string to_string( lead_family family )
{
    switch( family ) {
        case lead_family::none:
            return "none";
        case lead_family::site:
            return "site";
        case lead_family::corridor:
            return "corridor";
        case lead_family::moving_carrier:
            return "moving_carrier";
        case lead_family::friendly_pressure:
            return "friendly_pressure";
    }

    return "unknown";
}

std::string to_string( job_template job )
{
    switch( job ) {
        case job_template::hold_chill:
            return "hold / chill";
        case job_template::scout:
            return "scout";
        case job_template::scavenge:
            return "scavenge";
        case job_template::toll:
            return "toll";
        case job_template::stalk:
            return "stalk";
        case job_template::steal:
            return "steal";
        case job_template::raid:
            return "raid";
        case job_template::reinforce:
            return "reinforce";
    }

    return "unknown";
}

std::string to_string( shortage_band shortage )
{
    switch( shortage ) {
        case shortage_band::stable:
            return "stable";
        case shortage_band::low:
            return "low";
        case shortage_band::critical:
            return "critical";
    }

    return "unknown";
}

std::string to_string( threat_gate gate )
{
    switch( gate ) {
        case threat_gate::discount_only:
            return "discount_only";
        case threat_gate::soft_veto:
            return "soft_veto";
        case threat_gate::hard_veto:
            return "hard_veto";
    }

    return "unknown";
}

evaluation_result evaluate( const camp_input &camp, const std::vector<lead_input> &leads )
{
    evaluation_result result;
    result.metrics.input_lead_count = leads.size();

    candidate_debug hold;
    hold.generated = true;
    hold.winner = false;
    hold.score.threat_gate_result = threat_gate::discount_only;
    hold.notes.push_back( "always-available zero baseline" );
    result.candidates.push_back( hold );

    std::set<std::string> emitted_envelopes;

    for( const lead_input &lead : leads ) {
        lead_debug lead_view;
        lead_view.id = lead.id;
        lead_view.envelope_id = lead.envelope_id.empty() ? lead.id : lead.envelope_id;
        lead_view.family = lead.family;
        lead_view.has_real_envelope = lead.has_real_envelope;
        lead_view.still_valid = lead.still_valid;
        lead_view.notes = lead.validity_notes;

        if( !lead_view.has_real_envelope ) {
            result.metrics.rejected_no_envelope_count++;
            lead_view.still_valid = false;
            lead_view.notes.push_back( "no real outward lead envelope" );
            result.leads.push_back( lead_view );
            continue;
        }

        if( !lead_view.still_valid ) {
            result.metrics.rejected_invalid_lead_count++;
            if( lead_view.notes.empty() ) {
                lead_view.notes.push_back( "lead is no longer valid for generation" );
            }
            result.leads.push_back( lead_view );
            continue;
        }

        if( emitted_envelopes.count( lead_view.envelope_id ) > 0 ) {
            result.metrics.deduped_lead_count++;
            lead_view.deduped = true;
            if( lead_view.notes.empty() ) {
                lead_view.notes.push_back( "deduped into envelope " + lead_view.envelope_id );
            }
            result.leads.push_back( lead_view );
            continue;
        }

        result.metrics.accepted_lead_count++;
        emitted_envelopes.insert( lead_view.envelope_id );
        if( lead_view.notes.empty() ) {
            lead_view.notes.push_back( describe_valid_lead( lead ) );
        }
        result.leads.push_back( lead_view );
        lead_debug &stored_lead = result.leads.back();

        for( job_template job : compatible_jobs_for( lead.family ) ) {
            result.metrics.compatible_job_checks++;
            if( contains_job( lead.hard_blocked_jobs, job ) ) {
                result.metrics.hard_blocked_job_count++;
                stored_lead.notes.push_back( "hard preconditions block " + to_string( job ) );
                continue;
            }
            if( camp.available_manpower < required_min_manpower( job ) ) {
                result.metrics.manpower_rejection_count++;
                stored_lead.notes.push_back( "insufficient manpower for " + to_string( job ) );
                continue;
            }

            result.metrics.candidates_generated++;
            result.metrics.score_evaluations++;
            result.metrics.path_checks++;

            candidate_debug candidate;
            candidate.job = job;
            candidate.lead_id = lead.id;
            candidate.envelope_id = lead_view.envelope_id;
            candidate.family = lead.family;
            candidate.generated = true;
            candidate.score.lead_bounty_value = lead.lead_bounty_value;
            candidate.score.lead_confidence_bonus = lead.lead_confidence_bonus;
            candidate.score.job_lead_fit_bonus = job_lead_fit_bonus( job, lead.family );
            candidate.score.ordinary_need_alignment = ordinary_need_alignment( job, lead.reward_profile_match );
            candidate.score.temperament_bias = lookup_or_zero( camp.temperament_bias, job );
            candidate.score.job_type_bonus = lookup_or_zero( camp.job_type_bonus, job );
            candidate.score.positive_pull = candidate.score.lead_bounty_value +
                                            candidate.score.lead_confidence_bonus +
                                            candidate.score.job_lead_fit_bonus +
                                            candidate.score.ordinary_need_alignment +
                                            candidate.score.temperament_bias +
                                            candidate.score.job_type_bonus;
            candidate.score.distance_multiplier = std::clamp( lead.distance_multiplier, 0.0, 1.0 );
            candidate.score.travel_shaped_pull = candidate.score.positive_pull * candidate.score.distance_multiplier;
            candidate.score.threat_penalty = lead.threat_penalty;
            candidate.score.active_pressure_penalty = lead.active_pressure_penalty;
            candidate.score.pre_veto_job_score = candidate.score.travel_shaped_pull -
                                                 candidate.score.threat_penalty -
                                                 candidate.score.active_pressure_penalty;
            candidate.score.shortage_band_bonus = shortage_band_bonus( camp.shortage );
            candidate.score.reward_profile_match = std::clamp( lead.reward_profile_match, 0, 2 );

            if( candidate.score.shortage_band_bonus > 0 &&
                candidate.score.pre_veto_job_score >= -2.0 &&
                candidate.score.pre_veto_job_score <= 0.0 ) {
                candidate.score.need_override_bonus = candidate.score.shortage_band_bonus *
                                                     candidate.score.reward_profile_match;
            }

            candidate.score.need_adjusted_job_score = candidate.score.pre_veto_job_score +
                    candidate.score.need_override_bonus;
            candidate.score.threat_gate_result = lead.threat_gate_result;
            candidate.score.final_job_score = candidate.score.need_adjusted_job_score;
            candidate.notes.push_back( "generated from " + to_string( lead.family ) + " envelope" );

            if( candidate.score.need_override_bonus > 0 ) {
                result.metrics.need_override_rescues++;
                candidate.notes.push_back( "need-pressure override rescued a mediocre real lead" );
            }

            switch( lead.threat_gate_result ) {
                case threat_gate::discount_only:
                    candidate.notes.push_back( "discount_only: ordinary threat penalty already applied" );
                    break;
                case threat_gate::soft_veto:
                    if( is_threat_compatible( job ) ) {
                        result.metrics.soft_veto_caps++;
                        candidate.score.final_job_score = std::min( candidate.score.need_adjusted_job_score, 1.0 );
                        candidate.notes.push_back( "soft_veto: threat-compatible job capped to a marginal choice" );
                    } else {
                        result.metrics.soft_veto_collapses++;
                        candidate.score.final_job_score = 0.0;
                        candidate.notes.push_back( "soft_veto: pure extraction collapses back to hold / chill" );
                    }
                    break;
                case threat_gate::hard_veto:
                    result.metrics.hard_veto_invalidations++;
                    candidate.valid = false;
                    candidate.score.final_job_score = 0.0;
                    candidate.notes.push_back( "hard_veto: severe danger invalidates this dispatch" );
                    break;
            }

            if( !lead.has_path ) {
                result.metrics.no_path_invalidations++;
                candidate.valid = false;
                candidate.score.final_job_score = 0.0;
                candidate.notes.push_back( "no_path: unreachable this dispatch pass" );
            }

            if( candidate.valid ) {
                result.metrics.valid_outward_candidates++;
            } else {
                result.metrics.invalid_outward_candidates++;
            }

            result.candidates.push_back( candidate );
        }
    }

    size_t best_index = 0;
    double best_score = 0.0;
    for( size_t i = 1; i < result.candidates.size(); ++i ) {
        result.metrics.winner_comparisons++;
        const candidate_debug &candidate = result.candidates[i];
        if( candidate.valid && candidate.score.final_job_score > best_score ) {
            best_index = i;
            best_score = candidate.score.final_job_score;
        }
    }

    result.winner_index = best_index;
    result.candidates[best_index].winner = true;
    if( best_index == 0 ) {
        result.winner_reason = "hold / chill wins because no valid outward candidate beat the score-0 baseline";
    } else {
        const candidate_debug &winner = result.candidates[best_index];
        std::ostringstream reason;
        reason << to_string( winner.job ) << " on " << winner.envelope_id << " wins with final_job_score="
               << std::fixed << std::setprecision( 2 ) << winner.score.final_job_score
               << " over hold / chill=0.00";
        result.winner_reason = reason.str();
    }

    return result;
}

std::string render_report( const evaluation_result &result )
{
    std::ostringstream out;
    out << "bandit dry-run evaluator report\n";
    out << "Leads considered:\n";
    if( result.leads.empty() ) {
        out << "- none\n";
    } else {
        for( const lead_debug &lead : result.leads ) {
            out << "- " << lead.envelope_id << " [family=" << to_string( lead.family )
                << ", real=" << ( lead.has_real_envelope ? "yes" : "no" )
                << ", valid=" << ( lead.still_valid ? "yes" : "no" )
                << ", deduped=" << ( lead.deduped ? "yes" : "no" ) << "] notes="
                << join_notes( lead.notes ) << "\n";
        }
    }

    out << "Candidate board:\n";
    for( const candidate_debug &candidate : result.candidates ) {
        out << "- " << to_string( candidate.job );
        if( !candidate.envelope_id.empty() ) {
            out << " @ " << candidate.envelope_id;
        }
        out << " [valid=" << ( candidate.valid ? "yes" : "no" )
            << ", winner=" << ( candidate.winner ? "yes" : "no" ) << "]\n";
        out << "  lead_bounty_value=" << candidate.score.lead_bounty_value
            << ", lead_confidence_bonus=" << candidate.score.lead_confidence_bonus
            << ", job_lead_fit_bonus=" << candidate.score.job_lead_fit_bonus
            << ", ordinary_need_alignment=" << candidate.score.ordinary_need_alignment
            << ", temperament_bias=" << candidate.score.temperament_bias
            << ", job_type_bonus=" << candidate.score.job_type_bonus << "\n";
        out << "  positive_pull=" << std::fixed << std::setprecision( 2 ) << candidate.score.positive_pull
            << ", distance_multiplier=" << candidate.score.distance_multiplier
            << ", travel_shaped_pull=" << candidate.score.travel_shaped_pull << "\n";
        out << "  threat_penalty=" << candidate.score.threat_penalty
            << ", active_pressure_penalty=" << candidate.score.active_pressure_penalty
            << ", pre_veto_job_score=" << candidate.score.pre_veto_job_score << "\n";
        out << "  shortage_band_bonus=" << candidate.score.shortage_band_bonus
            << ", reward_profile_match=" << candidate.score.reward_profile_match
            << ", need_override_bonus=" << candidate.score.need_override_bonus
            << ", need_adjusted_job_score=" << candidate.score.need_adjusted_job_score << "\n";
        out << "  threat_gate=" << to_string( candidate.score.threat_gate_result )
            << ", final_job_score=" << candidate.score.final_job_score
            << ", notes=" << join_notes( candidate.notes ) << "\n";
    }

    out << "Measurement summary:\n";
    out << "- input_leads=" << result.metrics.input_lead_count
        << ", accepted_leads=" << result.metrics.accepted_lead_count
        << ", rejected_no_envelope=" << result.metrics.rejected_no_envelope_count
        << ", rejected_invalid=" << result.metrics.rejected_invalid_lead_count
        << ", deduped_leads=" << result.metrics.deduped_lead_count << "\n";
    out << "- compatible_job_checks=" << result.metrics.compatible_job_checks
        << ", hard_blocked_jobs=" << result.metrics.hard_blocked_job_count
        << ", manpower_rejections=" << result.metrics.manpower_rejection_count
        << ", candidates_generated=" << result.metrics.candidates_generated << "\n";
    out << "- score_evaluations=" << result.metrics.score_evaluations
        << ", path_checks=" << result.metrics.path_checks
        << ", valid_outward_candidates=" << result.metrics.valid_outward_candidates
        << ", invalid_outward_candidates=" << result.metrics.invalid_outward_candidates << "\n";
    out << "- need_override_rescues=" << result.metrics.need_override_rescues
        << ", soft_veto_caps=" << result.metrics.soft_veto_caps
        << ", soft_veto_collapses=" << result.metrics.soft_veto_collapses
        << ", hard_veto_invalidations=" << result.metrics.hard_veto_invalidations
        << ", no_path_invalidations=" << result.metrics.no_path_invalidations
        << ", winner_comparisons=" << result.metrics.winner_comparisons << "\n";

    out << "Winner:\n- " << result.winner_reason << "\n";
    out << "Return packet fields touched:\n";
    if( result.return_packet_fields_touched.empty() ) {
        out << "- none in v0\n";
    } else {
        for( const std::string &field : result.return_packet_fields_touched ) {
            out << "- " << field << "\n";
        }
    }

    return out.str();
}
} // namespace bandit_dry_run
