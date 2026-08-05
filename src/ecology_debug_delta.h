#pragma once
#ifndef CATA_SRC_ECOLOGY_DEBUG_DELTA_H
#define CATA_SRC_ECOLOGY_DEBUG_DELTA_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "ecology_debug_view.h"

namespace ecology_debug
{

inline constexpr size_t ecology_delta_record_cap = delta_cap;

enum class delta_kind {
    appeared,
    moved,
    phase_changed,
    hp_changed,
    anomaly,
};

enum class trigger_disposition {
    continue_capture,
    pause,
    fail,
};

struct immutable_entity_token {
    std::string world_identity;
    std::string canonical_id;
    int generation = 0;
    std::string owner;
    std::string authority_token;
};

struct selected_projection {
    entity_marker marker;
    std::optional<selected_detail> detail;
    immutable_entity_token token;
};

struct delta_member_summary {
    int npc_id = -1;
    std::optional<int> hp_percent;
    std::string status;
};

struct delta_entity_summary {
    std::string id;
    std::string alias;
    entity_kind kind = entity_kind::bandit_camp;
    entity_faction faction = entity_faction::bandit;
    tripoint_abs_omt omt;
    std::string owner;
    bool loaded = false;
    std::string phase;
    int generation = 0;
    std::optional<int> population;
    std::optional<int> interest;
    std::optional<tripoint_abs_omt> target;
    std::optional<int> hp_percent;
    std::vector<delta_member_summary> members;
};

struct delta_observation_context {
    int turn = -1;
    std::string timestamp;
    trigger_disposition disposition = trigger_disposition::continue_capture;
};

struct delta_record {
    uint64_t sequence = 0;
    delta_kind kind = delta_kind::appeared;
    int turn = -1;
    std::string timestamp;
    std::string entity_id;
    entity_provenance provenance = entity_provenance::natural;
    trigger_disposition disposition = trigger_disposition::continue_capture;
    std::optional<delta_entity_summary> before;
    std::optional<delta_entity_summary> after;
    std::string anomaly_reason;
};

struct delta_observation_result {
    size_t emitted = 0;
    trigger_disposition disposition = trigger_disposition::continue_capture;
    bool anomaly = false;
};

struct delta_ring_metadata {
    size_t retained_count = 0;
    size_t dropped_count = 0;
    size_t record_limit = ecology_delta_record_cap;
    bool truncated = false;
    size_t trace_bytes = 0;
};

class delta_ring
{
    public:
        delta_observation_result observe( const std::optional<selected_projection> &prior,
                                          const std::optional<selected_projection> &current,
                                          const delta_observation_context &context );

        const std::deque<delta_record> &records() const;
        delta_ring_metadata metadata() const;
        std::string serialize_compact_json() const;
        void clear();

    private:
        void append( delta_record record );

        std::deque<delta_record> records_;
        uint64_t next_sequence_ = 1;
        size_t dropped_count_ = 0;
};

std::string to_string( delta_kind kind );
std::string to_string( trigger_disposition disposition );

} // namespace ecology_debug

#endif // CATA_SRC_ECOLOGY_DEBUG_DELTA_H
