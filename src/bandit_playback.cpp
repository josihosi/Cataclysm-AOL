#include "bandit_playback.h"

#include <algorithm>
#include <sstream>

namespace bandit_playback
{
namespace
{
using namespace bandit_dry_run;

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
        }
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
        }
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
        }
    } );

    scenario.frames.push_back( {
        100,
        "smoke_gone",
        make_camp( 2 ),
        {},
        {
            "The smoke has gone cold, so the board should collapse back to hold / chill."
        }
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
        }
    } );

    scenario.frames.push_back( {
        100,
        "corridor_cooled",
        make_camp( 2 ),
        {},
        {
            "Once the corridor clue cools off, the camp should stop posturing and go back to hold / chill."
        }
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
        }
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
        }
    } );

    scenario.frames.push_back( {
        100,
        "both_clues_stale",
        make_camp( 2 ),
        {},
        {
            "Long horizon check: no durable fake pressure should remain once both leads cool off."
        }
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
        }
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
        }
    } );

    scenario.frames.push_back( {
        100,
        "traveler_gone",
        make_camp( 2 ),
        {},
        {
            "When the traveler is gone, the camp should not keep treating the forest as a permanent target."
        }
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
        }
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
        }
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
        }
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
        }
    } );

    scenario.frames.push_back( {
        100,
        "edge_abandoned",
        make_camp( 2 ),
        {},
        {
            "Long horizon check: the city edge should stay quiet after the hard-veto retreat."
        }
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

const candidate_debug &winner_candidate( const evaluation_result &evaluation )
{
    return evaluation.candidates[evaluation.winner_index];
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
        const scenario_frame &frame = frame_for_tick( scenario, tick );
        checkpoint_result checkpoint;
        checkpoint.tick = tick;
        checkpoint.phase = frame.phase;
        checkpoint.notes = frame.notes;
        checkpoint.evaluation = bandit_dry_run::evaluate( frame.camp, frame.leads );
        result.checkpoints.push_back( checkpoint );
    }

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
        out << "  reason: " << checkpoint.evaluation.winner_reason << "\n";
    }

    return out.str();
}
} // namespace bandit_playback
