#include "harness_creature_debug.h"

#include <sstream>

#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "character.h"
#include "creature.h"
#include "game.h"
#include "json.h"
#include "map.h"
#include "map_scale_constants.h"
#include "messages.h"
#include "monster.h"
#include "npc.h"
#include "string_formatter.h"
#include "translations.h"

namespace
{
std::string last_intervention;
std::string last_intervention_run;
} // namespace

std::string harness_last_debug_creature_intervention( const std::string &run_id )
{
    return last_intervention_run == run_id && !run_id.empty() ? last_intervention : "none_this_run";
}

std::string harness_creature_identity( const Creature &creature )
{
    if( const Character *actor = creature.as_character() ) {
        return string_format( "character:%d", actor->getID().get_value() );
    }
    if( creature.as_monster() ) {
        std::ostringstream identity;
        identity << "process:" << static_cast<const void *>( &creature );
        return identity.str();
    }
    return {};
}

std::vector<semantic_action_descriptor> harness_creature_debug_actions( avatar &viewer )
{
    std::vector<semantic_action_descriptor> result;
    for( Creature *creature : viewer.get_visible_creatures( SEEX ) ) {
        if( creature == nullptr || creature == &viewer || creature->is_dead_state() ) {
            continue;
        }
        const std::string identity = harness_creature_identity( *creature );
        if( !identity.empty() ) {
            const tripoint_rel_ms offset = creature->pos_abs() - viewer.pos_abs();
            result.push_back( { "world.debug_kill_creature", identity,
                                string_format( _( "Debug kill %s at (%d,%d,%d) - setup intervention" ),
                                               creature->disp_name(), offset.x(), offset.y(), offset.z() ), true } );
        }
    }
    return result;
}

std::string harness_debug_kill_creature( avatar &viewer, const std::string &identity,
        const std::string &request_id, const std::string &run_id )
{
    Creature *target = nullptr;
    for( Creature *creature : viewer.get_visible_creatures( SEEX ) ) {
        if( creature != nullptr && creature != &viewer && !creature->is_dead_state() &&
            !identity.empty() && harness_creature_identity( *creature ) == identity ) {
            target = creature;
            break;
        }
    }
    if( target == nullptr ) {
        return {};
    }
    const std::string name = target->disp_name();
    const tripoint_abs_ms position = target->pos_abs();
    std::ostringstream buffer;
    JsonOut out( buffer );
    out.start_object();
    out.member( "schema", "caol-debug-creature-intervention-v1" );
    out.member( "request_id", request_id );
    out.member( "run_id", run_id );
    out.member( "operation", "native_debug_kill" );
    out.member( "gameplay_credit", false );
    out.member( "provenance",
                "Explicit player-selected debug intervention; native death effects may occur." );
    out.member( "observed_turn", to_turns<int>( calendar::turn - calendar::turn_zero ) );
    out.member( "identity", identity );
    out.member( "name", name );
    out.member( "absolute_ms", std::vector<int> { position.x(), position.y(), position.z() } );
    out.member( "before" );
    out.start_object();
    out.member( "hp", target->get_hp() );
    out.member( "dead", target->is_dead_state() );
    out.end_object();
    if( monster *actor = target->as_monster() ) {
        actor->set_hp( 0 );
    } else if( npc *actor = target->as_npc() ) {
        actor->set_part_hp_cur( bodypart_id( "head" ), 0 );
    } else {
        return {};
    }
    target->die( &get_map(), nullptr );
    out.member( "after" );
    out.start_object();
    out.member( "hp", target->get_hp() );
    out.member( "dead", target->is_dead_state() );
    out.end_object();
    out.end_object();
    g->cleanup_dead();
    add_msg( m_info, _( "Debug killed %s (setup intervention)." ), name );
    last_intervention = buffer.str();
    last_intervention_run = run_id;
    return last_intervention;
}
