#include "bandit_live_world.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "json.h"

namespace
{
using bandit_live_world::anchor_source_kind;
using bandit_live_world::camp_lead_kind;
using bandit_live_world::camp_lead_status;
using bandit_live_world::camp_map_lead;
using bandit_live_world::hostile_site_profile;
using bandit_live_world::member_state;
using bandit_live_world::owned_site_kind;

std::optional<anchor_source_kind> anchor_source_kind_from_string( const std::string &value )
{
    if( value == "overmap_special" ) {
        return anchor_source_kind::overmap_special;
    }
    if( value == "map_extra" ) {
        return anchor_source_kind::map_extra;
    }
    if( value == "none" ) {
        return anchor_source_kind::none;
    }
    return std::nullopt;
}

std::optional<owned_site_kind> owned_site_kind_from_string( const std::string &value )
{
    if( value == "bandit_camp" ) {
        return owned_site_kind::bandit_camp;
    }
    if( value == "bandit_work_camp" ) {
        return owned_site_kind::bandit_work_camp;
    }
    if( value == "bandit_cabin" ) {
        return owned_site_kind::bandit_cabin;
    }
    if( value == "cannibal_camp" ) {
        return owned_site_kind::cannibal_camp;
    }
    if( value == "looters" ) {
        return owned_site_kind::looters;
    }
    if( value == "bandits_block" ) {
        return owned_site_kind::bandits_block;
    }
    if( value == "none" ) {
        return owned_site_kind::none;
    }
    return std::nullopt;
}

std::optional<hostile_site_profile> hostile_site_profile_from_string( const std::string &value )
{
    if( value == "camp_style" ) {
        return hostile_site_profile::camp_style;
    }
    if( value == "cannibal_camp" ) {
        return hostile_site_profile::cannibal_camp;
    }
    if( value == "small_hostile_site" ) {
        return hostile_site_profile::small_hostile_site;
    }
    if( value == "none" ) {
        return hostile_site_profile::none;
    }
    return std::nullopt;
}

std::optional<member_state> member_state_from_string( const std::string &value )
{
    if( value == "at_home" ) {
        return member_state::at_home;
    }
    if( value == "outbound" ) {
        return member_state::outbound;
    }
    if( value == "local_contact" ) {
        return member_state::local_contact;
    }
    if( value == "dead" ) {
        return member_state::dead;
    }
    if( value == "missing" ) {
        return member_state::missing;
    }
    return std::nullopt;
}

std::optional<camp_lead_kind> camp_lead_kind_from_string( const std::string &value )
{
    if( value == "structural_bounty" ) {
        return camp_lead_kind::structural_bounty;
    }
    if( value == "harvested_site" ) {
        return camp_lead_kind::harvested_site;
    }
    if( value == "human_activity" ) {
        return camp_lead_kind::human_activity;
    }
    if( value == "basecamp_activity" ) {
        return camp_lead_kind::basecamp_activity;
    }
    if( value == "moving_actor" ) {
        return camp_lead_kind::moving_actor;
    }
    if( value == "route_activity" ) {
        return camp_lead_kind::route_activity;
    }
    if( value == "smoke_signal" ) {
        return camp_lead_kind::smoke_signal;
    }
    if( value == "light_signal" ) {
        return camp_lead_kind::light_signal;
    }
    if( value == "sound_signal" ) {
        return camp_lead_kind::sound_signal;
    }
    if( value == "threat_memory" ) {
        return camp_lead_kind::threat_memory;
    }
    if( value == "loss_site" ) {
        return camp_lead_kind::loss_site;
    }
    if( value == "false_lead" ) {
        return camp_lead_kind::false_lead;
    }
    if( value == "frontier_probe" ) {
        return camp_lead_kind::frontier_probe;
    }
    return std::nullopt;
}

std::optional<camp_lead_status> camp_lead_status_from_string( const std::string &value )
{
    if( value == "suspected" ) {
        return camp_lead_status::suspected;
    }
    if( value == "scout_confirmed" ) {
        return camp_lead_status::scout_confirmed;
    }
    if( value == "active" ) {
        return camp_lead_status::active;
    }
    if( value == "harvested" ) {
        return camp_lead_status::harvested;
    }
    if( value == "stale" ) {
        return camp_lead_status::stale;
    }
    if( value == "invalidated" ) {
        return camp_lead_status::invalidated;
    }
    if( value == "dangerous" ) {
        return camp_lead_status::dangerous;
    }
    return std::nullopt;
}

std::optional<bandit_pursuit_handoff::remaining_return_pressure_state>
remaining_return_pressure_state_from_string( const std::string &value )
{
    using bandit_pursuit_handoff::remaining_return_pressure_state;
    if( value == "ample" ) {
        return remaining_return_pressure_state::ample;
    }
    if( value == "tight" ) {
        return remaining_return_pressure_state::tight;
    }
    if( value == "plain_return_only" ) {
        return remaining_return_pressure_state::plain_return_only;
    }
    if( value == "collapsed" ) {
        return remaining_return_pressure_state::collapsed;
    }
    return std::nullopt;
}

std::optional<bandit_dry_run::job_template> job_template_from_string( const std::string &value )
{
    using bandit_dry_run::job_template;
    if( value == "scout" ) {
        return job_template::scout;
    }
    if( value == "scavenge" ) {
        return job_template::scavenge;
    }
    if( value == "toll" ) {
        return job_template::toll;
    }
    if( value == "stalk" ) {
        return job_template::stalk;
    }
    if( value == "steal" ) {
        return job_template::steal;
    }
    if( value == "raid" ) {
        return job_template::raid;
    }
    if( value == "reinforce" ) {
        return job_template::reinforce;
    }
    if( value == "hold / chill" || value == "hold_chill" || value.empty() ) {
        return job_template::hold_chill;
    }
    return std::nullopt;
}

void push_unique_mark( std::vector<std::string> &marks, const std::string &mark )
{
    if( mark.empty() ) {
        return;
    }
    if( std::find( marks.begin(), marks.end(), mark ) == marks.end() ) {
        marks.push_back( mark );
    }
}

std::string camp_lead_id_for( const std::string &site_id, const camp_lead_kind kind,
                              const std::string &target_id, const tripoint_abs_omt &omt )
{
    std::ostringstream out;
    out << site_id << "#lead:" << bandit_live_world::to_string( kind ) << ':'
        << target_id << '@' << omt.x() << ',' << omt.y() << ',' << omt.z();
    return out.str();
}

std::string lowercase_copy( const std::string &value )
{
    std::string lowered = value;
    std::transform( lowered.begin(), lowered.end(), lowered.begin(), []( const unsigned char ch ) {
        return static_cast<char>( std::tolower( ch ) );
    } );
    return lowered;
}

bool contains_any_token( const std::string &haystack, const std::vector<std::string> &needles )
{
    for( const std::string &needle : needles ) {
        if( haystack.find( needle ) != std::string::npos ) {
            return true;
        }
    }
    return false;
}

camp_lead_kind signal_kind_to_camp_lead_kind( const std::string &kind )
{
    if( kind == "smoke" ) {
        return camp_lead_kind::smoke_signal;
    }
    if( kind == "light" ) {
        return camp_lead_kind::light_signal;
    }
    if( kind == "sound" ) {
        return camp_lead_kind::sound_signal;
    }
    return camp_lead_kind::human_activity;
}

bandit_dry_run::lead_family family_for_camp_map_lead( const camp_map_lead &lead )
{
    switch( lead.kind ) {
        case camp_lead_kind::moving_actor:
            return bandit_dry_run::lead_family::moving_carrier;
        case camp_lead_kind::route_activity:
        case camp_lead_kind::smoke_signal:
        case camp_lead_kind::light_signal:
        case camp_lead_kind::sound_signal:
            return bandit_dry_run::lead_family::corridor;
        case camp_lead_kind::structural_bounty:
        case camp_lead_kind::harvested_site:
        case camp_lead_kind::human_activity:
        case camp_lead_kind::basecamp_activity:
        case camp_lead_kind::threat_memory:
        case camp_lead_kind::loss_site:
        case camp_lead_kind::false_lead:
        case camp_lead_kind::frontier_probe:
            return bandit_dry_run::lead_family::site;
    }

    return bandit_dry_run::lead_family::site;
}

bandit_dry_run::candidate_debug make_camp_map_dispatch_candidate( const camp_map_lead &lead,
        const bandit_live_world::camp_map_dispatch_decision &decision )
{
    bandit_dry_run::candidate_debug candidate;
    candidate.job = decision.intent;
    candidate.lead_id = lead.lead_id.empty() ? lead.target_id : lead.lead_id;
    candidate.envelope_id = lead.target_id.empty() ? candidate.lead_id : lead.target_id;
    candidate.family = family_for_camp_map_lead( lead );
    candidate.generated = true;
    candidate.valid = decision.intent == bandit_dry_run::job_template::scout ||
                      decision.intent == bandit_dry_run::job_template::stalk;
    candidate.winner = candidate.valid;
    candidate.score.lead_bounty_value = lead.bounty;
    candidate.score.lead_confidence_bonus = lead.confidence;
    candidate.score.threat_penalty = lead.threat;
    candidate.score.threat_gate_result = lead.threat_confirmed ?
                                         bandit_dry_run::threat_gate::soft_veto :
                                         bandit_dry_run::threat_gate::discount_only;
    candidate.score.reward_profile_match = decision.reward_score;
    candidate.score.effective_threat_penalty = decision.risk_score;
    candidate.score.final_job_score = decision.margin;
    candidate.notes = decision.notes;
    candidate.notes.push_back( "camp-map remembered lead " + candidate.lead_id +
                               " selected " + bandit_dry_run::to_string( decision.intent ) +
                               " reward=" + std::to_string( decision.reward_score ) +
                               " risk=" + std::to_string( decision.risk_score ) +
                               " margin=" + std::to_string( decision.margin ) );
    return candidate;
}

void upsert_camp_map_lead( bandit_live_world::camp_intelligence_map &intelligence_map,
                           const camp_map_lead &lead )
{
    if( lead.lead_id.empty() ) {
        return;
    }
    if( camp_map_lead *existing = intelligence_map.find_lead( lead.lead_id ) ) {
        *existing = lead;
        return;
    }
    intelligence_map.leads.push_back( lead );
}

void migrate_scalar_memory_to_intelligence_map( bandit_live_world::site_record &site,
        const bool intelligence_map_was_present )
{
    if( intelligence_map_was_present || !site.intelligence_map.leads.empty() ||
        ( site.remembered_target_or_mark.empty() && site.remembered_bounty_estimate <= 0 &&
          site.remembered_threat_estimate <= 0 ) ) {
        return;
    }

    camp_map_lead lead;
    lead.kind = camp_lead_kind::human_activity;
    lead.status = camp_lead_status::suspected;
    lead.target_id = site.remembered_target_or_mark.empty() ? site.active_target_id :
                     site.remembered_target_or_mark;
    lead.omt = site.active_target_omt;
    lead.source_key = lead.target_id;
    lead.source_summary = "migrated from legacy remembered_* site memory";
    lead.bounty = std::max( 0, site.remembered_bounty_estimate );
    lead.threat = std::max( 0, site.remembered_threat_estimate );
    lead.confidence = lead.target_id.empty() ? 1 : 2;
    lead.threat_confirmed = lead.threat > 0;
    lead.last_outcome = "legacy_memory";
    lead.lead_id = camp_lead_id_for( site.site_id, lead.kind, lead.target_id, lead.omt );
    upsert_camp_map_lead( site.intelligence_map, lead );
}

void record_scout_return_lead( bandit_live_world::site_record &site,
                               const bandit_pursuit_handoff::return_packet &packet,
                               const int bandit_losses )
{
    if( packet.job_type != bandit_dry_run::job_template::scout ||
        packet.current_target_or_mark.empty() || packet.survivors_remaining <= 0 ) {
        return;
    }

    camp_map_lead lead;
    lead.kind = camp_lead_kind::basecamp_activity;
    lead.status = camp_lead_status::scout_confirmed;
    lead.target_id = packet.current_target_or_mark;
    lead.omt = site.active_target_omt;
    lead.source_key = packet.group_id;
    lead.source_summary = "scout-return writeback from active owned outing";
    lead.last_scouted_minutes = site.active_sortie_started_minutes;
    lead.last_checked_minutes = site.active_sortie_local_contact_minutes;
    lead.bounty = std::max( 1, site.remembered_bounty_estimate );
    lead.threat = std::max( 0, site.remembered_threat_estimate );
    lead.confidence = std::max( 2, packet.survivors_remaining + 1 );
    lead.threat_confirmed = lead.threat > 0 || packet.resolution == bandit_pursuit_handoff::lead_resolution::still_valid;
    lead.target_alert = packet.resolution == bandit_pursuit_handoff::lead_resolution::target_lost ||
                        packet.posture == bandit_pursuit_handoff::return_posture::broken_flee;
    lead.scout_seen = lead.target_alert;
    lead.prior_bandit_losses = bandit_losses;
    lead.last_outcome = bandit_pursuit_handoff::to_string( packet.result );
    lead.lead_id = camp_lead_id_for( site.site_id, lead.kind, lead.target_id, lead.omt );
    upsert_camp_map_lead( site.intelligence_map, lead );
}

bandit_pursuit_handoff::abstract_group_state make_site_memory_group(
    const bandit_live_world::site_record &site )
{
    bandit_pursuit_handoff::abstract_group_state group;
    group.group_id = site.active_group_id.empty() ? site.site_id + "#dispatch" : site.active_group_id;
    group.source_camp_id = site.site_id;
    group.group_strength = site.count_live_members();
    group.current_target_or_mark = site.remembered_target_or_mark.empty() ? site.active_target_id :
                                   site.remembered_target_or_mark;
    group.current_threat_estimate = site.remembered_threat_estimate;
    group.current_bounty_estimate = site.remembered_bounty_estimate;
    group.retreat_bias = site.remembered_retreat_bias;
    group.return_clock = site.remembered_return_clock;
    group.remaining_pressure = site.remembered_pressure;
    group.known_recent_marks = site.known_recent_marks;
    for( const character_id &member_id : site.active_member_ids ) {
        group.anchored_identities.push_back( { std::to_string( member_id.get_value() ), "alive" } );
    }
    return group;
}

void apply_group_memory( bandit_live_world::site_record &site,
                         const bandit_pursuit_handoff::abstract_group_state &group )
{
    site.remembered_target_or_mark = group.current_target_or_mark;
    site.remembered_threat_estimate = group.current_threat_estimate;
    site.remembered_bounty_estimate = group.current_bounty_estimate;
    site.remembered_retreat_bias = group.retreat_bias;
    site.remembered_return_clock = group.return_clock;
    site.remembered_pressure = group.remaining_pressure;
    site.known_recent_marks = group.known_recent_marks;
}

int shakedown_demand_modifier_percent( const bandit_live_world::site_record &site )
{
    if( site.shakedown_reopen_available && !site.shakedown_reopen_used ) {
        return 140;
    }
    if( site.shakedown_caution > 0 || site.shakedown_bandit_losses > 0 ) {
        return std::max( 50, 100 - 25 * std::max( site.shakedown_caution, site.shakedown_bandit_losses ) );
    }
    return 100;
}

std::string shakedown_outcome_label( const bandit_live_world::shakedown_outcome &outcome )
{
    if( outcome.paid ) {
        return "paid";
    }
    if( outcome.fought && outcome.bandit_losses > 0 ) {
        return "fight_bandit_loss";
    }
    if( outcome.fought && outcome.defender_losses > 0 ) {
        return "fight_defender_loss";
    }
    if( outcome.fought ) {
        return "fight_unresolved";
    }
    return "unknown";
}

struct shakedown_opening_beat {
    std::string id;
    std::string summary;
    std::string bark;
};

shakedown_opening_beat choose_shakedown_opening_beat( const bandit_live_world::site_record &site,
        const bandit_live_world::local_gate_input &input,
        const bandit_live_world::local_gate_decision &decision )
{
    if( site.shakedown_reopen_available && !site.shakedown_reopen_used ) {
        return { "reopened_demand",
                 "seen-you-before reopened demand after prior bloodshed",
                 "Last time you made this expensive.  Now you pay the higher cut, or we finish it." };
    }
    if( input.basecamp_or_camp_scene ) {
        return { "basecamp_pressure",
                 "basecamp leverage against supplies and workers",
                 "Nice camp.  Lots of hands, lots of supplies.  Pay our share and nobody has to count bodies." };
    }
    if( input.darkness_or_concealment || input.standoff_distance >= 2 ) {
        return { "warning_from_cover",
                 "bandits call from cover before closing the fork",
                 "You hear us before you see all of us.  Put the goods down and walk away breathing." };
    }
    if( input.local_threat <= 1 && decision.pressure_margin >= 3 ) {
        return { "weakness_read",
                 "bandits read the player's weak odds before demanding payment",
                 "You look light on friends and heavy on things worth taking.  Make this easy." };
    }
    return { "roadblock_toll",
             "roadblock toll demand",
             "Road's taxed now.  Pay the toll or fight for the privilege." };
}

int special_footprint_radius( const std::string &special_id )
{
    if( special_id == "bandit_camp" || special_id == "bandit_work_camp" ||
        special_id == "cannibal_camp" ) {
        return 1;
    }
    return 0;
}

bool omt_less( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs )
{
    if( lhs.z() != rhs.z() ) {
        return lhs.z() < rhs.z();
    }
    if( lhs.y() != rhs.y() ) {
        return lhs.y() < rhs.y();
    }
    return lhs.x() < rhs.x();
}

int required_dispatch_members( bandit_dry_run::job_template job )
{
    switch( job ) {
        case bandit_dry_run::job_template::hold_chill:
            return 0;
        case bandit_dry_run::job_template::scout:
        case bandit_dry_run::job_template::scavenge:
        case bandit_dry_run::job_template::stalk:
        case bandit_dry_run::job_template::steal:
            return 1;
        case bandit_dry_run::job_template::toll:
        case bandit_dry_run::job_template::raid:
        case bandit_dry_run::job_template::reinforce:
            return 2;
    }

    return 0;
}

bool counts_toward_live_headcount( member_state state )
{
    return state == member_state::at_home || state == member_state::outbound ||
           state == member_state::local_contact;
}

struct hostile_site_profile_rules {
    hostile_site_profile profile = hostile_site_profile::none;
    std::string id;
    int home_reserve = 0;
    int scout_job_bonus = 0;
    int threat_penalty = 1;
    int retreat_bias_floor = 1;
    int return_clock_floor = 1;
    bandit_pursuit_handoff::remaining_return_pressure_state default_remaining_pressure =
        bandit_pursuit_handoff::remaining_return_pressure_state::ample;
    std::string writeback_expectation;
};

hostile_site_profile effective_profile( const bandit_live_world::site_record &site )
{
    return site.profile == hostile_site_profile::none ?
           bandit_live_world::profile_for_site_kind( site.site_kind ) : site.profile;
}

hostile_site_profile_rules rules_for_profile( hostile_site_profile profile )
{
    switch( profile ) {
        case hostile_site_profile::camp_style:
            return { profile, "camp_style", 1, 1, 1, 1, 2,
                bandit_pursuit_handoff::remaining_return_pressure_state::ample,
                "checks the shared 30-minute cadence, keeps a home reserve, and writes back as persistent camp pressure" };
        case hostile_site_profile::cannibal_camp:
            return { profile, "cannibal_camp", 2, 2, 0, 3, 3,
                bandit_pursuit_handoff::remaining_return_pressure_state::tight,
                "checks the shared 30-minute cadence, keeps a larger home larder guard, and writes back as hungry camp pressure" };
        case hostile_site_profile::small_hostile_site:
            return { profile, "small_hostile_site", 0, 1, 0, 2, 1,
                bandit_pursuit_handoff::remaining_return_pressure_state::tight,
                "can commit its whole small roster and writes back as brittle local pressure" };
        case hostile_site_profile::none:
            return { profile, "none", 0, 0, 1, 1, 1,
                bandit_pursuit_handoff::remaining_return_pressure_state::ample,
                "falls back to minimal hostile-site defaults" };
    }

    return { hostile_site_profile::none, "none", 0, 0, 1, 1, 1,
        bandit_pursuit_handoff::remaining_return_pressure_state::ample,
        "falls back to minimal hostile-site defaults" };
}

int required_home_reserve( const bandit_live_world::site_record &site )
{
    const hostile_site_profile profile = effective_profile( site );
    if( profile != hostile_site_profile::camp_style ) {
        return rules_for_profile( profile ).home_reserve;
    }

    const int living_roster = site.count_live_members();
    if( living_roster <= 1 ) {
        return living_roster;
    }
    if( living_roster == 2 ) {
        return 1;
    }
    if( living_roster <= 4 ) {
        return 1;
    }
    if( living_roster <= 7 ) {
        return 2;
    }
    return std::max( 3, ( living_roster * 35 + 99 ) / 100 );
}

bool cannibal_job_requires_attack_pack( bandit_dry_run::job_template job )
{
    switch( job ) {
        case bandit_dry_run::job_template::toll:
        case bandit_dry_run::job_template::stalk:
        case bandit_dry_run::job_template::steal:
        case bandit_dry_run::job_template::raid:
        case bandit_dry_run::job_template::reinforce:
            return true;
        case bandit_dry_run::job_template::hold_chill:
        case bandit_dry_run::job_template::scout:
        case bandit_dry_run::job_template::scavenge:
            return false;
    }

    return false;
}

int required_dispatch_members_for_profile( const bandit_live_world::site_record &site,
        bandit_dry_run::job_template job )
{
    const int generic_required = required_dispatch_members( job );
    if( generic_required <= 0 ) {
        return generic_required;
    }

    if( effective_profile( site ) != hostile_site_profile::cannibal_camp ||
        !cannibal_job_requires_attack_pack( job ) ) {
        return generic_required;
    }

    // Cannibal attack pressure is a pack choice.  Explicit scouts may remain small, but a stalk/raid
    // handoff must not turn one disposable hunter into the whole fight.
    const int available = site.dispatchable_member_capacity();
    if( available < 2 ) {
        return 2;
    }
    return std::clamp( available, 2, 3 );
}

bandit_dry_run::camp_input make_dispatch_camp_input( const bandit_live_world::site_record &site )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    bandit_dry_run::camp_input camp;
    camp.available_manpower = site.dispatchable_member_capacity();
    if( camp.available_manpower >= 3 ) {
        camp.shortage = bandit_dry_run::shortage_band::stable;
    } else if( camp.available_manpower == 2 ) {
        camp.shortage = bandit_dry_run::shortage_band::low;
    } else {
        camp.shortage = bandit_dry_run::shortage_band::critical;
    }
    camp.job_type_bonus[bandit_dry_run::job_template::scout] = rules.scout_job_bonus;
    return camp;
}

bandit_dry_run::lead_input make_nearby_target_lead( const bandit_live_world::site_record &site,
        const tripoint_abs_omt &target_omt, const std::string &target_id )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    bandit_dry_run::lead_input lead;
    lead.id = target_id;
    lead.envelope_id = target_id;
    lead.family = bandit_dry_run::lead_family::site;
    const int distance = rl_dist( site.anchor, target_omt );
    lead.distance_multiplier = std::clamp( 1.0 - static_cast<double>( distance ) / 20.0, 0.35, 1.0 );
    lead.lead_bounty_value = distance <= 10 ? 2 : 1;
    lead.lead_confidence_bonus = 1;
    lead.threat_penalty = rules.threat_penalty;
    lead.threat_gate_result = bandit_dry_run::threat_gate::soft_veto;
    lead.hard_blocked_jobs = {
        bandit_dry_run::job_template::scavenge,
        bandit_dry_run::job_template::steal,
    };
    if( rules.profile == hostile_site_profile::cannibal_camp ) {
        if( site.dispatchable_member_capacity() >= 2 ) {
            lead.family = bandit_dry_run::lead_family::corridor;
            lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::scout );
            lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::toll );
            lead.validity_notes.push_back(
                "cannibal_camp pack pressure: nearby target promotes stalk pressure only after reserve leaves a pack" );
        } else {
            lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::raid );
            lead.validity_notes.push_back(
                "cannibal_camp scout/probe pressure: lone available member may scout but cannot become the whole attack pack" );
        }
    } else {
        lead.hard_blocked_jobs.push_back( bandit_dry_run::job_template::raid );
    }
    lead.validity_notes.push_back( "live-world nearby target envelope from owned site " + site.site_id );
    lead.validity_notes.push_back( "hostile profile " + rules.id + ": " + rules.writeback_expectation );
    lead.validity_notes.push_back( rules.profile == hostile_site_profile::cannibal_camp ?
                                   "bounded v0 dispatch separates cannibal scout/probe pressure from pack attack pressure" :
                                   "bounded v0 dispatch only promotes scout pursuit from real owned members" );
    return lead;
}

std::vector<character_id> select_dispatch_members( const bandit_live_world::site_record &site, int count )
{
    std::vector<character_id> member_ids;
    member_ids.reserve( std::max( count, 0 ) );
    for( const bandit_live_world::member_record &member : site.members ) {
        if( member.state != member_state::at_home || member.wounded_or_unready ) {
            continue;
        }
        member_ids.push_back( member.npc_id );
        if( static_cast<int>( member_ids.size() ) >= count ) {
            break;
        }
    }
    return member_ids;
}

bandit_pursuit_handoff::abstract_group_state make_dispatch_group( const bandit_live_world::site_record &site,
        const std::vector<character_id> &member_ids, const std::string &target_id )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    bandit_pursuit_handoff::abstract_group_state group = make_site_memory_group( site );
    group.group_id = site.site_id + "#dispatch";
    group.source_camp_id = site.site_id;
    group.group_strength = member_ids.size();
    group.confidence = std::clamp( site.count_live_members(), 1, 3 );
    group.panic_threshold = std::max( 1, static_cast<int>( member_ids.size() ) );
    group.cargo_capacity = std::max( 1, static_cast<int>( member_ids.size() ) * 2 );
    group.current_target_or_mark = target_id;
    group.current_threat_estimate = std::max( 1, group.current_threat_estimate );
    group.current_bounty_estimate = std::max( 2, group.current_bounty_estimate );
    group.mission_urgency = 1;
    group.retreat_bias = std::max( group.retreat_bias, rules.retreat_bias_floor );
    group.goal_stickiness = 1;
    group.goal_preemption_posture = 1;
    group.return_clock = std::max( group.return_clock, rules.return_clock_floor );
    group.remaining_pressure = rules.default_remaining_pressure;
    group.anchored_identities.clear();
    for( const character_id &member_id : member_ids ) {
        group.anchored_identities.push_back( { std::to_string( member_id.get_value() ), "alive" } );
    }
    push_unique_mark( group.known_recent_marks, target_id );
    return group;
}
} // namespace

namespace bandit_live_world
{
std::string to_string( anchor_source_kind source_kind )
{
    switch( source_kind ) {
        case anchor_source_kind::none:
            return "none";
        case anchor_source_kind::overmap_special:
            return "overmap_special";
        case anchor_source_kind::map_extra:
            return "map_extra";
    }

    return "none";
}

std::string to_string( owned_site_kind site_kind )
{
    switch( site_kind ) {
        case owned_site_kind::none:
            return "none";
        case owned_site_kind::bandit_camp:
            return "bandit_camp";
        case owned_site_kind::bandit_work_camp:
            return "bandit_work_camp";
        case owned_site_kind::bandit_cabin:
            return "bandit_cabin";
        case owned_site_kind::cannibal_camp:
            return "cannibal_camp";
        case owned_site_kind::looters:
            return "looters";
        case owned_site_kind::bandits_block:
            return "bandits_block";
    }

    return "none";
}

std::string to_string( hostile_site_profile profile )
{
    switch( profile ) {
        case hostile_site_profile::none:
            return "none";
        case hostile_site_profile::camp_style:
            return "camp_style";
        case hostile_site_profile::cannibal_camp:
            return "cannibal_camp";
        case hostile_site_profile::small_hostile_site:
            return "small_hostile_site";
    }

    return "none";
}

std::string to_string( member_state state )
{
    switch( state ) {
        case member_state::at_home:
            return "at_home";
        case member_state::outbound:
            return "outbound";
        case member_state::local_contact:
            return "local_contact";
        case member_state::dead:
            return "dead";
        case member_state::missing:
            return "missing";
    }

    return "at_home";
}

std::string to_string( active_member_observation_state state )
{
    switch( state ) {
        case active_member_observation_state::unresolved:
            return "unresolved";
        case active_member_observation_state::local_contact:
            return "local_contact";
        case active_member_observation_state::returning_home:
            return "returning_home";
        case active_member_observation_state::home:
            return "home";
        case active_member_observation_state::dead:
            return "dead";
        case active_member_observation_state::missing:
            return "missing";
    }

    return "unresolved";
}

std::string to_string( local_gate_posture posture )
{
    switch( posture ) {
        case local_gate_posture::stalk:
            return "stalk";
        case local_gate_posture::hold_off:
            return "hold_off";
        case local_gate_posture::probe:
            return "probe";
        case local_gate_posture::open_shakedown:
            return "open_shakedown";
        case local_gate_posture::attack_now:
            return "attack_now";
        case local_gate_posture::abort:
            return "abort";
    }

    return "abort";
}

std::string to_string( camp_lead_kind kind )
{
    switch( kind ) {
        case camp_lead_kind::structural_bounty:
            return "structural_bounty";
        case camp_lead_kind::harvested_site:
            return "harvested_site";
        case camp_lead_kind::human_activity:
            return "human_activity";
        case camp_lead_kind::basecamp_activity:
            return "basecamp_activity";
        case camp_lead_kind::moving_actor:
            return "moving_actor";
        case camp_lead_kind::route_activity:
            return "route_activity";
        case camp_lead_kind::smoke_signal:
            return "smoke_signal";
        case camp_lead_kind::light_signal:
            return "light_signal";
        case camp_lead_kind::sound_signal:
            return "sound_signal";
        case camp_lead_kind::threat_memory:
            return "threat_memory";
        case camp_lead_kind::loss_site:
            return "loss_site";
        case camp_lead_kind::false_lead:
            return "false_lead";
        case camp_lead_kind::frontier_probe:
            return "frontier_probe";
    }

    return "human_activity";
}

std::string to_string( camp_lead_status status )
{
    switch( status ) {
        case camp_lead_status::suspected:
            return "suspected";
        case camp_lead_status::scout_confirmed:
            return "scout_confirmed";
        case camp_lead_status::active:
            return "active";
        case camp_lead_status::harvested:
            return "harvested";
        case camp_lead_status::stale:
            return "stale";
        case camp_lead_status::invalidated:
            return "invalidated";
        case camp_lead_status::dangerous:
            return "dangerous";
    }

    return "suspected";
}

void member_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "npc_id", npc_id.get_value() );
    json.member( "npc_template_id", npc_template_id );
    json.member( "home_spawn_tile", home_spawn_tile );
    json.member( "state", to_string( state ) );
    json.member( "wounded_or_unready", wounded_or_unready );
    json.member( "last_writeback_summary", last_writeback_summary );
    json.end_object();
}

void member_record::deserialize( const JsonObject &jo )
{
    int raw_npc_id = -1;
    jo.read( "npc_id", raw_npc_id );
    npc_id.deserialize( raw_npc_id );
    jo.read( "npc_template_id", npc_template_id );
    jo.read( "home_spawn_tile", home_spawn_tile );
    std::string state_string = "at_home";
    jo.read( "state", state_string );
    state = member_state_from_string( state_string ).value_or( member_state::at_home );
    jo.read( "wounded_or_unready", wounded_or_unready );
    jo.read( "last_writeback_summary", last_writeback_summary );
}

void spawn_tile_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "tile", tile );
    json.member( "headcount", headcount );
    json.end_object();
}

void spawn_tile_record::deserialize( const JsonObject &jo )
{
    jo.read( "tile", tile );
    jo.read( "headcount", headcount );
}

void camp_map_lead::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "lead_id", lead_id );
    json.member( "kind", to_string( kind ) );
    json.member( "status", to_string( status ) );
    json.member( "target_id", target_id );
    json.member( "omt", omt );
    json.member( "radius_omt", radius_omt );
    json.member( "source_key", source_key );
    json.member( "source_summary", source_summary );
    json.member( "first_seen_minutes", first_seen_minutes );
    json.member( "last_seen_minutes", last_seen_minutes );
    json.member( "last_checked_minutes", last_checked_minutes );
    json.member( "last_scouted_minutes", last_scouted_minutes );
    json.member( "bounty", bounty );
    json.member( "threat", threat );
    json.member( "confidence", confidence );
    json.member( "threat_confirmed", threat_confirmed );
    json.member( "target_alert", target_alert );
    json.member( "scout_seen", scout_seen );
    json.member( "generated_by_this_camp_routine", generated_by_this_camp_routine );
    json.member( "prior_bandit_losses", prior_bandit_losses );
    json.member( "prior_defender_losses", prior_defender_losses );
    json.member( "times_checked_empty", times_checked_empty );
    json.member( "times_harvested", times_harvested );
    json.member( "last_outcome", last_outcome );
    json.end_object();
}

void camp_map_lead::deserialize( const JsonObject &jo )
{
    jo.read( "lead_id", lead_id );
    std::string kind_string = "human_activity";
    jo.read( "kind", kind_string );
    kind = camp_lead_kind_from_string( kind_string ).value_or( camp_lead_kind::human_activity );
    std::string status_string = "suspected";
    jo.read( "status", status_string );
    status = camp_lead_status_from_string( status_string ).value_or( camp_lead_status::suspected );
    jo.read( "target_id", target_id );
    jo.read( "omt", omt );
    jo.read( "radius_omt", radius_omt );
    jo.read( "source_key", source_key );
    jo.read( "source_summary", source_summary );
    jo.read( "first_seen_minutes", first_seen_minutes );
    jo.read( "last_seen_minutes", last_seen_minutes );
    jo.read( "last_checked_minutes", last_checked_minutes );
    jo.read( "last_scouted_minutes", last_scouted_minutes );
    jo.read( "bounty", bounty );
    jo.read( "threat", threat );
    jo.read( "confidence", confidence );
    jo.read( "threat_confirmed", threat_confirmed );
    jo.read( "target_alert", target_alert );
    jo.read( "scout_seen", scout_seen );
    jo.read( "generated_by_this_camp_routine", generated_by_this_camp_routine );
    jo.read( "prior_bandit_losses", prior_bandit_losses );
    jo.read( "prior_defender_losses", prior_defender_losses );
    jo.read( "times_checked_empty", times_checked_empty );
    jo.read( "times_harvested", times_harvested );
    jo.read( "last_outcome", last_outcome );
}

void camp_intelligence_map::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "schema_version", schema_version );
    json.member( "last_daily_cleanup_minutes", last_daily_cleanup_minutes );
    json.member( "next_near_tick_minutes", next_near_tick_minutes );
    json.member( "next_mid_tick_minutes", next_mid_tick_minutes );
    json.member( "next_far_tick_minutes", next_far_tick_minutes );
    json.member( "next_frontier_tick_minutes", next_frontier_tick_minutes );
    json.member( "known_radius_omt", known_radius_omt );
    json.member( "frontier_radius_omt", frontier_radius_omt );
    json.member( "leads", leads );
    json.end_object();
}

void camp_intelligence_map::deserialize( const JsonObject &jo )
{
    jo.read( "schema_version", schema_version );
    jo.read( "last_daily_cleanup_minutes", last_daily_cleanup_minutes );
    jo.read( "next_near_tick_minutes", next_near_tick_minutes );
    jo.read( "next_mid_tick_minutes", next_mid_tick_minutes );
    jo.read( "next_far_tick_minutes", next_far_tick_minutes );
    jo.read( "next_frontier_tick_minutes", next_frontier_tick_minutes );
    jo.read( "known_radius_omt", known_radius_omt );
    jo.read( "frontier_radius_omt", frontier_radius_omt );
    jo.read( "leads", leads );
}

camp_map_lead *camp_intelligence_map::find_lead( const std::string &lead_id )
{
    auto iter = std::find_if( leads.begin(), leads.end(), [&lead_id]( const camp_map_lead & lead ) {
        return lead.lead_id == lead_id;
    } );
    return iter != leads.end() ? &*iter : nullptr;
}

const camp_map_lead *camp_intelligence_map::find_lead( const std::string &lead_id ) const
{
    auto iter = std::find_if( leads.begin(), leads.end(), [&lead_id]( const camp_map_lead & lead ) {
        return lead.lead_id == lead_id;
    } );
    return iter != leads.end() ? &*iter : nullptr;
}

void site_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "site_id", site_id );
    json.member( "source_kind", to_string( source_kind ) );
    json.member( "site_kind", to_string( site_kind ) );
    json.member( "hostile_profile", to_string( effective_profile( *this ) ) );
    json.member( "source_id", source_id );
    json.member( "anchor", anchor );
    json.member( "headcount", headcount );
    json.member( "footprint", footprint );
    json.member( "members", members );
    json.member( "spawn_tiles", spawn_tiles );
    json.member( "active_group_id", active_group_id );
    json.member( "active_target_id", active_target_id );
    json.member( "active_target_omt", active_target_omt );
    json.member( "active_job_type", active_job_type );
    json.member( "active_sortie_started_minutes", active_sortie_started_minutes );
    json.member( "active_sortie_local_contact_minutes", active_sortie_local_contact_minutes );
    std::vector<int> raw_active_member_ids;
    raw_active_member_ids.reserve( active_member_ids.size() );
    for( const character_id &member_id : active_member_ids ) {
        raw_active_member_ids.push_back( member_id.get_value() );
    }
    json.member( "active_member_ids", raw_active_member_ids );
    json.member( "remembered_target_or_mark", remembered_target_or_mark );
    json.member( "remembered_threat_estimate", remembered_threat_estimate );
    json.member( "remembered_bounty_estimate", remembered_bounty_estimate );
    json.member( "remembered_retreat_bias", remembered_retreat_bias );
    json.member( "remembered_return_clock", remembered_return_clock );
    json.member( "remembered_pressure", bandit_pursuit_handoff::to_string( remembered_pressure ) );
    json.member( "known_recent_marks", known_recent_marks );
    json.member( "intelligence_map", intelligence_map );
    json.member( "last_shakedown_outcome", last_shakedown_outcome );
    json.member( "shakedown_last_demanded_value", shakedown_last_demanded_value );
    json.member( "shakedown_last_surrendered_value", shakedown_last_surrendered_value );
    json.member( "shakedown_last_reachable_value", shakedown_last_reachable_value );
    json.member( "shakedown_loot_value", shakedown_loot_value );
    json.member( "shakedown_defender_losses", shakedown_defender_losses );
    json.member( "shakedown_bandit_losses", shakedown_bandit_losses );
    json.member( "shakedown_anger", shakedown_anger );
    json.member( "shakedown_caution", shakedown_caution );
    json.member( "shakedown_basecamp_defenders_at_fight", shakedown_basecamp_defenders_at_fight );
    json.member( "shakedown_basecamp_defender_observation_pending",
                 shakedown_basecamp_defender_observation_pending );
    json.member( "shakedown_reopen_available", shakedown_reopen_available );
    json.member( "shakedown_reopen_used", shakedown_reopen_used );
    json.member( "retired_empty_site", retired_empty_site );
    json.member( "retirement_summary", retirement_summary );
    json.end_object();
}

void site_record::deserialize( const JsonObject &jo )
{
    jo.read( "site_id", site_id );
    std::string source_kind_string = "none";
    jo.read( "source_kind", source_kind_string );
    source_kind = anchor_source_kind_from_string( source_kind_string ).value_or( anchor_source_kind::none );
    std::string site_kind_string = "none";
    jo.read( "site_kind", site_kind_string );
    site_kind = owned_site_kind_from_string( site_kind_string ).value_or( owned_site_kind::none );
    std::string profile_string = "none";
    jo.read( "hostile_profile", profile_string );
    profile = hostile_site_profile_from_string( profile_string ).value_or( profile_for_site_kind( site_kind ) );
    if( profile == hostile_site_profile::none ) {
        profile = profile_for_site_kind( site_kind );
    }
    jo.read( "source_id", source_id );
    jo.read( "anchor", anchor );
    jo.read( "headcount", headcount );
    jo.read( "footprint", footprint );
    jo.read( "members", members );
    jo.read( "spawn_tiles", spawn_tiles );
    jo.read( "active_group_id", active_group_id );
    jo.read( "active_target_id", active_target_id );
    jo.read( "active_target_omt", active_target_omt );
    jo.read( "active_job_type", active_job_type );
    jo.read( "active_sortie_started_minutes", active_sortie_started_minutes );
    jo.read( "active_sortie_local_contact_minutes", active_sortie_local_contact_minutes );
    std::vector<int> raw_active_member_ids;
    jo.read( "active_member_ids", raw_active_member_ids );
    active_member_ids.clear();
    active_member_ids.reserve( raw_active_member_ids.size() );
    for( const int raw_member_id : raw_active_member_ids ) {
        character_id member_id;
        member_id.deserialize( raw_member_id );
        active_member_ids.push_back( member_id );
    }
    jo.read( "remembered_target_or_mark", remembered_target_or_mark );
    jo.read( "remembered_threat_estimate", remembered_threat_estimate );
    jo.read( "remembered_bounty_estimate", remembered_bounty_estimate );
    jo.read( "remembered_retreat_bias", remembered_retreat_bias );
    jo.read( "remembered_return_clock", remembered_return_clock );
    std::string remembered_pressure_string = "ample";
    jo.read( "remembered_pressure", remembered_pressure_string );
    remembered_pressure = remaining_return_pressure_state_from_string( remembered_pressure_string ).value_or(
                              bandit_pursuit_handoff::remaining_return_pressure_state::ample );
    jo.read( "known_recent_marks", known_recent_marks );
    const bool intelligence_map_was_present = jo.has_member( "intelligence_map" );
    if( intelligence_map_was_present ) {
        jo.read( "intelligence_map", intelligence_map );
    }
    migrate_scalar_memory_to_intelligence_map( *this, intelligence_map_was_present );
    jo.read( "last_shakedown_outcome", last_shakedown_outcome );
    jo.read( "shakedown_last_demanded_value", shakedown_last_demanded_value );
    jo.read( "shakedown_last_surrendered_value", shakedown_last_surrendered_value );
    jo.read( "shakedown_last_reachable_value", shakedown_last_reachable_value );
    jo.read( "shakedown_loot_value", shakedown_loot_value );
    jo.read( "shakedown_defender_losses", shakedown_defender_losses );
    jo.read( "shakedown_bandit_losses", shakedown_bandit_losses );
    jo.read( "shakedown_anger", shakedown_anger );
    jo.read( "shakedown_caution", shakedown_caution );
    jo.read( "shakedown_basecamp_defenders_at_fight", shakedown_basecamp_defenders_at_fight );
    jo.read( "shakedown_basecamp_defender_observation_pending",
             shakedown_basecamp_defender_observation_pending );
    jo.read( "shakedown_reopen_available", shakedown_reopen_available );
    jo.read( "shakedown_reopen_used", shakedown_reopen_used );
    jo.read( "retired_empty_site", retired_empty_site );
    jo.read( "retirement_summary", retirement_summary );
}

bool site_record::has_member( character_id target_npc_id ) const
{
    return std::any_of( members.begin(), members.end(), [target_npc_id]( const member_record & member ) {
        return member.npc_id == target_npc_id;
    } );
}

member_record *site_record::find_member( character_id target_npc_id )
{
    auto iter = std::find_if( members.begin(), members.end(), [target_npc_id]( const member_record & member ) {
        return member.npc_id == target_npc_id;
    } );
    return iter != members.end() ? &*iter : nullptr;
}

const member_record *site_record::find_member( character_id target_npc_id ) const
{
    auto iter = std::find_if( members.begin(), members.end(), [target_npc_id]( const member_record & member ) {
        return member.npc_id == target_npc_id;
    } );
    return iter != members.end() ? &*iter : nullptr;
}

spawn_tile_record *site_record::find_spawn_tile( const tripoint_abs_ms &tile )
{
    auto iter = std::find_if( spawn_tiles.begin(), spawn_tiles.end(), [&tile]( const spawn_tile_record & record ) {
        return record.tile == tile;
    } );
    return iter != spawn_tiles.end() ? &*iter : nullptr;
}

const spawn_tile_record *site_record::find_spawn_tile( const tripoint_abs_ms &tile ) const
{
    auto iter = std::find_if( spawn_tiles.begin(), spawn_tiles.end(), [&tile]( const spawn_tile_record & record ) {
        return record.tile == tile;
    } );
    return iter != spawn_tiles.end() ? &*iter : nullptr;
}

int site_record::count_members_in_state( member_state target_state ) const
{
    return static_cast<int>( std::count_if( members.begin(), members.end(),
    [target_state]( const member_record & member ) {
        return member.state == target_state;
    } ) );
}

int site_record::count_live_members() const
{
    return static_cast<int>( std::count_if( members.begin(), members.end(), []( const member_record & member ) {
        return counts_toward_live_headcount( member.state );
    } ) );
}

int site_record::count_home_side_signals() const
{
    int home_side_signals = count_members_in_state( member_state::at_home );
    home_side_signals += std::max( 0, headcount );
    for( const spawn_tile_record &spawn_tile : spawn_tiles ) {
        home_side_signals += std::max( 0, spawn_tile.headcount );
    }
    return home_side_signals;
}

int site_record::dispatchable_member_capacity() const
{
    if( retired_empty_site ) {
        return 0;
    }
    const int ready_at_home_members = static_cast<int>( std::count_if( members.begin(), members.end(),
    []( const member_record & member ) {
        return member.state == member_state::at_home && !member.wounded_or_unready;
    } ) );
    return std::max( 0, ready_at_home_members - required_home_reserve( *this ) );
}

bool site_record::has_active_outside_pressure() const
{
    return !active_group_id.empty() || !active_member_ids.empty() ||
           count_members_in_state( member_state::outbound ) > 0 ||
           count_members_in_state( member_state::local_contact ) > 0;
}

bool site_record::eligible_for_empty_site_retirement() const
{
    return !retired_empty_site && site_kind != owned_site_kind::none &&
           profile_for_site_kind( site_kind ) != hostile_site_profile::none &&
           count_home_side_signals() == 0 && !has_active_outside_pressure();
}

void world_state::clear()
{
    owner_id = "hells_raiders_live_owner_v0";
    sites.clear();
}

void world_state::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "owner_id", owner_id );
    json.member( "sites", sites );
    json.end_object();
}

void world_state::deserialize( const JsonObject &jo )
{
    clear();
    jo.read( "owner_id", owner_id );
    jo.read( "sites", sites );
}

site_record *world_state::find_site( const std::string &site_id )
{
    auto iter = std::find_if( sites.begin(), sites.end(), [&site_id]( const site_record & site ) {
        return site.site_id == site_id;
    } );
    return iter != sites.end() ? &*iter : nullptr;
}

const site_record *world_state::find_site( const std::string &site_id ) const
{
    auto iter = std::find_if( sites.begin(), sites.end(), [&site_id]( const site_record & site ) {
        return site.site_id == site_id;
    } );
    return iter != sites.end() ? &*iter : nullptr;
}

bool is_tracked_hostile_template( const std::string &npc_template_id )
{
    static const std::array<std::string, 9> tracked_templates = {
        "bandit",
        "thug",
        "bandit_trader",
        "bandit_quartermaster",
        "bandit_mechanic",
        "hells_raiders_boss",
        "cannibal_hunter",
        "cannibal_butcher",
        "cannibal_camp_leader",
    };

    return std::find( tracked_templates.begin(), tracked_templates.end(),
                      npc_template_id ) != tracked_templates.end();
}

std::optional<owned_site_kind> classify_tracked_source( anchor_source_kind source_kind,
        const std::string &source_id )
{
    switch( source_kind ) {
        case anchor_source_kind::overmap_special:
            if( source_id == "bandit_camp" ) {
                return owned_site_kind::bandit_camp;
            }
            if( source_id == "bandit_work_camp" ) {
                return owned_site_kind::bandit_work_camp;
            }
            if( source_id == "bandit_cabin" ) {
                return owned_site_kind::bandit_cabin;
            }
            if( source_id == "cannibal_camp" ) {
                return owned_site_kind::cannibal_camp;
            }
            break;
        case anchor_source_kind::map_extra:
            if( source_id == "mx_looters" ) {
                return owned_site_kind::looters;
            }
            if( source_id == "mx_bandits_block" ) {
                return owned_site_kind::bandits_block;
            }
            break;
        case anchor_source_kind::none:
            break;
    }

    return std::nullopt;
}

hostile_site_profile profile_for_site_kind( owned_site_kind site_kind )
{
    switch( site_kind ) {
        case owned_site_kind::bandit_camp:
        case owned_site_kind::bandit_work_camp:
        case owned_site_kind::bandit_cabin:
            return hostile_site_profile::camp_style;
        case owned_site_kind::cannibal_camp:
            return hostile_site_profile::cannibal_camp;
        case owned_site_kind::looters:
        case owned_site_kind::bandits_block:
            return hostile_site_profile::small_hostile_site;
        case owned_site_kind::none:
            return hostile_site_profile::none;
    }

    return hostile_site_profile::none;
}

footprint_snapshot make_special_footprint( const std::string &special_id,
        const tripoint_abs_omt &origin,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup )
{
    footprint_snapshot snapshot;
    snapshot.anchor = origin;
    snapshot.footprint.push_back( origin );

    if( !special_lookup ) {
        return snapshot;
    }

    snapshot.footprint.clear();
    const int radius = special_footprint_radius( special_id );
    for( int dx = -radius; dx <= radius; dx++ ) {
        for( int dy = -radius; dy <= radius; dy++ ) {
            const tripoint_abs_omt candidate( origin.x() + dx, origin.y() + dy, origin.z() );
            if( special_lookup( candidate ) == std::optional<std::string>( special_id ) ) {
                snapshot.footprint.push_back( candidate );
            }
        }
    }

    if( snapshot.footprint.empty() ) {
        snapshot.footprint.push_back( origin );
    }

    std::sort( snapshot.footprint.begin(), snapshot.footprint.end(), omt_less );
    snapshot.footprint.erase( std::unique( snapshot.footprint.begin(), snapshot.footprint.end() ),
                              snapshot.footprint.end() );
    snapshot.anchor = snapshot.footprint.front();
    return snapshot;
}

std::string make_site_id( anchor_source_kind source_kind, const std::string &source_id,
                          const tripoint_abs_omt &anchor )
{
    std::ostringstream out;
    out << to_string( source_kind ) << ':' << source_id << '@'
        << anchor.x() << ',' << anchor.y() << ',' << anchor.z();
    return out.str();
}

int abstract_roster_seed_for_site_kind( owned_site_kind site_kind )
{
    switch( site_kind ) {
        case owned_site_kind::bandit_camp:
        case owned_site_kind::bandit_work_camp:
            return 6;
        case owned_site_kind::cannibal_camp:
            return 5;
        case owned_site_kind::bandit_cabin:
            return 3;
        case owned_site_kind::looters:
        case owned_site_kind::bandits_block:
            return 2;
        case owned_site_kind::none:
            return 0;
    }

    return 0;
}

bool register_abstract_site( world_state &state, anchor_source_kind source_kind,
                             const std::string &source_id, const tripoint_abs_omt &origin,
                             const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup,
                             int abstract_headcount )
{
    const std::optional<owned_site_kind> site_kind = classify_tracked_source( source_kind, source_id );
    if( !site_kind ) {
        return false;
    }

    footprint_snapshot footprint;
    if( source_kind == anchor_source_kind::overmap_special ) {
        footprint = make_special_footprint( source_id, origin, special_lookup );
    } else {
        footprint.anchor = origin;
        footprint.footprint = { origin };
    }

    const std::string site_id = make_site_id( source_kind, source_id, footprint.anchor );
    site_record *site = state.find_site( site_id );
    if( site == nullptr ) {
        site_record new_site;
        new_site.site_id = site_id;
        new_site.source_kind = source_kind;
        new_site.site_kind = *site_kind;
        new_site.profile = profile_for_site_kind( *site_kind );
        new_site.source_id = source_id;
        new_site.anchor = footprint.anchor;
        new_site.footprint = footprint.footprint;
        new_site.headcount = std::max( 0, abstract_headcount );
        state.sites.push_back( new_site );
        return true;
    }

    if( site->retired_empty_site ) {
        return false;
    }

    if( site->footprint.size() < footprint.footprint.size() ) {
        site->footprint = footprint.footprint;
        site->anchor = footprint.anchor;
    }
    if( site->source_kind == anchor_source_kind::none ) {
        site->source_kind = source_kind;
    }
    if( site->site_kind == owned_site_kind::none ) {
        site->site_kind = *site_kind;
    }
    if( site->profile == hostile_site_profile::none ) {
        site->profile = profile_for_site_kind( site->site_kind );
    }
    if( site->source_id.empty() ) {
        site->source_id = source_id;
    }
    site->headcount = std::max( site->headcount, std::max( 0, abstract_headcount ) );
    return true;
}

bool claim_tracked_spawn( world_state &state, const std::string &npc_template_id,
                          character_id npc_id, const tripoint_abs_ms &spawn_tile,
                          const std::optional<std::string> &overmap_special_id,
                          const std::optional<std::string> &map_extra_id,
                          const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup )
{
    if( !is_tracked_hostile_template( npc_template_id ) ) {
        return false;
    }

    anchor_source_kind source_kind = anchor_source_kind::none;
    std::string source_id;
    std::optional<owned_site_kind> site_kind;

    if( overmap_special_id ) {
        site_kind = classify_tracked_source( anchor_source_kind::overmap_special, *overmap_special_id );
        if( site_kind ) {
            source_kind = anchor_source_kind::overmap_special;
            source_id = *overmap_special_id;
        }
    }

    if( !site_kind && map_extra_id ) {
        site_kind = classify_tracked_source( anchor_source_kind::map_extra, *map_extra_id );
        if( site_kind ) {
            source_kind = anchor_source_kind::map_extra;
            source_id = *map_extra_id;
        }
    }

    if( !site_kind ) {
        return false;
    }

    const tripoint_abs_omt spawn_omt = project_to<coords::omt>( spawn_tile );
    footprint_snapshot footprint;
    if( source_kind == anchor_source_kind::overmap_special ) {
        footprint = make_special_footprint( source_id, spawn_omt, special_lookup );
    } else {
        footprint.anchor = spawn_omt;
        footprint.footprint = { spawn_omt };
    }

    const std::string site_id = make_site_id( source_kind, source_id, footprint.anchor );
    site_record *site = state.find_site( site_id );
    if( site == nullptr ) {
        site_record new_site;
        new_site.site_id = site_id;
        new_site.source_kind = source_kind;
        new_site.site_kind = *site_kind;
        new_site.profile = profile_for_site_kind( *site_kind );
        new_site.source_id = source_id;
        new_site.anchor = footprint.anchor;
        new_site.footprint = footprint.footprint;
        state.sites.push_back( new_site );
        site = &state.sites.back();
    } else if( site->footprint.size() < footprint.footprint.size() ) {
        site->footprint = footprint.footprint;
        site->anchor = footprint.anchor;
    }
    if( site->profile == hostile_site_profile::none ) {
        site->profile = profile_for_site_kind( site->site_kind );
    }
    if( site->retired_empty_site ) {
        site->retired_empty_site = false;
        site->retirement_summary = "reactivated by tracked hostile spawn " + std::to_string( npc_id.get_value() );
    }

    if( site->has_member( npc_id ) ) {
        return true;
    }

    member_record member;
    member.npc_id = npc_id;
    member.npc_template_id = npc_template_id;
    member.home_spawn_tile = spawn_tile;
    site->members.push_back( member );
    site->headcount = std::max( site->headcount, site->count_live_members() );

    spawn_tile_record *spawn_tile_record_ptr = site->find_spawn_tile( spawn_tile );
    if( spawn_tile_record_ptr == nullptr ) {
        spawn_tile_record new_spawn_tile;
        new_spawn_tile.tile = spawn_tile;
        site->spawn_tiles.push_back( new_spawn_tile );
        spawn_tile_record_ptr = &site->spawn_tiles.back();
    }
    spawn_tile_record_ptr->headcount++;
    return true;
}


int ready_at_home_member_count( const bandit_live_world::site_record &site )
{
    return static_cast<int>( std::count_if( site.members.begin(), site.members.end(),
    []( const bandit_live_world::member_record & member ) {
        return member.state == member_state::at_home && !member.wounded_or_unready;
    } ) );
}

int wounded_or_unready_member_count( const bandit_live_world::site_record &site )
{
    return static_cast<int>( std::count_if( site.members.begin(), site.members.end(),
    []( const bandit_live_world::member_record & member ) {
        return counts_toward_live_headcount( member.state ) && member.wounded_or_unready;
    } ) );
}

int active_outside_member_count( const bandit_live_world::site_record &site )
{
    std::vector<character_id> outside_members;
    outside_members.reserve( site.active_member_ids.size() );
    for( const character_id &member_id : site.active_member_ids ) {
        if( std::find( outside_members.begin(), outside_members.end(), member_id ) == outside_members.end() ) {
            outside_members.push_back( member_id );
        }
    }
    for( const bandit_live_world::member_record &member : site.members ) {
        if( member.state != member_state::outbound && member.state != member_state::local_contact ) {
            continue;
        }
        if( std::find( outside_members.begin(), outside_members.end(), member.npc_id ) == outside_members.end() ) {
            outside_members.push_back( member.npc_id );
        }
    }
    return static_cast<int>( outside_members.size() );
}

int ceil_percent( const int value, const int percent )
{
    if( value <= 0 || percent <= 0 ) {
        return 0;
    }
    return ( value * percent + 99 ) / 100;
}

int camp_map_home_reserve_for_lead( const bandit_live_world::site_record &site,
                                    const bandit_live_world::camp_map_lead &lead,
                                    const int stockpile_pressure )
{
    int reserve = required_home_reserve( site );
    const int living_roster = site.count_live_members();
    const bool scout_confirmed_buddy_camp = effective_profile( site ) == hostile_site_profile::camp_style &&
            living_roster == 2 && lead.status == bandit_live_world::camp_lead_status::scout_confirmed;
    if( scout_confirmed_buddy_camp ) {
        reserve = 0;
    }
    if( effective_profile( site ) == hostile_site_profile::camp_style && !scout_confirmed_buddy_camp &&
        ( lead.prior_bandit_losses > 0 || lead.target_alert || lead.scout_seen ) ) {
        reserve += 1;
    }
    if( stockpile_pressure >= 3 ) {
        const int minimum_reserve = scout_confirmed_buddy_camp ? 0 : living_roster >= 5 ? 2 : 1;
        reserve = std::max( minimum_reserve, reserve - 1 );
    }
    return std::clamp( reserve, 0, living_roster );
}

int stalk_pressure_member_count( const int living_roster, const int dispatchable )
{
    if( dispatchable < 2 ) {
        return 0;
    }
    if( living_roster == 2 ) {
        return 2;
    }
    const int upper_bound = std::min( dispatchable, ceil_percent( living_roster, 35 ) );
    if( upper_bound < 2 ) {
        return 0;
    }
    return std::clamp( ceil_percent( dispatchable, 40 ), 2, upper_bound );
}

camp_map_dispatch_decision choose_camp_map_dispatch( const site_record &site,
        const camp_map_lead &lead, const camp_map_dispatch_pressure &pressure )
{
    camp_map_dispatch_decision decision;
    decision.valid = true;
    decision.living_roster = site.count_live_members();
    decision.ready_at_home = ready_at_home_member_count( site );
    decision.wounded_or_unready = wounded_or_unready_member_count( site );
    decision.active_outside = active_outside_member_count( site );
    decision.hard_home_reserve = camp_map_home_reserve_for_lead( site, lead,
                                 pressure.stockpile_pressure );
    decision.dispatchable = std::max( 0, decision.ready_at_home - decision.hard_home_reserve );

    decision.reward_score = std::max( 0, lead.bounty ) + std::max( 0, lead.confidence ) +
                            std::clamp( pressure.stockpile_pressure, 0, 3 ) +
                            std::min( 2, std::max( 0, lead.prior_defender_losses ) );
    if( pressure.opening_available ) {
        decision.reward_score += 1;
    }

    decision.risk_score = std::max( 0, lead.threat ) + std::max( 0, 2 - lead.confidence ) +
                          std::min( 4, std::max( 0, lead.prior_bandit_losses ) * 2 );
    if( lead.target_alert ) {
        decision.risk_score += 2;
    }
    if( lead.scout_seen ) {
        decision.risk_score += 1;
    }
    if( !pressure.opening_available ) {
        decision.risk_score += 2;
    }
    decision.margin = decision.reward_score - decision.risk_score;

    decision.notes.push_back( "camp-map decision uses ready roster, wounded/unready, reserve, and lead pressure" );
    decision.notes.push_back( "camp-map roster living=" + std::to_string( decision.living_roster ) +
                              " ready_at_home=" + std::to_string( decision.ready_at_home ) +
                              " wounded_or_unready=" + std::to_string( decision.wounded_or_unready ) +
                              " active_outside=" + std::to_string( decision.active_outside ) +
                              " reserve=" + std::to_string( decision.hard_home_reserve ) +
                              " dispatchable=" + std::to_string( decision.dispatchable ) );
    if( pressure.stockpile_pressure >= 3 ) {
        decision.notes.push_back( "stockpile pressure may loosen reserve by one but cannot cross hard minimum" );
    }
    if( effective_profile( site ) == hostile_site_profile::camp_style && decision.living_roster == 2 &&
        lead.status == camp_lead_status::scout_confirmed && decision.hard_home_reserve == 0 ) {
        decision.notes.push_back(
            "two-bandit camp: scout-confirmed pressure commits the buddy pair instead of preserving reserve" );
    }
    decision.notes.push_back( "camp-map opening_state=" + pressure.opening_state +
                              " opening_available=" + std::string( pressure.opening_available ? "yes" : "no" ) );
    if( lead.prior_bandit_losses > 0 || lead.target_alert || lead.scout_seen ) {
        decision.notes.push_back( "losses/alert add caution before greed can size the outing" );
    }

    if( site.retired_empty_site ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: retired empty site" );
        return decision;
    }
    if( site.has_active_outside_pressure() ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: unresolved active outside group/contact blocks dogpile" );
        return decision;
    }
    if( decision.dispatchable <= 0 ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: no ready members remain after reserve" );
        return decision;
    }

    if( !pressure.opening_available && lead.status == camp_lead_status::active ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: active stalk pressure found no opening and should return/decay" );
        return decision;
    }

    if( lead.confidence <= 1 || lead.status == camp_lead_status::stale ) {
        decision.intent = bandit_dry_run::job_template::scout;
        decision.selected_member_count = 1;
        decision.notes.push_back( "scout: low-confidence or stale memory needs eyes before pressure" );
        return decision;
    }

    if( decision.margin <= -2 || ( lead.threat >= lead.bounty + 2 && decision.margin <= 1 ) ) {
        decision.intent = bandit_dry_run::job_template::hold_chill;
        decision.notes.push_back( "hold: high threat or poor reward does not escalate by itself" );
        return decision;
    }

    if( decision.margin >= 2 ) {
        const int stalkers = stalk_pressure_member_count( decision.living_roster, decision.dispatchable );
        if( stalkers >= 2 ) {
            decision.intent = bandit_dry_run::job_template::stalk;
            decision.selected_member_count = stalkers;
            decision.notes.push_back( "stalk: remembered high-value lead permits larger-than-scout pressure" );
            return decision;
        }
        decision.intent = bandit_dry_run::job_template::scout;
        decision.selected_member_count = 1;
        decision.notes.push_back( "scout: pressure margin is good but reserve leaves no stalk pair" );
        return decision;
    }

    decision.intent = bandit_dry_run::job_template::hold_chill;
    decision.notes.push_back( "hold: marginal remembered lead waits for better evidence" );
    return decision;
}

const camp_map_lead *find_camp_map_dispatch_lead_for_target( const site_record &site,
        const tripoint_abs_omt &target_omt,
        const std::string &target_id )
{
    const camp_map_lead *best_lead = nullptr;
    int best_distance = 0;
    int best_score = 0;
    for( const camp_map_lead &lead : site.intelligence_map.leads ) {
        if( lead.target_id.empty() && lead.lead_id.empty() ) {
            continue;
        }
        if( lead.status == camp_lead_status::invalidated ||
            lead.status == camp_lead_status::harvested ||
            lead.status == camp_lead_status::dangerous ) {
            continue;
        }

        const bool target_matches = !target_id.empty() &&
                                    ( lead.target_id == target_id || lead.lead_id == target_id );
        const int radius = std::max( 2, lead.radius_omt );
        const bool omt_matches = lead.omt.z() == target_omt.z() && rl_dist( lead.omt, target_omt ) <= radius;
        if( !target_matches && !omt_matches ) {
            continue;
        }

        const int distance = rl_dist( site.anchor, lead.omt );
        const int score = std::max( 0, lead.confidence ) + std::max( 0, lead.bounty ) -
                          std::max( 0, lead.threat );
        if( best_lead == nullptr || distance < best_distance ||
            ( distance == best_distance && score > best_score ) ) {
            best_lead = &lead;
            best_distance = distance;
            best_score = score;
        }
    }
    return best_lead;
}

structural_bounty_read classify_structural_bounty_terrain( const std::string &overmap_terrain_id )
{
    const std::string id = lowercase_copy( overmap_terrain_id );
    structural_bounty_read read;
    read.terrain_class = "open";
    read.summary = "no structural bounty";

    if( id.empty() || contains_any_token( id, { "open", "field", "meadow", "road", "empty" } ) ) {
        return read;
    }

    if( contains_any_token( id, { "forest", "woods", "wood", "swamp", "wetland" } ) ) {
        read.terrain_class = "forest";
        read.bounty = 1;
        read.confidence = 1;
        read.latent_threat = 0;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "low structural forest/woods bounty";
        return read;
    }

    if( contains_any_token( id, { "downtown", "city", "mall", "office_tower", "apartment" } ) ) {
        read.terrain_class = "town";
        read.bounty = 3;
        read.confidence = 1;
        read.latent_threat = 2;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "medium-high structural urban bounty with latent threat";
        return read;
    }

    if( contains_any_token( id, { "town", "house", "home", "farm", "cabin", "building",
                                  "shop", "store", "garage", "shelter" } ) ) {
        read.terrain_class = "town";
        read.bounty = 2;
        read.confidence = 1;
        read.latent_threat = 1;
        read.radius_omt = 0;
        read.eligible = true;
        read.summary = "medium structural town/building bounty";
        return read;
    }

    return read;
}

std::string make_structural_bounty_lead_id( const std::string &site_id,
        const tripoint_abs_omt &omt, const std::string &terrain_class )
{
    std::ostringstream out;
    out << site_id << ":structural_bounty:" << omt.x() << ',' << omt.y() << ',' << omt.z()
        << ':' << ( terrain_class.empty() ? "unknown" : terrain_class );
    return out.str();
}

bool structural_bounty_memory_suppresses_refresh( const camp_intelligence_map &intelligence_map,
        const tripoint_abs_omt &omt, const std::string &terrain_class )
{
    const std::string terrain = terrain_class.empty() ? "unknown" : terrain_class;
    for( const camp_map_lead &lead : intelligence_map.leads ) {
        if( lead.omt != omt ) {
            continue;
        }
        if( lead.kind != camp_lead_kind::structural_bounty &&
            lead.kind != camp_lead_kind::harvested_site ) {
            continue;
        }
        if( !lead.target_id.empty() && lead.target_id != terrain ) {
            continue;
        }
        if( lead.status == camp_lead_status::harvested ||
            lead.status == camp_lead_status::dangerous ) {
            return true;
        }
    }
    return false;
}

bool structural_bounty_scan_memory_suppresses_refresh( const camp_intelligence_map &intelligence_map,
        const tripoint_abs_omt &omt, const std::string &terrain_class, const int now_minutes )
{
    if( structural_bounty_memory_suppresses_refresh( intelligence_map, omt, terrain_class ) ) {
        return true;
    }

    constexpr int recent_structural_check_cooldown_minutes = 6 * 60;
    const std::string terrain = terrain_class.empty() ? "unknown" : terrain_class;
    for( const camp_map_lead &lead : intelligence_map.leads ) {
        if( lead.omt != omt || lead.kind != camp_lead_kind::structural_bounty ) {
            continue;
        }
        if( !lead.target_id.empty() && lead.target_id != terrain ) {
            continue;
        }
        if( lead.last_checked_minutes >= 0 && now_minutes >= 0 &&
            now_minutes - lead.last_checked_minutes < recent_structural_check_cooldown_minutes ) {
            return true;
        }
    }
    return false;
}

bool upsert_structural_bounty_lead( site_record &site, const tripoint_abs_omt &omt,
                                   const structural_bounty_read &read, const int now_minutes )
{
    if( !read.eligible || read.bounty <= 0 ) {
        return false;
    }
    if( structural_bounty_memory_suppresses_refresh( site.intelligence_map, omt,
            read.terrain_class ) ) {
        return false;
    }

    camp_map_lead lead;
    lead.lead_id = make_structural_bounty_lead_id( site.site_id, omt, read.terrain_class );
    lead.kind = camp_lead_kind::structural_bounty;
    lead.status = camp_lead_status::suspected;
    lead.target_id = read.terrain_class;
    lead.omt = omt;
    lead.radius_omt = read.radius_omt;
    lead.source_key = "structural_bounty:" + read.terrain_class;
    lead.source_summary = read.summary;
    lead.first_seen_minutes = now_minutes;
    lead.last_seen_minutes = now_minutes;
    lead.bounty = std::max( 0, read.bounty );
    lead.threat = 0;
    lead.confidence = std::max( 0, read.confidence );
    lead.threat_confirmed = false;
    lead.last_outcome = "structural_bounty_suspected";

    if( camp_map_lead *existing = site.intelligence_map.find_lead( lead.lead_id ) ) {
        lead.first_seen_minutes = existing->first_seen_minutes;
        lead.times_checked_empty = existing->times_checked_empty;
        lead.times_harvested = existing->times_harvested;
        *existing = lead;
        return true;
    }
    site.intelligence_map.leads.push_back( lead );
    return true;
}

structural_bounty_scan_result advance_structural_bounty_scan( world_state &state,
        const int now_minutes, const int scan_budget,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &terrain_lookup )
{
    structural_bounty_scan_result result;
    result.scan_budget = std::max( 0, scan_budget );
    if( result.scan_budget == 0 ) {
        result.notes.push_back( "structural scan skipped: zero budget" );
        return result;
    }
    if( !terrain_lookup ) {
        result.notes.push_back( "structural scan skipped: no terrain lookup" );
        return result;
    }

    static const std::array<std::pair<int, int>, 12> near_offsets = { {
            { -4, 0 }, { 4, 0 }, { 0, -4 }, { 0, 4 },
            { -5, -1 }, { 5, 1 }, { -1, 5 }, { 1, -5 },
            { -6, 0 }, { 6, 0 }, { 0, -6 }, { 0, 6 },
        } };
    constexpr int per_site_near_sample_cap = 4;
    constexpr int near_scan_cadence_minutes = 60;
    constexpr int near_scan_radius_omt = 8;
    const int time_bucket = now_minutes >= 0 ? now_minutes / near_scan_cadence_minutes : 0;

    for( site_record &site : state.sites ) {
        if( result.budget_used >= result.scan_budget ) {
            result.budget_exhausted = true;
            break;
        }

        result.sites_considered++;
        if( effective_profile( site ) != hostile_site_profile::camp_style ) {
            result.sites_skipped_not_camp++;
            continue;
        }
        if( site.retired_empty_site ) {
            result.sites_skipped_retired++;
            continue;
        }
        if( site.has_active_outside_pressure() ) {
            result.sites_skipped_active_outside++;
            continue;
        }
        const bool has_ready_home_presence = ready_at_home_member_count( site ) > 0 ||
                                             site.count_home_side_signals() > 0;
        if( !has_ready_home_presence ) {
            result.sites_skipped_no_ready_home++;
            continue;
        }
        if( site.intelligence_map.next_near_tick_minutes >= 0 &&
            now_minutes >= 0 && now_minutes < site.intelligence_map.next_near_tick_minutes ) {
            result.sites_deferred_by_cadence++;
            continue;
        }

        int samples_for_site = 0;
        const int rotation = static_cast<int>( ( static_cast<long long>( std::max( 0, time_bucket ) ) *
                                                per_site_near_sample_cap ) % near_offsets.size() );
        for( int offset_index = 0;
             offset_index < static_cast<int>( near_offsets.size() ) &&
             samples_for_site < per_site_near_sample_cap && result.budget_used < result.scan_budget;
             offset_index++ ) {
            const std::pair<int, int> &offset = near_offsets[( rotation + offset_index ) %
                                               near_offsets.size()];
            const tripoint_abs_omt candidate( site.anchor.x() + offset.first,
                                              site.anchor.y() + offset.second, site.anchor.z() );
            result.candidates_sampled++;
            result.budget_used++;
            samples_for_site++;

            const std::optional<std::string> terrain_id = terrain_lookup( candidate );
            if( !terrain_id ) {
                continue;
            }
            const structural_bounty_read read = classify_structural_bounty_terrain( *terrain_id );
            if( !read.eligible || read.bounty <= 0 ) {
                continue;
            }
            if( structural_bounty_scan_memory_suppresses_refresh( site.intelligence_map, candidate,
                    read.terrain_class, now_minutes ) ) {
                result.leads_suppressed_by_memory++;
                continue;
            }
            if( upsert_structural_bounty_lead( site, candidate, read, now_minutes ) ) {
                result.leads_seeded++;
            }
        }

        if( samples_for_site > 0 ) {
            site.intelligence_map.known_radius_omt = std::max( site.intelligence_map.known_radius_omt,
                    near_scan_radius_omt );
            if( now_minutes >= 0 ) {
                site.intelligence_map.next_near_tick_minutes = now_minutes + near_scan_cadence_minutes;
            }
        }

        if( result.budget_used >= result.scan_budget ) {
            result.budget_exhausted = true;
        }
    }

    result.notes.push_back( "structural scan bounded to near-ring per-camp samples" );
    return result;
}

namespace
{
int structural_outing_stalking_delay_minutes( const site_record &site, const camp_map_lead &lead )
{
    const int distance = std::max( 1, rl_dist( site.anchor, lead.omt ) );
    return std::clamp( distance * 15, 30, 240 );
}

int structural_outing_arrival_delay_minutes( const site_record &site, const camp_map_lead &lead )
{
    const int distance = std::max( 1, rl_dist( site.anchor, lead.omt ) );
    return structural_outing_stalking_delay_minutes( site, lead ) + std::clamp( distance * 10, 30, 180 );
}

int structural_known_threat_for_interest( const camp_map_lead &lead )
{
    return lead.threat_confirmed ? std::max( 0, lead.threat ) : 0;
}

int structural_effective_interest( const camp_map_lead &lead, const int threat )
{
    return std::max( 0, lead.bounty ) + std::max( 0, lead.confidence ) - std::max( 0, threat );
}

bool structural_lead_recently_checked( const camp_map_lead &lead, const int now_minutes )
{
    constexpr int recent_structural_check_cooldown_minutes = 6 * 60;
    return lead.last_checked_minutes >= 0 && now_minutes >= 0 &&
           now_minutes - lead.last_checked_minutes < recent_structural_check_cooldown_minutes;
}

void clear_structural_active_group( site_record &site, const std::string &summary )
{
    for( const character_id &member_id : site.active_member_ids ) {
        update_member_state( site, member_id, member_state::at_home, summary );
    }
    site.active_group_id.clear();
    site.active_target_id.clear();
    site.active_target_omt = tripoint_abs_omt();
    site.active_job_type.clear();
    site.active_member_ids.clear();
    site.active_sortie_started_minutes = -1;
    site.active_sortie_local_contact_minutes = -1;
}
} // namespace

structural_outing_plan plan_structural_bounty_outing( const site_record &site,
        const camp_map_lead &lead, const int now_minutes )
{
    structural_outing_plan plan;
    plan.site_id = site.site_id;
    plan.lead_id = lead.lead_id;
    plan.target_omt = lead.omt;
    plan.known_threat = structural_known_threat_for_interest( lead );
    plan.effective_interest = structural_effective_interest( lead, plan.known_threat );

    if( effective_profile( site ) != hostile_site_profile::camp_style ) {
        plan.notes.push_back( "structural outing blocked: only camp-style bandit sites run routine structural outings" );
        return plan;
    }
    if( site.retired_empty_site ) {
        plan.notes.push_back( "structural outing blocked: retired empty site" );
        return plan;
    }
    if( site.has_active_outside_pressure() ) {
        plan.notes.push_back( "structural outing blocked: active outside group/contact blocks dogpile" );
        return plan;
    }
    if( lead.kind != camp_lead_kind::structural_bounty ) {
        plan.notes.push_back( "structural outing blocked: lead is not structural bounty" );
        return plan;
    }
    if( lead.status == camp_lead_status::active || lead.status == camp_lead_status::harvested ||
        lead.status == camp_lead_status::dangerous || lead.status == camp_lead_status::invalidated ) {
        plan.notes.push_back( "structural outing blocked: lead status suppresses dispatch" );
        return plan;
    }
    if( lead.bounty <= 0 ) {
        plan.notes.push_back( "structural outing blocked: no remaining structural bounty" );
        return plan;
    }
    if( structural_lead_recently_checked( lead, now_minutes ) ) {
        plan.notes.push_back( "structural outing blocked: recently checked structural lead is cooling down" );
        return plan;
    }
    if( plan.effective_interest <= 0 ) {
        plan.notes.push_back( "structural outing blocked: known threat cancels bounty interest" );
        return plan;
    }

    const int reserve = camp_map_home_reserve_for_lead( site, lead, 0 );
    const int ready = ready_at_home_member_count( site );
    const int dispatchable = std::max( 0, ready - reserve );
    if( dispatchable <= 0 ) {
        plan.notes.push_back( "structural outing blocked: no ready member remains after home reserve" );
        return plan;
    }

    plan.job = lead.target_id == "forest" ? bandit_dry_run::job_template::scavenge :
               bandit_dry_run::job_template::scout;
    plan.member_ids = select_dispatch_members( site, 1 );
    if( plan.member_ids.empty() ) {
        plan.notes.push_back( "structural outing blocked: no selectable at-home member" );
        return plan;
    }
    plan.expected_stalking_minutes = now_minutes >= 0 ?
                                     now_minutes + structural_outing_stalking_delay_minutes( site, lead ) : -1;
    plan.expected_arrival_minutes = now_minutes >= 0 ?
                                    now_minutes + structural_outing_arrival_delay_minutes( site, lead ) : -1;
    plan.valid = true;
    plan.notes.push_back( "structural outing candidate=" + lead.lead_id +
                          " bounty=" + std::to_string( lead.bounty ) +
                          " known_threat=" + std::to_string( plan.known_threat ) +
                          " confidence=" + std::to_string( lead.confidence ) +
                          " effective_interest=" + std::to_string( plan.effective_interest ) +
                          " decision=" + bandit_dry_run::to_string( plan.job ) );
    plan.notes.push_back( "structural outing is non-player camp routine traffic, not pursuit handoff" );
    return plan;
}

structural_outing_plan plan_structural_bounty_outing( const site_record &site, const int now_minutes )
{
    structural_outing_plan best;
    for( const camp_map_lead &lead : site.intelligence_map.leads ) {
        structural_outing_plan candidate = plan_structural_bounty_outing( site, lead, now_minutes );
        if( !candidate.valid ) {
            continue;
        }
        const int candidate_distance = rl_dist( site.anchor, candidate.target_omt );
        const int best_distance = best.valid ? rl_dist( site.anchor, best.target_omt ) : 0;
        if( !best.valid || candidate.effective_interest > best.effective_interest ||
            ( candidate.effective_interest == best.effective_interest && candidate_distance < best_distance ) ) {
            best = candidate;
        }
    }
    if( !best.valid ) {
        best.site_id = site.site_id;
        best.notes.push_back( "structural outing planner found no eligible structural bounty lead" );
    }
    return best;
}

bool apply_structural_bounty_outing_plan( site_record &site, const structural_outing_plan &plan,
        const int now_minutes )
{
    if( !plan.valid || plan.site_id != site.site_id || plan.lead_id.empty() || plan.member_ids.empty() ) {
        return false;
    }
    if( site.has_active_outside_pressure() ) {
        return false;
    }
    camp_map_lead *lead = site.intelligence_map.find_lead( plan.lead_id );
    if( lead == nullptr || lead->kind != camp_lead_kind::structural_bounty || lead->bounty <= 0 ||
        lead->status == camp_lead_status::active || lead->status == camp_lead_status::harvested ||
        lead->status == camp_lead_status::dangerous ||
        lead->status == camp_lead_status::invalidated ||
        structural_lead_recently_checked( *lead, now_minutes ) ) {
        return false;
    }
    if( structural_effective_interest( *lead, structural_known_threat_for_interest( *lead ) ) <= 0 ) {
        return false;
    }
    const int reserve = camp_map_home_reserve_for_lead( site, *lead, 0 );
    const int ready = ready_at_home_member_count( site );
    if( static_cast<int>( plan.member_ids.size() ) > std::max( 0, ready - reserve ) ) {
        return false;
    }
    std::vector<character_id> checked_member_ids;
    checked_member_ids.reserve( plan.member_ids.size() );
    for( const character_id &member_id : plan.member_ids ) {
        if( std::find( checked_member_ids.begin(), checked_member_ids.end(), member_id ) !=
            checked_member_ids.end() ) {
            return false;
        }
        const member_record *member = site.find_member( member_id );
        if( member == nullptr || member->state != member_state::at_home || member->wounded_or_unready ) {
            return false;
        }
        checked_member_ids.push_back( member_id );
    }

    const std::string summary = "structural " + bandit_dry_run::to_string( plan.job ) +
                                " outing toward " + plan.lead_id;
    for( const character_id &member_id : plan.member_ids ) {
        if( !update_member_state( site, member_id, member_state::outbound, summary ) ) {
            return false;
        }
    }
    site.active_group_id = site.site_id + "#structural";
    site.active_target_id = plan.lead_id;
    site.active_target_omt = plan.target_omt;
    site.active_job_type = bandit_dry_run::to_string( plan.job );
    site.active_member_ids = plan.member_ids;
    site.active_sortie_started_minutes = now_minutes;
    site.active_sortie_local_contact_minutes = -1;
    lead->status = camp_lead_status::active;
    lead->last_outcome = "structural_outing_active";
    return true;
}

structural_outing_result advance_structural_bounty_outings( world_state &state, const int now_minutes,
        const std::function<structural_threat_read( const site_record &, const camp_map_lead & )> &threat_lookup )
{
    structural_outing_result result;
    for( site_record &site : state.sites ) {
        result.sites_considered++;
        if( site.active_group_id != site.site_id + "#structural" || site.active_target_id.empty() ) {
            continue;
        }
        result.active_outings_considered++;
        camp_map_lead *lead = site.intelligence_map.find_lead( site.active_target_id );
        if( lead == nullptr || lead->kind != camp_lead_kind::structural_bounty ) {
            clear_structural_active_group( site, "structural outing cleared missing structural lead" );
            result.notes.push_back( "structural outing cleared: active target lead was missing" );
            continue;
        }
        if( site.active_sortie_started_minutes < 0 ) {
            site.active_sortie_started_minutes = now_minutes;
            continue;
        }

        const int elapsed = now_minutes - site.active_sortie_started_minutes;
        if( site.active_sortie_local_contact_minutes < 0 &&
            elapsed >= structural_outing_stalking_delay_minutes( site, *lead ) ) {
            structural_threat_read threat;
            if( threat_lookup ) {
                threat = threat_lookup( site, *lead );
            }
            lead->threat = std::max( 0, threat.threat );
            lead->threat_confirmed = true;
            lead->last_scouted_minutes = now_minutes;
            lead->last_checked_minutes = now_minutes;
            lead->source_summary = threat.summary.empty() ?
                                   "stalking-distance structural threat check" : threat.summary;
            site.active_sortie_local_contact_minutes = now_minutes;
            result.stalking_checks_processed++;

            const int effective_interest = structural_effective_interest( *lead, lead->threat );
            if( effective_interest <= 0 ) {
                const int returned = static_cast<int>( site.active_member_ids.size() );
                lead->status = lead->threat > 0 ? camp_lead_status::dangerous : camp_lead_status::stale;
                lead->last_outcome = "threat_revealed_lost_interest";
                clear_structural_active_group( site,
                                               "structural outing turned back before arrival after threat reveal" );
                result.lost_interest_returns++;
                result.members_returned += returned;
                result.notes.push_back( "structural outing turned back before arrival lead=" +
                                        lead->lead_id + " effective_interest=" +
                                        std::to_string( effective_interest ) );
                continue;
            }

            lead->status = camp_lead_status::scout_confirmed;
            lead->last_outcome = "threat_revealed_interest_survives";
            result.notes.push_back( "structural outing stalking check kept arrival open lead=" +
                                    lead->lead_id + " effective_interest=" +
                                    std::to_string( effective_interest ) );
            continue;
        }

        if( site.active_sortie_local_contact_minutes >= 0 &&
            elapsed >= structural_outing_arrival_delay_minutes( site, *lead ) ) {
            const int returned = static_cast<int>( site.active_member_ids.size() );
            lead->status = camp_lead_status::harvested;
            lead->bounty = 0;
            lead->times_harvested++;
            lead->last_checked_minutes = now_minutes;
            lead->last_outcome = "harvested_structural_bounty";
            clear_structural_active_group( site,
                                           "structural outing arrived and harvested structural bounty" );
            result.arrivals_processed++;
            result.members_returned += returned;
            result.notes.push_back( "structural outing harvested lead=" + lead->lead_id );
        }
    }
    return result;
}

structural_bounty_maintenance_result advance_structural_bounty_maintenance( world_state &state,
        const int now_minutes, const int scan_budget, const int dispatch_cap,
        const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &terrain_lookup,
        const std::function<structural_threat_read( const site_record &, const camp_map_lead & )> &threat_lookup )
{
    structural_bounty_maintenance_result result;
    result.dispatch_cap = std::max( 0, dispatch_cap );
    result.outing = advance_structural_bounty_outings( state, now_minutes, threat_lookup );
    result.scan = advance_structural_bounty_scan( state, now_minutes, scan_budget, terrain_lookup );

    if( result.dispatch_cap == 0 ) {
        result.notes.push_back( "structural maintenance dispatch skipped: zero cap" );
        return result;
    }

    for( site_record &site : state.sites ) {
        result.sites_considered_for_dispatch++;
        if( result.dispatches_applied >= result.dispatch_cap ) {
            result.dispatch_cap_reached = true;
            break;
        }
        const structural_outing_plan plan = plan_structural_bounty_outing( site, now_minutes );
        if( !plan.valid ) {
            continue;
        }
        result.dispatches_planned++;
        for( const std::string &note : plan.notes ) {
            result.notes.push_back( note );
        }
        if( apply_structural_bounty_outing_plan( site, plan, now_minutes ) ) {
            result.dispatches_applied++;
            result.notes.push_back( "structural maintenance dispatched site=" + site.site_id +
                                    " lead=" + plan.lead_id );
        } else {
            result.dispatches_blocked++;
            result.notes.push_back( "structural maintenance dispatch apply blocked site=" + site.site_id +
                                    " lead=" + plan.lead_id );
        }
    }

    return result;
}

std::string render_structural_bounty_maintenance_report(
    const structural_bounty_maintenance_result &result )
{
    std::ostringstream out;
    out << "bandit_live_world structural maintenance:"
        << " scan_budget=" << result.scan.scan_budget
        << " budget_used=" << result.scan.budget_used
        << " budget_exhausted=" << ( result.scan.budget_exhausted ? "yes" : "no" )
        << " sites_scanned=" << result.scan.sites_considered
        << " candidates_sampled=" << result.scan.candidates_sampled
        << " leads_seeded=" << result.scan.leads_seeded
        << " leads_suppressed=" << result.scan.leads_suppressed_by_memory
        << " dispatch_cap=" << result.dispatch_cap
        << " dispatches_planned=" << result.dispatches_planned
        << " dispatches_applied=" << result.dispatches_applied
        << " dispatch_cap_reached=" << ( result.dispatch_cap_reached ? "yes" : "no" )
        << " active_outings=" << result.outing.active_outings_considered
        << " stalking_checks=" << result.outing.stalking_checks_processed
        << " turnbacks=" << result.outing.lost_interest_returns
        << " arrivals=" << result.outing.arrivals_processed
        << " members_returned=" << result.outing.members_returned << '\n';
    for( const std::string &note : result.outing.notes ) {
        out << "- " << note << '\n';
    }
    for( const std::string &note : result.scan.notes ) {
        out << "- " << note << '\n';
    }
    for( const std::string &note : result.notes ) {
        out << "- " << note << '\n';
    }
    return out.str();
}

dispatch_plan plan_site_dispatch_from_camp_map_lead( const site_record &site,
        const camp_map_lead &lead,
        const camp_map_dispatch_pressure &pressure )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    dispatch_plan plan;
    plan.site_id = site.site_id;
    plan.profile = rules.profile;
    plan.target_id = lead.target_id.empty() ? lead.lead_id : lead.target_id;
    plan.target_omt = lead.omt;

    if( plan.target_id.empty() ) {
        plan.notes.push_back( "camp-map dispatch blocked: missing remembered target id" );
        return plan;
    }

    const camp_map_dispatch_decision decision = choose_camp_map_dispatch( site, lead, pressure );
    plan.notes = decision.notes;
    plan.notes.push_back( "camp-map dispatch lead=" + ( lead.lead_id.empty() ? plan.target_id : lead.lead_id ) +
                          " status=" + to_string( lead.status ) +
                          " bounty=" + std::to_string( lead.bounty ) +
                          " threat=" + std::to_string( lead.threat ) +
                          " confidence=" + std::to_string( lead.confidence ) +
                          " selected=" + bandit_dry_run::to_string( decision.intent ) );
    if( decision.intent == bandit_dry_run::job_template::hold_chill ||
        decision.selected_member_count <= 0 ) {
        plan.notes.push_back( "camp-map dispatch blocked: remembered risk/reward decision held pressure" );
        return plan;
    }

    plan.member_ids = select_dispatch_members( site, decision.selected_member_count );
    if( static_cast<int>( plan.member_ids.size() ) != decision.selected_member_count ) {
        plan.notes.push_back( "camp-map dispatch blocked: not enough at-home members survived selection" );
        return plan;
    }

    bandit_dry_run::candidate_debug winner = make_camp_map_dispatch_candidate( lead, decision );
    if( !bandit_pursuit_handoff::supports_pursuit_handoff( winner ) ) {
        plan.notes.push_back( "camp-map dispatch blocked: remembered decision stayed outside bounded scout/stalk handoff" );
        return plan;
    }

    plan.group = make_dispatch_group( site, plan.member_ids, plan.target_id );
    plan.group.current_threat_estimate = std::max( 0, lead.threat );
    plan.group.current_bounty_estimate = std::max( 0, lead.bounty );
    plan.group.confidence = std::max( plan.group.confidence, lead.confidence );
    bandit_pursuit_handoff::entry_context context;
    context.contact = rl_dist( site.anchor, plan.target_omt ) <= 4 ?
                      bandit_pursuit_handoff::contact_certainty::localized :
                      bandit_pursuit_handoff::contact_certainty::broad;
    plan.entry = bandit_pursuit_handoff::build_entry_payload( plan.group, winner, context );
    plan.notes = plan.entry.notes;
    if( !plan.entry.valid ) {
        plan.notes.push_back( "camp-map dispatch blocked: entry payload stayed outside the bounded handoff contract" );
        return plan;
    }

    plan.valid = true;
    plan.notes.push_back( "camp-map dispatch ready: " + bandit_dry_run::to_string( decision.intent ) +
                          " toward " + plan.target_id +
                          " members=" + std::to_string( plan.member_ids.size() ) +
                          " reserve=" + std::to_string( decision.hard_home_reserve ) +
                          " dispatchable=" + std::to_string( decision.dispatchable ) );
    plan.notes.push_back( "profile " + rules.id + ": " + rules.writeback_expectation );
    return plan;
}

dispatch_plan plan_site_dispatch( const site_record &site, const tripoint_abs_omt &target_omt,
                                  const std::string &target_id )
{
    const hostile_site_profile_rules rules = rules_for_profile( effective_profile( site ) );
    dispatch_plan plan;
    plan.site_id = site.site_id;
    plan.profile = rules.profile;
    plan.target_id = target_id;
    plan.target_omt = target_omt;

    if( target_id.empty() ) {
        plan.notes.push_back( "dispatch blocked: missing target id" );
        return plan;
    }

    if( site.retired_empty_site ) {
        plan.notes.push_back( "dispatch blocked: retired_empty_site" );
        return plan;
    }

    if( site.has_active_outside_pressure() ) {
        plan.notes.push_back( "dispatch blocked: site already has an active outside group/contact" );
        return plan;
    }

    const bandit_dry_run::camp_input camp = make_dispatch_camp_input( site );
    if( camp.available_manpower <= 0 ) {
        plan.notes.push_back( "dispatch blocked: no dispatchable at-home members remain after home reserve" );
        return plan;
    }

    const bandit_dry_run::lead_input lead = make_nearby_target_lead( site, target_omt, target_id );
    plan.evaluation = bandit_dry_run::evaluate( camp, { lead } );
    const bandit_dry_run::candidate_debug &winner = plan.evaluation.candidates[plan.evaluation.winner_index];
    if( !bandit_pursuit_handoff::supports_pursuit_handoff( winner ) ) {
        plan.notes.push_back( "dispatch blocked: " + plan.evaluation.winner_reason );
        return plan;
    }

    const int required_members = required_dispatch_members_for_profile( site, winner.job );
    if( required_members <= 0 ) {
        plan.notes.push_back( "dispatch blocked: winning job needs no live member handoff" );
        return plan;
    }
    if( rules.profile == hostile_site_profile::cannibal_camp && required_members > camp.available_manpower ) {
        plan.notes.push_back( "dispatch blocked: cannibal_camp pack pressure requires at least 2 at-home members after reserve" );
        return plan;
    }

    plan.member_ids = select_dispatch_members( site, required_members );
    if( static_cast<int>( plan.member_ids.size() ) != required_members ) {
        plan.notes.push_back( "dispatch blocked: not enough at-home members survived selection" );
        return plan;
    }

    plan.group = make_dispatch_group( site, plan.member_ids, target_id );
    bandit_pursuit_handoff::entry_context context;
    context.contact = rl_dist( site.anchor, target_omt ) <= 4 ?
                      bandit_pursuit_handoff::contact_certainty::localized :
                      bandit_pursuit_handoff::contact_certainty::broad;
    plan.entry = bandit_pursuit_handoff::build_entry_payload( plan.group, winner, context );
    plan.notes = plan.entry.notes;
    if( !plan.entry.valid ) {
        plan.notes.push_back( "dispatch blocked: entry payload stayed outside the bounded handoff contract" );
        return plan;
    }

    plan.valid = true;
    plan.notes.push_back( "profile " + rules.id + ": reserve " +
                          std::to_string( required_home_reserve( site ) ) +
                          ", retreat_floor " + std::to_string( rules.retreat_bias_floor ) +
                          ", return_clock_floor " + std::to_string( rules.return_clock_floor ) );
    plan.notes.push_back( "profile writeback: " + rules.writeback_expectation );
    if( rules.profile == hostile_site_profile::cannibal_camp ) {
        plan.notes.push_back( "cannibal_camp pack pressure: pack_size " +
                              std::to_string( plan.member_ids.size() ) +
                              ", available_after_reserve " + std::to_string( camp.available_manpower ) );
    }
    plan.notes.push_back( "dispatch ready: " + bandit_dry_run::to_string( winner.job ) + " toward " + target_id );
    return plan;
}

bool apply_dispatch_plan( site_record &site, const dispatch_plan &plan )
{
    if( !plan.valid || plan.site_id != site.site_id || plan.member_ids.empty() ) {
        return false;
    }

    for( const character_id &member_id : plan.member_ids ) {
        if( site.find_member( member_id ) == nullptr ) {
            return false;
        }
    }

    const std::string summary = "dispatch " + bandit_dry_run::to_string( plan.entry.job_type ) +
                                " toward " + plan.target_id;
    for( const character_id &member_id : plan.member_ids ) {
        if( !update_member_state( site, member_id, member_state::outbound, summary ) ) {
            return false;
        }
    }
    site.active_group_id = plan.entry.group_id;
    site.active_target_id = plan.target_id;
    site.active_target_omt = plan.target_omt;
    site.active_job_type = bandit_dry_run::to_string( plan.entry.job_type );
    site.active_member_ids = plan.member_ids;
    site.active_sortie_started_minutes = -1;
    site.active_sortie_local_contact_minutes = -1;
    site.remembered_target_or_mark = plan.entry.current_target_or_mark;
    site.remembered_threat_estimate = plan.group.current_threat_estimate;
    site.remembered_bounty_estimate = plan.group.current_bounty_estimate;
    site.remembered_retreat_bias = plan.group.retreat_bias;
    site.remembered_return_clock = plan.group.return_clock;
    site.remembered_pressure = plan.group.remaining_pressure;
    site.known_recent_marks = plan.group.known_recent_marks;
    return true;
}

local_gate_decision choose_local_gate_posture( const site_record &site,
        const local_gate_input &input )
{
    local_gate_decision decision;
    const hostile_site_profile profile = effective_profile( site );
    decision.dispatch_strength = static_cast<int>( site.active_member_ids.size() );
    decision.pressure_margin = decision.dispatch_strength + input.local_opportunity - input.local_threat;

    if( site.active_group_id.empty() || site.active_member_ids.empty() ) {
        decision.notes.push_back( "local gate blocked: no active owned outing is present" );
        return decision;
    }

    decision.valid = true;
    decision.notes.push_back( "active owned outing " + site.active_group_id + " toward " +
                              site.active_target_id );
    decision.notes.push_back( "inputs: strength " + std::to_string( decision.dispatch_strength ) +
                              ", threat " + std::to_string( input.local_threat ) +
                              ", opportunity " + std::to_string( input.local_opportunity ) +
                              ", margin " + std::to_string( decision.pressure_margin ) );
    const std::optional<bandit_dry_run::job_template> active_job =
        job_template_from_string( site.active_job_type );
    const bool cannibal_attack_intent = profile == hostile_site_profile::cannibal_camp &&
                                        active_job.has_value() &&
                                        cannibal_job_requires_attack_pack( *active_job );

    if( input.rolling_travel_scene ) {
        if( profile == hostile_site_profile::cannibal_camp &&
            ( !cannibal_attack_intent || decision.dispatch_strength < 2 ) ) {
            decision.posture = local_gate_posture::probe;
            decision.notes.push_back( "rolling travel cannibal scout/probe contact stays below attack until a pack attack intent exists" );
            return decision;
        }
        if( decision.pressure_margin >= 0 ) {
            decision.posture = local_gate_posture::attack_now;
            decision.combat_forward = true;
            decision.notes.push_back( "rolling travel scene skips polite shakedown and reads as an ambush window" );
            return decision;
        }
        decision.posture = local_gate_posture::probe;
        decision.notes.push_back( "rolling travel scene is tempting but still too protected for an immediate attack" );
        return decision;
    }

    if( decision.pressure_margin <= -3 ) {
        decision.posture = local_gate_posture::abort;
        decision.valid = false;
        decision.notes.push_back( "local gate aborts because local threat overwhelms dispatched pressure" );
        return decision;
    }

    if( profile == hostile_site_profile::cannibal_camp ) {
        const int cannibal_pressure_margin = decision.pressure_margin +
                                             ( input.darkness_or_concealment ? 1 : 0 );
        if( input.darkness_or_concealment ) {
            decision.notes.push_back( "darkness/concealment improves the cannibal killing window without overriding pack or threat gates" );
        }
        if( input.basecamp_or_camp_scene && ( input.current_exposure || input.recent_exposure ) ) {
            decision.posture = local_gate_posture::hold_off;
            decision.notes.push_back( "sight/exposure makes the cannibal camp hold off instead of continuing a visible beeline" );
            return decision;
        }
        if( input.local_contact_established &&
            ( !cannibal_attack_intent || decision.dispatch_strength < 2 ) ) {
            decision.posture = local_gate_posture::probe;
            decision.notes.push_back(
                "cannibal camp refuses to turn scout/probe contact or a lone hunter into the whole attack pack" );
            return decision;
        }
        if( input.local_contact_established && cannibal_attack_intent && decision.dispatch_strength >= 2 &&
            cannibal_pressure_margin >= 1 ) {
            decision.posture = local_gate_posture::attack_now;
            decision.combat_forward = true;
            decision.notes.push_back( "cannibal camp pressure does not negotiate; favorable pack contact becomes attack-to-kill pressure" );
            return decision;
        }
        if( input.basecamp_or_camp_scene && !input.darkness_or_concealment &&
            decision.pressure_margin <= 0 ) {
            decision.posture = local_gate_posture::hold_off;
            decision.notes.push_back( "daylight/no-cover camp pressure holds off instead of becoming a suicide rush" );
            return decision;
        }
        if( input.local_opportunity > 0 && cannibal_pressure_margin >= 0 ) {
            decision.posture = local_gate_posture::probe;
            decision.notes.push_back( "cannibal camp probes for a killing window instead of opening a shakedown" );
            return decision;
        }
        decision.posture = local_gate_posture::stalk;
        decision.notes.push_back( "cannibal camp pressure stalks until the kill window improves" );
        return decision;
    }

    if( input.basecamp_or_camp_scene &&
        ( input.current_exposure || input.recent_exposure || decision.pressure_margin <= 0 ) ) {
        decision.posture = local_gate_posture::hold_off;
        decision.notes.push_back( "camp-adjacent pressure holds off instead of collapsing onto the player tile" );
        return decision;
    }

    if( input.local_contact_established && decision.dispatch_strength >= 1 &&
        decision.pressure_margin >= 2 ) {
        decision.posture = local_gate_posture::open_shakedown;
        decision.opens_shakedown_surface = true;
        decision.notes.push_back( "contact is established and pressure is strong enough to open the later shakedown surface" );
        return decision;
    }

    if( input.local_opportunity > 0 && decision.pressure_margin >= 0 ) {
        decision.posture = local_gate_posture::probe;
        decision.notes.push_back( "opportunity is real but not yet strong enough for the robbery surface" );
        return decision;
    }

    decision.posture = local_gate_posture::stalk;
    decision.notes.push_back( "pressure stays readable as stalking until the scene changes" );
    return decision;
}

int ordinary_scout_watch_standoff_omt()
{
    return 2;
}

int minimum_hold_off_standoff_omt()
{
    return ordinary_scout_watch_standoff_omt();
}

tripoint_abs_omt choose_hold_off_standoff_goal( const tripoint_abs_omt &site_anchor,
        const tripoint_abs_omt &player_omt, const int requested_distance )
{
    const int desired_distance = std::max( requested_distance, minimum_hold_off_standoff_omt() );
    const int dx = ( site_anchor.x() > player_omt.x() ) -
                   ( site_anchor.x() < player_omt.x() );
    const int dy = ( site_anchor.y() > player_omt.y() ) -
                   ( site_anchor.y() < player_omt.y() );
    if( dx == 0 && dy == 0 ) {
        return player_omt;
    }
    return tripoint_abs_omt( player_omt.x() + dx * desired_distance,
                             player_omt.y() + dy * desired_distance, player_omt.z() );
}

std::string render_local_gate_report( const site_record &site, const local_gate_input &input,
                                      const local_gate_decision &decision )
{
    std::ostringstream out;
    out << "local_gate site=" << site.site_id
        << " active_group=" << site.active_group_id
        << " target=" << site.active_target_id
        << " active_job=" << ( site.active_job_type.empty() ? "unknown" : site.active_job_type )
        << " profile=" << to_string( effective_profile( site ) )
        << " posture=" << to_string( decision.posture )
        << " strength=" << decision.dispatch_strength
        << " pack_size=" << decision.dispatch_strength
        << " threat=" << input.local_threat
        << " opportunity=" << input.local_opportunity
        << " margin=" << decision.pressure_margin
        << " darkness_or_concealment=" << ( input.darkness_or_concealment ? "yes" : "no" )
        << " standoff_distance=" << input.standoff_distance
        << " basecamp_or_camp=" << ( input.basecamp_or_camp_scene ? "yes" : "no" )
        << " current_exposure=" << ( input.current_exposure ? "yes" : "no" )
        << " recent_exposure=" << ( input.recent_exposure ? "yes" : "no" )
        << " sight_exposure=" << ( input.current_exposure ? "current" :
                                      ( input.recent_exposure ? "recent" : "none" ) )
        << " local_contact=" << ( input.local_contact_established ? "yes" : "no" )
        << " rolling_travel=" << ( input.rolling_travel_scene ? "yes" : "no" )
        << " shakedown=" << ( decision.opens_shakedown_surface ? "yes" : "no" )
        << " combat_forward=" << ( decision.combat_forward ? "yes" : "no" )
        << '\n';
    for( const std::string &note : decision.notes ) {
        out << "- " << note << '\n';
    }
    return out.str();
}

sight_avoid_decision choose_sight_avoid_reposition( const tripoint_abs_ms &current_tile,
        const bool current_exposure, const bool recent_exposure,
        const std::vector<sight_avoid_candidate> &candidates )
{
    sight_avoid_decision decision;
    decision.valid = true;
    decision.destination = current_tile;

    if( !current_exposure && !recent_exposure ) {
        decision.reason = "still stalking";
        decision.notes.push_back( "sight_avoid: no current or recent exposure, no reposition needed" );
        return decision;
    }

    int best_score = -1000000;
    std::optional<sight_avoid_candidate> best_candidate;
    for( const sight_avoid_candidate &candidate : candidates ) {
        if( !candidate.passable || candidate.tile == current_tile || rl_dist( candidate.tile, current_tile ) > 1 ) {
            continue;
        }
        int score = candidate.cover_score;
        if( !candidate.visible_to_player ) {
            score += 80;
        }
        if( !candidate.visible_to_camp ) {
            score += 40;
        }
        if( candidate.visible_to_player && candidate.visible_to_camp ) {
            score -= 60;
        }
        if( score > best_score ) {
            best_score = score;
            best_candidate = candidate;
        }
    }

    if( !best_candidate.has_value() ) {
        decision.reason = "still stalking";
        decision.notes.push_back( "sight_avoid: exposed but no adjacent passable local reposition candidate" );
        return decision;
    }

    const bool breaks_player_sight = !best_candidate->visible_to_player;
    const bool breaks_camp_sight = !best_candidate->visible_to_camp;
    if( !breaks_player_sight && !breaks_camp_sight && best_candidate->cover_score <= 0 ) {
        decision.reason = "still stalking";
        decision.notes.push_back( "sight_avoid: exposed but adjacent candidates do not improve cover or line of sight" );
        return decision;
    }

    decision.repositions = true;
    decision.destination = best_candidate->tile;
    decision.reason = "repositioning because exposed";
    decision.notes.push_back( "sight_avoid: exposed -> bounded adjacent reposition" );
    decision.notes.push_back( std::string( "sight_avoid: breaks_player_los=" ) +
                              ( breaks_player_sight ? "yes" : "no" ) +
                              " breaks_camp_los=" + ( breaks_camp_sight ? "yes" : "no" ) +
                              " cover_score=" + std::to_string( best_candidate->cover_score ) );
    return decision;
}

bool note_active_sortie_started( site_record &site, const int current_minutes )
{
    if( site.active_group_id.empty() || site.active_member_ids.empty() || current_minutes < 0 ||
        site.active_sortie_started_minutes >= 0 ) {
        return false;
    }
    site.active_sortie_started_minutes = current_minutes;
    return true;
}

bool note_active_sortie_local_contact( site_record &site, const int current_minutes )
{
    if( site.active_group_id.empty() || site.active_member_ids.empty() || current_minutes < 0 ||
        site.active_sortie_local_contact_minutes >= 0 ) {
        return false;
    }
    site.active_sortie_local_contact_minutes = current_minutes;
    return true;
}

int ordinary_scout_sortie_limit_minutes()
{
    return 720;
}

bool scout_sortie_should_return_home( const site_record &site, const int current_minutes,
                                      const int sortie_limit_minutes )
{
    if( site.active_group_id.empty() || site.active_member_ids.size() != 1 ||
        sortie_limit_minutes <= 0 || current_minutes < 0 ||
        site.last_shakedown_outcome == "fight_unresolved" ) {
        return false;
    }

    const std::optional<bandit_dry_run::job_template> active_job =
        job_template_from_string( site.active_job_type );
    if( active_job.has_value() && *active_job != bandit_dry_run::job_template::scout &&
        *active_job != bandit_dry_run::job_template::hold_chill ) {
        return false;
    }

    const int anchor_minutes = site.active_sortie_local_contact_minutes >= 0 ?
                               site.active_sortie_local_contact_minutes : site.active_sortie_started_minutes;
    return anchor_minutes >= 0 && current_minutes - anchor_minutes >= sortie_limit_minutes;
}

shakedown_surface build_shakedown_surface( const site_record &site, const local_gate_input &input,
        const local_gate_decision &decision, const shakedown_goods_pool &goods_pool )
{
    shakedown_surface surface;

    if( !decision.valid || !decision.opens_shakedown_surface ||
        decision.posture != local_gate_posture::open_shakedown ) {
        surface.notes.push_back( "shakedown blocked: local gate did not open the robbery surface" );
        return surface;
    }

    if( effective_profile( site ) == hostile_site_profile::cannibal_camp ) {
        surface.notes.push_back( "shakedown blocked: cannibal camp profile attacks to kill instead of extorting" );
        return surface;
    }

    if( input.rolling_travel_scene ) {
        surface.notes.push_back( "shakedown blocked: rolling travel scene remains a direct-ambush context" );
        return surface;
    }

    surface.includes_basecamp_inventory = input.basecamp_or_camp_scene ||
                                          goods_pool.basecamp_or_camp_scene;
    surface.includes_vehicle_inventory = !surface.includes_basecamp_inventory;
    surface.reachable_goods_value = goods_pool.player_carried_value +
                                    goods_pool.companion_carried_value;
    if( surface.includes_basecamp_inventory ) {
        surface.reachable_goods_value += goods_pool.reachable_basecamp_value;
        surface.notes.push_back( "pool includes player, nearby companion, and reachable Basecamp goods" );
    } else {
        surface.reachable_goods_value += goods_pool.vehicle_carried_value;
        surface.notes.push_back( "pool includes player, companion, and current vehicle goods only" );
    }

    if( surface.reachable_goods_value <= 0 ) {
        surface.notes.push_back( "shakedown blocked: no honest reachable goods are present" );
        return surface;
    }

    const shakedown_opening_beat opening = choose_shakedown_opening_beat( site, input, decision );
    surface.opening_id = opening.id;
    surface.opening_summary = opening.summary;
    surface.bark = opening.bark;

    surface.valid = true;
    surface.pay_available = true;
    surface.fight_available = true;
    const int base_demanded_value = std::max( 1, ( surface.reachable_goods_value * 35 + 99 ) / 100 );
    const int demand_modifier_percent = shakedown_demand_modifier_percent( site );
    surface.demanded_value = ( base_demanded_value * demand_modifier_percent + 99 ) / 100;
    surface.demanded_value = std::clamp( surface.demanded_value, 1, surface.reachable_goods_value );
    if( site.shakedown_reopen_available && !site.shakedown_reopen_used ) {
        surface.notes.push_back( "renegotiation reopen: previous defender loss raises this one bounded demand" );
    } else if( demand_modifier_percent < 100 ) {
        surface.notes.push_back( "aftermath caution: previous bandit loss cools or shrinks this demand" );
    }
    surface.notes.push_back( "scenic opening beat: " + surface.opening_summary );
    surface.notes.push_back( "visible responses are Pay/Fight only; backout enters the fight/refusal branch" );
    surface.notes.push_back( "pay branch opens a trade/debt-style payment selector before any goods are surrendered" );
    surface.notes.push_back( "fight branch stays explicit whenever this surface is invoked" );
    surface.notes.push_back( "source site " + site.site_id + " opened the surface from " +
                             site.active_group_id );
    return surface;
}

std::string render_shakedown_surface_report( const site_record &site,
        const shakedown_surface &surface )
{
    std::ostringstream out;
    out << "shakedown_surface site=" << site.site_id
        << " active_group=" << site.active_group_id
        << " profile=" << to_string( effective_profile( site ) )
        << " posture=open_shakedown"
        << " valid=" << ( surface.valid ? "yes" : "no" )
        << " pay_option=" << ( surface.pay_available ? "yes" : "no" )
        << " fight_option=" << ( surface.fight_available ? "yes" : "no" )
        << " visible_responses=pay/fight"
        << " payment_surface=trade_debt_selector"
        << " reachable_goods=" << surface.reachable_goods_value
        << " demanded_toll=" << surface.demanded_value
        << " basecamp_inventory=" << ( surface.includes_basecamp_inventory ? "yes" : "no" )
        << " vehicle_inventory=" << ( surface.includes_vehicle_inventory ? "yes" : "no" )
        << " opening=" << ( surface.opening_id.empty() ? "none" : surface.opening_id )
        << " bark=\"" << surface.bark << "\"\n";
    for( const std::string &note : surface.notes ) {
        out << "- " << note << '\n';
    }
    return out.str();
}


shakedown_aftermath_effect apply_shakedown_outcome( site_record &site,
        const shakedown_outcome &outcome )
{
    shakedown_aftermath_effect effect;
    if( ( !outcome.paid && !outcome.fought ) || outcome.demanded_value <= 0 ) {
        effect.notes.push_back( "shakedown aftermath ignored: no concrete paid/fought outcome" );
        return effect;
    }

    effect.valid = true;
    site.last_shakedown_outcome = shakedown_outcome_label( outcome );
    site.shakedown_last_demanded_value = outcome.demanded_value;
    site.shakedown_last_surrendered_value = outcome.surrendered_value;
    site.shakedown_last_reachable_value = outcome.reachable_goods_value;

    if( outcome.paid ) {
        site.shakedown_loot_value += std::max( 0, outcome.surrendered_value );
        site.remembered_bounty_estimate += std::max( 1, outcome.surrendered_value / 1000 );
        effect.notes.push_back( "paid shakedown writes surrendered value into abstract bounty" );
    }

    if( outcome.fought ) {
        site.shakedown_anger += 1 + outcome.defender_losses;
        site.remembered_threat_estimate = std::max( 0,
                                          site.remembered_threat_estimate - outcome.defender_losses );
        effect.notes.push_back( "fight outcome writes anger and changed local threat into site memory" );
    }

    if( outcome.basecamp_or_camp_scene && outcome.defender_losses > 0 ) {
        site.shakedown_defender_losses += outcome.defender_losses;
        if( !site.shakedown_reopen_used ) {
            site.shakedown_reopen_available = true;
            effect.stronger_reopen = true;
            effect.notes.push_back( "defender loss opens exactly one stronger renegotiation demand" );
        }
    }

    if( outcome.bandit_losses > 0 || outcome.extraction_failed ) {
        site.shakedown_bandit_losses += outcome.bandit_losses;
        site.shakedown_caution += std::max( 1, outcome.bandit_losses );
        site.remembered_retreat_bias += std::max( 1, outcome.bandit_losses );
        site.remembered_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
        effect.cools_later_pressure = true;
        effect.notes.push_back( "bandit loss or failed extraction cools later pressure instead of only escalating" );
    }

    effect.demand_modifier_percent = shakedown_demand_modifier_percent( site );
    return effect;
}


void begin_shakedown_basecamp_defender_observation( site_record &site, const int live_defenders )
{
    site.shakedown_basecamp_defenders_at_fight = std::max( 0, live_defenders );
    site.shakedown_basecamp_defender_observation_pending =
        site.shakedown_basecamp_defenders_at_fight > 0;
}

shakedown_aftermath_effect apply_shakedown_basecamp_defender_observation( site_record &site,
        const int live_defenders )
{
    if( !site.shakedown_basecamp_defender_observation_pending ) {
        shakedown_aftermath_effect effect;
        effect.notes.push_back( "basecamp defender observation ignored: no pending shakedown fight" );
        return effect;
    }

    const int current_live_defenders = std::max( 0, live_defenders );
    const int defender_losses = site.shakedown_basecamp_defenders_at_fight - current_live_defenders;
    if( defender_losses <= 0 ) {
        shakedown_aftermath_effect effect;
        effect.notes.push_back( "basecamp defender observation unchanged: no defender strength drop" );
        return effect;
    }

    shakedown_outcome outcome;
    outcome.fought = true;
    outcome.basecamp_or_camp_scene = true;
    outcome.demanded_value = std::max( 1, site.shakedown_last_demanded_value );
    outcome.reachable_goods_value = site.shakedown_last_reachable_value;
    outcome.defender_losses = defender_losses;
    site.shakedown_basecamp_defender_observation_pending = false;
    return apply_shakedown_outcome( site, outcome );
}

bool mark_shakedown_reopen_used( site_record &site )
{
    if( !site.shakedown_reopen_available || site.shakedown_reopen_used ) {
        return false;
    }
    site.shakedown_reopen_used = true;
    site.shakedown_reopen_available = false;
    return true;
}

bool record_live_signal_mark( site_record &site, const live_signal_mark &mark )
{
    if( site.retired_empty_site || mark.mark_id.empty() || mark.range_cap_omt <= 0 ) {
        return false;
    }

    bool changed = site.remembered_target_or_mark != mark.mark_id;
    site.remembered_target_or_mark = mark.mark_id;
    const int old_bounty = site.remembered_bounty_estimate;
    const int old_threat = site.remembered_threat_estimate;
    site.remembered_bounty_estimate = std::max( site.remembered_bounty_estimate, mark.bounty_add );
    site.remembered_threat_estimate = std::max( site.remembered_threat_estimate, mark.threat_add );
    changed |= site.remembered_bounty_estimate != old_bounty;
    changed |= site.remembered_threat_estimate != old_threat;

    if( std::find( site.known_recent_marks.begin(), site.known_recent_marks.end(), mark.mark_id ) ==
        site.known_recent_marks.end() ) {
        static constexpr size_t max_live_signal_marks = 8;
        if( site.known_recent_marks.size() >= max_live_signal_marks ) {
            site.known_recent_marks.erase( site.known_recent_marks.begin() );
        }
        site.known_recent_marks.push_back( mark.mark_id );
        changed = true;
    }

    camp_map_lead lead;
    lead.kind = signal_kind_to_camp_lead_kind( mark.kind );
    lead.status = camp_lead_status::suspected;
    lead.target_id = mark.mark_id;
    lead.omt = mark.source_omt;
    lead.radius_omt = mark.range_cap_omt;
    lead.source_key = mark.mark_id;
    lead.source_summary = "live " + mark.kind + " signal mark";
    lead.last_seen_minutes = -1;
    lead.bounty = std::max( 0, mark.bounty_add );
    lead.threat = std::max( 0, mark.threat_add );
    lead.confidence = std::max( 1, mark.confidence );
    lead.threat_confirmed = lead.threat > 0;
    lead.generated_by_this_camp_routine = true;
    lead.last_outcome = "live_signal";
    lead.lead_id = camp_lead_id_for( site.site_id, lead.kind, lead.target_id, lead.omt );
    const camp_map_lead *old_lead = site.intelligence_map.find_lead( lead.lead_id );
    changed |= old_lead == nullptr || old_lead->bounty != lead.bounty ||
               old_lead->threat != lead.threat || old_lead->confidence != lead.confidence ||
               old_lead->radius_omt != lead.radius_omt;
    upsert_camp_map_lead( site.intelligence_map, lead );

    return changed;
}

std::string render_empty_site_retirement_report( const site_record &site )
{
    int spawn_tile_headcount = 0;
    for( const spawn_tile_record &spawn_tile : site.spawn_tiles ) {
        spawn_tile_headcount += std::max( 0, spawn_tile.headcount );
    }

    std::ostringstream out;
    out << "bandit_live_world retired_empty_site: site=" << site.site_id
        << " site_kind=" << to_string( site.site_kind )
        << " headcount=" << site.headcount
        << " at_home=" << site.count_members_in_state( member_state::at_home )
        << " spawn_tile_headcount=" << spawn_tile_headcount
        << " active_group=" << ( site.active_group_id.empty() ? "no" : site.active_group_id )
        << " active_member_ids=" << site.active_member_ids.size()
        << " outbound=" << site.count_members_in_state( member_state::outbound )
        << " local_contact=" << site.count_members_in_state( member_state::local_contact )
        << " home_side_signals=" << site.count_home_side_signals()
        << " active_outside=" << ( site.has_active_outside_pressure() ? "yes" : "no" );
    return out.str();
}

int retire_empty_hostile_sites( world_state &state, std::vector<std::string> *reports )
{
    int retired_count = 0;
    for( site_record &site : state.sites ) {
        if( !site.eligible_for_empty_site_retirement() ) {
            continue;
        }
        site.retired_empty_site = true;
        site.retirement_summary = render_empty_site_retirement_report( site );
        if( reports != nullptr ) {
            reports->push_back( site.retirement_summary );
        }
        retired_count++;
    }
    return retired_count;
}

bool apply_return_packet( site_record &site, const bandit_pursuit_handoff::return_packet &packet )
{
    if( !packet.valid || packet.source_camp_id != site.site_id ||
        site.active_group_id.empty() || packet.group_id != site.active_group_id ||
        site.active_member_ids.empty() ) {
        return false;
    }

    const auto status_for_member = [&packet]( character_id member_id ) -> std::string {
        const std::string member_token = std::to_string( member_id.get_value() );
        const auto iter = std::find_if( packet.anchored_identity_updates.begin(),
        packet.anchored_identity_updates.end(), [&member_token]( const bandit_pursuit_handoff::anchored_identity_state & update ) {
            return update.id == member_token;
        } );
        return iter != packet.anchored_identity_updates.end() ? iter->status : "alive";
    };

    int survivors_accounted = 0;
    const std::string target_label = packet.current_target_or_mark.empty() ? site.active_target_id :
                                     packet.current_target_or_mark;
    const std::string base_summary = "return " + bandit_pursuit_handoff::to_string( packet.result ) +
                                     " from " + target_label;
    bool shakedown_fight_outing = site.last_shakedown_outcome == "fight_unresolved";
    for( const character_id &member_id : site.active_member_ids ) {
        const member_record *member = site.find_member( member_id );
        shakedown_fight_outing |= member != nullptr &&
                                  member->last_writeback_summary.find( "shakedown_surface fight" ) != std::string::npos;
    }
    for( const character_id &member_id : site.active_member_ids ) {
        const std::string status = status_for_member( member_id );
        member_state new_state = member_state::at_home;
        std::string summary = base_summary;
        if( status == "dead" ) {
            new_state = member_state::dead;
            summary += " (dead)";
        } else if( status == "missing" ) {
            new_state = member_state::missing;
            summary += " (missing)";
        } else {
            survivors_accounted++;
            if( status != "alive" ) {
                summary += " (" + status + ")";
            }
        }
        if( !update_member_state( site, member_id, new_state, summary ) ) {
            return false;
        }
    }

    if( survivors_accounted != packet.survivors_remaining ) {
        return false;
    }

    int bandit_losses = 0;
    for( const bandit_pursuit_handoff::anchored_identity_state &update :
         packet.anchored_identity_updates ) {
        if( update.status == "dead" || update.status == "missing" ) {
            bandit_losses++;
        }
    }
    if( shakedown_fight_outing && bandit_losses > 0 ) {
        shakedown_outcome outcome;
        outcome.fought = true;
        outcome.extraction_failed = true;
        outcome.demanded_value = std::max( 1, site.shakedown_last_demanded_value );
        outcome.reachable_goods_value = site.shakedown_last_reachable_value;
        outcome.bandit_losses = bandit_losses;
        apply_shakedown_outcome( site, outcome );
    }

    record_scout_return_lead( site, packet, bandit_losses );

    bandit_pursuit_handoff::abstract_group_state remembered_group = make_site_memory_group( site );
    bandit_pursuit_handoff::apply_return_packet( remembered_group, packet );
    apply_group_memory( site, remembered_group );

    site.active_group_id.clear();
    site.active_target_id.clear();
    site.active_target_omt = tripoint_abs_omt();
    site.active_job_type.clear();
    site.active_member_ids.clear();
    site.active_sortie_started_minutes = -1;
    site.active_sortie_local_contact_minutes = -1;
    return true;
}

std::optional<bandit_pursuit_handoff::return_packet> resolve_active_group_aftermath(
    const site_record &site, const std::vector<active_member_observation> &observations )
{
    if( site.active_group_id.empty() || site.active_member_ids.empty() ||
        observations.size() != site.active_member_ids.size() ) {
        return std::nullopt;
    }

    bandit_pursuit_handoff::return_packet packet;
    packet.valid = true;
    packet.group_id = site.active_group_id;
    packet.source_camp_id = site.site_id;
    packet.job_type = job_template_from_string( site.active_job_type ).value_or(
                          bandit_dry_run::job_template::hold_chill );
    packet.current_target_or_mark = site.active_target_id;
    packet.result = bandit_pursuit_handoff::mission_result::withdrawn;
    packet.resolution = bandit_pursuit_handoff::lead_resolution::target_lost;
    packet.posture = bandit_pursuit_handoff::return_posture::escape_home;
    packet.remaining_pressure = rules_for_profile( effective_profile( site ) ).default_remaining_pressure;

    bool saw_loss = false;
    for( const character_id &member_id : site.active_member_ids ) {
        const auto iter = std::find_if( observations.begin(), observations.end(),
        [&member_id]( const active_member_observation &observation ) {
            return observation.npc_id == member_id;
        } );
        if( iter == observations.end() ) {
            return std::nullopt;
        }

        switch( iter->state ) {
            case active_member_observation_state::unresolved:
            case active_member_observation_state::local_contact:
            case active_member_observation_state::returning_home:
                return std::nullopt;
            case active_member_observation_state::home:
                packet.survivors_remaining++;
                break;
            case active_member_observation_state::dead:
                saw_loss = true;
                packet.anchored_identity_updates.push_back( { std::to_string( member_id.get_value() ), "dead" } );
                break;
            case active_member_observation_state::missing:
                saw_loss = true;
                packet.anchored_identity_updates.push_back( { std::to_string( member_id.get_value() ), "missing" } );
                break;
        }
    }

    if( saw_loss ) {
        packet.result = packet.survivors_remaining > 0 ?
                        bandit_pursuit_handoff::mission_result::repelled :
                        bandit_pursuit_handoff::mission_result::broken;
        packet.posture = packet.survivors_remaining > 0 ?
                         bandit_pursuit_handoff::return_posture::broken_flee :
                         bandit_pursuit_handoff::return_posture::escape_safe;
        packet.remaining_pressure = bandit_pursuit_handoff::remaining_return_pressure_state::collapsed;
    }

    packet.notes.push_back( "resolved live aftermath observations for active owned outing" );
    return packet;
}

bool update_member_state( site_record &site, character_id npc_id, member_state new_state,
                          const std::string &summary )
{
    member_record *member = site.find_member( npc_id );
    if( member == nullptr ) {
        return false;
    }

    const bool old_live = counts_toward_live_headcount( member->state );
    const bool new_live = counts_toward_live_headcount( new_state );
    if( old_live != new_live ) {
        site.headcount = std::max( 0, site.headcount + ( new_live ? 1 : -1 ) );
        if( spawn_tile_record *spawn_record = site.find_spawn_tile( member->home_spawn_tile ) ) {
            spawn_record->headcount = std::max( 0, spawn_record->headcount + ( new_live ? 1 : -1 ) );
        }
    }

    member->state = new_state;
    member->last_writeback_summary = summary;
    return true;
}
} // namespace bandit_live_world
