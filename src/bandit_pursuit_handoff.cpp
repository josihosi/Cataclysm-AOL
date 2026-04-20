#include "bandit_pursuit_handoff.h"

#include <algorithm>
#include <sstream>

namespace bandit_pursuit_handoff
{
namespace
{
bool is_pursuit_job( bandit_dry_run::job_template job )
{
    switch( job ) {
        case bandit_dry_run::job_template::scout:
        case bandit_dry_run::job_template::stalk:
            return true;
        case bandit_dry_run::job_template::hold_chill:
        case bandit_dry_run::job_template::scavenge:
        case bandit_dry_run::job_template::toll:
        case bandit_dry_run::job_template::steal:
        case bandit_dry_run::job_template::raid:
        case bandit_dry_run::job_template::reinforce:
            return false;
    }

    return false;
}

bool is_pursuit_family( bandit_dry_run::lead_family family )
{
    switch( family ) {
        case bandit_dry_run::lead_family::site:
        case bandit_dry_run::lead_family::corridor:
        case bandit_dry_run::lead_family::moving_carrier:
            return true;
        case bandit_dry_run::lead_family::none:
        case bandit_dry_run::lead_family::friendly_pressure:
            return false;
    }

    return false;
}

cargo_profile subtract_delivered_cargo( const cargo_profile &carried,
                                        const cargo_profile &delivered )
{
    cargo_profile remaining;
    remaining.food = std::max( 0, carried.food - delivered.food );
    remaining.meds = std::max( 0, carried.meds - delivered.meds );
    remaining.ammo = std::max( 0, carried.ammo - delivered.ammo );
    remaining.gear = std::max( 0, carried.gear - delivered.gear );
    remaining.fuel = std::max( 0, carried.fuel - delivered.fuel );
    remaining.trade_goods = std::max( 0, carried.trade_goods - delivered.trade_goods );
    return remaining;
}

std::string render_cargo_profile( const cargo_profile &cargo )
{
    std::ostringstream out;
    out << "food=" << cargo.food
        << ", meds=" << cargo.meds
        << ", ammo=" << cargo.ammo
        << ", gear=" << cargo.gear
        << ", fuel=" << cargo.fuel
        << ", trade=" << cargo.trade_goods;
    return out.str();
}

int max_writeback_strength( const std::vector<carrier_writeback> &writebacks )
{
    int value = 0;
    for( const carrier_writeback &writeback : writebacks ) {
        value = std::max( value, writeback.strength );
    }
    return value;
}

void apply_identity_updates( std::vector<anchored_identity_state> &identities,
                             const std::vector<anchored_identity_state> &updates )
{
    for( const anchored_identity_state &update : updates ) {
        const auto it = std::find_if( identities.begin(), identities.end(),
        [&update]( const anchored_identity_state &identity ) {
            return identity.id == update.id;
        } );
        if( it == identities.end() ) {
            identities.push_back( update );
        } else {
            it->status = update.status;
        }
    }
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
} // namespace

bool supports_pursuit_handoff( const bandit_dry_run::candidate_debug &candidate )
{
    return candidate.valid && is_pursuit_job( candidate.job ) && is_pursuit_family( candidate.family );
}

entry_mode choose_entry_mode( const bandit_dry_run::candidate_debug &candidate,
                              contact_certainty contact,
                              return_pressure_state return_pressure )
{
    if( return_pressure == return_pressure_state::withdraw_now ) {
        return entry_mode::withdrawal;
    }

    if( candidate.family == bandit_dry_run::lead_family::moving_carrier &&
        contact != contact_certainty::contact_ready ) {
        return entry_mode::shadow;
    }

    if( candidate.job == bandit_dry_run::job_template::scout &&
        contact == contact_certainty::broad ) {
        return entry_mode::scout;
    }

    return entry_mode::probe;
}

entry_payload build_entry_payload( const abstract_group_state &group,
                                   const bandit_dry_run::candidate_debug &winner,
                                   const entry_context &context )
{
    entry_payload payload;
    payload.group_id = group.group_id;
    payload.source_camp_id = group.source_camp_id;
    payload.job_type = winner.job;
    payload.lead_carrier = winner.family;
    payload.mode = choose_entry_mode( winner, context.contact, context.return_pressure );
    payload.current_target_or_mark = !winner.envelope_id.empty() ? winner.envelope_id : winner.lead_id;
    if( payload.current_target_or_mark.empty() ) {
        payload.current_target_or_mark = group.current_target_or_mark;
    }
    payload.group_strength = group.group_strength;
    payload.confidence = group.confidence;
    payload.panic_threshold = group.panic_threshold;
    payload.cargo_capacity = group.cargo_capacity;
    payload.current_threat_estimate = group.current_threat_estimate;
    payload.current_bounty_estimate = group.current_bounty_estimate;
    payload.mission_urgency = group.mission_urgency;
    payload.retreat_bias = group.retreat_bias;
    payload.goal_stickiness = group.goal_stickiness;
    payload.goal_preemption_posture = group.goal_preemption_posture;
    payload.return_clock = group.return_clock;
    payload.danger_posture = winner.score.threat_gate_result;
    payload.anchored_identities = group.anchored_identities;
    payload.known_recent_marks = group.known_recent_marks;
    payload.notes = winner.notes;

    if( supports_pursuit_handoff( winner ) ) {
        payload.valid = true;
        payload.notes.push_back( "pursuit v0 stays on scout/probe/shadow/withdrawal only" );
    } else {
        payload.valid = false;
        payload.notes.push_back( "winner is outside the bounded pursuit/investigation handoff contract" );
    }

    return payload;
}

return_packet build_return_packet( const entry_payload &entry, const local_outcome &outcome )
{
    return_packet packet;
    packet.valid = entry.valid;
    packet.group_id = entry.group_id;
    packet.source_camp_id = entry.source_camp_id;
    packet.job_type = entry.job_type;
    packet.mode = entry.mode;
    packet.current_target_or_mark = entry.current_target_or_mark;
    packet.survivors_remaining = outcome.survivors_remaining;
    packet.anchored_identity_updates = outcome.anchored_identity_updates;
    packet.wound_burden = outcome.wound_burden;
    packet.morale = outcome.morale;
    packet.cargo_profile_carried = subtract_delivered_cargo( outcome.cargo_profile_carried,
                                   outcome.camp_stockpile_delta );
    packet.camp_stockpile_delta = outcome.camp_stockpile_delta;
    packet.result = outcome.result;
    packet.resolution = outcome.resolution;
    packet.threat_writeback = outcome.threat_writeback;
    packet.bounty_writeback = outcome.bounty_writeback;
    packet.new_marks_learned = outcome.new_marks_learned;
    packet.loss_site_if_any = outcome.loss_site_if_any;
    packet.posture = outcome.posture;
    packet.remaining_pressure = outcome.remaining_pressure;
    packet.notes = entry.notes;

    if( !entry.valid ) {
        packet.notes.push_back( "return packet is inert because entry payload never crossed the bounded handoff seam" );
    }
    if( outcome.resolution == lead_resolution::narrowed && outcome.new_marks_learned.empty() ) {
        packet.notes.push_back( "narrowed lead returned without a sharper mark id" );
    }
    if( outcome.camp_stockpile_delta.food > outcome.cargo_profile_carried.food ||
        outcome.camp_stockpile_delta.meds > outcome.cargo_profile_carried.meds ||
        outcome.camp_stockpile_delta.ammo > outcome.cargo_profile_carried.ammo ||
        outcome.camp_stockpile_delta.gear > outcome.cargo_profile_carried.gear ||
        outcome.camp_stockpile_delta.fuel > outcome.cargo_profile_carried.fuel ||
        outcome.camp_stockpile_delta.trade_goods > outcome.cargo_profile_carried.trade_goods ) {
        packet.notes.push_back( "delivered stockpile share exceeded carried cargo and was clamped" );
    }

    return packet;
}

void apply_return_packet( abstract_group_state &group, const return_packet &packet )
{
    if( !packet.valid ) {
        return;
    }

    group.group_strength = packet.survivors_remaining;
    group.carried_cargo = packet.cargo_profile_carried;
    group.wound_burden = packet.wound_burden;
    group.morale = packet.morale;
    group.last_return_posture = packet.posture;
    group.remaining_pressure = packet.remaining_pressure;
    apply_identity_updates( group.anchored_identities, packet.anchored_identity_updates );

    for( const std::string &mark : packet.new_marks_learned ) {
        push_unique_mark( group.known_recent_marks, mark );
    }
    push_unique_mark( group.known_recent_marks, packet.loss_site_if_any );

    if( !packet.threat_writeback.empty() ) {
        group.current_threat_estimate = max_writeback_strength( packet.threat_writeback );
    }
    if( !packet.bounty_writeback.empty() ) {
        group.current_bounty_estimate = max_writeback_strength( packet.bounty_writeback );
    }

    switch( packet.resolution ) {
        case lead_resolution::narrowed:
            if( !packet.new_marks_learned.empty() ) {
                group.current_target_or_mark = packet.new_marks_learned.front();
            }
            break;
        case lead_resolution::still_valid:
        case lead_resolution::route_blocked:
            group.current_target_or_mark = packet.current_target_or_mark;
            break;
        case lead_resolution::harvested:
        case lead_resolution::cleared:
        case lead_resolution::target_lost:
            group.current_target_or_mark.clear();
            break;
    }

    switch( packet.remaining_pressure ) {
        case remaining_return_pressure_state::ample:
            group.return_clock = std::max( group.return_clock, 3 );
            break;
        case remaining_return_pressure_state::tight:
            group.return_clock = group.return_clock == 0 ? 2 : std::min( group.return_clock, 2 );
            break;
        case remaining_return_pressure_state::plain_return_only:
            group.return_clock = 1;
            break;
        case remaining_return_pressure_state::collapsed:
            group.return_clock = 0;
            break;
    }

    switch( packet.posture ) {
        case return_posture::resume_route:
            break;
        case return_posture::shadow_then_break:
            group.retreat_bias = std::max( group.retreat_bias, 1 );
            break;
        case return_posture::escape_home:
        case return_posture::escape_safe:
            group.retreat_bias = std::max( group.retreat_bias, 2 );
            break;
        case return_posture::broken_flee:
            group.retreat_bias = std::max( group.retreat_bias, 3 );
            break;
    }
}

std::string to_string( contact_certainty certainty )
{
    switch( certainty ) {
        case contact_certainty::broad:
            return "broad";
        case contact_certainty::localized:
            return "localized";
        case contact_certainty::contact_ready:
            return "contact_ready";
    }

    return "unknown";
}

std::string to_string( return_pressure_state pressure )
{
    switch( pressure ) {
        case return_pressure_state::normal:
            return "normal";
        case return_pressure_state::withdraw_now:
            return "withdraw_now";
    }

    return "unknown";
}

std::string to_string( entry_mode mode )
{
    switch( mode ) {
        case entry_mode::scout:
            return "scout";
        case entry_mode::probe:
            return "probe";
        case entry_mode::shadow:
            return "shadow";
        case entry_mode::withdrawal:
            return "withdrawal";
    }

    return "unknown";
}

std::string to_string( wound_burden_state wound )
{
    switch( wound ) {
        case wound_burden_state::none:
            return "none";
        case wound_burden_state::light:
            return "light";
        case wound_burden_state::moderate:
            return "moderate";
        case wound_burden_state::heavy:
            return "heavy";
        case wound_burden_state::crippled:
            return "crippled";
    }

    return "unknown";
}

std::string to_string( morale_state morale )
{
    switch( morale ) {
        case morale_state::steady:
            return "steady";
        case morale_state::shaken:
            return "shaken";
        case morale_state::breaking:
            return "breaking";
        case morale_state::routed:
            return "routed";
    }

    return "unknown";
}

std::string to_string( mission_result result )
{
    switch( result ) {
        case mission_result::no_contact:
            return "no_contact";
        case mission_result::scouted:
            return "scouted";
        case mission_result::partial_success:
            return "partial_success";
        case mission_result::clean_success:
            return "clean_success";
        case mission_result::withdrawn:
            return "withdrawn";
        case mission_result::repelled:
            return "repelled";
        case mission_result::broken:
            return "broken";
        case mission_result::destroyed:
            return "destroyed";
    }

    return "unknown";
}

std::string to_string( lead_resolution resolution )
{
    switch( resolution ) {
        case lead_resolution::still_valid:
            return "still_valid";
        case lead_resolution::narrowed:
            return "narrowed";
        case lead_resolution::harvested:
            return "harvested";
        case lead_resolution::cleared:
            return "cleared";
        case lead_resolution::route_blocked:
            return "route_blocked";
        case lead_resolution::target_lost:
            return "target_lost";
    }

    return "unknown";
}

std::string to_string( return_posture posture )
{
    switch( posture ) {
        case return_posture::resume_route:
            return "resume_route";
        case return_posture::escape_home:
            return "escape_home";
        case return_posture::escape_safe:
            return "escape_safe";
        case return_posture::shadow_then_break:
            return "shadow_then_break";
        case return_posture::broken_flee:
            return "broken_flee";
    }

    return "unknown";
}

std::string to_string( remaining_return_pressure_state pressure )
{
    switch( pressure ) {
        case remaining_return_pressure_state::ample:
            return "ample";
        case remaining_return_pressure_state::tight:
            return "tight";
        case remaining_return_pressure_state::plain_return_only:
            return "plain_return_only";
        case remaining_return_pressure_state::collapsed:
            return "collapsed";
    }

    return "unknown";
}

std::string render_report( const entry_payload &entry, const return_packet &packet )
{
    std::ostringstream out;
    out << "bandit pursuit handoff report\n";
    out << "entry_payload:\n";
    out << "- valid=" << ( entry.valid ? "yes" : "no" ) << "\n";
    out << "- group_id=" << entry.group_id
        << ", source_camp_id=" << entry.source_camp_id
        << ", job_type=" << bandit_dry_run::to_string( entry.job_type )
        << ", lead_carrier=" << bandit_dry_run::to_string( entry.lead_carrier )
        << ", mode=" << to_string( entry.mode ) << "\n";
    out << "- target=" << entry.current_target_or_mark
        << ", group_strength=" << entry.group_strength
        << ", confidence=" << entry.confidence
        << ", return_clock=" << entry.return_clock << "\n";
    out << "- threat=" << entry.current_threat_estimate
        << ", bounty=" << entry.current_bounty_estimate
        << ", urgency=" << entry.mission_urgency
        << ", retreat_bias=" << entry.retreat_bias
        << ", danger_posture=" << bandit_dry_run::to_string( entry.danger_posture ) << "\n";

    out << "return_packet:\n";
    out << "- valid=" << ( packet.valid ? "yes" : "no" )
        << ", mission_result=" << to_string( packet.result )
        << ", lead_resolution=" << to_string( packet.resolution )
        << ", return_posture=" << to_string( packet.posture )
        << ", remaining_pressure=" << to_string( packet.remaining_pressure ) << "\n";
    out << "- survivors_remaining=" << packet.survivors_remaining
        << ", wound_burden=" << to_string( packet.wound_burden )
        << ", morale=" << to_string( packet.morale ) << "\n";
    out << "- carried_cargo=" << render_cargo_profile( packet.cargo_profile_carried ) << "\n";
    out << "- camp_stockpile_delta=" << render_cargo_profile( packet.camp_stockpile_delta ) << "\n";

    if( !packet.threat_writeback.empty() ) {
        out << "- threat_writeback:\n";
        for( const carrier_writeback &writeback : packet.threat_writeback ) {
            out << "  - carrier=" << writeback.carrier_id
                << ", rewrite=" << writeback.rewrite
                << ", strength=" << writeback.strength << "\n";
        }
    }
    if( !packet.bounty_writeback.empty() ) {
        out << "- bounty_writeback:\n";
        for( const carrier_writeback &writeback : packet.bounty_writeback ) {
            out << "  - carrier=" << writeback.carrier_id
                << ", rewrite=" << writeback.rewrite
                << ", strength=" << writeback.strength << "\n";
        }
    }
    if( !packet.new_marks_learned.empty() ) {
        out << "- new_marks_learned:\n";
        for( const std::string &mark : packet.new_marks_learned ) {
            out << "  - " << mark << "\n";
        }
    }
    if( !packet.loss_site_if_any.empty() ) {
        out << "- loss_site_if_any=" << packet.loss_site_if_any << "\n";
    }
    if( !packet.notes.empty() ) {
        out << "notes:\n";
        for( const std::string &note : packet.notes ) {
            out << "- " << note << "\n";
        }
    }

    return out.str();
}
} // namespace bandit_pursuit_handoff
