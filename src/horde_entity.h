#pragma once
#ifndef CATA_SRC_HORDE_ENTITY_H
#define CATA_SRC_HORDE_ENTITY_H

#include <memory>
#include <string>

#include "calendar.h"
#include "coordinates.h"
#include "type_id.h"
#include "monster.h"

struct mtype;

// This represents a single entity that moves around at overmap scale.
// It needs to spawn the related monster, ideally with any notable details intact.
// TODO: There are a LOT of these stored in each overmap, leading to significant memory pressure.
// Need to investigate reducing the size of members pretty agresively.
struct horde_entity {
    // Create a heavy entity based on an existing monster.
    explicit horde_entity( const monster &original );
    // Create a lightweight entity based on a monster id.
    explicit horde_entity( const mtype_id &original );
    horde_entity( const horde_entity &other );
    horde_entity &operator=( const horde_entity &other );
    horde_entity( horde_entity && ) noexcept = default;
    horde_entity &operator=( horde_entity && ) noexcept = default;
    // Retrieve the mtype whether it's a light or heavy entity.
    const mtype *get_type() const;
    bool is_active() const;
    // Heavy entities retain the real monster state.  Lightweight entities can
    // only make the invariant type-level sight decision.
    bool can_perceive_light() const;
    void expire_light_interest( const time_point &now );
    // Materialize the full native payload only for a predator that actually
    // enters the abstract lifecycle.  The horde key remains its position.
    void ensure_predator_payload( const tripoint_abs_ms &position );
    void synchronize_payload( const tripoint_abs_ms &position );
    // Advance natural evolution through monster::try_upgrade once for this
    // abstract owner.  The payload is retained once it owns evolution timing.
    bool advance_evolution( const tripoint_abs_ms &position );
    // Species-owned abstract intent.  It consumes only actual horde light
    // evidence and writes the native stalker/rider state before the common
    // horde transport executes its one permitted step.
    bool advance_predator_intent( const tripoint_abs_ms &position, int now_turn );

    // Data here related to processing while acting as a horde entity.
    // a glaring omission is location, the parent horde container knows that.
    // TODO: There are a LOT of horde_entity instances, and destination is a large proportion of it,
    // investigate making it smaller.
    tripoint_abs_ms destination;
    // Shrink this one too?
    int tracking_intensity = 0;
    // Not sure how to shrink this?
    time_point last_processed;
    // Same here, this could probably be a byte.
    int moves = 0;
    // Persisted identity and lifetime of the last physical light observation.
    // This belongs to the abstract horde owner, so replaying a packet cannot
    // manufacture a later observation or extend an extinguished source.
    tripoint_abs_ms light_source;
    std::string light_sample_id;
    time_point light_observed;
    time_point light_expires;
    int light_interest_strength = 0;
    mtype_int_id type_id;
    // If this monster was never spawned, this member can be empty.
    // If it was, it has this populated to capture all the random bits of state that a monster can accumulate.
    // The vast majority of entities in an overmap have never actually been spawned,
    // meaning they don't have this member populated.
    // This could be a 16-bit index instead of a 32-to-64-bit+ unique_pointer
    std::unique_ptr<monster> monster_data;
};

#endif // CATA_SRC_HORDE_ENTITY_H
