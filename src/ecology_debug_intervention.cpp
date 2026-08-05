#include "ecology_debug_intervention.h"

#include <algorithm>

namespace ecology_debug
{
namespace
{

bool same_entity_token( const immutable_entity_token &lhs,
                        const immutable_entity_token &rhs )
{
    return lhs.world_identity == rhs.world_identity &&
           lhs.canonical_id == rhs.canonical_id &&
           lhs.generation == rhs.generation && lhs.owner == rhs.owner &&
           lhs.authority_token == rhs.authority_token;
}

bool same_cursor( const bandit_live_world::simulation_advance_cursor &lhs,
                  const bandit_live_world::simulation_advance_cursor &rhs )
{
    return lhs.activity_id == rhs.activity_id && lhs.generation == rhs.generation &&
           lhs.owner == rhs.owner && lhs.handoff_epoch == rhs.handoff_epoch &&
           lhs.last_advanced_minutes == rhs.last_advanced_minutes;
}

} // namespace

std::string validate_dispatch_member_edit( const dispatch_member_edit_guard &guard,
        const authoritative_dispatch_member_read &current )
{
    if( !same_entity_token( guard.entity, current.entity ) ||
        guard.entity.canonical_id.empty() || guard.entity.authority_token.empty() ) {
        return "entity_token_stale";
    }
    if( current.kind != entity_kind::bandit_dispatch &&
        current.kind != entity_kind::cannibal_dispatch ) {
        return "selected_entity_is_not_dispatch";
    }
    if( !current.structural_sortie || !current.local_handoff_active ||
        current.cursor.owner != bandit_live_world::simulation_owner::local ) {
        return "dispatch_is_inspect_only";
    }
    if( !same_cursor( guard.cursor, current.cursor ) ) {
        return "outing_cursor_stale";
    }
    if( guard.entity_omt != current.entity_omt ) {
        return "dispatch_location_stale";
    }
    if( guard.member_id != current.member_id ) {
        return "member_identity_stale";
    }
    if( !current.loaded || !current.alive ) {
        return "member_load_or_life_state_stale";
    }
    if( guard.member_omt != current.member_omt ) {
        return "member_location_stale";
    }
    if( guard.hp_percent != current.hp_percent ) {
        return "member_hp_stale";
    }
    return {};
}

uint64_t intervention_ledger::append( intervention_receipt receipt )
{
    if( receipt.entity.world_identity.empty() || receipt.entity.canonical_id.empty() ||
        receipt.entity.authority_token.empty() ||
        receipt.incident.entity_id != receipt.entity.canonical_id ||
        receipt.incident.turn < 0 || receipt.incident.timestamp.empty() ||
        receipt.incident.action.empty() || !receipt.incident.debug_intervention ) {
        return 0;
    }
    receipt.incident.sequence = next_sequence_++;
    receipts_.push_back( std::move( receipt ) );
    while( receipts_.size() > ecology_intervention_receipt_cap ) {
        receipts_.pop_front();
        ++dropped_count_;
    }
    return receipts_.back().incident.sequence;
}

uint64_t intervention_ledger::latest_sequence() const
{
    return next_sequence_ - 1;
}

std::vector<incident_intervention> intervention_ledger::matching_after(
    const immutable_entity_token &entity, uint64_t after_sequence ) const
{
    std::vector<incident_intervention> result;
    for( const intervention_receipt &receipt : receipts_ ) {
        if( receipt.incident.sequence > after_sequence && same_entity_token( receipt.entity, entity ) ) {
            result.push_back( receipt.incident );
        }
    }
    return result;
}

bool intervention_ledger::contains_at( const std::string &world_identity,
                                       const std::string &entity_id, int turn ) const
{
    return std::any_of( receipts_.begin(), receipts_.end(),
    [&]( const intervention_receipt & receipt ) {
        return receipt.entity.world_identity == world_identity &&
               receipt.entity.canonical_id == entity_id && receipt.incident.turn == turn;
    } );
}

size_t intervention_ledger::dropped_count() const
{
    return dropped_count_;
}

void intervention_ledger::clear()
{
    receipts_.clear();
    next_sequence_ = 1;
    dropped_count_ = 0;
}

intervention_ledger &process_intervention_ledger()
{
    static intervention_ledger ledger;
    return ledger;
}

std::string to_string( dispatch_member_edit_action action )
{
    switch( action ) {
        case dispatch_member_edit_action::wound:
            return "wound";
        case dispatch_member_edit_action::heal:
            return "heal";
        case dispatch_member_edit_action::kill:
            return "kill";
    }
    return "unknown";
}

} // namespace ecology_debug
