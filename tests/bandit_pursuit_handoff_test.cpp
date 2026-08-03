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
    group.activity_generation = 7;
    group.handoff_epoch = 3;
    group.return_application_key = bandit_pursuit_handoff::make_operation_component_key(
                                       group.group_id, group.activity_generation, "return" );
    group.report_application_key = bandit_pursuit_handoff::make_operation_component_key(
                                       group.group_id, group.activity_generation, "report" );
    group.cargo_application_key = bandit_pursuit_handoff::make_operation_component_key(
                                      group.group_id, group.activity_generation, "cargo" );
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

bool same_group_state( const bandit_pursuit_handoff::abstract_group_state &lhs,
                       const bandit_pursuit_handoff::abstract_group_state &rhs )
{
    if( lhs.group_id != rhs.group_id || lhs.source_camp_id != rhs.source_camp_id ||
        lhs.activity_generation != rhs.activity_generation ||
        lhs.handoff_epoch != rhs.handoff_epoch ||
        lhs.return_application_key != rhs.return_application_key ||
        lhs.report_application_key != rhs.report_application_key ||
        lhs.cargo_application_key != rhs.cargo_application_key ||
        lhs.group_strength != rhs.group_strength || lhs.confidence != rhs.confidence ||
        lhs.panic_threshold != rhs.panic_threshold || lhs.cargo_capacity != rhs.cargo_capacity ||
        lhs.current_target_or_mark != rhs.current_target_or_mark ||
        lhs.current_threat_estimate != rhs.current_threat_estimate ||
        lhs.current_bounty_estimate != rhs.current_bounty_estimate ||
        lhs.mission_urgency != rhs.mission_urgency || lhs.retreat_bias != rhs.retreat_bias ||
        lhs.goal_stickiness != rhs.goal_stickiness ||
        lhs.goal_preemption_posture != rhs.goal_preemption_posture ||
        lhs.return_clock != rhs.return_clock || lhs.wound_burden != rhs.wound_burden ||
        lhs.morale != rhs.morale || lhs.last_return_posture != rhs.last_return_posture ||
        lhs.remaining_pressure != rhs.remaining_pressure ||
        lhs.known_recent_marks != rhs.known_recent_marks ||
        lhs.anchored_identities.size() != rhs.anchored_identities.size() ) {
        return false;
    }

    for( std::size_t i = 0; i < lhs.anchored_identities.size(); ++i ) {
        if( lhs.anchored_identities[i].id != rhs.anchored_identities[i].id ||
            lhs.anchored_identities[i].status != rhs.anchored_identities[i].status ) {
            return false;
        }
    }

    const bandit_pursuit_handoff::cargo_profile &lhs_cargo = lhs.carried_cargo;
    const bandit_pursuit_handoff::cargo_profile &rhs_cargo = rhs.carried_cargo;
    return lhs_cargo.food == rhs_cargo.food && lhs_cargo.meds == rhs_cargo.meds &&
           lhs_cargo.ammo == rhs_cargo.ammo && lhs_cargo.gear == rhs_cargo.gear &&
           lhs_cargo.fuel == rhs_cargo.fuel &&
           lhs_cargo.trade_goods == rhs_cargo.trade_goods;
}
} // namespace

TEST_CASE( "bandit_pursuit_handoff_builds_a_bounded_scout_entry_packet", "[bandit][handoff]" )
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
    CHECK( entry.activity_generation == 7 );
    CHECK( entry.handoff_epoch == 3 );
    CHECK( entry.return_application_key == "ridge_pack:return:7" );
    CHECK( entry.report_application_key == "ridge_pack:report:7" );
    CHECK( entry.cargo_application_key == "ridge_pack:cargo:7" );
    CHECK( entry.current_target_or_mark == "ridge_smoke" );
    CHECK( entry.group_strength == 2 );
    CHECK( entry.confidence == 2 );
    CHECK( entry.return_clock == 3 );
    CHECK( entry.current_threat_estimate == 1 );
    CHECK( entry.current_bounty_estimate == 3 );
}

TEST_CASE( "bandit_pursuit_handoff_guards_return_application_by_generation_and_key",
           "[bandit][handoff]" )
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
    const bandit_pursuit_handoff::abstract_group_state source_group = make_group();
    const bandit_pursuit_handoff::entry_payload entry =
        bandit_pursuit_handoff::build_entry_payload( source_group, winner( evaluation ), {} );

    bandit_pursuit_handoff::local_outcome outcome;
    outcome.survivors_remaining = 1;
    outcome.anchored_identity_updates = { { "leader_marta", "wounded" } };
    outcome.wound_burden = bandit_pursuit_handoff::wound_burden_state::heavy;
    outcome.morale = bandit_pursuit_handoff::morale_state::breaking;
    outcome.cargo_profile_carried.ammo = 4;
    outcome.resolution = bandit_pursuit_handoff::lead_resolution::narrowed;
    outcome.threat_writeback = { { "ridge_smoke", "raise", 5 } };
    outcome.bounty_writeback = { { "ridge_smoke", "confirm", 6 } };
    outcome.new_marks_learned = { "ridge_smoke_cabin_edge" };
    outcome.posture = bandit_pursuit_handoff::return_posture::broken_flee;
    outcome.remaining_pressure =
        bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;

    const bandit_pursuit_handoff::return_packet packet =
        bandit_pursuit_handoff::build_return_packet( entry, outcome );
    REQUIRE( packet.valid );
    CHECK( packet.activity_generation == source_group.activity_generation );
    CHECK( packet.handoff_epoch == source_group.handoff_epoch );
    CHECK( packet.return_application_key == source_group.return_application_key );
    CHECK( packet.report_application_key == source_group.report_application_key );
    CHECK( packet.cargo_application_key == source_group.cargo_application_key );
    REQUIRE( packet.member_return_receipts.size() == source_group.anchored_identities.size() );
    CHECK( packet.member_return_receipts.front().member_id == "leader_marta" );
    CHECK( packet.member_return_receipts.front().application_key ==
           "ridge_pack:return:7:member:leader_marta" );

    bandit_pursuit_handoff::return_packet stale_generation = packet;
    ++stale_generation.activity_generation;
    bandit_pursuit_handoff::abstract_group_state generation_target = source_group;
    bandit_pursuit_handoff::apply_return_packet( generation_target, stale_generation );
    CHECK( same_group_state( generation_target, source_group ) );

    bandit_pursuit_handoff::return_packet stale_handoff = packet;
    --stale_handoff.handoff_epoch;
    bandit_pursuit_handoff::abstract_group_state handoff_target = source_group;
    bandit_pursuit_handoff::apply_return_packet( handoff_target, stale_handoff );
    CHECK( same_group_state( handoff_target, source_group ) );

    bandit_pursuit_handoff::return_packet wrong_key = packet;
    wrong_key.return_application_key = "ridge_pack:different-return:7";
    bandit_pursuit_handoff::abstract_group_state key_target = source_group;
    bandit_pursuit_handoff::apply_return_packet( key_target, wrong_key );
    CHECK( same_group_state( key_target, source_group ) );

    bandit_pursuit_handoff::return_packet wrong_report_key = packet;
    wrong_report_key.report_application_key = "ridge_pack:report:8";
    bandit_pursuit_handoff::abstract_group_state report_target = source_group;
    bandit_pursuit_handoff::apply_return_packet( report_target, wrong_report_key );
    CHECK( same_group_state( report_target, source_group ) );

    bandit_pursuit_handoff::return_packet wrong_cargo_key = packet;
    wrong_cargo_key.cargo_application_key = "ridge_pack:cargo:8";
    bandit_pursuit_handoff::abstract_group_state cargo_target = source_group;
    bandit_pursuit_handoff::apply_return_packet( cargo_target, wrong_cargo_key );
    CHECK( same_group_state( cargo_target, source_group ) );

    bandit_pursuit_handoff::return_packet missing_receipt = packet;
    missing_receipt.member_return_receipts.clear();
    bandit_pursuit_handoff::abstract_group_state missing_receipt_target = source_group;
    bandit_pursuit_handoff::apply_return_packet( missing_receipt_target, missing_receipt );
    CHECK( same_group_state( missing_receipt_target, source_group ) );

    bandit_pursuit_handoff::return_packet forged_receipt = packet;
    forged_receipt.member_return_receipts.front().application_key += ":forged";
    bandit_pursuit_handoff::abstract_group_state forged_receipt_target = source_group;
    bandit_pursuit_handoff::apply_return_packet( forged_receipt_target, forged_receipt );
    CHECK( same_group_state( forged_receipt_target, source_group ) );

    bandit_pursuit_handoff::abstract_group_state two_member_group = make_group();
    two_member_group.anchored_identities.push_back( { "scout_ines", "alive" } );
    const bandit_pursuit_handoff::entry_payload two_member_entry =
        bandit_pursuit_handoff::build_entry_payload( two_member_group, winner( evaluation ), {} );
    bandit_pursuit_handoff::return_packet duplicate_receipt =
        bandit_pursuit_handoff::build_return_packet( two_member_entry, outcome );
    REQUIRE( duplicate_receipt.member_return_receipts.size() == 2 );
    duplicate_receipt.member_return_receipts.back() = duplicate_receipt.member_return_receipts.front();
    bandit_pursuit_handoff::abstract_group_state duplicate_receipt_target = two_member_group;
    bandit_pursuit_handoff::apply_return_packet( duplicate_receipt_target, duplicate_receipt );
    CHECK( same_group_state( duplicate_receipt_target, two_member_group ) );
}

TEST_CASE( "bandit_pursuit_handoff_component_keys_do_not_collide_across_operations_or_members",
           "[bandit][handoff]" )
{
    const std::string return_key = bandit_pursuit_handoff::make_operation_component_key(
                                       "ridge_pack", 7, "return" );
    const std::string report_key = bandit_pursuit_handoff::make_operation_component_key(
                                       "ridge_pack", 7, "report" );
    const std::string next_generation_key = bandit_pursuit_handoff::make_operation_component_key(
                                                "ridge_pack", 8, "return" );
    const std::string other_operation_key = bandit_pursuit_handoff::make_operation_component_key(
                                                "river_pack", 7, "return" );
    const std::string leader_receipt = bandit_pursuit_handoff::make_operation_component_key(
                                           "ridge_pack", 7, "return", "leader_marta" );
    const std::string scout_receipt = bandit_pursuit_handoff::make_operation_component_key(
                                          "ridge_pack", 7, "return", "scout_ines" );

    CHECK( return_key == "ridge_pack:return:7" );
    CHECK( leader_receipt == "ridge_pack:return:7:member:leader_marta" );
    CHECK( return_key != report_key );
    CHECK( return_key != next_generation_key );
    CHECK( return_key != other_operation_key );
    CHECK( leader_receipt != scout_receipt );
}

TEST_CASE( "bandit_pursuit_handoff_keeps_return_consequences_explicit_and_applied", "[bandit][handoff]" )
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

TEST_CASE( "bandit_pursuit_handoff_uses_shadow_for_moving_carriers_unless_return_pressure_collapses", "[bandit][handoff]" )
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
