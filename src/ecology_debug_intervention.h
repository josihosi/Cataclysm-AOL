#pragma once
#ifndef CATA_SRC_ECOLOGY_DEBUG_INTERVENTION_H
#define CATA_SRC_ECOLOGY_DEBUG_INTERVENTION_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "bandit_live_world.h"
#include "character_id.h"
#include "coordinates.h"
#include "ecology_debug_incident.h"

namespace ecology_debug
{

inline constexpr size_t ecology_intervention_receipt_cap = ecology_incident_intervention_cap;

enum class dispatch_member_edit_action {
    wound,
    heal,
    kill,
};

struct dispatch_member_edit_guard {
    immutable_entity_token entity;
    character_id member_id;
    tripoint_abs_omt entity_omt;
    tripoint_abs_omt member_omt;
    int hp_percent = -1;
    bandit_live_world::simulation_advance_cursor cursor;
};

struct authoritative_dispatch_member_read {
    immutable_entity_token entity;
    entity_kind kind = entity_kind::bandit_camp;
    tripoint_abs_omt entity_omt;
    bool structural_sortie = false;
    bool local_handoff_active = false;
    bandit_live_world::simulation_advance_cursor cursor;
    character_id member_id;
    tripoint_abs_omt member_omt;
    int hp_percent = -1;
    bool loaded = false;
    bool alive = false;
};

std::string validate_dispatch_member_edit( const dispatch_member_edit_guard &guard,
        const authoritative_dispatch_member_read &current );

struct intervention_receipt {
    immutable_entity_token entity;
    incident_intervention incident;
};

class intervention_ledger
{
    public:
        uint64_t append( intervention_receipt receipt );
        uint64_t latest_sequence() const;
        std::vector<incident_intervention> matching_after(
            const immutable_entity_token &entity, uint64_t after_sequence ) const;
        bool contains_at( const std::string &world_identity, const std::string &entity_id,
                          int turn ) const;
        size_t dropped_count() const;
        void clear();

    private:
        std::deque<intervention_receipt> receipts_;
        uint64_t next_sequence_ = 1;
        size_t dropped_count_ = 0;
};

intervention_ledger &process_intervention_ledger();

std::string to_string( dispatch_member_edit_action action );

} // namespace ecology_debug

#endif // CATA_SRC_ECOLOGY_DEBUG_INTERVENTION_H
