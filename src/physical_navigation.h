#pragma once
#ifndef CATA_SRC_PHYSICAL_NAVIGATION_H
#define CATA_SRC_PHYSICAL_NAVIGATION_H

#include <string>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include "coordinates.h"
#include "pathfinding.h"
#include "type_id.h"

class map;
class monster;
class npc;

// Native short paths over existing physical geometry. Route reads do not
// generate maps, discover actors, move bodies or mutate terrain.
namespace physical_navigation
{
enum class action : int { none, walk, open_door };
enum class boundary_action { refused, spent, moved };
// One already-authorized local step, using existing physical geometry. This
// does not acquire missing geometry or select a destination.
boundary_action step_npc_boundary( npc &body, const tripoint_abs_ms &destination );
struct npc_entry_result {
    bool interrupted = false;
    bool acted = false;
    bool moved = false;
    bool entered = false;
};
// Consume only the supplied local move budget along an existing native route.
// Unknown geometry, hazards and occupied tiles remain unavailable.
npc_entry_result move_npc_into_loaded_area( npc &body, map &loaded,
        const tripoint_abs_omt &next_omt, int funded_moves,
        const std::function<bool()> &still_owned );
struct step_read {
    action next = action::none;
    tripoint_abs_ms position;
    int move_cost = 0;
    bool known = false;
    std::string reason;
    std::uint64_t hazard_revision = 0;
    // A request for facts at the actor's immediate frontier, never a route
    // certificate. Only an authenticated movement owner may acquire it.
    std::optional<tripoint_abs_ms> missing_geometry;
};
// Bounded, value-only short refusal backoff. The owner authenticates actor,
// generation and representation, and clears this on a binding change.
class failure_cache
{
        struct entry {
            tripoint_abs_ms origin;
            tripoint_abs_ms goal;
            mtype_id type;
            pathfinding_settings settings;
            int recorded_turn = 0;
            int hp = 0;
            int speed = 0;
            bool movement = false;
            bool flies = false;
            bool submerges = false;
            bool climbs = false;
            bool opens_doors = false;
        };
        std::vector<entry> entries;
    public:
        void clear() { entries.clear(); }
        // A local caller may recheck during the recording turn so an immediate
        // opening is visible. Later openings can wait for the bounded retry.
        std::optional<step_read> find( const monster &body, const tripoint_abs_ms &goal,
                                       bool movement, int now, bool fresh_on_recorded_turn = false );
        void remember( const monster &body, const tripoint_abs_ms &goal, bool movement,
                       int now, const step_read &result );
};
class preparation;
// Optional scoped work counter for bounded caller tests; counts attempts, not
// disk reads, and owns no navigation state.
struct query_work_counts {
    std::size_t reads = 0;
    std::size_t native_routes = 0;
    std::size_t cached_refusals = 0;
};
class scoped_query_work_counter
{
    public:
        explicit scoped_query_work_counter( query_work_counts &counts );
        ~scoped_query_work_counter();
        scoped_query_work_counter( const scoped_query_work_counter & ) = delete;
        scoped_query_work_counter &operator=( const scoped_query_work_counter & ) = delete;
    private:
        query_work_counts *previous;
};
// Disposable positive path value. No map/actor pointers, failures or history.
// The caller owns identity/lifetime; reads consume only actual position changes.
class segment
{
        tripoint_abs_ms origin;
        tripoint_abs_ms goal;
        mtype_id type;
        pathfinding_settings settings;
        std::vector<tripoint_abs_ms> points;
        std::vector<int> costs;
        friend step_read read_step( const monster &, const tripoint_abs_ms &,
                                    const std::function<bool( const tripoint_abs_ms & )> &, preparation *, segment *, bool, failure_cache *, bool );
    public:
        void clear() { *this = segment(); }
        std::size_t size() const { return points.size(); }
        std::optional<tripoint_abs_ms> target() const {
            return points.empty() ? std::nullopt : std::optional<tripoint_abs_ms>( goal );
        }
};
// One operation's synchronous, read-only service span. No native action,
// terrain/field edit, world/file mutation, save, unload or transport may occur.
// Owns one compatible view, never a route/result or borrowed actor pointer.
class preparation
{
        struct implementation;
        std::unique_ptr<implementation> impl;
        friend step_read read_step( const monster &, const tripoint_abs_ms &,
                                    const std::function<bool( const tripoint_abs_ms & )> &, preparation *, segment *, bool, failure_cache *, bool );
    public:
        preparation();
        ~preparation();
        preparation( const preparation & ) = delete;
        preparation &operator=( const preparation & ) = delete;
};
// A transport owner may supply its actual occupied cells. They affect this
// short physical route only, never perception or the destination's identity.
step_read read_step( const monster &body, const tripoint_abs_ms &goal,
                     const std::function<bool( const tripoint_abs_ms & )> &occupied = {},
                     preparation *prepared = nullptr, segment *retained = nullptr,
                     bool allow_partial_approach = false, failure_cache *failures = nullptr,
                     bool fresh_on_recorded_turn = false );
// Actual movement owners only, after authenticating the canonical source and
// active opportunity. Loads at most one current/adjacent OMT through the normal
// native loader. All read/preparation views must have ended before this call.
// Loading has native world effects, but confers no actor/action/time credit.
bool acquire_movement_geometry( const tripoint_abs_ms &origin, const step_read &request );
// Caller must first authenticate the current physical owner and spend this
// action once. This opens one adjacent native door, and never moves the body.
bool commit_door( const monster &body, const tripoint_abs_ms &door );
}
#endif
