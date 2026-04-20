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
}

TEST_CASE( "bandit_playback_reference_suite_covers_current_contract", "[bandit][playback]" )
{
    const std::vector<bandit_playback::scenario_definition> &scenarios =
        bandit_playback::reference_scenarios();

    REQUIRE( scenarios.size() == 11 );

    const std::vector<std::string> expected_ids = {
        "empty_region",
        "smoke_only_distant_clue",
        "smoke_searchlight_corridor",
        "forest_plus_town",
        "single_traveler_forest",
        "strong_camp_split_route",
        "city_edge_moving_hordes",
        "generated_smoke_mark_cools_off",
        "generated_night_light_mark_scopes_out",
        "generated_corridor_mark_refreshes_cleanly",
        "generated_confirmed_threat_stays_sticky",
    };

    for( const std::string &id : expected_ids ) {
        INFO( id );
        const bandit_playback::scenario_definition *scenario =
            bandit_playback::find_reference_scenario( id );
        REQUIRE( scenario != nullptr );
        CHECK( scenario->default_checkpoints == std::vector<int>( { 0, 5, 20, 100 } ) );
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

TEST_CASE( "bandit_playback_generated_corridor_refresh_stays_single_mark", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_corridor_mark_refreshes_cleanly" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    REQUIRE( tick20 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::stalk );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::stalk );
    REQUIRE( tick20->generated_marks.marks.size() == 1 );
    CHECK( tick20->generated_marks.marks[0].envelope_id == "searchlight_road" );
    CHECK( tick20->generated_marks.marks[0].confidence >= 2 );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::hold_chill );
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

    REQUIRE( budget.scenarios.size() == 11 );
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
