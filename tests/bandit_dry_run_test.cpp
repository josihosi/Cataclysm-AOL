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

    const std::string report = bandit_dry_run::render_report( result );
    CHECK( report.find( "need_override_bonus=2" ) != std::string::npos );
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

    const std::string report = bandit_dry_run::render_report( result );
    CHECK( report.find( "threat_gate=soft_veto" ) != std::string::npos );
    CHECK( report.find( "soft_veto: pure extraction collapses back to hold / chill" ) != std::string::npos );
}
