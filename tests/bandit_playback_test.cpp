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

const bandit_mark_generation::typed_mark *find_generated_mark(
    const bandit_playback::checkpoint_result &checkpoint,
    const std::string &envelope_id )
{
    for( const bandit_mark_generation::typed_mark &mark : checkpoint.generated_marks.marks ) {
        if( mark.envelope_id == envelope_id ) {
            return &mark;
        }
    }
    return nullptr;
}

const bandit_playback::benchmark_scenario_result *find_benchmark_scenario(
    const bandit_playback::benchmark_suite_result &result,
    const std::string &id )
{
    for( const bandit_playback::benchmark_scenario_result &scenario : result.scenarios ) {
        if( scenario.benchmark_id == id ) {
            return &scenario;
        }
    }
    return nullptr;
}

const bandit_playback::benchmark_metric *find_benchmark_metric(
    const bandit_playback::benchmark_scenario_result &scenario,
    const std::string &name )
{
    for( const bandit_playback::benchmark_metric &metric : scenario.metrics ) {
        if( metric.name == name ) {
            return &metric;
        }
    }
    return nullptr;
}
}

TEST_CASE( "bandit_playback_reference_suite_covers_current_contract", "[bandit][playback]" )
{
    const std::vector<bandit_playback::scenario_definition> &scenarios =
        bandit_playback::reference_scenarios();

    REQUIRE( scenarios.size() == 22 );

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

TEST_CASE( "bandit_playback_generated_windy_smoke_marks_report_fuzzy_weather", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_windy_smoke_mark_stays_fuzzy" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );
    const bandit_playback::checkpoint_result *tick0 = find_checkpoint( result, 0 );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    REQUIRE( tick0 != nullptr );
    REQUIRE( tick20 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::scout );
    REQUIRE( tick0->generated_marks.marks.size() == 1 );
    CHECK( tick0->generated_marks.marks[0].kind == "smoke" );
    CHECK( tick0->generated_marks.marks[0].notes[2].find( "weather=windy" ) != std::string::npos );
    CHECK( tick0->generated_marks.marks[0].notes[2].find( "verdict=fuzzed" ) != std::string::npos );
    CHECK( tick0->generated_marks.marks[0].notes[2].find( "origin_hint=drifted" ) != std::string::npos );

    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( tick20->generated_leads.empty() );
}

TEST_CASE( "bandit_playback_generated_portal_storm_light_keeps_exposed_bright_legible", "[bandit][playback]" )
{
    const bandit_playback::scenario_definition *scenario =
        bandit_playback::find_reference_scenario( "generated_portal_storm_exposed_light_stays_legible" );
    REQUIRE( scenario != nullptr );

    const bandit_playback::playback_result result = bandit_playback::run_scenario( *scenario );
    const bandit_playback::checkpoint_result *tick0 = find_checkpoint( result, 0 );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    REQUIRE( tick0 != nullptr );
    REQUIRE( tick20 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::scout );
    REQUIRE( tick0->generated_marks.marks.size() == 1 );
    CHECK( tick0->generated_marks.marks[0].kind == "light" );
    CHECK( tick0->generated_marks.marks[0].notes[2].find( "weather=portal_storm" ) != std::string::npos );
    CHECK( tick0->generated_marks.marks[0].notes[2].find( "storm_bright_light_preserved=yes" ) != std::string::npos );

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
        bandit_playback::run_scenario( *scenario, { 0, 20, 40, 100, 140, 220, 300, 380, 500 } );
    const bandit_playback::checkpoint_result *tick20 = find_checkpoint( result, 20 );
    const bandit_playback::checkpoint_result *tick40 = find_checkpoint( result, 40 );
    const bandit_playback::checkpoint_result *tick100 = find_checkpoint( result, 100 );
    const bandit_playback::checkpoint_result *tick140 = find_checkpoint( result, 140 );
    const bandit_playback::checkpoint_result *tick220 = find_checkpoint( result, 220 );
    const bandit_playback::checkpoint_result *tick300 = find_checkpoint( result, 300 );
    const bandit_playback::checkpoint_result *tick380 = find_checkpoint( result, 380 );
    const bandit_playback::checkpoint_result *tick500 = find_checkpoint( result, 500 );
    REQUIRE( tick20 != nullptr );
    REQUIRE( tick40 != nullptr );
    REQUIRE( tick100 != nullptr );
    REQUIRE( tick140 != nullptr );
    REQUIRE( tick220 != nullptr );
    REQUIRE( tick300 != nullptr );
    REQUIRE( tick380 != nullptr );
    REQUIRE( tick500 != nullptr );

    CHECK( winner_at( result, 0 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 20 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 40 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 100 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 140 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( result, 220 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( result, 300 ).job == bandit_dry_run::job_template::scout );

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
    CHECK_FALSE( winner_at( result, 220 ).job == bandit_dry_run::job_template::scavenge );
    CHECK_FALSE( winner_at( result, 220 ).job == bandit_dry_run::job_template::steal );
    CHECK_FALSE( winner_at( result, 220 ).job == bandit_dry_run::job_template::raid );
    CHECK_FALSE( winner_at( result, 300 ).job == bandit_dry_run::job_template::scavenge );
    CHECK_FALSE( winner_at( result, 300 ).job == bandit_dry_run::job_template::steal );
    CHECK_FALSE( winner_at( result, 300 ).job == bandit_dry_run::job_template::raid );

    CHECK( winner_at( result, 380 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( result, 500 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( tick380->generated_marks.marks.empty() );
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

TEST_CASE( "bandit_playback_elevated_light_and_z_level_visibility_packet_covers_the_active_contract", "[bandit][playback]" )
{
    const bandit_playback::proof_packet_result proof =
        bandit_playback::run_elevated_light_z_level_visibility_packet();
    const std::string report = bandit_playback::render_elevated_light_z_level_visibility_packet( proof );

    CHECK( proof.packet_id == "bandit_elevated_light_z_level_visibility_packet_v0" );
    CHECK( proof.checkpoints == std::vector<int>( { 0, 20, 100, 500 } ) );
    REQUIRE( proof.scenarios.size() == 5 );

    const bandit_playback::playback_result &cross_z = proof.scenarios[0];
    CHECK( cross_z.scenario_id == "generated_nearby_cross_z_light_stays_actionable" );
    CHECK( winner_at( cross_z, 0 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( cross_z, 20 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( cross_z, 100 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( cross_z, 500 ).job == bandit_dry_run::job_template::hold_chill );
    REQUIRE( find_checkpoint( cross_z, 0 ) != nullptr );
    CHECK( report.find( "generated_nearby_cross_z_light_stays_actionable" ) != std::string::npos );
    CHECK( report.find( "nearby_cross_z_visible=yes" ) != std::string::npos );
    CHECK( report.find( "vertical_offset=-1" ) != std::string::npos );

    const bandit_playback::playback_result &hidden = proof.scenarios[1];
    CHECK( hidden.scenario_id == "generated_ground_hidden_light_stays_bounded" );
    CHECK( winner_at( hidden, 0 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( hidden, 20 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( hidden, 500 ).job == bandit_dry_run::job_template::hold_chill );
    REQUIRE( find_checkpoint( hidden, 0 ) != nullptr );
    CHECK( find_checkpoint( hidden, 0 )->generated_leads.empty() );

    const bandit_playback::playback_result &elevated = proof.scenarios[2];
    CHECK( elevated.scenario_id == "generated_elevated_exposed_light_becomes_actionable" );
    CHECK( winner_at( elevated, 0 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( elevated, 20 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( elevated, 100 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( elevated, 500 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( report.find( "generated_elevated_exposed_light_becomes_actionable" ) != std::string::npos );
    CHECK( report.find( "elevated_exposure_extended=yes" ) != std::string::npos );
    CHECK( report.find( "elevation_bonus=2" ) != std::string::npos );

    const bandit_playback::playback_result &radio = proof.scenarios[3];
    CHECK( radio.scenario_id == "generated_radio_tower_fire_shares_horde_pressure" );
    CHECK( winner_at( radio, 0 ).family == bandit_dry_run::lead_family::corridor );
    CHECK( winner_at( radio, 20 ).family == bandit_dry_run::lead_family::corridor );
    CHECK( winner_at( radio, 100 ).family == bandit_dry_run::lead_family::corridor );
    CHECK( winner_at( radio, 500 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( report.find( "generated_radio_tower_fire_shares_horde_pressure" ) != std::string::npos );
    CHECK( report.find( "projected_range_omt=30" ) != std::string::npos );
    CHECK( report.find( "zombie pressure" ) != std::string::npos );

    const bandit_playback::playback_result &smoke = proof.scenarios[4];
    CHECK( smoke.scenario_id == "generated_vertical_smoke_stays_bounded" );
    const bandit_playback::checkpoint_result *smoke_tick0 = find_checkpoint( smoke, 0 );
    REQUIRE( smoke_tick0 != nullptr );
    REQUIRE( smoke_tick0->generated_marks.marks.size() == 2 );
    REQUIRE( smoke_tick0->generated_leads.size() == 2 );
    const bandit_mark_generation::typed_mark *ground_smoke =
        find_generated_mark( *smoke_tick0, "yard_smoke_ground" );
    const bandit_mark_generation::typed_mark *upper_smoke =
        find_generated_mark( *smoke_tick0, "yard_smoke_upper" );
    REQUIRE( ground_smoke != nullptr );
    REQUIRE( upper_smoke != nullptr );
    CHECK( upper_smoke->notes[2].find( "nearby_cross_z_visible=yes" ) != std::string::npos );
    CHECK( upper_smoke->notes[2].find( "vertical_range_bonus=0" ) != std::string::npos );
    CHECK( ground_smoke->notes[2].find( "nearby_cross_z_visible=no" ) != std::string::npos );
    CHECK( report.find( "generated_vertical_smoke_stays_bounded" ) != std::string::npos );
    CHECK( report.find( "vertical_range_bonus=0" ) != std::string::npos );

    CHECK( report.find( "bandit elevated-light and z-level visibility packet" ) != std::string::npos );
    CHECK( report.find( "packet: bandit_elevated_light_z_level_visibility_packet_v0" ) != std::string::npos );
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

TEST_CASE( "bandit_playback_overmap_local_handoff_interaction_packet_keeps_posture_rewrite_and_carryback_readable", "[bandit][playback]" )
{
    const bandit_playback::handoff_packet_result packet =
        bandit_playback::run_overmap_local_handoff_interaction_packet();
    const std::string report = bandit_playback::render_overmap_local_handoff_interaction_packet( packet );

    CHECK( packet.packet_id == "bandit_overmap_local_handoff_interaction_packet_v0" );
    CHECK( packet.checkpoints == std::vector<int>( { 0, 20, 160, 500 } ) );
    REQUIRE( packet.scenarios.size() == 3 );

    const bandit_playback::handoff_packet_scenario_result &smoke = packet.scenarios[0];
    CHECK( smoke.scenario_id == "smoke_scout_narrows_to_cabin_edge" );
    CHECK( smoke.entry.mode == bandit_pursuit_handoff::entry_mode::scout );
    CHECK( smoke.entry.current_target_or_mark == "ridge_smoke" );
    CHECK( smoke.local_return.resolution == bandit_pursuit_handoff::lead_resolution::narrowed );
    CHECK( smoke.returned_group.current_target_or_mark == "ridge_smoke_cabin_edge" );
    CHECK( smoke.returned_group.return_clock == 2 );
    CHECK( winner_at( smoke.playback, 0 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( smoke.playback, 160 ).job == bandit_dry_run::job_template::hold_chill );

    const bandit_playback::handoff_packet_scenario_result &granary = packet.scenarios[1];
    CHECK( granary.scenario_id == "granary_probe_breaks_after_local_danger" );
    CHECK( granary.entry.mode == bandit_pursuit_handoff::entry_mode::probe );
    CHECK( granary.local_return.result == bandit_pursuit_handoff::mission_result::repelled );
    CHECK( granary.returned_group.current_target_or_mark.empty() );
    CHECK( granary.returned_group.current_threat_estimate == 4 );
    CHECK( granary.returned_group.last_return_posture == bandit_pursuit_handoff::return_posture::escape_home );
    CHECK( winner_at( granary.playback, 0 ).envelope_id == "granary_road_corridor" );
    CHECK( winner_at( granary.playback, 20 ).job == bandit_dry_run::job_template::hold_chill );

    const bandit_playback::handoff_packet_scenario_result &follower = packet.scenarios[2];
    CHECK( follower.scenario_id == "player_present_follower_travel_shadow_breaks_on_contact_loss" );
    CHECK( follower.entry.mode == bandit_pursuit_handoff::entry_mode::shadow );
    CHECK( follower.local_return.resolution == bandit_pursuit_handoff::lead_resolution::narrowed );
    CHECK( follower.returned_group.current_target_or_mark == "forest_lane_recent" );
    CHECK( follower.returned_group.last_return_posture == bandit_pursuit_handoff::return_posture::shadow_then_break );
    CHECK( follower.returned_group.return_clock == 1 );
    CHECK( winner_at( follower.playback, 0 ).family == bandit_dry_run::lead_family::moving_carrier );
    CHECK( winner_at( follower.playback, 500 ).job == bandit_dry_run::job_template::hold_chill );

    CHECK( report.find( "bandit overmap/local handoff interaction packet" ) != std::string::npos );
    CHECK( report.find( "packet: bandit_overmap_local_handoff_interaction_packet_v0" ) != std::string::npos );
    CHECK( report.find( "posture_only=yes, exact_square_targeting=no, stale_route_puppetry=no" ) != std::string::npos );
    CHECK( report.find( "scenario: player_present_follower_travel_shadow_breaks_on_contact_loss" ) != std::string::npos );
    CHECK( report.find( "returned_abstract_state:" ) != std::string::npos );
    CHECK( report.find( "ridge_smoke_cabin_edge" ) != std::string::npos );
    CHECK( report.find( "forest_lane_recent" ) != std::string::npos );
    CHECK( report.find( "mission_result=repelled" ) != std::string::npos );
}

TEST_CASE( "bandit_playback_overmap_benchmark_suite_packet_covers_the_active_contract", "[bandit][playback]" )
{
    const bandit_playback::benchmark_suite_result packet =
        bandit_playback::run_overmap_benchmark_suite_packet();
    const std::string report = bandit_playback::render_overmap_benchmark_suite_packet( packet );

    CHECK( packet.packet_id == "bandit_overmap_benchmark_suite_packet_v0" );
    REQUIRE( packet.scenarios.size() == 14 );

    const std::vector<std::string> expected_ids = {
        "empty_frontier_expands_visibility",
        "blocked_route_stays_fail_closed_until_explicit_explore",
        "hidden_side_light_stays_inert",
        "visible_side_light_becomes_actionable",
        "corridor_light_shares_horde_pressure",
        "local_loss_rewrites_to_withdrawal",
        "local_loss_reroutes_to_safer_detour",
        "moving_prey_memory_stays_bounded",
        "repeated_site_interest_stays_bounded",
        "windy_smoke_stays_scoutable_but_fuzzy",
        "portal_storm_smoke_is_harder_to_localize",
        "portal_storm_exposed_light_stays_legible_but_sheltered_light_stays_bounded",
        "independent_camps_do_not_dogpile_by_default",
        "other_camps_read_as_threat_bearing_not_default_allies",
    };

    for( const std::string &id : expected_ids ) {
        INFO( id );
        const bandit_playback::benchmark_scenario_result *scenario = find_benchmark_scenario( packet, id );
        REQUIRE( scenario != nullptr );
        CHECK_FALSE( scenario->metrics.empty() );
        CHECK_FALSE( scenario->benchmark_100.empty() );
        CHECK( report.find( id ) != std::string::npos );
        for( const bandit_playback::benchmark_metric &metric : scenario->metrics ) {
            CHECK( report.find( metric.name + "=" + metric.value ) != std::string::npos );
        }
        for( const bandit_playback::benchmark_check_result &check : scenario->benchmark_100 ) {
            CHECK( check.passed );
        }
        for( const bandit_playback::benchmark_check_result &check : scenario->benchmark_500 ) {
            CHECK( check.passed );
        }
    }

    CHECK( report.find( "bandit overmap benchmark suite packet" ) != std::string::npos );
    CHECK( report.find( "cadence metrics:" ) != std::string::npos );
    CHECK( report.find( "first_non_idle_turn=" ) != std::string::npos );
    CHECK( report.find( "PASS [tick 100]" ) != std::string::npos );
    CHECK( report.find( "500-turn carry-through" ) != std::string::npos );
}

TEST_CASE( "bandit_playback_overmap_benchmark_suite_keeps_empty_frontier_and_blocked_route_honest", "[bandit][playback]" )
{
    const bandit_playback::benchmark_suite_result packet =
        bandit_playback::run_overmap_benchmark_suite_packet();

    const bandit_playback::benchmark_scenario_result *empty_frontier =
        find_benchmark_scenario( packet, "empty_frontier_expands_visibility" );
    const bandit_playback::benchmark_scenario_result *blocked_route =
        find_benchmark_scenario( packet, "blocked_route_stays_fail_closed_until_explicit_explore" );
    REQUIRE( empty_frontier != nullptr );
    REQUIRE( blocked_route != nullptr );

    const bandit_playback::benchmark_metric *empty_first_non_idle =
        find_benchmark_metric( *empty_frontier, "first_non_idle_turn" );
    const bandit_playback::benchmark_metric *empty_frontier_growth =
        find_benchmark_metric( *empty_frontier, "first_frontier_growth_turn" );
    const bandit_playback::benchmark_metric *empty_trip_count =
        find_benchmark_metric( *empty_frontier, "frontier_trip_count_500" );
    const bandit_playback::benchmark_metric *empty_route_flips =
        find_benchmark_metric( *empty_frontier, "route_flip_count_500" );
    const bandit_playback::benchmark_metric *blocked_first_non_idle =
        find_benchmark_metric( *blocked_route, "first_non_idle_turn" );
    const bandit_playback::benchmark_metric *blocked_explore =
        find_benchmark_metric( *blocked_route, "first_explicit_explore_turn" );
    REQUIRE( empty_first_non_idle != nullptr );
    REQUIRE( empty_frontier_growth != nullptr );
    REQUIRE( empty_trip_count != nullptr );
    REQUIRE( empty_route_flips != nullptr );
    REQUIRE( blocked_first_non_idle != nullptr );
    REQUIRE( blocked_explore != nullptr );

    CHECK( empty_first_non_idle->value == "0" );
    CHECK( empty_frontier_growth->value == "20" );
    CHECK( empty_trip_count->value == "1" );
    CHECK( empty_route_flips->value == "0" );
    CHECK( blocked_first_non_idle->value == "100" );
    CHECK( blocked_explore->value == "100" );

    CHECK( winner_at( empty_frontier->playback, 0 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( empty_frontier->playback, 0 ).envelope_id == "bounded_explore" );
    CHECK( winner_at( empty_frontier->playback, 100 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( empty_frontier->playback, 100 ).envelope_id == "bounded_explore" );
    CHECK( winner_at( empty_frontier->playback, 500 ).job == bandit_dry_run::job_template::hold_chill );

    const bandit_playback::checkpoint_result *blocked_tick0 = find_checkpoint( blocked_route->playback, 0 );
    REQUIRE( blocked_tick0 != nullptr );
    CHECK( winner_at( blocked_route->playback, 0 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( find_candidate( blocked_tick0->evaluation, bandit_dry_run::job_template::scout,
                           "bounded_explore" ) == nullptr );
    CHECK( winner_at( blocked_route->playback, 100 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( blocked_route->playback, 100 ).envelope_id == "bounded_explore" );
}

TEST_CASE( "bandit_playback_overmap_benchmark_suite_reports_bounded_repeated_site_revisits", "[bandit][playback]" )
{
    const bandit_playback::benchmark_suite_result packet =
        bandit_playback::run_overmap_benchmark_suite_packet();

    const bandit_playback::benchmark_scenario_result *repeated_site =
        find_benchmark_scenario( packet, "repeated_site_interest_stays_bounded" );
    REQUIRE( repeated_site != nullptr );

    const bandit_playback::benchmark_metric *site_visit_count =
        find_benchmark_metric( *repeated_site, "site_visit_count_500" );
    const bandit_playback::benchmark_metric *site_revisit_count =
        find_benchmark_metric( *repeated_site, "site_revisit_count_500" );
    const bandit_playback::benchmark_metric *cooldown_turn =
        find_benchmark_metric( *repeated_site, "cooldown_turn" );
    const bandit_playback::benchmark_metric *endless_pressure =
        find_benchmark_metric( *repeated_site, "endless_pressure_flag" );
    REQUIRE( site_visit_count != nullptr );
    REQUIRE( site_revisit_count != nullptr );
    REQUIRE( cooldown_turn != nullptr );
    REQUIRE( endless_pressure != nullptr );

    CHECK( site_visit_count->value == "2" );
    CHECK( site_revisit_count->value == "1" );
    CHECK( cooldown_turn->value == "380" );
    CHECK( endless_pressure->value == "false" );

    CHECK( winner_at( repeated_site->playback, 140 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( repeated_site->playback, 220 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( repeated_site->playback, 500 ).job == bandit_dry_run::job_template::hold_chill );
}

TEST_CASE( "bandit_playback_overmap_benchmark_suite_covers_weather_and_independence_edges", "[bandit][playback]" )
{
    const bandit_playback::benchmark_suite_result packet =
        bandit_playback::run_overmap_benchmark_suite_packet();

    const bandit_playback::benchmark_scenario_result *storm_smoke =
        find_benchmark_scenario( packet, "portal_storm_smoke_is_harder_to_localize" );
    const bandit_playback::benchmark_scenario_result *storm_light =
        find_benchmark_scenario( packet, "portal_storm_exposed_light_stays_legible_but_sheltered_light_stays_bounded" );
    const bandit_playback::benchmark_scenario_result *independent =
        find_benchmark_scenario( packet, "independent_camps_do_not_dogpile_by_default" );
    const bandit_playback::benchmark_scenario_result *other_camps =
        find_benchmark_scenario( packet, "other_camps_read_as_threat_bearing_not_default_allies" );
    REQUIRE( storm_smoke != nullptr );
    REQUIRE( storm_light != nullptr );
    REQUIRE( independent != nullptr );
    REQUIRE( other_camps != nullptr );

    const bandit_playback::benchmark_metric *storm_portal_turn =
        find_benchmark_metric( *storm_smoke, "first_portal_storm_turn" );
    const bandit_playback::benchmark_metric *storm_origin_hint_turn =
        find_benchmark_metric( *storm_smoke, "first_corridorish_origin_hint_turn" );
    const bandit_playback::benchmark_metric *storm_light_actionable =
        find_benchmark_metric( *storm_light, "first_actionable_turn" );
    const bandit_playback::benchmark_metric *independent_overlap =
        find_benchmark_metric( *independent, "other_camp_overlap_count_500" );
    const bandit_playback::benchmark_metric *other_camps_ally_flag =
        find_benchmark_metric( *other_camps, "ally_assumption_flag" );
    REQUIRE( storm_portal_turn != nullptr );
    REQUIRE( storm_origin_hint_turn != nullptr );
    REQUIRE( storm_light_actionable != nullptr );
    REQUIRE( independent_overlap != nullptr );
    REQUIRE( other_camps_ally_flag != nullptr );

    CHECK( storm_portal_turn->value == "20" );
    CHECK( storm_origin_hint_turn->value == "20" );
    CHECK( storm_light_actionable->value == "20" );
    CHECK( independent_overlap->value == "0" );
    CHECK( other_camps_ally_flag->value == "false" );

    const bandit_playback::checkpoint_result *storm_smoke_tick20 = find_checkpoint( storm_smoke->playback, 20 );
    const bandit_playback::checkpoint_result *storm_light_tick20 = find_checkpoint( storm_light->playback, 20 );
    REQUIRE( storm_smoke_tick20 != nullptr );
    REQUIRE( storm_light_tick20 != nullptr );

    CHECK( storm_smoke_tick20->generated_marks.marks[0].notes[2].find( "weather=portal_storm" ) != std::string::npos );
    CHECK( storm_smoke_tick20->generated_marks.marks[0].notes[2].find( "origin_hint=corridorish" ) != std::string::npos );
    CHECK( winner_at( storm_light->playback, 0 ).job == bandit_dry_run::job_template::hold_chill );
    CHECK( winner_at( storm_light->playback, 20 ).job == bandit_dry_run::job_template::scout );
    CHECK( winner_at( independent->playback, 0 ).envelope_id == "south_quarry_watch" );
    CHECK( winner_at( independent->playback, 100 ).envelope_id == "south_quarry_watch" );
    CHECK( winner_at( other_camps->playback, 0 ).job == bandit_dry_run::job_template::hold_chill );
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

    REQUIRE( budget.scenarios.size() == 22 );
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
