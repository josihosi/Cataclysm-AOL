#pragma once
#ifndef CATA_SRC_MAPBUFFER_H
#define CATA_SRC_MAPBUFFER_H

#include <cstddef>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "coordinates.h"

class JsonArray;
class cata_path;
class submap;
class vehicle;
struct submap_sunlight_input;

// Optional synchronous diagnostic counter for tests and focused performance
// investigations. Resident-only lookups are excluded; no data is retained.
struct submap_load_counts {
    std::size_t lookup_calls = 0;
    std::size_t unserialize_calls = 0;
    std::size_t deserialize_calls = 0;
};
class scoped_submap_load_counter
{
    public:
        explicit scoped_submap_load_counter( submap_load_counts &counts );
        ~scoped_submap_load_counter();
        scoped_submap_load_counter( const scoped_submap_load_counter & ) = delete;
        scoped_submap_load_counter &operator=( const scoped_submap_load_counter & ) = delete;
    private:
        submap_load_counts *previous;
};

/**
 * Store, buffer, save and load the entire world map.
 */
class mapbuffer
{
        struct existing_read_context;
    public:
        /** One synchronous, read-only geometry preparation. Reuse only the
         * current successfully opened archive; no geometry/absence cache.
         * Finish before world/dimension/file changes or native actions. */
        class existing_submap_reader
        {
            public:
                explicit existing_submap_reader( mapbuffer &source );
                ~existing_submap_reader();
                existing_submap_reader( const existing_submap_reader & ) = delete;
                existing_submap_reader &operator=( const existing_submap_reader & ) = delete;
                submap *lookup( const tripoint_abs_sm &p );
                /** Exact native uniform terrain only after checked serialized
                 * quad absence. Resident/serialized overrides, unknown terrain
                 * and read failures refuse. Caller owns the temporary value;
                 * this never inserts, saves, generates or simulates a submap.
                 * read_error distinguishes refusal from an existing override;
                 * callers must not attempt ordinary archive lookup after error. */
                std::unique_ptr<submap> read_omitted_uniform( const tripoint_abs_sm &p,
                        bool *read_error = nullptr );
                // Checked per-entry native physical metadata. No cold entity
                // decode/install or writable archive recovery. One decision
                // only; cache entries also bind the supplied native weather.
                std::unique_ptr<submap_sunlight_input> read_sunlight_input(
                    const tripoint_abs_sm &p, float outside_sight_penalty );
            private:
                mapbuffer &source;
                std::unique_ptr<existing_read_context> context;
        };

        mapbuffer();
        ~mapbuffer();

        /** Store all submaps in this instance into savefiles.
         * @param delete_after_save If true, the saved submaps are removed
         * from the mapbuffer (and deleted).
         **/
        void save( bool delete_after_save = false );

        /** Delete all buffered submaps. **/
        void clear();

        /** Delete all buffered submaps except those inside the reality bubble.
         *
         * This exists for the sake of the tests to reduce their memory
         * consumption; it's probably not sane to use in general gameplay.
         */
        void clear_outside_reality_bubble();

        /** Add a new submap to the buffer.
         *
         * @param p The absolute world position in submap coordinates.
         * Same as the ones in @ref lookup_submap.
         * @param sm The submap. If the submap has been added, the unique_ptr
         * is released (set to NULL).
         * @return true if the submap has been stored here. False if there
         * is already a submap with the specified coordinates. The submap
         * is not stored and the given unique_ptr retains ownsership.
         */
        bool add_submap( const tripoint_abs_sm &p, std::unique_ptr<submap> &sm );
        // Old overload that we should stop using, but it's complicated
        bool add_submap( const tripoint_abs_sm &p, submap *sm );

        /** Get a submap stored in this buffer.
         *
         * @param p The absolute world position in submap coordinates.
         * Same as the ones in @ref add_submap.
         * @return NULL if the submap is not in the mapbuffer
         * and could not be loaded. The mapbuffer takes care of the returned
         * submap object, don't delete it on your own.
         */
        submap *lookup_submap( const tripoint_abs_sm &p );
        /** Read cached or serialized geometry without generating omitted uniform submaps. */
        submap *lookup_submap_existing( const tripoint_abs_sm &p );
        /** Resident-only lookup; no serialized read, insertion or generation. */
        submap *lookup_submap_cached( const tripoint_abs_sm &p ) const {
            const auto it = submaps.find( p );
            return it == submaps.end() ? nullptr : it->second.get();
        }
        // Cheaper version of the above for when you only care about whether the
        // submap exists or not.
        bool submap_exists( const tripoint_abs_sm &p );

        // Cheaper version of the above for when you don't mind some false results
        bool submap_exists_approx( const tripoint_abs_sm &p );

        // Resident physical part positions, indexed when ownership/geometry changes.
        // A part may overhang arbitrarily far from its owning submap. Queries neither
        // scan unrelated owners nor load/generate missing geometry.
        using vehicle_part_position = std::pair<vehicle *, tripoint_abs_ms>;
        const std::vector<vehicle_part_position> &vehicle_parts_at( const tripoint_abs_sm &p ) const;
        void update_vehicle_index( vehicle &v );
        void forget_vehicle( vehicle &v );

    private:
        using submap_map_t = std::map<tripoint_abs_sm, std::unique_ptr<submap>>;

    public:
        inline submap_map_t::iterator begin() {
            return submaps.begin();
        }
        inline submap_map_t::iterator end() {
            return submaps.end();
        }

    private:
        // There's a very good reason this is private,
        // if not handled carefully, this can erase in-use submaps and crash the game.
        void remove_submap( const tripoint_abs_sm &addr );
        submap *lookup_submap_existing( const tripoint_abs_sm &p, existing_read_context *context );
        submap *unserialize_submaps( const tripoint_abs_sm &p, bool generate_uniform = true,
                                    existing_read_context *context = nullptr );
        bool submap_file_exists( const tripoint_abs_sm &p );
        void deserialize( const JsonArray &ja, bool skip_existing = false );
        void save_quad(
            const cata_path &dirname, const cata_path &filename,
            const tripoint_abs_omt &om_addr, std::list<tripoint_abs_sm> &submaps_to_delete,
            bool delete_after_save );
        void index_vehicle( vehicle &v, tripoint_abs_sm owner );
        struct indexed_vehicle {
            tripoint_abs_sm owner;
            std::set<tripoint_abs_sm> cells;
        };
        // Destroy submaps first: vehicle destructors unregister while these exist.
        std::map<tripoint_abs_sm, std::vector<vehicle_part_position>>
                vehicle_cells; // NOLINT(cata-serialize)
        std::map<vehicle *, indexed_vehicle> indexed_vehicles; // NOLINT(cata-serialize)
        submap_map_t submaps; // NOLINT(cata-serialize)
};

extern mapbuffer MAPBUFFER;

#endif // CATA_SRC_MAPBUFFER_H
