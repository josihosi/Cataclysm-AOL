#include "horde_entity.h"

#include "mtype.h"
#include "monster.h"
#include "point.h"

namespace {

bool is_predator_evolution_type( const mtype_id &type )
{
    // This adapter is deliberately narrower than monster::try_upgrade.  Most
    // horde types are upgradeable, but must remain lightweight until they are
    // actually spawned; only this lifecycle owns abstract evolution here.
    static const mtype_id mon_zombie_hunter( "mon_zombie_hunter" );
    static const mtype_id mon_zombie_predator( "mon_zombie_predator" );
    return type == mon_zombie_hunter || type == mon_zombie_predator;
}

} // namespace

horde_entity::horde_entity( const monster &original )
{
    if( original.has_dest() ) {
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
    light_sample_id( other.light_sample_id ), light_observed( other.light_observed ),
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
    return tracking_intensity > 0;
}

bool horde_entity::can_perceive_light() const
{
    if( monster_data ) {
        return !monster_data->is_dead() && monster_data->friendly == 0 && monster_data->can_see();
    }
    const mtype *type = get_type();
    return type != nullptr && type->has_flag( mon_flag_SEES );
}

void horde_entity::expire_light_interest( const time_point &now )
{
    if( light_expires == calendar::turn_zero || now < light_expires || destination != light_source ||
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
    monster_data->predator_state();
}

void horde_entity::synchronize_payload( const tripoint_abs_ms &position )
{
    ensure_predator_payload( position );
    if( !monster_data ) {
        return;
    }
    monster_data->set_pos_abs_only( position );
    monster_data->set_moves( moves );
    if( tracking_intensity > 0 ) {
        monster_data->wander_to( destination, tracking_intensity );
    }
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

bool horde_entity::advance_predator_intent( const tripoint_abs_ms &position, const int now_turn )
{
    ensure_predator_payload( position );
    if( !monster_data || !monster_data->is_caol_predator() ) {
        return false;
    }
    predator_lifecycle_state &life = monster_data->predator_state();
    if( life.last_advanced_turn == now_turn ) {
        return true;
    }
    life.last_advanced_turn = now_turn;
    if( monster_data->type->id.str() == "mon_writhing_stalker" ) {
        writhing_stalker::persistent_state &state = monster_data->writhing_stalker_state();
        state.advance_to( now_turn );
        if( !light_sample_id.empty() && light_expires > light_observed ) {
            state.observe_light_interest( light_source, light_sample_id,
                                          to_turn<int>( light_observed ),
                                          std::max( 1, to_turns<int>( light_expires - light_observed ) ) );
        }
        if( state.has_committed_waypoint ) {
            destination = state.committed_waypoint;
            tracking_intensity = std::max( tracking_intensity, 1 );
        }
    } else {
        zombie_rider_overmap_ai::rider_pursuit_state &state =
            monster_data->zombie_rider_pursuit_state();
        state.advance_to( now_turn );
        // Light is an investigation waypoint, never fake direct prey
        // recognition.  Real local observations remain the only way to set
        // target_identity.
        if( !state.evidence_valid( now_turn ) && !light_sample_id.empty() &&
            light_expires > calendar::turn ) {
            state.phase = zombie_rider_overmap_ai::pursuit_phase::searching;
            state.movement_waypoint = light_source;
            state.has_movement_waypoint = true;
            state.search_until_turn = to_turn<int>( light_expires );
        }
        if( state.has_movement_waypoint ) {
            destination = state.movement_waypoint;
            tracking_intensity = std::max( tracking_intensity, 1 );
        }
    }
    synchronize_payload( position );
    return true;
}
