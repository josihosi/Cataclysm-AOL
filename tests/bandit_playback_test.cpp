#include "bandit_playback.h"

#include "cata_catch.h"

namespace
{
const bandit_playback::checkpoint_result *find_checkpoint( const bandit_playback::playback_result &result,
        int tick )
{
    for( const bandit_playback::checkpoint_result &checkpoint : result.checkpoints ) {
        if( checkpoint.tick == tick ) {
            return &checkpoint;
        }
    }
    return nullptr;
}

const bandit_dry_run::candidate_debug &winner_at( const bandit_playback::playback_result &result,
        int tick )
{
    const bandit_playback::checkpoint_result *checkpoint = find_checkpoint( result, tick );
    REQUIRE( checkpoint != nullptr );
    return checkpoint->evaluation.candidates[checkpoint->evaluation.winner_index];
}

const bandit_dry_run::candidate_debug *find_candidate( const bandit_dry_run::evaluation_result &result,
        bandit_dry_run::job_template job, const std::string &envelope_id )
{
    for( const bandit_dry_run::candidate_debug &candidate : result.candidates ) {
        if( candidate.job == job && candidate.envelope_id == envelope_id ) {
            return &candidate;
        }
    }
    return nullptr;
}
}

TEST_CASE( "bandit_playback_reference_suite_covers_current_contract", "[bandit][playback]" )
{
    const std::vector<bandit_playback::scenario_definition> &scenarios =
        bandit_playback::reference_scenarios();

    REQUIRE( scenarios.size() == 20 );

    const std::vector<std::string> expected_ids = {
        "empty_region",
        "bounded_explore_frontier_tripwire",
        "smoke_only_distant_clue",
        "smoke_searchlight_corridor",
        "forest_plus_town",
        "single_traveler_forest",
        "strong_camp_split_route",
        "city_edge_moving_hordes",
        "generated_smoke_mark_cools_off",
        "generated_night_light_mark_scopes_out",
        "generated_directional_light_hidden_side_stays_inert",
        "generated_directional_light_visible_side_becomes_actionable",
        "generated_directional_light_corridor_shares_horde_pressure",
        "generated_corridor_mark_refreshes_cleanly",
        "generated_human_sighting_tracks_moving_carrier",
        "generated_shared_route_refresh_stays_corridor",
        "generated_repeated_site_reinforcement_stays_bounded",
        "generated_confirmed_threat_stays_sticky",
        "generated_local_loss_rewrites_corridor_to_withdrawal",
        "generated_local_loss_reroutes_to_safer_detour",
    };

    for( const std::string &id : expected_ids ) {
        INFO( id );
        const bandit_playback::scenario_definition *scenario =
            bandit_playback::find_reference_scenario( id );
        REQUIRE( scenario != nullptr );
        const std::vector<int> expected_checkpoints =
            id.find( "generated_local_loss_" ) == 0 ?
            std::vector<int>( { 0, 20, 100, 500 } ) :
            std::vector<int>( { 0, 5, 20, 100 } );
        CHECK( scenario->default_checkpoints == expected_checkpoints );
        CHECK_FALSE( scenario->questions.empty() );
        CHECK_FALSE( scenario->frames.empty() );
        CHECK( scenario->frames.front().tick == 0 );
    }
}

TEST_CASE( "bandit_playback_empty_region_stays_hold_chill", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "empty_region" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );

    for( int tick : { 0, 5, 20, 100 } ) {
        const bandit_dry_run::candidate_debug &winner = winner_at( result, tick );
        CHECK( winner.job == bandit_dry_run::job_template::hold_chill );
        CHECK( winner.envelope_id.empty() );
    }
}

TEST_CASE( "bandit_playback_bounded_explore_stays_explicit_and_secondary_to_real_leads", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "bounded_explore_frontier_tripwire" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    REQUIRE( tick20 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 0 ).envelope_id == "bounded_explore" );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::scavenge );
    CHECK( winner_at( result, 100 ).envelope_id == "quiet_house" );
    CHECK( find_candidate( tick20->evaluation, bandit_dry_run::job_template::scout,
                           "bounded_explore" ) == nullptr );
    CHECK( scenario->questions[0].find( "Goal:" ) != std::string::npos );
    CHECK( scenario->questions[2].find( "Tuning metrics:" ) != std::string::npos );
}

TEST_CASE( "bandit_playback_smoke_only_clue_gets_investigated_then_cools_off", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "smoke_only_distant_clue" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 5 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
}

TEST_CASE( "bandit_playback_searchlight_corridor_stays_corridor_shaped", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "smoke_searchlight_corridor" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );

    for( int tick : { 0, 5, 20 } ) {
        const bandit_dry_run::candidate_debug &winner = winner_at( result, tick );
        CHECK( winner.job == bandit_dry_run::job_template::stalk );
        CHECK( winner.envelope_id == "searchlight_road" );
        CHECK( winner.family == bandit_dry_run::lead_family::corridor );
    }
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
}

TEST_CASE( "bandit_playback_forest_plus_town_can_pivot_without_inventing_a_raid", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "forest_plus_town" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::scavenge );
    CHECK( winner_at( result, 0 ).envelope_id == "forest_patch" );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 20 ).envelope_id == "town_fringe" );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
}

TEST_CASE( "bandit_playback_single_traveler_stays_attached_to_the_moving_carrier", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "single_traveler_forest" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );

    for( int tick : { 0, 5, 20 } ) {
        const bandit_dry_run::candidate_debug &winner = winner_at( result, tick );
        CHECK( winner.job == bandit_dry_run::job_template::stalk );
        CHECK( winner.envelope_id == "lone_traveler" );
        CHECK( winner.family == bandit_dry_run::lead_family::moving_carrier );
    }
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
}

TEST_CASE( "bandit_playback_prefers_the_weaker_split_route_over_the_strong_camp", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "strong_camp_split_route" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );

    for( int tick : { 0, 5, 20 } ) {
        const bandit_dry_run::candidate_debug &winner = winner_at( result, tick );
        CHECK( winner.job == bandit_dry_run::job_template::stalk );
        CHECK( winner.envelope_id == "split_supply_track" );
    }
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
}

TEST_CASE( "bandit_playback_city_edge_pressure_peels_off_under_growing_hordes", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "city_edge_moving_hordes" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::stalk );
    CHECK( winner_at( result, 5 ).job == bandit_dry_run::job_template::stalk );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
}

TEST_CASE( "bandit_playback_generated_smoke_marks_bridge_into_the_evaluator", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_smoke_mark_cools_off" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );
    const bandit_playback::checkpoint_result *tick0 = find_checkpoint( result, 0 );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    const bandit_playback::checkpoint_result *tick100 = find_checkpoint( result, 100 );
    REQUIRE( tick0 != nullptr );
    REQUIRE( tick20 != nullptr );
    REQUIRE( tick100 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::scout );
    REQUIRE( tick0->generated_leads.size() == 1 );
    CHECK( tick0->generated_leads[0].envelope_id == "ridge_smoke" );
    REQUIRE( tick0->generated_marks.heat.size() == 1 );
    CHECK( tick0->generated_marks.heat[0].region_id == "ridge_rim" );
    REQUIRE_FALSE( tick0->generated_marks.marks.empty() );
    CHECK( tick0->generated_marks.marks[0].notes[1].find( "projected_range_omt=15" ) != std::string::npos );

    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( tick20->generated_leads.empty() );
    CHECK( tick20->generated_marks.marks.empty() );

    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( tick100->generated_marks.marks.empty() );
}

TEST_CASE( "bandit_playback_generated_night_light_marks_bridge_into_the_evaluator", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_night_light_mark_scopes_out" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );
    const bandit_playback::checkpoint_result *tick0 = find_checkpoint( result, 0 );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    REQUIRE( tick0 != nullptr );
    REQUIRE( tick20 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::scout );
    REQUIRE( tick0->generated_leads.size() == 1 );
    CHECK( tick0->generated_leads[0].envelope_id == "farm_window_light" );
    REQUIRE( tick0->generated_marks.marks.size() == 1 );
    CHECK( tick0->generated_marks.marks[0].kind == "light" );
    CHECK( tick0->generated_marks.marks[0].notes[1].find( "projected_range_omt=9" ) != std::string::npos );

    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( tick20->generated_leads.empty() );
}

TEST_CASE( "bandit_playback_generated_directional_light_hidden_side_stays_inert", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_directional_light_hidden_side_stays_inert" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result =
        bandit_playback::run_scenario( *scenario, { 0, 20, 100, 500 } );

    for( int tick : { 0, 20, 100, 500 } ) {
        const bandit_playback::checkpoint_result *checkpoint = find_checkpoint( result, tick );
        REQUIRE( checkpoint != nullptr );
        CHECK( winner_at( result, tick ).job == bandit_dry_run::job_template::hold_chill );
        CHECK( checkpoint->generated_leads.empty() );
        CHECK( checkpoint->generated_marks.marks.empty() );
    }

    CHECK( scenario->questions[0].find( "Goal:" ) != std::string::npos );
    CHECK( scenario->questions[2].find( "Tuning metrics:" ) != std::string::npos );
}

TEST_CASE( "bandit_playback_generated_directional_light_visible_side_becomes_actionable", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_directional_light_visible_side_becomes_actionable" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result =
        bandit_playback::run_scenario( *scenario, { 0, 20, 100, 500 } );
    const bandit_playback::checkpoint_result *tick0 = find_checkpoint( result, 0 );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    const bandit_playback::checkpoint_result *tick100 = find_checkpoint( result, 100 );
    const bandit_playback::checkpoint_result *tick500 = find_checkpoint( result, 500 );
    REQUIRE( tick0 != nullptr );
    REQUIRE( tick20 != nullptr );
    REQUIRE( tick100 != nullptr );
    REQUIRE( tick500 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 0 ).envelope_id == "north_farm_visible_side_light" );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 20 ).envelope_id == "north_farm_visible_side_light" );
    REQUIRE( tick0->generated_marks.marks.size() == 1 );
    CHECK( tick0->generated_marks.marks[0].notes[1].find( "side_leakage=2" ) != std::string::npos );
    REQUIRE( tick20->generated_marks.marks.size() == 1 );
    CHECK( tick20->generated_marks.marks[0].confidence >= 2 );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( tick100->generated_marks.marks.empty() );
    CHECK( winner_at( result, 500 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( tick500->generated_marks.marks.empty() );
}

TEST_CASE( "bandit_playback_generated_directional_light_corridor_shares_horde_pressure", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_directional_light_corridor_shares_horde_pressure" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result =
        bandit_playback::run_scenario( *scenario, { 0, 20, 100, 500 } );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    const bandit_playback::checkpoint_result *tick100 = find_checkpoint( result, 100 );
    const bandit_playback::checkpoint_result *tick500 = find_checkpoint( result, 500 );
    REQUIRE( tick20 != nullptr );
    REQUIRE( tick100 != nullptr );
    REQUIRE( tick500 != nullptr );

    CHECK( winner_at( result, 0 ).envelope_id == "south_visible_corridor_light" );
    CHECK( winner_at( result, 20 ).envelope_id == "south_visible_corridor_light" );
    REQUIRE( tick20->generated_leads.size() == 1 );
    CHECK( tick20->generated_leads[0].family == bandit_dry_run::lead_family::corridor );
    CHECK( tick20->generated_leads[0].monster_pressure_bonus == 2 );
    CHECK( tick20->generated_leads[0].target_coherence_bonus == 1 );
    bool saw_pressure_note = false;
    for( const std::string &note : tick20->generated_leads[0].validity_notes ) {
        if( note.find( "pressure/coherence packet" ) != std::string::npos ) {
            saw_pressure_note = true;
            break;
        }
    }
    CHECK( saw_pressure_note );
    REQUIRE( tick100->generated_marks.marks.size() == 1 );
    CHECK( tick100->generated_marks.marks[0].moving_memory.active );
    CHECK( tick100->generated_marks.marks[0].family == bandit_dry_run::lead_family::corridor );
    CHECK( winner_at( result, 500 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( tick500->generated_leads.empty() );
    REQUIRE( tick500->generated_marks.marks.size() == 1 );
    CHECK_FALSE( tick500->generated_marks.marks[0].moving_memory.active );
    CHECK( tick500->generated_marks.marks[0].family == bandit_dry_run::lead_family::none );
}

TEST_CASE( "bandit_playback_generated_corridor_refresh_stays_single_mark", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_corridor_mark_refreshes_cleanly" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result =
        bandit_playback::run_scenario( *scenario, { 0, 20, 100 } );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    const bandit_playback::checkpoint_result *tick100 = find_checkpoint( result, 100 );
    REQUIRE( tick20 != nullptr );
    REQUIRE( tick100 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::stalk );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::stalk );
    REQUIRE( tick20->generated_marks.marks.size() == 1 );
    CHECK( tick20->generated_marks.marks[0].envelope_id == "searchlight_road" );
    CHECK( tick20->generated_marks.marks[0].confidence >= 2 );
    REQUIRE( tick100->generated_marks.marks.size() == 1 );
    CHECK( tick100->generated_marks.marks[0].moving_memory.active );
    CHECK( tick100->generated_marks.marks[0].moving_memory.source_family ==
           bandit_dry_run::lead_family::corridor );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::stalk );
}

TEST_CASE( "bandit_playback_generated_human_sighting_stays_moving_carrier", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_human_sighting_tracks_moving_carrier" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result =
        bandit_playback::run_scenario( *scenario, { 0, 20, 160 } );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    REQUIRE( tick20 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::stalk );
    CHECK( winner_at( result, 0 ).family == bandit_dry_run::lead_family::moving_carrier );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::stalk );
    REQUIRE( tick20->generated_marks.marks.size() == 1 );
    CHECK( tick20->generated_marks.marks[0].kind == "human_sighting" );
    CHECK( tick20->generated_marks.marks[0].family == bandit_dry_run::lead_family::moving_carrier );
    CHECK( winner_at( result, 160 ).job == bandit_dry_run::job_template::hold_chill );
}

TEST_CASE( "bandit_playback_generated_shared_route_refresh_stays_corridor", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_shared_route_refresh_stays_corridor" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result =
        bandit_playback::run_scenario( *scenario, { 0, 20, 160 } );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    REQUIRE( tick20 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::toll );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::toll );
    REQUIRE( tick20->generated_marks.marks.size() == 1 );
    CHECK( tick20->generated_marks.marks[0].kind == "route_activity" );
    CHECK( tick20->generated_marks.marks[0].family == bandit_dry_run::lead_family::corridor );
    CHECK( winner_at( result, 160 ).job == bandit_dry_run::job_template::hold_chill );
}

TEST_CASE( "bandit_playback_generated_repeated_site_reinforcement_stays_bounded", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_repeated_site_reinforcement_stays_bounded" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result =
        bandit_playback::run_scenario( *scenario, { 0, 20, 40, 100, 500 } );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    const bandit_playback::checkpoint_result *tick40 = find_checkpoint( result, 40 );
    const bandit_playback::checkpoint_result *tick100 = find_checkpoint( result, 100 );
    const bandit_playback::checkpoint_result *tick500 = find_checkpoint( result, 500 );
    REQUIRE( tick20 != nullptr );
    REQUIRE( tick40 != nullptr );
    REQUIRE( tick100 != nullptr );
    REQUIRE( tick500 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 40 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::scout );

    REQUIRE( tick20->generated_marks.marks.size() == 1 );
    CHECK( tick20->generated_marks.marks[0].repeated_site_reinforcement.total_site_hits == 2 );
    CHECK( tick20->generated_marks.marks[0].repeated_site_reinforcement.distinct_signal_count == 2 );
    CHECK( tick20->generated_marks.marks[0].repeated_site_reinforcement.confidence_bonus == 1 );
    CHECK( tick20->generated_marks.marks[0].repeated_site_reinforcement.bounty_bonus == 1 );

    REQUIRE( tick40->generated_marks.marks.size() == 1 );
    CHECK( tick40->generated_marks.marks[0].kind == "route_activity" );
    CHECK( tick40->generated_marks.marks[0].repeated_site_reinforcement.total_site_hits == 3 );
    CHECK( tick40->generated_marks.marks[0].repeated_site_reinforcement.distinct_signal_count == 3 );
    CHECK( tick40->generated_marks.marks[0].repeated_site_reinforcement.confidence_bonus == 2 );
    CHECK( tick40->generated_marks.marks[0].repeated_site_reinforcement.bounty_bonus == 2 );
    REQUIRE( tick40->generated_leads.size() == 1 );
    CHECK( tick40->generated_leads[0].hard_blocked_jobs == std::vector<bandit_dry_run::job_template>( {
        bandit_dry_run::job_template::scavenge,
        bandit_dry_run::job_template::steal,
        bandit_dry_run::job_template::raid,
    } ) );

    REQUIRE( tick100->generated_marks.marks.size() == 1 );
    CHECK( tick100->generated_marks.marks[0].repeated_site_reinforcement.confidence_bonus == 2 );
    CHECK_FALSE( winner_at( result, 100 ).job == bandit_dry_run::job_template::scavenge );
    CHECK_FALSE( winner_at( result, 100 ).job == bandit_dry_run::job_template::steal );
    CHECK_FALSE( winner_at( result, 100 ).job == bandit_dry_run::job_template::raid );

    CHECK( winner_at( result, 500 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( tick500->generated_marks.marks.empty() );
}

TEST_CASE( "bandit_playback_generated_confirmed_threat_stays_visible_in_reports", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_confirmed_threat_stays_sticky" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );
    const bandit_playback::checkpoint_result *tick100 = find_checkpoint( result, 100 );
    REQUIRE( tick100 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
    REQUIRE( tick100->generated_marks.marks.size() == 1 );
    CHECK( tick100->generated_marks.marks[0].confirmed_threat );
    REQUIRE( tick100->generated_marks.heat.size() == 1 );
    CHECK( tick100->generated_marks.heat[0].threat_heat == 3 );
}

TEST_CASE( "bandit_playback_local_loss_can_rewrite_a_corridor_to_withdrawal", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_local_loss_rewrites_corridor_to_withdrawal" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result =
        bandit_playback::run_scenario( *scenario, { 0, 20, 100, 500 } );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    const bandit_playback::checkpoint_result *tick100 = find_checkpoint( result, 100 );
    const bandit_playback::checkpoint_result *tick500 = find_checkpoint( result, 500 );
    REQUIRE( tick20 != nullptr );
    REQUIRE( tick100 != nullptr );
    REQUIRE( tick500 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::stalk );
    CHECK( winner_at( result, 0 ).envelope_id == "granary_road_corridor" );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( result, 500 ).job == bandit_dry_run::job_template::hold_chill );
    const bandit_dry_run::candidate_debug *tick20_stalk = find_candidate( tick20->evaluation,
            bandit_dry_run::job_template::stalk, "granary_road_corridor" );
    REQUIRE( tick20_stalk != nullptr );
    CHECK( tick20_stalk->score.active_pressure_penalty >= 5 );
    CHECK( tick20_stalk->score.final_job_score < 0.0 );
    REQUIRE( tick100->generated_marks.heat.size() == 1 );
    CHECK( tick100->generated_marks.heat[0].threat_heat >= 6 );
    bool saw_tick500_confirmed_threat = false;
    for( const auto &mark : tick500->generated_marks.marks ) {
        if( mark.envelope_id == "granary_loss_site" && mark.confirmed_threat ) {
            saw_tick500_confirmed_threat = true;
            break;
        }
    }
    CHECK( saw_tick500_confirmed_threat );
    CHECK( scenario->questions[0].find( "Goal:" ) != std::string::npos );
    CHECK( scenario->questions[1].find( "Benchmark hook" ) != std::string::npos );
}

TEST_CASE( "bandit_playback_local_loss_can_reroute_to_a_safer_detour", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_local_loss_reroutes_to_safer_detour" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result =
        bandit_playback::run_scenario( *scenario, { 0, 20, 100, 500 } );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    const bandit_playback::checkpoint_result *tick500 = find_checkpoint( result, 500 );
    REQUIRE( tick20 != nullptr );
    REQUIRE( tick500 != nullptr );

    CHECK( winner_at( result, 0 ).envelope_id == "river_bridge_intercept" );
    CHECK( winner_at( result, 20 ).envelope_id == "ridge_detour_watch" );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::toll );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( result, 500 ).job == bandit_dry_run::job_template::hold_chill );
    const bandit_dry_run::candidate_debug *tick20_bridge = find_candidate( tick20->evaluation,
            bandit_dry_run::job_template::stalk, "river_bridge_intercept" );
    REQUIRE( tick20_bridge != nullptr );
    CHECK( tick20_bridge->score.active_pressure_penalty >= 5 );
    CHECK( tick20_bridge->score.final_job_score < 0.0 );
    bool saw_tick500_bridge_loss = false;
    for( const auto &mark : tick500->generated_marks.marks ) {
        if( mark.envelope_id == "river_bridge_loss_site" && mark.confirmed_threat ) {
            saw_tick500_bridge_loss = true;
            break;
        }
    }
    CHECK( saw_tick500_bridge_loss );
    CHECK( scenario->questions[1].find( "Benchmark hook" ) != std::string::npos );
    CHECK( scenario->questions[2].find( "Tuning hook" ) != std::string::npos );
}

TEST_CASE( "bandit_playback_report_renders_named_checkpoints", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "single_traveler_forest" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );
    const std::string report = bandit_playback::render_report( result );

    CHECK( report.find( "bandit playback report" ) != std::string::npos );
    CHECK( report.find( "single_traveler_forest" ) != std::string::npos );
    CHECK( report.find( "tick 0" ) != std::string::npos );
    CHECK( report.find( "tick 20" ) != std::string::npos );
    CHECK( report.find( "tick 100" ) != std::string::npos );
    CHECK( report.find( "winner=stalk @ lone_traveler" ) != std::string::npos );
    CHECK( report.find( "winner=hold / chill" ) != std::string::npos );

    const bandit_playback::scenario_definition *reinforced_scenario =
        bandit_playback::find_reference_scenario( "generated_repeated_site_reinforcement_stays_bounded" );
    REQUIRE( reinforced_scenario != nullptr );
    const std::string reinforced_report = bandit_playback::render_report(
                                              bandit_playback::run_scenario( *reinforced_scenario,
                                                      { 0, 20, 40, 100 } ) );
    CHECK( reinforced_report.find( "repeated_site_reinforcement=hits=3" ) != std::string::npos );
    CHECK( reinforced_report.find( "kinds=smoke; light; route_activity" ) != std::string::npos );
    CHECK( reinforced_report.find( "repeated-site reinforcement packet:" ) != std::string::npos );

    const bandit_playback::scenario_definition *generated_scenario =
        bandit_playback::find_reference_scenario( "generated_confirmed_threat_stays_sticky" );
    REQUIRE( generated_scenario != nullptr );
    const std::string generated_report = bandit_playback::render_report(
                                             bandit_playback::run_scenario( *generated_scenario ) );
    CHECK( generated_report.find( "generated_marks:" ) != std::string::npos );
    CHECK( generated_report.find( "bandit mark-generation report" ) != std::string::npos );
    CHECK( generated_report.find( "fortified_loss_site" ) != std::string::npos );
    CHECK( generated_report.find( "threat_heat=3" ) != std::string::npos );

    const bandit_playback::scenario_definition *smoke_generated_scenario =
        bandit_playback::find_reference_scenario( "generated_smoke_mark_cools_off" );
    REQUIRE( smoke_generated_scenario != nullptr );
    const std::string smoke_report = bandit_playback::render_report(
                                         bandit_playback::run_scenario( *smoke_generated_scenario ) );
    CHECK( smoke_report.find( "projected_range_omt=15" ) != std::string::npos );
    CHECK( smoke_report.find( "weather=clear" ) != std::string::npos );

    const bandit_playback::scenario_definition *light_generated_scenario =
        bandit_playback::find_reference_scenario( "generated_night_light_mark_scopes_out" );
    REQUIRE( light_generated_scenario != nullptr );
    const std::string light_report = bandit_playback::render_report(
                                         bandit_playback::run_scenario( *light_generated_scenario ) );
    CHECK( light_report.find( "kind=light" ) != std::string::npos );
    CHECK( light_report.find( "projected_range_omt=9" ) != std::string::npos );
    CHECK( light_report.find( "time=night" ) != std::string::npos );
    CHECK( light_report.find( "light concealment: verdict=allowed" ) != std::string::npos );
    CHECK( light_report.find( "terrain=open" ) != std::string::npos );
}

TEST_CASE( "bandit_playback_first_500_turn_proof_stays_bounded", "[bandit][playback]" )
{
    const bandit_playback::proof_packet_result proof =
        bandit_playback::run_first_500_turn_playback_proof();
    const std::string report = bandit_playback::render_first_500_turn_playback_proof( proof );

    CHECK( proof.packet_id == "bandit_first_500_turn_playback_proof_v0" );
    CHECK( proof.checkpoints == std::vector<int>( { 0, 20, 100, 500 } ) );
    REQUIRE( proof.scenarios.size() == 3 );

    const bandit_playback::playback_result &smoke = proof.scenarios[0];
    CHECK( smoke.scenario_id == "smoke_only_distant_clue" );
    CHECK( winner_at( smoke, 0 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( smoke, 500 ).job == bandit_dry_run::job_template::hold_chill );

    const bandit_playback::playback_result &city = proof.scenarios[1];
    CHECK( city.scenario_id == "city_edge_moving_hordes" );
    CHECK( winner_at( city, 0 ).job == bandit_dry_run::job_template::stalk );
    CHECK( winner_at( city, 500 ).job == bandit_dry_run::job_template::hold_chill );

    const bandit_playback::playback_result &reinforcement = proof.scenarios[2];
    CHECK( reinforcement.scenario_id == "generated_repeated_site_reinforcement_stays_bounded" );
    CHECK( winner_at( reinforcement, 100 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( reinforcement, 500 ).job == bandit_dry_run::job_template::hold_chill );

    CHECK( report.find( "bandit first 500-turn playback proof" ) != std::string::npos );
    CHECK( report.find( "packet: bandit_first_500_turn_playback_proof_v0" ) != std::string::npos );
    CHECK( report.find( "tick 500" ) != std::string::npos );
    CHECK( report.find( "scenario: smoke_only_distant_clue" ) != std::string::npos );
    CHECK( report.find( "scenario: city_edge_moving_hordes" ) != std::string::npos );
    CHECK( report.find( "scenario: generated_repeated_site_reinforcement_stays_bounded" ) != std::string::npos );
}

TEST_CASE( "bandit_playback_long_range_directional_light_proof_packet_stays_honest", "[bandit][playback]" )
{
    const bandit_playback::proof_packet_result proof =
        bandit_playback::run_long_range_directional_light_proof_packet();
    const std::string report = bandit_playback::render_long_range_directional_light_proof_packet( proof );

    CHECK( proof.packet_id == "bandit_long_range_directional_light_proof_packet_v0" );
    CHECK( proof.checkpoints == std::vector<int>( { 0, 20, 100, 500 } ) );
    REQUIRE( proof.scenarios.size() == 3 );

    const bandit_playback::playback_result &hidden = proof.scenarios[0];
    CHECK( hidden.scenario_id == "generated_directional_light_hidden_side_stays_inert" );
    CHECK( winner_at( hidden, 0 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( hidden, 500 ).job == bandit_dry_run::job_template::hold_chill );

    const bandit_playback::playback_result &visible = proof.scenarios[1];
    CHECK( visible.scenario_id == "generated_directional_light_visible_side_becomes_actionable" );
    CHECK( winner_at( visible, 0 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( visible, 20 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( visible, 100 ).job == bandit_dry_run::job_template::hold_chill );

    const bandit_playback::playback_result &corridor = proof.scenarios[2];
    CHECK( corridor.scenario_id == "generated_directional_light_corridor_shares_horde_pressure" );
    CHECK( winner_at( corridor, 0 ).family == bandit_dry_run::lead_family::corridor );
    CHECK( winner_at( corridor, 100 ).family == bandit_dry_run::lead_family::corridor );
    CHECK( winner_at( corridor, 500 ).job == bandit_dry_run::job_template::hold_chill );

    CHECK( report.find( "bandit long-range directional light proof packet" ) != std::string::npos );
    CHECK( report.find( "packet: bandit_long_range_directional_light_proof_packet_v0" ) != std::string::npos );
    CHECK( report.find( "generated_directional_light_hidden_side_stays_inert" ) != std::string::npos );
    CHECK( report.find( "generated_directional_light_visible_side_becomes_actionable" ) != std::string::npos );
    CHECK( report.find( "generated_directional_light_corridor_shares_horde_pressure" ) != std::string::npos );
    CHECK( report.find( "generated_leads=0" ) != std::string::npos );
    CHECK( report.find( "generated_leads=1" ) != std::string::npos );
    CHECK( report.find( "tick 500" ) != std::string::npos );
}

TEST_CASE( "bandit_playback_overmap_local_pressure_rewrite_packet_sets_up_the_active_scenario_family", "[bandit][playback]" )
{
    const bandit_playback::proof_packet_result proof =
        bandit_playback::run_overmap_local_pressure_rewrite_proof_packet();
    const std::string report = bandit_playback::render_overmap_local_pressure_rewrite_proof_packet( proof );

    CHECK( proof.packet_id == "bandit_overmap_local_pressure_rewrite_proof_packet_v0" );
    CHECK( proof.checkpoints == std::vector<int>( { 0, 20, 100, 500 } ) );
    REQUIRE( proof.scenarios.size() == 2 );

    const bandit_playback::playback_result &withdrawal = proof.scenarios[0];
    CHECK( withdrawal.scenario_id == "generated_local_loss_rewrites_corridor_to_withdrawal" );
    CHECK( winner_at( withdrawal, 0 ).envelope_id == "granary_road_corridor" );
    CHECK( winner_at( withdrawal, 20 ).job == bandit_dry_run::job_template::hold_chill );

    const bandit_playback::playback_result &reroute = proof.scenarios[1];
    CHECK( reroute.scenario_id == "generated_local_loss_reroutes_to_safer_detour" );
    CHECK( winner_at( reroute, 0 ).envelope_id == "river_bridge_intercept" );
    CHECK( winner_at( reroute, 20 ).envelope_id == "ridge_detour_watch" );
    CHECK( winner_at( reroute, 500 ).job == bandit_dry_run::job_template::hold_chill );

    CHECK( report.find( "bandit overmap/local pressure rewrite proof packet" ) != std::string::npos );
    CHECK( report.find( "packet: bandit_overmap_local_pressure_rewrite_proof_packet_v0" ) != std::string::npos );
    CHECK( report.find( "generated_local_loss_rewrites_corridor_to_withdrawal" ) != std::string::npos );
    CHECK( report.find( "generated_local_loss_reroutes_to_safer_detour" ) != std::string::npos );
    CHECK( report.find( "Benchmark hook for later review" ) != std::string::npos );
    CHECK( report.find( "tick 500" ) != std::string::npos );
}

TEST_CASE( "bandit_playback_budget_measurement_exposes_short_vs_long_horizon_churn", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "forest_plus_town" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::scenario_budget budget = bandit_playback::measure_scenario_budget( *scenario, 3,
            { 0, 100 } );

    REQUIRE( budget.checkpoints.size() == 2 );
    CHECK( budget.checkpoints[0].tick == 0 );
    CHECK( budget.checkpoints[0].iterations == 3 );
    CHECK( budget.checkpoints[0].metrics.input_lead_count == 2 );
    CHECK( budget.checkpoints[0].metrics.candidates_generated == 5 );
    CHECK( budget.checkpoints[0].metrics.path_checks == 5 );
    CHECK( budget.checkpoints[0].metrics.winner_comparisons == 5 );
    CHECK( budget.checkpoints[0].checksum != 0 );
    CHECK( budget.checkpoints[1].tick == 100 );
    CHECK( budget.checkpoints[1].metrics.input_lead_count == 0 );
    CHECK( budget.checkpoints[1].metrics.candidates_generated == 0 );
    CHECK( budget.checkpoints[1].metrics.path_checks == 0 );
    CHECK( budget.checkpoints[1].metrics.winner_comparisons == 0 );
}

TEST_CASE( "bandit_playback_reference_suite_budget_reports_perf_churn_and_persistence", "[bandit][playback]" )
{
    const bandit_playback::reference_suite_budget budget =
        bandit_playback::measure_reference_suite_budget( 2 );
    const std::string report = bandit_playback::render_budget_report( budget );

    REQUIRE( budget.scenarios.size() == 20 );
    CHECK( budget.persistence.sample_total_bytes == 512 );
    CHECK( budget.persistence.lines.size() == 6 );
    CHECK( budget.persistence.verdict.find( "cheap enough" ) != std::string::npos );
    CHECK( report.find( "bandit budget report" ) != std::string::npos );
    CHECK( report.find( "smoke_only_distant_clue" ) != std::string::npos );
    CHECK( report.find( "average_runtime_us=" ) != std::string::npos );
    CHECK( report.find( "candidates_generated=" ) != std::string::npos );
    CHECK( report.find( "persistence sample:" ) != std::string::npos );
    CHECK( report.find( "sample_total_bytes=512" ) != std::string::npos );
    CHECK( report.find( "overall_verdict:" ) != std::string::npos );
}
