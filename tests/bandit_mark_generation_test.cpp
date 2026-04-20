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

TEST_CASE( "bandit_mark_generation_light_adapter_keeps_night_leaks_long_range_but_bounded",
           "[bandit][marks]" )
{
    const bandit_mark_generation::light_packet exposed_night_packet = {
        "farm_window_light",
        "farm_window_light",
        "farmstead_edge",
        bandit_dry_run::lead_family::site,
        8,
        2,
        1,
        1,
        bandit_mark_generation::light_time_band::night,
        bandit_mark_generation::light_weather_band::clear,
        bandit_mark_generation::light_exposure_band::exposed,
        bandit_mark_generation::light_source_band::ordinary,
        { "Exposed night light can be worth scoping out from far away." }
    };
    const bandit_mark_generation::light_packet contained_night_packet = {
        "barn_lantern",
        "barn_lantern",
        "farmstead_edge",
        bandit_dry_run::lead_family::site,
        4,
        2,
        1,
        0,
        bandit_mark_generation::light_time_band::night,
        bandit_mark_generation::light_weather_band::clear,
        bandit_mark_generation::light_exposure_band::contained,
        bandit_mark_generation::light_source_band::ordinary,
        { "Contained night light should not become a world-map beacon." }
    };
    const bandit_mark_generation::light_packet daylight_packet = {
        "day_window_glow",
        "day_window_glow",
        "farmstead_edge",
        bandit_dry_run::lead_family::site,
        4,
        2,
        1,
        1,
        bandit_mark_generation::light_time_band::daylight,
        bandit_mark_generation::light_weather_band::clear,
        bandit_mark_generation::light_exposure_band::exposed,
        bandit_mark_generation::light_source_band::ordinary,
        { "Daylight should suppress distant light usefulness." }
    };
    const bandit_mark_generation::light_packet screened_side_packet = {
        "screened_side_light",
        "screened_side_light",
        "farmstead_edge",
        bandit_dry_run::lead_family::site,
        7,
        2,
        1,
        2,
        bandit_mark_generation::light_time_band::night,
        bandit_mark_generation::light_weather_band::clear,
        bandit_mark_generation::light_exposure_band::screened,
        bandit_mark_generation::light_source_band::ordinary,
        { "One side leak should extend the readable range compared to flat screened glow." }
    };
    const bandit_mark_generation::light_packet searchlight_packet = {
        "searchlight_road",
        "searchlight_road",
        "road_corridor",
        bandit_dry_run::lead_family::corridor,
        8,
        2,
        1,
        1,
        bandit_mark_generation::light_time_band::night,
        bandit_mark_generation::light_weather_band::clear,
        bandit_mark_generation::light_exposure_band::exposed,
        bandit_mark_generation::light_source_band::searchlight,
        { "Searchlight exposure should lean threat-first." }
    };

    const bandit_mark_generation::light_projection exposed_projection =
        bandit_mark_generation::adapt_light_packet( exposed_night_packet );
    const bandit_mark_generation::light_projection contained_projection =
        bandit_mark_generation::adapt_light_packet( contained_night_packet );
    const bandit_mark_generation::light_projection daylight_projection =
        bandit_mark_generation::adapt_light_packet( daylight_packet );
    const bandit_mark_generation::light_projection screened_side_projection =
        bandit_mark_generation::adapt_light_packet( screened_side_packet );
    const bandit_mark_generation::light_projection searchlight_projection =
        bandit_mark_generation::adapt_light_packet( searchlight_packet );

    CHECK( exposed_projection.viable );
    CHECK( exposed_projection.projected_range_omt == 9 );
    CHECK( exposed_projection.signal.kind == "light" );
    CHECK( exposed_projection.signal.bounty_add == 1 );
    CHECK( exposed_projection.signal.threat_add == 0 );
    CHECK( exposed_projection.signal.threat_gate_result == bandit_dry_run::threat_gate::discount_only );
    CHECK_FALSE( contained_projection.viable );
    CHECK_FALSE( daylight_projection.viable );
    CHECK( screened_side_projection.viable );
    CHECK( screened_side_projection.projected_range_omt == 8 );
    CHECK( searchlight_projection.viable );
    CHECK( searchlight_projection.projected_range_omt == 11 );
    CHECK( searchlight_projection.signal.kind == "searchlight" );
    CHECK( searchlight_projection.signal.bounty_add == 0 );
    CHECK( searchlight_projection.signal.threat_add == 1 );
    CHECK( searchlight_projection.signal.threat_gate_result == bandit_dry_run::threat_gate::soft_veto );
}

TEST_CASE( "bandit_mark_generation_human_route_adapter_keeps_sightings_mobile_and_routine_suppressed",
           "[bandit][marks]" )
{
    const bandit_mark_generation::human_route_packet direct_sighting_packet = {
        "road_travelers",
        "road_travelers",
        "forest_lane",
        bandit_dry_run::lead_family::site,
        6,
        3,
        0,
        bandit_mark_generation::human_route_kind::direct_sighting,
        bandit_mark_generation::human_route_origin::external,
        bandit_mark_generation::human_route_corroboration::none,
        { "Direct human sighting should stay attached to the travelers." }
    };
    const bandit_mark_generation::human_route_packet same_camp_packet = {
        "camp_errand",
        "camp_errand",
        "home_lane",
        bandit_dry_run::lead_family::corridor,
        4,
        2,
        1,
        bandit_mark_generation::human_route_kind::route_activity,
        bandit_mark_generation::human_route_origin::same_camp,
        bandit_mark_generation::human_route_corroboration::none,
        { "Routine camp traffic must not self-poison into hostile truth." }
    };
    const bandit_mark_generation::human_route_packet shared_route_packet = {
        "shared_supply_track",
        "shared_supply_track",
        "south_road",
        bandit_dry_run::lead_family::site,
        5,
        3,
        1,
        bandit_mark_generation::human_route_kind::route_activity,
        bandit_mark_generation::human_route_origin::shared,
        bandit_mark_generation::human_route_corroboration::corridor,
        { "Shared corroborated traffic should reinforce the corridor without inflating into a site." }
    };
    const bandit_mark_generation::human_route_packet corroborated_site_packet = {
        "trader_stop",
        "trader_stop",
        "market_edge",
        bandit_dry_run::lead_family::site,
        6,
        2,
        2,
        bandit_mark_generation::human_route_kind::route_activity,
        bandit_mark_generation::human_route_origin::external,
        bandit_mark_generation::human_route_corroboration::site,
        { "External corroborated traffic can refresh a bounded site clue." }
    };

    const bandit_mark_generation::human_route_projection direct_projection =
        bandit_mark_generation::adapt_human_route_packet( direct_sighting_packet );
    const bandit_mark_generation::human_route_projection same_camp_projection =
        bandit_mark_generation::adapt_human_route_packet( same_camp_packet );
    const bandit_mark_generation::human_route_projection shared_route_projection =
        bandit_mark_generation::adapt_human_route_packet( shared_route_packet );
    const bandit_mark_generation::human_route_projection corroborated_site_projection =
        bandit_mark_generation::adapt_human_route_packet( corroborated_site_packet );

    CHECK( direct_projection.viable );
    CHECK( direct_projection.projected_range_omt == 8 );
    CHECK( direct_projection.projected_family == bandit_dry_run::lead_family::moving_carrier );
    CHECK( direct_projection.signal.kind == "human_sighting" );
    CHECK( direct_projection.signal.bounty_add == 2 );
    CHECK( direct_projection.signal.confidence == 2 );

    CHECK_FALSE( same_camp_projection.viable );

    CHECK( shared_route_projection.viable );
    CHECK( shared_route_projection.projected_range_omt == 10 );
    CHECK( shared_route_projection.projected_family == bandit_dry_run::lead_family::corridor );
    CHECK( shared_route_projection.signal.kind == "route_activity" );
    CHECK( shared_route_projection.signal.confidence == 2 );

    CHECK( corroborated_site_projection.viable );
    CHECK( corroborated_site_projection.projected_family == bandit_dry_run::lead_family::site );
    CHECK( corroborated_site_projection.signal.bounty_add == 2 );
    CHECK( corroborated_site_projection.signal.hard_blocked_jobs == std::vector<bandit_dry_run::job_template>( {
        bandit_dry_run::job_template::scavenge,
        bandit_dry_run::job_template::steal,
        bandit_dry_run::job_template::raid,
    } ) );
}

TEST_CASE( "bandit_mark_generation_repeated_site_reinforcement_needs_mixed_signals_and_stays_bounded",
           "[bandit][marks]" )
{
    const std::vector<bandit_dry_run::job_template> blocked_extraction_jobs = {
        bandit_dry_run::job_template::scavenge,
        bandit_dry_run::job_template::steal,
        bandit_dry_run::job_template::raid,
    };

    const bandit_mark_generation::signal_input smoke_signal = {
        "farmstead_trace",
        "smoke",
        "farmstead_trace",
        "farmstead_edge",
        bandit_dry_run::lead_family::site,
        1,
        1,
        0,
        1,
        0,
        0,
        0,
        0.60,
        bandit_dry_run::threat_gate::discount_only,
        blocked_extraction_jobs,
        false,
        true,
        { "Smoke at the same site should create only tentative revisit pressure." }
    };
    const bandit_mark_generation::signal_input light_signal = {
        "farmstead_trace",
        "light",
        "farmstead_trace",
        "farmstead_edge",
        bandit_dry_run::lead_family::site,
        1,
        1,
        0,
        1,
        0,
        0,
        0,
        0.65,
        bandit_dry_run::threat_gate::discount_only,
        blocked_extraction_jobs,
        false,
        true,
        { "Night light at the same site should strengthen revisit interest without minting free extraction." }
    };
    const bandit_mark_generation::signal_input route_signal = {
        "farmstead_trace",
        "route_activity",
        "farmstead_trace",
        "farmstead_edge",
        bandit_dry_run::lead_family::site,
        1,
        2,
        0,
        2,
        0,
        0,
        0,
        0.70,
        bandit_dry_run::threat_gate::discount_only,
        blocked_extraction_jobs,
        false,
        true,
        { "Corroborated traffic at the same site should reinforce the same clue, not invent a settlement-signature class." }
    };

    bandit_mark_generation::ledger_state weak_state;
    bandit_mark_generation::advance_state( weak_state, 0,
                                           bandit_mark_generation::cadence_tier::nearby_active,
                                           { smoke_signal } );
    bandit_mark_generation::advance_state( weak_state, 20,
                                           bandit_mark_generation::cadence_tier::nearby_active,
                                           { smoke_signal } );
    REQUIRE( weak_state.marks.size() == 1 );
    CHECK( weak_state.marks[0].repeated_site_reinforcement.total_site_hits == 2 );
    CHECK( weak_state.marks[0].repeated_site_reinforcement.distinct_signal_count == 1 );
    CHECK_FALSE( weak_state.marks[0].repeated_site_reinforcement.viable );
    CHECK( weak_state.marks[0].repeated_site_reinforcement.confidence_bonus == 0 );
    CHECK( weak_state.marks[0].repeated_site_reinforcement.bounty_bonus == 0 );
    CHECK( weak_state.marks[0].bounty_add == 1 );

    bandit_mark_generation::ledger_state mixed_state;
    bandit_mark_generation::advance_state( mixed_state, 0,
                                           bandit_mark_generation::cadence_tier::nearby_active,
                                           { smoke_signal } );
    bandit_mark_generation::advance_state( mixed_state, 20,
                                           bandit_mark_generation::cadence_tier::nearby_active,
                                           { light_signal } );
    REQUIRE( mixed_state.marks.size() == 1 );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.total_site_hits == 2 );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.distinct_signal_count == 2 );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.viable );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.confidence_bonus == 1 );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.bounty_bonus == 1 );
    CHECK( mixed_state.marks[0].bounty_add == 2 );

    bandit_mark_generation::advance_state( mixed_state, 40,
                                           bandit_mark_generation::cadence_tier::nearby_active,
                                           { route_signal } );
    REQUIRE( mixed_state.marks.size() == 1 );
    CHECK( mixed_state.marks[0].kind == "route_activity" );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.total_site_hits == 3 );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.distinct_signal_count == 3 );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.confidence_bonus == 2 );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.bounty_bonus == 2 );
    CHECK( mixed_state.marks[0].bounty_add == 3 );

    const int bounded_confidence_bonus = mixed_state.marks[0].repeated_site_reinforcement.confidence_bonus;
    const int bounded_bounty_bonus = mixed_state.marks[0].repeated_site_reinforcement.bounty_bonus;
    const int bounded_bounty_add = mixed_state.marks[0].bounty_add;
    bandit_mark_generation::advance_state( mixed_state, 60,
                                           bandit_mark_generation::cadence_tier::nearby_active,
                                           { smoke_signal } );
    REQUIRE( mixed_state.marks.size() == 1 );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.total_site_hits == 4 );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.distinct_signal_count == 3 );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.confidence_bonus == bounded_confidence_bonus );
    CHECK( mixed_state.marks[0].repeated_site_reinforcement.bounty_bonus == bounded_bounty_bonus );
    CHECK( mixed_state.marks[0].bounty_add == bounded_bounty_add );

    const std::vector<bandit_dry_run::lead_input> leads = bandit_mark_generation::emit_leads( mixed_state );
    REQUIRE( leads.size() == 1 );
    CHECK( leads[0].hard_blocked_jobs == blocked_extraction_jobs );
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
