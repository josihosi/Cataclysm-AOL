#pragma once
#ifndef CATA_SRC_ECOLOGY_DEBUG_SNAPSHOT_H
#define CATA_SRC_ECOLOGY_DEBUG_SNAPSHOT_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ecology_debug_view.h"

class JsonOut;

namespace ecology_debug
{

inline constexpr size_t snapshot_identity_limit = 128;

struct snapshot_context {
    std::string schema = "c-aol.ecology.observer";
    int version = 1;
    std::optional<std::string> commit;
    std::optional<std::string> binary;
    std::optional<std::string> scenario;
    std::string calendar_turn;
    std::string timestamp;
    tripoint_abs_omt player_omt;
    query_region region;
    bool selected_outside_region_included = false;
    query_filters filters;
    std::vector<std::string> filter_labels;
    std::string selected_id;
};

void write_snapshot( JsonOut &json, const view_snapshot &view,
                     const snapshot_context &context );

std::string serialize_snapshot( const view_snapshot &view,
                                const snapshot_context &context );

std::string serialize_sized_snapshot( const view_snapshot &view,
                                      const snapshot_context &context );

view_snapshot selected_monitor_projection( const view_snapshot &view,
        std::string_view selected_id );

std::string serialize_monitor_snapshot( const view_snapshot &view,
                                        const snapshot_context &context );

} // namespace ecology_debug

#endif // CATA_SRC_ECOLOGY_DEBUG_SNAPSHOT_H
