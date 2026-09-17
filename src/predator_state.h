#pragma once
#ifndef CATA_SRC_PREDATOR_STATE_H
#define CATA_SRC_PREDATOR_STATE_H

#include <cstdint>
#include <string>

// This is deliberately small and lives beside the native monster state.  It
// identifies a predator across the copies used by the horde/local handoff;
// it is not a second simulation or a positional actor registry.
struct predator_lifecycle_state {
    int schema_version = 2;
    std::string actor_id;
    std::uint64_t handoff_epoch = 0;
    int last_advanced_turn = -1;
    // These are inert continuity facts.  Evolution owns their meaning and
    // writes; rider bands own membership and union semantics.
    int ammo_initialization_version = 0;
    std::string band_reference;
    std::uint64_t band_revision_cache = 0;
};

#endif // CATA_SRC_PREDATOR_STATE_H
