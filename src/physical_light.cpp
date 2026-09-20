#include "physical_light.h"

#include "character.h"
#include "field.h"
#include "vehicle.h"
#include "item.h"
#include "item_pocket.h"
#include "itype.h"
#include "map.h"
#include "mapdata.h"
#include "game.h"
#include "monster.h"
#include "mtype.h"
#include "veh_type.h"

#include <algorithm>
#include <functional>
#include <map>

namespace physical_light
{
namespace
{
static const ter_str_id ter_t_flat_roof( "t_flat_roof" );
static const ter_str_id ter_t_tile_flat_roof( "t_tile_flat_roof" );

bool gun_root_forwards_to_light_mod( const item &it )
{
    if( !it.is_gun() ) {
        return false;
    }
    // gunmods() returns a vector by value.  Retain that vector for the whole
    // traversal; begin() and end() from separate temporaries leave the first
    // iterator dangling and can crash the advancing-turn source pass.
    const std::vector<const item *> mods = it.gunmods();
    return std::any_of( mods.begin(), mods.end(), []( const item *mod ) {
        return mod != nullptr && mod->type->light_emission > 0;
    } );
}

void append_item( std::vector<emitter> &out, const item &it, const Character *carrier,
                  const tripoint_abs_ms &pos, const source_kind kind )
{
    float luminance = 0.0f;
    units::angle width = 0_degrees;
    units::angle direction = 0_degrees;
    it.getlight( luminance, width, direction );
    // The native query is authoritative for whether a source is powered;
    // getlight() cannot account for a carrier's UPS/bionic power.
    luminance = static_cast<float>( it.getlight_emit( carrier ) );
    if( luminance <= 0 ) {
        return;
    }
    emitter result;
    result.position = pos;
    result.kind = kind;
    result.luminance = std::max( 0, static_cast<int>( luminance ) );
    result.directional = width > 0_degrees;
    result.provenance = it.typeId().str();
    if( result.luminance > 0 ) {
        out.push_back( std::move( result ) );
    }
}

void append_container_tree( std::vector<emitter> &out, const item &root,
                            const Character *carrier, const tripoint_abs_ms &pos,
                            const source_kind kind, const bool root_can_escape )
{
    if( !root_can_escape ) {
        return;
    }
    const bool forwarding_ground_gun = kind == source_kind::ground_item &&
                                       gun_root_forwards_to_light_mod( root );
    if( !forwarding_ground_gun ) {
        append_item( out, root, carrier, pos, kind );
    }
    // MOD pockets are not part of the generic CONTAINER tree.  A ground gun
    // forwards its emission to a mounted light too, so index that actual
    // source instead of silently dropping it along with the forwarding root.
    if( forwarding_ground_gun ) {
        const std::vector<const item *> mods = root.gunmods();
        for( const item *mod : mods ) {
            if( mod != nullptr ) {
                append_container_tree( out, *mod, carrier, pos, kind, true );
            }
        }
    }
    for( const item *nested : root.all_items_top( pocket_type::CONTAINER ) ) {
        if( nested == nullptr ) {
            continue;
        }
        const item_pocket *pocket = root.contained_where( *nested );
        append_container_tree( out, *nested, carrier, pos, kind,
                               pocket != nullptr && pocket->transparent() );
    }
}

void append_stationary_tile_emitters( std::vector<emitter> &out, map &here,
                                      const tripoint_bub_ms &p )
{
    const tripoint_abs_ms pos = here.get_abs( p );
    const auto add = [&out, &pos]( const source_kind kind, const int luminance,
    const bool directional, std::string provenance ) {
        if( luminance > 0 ) {
            out.push_back( { pos, kind, luminance, directional, std::move( provenance ) } );
        }
    };
    if( here.ter( p )->light_emitted > 0 ) {
        add( source_kind::terrain, here.ter( p )->light_emitted, false,
             "terrain:" + here.ter( p )->id.str() );
    }
    if( here.furn( p )->light_emitted > 0 ) {
        add( source_kind::furniture, here.furn( p )->light_emitted, false,
             "furniture:" + here.furn( p )->id.str() );
    }
    for( const auto &entry : here.field_at( p ) ) {
        add( source_kind::field, static_cast<int>( entry.second.get_intensity_level().light_emitted ),
             false, "field:" + entry.first.obj().id.str() );
    }
}

void append_stationary_vehicle_emitters( std::vector<emitter> &out, map &here,
        const std::function<bool( const tripoint_bub_ms & )> &selected )
{
    for( wrapped_vehicle &wrapped : here.get_vehicles() ) {
        vehicle *veh = wrapped.v;
        if( veh == nullptr ) {
            continue;
        }
        for( vehicle_part *part : veh->lights() ) {
            if( part == nullptr ) {
                continue;
            }
            const tripoint_bub_ms p = veh->bub_part_pos( here, *part );
            if( !here.inbounds( p ) || !selected( p ) ) {
                continue;
            }
            const vpart_info &info = part->info();
            if( info.bonus > 0 ) {
                out.push_back( { here.get_abs( p ), source_kind::vehicle_part, info.bonus,
                                 info.has_flag( VPFLAG_CONE_LIGHT ) ||
                                 info.has_flag( VPFLAG_WIDE_CONE_LIGHT ),
                                 "vehicle:" + info.id.str() } );
            }
        }
    }
}

void append_stationary_monster_emitters( std::vector<emitter> &out, map &here,
        const std::function<bool( const tripoint_bub_ms & )> &selected )
{
    if( g == nullptr ) {
        return;
    }
    for( monster &critter : g->all_monsters() ) {
        const tripoint_bub_ms p = critter.pos_bub();
        if( critter.is_hallucination() || !here.inbounds( p ) || !selected( p ) ) {
            continue;
        }
        const int luminance = static_cast<int>( critter.calculate_by_enchantment(
                                  critter.type->luminance, enchant_vals::mod::LUMINATION, true ) );
        if( luminance > 0 ) {
            out.push_back( { here.get_abs( p ), source_kind::luminous_monster, luminance, false,
                             "monster:" + critter.type->id.str() } );
        }
    }
}
}

std::vector<emitter> collect_item_emitters( const Character &carrier, map &here,
        const int radius )
{
    std::vector<emitter> result;
    const tripoint_bub_ms origin = carrier.pos_bub();
    std::map<const item *, bool> visible;
    carrier.visit_items( [&result, &carrier, &here, &visible]( item *it, item *parent ) {
        if( it != nullptr ) {
            bool can_escape = parent == nullptr;
            if( parent != nullptr ) {
                const item_pocket *pocket = parent->contained_where( *it );
                can_escape = visible[parent] && pocket != nullptr && pocket->transparent();
            }
            visible[it] = can_escape;
            if( !can_escape ) {
                return VisitResponse::NEXT;
            }
            // getlight_emit() forwards a gun's light to its mounted mod.  The
            // mounted mod is visited separately, so retain its provenance and
            // avoid reporting the forwarding gun root a second time.
            if( parent == nullptr && gun_root_forwards_to_light_mod( *it ) ) {
                return VisitResponse::NEXT;
            }
            source_kind kind = source_kind::carried_item;
            if( parent != nullptr && parent->is_gun() ) {
                kind = source_kind::weapon_mounted;
            } else if( carrier.is_worn( *it ) ) {
                kind = source_kind::worn_item;
            }
            append_item( result, *it, &carrier, here.get_abs( carrier.pos_bub() ), kind );
        }
        return VisitResponse::NEXT;
    } );

    // Gun mods live in MOD pockets, which are intentionally excluded from the
    // generic container visitor.  Visit them explicitly so a mounted source
    // is retained while its forwarding gun root is not double-counted.
    carrier.visit_items( [&result, &carrier, &here]( item *it, item *parent ) {
        if( it != nullptr && parent == nullptr && it->is_gun() ) {
            for( const item *mod : it->gunmods() ) {
                if( mod != nullptr ) {
                    append_container_tree( result, *mod, &carrier,
                                           here.get_abs( carrier.pos_bub() ),
                                           source_kind::weapon_mounted, true );
                }
            }
        }
        return VisitResponse::SKIP;
    } );

    for( const tripoint_bub_ms &p : here.points_in_radius( origin, radius ) ) {
        for( const item &it : here.i_at( p ) ) {
            append_container_tree( result, it, nullptr, here.get_abs( p ),
                                   source_kind::ground_item, true );
        }
    }
    return result;
}

std::vector<emitter> collect_stationary_emitters( map &here, const tripoint_bub_ms &origin,
        const int radius )
{
    std::vector<emitter> result;
    for( const tripoint_bub_ms &p : here.points_in_radius( origin, radius ) ) {
        append_stationary_tile_emitters( result, here, p );
    }
    const auto in_radius = [&origin, radius]( const tripoint_bub_ms &p ) {
        return rl_dist( origin, p ) <= radius;
    };
    append_stationary_vehicle_emitters( result, here, in_radius );
    append_stationary_monster_emitters( result, here, in_radius );
    return result;
}

loaded_z_source_index index_loaded_z_sources( const Character &carrier, map &here )
{
    loaded_z_source_index result;
    // Keep carried equipment in the index even though it has no map tile.
    // Radius zero also retains a ground item directly under the carrier; the
    // full map pass below deliberately skips that one tile to avoid a duplicate.
    result.item_emitters = collect_item_emitters( carrier, here, 0 );
    const tripoint_bub_ms carrier_pos = carrier.pos_bub();

    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        for( const tripoint_bub_ms &p : here.points_on_zlevel( z ) ) {
            // The radius-zero item pass already indexes a ground lamp on the
            // carrier tile.  Only skip that duplicate item enumeration; the
            // native terrain, furniture, and field owners on the same tile
            // remain valid stationary sources and must still be indexed.
            if( p != carrier_pos ) {
                for( const item &it : here.i_at( p ) ) {
                    append_container_tree( result.item_emitters, it, nullptr, here.get_abs( p ),
                                           source_kind::ground_item, true );
                }
            }
            append_stationary_tile_emitters( result.stationary_emitters, here, p );
        }
    }
    const auto all_loaded = []( const tripoint_bub_ms & ) {
        return true;
    };
    append_stationary_vehicle_emitters( result.stationary_emitters, here, all_loaded );
    append_stationary_monster_emitters( result.stationary_emitters, here, all_loaded );
    return result;
}

escape evaluate_escape( map &here, const tripoint_bub_ms &source )
{
    escape result;
    result.exposed_to_sky = here.is_outside( source );

    // Do not treat an arbitrary opening in a large building as an escape
    // route.  This is a local aperture test, not a replacement lightmap.
    constexpr int aperture_radius = 3;
    for( int dx = -aperture_radius; dx <= aperture_radius; ++dx ) {
        for( int dy = -aperture_radius; dy <= aperture_radius; ++dy ) {
            if( dx == 0 && dy == 0 ) {
                continue;
            }
            const int distance = std::max( std::abs( dx ), std::abs( dy ) );
            const tripoint_bub_ms candidate( source.x() + dx, source.y() + dy, source.z() );
            if( !here.inbounds( candidate ) || !here.is_outside( candidate ) ||
                !here.sees( source, candidate, distance, false ) ) {
                continue;
            }
            result.side_leakage = 2;
        }
    }
    const ter_id terrain = here.ter( source );
    result.elevated_exposed = result.exposed_to_sky && source.z() > 0 &&
                              ( terrain == ter_t_flat_roof || terrain == ter_t_tile_flat_roof );
    return result;
}

detection detect( const route &path )
{
    detection result;
    if( !path.source_exposed || path.brightness_range_omt <= 0 || path.distance_omt < 0 ||
        path.distance_omt > path.brightness_range_omt ||
        ( path.vertical_offset != 0 && !path.vertical_sightline ) ) {
        return result;
    }
    int remaining = path.brightness_range_omt - path.distance_omt;
    for( const int see_cost : path.terrain_see_costs ) {
        if( see_cost < 0 ) {
            return result;
        }
        remaining -= see_cost;
        if( remaining < 0 ) {
            return result;
        }
    }
    result.visible = true;
    result.remaining_brightness = remaining;
    return result;
}

bool turn_sample_gate::begin_advancing_turn( const int turn, const bool advancing )
{
    if( !advancing || turn < 0 || turn == last_sampled_turn ) {
        return false;
    }
    // A load/debug time rewind starts a distinct simulation timeline.  It
    // does not fabricate an observation; the next advancing turn may sample.
    last_sampled_turn = turn;
    return true;
}
}
