#include "migo_population_native.h"
#include "migo_tower_native.h"

#include <algorithm>
#include <set>
#include <utility>

#include "calendar.h"
#include "creature_tracker.h"
#include "field.h"
#include "field_type.h"
#include "game.h"
#include "horde_entity.h"
#include "map.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "monster.h"
#include "mtype.h"
#include "omdata.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "rng.h"
#include "submap.h"

namespace migo_population
{
namespace
{
const species_id species_MIGO( "MIGO" );
const field_type_str_id fd_fire( "fd_fire" );

std::string terrain_type( const tripoint_abs_omt &p )
{
    return overmap_buffer.ter_existing( p )->get_type_id().str();
}

bool interior( const site_geometry &geometry, const tripoint_abs_omt &p )
{
    if( p.z() < 0 || p.z() > 3 ) {
        return false;
    }
    const std::string type = terrain_type( p );
    return geometry.tower ? type.rfind( "mi-go_scout_tower_", 0 ) == 0 :
           type.rfind( "mi-go_camp", 0 ) == 0;
}

bool contains( const site_geometry &geometry, const tripoint_abs_ms &p )
{
    const tripoint_abs_omt omt = project_to<coords::omt>( p );
    return std::any_of( geometry.parts.begin(), geometry.parts.end(), [&]( const auto & part ) {
        return part.xy() == omt.xy() && interior( geometry, omt );
    } );
}

struct saved_part {
    tripoint_abs_sm position;
    submap *sm;
};

std::vector<saved_part> saved_parts( const site_geometry &geometry )
{
    std::vector<saved_part> result;
    for( const tripoint_abs_omt &part : geometry.parts ) {
        for( int z = 0; z <= 3; ++z ) {
            const tripoint_abs_omt omt( part.xy(), z );
            if( !interior( geometry, omt ) ) {
                continue;
            }
            for( int x = 0; x < 2; ++x ) {
                for( int y = 0; y < 2; ++y ) {
                    const tripoint_abs_sm p = project_to<coords::sm>( omt ) + point( x, y );
                    submap *sm = MAPBUFFER.lookup_submap_existing( p );
                    if( !sm ) {
                        return {};
                    }
                    result.push_back( { p, sm } );
                }
            }
        }
    }
    return result;
}

bool resident_slot( const spawn_point &slot )
{
    return slot.count > 0 && slot.mission_id < 0 && !slot.friendly &&
           slot.type.is_valid() && slot.type->in_species( species_MIGO );
}

void identify_slots( submap &sm, const site_geometry &geometry )
{
    std::vector<spawn_point> identified;
    for( const spawn_point &slot : sm.spawns ) {
        if( !resident_slot( slot ) || !slot.population.empty() ) {
            identified.push_back( slot );
            continue;
        }
        for( int i = 0; i < slot.count; ++i ) {
            spawn_point member = slot;
            member.count = 1;
            member.population = { geometry.id, random_string( 32 ), 0, geometry.anchor };
            identified.push_back( std::move( member ) );
        }
    }
    sm.spawns = std::move( identified );
}

tripoint_abs_ms slot_position( const saved_part &part, const spawn_point &slot )
{
    return project_to<coords::ms>( part.position ) + point_rel_ms( slot.pos.raw() );
}

// Both discovery orders consume this same stored manifest, not a new group
// draw. No receiving owner means the exact original slot remains available.
void retain_slots( site_record &site, const std::vector<saved_part> &parts )
{
    for( const saved_part &part : parts ) {
        auto &slots = part.sm->spawns;
        for( auto it = slots.begin(); it != slots.end(); ) {
            if( it->population.site_id != site.id || it->population.empty() ) {
                ++it;
                continue;
            }
            member_record *record = site.find( it->population.member_id );
            if( !record ) {
                ++it;
                continue;
            }
            if( record->owner != representation::slot ) {
                // A stale saved source cannot refill a transferred or dead ID.
                it = slots.erase( it );
                continue;
            }
            const tripoint_abs_ms position = slot_position( part, *it );
            if( !site.owns( it->population, representation::slot ) ) {
                ++it;
                continue;
            }
            monster payload( it->type );
            payload.population_identity() = it->population;
            payload.set_pos_abs_only( position );
            payload.unique_name = it->name.value_or( "" );
            for( const auto &ammo : it->data.ammo ) {
                payload.ammo.insert_or_assign( ammo.first, ammo.second.get() );
            }
            payload.set_patrol_route( it->data.patrol_points_rel_ms );
            horde_entity *retained = nullptr;
            tripoint_abs_ms placed_at = position;
            // This is creation from a saved spawn slot, not transport of an
            // already positioned actor. Preserve the native bounded home
            // placement allowance, checking actual saved geometry and owners.
            for( int radius = 0; radius <= 3 && !retained; ++radius ) {
                for( int dx = -radius; dx <= radius && !retained; ++dx ) {
                    for( int dy = -radius; dy <= radius && !retained; ++dy ) {
                        if( std::max( std::abs( dx ), std::abs( dy ) ) != radius ) {
                            continue;
                        }
                        const tripoint_abs_ms candidate = position + point( dx, dy );
                        const tripoint_abs_sm sm_position = project_to<coords::sm>( candidate );
                        const submap *sm = MAPBUFFER.lookup_submap_existing( sm_position );
                        const point_sm_ms cell( candidate.x() - sm_position.x() * SEEX,
                                                candidate.y() - sm_position.y() * SEEY );
                        if( !sm || sm->get_ter( cell )->movecost <= 0 ||
                            sm->get_ter( cell )->has_flag( ter_furn_flag::TFLAG_NO_FLOOR ) ||
                            sm->get_furn( cell )->movecost < 0 || sm->get_field( cell ).find_field( fd_fire ) ||
                            get_creature_tracker().creature_at( candidate, true ) ) {
                            continue;
                        }
                        if( get_map().inbounds( candidate ) &&
                            !payload.can_move_to( get_map().get_bub( candidate ) ) ) {
                            continue;
                        }
                        payload.set_pos_abs_only( candidate );
                        retained = overmap_buffer.get( project_to<coords::om>( candidate ).xy() ).
                                   retain_monster_payload( candidate, payload );
                        placed_at = candidate;
                    }
                }
            }
            if( !retained ) {
                ++it;
                continue;
            }
            // Native insertion succeeded while the source still existed.
            // Commit its lease once, then release only that source slot.
            if( !site.transfer( it->population, representation::slot, representation::abstract ) ) {
                overmap_buffer.get( project_to<coords::om>( placed_at ).xy() ).release_monster_payload(
                    placed_at, it->population.member_id );
                ++it;
                continue;
            }
            retained->monster_data->population_identity() = record->token;
            record->hp = retained->monster_data->get_hp();
            record->home = placed_at;
            it = slots.erase( it );
        }
    }
}

int ambient_count( const site_geometry &geometry )
{
    int count = 0;
    for( const monster &m : g->all_monsters() ) {
        if( m.population_identity().empty() && m.type->in_species( species_MIGO ) &&
            !m.is_dead() && contains( geometry, m.pos_abs() ) ) {
            ++count;
        }
    }
    std::set<point_abs_om> inspected;
    for( const tripoint_abs_omt &part : geometry.parts ) {
        const point_abs_om om = project_to<coords::om>( part ).xy();
        if( !inspected.insert( om ).second ) {
            continue;
        }
        for( const auto &entry : overmap_buffer.get( om ).horde_entities() ) {
            const horde_entity &entity = *entry.second;
            if( entity.get_type()->in_species( species_MIGO ) &&
                ( !entity.monster_data || entity.monster_data->population_identity().empty() ) &&
                contains( geometry, entry.first ) ) {
                ++count;
            }
        }
    }
    return count;
}

bool occupancy_known( const site_geometry &geometry )
{
    const map &here = get_map();
    for( const tripoint_abs_omt &part : geometry.parts ) {
        for( int z = 0; z <= 3; ++z ) {
            const tripoint_abs_omt omt( part.xy(), z );
            if( !interior( geometry, omt ) ) {
                continue;
            }
            const tripoint_abs_ms corner = project_to<coords::ms>( omt );
            if( !here.inbounds( corner ) || !here.inbounds( corner + point( 23, 23 ) ) ) {
                return false;
            }
        }
    }
    return true;
}
} // namespace

std::optional<site_geometry> geometry_at( const tripoint_abs_omt &position )
{
    const tripoint_abs_omt ground( position.xy(), 0 );
    const std::string type = terrain_type( ground );
    site_geometry result;
    if( type == "mi-go_scout_tower_1" ) {
        result.anchor = ground;
        result.tower = true;
        result.parts.push_back( ground );
    } else if( type == "mi-go_camp1" || type == "mi-go_camp2" ) {
        std::vector<tripoint_abs_omt> anchors;
        for( int dx = -1; dx <= 1; ++dx ) {
            for( int dy = -1; dy <= 1; ++dy ) {
                const tripoint_abs_omt candidate = ground + point( dx, dy );
                if( terrain_type( candidate ) == "mi-go_camp1" ) {
                    anchors.push_back( candidate );
                }
            }
        }
        if( anchors.size() != 1 ) {
            return std::nullopt; // Ambiguous geometry cannot grant membership.
        }
        result.anchor = anchors.front();
        result.parts.push_back( result.anchor );
        for( const point &offset : four_adjacent_offsets ) {
            const tripoint_abs_omt candidate = result.anchor + offset;
            if( terrain_type( candidate ) == "mi-go_camp2" ) {
                result.parts.push_back( candidate );
            }
        }
        if( result.parts.size() != 2 ) {
            return std::nullopt;
        }
    } else {
        return std::nullopt; // Barry/mission towers and roaming groups excluded.
    }
    result.id = std::string( result.tower ? "mi-go-tower:" : "mi-go-camp:" ) +
                result.anchor.to_string_writable();
    return result;
}

void tag_generated_slots( map &here, submap &sm, const tripoint_abs_sm &submap_position )
{
    const tripoint_abs_omt position = project_to<coords::omt>( submap_position );
    const auto geometry = geometry_at( position );
    if( !geometry || !interior( *geometry, position ) ) {
        return;
    }
    world_state &world = overmap_buffer.global_state.migo_populations;
    site_record *site = world.find( geometry->id );
    if( !site ) {
        site_record pending;
        pending.id = geometry->id;
        pending.anchor = geometry->anchor;
        pending.tower = geometry->tower;
        // This map has not yet published its fresh submaps to MAPBUFFER.
        // A previously saved interior anywhere in the footprint makes this
        // a legacy site, including a partially missing old map.
        pending.new_content = true;
        for( const tripoint_abs_omt &part : geometry->parts ) {
            for( int z = 0; z <= 3; ++z ) {
                if( !interior( *geometry, tripoint_abs_omt( part.xy(), z ) ) ) {
                    continue;
                }
                for( int x = 0; x < 2; ++x ) {
                    for( int y = 0; y < 2; ++y ) {
                        if( MAPBUFFER.lookup_submap_existing( project_to<coords::sm>(
                                tripoint_abs_omt( part.xy(), z ) ) + point( x, y ) ) ) {
                            pending.new_content = false;
                        }
                    }
                }
            }
        }
        world.sites.push_back( std::move( pending ) );
        site = &world.sites.back();
        world.refresh_site( site->id );
    }
    if( site->seed_committed ) {
        // Regenerated map data cannot refill established biological residents.
        auto &slots = sm.spawns;
        slots.erase( std::remove_if( slots.begin(), slots.end(), resident_slot ), slots.end() );
        if( site->source && project_to<coords::sm>( *site->source ) == submap_position ) {
            site->source_destroyed = true;
            site->production_elapsed = 0;
            here.furn_set( here.get_bub( *site->source ), furn_str_id::NULL_ID().id() );
        }
        return;
    }
    if( !site->new_content ) {
        // These are newly reconstructed maps of an already existing site.
        // Only its surviving saved slots/actors are resident provenance;
        // another generator draw or fresh pod cannot refill that history.
        auto &slots = sm.spawns;
        slots.erase( std::remove_if( slots.begin(), slots.end(), resident_slot ), slots.end() );
        for( int sx = 0; sx < SEEX; ++sx ) {
            for( int sy = 0; sy < SEEY; ++sy ) {
                const point_sm_ms cell( sx, sy );
                const std::string furniture = sm.get_furn( cell ).id().str();
                if( furniture == "f_migo_brood_pod" || furniture == "f_alien_pod" ) {
                    here.furn_set( here.get_bub( project_to<coords::ms>( submap_position ) +
                                                point_rel_ms( sx, sy ) ), furn_str_id::NULL_ID().id() );
                }
            }
        }
        return;
    }
    identify_slots( sm, *geometry );
}

site_record *discover( const tripoint_abs_omt &position, const monster *incoming,
                       representation incoming_owner )
{
    const auto geometry = geometry_at( position );
    if( !geometry || !interior( *geometry, position ) ) {
        return nullptr;
    }
    world_state &world = overmap_buffer.global_state.migo_populations;
    if( site_record *site = world.find( geometry->id ); site && site->seed_committed ) {
        const auto parts = saved_parts( *geometry );
        if( !parts.empty() ) {
            retain_slots( *site, parts );
        }
        return site;
    }
    // Loading geometry neither spawns its slots nor grants production time.
    for( const tripoint_abs_omt &part : geometry->parts ) {
        tinymap generated;
        generated.load( part, false );
    }
    const auto parts = saved_parts( *geometry );
    if( parts.empty() ) {
        return nullptr;
    }
    site_record candidate;
    if( const site_record *pending = world.find( geometry->id ) ) {
        candidate = *pending;
    }
    candidate.id = geometry->id;
    candidate.anchor = geometry->anchor;
    candidate.tower = geometry->tower;
    std::vector<member_record> members;
    std::vector<std::pair<tripoint_abs_ms, std::string>> dedicated;
    std::vector<std::pair<tripoint_abs_ms, std::string>> legacy;
    for( const saved_part &part : parts ) {
        // Saved resident slots are provenance; untagged physical actors are not.
        identify_slots( *part.sm, *geometry );
        for( const spawn_point &slot : part.sm->spawns ) {
            if( slot.population.site_id == candidate.id && resident_slot( slot ) ) {
                member_record member;
                member.token = slot.population;
                member.type = slot.type;
                member.home = slot_position( part, slot );
                member.hp = monster( slot.type ).get_hp();
                members.push_back( std::move( member ) );
            }
        }
        for( int x = 0; x < SEEX; ++x ) {
            for( int y = 0; y < SEEY; ++y ) {
                const point_sm_ms local( x, y );
                const std::string furniture = part.sm->get_furn( local ).id().str();
                const tripoint_abs_ms absolute = project_to<coords::ms>( part.position ) +
                                                point_rel_ms( x, y );
                if( furniture == "f_migo_brood_pod" ) {
                    dedicated.emplace_back( absolute, furniture );
                } else if( furniture == "f_alien_pod" ) {
                    legacy.emplace_back( absolute, furniture );
                }
            }
        }
    }
    bool prior_payload = false;
    const auto adopt = [&]( const monster &payload, representation owner ) {
        const identity &token = payload.population_identity();
        if( token.empty() || token.site_id != candidate.id ||
            !payload.type->in_species( species_MIGO ) ) {
            return;
        }
        prior_payload = true;
        auto existing = std::find_if( members.begin(), members.end(), [&]( const member_record & member ) {
            return member.token.member_id == token.member_id;
        } );
        if( existing != members.end() && ( existing->token.epoch > token.epoch ||
                ( existing->token.epoch == token.epoch && existing->owner != representation::slot ) ) ) {
            return;
        }
        member_record observed;
        observed.token = token;
        observed.type = payload.type->id;
        observed.home = contains( *geometry, payload.pos_abs() ) ? payload.pos_abs() :
                        project_to<coords::ms>( candidate.anchor ) + point( 12, 12 );
        observed.owner = payload.is_dead() ? representation::dead : owner;
        observed.hp = payload.get_hp();
        if( existing == members.end() ) {
            members.push_back( std::move( observed ) );
        } else {
            *existing = std::move( observed );
        }
    };
    for( const monster &payload : g->all_monsters() ) {
        adopt( payload, representation::local );
    }
    if( incoming ) {
        adopt( *incoming, incoming_owner );
    }
    std::set<point_abs_om> inspected;
    for( const auto &part : geometry->parts ) {
        const point_abs_om om = project_to<coords::om>( part ).xy();
        if( !inspected.insert( om ).second ) {
            continue;
        }
        for( const auto &entry : overmap_buffer.get( om ).horde_entities() ) {
            if( entry.second->monster_data ) {
                adopt( *entry.second->monster_data, representation::abstract );
            }
        }
    }
    auto &sources = candidate.new_content ? dedicated : legacy;
    if( prior_payload && !candidate.new_content ) {
        // Explicit prior actors can recover membership, but cannot establish
        // the lost factory's original coordinate/destruction fact. A surviving
        // decoration is not permission to reconstruct or switch that source.
        sources.clear();
    }
    std::sort( sources.begin(), sources.end() );
    const auto source = sources.empty() ? std::optional<tripoint_abs_ms>() : sources.front().first;
    const std::string furniture = sources.empty() ? "" : sources.front().second;
    // A new tower has its dedicated source and exact designed slot manifest.
    // Legacy types/counts are preserved, including over-capacity residents.
    const bool new_tower = candidate.tower && candidate.new_content;
    if( !candidate.commit_seed( members, source, furniture, ambient_count( *geometry ),
                               occupancy_known( *geometry ),
                               new_tower ) ) {
        return nullptr;
    }
    site_record *committed = world.find( geometry->id );
    if( committed ) {
        *committed = std::move( candidate );
    } else {
        world.sites.push_back( std::move( candidate ) );
        committed = &world.sites.back();
    }
    world.refresh_site( committed->id );
    site_record &site = *committed;
    retain_slots( site, parts );
    return &site;
}

bool can_transfer( const monster &payload, representation from )
{
    const identity &token = payload.population_identity();
    if( token.empty() ) {
        return true;
    }
    const site_record *site = overmap_buffer.global_state.migo_populations.find( token.site_id );
    return site && site->owns( token, from );
}

bool reconcile_payload( const monster &payload, representation observed_owner )
{
    const identity &token = payload.population_identity();
    if( token.empty() ) {
        return true;
    }
    if( !payload.type->in_species( species_MIGO ) ) {
        return false;
    }
    site_record *site = overmap_buffer.global_state.migo_populations.find( token.site_id );
    if( !site ) {
        const auto geometry = geometry_at( token.site_anchor );
        if( !geometry || geometry->id != token.site_id ) {
            return false;
        }
        site = discover( token.site_anchor, &payload, observed_owner );
    }
    if( !site || !site->seed_committed || site->id != token.site_id ) {
        return false;
    }
    member_record *member = site->find( token.member_id );
    if( !member ) {
        // Saved explicit provenance can recover an owner after a partial
        // global save. No untagged nearby actor can enter this branch.
        member_record recovered;
        recovered.token = token;
        recovered.type = payload.type->id;
        recovered.home = project_to<coords::ms>( site->anchor ) + point( 12, 12 );
        recovered.owner = payload.is_dead() ? representation::dead : observed_owner;
        recovered.hp = payload.get_hp();
        site->members.push_back( std::move( recovered ) );
        member = &site->members.back();
    }
    if( member->owner == representation::dead || member->token.epoch > token.epoch ) {
        return false;
    }
    if( member->token.epoch < token.epoch ) {
        member->token = token;
        member->owner = payload.is_dead() ? representation::dead : observed_owner;
    }
    if( member->owner != observed_owner || payload.is_dead() ) {
        return false;
    }
    member->hp = payload.get_hp();
    member->type = payload.type->id;
    if( site->returned( token, payload.pos_abs() ) ) {
        overmap_buffer.global_state.migo_populations.refresh_site( site->id );
        overmap_buffer.global_state.migo_populations.settlement_sites.insert( site->id );
    }
    // Highest committed epoch wins across a partial cross-file save. Retire
    // stale physical carriers without generating a biological death/corpse.
    std::vector<monster *> stale_local;
    for( monster &existing : g->all_monsters() ) {
        const identity &old = existing.population_identity();
        if( old.site_id == token.site_id && old.member_id == token.member_id &&
            old.epoch < member->token.epoch ) {
            stale_local.push_back( &existing );
        }
    }
    for( const monster *existing : stale_local ) {
        g->remove_zombie( *existing );
    }
    const std::set<point_abs_om> owners = { project_to<coords::om>( member->home ).xy(),
                                          project_to<coords::om>( payload.pos_abs() ).xy() };
    for( const point_abs_om &owner : owners ) {
        overmap &om = overmap_buffer.get( owner );
        for( const auto &entry : om.horde_entities() ) {
            if( !entry.second->monster_data ) {
                continue;
            }
            const identity &old = entry.second->monster_data->population_identity();
            if( old.site_id == token.site_id && old.member_id == token.member_id &&
                old.epoch < member->token.epoch ) {
                om.release_monster_payload( entry.first, old.member_id );
            }
        }
    }
    return true;
}

bool commit_position( const monster &payload, representation owner,
                      const tripoint_abs_ms &actual_position )
{
    auto &world = overmap_buffer.global_state.migo_populations;
    auto *site = world.find( payload.population_identity().site_id );
    if( !site || !site->owns( payload.population_identity(), owner ) ||
        !payload.type->in_species( species_MIGO ) ) {
        return false;
    }
    if( payload.is_dead() ) {
        if( site->record_death( payload.population_identity() ) ) {
            world.refresh_site( site->id );
            world.settlement_sites.insert( site->id );
        }
        return false;
    }
    const bool confirmed = world.confirm_position( payload.population_identity(), owner,
        actual_position );
    const auto *tower = overmap_buffer.global_state.migo_towers.find( site->id );
    const auto *intent = payload.migo_intent();
    const auto *member = site->find( payload.population_identity().member_id );
    if( confirmed && tower && tower->operation && !tower->operation->resolved && intent &&
        intent->generation == tower->operation->generation &&
        tower->operation->contains( payload.population_identity().member_id ) &&
        ( intent->phase == migo_tower::member_phase::returning ||
          intent->phase == migo_tower::member_phase::settled ) &&
        payload.pos_abs() == actual_position &&
        project_to<coords::omt>( actual_position ) == project_to<coords::omt>( member->home ) ) {
        // A native return can need policy reconciliation even outside an
        // inactive home, including the first authenticated owner after load.
        world.settlement_sites.insert( site->id );
    }
    return confirmed;
}

bool transfer( monster &payload, representation from, representation to )
{
    const identity token = payload.population_identity();
    if( token.empty() ) {
        return true;
    }
    site_record *site = overmap_buffer.global_state.migo_populations.find( token.site_id );
    if( !site || !site->transfer( token, from, to ) ) {
        return false;
    }
    payload.population_identity() = site->find( token.member_id )->token;
    site->observe_health( payload.population_identity(), payload.get_hp() );
    if( site->returned( payload.population_identity(), payload.pos_abs() ) ) {
        overmap_buffer.global_state.migo_populations.refresh_site( site->id );
        overmap_buffer.global_state.migo_populations.settlement_sites.insert( site->id );
    }
    commit_position( payload, to, payload.pos_abs() );
    return true;
}

bool superseded( const monster &payload )
{
    const identity &token = payload.population_identity();
    const site_record *site = overmap_buffer.global_state.migo_populations.find( token.site_id );
    const member_record *member = site ? site->find( token.member_id ) : nullptr;
    return member && ( member->owner == representation::dead || member->token.epoch > token.epoch );
}

void source_changed( const tripoint_abs_ms &position, const std::string &furniture )
{
    for( site_record &site : overmap_buffer.global_state.migo_populations.sites ) {
        if( site.source == position && furniture != site.source_furniture ) {
            site.source_destroyed = true;
            site.production_elapsed = 0;
        }
    }
}

void service_loaded()
{
    map &here = get_map();
    const int now = to_turn<int>( calendar::turn );
    migo_tower::refresh_activation( now );
    // Settlement/health is independent of a functioning or active brood pod.
    for( monster &m : g->all_monsters() ) {
        const identity &token = m.population_identity();
        if( token.empty() ) {
            continue;
        }
        if( site_record *site = overmap_buffer.global_state.migo_populations.find( token.site_id ) ) {
            site->observe_health( token, m.get_hp() );
            if( !m.is_dead() ) {
                commit_position( m, representation::local, m.pos_abs() );
                if( site->returned( token, m.pos_abs() ) ) {
                    overmap_buffer.global_state.migo_populations.refresh_site( site->id );
                    overmap_buffer.global_state.migo_populations.settlement_sites.insert( site->id );
                }
            }
        }
    }
    const auto active = overmap_buffer.global_state.migo_populations.active_sites;
    for( const auto &id : active ) {
        auto *record = overmap_buffer.global_state.migo_populations.find( id );
        if( !record ) {
            continue;
        }
        site_record &site = *record;
        if( !site.activity.active ) {
            continue;
        }
        if( !site.source ) {
            continue;
        }
        const auto geometry = geometry_at( site.anchor );
        if( !geometry ) {
            continue;
        }
        const int elapsed = site.last_service_world == now - 1 ?
                            std::min( 1, site.activity.active_delta_turns ) : 0;
        if( !here.inbounds( *site.source ) ) {
            // Unknown interiors retry at most once per minute; no unverified
            // time is credited while their saved geometry is unavailable.
            if( site.source_destroyed || ( !site.occupancy_verified &&
                site.last_service_world >= 0 && now % 60 != 0 ) ) {
                site.advance( now, 0, std::nullopt, []( const member_record & ) {
                    return false;
                } );
                continue;
            }
            // Read only existing submaps. A missing part is unknown, not a
            // destroyed source or permission to generate a replacement map.
            // Resident submaps are served from MAPBUFFER after the first read.
            const std::vector<saved_part> parts = saved_parts( *geometry );
            const tripoint_abs_sm source_sm = project_to<coords::sm>( *site.source );
            const auto source_part = std::find_if( parts.begin(), parts.end(),
            [&]( const saved_part &part ) {
                return part.position == source_sm;
            } );
            if( source_part == parts.end() ) {
                site.occupancy_verified = false;
                site.advance( now, 0, std::nullopt, []( const member_record & ) {
                    return false;
                } );
                continue;
            }
            const point_sm_ms cell( site.source->x() - source_sm.x() * SEEX,
                                    site.source->y() - source_sm.y() * SEEY );
            const bool source_exists = source_part->sm->get_furn( cell ).id().str() ==
                                       site.source_furniture;
            site.ambient_occupancy = ambient_count( *geometry );
            for( const saved_part &part : parts ) {
                for( const spawn_point &slot : part.sm->spawns ) {
                    if( resident_slot( slot ) &&
                        ( slot.population.empty() || slot.population.site_id != site.id ||
                          !site.find( slot.population.member_id ) ) ) {
                        site.ambient_occupancy += slot.count;
                    }
                }
            }
            site.occupancy_verified = true;
            // Time is earned offscreen, but placement is refused until the
            // native loaded map can authenticate geometry and retain a body.
            site.advance( now, elapsed, source_exists, {} );
            continue;
        }
        site.ambient_occupancy = ambient_count( *geometry );
        site.occupancy_verified = occupancy_known( *geometry );
        const bool source_exists = here.furn( here.get_bub( *site.source ) ).id().str() ==
                                   site.source_furniture;
        std::optional<tripoint_abs_ms> birth_position;
        const bool born = site.advance( now, elapsed, source_exists, [&]( const member_record & birth ) {
            monster payload( birth.type );
            payload.population_identity() = birth.token;
            payload.set_pos_abs_only( birth.home );
            for( const tripoint_bub_ms &p : here.points_in_radius( here.get_bub( birth.home ), 3, 0 ) ) {
                if( !here.inbounds( p ) || !payload.can_move_to( p ) ||
                    ( !payload.flies() && !here.has_floor_or_water( p ) ) ||
                    here.get_field( p, fd_fire ) ||
                    get_creature_tracker().creature_at( here.get_abs( p ), true ) ) {
                    continue;
                }
                payload.set_pos_abs_only( here.get_abs( p ) );
                if( overmap_buffer.get( project_to<coords::om>( payload.pos_abs() ).xy() ).
                    retain_monster_payload( payload.pos_abs(), payload ) ) {
                    birth_position = payload.pos_abs();
                    return true;
                }
            }
            return false;
        } );
        if( born && birth_position ) {
            overmap_buffer.global_state.migo_populations.refresh_site( site.id );
            overmap_buffer.global_state.migo_populations.confirm_position( site.members.back().token,
                    representation::abstract, *birth_position );
            // The ledger now owns this birth, so ordinary exact materialization
            // can claim it. No separate debug-only population counter exists.
            overmap_buffer.spawn_monster( project_to<coords::sm>( *birth_position ), false );
        }
    }
}
} // namespace migo_population
