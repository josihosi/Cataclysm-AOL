#pragma once
#ifndef CATA_SRC_SCOUT_OBSERVATION_H
#define CATA_SRC_SCOUT_OBSERVATION_H
#include <string>
#include <memory>
#include <vector>
#include "coordinates.h"
class Character;
class npc;
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
struct visibility_read {
    bool known = false;
    bool visible = false;
    std::string reason;
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
        visibility_read read_signal( const Character &observer, const tripoint_abs_ms &origin,
                                     const tripoint_abs_ms &target,
                                     const std::vector<tripoint_abs_omt> &footprint );
        bool watching_member( const bandit_live_world::site_record &site, const npc &observer );
        size_t cached_view_count() const;
        // Decision-local work counts, not native simulation timing.
        size_t cached_vehicle_scan_count() const;
        size_t cached_vehicle_cell_query_count() const;
        size_t cached_vehicle_part_read_count() const;
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
