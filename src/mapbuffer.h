#pragma once
#ifndef CATA_SRC_MAPBUFFER_H
#define CATA_SRC_MAPBUFFER_H

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

/**
 * Store, buffer, save and load the entire world map.
 */
class mapbuffer
{
    public:
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
        submap *unserialize_submaps( const tripoint_abs_sm &p, bool generate_uniform = true );
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
        std::map<tripoint_abs_sm, std::vector<vehicle_part_position>> vehicle_cells; // NOLINT(cata-serialize)
        std::map<vehicle *, indexed_vehicle> indexed_vehicles; // NOLINT(cata-serialize)
        submap_map_t submaps; // NOLINT(cata-serialize)
};

extern mapbuffer MAPBUFFER;

#endif // CATA_SRC_MAPBUFFER_H
