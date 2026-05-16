#include "bandit_dry_run.h"

#include "cata_catch.h"

namespace
{
const bandit_dry_run::candidate_debug *find_candidate( const bandit_dry_run::evaluation_result &result,
        bandit_dry_run::job_template job,
        const std::string &envelope_id = std::string() )
{
    for( const bandit_dry_run::candidate_debug &candidate : result.candidates ) {
        if( candidate.job == job && ( envelope_id.empty() || candidate.envelope_id == envelope_id ) ) {
            return &candidate;
        }
    }
    return nullptr;
}
}

TEST_CASE( "bandit_dry_run_falls_back_to_hold_chill_without_real_leads", "[bandit][dry_run]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;

    const bandit_dry_run::evaluation_result result = bandit_dry_run::evaluate( camp, {} );

    REQUIRE( result.candidates.size() == 1 );
    CHECK( result.winner_index == 0 );
    CHECK( result.candidates[0].job == bandit_dry_run::job_template::hold_chill );
    CHECK( result.candidates[0].winner );
    CHECK( result.winner_reason.find( "hold / chill wins" ) != std::string::npos );
    CHECK( result.metrics.input_lead_count == 0 );
    CHECK( result.metrics.candidates_generated == 0 );
    CHECK( result.metrics.path_checks == 0 );

    const std::string report = bandit_dry_run::render_report( result );
    CHECK( report.find( "Leads considered:" ) != std::string::npos );
    CHECK( report.find( "- none" ) != std::string::npos );
    CHECK( report.find( "Candidate board:" ) != std::string::npos );
    CHECK( report.find( "hold / chill" ) != std::string::npos );
    CHECK( report.find( "Return packet fields touched:" ) != std::string::npos );
}

TEST_CASE( "bandit_dry_run_strong_local_site_lead_can_beat_hold_chill", "[bandit][dry_run]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;

    bandit_dry_run::lead_input lead;
    lead.id = "quiet_house";
    lead.envelope_id = "quiet_house";
    lead.family = bandit_dry_run::lead_family::site;
    lead.lead_bounty_value = 5;
    lead.lead_confidence_bonus = 1;
    lead.threat_penalty = 2;
    lead.distance_multiplier = 0.85;
    lead.reward_profile_match = 2;

    const bandit_dry_run::evaluation_result result = bandit_dry_run::evaluate( camp, { lead } );
    const bandit_dry_run::candidate_debug *scavenge = find_candidate( result,
            bandit_dry_run::job_template::scavenge, "quiet_house" );

    REQUIRE( scavenge != nullptr );
    CHECK( scavenge->valid );
    CHECK( scavenge->score.monster_pressure_bonus == 0 );
    CHECK( scavenge->score.target_coherence_bonus == 0 );
    CHECK( scavenge->score.effective_threat_penalty == 2 );
    CHECK( scavenge->score.opportunism_bonus == 0 );
    CHECK( scavenge->score.pre_veto_job_score > 0.0 );
    CHECK( result.candidates[result.winner_index].job == bandit_dry_run::job_template::scavenge );
    CHECK( result.winner_reason.find( "quiet_house" ) != std::string::npos );

    const std::string report = bandit_dry_run::render_report( result );
    CHECK( report.find( "pre_veto_job_score" ) != std::string::npos );
    CHECK( report.find( "final_job_score" ) != std::string::npos );
    CHECK( report.find( "Winner:" ) != std::string::npos );
}

TEST_CASE( "bandit_dry_run_unreachable_lead_fails_closed_without_fake_wander", "[bandit][dry_run]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;

    bandit_dry_run::lead_input lead;
    lead.id = "far_farm";
    lead.envelope_id = "far_farm";
    lead.family = bandit_dry_run::lead_family::site;
    lead.lead_bounty_value = 6;
    lead.lead_confidence_bonus = 2;
    lead.threat_penalty = 1;
    lead.distance_multiplier = 0.90;
    lead.reward_profile_match = 2;
    lead.has_path = false;

    const bandit_dry_run::evaluation_result result = bandit_dry_run::evaluate( camp, { lead } );
    const bandit_dry_run::candidate_debug *scavenge = find_candidate( result,
            bandit_dry_run::job_template::scavenge, "far_farm" );

    REQUIRE( scavenge != nullptr );
    CHECK_FALSE( scavenge->valid );
    CHECK( scavenge->score.pre_veto_job_score > 0.0 );
    CHECK( scavenge->score.final_job_score == Approx( 0.0 ) );
    CHECK( result.candidates[result.winner_index].job == bandit_dry_run::job_template::hold_chill );
    CHECK( result.metrics.candidates_generated == 4 );
    CHECK( result.metrics.no_path_invalidations == 4 );
    CHECK( result.metrics.invalid_outward_candidates == 4 );

    const std::string report = bandit_dry_run::render_report( result );
    CHECK( report.find( "no_path: unreachable this dispatch pass" ) != std::string::npos );
}

TEST_CASE( "bandit_dry_run_need_pressure_can_rescue_a_mediocre_real_lead", "[bandit][dry_run]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;
    camp.shortage = bandit_dry_run::shortage_band::low;

    bandit_dry_run::lead_input lead;
    lead.id = "lean_food_house";
    lead.envelope_id = "lean_food_house";
    lead.family = bandit_dry_run::lead_family::site;
    lead.distance_multiplier = 0.30;
    lead.threat_penalty = 1;
    lead.reward_profile_match = 2;

    const bandit_dry_run::evaluation_result result = bandit_dry_run::evaluate( camp, { lead } );
    const bandit_dry_run::candidate_debug *scavenge = find_candidate( result,
            bandit_dry_run::job_template::scavenge, "lean_food_house" );

    REQUIRE( scavenge != nullptr );
    CHECK( scavenge->score.pre_veto_job_score <= 0.0 );
    CHECK( scavenge->score.pre_veto_job_score >= -2.0 );
    CHECK( scavenge->score.need_override_bonus == 2 );
    CHECK( scavenge->score.final_job_score > 0.0 );
    CHECK( result.candidates[result.winner_index].job == bandit_dry_run::job_template::scavenge );
    CHECK( result.metrics.need_override_rescues == 4 );

    const std::string report = bandit_dry_run::render_report( result );
    CHECK( report.find( "need_override_bonus=2" ) != std::string::npos );
}

TEST_CASE( "bandit_dry_run_explicit_bounded_explore_can_beat_hold_chill_without_real_leads", "[bandit][dry_run]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;
    camp.allow_bounded_explore = true;
    camp.explore_pressure = 2;
    camp.explore_distance_multiplier = 0.40;

    const bandit_dry_run::evaluation_result result = bandit_dry_run::evaluate( camp, {} );
    const bandit_dry_run::candidate_debug *scout = find_candidate( result,
            bandit_dry_run::job_template::scout, "bounded_explore" );

    REQUIRE( scout != nullptr );
    CHECK( scout->valid );
    CHECK( scout->score.lead_bounty_value == 2 );
    CHECK( scout->score.distance_multiplier == Approx( 0.40 ) );
    CHECK( scout->score.pre_veto_job_score > 0.0 );
    CHECK( result.candidates[result.winner_index].job == bandit_dry_run::job_template::scout );
    CHECK( result.candidates[result.winner_index].envelope_id == "bounded_explore" );

    const std::string report = bandit_dry_run::render_report( result );
    CHECK( report.find( "explicit bounded scout/explore outing for map uncovering" ) != std::string::npos );
    CHECK( report.find( "not a no_path fallback and not accidental random wandering" ) != std::string::npos );
}

TEST_CASE( "bandit_dry_run_unreachable_targets_do_not_mint_explore_without_explicit_greenlight", "[bandit][dry_run]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;

    bandit_dry_run::lead_input lead;
    lead.id = "blocked_bridge_farm";
    lead.envelope_id = "blocked_bridge_farm";
    lead.family = bandit_dry_run::lead_family::site;
    lead.lead_bounty_value = 5;
    lead.lead_confidence_bonus = 1;
    lead.distance_multiplier = 0.80;
    lead.has_path = false;

    const bandit_dry_run::evaluation_result result = bandit_dry_run::evaluate( camp, { lead } );

    CHECK( find_candidate( result, bandit_dry_run::job_template::scout, "bounded_explore" ) == nullptr );
    CHECK( result.candidates[result.winner_index].job == bandit_dry_run::job_template::hold_chill );
}

TEST_CASE( "bandit_dry_run_explicit_bounded_explore_stays_secondary_to_strong_real_leads", "[bandit][dry_run]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;
    camp.allow_bounded_explore = true;
    camp.explore_pressure = 1;
    camp.explore_distance_multiplier = 0.30;

    bandit_dry_run::lead_input lead;
    lead.id = "quiet_house";
    lead.envelope_id = "quiet_house";
    lead.family = bandit_dry_run::lead_family::site;
    lead.lead_bounty_value = 5;
    lead.lead_confidence_bonus = 1;
    lead.threat_penalty = 1;
    lead.distance_multiplier = 0.85;
    lead.reward_profile_match = 2;

    const bandit_dry_run::evaluation_result result = bandit_dry_run::evaluate( camp, { lead } );
    const bandit_dry_run::candidate_debug *scout = find_candidate( result,
            bandit_dry_run::job_template::scout, "bounded_explore" );
    const bandit_dry_run::candidate_debug *scavenge = find_candidate( result,
            bandit_dry_run::job_template::scavenge, "quiet_house" );

    REQUIRE( scout != nullptr );
    REQUIRE( scavenge != nullptr );
    CHECK( scout->score.final_job_score < scavenge->score.final_job_score );
    CHECK( result.candidates[result.winner_index].job == bandit_dry_run::job_template::scavenge );
}

TEST_CASE( "bandit_dry_run_strong_targets_stay_deferred_without_a_real_opening", "[bandit][dry_run]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;

    bandit_dry_run::lead_input lead;
    lead.id = "guarded_convoy";
    lead.envelope_id = "guarded_convoy";
    lead.family = bandit_dry_run::lead_family::moving_carrier;
    lead.lead_bounty_value = 4;
    lead.lead_confidence_bonus = 1;
    lead.threat_penalty = 6;
    lead.distance_multiplier = 0.60;

    const bandit_dry_run::evaluation_result result = bandit_dry_run::evaluate( camp, { lead } );
    const bandit_dry_run::candidate_debug *stalk = find_candidate( result,
            bandit_dry_run::job_template::stalk, "guarded_convoy" );

    REQUIRE( stalk != nullptr );
    CHECK( stalk->valid );
    CHECK( stalk->score.threat_penalty == 6 );
    CHECK( stalk->score.effective_threat_penalty == 6 );
    CHECK( stalk->score.opportunism_bonus == 0 );
    CHECK( stalk->score.pre_veto_job_score < 0.0 );
    CHECK( result.candidates[result.winner_index].job == bandit_dry_run::job_template::hold_chill );

    const std::string report = bandit_dry_run::render_report( result );
    CHECK( report.find( "effective_threat_penalty=6" ) != std::string::npos );
    CHECK( report.find( "opportunism_bonus=0" ) != std::string::npos );
}

TEST_CASE( "bandit_dry_run_target_distraction_opens_a_bounded_opportunistic_window", "[bandit][dry_run]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;

    bandit_dry_run::lead_input lead;
    lead.id = "guarded_convoy";
    lead.envelope_id = "guarded_convoy";
    lead.family = bandit_dry_run::lead_family::moving_carrier;
    lead.lead_bounty_value = 4;
    lead.lead_confidence_bonus = 1;
    lead.threat_penalty = 6;
    lead.monster_pressure_bonus = 2;
    lead.target_coherence_bonus = 1;
    lead.distance_multiplier = 0.60;

    const bandit_dry_run::evaluation_result result = bandit_dry_run::evaluate( camp, { lead } );
    const bandit_dry_run::candidate_debug *stalk = find_candidate( result,
            bandit_dry_run::job_template::stalk, "guarded_convoy" );

    REQUIRE( stalk != nullptr );
    CHECK( stalk->valid );
    CHECK( stalk->score.threat_penalty == 6 );
    CHECK( stalk->score.monster_pressure_bonus == 2 );
    CHECK( stalk->score.target_coherence_bonus == 1 );
    CHECK( stalk->score.effective_threat_penalty == 3 );
    CHECK( stalk->score.opportunism_bonus == 2 );
    CHECK( stalk->score.pre_veto_job_score > 0.0 );
    CHECK( result.candidates[result.winner_index].job == bandit_dry_run::job_template::stalk );

    const std::string report = bandit_dry_run::render_report( result );
    CHECK( report.find( "monster_pressure_bonus=2" ) != std::string::npos );
    CHECK( report.find( "target_coherence_bonus=1" ) != std::string::npos );
    CHECK( report.find( "effective_threat_penalty=3" ) != std::string::npos );
    CHECK( report.find( "opportunism_bonus=2" ) != std::string::npos );
    CHECK( report.find( "danger collapse: raw threat 6 minus monster pressure 2 and coherence break 1 -> effective threat 3" ) != std::string::npos );
    CHECK( report.find( "opportunism window: degraded target coherence adds a bounded push" ) != std::string::npos );
}

TEST_CASE( "bandit_dry_run_soft_veto_clamps_extraction_but_keeps_pressure_jobs", "[bandit][dry_run]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;

    bandit_dry_run::lead_input lead;
    lead.id = "escorted_convoy";
    lead.envelope_id = "escorted_convoy";
    lead.family = bandit_dry_run::lead_family::moving_carrier;
    lead.lead_bounty_value = 4;
    lead.lead_confidence_bonus = 1;
    lead.threat_penalty = 2;
    lead.distance_multiplier = 0.70;
    lead.reward_profile_match = 1;
    lead.threat_gate_result = bandit_dry_run::threat_gate::soft_veto;

    const bandit_dry_run::evaluation_result result = bandit_dry_run::evaluate( camp, { lead } );
    const bandit_dry_run::candidate_debug *stalk = find_candidate( result,
            bandit_dry_run::job_template::stalk, "escorted_convoy" );
    const bandit_dry_run::candidate_debug *steal = find_candidate( result,
            bandit_dry_run::job_template::steal, "escorted_convoy" );

    REQUIRE( stalk != nullptr );
    REQUIRE( steal != nullptr );
    CHECK( stalk->valid );
    CHECK( stalk->score.final_job_score == Approx( 1.0 ) );
    CHECK( steal->score.final_job_score == Approx( 0.0 ) );
    CHECK( result.candidates[result.winner_index].job == bandit_dry_run::job_template::stalk );
    CHECK( result.metrics.soft_veto_caps == 2 );
    CHECK( result.metrics.soft_veto_collapses == 1 );

    const std::string report = bandit_dry_run::render_report( result );
    CHECK( report.find( "threat_gate=soft_veto" ) != std::string::npos );
    CHECK( report.find( "soft_veto: pure extraction collapses back to hold / chill" ) != std::string::npos );
}

TEST_CASE( "bandit_dry_run_metrics_expose_lead_filtering_and_churn", "[bandit][dry_run]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 1;

    bandit_dry_run::lead_input accepted;
    accepted.id = "farmhouse";
    accepted.envelope_id = "farmhouse";
    accepted.family = bandit_dry_run::lead_family::site;
    accepted.lead_bounty_value = 3;
    accepted.lead_confidence_bonus = 1;

    bandit_dry_run::lead_input duplicate = accepted;
    duplicate.id = "farmhouse_echo";

    bandit_dry_run::lead_input invalid = accepted;
    invalid.id = "stale_farmhouse";
    invalid.envelope_id = "stale_farmhouse";
    invalid.still_valid = false;

    bandit_dry_run::lead_input no_envelope = accepted;
    no_envelope.id = "ghost_signal";
    no_envelope.envelope_id = "ghost_signal";
    no_envelope.has_real_envelope = false;

    const bandit_dry_run::evaluation_result result = bandit_dry_run::evaluate( camp,
            { accepted, duplicate, invalid, no_envelope } );

    CHECK( result.metrics.input_lead_count == 4 );
    CHECK( result.metrics.accepted_lead_count == 1 );
    CHECK( result.metrics.rejected_no_envelope_count == 1 );
    CHECK( result.metrics.rejected_invalid_lead_count == 1 );
    CHECK( result.metrics.deduped_lead_count == 1 );
    CHECK( result.metrics.compatible_job_checks == 4 );
    CHECK( result.metrics.manpower_rejection_count == 1 );
    CHECK( result.metrics.candidates_generated == 3 );
    CHECK( result.metrics.score_evaluations == 3 );
    CHECK( result.metrics.path_checks == 3 );
    CHECK( result.metrics.valid_outward_candidates == 3 );
    CHECK( result.metrics.invalid_outward_candidates == 0 );
    CHECK( result.metrics.winner_comparisons == 3 );
}
