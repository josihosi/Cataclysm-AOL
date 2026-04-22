#include "bandit_live_world.h"

#include <algorithm>
#include <array>
#include <optional>
#include <sstream>
#include <string>

#include "json.h"

namespace
{
using bandit_live_world::anchor_source_kind;
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

int special_footprint_radius( const std::string &special_id )
{
    if( special_id == "bandit_camp" || special_id == "bandit_work_camp" ) {
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

int required_home_reserve( const bandit_live_world::site_record &site )
{
    switch( site.site_kind ) {
        case owned_site_kind::bandit_camp:
        case owned_site_kind::bandit_work_camp:
        case owned_site_kind::bandit_cabin:
            return 1;
        case owned_site_kind::looters:
        case owned_site_kind::bandits_block:
        case owned_site_kind::none:
            return 0;
    }

    return 0;
}

bandit_dry_run::camp_input make_dispatch_camp_input( const bandit_live_world::site_record &site )
{
    bandit_dry_run::camp_input camp;
    camp.available_manpower = site.dispatchable_member_capacity();
    if( camp.available_manpower >= 3 ) {
        camp.shortage = bandit_dry_run::shortage_band::stable;
    } else if( camp.available_manpower == 2 ) {
        camp.shortage = bandit_dry_run::shortage_band::low;
    } else {
        camp.shortage = bandit_dry_run::shortage_band::critical;
    }
    camp.job_type_bonus[bandit_dry_run::job_template::scout] = 1;
    return camp;
}

bandit_dry_run::lead_input make_nearby_target_lead( const bandit_live_world::site_record &site,
        const tripoint_abs_omt &target_omt, const std::string &target_id )
{
    bandit_dry_run::lead_input lead;
    lead.id = target_id;
    lead.envelope_id = target_id;
    lead.family = bandit_dry_run::lead_family::site;
    const int distance = rl_dist( site.anchor, target_omt );
    lead.distance_multiplier = std::clamp( 1.0 - static_cast<double>( distance ) / 20.0, 0.35, 1.0 );
    lead.lead_bounty_value = distance <= 10 ? 2 : 1;
    lead.lead_confidence_bonus = 1;
    lead.threat_penalty = 1;
    lead.threat_gate_result = bandit_dry_run::threat_gate::soft_veto;
    lead.hard_blocked_jobs = {
        bandit_dry_run::job_template::scavenge,
        bandit_dry_run::job_template::steal,
        bandit_dry_run::job_template::raid,
    };
    lead.validity_notes.push_back( "live-world nearby target envelope from owned site " + site.site_id );
    lead.validity_notes.push_back( "bounded v0 dispatch only promotes scout pursuit from real owned members" );
    return lead;
}

std::vector<character_id> select_dispatch_members( const bandit_live_world::site_record &site, int count )
{
    std::vector<character_id> member_ids;
    member_ids.reserve( std::max( count, 0 ) );
    for( const bandit_live_world::member_record &member : site.members ) {
        if( member.state != member_state::at_home ) {
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
    bandit_pursuit_handoff::abstract_group_state group;
    group.group_id = site.site_id + "#dispatch";
    group.source_camp_id = site.site_id;
    group.group_strength = member_ids.size();
    group.confidence = std::clamp( site.count_live_members(), 1, 3 );
    group.panic_threshold = std::max( 1, static_cast<int>( member_ids.size() ) );
    group.cargo_capacity = std::max( 1, static_cast<int>( member_ids.size() ) * 2 );
    group.current_target_or_mark = target_id;
    group.current_threat_estimate = 1;
    group.current_bounty_estimate = 2;
    group.mission_urgency = 1;
    group.retreat_bias = site.site_kind == owned_site_kind::bandit_cabin ? 2 : 1;
    group.goal_stickiness = 1;
    group.goal_preemption_posture = 1;
    group.return_clock = 2;
    for( const character_id &member_id : member_ids ) {
        group.anchored_identities.push_back( { std::to_string( member_id.get_value() ), "alive" } );
    }
    group.known_recent_marks.push_back( target_id );
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
        case owned_site_kind::looters:
            return "looters";
        case owned_site_kind::bandits_block:
            return "bandits_block";
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

void member_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "npc_id", npc_id.get_value() );
    json.member( "npc_template_id", npc_template_id );
    json.member( "home_spawn_tile", home_spawn_tile );
    json.member( "state", to_string( state ) );
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

void site_record::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "site_id", site_id );
    json.member( "source_kind", to_string( source_kind ) );
    json.member( "site_kind", to_string( site_kind ) );
    json.member( "source_id", source_id );
    json.member( "anchor", anchor );
    json.member( "headcount", headcount );
    json.member( "footprint", footprint );
    json.member( "members", members );
    json.member( "spawn_tiles", spawn_tiles );
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
    jo.read( "source_id", source_id );
    jo.read( "anchor", anchor );
    jo.read( "headcount", headcount );
    jo.read( "footprint", footprint );
    jo.read( "members", members );
    jo.read( "spawn_tiles", spawn_tiles );
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

int site_record::dispatchable_member_capacity() const
{
    const int at_home_members = count_members_in_state( member_state::at_home );
    return std::max( 0, at_home_members - required_home_reserve( *this ) );
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

bool is_tracked_bandit_template( const std::string &npc_template_id )
{
    static const std::array<std::string, 6> tracked_templates = {
        "bandit",
        "thug",
        "bandit_trader",
        "bandit_quartermaster",
        "bandit_mechanic",
        "hells_raiders_boss",
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

bool claim_tracked_spawn( world_state &state, const std::string &npc_template_id,
                          character_id npc_id, const tripoint_abs_ms &spawn_tile,
                          const std::optional<std::string> &overmap_special_id,
                          const std::optional<std::string> &map_extra_id,
                          const std::function<std::optional<std::string>( const tripoint_abs_omt & )> &special_lookup )
{
    if( !is_tracked_bandit_template( npc_template_id ) ) {
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
        new_site.source_id = source_id;
        new_site.anchor = footprint.anchor;
        new_site.footprint = footprint.footprint;
        state.sites.push_back( new_site );
        site = &state.sites.back();
    } else if( site->footprint.size() < footprint.footprint.size() ) {
        site->footprint = footprint.footprint;
        site->anchor = footprint.anchor;
    }

    if( site->has_member( npc_id ) ) {
        return true;
    }

    member_record member;
    member.npc_id = npc_id;
    member.npc_template_id = npc_template_id;
    member.home_spawn_tile = spawn_tile;
    site->members.push_back( member );
    site->headcount++;

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

dispatch_plan plan_site_dispatch( const site_record &site, const tripoint_abs_omt &target_omt,
                                  const std::string &target_id )
{
    dispatch_plan plan;
    plan.site_id = site.site_id;
    plan.target_id = target_id;
    plan.target_omt = target_omt;

    if( target_id.empty() ) {
        plan.notes.push_back( "dispatch blocked: missing target id" );
        return plan;
    }

    if( site.count_members_in_state( member_state::outbound ) > 0 ||
        site.count_members_in_state( member_state::local_contact ) > 0 ) {
        plan.notes.push_back( "dispatch blocked: site already has an active outbound/contact group" );
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

    const int required_members = required_dispatch_members( winner.job );
    if( required_members <= 0 ) {
        plan.notes.push_back( "dispatch blocked: winning job needs no live member handoff" );
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
    return true;
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
