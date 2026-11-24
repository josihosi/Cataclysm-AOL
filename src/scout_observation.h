#pragma once
#ifndef CATA_SRC_SCOUT_OBSERVATION_H
#define CATA_SRC_SCOUT_OBSERVATION_H
#include <string>
#include <memory>
#include <optional>
#include <vector>
#include "coordinates.h"
class Character;
class Creature;
class npc;
class monster;
namespace bandit_live_world
{
struct site_record;
}
namespace scout_observation
{
// Fixed physical bearing/elevation, including equal minor axes; bounded by the
// site reader's native optical range before any geometry is inspected.
std::vector<tripoint_abs_ms> site_sightline( const tripoint_abs_ms &origin,
        const tripoint_abs_ms &target );
// The target OMT segment, plus its preceding point for a boundary z transition.
std::vector<tripoint_abs_ms> site_edge_sightline( const tripoint_abs_ms &origin,
        const tripoint_abs_ms &target );
struct visibility_read {
    bool known = false;
    bool visible = false;
    std::string reason;
};
// Physical access facts are present only after native optical visibility succeeds.
struct access_read {
    visibility_read visibility;
    std::string facts;
};
struct actor_capability {
    int maximum_range = 0;
    float native_range_scale = 1.0f;
};
// Decision-scoped geometry reuse; no visibility state survives movement or another turn.
class site_reader
{
        struct implementation;
        std::unique_ptr<implementation> impl;
    public:
        site_reader();
        ~site_reader();
        visibility_read read( const Character &observer, const tripoint_abs_ms &origin,
                              const tripoint_abs_ms &target,
                              const std::vector<tripoint_abs_omt> &footprint = {} );
        visibility_read read_character( const Character &observer, const Character &target,
                                        const std::vector<tripoint_abs_omt> &footprint = {} );
        // Unlike site-directed scouts this requires the entire physical ray,
        // including the observer. Unknown cells cannot reveal an occupant.
        visibility_read read_actor( const Creature &observer, const Creature &target,
                                    const actor_capability &capability,
                                    bool visibility_only = false );
        // Exact physical geometry only, including the origin. Signal consumers
        // use this for the actual lookout's exit ray, separately from distant
        // source reach and from recognition of any creature.
        access_read read_access( const monster &observer, const tripoint_abs_ms &target );
        // Prospective footing and geometric view only, not a sighting. The
        // caller supplies its observer type's potential range; arrived bodies
        // still acquire actual evidence under current light and capability.
        visibility_read read_lookout( const tripoint_abs_ms &origin,
                                      const tripoint_abs_ms &target, int potential_range );
        visibility_read read_ray( const tripoint_abs_ms &origin,
                                  const tripoint_abs_ms &target );
        visibility_read read_signal( const Character &observer, const tripoint_abs_ms &origin,
                                     const tripoint_abs_ms &target,
                                     const std::vector<tripoint_abs_omt> &footprint );
        visibility_read read_watch_signal( const Character &observer, const tripoint_abs_omt &watch,
                                           const tripoint_abs_ms &target );
        visibility_read read_watch_character( const Character &observer, const Character &target,
                                              const tripoint_abs_omt &watch );
        bool watching_member( const bandit_live_world::site_record &site, const npc &observer );
        // Optional first refusal from the actual decision; no additional geometry read.
        bool watching_member( const bandit_live_world::site_record &site, const npc &observer,
                              std::string *reason );
        // Current native perception only; missing hostile optics supplies no position.
        std::vector<tripoint_abs_ms> perceived_threats( const npc &observer );
        size_t cached_view_count() const;
        // Decision-local work counts, not native simulation timing.
        size_t cached_vehicle_scan_count() const;
        size_t cached_vehicle_cell_query_count() const;
        size_t cached_vehicle_part_read_count() const;
        // Existing decision-local inputs, not physical I/O or cast-ray counts.
        size_t cached_lighting_submap_count() const;
        size_t cached_sunlight_summary_count() const;
        size_t cached_lighting_path_group_count() const;
        // Snapshot only an already computed decision value. No physical read,
        // cache preparation, native action or clock settlement is performed.
        std::optional<float> cached_lighting_at( const tripoint_abs_ms &point ) const;
};
// Stationary, site-directed callers own watch/identity validation. No map generation or actor spawning.
bool watching_member( const bandit_live_world::site_record &site, const npc &observer );
visibility_read read_character( const Character &observer, const Character &target );
visibility_read read_site_tile( const Character &observer, const tripoint_abs_ms &origin,
                                const tripoint_abs_ms &target );
bool associated_with_site( const tripoint_abs_omt &position,
                           const std::vector<tripoint_abs_omt> &footprint,
                           const std::vector<tripoint_abs_omt> &previous_sightings = {} );
}
#endif
