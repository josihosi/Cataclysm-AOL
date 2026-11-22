#include "horde_entity.h"

#include <algorithm>
#include <limits>

#include "mtype.h"
#include "monster.h"
#include "migo_tower_ai.h"
#include "migo_tower_native.h"
#include "point.h"
#include "overmapbuffer.h"

namespace
{

bool is_predator_evolution_type( const mtype_id &type )
{
    // This adapter is deliberately narrower than monster::try_upgrade.  Most
    // horde types are upgradeable, but must remain lightweight until they are
    // actually spawned; only this lifecycle owns abstract evolution here.
    static const mtype_id mon_zombie_hunter( "mon_zombie_hunter" );
    static const mtype_id mon_zombie_predator( "mon_zombie_predator" );
    return type == mon_zombie_hunter || type == mon_zombie_predator;
}

bool has_live_sound( const monster &payload )
{
    return payload.provocative_sound && payload.wandf > 0;
}

bool sound_owns_destination( const monster &payload, const tripoint_abs_ms &destination )
{
    return has_live_sound( payload ) && payload.wander_pos == destination;
}

} // namespace

horde_entity::horde_entity( const monster &original )
{
    if( original.migo_intent() && original.migo_intent()->active() &&
        !original.population_identity().empty() ) {
        // A mission/combat destination is not a heard source or a fresh
        // thousand-turn wander budget. Keep independent native sound intact.
        destination = original.has_dest() ? original.get_dest() : original.pos_abs();
        if( original.provocative_sound && original.wandf > 0 ) {
            destination = original.wander_pos;
            tracking_intensity = original.wandf;
        }
    } else if( original.has_dest() ) {
        destination = original.get_dest();
        // TODO figure out a value? go here no matter what?
        tracking_intensity = 1000;
    } else if( original.wandf > 0 ) {
        destination = original.wander_pos;
        tracking_intensity = original.wandf;
    }
    moves = original.get_moves();
    type_id = original.type->id.id();
    monster_data = std::make_unique<monster>( original );
}

horde_entity::horde_entity( const mtype_id &original )
{
    type_id = original.id();
}

horde_entity::horde_entity( const horde_entity &other ) :
    destination( other.destination ), tracking_intensity( other.tracking_intensity ),
    last_processed( other.last_processed ), moves( other.moves ), light_source( other.light_source ),
    light_scan_slot( other.light_scan_slot ), light_sample_id( other.light_sample_id ),
    light_observed( other.light_observed ),
    light_expires( other.light_expires ), light_interest_strength( other.light_interest_strength ),
    type_id( other.type_id )
{
    if( other.monster_data ) {
        monster_data = std::make_unique<monster>( *other.monster_data );
    }
}

horde_entity &horde_entity::operator=( const horde_entity &other )
{
    if( this == &other ) {
        return *this;
    }
    destination = other.destination;
    tracking_intensity = other.tracking_intensity;
    last_processed = other.last_processed;
    moves = other.moves;
    light_source = other.light_source;
    light_sample_id = other.light_sample_id;
    light_observed = other.light_observed;
    light_expires = other.light_expires;
    light_interest_strength = other.light_interest_strength;
    light_scan_slot = other.light_scan_slot;
    type_id = other.type_id;
    monster_data = other.monster_data ? std::make_unique<monster>( *other.monster_data ) : nullptr;
    return *this;
}

const mtype *horde_entity::get_type() const
{
    return type_id ? &type_id.obj() : monster_data->type;
}

bool horde_entity::is_active() const
{
    if( monster_data && migo_tower::owns_intent( *monster_data,
            migo_population::representation::abstract ) ) {
        return true;
    }
    if( tracking_intensity > 0 ) {
        return true;
    }
    if( !monster_data || monster_data->type->id.str() != "mon_writhing_stalker" ) {
        return false;
    }
    const monster &payload = *monster_data;
    const auto &state = payload.writhing_stalker_state();
    // A finite light investigation/search also needs arrival and expiry
    // service when this actor has never acquired a character. Source expiry
    // does not erase its accepted location job. Keep even an overdue job
    // scheduled so the guarded native owner can retire it, without renewing
    // the source or its allowance here.
    const bool retained_light_job = !state.light_sample_id.empty() &&
                                    state.light_sample_id == payload.predator_state().light_work_sample;
    if( ( ( state.has_light_observed_position || retained_light_job ) && state.has_committed_waypoint &&
          ( state.phase == writhing_stalker::lifecycle_phase::shadowing ||
            state.phase == writhing_stalker::lifecycle_phase::approaching ) ) ||
        ( state.phase == writhing_stalker::lifecycle_phase::searching &&
          state.search_until_turn >= 0 ) ) {
        return true;
    }
    // Arrival and a pending familiar-place visit still need lifecycle turns.
    // Scheduling them does not create a movement or sound-tracking budget.
    if( !state.has_pursuit_memory ) {
        return false;
    }
    return state.sighting_active_turn >= 0 || state.search_started_active >= 0 ||
    std::any_of( state.familiar.begin(), state.familiar.end(), []( const auto &place ) {
        return place.valid;
    } );
}

bool horde_entity::can_perceive_light() const
{
    if( monster_data ) {
        return !monster_data->is_dead() && monster_data->friendly == 0 && monster_data->can_see();
    }
    const mtype *type = get_type();
    return type != nullptr && type->has_flag( mon_flag_SEES );
}

void horde_entity::expire_light_interest( const time_point &now, light_interest_expiry expiry )
{
    if( expiry == light_interest_expiry::retain_actor_work ||
        ( expiry == light_interest_expiry::native_world &&
          ( light_expires == calendar::turn_zero || now < light_expires ) ) || destination != light_source ||
        tracking_intensity > light_interest_strength ) {
        return;
    }
    tracking_intensity = 0;
    light_interest_strength = 0;
}

void horde_entity::ensure_predator_payload( const tripoint_abs_ms &position )
{
    const mtype *type = get_type();
    if( monster_data || type == nullptr ||
        ( type->id.str() != "mon_writhing_stalker" && type->id.str() != "mon_zombie_rider" ) ) {
        return;
    }
    monster_data = std::make_unique<monster>( type->id );
    monster_data->set_pos_abs_only( position );
    monster_data->set_moves( moves );
    overmap_buffer.adopt_legacy_predator_identity( *monster_data,
            "horde:" + position.to_string() + ":" + type->id.str() );
    monster_data->predator_state();
}

void horde_entity::synchronize_payload( const tripoint_abs_ms &position )
{
    ensure_predator_payload( position );
    if( !monster_data ) {
        return;
    }
    if( monster_data->type->id.str() == "mon_writhing_stalker" &&
        monster_data->pos_abs() != position && has_live_sound( *monster_data ) &&
        !sound_owns_destination( *monster_data, destination ) ) {
        // A committed route step spends the same pending native budget as
        // monster::move. Same-position handoff/repeated sync spends nothing.
        --monster_data->wandf;
    }
    monster_data->set_pos_abs_only( position );
    monster_data->set_moves( moves );
    synchronize_navigation( *monster_data );
}

void horde_entity::synchronize_navigation( monster &payload ) const
{
    if( migo_tower::owns_intent( payload, migo_population::representation::local ) ||
        migo_tower::owns_intent( payload, migo_population::representation::abstract ) ) {
        if( payload.migo_intent()->waypoint ) {
            payload.set_dest( *payload.migo_intent()->waypoint );
        }
        return;
    }
    if( payload.type->id.str() == "mon_writhing_stalker" &&
        writhing_stalker::retire_arrived_sound( payload, destination, payload.pos_abs() ) ) {
        return;
    }
    if( tracking_intensity > 0 ) {
        const bool stalker = payload.type->id.str() == "mon_writhing_stalker";
        const bool heard_here = sound_owns_destination( payload, destination );
        if( stalker && has_live_sound( payload ) && !heard_here ) {
            // The current route may have priority, but a genuine pending
            // sound keeps its separate native source/budget through handoff.
            payload.set_dest( destination );
        } else {
            payload.wander_to( destination, tracking_intensity );
            if( stalker ) {
                // Route transport cannot turn an expired provocative flag
                // into new sound evidence, including before first acquisition.
                payload.provocative_sound = heard_here;
            }
        }
    }
}

bool horde_entity::accept_light_interest( const tripoint_abs_ms &position,
        const std::string &sample_id, const int observed_turn, const int duration_turns )
{
    if( !monster_data || monster_data->type->id.str() != "mon_writhing_stalker" ) {
        return true;
    }
    // Admission must precede carrier writes. Light cannot displace a native
    // sound pending beneath another route, or bypass combat/recovery refusal.
    return !has_live_sound( *monster_data ) &&
           monster_data->writhing_stalker_state().observe_light_interest(
               position, sample_id, observed_turn, duration_turns );
}

bool horde_entity::advance_evolution( const tripoint_abs_ms &position )
{
    if( !monster_data ) {
        const mtype *type = get_type();
        if( type == nullptr || !type->upgrades || !is_predator_evolution_type( type->id ) ) {
            return false;
        }
        monster_data = std::make_unique<monster>( type->id );
        monster_data->set_pos_abs_only( position );
        monster_data->set_moves( moves );
    }
    monster_data->set_pos_abs_only( position );
    monster_data->try_upgrade( false );
    type_id = monster_data->type->id.id();
    return true;
}

bool horde_entity::advance_predator_intent( const tripoint_abs_ms &position, const int now_turn,
        const int active_elapsed )
{
    ensure_predator_payload( position );
    if( !monster_data || !monster_data->is_caol_predator() ) {
        return false;
    }
    predator_lifecycle_state &life = monster_data->predator_state();
    const bool stalker = monster_data->type->id.str() == "mon_writhing_stalker";
    if( life.last_advanced_turn == now_turn ||
        ( stalker && ( life.last_advanced_turn > now_turn ||
                       monster_data->writhing_stalker_state().last_advanced_turn > now_turn ) ) ) {
        return true;
    }
    // A newly authenticated interval earns no historical work. It still has
    // a current native opportunity to observe arrival or retire a sound,
    // but a frozen or unauthenticated body must not consume that opportunity.
    if( active_elapsed == 0 && stalker &&
        ( now_turn != to_turn<int>( calendar::turn ) ||
          !overmap_buffer.predator_activity_for( *monster_data,
                  predator_activation::representation::abstract, position ) ) ) {
        return true;
    }
    life.last_advanced_turn = now_turn;
    if( active_elapsed == 0 && !stalker ) {
        return true;
    }
    if( stalker ) {
        writhing_stalker::persistent_state &state = monster_data->writhing_stalker_state();
        const bool carried_route = state.owns_waypoint( destination ) &&
                                   !sound_owns_destination( *monster_data, destination );
        writhing_stalker::advance_active_work( *monster_data, now_turn );
        // Also settle an arrived carrier restored from an older payload
        // before a new speculative waypoint can replace its destination.
        if( writhing_stalker::retire_arrived_sound( *monster_data, destination, position ) ) {
            tracking_intensity = 0;
        }
        monster_data->set_pos_abs_only( position );
        const bool saw_target = ( has_live_sound( *monster_data ) ||
                                  writhing_stalker::target_observation_due( *monster_data, true ) ) &&
                                writhing_stalker::observe_current_target( *monster_data ) != nullptr;
        if( !has_live_sound( *monster_data ) && destination == light_source &&
            !light_sample_id.empty() && to_turn<int>( light_observed ) <= now_turn &&
            to_turn<int>( light_expires ) > now_turn ) {
            state.observe_light_interest( light_source, light_sample_id,
                                          to_turn<int>( light_observed ),
                                          to_turns<int>( light_expires - light_observed ) );
        }
        state.arrive_at( position );
        const bool sound_owns_navigation = has_live_sound( *monster_data ) &&
                                           ( carried_route || sound_owns_destination( *monster_data, destination ) ) &&
                                           monster_data->wander_pos != position && !saw_target &&
                                           state.phase != writhing_stalker::lifecycle_phase::retreating &&
                                           state.phase != writhing_stalker::lifecycle_phase::cooldown;
        if( sound_owns_navigation && !sound_owns_destination( *monster_data, destination ) ) {
            // Select a newly pending sound once. An already selected sound
            // keeps its native remaining budget instead of renewing it.
            destination = monster_data->wander_pos;
            tracking_intensity = monster_data->wandf;
        }
        // Abstract transport executes its own native one-tile steps. A local
        // intermediate light step must not replace the observed source goal.
        const bool retained_light_location = overmap_buffer.predator_light_work_alive( *monster_data ) &&
                                             !state.light_sample_id.empty() && life.light_work_sample == state.light_sample_id &&
                                             state.light_observed_turn >= 0 && state.light_observed_turn <= now_turn;
        if( state.sighting_active_turn < 0 && state.search_started_active < 0 &&
            ( state.light_interest_active( now_turn ) || retained_light_location ) &&
            ( state.phase == writhing_stalker::lifecycle_phase::shadowing ||
              state.phase == writhing_stalker::lifecycle_phase::approaching ) ) {
            state.committed_waypoint = state.light_observed_position;
            state.has_committed_waypoint = true;
        }
        if( state.has_pursuit_memory && ( state.sighting_active_turn >= 0 ||
                                        state.phase == writhing_stalker::lifecycle_phase::idle ||
                                        state.revisiting_slot >= 0 ) &&
            !( sound_owns_navigation && state.phase == writhing_stalker::lifecycle_phase::idle ) ) {
            writhing_stalker::pursuit_destination( *monster_data, true );
        }
        if( sound_owns_navigation ) {
            // Accepted sound outranks speculative light/memory navigation.
            // Due visits wait; active searches keep their finite elapsed budget.
            // Actual sight and native retreat retain their higher authority.
            monster_data->unset_dest();
            return true;
        }
        if( state.phase == writhing_stalker::lifecycle_phase::retreating ||
            state.phase == writhing_stalker::lifecycle_phase::cooldown ) {
            // The local caller handed off its actual retreat destination.
            // A retained approach hypothesis cannot supersede that authority.
            if( state.has_retreat_waypoint ) {
                destination = state.retreat_waypoint;
                tracking_intensity = position == destination ? 0 : std::max( tracking_intensity, 1 );
            }
        } else if( state.has_committed_waypoint ) {
            destination = state.committed_waypoint;
            tracking_intensity = position == destination ? 0 : std::max( tracking_intensity, 1 );
        } else if( carried_route ) {
            tracking_intensity = 0;
        }
        if( tracking_intensity == 0 && carried_route ) {
            monster_data->unset_dest();
            // Retire the completed route, not an independent native sound
            // waiting beneath its retreat/recovery authority.
            if( !has_live_sound( *monster_data ) ) {
                monster_data->wander_to( position, 0 );
                monster_data->provocative_sound = false;
            }
        }
    } else {
        zombie_rider_overmap_ai::rider_pursuit_state &state =
            monster_data->zombie_rider_pursuit_state();
        state.advance_to( now_turn );
        // Light is an investigation waypoint, never fake direct prey
        // recognition.  Real local observations remain the only way to set
        // target_identity.
        if( !state.evidence_valid( now_turn ) && !light_sample_id.empty() &&
            overmap_buffer.predator_light_work_alive( *monster_data ) ) {
            state.phase = zombie_rider_overmap_ai::pursuit_phase::searching;
            state.movement_waypoint = light_source;
            state.has_movement_waypoint = true;
            const auto remaining = life.light_work_until_active -
                                   overmap_buffer.predator_active_time( *monster_data );
            state.search_until_turn = static_cast<int>( std::min<std::int64_t>(
                                          std::numeric_limits<int>::max(), now_turn + remaining ) );
        }
        if( state.has_movement_waypoint ) {
            destination = state.movement_waypoint;
            tracking_intensity = std::max( tracking_intensity, 1 );
        }
    }
    synchronize_payload( position );
    return true;
}
