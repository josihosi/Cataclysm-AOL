#include "bandit_mark_generation.h"

#include "cata_catch.h"

TEST_CASE( "bandit_mark_generation_refresh_and_cooling_are_tiered", "[bandit][marks]" )
{
    bandit_mark_generation::ledger_state state;

    const bandit_mark_generation::signal_input smoke_signal = {
        "ridge_smoke",
        "smoke",
        "ridge_smoke",
        "ridge_rim",
        bandit_dry_run::lead_family::site,
        1,
        1,
        1,
        1,
        0,
        0,
        0,
        0.55,
        bandit_dry_run::threat_gate::discount_only,
        { bandit_dry_run::job_template::scavenge, bandit_dry_run::job_template::steal,
          bandit_dry_run::job_template::raid },
        false,
        true,
        { "Thin smoke should stay a scouting clue." }
    };

    const bandit_mark_generation::update_result created =
        bandit_mark_generation::advance_state( state, 0,
                bandit_mark_generation::cadence_tier::nearby_active, { smoke_signal } );
    REQUIRE( state.marks.size() == 1 );
    REQUIRE( state.heat.size() == 1 );
    CHECK( created.metrics.marks_created == 1 );
    CHECK( created.leads.size() == 1 );
    CHECK( created.leads[0].envelope_id == "ridge_smoke" );
    CHECK( state.heat[0].threat_heat == 1 );
    CHECK( state.heat[0].bounty_heat == 1 );

    const int created_strength = state.marks[0].strength;
    const int created_confidence = state.marks[0].confidence;

    const bandit_mark_generation::update_result refreshed =
        bandit_mark_generation::advance_state( state, 20,
                bandit_mark_generation::cadence_tier::nearby_active, { smoke_signal } );
    REQUIRE( state.marks.size() == 1 );
    CHECK( refreshed.metrics.marks_refreshed == 1 );
    CHECK( state.marks[0].strength > created_strength );
    CHECK( state.marks[0].confidence > created_confidence );

    const int refreshed_strength = state.marks[0].strength;
    const int refreshed_confidence = state.marks[0].confidence;

    const bandit_mark_generation::update_result cooled =
        bandit_mark_generation::advance_state( state, 40,
                bandit_mark_generation::cadence_tier::distant_inactive, {} );
    REQUIRE( state.marks.size() == 1 );
    CHECK( cooled.metrics.marks_cooled >= 1 );
    CHECK( state.marks[0].strength < refreshed_strength );
    CHECK( state.marks[0].confidence < refreshed_confidence );

    const bandit_mark_generation::update_result cleaned =
        bandit_mark_generation::advance_state( state, 100,
                bandit_mark_generation::cadence_tier::daily_cleanup, {} );
    CHECK( cleaned.metrics.marks_pruned == 1 );
    CHECK( state.marks.empty() );
    CHECK( state.heat.empty() );
    CHECK( cleaned.leads.empty() );
}

TEST_CASE( "bandit_mark_generation_smoke_adapter_keeps_clear_plumes_long_range_but_bounded",
           "[bandit][marks]" )
{
    const bandit_mark_generation::smoke_packet clear_packet = {
        "ridge_smoke",
        "ridge_smoke",
        "ridge_rim",
        bandit_dry_run::lead_family::site,
        12,
        3,
        2,
        1,
        0,
        bandit_mark_generation::smoke_weather_band::clear,
        { "Clear sustained smoke should stay several OMT legible." }
    };
    const bandit_mark_generation::smoke_packet fogged_packet = {
        "ridge_smoke_fog",
        "ridge_smoke_fog",
        "ridge_rim",
        bandit_dry_run::lead_family::site,
        6,
        1,
        0,
        0,
        1,
        bandit_mark_generation::smoke_weather_band::fog,
        { "Weak fogged smoke should not pretend to be long-range truth." }
    };

    const bandit_mark_generation::smoke_projection clear_projection =
        bandit_mark_generation::adapt_smoke_packet( clear_packet );
    const bandit_mark_generation::smoke_projection fogged_projection =
        bandit_mark_generation::adapt_smoke_packet( fogged_packet );

    CHECK( clear_projection.viable );
    CHECK( clear_projection.projected_range_omt == 15 );
    CHECK( clear_projection.signal.kind == "smoke" );
    CHECK( clear_projection.signal.confidence == 1 );
    CHECK( clear_projection.signal.bounty_add == 1 );
    CHECK( clear_projection.signal.distance_multiplier >= 0.25 );
    CHECK( clear_projection.signal.distance_multiplier < 1.0 );
    CHECK( clear_projection.signal.hard_blocked_jobs == std::vector<bandit_dry_run::job_template>( {
        bandit_dry_run::job_template::scavenge,
        bandit_dry_run::job_template::steal,
        bandit_dry_run::job_template::raid,
    } ) );
    CHECK_FALSE( fogged_projection.viable );
    CHECK( fogged_projection.projected_range_omt < clear_projection.projected_range_omt );
}

TEST_CASE( "bandit_mark_generation_keeps_confirmed_threat_sticky", "[bandit][marks]" )
{
    bandit_mark_generation::ledger_state state;

    const bandit_mark_generation::signal_input confirmed_loss = {
        "fortified_loss_site",
        "loss_site",
        "fortified_loss_site",
        "city_edge",
        bandit_dry_run::lead_family::site,
        1,
        2,
        3,
        0,
        0,
        0,
        0,
        0.60,
        bandit_dry_run::threat_gate::hard_veto,
        { bandit_dry_run::job_template::scavenge, bandit_dry_run::job_template::steal,
          bandit_dry_run::job_template::raid },
        true,
        true,
        { "Confirmed ugly loss should freeze until a later real rewrite." }
    };

    bandit_mark_generation::advance_state( state, 0,
                                           bandit_mark_generation::cadence_tier::nearby_active,
                                           { confirmed_loss } );
    REQUIRE( state.marks.size() == 1 );
    const int original_threat = state.marks[0].threat_add;
    REQUIRE( state.heat.size() == 1 );
    const int original_heat = state.heat[0].threat_heat;

    const bandit_mark_generation::update_result cleaned =
        bandit_mark_generation::advance_state( state, 100,
                bandit_mark_generation::cadence_tier::daily_cleanup, {} );

    REQUIRE( state.marks.size() == 1 );
    REQUIRE( state.heat.size() == 1 );
    CHECK( cleaned.metrics.sticky_threat_preserved >= 1 );
    CHECK( state.marks[0].confirmed_threat );
    CHECK( state.marks[0].threat_add == original_threat );
    CHECK( state.heat[0].threat_heat == original_heat );
    REQUIRE( cleaned.leads.size() == 1 );
    CHECK( cleaned.leads[0].envelope_id == "fortified_loss_site" );
    CHECK( cleaned.leads[0].threat_gate_result == bandit_dry_run::threat_gate::hard_veto );
}
