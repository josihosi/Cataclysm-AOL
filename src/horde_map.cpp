#include "horde_map.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "debug.h"
#include "map_scale_constants.h"
#include "monster.h"
#include "migo_tower_native.h"
#include "mtype.h"
#include "overmapbuffer.h"
#include "rng.h"

static const species_id species_FERAL( "FERAL" );
static const species_id species_ZOMBIE( "ZOMBIE" );

// Is just entity enough or do we need to wrap it in a tuple with a coordinate?
// Or worse an iterator?
horde_entity *horde_map::entity_at( const tripoint_om_ms &p )
{
    tripoint_om_sm submap_offset = project_to<coords::sm>( p );
    // TODO reconsider pruning p down to tripoint_om_ms in the first place.
    tripoint_abs_ms entity_loc = project_combine( location, p );
    std::unordered_map<tripoint_om_sm, std::unordered_multimap<tripoint_abs_ms, horde_entity>>::iterator
            active_monster_submap = active_monster_map.find( submap_offset );
    bool active_map_exists = active_monster_submap != active_monster_map.end() &&
                             !active_monster_submap->second.empty();
    if( active_map_exists ) {
        std::unordered_multimap<tripoint_abs_ms, horde_entity>::iterator iter =
            active_monster_submap->second.find( entity_loc );
        if( iter != active_monster_submap->second.end() ) {
            return &iter->second;
        }
    }
    std::unordered_map<tripoint_om_sm, std::unordered_multimap<tripoint_abs_ms, horde_entity>>::iterator
            idle_monster_submap = idle_monster_map.find( submap_offset );
    bool idle_map_exists = idle_monster_submap != idle_monster_map.end() &&
                           !idle_monster_submap->second.empty();
    if( idle_map_exists ) {
        std::unordered_multimap<tripoint_abs_ms, horde_entity>::iterator iter =
            idle_monster_submap->second.find( entity_loc );
        if( iter != idle_monster_submap->second.end() ) {
            return &iter->second;
        }
    }
    std::unordered_map<tripoint_om_sm, std::unordered_multimap<tripoint_abs_ms, horde_entity>>::iterator
            dormant_monster_submap = dormant_monster_map.find( submap_offset );
    bool dormant_map_exists = dormant_monster_submap != dormant_monster_map.end() &&
                              !dormant_monster_submap->second.empty();
    if( dormant_map_exists ) {
        std::unordered_multimap<tripoint_abs_ms, horde_entity>::iterator iter =
            dormant_monster_submap->second.find( entity_loc );
        if( iter != dormant_monster_submap->second.end() ) {
            return  &iter->second;
        }
    }
    std::unordered_map<tripoint_om_sm, std::unordered_multimap<tripoint_abs_ms, horde_entity>>::iterator
            immobile_monster_submap = immobile_monster_map.find( submap_offset );
    bool immobile_map_exists = immobile_monster_submap != immobile_monster_map.end() &&
                               !immobile_monster_submap->second.empty();
    if( immobile_map_exists ) {
        std::unordered_multimap<tripoint_abs_ms, horde_entity>::iterator iter =
            immobile_monster_submap->second.find( entity_loc );
        if( iter != immobile_monster_submap->second.end() ) {
            return  &iter->second;
        }
    }
    return nullptr;
}

std::vector<horde_entity *> horde_map::entities_at( const tripoint_om_ms &p )
{
    std::vector<horde_entity *> result;
    const auto key = project_combine( location, p );
    const auto submap = project_to<coords::sm>( p );
    for( map_type *table : { &active_monster_map, &idle_monster_map,
                           &dormant_monster_map, &immobile_monster_map } ) {
        const auto outer = table->find( submap );
        if( outer == table->end() ) {
            continue;
        }
        const auto range = outer->second.equal_range( key );
        for( auto it = range.first; it != range.second; ++it ) {
            result.push_back( &it->second );
        }
    }
    return result;
}

// TODO: if callers want to filter for dormant vs idle vs active, etc we can do it cheaply.
std::vector < std::unordered_multimap<tripoint_abs_ms, horde_entity> *> horde_map::entity_group_at(
    const tripoint_om_omt &p, int filter )
{
    // TODO: It might be worthwhile to have a single top level map of per-submap containers,
    // and each entry of that map holds each variant container.
    // This eliminates multiple top-level lookups.
    std::vector<std::unordered_multimap<tripoint_abs_ms, horde_entity>*> horde_chunk;
    // TODO: Find all 4 submaps worth of monsters and return them.
    for( int y = 0; y <= 1; ++y ) {
        for( int x = 0; x <= 1; ++x ) {
            tripoint_om_sm target_submap = project_to<coords::sm>( p ) + point{ x, y };
            std::vector<std::unordered_multimap<tripoint_abs_ms, horde_entity> *> submap_of_hordes =
                entity_group_at( target_submap, filter );
            horde_chunk.insert( horde_chunk.end(), submap_of_hordes.begin(), submap_of_hordes.end() );
        }
    }
    return horde_chunk;
}

std::vector<std::unordered_multimap<tripoint_abs_ms, horde_entity> *> horde_map::entity_group_at(
    const tripoint_om_sm &p, int filter )
{
    std::vector<std::unordered_multimap<tripoint_abs_ms, horde_entity>*> horde_chunk;

    if( filter & horde_map_flavors::active ) {
        auto active_monster_map_iter = active_monster_map.find( p );
        if( active_monster_map_iter != active_monster_map.end() ) {
            horde_chunk.push_back( &active_monster_map_iter->second );
        }
    }

    if( filter & horde_map_flavors::idle ) {
        auto idle_monster_map_iter = idle_monster_map.find( p );
        if( idle_monster_map_iter != idle_monster_map.end() ) {
            horde_chunk.push_back( &idle_monster_map_iter->second );
        }
    }

    if( filter & horde_map_flavors::dormant ) {
        auto dormant_monster_map_iter = dormant_monster_map.find( p );
        if( dormant_monster_map_iter != dormant_monster_map.end() ) {
            horde_chunk.push_back( &dormant_monster_map_iter->second );
        }
    }

    if( filter & horde_map_flavors::immobile ) {
        auto immobile_monster_map_iter = immobile_monster_map.find( p );
        if( immobile_monster_map_iter != immobile_monster_map.end() ) {
            horde_chunk.push_back( &immobile_monster_map_iter->second );
        }
    }
    return horde_chunk;
}

// Helper because this is too much to inline.
// This essentially means "these should get sounds broadcast to them".
static bool is_alert( const mtype &type )
{
    bool aggro = type.in_species( species_ZOMBIE ) || type.in_species( species_FERAL );
    return aggro && type.has_flag( mon_flag_HEARS );
}

// These have no goal so they can't go in the active map.
std::optional<std::unordered_multimap<tripoint_abs_ms, horde_entity>::iterator> horde_map::spawn_entity(
    const tripoint_abs_ms &p, mtype_id id, bool allow_occupied )
{
    std::optional<std::unordered_multimap<tripoint_abs_ms, horde_entity>::iterator> result;
    if( id.is_null() || !id.is_valid() ) {
        return result; // Bail out, blacklisted monster or something's wrong.
    }
    std::unordered_map <tripoint_om_sm, std::unordered_multimap<tripoint_abs_ms, horde_entity>> &target_map =
                id->has_flag( mon_flag_DORMANT ) ? dormant_monster_map :
                is_alert( *id ) ? idle_monster_map :
                immobile_monster_map;
    point_abs_om omp;
    tripoint_om_sm sm;
    std::tie( omp, sm ) = project_remain<coords::om>( project_to<coords::sm>( p ) );
    auto &bucket = target_map[sm];
    auto existing = bucket.find( p );
    const bool inserted = allow_occupied || existing == bucket.end();
    result = inserted ? bucket.emplace( p, id ) : existing;
    if( inserted && ( id.str() == "mon_zombie_rider" || id.str() == "mon_writhing_stalker" ) ) {
        overmap *owner = overmap_buffer.find_loaded_overmap( location );
        if( owner && &owner->hordes == this ) {
            // Register this accepted biological entity without constructing a
            // payload or running a range/discovery query during worldgen.
            auto &world = overmap_buffer.global_state.predator_activity;
            const std::string locator = "horde:" + p.to_string() + ":" + id.str();
            std::string actor = world.legacy_identity( locator, {} );
            if( actor.empty() ) {
                actor = world.legacy_identity( locator, random_string( 32 ) );
            }
            world.confirm( actor, id.str(), 0, predator_activation::representation::abstract,
                           p, to_turn<int>( calendar::turn ) );
        }
    }
    return result;
}

// Use the same movement/lifecycle admission as node reinsertion and reload.
std::optional<std::unordered_multimap<tripoint_abs_ms, horde_entity>::iterator> horde_map::spawn_entity(
    const tripoint_abs_ms &p, const monster &mon, bool allow_occupied )
{
    const horde_entity candidate( mon );
    std::unordered_map <tripoint_om_sm, std::unordered_multimap<tripoint_abs_ms, horde_entity>> &target_map =
                mon.type->has_flag( mon_flag_DORMANT ) ? dormant_monster_map :
                !is_alert( *mon.type ) && !migo_tower::owns_intent( mon,
                        migo_population::representation::abstract ) ? immobile_monster_map :
                candidate.is_active() ? active_monster_map :
                idle_monster_map;
    std::optional<std::unordered_multimap<tripoint_abs_ms, horde_entity>::iterator> result;
    point_abs_om omp;
    tripoint_om_sm sm;
    std::tie( omp, sm ) = project_remain<coords::om>( project_to<coords::sm>( p ) );
    // Do this before emplacing.  Returning an iterator to an already-owned
    // actor looks like a successful local->abstract handoff to callers and
    // can make them release the real local owner.
    if( !can_accept_entity( p, candidate, allow_occupied ) ) {
        DebugLog( D_WARNING, D_GAME ) << "Refused duplicate native actor ownership for " << mon.name();
        return result;
    }
    auto &bucket = target_map[sm];
    auto existing = bucket.find( p );
    const bool inserted = allow_occupied || existing == bucket.end();
    result = inserted ? bucket.emplace( p, mon ) : existing;
    if( inserted ) {
        ( *result )->second.monster_data->set_pos_abs_only( p );
    } else {
        debugmsg( "Attempted to insert a %s at %s, but there's already a %s there!",
                  mon.name(), p.to_string(), ( *result )->second.get_type()->nname() );
    }
    return inserted ? result : std::optional<std::unordered_multimap<tripoint_abs_ms,
           horde_entity>::iterator>();
}

bool horde_map::can_accept_entity( const tripoint_abs_ms &p, const horde_entity &entity,
                                   bool allow_occupied )
{
    const monster *payload = entity.monster_data.get();
    if( payload && payload->has_handoff_identity() ) {
        const std::string &actor_id = payload->handoff_actor_id();
        if( !actor_id.empty() ) {
            for( const std::pair<const tripoint_abs_ms, horde_entity> &existing : get_view(
                     horde_map_flavors::active |
                     horde_map_flavors::idle | horde_map_flavors::dormant | horde_map_flavors::immobile ) ) {
                if( existing.second.monster_data && existing.second.monster_data->has_handoff_identity() &&
                    existing.second.monster_data->handoff_actor_id() == actor_id ) {
                    return false;
                }
            }
        }
    }
    // Movement admits only an unoccupied position. A local handoff or reload
    // may retain distinct owners at the same key, but never a duplicate ID.
    point_abs_om ignored_om;
    tripoint_om_ms local;
    std::tie( ignored_om, local ) = project_remain<coords::om>( p );
    return allow_occupied || entity_at( local ) == nullptr;
}

static void signal_sm( const tripoint_abs_ms &origin, const tripoint_abs_sm &sm_dest,
                       const tripoint_abs_sm &sm_origin, int volume,
                       std::unordered_map <tripoint_om_sm, std::unordered_multimap<tripoint_abs_ms, horde_entity>>::iterator
                       &sm_iter, bool active, std::unordered_multimap<tripoint_abs_ms, horde_entity> &migrating_hordes )
{

    const int dist = rl_dist( sm_dest, sm_origin );
    int eff_power = volume - dist;
    if( eff_power <= 0 ) {
        return;
    }
    int scaled_eff_power = eff_power * SEEX;
    for( std::unordered_multimap<tripoint_abs_ms, horde_entity>::iterator mon = sm_iter->second.begin();
         mon != sm_iter->second.end(); ) {
        const auto &type = mon->second.get_type()->id.str();
        if( ( type == "mon_zombie_rider" || type == "mon_writhing_stalker" ) &&
            !overmap_buffer.predator_activity_at( mon->second, mon->first ) ) {
            ++mon;
            continue;
        }
        if( active && mon->second.tracking_intensity >= scaled_eff_power ) {
            ++mon;
            continue;
        }
        mon->second.destination = origin;
        mon->second.tracking_intensity = scaled_eff_power;
        if( mon->second.monster_data &&
            mon->second.monster_data->type->id.str() == "mon_writhing_stalker" ) {
            // Accepted sound owns the native carrier, even if its coordinate
            // happens to match an old pursuit/light hypothesis.
            mon->second.monster_data->wander_to( origin, scaled_eff_power );
            mon->second.monster_data->provocative_sound = true;
        }
        // Avoid unecessary extract/insert for already-active horde entities.
        if( !active ) {
            std::unordered_multimap<tripoint_abs_ms, horde_entity>::iterator moving_mon = mon;
            // Advance the loop iterator past the current node, which we will be removing.
            mon++;
            auto monster_node = sm_iter->second.extract( moving_mon );
            migrating_hordes.insert( std::move( monster_node ) );
        } else {
            ++mon;
        }
    }
}

// Volume is scaled down by SEEX so it matches the scale of tripoint_om_sm
// dormant_monster_map and immobile_monster_map are intentionally excluded here.
void horde_map::signal_entities( const tripoint_abs_ms &origin, int volume )
{
    std::unordered_multimap<tripoint_abs_ms, horde_entity> migrating_hordes;
    tripoint_abs_sm sm_dest = project_to<coords::sm>( origin );
    for( std::unordered_map
         <tripoint_om_sm, std::unordered_multimap<tripoint_abs_ms, horde_entity>>::iterator active_sm_iter =
             active_monster_map.begin(); active_sm_iter != active_monster_map.end(); ++active_sm_iter ) {
        tripoint_abs_sm abs_sm = project_combine( location, active_sm_iter->first );
        signal_sm( origin, sm_dest, abs_sm, volume, active_sm_iter, true, migrating_hordes );
    }
    for( std::unordered_map
         <tripoint_om_sm, std::unordered_multimap<tripoint_abs_ms, horde_entity>>::iterator idle_sm_iter =
             idle_monster_map.begin(); idle_sm_iter != idle_monster_map.end(); ) {
        tripoint_abs_sm abs_sm = project_combine( location, idle_sm_iter->first );
        signal_sm( origin, sm_dest, abs_sm, volume, idle_sm_iter, false, migrating_hordes );
        if( idle_sm_iter->second.empty() ) {
            idle_sm_iter = idle_monster_map.erase( idle_sm_iter );
        } else {
            ++idle_sm_iter;
        }
    }

    while( !migrating_hordes.empty() ) {
        auto monster_node = migrating_hordes.extract( migrating_hordes.begin() );
        insert( std::move( monster_node ) );
    }
}

int horde_map::attract_entities_to_light( const tripoint_abs_ms &origin, const int intensity,
        const int observer_radius_sm, const std::string &sample_id, const time_point &observed,
        const std::function<bool( const tripoint_abs_ms & )> &visible, int *candidate_count,
        const bool stagger )
{
    if( candidate_count != nullptr ) {
        *candidate_count = 0;
    }
    if( intensity <= 0 || observer_radius_sm <= 0 || sample_id.empty() ) {
        return 0;
    }
    int attracted = 0;
    static int next_light_scan_slot = 0;
    const int slots = to_turns<int>( 5_minutes );
    const int due_slot = to_turn<int>( observed ) % slots;
    std::unordered_multimap<tripoint_abs_ms, horde_entity> migrating_hordes;
    const tripoint_abs_sm source_sm = project_to<coords::sm>( origin );
    const auto consider = [this, &origin, &source_sm, intensity, observer_radius_sm, &sample_id,
          &observed, &visible, &attracted, &migrating_hordes, candidate_count, stagger,
          due_slot]( auto & outer ) {
        for( auto outer_it = outer.begin(); outer_it != outer.end(); ) {
            // The outer key is an existing spatial bucket.  Reject it before
            // touching its entities so source delivery is bounded by the
            // source-centred envelope rather than by every horde in a loaded
            // overmap.
            const tripoint_abs_sm bucket_sm = project_combine( location, outer_it->first );
            if( rl_dist( bucket_sm, source_sm ) > observer_radius_sm ) {
                ++outer_it;
                continue;
            }
            auto &bucket = outer_it->second;
            for( auto entity_it = bucket.begin(); entity_it != bucket.end(); ) {
                horde_entity &entity = entity_it->second;
                const auto &type = entity.get_type()->id.str();
                if( stagger && entity.get_type()->in_species( species_ZOMBIE ) &&
                    type != "mon_writhing_stalker" && type != "mon_zombie_rider" ) {
                    if( entity.light_scan_slot < 0 ) {
                        entity.light_scan_slot = next_light_scan_slot;
                        next_light_scan_slot = ( next_light_scan_slot + 1 ) % slots;
                    }
                    // Already noticed this source. Preserve its ordinary refresh
                    // and expiry behavior even between acquisition slots.
                    const bool tracking_source = !entity.light_sample_id.empty() &&
                                                 entity.destination == origin && entity.light_source == origin &&
                                                 observed <= entity.light_expires + 1_turns;
                    if( entity.light_scan_slot != due_slot && !tracking_source ) {
                        ++entity_it;
                        continue;
                    }
                }
                if( candidate_count != nullptr ) {
                    ++*candidate_count;
                }
                if( type == "mon_zombie_rider" || type == "mon_writhing_stalker" ) {
                    if( !overmap_buffer.predator_activity_at( entity, entity_it->first ) ) {
                        ++entity_it;
                        continue;
                    }
                    entity.ensure_predator_payload( entity_it->first );
                    if( !overmap_buffer.predator_activity_for( *entity.monster_data,
                            predator_activation::representation::abstract, entity_it->first ) ) {
                        ++entity_it;
                        continue;
                    }
                }
                if( entity.light_sample_id == sample_id || !entity.can_perceive_light() ) {
                    ++entity_it;
                    continue;
                }
                const int distance = rl_dist( project_to<coords::sm>( entity_it->first ), source_sm );
                if( distance > observer_radius_sm ) {
                    ++entity_it;
                    continue;
                }
                const int remaining_interest = intensity - distance;
                if( remaining_interest <= entity.tracking_intensity ) {
                    ++entity_it;
                    continue;
                }
                if( visible && !visible( entity_it->first ) ) {
                    ++entity_it;
                    continue;
                }
                if( !entity.accept_light_interest( origin, sample_id, to_turn<int>( observed ),
                                                  intensity ) ) {
                    ++entity_it;
                    continue;
                }
                if( entity.monster_data && entity.monster_data->is_caol_predator() ) {
                    overmap_buffer.retain_predator_light_work( *entity.monster_data, sample_id,
                            to_turn<int>( observed ), intensity );
                }
                entity.destination = origin;
                entity.tracking_intensity = remaining_interest;
                entity.light_source = origin;
                entity.light_sample_id = sample_id;
                entity.light_observed = observed;
                entity.light_expires = observed + intensity * 1_turns;
                entity.light_interest_strength = remaining_interest;
                if( entity.monster_data &&
                    entity.monster_data->type->id.str() == "mon_writhing_stalker" ) {
                    entity.monster_data->wander_to( origin, remaining_interest );
                    entity.monster_data->provocative_sound = false;
                }
                auto moving = entity_it++;
                auto node = bucket.extract( moving );
                migrating_hordes.insert( std::move( node ) );
                attracted++;
            }
            if( bucket.empty() ) {
                outer_it = outer.erase( outer_it );
            } else {
                ++outer_it;
            }
        }
    };
    // Active entities may revise a weaker destination; idle entities become
    // active. Dormant and immobile entries remain outside ordinary light AI.
    consider( active_monster_map );
    consider( idle_monster_map );
    while( !migrating_hordes.empty() ) {
        auto node = migrating_hordes.extract( migrating_hordes.begin() );
        insert( std::move( node ) );
    }
    return attracted;
}

void horde_map::expire_light_interest( const time_point &now )
{
    std::unordered_multimap<tripoint_abs_ms, horde_entity> migrating_hordes;
    for( auto outer_it = active_monster_map.begin(); outer_it != active_monster_map.end(); ) {
        auto &bucket = outer_it->second;
        for( auto entity_it = bucket.begin(); entity_it != bucket.end(); ) {
            horde_entity &entity = entity_it->second;
            light_interest_expiry expiry = light_interest_expiry::native_world;
            const auto &type = entity.get_type()->id.str();
            if( type == "mon_zombie_rider" || type == "mon_writhing_stalker" ) {
                const bool eligible = overmap_buffer.predator_activity_at( entity, entity_it->first );
                if( eligible ) {
                    entity.ensure_predator_payload( entity_it->first );
                    monster &body = *entity.monster_data;
                    if( body.predator_state().light_work_sample.empty() && !entity.light_sample_id.empty() ) {
                        // Project a legacy carrier before first body binding
                        // settles its frozen interval and rebases this anchor.
                        overmap_buffer.retain_predator_light_work( body, entity.light_sample_id,
                                to_turn<int>( entity.light_observed ),
                                to_turns<int>( entity.light_expires - entity.light_observed ) );
                    }
                    overmap_buffer.predator_activity_for( body,
                            predator_activation::representation::abstract, entity_it->first );
                }
                if( entity.monster_data ) {
                    monster &body = *entity.monster_data;
                    if( overmap_buffer.predator_light_work_alive( body ) ) {
                        expiry = light_interest_expiry::retain_actor_work;
                    } else if( !body.predator_state().light_work_sample.empty() ) {
                        expiry = light_interest_expiry::exhaust_actor_work;
                    }
                }
                if( !eligible && expiry == light_interest_expiry::native_world ) {
                    // No payload construction or observer read just to decide
                    // range. Authentication/freeze does not erase saved work.
                    expiry = light_interest_expiry::retain_actor_work;
                }
            }
            entity.expire_light_interest( now, expiry );
            if( entity_it->second.is_active() ) {
                ++entity_it;
                continue;
            }
            auto moving = entity_it++;
            auto node = bucket.extract( moving );
            migrating_hordes.insert( std::move( node ) );
        }
        if( bucket.empty() ) {
            outer_it = active_monster_map.erase( outer_it );
        } else {
            ++outer_it;
        }
    }
    while( !migrating_hordes.empty() ) {
        auto node = migrating_hordes.extract( migrating_hordes.begin() );
        insert( std::move( node ) );
    }
}

void horde_map::insert( std::unordered_multimap<tripoint_abs_ms, horde_entity>::node_type &&node )
{
    std::unordered_map <tripoint_om_sm, std::unordered_multimap<tripoint_abs_ms, horde_entity>> &target_map =
                node.mapped().get_type()->has_flag( mon_flag_DORMANT ) ? dormant_monster_map :
                node.mapped().is_active() ? active_monster_map :
                is_alert( *node.mapped().get_type() ) ? idle_monster_map :
                immobile_monster_map;
    point_abs_om omp;
    tripoint_om_sm sm;
    std::tie( omp, sm ) = project_remain<coords::om>( project_to<coords::sm> ( node.key() ) );
    // The [] operator creates a nested std::map if not present already.
    target_map[sm].insert( std::move( node ) );
}

void horde_map::refresh_entity_bucket( const tripoint_abs_ms &p )
{
    point_abs_om ignored_om;
    tripoint_om_ms local;
    std::tie( ignored_om, local ) = project_remain<coords::om>( p );
    for( horde_entity *entity : entities_at( local ) ) {
        refresh_entity_bucket( p, entity );
    }
}

void horde_map::refresh_entity_bucket( const tripoint_abs_ms &p, horde_entity *entity )
{
    point_abs_om ignored;
    tripoint_om_ms local;
    std::tie( ignored, local ) = project_remain<coords::om>( p );
    iterator existing = find( local, entity );
    if( existing != end() ) {
        insert( extract( existing ) );
    }
}

void horde_map::clear()
{
    active_monster_map.clear();
    idle_monster_map.clear();
    dormant_monster_map.clear();
    immobile_monster_map.clear();
}

void horde_map::clear_chunk( const tripoint_om_sm &p )
{
    active_monster_map.erase( p );
    idle_monster_map.erase( p );
    dormant_monster_map.erase( p );
    immobile_monster_map.erase( p );
}

// horde_map::iterator definitions

void horde_map::iterator::next_map()
{
    if( outer_map == &parent->active_monster_map ) {
        if( filter & horde_map_flavors::idle ) {
            outer_map = &parent->idle_monster_map;
        } else if( filter & horde_map_flavors::dormant ) {
            outer_map = &parent->dormant_monster_map;
        } else if( filter & horde_map_flavors::immobile ) {
            outer_map = &parent->immobile_monster_map;
        } else {
            outer_map = nullptr;
        }
    } else if( outer_map == &parent->idle_monster_map ) {
        if( filter & horde_map_flavors::dormant ) {
            outer_map = &parent->dormant_monster_map;
        } else if( filter & horde_map_flavors::immobile ) {
            outer_map = &parent->immobile_monster_map;
        } else {
            outer_map = nullptr;
        }
    } else if( outer_map == &parent->dormant_monster_map ) {
        if( filter & horde_map_flavors::immobile ) {
            outer_map = &parent->immobile_monster_map;
        } else {
            outer_map = nullptr;
        }
    } else {
        // At the end.
        outer_map = nullptr;
    }
}

void horde_map::iterator::skip_empty_buckets()
{
    while( outer_map != nullptr ) {
        while( outer_iter != outer_map->end() && outer_iter->second.empty() ) {
            ++outer_iter;
        }
        if( outer_iter != outer_map->end() ) {
            inner_iter = outer_iter->second.begin();
            return;
        }
        next_map();
        if( outer_map != nullptr ) {
            outer_iter = outer_map->begin();
        }
    }
}

// This helper insures that a freshly created iterator is in a valid state.
void horde_map::iterator::insure_valid()
{
    if( !( filter & horde_map_flavors::active ) ) {
        next_map();
        if( outer_map == nullptr ) {
            return;
        }
        outer_iter = outer_map->begin();
    }
    skip_empty_buckets();
}

horde_map::iterator &horde_map::iterator::operator++()
{
    if( outer_map == nullptr ) {
        return *this;
    }
    if( inner_iter != outer_iter->second.end() ) {
        ++inner_iter;
    }
    if( inner_iter == outer_iter->second.end() ) {
        ++outer_iter;
        skip_empty_buckets();
    }
    return *this;
}

horde_map::iterator horde_map::iterator::operator++( int )
{
    iterator retval = *this;
    ++( *this );
    return retval;
}

bool horde_map::iterator::operator==( iterator other ) const
{
    return ( outer_map == nullptr && other.outer_map == nullptr ) ||
           ( outer_map == other.outer_map && outer_iter == other.outer_iter &&
             inner_iter == other.inner_iter );
}

bool horde_map::iterator::operator!=( iterator other ) const
{
    return !( *this == other );
}

horde_map::iterator::reference horde_map::iterator::operator*() const
{
    return *inner_iter;
}

horde_map::iterator::pointer horde_map::iterator::operator->() const
{
    return &*inner_iter;
}

horde_map::iterator horde_map::find( const tripoint_om_ms &loc )
{
    const tripoint_om_sm submap_loc = project_to<coords::sm>( loc );
    const tripoint_abs_ms monster_loc = project_combine( location, loc );
    // Match entity_at and the ordinary all-flavor view. Population handoff
    // rollback also needs to find Mi-go retained in the immobile bucket.
    for( map_type *table : { &active_monster_map, &idle_monster_map,
                           &dormant_monster_map, &immobile_monster_map } ) {
        const auto submap_iter = table->find( submap_loc );
        if( submap_iter == table->end() ) {
            continue;
        }
        const auto mon_iter = submap_iter->second.find( monster_loc );
        if( mon_iter != submap_iter->second.end() ) {
            return iterator( *this, *table, submap_iter, mon_iter );
        }
    }
    return end();
}

horde_map::iterator horde_map::find( const tripoint_om_ms &loc, const horde_entity *entity )
{
    const auto submap_loc = project_to<coords::sm>( loc );
    const auto key = project_combine( location, loc );
    for( map_type *table : { &active_monster_map, &idle_monster_map,
                           &dormant_monster_map, &immobile_monster_map } ) {
        const auto outer = table->find( submap_loc );
        if( outer == table->end() ) {
            continue;
        }
        const auto range = outer->second.equal_range( key );
        for( auto it = range.first; it != range.second; ++it ) {
            if( &it->second == entity ) {
                return iterator( *this, *table, outer, it );
            }
        }
    }
    return end();
}

horde_map::iterator horde_map::erase( iterator iter )
{
    iterator old_iter = iter;
    iter.inner_iter =
        iter.outer_iter->second.erase( iter.inner_iter );
    if( iter.inner_iter == iter.outer_iter->second.end() ) {
        ++iter;
        if( old_iter.outer_iter->second.empty() ) {
            old_iter.outer_map->erase( old_iter.outer_iter );
        }
    }
    return iter;
}

horde_map::node_type horde_map::extract( iterator iter )
{
    node_type node = iter.outer_iter->second.extract( iter.inner_iter );
    if( iter.outer_iter->second.empty() ) {
        iter.outer_map->erase( iter.outer_iter );
    }
    return node;
}
