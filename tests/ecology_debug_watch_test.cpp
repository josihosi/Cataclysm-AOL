#include "ecology_debug_watch.h"

#include <optional>
#include <string>

#include "cata_catch.h"
#include "json.h"
#include "json_loader.h"

namespace
{

ecology_debug::selected_projection projection( std::string phase = "observing" )
{
    ecology_debug::selected_projection result;
    result.marker.id = "dispatch/camp/outing/1";
    result.marker.alias = "BD-TEST";
    result.marker.kind = ecology_debug::entity_kind::bandit_dispatch;
    result.marker.faction = ecology_debug::entity_faction::bandit;
    result.marker.omt = tripoint_abs_omt( 10, 20, 0 );
    result.marker.owner = "local";
    result.marker.loaded = true;
    result.marker.state = phase;
    result.marker.generation = 1;
    result.token.world_identity = "world/test";
    result.token.canonical_id = result.marker.id;
    result.token.generation = 1;
    result.token.owner = "local";
    result.token.authority_token = "site/4:local/42";

    ecology_debug::selected_detail detail;
    detail.entity_id = result.marker.id;
    detail.phase = phase;
    ecology_debug::member_detail first;
    first.npc_id = character_id( 42 );
    first.name = "Scout";
    first.hp_percent = 100;
    first.status = "outbound";
    detail.members.push_back( first );
    ecology_debug::member_detail second;
    second.npc_id = character_id( 43 );
    second.name = "Guard";
    second.hp_percent = 100;
    second.status = "outbound";
    detail.members.push_back( second );
    result.detail = detail;
    return result;
}

ecology_debug::watch_input transition_input( const ecology_debug::selected_projection &prior,
        const ecology_debug::selected_projection &current )
{
    ecology_debug::watch_input input;
    input.prior = prior;
    input.current = current;
    input.now_minutes = 110;
    input.armed_minutes = 100;
    input.last_progress_minutes = 100;
    return input;
}

ecology_debug::watch_spec spec( ecology_debug::watch_preset preset )
{
    ecology_debug::watch_spec result;
    result.preset = preset;
    return result;
}

} // namespace

TEST_CASE( "ecology_watch_phase_and_evidence_presets_are_transition_only",
           "[ecology_debug][watch][phase4]" )
{
    const ecology_debug::selected_projection before = projection();
    ecology_debug::selected_projection moved = before;
    moved.marker.omt = tripoint_abs_omt( 11, 20, -1 );

    const ecology_debug::watch_result no_phase = ecology_debug::evaluate_watch(
                spec( ecology_debug::watch_preset::selected_phase_change ),
                transition_input( before, moved ) );
    CHECK( no_phase.status == ecology_debug::watch_status::watching );
    CHECK( no_phase.meaningful_progress );

    ecology_debug::selected_projection changed = moved;
    changed.detail->phase = "approaching";
    const ecology_debug::watch_result phase = ecology_debug::evaluate_watch(
                spec( ecology_debug::watch_preset::selected_phase_change ),
                transition_input( before, changed ) );
    CHECK( phase.status == ecology_debug::watch_status::triggered );
    CHECK( phase.reason == "selected_phase_changed" );

    ecology_debug::selected_projection aged = before;
    aged.detail->evidence_age_minutes = 30;
    const ecology_debug::watch_result age_only = ecology_debug::evaluate_watch(
                spec( ecology_debug::watch_preset::evidence_acquired ),
                transition_input( before, aged ) );
    CHECK( age_only.status == ecology_debug::watch_status::watching );

    ecology_debug::selected_projection evidence = aged;
    evidence.detail->evidence_id = "light@10,20,0";
    evidence.detail->evidence_kind = "routine";
    evidence.detail->evidence_observed_minutes = 110;
    evidence.detail->evidence_reason = "shelter_light";
    const ecology_debug::watch_result acquired = ecology_debug::evaluate_watch(
                spec( ecology_debug::watch_preset::evidence_acquired ),
                transition_input( aged, evidence ) );
    CHECK( acquired.status == ecology_debug::watch_status::triggered );

    ecology_debug::selected_projection older = evidence;
    older.detail->evidence_age_minutes = 90;
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::evidence_acquired ),
               transition_input( evidence, older ) ).status ==
           ecology_debug::watch_status::watching );

    ecology_debug::selected_projection replaced = older;
    replaced.detail->evidence_id = "smoke@10,20,0";
    replaced.detail->evidence_observed_minutes = 120;
    replaced.detail->evidence_reason = "smoke_column";
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::evidence_acquired ),
               transition_input( older, replaced ) ).status ==
           ecology_debug::watch_status::triggered );
}

TEST_CASE( "ecology_watch_exposure_uses_exact_authoritative_phases",
           "[ecology_debug][watch][phase4]" )
{
    const ecology_debug::selected_projection before = projection( "observing" );
    ecology_debug::selected_projection burned = before;
    burned.detail->phase = "burned_withdrawal";
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::exposure_or_burn ),
               transition_input( before, burned ) ).status ==
           ecology_debug::watch_status::triggered );

    ecology_debug::selected_projection exposed = burned;
    exposed.detail->phase = "returning_exposed";
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::exposure_or_burn ),
               transition_input( burned, exposed ) ).status ==
           ecology_debug::watch_status::triggered );

    ecology_debug::selected_projection loose_match = before;
    loose_match.detail->phase = "almost_burned_withdrawal";
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::exposure_or_burn ),
               transition_input( before, loose_match ) ).status ==
           ecology_debug::watch_status::watching );

    ecology_debug::selected_projection burn_observation = before;
    burn_observation.detail->evidence_id = "burn@10,20,0";
    burn_observation.detail->evidence_kind = "burn";
    burn_observation.detail->evidence_state = "burned";
    burn_observation.detail->evidence_observed_minutes = 111;
    burn_observation.detail->evidence_reason = "route burned";
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::exposure_or_burn ),
               transition_input( before, burn_observation ) ).status ==
           ecology_debug::watch_status::triggered );

    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::exposure_or_burn ),
               transition_input( burned, burned ) ).status ==
           ecology_debug::watch_status::watching );
}

TEST_CASE( "ecology_watch_casualty_distinguishes_wounds_and_terminal_members",
           "[ecology_debug][watch][phase4]" )
{
    const ecology_debug::selected_projection before = projection();
    ecology_debug::selected_projection wounded = before;
    wounded.detail->members.front().hp_percent = 1;
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::casualty ),
               transition_input( before, wounded ) ).status ==
           ecology_debug::watch_status::watching );

    ecology_debug::selected_projection killed = wounded;
    killed.detail->members.front().hp_percent = 0;
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::casualty ),
               transition_input( wounded, killed ) ).status ==
           ecology_debug::watch_status::triggered );

    ecology_debug::selected_projection missing = before;
    missing.detail->members.front().status = "missing";
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::casualty ),
               transition_input( before, missing ) ).status ==
           ecology_debug::watch_status::triggered );

    ecology_debug::selected_projection reduced = before;
    reduced.detail->members.pop_back();
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::casualty ),
               transition_input( before, reduced ) ).status ==
           ecology_debug::watch_status::triggered );
}

TEST_CASE( "ecology_watch_return_completion_and_disappearance_fail_closed",
           "[ecology_debug][watch][phase4]" )
{
    const ecology_debug::selected_projection before = projection();
    ecology_debug::selected_projection returning = before;
    returning.detail->phase = "returning_home";
    const ecology_debug::watch_result return_started = ecology_debug::evaluate_watch(
                spec( ecology_debug::watch_preset::return_or_completion ),
                transition_input( before, returning ) );
    CHECK( return_started.status == ecology_debug::watch_status::triggered );

    ecology_debug::watch_input completed = transition_input( returning, returning );
    completed.current.reset();
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::return_or_completion ), completed ).status ==
           ecology_debug::watch_status::completed );

    ecology_debug::watch_input unexpected = transition_input( before, before );
    unexpected.current.reset();
    const ecology_debug::watch_result anomaly = ecology_debug::evaluate_watch(
                spec( ecology_debug::watch_preset::return_or_completion ), unexpected );
    CHECK( anomaly.status == ecology_debug::watch_status::anomaly );
    CHECK( anomaly.disposition == ecology_debug::trigger_disposition::fail );

    ecology_debug::selected_projection dead = before;
    for( ecology_debug::member_detail &member : dead.detail->members ) {
        member.hp_percent = 0;
        member.status = "dead";
    }
    ecology_debug::watch_input died = transition_input( dead, dead );
    died.current.reset();
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::return_or_completion ), died ).status ==
           ecology_debug::watch_status::died );

    ecology_debug::selected_projection all_missing = before;
    for( ecology_debug::member_detail &member : all_missing.detail->members ) {
        member.hp_percent.reset();
        member.status = "missing";
    }
    ecology_debug::watch_input unresolved = transition_input( all_missing, all_missing );
    unresolved.current.reset();
    CHECK( ecology_debug::evaluate_watch(
               spec( ecology_debug::watch_preset::return_or_completion ), unresolved ).status ==
           ecology_debug::watch_status::anomaly );
}

TEST_CASE( "ecology_watch_deadline_is_explicit_bounded_and_one_shot",
           "[ecology_debug][watch][phase4]" )
{
    const ecology_debug::selected_projection selected = projection();
    ecology_debug::watch_spec deadline_spec = spec(
                ecology_debug::watch_preset::no_progress_by_deadline );
    deadline_spec.absolute_deadline_minutes = 160;
    deadline_spec.disposition = ecology_debug::trigger_disposition::pause;
    ecology_debug::watch_input input = transition_input( selected, selected );
    input.now_minutes = 159;
    CHECK( ecology_debug::evaluate_watch( deadline_spec, input ).status ==
           ecology_debug::watch_status::watching );
    input.now_minutes = 160;
    const ecology_debug::watch_result boundary = ecology_debug::evaluate_watch( deadline_spec,
            input );
    CHECK( boundary.status == ecology_debug::watch_status::timed_out );
    CHECK( boundary.reason == "no_progress_deadline_reached" );
    CHECK( boundary.disposition == ecology_debug::trigger_disposition::pause );
    CHECK( boundary.newly_triggered );

    input.already_triggered = true;
    const ecology_debug::watch_result consumed = ecology_debug::evaluate_watch( deadline_spec,
            input );
    CHECK( consumed.status == ecology_debug::watch_status::consumed );
    CHECK_FALSE( consumed.newly_triggered );
    CHECK( consumed.disposition == ecology_debug::trigger_disposition::continue_capture );

    input.already_triggered = false;
    input.last_progress_minutes = 130;
    CHECK( ecology_debug::evaluate_watch( deadline_spec, input ).status ==
           ecology_debug::watch_status::timed_out );

    ecology_debug::watch_spec rolled = deadline_spec;
    rolled.absolute_deadline_minutes = 190;
    input.now_minutes = 189;
    CHECK( ecology_debug::evaluate_watch( rolled, input ).status ==
           ecology_debug::watch_status::watching );
    input.now_minutes = 190;
    CHECK( ecology_debug::evaluate_watch( rolled, input ).status ==
           ecology_debug::watch_status::timed_out );

    ecology_debug::selected_projection moved = selected;
    moved.marker.omt = tripoint_abs_omt( 11, 20, 0 );
    input = transition_input( selected, moved );
    input.now_minutes = 160;
    CHECK( ecology_debug::evaluate_watch( deadline_spec, input ).meaningful_progress );

    ecology_debug::selected_projection wounded = selected;
    wounded.detail->members.front().hp_percent = 75;
    input = transition_input( selected, wounded );
    input.now_minutes = 160;
    CHECK( ecology_debug::evaluate_watch( deadline_spec, input ).meaningful_progress );

    ecology_debug::watch_spec missing_deadline = spec(
                ecology_debug::watch_preset::no_progress_by_deadline );
    CHECK( ecology_debug::evaluate_watch( missing_deadline, input ).status ==
           ecology_debug::watch_status::invalid );

    ecology_debug::watch_spec guarded = spec(
                                            ecology_debug::watch_preset::evidence_acquired );
    guarded.absolute_deadline_minutes = 160;
    input.last_progress_minutes = 100;
    const ecology_debug::watch_result timed_out = ecology_debug::evaluate_watch( guarded, input );
    CHECK( timed_out.status == ecology_debug::watch_status::timed_out );
    CHECK( timed_out.reason == "deadline_reached" );

    ecology_debug::selected_projection evidence = selected;
    evidence.detail->evidence_reason = "light";
    input.current = evidence;
    CHECK( ecology_debug::evaluate_watch( guarded, input ).status ==
           ecology_debug::watch_status::triggered );
}

TEST_CASE( "ecology_watch_identity_policy_and_json_are_stable",
           "[ecology_debug][watch][phase4]" )
{
    for( const ecology_debug::watch_preset preset : {
             ecology_debug::watch_preset::selected_phase_change,
             ecology_debug::watch_preset::evidence_acquired,
             ecology_debug::watch_preset::exposure_or_burn,
             ecology_debug::watch_preset::casualty,
             ecology_debug::watch_preset::return_or_completion,
             ecology_debug::watch_preset::no_progress_by_deadline
         } ) {
        const std::string serialized = ecology_debug::to_string( preset );
        REQUIRE( ecology_debug::watch_preset_from_string( serialized ) );
        CHECK( *ecology_debug::watch_preset_from_string( serialized ) == preset );
    }
    CHECK_FALSE( ecology_debug::watch_preset_from_string( "unknown" ) );

    const ecology_debug::selected_projection before = projection();
    ecology_debug::selected_projection after = before;
    after.detail->phase = "approaching";
    ecology_debug::watch_input input = transition_input( before, after );

    for( const ecology_debug::trigger_disposition disposition : {
             ecology_debug::trigger_disposition::continue_capture,
             ecology_debug::trigger_disposition::pause,
             ecology_debug::trigger_disposition::fail
         } ) {
        ecology_debug::watch_spec phase_spec = spec(
                    ecology_debug::watch_preset::selected_phase_change );
        phase_spec.disposition = disposition;
        CHECK( ecology_debug::evaluate_watch( phase_spec, input ).disposition == disposition );
    }

    ecology_debug::selected_projection replacement = after;
    replacement.token.authority_token = "site/9:local/99";
    input.current = replacement;
    const ecology_debug::watch_result mismatch = ecology_debug::evaluate_watch(
                spec( ecology_debug::watch_preset::selected_phase_change ), input );
    CHECK( mismatch.status == ecology_debug::watch_status::anomaly );
    CHECK( mismatch.disposition == ecology_debug::trigger_disposition::fail );
    CHECK( mismatch.reason == "entity_token_mismatch" );

    input.current = after;
    const ecology_debug::watch_spec phase_spec = spec(
                ecology_debug::watch_preset::selected_phase_change );
    const ecology_debug::watch_result result = ecology_debug::evaluate_watch( phase_spec, input );
    const std::string first_json = ecology_debug::serialize_watch_json( phase_spec, input, result );
    CHECK( first_json == ecology_debug::serialize_watch_json( phase_spec, input, result ) );
    const JsonObject parsed = json_loader::from_string( first_json ).get_object();
    parsed.allow_omitted_members();
    CHECK( parsed.get_string( "preset" ) == "selected_phase_change" );
    CHECK( parsed.get_string( "status" ) == "triggered" );
    CHECK( parsed.get_string( "disposition" ) == "continue_capture" );
}
