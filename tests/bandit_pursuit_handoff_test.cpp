#include "bandit_dry_run.h"
#include "bandit_pursuit_handoff.h"

#include "catch/catch.hpp"

namespace
{
const bandit_dry_run::candidate_debug &winner( const bandit_dry_run::evaluation_result &result )
{
    return result.candidates[result.winner_index];
}

bandit_pursuit_handoff::abstract_group_state make_group()
{
    bandit_pursuit_handoff::abstract_group_state group;
    group.group_id = "ridge_pack";
    group.source_camp_id = "oak_camp";
    group.group_strength = 2;
    group.confidence = 2;
    group.panic_threshold = 2;
    group.cargo_capacity = 2;
    group.current_target_or_mark = "ridge_smoke";
    group.current_threat_estimate = 1;
    group.current_bounty_estimate = 3;
    group.mission_urgency = 1;
    group.retreat_bias = 0;
    group.goal_stickiness = 2;
    group.goal_preemption_posture = 1;
    group.return_clock = 3;
    group.anchored_identities.push_back( { "leader_marta", "alive" } );
    group.known_recent_marks.push_back( "ridge_smoke" );
    return group;
}
} // namespace

TEST_CASE( "bandit pursuit handoff builds a bounded scout entry packet", "[bandit][handoff]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;

    bandit_dry_run::lead_input lead;
    lead.id = "ridge_smoke";
    lead.envelope_id = "ridge_smoke";
    lead.family = bandit_dry_run::lead_family::site;
    lead.lead_bounty_value = 3;
    lead.lead_confidence_bonus = 2;
    lead.distance_multiplier = 0.55;
    lead.threat_penalty = 1;
    lead.hard_blocked_jobs = { bandit_dry_run::job_template::scavenge,
                               bandit_dry_run::job_template::steal,
                               bandit_dry_run::job_template::raid };
    lead.validity_notes = { "Thin smoke is a scouting clue, not loot permission." };

    const bandit_dry_run::evaluation_result evaluation = bandit_dry_run::evaluate( camp, { lead } );
    REQUIRE( winner( evaluation ).job == bandit_dry_run::job_template::scout );
    REQUIRE( bandit_pursuit_handoff::supports_pursuit_handoff( winner( evaluation ) ) );

    bandit_pursuit_handoff::entry_context context;
    context.contact = bandit_pursuit_handoff::contact_certainty::broad;

    const bandit_pursuit_handoff::entry_payload entry =
        bandit_pursuit_handoff::build_entry_payload( make_group(), winner( evaluation ), context );

    CHECK( entry.valid );
    CHECK( entry.mode == bandit_pursuit_handoff::entry_mode::scout );
    CHECK( entry.job_type == bandit_dry_run::job_template::scout );
    CHECK( entry.lead_carrier == bandit_dry_run::lead_family::site );
    CHECK( entry.current_target_or_mark == "ridge_smoke" );
    CHECK( entry.group_strength == 2 );
    CHECK( entry.confidence == 2 );
    CHECK( entry.return_clock == 3 );
    CHECK( entry.current_threat_estimate == 1 );
    CHECK( entry.current_bounty_estimate == 3 );
}

TEST_CASE( "bandit pursuit handoff keeps return consequences explicit and applied", "[bandit][handoff]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;

    bandit_dry_run::lead_input lead;
    lead.id = "ridge_smoke";
    lead.envelope_id = "ridge_smoke";
    lead.family = bandit_dry_run::lead_family::site;
    lead.lead_bounty_value = 3;
    lead.lead_confidence_bonus = 2;
    lead.distance_multiplier = 0.55;
    lead.threat_penalty = 1;
    lead.hard_blocked_jobs = { bandit_dry_run::job_template::scavenge,
                               bandit_dry_run::job_template::steal,
                               bandit_dry_run::job_template::raid };

    const bandit_dry_run::evaluation_result evaluation = bandit_dry_run::evaluate( camp, { lead } );
    const bandit_pursuit_handoff::entry_payload entry =
        bandit_pursuit_handoff::build_entry_payload( make_group(), winner( evaluation ), {} );
    REQUIRE( entry.valid );

    bandit_pursuit_handoff::local_outcome outcome;
    outcome.survivors_remaining = 1;
    outcome.anchored_identity_updates = { { "leader_marta", "wounded" } };
    outcome.wound_burden = bandit_pursuit_handoff::wound_burden_state::moderate;
    outcome.morale = bandit_pursuit_handoff::morale_state::shaken;
    outcome.cargo_profile_carried.meds = 1;
    outcome.cargo_profile_carried.ammo = 1;
    outcome.camp_stockpile_delta.meds = 1;
    outcome.result = bandit_pursuit_handoff::mission_result::partial_success;
    outcome.resolution = bandit_pursuit_handoff::lead_resolution::narrowed;
    outcome.threat_writeback = { { "ridge_smoke", "raise", 3 } };
    outcome.bounty_writeback = { { "ridge_smoke", "confirm", 2 } };
    outcome.new_marks_learned = { "ridge_smoke_cabin_edge" };
    outcome.posture = bandit_pursuit_handoff::return_posture::escape_home;
    outcome.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::tight;

    const bandit_pursuit_handoff::return_packet packet =
        bandit_pursuit_handoff::build_return_packet( entry, outcome );
    CHECK( packet.valid );
    CHECK( packet.cargo_profile_carried.meds == 0 );
    CHECK( packet.cargo_profile_carried.ammo == 1 );
    CHECK( packet.camp_stockpile_delta.meds == 1 );
    CHECK( packet.result == bandit_pursuit_handoff::mission_result::partial_success );
    CHECK( packet.resolution == bandit_pursuit_handoff::lead_resolution::narrowed );
    REQUIRE( packet.new_marks_learned.size() == 1 );
    CHECK( packet.new_marks_learned.front() == "ridge_smoke_cabin_edge" );

    bandit_pursuit_handoff::abstract_group_state group = make_group();
    bandit_pursuit_handoff::apply_return_packet( group, packet );

    CHECK( group.group_strength == 1 );
    REQUIRE( group.anchored_identities.size() == 1 );
    CHECK( group.anchored_identities.front().status == "wounded" );
    CHECK( group.carried_cargo.meds == 0 );
    CHECK( group.carried_cargo.ammo == 1 );
    CHECK( group.current_target_or_mark == "ridge_smoke_cabin_edge" );
    CHECK( group.current_threat_estimate == 3 );
    CHECK( group.current_bounty_estimate == 2 );
    CHECK( group.last_return_posture == bandit_pursuit_handoff::return_posture::escape_home );
    CHECK( group.remaining_pressure == bandit_pursuit_handoff::remaining_return_pressure_state::tight );
    CHECK( group.return_clock == 2 );
    REQUIRE( group.known_recent_marks.size() == 2 );
    CHECK( group.known_recent_marks.back() == "ridge_smoke_cabin_edge" );

    const std::string report = bandit_pursuit_handoff::render_report( entry, packet );
    CHECK( report.find( "mode=scout" ) != std::string::npos );
    CHECK( report.find( "mission_result=partial_success" ) != std::string::npos );
    CHECK( report.find( "lead_resolution=narrowed" ) != std::string::npos );
    CHECK( report.find( "ridge_smoke_cabin_edge" ) != std::string::npos );
}

TEST_CASE( "bandit pursuit handoff uses shadow for moving carriers unless return pressure collapses", "[bandit][handoff]" )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = 2;

    bandit_dry_run::lead_input lead;
    lead.id = "lonely_trader";
    lead.envelope_id = "lonely_trader";
    lead.family = bandit_dry_run::lead_family::moving_carrier;
    lead.lead_bounty_value = 4;
    lead.lead_confidence_bonus = 2;
    lead.distance_multiplier = 0.85;
    lead.threat_penalty = 1;

    const bandit_dry_run::evaluation_result evaluation = bandit_dry_run::evaluate( camp, { lead } );
    REQUIRE( winner( evaluation ).job == bandit_dry_run::job_template::stalk );

    CHECK( bandit_pursuit_handoff::choose_entry_mode( winner( evaluation ),
                                                      bandit_pursuit_handoff::contact_certainty::localized,
                                                      bandit_pursuit_handoff::return_pressure_state::normal ) ==
           bandit_pursuit_handoff::entry_mode::shadow );
    CHECK( bandit_pursuit_handoff::choose_entry_mode( winner( evaluation ),
                                                      bandit_pursuit_handoff::contact_certainty::localized,
                                                      bandit_pursuit_handoff::return_pressure_state::withdraw_now ) ==
           bandit_pursuit_handoff::entry_mode::withdrawal );
}
