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

struct handoff_case_definition {
    std::string scenario_id;
    std::string title;
    std::string playback_scenario_id;
    std::vector<std::string> questions;
    bandit_pursuit_handoff::abstract_group_state group;
    int entry_tick = 0;
    bandit_pursuit_handoff::entry_context entry_context;
    bandit_pursuit_handoff::local_outcome outcome;
};

bandit_pursuit_handoff::cargo_profile make_cargo( int food, int meds, int ammo,
        int gear = 0, int fuel = 0, int trade_goods = 0 )
{
    bandit_pursuit_handoff::cargo_profile cargo;
    cargo.food = food;
    cargo.meds = meds;
    cargo.ammo = ammo;
    cargo.gear = gear;
    cargo.fuel = fuel;
    cargo.trade_goods = trade_goods;
    return cargo;
}

bandit_pursuit_handoff::abstract_group_state make_handoff_group( const std::string &group_id,
        const std::string &source_camp_id,
        const std::string &target,
        int strength,
        int confidence,
        int threat,
        int bounty,
        int return_clock,
        int retreat_bias = 0,
        int mission_urgency = 1 )
{
    bandit_pursuit_handoff::abstract_group_state group;
    group.group_id = group_id;
    group.source_camp_id = source_camp_id;
    group.group_strength = strength;
    group.confidence = confidence;
    group.panic_threshold = 2;
    group.cargo_capacity = 2;
    group.current_target_or_mark = target;
    group.current_threat_estimate = threat;
    group.current_bounty_estimate = bounty;
    group.mission_urgency = mission_urgency;
    group.retreat_bias = retreat_bias;
    group.goal_stickiness = 2;
    group.goal_preemption_posture = 1;
    group.return_clock = return_clock;
    group.anchored_identities.push_back( { group_id + "_lead", "alive" } );
    group.known_recent_marks.push_back( target );
    return group;
}

const checkpoint_result *find_playback_checkpoint( const playback_result &result, int tick )
{
    for( const checkpoint_result &checkpoint : result.checkpoints ) {
        if( checkpoint.tick == tick ) {
            return &checkpoint;
        }
    }
    return nullptr;
}

std::string render_handoff_group_state( const bandit_pursuit_handoff::abstract_group_state &group )
{
    std::ostringstream out;
    out << "group_id=" << group.group_id
        << ", source_camp_id=" << group.source_camp_id
        << ", target=" << ( group.current_target_or_mark.empty() ? "none" : group.current_target_or_mark )
        << ", group_strength=" << group.group_strength
        << ", confidence=" << group.confidence
        << ", threat=" << group.current_threat_estimate
        << ", bounty=" << group.current_bounty_estimate
        << ", return_clock=" << group.return_clock
        << ", retreat_bias=" << group.retreat_bias
        << ", wound_burden=" << bandit_pursuit_handoff::to_string( group.wound_burden )
        << ", morale=" << bandit_pursuit_handoff::to_string( group.morale )
        << ", last_return_posture=" << bandit_pursuit_handoff::to_string( group.last_return_posture )
        << ", remaining_pressure=" << bandit_pursuit_handoff::to_string( group.remaining_pressure )
        << ", carried_cargo=food=" << group.carried_cargo.food
        << ",meds=" << group.carried_cargo.meds
        << ",ammo=" << group.carried_cargo.ammo
        << ",gear=" << group.carried_cargo.gear
        << ",fuel=" << group.carried_cargo.fuel
        << ",trade=" << group.carried_cargo.trade_goods;
    if( !group.known_recent_marks.empty() ) {
        out << ", known_recent_marks=";
        for( size_t i = 0; i < group.known_recent_marks.size(); ++i ) {
            if( i > 0 ) {
                out << ",";
            }
            out << group.known_recent_marks[i];
        }
    }
    if( !group.anchored_identities.empty() ) {
        out << ", anchored_identities=";
        for( size_t i = 0; i < group.anchored_identities.size(); ++i ) {
            if( i > 0 ) {
                out << ",";
            }
            out << group.anchored_identities[i].id << ":" << group.anchored_identities[i].status;
        }
    }
    return out.str();
}

std::vector<handoff_case_definition> overmap_local_handoff_cases()
{
    using namespace bandit_pursuit_handoff;

    abstract_group_state smoke_group = make_handoff_group( "ridge_pack", "oak_camp", "ridge_smoke",
                                      2, 2, 1, 3, 3 );
    smoke_group.anchored_identities.front().id = "leader_marta";

    local_outcome smoke_outcome;
    smoke_outcome.survivors_remaining = 2;
    smoke_outcome.result = mission_result::scouted;
    smoke_outcome.resolution = lead_resolution::narrowed;
    smoke_outcome.threat_writeback = { { "ridge_smoke", "raise", 2 } };
    smoke_outcome.bounty_writeback = { { "ridge_smoke", "confirm", 2 } };
    smoke_outcome.new_marks_learned = { "ridge_smoke_cabin_edge" };
    smoke_outcome.posture = return_posture::resume_route;
    smoke_outcome.remaining_pressure = remaining_return_pressure_state::tight;

    abstract_group_state granary_group = make_handoff_group( "granary_watch", "oak_camp",
                                        "granary_road_corridor", 2, 2, 1, 4, 3 );
    granary_group.anchored_identities.front().id = "scar_nik";

    local_outcome granary_outcome;
    granary_outcome.survivors_remaining = 1;
    granary_outcome.anchored_identity_updates = { { "scar_nik", "wounded" } };
    granary_outcome.wound_burden = wound_burden_state::moderate;
    granary_outcome.morale = morale_state::shaken;
    granary_outcome.cargo_profile_carried = make_cargo( 0, 1, 1 );
    granary_outcome.camp_stockpile_delta = make_cargo( 0, 1, 0 );
    granary_outcome.result = mission_result::repelled;
    granary_outcome.resolution = lead_resolution::target_lost;
    granary_outcome.threat_writeback = { { "granary_loss_site", "raise", 4 } };
    granary_outcome.bounty_writeback = { { "granary_road_corridor", "collapse", 1 } };
    granary_outcome.loss_site_if_any = "granary_loss_site";
    granary_outcome.posture = return_posture::escape_home;
    granary_outcome.remaining_pressure = remaining_return_pressure_state::collapsed;

    abstract_group_state follower_group = make_handoff_group( "lane_riders", "oak_camp",
                                         "road_travelers", 2, 3, 1, 4, 3 );
    follower_group.anchored_identities.front().id = "rider_vesna";

    local_outcome follower_outcome;
    follower_outcome.survivors_remaining = 2;
    follower_outcome.wound_burden = wound_burden_state::light;
    follower_outcome.morale = morale_state::steady;
    follower_outcome.result = mission_result::withdrawn;
    follower_outcome.resolution = lead_resolution::narrowed;
    follower_outcome.threat_writeback = { { "forest_lane_recent", "watch", 2 } };
    follower_outcome.bounty_writeback = { { "forest_lane_recent", "soft_confirm", 2 } };
    follower_outcome.new_marks_learned = { "forest_lane_recent" };
    follower_outcome.posture = return_posture::shadow_then_break;
    follower_outcome.remaining_pressure = remaining_return_pressure_state::plain_return_only;

    return {
        {
            "smoke_scout_narrows_to_cabin_edge",
            "Smoke scout enters as posture and returns a sharper mark",
            "smoke_only_distant_clue",
            {
                "Goal: a broad smoke clue should enter local play as `scout` posture, not exact-square puppetry, and local contact should be able to return one sharper mark instead of pretending the original blob was destiny.",
                "Carryback: the returned abstract state should now point at `ridge_smoke_cabin_edge` with a tighter clock instead of hauling the whole old smoke envelope around forever.",
                "Playback horizon: the same overmap-side clue should still cool back to `hold / chill` by the later checkpoints instead of becoming immortal scout pressure."
            },
            smoke_group,
            0,
            { contact_certainty::broad, return_pressure_state::normal },
            smoke_outcome
        },
        {
            "granary_probe_breaks_after_local_danger",
            "Granary pressure probe can come home wounded after local danger rewrites it",
            "generated_local_loss_rewrites_corridor_to_withdrawal",
            {
                "Goal: a corridor pressure seam should enter local play as bounded `probe` posture, then local danger should be allowed to break that posture hard enough to send the group home instead of preserving stale route puppetry.",
                "Carryback: the returned abstract state should expose wounds, morale drop, cargo change, raised threat, and a cleared active target reviewer-cleanly enough to debug the seam without guessing.",
                "Playback horizon: the same overmap-side corridor should already stop winning once the local-loss pressure lands, and the later checkpoints should stay off it."
            },
            granary_group,
            0,
            { contact_certainty::localized, return_pressure_state::normal },
            granary_outcome
        },
        {
            "player_present_follower_travel_shadow_breaks_on_contact_loss",
            "Player-present follower travel degrades to rough search after contact loss",
            "generated_human_sighting_tracks_moving_carrier",
            {
                "Goal: a player-present follower-travel sighting should enter local play as `shadow`, not exact-carrier omniscience, and contact loss should degrade it to a rough lane read instead of immortal perfect tracking.",
                "Carryback: after the local scene loses contact, the returned abstract state should keep only `forest_lane_recent` rather than the exact `road_travelers` carrier id, with a shorter clock and break posture.",
                "Playback horizon: the underlying moving-carrier pressure may stay alive while sightings are fresh, but the longer checkpoints should still cool back out instead of making the lane a permanent haunted intercept."
            },
            follower_group,
            0,
            { contact_certainty::localized, return_pressure_state::normal },
            follower_outcome
        }
    };
}

smoke_packet make_smoke_packet( const std::string &id, const std::string &envelope_id,
                                const std::string &region_id, int observed_range_omt,
                                int source_strength, int persistence, int height_bias,
                                int spread_bias, smoke_weather_band weather,
                                lead_family family = lead_family::site,
                                const std::vector<std::string> &notes = {},
                                int vertical_offset = 0,
                                bool vertical_sightline = false )
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
    packet.vertical_offset = vertical_offset;
    packet.vertical_sightline = vertical_sightline;
    return packet;
}

light_packet make_light_packet( const std::string &id, const std::string &envelope_id,
                                const std::string &region_id, int observed_range_omt,
                                int source_strength, int persistence, int side_leakage,
                                light_time_band time, light_weather_band weather,
                                light_exposure_band exposure, light_source_band source,
                                lead_family family, light_terrain_band terrain,
                                const std::vector<std::string> &notes,
                                int vertical_offset = 0,
                                bool vertical_sightline = false,
                                int elevation_bonus = 0 )
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
    packet.terrain = terrain;
    packet.notes = notes;
    packet.vertical_offset = vertical_offset;
    packet.vertical_sightline = vertical_sightline;
    packet.elevation_bonus = elevation_bonus;
    return packet;
}

light_packet make_light_packet( const std::string &id, const std::string &envelope_id,
                                const std::string &region_id, int observed_range_omt,
                                int source_strength, int persistence, int side_leakage,
                                light_time_band time, light_weather_band weather,
                                light_exposure_band exposure, light_source_band source,
                                lead_family family = lead_family::site,
                                const std::vector<std::string> &notes = {},
                                int vertical_offset = 0,
                                bool vertical_sightline = false,
                                int elevation_bonus = 0 )
{
    return make_light_packet( id, envelope_id, region_id, observed_range_omt, source_strength,
                              persistence, side_leakage, time, weather, exposure, source,
                              family, light_terrain_band::open, notes, vertical_offset,
                              vertical_sightline, elevation_bonus );
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

scenario_definition make_generated_windy_smoke_mark_stays_fuzzy()
{
    scenario_definition scenario;
    scenario.id = "generated_windy_smoke_mark_stays_fuzzy";
    scenario.title = "Generated windy smoke mark stays fuzzy";
    scenario.default_checkpoints = { 0, 20, 100 };
    scenario.questions = {
        "Can windy smoke stay actionable while still reading as a fuzzier, drift-prone clue instead of crisp site truth?"
    };

    scenario.frames.push_back( {
        0,
        "fresh_windy_smoke",
        make_camp( 2 ),
        {},
        {
            "Wind should leave a scoutable smoke clue, but the notes should say it drifted instead of pretending the origin is perfectly pinned."
        },
        cadence_tier::nearby_active,
        {},
        {
            make_smoke_packet( "ridge_smoke_windy", "ridge_smoke_windy", "ridge_rim", 7, 3, 2, 1, 1,
                               smoke_weather_band::windy,
                               lead_family::site,
                               { "Wind should keep this clue useful but obviously fuzzier." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "windy_smoke_cooled_without_refresh",
        make_camp( 2 ),
        {},
        {
            "A distant inactive pass should still cool the windy smoke clue instead of letting fuzz become immortality."
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
            "Daily cleanup should prune the stale windy smoke clutter too."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_portal_storm_exposed_light_stays_legible()
{
    scenario_definition scenario;
    scenario.id = "generated_portal_storm_exposed_light_stays_legible";
    scenario.title = "Generated portal-storm exposed light stays legible";
    scenario.default_checkpoints = { 0, 20, 100 };
    scenario.questions = {
        "Can dark portal-storm weather keep a bright exposed light legible without turning sheltered ordinary light into nonsense?"
    };

    scenario.frames.push_back( {
        0,
        "fresh_portal_storm_light",
        make_camp( 2 ),
        {},
        {
            "Portal-storm darkness can still leave an exposed bright light worth scouting, and the review notes should admit that directly."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "storm_watchfire", "storm_watchfire", "storm_ridge", 6, 3, 1, 1,
                               light_time_band::night, light_weather_band::portal_storm,
                               light_exposure_band::exposed, light_source_band::ordinary,
                               lead_family::site,
                               { "Dark portal-storm conditions can still leave bright exposed light legible." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "portal_storm_light_cooled_without_refresh",
        make_camp( 2 ),
        {},
        {
            "Once the light is gone, the usual cooling path should still win instead of preserving mythical storm clairvoyance."
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
            "Daily cleanup should prune the stale portal-storm light clutter as well."
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

scenario_definition make_generated_nearby_cross_z_light_stays_actionable()
{
    scenario_definition scenario;
    scenario.id = "generated_nearby_cross_z_light_stays_actionable";
    scenario.title = "Generated nearby cross-z light stays actionable";
    scenario.default_checkpoints = { 0, 20, 100, 500 };
    scenario.questions = {
        "Goal: nearby cross-z exposed light should stay visible instead of acting like stairs are amnesia beams.",
        "Benchmark: ticks 0 and 20 should keep one bounded scout on the same envelope while the one-floor vertical sightline stays open, then ticks 100 and 500 should cool back to `hold / chill` once refreshes stop.",
        "Tuning metrics: reviewer-readable notes should show vertical_offset=-1, nearby_cross_z_visible=yes, and nearby_cross_z_bonus=1 without pretending every upstairs glow is magical global sight."
    };

    scenario.frames.push_back( {
        0,
        "basement_fire_first_glimpse",
        make_camp( 2 ),
        {},
        {
            "A one-floor-open basement fire should still register from ground level instead of being erased by z-level amnesia."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "basement_fire_cross_z", "basement_fire_cross_z", "mill_basement", 7, 2, 1, 1,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::exposed, light_source_band::ordinary,
                               lead_family::site,
                               { "The basement fire is one floor down but plainly visible through the open stairwell." },
                               -1, true, 0 )
        }
    } );

    scenario.frames.push_back( {
        20,
        "basement_fire_recheck",
        make_camp( 2 ),
        {},
        {
            "The same nearby cross-z light should refresh one bounded mark instead of blinking in and out of existence."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "basement_fire_cross_z", "basement_fire_cross_z", "mill_basement", 7, 2, 1, 1,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::exposed, light_source_band::ordinary,
                               lead_family::site,
                               { "A second look through the same stairwell should keep the one-floor clue honest." },
                               -1, true, 0 )
        }
    } );

    scenario.frames.push_back( {
        60,
        "cross_z_light_fades",
        make_camp( 2 ),
        {},
        {
            "After the nearby cross-z light goes dark, the clue should start cooling instead of staying suspiciously vivid."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "cross_z_light_cleanup",
        make_camp( 2 ),
        {},
        {
            "Once the cross-z light stops refreshing, the clue should cool back out like ordinary bounded light."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "cross_z_light_long_horizon",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later the nearby cross-z light should be gone again instead of haunting the map forever."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_ground_hidden_light_stays_bounded()
{
    scenario_definition scenario;
    scenario.id = "generated_ground_hidden_light_stays_bounded";
    scenario.title = "Generated ground hidden light stays bounded";
    scenario.default_checkpoints = { 0, 20, 100, 500 };
    scenario.questions = {
        "Goal: ordinary hidden ground light should stay bounded even when a brighter elevated twin would be worth acting on.",
        "Benchmark: ticks 0, 20, 100, and 500 should all stay `hold / chill`, with no generated lead surviving from the hidden ground packet.",
        "Tuning metrics: keep the same rough source strength and longish observed range as the elevated twin, but trap it in covered hidden footing so it does not become a fake world beacon."
    };

    scenario.frames.push_back( {
        0,
        "hidden_ground_lantern_first_glimpse",
        make_camp( 2 ),
        {},
        {
            "A tucked-away ground lantern behind cover should not suddenly become long-range actionable truth."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "ridge_hidden_lantern", "ridge_hidden_lantern", "ridge_shed", 10, 2, 1, 0,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::contained, light_source_band::ordinary,
                               lead_family::site, light_terrain_band::built_cover,
                               { "This hidden ground lantern should stay politely bounded." },
                               0, false, 0 )
        }
    } );

    scenario.frames.push_back( {
        20,
        "hidden_ground_lantern_recheck",
        make_camp( 2 ),
        {},
        {
            "A second look should stay equally inert instead of teaching the system clairvoyance."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "ridge_hidden_lantern", "ridge_hidden_lantern", "ridge_shed", 10, 2, 1, 0,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::contained, light_source_band::ordinary,
                               lead_family::site, light_terrain_band::built_cover,
                               { "Repeated hidden ground light still should not leak into magical long-range truth." },
                               0, false, 0 )
        }
    } );

    scenario.frames.push_back( {
        100,
        "hidden_ground_light_cleanup",
        make_camp( 2 ),
        {},
        {
            "Cleanup should remain empty because the hidden ground light never honestly crossed the actionable line."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "hidden_ground_light_long_horizon",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later this should still be quiet, because a hidden ground lantern is not orbital surveillance."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_elevated_exposed_light_becomes_actionable()
{
    scenario_definition scenario;
    scenario.id = "generated_elevated_exposed_light_becomes_actionable";
    scenario.title = "Generated elevated exposed light becomes actionable";
    scenario.default_checkpoints = { 0, 20, 100, 500 };
    scenario.questions = {
        "Goal: elevated exposed light can stay legible farther than ordinary hidden ground light under the same broad night footing without becoming global omniscience.",
        "Benchmark: ticks 0 and 20 should win with `scout` on the elevated exposed envelope, then ticks 100 and 500 should cool back to `hold / chill` once refreshes stop.",
        "Tuning metrics: require reviewer-readable notes to show elevated_exposure_extended=yes and a positive elevation bonus instead of quietly sneaking in magical universal sight."
    };

    scenario.frames.push_back( {
        0,
        "ridge_bonfire_first_glimpse",
        make_camp( 2 ),
        {},
        {
            "The only honest difference from the hidden-ground twin is exposed elevated footing, so this one should cross into bounded actionability."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "ridge_bonfire_elevated", "ridge_bonfire_elevated", "ridge_watch", 10, 2, 1, 0,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::exposed, light_source_band::ordinary,
                               lead_family::site,
                               { "An exposed ridge bonfire should stay legible farther than a hidden ground lantern, but only under bounded footing." },
                               2, true, 2 )
        }
    } );

    scenario.frames.push_back( {
        20,
        "ridge_bonfire_recheck",
        make_camp( 2 ),
        {},
        {
            "A second exposed elevated packet should refresh the same bounded clue instead of inventing a stronger class of magic knowledge."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "ridge_bonfire_elevated", "ridge_bonfire_elevated", "ridge_watch", 10, 2, 1, 0,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::exposed, light_source_band::ordinary,
                               lead_family::site,
                               { "Repeated exposed elevated light should refresh one bounded actionable mark." },
                               2, true, 2 )
        }
    } );

    scenario.frames.push_back( {
        60,
        "elevated_light_fades",
        make_camp( 2 ),
        {},
        {
            "After the elevated fire drops out, the clue should start cooling instead of clinging to the map forever."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "elevated_light_cleanup",
        make_camp( 2 ),
        {},
        {
            "Once the elevated light goes away, the clue should cool instead of turning into immortal certainty."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "elevated_light_long_horizon",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later the elevated clue should be gone again rather than living forever."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_radio_tower_fire_shares_horde_pressure()
{
    scenario_definition scenario;
    scenario.id = "generated_radio_tower_fire_shares_horde_pressure";
    scenario.title = "Generated radio tower fire shares horde pressure";
    scenario.default_checkpoints = { 0, 20, 100, 500 };
    scenario.questions = {
        "Goal: a strong exposed elevated fire in a dead dark world should read as a long-range event, not an upstairs candle.",
        "Benchmark: tick 0 should open a bounded corridor posture on the same far-away tower-fire envelope, tick 20 should still act on it while carrying zombie-horde pressure, tick 100 should stay corridor-shaped under bounded memory, and tick 500 should cool back to `hold / chill`.",
        "Tuning metrics: reviewer-readable notes should show projected_range_omt=30, elevated_exposure_extended=yes, and the shared horde-pressure footing instead of private bandit theater."
    };

    scenario.frames.push_back( {
        0,
        "radio_tower_fire_seen_far_off",
        make_camp( 2 ),
        {},
        {
            "A radio-tower fire in dead darkness should be visible from very far away and should open shared-world corridor pressure, not toy-local gossip."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "radio_tower_fire_corridor", "radio_tower_fire_corridor", "north_highway", 24, 8, 4, 2,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::exposed, light_source_band::ordinary,
                               lead_family::corridor,
                               { "The radio tower is burning hard enough that the whole corridor can read it from far away." },
                               2, true, 6 )
        }
    } );

    scenario.frames.push_back( {
        20,
        "radio_tower_fire_draws_horde_pressure",
        make_camp( 2 ),
        {},
        {
            "The same long-range fire should stay shared-world: zombie pressure now rides the same corridor instead of living in private bandit-only theater.",
            "Reviewer checkpoint: the emitted corridor clue should still mention zombie pressure instead of pretending the fire belongs to bandits alone."
        },
        cadence_tier::nearby_active,
        {
            make_signal( "radio_tower_fire_horde_pressure", "route_pressure",
                         "radio_tower_fire_corridor", "north_highway",
                         lead_family::corridor, 0, 0, 0, 0, 0.30,
                         0, threat_gate::discount_only, 2, 1, false, true,
                         {},
                         { "The same tower-fire corridor now has zombie pressure piling onto it too." } )
        },
        {},
        {
            make_light_packet( "radio_tower_fire_corridor", "radio_tower_fire_corridor", "north_highway", 24, 8, 4, 2,
                               light_time_band::night, light_weather_band::clear,
                               light_exposure_band::exposed, light_source_band::ordinary,
                               lead_family::corridor,
                               { "Repeated visibility from the radio-tower fire should refresh the same long corridor clue." },
                               2, true, 6 )
        }
    } );

    scenario.frames.push_back( {
        100,
        "radio_tower_fire_memory_holds_shape",
        make_camp( 2 ),
        {},
        {
            "Bounded moving-memory persistence should keep the long-range fire corridor shaped like a corridor for a while instead of collapsing instantly or inflating into a site raid."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "radio_tower_fire_long_horizon_cools",
        make_camp( 2 ),
        {},
        {
            "Five hundred turns later the long-range fire clue should finally cool instead of haunting the map forever."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_generated_vertical_smoke_stays_bounded()
{
    scenario_definition scenario;
    scenario.id = "generated_vertical_smoke_stays_bounded";
    scenario.title = "Generated vertical smoke stays bounded";
    scenario.default_checkpoints = { 0, 20, 100, 500 };
    scenario.questions = {
        "Goal: smoke can stay nearby cross-z legible without gaining magical extra general reach merely from floor changes.",
        "Benchmark: tick 0 should yield two bounded scoutable smoke marks, one same-floor and one one-floor-up, and the reviewer-readable notes should keep the vertical packet on vertical_range_bonus=0 instead of quietly extending range.",
        "Tuning metrics: the same source stats should keep the same projected range on both packets; only the nearby_cross_z_visible flag should flip."
    };

    scenario.frames.push_back( {
        0,
        "paired_ground_and_vertical_smoke",
        make_camp( 2 ),
        {},
        {
            "Smoke can be seen nearby across one floor, sure, but the upper-floor packet must not get free extra range just because the source changed z-levels."
        },
        cadence_tier::nearby_active,
        {},
        {
            make_smoke_packet( "yard_smoke_ground", "yard_smoke_ground", "mill_yard", 10, 3, 1, 1, 0,
                               smoke_weather_band::clear,
                               lead_family::site,
                               { "Same-floor control smoke for the vertical honesty comparison." },
                               0, false ),
            make_smoke_packet( "yard_smoke_upper", "yard_smoke_upper", "mill_yard_upper", 10, 3, 1, 1, 0,
                               smoke_weather_band::clear,
                               lead_family::site,
                               { "One-floor-up smoke should stay nearby legible without gaining free long-range reach." },
                               1, true )
        }
    } );

    scenario.frames.push_back( {
        100,
        "vertical_smoke_cleanup",
        make_camp( 2 ),
        {},
        {
            "Cleanup should prune both smoke marks once the packet stops refreshing."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "vertical_smoke_long_horizon",
        make_camp( 2 ),
        {},
        {
            "Long after the smoke is gone, there should be no magical leftover range advantage from the vertical packet."
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

    camp_input cooling_camp = make_camp( 0 );
    scenario.frames.push_back( {
        140,
        "reinforcement_cools_while_no_scout_is_free",
        cooling_camp,
        {},
        {
            "Once corroboration stops, the camp can briefly fall back to hold / chill when no scout is free, but the strengthened site should not be forgotten outright."
        },
        cadence_tier::distant_inactive,
        {}
    } );

    scenario.frames.push_back( {
        220,
        "reinforcement_one_last_cautious_revisit",
        make_camp( 2 ),
        {},
        {
            "Once a scout is free again, the corroborated site can justify one last bounded cautious revisit before the clue finally dies."
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

scenario_definition renamed_scenario( const scenario_definition &base,
                                      const std::string &id,
                                      const std::string &title,
                                      const std::vector<int> &checkpoints,
                                      const std::vector<std::string> &questions )
{
    scenario_definition scenario = base;
    scenario.id = id;
    scenario.title = title;
    scenario.default_checkpoints = checkpoints;
    scenario.questions = questions;
    return scenario;
}

scenario_definition make_empty_frontier_expands_visibility()
{
    scenario_definition scenario;
    scenario.id = "empty_frontier_expands_visibility";
    scenario.title = "Empty frontier expands visibility";
    scenario.default_checkpoints = { 0, 20, 100, 500 };
    scenario.questions = {
        "Goal: when nothing useful is nearby, the camp should leave indefinite hold / chill behind through one explicit bounded scout/explore outing.",
        "Benchmark: inside the first 100 turns the winner should stay on `bounded_explore`, and the notes should show frontier visibility climbing relative to tick 0.",
        "Carry-through: by tick 500 the packet should still read as a bounded frontier-expansion pass rather than immortal random wandering."
    };

    camp_input opening_camp = make_camp( 2 );
    opening_camp.allow_bounded_explore = true;
    opening_camp.explore_pressure = 1;
    opening_camp.explore_distance_multiplier = 0.40;
    scenario.frames.push_back( {
        0,
        "frontier_initial_probe",
        opening_camp,
        {},
        {
            "No real site, corridor, or moving-carrier lead exists yet, so the only honest outward pressure is explicit bounded exploration.",
            "frontier_visibility=1/4"
        },
        cadence_tier::nearby_active,
        {}
    } );

    camp_input mid_camp = opening_camp;
    mid_camp.explore_pressure = 2;
    scenario.frames.push_back( {
        20,
        "frontier_arc_extends",
        mid_camp,
        {},
        {
            "The same bounded scout arc keeps uncovering nearby frontier instead of collapsing immediately back into passivity.",
            "frontier_visibility=2/4"
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "frontier_visibility_now_wider",
        mid_camp,
        {},
        {
            "The report should now show more uncovered frontier than tick 0 while still staying on the explicit bounded explore envelope.",
            "frontier_visibility=4/4"
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "frontier_arc_exhausted",
        make_camp( 2 ),
        {},
        {
            "The bounded scout pass has done its limited uncover work and now stops cleanly instead of degrading into immortal random wandering.",
            "frontier_visibility=4/4"
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_blocked_route_stays_fail_closed_until_explicit_explore()
{
    scenario_definition scenario;
    scenario.id = "blocked_route_stays_fail_closed_until_explicit_explore";
    scenario.title = "Blocked route stays fail-closed until explicit explore";
    scenario.default_checkpoints = { 0, 20, 100 };
    scenario.questions = {
        "Goal: unreachable work should fail closed instead of quietly inheriting engine random-goal nonsense.",
        "Benchmark: ticks 0 and 20 should stay on `hold / chill` with no synthetic explore candidate present.",
        "Benchmark: only the later explicit explore greenlight may break idle, and it should do so on the bounded `bounded_explore` envelope instead of mutating the blocked route into fake path success."
    };

    lead_input blocked_route = make_lead( "blocked_bridge_farm", "blocked_bridge_farm",
                                          lead_family::site, 5, 1, 0.80, 1 );
    blocked_route.has_path = false;
    scenario.frames.push_back( {
        0,
        "blocked_route_fail_closed",
        make_camp( 2 ),
        { blocked_route },
        {
            "The bridge farm is tempting on paper but unreachable this dispatch pass, so hold / chill should win cleanly.",
            "no implicit bounded_explore candidate should appear"
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        20,
        "blocked_route_stays_closed",
        make_camp( 2 ),
        { blocked_route },
        {
            "Repeated blocked-route truth still does not justify accidental wander behavior."
        },
        cadence_tier::nearby_active,
        {}
    } );

    camp_input explore_greenlit_camp = make_camp( 2 );
    explore_greenlit_camp.allow_bounded_explore = true;
    explore_greenlit_camp.explore_pressure = 2;
    explore_greenlit_camp.explore_distance_multiplier = 0.40;
    scenario.frames.push_back( {
        100,
        "explicit_explore_breaks_idle",
        explore_greenlit_camp,
        { blocked_route },
        {
            "The blocked route stays blocked, but explicit frontier work is now allowed, so bounded explore may finally beat hold / chill.",
            "frontier_visibility=2/4"
        },
        cadence_tier::nearby_active,
        {}
    } );

    return scenario;
}

scenario_definition make_portal_storm_smoke_is_harder_to_localize()
{
    scenario_definition scenario;
    scenario.id = "portal_storm_smoke_is_harder_to_localize";
    scenario.title = "Portal-storm smoke is harder to localize";
    scenario.default_checkpoints = { 0, 20, 100 };
    scenario.questions = {
        "Goal: portal-storm smoke should stay explicit, weird, and less trustworthy than the ordinary baseline instead of looking like normal night smoke with a cosmetic label.",
        "Benchmark: the 100-turn packet should show a baseline clear-weather read first, then a portal-storm refresh with materially worse localization notes.",
        "Tuning metrics: reviewer-readable smoke notes should surface `weather=portal_storm` plus a corridor-ish or otherwise displaced origin hint."
    };

    scenario.frames.push_back( {
        0,
        "ordinary_smoke_baseline",
        make_camp( 2 ),
        {},
        {
            "Baseline for comparison: ordinary smoke should stay more direct and less weird than the later portal-storm read."
        },
        cadence_tier::nearby_active,
        {},
        {
            make_smoke_packet( "stormline_smoke", "stormline_smoke", "stormline_ridge", 7, 3, 2, 1, 1,
                               smoke_weather_band::clear,
                               lead_family::site,
                               { "Ordinary smoke baseline before the portal-storm distortion arrives." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "portal_storm_refreshes_same_smoke",
        make_camp( 2 ),
        {},
        {
            "The same source now sits under portal-storm weather, so the review notes should become darker, stranger, and less precise."
        },
        cadence_tier::nearby_active,
        {},
        {
            make_smoke_packet( "stormline_smoke", "stormline_smoke", "stormline_ridge", 7, 3, 2, 1, 1,
                               smoke_weather_band::portal_storm,
                               lead_family::site,
                               { "Portal-storm smoke should read as less trustworthy and more corridor-ish than the ordinary baseline." } )
        }
    } );

    scenario.frames.push_back( {
        100,
        "portal_smoke_cleanup",
        make_camp( 2 ),
        {},
        {
            "Cleanup should eventually prune the weird storm smoke too instead of leaving permanent paranormal certainty behind."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_portal_storm_exposed_light_stays_legible_but_sheltered_light_stays_bounded()
{
    scenario_definition scenario;
    scenario.id = "portal_storm_exposed_light_stays_legible_but_sheltered_light_stays_bounded";
    scenario.title = "Portal-storm exposed light stays legible but sheltered light stays bounded";
    scenario.default_checkpoints = { 0, 20, 100 };
    scenario.questions = {
        "Goal: sheltered ordinary light should not become a magical portal-storm beacon.",
        "Goal: the matching exposed bright light can still stay legible when that is the honest read.",
        "Benchmark: the 100-turn packet should show the sheltered ordinary case staying bounded while the exposed bright case later becomes actionable under the same storm family."
    };

    scenario.frames.push_back( {
        0,
        "sheltered_portal_light_stays_bounded",
        make_camp( 2 ),
        {},
        {
            "A contained ordinary lantern under portal-storm weather should stay materially more bounded than an exposed bright light."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "storm_cabin_lantern", "storm_cabin_lantern", "storm_ridge", 3, 2, 1, 0,
                               light_time_band::night, light_weather_band::portal_storm,
                               light_exposure_band::contained, light_source_band::ordinary,
                               lead_family::site, light_terrain_band::built_cover,
                               { "A sheltered ordinary lantern should not become a magical portal-storm beacon." } )
        }
    } );

    scenario.frames.push_back( {
        20,
        "exposed_portal_light_stays_legible",
        make_camp( 2 ),
        {},
        {
            "Dark portal-storm conditions can still leave a bright exposed light worth scouting, and the packet should say so plainly."
        },
        cadence_tier::nearby_active,
        {},
        {},
        {
            make_light_packet( "storm_watchfire", "storm_watchfire", "storm_ridge", 6, 3, 1, 1,
                               light_time_band::night, light_weather_band::portal_storm,
                               light_exposure_band::exposed, light_source_band::ordinary,
                               lead_family::site, light_terrain_band::open,
                               { "Dark portal-storm conditions can still leave bright exposed light legible." } )
        }
    } );

    scenario.frames.push_back( {
        100,
        "portal_light_cleanup",
        make_camp( 2 ),
        {},
        {
            "Cleanup should prune both readings later without preserving mystical storm clairvoyance."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_independent_camps_do_not_dogpile_by_default()
{
    scenario_definition scenario;
    scenario.id = "independent_camps_do_not_dogpile_by_default";
    scenario.title = "Independent camps do not dogpile by default";
    scenario.default_checkpoints = { 0, 100, 500 };
    scenario.questions = {
        "Goal: camps should read like mostly independent actors instead of automatic coalition spam.",
        "Benchmark: inside the first 100 turns the packet should stay off the already-occupied target and choose a separate honest local route instead.",
        "Carry-through: by tick 500 the same-region pile-on stays suppressed unless a later separate reason appears, which this bounded packet does not provide."
    };

    scenario.frames.push_back( {
        0,
        "occupied_target_suppresses_pile_on",
        make_camp( 2 ),
        {
            make_lead( "occupied_granary_convoy", "occupied_granary_convoy", lead_family::corridor,
                       4, 2, 0.75, 4, 1, 0, threat_gate::discount_only,
                       {},
                       { "Another armed camp is already riding this same convoy lane, so default pile-on pressure should stay suppressed." } ),
            make_lead( "south_quarry_watch", "south_quarry_watch", lead_family::corridor,
                       3, 1, 0.85, 1, 0, 0, threat_gate::discount_only,
                       {},
                       { "A separate local route gives this camp its own honest reason to act without coalition dogpile fiction." } )
        },
        {
            "The already-occupied convoy should not become free coalition truth just because another camp is there first."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "independence_still_holds",
        make_camp( 2 ),
        {
            make_lead( "occupied_granary_convoy", "occupied_granary_convoy", lead_family::corridor,
                       4, 1, 0.70, 5, 2, 0, threat_gate::soft_veto,
                       {},
                       { "Repeated same-region pressure still reads as somebody else's hot mess, not automatic coalition support." } ),
            make_lead( "south_quarry_watch", "south_quarry_watch", lead_family::corridor,
                       2, 1, 0.80, 1, 0, 0, threat_gate::discount_only,
                       {},
                       { "The local separate route is still the cleaner independent option." } )
        },
        {
            "Without a fresh local reason, the packet should keep suppressing dogpile behavior."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        500,
        "pile_on_stays_suppressed",
        make_camp( 2 ),
        {},
        {
            "No later separate reason arrived, so the packet should still stay off the occupied target by the long horizon."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

scenario_definition make_other_camps_read_as_threat_bearing_not_default_allies()
{
    scenario_definition scenario;
    scenario.id = "other_camps_read_as_threat_bearing_not_default_allies";
    scenario.title = "Other camps read as threat-bearing, not default allies";
    scenario.default_checkpoints = { 0, 20, 100 };
    scenario.questions = {
        "Goal: another camp should usually read as threat-bearing pressure, not as free coalition support on first read.",
        "Benchmark: the packet should not mint a `reinforce` answer or other automatic ally truth from the first sight of another camp.",
        "Tuning metrics: a threatening rival-camp signal should keep the winner on bounded caution or hold / chill instead of acting like a friendly-pressure invite."
    };

    scenario.frames.push_back( {
        0,
        "rival_camp_first_read",
        make_camp( 2 ),
        {
            make_lead( "ridge_rival_camp", "ridge_rival_camp", lead_family::corridor,
                       2, 1, 0.70, 6, 1, 0, threat_gate::hard_veto,
                       {},
                       { "First read: another camp on the ridge is an armed separate actor, not automatic ally truth." } )
        },
        {
            "The honest first reaction is caution, not coalition spam."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        20,
        "rival_camp_stays_threat_bearing",
        make_camp( 2 ),
        {
            make_lead( "ridge_rival_camp", "ridge_rival_camp", lead_family::corridor,
                       2, 1, 0.70, 6, 1, 0, threat_gate::hard_veto,
                       {},
                       { "A repeated first-family sighting still reads as risk, not free support." } )
        },
        {
            "Nothing about the same rival-camp read should magically convert it into friendly pressure."
        },
        cadence_tier::nearby_active,
        {}
    } );

    scenario.frames.push_back( {
        100,
        "rival_camp_cleanup",
        make_camp( 2 ),
        {},
        {
            "Longer-horizon cleanup should return to quiet without ever pretending the rival camp was allied truth by default."
        },
        cadence_tier::daily_cleanup,
        {}
    } );

    return scenario;
}

const checkpoint_result *find_checkpoint_result( const playback_result &result, int tick )
{
    for( const checkpoint_result &checkpoint : result.checkpoints ) {
        if( checkpoint.tick == tick ) {
            return &checkpoint;
        }
    }
    return nullptr;
}

std::string winner_summary( const checkpoint_result &checkpoint )
{
    const candidate_debug &winner = checkpoint.evaluation.candidates[checkpoint.evaluation.winner_index];
    std::ostringstream out;
    out << "tick " << checkpoint.tick << " winner=" << bandit_dry_run::to_string( winner.job );
    if( !winner.envelope_id.empty() ) {
        out << " @ " << winner.envelope_id;
    }
    return out.str();
}

bool checkpoint_has_text( const checkpoint_result &checkpoint, const std::string &needle )
{
    for( const std::string &note : checkpoint.notes ) {
        if( note.find( needle ) != std::string::npos ) {
            return true;
        }
    }
    for( const bandit_mark_generation::typed_mark &mark : checkpoint.generated_marks.marks ) {
        for( const std::string &note : mark.notes ) {
            if( note.find( needle ) != std::string::npos ) {
                return true;
            }
        }
    }
    if( checkpoint.evaluation.winner_reason.find( needle ) != std::string::npos ) {
        return true;
    }
    return false;
}

bool has_candidate( const bandit_dry_run::evaluation_result &evaluation,
                    bandit_dry_run::job_template job,
                    const std::string &envelope_id )
{
    for( const candidate_debug &candidate : evaluation.candidates ) {
        if( candidate.job == job && candidate.envelope_id == envelope_id ) {
            return true;
        }
    }
    return false;
}

const candidate_debug *winner_at_or_null( const checkpoint_result *checkpoint )
{
    if( checkpoint == nullptr || checkpoint->evaluation.candidates.empty() ) {
        return nullptr;
    }
    return &checkpoint->evaluation.candidates[checkpoint->evaluation.winner_index];
}

int first_tick_with_winner( const playback_result &playback,
                            const std::function<bool( const checkpoint_result &, const candidate_debug & )> &predicate,
                            int up_to_tick )
{
    for( const checkpoint_result &checkpoint : playback.checkpoints ) {
        if( checkpoint.tick > up_to_tick ) {
            break;
        }
        const candidate_debug *winner = winner_at_or_null( &checkpoint );
        if( winner != nullptr && predicate( checkpoint, *winner ) ) {
            return checkpoint.tick;
        }
    }
    return -1;
}

int first_tick_with_note( const playback_result &playback, const std::string &needle, int up_to_tick )
{
    for( const checkpoint_result &checkpoint : playback.checkpoints ) {
        if( checkpoint.tick > up_to_tick ) {
            break;
        }
        if( checkpoint_has_text( checkpoint, needle ) ) {
            return checkpoint.tick;
        }
    }
    return -1;
}

int first_hold_after_non_idle_turn( const playback_result &playback, int up_to_tick )
{
    bool saw_non_idle = false;
    for( const checkpoint_result &checkpoint : playback.checkpoints ) {
        if( checkpoint.tick > up_to_tick ) {
            break;
        }
        const candidate_debug *winner = winner_at_or_null( &checkpoint );
        if( winner == nullptr ) {
            continue;
        }
        if( winner->job != job_template::hold_chill ) {
            saw_non_idle = true;
            continue;
        }
        if( saw_non_idle ) {
            return checkpoint.tick;
        }
    }
    return -1;
}

int first_hold_after_last_match( const playback_result &playback,
                                 const std::function<bool( const checkpoint_result &, const candidate_debug & )> &predicate,
                                 int up_to_tick )
{
    int last_matching_tick = -1;
    for( const checkpoint_result &checkpoint : playback.checkpoints ) {
        if( checkpoint.tick > up_to_tick ) {
            break;
        }
        const candidate_debug *winner = winner_at_or_null( &checkpoint );
        if( winner != nullptr && predicate( checkpoint, *winner ) ) {
            last_matching_tick = checkpoint.tick;
        }
    }

    if( last_matching_tick < 0 ) {
        return -1;
    }

    for( const checkpoint_result &checkpoint : playback.checkpoints ) {
        if( checkpoint.tick <= last_matching_tick ) {
            continue;
        }
        if( checkpoint.tick > up_to_tick ) {
            break;
        }
        const candidate_debug *winner = winner_at_or_null( &checkpoint );
        if( winner != nullptr && winner->job == job_template::hold_chill ) {
            return checkpoint.tick;
        }
    }

    return -1;
}

size_t count_winner_segments( const playback_result &playback,
                              const std::function<bool( const checkpoint_result &, const candidate_debug & )> &predicate,
                              int up_to_tick )
{
    size_t count = 0;
    bool active = false;
    for( const checkpoint_result &checkpoint : playback.checkpoints ) {
        if( checkpoint.tick > up_to_tick ) {
            break;
        }
        const candidate_debug *winner = winner_at_or_null( &checkpoint );
        const bool matches = winner != nullptr && predicate( checkpoint, *winner );
        if( matches && !active ) {
            ++count;
        }
        active = matches;
    }
    return count;
}

size_t count_route_flips( const playback_result &playback, int up_to_tick )
{
    std::string previous_envelope;
    bool have_previous = false;
    size_t flips = 0;

    for( const checkpoint_result &checkpoint : playback.checkpoints ) {
        if( checkpoint.tick > up_to_tick ) {
            break;
        }
        const candidate_debug *winner = winner_at_or_null( &checkpoint );
        if( winner == nullptr || winner->job == job_template::hold_chill || winner->envelope_id.empty() ) {
            continue;
        }
        if( have_previous && previous_envelope != winner->envelope_id ) {
            ++flips;
        }
        previous_envelope = winner->envelope_id;
        have_previous = true;
    }

    return flips;
}

int extract_frontier_visibility( const checkpoint_result &checkpoint )
{
    static const std::string prefix = "frontier_visibility=";
    for( const std::string &note : checkpoint.notes ) {
        const size_t pos = note.find( prefix );
        if( pos == std::string::npos ) {
            continue;
        }
        const size_t start = pos + prefix.size();
        const size_t slash = note.find( '/', start );
        if( slash == std::string::npos ) {
            continue;
        }
        return std::stoi( note.substr( start, slash - start ) );
    }
    return -1;
}

std::string metric_value_or_none( int value )
{
    return value >= 0 ? std::to_string( value ) : "none";
}

void add_metric( std::vector<benchmark_metric> &metrics, const std::string &name,
                 const std::string &value )
{
    metrics.push_back( { name, value } );
}

std::vector<benchmark_metric> collect_overmap_benchmark_metrics( const playback_result &playback )
{
    std::vector<benchmark_metric> metrics;

    const int first_non_idle_turn = first_tick_with_winner( playback,
    []( const checkpoint_result &, const candidate_debug &winner ) {
        return winner.job != job_template::hold_chill;
    }, 500 );
    const int first_actionable_turn = first_tick_with_winner( playback,
    []( const checkpoint_result &checkpoint, const candidate_debug &winner ) {
        return winner.job != job_template::hold_chill || !checkpoint.generated_leads.empty();
    }, 500 );

    if( playback.scenario_id == "empty_frontier_expands_visibility" ) {
        const checkpoint_result *tick0 = find_checkpoint_result( playback, 0 );
        const int base_visibility = tick0 != nullptr ? extract_frontier_visibility( *tick0 ) : -1;
        int first_growth_turn = -1;
        for( const checkpoint_result &checkpoint : playback.checkpoints ) {
            const int visibility = extract_frontier_visibility( checkpoint );
            if( visibility > base_visibility ) {
                first_growth_turn = checkpoint.tick;
                break;
            }
        }
        const size_t trip_count = count_winner_segments( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.job == job_template::scout && winner.envelope_id == "bounded_explore";
        }, 500 );
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_actionable_turn", metric_value_or_none( first_actionable_turn ) );
        add_metric( metrics, "first_scout_departure_turn", metric_value_or_none( first_tick_with_winner( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.job == job_template::scout && winner.envelope_id == "bounded_explore";
        }, 500 ) ) );
        add_metric( metrics, "first_frontier_growth_turn", metric_value_or_none( first_growth_turn ) );
        add_metric( metrics, "frontier_trip_count_500", std::to_string( trip_count ) );
        add_metric( metrics, "frontier_revisit_count_500", std::to_string( trip_count > 0 ? trip_count - 1 : 0 ) );
        add_metric( metrics, "route_flip_count_500", std::to_string( count_route_flips( playback, 500 ) ) );
        return metrics;
    }

    if( playback.scenario_id == "blocked_route_stays_fail_closed_until_explicit_explore" ) {
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_actionable_turn", metric_value_or_none( first_actionable_turn ) );
        add_metric( metrics, "first_explicit_explore_turn", metric_value_or_none( first_tick_with_winner( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.job == job_template::scout && winner.envelope_id == "bounded_explore";
        }, 100 ) ) );
        add_metric( metrics, "route_flip_count_100", std::to_string( count_route_flips( playback, 100 ) ) );
        return metrics;
    }

    if( playback.scenario_id == "hidden_side_light_stays_inert" ) {
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_actionable_turn", metric_value_or_none( first_actionable_turn ) );
        add_metric( metrics, "generated_lead_count_100", std::to_string( find_checkpoint_result( playback, 100 ) != nullptr ?
                    find_checkpoint_result( playback, 100 )->generated_leads.size() : 0 ) );
        return metrics;
    }

    if( playback.scenario_id == "visible_side_light_becomes_actionable" ) {
        const size_t visit_count = count_winner_segments( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.envelope_id == "north_farm_visible_side_light";
        }, 100 );
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_actionable_turn", metric_value_or_none( first_actionable_turn ) );
        add_metric( metrics, "first_scout_departure_turn", metric_value_or_none( first_tick_with_winner( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.job == job_template::scout && winner.envelope_id == "north_farm_visible_side_light";
        }, 100 ) ) );
        add_metric( metrics, "target_visit_count_100", std::to_string( visit_count ) );
        return metrics;
    }

    if( playback.scenario_id == "corridor_light_shares_horde_pressure" ) {
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_actionable_turn", metric_value_or_none( first_actionable_turn ) );
        add_metric( metrics, "first_shared_pressure_turn", metric_value_or_none( first_tick_with_note( playback, "zombie pressure", 500 ) ) );
        add_metric( metrics, "route_flip_count_500", std::to_string( count_route_flips( playback, 500 ) ) );
        return metrics;
    }

    if( playback.scenario_id == "local_loss_rewrites_to_withdrawal" ) {
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_withdrawal_turn", metric_value_or_none( first_hold_after_non_idle_turn( playback, 500 ) ) );
        add_metric( metrics, "stale_route_revisit_count_500", std::to_string( count_winner_segments( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.envelope_id == "granary_road_corridor";
        }, 500 ) > 0 ? count_winner_segments( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.envelope_id == "granary_road_corridor";
        }, 500 ) - 1 : 0 ) );
        return metrics;
    }

    if( playback.scenario_id == "local_loss_reroutes_to_safer_detour" ) {
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_detour_turn", metric_value_or_none( first_tick_with_winner( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.envelope_id == "ridge_detour_watch";
        }, 500 ) ) );
        add_metric( metrics, "route_flip_count_100", std::to_string( count_route_flips( playback, 100 ) ) );
        add_metric( metrics, "stale_route_revisit_count_500", std::to_string( count_winner_segments( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.envelope_id == "river_bridge_intercept";
        }, 500 ) > 0 ? count_winner_segments( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.envelope_id == "river_bridge_intercept";
        }, 500 ) - 1 : 0 ) );
        return metrics;
    }

    if( playback.scenario_id == "moving_prey_memory_stays_bounded" ) {
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_actionable_turn", metric_value_or_none( first_actionable_turn ) );
        add_metric( metrics, "first_drop_turn", metric_value_or_none( first_hold_after_non_idle_turn( playback, 500 ) ) );
        add_metric( metrics, "route_flip_count_500", std::to_string( count_route_flips( playback, 500 ) ) );
        return metrics;
    }

    if( playback.scenario_id == "repeated_site_interest_stays_bounded" ) {
        const checkpoint_result *tick100 = find_checkpoint_result( playback, 100 );
        int total_site_hits = -1;
        int confidence_bonus = -1;
        if( tick100 != nullptr && !tick100->generated_marks.marks.empty() ) {
            total_site_hits = tick100->generated_marks.marks.front().repeated_site_reinforcement.total_site_hits;
            confidence_bonus = tick100->generated_marks.marks.front().repeated_site_reinforcement.confidence_bonus;
        }
        const auto repeated_site_visit = []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.family == lead_family::site && winner.job == job_template::scout;
        };
        const size_t site_visit_count = count_winner_segments( playback, repeated_site_visit, 500 );
        const int cooldown_turn = first_hold_after_last_match( playback, repeated_site_visit, 500 );
        const bool endless_pressure = first_tick_with_winner( playback, repeated_site_visit, 500 ) >= 0 &&
                                      cooldown_turn < 0;
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_revisit_turn", metric_value_or_none( first_tick_with_winner( playback,
        []( const checkpoint_result &checkpoint, const candidate_debug &winner ) {
            return checkpoint.tick >= 20 && winner.family == lead_family::site &&
                   winner.job == job_template::scout;
        }, 500 ) ) );
        add_metric( metrics, "repeated_site_hits_100", metric_value_or_none( total_site_hits ) );
        add_metric( metrics, "repeated_site_confidence_bonus_100", metric_value_or_none( confidence_bonus ) );
        add_metric( metrics, "site_visit_count_500", std::to_string( site_visit_count ) );
        add_metric( metrics, "site_revisit_count_500",
                    std::to_string( site_visit_count > 0 ? site_visit_count - 1 : 0 ) );
        add_metric( metrics, "cooldown_turn", metric_value_or_none( cooldown_turn ) );
        add_metric( metrics, "endless_pressure_flag", endless_pressure ? "true" : "false" );
        return metrics;
    }

    if( playback.scenario_id == "windy_smoke_stays_scoutable_but_fuzzy" ) {
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_fuzzed_weather_turn", metric_value_or_none( first_tick_with_note( playback, "weather=windy", 100 ) ) );
        add_metric( metrics, "first_cooldown_turn", metric_value_or_none( first_hold_after_non_idle_turn( playback, 100 ) ) );
        return metrics;
    }

    if( playback.scenario_id == "portal_storm_smoke_is_harder_to_localize" ) {
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_portal_storm_turn", metric_value_or_none( first_tick_with_note( playback, "weather=portal_storm", 100 ) ) );
        add_metric( metrics, "first_corridorish_origin_hint_turn", metric_value_or_none( first_tick_with_note( playback, "origin_hint=corridorish", 100 ) ) );
        return metrics;
    }

    if( playback.scenario_id == "portal_storm_exposed_light_stays_legible_but_sheltered_light_stays_bounded" ) {
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_actionable_turn", metric_value_or_none( first_actionable_turn ) );
        add_metric( metrics, "sheltered_light_actionable_turn", metric_value_or_none( first_tick_with_winner( playback,
        []( const checkpoint_result &checkpoint, const candidate_debug &winner ) {
            return checkpoint.tick == 0 && winner.job != job_template::hold_chill;
        }, 0 ) ) );
        return metrics;
    }

    if( playback.scenario_id == "independent_camps_do_not_dogpile_by_default" ) {
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "first_independent_route_turn", metric_value_or_none( first_tick_with_winner( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.envelope_id == "south_quarry_watch";
        }, 500 ) ) );
        add_metric( metrics, "other_camp_overlap_count_500", std::to_string( count_winner_segments( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.envelope_id == "occupied_granary_convoy";
        }, 500 ) ) );
        add_metric( metrics, "route_flip_count_500", std::to_string( count_route_flips( playback, 500 ) ) );
        return metrics;
    }

    if( playback.scenario_id == "other_camps_read_as_threat_bearing_not_default_allies" ) {
        const checkpoint_result *tick0 = find_checkpoint_result( playback, 0 );
        const bool assumes_ally = tick0 != nullptr && has_candidate( tick0->evaluation,
                                  job_template::reinforce, "ridge_rival_camp" );
        add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
        add_metric( metrics, "threat_bearing_read_turn", metric_value_or_none( first_tick_with_winner( playback,
        []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.job == job_template::hold_chill;
        }, 100 ) ) );
        add_metric( metrics, "ally_assumption_flag", assumes_ally ? "true" : "false" );
        return metrics;
    }

    add_metric( metrics, "first_non_idle_turn", metric_value_or_none( first_non_idle_turn ) );
    add_metric( metrics, "first_actionable_turn", metric_value_or_none( first_actionable_turn ) );
    return metrics;
}

benchmark_check_result make_benchmark_check( int tick, const std::string &label,
        bool passed, const std::string &details )
{
    benchmark_check_result result;
    result.tick = tick;
    result.label = label;
    result.passed = passed;
    result.details = details;
    return result;
}

std::vector<scenario_definition> overmap_benchmark_suite_scenarios()
{
    return {
        make_empty_frontier_expands_visibility(),
        make_blocked_route_stays_fail_closed_until_explicit_explore(),
        renamed_scenario( make_generated_directional_light_hidden_side_stays_inert(),
                          "hidden_side_light_stays_inert",
                          "Hidden-side light stays inert",
                          { 0, 20, 100, 500 },
                          {
                              "Goal: directional light that does not leak to the bandit-facing side stays non-actionable.",
                              "Benchmark: through tick 100 the packet should stay inert with no honest pursuit posture.",
                              "Carry-through: the long horizon should stay equally quiet instead of learning psychic vision later."
                          } ),
        renamed_scenario( make_generated_directional_light_visible_side_becomes_actionable(),
                          "visible_side_light_becomes_actionable",
                          "Visible-side light becomes actionable",
                          { 0, 20, 100 },
                          {
                              "Goal: the visible-side twin of the same directional-light footing should become actionable instead of staying inert.",
                              "Benchmark: inside the 100-turn packet the light-born clue should produce a real bounded posture reviewer-cleanly.",
                              "Tuning metrics: keep the same broader footing as the hidden-side twin and only change the honest visible-side leakage."
                          } ),
        renamed_scenario( make_generated_directional_light_corridor_shares_horde_pressure(),
                          "corridor_light_shares_horde_pressure",
                          "Corridor light shares horde pressure",
                          { 0, 20, 100, 500 },
                          {
                              "Goal: corridor light should stay shared-world pressure instead of private bandit omniscience.",
                              "Benchmark: inside the 100-turn packet the same corridor clue should still carry zombie-pressure consequences reviewer-readably.",
                              "Carry-through: the clue should cool later without changing family or becoming magical site truth."
                          } ),
        renamed_scenario( make_generated_local_loss_rewrites_corridor_to_withdrawal(),
                          "local_loss_rewrites_to_withdrawal",
                          "Local loss rewrites to withdrawal",
                          { 0, 20, 100, 500 },
                          {
                              "Goal: stale pursuit should collapse once local reality makes the tile too hot.",
                              "Benchmark: by tick 100 the old corridor posture should have rewritten to withdrawal or hold / chill.",
                              "Carry-through: tick 500 should stay off the burned route instead of regrowing stale pursuit by habit."
                          } ),
        renamed_scenario( make_generated_local_loss_reroutes_to_safer_detour(),
                          "local_loss_reroutes_to_safer_detour",
                          "Local loss reroutes to safer detour",
                          { 0, 20, 100, 500 },
                          {
                              "Goal: some hot-route cases should reroute instead of merely giving up.",
                              "Benchmark: inside the first 100 turns the packet should show a safer detour rather than recommitting to the burned path.",
                              "Carry-through: later cooling should stay off the stale bridge route."
                          } ),
        renamed_scenario( make_generated_human_sighting_tracks_moving_carrier(),
                          "moving_prey_memory_stays_bounded",
                          "Moving prey memory stays bounded",
                          { 0, 20, 100, 500 },
                          {
                              "Goal: moving bounty can stay warm briefly without turning into immortal pursuit.",
                              "Benchmark: by tick 100 the moving-memory packet should still support a bounded moving-carrier posture while the clue remains warm enough.",
                              "Carry-through: by tick 500 stale moving contact should have dropped cleanly instead of retrying forever."
                          } ),
        renamed_scenario( make_generated_repeated_site_reinforcement_stays_bounded(),
                          "repeated_site_interest_stays_bounded",
                          "Repeated site interest stays bounded",
                          { 0, 20, 100, 140, 220, 300, 380, 500 },
                          {
                              "Goal: repeated mixed site signals can strengthen revisit interest without unlocking immortal settlement truth.",
                              "Benchmark: by tick 100 the repeated site mark may be modestly stronger, but it must stay reviewer-readable and bounded.",
                              "Carry-through: by tick 500 the packet should show one last bounded revisit, a real cooldown turn, and no endless pressure."
                          } ),
        renamed_scenario( make_generated_windy_smoke_mark_stays_fuzzy(),
                          "windy_smoke_stays_scoutable_but_fuzzy",
                          "Windy smoke stays scoutable but fuzzy",
                          { 0, 20, 100 },
                          {
                              "Goal: strong wind should leave smoke potentially scoutable while clearly degrading source precision.",
                              "Benchmark: inside the 100-turn packet the weather notes should show a fuzzier, drifted read than the ordinary clear-weather baseline.",
                              "Tuning metrics: keep the clue bounded and reviewer-readable, not accidentally promoted to magical source certainty."
                          } ),
        make_portal_storm_smoke_is_harder_to_localize(),
        make_portal_storm_exposed_light_stays_legible_but_sheltered_light_stays_bounded(),
        make_independent_camps_do_not_dogpile_by_default(),
        make_other_camps_read_as_threat_bearing_not_default_allies(),
    };
}

std::vector<benchmark_check_result> evaluate_overmap_benchmark_100( const playback_result &playback )
{
    const checkpoint_result *tick0 = find_checkpoint_result( playback, 0 );
    const checkpoint_result *tick20 = find_checkpoint_result( playback, 20 );
    const checkpoint_result *tick100 = find_checkpoint_result( playback, 100 );

    if( playback.scenario_id == "empty_frontier_expands_visibility" ) {
        const bool bounded_outing = tick100 != nullptr &&
                                    tick100->evaluation.candidates[tick100->evaluation.winner_index].job == job_template::scout &&
                                    tick100->evaluation.candidates[tick100->evaluation.winner_index].envelope_id == "bounded_explore";
        const bool frontier_grew = tick0 != nullptr && tick100 != nullptr &&
                                   checkpoint_has_text( *tick0, "frontier_visibility=1/4" ) &&
                                   checkpoint_has_text( *tick100, "frontier_visibility=4/4" );
        return {
            make_benchmark_check( 100, "bounded frontier outing replaces indefinite idle", bounded_outing,
                                  tick100 != nullptr ? winner_summary( *tick100 ) : "missing tick 100" ),
            make_benchmark_check( 100, "frontier visibility increased relative to tick 0", frontier_grew,
                                  frontier_grew ? "notes show frontier_visibility=1/4 -> 4/4" : "frontier visibility notes missing or unchanged" )
        };
    }

    if( playback.scenario_id == "blocked_route_stays_fail_closed_until_explicit_explore" ) {
        const bool fail_closed = tick0 != nullptr &&
                                 tick0->evaluation.candidates[tick0->evaluation.winner_index].job == job_template::hold_chill &&
                                 !has_candidate( tick0->evaluation, job_template::scout, "bounded_explore" );
        const bool explicit_explore_breaks_idle = tick100 != nullptr &&
                                                  tick100->evaluation.candidates[tick100->evaluation.winner_index].job == job_template::scout &&
                                                  tick100->evaluation.candidates[tick100->evaluation.winner_index].envelope_id == "bounded_explore";
        return {
            make_benchmark_check( 0, "blocked route stays fail-closed before explore is greenlit", fail_closed,
                                  tick0 != nullptr ? winner_summary( *tick0 ) : "missing tick 0" ),
            make_benchmark_check( 100, "only explicit bounded explore may break idle", explicit_explore_breaks_idle,
                                  tick100 != nullptr ? winner_summary( *tick100 ) : "missing tick 100" )
        };
    }

    if( playback.scenario_id == "hidden_side_light_stays_inert" ) {
        const bool inert = tick0 != nullptr && tick20 != nullptr && tick100 != nullptr &&
                           tick0->evaluation.candidates[tick0->evaluation.winner_index].job == job_template::hold_chill &&
                           tick20->evaluation.candidates[tick20->evaluation.winner_index].job == job_template::hold_chill &&
                           tick100->evaluation.candidates[tick100->evaluation.winner_index].job == job_template::hold_chill &&
                           tick0->generated_leads.empty() && tick20->generated_leads.empty() && tick100->generated_leads.empty();
        return {
            make_benchmark_check( 100, "hidden-side light stays inert through the 100-turn packet", inert,
                                  inert ? "ticks 0, 20, and 100 all stay on hold / chill with no generated lead" : "one hidden-side checkpoint became actionable" )
        };
    }

    if( playback.scenario_id == "visible_side_light_becomes_actionable" ) {
        const bool actionable = tick0 != nullptr && tick20 != nullptr &&
                                tick0->evaluation.candidates[tick0->evaluation.winner_index].job == job_template::scout &&
                                !tick0->generated_leads.empty() &&
                                tick20->evaluation.candidates[tick20->evaluation.winner_index].job == job_template::scout;
        return {
            make_benchmark_check( 100, "visible-side light becomes a real actionable posture", actionable,
                                  tick0 != nullptr ? winner_summary( *tick0 ) : "missing tick 0" )
        };
    }

    if( playback.scenario_id == "corridor_light_shares_horde_pressure" ) {
        const bool shared_pressure = tick20 != nullptr &&
                                     tick20->evaluation.candidates[tick20->evaluation.winner_index].family == lead_family::corridor &&
                                     checkpoint_has_text( *tick20, "zombie pressure" );
        return {
            make_benchmark_check( 100, "corridor light keeps shared horde-pressure footing", shared_pressure,
                                  tick20 != nullptr ? winner_summary( *tick20 ) : "missing tick 20" )
        };
    }

    if( playback.scenario_id == "local_loss_rewrites_to_withdrawal" ) {
        const bool withdrawal = tick100 != nullptr &&
                                tick100->evaluation.candidates[tick100->evaluation.winner_index].job == job_template::hold_chill;
        return {
            make_benchmark_check( 100, "stale corridor posture has rewritten to withdrawal or hold / chill by tick 100", withdrawal,
                                  tick100 != nullptr ? winner_summary( *tick100 ) : "missing tick 100" )
        };
    }

    if( playback.scenario_id == "local_loss_reroutes_to_safer_detour" ) {
        const bool rerouted = tick20 != nullptr &&
                              tick20->evaluation.candidates[tick20->evaluation.winner_index].envelope_id == "ridge_detour_watch";
        return {
            make_benchmark_check( 100, "hot-route loss reroutes to a safer detour", rerouted,
                                  tick20 != nullptr ? winner_summary( *tick20 ) : "missing tick 20" )
        };
    }

    if( playback.scenario_id == "moving_prey_memory_stays_bounded" ) {
        const bool warm_memory = tick100 != nullptr &&
                                 tick100->evaluation.candidates[tick100->evaluation.winner_index].job == job_template::stalk &&
                                 tick100->evaluation.candidates[tick100->evaluation.winner_index].family == lead_family::moving_carrier;
        return {
            make_benchmark_check( 100, "moving prey memory stays warm enough to matter through tick 100", warm_memory,
                                  tick100 != nullptr ? winner_summary( *tick100 ) : "missing tick 100" )
        };
    }

    if( playback.scenario_id == "repeated_site_interest_stays_bounded" ) {
        const bool bounded_revisit = tick100 != nullptr &&
                                     tick100->evaluation.candidates[tick100->evaluation.winner_index].job == job_template::scout &&
                                     !tick100->generated_marks.marks.empty() &&
                                     tick100->generated_marks.marks[0].repeated_site_reinforcement.confidence_bonus >= 1;
        return {
            make_benchmark_check( 100, "repeated site interest strengthens modestly but stays bounded", bounded_revisit,
                                  bounded_revisit ? "tick 100 keeps a bounded scout with visible repeated-site reinforcement" : "repeated-site reinforcement did not stay visible at tick 100" )
        };
    }

    if( playback.scenario_id == "windy_smoke_stays_scoutable_but_fuzzy" ) {
        const bool fuzzy = tick0 != nullptr &&
                           tick0->evaluation.candidates[tick0->evaluation.winner_index].job == job_template::scout &&
                           checkpoint_has_text( *tick0, "weather=windy" ) &&
                           checkpoint_has_text( *tick0, "verdict=fuzzed" ) &&
                           checkpoint_has_text( *tick0, "origin_hint=drifted" );
        return {
            make_benchmark_check( 100, "windy smoke stays scoutable but reviewer-readably fuzzier", fuzzy,
                                  tick0 != nullptr ? winner_summary( *tick0 ) : "missing tick 0" )
        };
    }

    if( playback.scenario_id == "portal_storm_smoke_is_harder_to_localize" ) {
        const bool harder = tick0 != nullptr && tick20 != nullptr &&
                            checkpoint_has_text( *tick20, "weather=portal_storm" ) &&
                            checkpoint_has_text( *tick20, "origin_hint=corridorish" ) &&
                            !checkpoint_has_text( *tick0, "origin_hint=corridorish" );
        return {
            make_benchmark_check( 100, "portal-storm smoke reads darker and less localizable than the ordinary baseline", harder,
                                  harder ? "tick 20 portal-storm notes become corridor-ish while the baseline does not" : "portal-storm smoke did not look materially stranger than baseline" )
        };
    }

    if( playback.scenario_id == "portal_storm_exposed_light_stays_legible_but_sheltered_light_stays_bounded" ) {
        const bool sheltered_bounded = tick0 != nullptr &&
                                       tick0->evaluation.candidates[tick0->evaluation.winner_index].job == job_template::hold_chill &&
                                       tick0->generated_leads.empty();
        const bool exposed_legible = tick20 != nullptr &&
                                     tick20->evaluation.candidates[tick20->evaluation.winner_index].job == job_template::scout &&
                                     checkpoint_has_text( *tick20, "weather=portal_storm" ) &&
                                     checkpoint_has_text( *tick20, "storm_bright_light_preserved=yes" );
        return {
            make_benchmark_check( 0, "sheltered ordinary light stays materially bounded under portal-storm weather", sheltered_bounded,
                                  tick0 != nullptr ? winner_summary( *tick0 ) : "missing tick 0" ),
            make_benchmark_check( 20, "exposed bright light stays legible under the same storm family", exposed_legible,
                                  tick20 != nullptr ? winner_summary( *tick20 ) : "missing tick 20" )
        };
    }

    if( playback.scenario_id == "independent_camps_do_not_dogpile_by_default" ) {
        const bool independent = tick0 != nullptr && tick100 != nullptr &&
                                 tick0->evaluation.candidates[tick0->evaluation.winner_index].envelope_id == "south_quarry_watch" &&
                                 tick100->evaluation.candidates[tick100->evaluation.winner_index].envelope_id == "south_quarry_watch";
        return {
            make_benchmark_check( 100, "camp stays on its own honest route instead of dogpiling the occupied target", independent,
                                  tick100 != nullptr ? winner_summary( *tick100 ) : "missing tick 100" )
        };
    }

    if( playback.scenario_id == "other_camps_read_as_threat_bearing_not_default_allies" ) {
        bool minted_reinforce = false;
        if( tick0 != nullptr ) {
            for( const candidate_debug &candidate : tick0->evaluation.candidates ) {
                if( candidate.job == job_template::reinforce ) {
                    minted_reinforce = true;
                    break;
                }
            }
        }
        const bool threat_bearing = tick0 != nullptr &&
                                    tick0->evaluation.candidates[tick0->evaluation.winner_index].job == job_template::hold_chill &&
                                    !minted_reinforce;
        return {
            make_benchmark_check( 100, "other camp stays threat-bearing and does not mint default ally truth", threat_bearing,
                                  tick0 != nullptr ? winner_summary( *tick0 ) : "missing tick 0" )
        };
    }

    return {};
}

std::vector<benchmark_check_result> evaluate_overmap_benchmark_500( const playback_result &playback )
{
    const checkpoint_result *tick500 = find_checkpoint_result( playback, 500 );

    if( playback.scenario_id == "empty_frontier_expands_visibility" ) {
        const bool bounded = tick500 != nullptr &&
                             tick500->evaluation.candidates[tick500->evaluation.winner_index].job == job_template::hold_chill &&
                             checkpoint_has_text( *tick500, "frontier_visibility=4/4" );
        return {
            make_benchmark_check( 500, "frontier outing stays bounded on the long horizon", bounded,
                                  tick500 != nullptr ? winner_summary( *tick500 ) : "missing tick 500" )
        };
    }

    if( playback.scenario_id == "local_loss_rewrites_to_withdrawal" ) {
        const bool stays_off_burned_route = tick500 != nullptr &&
                                            tick500->evaluation.candidates[tick500->evaluation.winner_index].job == job_template::hold_chill;
        return {
            make_benchmark_check( 500, "burned route stays abandoned through tick 500", stays_off_burned_route,
                                  tick500 != nullptr ? winner_summary( *tick500 ) : "missing tick 500" )
        };
    }

    if( playback.scenario_id == "moving_prey_memory_stays_bounded" ) {
        const bool cooled = tick500 != nullptr &&
                            tick500->evaluation.candidates[tick500->evaluation.winner_index].job == job_template::hold_chill;
        return {
            make_benchmark_check( 500, "stale moving prey memory drops cleanly by tick 500", cooled,
                                  tick500 != nullptr ? winner_summary( *tick500 ) : "missing tick 500" )
        };
    }

    if( playback.scenario_id == "repeated_site_interest_stays_bounded" ) {
        const auto repeated_site_visit = []( const checkpoint_result &, const candidate_debug &winner ) {
            return winner.family == lead_family::site && winner.job == job_template::scout;
        };
        const size_t site_visit_count = count_winner_segments( playback, repeated_site_visit, 500 );
        const int cooldown_turn = first_hold_after_last_match( playback, repeated_site_visit, 500 );
        const bool endless_pressure = first_tick_with_winner( playback, repeated_site_visit, 500 ) >= 0 &&
                                      cooldown_turn < 0;
        const bool extraction_unlocked = std::any_of( playback.checkpoints.begin(), playback.checkpoints.end(),
        []( const checkpoint_result &checkpoint ) {
            if( checkpoint.tick > 500 ) {
                return false;
            }
            const candidate_debug *winner = winner_at_or_null( &checkpoint );
            return winner != nullptr && ( winner->job == job_template::scavenge ||
                                          winner->job == job_template::steal ||
                                          winner->job == job_template::raid );
        } );
        return {
            make_benchmark_check( 500, "repeated site visit count stays in the bounded target band",
                                  site_visit_count >= 1 && site_visit_count <= 3,
                                  "site_visit_count_500=" + std::to_string( site_visit_count ) ),
            make_benchmark_check( 500, "repeated site revisit count stays modest",
                                  site_visit_count > 0 ? site_visit_count - 1 <= 2 : true,
                                  "site_revisit_count_500=" + std::to_string( site_visit_count > 0 ? site_visit_count - 1 : 0 ) ),
            make_benchmark_check( 500, "repeated site pressure cools back out instead of becoming endless",
                                  !endless_pressure && cooldown_turn >= 0 && tick500 != nullptr &&
                                  tick500->evaluation.candidates[tick500->evaluation.winner_index].job == job_template::hold_chill,
                                  "cooldown_turn=" + metric_value_or_none( cooldown_turn ) +
                                  ", endless_pressure_flag=" + ( endless_pressure ? std::string( "true" ) : std::string( "false" ) ) +
                                  ( tick500 != nullptr ? ", " + winner_summary( *tick500 ) : ", missing tick 500" ) ),
            make_benchmark_check( 500, "repeated site corroboration still never unlocks free extraction truth",
                                  !extraction_unlocked,
                                  extraction_unlocked ? "an extraction winner appeared inside the bounded packet" :
                                  "no sampled checkpoint promoted scavenge, steal, or raid" )
        };
    }

    if( playback.scenario_id == "independent_camps_do_not_dogpile_by_default" ) {
        const bool suppressed = tick500 != nullptr &&
                                tick500->evaluation.candidates[tick500->evaluation.winner_index].job == job_template::hold_chill;
        return {
            make_benchmark_check( 500, "same-region pile-on pressure stays suppressed on the long horizon", suppressed,
                                  tick500 != nullptr ? winner_summary( *tick500 ) : "missing tick 500" )
        };
    }

    return {};
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
        make_generated_windy_smoke_mark_stays_fuzzy(),
        make_generated_portal_storm_exposed_light_stays_legible(),
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

proof_packet_result run_elevated_light_z_level_visibility_packet()
{
    static const std::vector<int> proof_checkpoints = { 0, 20, 100, 500 };
    static const std::vector<scenario_definition> proof_scenarios = {
        make_generated_nearby_cross_z_light_stays_actionable(),
        make_generated_ground_hidden_light_stays_bounded(),
        make_generated_elevated_exposed_light_becomes_actionable(),
        make_generated_radio_tower_fire_shares_horde_pressure(),
        make_generated_vertical_smoke_stays_bounded(),
    };

    proof_packet_result result;
    result.packet_id = "bandit_elevated_light_z_level_visibility_packet_v0";
    result.checkpoints = proof_checkpoints;

    for( const scenario_definition &scenario : proof_scenarios ) {
        result.scenarios.push_back( run_scenario( scenario, proof_checkpoints ) );
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

handoff_packet_result run_overmap_local_handoff_interaction_packet()
{
    static const std::vector<int> proof_checkpoints = { 0, 20, 160, 500 };

    handoff_packet_result result;
    result.packet_id = "bandit_overmap_local_handoff_interaction_packet_v0";
    result.checkpoints = proof_checkpoints;

    for( const handoff_case_definition &definition : overmap_local_handoff_cases() ) {
        const scenario_definition *scenario = find_reference_scenario( definition.playback_scenario_id );
        if( scenario == nullptr ) {
            continue;
        }

        handoff_packet_scenario_result packet_scenario;
        packet_scenario.scenario_id = definition.scenario_id;
        packet_scenario.title = definition.title;
        packet_scenario.playback_scenario_id = definition.playback_scenario_id;
        packet_scenario.questions = definition.questions;
        packet_scenario.playback = run_scenario( *scenario, proof_checkpoints );

        const checkpoint_result *entry_checkpoint = find_playback_checkpoint( packet_scenario.playback,
                                                 definition.entry_tick );
        if( entry_checkpoint == nullptr ) {
            continue;
        }

        packet_scenario.entry = bandit_pursuit_handoff::build_entry_payload( definition.group,
                               winner_candidate( entry_checkpoint->evaluation ),
                               definition.entry_context );
        packet_scenario.local_return = bandit_pursuit_handoff::build_return_packet( packet_scenario.entry,
                                      definition.outcome );
        packet_scenario.returned_group = definition.group;
        bandit_pursuit_handoff::apply_return_packet( packet_scenario.returned_group,
                packet_scenario.local_return );
        result.scenarios.push_back( packet_scenario );
    }

    return result;
}

benchmark_suite_result run_overmap_benchmark_suite_packet()
{
    benchmark_suite_result result;
    result.packet_id = "bandit_overmap_benchmark_suite_packet_v0";

    for( const scenario_definition &scenario : overmap_benchmark_suite_scenarios() ) {
        benchmark_scenario_result benchmark_scenario;
        benchmark_scenario.benchmark_id = scenario.id;
        benchmark_scenario.benchmark_title = scenario.title;
        benchmark_scenario.playback = run_scenario( scenario, scenario.default_checkpoints );
        benchmark_scenario.metrics = collect_overmap_benchmark_metrics( benchmark_scenario.playback );
        benchmark_scenario.benchmark_100 = evaluate_overmap_benchmark_100( benchmark_scenario.playback );
        benchmark_scenario.benchmark_500 = evaluate_overmap_benchmark_500( benchmark_scenario.playback );
        result.scenarios.push_back( benchmark_scenario );
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

static std::vector<std::string> visibility_reads( const checkpoint_result &checkpoint )
{
    std::vector<std::string> reads;
    for( const bandit_mark_generation::typed_mark &mark : checkpoint.generated_marks.marks ) {
        for( const std::string &note : mark.notes ) {
            if( note.find( "light concealment:" ) == 0 || note.find( "smoke weather:" ) == 0 ) {
                reads.push_back( mark.envelope_id + ": " + note );
            }
        }
    }
    return reads;
}

std::string render_elevated_light_z_level_visibility_packet( const proof_packet_result &result )
{
    std::ostringstream out;
    out << "bandit elevated-light and z-level visibility packet\n";
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
            for( const std::string &note : checkpoint.notes ) {
                out << "    note: " << note << "\n";
            }
            for( const std::string &read : visibility_reads( checkpoint ) ) {
                out << "    visibility_read: " << read << "\n";
            }
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

std::string render_overmap_local_handoff_interaction_packet( const handoff_packet_result &result )
{
    std::ostringstream out;
    out << "bandit overmap/local handoff interaction packet\n";
    out << "packet: " << result.packet_id << "\n";
    out << "checkpoints:";
    for( int tick : result.checkpoints ) {
        out << " " << tick;
    }
    out << "\n";

    for( const handoff_packet_scenario_result &scenario : result.scenarios ) {
        out << "scenario: " << scenario.scenario_id << " - " << scenario.title << "\n";
        out << "  playback_source: " << scenario.playback_scenario_id << "\n";
        for( const std::string &question : scenario.questions ) {
            out << "  - " << question << "\n";
        }
        out << "  playback checkpoints:\n";
        for( const checkpoint_result &checkpoint : scenario.playback.checkpoints ) {
            const candidate_debug &winner = winner_candidate( checkpoint.evaluation );
            out << "    - tick " << checkpoint.tick << " [phase=" << checkpoint.phase << "] winner="
                << bandit_dry_run::to_string( winner.job );
            if( !winner.envelope_id.empty() ) {
                out << " @ " << winner.envelope_id;
            }
            out << ", reason=" << checkpoint.evaluation.winner_reason << "\n";
        }
        out << "  handoff contract: posture_only=yes, exact_square_targeting=no, stale_route_puppetry=no\n";
        const std::string handoff_report = bandit_pursuit_handoff::render_report( scenario.entry,
                                           scenario.local_return );
        std::istringstream handoff_stream( handoff_report );
        std::string line;
        while( std::getline( handoff_stream, line ) ) {
            out << "    " << line << "\n";
        }
        out << "  returned_abstract_state:\n";
        out << "    - " << render_handoff_group_state( scenario.returned_group ) << "\n";
    }

    return out.str();
}

std::string render_overmap_benchmark_suite_packet( const benchmark_suite_result &result )
{
    std::ostringstream out;
    out << "bandit overmap benchmark suite packet\n";
    out << "packet: " << result.packet_id << "\n";

    for( const benchmark_scenario_result &scenario : result.scenarios ) {
        out << "scenario: " << scenario.benchmark_id << " - " << scenario.benchmark_title << "\n";
        for( const std::string &question : scenario.playback.questions ) {
            out << "  - " << question << "\n";
        }

        if( !scenario.metrics.empty() ) {
            out << "  cadence metrics:\n";
            for( const benchmark_metric &metric : scenario.metrics ) {
                out << "    - " << metric.name << "=" << metric.value << "\n";
            }
        }

        out << "  100-turn benchmark:\n";
        for( const benchmark_check_result &check : scenario.benchmark_100 ) {
            out << "    - " << ( check.passed ? "PASS" : "FAIL" ) << " [tick " << check.tick << "] "
                << check.label << ": " << check.details << "\n";
        }

        if( scenario.benchmark_500.empty() ) {
            out << "  500-turn carry-through: not required for this bounded scenario\n";
        } else {
            out << "  500-turn carry-through:\n";
            for( const benchmark_check_result &check : scenario.benchmark_500 ) {
                out << "    - " << ( check.passed ? "PASS" : "FAIL" ) << " [tick " << check.tick << "] "
                    << check.label << ": " << check.details << "\n";
            }
        }

        out << "  checkpoints:\n";
        for( const checkpoint_result &checkpoint : scenario.playback.checkpoints ) {
            const candidate_debug &winner = winner_candidate( checkpoint.evaluation );
            out << "    - tick " << checkpoint.tick << " [phase=" << checkpoint.phase << "] winner="
                << bandit_dry_run::to_string( winner.job );
            if( !winner.envelope_id.empty() ) {
                out << " @ " << winner.envelope_id;
            }
            out << ", generated_leads=" << checkpoint.generated_leads.size()
                << ", generated_marks=" << checkpoint.generated_marks.marks.size()
                << "\n";
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
