#include "scout_observation.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <functional>
#include <set>
#include <memory>
#include <map>

#include "character.h"
#include "avatar.h"
#include "bandit_live_world.h"
#include "effect.h"
#include "do_turn.h"
#include "field.h"
#include "flag.h"
#include "lightmap.h"
#include "game.h"
#include "level_cache.h"
#include "line.h"
#include "map.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "monster.h"
#include "npc.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "submap.h"
#include "weather.h"
#include "weather_gen.h"
#include "weather_type.h"
#include "vehicle.h"

namespace
{
// Decision-local snapshots of directed cells in the resident physical part index.
// Native ownership/cache updates maintain that index; observation never enumerates
// MAPBUFFER or unrelated vehicle owners. Snapshots survive native cache refreshes
// while constructing a view over these same vehicles.
class vehicle_footprint_index
{
        mutable size_t queries = 0;
        mutable size_t part_reads = 0;
        mutable std::map<tripoint_abs_sm, std::vector<mapbuffer::vehicle_part_position>> cells;
    public:
        bool coordinates_match( const vehicle &v ) const {
            // Native map::loadn binds sm_pos. A decoded cold body's default
            // zero is valid only if that really is its resident owner; do not
            // relocate it merely to make a read succeed.
            const submap *owner = MAPBUFFER.lookup_submap_cached( v.sm_pos );
            if( !owner || std::none_of( owner->vehicles.begin(), owner->vehicles.end(),
            [&]( const auto &candidate ) { return candidate.get() == &v; } ) ) {
                return false;
            }
            // Native cache projection visits all parts, including overhangs.
            // Authenticate each against the existing resident index before
            // admitting that vehicle; do not bind or move an unproved body.
            for( const auto &part : v.get_all_parts_with_fakes() ) {
                if( part.part().removed ) {
                    continue;
                }
                const auto position = v.abs_part_pos( part.part() );
                if( position.z() < -OVERMAP_DEPTH || position.z() > OVERMAP_HEIGHT ) {
                    return false;
                }
                const auto &indexed = at( project_to<coords::sm>( position ) );
                if( std::none_of( indexed.begin(), indexed.end(), [&]( const auto &entry ) {
                return entry.first == &v && entry.second == position;
                } ) ) {
                    return false;
                }
            }
            return true;
        }
        bool coordinates_match( const mapbuffer::vehicle_part_position &entry ) const {
            if( !entry.first || !coordinates_match( *entry.first ) ) {
                return false;
            }
            for( const auto &part : entry.first->get_all_parts_with_fakes() ) {
                if( entry.first->abs_part_pos( part.part() ) == entry.second ) {
                    return true;
                }
            }
            return false;
        }
        const std::vector<mapbuffer::vehicle_part_position> &at( const tripoint_abs_sm &pos ) const {
            ++queries;
            const auto found = cells.find( pos );
            if( found != cells.end() ) {
                return found->second;
            }
            const auto &parts = MAPBUFFER.vehicle_parts_at( pos );
            part_reads += parts.size();
            return cells.emplace( pos, parts ).first->second;
        }
        size_t query_count() const {
            return queries;
        }
        size_t part_count() const {
            return part_reads;
        }
        size_t scan_count() const {
            return 0;
        }
};

// A bounded read-only window over existing submaps. Missing cells are opaque placeholders,
// never accepted as observed geometry; neither load/mapgen nor active-item simulation runs.
class existing_view : public map
{
        submap unknown;
        submap ignored;
        int size;
        int low;
        int high;
        bool lighting_view;
        int observed_high;
        tripoint_abs_sm lighting_center;
        // These are bounded window membership facts, not a coordinate index.
        // Native light preparation visits each plane repeatedly; keep its
        // successful admissions in the same local grid as the borrowed map.
        using submap_mask = std::bitset<MAPSIZE * MAPSIZE * OVERMAP_LAYERS>;
        submap_mask available;
        std::vector<std::unique_ptr<submap>> omitted_uniform;
        std::vector<tripoint_abs_omt> lighting_footprint;
        std::map<tripoint_abs_seg, std::unique_ptr<mapbuffer::existing_submap_reader>> lighting_readers;
        using cell_mask = std::bitset<MAPSIZE_X * MAPSIZE_Y>;
        std::array<std::unique_ptr<cell_mask>, OVERMAP_LAYERS> lighting_validated;
        vehicle_footprint_index *lighting_vehicles = nullptr;
        submap_mask unavailable;
        std::set<int> lighting_dirty;
        std::map<tripoint_abs_ms, std::optional<float>> light_results;
        std::map<tripoint_abs_ms, float> source_results;
        std::map<tripoint_abs_ms, std::pair<float, float>> light_bounds;
        std::map<tripoint_abs_sm, std::optional<submap_sunlight_input>> lighting_prefix_inputs;
        bool visibility_prepared = false;

        std::optional<size_t> submap_index( const tripoint_abs_sm &pos ) const {
            const int x = pos.x() - get_abs_sub().x();
            const int y = pos.y() - get_abs_sub().y();
            if( x < 0 || x >= size || y < 0 || y >= size ||
                pos.z() < -OVERMAP_DEPTH || pos.z() > OVERMAP_HEIGHT ) {
                return std::nullopt;
            }
            // The map's z coordinate is absolute, even when its origin is on
            // a roof or underground. Only x/y are relative to get_abs_sub().
            return ( pos.z() + OVERMAP_DEPTH ) * MAPSIZE * MAPSIZE + y * MAPSIZE + x;
        }
        bool submap_marked( const submap_mask &mask, const tripoint_abs_sm &pos ) const {
            const auto index = submap_index( pos );
            return index && mask.test( *index );
        }
        void mark_submap( submap_mask &mask, const tripoint_abs_sm &pos, bool value = true ) {
            const auto index = submap_index( pos );
            if( index ) {
                mask.set( *index, value );
            }
        }

        bool load_light_submap( const tripoint_abs_sm &pos ) {
            if( submap_marked( available, pos ) ) {
                return true;
            }
            if( submap_marked( unavailable, pos ) ) {
                return false;
            }
            auto &reader = lighting_readers[project_to<coords::seg>( pos )];
            if( !reader ) {
                reader = std::make_unique<mapbuffer::existing_submap_reader>( MAPBUFFER );
            }
            bool read_error = false;
            auto uniform = reader->read_omitted_uniform( pos, &read_error );
            submap *sm = uniform ? uniform.get() : read_error ? nullptr : reader->lookup( pos );
            if( !sm ) {
                mark_submap( unavailable, pos );
                return false;
            }
            const auto &parts = lighting_vehicles->at( pos );
            if( std::any_of( sm->vehicles.begin(), sm->vehicles.end(), [&]( const auto &v ) {
                return v->sm_pos != pos || !lighting_vehicles->coordinates_match( *v );
            } ) || std::any_of( parts.begin(), parts.end(), [&]( const auto &part ) {
                return !lighting_vehicles->coordinates_match( part );
            } ) ) {
                mark_submap( unavailable, pos );
                return false;
            }
            if( uniform ) {
                omitted_uniform.push_back( std::move( uniform ) );
            }
            const tripoint_rel_sm local( pos.x() - get_abs_sub().x(),
                                         pos.y() - get_abs_sub().y(), pos.z() );
            setsubmap( get_nonant( local ), sm );
            mark_submap( available, pos );
            update_vehicle_list( sm, pos.z() );
            for( const auto &part : parts ) {
                auto &cache = const_cast<level_cache &>( get_cache_ref( part.second.z() ) );
                cache.vehicle_list.insert( part.first );
            }
            invalidate_map_cache( pos.z() );
            lighting_dirty.insert( pos.z() );
            if( pos.z() < OVERMAP_HEIGHT ) {
                set_floor_cache_dirty( pos.z() + 1 );
                lighting_dirty.insert( pos.z() + 1 );
            }
            return true;
        }

        bool require_light_halo( const tripoint_bub_ms &p ) {
            // Outside flags depend on the 3x3 tile halo, but most of those
            // tiles share one submap. Admit each intersecting owner once.
            // Clipping before projection preserves the native window edge.
            const int first_x = std::max( 0, p.x() - 1 ) / SEEX;
            const int last_x = std::min( size * SEEX - 1, p.x() + 1 ) / SEEX;
            const int first_y = std::max( 0, p.y() - 1 ) / SEEY;
            const int last_y = std::min( size * SEEY - 1, p.y() + 1 ) / SEEY;
            for( int x = first_x; x <= last_x; ++x ) {
                for( int y = first_y; y <= last_y; ++y ) {
                    const tripoint_abs_sm owner( get_abs_sub().xy() + point( x, y ), p.z() );
                    if( !load_light_submap( owner ) ) {
                        return false;
                    }
                }
            }
            return true;
        }

        // Authenticate only physical inputs here. Cache preparation is grouped
        // until a native consumer actually reads the requested cache planes.
        bool require_light_cell( const tripoint_bub_ms &p ) {
            if( !inbounds( p ) ) {
                return false;
            }
            auto &validated = lighting_validated[p.z() + OVERMAP_DEPTH];
            const size_t index = p.y() * MAPSIZE_X + p.x();
            if( validated && validated->test( index ) ) {
                return true;
            }
            // Native outside flags consult the tile and its immediate halo.
            // Load only intersecting submaps, not an upper-volume window.
            if( !require_light_halo( p ) ) {
                return false;
            }
            const submap *physical = get_submap_at_grid(
                                         tripoint_rel_sm( p.x() / SEEX, p.y() / SEEY, p.z() ) );
            if( physical && physical->is_uniform() ) {
                // Native uniform transparency samples outside at the NW tile
                // and copies it across this entire submap. Prove that tile's
                // physical outside halo too, without recursively preparing
                // the neighbouring submaps' own transparency samples.
                const tripoint_bub_ms corner( p.x() / SEEX * SEEX, p.y() / SEEY * SEEY, p.z() );
                if( !require_light_halo( corner ) ) {
                    return false;
                }
            }
            const auto &terrain = ter( p ).obj();
            if( p.z() > -OVERMAP_DEPTH &&
                ( terrain.has_flag( ter_furn_flag::TFLAG_NO_FLOOR ) ||
                  terrain.has_flag( ter_furn_flag::TFLAG_NO_FLOOR_WATER ) ||
                  terrain.has_flag( ter_furn_flag::TFLAG_GOES_DOWN ) ||
                  terrain.has_flag( ter_furn_flag::TFLAG_TRANSPARENT_FLOOR ) ) &&
                !load_light_submap( project_to<coords::sm>( get_abs( p - tripoint::above ) ) ) ) {
                return false; // Native floor may be supplied by furniture below.
            }
            if( !validated ) {
                validated = std::make_unique<cell_mask>();
            }
            validated->set( index );
            return true;
        }

        void prepare_light_cache( const int low_z, const int high_z ) {
            std::set<int> levels;
            for( auto it = lighting_dirty.lower_bound( low_z );
                 it != lighting_dirty.end() && *it <= high_z; ++it ) {
                levels.insert( *it );
            }
            if( levels.empty() ) {
                return;
            }
            raid_perf_scope diagnostic( raid_perf_stage::scout_view_cache );
            rebuild_vehicle_level_caches();
            for( int z : levels ) {
                build_outside_cache( z );
                build_transparency_cache( z );
                build_floor_cache( z );
            }
            // All requested terrain planes first, then their contributing
            // vehicle owners. In particular a rebuilt upper floor must regain
            // an already admitted lower vehicle's roof before it is consumed.
            std::set<int> vehicle_levels = levels;
            for( int z : levels ) {
                if( z > -OVERMAP_DEPTH ) {
                    vehicle_levels.insert( z - 1 );
                }
            }
            for( int z : vehicle_levels ) {
                do_vehicle_caching( z );
            }
            for( int z : levels ) {
                lighting_dirty.erase( z );
            }
        }

        const std::optional<submap_sunlight_input> &sunlight_metadata( const tripoint_abs_sm &pos ) {
            const auto found = lighting_prefix_inputs.find( pos );
            if( found != lighting_prefix_inputs.end() ) {
                return found->second;
            }
            auto &reader = lighting_readers[project_to<coords::seg>( pos )];
            if( !reader ) {
                reader = std::make_unique<mapbuffer::existing_submap_reader>( MAPBUFFER );
            }
            raid_perf_scope diagnostic( raid_perf_stage::scout_light_dependencies );
            auto physical = reader->read_sunlight_input( pos, get_weather().weather_id->sight_penalty );
            std::optional<submap_sunlight_input> value;
            if( physical ) {
                value = std::move( *physical );
            }
            return lighting_prefix_inputs.emplace( pos, std::move( value ) ).first->second;
        }

        std::optional<sunlight_submap_input> sunlight_summary( const tripoint_rel_sm &local ) {
            const tripoint_abs_sm pos( get_abs_sub().xy() + local.xy().raw(), local.z() );
            const auto &physical = sunlight_metadata( pos );
            if( !physical ) {
                return std::nullopt;
            }
            sunlight_submap_input result{ physical->uniform, std::nullopt, std::nullopt, {} };
            auto floors = physical->floors;
            const auto &below = sunlight_metadata( pos - tripoint::above );
            if( below ) {
                floors |= below->sun_roofs;
            }
            if( ( floors | physical->nonclear ).any() ) {
                result.clear = false; // Additional vehicle facts cannot undo this blocker.
            }
            if( submap_marked( available, pos ) && submap_marked( available, pos - tripoint::above ) ) {
                prepare_light_cache( pos.z() - 1, pos.z() );
            }
            // A floor already proved by the native terrain/vehicle cache is
            // also a decisive blocker. This admits no extra geometry: it only
            // uses clean, authenticated planes already needed by the point.
            if( submap_marked( available, pos ) && submap_marked( available, pos - tripoint::above ) &&
                !lighting_dirty.count( pos.z() ) && !lighting_dirty.count( pos.z() - 1 ) ) {
                const auto &cache = get_cache_ref( pos.z() );
                for( int x = local.x() * SEEX; x < ( local.x() + 1 ) * SEEX; ++x ) {
                    for( int y = local.y() * SEEY; y < ( local.y() + 1 ) * SEEY; ++y ) {
                        if( cache.floor_cache[x][y] ) {
                            result.clear = false;
                            break;
                        }
                    }
                    if( result.clear && !*result.clear ) {
                        break;
                    }
                }
            }
            if( ( floors | physical->opaque ).all() ) {
                result.all_blocking = true;
            }
            const bool no_vehicles = below && !physical->vehicles && !below->vehicles &&
                                     lighting_vehicles->at( pos ).empty() &&
                                     lighting_vehicles->at( pos - tripoint::above ).empty();
            if( no_vehicles ) {
                const auto clear = physical->clear & ~floors;
                if( clear.all() ) {
                    result.clear = true;
                }
                if( clear.any() ) {
                    result.all_blocking = false;
                }
                result.air_path = physical->air & ~floors;
            }
            return result;
        }

    public:
        size_t lighting_submap_count() const {
            return lighting_view ? available.count() : 0;
        }
        size_t sunlight_summary_count() const {
            return lighting_prefix_inputs.size();
        }
        size_t lighting_path_group_count() const {
            return lighting_view ? lighting_readers.size() : 0;
        }
        existing_view( vehicle_footprint_index &vehicles, const tripoint_abs_ms &origin,
                       const tripoint_abs_ms &target,
                       const bool padding = true, const bool full_window = false,
                       const bool native_lighting = false,
                       const std::vector<tripoint_abs_omt> *footprint = nullptr ) :
            map( full_window ? MAPSIZE : std::min( MAPSIZE, std::max( std::abs( origin.x() - target.x() ),
                                                   std::abs( origin.y() - target.y() ) ) / SEEX + ( padding ? 3 : 2 ) ), true ),
            size( full_window ? MAPSIZE : std::min( MAPSIZE, std::max( std::abs( origin.x() - target.x() ),
                                                    std::abs( origin.y() - target.y() ) ) / SEEX + ( padding ? 3 : 2 ) ) ) {
            unknown.set_all_ter( ter_str_id( "t_wall" ), native_lighting );
            ignored.set_all_ter( ter_str_id( "t_open_air" ) );
            lighting_view = native_lighting;
            lighting_center = project_to<coords::sm>( origin );
            if( footprint ) {
                lighting_footprint = *footprint;
            }
            const tripoint_abs_sm first = native_lighting ?
                                          lighting_center - point( MAPSIZE / 2, MAPSIZE / 2 ) :
                                          project_to<coords::sm>( tripoint_abs_ms(
                                                  std::min( origin.x(), target.x() ) - ( padding ? SEEX : 0 ),
                                                  std::min( origin.y(), target.y() ) - ( padding ? SEEY : 0 ), origin.z() ) );
            set_abs_sub( first );
            low = std::max( -OVERMAP_DEPTH, std::min( origin.z(), target.z() ) - 1 );
            observed_high = std::min( OVERMAP_HEIGHT, std::max( origin.z(), target.z() ) );
            high = native_lighting ? OVERMAP_HEIGHT : observed_high;
            if( native_lighting ) {
                lighting_vehicles = &vehicles;
                for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
                    for( int x = 0; x < size; ++x ) {
                        for( int y = 0; y < size; ++y ) {
                            setsubmap( get_nonant( tripoint_rel_sm( x, y, z ) ), &unknown );
                        }
                    }
                }
                return;
            }
            mapbuffer::existing_submap_reader existing( MAPBUFFER );
            for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
                for( int x = 0; x < size; ++x ) {
                    for( int y = 0; y < size; ++y ) {
                        const tripoint_abs_sm pos( first.x() + x, first.y() + y, z );
                        const auto omt = project_to<coords::omt>( pos );
                        const bool in_site = !footprint || std::any_of( footprint->begin(), footprint->end(),
                        [&]( const auto & tile ) {
                            return tile.xy() == omt.xy();
                        } );
                        submap *sm = nullptr;
                        if( in_site && z >= low && z <= high ) {
                            bool read_error = false;
                            auto uniform = existing.read_omitted_uniform( pos, &read_error );
                            if( uniform ) {
                                sm = uniform.get();
                                omitted_uniform.push_back( std::move( uniform ) );
                            } else if( !read_error ) {
                                sm = existing.lookup( pos );
                            }
                        }
                        // Geometry admission indexes physical vehicle parts independently of
                        // this view, including overhangs from excluded resident owners.
                        if( sm && ( std::any_of( sm->vehicles.begin(), sm->vehicles.end(),
                        [&]( const auto &v ) { return v->sm_pos != pos || !vehicles.coordinates_match( *v ); } ) ||
                        std::any_of( vehicles.at( pos ).begin(), vehicles.at( pos ).end(),
                        [&]( const auto &part ) { return !vehicles.coordinates_match( part ); } ) ) ) {
                            mark_submap( unavailable, pos );
                            sm = nullptr;
                        }
                        if( sm ) {
                            update_vehicle_list( sm, z );
                            mark_submap( available, pos );
                        } else {
                            sm = in_site ? &unknown : &ignored;
                        }
                        setsubmap( get_nonant( tripoint_rel_sm( x, y, z ) ), sm );
                    }
                }
            }
            for( int z = low; z <= high; ++z ) {
                for( int x = 0; x < size; ++x ) {
                    for( int y = 0; y < size; ++y ) {
                        const tripoint_abs_sm pos( first.x() + x, first.y() + y, z );
                        if( submap_marked( unavailable, pos ) ) {
                            continue;
                        }
                        const auto omt = project_to<coords::omt>( pos );
                        if( footprint && std::none_of( footprint->begin(), footprint->end(),
                        [&]( const auto & tile ) {
                        return tile.xy() == omt.xy();
                        } ) ) {
                            continue;
                        }
                        for( const auto &part : vehicles.at( pos ) ) {
                            if( !vehicles.coordinates_match( part ) ) {
                                mark_submap( available, pos, false );
                                setsubmap( get_nonant( tripoint_rel_sm( x, y, z ) ), &unknown );
                                continue;
                            }
                            auto &cache = const_cast<level_cache &>( get_cache_ref( z ) );
                            const auto part_omt = project_to<coords::omt>( part.second );
                            if( footprint && std::none_of( footprint->begin(), footprint->end(),
                            [&]( const auto & tile ) {
                            return tile.xy() == part_omt.xy();
                            } ) ) {
                                continue;
                            }
                            cache.vehicle_list.insert( part.first );
                        }
                    }
                }
            }
        }
        void prepare_visibility() {
            if( lighting_view || visibility_prepared ) {
                return;
            }
            visibility_prepared = true;
            raid_perf_scope diagnostic( raid_perf_stage::scout_view_cache );
            rebuild_vehicle_level_caches();
            for( int z = low; z <= high; ++z ) {
                build_outside_cache( z );
                build_transparency_cache( z );
                set_floor_cache_dirty( z );
                build_floor_cache( z );
            }
            // Match native build_map_cache: every terrain level first, then
            // vehicle caches. A roof changes the next level's floor cache and
            // must not be overwritten by a later terrain rebuild.
            for( int z = low; z <= high; ++z ) {
                do_vehicle_caching( z );
            }
            for( int z = low; z <= high; ++z ) {
                level_cache &cache = const_cast<level_cache &>( get_cache_ref( z ) );
                cache.natural_light_level_cache = g->natural_light_level( z );
                for( int x = 0; x < size * SEEX; ++x ) {
                    for( int y = 0; y < size * SEEY; ++y ) {
                        const tripoint_bub_ms p( x, y, z );
                        const tripoint_abs_ms abs = get_abs( p );
                        const map &live = get_map();
                        if( live.inbounds( abs ) ) {
                            const tripoint_bub_ms live_p = live.get_bub( abs );
                            const level_cache &live_cache = live.get_cache_ref( z );
                            cache.lm[x][y] = live_cache.lm[live_p.x()][live_p.y()];
                            cache.transparency_cache[x][y] = live_cache.transparency_cache[live_p.x()][live_p.y()];
                            cache.floor_cache[x][y] = live_cache.floor_cache[live_p.x()][live_p.y()];
                            cache.outside_cache[x][y] = live_cache.outside_cache[live_p.x()][live_p.y()];
                        } else {
                            cache.lm[x][y] = four_quadrants{};
                        }
                    }
                }
            }
        }
        bool scope_matches( const std::vector<tripoint_abs_omt> *footprint ) const {
            return footprint ? lighting_footprint == *footprint : lighting_footprint.empty();
        }
        bool light_matches( const tripoint_abs_ms &point ) const {
            return lighting_view && project_to<coords::sm>( point ) == lighting_center;
        }
        std::optional<float> cached_lighting_at( const tripoint_abs_ms &point ) const {
            const auto found = light_results.find( point );
            return found != light_results.end() ? found->second : std::nullopt;
        }
        std::optional<float> native_ambient( const tripoint_abs_ms &point,
                const std::function<bool( float )> *sufficient = nullptr,
                std::optional<float> *source_light = nullptr,
                const std::function<bool( float, float )> *predicate_bounds = nullptr ) {
            if( source_light ) {
                source_light->reset();
            }
            if( !lighting_view ) {
                return std::nullopt;
            }
            const auto found = light_results.find( point );
            const auto source = source_results.find( point );
            if( found != light_results.end() && ( found->second || !sufficient ) &&
                ( !source_light || source != source_results.end() ) ) {
                if( source_light ) {
                    *source_light = source->second;
                }
                return found->second;
            }
            // A previous observer may have settled on physical light bounds.
            // Let this observer test those bounds with its own sight predicate.
            const auto bounds = light_bounds.find( point );
            if( predicate_bounds && source_light && source != source_results.end() &&
                bounds != light_bounds.end() ) {
                *source_light = source->second;
                if( ( *predicate_bounds )( bounds->second.first, bounds->second.second ) ) {
                    return std::nullopt;
                }
                source_light->reset();
            }
            const sunlight_prefix_input prefix = [&]( const tripoint_rel_sm &sm ) {
                return sunlight_summary( sm );
            };
            const std::function<void( int, int )> prepare = [&]( const int first, const int last ) {
                prepare_light_cache( first, last );
            };
            const std::function<bool( float, float )> remember_bounds = [&]( float lower, float upper ) {
                if( source_light && *source_light ) {
                    light_bounds.insert_or_assign( point, std::make_pair( lower, upper ) );
                }
                return ( *predicate_bounds )( lower, upper );
            };
            const auto light = generate_observed_lightmap( get_bub( point ),
            [&]( const tripoint_bub_ms &p ) { return require_light_cell( p ); }, &prefix, sufficient,
            source_light, &prepare, predicate_bounds ? &remember_bounds : nullptr );
            if( source_light && *source_light ) {
                source_results.insert_or_assign( point, **source_light );
            }
            light_results.insert_or_assign( point, light );
            return light;
        }
        bool covers( const tripoint_abs_ms &origin, const tripoint_abs_ms &target ) const {
            return !lighting_view && inbounds( origin ) && inbounds( target ) &&
                   origin.z() >= low && origin.z() <= high && target.z() >= low && target.z() <= high;
        }
        bool has_geometry( const tripoint_abs_ms &p ) const {
            // Terrain and every indexed vehicle part were authenticated before
            // native cache projection. Unbound/mismatched owners stay absent.
            return inbounds( p ) && submap_marked( available, project_to<coords::sm>( p ) );
        }
        bool has_ledge_geometry( const tripoint_abs_ms &origin, const tripoint_abs_ms &target,
                                 const std::vector<tripoint_abs_omt> &footprint ) const {
            if( origin.z() == target.z() ) {
                return true;
            }
            const auto &high = origin.z() > target.z() ? origin : target;
            const auto &low_point = origin.z() > target.z() ? target : origin;
            // Same first-ledge scan and footprint as native ledge_coverage.
            // A known floor is decisive. Without one, unknown lower furniture
            // or an unbound vehicle could still supply this above-floor bit.
            for( const auto &point : line_to( tripoint_abs_ms( low_point.xy(), high.z() ), high ) ) {
                const auto omt = project_to<coords::omt>( point );
                if( std::none_of( footprint.begin(), footprint.end(), [&]( const auto &tile ) {
                return tile.xy() == omt.xy();
                } ) ) {
                    continue;
                }
                if( !has_geometry( point ) ) {
                    return false;
                }
                if( point == high || dont_draw_lower_floor( get_bub( point ) ) ) {
                    return true; // Native also falls back to high if no earlier ledge exists.
                }
                if( !has_geometry( point - tripoint::above ) ) {
                    return false;
                }
            }
            return true;
        }
        bool has_ray( const tripoint_abs_ms &origin, const tripoint_abs_ms &target ) const {
            if( !has_geometry( origin ) || !has_geometry( target ) ) {
                return false;
            }
            tripoint_abs_ms previous = origin;
            for( const tripoint_abs_ms &p : line_to( origin, target ) ) {
                if( !has_geometry( p ) ) {
                    return false;
                }
                if( p.z() != previous.z() ) {
                    const int high = std::max( p.z(), previous.z() );
                    for( const auto &xy : {
                             p.xy(), previous.xy()
                         } ) {
                        if( !has_geometry( tripoint_abs_ms( xy, high ) ) ||
                            !has_geometry( tripoint_abs_ms( xy, high - 1 ) ) ) {
                            return false;
                        }
                    }
                }
                previous = p;
            }
            return true;
        }
};

std::optional<float> existing_ambient( const tripoint_abs_ms &point,
                                       std::vector<std::unique_ptr<existing_view>> &views,
                                       vehicle_footprint_index &vehicles,
                                       const std::vector<tripoint_abs_omt> *footprint = nullptr,
                                       const std::function<bool( float )> *sufficient = nullptr,
                                       std::optional<float> *source_light = nullptr,
                                       const std::function<bool( float, float )> *predicate_bounds = nullptr )
{
    raid_perf_scope diagnostic( raid_perf_stage::scout_light_read );
    ( void )footprint; // Site/ray ownership is not an illumination boundary.
    for( const auto &view : views ) {
        if( view->light_matches( point ) ) {
            return view->native_ambient( point, sufficient, source_light, predicate_bounds );
        }
    }
    std::unique_ptr<existing_view> view;
    {
        raid_perf_scope build( raid_perf_stage::scout_view_build );
        view = std::make_unique<existing_view>( vehicles, point, point, false, true, true );
    }
    view->prepare_visibility();
    const auto light = view->native_ambient( point, sufficient, source_light, predicate_bounds );
    views.push_back( std::move( view ) );
    return light;
}
void set_view_ambient( map &view, const tripoint_abs_ms &point, const float light )
{
    const auto local = view.get_bub( point );
    auto &cache = const_cast<level_cache &>( view.get_cache_ref( point.z() ) );
    cache.lm[local.x()][local.y()] = four_quadrants( light );
}

void set_view_source( map &view, const tripoint_abs_ms &point, const float source )
{
    const auto local = view.get_bub( point );
    auto &cache = const_cast<level_cache &>( view.get_cache_ref( point.z() ) );
    cache.sm[local.x()][local.y()] = source;
}

float optical_magnification()
{
    // The ordinary light threshold is preserved; optics doubles recognition range while
    // existing weather attenuation reduces it. There is no separate darkness threshold.
    return 2.0f / std::max( 1.0f, get_weather().weather_id->sight_penalty );
}

scout_observation::visibility_read read_visibility( const Character &observer,
        const tripoint_abs_ms &origin, const tripoint_abs_ms &target,
        const Character *actor,
        std::vector<std::unique_ptr<existing_view>> *cached_views = nullptr,
        vehicle_footprint_index *cached_vehicles = nullptr,
        std::vector<tripoint_abs_omt> footprint = {}, const bool watch_edge = false )
{
    if( observer.is_dead_state() || observer.is_blind() || observer.in_sleep_state() ||
        observer.has_effect( efftype_id( "no_sight" ) ) ||
        observer.has_effect( efftype_id( "narcosis" ) ) || observer.has_effect( efftype_id( "stunned" ) ) ||
        observer.has_effect( efftype_id( "downed" ) ) ||
        observer.has_effect( efftype_id( "psi_stunned" ) ) ||
        observer.has_flag( json_character_flag( "CANNOT_MOVE" ) ) ) {
        return { true, false, "observer_incapable" };
    }
    // A newly applied eye impairment precedes the native sight cache refresh.
    // Use its current native one-tile limit without mutating the observer;
    // retain the native slime-eye exemption and optical magnification.
    if( observer.has_effect( efftype_id( "boomered" ) ) &&
        !observer.has_trait( trait_id( "PER_SLIME_OK" ) ) &&
        rl_dist( origin, target ) > static_cast<int>( std::floor( optical_magnification() ) ) ) {
        return { true, false, "current_native_sight_limit" };
    }
    if( rl_dist( origin, target ) > MAX_VIEW_DISTANCE * 2 ||
        std::abs( origin.z() - target.z() ) > fov_3d_z_range ) {
        return { true, false, "optical_range" };
    }
    const auto target_omt = project_to<coords::omt>( target );
    if( std::none_of( footprint.begin(), footprint.end(), [&]( const auto & tile ) {
    return tile.xy() == target_omt.xy();
    } ) ) {
        // A caller may supply a previously identified adjacent site leaver. Its
        // current target-local cell is checked too; this never selects identities.
        footprint.push_back( target_omt );
    }
    const auto checked = [&]( const tripoint_abs_ms & p ) {
        const auto omt = project_to<coords::omt>( p );
        return std::any_of( footprint.begin(), footprint.end(), [&]( const auto & tile ) {
            return tile.xy() == omt.xy();
        } );
    };
    const auto target_line = [watch_edge]( const tripoint_abs_ms &from, const tripoint_abs_ms &to ) {
        return watch_edge ? scout_observation::site_edge_sightline( from, to ) :
               scout_observation::site_sightline( from, to );
    };
    const auto line = target_line( origin, target );
    const auto entry = std::find_if( line.begin(), line.end(), checked );
    if( entry == line.end() ) {
        return { false, false, "target_segment_unavailable" };
    }
    vehicle_footprint_index local_vehicles;
    auto &vehicles = cached_vehicles ? *cached_vehicles : local_vehicles;
    std::vector<std::unique_ptr<existing_view>> local_views;
    auto &views = cached_views ? *cached_views : local_views;
    const map *here = &get_map();
    existing_view *view = nullptr;
    if( actor || !here->inbounds( *entry ) || !here->inbounds( target ) ) {
        // Borrow only target-local physical cells. Missing corridor/lookout cells
        // are deliberately irrelevant and never fetched to solve this sightline.
        // Actor predicates always use this isolated value view: reconstructed
        // target light must not fall back to a different main-map light window.
        for( const auto &candidate : views ) {
            if( candidate->covers( *entry, target ) && candidate->scope_matches( &footprint ) ) {
                view = candidate.get();
                break;
            }
        }
        if( !view ) {
            auto owned = std::make_unique<existing_view>( vehicles, *entry, target, true, false,
                         false, &footprint );
            owned->prepare_visibility();
            view = owned.get();
            views.push_back( std::move( owned ) );
        }
        here = view;
        tripoint_abs_ms previous = origin;
        for( const auto &point : line ) {
            if( checked( point ) ) {
                if( !view->has_geometry( point ) ) {
                    return { false, false, "geometry_unavailable" };
                }
                if( point.z() != previous.z() ) {
                    const int high = std::max( point.z(), previous.z() );
                    for( const auto &xy : {
                             point.xy(), previous.xy()
                         } ) {
                        const tripoint_abs_ms top( xy, high );
                        if( checked( top ) && ( !view->has_geometry( top ) ||
                                                !view->has_geometry( top - tripoint( 0, 0, 1 ) ) ) ) {
                            return { false, false, "geometry_unavailable" };
                        }
                    }
                }
            }
            previous = point;
        }
        if( actor && ( origin.z() != target.z() || actor->is_crouching() || actor->is_prone() ||
                       actor->has_effect( efftype_id( "all_fours" ) ) ) ) {
            const int high = std::max( origin.z(), target.z() );
            for( const auto &point : target_line( tripoint_abs_ms( origin.xy(), high ),
                    tripoint_abs_ms( target.xy(), high ) ) ) {
                if( checked( point ) && !view->has_geometry( point ) ) {
                    return { false, false, "geometry_unavailable" };
                }
            }
        }
        if( actor && !view->has_ledge_geometry( origin, target, footprint ) ) {
            return { false, false, "geometry_unavailable" };
        }
    }
    const auto from = here->get_bub( origin );
    const auto to = here->get_bub( target );
    if( !here->is_outside( to ) ) {
        return { true, false, "interior_unknown" };
    }
    const float optics = optical_magnification();
    if( !here->sees_site_with_optics( from, to, MAX_VIEW_DISTANCE * 2, optics, footprint ) ) {
        return { true, false, "target_local_occlusion_or_attenuation" };
    }
    // Packets still supply emission/weather reach, but a non-luminous plume
    // needs the same real target illumination as the watched exterior tile.
    const float observer_light = g->natural_light_level( origin.z() );
    const auto optical_range = [&]( const float light ) {
        return static_cast<int>( std::floor( optics * observer.sight_range( light, observer_light ) ) );
    };
    const int maximum = std::max( optical_range( default_daylight_level() ), optical_range( 0 ) );
    const auto range_at = [&]( const float light ) {
        return light > here->get_cache_ref( target.z() ).natural_light_level_cache ?
               maximum : std::min( maximum, optical_range( light ) );
    };
    std::optional<bool> proven;
    std::optional<float> target_source;
    std::unique_ptr<existing_view> probe;
    const std::function<bool( float )> sufficient = [&]( const float lower ) {
        if( actor ) {
            if( !target_source ) {
                return false;
            }
            if( !probe ) {
                probe = std::make_unique<existing_view>( vehicles, *entry, target, true, false,
                                                       false, &footprint );
                probe->prepare_visibility();
            }
            for( const auto &point : scout_observation::site_sightline( *entry, target ) ) {
                if( checked( point ) && !probe->has_geometry( point ) ) {
                    return false;
                }
            }
            if( !probe->has_geometry( target ) ) {
                return false;
            }
            if( !probe->has_ledge_geometry( origin, target, footprint ) ) {
                return false;
            }
            const auto visible_at = [&]( const float light ) {
                set_view_ambient( *probe, target, light );
                set_view_source( *probe, target, *target_source );
                return watch_edge ? observer.sees_site_with_optics( *probe, *actor, optics, footprint,
                                    observer_light, origin ) :
                       observer.sees_site_with_optics( *probe, *actor, optics, footprint, observer_light );
            };
            // Character sight/range and the native light-dependent concealment
            // gates are monotone. All posture/invisibility/terrain gates still
            // execute in Creature's native owner, on this isolated value view.
            if( visible_at( lower ) ) {
                proven = true;
            } else if( !visible_at( std::max( default_daylight_level(),
                                            g->natural_light_level( target.z() ) ) + LIGHT_AMBIENT_LIT ) ) {
                proven = false;
            }
        } else if( rl_dist( origin, target ) <= range_at( lower ) ) {
            proven = true;
        } else if( rl_dist( origin, target ) > maximum ) {
            proven = false;
        }
        return proven.has_value();
    };
    const auto target_light = existing_ambient( target, views, vehicles, &footprint, &sufficient,
                              actor ? &target_source : nullptr );
    if( proven ) {
        return { true, *proven, actor ? "optical_light_weather_or_actor_visibility" :
                 "target_local_visibility" };
    }
    if( !target_light || ( actor && !target_source ) ) {
        return { false, false, "lighting_geometry_unavailable" };
    }
    if( view ) {
        set_view_ambient( *view, target, *target_light );
        if( actor ) {
            set_view_source( *view, target, *target_source );
        }
    }
    // Site-only range uses the native outdoor environmental illumination at the
    // actual scout height, without inspecting ignored lookout/corridor terrain.
    if( actor ) {
        const bool visible = watch_edge ? observer.sees_site_with_optics( *here, *actor, optics,
                             footprint, observer_light, origin ) :
                             observer.sees_site_with_optics( *here, *actor, optics, footprint, observer_light );
        return { true, visible, "optical_light_weather_or_actor_visibility" };
    }
    const int range = range_at( *target_light );
    return { true, rl_dist( origin, target ) <= range, "target_local_visibility" };
}
}

namespace scout_observation
{
namespace
{
std::vector<tripoint_abs_ms> make_site_sightline( const tripoint_abs_ms &origin,
        const tripoint_abs_ms &target, const bool edge_only )
{
    const auto delta = target - origin;
    const int steps = std::max( { std::abs( delta.x() ), std::abs( delta.y() ),
                                  std::abs( delta.z() ) } );
    if( steps > MAX_VIEW_DISTANCE * 2 ) {
        return {};
    }
    if( steps == 0 ) {
        return { target };
    }
    const auto point_at = [&]( const int step ) {
        return origin + tripoint_rel_ms(
                   static_cast<int>( std::lround( static_cast<double>( delta.x() ) * step / steps ) ),
                   static_cast<int>( std::lround( static_cast<double>( delta.y() ) * step / steps ) ),
                   static_cast<int>( std::lround( static_cast<double>( delta.z() ) * step / steps ) ) );
    };
    int first = 1;
    if( edge_only ) {
        const auto target_omt = project_to<coords::omt>( target ).xy();
        int last = steps;
        // The discrete ray enters the target's horizontal column at most once.
        // Find the entry without building or walking the intervening ray.
        while( first < last ) {
            const int midpoint = first + ( last - first ) / 2;
            if( project_to<coords::omt>( point_at( midpoint ) ).xy() == target_omt ) {
                last = midpoint;
            } else {
                first = midpoint + 1;
            }
        }
        first = std::max( 1, first - 1 );
    }
    std::vector<tripoint_abs_ms> result;
    result.reserve( steps - first + 1 );
    for( int i = first; i <= steps; ++i ) {
        result.push_back( point_at( i ) );
    }
    return result;
}
} // namespace

std::vector<tripoint_abs_ms> site_sightline( const tripoint_abs_ms &origin,
        const tripoint_abs_ms &target )
{
    return make_site_sightline( origin, target, false );
}

std::vector<tripoint_abs_ms> site_edge_sightline( const tripoint_abs_ms &origin,
        const tripoint_abs_ms &target )
{
    return make_site_sightline( origin, target, true );
}
struct site_reader::implementation {
    vehicle_footprint_index vehicles;
    std::vector<std::unique_ptr<existing_view>> views;
    std::map<tripoint_abs_ms, visibility_read> lookout_footing;

    existing_view &lookout_view( const tripoint_abs_ms &origin, const tripoint_abs_ms &target ) {
        std::vector<tripoint_abs_omt> footprint{ project_to<coords::omt>( origin ) };
        for( const auto &point : site_sightline( origin, target ) ) {
            const auto omt = project_to<coords::omt>( point );
            if( std::find( footprint.begin(), footprint.end(), omt ) == footprint.end() ) {
                footprint.push_back( omt );
            }
        }
        const auto found = std::find_if( views.begin(), views.end(), [&]( const auto &view ) {
            return view->covers( origin, target ) && view->scope_matches( &footprint );
        } );
        if( found != views.end() ) {
            // Retain recently used views when the planner moves to the next candidate.
            std::rotate( found, std::next( found ), views.end() );
        } else {
            constexpr size_t max_lookout_views = 16;
            if( views.size() >= max_lookout_views ) {
                views.erase( views.begin() );
            }
            views.push_back( std::make_unique<existing_view>( vehicles, origin, target, true,
                             false, false, &footprint ) );
        }
        return *views.back();
    }
};
site_reader::site_reader() : impl( std::make_unique<implementation>() ) {}
site_reader::~site_reader()
{
    raid_perf_scope diagnostic( raid_perf_stage::scout_reader_retire );
    impl.reset();
}
visibility_read site_reader::read( const Character &observer, const tripoint_abs_ms &origin,
                                   const tripoint_abs_ms &target,
                                   const std::vector<tripoint_abs_omt> &footprint )
{
    return read_visibility( observer, origin, target, nullptr, &impl->views, &impl->vehicles,
                            footprint );
}
visibility_read site_reader::read_character( const Character &observer, const Character &target,
        const std::vector<tripoint_abs_omt> &footprint )
{
    return read_visibility( observer, observer.pos_abs(), target.pos_abs(), &target, &impl->views,
                            &impl->vehicles, footprint );
}
visibility_read site_reader::read_actor( const Creature &observer, const Creature &target,
                                       const actor_capability &capability, bool visibility_only )
{
    raid_perf_scope diagnostic( raid_perf_stage::scout_actor_read );
    const auto origin = observer.pos_abs();
    const auto endpoint = target.pos_abs();
    if( observer.is_dead_state() || target.is_dead_state() || capability.maximum_range <= 0 ||
        capability.maximum_range > MAX_VIEW_DISTANCE * 2 ||
        !std::isfinite( capability.native_range_scale ) || capability.native_range_scale <= 0 ||
        rl_dist( origin, endpoint ) > capability.maximum_range ||
        std::abs( origin.z() - endpoint.z() ) > fov_3d_z_range ) {
        return { true, false, "actor_capability_range_or_state" };
    }
    const auto ray = site_sightline( origin, endpoint );
    std::vector<tripoint_abs_omt> footprint{ project_to<coords::omt>( origin ) };
    for( const auto &point : ray ) {
        const auto omt = project_to<coords::omt>( point );
        if( std::find( footprint.begin(), footprint.end(), omt ) == footprint.end() ) {
            footprint.push_back( omt );
        }
    }
    const map *here = &get_map();
    existing_view *view = nullptr;
    // The same isolated geometry/value owner applies to loaded and remote actors.
    {
        for( const auto &candidate : impl->views ) {
            if( candidate->covers( origin, endpoint ) && candidate->scope_matches( &footprint ) ) {
                view = candidate.get();
                break;
            }
        }
        if( !view ) {
            std::unique_ptr<existing_view> owned;
            {
                raid_perf_scope build( raid_perf_stage::scout_view_build );
                owned = std::make_unique<existing_view>( impl->vehicles, origin, endpoint, true,
                        false, false, &footprint );
            }
            owned->prepare_visibility();
            view = owned.get();
            impl->views.push_back( std::move( owned ) );
        }
        if( !view->has_ray( origin, endpoint ) ||
            ( target.as_character() && !view->has_ledge_geometry( origin, endpoint, footprint ) ) ) {
            return { false, false, "actor_geometry_unavailable" };
        }
        here = view;
    }
    // A caller that needs only the visible bit can reject an authenticated
    // blocked ray without projecting target light.  Keep the full read for
    // callers that distinguish unavailable lighting from a known negative.
    // Native creature sight has its own adjacent-actor rule, so leave that
    // case to the complete predicate.
    if( visibility_only && rl_dist( origin, endpoint ) > 1 &&
        !here->sees_site_with_optics( here->get_bub( origin ), here->get_bub( endpoint ),
                                     capability.maximum_range, 2.0f, footprint ) ) {
        return { true, false, "actor_physical_ray_blocked" };
    }
    std::optional<bool> proven;
    std::optional<float> target_source;
    const auto settle_actor = [&]( const float lower, const float upper, const int upper_range ) {
        if( !target_source ) {
            return false;
        }
        // Monster native day/night interpolation and Character native log
        // sight are bounded by these endpoint values above a light floor.
        const int lower_range = std::min( capability.maximum_range, static_cast<int>( std::floor(
                                             std::min( observer.sight_range( lower ),
                                                       observer.sight_range( upper ) ) *
                                             capability.native_range_scale ) ) );
        set_view_ambient( *view, endpoint, lower );
        set_view_source( *view, endpoint, *target_source );
        raid_perf_scope predicate( raid_perf_stage::scout_actor_predicate );
        if( observer.sees_site_with_range( *view, target, lower_range, footprint ) ) {
            proven = true;
        } else {
            set_view_ambient( *view, endpoint, upper );
            if( !observer.sees_site_with_range( *view, target, upper_range, footprint ) ) {
                proven = false;
            }
        }
        return proven.has_value();
    };
    const std::function<bool( float )> sufficient = [&]( const float lower ) {
        return settle_actor( lower, default_daylight_level(), capability.maximum_range );
    };
    const std::function<bool( float, float )> bounded = [&]( const float lower, const float upper ) {
        const int upper_range = std::min( capability.maximum_range, static_cast<int>( std::floor(
                                             std::max( observer.sight_range( lower ),
                                                       observer.sight_range( upper ) ) *
                                             capability.native_range_scale ) ) );
        return settle_actor( lower, upper, upper_range );
    };
    const auto light = existing_ambient( endpoint, impl->views, impl->vehicles, &footprint,
                                         &sufficient, &target_source, visibility_only ? &bounded : nullptr );
    if( proven ) {
        return { true, *proven, "actor_light_weather_cover_visibility" };
    }
    if( !light || !target_source ) {
        return { false, false, "actor_lighting_unavailable" };
    }
    if( view ) {
        set_view_ambient( *view, endpoint, *light );
        set_view_source( *view, endpoint, *target_source );
    }
    // The physical ray's transparency cache already applies weather to its
    // outdoor cells. A second range penalty also penalizes fully indoor rays.
    const int range = std::min( capability.maximum_range, static_cast<int>( std::floor(
                                   observer.sight_range( *light ) * capability.native_range_scale ) ) );
    raid_perf_scope predicate( raid_perf_stage::scout_actor_predicate );
    return { true, observer.sees_site_with_range( *here, target, range, footprint ),
             "actor_light_weather_cover_visibility" };
}
access_read site_reader::read_access( const monster &observer, const tripoint_abs_ms &target )
{
    const auto origin = observer.pos_abs();
    if( observer.is_dead_state() || !observer.can_see() ) {
        return { { true, false, "access_observer_unavailable" }, {} };
    }
    const auto ray = read_ray( origin, target );
    if( !ray.known || !ray.visible ) {
        return { ray, {} };
    }
    std::vector<tripoint_abs_omt> footprint{ project_to<coords::omt>( origin ) };
    for( const auto &point : site_sightline( origin, target ) ) {
        const auto omt = project_to<coords::omt>( point );
        if( std::find( footprint.begin(), footprint.end(), omt ) == footprint.end() ) {
            footprint.push_back( omt );
        }
    }
    const map *here = &get_map();
    if( !here->inbounds( origin ) || !here->inbounds( target ) ) {
        const auto found = std::find_if( impl->views.begin(), impl->views.end(),
        [&]( const auto &view ) {
            return view->covers( origin, target ) && view->scope_matches( &footprint );
        } );
        if( found == impl->views.end() ) {
            return { { false, false, "access_geometry_unavailable" }, {} };
        }
        here = found->get();
    }
    const auto from = here->get_bub( origin );
    const auto p = here->get_bub( target );
    std::optional<bool> proven;
    const std::function<bool( float )> sufficient = [&]( const float lower ) {
        const int lower_range = std::min( MAX_VIEW_DISTANCE * 2,
                                         2 * std::min( observer.sight_range( lower ),
                                                       observer.sight_range( default_daylight_level() ) ) );
        const int upper_range = std::min( MAX_VIEW_DISTANCE * 2,
                                         2 * std::max( observer.sight_range( 0 ),
                                                       observer.sight_range( default_daylight_level() ) ) );
        if( lower_range > 0 && here->sees_with_optics( from, p, lower_range, 2.0f ) ) {
            proven = true;
        } else if( upper_range <= 0 || !here->sees_with_optics( from, p, upper_range, 2.0f ) ) {
            proven = false;
        }
        return proven.has_value();
    };
    const auto light = existing_ambient( target, impl->views, impl->vehicles, &footprint,
                                         &sufficient );
    if( !light && !proven ) {
        return { { false, false, "access_lighting_unavailable" }, {} };
    }
    if( proven && !*proven ) {
        return { { true, false, "access_light_or_occlusion" }, {} };
    }
    const int range = light ? std::min( MAX_VIEW_DISTANCE * 2,
                                       2 * observer.sight_range( *light ) ) : 0;
    if( !proven && ( range <= 0 || !here->sees_with_optics( from, p, range, 2.0f ) ) ) {
        return { { true, false, "access_light_or_occlusion" }, {} };
    }
    const bool fire = here->get_field_intensity( p, field_type_id( "fd_fire" ) ) > 0;
    return { { true, true, "physical_access" },
        std::to_string( here->move_cost( p ) ) + ":" +
        std::to_string( here->has_floor_or_water( p ) ) + ":" +
        std::to_string( here->has_flag( ter_furn_flag::TFLAG_GOES_UP, p ) ) + ":" +
        std::to_string( here->has_flag( ter_furn_flag::TFLAG_GOES_DOWN, p ) ) + ":" +
        std::to_string( static_cast<bool>( here->ter( p )->open ) ||
                        static_cast<bool>( here->furn( p )->open ) ) + ":" +
        std::to_string( here->get_field_intensity( p, field_type_id( "fd_fire" ) ) ) + ":" +
        std::to_string( fire && here->has_flag( ter_furn_flag::TFLAG_FIRE_CONTAINER, p ) ) };
}
visibility_read site_reader::read_lookout( const tripoint_abs_ms &origin,
        const tripoint_abs_ms &target, const int potential_range )
{
    if( potential_range <= 0 || potential_range > MAX_VIEW_DISTANCE * 2 ||
        rl_dist( origin, target ) > potential_range ||
        std::abs( origin.z() - target.z() ) > fov_3d_z_range ) {
        return { true, false, "lookout_potential_range" };
    }
    auto footing = impl->lookout_footing.find( origin );
    if( footing == impl->lookout_footing.end() ) {
        const auto read_footing = [&]() -> visibility_read {
            map &live = get_map();
            map *here = &live;
            if( !live.inbounds( origin ) ) {
                auto &view = impl->lookout_view( origin, origin );
                if( !view.has_geometry( origin ) ) {
                    return { false, false, "lookout_geometry_unavailable" };
                }
                view.rebuild_vehicle_level_caches();
                here = &view;
            }
            const auto standing = here->get_bub( origin );
            if( !here->passable( standing ) || !here->has_floor_or_water( standing ) ||
                here->has_flag( ter_furn_flag::TFLAG_DEEP_WATER, standing ) ) {
                return { true, false, "lookout_unusable_footing" };
            }
            for( const auto &entry : here->field_at( standing ) ) {
                if( entry.second.is_dangerous() ) {
                    return { true, false, "lookout_dangerous_field" };
                }
            }
            return { true, true, "lookout_usable_footing" };
        };
        footing = impl->lookout_footing.emplace( origin, read_footing() ).first;
    }
    if( !footing->second.visible ) {
        return footing->second;
    }
    const map *here = &get_map();
    if( !here->inbounds( origin ) || !here->inbounds( target ) ) {
        auto &view = impl->lookout_view( origin, target );
        if( !view.has_ray( origin, target ) ) {
            return { false, false, "physical_ray_geometry_unavailable" };
        }
        view.prepare_visibility();
        here = &view;
    }
    if( !here->sees( here->get_bub( origin ), here->get_bub( target ), MAX_VIEW_DISTANCE * 2 ) ) {
        return { true, false, "physical_ray" };
    }
    // Transient occupants and their identities do not choose a prospective
    // site. Native movement/arrival still owns occupancy and actual placement.
    return { true, true, "lookout_geometric_view" };
}
visibility_read site_reader::read_ray( const tripoint_abs_ms &origin,
                                      const tripoint_abs_ms &target )
{
    if( rl_dist( origin, target ) > MAX_VIEW_DISTANCE * 2 ||
        std::abs( origin.z() - target.z() ) > fov_3d_z_range ) {
        return { true, false, "physical_ray_range" };
    }
    std::vector<tripoint_abs_omt> footprint{ project_to<coords::omt>( origin ) };
    for( const auto &point : site_sightline( origin, target ) ) {
        const auto omt = project_to<coords::omt>( point );
        if( std::find( footprint.begin(), footprint.end(), omt ) == footprint.end() ) {
            footprint.push_back( omt );
        }
    }
    const map *here = &get_map();
    if( !here->inbounds( origin ) || !here->inbounds( target ) ) {
        existing_view *view = nullptr;
        for( const auto &candidate : impl->views ) {
            if( candidate->covers( origin, target ) && candidate->scope_matches( &footprint ) ) {
                view = candidate.get();
                break;
            }
        }
        if( !view ) {
            auto owned = std::make_unique<existing_view>( impl->vehicles, origin, target, true,
                         false, false, &footprint );
            owned->prepare_visibility();
            view = owned.get();
            impl->views.push_back( std::move( owned ) );
        }
        if( !view->has_ray( origin, target ) ) {
            return { false, false, "physical_ray_geometry_unavailable" };
        }
        here = view;
    }
    return { true, here->sees( here->get_bub( origin ), here->get_bub( target ),
                              MAX_VIEW_DISTANCE * 2 ), "physical_ray" };
}
visibility_read site_reader::read_signal( const Character &observer, const tripoint_abs_ms &origin,
        const tripoint_abs_ms &target,
        const std::vector<tripoint_abs_omt> &footprint )
{
    return read_visibility( observer, origin, target, nullptr, &impl->views, &impl->vehicles,
                            footprint );
}
visibility_read site_reader::read_watch_signal( const Character &observer,
        const tripoint_abs_omt &watch, const tripoint_abs_ms &target )
{
    const auto eye = project_to<coords::ms>( watch ) + point( SEEX, SEEY );
    return read_visibility( observer, eye, target, nullptr, &impl->views, &impl->vehicles,
                            { project_to<coords::omt>( target ) }, true );
}

visibility_read site_reader::read_watch_character( const Character &observer, const Character &target,
        const tripoint_abs_omt &watch )
{
    const auto eye = project_to<coords::ms>( watch ) + point( SEEX, SEEY );
    return read_visibility( observer, eye, target.pos_abs(), &target, &impl->views, &impl->vehicles,
                            { target.pos_abs_omt() }, true );
}

size_t site_reader::cached_view_count() const
{
    return impl->views.size();
}
// Immediate physical safety is observer-relative, independent of exact/fallback
// acquisition rules. Reuse the decision's existing geometry and lighting views.
static bool watch_position_safe_impl( const npc &observer,
                                      vehicle_footprint_index &vehicles,
                                      std::vector<std::unique_ptr<existing_view>> &lighting_views,
                                      std::string *reason,
                                      std::vector<tripoint_abs_ms> *perceived = nullptr )
{
    const auto refuse = [reason]( const char *why ) {
        if( reason ) {
            *reason = why;
        }
        return false;
    };
    // Native turns refresh these tactical plans only for active actors. Unload
    // and body catch-up retain the cache; its old target/danger/attack is not a
    // current off-bubble perception. Preserve it for Native AI, and read actual
    // fields and visible hostiles below for either owner instead of clearing it.
    if( observer.is_active() && perceived == nullptr ) {
        if( observer.get_ai_danger() > 0 ) {
            return refuse( "ai_danger" );
        }
        if( observer.get_ai_target().lock() ) {
            return refuse( "ai_target" );
        }
        if( observer.get_current_attack() ) {
            return refuse( "attack_pending" );
        }
    }
    for( const char *effect : {
             "narcosis", "stunned", "downed", "psi_stunned",
             "npc_flee_player", "npc_run_away", "npc_fire_bad", "onfire"
         } ) {
        if( perceived == nullptr && observer.has_effect( efftype_id( effect ) ) ) {
            return refuse( effect );
        }
    }
    if( perceived == nullptr && observer.faction_alarm_requires_response() ) {
        return refuse( "received_alarm" );
    }
    const map &here = get_map();
    std::vector<const Creature *> threats;
    tripoint_abs_ms minimum = observer.pos_abs();
    tripoint_abs_ms maximum = minimum;
    const auto consider = [&]( const Creature & enemy ) {
        if( &enemy == &observer || enemy.is_dead_state() ||
            rl_dist( observer.pos_abs(), enemy.pos_abs() ) > MAX_VIEW_DISTANCE ||
            std::abs( observer.pos_abs().z() - enemy.pos_abs().z() ) > fov_3d_z_range ||
            observer.attitude_to( enemy ) != Creature::Attitude::HOSTILE ) {
            return;
        }
        threats.push_back( &enemy );
        if( !here.inbounds( observer.pos_abs() ) || !here.inbounds( enemy.pos_abs() ) ) {
            minimum = tripoint_abs_ms( std::min( minimum.x(), enemy.pos_abs().x() ),
                                       std::min( minimum.y(), enemy.pos_abs().y() ),
                                       std::min( minimum.z(), enemy.pos_abs().z() ) );
            maximum = tripoint_abs_ms( std::max( maximum.x(), enemy.pos_abs().x() ),
                                       std::max( maximum.y(), enemy.pos_abs().y() ),
                                       std::max( maximum.z(), enemy.pos_abs().z() ) );
        }
    };
    consider( get_avatar() );
    for( const npc &enemy : g->all_npcs() ) {
        consider( enemy );
    }
    // An already resident hostile NPC remains an ordinary physical danger even
    // outside the active player list. This query loads or materializes nothing.
    std::set<character_id> known_npcs;
    for( const npc &enemy : g->all_npcs() ) {
        known_npcs.insert( enemy.getID() );
    }
    for( const overmap *om : overmap_buffer.get_loaded_overmaps_near(
             project_to<coords::sm>( observer.pos_abs() ).xy(), MAX_VIEW_DISTANCE / SEEX + 1 ) ) {
        for( const auto &enemy : om->get_npcs() ) {
            if( !enemy->is_fake() && known_npcs.insert( enemy->getID() ).second ) {
                consider( *enemy );
            }
        }
    }
    for( const monster &enemy : g->all_monsters() ) {
        consider( enemy );
    }
    std::unique_ptr<existing_view> view;
    if( !here.inbounds( observer.pos_abs() ) || minimum != maximum ) {
        // One observer-relative window covers every available nearby threat; a
        // concealed horde cannot multiply cache rebuilding or submap retrieval.
        view = std::make_unique<existing_view>( vehicles, minimum, maximum, false );
    }
    if( view ) {
        view->prepare_visibility();
    }
    const map &origin_map = here.inbounds( observer.pos_abs() ) ? here : *view;
    // Missing lookout terrain cannot veto target-local optics. Native bodily
    // interruptions above and available actual fields/threats still interrupt.
    const bool origin_known = here.inbounds( observer.pos_abs() ) ||
                              ( view && view->has_geometry( observer.pos_abs() ) );
    // Watch incapacity/hazard is not loss of the party's actual perception.
    // The collector still uses the unchanged Native sight/tactical predicates;
    // it neither grants watch credit nor clears the partner's incident/field.
    if( perceived == nullptr && origin_known && observer.is_dangerous_fields(
            origin_map.field_at( origin_map.get_bub( observer.pos_abs() ) ) ) ) {
        return refuse( "dangerous_field" );
    }
    bool threatened = false;
    bool hostile_optics_unknown = false;
    const auto record_threat = [&]( const Creature & enemy ) {
        threatened = true;
        if( perceived ) {
            perceived->push_back( enemy.pos_abs() );
        }
    };
    // The AI cache is transient across reload. Read available actual threats with
    // ordinary physical range for loaded and abstract observers, never magnified.
    for( const Creature *enemy : threats ) {
        if( here.inbounds( observer.pos_abs() ) && here.inbounds( enemy->pos_abs() ) ) {
            if( observer.sees_without_clairvoyance( here, *enemy ) ) {
                if( observer.scouting_creature_requires_response( *enemy, "watch", here ) ) {
                    record_threat( *enemy );
                }
            }
        } else {
            if( !view->has_ray( observer.pos_abs(), enemy->pos_abs() ) ) {
                // No sighting was established. This does not clear an existing
                // alarm or veto an independently valid target observation.
                hostile_optics_unknown = true;
                continue;
            }
            std::optional<float> observer_source;
            std::optional<float> target_source;
            const auto observer_light = existing_ambient( observer.pos_abs(), lighting_views, vehicles,
                                        nullptr, nullptr, &observer_source );
            const auto target_light = existing_ambient( enemy->pos_abs(), lighting_views, vehicles,
                                      nullptr, nullptr, &target_source );
            if( !observer_light || !target_light || !observer_source || !target_source ) {
                hostile_optics_unknown = true;
                continue;
            }
            set_view_ambient( *view, observer.pos_abs(), *observer_light );
            set_view_ambient( *view, enemy->pos_abs(), *target_light );
            set_view_source( *view, observer.pos_abs(), *observer_source );
            set_view_source( *view, enemy->pos_abs(), *target_source );
            // Non-avatar interruption uses the unchanged native creature path.
            // The avatar requires a physical range/light read because this view
            // has no avatar-centred seen cache, never enhanced combat perception.
            const bool seen = enemy->is_avatar() ?
                              observer.sees_without_clairvoyance_physical( *view, *enemy ) :
                              observer.sees_without_clairvoyance( *view, *enemy );
            if( seen && observer.scouting_creature_requires_response( *enemy, "watch", *view ) ) {
                record_threat( *enemy );
            }
        }
    }
    if( threatened ) {
        return refuse( "visible_hostile" );
    }
    if( reason ) {
        *reason = hostile_optics_unknown ? "eligible_hostile_optics_unknown" :
                  origin_known ? "eligible" : "eligible_lookout_geometry_unknown";
    }
    return true;
}

static bool watching_member_impl( const bandit_live_world::site_record &site, const npc &observer,
                                  vehicle_footprint_index &vehicles,
                                  std::vector<std::unique_ptr<existing_view>> &lighting_views,
                                  std::string *reason = nullptr )
{
    // Optional explanation of this same acquisition decision, not another
    // eligibility read or a persisted watcher mode.
    const auto refuse = [reason]( const char *why ) {
        if( reason ) {
            *reason = why;
        }
        return false;
    };
    const auto &outing = site.active_outing;
    if( site.retired_empty_site ) {
        return refuse( "site_retired" );
    }
    if( outing.local_projection_reconciliation_rejected ) {
        return refuse( "projection_rejected" );
    }
    if( outing.schema_version < 8 ) {
        return refuse( "legacy_assignment" );
    }
    if( outing.kind != bandit_live_world::outing_kind::structural_sortie ) {
        return refuse( "wrong_outing_kind" );
    }
    if( outing.phase != bandit_live_world::scout_phase::observing ) {
        return refuse( "not_observing" );
    }
    const bool exact_watch =
        outing.selected_watch_kind == bandit_live_world::structural_watch_kind::exact;
    const bool fallback_watch =
        outing.selected_watch_kind == bandit_live_world::structural_watch_kind::fallback;
    if( !exact_watch && !fallback_watch ) {
        return refuse( "not_exact_watch" );
    }
    const auto distance = bandit_live_world::target_footprint_watch_distance(
                              outing.selected_watch_omt, outing.target_footprint );
    if( !distance || ( exact_watch ? *distance != 3 : ( *distance < 4 || *distance > 5 ) ) ) {
        return refuse( "watch_distance" );
    }
    if( outing.alternate_watch_reposition_pending ) {
        return refuse( "reposition_pending" );
    }
    if( outing.member_is_resolved( observer.getID() ) ) {
        return refuse( "member_resolved" );
    }
    if( std::find( outing.member_ids.begin(), outing.member_ids.end(), observer.getID() ) ==
        outing.member_ids.end() ) {
        return refuse( "member_unassigned" );
    }
    if( observer.pos_abs_omt() != outing.selected_watch_omt ) {
        return refuse( "not_at_watch" );
    }
    if( ( outing.owner == bandit_live_world::simulation_owner::local &&
          ( !outing.local_handoff.is_active() || outing.local_handoff.phase != outing.phase ||
            !outing.local_handoff.cohesion_assembled || outing.local_handoff.cohesion_abort_return ) ) ) {
        return refuse( "local_handoff_not_ready" );
    }
    if( observer.is_dead_state() ) {
        return refuse( "dead" );
    }
    if( observer.is_blind() ) {
        return refuse( "blind" );
    }
    if( observer.in_sleep_state() ) {
        return refuse( "sleeping" );
    }
    if( observer.has_flag( json_character_flag( "CANNOT_MOVE" ) ) ) {
        return refuse( "cannot_move" );
    }
    if( !observer.path.empty() ) {
        return refuse( "local_path_pending" );
    }
    if( !observer.omt_path.empty() ) {
        return refuse( "overmap_path_pending" );
    }
    if( observer.goto_to_this_pos ) {
        return refuse( "local_destination_pending" );
    }
    if( observer.mission == NPC_MISSION_TRAVELLING ) {
        return refuse( "travelling" );
    }
    if( ( observer.goal != npc::no_goal_point && observer.goal != outing.selected_watch_omt ) ) {
        return refuse( "other_goal" );
    }
    if( observer.get_attitude() == NPCATT_FLEE || observer.get_attitude() == NPCATT_FLEE_TEMP ) {
        return refuse( "flight" );
    }
    if( observer.get_attitude() == NPCATT_KILL ) {
        return refuse( "combat_attitude" );
    }
    return watch_position_safe_impl( observer, vehicles, lighting_views, reason );
}
bool watching_member( const bandit_live_world::site_record &site, const npc &observer )
{
    vehicle_footprint_index vehicles;
    std::vector<std::unique_ptr<existing_view>> lighting_views;
    return watching_member_impl( site, observer, vehicles, lighting_views );
}
bool site_reader::watching_member( const bandit_live_world::site_record &site, const npc &observer )
{
    return watching_member( site, observer, nullptr );
}
bool site_reader::watching_member( const bandit_live_world::site_record &site, const npc &observer,
                                   std::string *reason )
{
    return watching_member_impl( site, observer, impl->vehicles, impl->views, reason );
}
std::vector<tripoint_abs_ms> site_reader::perceived_threats( const npc &observer )
{
    // A movement/watch veto does not erase actual tactical vision, but an
    // unconscious or blind partner supplies no new sighting. Its authenticated
    // incident memory remains separately available to the party route owner.
    if( observer.is_dead_state() || observer.is_blind() || observer.in_sleep_state() ||
        observer.has_effect( efftype_id( "no_sight" ) ) ||
        observer.has_effect( efftype_id( "narcosis" ) ) ) {
        return {};
    }
    std::vector<tripoint_abs_ms> positions;
    watch_position_safe_impl( observer, impl->vehicles, impl->views, nullptr, &positions );
    return positions;
}
size_t site_reader::cached_vehicle_scan_count() const
{
    return impl->vehicles.scan_count();
}
visibility_read read_character( const Character &observer, const Character &target )
{
    return read_visibility( observer, observer.pos_abs(), target.pos_abs(), &target );
}
visibility_read read_site_tile( const Character &observer, const tripoint_abs_ms &origin,
                                const tripoint_abs_ms &target )
{
    return read_visibility( observer, origin, target, nullptr );
}
bool associated_with_site( const tripoint_abs_omt &position,
                           const std::vector<tripoint_abs_omt> &footprint,
                           const std::vector<tripoint_abs_omt> &previous_sightings )
{
    const auto same_column = []( const tripoint_abs_omt & a, const tripoint_abs_omt & b ) {
        return a.xy() == b.xy();
    };
    if( std::any_of( footprint.begin(), footprint.end(), [&]( const auto & tile ) {
    return same_column( tile, position );
    } ) ) {
        return true;
    }
    // Leaving activity is associated only after this identity was actually seen at the site.
    return std::any_of( previous_sightings.begin(), previous_sightings.end(), [&]( const auto & seen ) {
        return std::any_of( footprint.begin(), footprint.end(), [&]( const auto & tile ) {
            return same_column( tile, seen ) && rl_dist( tile.xy(), position.xy() ) <= 1;
        } );
    } );
}
}

size_t scout_observation::site_reader::cached_vehicle_cell_query_count() const
{
    return impl->vehicles.query_count();
}
size_t scout_observation::site_reader::cached_vehicle_part_read_count() const
{
    return impl->vehicles.part_count();
}
size_t scout_observation::site_reader::cached_lighting_submap_count() const
{
    size_t count = 0;
    for( const auto &view : impl->views ) {
        count += view->lighting_submap_count();
    }
    return count;
}
size_t scout_observation::site_reader::cached_sunlight_summary_count() const
{
    size_t count = 0;
    for( const auto &view : impl->views ) {
        count += view->sunlight_summary_count();
    }
    return count;
}
size_t scout_observation::site_reader::cached_lighting_path_group_count() const
{
    size_t count = 0;
    for( const auto &view : impl->views ) {
        count += view->lighting_path_group_count();
    }
    return count;
}
std::optional<float> scout_observation::site_reader::cached_lighting_at(
    const tripoint_abs_ms &point ) const
{
    for( const auto &view : impl->views ) {
        if( view->light_matches( point ) ) {
            return view->cached_lighting_at( point );
        }
    }
    return std::nullopt;
}
