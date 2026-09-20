#pragma once

#include "coordinates.h"

#include <set>
#include <string>
#include <vector>

class Character;
class map;

namespace physical_light
{
/** A single source, before any OMT or signal aggregation occurs. */
enum class source_kind { carried_item, worn_item, weapon_mounted, ground_item,
                         vehicle_part, luminous_monster, field, furniture, terrain };

struct emitter {
    tripoint_abs_ms position;
    source_kind kind = source_kind::ground_item;
    int luminance = 0;
    bool directional = false;
    std::string provenance;
};

// A turn-local inventory of emitters in the loaded reality bubble.  It is
// indexed by actual source position, rather than by an avatar-centered
// observation radius; consumers decide their own source-derived envelope.
struct loaded_z_source_index {
    std::vector<emitter> item_emitters;
    std::vector<emitter> stationary_emitters;
    struct field_source {
        tripoint_abs_ms position;
        int fire_intensity = 0;
        int smoke_intensity = 0;
    };
    std::vector<field_source> field_sources;
    int tiles_examined = 0;
};

// Distant discovery may take fifteen in-game minutes.  Known sources are
// checked afresh, so this cache never supplies stale power or optical facts.
class loaded_source_sampler
{
    public:
        static constexpr int discovery_interval_turns = 15 * 60;
        loaded_z_source_index sample( const Character &carrier, map &here, int turn );
        void reset();

    private:
        std::set<tripoint_abs_ms> active_tiles;
        int last_turn = -1;
};

/**
 * The part of an emitted light which can leave its immediate local geometry.
 * This is deliberately source-local: callers must not merge it with another
 * emitter before deciding whether that other emitter can be seen.
 */
struct escape {
    bool exposed_to_sky = false;
    int side_leakage = 0;
    bool elevated_exposed = false;
};

/** A cache-free optical route supplied by the overmap observer. */
struct route {
    int distance_omt = 0;
    int vertical_offset = 0;
    bool source_exposed = false;
    bool vertical_sightline = false;
    int brightness_range_omt = 0;
    std::vector<int> terrain_see_costs;
};

struct detection {
    bool visible = false;
    int remaining_brightness = 0;
    // Light creates only an uncertain location.  Identity is never an output
    // of this physical query.
    bool recognizes_identity = false;
};

// Small, persistence-free gate for a loaded source pass.  Game state owns
// observations; this only prevents UI/render/debug and scheduler re-entry
// from claiming that a second physical turn occurred.
struct turn_sample_gate {
    int last_sampled_turn = -1;

    bool begin_advancing_turn( int turn, bool advancing );
};

/**
 * Collect powered item emitters in the local bubble and the character's
 * carried hierarchy.  Power is read from the item and its actual carrier;
 * this function never consumes charges.  Each returned record remains
 * separate so callers can resolve exposure before aggregating signals.
 */
std::vector<emitter> collect_item_emitters( const Character &carrier, map &here,
        int radius );

/** Collect current non-item light owners in the local bubble without mutation. */
std::vector<emitter> collect_stationary_emitters( map &here, const tripoint_bub_ms &origin,
        int radius );

/** Collect powered emitters on every currently loaded z-level without
 * generating map data or using player visibility. */
loaded_z_source_index index_loaded_z_sources( const Character &carrier, map &here );

/**
 * Query local transparency without building or changing a player's seen
 * cache.  Clear glass is a valid path; curtains, shutters, walls and floors
 * are not.  The bounded scan prevents a deep interior hall from borrowing a
 * distant opening's exposure.
 */
escape evaluate_escape( map &here, const tripoint_bub_ms &source );

/**
 * Decide whether an already-escaped bright source is detectable by one
 * observer.  This intentionally does not consume ordinary terrain-recognition
 * range: a successful result means only "light near this OMT".
 */
detection detect( const route &path );
}
