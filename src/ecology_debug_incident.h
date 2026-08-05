#pragma once
#ifndef CATA_SRC_ECOLOGY_DEBUG_INCIDENT_H
#define CATA_SRC_ECOLOGY_DEBUG_INCIDENT_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "coordinates.h"
#include "ecology_debug_delta.h"

namespace ecology_debug
{

inline constexpr size_t ecology_incident_note_byte_cap = 256;
inline constexpr size_t ecology_incident_intervention_cap = 32;
inline constexpr size_t ecology_incident_caller_text_byte_cap = 256;

struct incident_identity {
    int turn = -1;
    std::string timestamp;
    tripoint_abs_omt player_omt;
    std::string scenario;
    std::string commit;
    std::string binary;
    std::string run_identity;
};

struct incident_intervention {
    uint64_t sequence = 0;
    int turn = -1;
    std::string timestamp;
    std::string entity_id;
    std::string action;
    std::string before_summary;
    std::string after_summary;
    bool debug_intervention = false;
};

struct incident_bundle_result {
    bool valid = false;
    std::string error;
    std::string payload;
    size_t payload_bytes = 0;
    bool note_truncated = false;
    size_t intervention_dropped_count = 0;
};

incident_bundle_result serialize_incident_bundle(
    const incident_identity &identity,
    const std::optional<selected_projection> &selected,
    const delta_ring &deltas,
    const std::optional<std::string> &human_note,
    const std::vector<incident_intervention> &interventions );

} // namespace ecology_debug

#endif // CATA_SRC_ECOLOGY_DEBUG_INCIDENT_H
