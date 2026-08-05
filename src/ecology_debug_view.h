#pragma once
#ifndef CATA_SRC_ECOLOGY_DEBUG_VIEW_H
#define CATA_SRC_ECOLOGY_DEBUG_VIEW_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "character_id.h"
#include "coordinates.h"

namespace bandit_live_world
{
struct world_state;
} // namespace bandit_live_world

namespace ecology_debug
{

inline constexpr size_t candidate_cap = 2048;
inline constexpr size_t marker_cap = 256;
inline constexpr size_t delta_cap = 128;

enum class entity_kind {
    bandit_camp,
    cannibal_camp,
    bandit_dispatch,
    cannibal_dispatch,
    zombie_horde,
    writhing_stalker,
};

enum class entity_faction {
    bandit,
    cannibal,
    zombie,
};

enum class entity_owner {
    abstract,
    concrete,
};

enum class entity_provenance {
    natural,
    debug_intervention,
};

struct query_region {
    bool enabled = false;
    tripoint_abs_omt minimum;
    tripoint_abs_omt maximum;
};

struct query_filters {
    bool camps = true;
    bool dispatches = true;
    bool bandits = true;
    bool cannibals = true;
    bool hordes = false;
    bool stalkers = false;
    bool loaded_only = false;
};

struct runtime_member_read {
    std::string name;
    std::optional<int> hp_percent;
    bool loaded = false;
};

struct mobile_entity_read {
    std::string id;
    entity_kind kind = entity_kind::zombie_horde;
    entity_faction faction = entity_faction::zombie;
    tripoint_abs_omt omt;
    entity_owner owner = entity_owner::abstract;
    bool loaded = false;
    std::string state;
    int generation = 0;
    std::optional<int> population;
    std::optional<int> interest;
    std::optional<tripoint_abs_omt> target;
    std::optional<int> hp_percent;
};

struct query_request {
    bool enabled = false;
    int now_minutes = -1;
    query_region region;
    query_filters filters;
    std::string selected_id;
    std::function<bool( character_id )> member_is_loaded;
    std::function<std::optional<runtime_member_read>( character_id )> read_selected_member;
    std::function<entity_provenance( const std::string & )> provenance_for_entity;
    std::function<std::vector<mobile_entity_read>( const query_region &, std::string_view,
            size_t )>
    read_mobile_entities;
};

struct entity_marker {
    std::string id;
    std::string alias;
    entity_kind kind = entity_kind::bandit_camp;
    entity_faction faction = entity_faction::bandit;
    tripoint_abs_omt omt;
    std::string owner;
    bool loaded = false;
    std::string state;
    entity_provenance provenance = entity_provenance::natural;
    int generation = 0;
    // Process-local owner token. Never serialized or persisted.
    std::optional<size_t> authority_index;
};

struct member_detail {
    character_id npc_id;
    std::string name;
    std::optional<int> hp_percent;
    std::string status;
    bool loaded = false;
};

struct selected_detail {
    std::string entity_id;
    std::string source_camp_id;
    std::string phase;
    int last_transition_minutes = -1;
    std::string last_transition_reason;
    std::string blocked_reason;
    std::string evidence_id;
    std::string evidence_kind;
    std::string evidence_state;
    std::string evidence_reason;
    int evidence_observed_minutes = -1;
    int evidence_age_minutes = -1;
    int next_deadline_minutes = -1;
    tripoint_abs_omt destination;
    std::vector<tripoint_abs_omt> route;
    std::vector<member_detail> members;
    std::optional<int> population;
    std::optional<int> interest;
    std::optional<tripoint_abs_omt> target;
    std::optional<int> hp_percent;
};

struct query_metadata {
    size_t candidate_count = 0;
    size_t considered_count = 0;
    size_t emitted_count = 0;
    size_t dropped_count = 0;
    size_t candidate_limit = candidate_cap;
    size_t marker_limit = marker_cap;
    size_t event_limit = delta_cap;
    bool truncated = false;
    long long query_microseconds = 0;
    long long render_microseconds = 0;
    size_t trace_bytes = 0;
};

struct view_snapshot {
    std::vector<entity_marker> entities;
    std::optional<selected_detail> selected;
    query_metadata metadata;
};

view_snapshot query_bandit_ecology( const bandit_live_world::world_state &world,
                                    const query_request &request );

view_snapshot query_selected_bandit_ecology( const bandit_live_world::world_state &world,
        const query_request &request, size_t authority_index );

std::string to_string( entity_kind kind );
std::string to_string( entity_faction faction );
std::string to_string( entity_owner owner );
std::string to_string( entity_provenance provenance );

} // namespace ecology_debug

#endif // CATA_SRC_ECOLOGY_DEBUG_VIEW_H
