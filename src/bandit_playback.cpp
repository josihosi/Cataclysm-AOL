#include "bandit_playback.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>

namespace bandit_playback
{
namespace
{
using namespace bandit_dry_run;
using namespace bandit_mark_generation;

camp_input make_camp( int manpower, shortage_band shortage = shortage_band::stable )
{
    camp_input camp;
    camp.available_manpower = manpower;
    camp.shortage = shortage;
    return camp;
}

lead_input make_lead( const std::string &id, const std::string &envelope_id,
                      lead_family family, int bounty, int confidence,
                      double distance_multiplier, int threat_penalty,
                      int active_pressure_penalty = 0,
                      int reward_profile_match = 0,
                      threat_gate gate = threat_gate::discount_only,
                      const std::vector<job_template> &hard_blocked_jobs = {},
                      const std::vector<std::string> &notes = {} )
{
    lead_input lead;
    lead.id = id;
    lead.envelope_id = envelope_id;
    lead.family = family;
    lead.lead_bounty_value = bounty;
    lead.lead_confidence_bonus = confidence;
    lead.distance_multiplier = distance_multiplier;
    lead.threat_penalty = threat_penalty;
    lead.active_pressure_penalty = active_pressure_penalty;
    lead.reward_profile_match = reward_profile_match;
    lead.threat_gate_result = gate;
    lead.hard_blocked_jobs = hard_blocked_jobs;
    lead.validity_notes = notes;
    return lead;
}

signal_input make_signal( const std::string &id, const std::string &kind,
                          const std::string &envelope_id, const std::string &region_id,
                          lead_family family, int strength, int confidence,
                          int threat_add, int bounty_add, double distance_multiplier,
                          int reward_profile_match = 0,
                          threat_gate gate = threat_gate::discount_only,
                          int monster_pressure_add = 0,
                          int target_coherence_subtract = 0,
                          bool confirmed_threat = false,
                          bool soft_decay = true,
                          const std::vector<job_template> &hard_blocked_jobs = {},
                          const std::vector<std::string> &notes = {} )
{
    signal_input signal;
    signal.id = id;
    signal.kind = kind;
    signal.envelope_id = envelope_id;
    signal.region_id = region_id;
    signal.family = family;
    signal.strength = strength;
    signal.confidence = confidence;
    signal.threat_add = threat_add;
    signal.bounty_add = bounty_add;
    signal.reward_profile_match = reward_profile_match;
    signal.distance_multiplier = distance_multiplier;
    signal.threat_gate_result = gate;
    signal.monster_pressure_add = monster_pressure_add;
    signal.target_coherence_subtract = target_coherence_subtract;
    signal.confirmed_threat = confirmed_threat;
    signal.soft_decay = soft_decay;
    signal.hard_blocked_jobs = hard_blocked_jobs;
    signal.notes = notes;
    return signal;
}

smoke_packet make_smoke_packet( const std::string &id, const std::string &envelope_id,
                                const std::string &region_id, int observed_range_omt,
                                int source_strength, int persistence, int height_bias,
                                int spread_bias, smoke_weather_band weather,
                                lead_family family = lead_family::site,
                                const std::vector<std::string> &notes = {} )
{
    smoke_packet packet;
    packet.id = id;
    packet.envelope_id = envelope_id;
    packet.region_id = region_id;
    packet.family = family;
    packet.observed_range_omt = observed_range_omt;
    packet.source_strength = source_strength;
    packet.persistence = persistence;
    packet.height_bias = height_bias;
    packet.spread_bias = spread_bias;
    packet.weather = weather;
    packet.notes = notes;
    return packet;
}

light_packet make_light_packet( const std::string &id, const std::string &envelope_id,
                                const std::string &region_id, int observed_range_omt,
                                int source_strength, int persistence, int side_leakage,
                                light_time_band time, light_weather_band weather,
                                light_exposure_band exposure, light_source_band source,
                                lead_family family = lead_family::site,
                                const std::vector<std::string> &notes = {} )
{
    light_packet packet;
    packet.id = id;
    packet.envelope_id = envelope_id;
    packet.region_id = region_id;
    packet.family = family;
    packet.observed_range_omt = observed_range_omt;
    packet.source_strength = source_strength;
    packet.persistence = persistence;
    packet.side_leakage = side_leakage;
    packet.time = time;
    packet.weather = weather;
    packet.exposure = exposure;
    packet.source = source;
    packet.notes = notes;
    return packet;
}

human_route_packet make_human_route_packet( const std::string &id, const std::string &envelope_id,
        const std::string &region_id, int observed_range_omt,
        int source_strength, int persistence, human_route_kind kind,
        human_route_origin origin, human_route_corroboration corroboration,
        lead_family family = lead_family::corridor,
        const std::vector<std::string> &notes = {} )
{
    human_route_packet packet;
    packet.id = id;
    packet.envelope_id = envelope_id;
    packet.region_id = region_id;
    packet.family = family;
    packet.observed_range_omt = observed_range_omt;
    packet.source_strength = source_strength;
    packet.persistence = persistence;
    packet.kind = kind;
    packet.origin = origin;
    packet.corroboration = corroboration;
    packet.notes = notes;
    return packet;
}

scenario_definition make_empty_region()
{
    scenario_definition scenario;
    scenario.id = "empty_region";
    scenario.title = "Empty region / nothing around";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Do they stay on hold / chill when no real lead exists?"
    };

    scenario.frames.push_back( {
        0,
        "quiet_start",
        make_camp( 2 ),
        {},
        {
            "No real marks, no moving carriers, no corridor pressure.",
            "The only honest candidate should be hold / chill across the whole playback."
        },
        cadence_tier::nearby_active,
        {}
    } );

    return scenario;
}

scenario_definition make_bounded_explore_frontier_tripwire()
{
    scenario_definition scenario;
    scenario.id = "bounded_explore_frontier_tripwire";
    scenario.title = "Bounded explore frontier tripwire";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Goal: explicit bounded scout/explore should beat hold / chill only when a camp deliberately chooses map uncovering.",
        "Goal: blocked routes should stay fail-closed instead of minting accidental random wandering.",
        "Tuning metrics: tick 0 winner = scout on `bounded_explore`, tick 20 winner = hold / chill on a blocked route without greenlight, tick 100 winner = a stronger real reachable lead instead of exploratory drift."
    };

    camp_input quiet_frontier_camp = make_camp( 2 );
    quiet_frontier_camp.allow_bounded_explore = true;
    quiet_frontier_camp.explore_pressure = 2;
    quiet_frontier_camp.explore_distance_multiplier = 0.40;
    scenario.frames.push_back( {
        0,
        "quiet_frontier",
        quiet_frontier_camp,
        {},
        {
            "Goal: explicit frontier uncovering should justify one bounded scout when nothing real beats it.",
            "Tuning metric: the winner should be `scout` on the synthetic `bounded_explore` envelope, not `hold / chill`."
        },
        cadence_tier::nearby_active,
        {}
    } );

    lead_input blocked_route = make_lead( "blocked_bridge_farm", "blocked_bridge_farm",
                                          lead_family::site, 5, 1, 0.80, 1 );
    blocked_route.has_path = false;
    scenario.frames.push_back( {
        20,
        "blocked_route_no_greenlight",
        make_camp( 2 ),
        { blocked_route },
        {
            "Goal: a blocked route without explicit explore greenlight should fail closed instead of inventing wander behavior.",
            "Tuning metric: the winner should collapse to `hold / chill`, and no synthetic explore candidate should appear."
        },
        cadence_tier::nearby_active,
        {}
    } );

    camp_input reachable_lead_camp = make_camp( 2 );
    reachable_lead_camp.allow_bounded_explore = true;
    reachable_lead_camp.explore_pressure = 1;
    reachable_lead_camp.explore_distance_multiplier = 0.30;
    scenario.frames.push_back( {
        100,
        "real_lead_beats_explore",
        reachable_lead_camp,
        {
            make_lead( "quiet_house", "quiet_house", lead_family::site, 5, 1, 0.85, 1,
                       0, 2, threat_gate::discount_only,
                       {},
                       { "A real reachable scoreable lead should outrank exploratory wandering once it exists." } )
        },
        {
            "Goal: explicit explore stays secondary once a stronger real reachable lead shows up.",
            "Tuning metric: the winner should be the real `quiet_house` lead, not the synthetic explore packet."
        },
        cadence_tier::nearby_active,
        {}
    } );

    return scenario;
}

scenario_definition make_smoke_only_distant_clue()
{
    scenario_definition scenario;
    scenario.id = "smoke_only_distant_clue";
    scenario.title = "Smoke-only distant clue";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Do they investigate a thin distant smoke clue, then cool off when the clue loses weight?"
    };

    scenario.frames.push_back( {
        0,
        "fresh_smoke",
        make_camp( 2 ),
        {
            make_lead( "ridge_smoke", "ridge_smoke", lead_family::site, 3, 2, 0.55, 1,
                       0, 0, threat_gate::discount_only,
                       { job_template::scavenge, job_template::steal, job_template::raid },
                       { "Thin smoke is a scouting clue, not instant loot permission." } )
        },
        {
            "Fresh smoke should justify a bounded scout.",
            "Extraction jobs stay blocked because the signal is only smoke."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        20,
        "smoke_thinning",
        make_camp( 2 ),
        {
            make_lead( "ridge_smoke", "ridge_smoke", lead_family::site, 0, 0, 0.55, 1,
                       0, 0, threat_gate::discount_only,
                       { job_template::scavenge, job_template::steal, job_template::raid },
                       { "The same smoke hangs around, but it no longer carries enough confidence to beat hold / chill." } )
        },
        {
            "The camp should stop orbiting a weak stale smoke clue.",
            "This is the cheap passivity check, not a persistence lane."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "smoke_gone",
        make_camp( 2 ),
        {},
        {
            "The smoke has gone cold, so the board should collapse back to hold / chill."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "smoke_stays_cold",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later the stale smoke should still be dead, not quietly regrow pressure."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_smoke_searchlight_corridor()
{
    scenario_definition scenario;
    scenario.id = "smoke_searchlight_corridor";
    scenario.title = "Smoke plus searchlight corridor";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Do they pressure a corridor-shaped clue instead of upgrading the whole area into a fake raid region?"
    };

    scenario.frames.push_back( {
        0,
        "fresh_corridor_contact",
        make_camp( 2 ),
        {
            make_lead( "searchlight_road", "searchlight_road", lead_family::corridor, 4, 2, 0.70, 1,
                       0, 0, threat_gate::soft_veto,
                       { job_template::scout, job_template::toll },
                       { "Smoke plus visible searchlight reads like guarded corridor movement, not a free house raid." } )
        },
        {
            "Soft-veto pressure should keep a marginal stalk alive.",
            "The winner should stay corridor-shaped instead of mutating into a broad site assault."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "corridor_cooled",
        make_camp( 2 ),
        {},
        {
            "Once the corridor clue cools off, the camp should stop posturing and go back to hold / chill."
        },
        cadence_tier::nearby_active,
        {}
    } );

    return scenario;
}

scenario_definition make_forest_plus_town()
{
    scenario_definition scenario;
    scenario.id = "forest_plus_town";
    scenario.title = "Forest plus town";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Do they prefer the easy forest skim first, then pivot to bounded town-edge scouting only when the town clue becomes stronger?"
    };

    scenario.frames.push_back( {
        0,
        "quiet_edges",
        make_camp( 2 ),
        {
            make_lead( "forest_patch", "forest_patch", lead_family::site, 1, 1, 0.95, 0,
                       0, 1, threat_gate::discount_only, {},
                       { "Nearby woods still offer a thin but easy forage-style skim." } ),
            make_lead( "town_fringe", "town_fringe", lead_family::site, 4, 1, 0.45, 3,
                       0, 0, threat_gate::discount_only,
                       { job_template::scavenge, job_template::steal, job_template::raid },
                       { "The town edge is visible, but still too risky and uncertain for more than a future scout." } )
        },
        {
            "At the start, the town should not magically beat the safer nearby forest skim."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        20,
        "town_edge_now_hotter",
        make_camp( 2 ),
        {
            make_lead( "forest_patch", "forest_patch", lead_family::site, 0, 0, 0.95, 0,
                       0, 1, threat_gate::discount_only, {},
                       { "The forest skim has largely played out by now." } ),
            make_lead( "town_fringe", "town_fringe", lead_family::site, 4, 2, 0.75, 2,
                       0, 0, threat_gate::discount_only,
                       { job_template::scavenge, job_template::steal, job_template::raid },
                       { "Fresh town-edge activity makes a bounded scout finally worth it." } )
        },
        {
            "The winner may pivot, but it should still stay a scout and not an invented deep-city raid."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "both_clues_stale",
        make_camp( 2 ),
        {},
        {
            "Long horizon check: no durable fake pressure should remain once both leads cool off."
        },
        cadence_tier::nearby_active,
        {}
    } );

    return scenario;
}

scenario_definition make_single_traveler_forest()
{
    scenario_definition scenario;
    scenario.id = "single_traveler_forest";
    scenario.title = "Single traveler in forest";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Does interest stay attached to the moving carrier instead of upgrading the whole forest into a fake permanent hotspot?"
    };

    scenario.frames.push_back( {
        0,
        "traveler_spotted",
        make_camp( 2 ),
        {
            make_lead( "lone_traveler", "lone_traveler", lead_family::moving_carrier, 3, 2, 0.80, 1,
                       0, 0, threat_gate::discount_only, {},
                       { "A single traveler in forest should stay a moving-carrier problem." } )
        },
        {
            "Stalk should beat hold / chill while the traveler is current and reachable."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        20,
        "traveler_fading",
        make_camp( 2 ),
        {
            make_lead( "lone_traveler", "lone_traveler", lead_family::moving_carrier, 3, 1, 0.35, 1,
                       0, 0, threat_gate::discount_only, {},
                       { "The same moving carrier is farther away now, but the interest should still stay carrier-shaped." } )
        },
        {
            "The winner can stay stalk while the carrier is still real.",
            "The forest itself should not become the envelope."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "traveler_gone",
        make_camp( 2 ),
        {},
        {
            "When the traveler is gone, the camp should not keep treating the forest as a permanent target."
        },
        cadence_tier::nearby_active,
        {}
    } );

    return scenario;
}

scenario_definition make_strong_camp_split_route()
{
    scenario_definition scenario;
    scenario.id = "strong_camp_split_route";
    scenario.title = "Strong camp near weaker split-off route";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Do they prefer the weaker split-off route over a directly visible stronger camp they should not rush head-on?"
    };

    scenario.frames.push_back( {
        0,
        "route_open",
        make_camp( 2 ),
        {
            make_lead( "fortified_camp", "fortified_camp", lead_family::site, 6, 2, 0.70, 4,
                       0, 0, threat_gate::soft_veto,
                       { job_template::scavenge, job_template::steal, job_template::raid },
                       { "The strong camp is real, but only bounded scouting pressure should survive its threat." } ),
            make_lead( "split_supply_track", "split_supply_track", lead_family::corridor, 3, 2, 0.85, 1,
                       0, 0, threat_gate::discount_only,
                       { job_template::scout, job_template::toll },
                       { "A weaker split-off route is the honest pressure seam." } )
        },
        {
            "The split route should win over direct bravado against the strong camp."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "route_closed",
        make_camp( 2 ),
        {
            make_lead( "fortified_camp", "fortified_camp", lead_family::site, 4, 1, 0.60, 4,
                       0, 0, threat_gate::soft_veto,
                       { job_template::scavenge, job_template::steal, job_template::raid },
                       { "Once the weak route dries up, the strong camp alone should not force a fake heroic dispatch." } )
        },
        {
            "Long horizon check: a strong defended camp by itself should collapse back to hold / chill if the good route disappears."
        },
        cadence_tier::nearby_active,
        {}
    } );

    return scenario;
}

scenario_definition make_city_edge_moving_hordes()
{
    scenario_definition scenario;
    scenario.id = "city_edge_moving_hordes";
    scenario.title = "City edge with moving hordes";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Do they stalk the city edge at first, then peel off under growing horde pressure instead of charging deeper in forever?"
    };

    scenario.frames.push_back( {
        0,
        "tense_edge",
        make_camp( 2 ),
        {
            make_lead( "city_edge_skirt", "city_edge_skirt", lead_family::corridor, 4, 2, 0.65, 2,
                       0, 0, threat_gate::soft_veto,
                       { job_template::scout, job_template::toll },
                       { "The only honest move is a marginal edge-stalk while the hordes are still shifting." } )
        },
        {
            "Soft-veto should keep a cautious stalk alive, not a deep raid."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        20,
        "hordes_closing",
        make_camp( 2 ),
        {
            make_lead( "city_edge_skirt", "city_edge_skirt", lead_family::corridor, 4, 1, 0.50, 4,
                       1, 0, threat_gate::hard_veto,
                       { job_template::scout, job_template::toll },
                       { "Growing horde pressure should hard-veto the same edge route instead of creating immortal vendetta behavior." } )
        },
        {
            "This is the peel-off check.",
            "When the pressure grows, the same route should stop winning and fall back to hold / chill."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "edge_abandoned",
        make_camp( 2 ),
        {},
        {
            "Long horizon check: the city edge should stay quiet after the hard-veto retreat."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "edge_stays_abandoned",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later the abandoned city edge should still not regrow a fake vendetta on its own."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_smoke_mark_cools_off()
{
    scenario_definition scenario;
    scenario.id = "generated_smoke_mark_cools_off";
    scenario.title = "Generated smoke mark cools off";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Can the writer-side seam create a smoke mark, feed a scout lead, and then cool it off without hand-authored lead input?"
    };

    scenario.frames.push_back( {
        0,
        "fresh_smoke_mark",
        make_camp( 2 ),
        {},
        {
            "The mark ledger, not a hand-authored lead list, should create the first scout pressure."
        },
        cadence_tier::nearby_active,
        {},
        {
            make_smoke_packet( "ridge_smoke", "ridge_smoke", "ridge_rim", 9, 3, 2, 1, 0,
                               smoke_weather_band::clear,
                               lead_family::site,
                               { "Sustained clear-weather smoke should stay several OMT legible without becoming identity truth." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "smoke_cooled_without_refresh",
        make_camp( 2 ),
        {},
        {
            "A distant inactive pass should cool the weak smoke mark back below hold / chill."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "daily_cleanup",
        make_camp( 2 ),
        {},
        {
            "Daily cleanup should prune the stale smoke clutter instead of letting it live forever."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_night_light_mark_scopes_out()
{
    scenario_definition scenario;
    scenario.id = "generated_night_light_mark_scopes_out";
    scenario.title = "Generated night light mark scopes out";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Can exposed night light create a bounded site scout lead without turning into magical daytime or identity truth?"
    };

    scenario.frames.push_back( {
        0,
        "fresh_night_light",
        make_camp( 2 ),
        {},
        {
            "The writer-side light packet should create the first scout pressure with no hand-authored lead list.",
            "This is ordinary exposed night light, so it stays occupancy-first rather than threat-first."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "farm_window_light", "farm_window_light", "farmstead_edge", 8, 2, 1, 1,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::exposed, light_source_band::ordinary,
                               lead_family::site,
                               { "Exposed night light can be legible from far away without becoming free identity truth." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "night_light_cooled_without_refresh",
        make_camp( 2 ),
        {},
        {
            "A distant inactive pass should cool the ordinary night-light clue back below hold / chill."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "daily_cleanup",
        make_camp( 2 ),
        {},
        {
            "Daily cleanup should prune the stale light clutter instead of keeping it immortal."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_directional_light_hidden_side_stays_inert()
{
    scenario_definition scenario;
    scenario.id = "generated_directional_light_hidden_side_stays_inert";
    scenario.title = "Generated directional light hidden side stays inert";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Goal: a north-side camp whose night light only leaks away from the bandit-facing side should stay non-actionable on the current playback seam.",
        "Benchmark: ticks 0, 20, 100, and 500 should all stay `hold / chill`, with no generated lead or mark surviving from the hidden-side packet.",
        "Tuning metrics: use the same observed range and ordinary night-light footing as the visible-side twin, but keep side leakage at 0 so the projected range stays below the bandit-side read."
    };

    scenario.frames.push_back( {
        0,
        "hidden_side_first_glimpse",
        make_camp( 2 ),
        {},
        {
            "The north camp leaks east, not south toward the bandits, so the bandit-facing read should stay inert.",
            "Reviewer checkpoint: no generated lead should appear even though the broader night-light footing matches the visible-side twin."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "north_farm_hidden_side_light", "north_farm_hidden_side_light",
                               "north_farmstead", 7, 2, 1, 0,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::screened, light_source_band::ordinary,
                               lead_family::site,
                               { "North-side camp leaks east only, so the bandit-facing south read should stay below actionable range." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "hidden_side_recheck",
        make_camp( 2 ),
        {},
        {
            "A second hidden-side packet under the same footing should stay equally inert instead of gradually teaching the bandits psychic vision."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "north_farm_hidden_side_light", "north_farm_hidden_side_light",
                               "north_farmstead", 7, 2, 1, 0,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::screened, light_source_band::ordinary,
                               lead_family::site,
                               { "Repeated east-only leakage should still stay hidden from the bandit-facing side." } )
        }
    } );

    scenario.frames.push_back( {
        60,
        "hidden_side_stays_quiet",
        make_camp( 2 ),
        {},
        {
            "Nothing actionable should have entered the ledger, so the intermediate horizon should stay quiet too."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "hidden_side_cleanup",
        make_camp( 2 ),
        {},
        {
            "Cleanup should remain empty because the hidden-side packet never honestly crossed into the bandit-facing read."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "hidden_side_long_horizon",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later this should still be quiet, because hidden-side leakage is not magical long-range truth."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_directional_light_visible_side_becomes_actionable()
{
    scenario_definition scenario;
    scenario.id = "generated_directional_light_visible_side_becomes_actionable";
    scenario.title = "Generated directional light visible side becomes actionable";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Goal: when the same north-side camp honestly leaks toward the bandit-facing side, the current playback seam should emit a bounded actionable clue.",
        "Benchmark: ticks 0 and 20 should win with `scout` on the generated light envelope, then ticks 100 and 500 should cool back to `hold / chill` once refreshes stop.",
        "Tuning metrics: keep the same observed range, source strength, persistence, and screened exposure as the hidden-side twin, but raise side leakage to 2 so the projected range reaches the bandit-side read."
    };

    scenario.frames.push_back( {
        0,
        "visible_side_first_glimpse",
        make_camp( 2 ),
        {},
        {
            "The only material change from the hidden-side twin is that the light now leaks toward the bandits, so a bounded scout should appear.",
            "Reviewer checkpoint: this should stay occupancy-first ordinary light, not threat-first searchlight panic."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "north_farm_visible_side_light", "north_farm_visible_side_light",
                               "north_farmstead", 7, 2, 1, 2,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::screened, light_source_band::ordinary,
                               lead_family::site,
                               { "The same north camp now leaks toward the bandit-facing side, so the clue should become actionable." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "visible_side_recheck",
        make_camp( 2 ),
        {},
        {
            "A second honest visible-side packet should refresh the same mark instead of inventing a stronger class of magic knowledge."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "north_farm_visible_side_light", "north_farm_visible_side_light",
                               "north_farmstead", 7, 2, 1, 2,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::screened, light_source_band::ordinary,
                               lead_family::site,
                               { "Repeated south-visible leakage should refresh one bounded actionable mark." } )
        }
    } );

    scenario.frames.push_back( {
        60,
        "visible_side_cooling",
        make_camp( 2 ),
        {},
        {
            "Once the visible-side leakage stops refreshing, the clue should cool instead of hardening into immortal omniscience."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "visible_side_cleanup",
        make_camp( 2 ),
        {},
        {
            "Cleanup should prune the ordinary light clue after the refresh window closes."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "visible_side_long_horizon",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later the ordinary light clue should be gone again instead of persisting as a fake permanent target."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_directional_light_corridor_shares_horde_pressure()
{
    scenario_definition scenario;
    scenario.id = "generated_directional_light_corridor_shares_horde_pressure";
    scenario.title = "Generated directional light corridor shares horde pressure";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Goal: the same visible-side directional light should stay on a shared corridor seam where zombie pressure can still shape the outcome, not in private bandit-only theater.",
        "Benchmark: tick 0 should create a corridor action on the generated light envelope, tick 20 should still act on that corridor while carrying shared horde-pressure fields, tick 100 should remain corridor-shaped under moving memory, and tick 500 should cool back to `hold / chill`.",
        "Tuning metrics: keep the visible-side leakage footing, switch the family to corridor, then refresh the same envelope with monster-pressure and coherence pressure so the emitted lead shows shared-world pressure instead of private knowledge."
    };

    scenario.frames.push_back( {
        0,
        "corridor_light_opens",
        make_camp( 2 ),
        {},
        {
            "Visible-side leakage along a road corridor should create bounded corridor pressure, not a site raid fantasy."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "south_visible_corridor_light", "south_visible_corridor_light",
                               "south_road", 7, 2, 1, 2,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::screened, light_source_band::ordinary,
                               lead_family::corridor,
                               { "The same south-visible leakage now sits on a corridor, so the evaluator should treat it as route pressure rather than a site anchor." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "corridor_light_shared_with_horde_pressure",
        make_camp( 2 ),
        {},
        {
            "The corridor stays actionable, but now the same envelope also carries zombie pressure and broken-coherence consequences.",
            "Reviewer checkpoint: the emitted lead should show shared pressure fields rather than a private bandit-only light clue."
        },
        cadence_tier::nearby_active,
        {
            make_signal( "south_visible_corridor_horde_pressure", "route_pressure",
                         "south_visible_corridor_light", "south_road",
                         lead_family::corridor, 0, 0, 0, 0, 0.35,
                         0, threat_gate::discount_only, 2, 1, false, true,
                         {},
                         { "The same lit corridor is also noisy enough to attract zombie pressure, so the packet should stay shared-world rather than private bandit theater." } )
        },
        {},
        {
            make_light_packet( "south_visible_corridor_light", "south_visible_corridor_light",
                               "south_road", 7, 2, 1, 2,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::screened, light_source_band::ordinary,
                               lead_family::corridor,
                               { "Repeated corridor leakage should refresh the same envelope while leaving room for shared zombie pressure to matter." } )
        }
    } );

    scenario.frames.push_back( {
        100,
        "corridor_memory_holds_shape",
        make_camp( 2 ),
        {},
        {
            "Moving-memory persistence should keep the corridor clue shaped like a route for a while instead of collapsing instantly or inflating into a site."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "corridor_long_horizon_cools",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later the shared corridor clue should finally cool off instead of living forever."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_corridor_mark_refreshes_cleanly()
{
    scenario_definition scenario;
    scenario.id = "generated_corridor_mark_refreshes_cleanly";
    scenario.title = "Generated corridor mark refreshes cleanly";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Can repeated searchlight evidence refresh one mark cleanly and keep it corridor-shaped instead of writing ghost duplicates?"
    };

    scenario.frames.push_back( {
        0,
        "fresh_corridor_mark",
        make_camp( 2 ),
        {},
        {
            "The first searchlight corridor mark should yield a bounded stalk, not a site raid."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "searchlight_road", "searchlight_road", "road_corridor", 8, 2, 1, 1,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::exposed, light_source_band::searchlight,
                               lead_family::corridor,
                               { "Searchlight exposure should stay corridor-pressure evidence, not a free extraction lane." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "corridor_refreshed",
        make_camp( 2 ),
        {},
        {
            "Refresh should keep the winner corridor-shaped and strengthen confidence modestly."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "searchlight_road", "searchlight_road", "road_corridor", 8, 2, 1, 1,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::exposed, light_source_band::searchlight,
                               lead_family::corridor,
                               { "Repeated matching searchlight evidence should refresh the same corridor mark instead of cloning it." } )
        }
    } );

    scenario.frames.push_back( {
        60,
        "corridor_cooling",
        make_camp( 2 ),
        {},
        {
            "A later distant inactive pass should cool the corridor read instead of keeping it immortal."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "corridor_pruned",
        make_camp( 2 ),
        {},
        {
            "Without more corroboration, the corridor mark should eventually collapse back to quiet."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_human_sighting_tracks_moving_carrier()
{
    scenario_definition scenario;
    scenario.id = "generated_human_sighting_tracks_moving_carrier";
    scenario.title = "Generated human sighting tracks moving carrier";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Can a direct human sighting generate moving-carrier pressure without inflating into magical site truth?"
    };

    scenario.frames.push_back( {
        0,
        "fresh_human_sighting",
        make_camp( 2 ),
        {},
        {
            "The writer-side route packet should create a moving-carrier lead with no hand-authored lead list.",
            "A direct sighting stays mobile bounty pressure first, even if the caller asked for a broader site family."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {},
        {
            make_human_route_packet( "road_travelers", "road_travelers", "forest_lane", 6, 3, 0,
                                     human_route_kind::direct_sighting,
                                     human_route_origin::external,
                                     human_route_corroboration::none,
                                     lead_family::site,
                                     { "Direct human sighting should stay attached to the travelers instead of upgrading the whole lane." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "sighting_refreshed",
        make_camp( 2 ),
        {},
        {
            "A second matching sighting should refresh the same moving-carrier mark instead of inventing a site anchor."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {},
        {
            make_human_route_packet( "road_travelers", "road_travelers", "forest_lane", 6, 3, 0,
                                     human_route_kind::direct_sighting,
                                     human_route_origin::external,
                                     human_route_corroboration::none,
                                     lead_family::site,
                                     { "Repeated direct sighting should keep the clue mobile, not site-shaped." } )
        }
    } );

    scenario.frames.push_back( {
        60,
        "sighting_cooling",
        make_camp( 2 ),
        {},
        {
            "A later distant inactive pass should cool the moving-carrier clue instead of leaving it immortal."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "sighting_cleanup_started",
        make_camp( 2 ),
        {},
        {
            "The first cleanup pass should start pruning the stale moving-carrier clue."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        160,
        "sighting_pruned",
        make_camp( 2 ),
        {},
        {
            "Without fresh sightings, the moving-carrier clue should eventually cool back to quiet."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_shared_route_refresh_stays_corridor()
{
    scenario_definition scenario;
    scenario.id = "generated_shared_route_refresh_stays_corridor";
    scenario.title = "Generated shared route refresh stays corridor";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Can shared corroborated route activity refresh one corridor mark cleanly without inflating into a site?"
    };

    scenario.frames.push_back( {
        0,
        "fresh_shared_route",
        make_camp( 2 ),
        {},
        {
            "Shared corridor activity should create bounded corridor pressure, not a magical settlement site."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {},
        {
            make_human_route_packet( "shared_supply_track", "shared_supply_track", "south_road", 5, 3, 1,
                                     human_route_kind::route_activity,
                                     human_route_origin::shared,
                                     human_route_corroboration::corridor,
                                     lead_family::site,
                                     { "Shared traffic with corridor corroboration should reinforce the road, not promote the whole site." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "shared_route_refreshed",
        make_camp( 2 ),
        {},
        {
            "Refresh should keep one corridor mark alive and modestly stronger instead of cloning or site-inflating it."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {},
        {
            make_human_route_packet( "shared_supply_track", "shared_supply_track", "south_road", 5, 3, 1,
                                     human_route_kind::route_activity,
                                     human_route_origin::shared,
                                     human_route_corroboration::corridor,
                                     lead_family::site,
                                     { "Repeated shared-route evidence should refresh the same corridor mark." } )
        }
    } );

    scenario.frames.push_back( {
        60,
        "shared_route_cooling",
        make_camp( 2 ),
        {},
        {
            "A later distant inactive pass should cool the shared-route clue instead of leaving it immortal."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "shared_route_cleanup_started",
        make_camp( 2 ),
        {},
        {
            "The first cleanup pass should start pruning the stale shared-route clue."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        160,
        "shared_route_pruned",
        make_camp( 2 ),
        {},
        {
            "Without more corroboration, the shared-route clue should still collapse back to quiet."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_repeated_site_reinforcement_stays_bounded()
{
    scenario_definition scenario;
    scenario.id = "generated_repeated_site_reinforcement_stays_bounded";
    scenario.title = "Generated repeated site reinforcement stays bounded";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Can mixed repeated site-centered activity strengthen one site mark cleanly without unlocking free extraction jobs or a magical settlement-signature class?"
    };

    scenario.frames.push_back( {
        0,
        "first_smoke_trace",
        make_camp( 2 ),
        {},
        {
            "A first smoke trace should create only tentative scout pressure on the site-shaped mark."
        },
        cadence_tier::nearby_active,
        {},
        {
            make_smoke_packet( "farmstead_trace", "farmstead_trace", "farmstead_edge", 9, 3, 2, 1, 0,
                               smoke_weather_band::clear,
                               lead_family::site,
                               { "One smoke trace alone is only a tentative site clue." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "night_light_confirms_site",
        make_camp( 2 ),
        {},
        {
            "A second light clue at the same site should reinforce one mark instead of cloning a new one or unlocking extraction."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "farmstead_trace", "farmstead_trace", "farmstead_edge", 8, 2, 1, 1,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::exposed, light_source_band::ordinary,
                               lead_family::site,
                               { "Night light at the same site should reinforce revisit value first." } )
        }
    } );

    scenario.frames.push_back( {
        40,
        "route_activity_reinforces_same_site",
        make_camp( 2 ),
        {},
        {
            "Corroborated traffic at the same site can strengthen the same mark again, but it should still stay bounded scout pressure."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {},
        {
            make_human_route_packet( "farmstead_trace", "farmstead_trace", "farmstead_edge", 5, 2, 1,
                                     human_route_kind::route_activity,
                                     human_route_origin::external,
                                     human_route_corroboration::site,
                                     lead_family::site,
                                     { "Mixed repeated site activity should strengthen revisit interest, not mint free extraction truth." } )
        }
    } );

    scenario.frames.push_back( {
        100,
        "bounded_reinforcement_survives_cleanup",
        make_camp( 2 ),
        {},
        {
            "Even after a cleanup pass the reinforced site should stay a bounded scout clue, not a free raid warrant."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        140,
        "reinforcement_no_new_contact_1",
        make_camp( 2 ),
        {},
        {
            "Once corroboration stops, the reinforced site should start cooling on ordinary inactive cadence instead of staying magically hot."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        220,
        "reinforcement_no_new_contact_2",
        make_camp( 2 ),
        {},
        {
            "The same site should keep losing pressure when nothing fresh happens there."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        300,
        "reinforcement_no_new_contact_3",
        make_camp( 2 ),
        {},
        {
            "Repeated-site interest should keep draining on long idle horizons instead of becoming immortal scout pressure."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        380,
        "reinforcement_no_new_contact_4",
        make_camp( 2 ),
        {},
        {
            "Still no new corroboration, so the same clue should be close to dead by now."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "reinforcement_finally_cools",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later the reinforced site should have cooled back out instead of becoming immortal scout pressure."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_confirmed_threat_stays_sticky()
{
    scenario_definition scenario;
    scenario.id = "generated_confirmed_threat_stays_sticky";
    scenario.title = "Generated confirmed threat stays sticky";
    scenario.default_checkpoints = { 0, 5, 20, 100 };
    scenario.questions = {
        "Does confirmed threat remain on the overmap ledger across cleanup passes instead of passively melting away?"
    };

    scenario.frames.push_back( {
        0,
        "loss_site_written",
        make_camp( 2 ),
        {},
        {
            "The generated lead should be present but hard-vetoed, leaving hold / chill as the winner."
        },
        cadence_tier::nearby_active,
        {
            make_signal( "fortified_loss_site", "loss_site", "fortified_loss_site", "city_edge",
                         lead_family::site, 1, 2, 3, 0, 0.60,
                         0, threat_gate::hard_veto, 0, 0, true, true,
                         { job_template::scavenge, job_template::steal, job_template::raid },
                         { "A confirmed ugly loss should freeze as threat until later real observation rewrites it." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "threat_not_forgotten",
        make_camp( 2 ),
        {},
        {
            "A distant inactive pass should not remote-rewrite the scary read into safety."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "daily_cleanup_keeps_threat",
        make_camp( 2 ),
        {},
        {
            "Daily cleanup should still keep the confirmed threat mark alive."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_local_loss_rewrites_corridor_to_withdrawal()
{
    scenario_definition scenario;
    scenario.id = "generated_local_loss_rewrites_corridor_to_withdrawal";
    scenario.title = "Generated local loss rewrites corridor to withdrawal";
    scenario.default_checkpoints = { 0, 20, 100, 500 };
    scenario.questions = {
        "Goal: a plausible corridor stalk should withdraw once the same region gets a confirmed local loss that makes the tile much hotter than the old overmap read.",
        "Benchmark hook for later review: tick 0 should still pressure `granary_road_corridor`, while ticks 20, 100, and 500 should stay off that stale route after the loss signal lands.",
        "Tuning hook: Josef and Schani still need to lock the exact scenario-specific benchmark line later; for now the suite should expose winner drift, generated heat, and active-pressure penalties reviewer-cleanly."
    };

    scenario.frames.push_back( {
        0,
        "granary_route_plausible",
        make_camp( 2 ),
        {},
        {
            "The old overmap read still looks barely worth stalking, so a marginal corridor posture is allowed to exist before the local rewrite arrives."
        },
        cadence_tier::nearby_active,
        {
            make_signal( "granary_route_activity", "route_activity",
                         "granary_road_corridor", "granary_edge",
                         lead_family::corridor, 4, 2, 1, 0, 0.65,
                         0, threat_gate::soft_veto, 0, 0, false, true,
                         { job_template::scout, job_template::toll },
                         { "A still-plausible corridor read exists before the camp learns the granary edge has gone hot." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "granary_local_loss_turns_tile_hot",
        make_camp( 2 ),
        {},
        {
            "Local contact rewrites the same region as ugly enough that the old corridor posture should collapse instead of homing forever.",
            "Reviewer checkpoint: the old corridor lead may still exist in generated output, but active regional pressure should keep it from winning."
        },
        cadence_tier::nearby_active,
        {
            make_signal( "granary_loss_site", "loss_site", "granary_loss_site", "granary_edge",
                         lead_family::site, 1, 2, 6, 0, 0.60,
                         0, threat_gate::hard_veto, 0, 0, true, true,
                         { job_template::scavenge, job_template::steal, job_template::raid },
                         { "Fresh local loss says the granary edge is hotter than the old overmap stalk read." } )
        }
    } );

    scenario.frames.push_back( {
        100,
        "granary_route_stays_abandoned",
        make_camp( 2 ),
        {},
        {
            "At the short aftermath horizon the stale corridor should still stay abandoned while the scary read remains sticky."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "granary_long_horizon_stays_off",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later the packet should still show no forced return to the stale granary route."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_local_loss_reroutes_to_safer_detour()
{
    scenario_definition scenario;
    scenario.id = "generated_local_loss_reroutes_to_safer_detour";
    scenario.title = "Generated local loss reroutes to safer detour";
    scenario.default_checkpoints = { 0, 20, 100, 500 };
    scenario.questions = {
        "Goal: when one intercept corridor becomes locally too dangerous, the same outing should be able to reroute toward a narrower safer detour instead of pretending stale intent is destiny.",
        "Benchmark hook for later review: tick 0 should act on `river_bridge_intercept`, tick 20 should switch away from that envelope onto `ridge_detour_watch`, and the later horizons should not drift back into the burned bridge route on their own.",
        "Tuning hook: the exact winner spread still needs Josef and Schani to lock the scenario-specific benchmark later, but the setup should already expose the old corridor's pressure penalty beside the safer reroute candidate."
    };

    scenario.frames.push_back( {
        0,
        "bridge_intercept_open",
        make_camp( 2 ),
        {},
        {
            "Before the local rewrite, the bridge intercept still looks like the honest pressure seam."
        },
        cadence_tier::nearby_active,
        {
            make_signal( "river_bridge_route_activity", "route_activity",
                         "river_bridge_intercept", "river_bridge",
                         lead_family::corridor, 4, 2, 1, 0, 0.70,
                         0, threat_gate::soft_veto, 0, 0, false, true,
                         {},
                         { "The bridge corridor is initially plausible enough to justify a bounded intercept posture." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "bridge_loss_promotes_detour",
        make_camp( 2 ),
        {
            make_lead( "ridge_detour_watch", "ridge_detour_watch", lead_family::corridor, 3, 1, 0.75, 0,
                       0, 0, threat_gate::discount_only,
                       {},
                       { "A ridge detour stays cooler than the burned bridge and offers a bounded reroute instead of blind persistence." } )
        },
        {
            "The bridge route should stop winning once the same region gets a confirmed ugly loss, and the safer detour should become the honest outlet.",
            "Reviewer checkpoint: this is a reroute proof, not a claim that every hot route should collapse straight to hold / chill."
        },
        cadence_tier::nearby_active,
        {
            make_signal( "river_bridge_loss_site", "loss_site", "river_bridge_loss_site", "river_bridge",
                         lead_family::site, 1, 2, 6, 0, 0.60,
                         0, threat_gate::hard_veto, 0, 0, true, true,
                         { job_template::scavenge, job_template::steal, job_template::raid },
                         { "Local contact says the bridge is now hotter than the old intercept plan admitted." } )
        }
    } );

    scenario.frames.push_back( {
        100,
        "detour_cools_without_bridge_relapse",
        make_camp( 2 ),
        {},
        {
            "After the short reroute window, the camp should be free to cool off instead of snapping back to the burned bridge route."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "bridge_remains_non_destiny",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later the packet should still not regrow the stale bridge intercept as if nothing happened."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

const scenario_frame &frame_for_tick( const scenario_definition &scenario, int tick )
{
    const scenario_frame *active = &scenario.frames.front();
    for( const scenario_frame &frame : scenario.frames ) {
        if( frame.tick > tick ) {
            break;
        }
        active = &frame;
    }
    return *active;
}

std::vector<int> normalized_checkpoints( const scenario_definition &scenario,
        const std::vector<int> &requested_checkpoints )
{
    std::vector<int> checkpoints = requested_checkpoints.empty() ? scenario.default_checkpoints : requested_checkpoints;
    std::sort( checkpoints.begin(), checkpoints.end() );
    checkpoints.erase( std::unique( checkpoints.begin(), checkpoints.end() ), checkpoints.end() );
    return checkpoints;
}

checkpoint_result build_checkpoint( const scenario_definition &scenario, int tick )
{
    checkpoint_result checkpoint;
    checkpoint.tick = tick;

    ledger_state generated_marks;
    for( const scenario_frame &frame : scenario.frames ) {
        if( frame.tick > tick ) {
            break;
        }
        std::vector<signal_input> signals = frame.mark_signals;
        for( const smoke_packet &packet : frame.smoke_packets ) {
            const smoke_projection projection = adapt_smoke_packet( packet );
            if( projection.viable ) {
                signals.push_back( projection.signal );
            }
        }
        for( const light_packet &packet : frame.light_packets ) {
            const light_projection projection = adapt_light_packet( packet );
            if( projection.viable ) {
                signals.push_back( projection.signal );
            }
        }
        for( const human_route_packet &packet : frame.human_route_packets ) {
            const human_route_projection projection = adapt_human_route_packet( packet );
            if( projection.viable ) {
                signals.push_back( projection.signal );
            }
        }
        advance_state( generated_marks, frame.tick, frame.cadence, signals );
    }

    const scenario_frame &frame = frame_for_tick( scenario, tick );
    checkpoint.phase = frame.phase;
    checkpoint.notes = frame.notes;
    checkpoint.generated_marks = generated_marks;
    checkpoint.generated_leads = emit_leads( checkpoint.generated_marks );

    std::vector<lead_input> leads = checkpoint.generated_leads;
    leads.insert( leads.end(), frame.leads.begin(), frame.leads.end() );
    checkpoint.evaluation = bandit_dry_run::evaluate( frame.camp, leads );
    return checkpoint;
}

const candidate_debug &winner_candidate( const evaluation_result &evaluation )
{
    return evaluation.candidates[evaluation.winner_index];
}

size_t hash_combine( size_t seed, size_t value )
{
    return seed ^ ( value + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 ) );
}

size_t evaluation_checksum( const evaluation_result &evaluation )
{
    size_t seed = 0;
    seed = hash_combine( seed, evaluation.winner_index );
    seed = hash_combine( seed, evaluation.leads.size() );
    seed = hash_combine( seed, evaluation.candidates.size() );
    seed = hash_combine( seed, evaluation.metrics.input_lead_count );
    seed = hash_combine( seed, evaluation.metrics.candidates_generated );
    seed = hash_combine( seed, evaluation.metrics.path_checks );

    for( const candidate_debug &candidate : evaluation.candidates ) {
        seed = hash_combine( seed, static_cast<size_t>( candidate.job ) );
        seed = hash_combine( seed, static_cast<size_t>( candidate.family ) );
        seed = hash_combine( seed, candidate.valid ? 1 : 0 );
        seed = hash_combine( seed, candidate.winner ? 1 : 0 );
        seed = hash_combine( seed, std::hash<std::string>()( candidate.envelope_id ) );
        seed = hash_combine( seed, static_cast<size_t>( std::llround( candidate.score.final_job_score * 100.0 ) ) );
    }

    return seed;
}

void add_persistence_line( persistence_budget &budget, const std::string &label,
                           size_t count, size_t bytes, const std::string &notes )
{
    persistence_budget_line line;
    line.label = label;
    line.count = count;
    line.bytes = bytes;
    line.notes = notes;
    budget.sample_total_bytes += bytes;
    budget.lines.push_back( line );
}

std::string classify_budget_verdict( const reference_suite_budget &result )
{
    long long max_average_runtime_us = 0;
    size_t max_candidates_generated = 0;
    size_t max_path_checks = 0;

    for( const scenario_budget &scenario : result.scenarios ) {
        for( const checkpoint_budget &checkpoint : scenario.checkpoints ) {
            max_average_runtime_us = std::max<long long>( max_average_runtime_us,
                                     static_cast<long long>( std::llround( checkpoint.average_runtime_us ) ) );
            max_candidates_generated = std::max( max_candidates_generated,
                                                 checkpoint.metrics.candidates_generated );
            max_path_checks = std::max( max_path_checks, checkpoint.metrics.path_checks );
        }
    }

    if( result.persistence.sample_total_bytes > 2048 ||
        max_candidates_generated > 16 ||
        max_path_checks > 16 ||
        max_average_runtime_us > 5000 ) {
        return "suspicious: the bounded v0 seam is already bigger or slower than it should be, so broader rollout should wait for cleanup";
    }

    return "cheap enough: the bounded v0 seam stays tiny across the reference suite, and the first persistence sample remains well below obvious save-bloat territory";
}
} // namespace

const std::vector<scenario_definition> &reference_scenarios()
{
    static const std::vector<scenario_definition> scenarios = {
        make_empty_region(),
        make_bounded_explore_frontier_tripwire(),
        make_smoke_only_distant_clue(),
        make_smoke_searchlight_corridor(),
        make_forest_plus_town(),
        make_single_traveler_forest(),
        make_strong_camp_split_route(),
        make_city_edge_moving_hordes(),
        make_generated_smoke_mark_cools_off(),
        make_generated_night_light_mark_scopes_out(),
        make_generated_directional_light_hidden_side_stays_inert(),
        make_generated_directional_light_visible_side_becomes_actionable(),
        make_generated_directional_light_corridor_shares_horde_pressure(),
        make_generated_corridor_mark_refreshes_cleanly(),
        make_generated_human_sighting_tracks_moving_carrier(),
        make_generated_shared_route_refresh_stays_corridor(),
        make_generated_repeated_site_reinforcement_stays_bounded(),
        make_generated_confirmed_threat_stays_sticky(),
        make_generated_local_loss_rewrites_corridor_to_withdrawal(),
        make_generated_local_loss_reroutes_to_safer_detour(),
    };
    return scenarios;
}

const scenario_definition *find_reference_scenario( const std::string &id )
{
    const std::vector<scenario_definition> &scenarios = reference_scenarios();
    const auto it = std::find_if( scenarios.begin(), scenarios.end(), [&id]( const scenario_definition &scenario ) {
        return scenario.id == id;
    } );
    return it == scenarios.end() ? nullptr : &*it;
}

playback_result run_scenario( const scenario_definition &scenario,
                              const std::vector<int> &checkpoints )
{
    playback_result result;
    result.scenario_id = scenario.id;
    result.title = scenario.title;
    result.questions = scenario.questions;

    for( int tick : normalized_checkpoints( scenario, checkpoints ) ) {
        result.checkpoints.push_back( build_checkpoint( scenario, tick ) );
    }

    return result;
}

proof_packet_result run_first_500_turn_playback_proof()
{
    static const std::vector<int> proof_checkpoints = { 0, 20, 100, 500 };
    static const std::vector<std::string> proof_scenarios = {
        "smoke_only_distant_clue",
        "city_edge_moving_hordes",
        "generated_repeated_site_reinforcement_stays_bounded",
    };

    proof_packet_result result;
    result.packet_id = "bandit_first_500_turn_playback_proof_v0";
    result.checkpoints = proof_checkpoints;

    for( const std::string &scenario_id : proof_scenarios ) {
        const scenario_definition *scenario = find_reference_scenario( scenario_id );
        if( scenario != nullptr ) {
            result.scenarios.push_back( run_scenario( *scenario, proof_checkpoints ) );
        }
    }

    return result;
}

proof_packet_result run_long_range_directional_light_proof_packet()
{
    static const std::vector<int> proof_checkpoints = { 0, 20, 100, 500 };
    static const std::vector<std::string> proof_scenarios = {
        "generated_directional_light_hidden_side_stays_inert",
        "generated_directional_light_visible_side_becomes_actionable",
        "generated_directional_light_corridor_shares_horde_pressure",
    };

    proof_packet_result result;
    result.packet_id = "bandit_long_range_directional_light_proof_packet_v0";
    result.checkpoints = proof_checkpoints;

    for( const std::string &scenario_id : proof_scenarios ) {
        const scenario_definition *scenario = find_reference_scenario( scenario_id );
        if( scenario != nullptr ) {
            result.scenarios.push_back( run_scenario( *scenario, proof_checkpoints ) );
        }
    }

    return result;
}

proof_packet_result run_overmap_local_pressure_rewrite_proof_packet()
{
    static const std::vector<int> proof_checkpoints = { 0, 20, 100, 500 };
    static const std::vector<std::string> proof_scenarios = {
        "generated_local_loss_rewrites_corridor_to_withdrawal",
        "generated_local_loss_reroutes_to_safer_detour",
    };

    proof_packet_result result;
    result.packet_id = "bandit_overmap_local_pressure_rewrite_proof_packet_v0";
    result.checkpoints = proof_checkpoints;

    for( const std::string &scenario_id : proof_scenarios ) {
        const scenario_definition *scenario = find_reference_scenario( scenario_id );
        if( scenario != nullptr ) {
            result.scenarios.push_back( run_scenario( *scenario, proof_checkpoints ) );
        }
    }

    return result;
}

scenario_budget measure_scenario_budget( const scenario_definition &scenario,
        size_t iterations_per_checkpoint, const std::vector<int> &checkpoints )
{
    scenario_budget result;
    result.scenario_id = scenario.id;
    result.title = scenario.title;
    result.questions = scenario.questions;

    const size_t iterations = std::max<size_t>( iterations_per_checkpoint, 1 );

    for( int tick : normalized_checkpoints( scenario, checkpoints ) ) {
        const scenario_frame &frame = frame_for_tick( scenario, tick );
        checkpoint_budget checkpoint;
        checkpoint.tick = tick;
        checkpoint.phase = frame.phase;
        checkpoint.iterations = iterations;

        for( size_t iteration = 0; iteration < iterations; ++iteration ) {
            const auto started = std::chrono::steady_clock::now();
            const checkpoint_result evaluated_checkpoint = build_checkpoint( scenario, tick );
            const auto finished = std::chrono::steady_clock::now();
            const long long runtime_us = std::chrono::duration_cast<std::chrono::microseconds>( finished - started ).count();

            checkpoint.total_runtime_us += runtime_us;
            checkpoint.metrics = evaluated_checkpoint.evaluation.metrics;
            checkpoint.checksum = hash_combine( checkpoint.checksum,
                                                evaluation_checksum( evaluated_checkpoint.evaluation ) );
        }

        checkpoint.average_runtime_us = static_cast<double>( checkpoint.total_runtime_us ) /
                                        static_cast<double>( iterations );
        result.checkpoints.push_back( checkpoint );
    }

    return result;
}

persistence_budget estimate_v0_persistence_budget()
{
    persistence_budget result;
    result.sample_shape = "1 camp ledger + 8 marks + 1 active abstract outing + 2 anchored identities + 1 bubble seam key";
    result.assumptions = {
        "Approximate payload bytes for the bounded abstract state only, not exact serializer or JSON text overhead.",
        "Compact ids, enums, bands, and small state flags are assumed for the persisted shape.",
        "Exact loaded-bubble NPC truth, old candidate boards, and already-consumed return deltas stay out of this budget."
    };

    add_persistence_line( result, "camp ledger", 1, 64,
                          "camp id, home region, stockpile buckets, shortage band, manpower state, and dispatch cooldown/load" );
    add_persistence_line( result, "source-shaped mark ledger", 8, 256,
                          "eight mixed site/corridor/actor/loss/route-blocked style marks at about 32 bytes each" );
    add_persistence_line( result, "active abstract outing ledger", 1, 104,
                          "group id, owning camp, current job/lead, mission posture, survivor count, cargo profile, burden, travel credit, and return pressure" );
    add_persistence_line( result, "anchored identity slice", 2, 48,
                          "two anchored identities at about 24 bytes each for status and join continuity" );
    add_persistence_line( result, "camp/group hard links", 1, 24,
                          "ownership, reserve commitment, and pending return-writeback linkage" );
    add_persistence_line( result, "bubble seam key", 1, 16,
                          "minimal group/camp/mission key plus bubble-owned or return-pending flag" );

    if( result.sample_total_bytes > 2048 ) {
        result.verdict = "suspicious: even the bounded abstract sample is already large, so the schema shape should be trimmed before wider rollout";
    } else {
        result.verdict = "cheap enough: the bounded abstract sample stays around half a kilobyte before serializer overhead, so only duplicated tactical truth or historical deltas look dangerous";
    }

    return result;
}

reference_suite_budget measure_reference_suite_budget( size_t iterations_per_checkpoint )
{
    reference_suite_budget result;
    for( const scenario_definition &scenario : reference_scenarios() ) {
        result.scenarios.push_back( measure_scenario_budget( scenario, iterations_per_checkpoint ) );
    }
    result.persistence = estimate_v0_persistence_budget();
    return result;
}

std::string render_report( const playback_result &result )
{
    std::ostringstream out;
    out << "bandit playback report\n";
    out << "scenario: " << result.scenario_id << " - " << result.title << "\n";
    out << "questions:\n";
    if( result.questions.empty() ) {
        out << "- none\n";
    } else {
        for( const std::string &question : result.questions ) {
            out << "- " << question << "\n";
        }
    }

    out << "checkpoints:\n";
    for( const checkpoint_result &checkpoint : result.checkpoints ) {
        const candidate_debug &winner = winner_candidate( checkpoint.evaluation );
        out << "- tick " << checkpoint.tick << " [phase=" << checkpoint.phase << "] winner="
            << bandit_dry_run::to_string( winner.job );
        if( !winner.envelope_id.empty() ) {
            out << " @ " << winner.envelope_id;
        }
        out << "\n";
        for( const std::string &note : checkpoint.notes ) {
            out << "  note: " << note << "\n";
        }
        if( !checkpoint.generated_marks.marks.empty() || !checkpoint.generated_marks.heat.empty() ) {
            out << "  generated_marks:\n";
            const std::string mark_report = bandit_mark_generation::render_report( checkpoint.generated_marks,
                                            &checkpoint.generated_leads );
            std::istringstream report_stream( mark_report );
            std::string line;
            while( std::getline( report_stream, line ) ) {
                out << "    " << line << "\n";
            }
        }
        out << "  reason: " << checkpoint.evaluation.winner_reason << "\n";
    }

    return out.str();
}

std::string render_first_500_turn_playback_proof( const proof_packet_result &result )
{
    std::ostringstream out;
    out << "bandit first 500-turn playback proof\n";
    out << "packet: " << result.packet_id << "\n";
    out << "checkpoints:";
    for( int tick : result.checkpoints ) {
        out << " " << tick;
    }
    out << "\n";

    for( const playback_result &scenario : result.scenarios ) {
        out << "scenario: " << scenario.scenario_id << " - " << scenario.title << "\n";
        for( const checkpoint_result &checkpoint : scenario.checkpoints ) {
            const candidate_debug &winner = winner_candidate( checkpoint.evaluation );
            out << "- tick " << checkpoint.tick << " [phase=" << checkpoint.phase << "] winner="
                << bandit_dry_run::to_string( winner.job );
            if( !winner.envelope_id.empty() ) {
                out << " @ " << winner.envelope_id;
            }
            out << "\n";
        }
    }

    return out.str();
}

std::string render_long_range_directional_light_proof_packet( const proof_packet_result &result )
{
    std::ostringstream out;
    out << "bandit long-range directional light proof packet\n";
    out << "packet: " << result.packet_id << "\n";
    out << "checkpoints:";
    for( int tick : result.checkpoints ) {
        out << " " << tick;
    }
    out << "\n";

    for( const playback_result &scenario : result.scenarios ) {
        out << "scenario: " << scenario.scenario_id << " - " << scenario.title << "\n";
        for( const std::string &question : scenario.questions ) {
            out << "  - " << question << "\n";
        }
        for( const checkpoint_result &checkpoint : scenario.checkpoints ) {
            const candidate_debug &winner = winner_candidate( checkpoint.evaluation );
            out << "  - tick " << checkpoint.tick << " [phase=" << checkpoint.phase << "] winner="
                << bandit_dry_run::to_string( winner.job );
            if( !winner.envelope_id.empty() ) {
                out << " @ " << winner.envelope_id;
            }
            out << ", generated_leads=" << checkpoint.generated_leads.size()
                << ", generated_marks=" << checkpoint.generated_marks.marks.size()
                << ", reason=" << checkpoint.evaluation.winner_reason << "\n";
        }
    }

    return out.str();
}

std::string render_overmap_local_pressure_rewrite_proof_packet( const proof_packet_result &result )
{
    std::ostringstream out;
    out << "bandit overmap/local pressure rewrite proof packet\n";
    out << "packet: " << result.packet_id << "\n";
    out << "checkpoints:";
    for( int tick : result.checkpoints ) {
        out << " " << tick;
    }
    out << "\n";

    for( const playback_result &scenario : result.scenarios ) {
        out << "scenario: " << scenario.scenario_id << " - " << scenario.title << "\n";
        for( const std::string &question : scenario.questions ) {
            out << "  - " << question << "\n";
        }
        for( const checkpoint_result &checkpoint : scenario.checkpoints ) {
            const candidate_debug &winner = winner_candidate( checkpoint.evaluation );
            out << "  - tick " << checkpoint.tick << " [phase=" << checkpoint.phase << "] winner="
                << bandit_dry_run::to_string( winner.job );
            if( !winner.envelope_id.empty() ) {
                out << " @ " << winner.envelope_id;
            }
            out << ", generated_leads=" << checkpoint.generated_leads.size()
                << ", generated_marks=" << checkpoint.generated_marks.marks.size()
                << ", reason=" << checkpoint.evaluation.winner_reason << "\n";
        }
    }

    return out.str();
}

std::string render_budget_report( const reference_suite_budget &result )
{
    std::ostringstream out;
    out << "bandit budget report\n";
    out << "scenarios:\n";
    for( const scenario_budget &scenario : result.scenarios ) {
        out << "- " << scenario.scenario_id << " - " << scenario.title << "\n";
        for( const checkpoint_budget &checkpoint : scenario.checkpoints ) {
            out << "  - tick " << checkpoint.tick << " [phase=" << checkpoint.phase << "] iterations="
                << checkpoint.iterations << ", total_runtime_us=" << checkpoint.total_runtime_us
                << ", average_runtime_us=" << std::fixed << std::setprecision( 2 )
                << checkpoint.average_runtime_us << ", checksum=" << checkpoint.checksum << "\n";
            out << "    churn: input_leads=" << checkpoint.metrics.input_lead_count
                << ", accepted_leads=" << checkpoint.metrics.accepted_lead_count
                << ", candidates_generated=" << checkpoint.metrics.candidates_generated
                << ", score_evaluations=" << checkpoint.metrics.score_evaluations
                << ", path_checks=" << checkpoint.metrics.path_checks
                << ", winner_comparisons=" << checkpoint.metrics.winner_comparisons << "\n";
            out << "    waste guards: deduped=" << checkpoint.metrics.deduped_lead_count
                << ", hard_blocked=" << checkpoint.metrics.hard_blocked_job_count
                << ", manpower_rejections=" << checkpoint.metrics.manpower_rejection_count
                << ", invalid_outward=" << checkpoint.metrics.invalid_outward_candidates << "\n";
        }
    }

    out << "persistence sample:\n";
    out << "- shape: " << result.persistence.sample_shape << "\n";
    for( const persistence_budget_line &line : result.persistence.lines ) {
        out << "  - " << line.label << ": count=" << line.count << ", bytes=" << line.bytes
            << " (" << line.notes << ")\n";
    }
    out << "- sample_total_bytes=" << result.persistence.sample_total_bytes << "\n";
    out << "- assumptions:\n";
    for( const std::string &assumption : result.persistence.assumptions ) {
        out << "  - " << assumption << "\n";
    }
    out << "- persistence_verdict: " << result.persistence.verdict << "\n";
    out << "overall_verdict:\n- " << classify_budget_verdict( result ) << "\n";

    return out.str();
}
} // namespace bandit_playback
