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
        make_smoke_only_distant_clue(),
        make_smoke_searchlight_corridor(),
        make_forest_plus_town(),
        make_single_traveler_forest(),
        make_strong_camp_split_route(),
        make_city_edge_moving_hordes(),
        make_generated_smoke_mark_cools_off(),
        make_generated_night_light_mark_scopes_out(),
        make_generated_corridor_mark_refreshes_cleanly(),
        make_generated_confirmed_threat_stays_sticky(),
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
