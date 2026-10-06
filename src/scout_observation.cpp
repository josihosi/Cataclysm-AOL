#include "scout_observation.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <memory>
#include <map>

#include "character.h"
#include "avatar.h"
#include "bandit_live_world.h"
#include "effect.h"
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
        size_t query_count() const { return queries; }
        size_t part_count() const { return part_reads; }
        size_t scan_count() const { return 0; }
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
        bool lighting_complete = false;
        bool remote_vehicle_parts = false;
        tripoint_abs_sm lighting_center;
        std::set<tripoint_abs_sm> available;
        std::vector<tripoint_abs_omt> lighting_footprint;
    public:
        existing_view( vehicle_footprint_index &vehicles, const tripoint_abs_ms &origin,
                       const tripoint_abs_ms &target,
                       const bool padding = true, const bool full_window = false,
                       const bool native_lighting = false,
                       const std::vector<tripoint_abs_omt> *footprint = nullptr ) :
            map( full_window ? MAPSIZE : std::min( MAPSIZE, std::max( std::abs( origin.x() - target.x() ),
                                                   std::abs( origin.y() - target.y() ) ) / SEEX + ( padding ? 3 : 2 ) ), true ),
            size( full_window ? MAPSIZE : std::min( MAPSIZE, std::max( std::abs( origin.x() - target.x() ),
                                                    std::abs( origin.y() - target.y() ) ) / SEEX + ( padding ? 3 : 2 ) ) ) {
            unknown.set_all_ter( ter_str_id( "t_wall" ) );
            ignored.set_all_ter( ter_str_id( "t_open_air" ) );
            lighting_view = native_lighting;
            lighting_center = project_to<coords::sm>( origin );
            if( footprint ) { lighting_footprint = *footprint; }
            const tripoint_abs_sm first = native_lighting ?
                                          lighting_center - point( MAPSIZE / 2, MAPSIZE / 2 ) :
                                          project_to<coords::sm>( tripoint_abs_ms(
                                                  std::min( origin.x(), target.x() ) - ( padding ? SEEX : 0 ),
                                                  std::min( origin.y(), target.y() ) - ( padding ? SEEY : 0 ), origin.z() ) );
            set_abs_sub( first );
            low = std::max( -OVERMAP_DEPTH, std::min( origin.z(), target.z() ) - 1 );
            observed_high = std::min( OVERMAP_HEIGHT, std::max( origin.z(), target.z() ) );
            high = native_lighting ? OVERMAP_HEIGHT : observed_high;
            for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
                for( int x = 0; x < size; ++x ) {
                    for( int y = 0; y < size; ++y ) {
                        const tripoint_abs_sm pos( first.x() + x, first.y() + y, z );
                        const auto omt = project_to<coords::omt>( pos );
                        const bool in_site = !footprint || std::any_of( footprint->begin(), footprint->end(),
                        [&]( const auto &tile ) { return tile.xy() == omt.xy(); } );
                        submap *sm = in_site && z >= low && z <= high ?
                                     MAPBUFFER.lookup_submap_existing( pos ) : nullptr;
                        // Geometry admission indexes physical vehicle parts independently of
                        // this view, including overhangs from excluded resident owners.
                        if( sm ) {
                            update_vehicle_list( sm, z );
                        }
                        if( sm && ( sm->vehicles.empty() || get_map().inbounds( project_to<coords::ms>( pos ) ) ) ) {
                            available.insert( pos );
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
                        const auto omt = project_to<coords::omt>( tripoint_abs_sm( first.x() + x, first.y() + y, z ) );
                        if( footprint && std::none_of( footprint->begin(), footprint->end(),
                            [&]( const auto &tile ) { return tile.xy() == omt.xy(); } ) ) { continue; }
                        for( const auto &part : vehicles.at( tripoint_abs_sm( first.x() + x,
                                first.y() + y, z ) ) ) {
                            auto &cache = const_cast<level_cache &>( get_cache_ref( z ) );
                            const auto part_omt = project_to<coords::omt>( part.second );
                            if( footprint && std::none_of( footprint->begin(), footprint->end(),
                                [&]( const auto &tile ) { return tile.xy() == part_omt.xy(); } ) ) { continue; }
                            cache.vehicle_list.insert( part.first );
                            remote_vehicle_parts |= !get_map().inbounds( part.second );
                        }
                    }
                }
            }
        }
        void prepare_visibility() {
            rebuild_vehicle_level_caches();
            if( lighting_view ) {
                lighting_complete = true;
                for( int z = lighting_center.z(); z <= high; ++z ) {
                    for( int x = 0; x < size; ++x ) {
                        for( int y = 0; y < size; ++y ) {
                            const auto absolute = get_abs_sub() + tripoint( x, y, z - get_abs_sub().z() );
                            const auto omt = project_to<coords::omt>( absolute );
                            if( !lighting_footprint.empty() && std::none_of( lighting_footprint.begin(),
                                lighting_footprint.end(), [&]( const auto &tile ) { return tile.xy() == omt.xy(); } ) ) {
                                continue;
                            }
                            if( !available.count( absolute ) ) {
                                lighting_complete = false;
                            }
                        }
                    }
                }
                lighting_complete = lighting_complete && !remote_vehicle_parts;
                if( !lighting_complete ) {
                    return;
                }
            }
            for( int z = low; z <= high; ++z ) {
                build_outside_cache( z );
                build_transparency_cache( z );
                set_floor_cache_dirty( z );
                build_floor_cache( z );
                // Loaded vehicle visibility is copied below in absolute coordinates.
                // The generic vehicle visibility helper assumes the main map's origin.
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
            if( lighting_view ) {
                // Use the ordinary native sunlight and item/field/terrain/vehicle
                // light calculation on a full observer-centred native window.
                // Observation never ticks items, sets character effects or uses
                // main-map-relative character/monster light coordinates.
                generate_lightmap( lighting_center.z(), true );
            }
        }
        bool scope_matches( const std::vector<tripoint_abs_omt> *footprint ) const {
            return footprint ? lighting_footprint == *footprint : lighting_footprint.empty();
        }
        bool light_matches( const tripoint_abs_ms &point ) const {
            return lighting_view && project_to<coords::sm>( point ) == lighting_center;
        }
        std::optional<float> native_ambient( const tripoint_abs_ms &point ) const {
            return lighting_view && lighting_complete ?
                   std::optional<float>( ambient_light_at( get_bub( point ) ) ) : std::nullopt;
        }
        bool covers( const tripoint_abs_ms &origin, const tripoint_abs_ms &target ) const {
            return !lighting_view && inbounds( origin ) && inbounds( target ) &&
                   origin.z() >= low && origin.z() <= high && target.z() >= low && target.z() <= high;
        }
        bool has_geometry( const tripoint_abs_ms &p ) const {
            // Parts may extend beyond their loaded origin submap. The main map only
            // supplies visibility caches for loaded cells; remote vehicle cells stay unknown.
            return inbounds( p ) && available.count( project_to<coords::sm>( p ) ) &&
                   ( get_map().inbounds( p ) || !veh_at( get_bub( p ) ) );
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
                                       const std::vector<tripoint_abs_omt> *footprint = nullptr )
{
    if( get_map().inbounds( point ) ) {
        return get_map().ambient_light_at( get_map().get_bub( point ) );
    }
    for( const auto &view : views ) {
        if( view->light_matches( point ) && view->scope_matches( footprint ) ) {
            return view->native_ambient( point );
        }
    }
    auto view = std::make_unique<existing_view>( vehicles, point, point, false, true, true, footprint );
    view->prepare_visibility();
    const auto light = view->native_ambient( point );
    views.push_back( std::move( view ) );
    return light;
}
void set_view_ambient( map &view, const tripoint_abs_ms &point, const float light )
{
    const auto local = view.get_bub( point );
    auto &cache = const_cast<level_cache &>( view.get_cache_ref( point.z() ) );
    cache.lm[local.x()][local.y()] = four_quadrants( light );
}

float optical_magnification()
{
    // The ordinary light threshold is preserved; optics doubles recognition range while
    // existing weather attenuation reduces it. There is no separate darkness threshold.
    return 2.0f / std::max( 1.0f, get_weather().weather_id->sight_penalty );
}

scout_observation::visibility_read read( const Character &observer,
        const tripoint_abs_ms &origin, const tripoint_abs_ms &target,
        const Character *actor,
        std::vector<std::unique_ptr<existing_view>> *cached_views = nullptr,
        vehicle_footprint_index *cached_vehicles = nullptr,
        std::vector<tripoint_abs_omt> footprint = {} )
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
    if( std::none_of( footprint.begin(), footprint.end(), [&]( const auto &tile ) {
        return tile.xy() == target_omt.xy();
    } ) ) {
        // A caller may supply a previously identified adjacent site leaver. Its
        // current target-local cell is checked too; this never selects identities.
        footprint.push_back( target_omt );
    }
    const auto checked = [&]( const tripoint_abs_ms &p ) {
        const auto omt = project_to<coords::omt>( p );
        return std::any_of( footprint.begin(), footprint.end(), [&]( const auto &tile ) {
            return tile.xy() == omt.xy();
        } );
    };
    const auto line = scout_observation::site_sightline( origin, target );
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
    if( !here->inbounds( *entry ) || !here->inbounds( target ) ) {
        // Borrow only target-local physical cells. Missing corridor/lookout cells
        // are deliberately irrelevant and never fetched to solve this sightline.
        for( const auto &candidate : views ) {
            if( candidate->covers( *entry, target ) && candidate->scope_matches( &footprint ) ) { view = candidate.get(); break; }
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
                    for( const auto &xy : {point.xy(), previous.xy()} ) {
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
            for( const auto &point : scout_observation::site_sightline( tripoint_abs_ms( origin.xy(), high ),
                                           tripoint_abs_ms( target.xy(), high ) ) ) {
                if( checked( point ) && !view->has_geometry( point ) ) {
                    return { false, false, "geometry_unavailable" };
                }
            }
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
    const auto target_light = existing_ambient( target, views, vehicles, &footprint );
    if( !target_light ) {
        return { false, false, "lighting_geometry_unavailable" };
    }
    if( view ) { set_view_ambient( *view, target, *target_light ); }
    // Site-only range uses the native outdoor environmental illumination at the
    // actual scout height, without inspecting ignored lookout/corridor terrain.
    const float observer_light = g->natural_light_level( origin.z() );
    if( actor ) {
        return { true, observer.sees_site_with_optics( *here, *actor, optics, footprint,
                 observer_light ), "optical_light_weather_or_actor_visibility" };
    }
    const auto optical_range = [&]( const float light ) {
        return static_cast<int>( std::floor( optics * observer.sight_range( light, observer_light ) ) );
    };
    const int maximum = std::max( optical_range( default_daylight_level() ), optical_range( 0 ) );
    const bool lit_target = *target_light > here->get_cache_ref( target.z() ).natural_light_level_cache;
    const int range = lit_target ? maximum : std::min( maximum, optical_range( *target_light ) );
    return { true, rl_dist( origin, target ) <= range, "target_local_visibility" };
}
}

namespace scout_observation
{
std::vector<tripoint_abs_ms> site_sightline( const tripoint_abs_ms &origin,
                                          const tripoint_abs_ms &target )
{
    const auto delta = target - origin;
    const int steps = std::max( { std::abs( delta.x() ), std::abs( delta.y() ),
                                 std::abs( delta.z() ) } );
    if( steps > MAX_VIEW_DISTANCE * 2 ) { return {}; }
    if( steps == 0 ) { return { target }; }
    std::vector<tripoint_abs_ms> result;
    result.reserve( steps );
    for( int i = 1; i <= steps; ++i ) {
        result.push_back( origin + tripoint_rel_ms(
            static_cast<int>( std::lround( static_cast<double>( delta.x() ) * i / steps ) ),
            static_cast<int>( std::lround( static_cast<double>( delta.y() ) * i / steps ) ),
            static_cast<int>( std::lround( static_cast<double>( delta.z() ) * i / steps ) ) ) );
    }
    return result;
}
struct site_reader::implementation {
    vehicle_footprint_index vehicles;
    std::vector<std::unique_ptr<existing_view>> views;
};
site_reader::site_reader() : impl( std::make_unique<implementation>() ) {}
site_reader::~site_reader() = default;
visibility_read site_reader::read( const Character &observer, const tripoint_abs_ms &origin,
                                   const tripoint_abs_ms &target,
                                   const std::vector<tripoint_abs_omt> &footprint )
{
    return ::read( observer, origin, target, nullptr, &impl->views, &impl->vehicles, footprint );
}
visibility_read site_reader::read_character( const Character &observer, const Character &target,
                                             const std::vector<tripoint_abs_omt> &footprint )
{
    return ::read( observer, observer.pos_abs(), target.pos_abs(), &target, &impl->views, &impl->vehicles,
                   footprint );
}
visibility_read site_reader::read_signal( const Character &observer, const tripoint_abs_ms &origin,
                                          const tripoint_abs_ms &target,
                                          const std::vector<tripoint_abs_omt> &footprint )
{
    return ::read( observer, origin, target, nullptr, &impl->views, &impl->vehicles, footprint );
}
size_t site_reader::cached_view_count() const
{
    return impl->views.size();
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
    if( outing.selected_watch_kind != bandit_live_world::structural_watch_kind::exact ) {
        return refuse( "not_exact_watch" );
    }
    if( bandit_live_world::target_footprint_watch_distance( outing.selected_watch_omt,
            outing.target_footprint ) != 3 ) {
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
    if( observer.get_ai_danger() > 0 ) {
        return refuse( "ai_danger" );
    }
    if( observer.get_ai_target().lock() ) {
        return refuse( "ai_target" );
    }
    if( observer.get_current_attack() ) {
        return refuse( "attack_pending" );
    }
    for( const char *effect : {
             "narcosis", "stunned", "downed", "psi_stunned",
             "npc_flee_player", "npc_run_away", "npc_fire_bad", "onfire"
         } ) {
        if( observer.has_effect( efftype_id( effect ) ) ) {
            return refuse( effect );
        }
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
    if( origin_known && observer.is_dangerous_fields(
            origin_map.field_at( origin_map.get_bub( observer.pos_abs() ) ) ) ) {
        return refuse( "dangerous_field" );
    }
    // The AI cache is transient across reload. Read available actual threats with
    // ordinary physical range for loaded and abstract observers, never magnified.
    for( const Creature *enemy : threats ) {
        if( here.inbounds( observer.pos_abs() ) && here.inbounds( enemy->pos_abs() ) ) {
            if( observer.sees_without_clairvoyance( here, *enemy ) ) {
                return refuse( "visible_hostile" );
            }
        } else if( view->has_ray( observer.pos_abs(), enemy->pos_abs() ) ) {
            const auto observer_light = existing_ambient( observer.pos_abs(), lighting_views, vehicles );
            const auto target_light = existing_ambient( enemy->pos_abs(), lighting_views, vehicles );
            if( !observer_light || !target_light ) {
                continue;
            }
            set_view_ambient( *view, observer.pos_abs(), *observer_light );
            set_view_ambient( *view, enemy->pos_abs(), *target_light );
            // Non-avatar interruption uses the unchanged native creature path.
            // The avatar requires a physical range/light read because this view
            // has no avatar-centred seen cache, never enhanced combat perception.
            const bool seen = enemy->is_avatar() ?
                              observer.sees_without_clairvoyance_physical( *view, *enemy ) :
                              observer.sees_without_clairvoyance( *view, *enemy );
            if( seen ) {
                return refuse( "visible_hostile" );
            }
        }
    }
    if( reason ) {
        *reason = "eligible";
    }
    return true;
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
size_t site_reader::cached_vehicle_scan_count() const
{
    return impl->vehicles.scan_count();
}
visibility_read read_character( const Character &observer, const Character &target )
{
    return read( observer, observer.pos_abs(), target.pos_abs(), &target );
}
visibility_read read_site_tile( const Character &observer, const tripoint_abs_ms &origin,
                                const tripoint_abs_ms &target )
{
    return read( observer, origin, target, nullptr );
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
