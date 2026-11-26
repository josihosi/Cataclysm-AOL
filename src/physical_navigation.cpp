#include "physical_navigation.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "calendar.h"
#include "creature_tracker.h"
#include "current_map.h"
#include "do_turn.h"
#include "field_type.h"
#include "horde_map.h"
#include "line.h"
#include "map.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "omdata.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "pathfinding.h"
#include "raid_sweep.h"
#include "submap.h"
#include "trap.h"

namespace physical_navigation
{
namespace
{
thread_local query_work_counts *current_query_work_counts = nullptr;
bool same_settings( const pathfinding_settings &a, const pathfinding_settings &b )
{
    return std::tie( a.bash_strength, a.max_dist, a.max_length, a.climb_cost,
                     a.allow_open_doors, a.allow_unlock_doors, a.avoid_traps, a.allow_climb_stairs,
                     a.avoid_rough_terrain, a.avoid_sharp, a.avoid_dangerous_fields, a.size ) ==
           std::tie( b.bash_strength, b.max_dist, b.max_length, b.climb_cost,
                     b.allow_open_doors, b.allow_unlock_doors, b.avoid_traps, b.allow_climb_stairs,
                     b.avoid_rough_terrain, b.avoid_sharp, b.avoid_dangerous_fields, b.size );
}

struct view_bounds {
    tripoint_abs_sm first;
    int size;
    int low;
    int high;
    view_bounds( const tripoint_abs_ms &from, const tripoint_abs_ms &to ) :
        first( project_to<coords::sm>( tripoint_abs_ms(
                   std::min( from.x(), to.x() ) - SEEX,
                   std::min( from.y(), to.y() ) - SEEY, from.z() ) ) ),
        size( [&]() {
            // Include a whole submap around both endpoints. Dividing only the
            // distance loses a boundary submap when either endpoint is not
            // aligned, hiding valid local detours behind the origin.
            const tripoint_abs_sm last = project_to<coords::sm>( tripoint_abs_ms(
                    std::max( from.x(), to.x() ) + SEEX,
                    std::max( from.y(), to.y() ) + SEEY, from.z() ) );
            return std::min( MAPSIZE, std::max( last.x() - first.x() + 1,
                                                last.y() - first.y() + 1 ) );
        }() ),
        low( std::max( -OVERMAP_DEPTH, std::min( from.z(), to.z() ) - 1 ) ),
        high( std::min( OVERMAP_HEIGHT, std::max( from.z(), to.z() ) + 1 ) ) {}
    bool operator==( const view_bounds &other ) const {
        return first == other.first && size == other.size && low == other.low && high == other.high;
    }
};

// Borrow existing geometry. Only a same-level ingress may estimate its
// offscreen approach from the OMTs on the already selected travel edge.
class navigation_view : public map
{
        struct binding {
            tripoint_abs_sm position;
            submap *resident;
            bool accepted;
            int field_count;
        };
        submap unknown;
        std::optional<submap> estimated_ground;
        std::set<tripoint_abs_sm> available;
        std::set<tripoint_abs_sm> estimated;
        std::set<tripoint_abs_sm> missing;
        std::vector<binding> bindings;
        view_bounds bounds;
        static bool accepted( submap *sm, const tripoint_abs_sm &p ) {
            return sm && sm->vehicles.empty() && MAPBUFFER.vehicle_parts_at( p ).empty();
        }
    public:
        navigation_view( const tripoint_abs_ms &from, const tripoint_abs_ms &to ) :
            navigation_view( view_bounds( from, to ) ) {}
        explicit navigation_view( const view_bounds &window, bool retain_bindings = false,
                                  const map *loaded = nullptr,
                                  const std::set<tripoint_abs_omt> &approach = {} ) :
            map( window.size, true ),
            bounds( window ) {
            unknown.set_all_ter( ter_str_id( "t_wall" ) );
            set_abs_sub( bounds.first );
            if( retain_bindings ) {
                bindings.reserve( static_cast<std::size_t>( bounds.size * bounds.size *
                                  std::max( 0, bounds.high - bounds.low + 1 ) ) );
            }
            // Archive handles end before any route. The view's submaps remain
            // borrowed only through its caller's read-only preparation span.
            std::optional<mapbuffer::existing_submap_reader> existing;
            if( !loaded ) {
                existing.emplace( MAPBUFFER );
            }
            for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
                for( int x = 0; x < getmapsize(); ++x ) {
                    for( int y = 0; y < getmapsize(); ++y ) {
                        const tripoint_abs_sm p( bounds.first.x() + x, bounds.first.y() + y, z );
                        const bool eligible = z >= bounds.low && z <= bounds.high;
                        submap *sm = eligible ? ( existing ? existing->lookup( p ) :
                                                 MAPBUFFER.lookup_submap_cached( p ) ) : nullptr;
                        const bool usable = accepted( sm, p );
                        if( retain_bindings && eligible ) {
                            bindings.push_back( {p, sm, usable, sm ? sm->field_count : 0} );
                        }
                        if( usable ) {
                            available.insert( p );
                        } else if( !sm && loaded &&
                                   !loaded->inbounds( project_to<coords::ms>( p ) ) &&
                                   approach.count( project_to<coords::omt>( p ) ) != 0 &&
                                   MAPBUFFER.vehicle_parts_at( p ).empty() ) {
                            if( !estimated_ground ) {
                                estimated_ground.emplace();
                                estimated_ground->set_all_ter( ter_str_id( "t_grass" ) );
                            }
                            sm = &*estimated_ground;
                            estimated.insert( p );
                        } else {
                            if( eligible && !sm ) {
                                missing.insert( p );
                            }
                            sm = &unknown;
                        }
                        const tripoint_rel_sm grid( x, y, z );
                        setsubmap( get_nonant( grid ), sm );
                        if( usable ) {
                            cache_submap_fields( grid );
                        }
                    }
                }
            }
        }
        bool compatible( const view_bounds &window ) const {
            if( !( bounds == window ) ) {
                return false;
            }
            // Other existing-only probes may have loaded a previously unknown
            // resident or indexed an overhanging vehicle. Pointer identity
            // alone does not establish the original geometry admission.
            for( const binding &old : bindings ) {
                submap *now = MAPBUFFER.lookup_submap_cached( old.position );
                if( now != old.resident || accepted( now, old.position ) != old.accepted ||
                    ( now && now->field_count != old.field_count ) ) {
                    return false;
                }
            }
            return true;
        }
        bool known( const tripoint_abs_ms &p ) const {
            return inbounds( p ) && available.count( project_to<coords::sm>( p ) ) != 0;
        }
        bool routable( const tripoint_abs_ms &p ) const {
            return known( p ) || ( inbounds( p ) && estimated.count( project_to<coords::sm>( p ) ) != 0 );
        }
        bool unavailable( const tripoint_abs_ms &p ) const {
            return inbounds( p ) && missing.count( project_to<coords::sm>( p ) ) != 0;
        }
};

} // namespace

scoped_query_work_counter::scoped_query_work_counter( query_work_counts &counts ) :
    previous( current_query_work_counts )
{
    current_query_work_counts = &counts;
}

scoped_query_work_counter::~scoped_query_work_counter()
{
    current_query_work_counts = previous;
}

boundary_action step_npc_boundary( npc &body, const tripoint_abs_ms &destination )
{
    const auto origin = body.pos_abs();
    if( origin.z() != destination.z() || rl_dist( origin, destination ) != 1 ||
        body.get_moves() <= 0 ) {
        return boundary_action::refused;
    }
    navigation_view view( origin, destination );
    if( !view.known( origin ) || !view.known( destination ) ) {
        return boundary_action::refused;
    }
    const auto from = view.get_bub( origin );
    const auto to = view.get_bub( destination );
    if( !view.passable( to ) || !view.valid_move( from, to ) ) {
        return boundary_action::refused;
    }
    const int before = body.get_moves();
    {
        swap_map use_view( view );
        const trap &hazard = view.tr_at( to );
        if( !body.can_move_to( to, true ) || body.sees_dangerous_field( to ) ||
            ( !hazard.is_null() && hazard.can_see( to, body ) && !hazard.is_benign() ) ||
            get_creature_tracker().creature_at( to ) != nullptr ) {
            return boundary_action::refused;
        }
        body.move_to( to, true );
    }
    if( body.get_moves() >= before ) {
        return boundary_action::refused;
    }
    return body.pos_abs() == destination ? boundary_action::moved : boundary_action::spent;
}

npc_entry_result move_npc_into_loaded_area( npc &body, map &loaded,
        const tripoint_abs_omt &next_omt, const int funded_moves,
        const std::function<bool()> &still_owned )
{
    npc_entry_result result;
    const tripoint_abs_ms origin = body.pos_abs();
    const tripoint_abs_omt origin_omt = project_to<coords::omt>( origin );
    if( !still_owned || !still_owned() || body.is_active() || body.is_dead() ||
        body.in_sleep_state() || body.duty_incapacitated() || body.is_mounted() || body.in_vehicle ||
        ( body.current_target() && body.get_ai_danger() > 0 ) ||
        body.has_effect( efftype_id( "stunned" ) ) || body.has_effect( efftype_id( "psi_stunned" ) ) ) {
        result.interrupted = true;
        return result;
    }
    if( loaded.inbounds( origin ) || funded_moves <= 0 || next_omt.z() != origin_omt.z() ||
        rl_dist( origin_omt, next_omt ) > 1 ) {
        return result;
    }
    const int last = loaded.getmapsize() * SEEX - 1;
    const tripoint_bub_ms relative = loaded.get_bub( origin );
    const tripoint_bub_ms nearest( std::clamp( relative.x(), 0, last ),
                                  std::clamp( relative.y(), 0, last ), relative.z() );
    const auto params = overmap_path_params::for_npc();
    std::set<tripoint_abs_omt> approach;
    for( const auto &omt : { origin_omt, next_omt } ) {
        const auto location = project_remain<coords::om>( omt );
        const auto &om_position = location.quotient;
        const auto &local = location.remainder_tripoint;
        overmap *container = overmap_buffer.find_loaded_overmap( om_position );
        if( !container ) {
            continue;
        }
        const oter_id &terrain = container->ter( local );
        if( terrain == oter_id() || !terrain.is_valid() ||
            params.get_cost( terrain->get_travel_cost_type() ) < 0 ||
            terrain->has_flag( oter_flags::known_up ) || terrain->has_flag( oter_flags::known_down ) ||
            terrain->get_type_id().str().compare( 0, 6, "bridge" ) == 0 ) {
            continue;
        }
        approach.insert( omt );
    }
    view_bounds window( origin, loaded.get_abs( nearest ) );
    // A bounded local window permits a real detour around a nearby obstacle.
    // The loaded boundary and resident remote obstacles use real geometry.
    // The coarse approach never reads detailed map archives.
    const int grow = std::min( 4, MAPSIZE - window.size );
    window.first = tripoint_abs_sm( window.first.x() - grow / 2,
                                    window.first.y() - grow / 2, window.first.z() );
    window.size += grow;
    navigation_view view( window, false, &loaded, approach );
    if( !view.routable( origin ) ) {
        return result;
    }
    std::set<tripoint_abs_ms> occupied;
    std::set<tripoint_abs_omt> local_omts;
    for( const tripoint_bub_ms &p : view.points_on_zlevel( origin.z() ) ) {
        if( view.routable( view.get_abs( p ) ) ) {
            local_omts.insert( project_to<coords::omt>( view.get_abs( p ) ) );
        }
    }
    std::set<point_abs_om> inspected_overmaps;
    std::set<point_abs_om> unknown_overmaps;
    for( const auto &omt : local_omts ) {
        point_abs_om om_position;
        tripoint_om_omt local_omt;
        std::tie( om_position, local_omt ) = project_remain<coords::om>( omt );
        overmap *container = overmap_buffer.find_loaded_overmap( om_position );
        if( !container ) {
            unknown_overmaps.insert( om_position );
            continue; // Its tiles cannot certify occupancy and remain unavailable.
        }
        if( inspected_overmaps.insert( om_position ).second ) {
            for( const auto &other : container->get_npcs() ) {
                if( other && other.get() != &body && !other->is_dead() ) {
                    occupied.insert( other->pos_abs() );
                }
            }
        }
        for( const auto *bucket : container->hordes_at(
                 local_omt, horde_map_flavors::active | horde_map_flavors::idle |
                 horde_map_flavors::dormant | horde_map_flavors::immobile ) ) {
            for( const auto &entity : *bucket ) {
                occupied.insert( entity.first );
            }
        }
    }
    swap_map use_view( view );
    std::vector<tripoint_bub_ms> frontier;
    for( const tripoint_bub_ms &p : view.points_on_zlevel( origin.z() ) ) {
        const tripoint_abs_ms absolute = view.get_abs( p );
        const auto tile_omt = project_to<coords::omt>( absolute );
        if( !loaded.inbounds( absolute ) || !view.known( absolute ) ||
            ( tile_omt != origin_omt && tile_omt != next_omt ) ||
            unknown_overmaps.count( project_to<coords::om>( absolute.xy() ) ) != 0 ||
            ( !view.passable( p ) &&
              !view.open_door( body, p, !view.is_outside( body.pos_bub( view ) ), true ) ) ||
            ( occupied.count( absolute ) != 0 && rl_dist( origin, absolute ) <= 1 ) ||
            get_creature_tracker().creature_at( absolute ) ) {
            continue;
        }
        bool borders_remote = false;
        for( int y = -1; y <= 1 && !borders_remote; ++y ) {
            for( int x = -1; x <= 1; ++x ) {
                if( ( x != 0 || y != 0 ) && !loaded.inbounds( absolute + point( x, y ) ) ) {
                    borders_remote = true;
                    break;
                }
            }
        }
        if( borders_remote ) {
            frontier.push_back( p );
        }
    }
    std::sort( frontier.begin(), frontier.end(), [&]( const auto &lhs, const auto &rhs ) {
        return std::make_tuple( rl_dist( origin, view.get_abs( lhs ) ), lhs ) <
               std::make_tuple( rl_dist( origin, view.get_abs( rhs ) ), rhs );
    } );
    const auto native_avoid = body.get_path_avoid();
    const auto avoid = [&]( const tripoint_bub_ms &p ) {
        if( !view.inbounds( p ) || p.z() != origin.z() || !view.routable( view.get_abs( p ) ) ) {
            return true;
        }
        const auto absolute = view.get_abs( p );
        const auto tile_omt = project_to<coords::omt>( absolute );
        const trap &hazard = view.tr_at( p );
        return ( tile_omt != origin_omt && tile_omt != next_omt ) ||
               unknown_overmaps.count( project_to<coords::om>( absolute.xy() ) ) != 0 ||
               ( occupied.count( absolute ) != 0 && rl_dist( origin, absolute ) <= 1 ) ||
               get_creature_tracker().creature_at( absolute ) != nullptr || native_avoid( p ) ||
               view.dangerous_field_at( p ) ||
               ( !hazard.is_null() && hazard.can_see( p, body ) && !hazard.is_benign() );
    };
    std::vector<tripoint_bub_ms> path;
    for( const auto &entry : frontier ) {
        auto candidate = view.route( body.pos_bub( view ), pathfinding_target::point( entry ),
                                     body.get_pathfinding_settings( true ), avoid );
        while( !candidate.empty() && candidate.front() == body.pos_bub( view ) ) {
            candidate.erase( candidate.begin() );
        }
        if( !candidate.empty() && candidate.back() == entry ) {
            path = std::move( candidate );
            break;
        }
    }
    const int old_moves = body.get_moves();
    bool lost_ownership = false;
    body.set_moves( funded_moves );
    while( body.get_moves() > 0 && !path.empty() && !loaded.inbounds( body.pos_abs() ) ) {
        if( !still_owned() || body.is_dead() || body.in_sleep_state() || body.duty_incapacitated() ||
            body.is_mounted() || body.in_vehicle || body.has_effect( efftype_id( "stunned" ) ) ||
            body.has_effect( efftype_id( "psi_stunned" ) ) ||
            ( body.current_target() && body.get_ai_danger() > 0 ) ) {
            lost_ownership = true;
            break;
        }
        const auto next = path.front();
        if( !view.inbounds( next ) || avoid( next ) ||
            occupied.count( view.get_abs( next ) ) != 0 ||
            rl_dist( body.pos_bub( view ), next ) != 1 || !body.can_move_to( next, true ) ) {
            break;
        }
        const int before = body.get_moves();
        const auto prior = body.pos_abs();
        body.move_to( next, true );
        result.acted |= body.get_moves() < before;
        if( body.pos_abs() != prior ) {
            result.moved = true;
        }
        // Native movement can change the operation on the final tile too.
        // Preserve the physical entry fact, but do not report continued
        // authority merely because the loop is about to end.
        if( !still_owned() ) {
            lost_ownership = true;
            break;
        }
        if( body.get_moves() >= before || body.is_dead() ) {
            break;
        }
        if( body.pos_bub( view ) != next ) {
            if( view.passable( next ) ) {
                continue; // A native door action opened this same next tile.
            }
            break;
        }
        path.erase( path.begin() );
    }
    body.set_moves( lost_ownership && !result.acted ? old_moves :
                    std::min( std::max( old_moves, 0 ), body.get_moves() ) );
    result.interrupted = lost_ownership;
    result.entered = loaded.inbounds( body.pos_abs() );
    return result;
}

std::optional<step_read> failure_cache::find( const monster &body, const tripoint_abs_ms &goal,
        const bool movement, const int now, const bool fresh_on_recorded_turn )
{
    const auto origin = body.pos_abs();
    // Effects can change movement, fields or other actor capabilities beyond
    // the static path settings. Refuse the shortcut while any are present.
    if( !body.get_effects().empty() ) {
        entries.clear();
        return std::nullopt;
    }
    const auto &settings = body.get_pathfinding_settings();
    if( !entries.empty() ) {
        const auto &old = entries.front();
        if( old.origin != origin || old.type != body.type->id ||
            old.hp != body.get_hp() || old.speed != body.get_speed() ||
            old.flies != body.flies() || old.submerges != body.can_submerge() ||
            old.climbs != body.can_climb() ||
            old.opens_doors != body.has_flag( mon_flag_CAN_OPEN_DOORS ) ||
            !same_settings( old.settings, settings ) ) {
            entries.clear();
        }
    }
    for( auto it = entries.begin(); it != entries.end(); ) {
        if( now < it->recorded_turn ||
            static_cast<std::int64_t>( now ) - it->recorded_turn >= 10 ) {
            it = entries.erase( it );
            continue;
        }
        if( ( !fresh_on_recorded_turn || now != it->recorded_turn ) &&
            it->goal == goal && it->movement == movement && it->type == body.type->id &&
            it->hp == body.get_hp() && it->speed == body.get_speed() &&
            it->flies == body.flies() && it->submerges == body.can_submerge() &&
            it->climbs == body.can_climb() &&
            it->opens_doors == body.has_flag( mon_flag_CAN_OPEN_DOORS ) &&
            same_settings( it->settings, settings ) ) {
            step_read result;
            result.position = origin;
            result.known = true;
            result.reason = "no_existing_safe_native_route";
            return result;
        }
        ++it;
    }
    return std::nullopt;
}

void failure_cache::remember( const monster &body, const tripoint_abs_ms &goal,
                              const bool movement, const int now, const step_read &result )
{
    if( !body.get_effects().empty() || result.next != action::none || !result.known ||
        result.missing_geometry || result.hazard_revision ||
        result.reason != "no_existing_safe_native_route" ) {
        entries.erase( std::remove_if( entries.begin(), entries.end(), [&]( const entry &old ) {
            return old.goal == goal && old.movement == movement;
        } ), entries.end() );
        return;
    }
    const auto origin = body.pos_abs();
    if( !entries.empty() && entries.front().origin != origin ) {
        entries.clear();
    }
    entries.erase( std::remove_if( entries.begin(), entries.end(), [&]( const entry &old ) {
        return old.goal == goal && old.movement == movement;
    } ), entries.end() );
    // One sweep has at most 91 initial cells; retain bounded value-only
    // refusals, evicting oldest when later observed floors add more.
    if( entries.size() >= 128 ) {
        entries.erase( entries.begin() );
    }
    entries.push_back( { origin, goal, body.type->id, body.get_pathfinding_settings(), now,
                         body.get_hp(), body.get_speed(), movement, body.flies(),
                         body.can_submerge(), body.can_climb(),
                         body.has_flag( mon_flag_CAN_OPEN_DOORS ) } );
}

struct preparation::implementation {
    tripoint_abs_ms goal;
    std::unique_ptr<navigation_view> view;
    pathfinding_region region;
};
preparation::preparation() = default;
preparation::~preparation() = default;

step_read read_step( const monster &body, const tripoint_abs_ms &goal,
                     const std::function<bool( const tripoint_abs_ms & )> &occupied, preparation *prepared,
                     segment *retained, bool allow_partial_approach, failure_cache *failures,
                     bool fresh_on_recorded_turn )
{
    if( current_query_work_counts ) {
        ++current_query_work_counts->reads;
    }
    // Includes every refusal and locally owned view destruction. A retained
    // service workspace tears down under its inclusive operation parent.
    raid_perf_scope read_scope( raid_perf_stage::physical_navigation_read );
    raid_perf_note_query( !retained ? raid_perf_query_event::no_segment : retained->size() ?
                          raid_perf_query_event::segment_present : raid_perf_query_event::segment_empty );
    const auto origin = body.pos_abs();
    if( origin.z() != goal.z() ) {
        raid_perf_note_query( raid_perf_query_event::cross_z );
    }
    step_read out;
    out.position = origin;
    const int now = to_turn<int>( calendar::turn );
    if( failures ) {
        if( const auto cached = failures->find( body, goal, allow_partial_approach, now,
                                               fresh_on_recorded_turn ) ) {
            if( current_query_work_counts ) {
                ++current_query_work_counts->cached_refusals;
            }
            raid_perf_note_query( raid_perf_query_event::no_route );
            return *cached;
        }
    }
    const auto finish = [&]() {
        if( failures ) {
            failures->remember( body, goal, allow_partial_approach, now, out );
        }
        return out;
    };
    const auto &native = body.get_pathfinding_settings();
    if( native.max_dist <= 0 ) {
        if( retained ) {
            retained->clear();
        }
        out.reason = "no_native_pathfinding";
        raid_perf_note_query( raid_perf_query_event::other_refusal );
        return finish();
    }
    const auto delta = goal - origin;
    // Match the native route's distance bound. A diagonal can exceed it even
    // when each axis fits; slow terrain also disables its straight-line shortcut.
    const int length = rl_dist( origin.xy(), goal.xy() );
    const int segment = std::max( 1, std::min( 60, native.max_dist - 2 ) );
    tripoint_abs_ms aim( goal.xy(), origin.z() );
    if( length > segment ) {
        aim = origin + tripoint_rel_ms( delta.x() * segment / length,
                                        delta.y() * segment / length, 0 );
    }
    // Include the bounded horizontal approach when looking for a real floor
    // connection. A distant upper-floor home need not have stairs beside the
    // returning actor; a safe current-floor segment can bring it closer.
    const tripoint_abs_ms extent( aim.xy(), goal.z() );
    std::unique_ptr<navigation_view> owned;
    navigation_view *remote = nullptr;
    map *here = &get_map();
    if( !here->inbounds( origin ) || !here->inbounds( extent ) ) {
        const view_bounds window( origin, extent );
        if( prepared ) {
            if( !prepared->impl ) {
                prepared->impl = std::make_unique<preparation::implementation>();
            }
            auto &slot = *prepared->impl;
            if( slot.view && ( slot.goal != goal || !slot.view->compatible( window ) ) ) {
                slot.view.reset();
                slot.region = pathfinding_region();
            }
            if( !slot.view ) {
                raid_perf_scope build_scope( raid_perf_stage::physical_navigation_view_build );
                slot.view = std::make_unique<navigation_view>( window, true );
                slot.goal = goal;
            }
            remote = slot.view.get();
        } else {
            raid_perf_scope build_scope( raid_perf_stage::physical_navigation_view_build );
            owned = std::make_unique<navigation_view>( window );
            remote = owned.get();
        }
        here = remote;
    }
    const auto known = [&]( const tripoint_bub_ms & p ) {
        return here->inbounds( p ) && ( !remote || remote->known( here->get_abs( p ) ) );
    };
    const auto from = here->get_bub( origin );
    const auto request_frontier = [&]() {
        if( !remote ) {
            return;
        }
        if( !remote->known( origin ) ) {
            if( remote->unavailable( origin ) ) {
                out.missing_geometry = origin;
            }
            // Existing unsupported geometry (notably vehicles) is not an
            // invitation to load a different neighboring column.
            return;
        }
        // Preserve the existing native approach ordering. This only requests
        // physical facts; the retry must still prove every actual step.
        for( const auto &p : squares_closer_to( origin, goal ) ) {
            if( p.z() == origin.z() && rl_dist( origin, p ) <= 1 && remote->unavailable( p ) ) {
                out.missing_geometry = p;
                return;
            }
        }
    };
    if( !known( from ) ) {
        if( retained ) {
            retained->clear();
        }
        out.reason = "origin_geometry_unknown";
        request_frontier();
        raid_perf_note_query( raid_perf_query_event::unknown_geometry );
        return finish();
    }
    if( goal == origin && raid_sweep::fire_approach( *here, from ) ) {
        if( retained ) {
            retained->clear();
        }
        out.known = true;
        out.reason = "approach_fire_hazard";
        out.hazard_revision = raid_sweep::fire_revision( *here, {from} );
        raid_perf_note_query( raid_perf_query_event::fire_refused );
        return finish();
    }
    const bool can_open = body.has_flag( mon_flag_CAN_OPEN_DOORS );
    const auto door = [&]( const tripoint_bub_ms & p ) {
        return can_open && here->open_door( body, p, !here->is_outside( from ), true );
    };
    const auto avoid = [&]( const tripoint_bub_ms & p ) {
        return !known( p ) || raid_sweep::fire_approach( *here, p ) ||
               ( occupied && occupied( here->get_abs( p ) ) ) ||
               ( !body.will_move_to( here, p ) && !door( p ) ) ||
               ( !body.flies() && !here->has_floor_or_water( p ) ) || !body.know_danger_at( here, p );
    };
    const auto geometry_only = [&]( const tripoint_bub_ms &p ) {
        return !known( p ) || ( occupied && occupied( here->get_abs( p ) ) ) ||
               ( !body.will_move_to( here, p ) && !door( p ) ) ||
               ( !body.flies() && !here->has_floor_or_water( p ) );
    };
    pathfinding_settings settings = native;
    settings.allow_climb_stairs = false;
    settings.allow_open_doors = can_open;
    // The callback above enforces this creature's actual field safety.
    // The pathfinder's generic danger penalty ignores immunity and can
    // exhaust the native route budget on harmless home-world atmosphere.
    settings.avoid_dangerous_fields = false;
    settings.bash_strength.clear(); // An unseen wall is not an invented doorway.
    const auto native_route = [&]( const tripoint_bub_ms &to, const auto &exclude ) {
        if( current_query_work_counts ) {
            ++current_query_work_counts->native_routes;
        }
        raid_perf_scope route_scope( raid_perf_stage::physical_navigation_native_route );
        return here->route( from, pathfinding_target::point( to ), settings, exclude );
    };
    std::vector<tripoint_bub_ms> route;
    bool reused = false;
    if( retained && retained->size() ) {
        if( retained->goal != goal || retained->type != body.type->id ||
            origin.z() != goal.z() || !same_settings( retained->settings, settings ) ) {
            raid_perf_note_query( retained->goal != goal ? raid_perf_query_event::goal_changed :
                                  raid_perf_query_event::capability_or_level_changed );
            retained->clear();
        } else {
            // A speculative read/failed commit never consumes a point. Only
            // the actual body's next physical position establishes progress.
            if( origin != retained->origin && origin == retained->points.front() ) {
                retained->points.erase( retained->points.begin() );
                retained->costs.erase( retained->costs.begin() );
                retained->origin = origin;
            }
            if( origin == retained->origin && retained->size() ) {
                std::vector<tripoint_bub_ms> candidate;
                candidate.reserve( retained->size() );
                for( const auto &point : retained->points ) {
                    candidate.push_back( here->get_bub( point ) );
                }
                const auto costs = here->validate_route( from, candidate, settings, avoid );
                if( costs && *costs == retained->costs ) {
                    route = std::move( candidate );
                    reused = true;
                    raid_perf_note_query( raid_perf_query_event::segment_reused );
                } else {
                    raid_perf_note_query( costs ? raid_perf_query_event::cost_changed :
                                          raid_perf_query_event::validation_failed );
                }
            } else {
                raid_perf_note_query( origin == retained->origin ? raid_perf_query_event::segment_exhausted :
                                      raid_perf_query_event::origin_changed );
            }
            if( !reused ) {
                retained->clear();
            }
        }
    }
    if( route.empty() && origin.z() != goal.z() ) {
        const int dz = goal.z() > origin.z() ? 1 : -1;
        for( const auto &stair : here->points_in_rectangle(
                 tripoint_bub_ms( 0, 0, origin.z() ),
                 tripoint_bub_ms( here->getmapsize() * SEEX - 1,
                                  here->getmapsize() * SEEY - 1, origin.z() ) ) ) {
            const auto landing = stair + tripoint_rel_ms( 0, 0, dz );
            if( !known( stair ) || !known( landing ) ||
                !here->on_matching_stairs( stair, landing ) || avoid( stair ) || avoid( landing ) ||
                !here->valid_move( stair, landing ) ) {
                continue;
            }
            if( stair == from ) {
                route = {landing};
                break;
            }
            auto candidate = native_route( stair, avoid );
            if( !candidate.empty() && ( route.empty() || candidate.size() < route.size() ) ) {
                route = std::move( candidate );
            }
        }
    }
    if( route.empty() ) {
        if( aim == origin ) {
            if( retained ) {
                retained->clear();
            }
            out.known = true;
            out.reason = origin == goal ? "goal_reached" : "no_existing_safe_native_route";
            raid_perf_note_query( origin == goal ? raid_perf_query_event::goal_reached :
                                  raid_perf_query_event::no_route );
            return finish();
        }
        // A shortened endpoint is an approach preference, not a required
        // physical destination. Choose the farthest native-reachable point
        // on this bounded segment. In particular, a projection inside a
        // building wall must not prevent approaching its real entrance.
        // An unshortened goal is tried exactly first; neither an unknown tile
        // nor a nearby substitute can count as arrival.
        const int reach = std::min( length, segment );
        // A movement proposal may approach an unknown goal through a real
        // known prefix. The original goal remains authoritative for arrival;
        // ordinary read-only callers keep their exact-endpoint semantics.
        const int last = length > segment || ( allow_partial_approach &&
                         !known( here->get_bub( goal ) ) ) ? 1 : reach;
        for( int distance = reach; distance >= last; --distance ) {
            const auto approach = origin + tripoint_rel_ms( delta.x() * distance / length,
                                  delta.y() * distance / length, 0 );
            const auto to = here->get_bub( approach );
            if( !known( to ) ) {
                continue;
            }
            out.known = true;
            if( avoid( to ) ) {
                continue;
            }
            route = native_route( to, avoid );
            if( !route.empty() ) {
                break;
            }
        }
        if( route.empty() && allow_partial_approach && length <= segment &&
            origin.z() == goal.z() && known( here->get_bub( goal ) ) ) {
            // The exact square may be blocked by a building or outside the
            // native route budget even while a real approach is available.
            // Movement owners may advance toward it, but retain the original
            // goal for arrival and revalidate the positive segment. Bound
            // retries rather than searching every nearer square each turn.
            std::optional<tripoint_abs_ms> previous_approach;
            for( int distance : { reach / 2, reach / 4 } ) {
                if( distance <= 0 ) {
                    continue;
                }
                const auto approach = origin + tripoint_rel_ms( delta.x() * distance / length,
                                      delta.y() * distance / length, 0 );
                if( approach == origin || previous_approach == approach ) {
                    continue;
                }
                previous_approach = approach;
                const auto to = here->get_bub( approach );
                if( known( to ) && !avoid( to ) ) {
                    route = native_route( to, avoid );
                    if( !route.empty() ) {
                        break;
                    }
                }
            }
        }
        if( !out.known ) {
            out.reason = "segment_geometry_unknown";
            request_frontier();
            raid_perf_note_query( raid_perf_query_event::unknown_geometry );
            return finish();
        }
    }
    if( route.empty() ) {
        out.reason = "no_existing_safe_native_route";
        request_frontier();
        if( origin.z() == aim.z() && known( here->get_bub( aim ) ) ) {
            // Diagnose the same short approach; this second route is never
            // executed. A fire-excluded route is a hazard, not a searched
            // candidate or an invented impassable wall. Other geometry and
            // native door/floor constraints remain in force.
            pathfinding_region local_region;
            // Only a retained remote view has the proven read-only service
            // lifetime. Loaded/action reads always get a fresh certificate.
            pathfinding_region &region = remote && prepared ? prepared->impl->region : local_region;
            const auto approach = [&]() {
                if( current_query_work_counts ) {
                    ++current_query_work_counts->native_routes;
                }
                raid_perf_scope route_scope( raid_perf_stage::physical_navigation_native_route );
                return here->route( from, pathfinding_target::point( here->get_bub( aim ) ),
                                    settings, geometry_only, &region );
            }();
            const bool fire_blocked = std::any_of( approach.begin(), approach.end(),
            [&]( const auto & cell ) {
                return raid_sweep::fire_approach( *here, cell );
            } );
            if( fire_blocked ) {
                out.reason = "approach_fire_hazard";
                out.hazard_revision = raid_sweep::fire_revision( *here, approach );
                out.known = true;
            }
        }
        raid_perf_note_query( out.reason == "approach_fire_hazard" ? raid_perf_query_event::fire_refused :
                              raid_perf_query_event::no_route );
        return finish();
    }
    const auto next = route.front();
    if( !known( next ) || avoid( next ) ) {
        out.reason = "next_step_refused";
        raid_perf_note_query( raid_perf_query_event::other_refusal );
        return finish();
    }
    out.position = here->get_abs( next );
    out.known = true;
    if( !body.will_move_to( here, next ) && door( next ) ) {
        out.next = action::open_door;
        out.move_cost = 100;
    } else if( here->valid_move( from, next, false, body.flies() ) ) {
        out.next = action::walk;
        out.move_cost = body.calc_movecost( *here, from, next );
    }
    if( out.move_cost <= 0 ) {
        out.next = action::none;
        out.reason = "native_step_not_executable";
    }
    if( retained ) {
        if( out.next == action::none ) {
            retained->clear();
        } else if( !reused && origin.z() == goal.z() ) {
            // Retain only a bounded positive prefix. Its endpoint never owns
            // full-goal arrival, and vertical/ledge special cases remain cold.
            route.resize( std::min<std::size_t>( route.size(), 60 ) );
            const auto costs = here->validate_route( from, route, settings, avoid );
            if( costs ) {
                retained->origin = origin;
                retained->goal = goal;
                retained->type = body.type->id;
                retained->settings = settings;
                retained->costs = *costs;
                retained->points.clear();
                retained->points.reserve( route.size() );
                for( const auto &point : route ) {
                    retained->points.push_back( here->get_abs( point ) );
                }
                raid_perf_note_query( raid_perf_query_event::segment_stored );
            } else {
                raid_perf_note_query( raid_perf_query_event::store_rejected );
            }
        }
    }
    raid_perf_note_query( out.next == action::walk ? raid_perf_query_event::walk :
                          out.next == action::open_door ? raid_perf_query_event::open_door :
                          raid_perf_query_event::other_refusal );
    return finish();
}

bool acquire_movement_geometry( const tripoint_abs_ms &origin, const step_read &request )
{
    if( request.next != action::none || request.position != origin ||
        !request.missing_geometry ) {
        return false;
    }
    const auto p = *request.missing_geometry;
    if( p.z() != origin.z() || rl_dist( origin, p ) > 1 || get_map().inbounds( p ) ||
        MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( p ) ) ) {
        return false;
    }
    // The native loader preserves resident/saved layers, generates the normal
    // vertical column and runs its normal extras/spawn/actualization policy.
    // Its map borrows MAPBUFFER owners; never install a second submap copy.
    tinymap terrain;
    terrain.load( project_to<coords::omt>( p ), true, false );
    return MAPBUFFER.lookup_submap_cached( project_to<coords::sm>( p ) ) != nullptr;
}

bool commit_door( const monster &body, const tripoint_abs_ms &door )
{
    const auto origin = body.pos_abs();
    if( origin.z() != door.z() || rl_dist( origin, door ) != 1 ||
        !body.has_flag( mon_flag_CAN_OPEN_DOORS ) ) {
        return false;
    }
    map &live = get_map();
    if( live.inbounds( origin ) && live.inbounds( door ) ) {
        return live.open_door( body, live.get_bub( door ), !live.is_outside( body.pos_bub( live ) ) );
    }
    navigation_view view( origin, door );
    const auto from = view.get_bub( origin );
    const auto to = view.get_bub( door );
    if( !view.known( origin ) || !view.known( door ) ||
        !view.open_door( body, to, !view.is_outside( from ), true ) ) {
        return false;
    }
    // Native terrain/furniture open transitions, without emitting a sound in
    // the unrelated main bubble's relative coordinates. Remote vehicles are
    // unknown above and cannot enter this branch.
    if( const auto &open = view.ter( to )->open ) {
        view.ter_set( to, open );
        return true;
    }
    if( const auto &open = view.furn( to )->open ) {
        view.furn_set( to, open );
        return true;
    }
    return false;
}
} // namespace physical_navigation
