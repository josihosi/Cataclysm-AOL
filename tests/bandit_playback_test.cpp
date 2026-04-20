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

    REQUIRE( scenarios.size() == 7 );

    const std::vector<std::string> expected_ids = {
        "empty_region",
        "smoke_only_distant_clue",
        "smoke_searchlight_corridor",
        "forest_plus_town",
        "single_traveler_forest",
        "strong_camp_split_route",
        "city_edge_moving_hordes",
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
}
