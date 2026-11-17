#pragma once
#ifndef CATA_SRC_OVERMAP_PARTY_TRAVEL_H
#define CATA_SRC_OVERMAP_PARTY_TRAVEL_H

#include <cstdint>
#include <functional>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "coordinates.h"

class JsonObject;
class JsonOut;

// Paid coarse OMT-edge work. Faction policy, party admission and actual
// ownership/position commits remain with the caller.
namespace overmap_party_travel
{
// A partially loaded OMT is local, even when its center or an actor offset is outside.
bool omt_intersects_loaded_map( const tripoint_abs_omt &omt );
// Disposable route knowledge belongs to the authenticated actor/operation.
// Callers invalidate it when capabilities or route policy change.
// Synchronous diagnostic only; counts actual route-finder calls while scoped.
struct route_search_counts {
    std::size_t searches = 0;
};
class scoped_route_search_counter
{
    public:
        explicit scoped_route_search_counter( route_search_counts &counts );
        ~scoped_route_search_counter();
        scoped_route_search_counter( const scoped_route_search_counter & ) = delete;
        scoped_route_search_counter &operator=( const scoped_route_search_counter & ) = delete;
    private:
        route_search_counts *previous;
};

struct boundary_refusal {
    std::string activity_id;
    std::uint64_t operation_generation = 0;
    std::uint64_t lease_epoch = 0;
    tripoint_abs_ms origin;
    tripoint_abs_ms destination;
    tripoint_abs_omt goal;
    int retry_after = -1;
};

struct route_state {
    std::string actor_id;
    std::uint64_t operation_generation = 0;
    std::uint64_t lease_epoch = 0;
    std::optional<tripoint_abs_omt> target;
    std::vector<tripoint_abs_omt> path;
    int retry_after = -1;
    std::optional<tripoint_abs_omt> failed_origin;
    std::optional<int> destination_cost;
    // Caller-supplied exclusion policy for a retained human route.
    std::vector<tripoint_abs_omt> policy_exclusions;
    // Short local-edge refusal, authenticated by the actor's operation.
    std::vector<boundary_refusal> local_boundary_refusals;
    void clear();
};

enum class route_status { step, reached, retry_pending, unavailable };
struct route_result {
    route_status status = route_status::unavailable;
    std::optional<tripoint_abs_omt> next;
};
using route_finder = std::function<std::vector<tripoint_abs_omt>( const tripoint_abs_omt &,
                     const tripoint_abs_omt & )>;
using edge_validator = std::function<bool( const tripoint_abs_omt &, const tripoint_abs_omt & )>;
route_result next_route_step( route_state &state, const tripoint_abs_omt &origin,
                              const tripoint_abs_omt &destination, int now, int retry_delay,
                              std::size_t retained_edges, const route_finder &find,
                              const edge_validator &valid,
                              std::optional<int> destination_cost = std::nullopt );

struct work {
    std::int64_t work_millis = 0;
    std::int64_t edge_cost_millis = 0;
    std::int64_t serviced_active_turns = -1;
    bool edge_bound = false;
    std::vector<tripoint_abs_omt> from;
    std::vector<tripoint_abs_omt> to;
    // Optional for old saves; the authenticated first shared call adopts it.
    std::vector<std::string> actor_ids;
    std::vector<std::uint64_t> actor_epochs;
    std::uint64_t operation_generation = 0;

    void clear();
    void serialize( JsonOut &json ) const;
    void deserialize( const JsonObject &jo );
};

// Shared preparation result; actor adapters authenticate and commit canonical
// bodies, while this owner selects routes, retries and paid edge work.
enum class phase { distant, local };
enum class edge_gate { distant, local_handoff, blocked };
enum class transit_status { route_ready, waiting, step, reached, retry_pending, unavailable, local_handoff };
struct actor_input {
    std::string actor_id;
    std::uint64_t operation_generation = 0;
    std::uint64_t lease_epoch = 0;
    tripoint_abs_omt origin;
    tripoint_abs_omt destination;
    route_state *route = nullptr;
    route_finder find;
    edge_validator valid;
    std::function<int( const tripoint_abs_omt & )> terrain_weight;
    std::function<edge_gate( const tripoint_abs_omt & )> gate;
    std::optional<int> destination_cost;
};
struct transit_policy {
    phase movement_phase = phase::distant;
    int now = 0;
    int retry_delay = 0;
    std::size_t retained_edges = 1;
    int slowest_speed = 0;
    std::optional<int> active_millis;
    std::optional<std::int64_t> fixed_edge_cost_millis;
    // Compatibility for an already bound retained pair with one shared edge.
    bool collapse_shared_edge = false;
};
struct transit_result {
    transit_status status = transit_status::unavailable;
    std::vector<std::optional<tripoint_abs_omt>> next;
    std::vector<std::vector<tripoint_abs_omt>> routes;
};
transit_result prepare_transit( work &state, const std::vector<actor_input> &actors,
                                const transit_policy &policy );

std::optional<std::int64_t> edge_cost_millis( int terrain_weight, int slowest_speed );
bool service( work &state, const tripoint_abs_omt &from, const tripoint_abs_omt &to,
              int terrain_weight, int slowest_speed, int active_millis, bool &move );
bool service_set( work &state, const std::vector<tripoint_abs_omt> &from,
                  const std::vector<tripoint_abs_omt> &to, int terrain_weight,
                  int slowest_speed, int active_millis, bool &move,
                  std::optional<std::int64_t> fixed_edge_cost_millis = std::nullopt );
} // namespace overmap_party_travel
#endif
