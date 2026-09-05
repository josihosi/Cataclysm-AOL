#include "game.h" // IWYU pragma: associated

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "action.h"
#include "activity_actor_definitions.h"
#include "advanced_inv.h"
#include "auto_note.h"
#include "auto_pickup.h"
#include "avatar.h"
#include "avatar_action.h"
#include "bionics.h"
#include "bodygraph.h"
#include "bodypart.h"
#include "basecamp.h"
#include "cached_options.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character.h"
#include "character_attire.h"
#include "character_martial_arts.h"
#include "clzones.h"
#include "color.h"
#include "construction.h"
#include "creature_tracker.h"
#include "cursesdef.h"
#include "damage.h"
#include "debug.h"
#include "debug_menu.h"
#include "diary.h"
#include "distraction_manager.h"
#include "do_turn.h"
#include "event.h"
#include "event_bus.h"
#include "faction.h"
#include "faction_camp.h"
#include "field.h"
#include "field_type.h"
#include "flag.h"
#include "game_constants.h"
#include "game_inventory.h"
#include "gamemode.h"
#include "gates.h"
#include "gun_mode.h"
#include "help.h"
#include "input_context.h"
#include "input_enums.h"
#include "input_popup.h"
#include "inventory_ui.h"
#include "item.h"
#include "item_group.h"
#include "itype.h"
#include "iuse.h"
#include "level_cache.h"
#include "live_view.h"
#include "magic.h"
#include "magic_enchantment.h"
#include "magic_type.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "mapsharing.h"
#include "math_parser_diag_value.h"
#include "mdarray.h"
#include "messages.h"
#include "monster.h"
#include "move_mode.h"
#include "mtype.h"
#include "mutation.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmapbuffer.h"
#include "overmap_ui.h"
#include "panels.h"
#include "pathfinding.h"
#include "point.h"
#include "popup.h"
#include "ranged.h"
#include "rng.h"
#include "safemode_ui.h"
#include "semantic_surface.h"
#if defined(TILES)
#include "sdl_gamepad.h"
#endif
#include "sleep.h"
#include "sounds.h"
#include "string_formatter.h"
#include "timed_event.h"
#include "translation.h"
#include "translations.h"
#include "ui_manager.h"
#include "uilist.h"
#include "uistate.h"
#include "units.h"
#include "value_ptr.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vpart_position.h"
#include "vpart_range.h"
#include "weather.h"
#include "weather_type.h"
#include "worldfactory.h"

enum class direction : unsigned int;

#if defined(TILES)
#include "cata_tiles.h" // all animation functions will be pushed out to a cata_tiles function in some manner
#include "sdltiles.h"
#endif

static const bionic_id bio_remote( "bio_remote" );

static const character_modifier_id
character_modifier_move_mode_move_cost_mod( "move_mode_move_cost_mod" );

static const damage_type_id damage_cut( "cut" );

static const efftype_id effect_alarm_clock( "alarm_clock" );
static const efftype_id effect_incorporeal( "incorporeal" );
static const efftype_id effect_laserlocked( "laserlocked" );
static const efftype_id effect_stunned( "stunned" );

static const flag_id json_flag_ALLOWS_REMOTE_USE( "ALLOWS_REMOTE_USE" );
static const flag_id json_flag_MOP( "MOP" );
static const flag_id json_flag_NO_UNLOAD( "NO_UNLOAD" );

static const gun_mode_id gun_mode_AUTO( "AUTO" );
static const gun_mode_id gun_mode_BURST( "BURST" );

static const itype_id fuel_type_animal( "animal" );
static const itype_id itype_radiocontrol( "radiocontrol" );

static const json_character_flag json_flag_ALARMCLOCK( "ALARMCLOCK" );
static const json_character_flag json_flag_BIONIC_SLEEP_FRIENDLY( "BIONIC_SLEEP_FRIENDLY" );
static const json_character_flag json_flag_CANNOT_ATTACK( "CANNOT_ATTACK" );
static const json_character_flag json_flag_HANDS_CANNOT_USE_FIREARMS( "HANDS_CANNOT_USE_FIREARMS" );
static const json_character_flag json_flag_LEVITATION( "LEVITATION" );
static const json_character_flag json_flag_PHASE_MOVEMENT( "PHASE_MOVEMENT" );
static const json_character_flag json_flag_SUBTLE_SPELL( "SUBTLE_SPELL" );
static const json_character_flag
json_flag_TEMPORARY_SHAPESHIFT_NO_HANDS( "TEMPORARY_SHAPESHIFT_NO_HANDS" );

static const material_id material_glass( "glass" );

static const quality_id qual_CUT( "CUT" );

static const skill_id skill_melee( "melee" );

static bool openclaw_harness_action_trace_enabled()
{
    const char *enabled = std::getenv( "OPENCLAW_HARNESS_UI_TRACE" );
    return enabled != nullptr && enabled[0] != '\0' && enabled[0] != '0';
}

static std::string openclaw_harness_quote_action_value( const std::string &value )
{
    std::string out = "\"";
    for( const char c : value ) {
        switch( c ) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            default:
                out += c;
                break;
        }
    }
    out += "\"";
    return out;
}

static void openclaw_harness_trace_default_action_dispatch( const std::string &event,
        const std::string &raw_action, const action_id act )
{
    if( !openclaw_harness_action_trace_enabled() ) {
        return;
    }

    DebugLog( D_INFO, DC_ALL )
            << "openclaw_harness_ui_trace: component=default_action_dispatch"
            << " event=" << event
            << " raw_action=" << openclaw_harness_quote_action_value( raw_action )
            << " action_id=" << openclaw_harness_quote_action_value( action_ident( act ) );
}

static void openclaw_harness_trace_wait_action_dispatch()
{
    const char *const selected_run_id = std::getenv( "OPENCLAW_HARNESS_WAIT_INPUT_TRACE_RUN_ID" );
    const char *const active_run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    if( selected_run_id == nullptr || active_run_id == nullptr || selected_run_id[0] == '\0' ||
        std::strcmp( selected_run_id, active_run_id ) != 0 ) {
        return;
    }

    DebugLog( D_INFO, DC_ALL )
            << "openclaw_harness_wait_input_trace: component=action_dispatch"
            << " event=invoke_wait"
            << " run_id=\"" << active_run_id << "\""
            << " action_id=\"wait\"";
}

static void openclaw_harness_trace_wait_menu_selection( const std::string &action_id,
        bool accepted )
{
    const char *const selected_run_id = std::getenv( "OPENCLAW_HARNESS_WAIT_INPUT_TRACE_RUN_ID" );
    const char *const active_run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    if( selected_run_id == nullptr || active_run_id == nullptr || selected_run_id[0] == '\0' ||
        std::strcmp( selected_run_id, active_run_id ) != 0 ) {
        return;
    }

    DebugLog( D_INFO, DC_ALL )
            << "openclaw_harness_wait_input_trace: component=wait_menu"
            << " event=selection"
            << " run_id=\"" << active_run_id << "\""
            << " action_id=" << openclaw_harness_quote_action_value( action_id )
            << " accepted=" << ( accepted ? "yes" : "no" );
}

static std::string openclaw_harness_bound_semantic_run_id()
{
    const char *const active_run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    const char *const bound_run_id = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID" );
    if( active_run_id == nullptr || bound_run_id == nullptr || active_run_id[0] == '\0' ||
        bound_run_id[0] == '\0' || std::strcmp( active_run_id, bound_run_id ) != 0 ) {
        return {};
    }
    return active_run_id;
}

static bool openclaw_harness_semantic_step_trace_enabled()
{
    return !openclaw_harness_bound_semantic_run_id().empty();
}

static void openclaw_harness_write_semantic_step_event( const std::string &event )
{
    const char *const path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH" );
    if( path == nullptr || path[0] == '\0' || event.empty() ) {
        return;
    }
    std::ofstream stream( path, std::ios::app | std::ios::binary );
    if( stream ) {
        stream << "openclaw_harness_semantic_step: " << event << '\n';
    }
}

static void openclaw_harness_semantic_surface_descriptor(
    const semantic_surface_descriptor &descriptor )
{
    if( descriptor.run_id != openclaw_harness_bound_semantic_run_id() ) {
        return;
    }

    std::ostringstream breadcrumbs;
    breadcrumbs << '[';
    for( size_t index = 0; index < descriptor.breadcrumbs.size(); ++index ) {
        if( index > 0 ) {
            breadcrumbs << ',';
        }
        breadcrumbs << openclaw_harness_quote_action_value( descriptor.breadcrumbs[index] );
    }
    breadcrumbs << ']';

    std::ostringstream payload;
    payload << '{';
    for( auto iter = descriptor.payload.begin(); iter != descriptor.payload.end(); ++iter ) {
        if( iter != descriptor.payload.begin() ) {
            payload << ',';
        }
        payload << openclaw_harness_quote_action_value( iter->first ) << ':'
                << openclaw_harness_quote_action_value( iter->second );
    }
    payload << '}';

    std::ostringstream actions;
    actions << '[';
    for( size_t index = 0; index < descriptor.valid_actions.size(); ++index ) {
        const semantic_action_descriptor &action = descriptor.valid_actions[index];
        if( index > 0 ) {
            actions << ',';
        }
        actions << "{\"id\":" << openclaw_harness_quote_action_value( action.id )
                << ",\"stable_id\":" << openclaw_harness_quote_action_value( action.stable_id )
                << ",\"label\":" << openclaw_harness_quote_action_value( action.label )
                << ",\"enabled\":" << ( action.enabled ? "true" : "false" ) << '}';
    }
    actions << ']';

    std::ostringstream event;
    event << "{\"event\":\"surface_descriptor\",\"schema_version\":"
          << descriptor.schema_version
          << ",\"run_id\":" << openclaw_harness_quote_action_value( descriptor.run_id )
          << ",\"surface_id\":" << openclaw_harness_quote_action_value( descriptor.surface_id )
          << ",\"frame_id\":" << openclaw_harness_quote_action_value( descriptor.frame_id )
          << ",\"kind\":" << openclaw_harness_quote_action_value( descriptor.kind )
          << ",\"breadcrumbs\":" << breadcrumbs.str()
          << ",\"payload\":" << payload.str()
          << ",\"valid_actions\":" << actions.str() << '}';
    openclaw_harness_write_semantic_step_event( event.str() );
    DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_semantic_step: " << event.str();
}

static void openclaw_harness_semantic_surface_receipt(
    const semantic_action_receipt &receipt )
{
    const std::string run_id = openclaw_harness_bound_semantic_run_id();
    if( run_id.empty() || receipt.requested_run_id != run_id ) {
        return;
    }

    std::ostringstream event;
    event << "{\"event\":\"surface_receipt\",\"run_id\":"
          << openclaw_harness_quote_action_value( run_id )
          << ",\"request_id\":" << openclaw_harness_quote_action_value( receipt.request_id )
          << ",\"requested_run_id\":" << openclaw_harness_quote_action_value(
                receipt.requested_run_id )
          << ",\"requested_surface_id\":" << openclaw_harness_quote_action_value(
                receipt.requested_surface_id )
          << ",\"requested_frame_id\":" << openclaw_harness_quote_action_value(
                receipt.requested_frame_id )
          << ",\"consuming_surface_id\":" << openclaw_harness_quote_action_value(
                receipt.consuming_surface_id )
          << ",\"consuming_frame_id\":" << openclaw_harness_quote_action_value(
                receipt.consuming_frame_id )
          << ",\"action_id\":" << openclaw_harness_quote_action_value( receipt.action_id )
          << ",\"accepted\":" << ( receipt.accepted ? "true" : "false" )
          << ",\"rejection_reason\":" << openclaw_harness_quote_action_value(
                receipt.rejection_reason )
          << ",\"resulting_frame_id\":" << openclaw_harness_quote_action_value(
                receipt.resulting_frame_id ) << '}';
    openclaw_harness_write_semantic_step_event( event.str() );
    DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_semantic_step: " << event.str();
}

semantic_surface_manager &openclaw_harness_semantic_surface_manager()
{
    static std::optional<semantic_surface_manager> manager;
    static std::string manager_run_id;
    const std::string run_id = openclaw_harness_bound_semantic_run_id();
    if( !manager || manager_run_id != run_id ) {
        manager.emplace( run_id );
        manager_run_id = run_id;
        manager->set_descriptor_observer( openclaw_harness_semantic_surface_descriptor );
        manager->set_receipt_observer( openclaw_harness_semantic_surface_receipt );
    }
    return *manager;
}

bool openclaw_harness_semantic_session_active()
{
    return !openclaw_harness_bound_semantic_run_id().empty();
}

static std::string openclaw_harness_pending_world_frame;

namespace
{
struct openclaw_harness_native_travel_record {
    std::string run_id;
    std::string travel_id;
    tripoint_abs_omt destination = tripoint_abs_omt::invalid;
    std::vector<tripoint_abs_omt> planned_omt_path;
    size_t sequence = 0;
    bool hostile_boundary_recorded = false;
};

openclaw_harness_native_travel_record openclaw_harness_native_travel;

void openclaw_harness_semantic_native_travel_event( const Character &player,
        const char *state )
{
    if( !player.is_avatar() || !openclaw_harness_semantic_step_trace_enabled() ||
        openclaw_harness_native_travel.travel_id.empty() ) {
        return;
    }
    const std::string run_id = openclaw_harness_bound_semantic_run_id();
    if( run_id != openclaw_harness_native_travel.run_id ) {
        return;
    }
    const bool destination_present = player.has_destination() || player.has_destination_activity() ||
                                     !player.omt_path.empty();
    const bool destination_cleared = !destination_present;
    const int turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    std::ostringstream event;
    event << "openclaw_harness_semantic_step: {\"event\":\"travel\",\"run_id\":"
          << openclaw_harness_quote_action_value( run_id )
          << ",\"travel_id\":" << openclaw_harness_quote_action_value(
              openclaw_harness_native_travel.travel_id )
          << ",\"receipt_id\":" << openclaw_harness_quote_action_value(
              openclaw_harness_native_travel.travel_id + ":" + state )
          << ",\"state\":" << openclaw_harness_quote_action_value( state )
          << ",\"destination\":[" << openclaw_harness_native_travel.destination.x() << ','
          << openclaw_harness_native_travel.destination.y() << ','
          << openclaw_harness_native_travel.destination.z() << ']'
          << ",\"avatar_omt\":[" << player.pos_abs_omt().x() << ','
          << player.pos_abs_omt().y() << ',' << player.pos_abs_omt().z() << ']'
          << ",\"remaining_omt_path\":" << player.omt_path.size()
          << ",\"destination_present\":" << ( destination_present ? "true" : "false" )
          << ",\"destination_cleared\":" << ( destination_cleared ? "true" : "false" )
          << ",\"observed_turn\":" << turn;
    if( std::strcmp( state, "active" ) == 0 ) {
        event << ",\"planned_omt_path\":[";
        for( size_t index = 0; index < openclaw_harness_native_travel.planned_omt_path.size(); ++index ) {
            const tripoint_abs_omt &omt = openclaw_harness_native_travel.planned_omt_path[index];
            event << ( index == 0 ? "" : "," ) << '[' << omt.x() << ',' << omt.y() << ',' << omt.z() << ']';
        }
        event << ']';
    }
    event << '}';
    DebugLog( D_INFO, DC_ALL ) << event.str();
}
} // namespace

void openclaw_harness_semantic_native_travel_started( const avatar &player,
        const tripoint_abs_omt &destination )
{
    if( !openclaw_harness_semantic_step_trace_enabled() ) {
        return;
    }
    const std::string run_id = openclaw_harness_bound_semantic_run_id();
    if( openclaw_harness_native_travel.run_id != run_id ) {
        openclaw_harness_native_travel = {};
        openclaw_harness_native_travel.run_id = run_id;
    }
    openclaw_harness_native_travel.destination = destination;
    openclaw_harness_native_travel.planned_omt_path = player.omt_path;
    // A hostile boundary belongs to one native travel operation.  A prior
    // corridor in the same bound run must not suppress this operation's first
    // hostile receipt.
    openclaw_harness_native_travel.hostile_boundary_recorded = false;
    openclaw_harness_native_travel.travel_id = string_format( "%s:travel:%zu", run_id,
            ++openclaw_harness_native_travel.sequence );
    openclaw_harness_semantic_native_travel_event( player, "active" );
}

void openclaw_harness_semantic_native_travel_progress( const Character &player )
{
    openclaw_harness_semantic_native_travel_event( player, "progress" );
}

void openclaw_harness_semantic_native_travel_hostile_boundary( const Character &player )
{
    if( openclaw_harness_native_travel.hostile_boundary_recorded ) {
        return;
    }
    openclaw_harness_native_travel.hostile_boundary_recorded = true;
    openclaw_harness_semantic_native_travel_event( player, "hostile_boundary" );
}

void openclaw_harness_semantic_native_travel_terminal( const Character &player,
        const char *terminal_state )
{
    if( terminal_state == nullptr || ( std::strcmp( terminal_state, "completed_cleared" ) == 0 &&
                                       ( player.has_destination() || player.has_destination_activity() ||
                                         !player.omt_path.empty() ) ) ) {
        return;
    }
    openclaw_harness_semantic_native_travel_event( player, terminal_state );
}

static std::string openclaw_harness_keep_watch_safety( const std::string &run_id,
        const avatar &player )
{
    // Keep this classification beside the native visibility and body owners.
    // The cockpit consumes the result as a fail-closed capability: an absent
    // or unfamiliar classification never authorizes another wait.
    static std::string last_run_id;
    static int last_hp = 0;
    static bool has_last_hp = false;
    if( last_run_id != run_id ) {
        last_run_id = run_id;
        has_last_hp = false;
    }

    const int current_hp = player.get_hp();
    const bool damage = has_last_hp && current_hp < last_hp;
    last_hp = current_hp;
    has_last_hp = true;

    bool monster = false;
    bool danger = false;
    for( Creature *creature : player.get_visible_creatures( SEEX ) ) {
        if( creature == nullptr || creature == &player ) {
            continue;
        }
        monster = monster || creature->as_monster() != nullptr;
        danger = danger || player.attitude_to( *creature ) == Creature::Attitude::HOSTILE;
    }

    const char *classification = "clear";
    if( damage ) {
        classification = "damage_detected";
    } else if( monster ) {
        classification = "monster_spotted";
    } else if( danger ) {
        classification = "danger_spotted";
    }
    return string_format( "{\"classification\":\"%s\",\"monster\":%s,\"danger\":%s,\"damage\":%s}",
                          classification, monster ? "true" : "false", danger ? "true" : "false",
                          damage ? "true" : "false" );
}

static std::string openclaw_harness_visible_local_facts( const map &here,
        const tripoint_bub_ms &avatar_pos, int radius, bool include_unknown, bool include_identity )
{
    const visibility_variables &cache = here.get_visibility_variables_cache();
    const level_cache &map_cache = here.get_cache_ref( avatar_pos.z() );
    const auto &visibility_cache = map_cache.visibility_cache;
    std::ostringstream facts;
    facts << '[';
    bool first = true;
    for( int y = -radius; y <= radius; ++y ) {
        for( int x = -radius; x <= radius; ++x ) {
            const tripoint_bub_ms fact_pos( avatar_pos.x() + x, avatar_pos.y() + y, avatar_pos.z() );
            const bool clear = here.inbounds( fact_pos ) && here.get_visibility(
                                   visibility_cache[fact_pos.x()][fact_pos.y()], cache ) == visibility_type::CLEAR;
            if( !clear && !include_unknown ) {
                continue;
            }
            if( !first ) {
                facts << ',';
            }
            first = false;
            facts << "{\"dx\":" << x << ",\"dy\":" << y;
            if( clear ) {
                facts << ",\"visibility\":\"clear\"";
                if( include_identity ) {
                    facts << ",\"identity\":{\"dx\":" << x
                          << ",\"dy\":" << y << ",\"terrain\":"
                          << openclaw_harness_quote_action_value( here.ter( fact_pos ).obj().id.str() )
                          << '}';
                    const furn_id furniture = here.furn( fact_pos );
                    if( furniture != furn_str_id::NULL_ID() ) {
                        facts << ",\"furniture\":" << openclaw_harness_quote_action_value(
                                  furniture.obj().id.str() );
                    }
                    facts << ",\"fields\":[";
                    bool first_field = true;
                    for( const std::pair<const field_type_id, field_entry> &field : here.field_at( fact_pos ) ) {
                        if( !first_field ) {
                            facts << ',';
                        }
                        first_field = false;
                        facts << openclaw_harness_quote_action_value( field.first.obj().id.str() );
                    }
                    facts << ']';
                }
                facts << ",\"terrain\":" << openclaw_harness_quote_action_value(
                          here.tername( fact_pos ) );
            } else {
                facts << ",\"visibility\":\"unknown\"";
            }
            facts << '}';
        }
    }
    facts << ']';
    return facts.str();
}

static const char *openclaw_harness_overmap_vision_name( const om_vision_level vision )
{
    switch( vision ) {
        case om_vision_level::unseen:
            return "unknown";
        case om_vision_level::vague:
            return "vague";
        case om_vision_level::outlines:
            return "outlines";
        case om_vision_level::details:
            return "details";
        case om_vision_level::full:
            return "full";
        case om_vision_level::last:
            break;
    }
    return "unknown";
}

static std::string openclaw_harness_overmap_facts( const avatar &player, const int radius,
        const int observed_turn )
{
    const tripoint_abs_omt center = player.pos_abs_omt();
    std::ostringstream facts;
    facts << '[';
    bool first = true;
    for( int y = -radius; y <= radius; ++y ) {
        for( int x = -radius; x <= radius; ++x ) {
            const tripoint_abs_omt position = center + point( x, y );
            const om_vision_level vision = overmap_buffer.seen( position );
            if( !first ) {
                facts << ',';
            }
            first = false;
            facts << "{\"dx\":" << x << ",\"dy\":" << y
                  << ",\"state\":\"" << ( vision == om_vision_level::unseen ? "unknown" : "clear" )
                  << "\",\"vision\":" << openclaw_harness_quote_action_value(
                      openclaw_harness_overmap_vision_name( vision ) )
                  << ",\"provenance\":\"avatar_discovered_overmap\""
                  << ",\"recency\":{\"state\":\"current_frame\",\"observed_turn\":"
                  << observed_turn << '}';
            // The persisted overmap stores discovery level, not a last-seen
            // timestamp.  Never manufacture terrain for an unseen cell.
            if( vision != om_vision_level::unseen ) {
                facts << ",\"terrain\":" << openclaw_harness_quote_action_value(
                          overmap_buffer.ter_existing( position )->get_name( vision ) );
            }
            facts << '}';
        }
    }
    facts << ']';
    return facts.str();
}

static std::string openclaw_harness_attitude( const avatar &viewer, const Creature &creature )
{
    switch( viewer.attitude_to( creature ) ) {
        case Creature::Attitude::HOSTILE:
            return "hostile";
        case Creature::Attitude::FRIENDLY:
            return "friendly";
        case Creature::Attitude::NEUTRAL:
            return "neutral";
        case Creature::Attitude::ANY:
            return "unknown";
    }
    return "unknown";
}

static std::string openclaw_harness_visible_entities( avatar &viewer )
{
    std::ostringstream entities;
    entities << '[';
    bool first = true;
    for( Creature *creature : viewer.get_visible_creatures( SEEX ) ) {
        if( creature == nullptr || creature == &viewer ) {
            continue;
        }
        std::string kind = "creature";
        std::string identity;
        if( const Character *character = creature->as_character() ) {
            kind = creature->as_npc() != nullptr ? "npc" : "character";
            identity = string_format( "character:%d", character->getID().get_value() );
        } else if( creature->as_monster() != nullptr ) {
            kind = "monster";
            std::ostringstream process_identity;
            process_identity << "process:" << static_cast<const void *>( creature );
            identity = process_identity.str();
        }
        if( identity.empty() ) {
            continue;
        }
        if( !first ) {
            entities << ',';
        }
        first = false;
        const tripoint_rel_ms relative = creature->pos_bub() - viewer.pos_bub();
        const tripoint_abs_ms absolute = creature->pos_abs();
        entities << "{\"identity\":{\"kind\":"
                 << openclaw_harness_quote_action_value( kind ) << ",\"id\":"
                 << openclaw_harness_quote_action_value( identity ) << "},\"kind\":"
                 << openclaw_harness_quote_action_value( kind ) << ",\"name\":"
                 << openclaw_harness_quote_action_value( creature->disp_name() ) << ",\"attitude\":"
                 << openclaw_harness_quote_action_value( openclaw_harness_attitude( viewer, *creature ) )
                 << ",\"dx\":" << relative.x() << ",\"dy\":" << relative.y();
        entities << ",\"absolute_ms\":[" << absolute.x() << ',' << absolute.y() << ',' << absolute.z() << ']';
        if( const monster *monster = creature->as_monster() ) {
            // The process address is only a run-local handle.  A fixture tag
            // binds this visible monster back to its applied save receipt.
            entities << ",\"fixture_actor_id\":"
                     << openclaw_harness_quote_action_value(
                            monster->get_value( "caol_fixture_actor_id" ).str() )
                     << ",\"typeid\":"
                     << openclaw_harness_quote_action_value( monster->type->id.str() )
                     << ",\"faction\":"
                     << openclaw_harness_quote_action_value( monster->get_monster_faction().id().str() )
                     << ",\"friendly\":" << monster->friendly
                     << ",\"anger\":" << monster->anger
                     << ",\"morale\":" << monster->morale
                     << ",\"aggro_character\":" << ( monster->aggro_character ? "true" : "false" );
        }
        entities << '}';
    }
    entities << ']';
    return entities.str();
}

static std::string openclaw_harness_visible_zones( const map &here,
        const tripoint_bub_ms &avatar_pos )
{
    const visibility_variables &cache = here.get_visibility_variables_cache();
    const level_cache &map_cache = here.get_cache_ref( avatar_pos.z() );
    const auto &visibility_cache = map_cache.visibility_cache;
    const zone_manager &manager = zone_manager::get_manager();
    std::ostringstream zones;
    zones << '[';
    bool first = true;
    for( const zone_manager::ref_const_zone_data &zone_ref : manager.get_zones() ) {
        const zone_data &zone = zone_ref.get();
        if( !zone.get_enabled() ) {
            continue;
        }
        const tripoint_bub_ms center = here.get_bub( zone.get_center_point() );
        const tripoint_rel_ms relative = center - avatar_pos;
        if( std::abs( relative.x() ) > SEEX || std::abs( relative.y() ) > SEEX ||
            !here.inbounds( center ) || here.get_visibility(
                visibility_cache[center.x()][center.y()], cache ) != visibility_type::CLEAR ) {
            continue;
        }
        if( !first ) {
            zones << ',';
        }
        first = false;
        zones << "{\"name\":" << openclaw_harness_quote_action_value( zone.get_name() )
              << ",\"type\":" << openclaw_harness_quote_action_value( zone.get_type().str() )
              << ",\"dx\":" << relative.x() << ",\"dy\":" << relative.y() << '}';
    }
    zones << ']';
    return zones.str();
}

static std::string openclaw_harness_semantic_step_frame(
    const std::string &state, const std::vector<std::pair<std::string, std::string>> &actions,
    const std::string &producer = "" )
{
    if( !openclaw_harness_semantic_step_trace_enabled() ) {
        return {};
    }

    static size_t sequence = 0;
    const std::string run_id = openclaw_harness_bound_semantic_run_id();
    if( run_id.empty() ) {
        return {};
    }
    const int turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    const int game_minutes = to_minutes<int>( calendar::turn - calendar::start_of_cataclysm );
    const std::string frame_id = string_format( "%s:%d:%zu", run_id, turn, ++sequence );
    const bool include_minimap = state == "world";
    const map &here = get_map();
    avatar &player = get_avatar();
    const tripoint_bub_ms avatar_pos = player.pos_bub( here );
    std::ostringstream action_ids;
    action_ids << '[';
    for( size_t index = 0; index < actions.size(); ++index ) {
        if( index > 0 ) {
            action_ids << ',';
        }
        action_ids << openclaw_harness_quote_action_value( actions[index].first );
    }
    action_ids << ']';
    std::ostringstream minimap;
    if( include_minimap ) {
        minimap << ",\"minimap\":{\"schema\":\"caol-native-minimap-v1\",\"radius\":" << SEEX
                << ",\"cells\":" << openclaw_harness_visible_local_facts( here, avatar_pos, SEEX, true,
                        false ) << '}';
    }
    // The native HUD's seven-cell minimap is the measured compact mission
    // surface.  Its half-width is therefore the public overmap observation
    // radius; the cockpit must not choose a larger, invented world window.
    const int overmap_radius = MINIMAP_WIDTH / 2;
    std::ostringstream overmap;
    overmap << ",\"overmap\":{\"schema\":\"caol-avatar-overmap-v1\",\"coordinate_system\":"
            << "\"avatar_relative_omt\",\"radius\":" << overmap_radius
            << ",\"bound_source\":\"native_hud_minimap_width\",\"center_absolute_omt\":["
            << player.pos_abs_omt().x() << ',' << player.pos_abs_omt().y() << ','
            << player.pos_abs_omt().z() << "],\"cells\":"
            << openclaw_harness_overmap_facts( player, overmap_radius, turn ) << '}';

    std::ostringstream event;
    event << "{\"event\":\"frame\",\"run_id\":"
          << openclaw_harness_quote_action_value( run_id )
          << ",\"frame_id\":" << openclaw_harness_quote_action_value( frame_id )
          << ",\"state\":" << openclaw_harness_quote_action_value( state )
          << ",\"observed_turn\":" << turn
          << ",\"game_minutes\":" << game_minutes
          << ",\"producer\":" << openclaw_harness_quote_action_value( producer )
          << ",\"initial_world_ready\":" << ( producer == "hud_world_ready" ? "true" : "false" )
          << ",\"keep_watch_safety\":"
          << openclaw_harness_keep_watch_safety( run_id, player )
          << ",\"observation\":{\"schema\":\"caol-avatar-visible-v1\",\"avatar\":{\"name\":"
          << openclaw_harness_quote_action_value( player.name )
          << ",\"absolute_ms\":[" << player.pos_abs().x() << ',' << player.pos_abs().y() << ','
          << player.pos_abs().z() << "]},\"visible_local\":"
          << openclaw_harness_visible_local_facts( here, avatar_pos, 1, false, true )
          << minimap.str()
          << overmap.str()
          << ",\"visible_entities\":" << openclaw_harness_visible_entities( player )
          << ",\"visible_zones\":" << openclaw_harness_visible_zones( here, avatar_pos ) << '}'
          << ",\"valid_actions\":" << action_ids.str() << '}';
    openclaw_harness_write_semantic_step_event( event.str() );
    DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_semantic_step: " << event.str();
    return frame_id;
}

static std::vector<std::pair<std::string, std::string>> openclaw_harness_world_actions(
    const input_context &context )
{
    static const std::vector<std::pair<std::string, std::string>> action_ids = {
        { "world.pause", "pause" },
        { "world.wait", "wait" },
        { "world.quicksave", "quicksave" },
        { "world.save_quit", "save" },
        { "world.inventory", "inventory" },
        { "world.zone_manager", "zones" },
        { "world.overmap", "map" },
        { "world.chat", "chat" },
        { "world.fire", "fire" },
        { "world.debug_menu", "debug" },
        { "world.move.north", "UP" },
        { "world.move.south", "DOWN" },
        { "world.move.west", "LEFT" },
        { "world.move.east", "RIGHT" },
    };
    std::vector<std::pair<std::string, std::string>> result;
    for( const std::pair<std::string, std::string> &action : action_ids ) {
        // The native context, rather than an optional physical keybinding,
        // decides whether this logical action is currently available.
        if( context.is_registered_action( action.second ) ) {
            result.emplace_back( action );
        }
    }
    return result;
}

void openclaw_harness_semantic_initial_world_frame_if_ready( const input_context *active_input_context,
        const bool no_activity_owns_turn, const bool no_auto_move_owns_turn,
        const bool no_dead_watch_owns_turn )
{
    // The completed world-HUD render is the only startup producer.  The
    // active native input context, not UI topology, decides whether a modal
    // owner prevents publication.
    if( active_input_context == nullptr || active_input_context->get_category() != "DEFAULTMODE" ||
        !no_activity_owns_turn || !no_auto_move_owns_turn || !no_dead_watch_owns_turn ||
        !openclaw_harness_semantic_step_trace_enabled() ) {
        return;
    }
    if( !active_input_context->first_keyboard_character_for_action( "wait" ) ) {
        return;
    }

    const std::string active_run_id = openclaw_harness_bound_semantic_run_id();
    static std::string bound_run_id;
    static bool emitted = false;
    if( bound_run_id != active_run_id ) {
        bound_run_id = active_run_id;
        emitted = false;
    }
    if( active_run_id.empty() || emitted || get_avatar().is_dead_state() ) {
        return;
    }

    const std::vector<std::pair<std::string, std::string>> actions =
        openclaw_harness_world_actions( *active_input_context );
    if( actions.empty() ) {
        return;
    }

    if( !openclaw_harness_semantic_step_frame( "world", actions, "hud_world_ready" ).empty() ) {
        emitted = true;
    }
}

static std::vector<character_id> openclaw_harness_basecamp_mission_candidates()
{
    std::vector<character_id> result;
    avatar &you = get_avatar();
    for( const shared_ptr_fast<npc> &candidate : overmap_buffer.get_npcs_near_player( HALF_MAPSIZE ) ) {
        if( !candidate || candidate->is_dead() || !candidate->assigned_camp ) {
            continue;
        }
        // This entry deliberately supports only the ordinary nearby-contact
        // route.  Radio contact remains owned by the existing faction UI.
        if( rl_dist( you.pos_abs_omt(), candidate->pos_abs_omt() ) > 3 ) {
            continue;
        }
        std::optional<basecamp *> camp = overmap_buffer.find_camp( candidate->assigned_camp->xy() );
        if( camp && ( *camp )->allowed_access_by( you ) ) {
            result.push_back( candidate->getID() );
        }
    }
    return result;
}

static std::string openclaw_harness_world_messages()
{
    const std::vector<std::pair<std::string, std::string>> messages =
        Messages::recent_messages( Messages::size() );
    std::ostringstream result;
    result << '[';
    for( size_t index = 0; index < messages.size(); ++index ) {
        if( index > 0 ) {
            result << ',';
        }
        result << "{\"time\":" << openclaw_harness_quote_action_value( messages[index].first )
               << ",\"text\":" << openclaw_harness_quote_action_value( messages[index].second ) << '}';
    }
    result << ']';
    return result.str();
}

static std::map<std::string, std::string> openclaw_harness_world_payload()
{
    const map &here = get_map();
    avatar &player = get_avatar();
    const tripoint_bub_ms avatar_pos = player.pos_bub( here );
    const int turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    const int overmap_radius = MINIMAP_WIDTH / 2;

    std::ostringstream avatar_fact;
    avatar_fact << "{\"name\":" << openclaw_harness_quote_action_value( player.name )
                << ",\"absolute_ms\":[" << player.pos_abs().x() << ',' << player.pos_abs().y() << ','
                << player.pos_abs().z() << "]}";
    std::ostringstream minimap;
    minimap << "{\"schema\":\"caol-native-minimap-v1\",\"radius\":" << SEEX
            << ",\"cells\":" << openclaw_harness_visible_local_facts( here, avatar_pos, SEEX, true,
                    false ) << '}';
    std::ostringstream overmap;
    overmap << "{\"schema\":\"caol-avatar-overmap-v1\",\"coordinate_system\":"
            << "\"avatar_relative_omt\",\"radius\":" << overmap_radius
            << ",\"bound_source\":\"native_hud_minimap_width\",\"center_absolute_omt\":["
            << player.pos_abs_omt().x() << ',' << player.pos_abs_omt().y() << ','
            << player.pos_abs_omt().z() << "],\"cells\":"
            << openclaw_harness_overmap_facts( player, overmap_radius, turn ) << '}';

    return {
        { "avatar", avatar_fact.str() },
        { "visible_local", openclaw_harness_visible_local_facts( here, avatar_pos, 1, false, true ) },
        { "minimap", minimap.str() },
        { "overmap", overmap.str() },
        { "visible_entities", openclaw_harness_visible_entities( player ) },
        { "visible_zones", openclaw_harness_visible_zones( here, avatar_pos ) },
        { "messages", openclaw_harness_world_messages() },
        { "world_mode", player.current_movement_mode().str() },
        { "last_save_result", g == nullptr ? "unavailable" : g->last_save_result() }
    };
}

static std::vector<semantic_action_descriptor> semantic_surface_actions(
    const std::vector<std::pair<std::string, std::string>> &actions )
{
    std::vector<semantic_action_descriptor> result;
    result.reserve( actions.size() );
    for( const std::pair<std::string, std::string> &action : actions ) {
        result.push_back( { action.first, "", action.first, true } );
    }
    return result;
}

void openclaw_harness_semantic_wait_activity_complete()
{
    openclaw_harness_semantic_step_frame( "wait_activity_complete", {} );
}

void openclaw_harness_semantic_activity_distraction()
{
    openclaw_harness_semantic_step_frame( "activity_distraction", {
        { "activity.stop", "Y" },
        { "activity.continue", "N" },
        { "activity.manage", "M" },
        { "activity.ignore", "I" },
    }, "activity_distraction_query" );
}

void openclaw_harness_semantic_world_after_activity_distraction()
{
    if( !openclaw_harness_semantic_step_trace_enabled() ) {
        return;
    }
    input_context world_context = get_default_mode_input_context();
    openclaw_harness_pending_world_frame = openclaw_harness_semantic_step_frame(
                    "world", openclaw_harness_world_actions( world_context ) );
}

static void openclaw_harness_semantic_step_receipt( const std::string &frame_id,
        const std::string &action_id, bool accepted )
{
    if( frame_id.empty() || !openclaw_harness_semantic_step_trace_enabled() ) {
        return;
    }
    const char *const run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    const int turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    DebugLog( D_INFO, DC_ALL )
            << "openclaw_harness_semantic_step: {\"event\":\"receipt\",\"run_id\":"
            << openclaw_harness_quote_action_value( run_id )
            << ",\"frame_id\":" << openclaw_harness_quote_action_value( frame_id )
            << ",\"action_id\":" << openclaw_harness_quote_action_value( action_id )
            << ",\"accepted\":" << ( accepted ? "true" : "false" )
            << ",\"observed_turn\":" << turn << '}';
}

static void openclaw_harness_semantic_movement_receipt( const std::string &frame_id,
        const std::string &action_id, const tripoint_abs_ms &before,
        const tripoint_abs_ms &expected, const tripoint_abs_ms &after, const map &here,
        const tripoint_bub_ms &after_bub,
        bool move_handled )
{
    if( frame_id.empty() || !openclaw_harness_semantic_step_trace_enabled() ) {
        return;
    }
    const char *const run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    const char *const outcome = !move_handled ? "blocked" : after == expected ? "moved" :
                                after == before ? "no_progress" : "unexpected_displacement";
    // Movement may shift the map bubble.  Capture receipt coordinates in
    // absolute map-square space at their own side of that shift, matching the
    // semantic frame contract rather than translating pre-move bubble points
    // through the post-move map origin.
    const int turn = to_turns<int>( calendar::turn - calendar::turn_zero );
    std::ostringstream event;
    event << "{\"event\":\"receipt\",\"run_id\":"
          << openclaw_harness_quote_action_value( run_id )
          << ",\"frame_id\":" << openclaw_harness_quote_action_value( frame_id )
          << ",\"action_id\":" << openclaw_harness_quote_action_value( action_id )
          << ",\"accepted\":" << ( std::strcmp( outcome, "moved" ) == 0 ? "true" : "false" )
          << ",\"outcome\":" << openclaw_harness_quote_action_value( outcome )
          << ",\"coordinate_space\":\"absolute_ms\""
          << ",\"before_absolute_ms\":[" << before.x() << ',' << before.y() << ',' << before.z()
          << "],\"expected_absolute_ms\":[" << expected.x() << ',' << expected.y() << ',' << expected.z()
          << "],\"after_absolute_ms\":[" << after.x() << ',' << after.y() << ',' << after.z()
          << "],\"after_terrain\":" << openclaw_harness_quote_action_value(
              here.ter( after_bub ).obj().id.str() )
          << ",\"observed_turn\":" << turn << '}';
    openclaw_harness_write_semantic_step_event( event.str() );
    DebugLog( D_INFO, DC_ALL ) << "openclaw_harness_semantic_step: " << event.str();
}

static std::string openclaw_harness_semantic_movement_action_id( action_id action )
{
    switch( action ) {
        case ACTION_MOVE_FORTH:
            return "world.move.north";
        case ACTION_MOVE_BACK:
            return "world.move.south";
        case ACTION_MOVE_LEFT:
            return "world.move.west";
        case ACTION_MOVE_RIGHT:
            return "world.move.east";
        default:
            return {};
    }
}

static const trait_id trait_BRAWLER( "BRAWLER" );
static const trait_id trait_GUNSHY( "GUNSHY" );
static const trait_id trait_HIBERNATE( "HIBERNATE" );
static const trait_id trait_PROF_CHURL( "PROF_CHURL" );
static const trait_id trait_SHELL2( "SHELL2" );
static const trait_id trait_SHELL3( "SHELL3" );
static const trait_id trait_WAYFARER( "WAYFARER" );

static const zone_type_id zone_type_CHOP_TREES( "CHOP_TREES" );
static const zone_type_id zone_type_CONSTRUCTION_BLUEPRINT( "CONSTRUCTION_BLUEPRINT" );
static const zone_type_id zone_type_DISASSEMBLE( "DISASSEMBLE" );
static const zone_type_id zone_type_FARM_PLOT( "FARM_PLOT" );
static const zone_type_id zone_type_LOOT_CORPSE( "LOOT_CORPSE" );
static const zone_type_id zone_type_LOOT_UNSORTED( "LOOT_UNSORTED" );
static const zone_type_id zone_type_LOOT_WOOD( "LOOT_WOOD" );
static const zone_type_id zone_type_MINING( "MINING" );
static const zone_type_id zone_type_MOPPING( "MOPPING" );
static const zone_type_id zone_type_STRIP_CORPSES( "STRIP_CORPSES" );
static const zone_type_id zone_type_STUDY_ZONE( "STUDY_ZONE" );
static const zone_type_id zone_type_UNLOAD_ALL( "UNLOAD_ALL" );
static const zone_type_id zone_type_VEHICLE_DECONSTRUCT( "VEHICLE_DECONSTRUCT" );
static const zone_type_id zone_type_VEHICLE_REPAIR( "VEHICLE_REPAIR" );

#define dbg(x) DebugLog((x),D_GAME) << __FILE__ << ":" << __LINE__ << ": "

#if defined(__ANDROID__)
extern std::map<std::string, std::list<input_event>> quick_shortcuts_map;
extern bool add_best_key_for_action_to_quick_shortcuts( action_id action,
        const std::string &category, bool back );
extern bool add_key_to_quick_shortcuts( int key, const std::string &category, bool back );
#endif

static bool has_vehicle_control( avatar &player_character );

namespace
{
class user_turn
{

    private:
        std::chrono::time_point<std::chrono::steady_clock> user_turn_start;
    public:
        user_turn() {
            user_turn_start = std::chrono::steady_clock::now();
        }

        bool has_timeout_elapsed() {
            return moves_elapsed() > 100;
        }

        int moves_elapsed() {
            const float turn_duration = get_option<float>( "TURN_DURATION" );
            // Magic number 0.005 chosen due to option menu's 2 digit precision and
            // the option menu UI rounding <= 0.005 down to "0.00" in the display.
            // This conditional will catch values (e.g. 0.003) that the options menu
            // would round down to "0.00" in the options menu display. This prevents
            // the user from being surprised by floating point rounding near zero.
            if( turn_duration <= 0.005 ) {
                return 0;
            }
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            std::chrono::milliseconds elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>( now - user_turn_start );
            return elapsed_ms.count() / ( 10.0 * turn_duration );
        }

        bool async_anim_timeout() {
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            std::chrono::milliseconds elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>( now - user_turn_start );
            return elapsed_ms.count() > get_option<int>( "ANIMATION_DELAY" );
        }

        std::chrono::steady_clock::time_point last_blink_transition = std::chrono::steady_clock::now();
        bool blink_timeout() {
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            std::chrono::milliseconds elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>( now - last_blink_transition );
            if( elapsed_ms.count() > get_option<int>( "BLINK_SPEED" ) ) {
                last_blink_transition = now;
                return true;
            }
            return false;
        }

};
} // namespace

input_context game::get_player_input( std::string &action )
{
    map &here = get_map();

    const tripoint_bub_ms pos = u.pos_bub( here );

    input_context ctxt;
    // A save can carry the watch quit-status while its transformed avatar is
    // alive again.  Gate the death-cam input surface on the avatar's actual
    // death state so the ordinary HUD retains its native semantic producer.
    const bool watching_dead_avatar = uquit == QUIT_WATCH && u.is_dead_state();
    if( watching_dead_avatar ) {
        ctxt = input_context( "DEFAULTMODE", keyboard_mode::keycode );
        ctxt.set_iso( true );
        // The list of allowed actions in death-cam mode in game::handle_action
        // *INDENT-OFF*
        for( const action_id id : {
            ACTION_CENTER,
            ACTION_SHIFT_N,
            ACTION_SHIFT_NE,
            ACTION_SHIFT_E,
            ACTION_SHIFT_SE,
            ACTION_SHIFT_S,
            ACTION_SHIFT_SW,
            ACTION_SHIFT_W,
            ACTION_SHIFT_NW,
            ACTION_LOOK,
            ACTION_KEYBINDINGS,
        } ) {
            ctxt.register_action( action_ident( id ) );
        }
        // *INDENT-ON*
        ctxt.register_action( "QUIT", to_translation( "Accept your fate" ) );
    } else {
        ctxt = get_default_mode_input_context();
    }

    const char *const startup_boundary_trace = std::getenv( "OPENCLAW_HARNESS_UI_TRACE" );
    const char *const active_run_id = std::getenv( "OPENCLAW_HARNESS_RUN_ID" );
    const bool startup_boundary_trace_enabled = startup_boundary_trace != nullptr &&
            startup_boundary_trace[0] != '\0' && startup_boundary_trace[0] != '0' &&
            active_run_id != nullptr && active_run_id[0] != '\0';
    if( startup_boundary_trace_enabled ) {
        DebugLog( D_INFO, DC_ALL )
                << "openclaw_harness_startup_boundary: event=native_input_entered"
                << " run_id=\"" << active_run_id << '\"'
                << " avatar_live=" << ( u.is_dead_state() ? "no" : "yes" )
                << " modal_owner=none"
                << " world_wait_available=" <<
                ( ctxt.first_keyboard_character_for_action( "wait" ) ? "yes" : "no" );
    }

    here.update_visibility_cache( pos.z() );
    if( !watching_dead_avatar ) {
        // This later input owner may publish ordinary per-action frames, but
        // never the initial world frame: do_turn publishes that once before
        // any native input can be read.
        if( startup_boundary_trace_enabled ) {
            DebugLog( D_INFO, DC_ALL )
                    << "openclaw_harness_startup_boundary: event=descriptor_publication"
                    << " run_id=\"" << active_run_id << '\"'
                    << " avatar_live=yes modal_owner=none"
                    << " world_wait_available=" <<
                    ( ctxt.first_keyboard_character_for_action( "wait" ) ? "yes" : "no" );
        }
        const std::vector<std::pair<std::string, std::string>> semantic_actions =
            openclaw_harness_world_actions( ctxt );
        openclaw_harness_pending_world_frame = openclaw_harness_semantic_step_frame(
                    "world", semantic_actions );
    }

    const visibility_variables &cache = here.get_visibility_variables_cache();
    const level_cache &map_cache = here.get_cache_ref( pos.z() );
    const auto &visibility_cache = map_cache.visibility_cache;
#if defined(TILES)
    // Mark cata_tiles draw caches as dirty
    tilecontext->set_draw_cache_dirty();
#endif

    user_turn current_turn;

    if( get_option<bool>( "ANIMATIONS" ) ) {
        const int TOTAL_VIEW = MAX_VIEW_DISTANCE * 2 + 1;
        point iStart( ( TERRAIN_WINDOW_WIDTH > TOTAL_VIEW ) ? ( TERRAIN_WINDOW_WIDTH - TOTAL_VIEW ) / 2 : 0,
                      ( TERRAIN_WINDOW_HEIGHT > TOTAL_VIEW ) ? ( TERRAIN_WINDOW_HEIGHT - TOTAL_VIEW ) / 2 :
                      0 );
        point iEnd( ( TERRAIN_WINDOW_WIDTH > TOTAL_VIEW ) ? TERRAIN_WINDOW_WIDTH -
                    ( TERRAIN_WINDOW_WIDTH - TOTAL_VIEW ) /
                    2 :
                    TERRAIN_WINDOW_WIDTH, ( TERRAIN_WINDOW_HEIGHT > TOTAL_VIEW ) ? TERRAIN_WINDOW_HEIGHT -
                    ( TERRAIN_WINDOW_HEIGHT - TOTAL_VIEW ) /
                    2 : TERRAIN_WINDOW_HEIGHT );

        if( fullscreen ) {
            iStart.x = 0;
            iStart.y = 0;
            iEnd.x = TERMX;
            iEnd.y = TERMY;
        }

        //x% of the Viewport, only shown on visible areas
        const weather_animation_t weather_info = weather.weather_id->weather_animation;
        point offset( u.view_offset.xy().raw() + point( -getmaxx( w_terrain ) / 2 + pos.x(),
                      -getmaxy( w_terrain ) / 2 + pos.y() ) );

#if defined(TILES)
        if( g->is_tileset_isometric() ) {
            iStart.x = 0;
            iStart.y = 0;
            iEnd.x = MAPSIZE_X;
            iEnd.y = MAPSIZE_Y;
            offset.x = 0;
            offset.y = 0;
        }
#endif //TILES

        // TODO: Move the weather calculations out of here.
        const bool bWeatherEffect = weather_info.symbol != NULL_UNICODE;
        const int dropCount = static_cast<int>( iEnd.x * iEnd.y * weather_info.factor );

        weather_printable wPrint;
        wPrint.colGlyph = weather_info.color;
        wPrint.cGlyph = weather_info.symbol;
        wPrint.wtype = weather.weather_id;
        wPrint.vdrops.clear();

        ctxt.set_timeout( 125 );

        shared_ptr_fast<game::draw_callback_t> animation_cb =
        make_shared_fast<game::draw_callback_t>( [&]() {
            draw_weather( wPrint );

            if( uquit != QUIT_WATCH ) {
                draw_sct();
            }
        } );
        add_draw_callback( animation_cb );

        creature_tracker &creatures = get_creature_tracker();
        do {
            if( bWeatherEffect && get_option<bool>( "ANIMATION_RAIN" ) ) {
                /*
                Location to add rain drop animation bits! Since it refreshes w_terrain it can be added to the animation section easily
                Get tile information from above's weather information:
                WEATHER_DRIZZLE | WEATHER_LIGHT_DRIZZLE | WEATHER_RAINY | WEATHER_RAINSTORM | WEATHER_THUNDER | WEATHER_LIGHTNING = "weather_rain_drop"
                WEATHER_FLURRIES | WEATHER_SNOW | WEATHER_SNOWSTORM = "weather_snowflake"
                */
                const bool had_weather_animation = !wPrint.vdrops.empty();
                wPrint.vdrops.clear();

                for( int i = 0; i < dropCount; i++ ) {
                    const point iRand( rng( iStart.x, iEnd.x - 1 ), rng( iStart.y, iEnd.y - 1 ) );
                    const point map( iRand + offset );

                    const tripoint_bub_ms mapp( map.x, map.y, pos.z() );
                    const bool no_roof_above = here.has_flag( ter_furn_flag::TFLAG_NO_FLOOR, mapp + tripoint::above );
                    if( here.inbounds( mapp ) && here.is_outside( mapp ) &&
                        no_roof_above &&
                        here.get_visibility( visibility_cache[mapp.x()][mapp.y()], cache ) ==
                        visibility_type::CLEAR &&
                        !creatures.creature_at( mapp, true ) ) {
                        // Suppress if a critter is there
                        wPrint.vdrops.emplace_back( iRand.x, iRand.y );
                    }
                }
                if( had_weather_animation || !wPrint.vdrops.empty() ) {
                    invalidate_main_ui_adaptor();
                }
            }
            // don't bother calculating SCT if we won't show it
            if( uquit != QUIT_WATCH && get_option<bool>( "ANIMATION_SCT" ) && !SCT.vSCT.empty() ) {
                invalidate_main_ui_adaptor();

                SCT.advanceAllSteps();

                //Check for creatures on all drawing positions and offset if necessary
                for( auto iter = SCT.vSCT.rbegin(); iter != SCT.vSCT.rend(); ++iter ) {
                    const direction oCurDir = iter->getDirection();
                    const int width = utf8_width( iter->getText() );
                    for( int i = 0; i < width; ++i ) {
                        tripoint_bub_ms tmp( iter->getPosX() + i, iter->getPosY(), here.get_abs_sub().z() );
                        const Creature *critter = creatures.creature_at( tmp, true );

                        if( critter != nullptr && u.sees( here, *critter ) ) {
                            i = -1;
                            int iPos = iter->getStep() + iter->getStepOffset();
                            for( auto iter2 = iter; iter2 != SCT.vSCT.rend(); ++iter2 ) {
                                if( iter2->getDirection() == oCurDir &&
                                    iter2->getStep() + iter2->getStepOffset() <= iPos ) {
                                    if( iter2->getType() == "hp" ) {
                                        iter2->advanceStepOffset();
                                    }

                                    iter2->advanceStepOffset();
                                    iPos = iter2->getStep() + iter2->getStepOffset();
                                }
                            }
                        }
                    }
                }
            }

            if( pixel_minimap_option && g->w_pixel_minimap ) {
                if( liveview.is_enabled() ) {
                    // Mouse View overlaps the minimap; a direct wnoutrefresh
                    // ignores ui_adaptor z-order and paints over it.
                    invalidate_main_ui_adaptor();
                } else {
#if defined(TILES)
                    // Mark minimap dirty so beacon colors keep cycling
                    if( tilecontext->has_blinking_minimap() ) {
                        werase( g->w_pixel_minimap );
                    }
#endif
                    wnoutrefresh( g->w_pixel_minimap );
                }
            }

            std::unique_ptr<static_popup> deathcam_msg_popup;
            if( uquit == QUIT_WATCH ) {
                deathcam_msg_popup = std::make_unique<static_popup>();
                deathcam_msg_popup
                ->wait_message( c_red, _( "Press %s to accept your fate…" ), ctxt.get_desc( "QUIT" ) )
                .on_top( true );
            }

            // Remove asynchronous animations after animation delay if no input
            if( current_turn.async_anim_timeout() ) {
                bool cleared = g->void_async_anim_curses();
#if defined(TILES)
                cleared |= tilecontext->void_async_anim();
#endif
                if( cleared ) {
                    invalidate_main_ui_adaptor();
                }
            }

#if defined(TILES)
            // Expire stale hit-animation overlays between draws
            if( tilecontext->expire_hit_animations() ) {
                invalidate_main_ui_adaptor();
            }

            // Animated tiles need periodic redraws to cycle frames
            if( tilecontext->has_animated_tiles() ) {
                invalidate_main_ui_adaptor();
            }
#endif

            if( g->has_blink_curses() && current_turn.blink_timeout() ) {
                // Toggle blink phase and redraw
                g->blink_active_phase = !g->blink_active_phase;
                g->invalidate_main_ui_adaptor();
            }

            ui_manager::redraw_invalidated();
        } while( handle_mouseview( ctxt, action ) && uquit != QUIT_WATCH
                 && ( action != "TIMEOUT" || !current_turn.has_timeout_elapsed() ) );
        ctxt.reset_timeout();
    } else {
        ctxt.set_timeout( 125 );
        while( handle_mouseview( ctxt, action ) ) {
            if( action == "TIMEOUT" && current_turn.has_timeout_elapsed() ) {
                break;
            }
        }
        ctxt.reset_timeout();
    }

    return ctxt;
}

static void rcdrive( const point_rel_ms &d )
{
    Character &player_character = get_player_character();
    map &here = get_map();
    diag_value const *car_location = player_character.maybe_get_value( "remote_controlling" );

    if( !car_location ) {
        //no turned radio car found
        add_msg( m_warning, _( "No radio car connected." ) );
        return;
    }
    // FIXME: migrate to abs
    tripoint_bub_ms c{ car_location->tripoint().raw() };

    auto rc_pairs = here.get_rc_items( c );
    auto rc_pair = rc_pairs.begin();
    for( ; rc_pair != rc_pairs.end(); ++rc_pair ) {
        if( rc_pair->second->has_flag( flag_RADIOCAR ) && rc_pair->second->active ) {
            break;
        }
    }
    if( rc_pair == rc_pairs.end() ) {
        add_msg( m_warning, _( "No radio car connected." ) );
        player_character.remove_value( "remote_controlling" );
        return;
    }
    item *rc_car = rc_pair->second;

    tripoint_bub_ms dest( c + d );
    if( here.impassable( dest ) || !here.can_put_items_ter_furn( dest ) ||
        here.has_furn( dest ) ) {
        sounds::sound( dest, 7, sounds::sound_t::combat,
                       _( "sound of a collision with an obstacle." ), true, "misc", "rc_car_hits_obstacle" );
        return;
    } else if( !here.add_item_or_charges( dest, *rc_car ).is_null() ) {
        tripoint_bub_ms src( c );
        //~ Sound of moving a remote controlled car
        sounds::sound( src, 6, sounds::sound_t::movement, _( "zzz…" ), true, "misc", "rc_car_drives" );
        player_character.mod_moves( -to_moves<int>( 1_seconds ) * 0.5 );
        here.i_rem( src, rc_car );
        // FIXME: migrate to abs
        player_character.set_value( "remote_controlling", tripoint_abs_ms{ dest.raw() } );
        return;
    }
}

static void pldrive( const tripoint_rel_ms &p )
{
    if( !g->check_safe_mode_allowed() ) {
        return;
    }
    vehicle *veh = g->remoteveh();
    bool remote = true;
    int part = -1;
    Character &player_character = get_player_character();
    map &here = get_map();
    if( !veh ) {
        if( const optional_vpart_position vp = here.veh_at( player_character.pos_bub() ) ) {
            veh = &vp->vehicle();
            part = vp->part_index();
        }
        remote = false;
    }
    if( !veh ) {
        dbg( D_ERROR ) << "game::pldrive: can't find vehicle!  Drive mode is now off.";
        debugmsg( "game::pldrive error: can't find vehicle!  Drive mode is now off." );
        player_character.in_vehicle = false;
        return;
    }
    if( veh->is_on_ramp && p.x() != 0 ) {
        add_msg( m_bad, _( "You can't turn the vehicle while on a ramp." ) );
        return;
    }
    if( !remote ) {
        const vehicle_part &vp = veh->part( part );
        const bool has_animal_controls = veh->part_with_feature( vp.mount, "CONTROL_ANIMAL", true ) >= 0;
        const bool has_controls = veh->part_with_feature( vp.mount, "CONTROLS", true ) >= 0;
        const bool has_animal = veh->has_engine_type( fuel_type_animal, false ) &&
                                veh->get_harnessed_animal( here );
        if( !has_controls && !has_animal_controls ) {
            add_msg( m_info, _( "You can't drive the vehicle from here.  You need controls!" ) );
            player_character.controlling_vehicle = false;
            return;
        } else if( !has_controls && has_animal_controls && !has_animal ) {
            add_msg( m_info, _( "You can't drive this vehicle without an animal to pull it." ) );
            player_character.controlling_vehicle = false;
            return;
        }
    } else {
        if( empty( veh->get_avail_parts( "REMOTE_CONTROLS" ) ) ) {
            add_msg( m_info, _( "Can't drive this vehicle remotely.  It has no working controls." ) );
            return;
        }
    }
    if( p.z() != 0 ) {
        if( !veh->can_control_in_air( player_character ) ) {
            player_character.add_msg_if_player( m_info, _( "You have no idea how to make the vehicle fly." ) );
            return;
        }
        if( !veh->is_flyable() ) {
            player_character.add_msg_if_player( m_info, _( "This vehicle doesn't look very airworthy." ) );
            return;
        }
    }
    if( p.z() == -1 ) {
        if( veh->check_heli_descend( here, player_character ) ) {
            player_character.add_msg_if_player( m_info, _( "You steer the vehicle into a descent." ) );
        } else {
            return;
        }
    } else if( p.z() == 1 ) {
        if( veh->check_heli_ascend( here,  player_character ) ) {
            player_character.add_msg_if_player( m_info, _( "You steer the vehicle into an ascent." ) );
        } else {
            return;
        }
    }
    if( !veh->is_flying_in_air() ) {
        if( !veh->can_control_on_land( player_character ) ) {
            player_character.add_msg_if_player( m_info, _( "You have no idea how to make the vehicle move." ) );
            return;
        }
    }
    veh->pldrive( here, get_avatar(), p.x(), p.y(), p.z() );
}

static void pldrive( point_rel_ms d )
{
    pldrive( tripoint_rel_ms( d, 0 ) );
}

static void open( const std::optional<tripoint_bub_ms> &p = std::nullopt )
{
    map &here = get_map();

    avatar &player_character = get_avatar();
    std::optional<tripoint_bub_ms> openp_ = p;
    if( !openp_ ) {
        openp_ = choose_adjacent_highlight( here, _( "Open where?" ),
                                            pgettext( "no door, gate, curtain, etc.", "There is nothing that can be opened nearby." ),
                                            ACTION_OPEN, false );
    }

    if( !openp_ ) {
        return;
    }
    const tripoint_bub_ms openp = *openp_;

    player_character.mod_moves( -to_moves<int>( 1_seconds ) );

    // Is a vehicle part here?
    if( const optional_vpart_position vp = here.veh_at( openp ) ) {
        vehicle *const veh = &vp->vehicle();
        // Check for potential thievery, and restore moves if action is canceled
        if( !veh->handle_potential_theft( player_character ) ) {
            player_character.mod_moves( to_moves<int>( 1_seconds ) );
            return;
        }
        // Check if vehicle has a part here that can be opened
        int openable = veh->next_part_to_open( vp->part_index() );
        if( openable >= 0 ) {
            // If player is inside vehicle, open the door/window/curtain
            const vehicle *player_veh = veh_pointer_or_null( here.veh_at( player_character.pos_bub() ) );
            const std::string part_name = veh->part( openable ).name();
            bool outside = !player_veh || player_veh != veh;
            if( !outside ) {
                veh->open( here, openable );
                //~ %1$s - vehicle name, %2$s - part name
                player_character.add_msg_if_player( _( "You open the %1$s's %2$s." ), veh->name, part_name );
            } else {
                // Outside means we check if there's anything in that tile outside-openable.
                // If there is, we open everything on tile. This means opening a closed,
                // curtained door from outside is possible, but it will magically open the
                // curtains as well.
                int outside_openable = veh->next_part_to_open( vp->part_index(), true );
                if( outside_openable == -1 ) {
                    add_msg( m_info, _( "That %s can only be opened from the inside." ), part_name );
                    player_character.mod_moves( to_moves<int>( 1_seconds ) );
                } else {
                    veh->open_all_at( here, openable );
                    //~ %1$s - vehicle name, %2$s - part name
                    player_character.add_msg_if_player( _( "You open the %1$s's %2$s." ), veh->name, part_name );
                }
            }
        } else {
            // If there are any OPENABLE parts here, they must be already open or locked
            if( const std::optional<vpart_reference> openable_part = vp.part_with_feature( "OPENABLE",
                    true ); openable_part.has_value() ) {
                const std::string name = openable_part->info().name();
                if( vp->vehicle().part( openable_part->part_index() ).locked ) {
                    add_msg( m_info, _( "That %s is locked." ), name );
                } else {
                    add_msg( m_info, _( "That %s is already open." ), name );
                }
            }
            player_character.mod_moves( to_moves<int>( 1_seconds ) );
        }
        return;
    }
    // Not a vehicle part, just a regular door
    bool didit = here.open_door( player_character, openp,
                                 !here.is_outside( player_character.pos_bub() ) );
    if( didit ) {
        player_character.add_msg_if_player( _( "You open the %s." ), here.name( openp ) );
    } else {
        const ter_str_id tid = here.ter( openp ).id();

        if( here.has_flag( ter_furn_flag::TFLAG_LOCKED, openp ) ) {
            add_msg( m_info, _( "The door is locked!" ) );
            return;
        } else if( tid.obj().close ) {
            // if the following message appears unexpectedly, the prior check was for t_door_o
            add_msg( m_info, _( "The door is already open." ) );
            player_character.mod_moves( to_moves<int>( 1_seconds ) );
            return;
        }
        add_msg( m_info, _( "No door there." ) );
        player_character.mod_moves( to_moves<int>( 1_seconds ) );
    }
}

static void close( const std::optional<tripoint_bub_ms> &p = std::nullopt )
{
    map &here = get_map();

    std::optional<tripoint_bub_ms> pnt = p;
    if( !pnt ) {
        pnt = choose_adjacent_highlight(
                  here, _( "Close where?" ),
                  pgettext( "no door, gate, etc.", "There is nothing that can be closed nearby." ),
                  ACTION_CLOSE, false );
    }

    if( pnt ) {
        doors::close_door( here, get_player_character(), *pnt );
    }
}

static void set_next_option( const std::string &option )
{
    get_options().get_option( option ).setNext();
    get_options().save();
    add_msg( _( "Set %s to %s." ),
             get_options().get_option( option ).getMenuText(),
             get_options().get_option( option ).getValueName() );
}

static void auto_features_warn()
{
    if( !get_option<bool>( "AUTO_FEATURES" ) ) {
        const options_manager::cOpt &auto_features = get_options().get_option( "AUTO_FEATURES" );
        add_msg( _( "Warning: Options > %s > %s > %s set to %s." ),
                 auto_features.getPage(),
                 auto_features.getGroupName(),
                 auto_features.getMenuText(),
                 auto_features.getValueName()  // false in locale
               );
    }
}

// Establish or release a grab on a vehicle
static void grab( const std::optional<tripoint_bub_ms> &p = std::nullopt )
{
    avatar &you = get_avatar();
    map &here = get_map();

    if( you.get_grab_type() != object_type::NONE ) {
        if( const optional_vpart_position vp = here.veh_at( you.pos_bub() + you.grab_point ) ) {
            add_msg( _( "You release the %s." ), vp->vehicle().name );
        } else if( here.has_furn( you.pos_bub() + you.grab_point ) ) {
            add_msg( _( "You release the %s." ), here.furnname( you.pos_bub() + you.grab_point ) );
        }

        you.grab( object_type::NONE );
        return;
    }

    std::optional<tripoint_bub_ms> grabp_ = p;
    if( !grabp_ ) {
        grabp_ = choose_adjacent( _( "Grab where?" ) );
    }
    if( !grabp_ ) {
        add_msg( _( "Never mind." ) );
        return;
    }
    tripoint_bub_ms grabp = *grabp_;

    if( grabp == you.pos_bub() ) {
        add_msg( _( "You get a hold of yourself." ) );
        you.grab( object_type::NONE );
        return;
    }

    const optional_vpart_position &vp = here.veh_at( grabp );
    // Object might not be on the same z level if on a ramp.
    if( !( vp || here.has_furn( grabp ) ) ) {
        if( here.has_flag( ter_furn_flag::TFLAG_RAMP_UP, grabp ) ||
            here.has_flag( ter_furn_flag::TFLAG_RAMP_UP, you.pos_bub() ) ) {
            grabp.z() += 1;
        } else if( here.has_flag( ter_furn_flag::TFLAG_RAMP_DOWN, grabp ) ||
                   here.has_flag( ter_furn_flag::TFLAG_RAMP_DOWN, you.pos_bub() ) ) {
            grabp.z() -= 1;
        }
    }

    if( vp ) {
        std::string veh_name = vp->vehicle().name;
        if( !vp->vehicle().handle_potential_theft( you ) ) {
            return;
        }
        if( vp.part_with_feature( VPFLAG_WALL_MOUNTED, false ) ) {
            add_msg( m_info, _( "You can't move that, it's attached to the wall." ) );
            return;
        }
        // Powergrids with more than one part are undraggable.
        // Offer to split the targeted part off onto its own, making it draggable.
        if( vp->vehicle().is_powergrid() && vp->vehicle().part_count() > 1 ) {
            if( !query_yn(
                    _( "That's part of a power grid.  Separate it from the grid so you can move it?" ) ) ) {
                return;
            }
            get_player_character().pause();
            vp->vehicle().separate_from_grid( &here, vp.value().mount_pos() );
            if( const optional_vpart_position &split_vp = here.veh_at( grabp ) ) {
                veh_name = split_vp->vehicle().name;
            } else {
                debugmsg( "Lost the part to drag after splitting power grid!" );
                return;
            }
        }
        if( vp->has_loaded_furniture() ) {
            furn_str_id furn( vp->part_with_feature( "FURNITURE_TIEDOWN",
                              true )->part().get_base().get_var( "tied_down_furniture" ) );
            //~ %1$s - furniture name, %2$s - vehicle name
            if( query_yn( _( "Grab %1$s on %2$s?" ), furn->name(), veh_name ) ) {
                you.grab( object_type::FURNITURE_ON_VEHICLE, grabp - you.pos_bub() );
                add_msg( m_info, _( "You grab the %1$s loaded on the %2$s." ), furn->name(), veh_name );
                return;
            }
        }
        //solid vehicles can't be grabbed while boarded
        const optional_vpart_position vp_boarded = here.veh_at( you.pos_bub() );
        if( vp_boarded ) {
            if( &vp_boarded->vehicle() == &vp->vehicle() &&
                !empty( vp->vehicle().get_avail_parts( VPFLAG_OBSTACLE ) ) ) {
                add_msg( m_info, _( "You can't move the %s while you're boarding it." ), veh_name );
                return;
            }
        }
        you.grab( object_type::VEHICLE, grabp - you.pos_bub() );
        add_msg( _( "You grab the %s." ), veh_name );
    } else if( here.has_furn( grabp ) ) { // If not, grab furniture if present
        if( !here.furn( grabp ).obj().is_movable() ) {
            add_msg( _( "You can not grab the %s." ), here.furnname( grabp ) );
            return;
        }
        if( !g->warn_player_maybe_anger_local_faction( true ) ) {
            return; // player declined to mess with faction's stuff
        }
        you.grab( object_type::FURNITURE, grabp - you.pos_bub() );
        if( !here.can_move_furniture( grabp, &you ) ) {
            add_msg( _( "You grab the %s. It feels really heavy." ), here.furnname( grabp ) );
        } else {
            add_msg( _( "You grab the %s." ), here.furnname( grabp ) );
        }
    } else { // TODO: grab mob? Captured squirrel = pet (or meat that stays fresh longer).
        add_msg( m_info, _( "There's nothing to grab there!" ) );
    }
}

static void haul()
{
    Character &player_character = get_player_character();

    uilist menu;

    bool hauling = player_character.is_hauling();
    bool autohaul = player_character.is_autohauling();
    std::vector<item_location> &haul_list = player_character.haul_list;
    std::vector<item_location> haulable_items = get_map().get_haulable_items(
                player_character.pos_bub() );
    int haul_qty = haul_list.size();
    std::string &haul_filter = player_character.hauling_filter;


    std::string help_header =
        _( "Select items on current tile to haul with you as you move.\nIf autohaul is enabled, you will automatically add encountered items to the haul.\nYou can set a filter to limit which items are automatically added." );

    std::string status_header;
    if( hauling && autohaul ) {
        if( haul_qty == 0 ) {
            status_header =
                _( "You are currently not hauling any items, but autohaul is enabled, so any new items encountered on the ground will be hauled." );
        } else {
            status_header = string_format(
                                _( "You are currently hauling %d items.\nAutohaul is enabled, so any new items encountered on the ground will be hauled." ),
                                haul_qty );
        }
    }

    if( hauling && !autohaul ) {
        if( haul_qty == 0 ) {
            debugmsg( "Invalid hauling state: hauling enabled, nothing is being hauled, autohaul is off." );
        } else {
            status_header = string_format( _( "You are currently hauling %d items.\nAutohaul is disabled." ),
                                           haul_qty );
        }
    }

    if( !hauling ) {
        status_header =
            _( "You are currently not hauling.  Select items to haul or enable autohaul to start." );
    }

    if( status_header.empty() ) {
        debugmsg( "Failed to construct status header for haul interface" );
    }

    status_header += haul_filter.empty() ? _( "\nAutohaul filter not set." ) : string_format(
                         _( "\nAutohaul filter: %s" ), haul_filter );

    menu.text = help_header + "\n\n" + status_header;

    input_context ctxt = get_default_mode_input_context();
    std::vector<input_event> haul_keys = ctxt.keys_bound_to( "haul" );
    int haul_key = haul_keys.empty() ? '\\' : haul_keys.front().sequence.front();

    menu.entries.emplace_back( 0, hauling, hauling ? haul_key : 'h', _( "Stop hauling" ) );
    menu.entries.emplace_back( 1, !haulable_items.empty(), !hauling ? haul_key : 'h',
                               _( "Haul everything here" ) );
    menu.entries.emplace_back( 2, !haulable_items.empty(), 'p', _( "Choose items to haul" ) );
    menu.entries.emplace_back( 3, true, 'a',
                               autohaul ? _( "Disable autohaul" ) : _( "Enable autohaul" ) );
    menu.entries.emplace_back( 4, true, 'f', _( "Edit autohaul filter" ) );
    menu.entries.emplace_back( 9, true, 'c', _( "Cancel" ) );

    menu.query();

    switch( menu.ret ) {
        case 0:
            player_character.stop_hauling();
            break;
        case 1:
            player_character.start_hauling( haulable_items );
            break;
        case 2: {
            inventory_haul_selector selector( player_character );
            selector.add_map_items( player_character.pos_bub() );
            selector.apply_selection( player_character.haul_list );
            selector.set_title( _( "Select items to haul" ) );
            selector.set_hint( _( "To select items, type a number before selecting." ) );
            drop_locations result = selector.execute( true );
            haulable_items.clear();
            for( const drop_location &dl : result ) {
                haulable_items.push_back( dl.first );
            }
            if( haulable_items != player_character.haul_list ) {
                player_character.suppress_autohaul = true;
            }
            player_character.start_hauling( haulable_items );
        }
        break;
        case 3:
            autohaul ? player_character.stop_autohaul() : player_character.start_autohaul();
            break;
        case 4: {
            string_input_popup_imgui filter_popup( 50, player_character.hauling_filter );
            filter_popup.set_label( _( "Filter:" ) );
            filter_popup.set_description( item_filter_rule_string( item_filter_type::FILTER ), c_white, true );
            filter_popup.set_identifier( "item_filter" );
            player_character.hauling_filter = filter_popup.query();
        }
        break;
        case 9:
        default:
            break;
    }
}

static void haul_toggle()
{
    get_avatar().toggle_hauling();
}

static void smash( const std::optional<tripoint_bub_ms> &p = std::nullopt )
{
    const bool allow_floor_bash = debug_mode; // Should later become "true"
    std::optional<tripoint_bub_ms> smashp_ = p;
    if( !smashp_ ) {
        smashp_ = choose_adjacent( _( "Smash where?" ),
                                   allow_floor_bash );
    }
    if( !smashp_ ) {
        return;
    }
    tripoint_bub_ms smashp = *smashp_;

    // Little hack: If there's a smashable corpse, it'll always be bashed first. So don't bother warning about
    // terrain smashing unless it's actually possible.
    bool smashable_corpse_at_target = false;
    for( const item &maybe_corpse : get_map().i_at( smashp ) ) {
        if( maybe_corpse.can_revive() ) {
            smashable_corpse_at_target = true;
            break;
        }
    }

    if( !smashable_corpse_at_target && !g->warn_player_maybe_anger_local_faction( true ) ) {
        return; // player declined to smash faction's stuff
    }

    avatar &player_character = get_avatar();
    avatar::smash_result res = player_character.smash( smashp );
    if( res.did_smash && !res.success && res.can_smash &&
        query_yn( _( "Keep smashing until destroyed?" ) ) ) {
        player_character.assign_activity( bash_activity_actor( smashp ) );
    }
}

avatar::smash_result avatar::smash( tripoint_bub_ms &smashp )
{
    avatar::smash_result ret;
    ret.can_smash = false;
    ret.did_smash = false;
    ret.success = false;

    map &here = get_map();
    if( is_mounted() ) {
        auto *mons = mounted_creature.get();
        if( mons->has_flag( mon_flag_RIDEABLE_MECH ) ) {
            if( !mons->check_mech_powered() ) {
                add_msg( m_bad, _( "Your %s refuses to move as its batteries have been drained." ),
                         mons->get_name() );
                return ret;
            }
        }
    }
    const int move_cost = !is_armed() ? 80 :
                          get_wielded_item()->attack_time( *this ) * 0.8;
    bool mech_smash = false;
    if( is_mounted() ) {
        mech_smash = true;
    }

    bool smash_floor = false;
    if( smashp.z() != posz() ) {
        if( smashp.z() > posz() ) {
            // TODO: Knock on the ceiling
            return ret;
        }

        smashp.z() = posz();
        smash_floor = true;
    }

    get_event_bus().send<event_type::character_smashes_tile>(
        getID(), here.ter( smashp ).id(), here.furn( smashp ).id() );
    if( is_mounted() ) {
        monster *crit = mounted_creature.get();
        if( crit->has_flag( mon_flag_RIDEABLE_MECH ) ) {
            crit->use_mech_power( 3_kJ );
        }
    }
    std::map<damage_type_id, int> smash_damage = smash_ability();
    for( std::pair<const field_type_id, field_entry> &fd_to_smsh : here.field_at( smashp ) ) {
        const std::optional<map_fd_bash_info> &bash_info = fd_to_smsh.first->bash_info;
        if( !bash_info ) {
            continue;
        }
        int damage = bash_info->damage_to( smash_damage );
        if( ( damage <= 0 && one_in( 10 ) ) || fd_to_smsh.first->indestructible ) {
            add_msg( m_neutral, _( "You don't seem to be damaging the %s." ), fd_to_smsh.first->get_name() );
            ret.did_smash = true;
        } else if( damage > 0 && x_in_y( damage, bash_info->hp() ) ) {
            sounds::sound( smashp, bash_info->sound_vol, sounds::sound_t::combat, bash_info->sound, true,
                           "smash",
                           "field" );
            here.remove_field( smashp, fd_to_smsh.first );
            here.spawn_items( smashp, item_group::items_from( bash_info->drop_group, calendar::turn ) );
            if( !bash_info->destroyed_field.first.is_null() ) {
                here.add_field( smashp, bash_info->destroyed_field.first, bash_info->destroyed_field.second );
            }
            mod_moves( - bash_info->fd_bash_move_cost );
            add_msg( m_info, bash_info->field_bash_msg_success.translated() );
            ret.did_smash = true;
            ret.success = true;
        } else {
            sounds::sound( smashp, bash_info->sound_fail_vol, sounds::sound_t::combat, bash_info->sound_fail,
                           true, "smash",
                           "field" );

            ret.can_smash = bash_info->damage_to( smash_damage ) > 0;
            ret.did_smash = true;
        }
        if( ret.did_smash && !bash_info->hit_field.first.is_null() ) {
            here.add_field( smashp, bash_info->hit_field.first, bash_info->hit_field.second );
        }
        return ret;
    }

    bool should_pulp = false;
    for( const item &maybe_corpse : here.i_at( smashp ) ) {
        if( maybe_corpse.can_revive() ) {
            should_pulp = true; // There is at least one corpse to pulp
        }
    }

    if( should_pulp ) {
        assign_activity( pulp_activity_actor( here.get_abs( smashp ) ) );
        return ret; // don't smash terrain if we've smashed a corpse
    }

    vehicle *veh = veh_pointer_or_null( here.veh_at( smashp ) );
    if( veh != nullptr ) {
        if( !veh->handle_potential_theft( *this ) ) {
            return ret;
        }
    }

    if( !has_weapon() ) {
        std::pair<bodypart_id, int> best_part_to_smash = this->best_part_to_smash();
        if( best_part_to_smash.first != bodypart_str_id::NULL_ID() && here.is_bashable( smashp ) ) {
            std::string name_to_bash = _( "thing" );
            if( here.is_bashable_furn( smashp ) ) {
                name_to_bash = here.furnname( smashp );
            } else if( here.is_bashable_ter( smashp ) ) {
                name_to_bash = here.tername( smashp );
            }
            if( !best_part_to_smash.first->smash_message.empty() ) {
                add_msg( best_part_to_smash.first->smash_message, name_to_bash );
            } else {
                //~ %1$s - bodypart name in accusative, %2$s - furniture/terrain name
                add_msg( _( "You use your %1$s to smash the %2$s." ),
                         body_part_name_accusative( best_part_to_smash.first ), name_to_bash );
            }
        }
    }

    const bash_params bash_result = here.bash( smashp, smash_damage, false, false, smash_floor );
    ret.can_smash = bash_result.can_bash;
    // Weariness scaling
    float weary_mult = 1.0f;
    item_location weapon = used_weapon();
    if( bash_result.did_bash ) {
        ret.did_smash = true;
        if( !mech_smash ) {
            set_activity_level( MODERATE_EXERCISE );
            handle_melee_wear( weapon );
            weary_mult = 1.0f / exertion_adjusted_move_multiplier( MODERATE_EXERCISE );

            const int mod_sta = 2 * get_standard_stamina_cost();
            burn_energy_arms( mod_sta );

            if( static_cast<int>( get_skill_level( skill_melee ) ) == 0 ) {
                practice( skill_melee, rng( 0, 1 ) * rng( 0, 1 ) );
            }
            if( weapon ) {
                const int glass_portion = weapon->made_of( material_glass );
                float glass_fraction = glass_portion / static_cast<float>( weapon->type->mat_portion_total );
                if( std::isnan( glass_fraction ) || glass_fraction > 1.f ) {
                    glass_fraction = 0.f;
                }
                const int vol = weapon->volume() * glass_fraction / 250_ml;
                if( glass_portion && rng( 0, vol + 3 ) < vol ) {
                    add_msg( m_bad, _( "Your %s shatters!" ), weapon->tname() );
                    weapon->spill_contents( pos_bub() );
                    sounds::sound( pos_bub(), 24, sounds::sound_t::combat, "CRACK!", true, "smash",
                                   "glass" );
                    deal_damage( nullptr, bodypart_id( "hand_r" ), damage_instance( damage_cut,
                                 rng( 0,
                                      vol ) ) );
                    if( vol > 20 ) {
                        // Hurt left arm too, if it was big
                        deal_damage( nullptr, bodypart_id( "hand_l" ), damage_instance( damage_cut,
                                     rng( 0, static_cast<int>( vol * .5 ) ) ) );
                    }
                    remove_weapon();
                    check_dead_state( &here );
                }
            }
        }
        mod_moves( -move_cost * weary_mult );
        recoil = MAX_RECOIL;

        if( bash_result.success ) {
            ret.success = true;
            // Bash results in destruction of target
            g->draw_async_anim( smashp, "bash_complete", "X", c_light_gray );
        } else if( bash_result.did_bash ) {
            // Bash effective but target not yet destroyed
            g->draw_async_anim( smashp, "bash_effective", "/", c_light_gray );
        } else {
            // Bash not effective
            g->draw_async_anim( smashp, "bash_ineffective" );
            if( one_in( 10 ) ) {
                if( here.has_furn( smashp ) && here.furn( smashp ).obj().bash ) {
                    // %s is the smashed furniture
                    add_msg( m_neutral, _( "You don't seem to be damaging the %s." ), here.furnname( smashp ) );
                } else {
                    // %s is the smashed terrain
                    add_msg( m_neutral, _( "You don't seem to be damaging the %s." ), here.tername( smashp ) );
                }
            }
        }

    } else {
        add_msg( _( "There's nothing there to smash!" ) );
    }

    return ret;
}

static int try_set_alarm()
{
    uilist as_m;
    const bool already_set = get_player_character().has_effect( effect_alarm_clock );

    as_m.text = already_set ?
                _( "You already have an alarm set.  What do you want to do?" ) :
                _( "You have an alarm clock.  What do you want to do?" );

    as_m.entries.emplace_back( 0, true, 'w', already_set ?
                               _( "Keep the alarm and wait a while" ) :
                               _( "Wait a while" ) );
    as_m.entries.emplace_back( 1, true, 'a', already_set ?
                               _( "Change your alarm" ) :
                               _( "Set an alarm for later" ) );
    // These two entries are recreated for every wait.  Their generic uilist
    // identities are deliberately process-local, so bind the native logical
    // choices explicitly for semantic clients which must safely repeat a
    // bounded wait.  Other uilists retain their generic identities.
    as_m.entries[0].semantic_stable_id = "wait-mode:wait-a-while";
    as_m.entries[1].semantic_stable_id = "wait-mode:set-alarm";
    as_m.query();

    return as_m.ret;
}

static std::string openclaw_harness_wait_duration_action( bool setting_alarm, int retval )
{
    if( setting_alarm && retval >= 0 && retval <= 9 ) {
        return retval == 0 ? "alarm.30m" : string_format( "alarm.%dh", retval );
    }
    if( retval == 13 ) {
        return setting_alarm ? "alarm.cancel" : "wait.weather_change";
    }
    if( retval == 14 ) {
        return "wait.catch_breath";
    }
    if( retval == 15 ) {
        return "wait.followers";
    }
    const std::map<int, std::string> duration_names = {
        { 1, "20s" }, { 2, "1m" }, { 3, "5m" }, { 4, "30m" }, { 5, "1h" }, { 6, "2h" },
        { 7, "3h" }, { 8, "6h" }, { 9, "daylight" }, { 10, "noon" },
        { 11, "night" }, { 12, "midnight" }
    };
    const auto found = duration_names.find( retval );
    if( found == duration_names.end() ) {
        return {};
    }
    return std::string( setting_alarm ? "alarm." : "wait." ) + found->second;
}

static void wait()
{
    std::map<int, time_duration> durations;
    std::map<int, char> semantic_hotkeys;
    uilist as_m;
    Character &player_character = get_player_character();
    bool setting_alarm = false;
    map &here = get_map();

    if( player_character.controlling_vehicle ) {
        const vehicle &veh = here.veh_at( player_character.pos_bub() )->vehicle();
        if( !veh.can_use_rails( here ) && ( // control optional if on rails
                veh.is_flying_in_air() ||   // control required: fuel is consumed even at hover
                veh.is_falling ||           // *not* vertical_velocity, which is only used for collisions
                veh.velocity ||             // is moving
                ( veh.cruise_velocity && (  // would move if it could
                      ( veh.is_watercraft() && veh.can_float( here ) ) || // is viable watercraft floating on water
                      veh.sufficient_wheel_config() // is viable land vehicle on ground or fording shallow water
                  ) ) ||
                ( veh.is_in_water( true ) && !veh.can_float( here ) ) // is sinking in deep water
            ) ) {
            std::optional<semantic_surface_scope> semantic_scope;
            if( semantic_surface_manager *manager = active_semantic_surface_manager() ) {
                semantic_scope.emplace( *manager, "unsupported", "Wait unavailable",
                                        std::map<std::string, std::string>{
                    { "stop_reason", "moving vehicle wait prompt lacks semantic bindings" }
                } );
                semantic_scope->consume_request();
            }
            popup( _( "You can't pass time while controlling a moving vehicle." ) );
            return;
        }
    }

    if( player_character.has_alarm_clock() ) {
        int alarm_query = try_set_alarm();
        if( alarm_query == UILIST_CANCEL ) {
            return;
        }
        setting_alarm = alarm_query == 1;
    }

    const bool has_watch = player_character.has_watch() || setting_alarm;

    const auto add_menu_item = [ &as_m, &durations, &semantic_hotkeys, has_watch, setting_alarm ]
                               ( int retval, int hotkey, const std::string &caption = "",
    const time_duration &duration = time_duration::from_turns( calendar::INDEFINITELY_LONG ) ) {

        std::string text( caption );

        if( has_watch && duration != time_duration::from_turns( calendar::INDEFINITELY_LONG ) ) {
            const std::string dur_str( to_string( duration ) );
            text += ( text.empty() ? dur_str : string_format( " (%s)", dur_str ) );
        }
        as_m.addentry( retval, true, hotkey, text );
        const std::string action_id = openclaw_harness_wait_duration_action( setting_alarm, retval );
        if( !action_id.empty() ) {
            // Like the parent choice, duration entries are reconstructed for
            // each wait.  Use their native logical operation as the stable
            // identity; validation still requires one enabled exact match.
            as_m.entries.back().semantic_stable_id = "wait-duration:" + action_id;
        }
        durations.emplace( retval, duration );
        semantic_hotkeys.emplace( retval, static_cast<char>( hotkey ) );
    };

    if( setting_alarm ) {

        add_menu_item( 0, '0', "", 30_minutes );

        for( int i = 1; i <= 9; ++i ) {
            add_menu_item( i, '0' + i, "", i * 1_hours );
        }

    } else {
        if( player_character.get_stamina() < player_character.get_stamina_max() ) {
            as_m.addentry( 14, true, 'w', _( "Wait until you catch your breath" ) );
            durations.emplace( 14, 15_minutes ); // to hide it from showing
            semantic_hotkeys.emplace( 14, 'w' );
        }
        if( !wait_followers_activity_actor::get_absent_followers( player_character ).empty() ) {
            as_m.addentry( 15, true, 'f', _( "Wait for followers to catch up" ) );
            durations.emplace( 15, 15_minutes ); // to hide it from showing(?)
            semantic_hotkeys.emplace( 15, 'f' );
        }
        add_menu_item( 1, '1', !has_watch ? _( "Wait 20 heartbeats" ) : "", 20_seconds );
        add_menu_item( 2, '2', !has_watch ? _( "Wait 60 heartbeats" ) : "", 1_minutes );
        add_menu_item( 3, '3', !has_watch ? _( "Wait 300 heartbeats" ) : "", 5_minutes );
        add_menu_item( 4, '4', !has_watch ? _( "Wait 1800 heartbeats" ) : "", 30_minutes );

        if( has_watch ) {
            add_menu_item( 5, '5', "", 1_hours );
            add_menu_item( 6, '6', "", 2_hours );
            add_menu_item( 7, '7', "", 3_hours );
            add_menu_item( 8, '8', "", 6_hours );
        }
    }

    if( here.get_abs_sub().z() >= 0 || has_watch ) {
        const time_point last_midnight = calendar::turn - time_past_midnight( calendar::turn );
        const auto diurnal_time_before = []( const time_point & p ) {
            // Either the given time is in the future (e.g. waiting for sunset while it's early morning),
            // than use it directly. Otherwise (in the past), add a single day to get the same time tomorrow
            // (e.g. waiting for sunrise while it's noon).
            const time_point target_time = p > calendar::turn ? p : p + 1_days;
            return target_time - calendar::turn;
        };

        add_menu_item( 9,  'd',
                       setting_alarm ? _( "Set alarm for dawn" ) : _( "Wait till daylight" ),
                       diurnal_time_before( daylight_time( calendar::turn ) ) );
        add_menu_item( 10,  'n',
                       setting_alarm ? _( "Set alarm for noon" ) : _( "Wait till noon" ),
                       diurnal_time_before( last_midnight + 12_hours ) );
        add_menu_item( 11,  'k',
                       setting_alarm ? _( "Set alarm for dusk" ) : _( "Wait till night" ),
                       diurnal_time_before( night_time( calendar::turn ) ) );
        add_menu_item( 12, 'm',
                       setting_alarm ? _( "Set alarm for midnight" ) : _( "Wait till midnight" ),
                       diurnal_time_before( last_midnight ) );
        if( setting_alarm ) {
            if( player_character.has_effect( effect_alarm_clock ) ) {
                add_menu_item( 13, 'x', _( "Cancel the currently set alarm." ),
                               0_turns );
            }
        } else {
            add_menu_item( 13, 'W', _( "Wait till weather changes" ) );
        }
    }

    // NOLINTNEXTLINE(cata-text-style): spaces required for concatenation
    as_m.text = has_watch ? string_format( _( "It's %s now.  " ),
                                           to_string_time_of_day( calendar::turn ) ) : "";
    as_m.text += setting_alarm ? _( "Set alarm for when?" ) : _( "Wait for how long?" );
    std::vector<std::pair<std::string, std::string>> semantic_actions;
    for( const auto &entry : semantic_hotkeys ) {
        const std::string action_id = openclaw_harness_wait_duration_action( setting_alarm, entry.first );
        if( !action_id.empty() ) {
            semantic_actions.emplace_back( action_id, std::string( 1, entry.second ) );
        }
    }
    const std::string semantic_frame = openclaw_harness_semantic_step_frame(
                                           setting_alarm ? "alarm_duration_choice" : "wait_duration_choice",
                                           semantic_actions );
    std::optional<semantic_surface_scope> semantic_scope;
    bool semantic_action_consumed = false;
    if( semantic_surface_manager *manager = active_semantic_surface_manager() ) {
        // This scope is the duration menu's native semantic owner.  Do not
        // let uilist::query create a second generic menu scope below it: a
        // wake can arrive after the first pre-query consume attempt, at
        // which point that duplicate owner would retire the advertised
        // wait-duration surface before it consumed its exact request.
        as_m.semantic_owner = false;
        const std::vector<semantic_action_descriptor> semantic_actions_descriptor =
            semantic_surface_actions( semantic_actions );
        std::map<std::string, int> semantic_action_retvals;
        for( const auto &entry : semantic_hotkeys ) {
            semantic_action_retvals.emplace( openclaw_harness_wait_duration_action( setting_alarm, entry.first ),
                                             entry.first );
        }
        semantic_scope.emplace( *manager, "menu", setting_alarm ? "Set alarm" : "Wait duration",
                                std::map<std::string, std::string>{},
                                semantic_actions_descriptor,
        [ &as_m, semantic_action_retvals ]
        ( const semantic_action_request &request ) {
            const auto selected = semantic_action_retvals.find( request.action_id );
            if( selected == semantic_action_retvals.end() ) {
                return semantic_action_dispatch_result{ false, "unadvertised_action", "" };
            }
            as_m.ret = selected->second;
            return semantic_action_dispatch_result{ true, "", "" };
        } );
        // The duration menu owns input until query returns, so it alone may
        // consume one queued semantic request before blocking for input.
        semantic_action_consumed = semantic_scope->consume_request();
    }
    if( !semantic_action_consumed ) {
        as_m.query(); /* calculate key and window variables, generate window, and loop until we get a valid answer */
    }

    const auto dur_iter = durations.find( as_m.ret );
    const std::string semantic_action = openclaw_harness_wait_duration_action( setting_alarm, as_m.ret );
    openclaw_harness_trace_wait_menu_selection( semantic_action,
                                                dur_iter != durations.end() && !semantic_action.empty() );
    openclaw_harness_semantic_step_receipt( semantic_frame, semantic_action,
                                            dur_iter != durations.end() && !semantic_action.empty() );
    if( dur_iter == durations.end() ) {
        return;
    }
    const time_duration time_to_wait = dur_iter->second;

    if( setting_alarm ) {
        // Setting alarm
        player_character.remove_effect( effect_alarm_clock );
        if( as_m.ret == 13 ) {
            add_msg( _( "You cancel your alarm." ) );
        } else {
            player_character.add_effect( effect_alarm_clock, time_to_wait );
            add_msg( _( "You set your alarm." ) );
        }

    } else {
        // Waiting
        activity_id actType;
        if( as_m.ret == 13 ) {
            player_character.assign_activity( wait_weather_activity_actor() );
        } else if( as_m.ret == 14 ) {
            player_character.assign_activity( wait_stamina_activity_actor() );
        } else if( as_m.ret == 15 ) {
            player_character.assign_activity( wait_followers_activity_actor() );
        } else {
            player_character.assign_activity( wait_activity_actor( time_to_wait ) );
        }
        openclaw_harness_semantic_step_frame( "wait_activity", {} );
    }
}

static void sleep()
{
    avatar &player_character = get_avatar();
    if( has_vehicle_control( player_character ) ) {
        add_msg( m_info, _( "You can't sleep while controlling a vehicle." ) );
        return;
    }
    if( player_character.is_mounted() ) {
        add_msg( m_info, _( "You cannot sleep while mounted." ) );
        return;
    }

    const map &here = get_map();
    const tripoint_bub_ms &p = player_character.pos_bub();
    const optional_vpart_position vp = here.veh_at( p );
    const comfort_data::response &comfort = player_character.get_comfort_at( p );
    if( !vp && comfort.data->human_or_impossible() ) {
        if( here.has_flag( ter_furn_flag::TFLAG_DEEP_WATER, p ) ) {
            add_msg( m_info, _( "You cannot sleep while swimming." ) );
            return;
        }
        if( here.has_flag( ter_furn_flag::TFLAG_NO_FLOOR, p ) ) {
            add_msg( m_info, _( "You cannot sleep while airborne." ) );
            return;
        }
    }

    uilist as_m;
    as_m.title = _( "Are you sure you want to sleep?" );
    // (Y)es/(S)ave before sleeping/(N)o
    as_m.entries.emplace_back( 0, true,
                               get_option<bool>( "FORCE_CAPITAL_YN" ) ? 'Y' : 'y',
                               _( "Yes." ) );
    as_m.entries.emplace_back( 1, g->get_moves_since_last_save(),
                               get_option<bool>( "FORCE_CAPITAL_YN" ) ? 'S' : 's',
                               _( "Yes, and save game before sleeping." ) );
    as_m.entries.emplace_back( 2, true,
                               get_option<bool>( "FORCE_CAPITAL_YN" ) ? 'N' : 'n',
                               _( "No." ) );

    // List all active items, bionics or mutations so player can deactivate them
    std::vector<std::string> active;
    for( item_location &it : player_character.all_items_loc() ) {
        if( it->has_flag( flag_LITCIG ) || ( it->active && it->ammo_sufficient( &player_character ) &&
                                             it->is_tool() && !it->has_flag( flag_SLEEP_IGNORE ) ) ) {
            active.push_back( it->tname() );
        }
    }
    for( int i = 0; i < player_character.num_bionics(); i++ ) {
        const bionic &bio = player_character.bionic_at_index( i );
        if( !bio.powered ) {
            continue;
        }

        // some bionics
        // bio_alarm is useful for waking up during sleeping
        if( bio.info().has_flag( json_flag_BIONIC_SLEEP_FRIENDLY ) ) {
            continue;
        }

        const bionic_data &info = bio.info();
        if( info.power_over_time > 0_kJ ) {
            active.push_back( info.name.translated() );
        }
    }
    for( auto &mut : player_character.get_functioning_mutations() ) {
        const mutation_branch &mdata = mut.obj();
        if( mdata.cost > 0 && player_character.has_active_mutation( mut ) ) {
            active.push_back( player_character.mutation_name( mut ) );
        }
    }

    // check for deactivating any currently played music instrument.
    for( item *&item : player_character.inv_dump() ) {
        if( item->active && item->get_use( "musical_instrument" ) != nullptr ) {
            player_character.add_msg_if_player( _( "You stop playing your %s before trying to sleep." ),
                                                item->tname() );
            // deactivate instrument
            item->active = false;
        }
    }

    // ask for deactivation
    std::stringstream data;
    if( !active.empty() ) {
        as_m.selected = 2;
        data << _( "You may want to extinguish or turn off:" ) << std::endl;
        for( auto &a : active ) {
            data << "<color_red>" << a << "</color>" << std::endl;
        }
        as_m.text = data.str();
    }

    /* Calculate key and window variables, generate window,
       and loop until we get a valid answer. */
    as_m.query();

    if( as_m.ret == 1 ) {
        g->quicksave();
    } else if( as_m.ret == 2 || as_m.ret < 0 ) {
        return;
    }

    if( !g->warn_player_maybe_anger_local_faction( false, true ) ) {
        return; // player declined to annoy locals by illegally sleeping in their territory
    }

    time_duration try_sleep_dur = 24_hours;
    std::string deaf_text;
    if( player_character.is_deaf() && !player_character.has_flag( json_flag_ALARMCLOCK ) ) {
        deaf_text = _( "<color_c_red> (DEAF!)</color>" );
    }
    if( player_character.has_alarm_clock() ) {
        /* Reuse menu to ask player whether they want to set an alarm. */
        bool can_hibernate = player_character.get_hunger() < -60 &&
                             player_character.has_active_mutation( trait_HIBERNATE );

        as_m.reset();
        as_m.text = can_hibernate ?
                    _( "You're engorged to hibernate.  The alarm would only attract attention.  "
                       "Set an alarm anyway?" ) :
                    _( "You have an alarm clock.  Set an alarm?" );
        as_m.text += deaf_text;

        as_m.entries.emplace_back( 0, true,
                                   get_option<bool>( "FORCE_CAPITAL_YN" ) ? 'N' : 'n',
                                   _( "No, don't set an alarm." ) );

        for( int i = 3; i <= 9; ++i ) {
            as_m.entries.emplace_back( i, true, '0' + i,
                                       string_format( _( "Set alarm to wake up in %i hours." ), i ) + deaf_text );
        }

        as_m.query();
        if( as_m.ret >= 3 && as_m.ret <= 9 ) {
            player_character.add_effect( effect_alarm_clock, 1_hours * as_m.ret );
            try_sleep_dur = 1_hours * as_m.ret + 1_turns;
        } else if( as_m.ret < 0 ) {
            return;
        }
    }

    player_character.set_moves( 0 );
    player_character.try_to_sleep( try_sleep_dur );
}

static void loot()
{
    enum ZoneFlags {
        None = 1,
        SortLoot = 2,
        SortLootStatic = 4,
        SortLootPersonal = 8,
        ConstructPlots = 64,
        MultiFarmPlots = 128,
        Multichoptrees = 256,
        Multichopplanks = 512,
        Multideconvehicle = 1024,
        Multirepairvehicle = 2048,
        MultiButchery = 4096,
        MultiMining = 8192,
        MultiDis = 16384,
        MultiMopping = 32768,
        MultiStudy = 131072,
        UnloadLoot = 65536
    };

    Character &player_character = get_player_character();
    int flags = 0;
    zone_manager &mgr = zone_manager::get_manager();

    // reset any potentially disabled zones from a past activity
    mgr.reset_disabled();

    // cache should only happen if we have personal zones defined
    if( mgr.has_personal_zones() ) {
        mgr.cache_data();
    }

    // Manually update vehicle cache.
    // In theory this would be handled by the related activity (zone_sort_activity_actor())
    // but with a stale cache we never get that far.
    mgr.cache_vzones();

    flags |= g->check_near_zone( zone_type_LOOT_UNSORTED,
                                 player_character.pos_bub() ) ? SortLoot : 0;
    flags |= g->check_near_zone( zone_type_UNLOAD_ALL, player_character.pos_bub() ) ||
             g->check_near_zone( zone_type_STRIP_CORPSES, player_character.pos_bub() ) ? UnloadLoot : 0;
    if( g->check_near_zone( zone_type_FARM_PLOT, player_character.pos_bub() ) ) {
        flags |= MultiFarmPlots;
    }
    flags |= g->check_near_zone( zone_type_CONSTRUCTION_BLUEPRINT,
                                 player_character.pos_bub() ) ? ConstructPlots : 0;

    flags |= g->check_near_zone( zone_type_CHOP_TREES,
                                 player_character.pos_bub() ) ? Multichoptrees : 0;
    flags |= g->check_near_zone( zone_type_LOOT_WOOD,
                                 player_character.pos_bub() ) ? Multichopplanks : 0;
    flags |= g->check_near_zone( zone_type_VEHICLE_DECONSTRUCT,
                                 player_character.pos_bub() ) ? Multideconvehicle : 0;
    flags |= g->check_near_zone( zone_type_VEHICLE_REPAIR,
                                 player_character.pos_bub() ) ? Multirepairvehicle : 0;
    flags |= g->check_near_zone( zone_type_LOOT_CORPSE,
                                 player_character.pos_bub() ) ? MultiButchery : 0;
    flags |= g->check_near_zone( zone_type_MINING, player_character.pos_bub() ) ? MultiMining : 0;
    flags |= g->check_near_zone( zone_type_DISASSEMBLE,
                                 player_character.pos_bub() ) ? MultiDis : 0;
    flags |= g->check_near_zone( zone_type_MOPPING, player_character.pos_bub() ) ? MultiMopping : 0;
    flags |= g->check_near_zone( zone_type_STUDY_ZONE, player_character.pos_bub() ) ? MultiStudy : 0;
    if( flags == 0 ) {
        add_msg( m_info, _( "There is no compatible zone nearby." ) );
        add_msg( m_info, _( "Compatible zones are %s and %s" ),
                 mgr.get_name_from_type( zone_type_LOOT_UNSORTED ),
                 mgr.get_name_from_type( zone_type_FARM_PLOT ) );
        return;
    }

    auto wrap60 = []( const std::string & text ) {
        return string_join( foldstring( text, 60 ), "\n" );
    };

    uilist menu;
    menu.title = _( "Pick action:" );
    menu.desc_enabled = true;

    if( flags & SortLoot ) {
        menu.addentry_desc( SortLootStatic, true, 'o', _( "Sort out my loot (static zones only)" ),
                            wrap60( _( "Sorts out the loot from Loot: Unsorted zone to nearby appropriate Loot zones ignoring personal zones.  Uses empty space in your inventory or utilizes a cart, if you are holding one." ) ) );
        menu.addentry_desc( SortLootPersonal, true, 'O', _( "Sort out my loot (personal zones only)" ),
                            wrap60( _( "Sorts out the loot from Loot: Unsorted zone to nearby appropriate Loot zones ignoring static zones.  Uses empty space in your inventory or utilizes a cart, if you are holding one." ) ) );
        menu.addentry_desc( SortLoot, true, 'I', _( "Sort out my loot (all)" ),
                            wrap60( _( "Sorts out the loot from Loot: Unsorted zone to nearby appropriate Loot zones.  Uses empty space in your inventory or utilizes a cart, if you are holding one." ) ) );
    }

    if( flags & UnloadLoot ) {
        menu.addentry_desc( UnloadLoot, true, 'U', _( "Unload nearby containers" ),
                            wrap60( _( "Unloads any corpses or containers that are in their respective zones." ) ) );
    }
    if( flags & ConstructPlots ) {
        menu.addentry_desc( ConstructPlots, true, 'c', _( "Construct plots" ),
                            wrap60( _( "Work on any nearby Blueprint: construction zones." ) ) );
    }
    if( flags & MultiFarmPlots ) {
        menu.addentry_desc( MultiFarmPlots, true, 'm', _( "Farm plots" ),
                            wrap60( _( "Till and plant on any nearby farm plots - auto-fetch seeds and tools." ) ) );
    }
    if( flags & Multichoptrees ) {
        menu.addentry_desc( Multichoptrees, true, 'C', _( "Chop trees" ),
                            wrap60( _( "Chop down any trees in the designated zone - auto-fetch tools." ) ) );
    }
    if( flags & Multichopplanks ) {
        menu.addentry_desc( Multichopplanks, true, 'P', _( "Chop planks" ),
                            wrap60( _( "Auto-chop logs in wood loot zones into planks - auto-fetch tools." ) ) );
    }
    if( flags & Multideconvehicle ) {
        menu.addentry_desc( Multideconvehicle, true, 'v', _( "Deconstruct vehicle" ),
                            wrap60( _( "Auto-deconstruct vehicle in designated zone - auto-fetch tools." ) ) );
    }
    if( flags & Multirepairvehicle ) {
        menu.addentry_desc( Multirepairvehicle, true, 'V', _( "Repair vehicle" ),
                            wrap60( _( "Auto-repair vehicle in designated zone - auto-fetch tools." ) ) );
    }
    if( flags & MultiButchery ) {
        menu.addentry_desc( MultiButchery, true, 'B', _( "Butcher corpses" ),
                            wrap60( _( "Auto-butcher anything in corpse loot zones - auto-fetch tools." ) ) );
    }
    if( flags & MultiMining ) {
        menu.addentry_desc( MultiMining, true, 'M', _( "Mine Area" ),
                            wrap60( _( "Auto-mine anything in mining zone - auto-fetch tools." ) ) );
    }
    if( flags & MultiDis ) {
        menu.addentry_desc( MultiDis, true, 'D', _( "Disassemble items" ),
                            wrap60( _( "Auto-disassemble anything in disassembly zone - auto-fetch tools." ) ) );

    }
    if( flags & MultiMopping ) {
        menu.addentry_desc( MultiMopping, true, 'p', _( "Mop area" ), _( "Mop clean the area." ) );
    }
    if( flags & MultiStudy ) {
        menu.addentry_desc( MultiStudy, true, 's', _( "Study from books in study zones" ),
                            wrap60( _( "Find and read books from study zones." ) ) );
    }

    menu.query();
    flags = ( menu.ret >= 0 ) ? menu.ret : None;
    bool recache = false;

    switch( flags ) {
        case None:
            add_msg( _( "Never mind." ) );
            break;
        case SortLoot:
            player_character.assign_activity( zone_sort_activity_actor() );
            break;
        case SortLootStatic:
            //temporarily disable personal zones
            for( const auto &i : mgr.get_zones() ) {
                zone_data &zone = i.get();
                if( zone.get_is_personal() && zone.get_enabled() ) {
                    zone.set_enabled( false );
                    zone.set_temporary_disabled( true );
                    recache = true;
                }
            }
            if( recache ) {
                mgr.cache_data();
            }
            player_character.assign_activity( zone_sort_activity_actor() );
            break;
        case SortLootPersonal:
            //temporarily disable non personal zones
            for( const auto &i : mgr.get_zones() ) {
                zone_data &zone = i.get();
                if( !zone.get_is_personal() && zone.get_enabled() ) {
                    zone.set_enabled( false );
                    zone.set_temporary_disabled( true );
                    recache = true;
                }
            }
            if( recache ) {
                mgr.cache_data();
            }
            player_character.assign_activity( zone_sort_activity_actor() );
            break;
        case UnloadLoot:
            player_character.assign_activity( unload_loot_activity_actor() );
            break;
        case ConstructPlots:
            player_character.assign_activity( multi_build_construction_activity_actor() );
            break;
        case MultiFarmPlots:
            player_character.assign_activity( multi_farm_activity_actor() );
            break;
        case Multichoptrees:
            player_character.assign_activity( multi_chop_trees_activity_actor() );
            break;
        case Multichopplanks:
            player_character.assign_activity( multi_chop_planks_activity_actor() );
            break;
        case Multideconvehicle:
            player_character.assign_activity( multi_vehicle_deconstruct_activity_actor() );
            break;
        case Multirepairvehicle:
            player_character.assign_activity( multi_vehicle_repair_activity_actor() );
            break;
        case MultiButchery:
            player_character.assign_activity( multi_butchery_activity_actor() );
            break;
        case MultiMining:
            player_character.assign_activity( multi_mine_activity_actor() );
            break;
        case MultiDis:
            player_character.assign_activity( multi_disassemble_activity_actor() );
            break;
        case MultiMopping:
            player_character.assign_activity( multi_mop_activity_actor() );
            break;
        case MultiStudy:
            player_character.assign_activity( multi_study_activity_actor() );
            break;
        default:
            debugmsg( "Unsupported flag" );
            break;
    }
}

static void wear()
{
    avatar &player_character = get_avatar();
    item_location loc = game_menus::inv::wear( player_character );

    if( loc ) {
        player_character.wear( loc );
    } else {
        add_msg( _( "Never mind." ) );
    }
}

static void takeoff()
{
    avatar &player_character = get_avatar();
    item_location loc = game_menus::inv::take_off();

    if( loc ) {
        player_character.takeoff( loc.obtain( player_character ) );
    } else {
        add_msg( _( "Never mind." ) );
    }
}

static void read()
{
    avatar &player_character = get_avatar();
    // Can read items from inventory or within one tile (including in vehicles)
    item_location loc = game_menus::inv::read( player_character );

    if( loc ) {
        item the_book = *loc.get_item();
        if( avatar_action::check_stealing( get_player_character(), the_book ) ) {
            item_location parent_loc = loc.parent_item();
            if( loc->type->can_use( "learn_spell" ) ) {
                the_book.get_use( "learn_spell" )->call( &player_character, the_book, player_character.pos_bub() );
            } else if( loc.is_efile() ) {
                // obtain e-storage device if not allowed to use remotely
                if( !parent_loc->has_flag( json_flag_ALLOWS_REMOTE_USE ) ) {
                    parent_loc = parent_loc.obtain( player_character );
                    item *newit = parent_loc->get_item_with( [&]( const item & it ) {
                        return it.typeId() == the_book.typeId();
                    } );
                    loc = item_location( parent_loc, &*newit );
                }
                player_character.read( loc, parent_loc );
            } else {
                if( ( parent_loc && parent_loc->has_flag( json_flag_NO_UNLOAD ) ) || loc.is_efile() ) {
                    debugmsg( "tried to get an item you shouldn't obtain." );
                }
                loc = loc.obtain( player_character );
                player_character.read( loc );
            }
        }
    } else {
        add_msg( _( "Never mind." ) );
    }
}

// Perform a reach attack
static void reach_attack( avatar &you )
{
    g->temp_exit_fullscreen();

    target_handler::trajectory traj;
    if( you.get_wielded_item() ) {
        traj = target_handler::mode_reach( you, you.get_wielded_item() );
    } else {
        traj = target_handler::mode_unarmed_reach( you );
    }

    if( !traj.empty() ) {
        you.reach_attack( traj.back() );
    }
    g->reenter_fullscreen();
}

static void fire()
{
    avatar &you = get_avatar();
    map &here = get_map();

    if( you.has_flag( json_flag_CANNOT_ATTACK ) ) {
        add_msg( m_info, _( "You are incapable of attacking!" ) );
        return;
    }

    if( !you.try_break_relax_gas( _( "Your willpower asserts itself, and so do you!" ),
                                  _( "You're too pacified to strike anything…" ) ) ) {
        return;
    }

    const item_location weapon = you.get_wielded_item();
    // try reach weapon
    // used_weapon() returns null location if force unarmed is selected
    if( weapon && !you.used_weapon() && !weapon->is_gun() ) {
        add_msg( m_info, _( "You can't use reach attacks while forcing yourself to fight unarmed." ) );
        return;
    }
    if( weapon && !weapon->is_gun() && weapon->current_reach_range( you ).first > 1 ) {
        reach_attack( you );
        return;
    }
    if( !weapon &&
        static_cast<int>( you.calculate_by_enchantment( 1,
                          enchant_vals::mod::MELEE_RANGE_MODIFIER ) ) > 1 ) {
        reach_attack( you );
        return;
    }
    if( you.has_trait( trait_BRAWLER ) ) {
        add_msg( m_bad, _( "You refuse to use ranged weapons." ) );
        return;
    }
    if( you.has_trait( trait_GUNSHY ) && weapon && weapon->is_firearm() ) {
        add_msg( m_bad, _( "You refuse to use firearms." ) );
        return;
    }
    if( you.has_flag( json_flag_TEMPORARY_SHAPESHIFT_NO_HANDS ) ) {
        add_msg( m_bad, _( "You have no hands and cannot use ranged weapons!" ) );
        return;
    }
    if( you.has_flag( json_flag_HANDS_CANNOT_USE_FIREARMS ) && weapon && weapon->is_firearm() ) {
        add_msg( m_bad, _( "Your hands aren't suited for using firearms!" ) );
        return;
    }
    // try firing gun
    if( weapon && weapon->is_gun() && !weapon->gun_current_mode().melee() ) {
        avatar_action::fire_wielded_weapon( you );
        return;
    }
    // try firing turrets
    if( const optional_vpart_position ovp = here.veh_at( you.pos_abs() ) ) {
        if( turret_data turret_here = ovp->vehicle().turret_query( you.pos_abs() ) ) {
            if( avatar_action::fire_turret_manual( you, here, turret_here ) ) {
                return;
            }
        } else if( ovp.part_with_feature( VPFLAG_CONTROLS, true ) ) {
            if( ovp->vehicle().turrets_aim_and_fire_all_manual() ) {
                return;
            }
        }
    }
    // offer to draw a gun from worn holster
    std::vector<std::string> options;
    std::vector<std::function<void()>> actions;
    you.worn.fire_options( you, options, actions );
    if( !options.empty() ) {
        int sel = uilist( _( "Draw what?" ), options );
        if( sel >= 0 ) {
            actions[sel]();
            return;
        }
    }
}

static void open_movement_mode_menu()
{
    avatar &player_character = get_avatar();
    const std::vector<move_mode_id> &modes = move_modes_by_speed();
    const int cycle = 1027;
    uilist as_m;

    as_m.text = _( "Change to which movement mode?" );

    for( size_t i = 0; i < modes.size(); ++i ) {
        const move_mode_id &curr = modes[i];
        std::string label = curr->name();
        const float required_moves = player_character.move_mode_switch_cost(
                                         player_character.current_movement_mode(), curr );
        const float required_seconds = required_moves / player_character.get_speed();

        if( required_seconds > 0 ) {
            label += string_format( _( " (%.2f s)" ), required_seconds );
        }

        as_m.entries.emplace_back( static_cast<int>( i ), player_character.can_switch_to( curr ),
                                   curr->letter(),
                                   label );
    }
    as_m.entries.emplace_back( cycle,
                               true, // cycling movement is controlled in relevant functions, always allow
                               hotkey_for_action( ACTION_OPEN_MOVEMENT, /*maximum_modifier_count=*/1 ),
                               _( "Cycle move mode" ) );
    // This should select the middle move mode
    as_m.selected = std::floor( modes.size() / 2 );
    as_m.query();

    if( as_m.ret != UILIST_CANCEL ) {
        if( as_m.ret == cycle ) {
            player_character.cycle_desired_move_mode();
        } else {
            player_character.set_desired_movement_mode( modes[as_m.ret] );
        }
    }
}

static void cast_spell( bool recast_spell = false )
{
    Character &player_character = get_player_character();
    player_character.magic->clear_opens_spellbook_data();
    get_event_bus().send<event_type::opens_spellbook>( player_character.getID() ); // trigger EoC
    player_character.magic->evaluate_opens_spellbook_data();
    std::vector<spell_id> spells = player_character.magic->spells();

    if( player_character.has_flag( json_flag_CANNOT_ATTACK ) ) {
        add_msg( game_message_params{ m_bad, gmf_bypass_cooldown },
                 _( "You cannot cast spells at this moment." ) );
        return;
    }
    if( spells.empty() ) {
        add_msg( game_message_params{ m_bad, gmf_bypass_cooldown },
                 _( "You don't know any spells to cast." ) );
        return;
    }

    std::map<magic_type_id, bool> success_tracker = {};
    for( const spell_id &sp : spells ) {
        spell &temp_spell = player_character.magic->get_spell( sp );
        temp_spell.can_cast( player_character, success_tracker );
    }

    for( auto const& [m_type, any_success] : success_tracker ) {
        if( !any_success && m_type->cannot_cast_message.has_value() ) {
            add_msg( game_message_params{ m_bad, gmf_bypass_cooldown },
                     m_type->cannot_cast_message.value() );
        }
    }

    if( recast_spell && player_character.magic->last_spell.is_null() ) {
        popup( _( "Cast a spell first" ) );
        return;
    }

    spell &sp = recast_spell
                ? player_character.magic->get_spell( player_character.magic->last_spell )
                : player_character.magic->select_spell( player_character );
    // if no spell was selected
    if( sp.id().is_null() ) {
        return;
    }
    player_character.magic->last_spell = sp.id();
    player_character.cast_spell( sp, false, std::nullopt );
}

// returns true if the spell was assigned
bool Character::cast_spell( spell &sp, bool fake_spell,
                            const std::optional<tripoint_bub_ms> &target = std::nullopt )
{
    if( is_armed() && !sp.no_hands() && !has_flag( json_flag_SUBTLE_SPELL ) &&
        !get_wielded_item()->has_flag( flag_MAGIC_FOCUS ) && !sp.check_if_component_in_hand( *this ) ) {
        add_msg( game_message_params{ m_bad, gmf_bypass_cooldown },
                 _( "You need your hands free to cast this spell!" ) );
        return false;
    }

    if( !sp.valid_caster_condition( *this ) ) {
        add_msg( game_message_params{ m_bad, gmf_bypass_cooldown }, sp.failed_caster_condition_message() );
        return false;
    }

    if( !magic->has_enough_energy( *this, sp ) ) {
        add_msg( game_message_params{ m_bad, gmf_bypass_cooldown },
                 _( "You don't have enough %s to cast the spell." ),
                 sp.energy_string() );
        return false;
    }

    if( !sp.no_hands() && !has_flag( json_flag_SUBTLE_SPELL ) &&
        has_effect( effect_stunned ) ) {
        add_msg( game_message_params{ m_bad, gmf_bypass_cooldown },
                 _( "You can't focus enough to cast spell." ) );
        return false;
    }

    if( sp.energy_source() == magic_energy_type::hp && !has_quality( qual_CUT ) ) {
        add_msg( game_message_params{ m_bad, gmf_bypass_cooldown },
                 _( "You cannot cast Blood Magic without a cutting implement." ) );
        return false;
    }

    std::optional<int> fake_spell_level;
    if( fake_spell ) {
        fake_spell_level = sp.get_level();
    }
    std::optional<tripoint_abs_ms> abs_target;
    if( target ) {
        abs_target = get_map().get_abs( *target );
    }

    assign_activity( spellcasting_activity_actor(
                         time_duration::from_moves<int>( sp.casting_time( *this ) ),
                         sp.id(), abs_target, fake_spell_level ) );
    return true;
}

// this is here because it shares some things in common with cast_spell
bool bionic::activate_spell( Character &caster ) const
{
    if( !caster.is_avatar() || !id->spell_on_activate ) {
        // the return value tells us if the spell fails. if it has no spell it can't fail
        return true;
    }
    spell sp = id->spell_on_activate->get_spell( caster );
    return caster.cast_spell( sp, true );
}

void game::open_consume_item_menu()
{
    uilist as_m;

    as_m.text = _( "What do you want to consume?" );

    as_m.entries.emplace_back( 0, true, 'f', _( "Food" ) );
    as_m.entries.emplace_back( 1, true, 'd', _( "Drink" ) );
    as_m.entries.emplace_back( 2, true, 'm', _( "Medication" ) );
    as_m.query();

    const int query_returned_index = as_m.ret;
    if( query_returned_index >= 0 && query_returned_index < 3 ) {
        const std::array<std::string, 3> comestype_strings = { "FOOD", "DRINK", "MED" };
        const std::string &selected_comestype = comestype_strings[query_returned_index];
        uistate.open_menu = [selected_comestype = selected_comestype]() {
            avatar_action::eat_or_use( get_avatar(), game_menus::inv::consume( selected_comestype ) );
        };
    }
}

static void handle_debug_mode()
{
    auto debug_mode_setup = []( uilist_entry & entry ) -> void {
        entry.txt = string_format( _( "Debug Mode (%1$s)" ), debug_mode ? _( "ON" ) : _( "OFF" ) );
        entry.text_color = debug_mode ? c_green : c_light_gray;
    };

    // returns if entry became active
    auto debugmode_entry_setup = []( uilist_entry & entry, bool active ) -> void {
        if( active )
        {
            entry.extratxt.txt = entry.hotkey->short_description() + " " + _( "A" );
            entry.extratxt.color = c_white_green;
            entry.text_color = c_green;
        } else
        {
            entry.extratxt.txt = entry.hotkey->short_description() + " ";
            entry.extratxt.color = c_unset;
            entry.text_color = c_light_gray;
        }
    };

    input_context ctxt( "DEFAULTMODE" );
    ctxt.register_action( "debug_mode" );

    uilist dbmenu;
    dbmenu.allow_anykey = true;
    dbmenu.title = _( "Debug Mode Filters" );
    dbmenu.text = string_format( _( "Press [%1$s] to quickly toggle debug mode." ),
                                 ctxt.get_desc( "debug_mode" ) );

    dbmenu.entries.reserve( 1 + debugmode::DF_LAST );

    dbmenu.addentry( 0, true, 'd', " " );
    debug_mode_setup( dbmenu.entries[0] );

    dbmenu.addentry( 1, true, 't', _( "Toggle all filters" ) );
    bool toggle_value = true;
    const hotkey_queue &hotkeys = hotkey_queue::alpha_digits();
    input_event assigned_hotkey = ctxt.first_unassigned_hotkey( hotkeys );
    int reserved_character_d = 'd';
    int reserved_character_t = 't';

    for( int i = 0; i < debugmode::DF_LAST; ++i ) {
        uilist_entry entry( i + 2, true, assigned_hotkey,
                            "  " + debugmode::filter_name( static_cast<debugmode::debug_filter>( i ) ) );
        assigned_hotkey = ctxt.next_unassigned_hotkey( hotkeys, assigned_hotkey );
        // Skip d and t, special cased
        if( assigned_hotkey.sequence.front() == reserved_character_d ||
            assigned_hotkey.sequence.front() == reserved_character_t ) {
            assigned_hotkey = hotkeys.next( assigned_hotkey );
        }

        entry.extratxt.left = 1;

        const bool active = debugmode::enabled_filters.count( static_cast<debugmode::debug_filter>
                            ( i ) ) == 1;

        if( toggle_value && active ) {
            toggle_value = false;
        }

        debugmode_entry_setup( entry, active );
        dbmenu.entries.push_back( entry );
    }

    do {
        dbmenu.query();
        if( ctxt.input_to_action( dbmenu.ret_evt ) == "debug_mode" ) {
            debug_mode = !debug_mode;
            if( debug_mode ) {
                add_msg( m_info, _( "Debug mode ON!" ) );
            } else {
                add_msg( m_info, _( "Debug mode OFF!" ) );
            }
            break;
        }

        if( dbmenu.ret == 0 ) {
            debug_mode = !debug_mode;
            debug_mode_setup( dbmenu.entries[0] );

        } else if( dbmenu.ret == 1 ) {
            debugmode::enabled_filters.clear();

            for( int i = 0; i < debugmode::DF_LAST; ++i ) {
                debugmode_entry_setup( dbmenu.entries[i + 2], toggle_value );

                if( toggle_value ) {
                    debugmode::enabled_filters.emplace( static_cast<debugmode::debug_filter>( i ) );
                }
            }

            toggle_value = !toggle_value;

        } else if( dbmenu.ret > 1 ) {
            uilist_entry &entry = dbmenu.entries[dbmenu.ret];

            const auto filter_iter = debugmode::enabled_filters.find( static_cast<debugmode::debug_filter>
                                     ( dbmenu.ret - 2 ) );

            const bool active = filter_iter != debugmode::enabled_filters.end();

            debugmode_entry_setup( entry, !active );

            if( active ) {
                debugmode::enabled_filters.erase( filter_iter );
            } else {
                debugmode::enabled_filters.emplace(
                    static_cast<debugmode::debug_filter>( dbmenu.ret - 2 ) );
            }
        }
    } while( dbmenu.ret != UILIST_CANCEL );
}

static bool has_vehicle_control( avatar &player_character )
{
    map &here = get_map();

    if( player_character.is_dead_state() ) {
        return false;
    }
    const optional_vpart_position vp = here.veh_at( player_character.pos_bub() );
    if( vp && vp->vehicle().player_in_control( here, player_character ) ) {
        return true;
    }
    return g->remoteveh() != nullptr;
}

static void do_deathcam_action( const action_id &act, avatar &player_character )
{
    switch( act ) {
        case ACTION_CENTER:
            player_character.view_offset.x() = g->driving_view_offset.x();
            player_character.view_offset.y() = g->driving_view_offset.y();
            player_character.view_offset.z() = 0;
            break;

        case ACTION_SHIFT_N:
        case ACTION_SHIFT_NE:
        case ACTION_SHIFT_E:
        case ACTION_SHIFT_SE:
        case ACTION_SHIFT_S:
        case ACTION_SHIFT_SW:
        case ACTION_SHIFT_W:
        case ACTION_SHIFT_NW: {
            static const std::map<action_id, std::pair<point, point>> shift_delta = {
                { ACTION_SHIFT_N, { point::north, point::north_east } },
                { ACTION_SHIFT_NE, { point::north_east, point::east } },
                { ACTION_SHIFT_E, { point::east, point::south_east } },
                { ACTION_SHIFT_SE, { point::south_east, point::south } },
                { ACTION_SHIFT_S, { point::south, point::south_west } },
                { ACTION_SHIFT_SW, { point::south_west, point::west } },
                { ACTION_SHIFT_W, { point::west, point::north_west } },
                { ACTION_SHIFT_NW, { point::north_west, point::north } },
            };
            int soffset = get_option<int>( "MOVE_VIEW_OFFSET" );
            player_character.view_offset += g->is_tileset_isometric()
                                            ? shift_delta.at( act ).second * soffset
                                            : shift_delta.at( act ).first * soffset;
        }
        break;

        case ACTION_LOOK:
            g->look_around();
            break;

        case ACTION_KEYBINDINGS: // already handled by input context
        default:
            break;
    }
}

static std::map<action_id, std::string> get_actions_disabled_in_shell()
{
    return std::map<action_id, std::string> {
        { ACTION_OPEN,               _( "You can't open things while you're in your shell." ) },
        { ACTION_CLOSE,              _( "You can't close things while you're in your shell." ) },
        { ACTION_SMASH,              _( "You can't smash things while you're in your shell." ) },
        { ACTION_EXAMINE,            _( "You can't examine your surroundings while you're in your shell." ) },
        { ACTION_EXAMINE_AND_PICKUP, _( "You can't examine your surroundings while you're in your shell." ) },
        { ACTION_ADVANCEDINV,        _( "You can't move mass quantities while you're in your shell." ) },
        { ACTION_PICKUP,             _( "You can't pick anything up while you're in your shell." ) },
        { ACTION_PICKUP_ALL,         _( "You can't pick anything up while you're in your shell." ) },
        { ACTION_GRAB,               _( "You can't grab things while you're in your shell." ) },
        { ACTION_HAUL,               _( "You can't haul things while you're in your shell." ) },
        { ACTION_HAUL_TOGGLE,        _( "You can't haul things while you're in your shell." ) },
        { ACTION_BUTCHER,            _( "You can't butcher while you're in your shell." ) },
        { ACTION_PEEK,               _( "You can't peek around corners while you're in your shell." ) },
        { ACTION_DROP,               _( "You can't drop things while you're in your shell." ) },
        { ACTION_CRAFT,              _( "You can't craft while you're in your shell." ) },
        { ACTION_RECRAFT,            _( "You can't craft while you're in your shell." ) },
        { ACTION_LONGCRAFT,          _( "You can't craft while you're in your shell." ) },
        { ACTION_DISASSEMBLE,        _( "You can't disassemble while you're in your shell." ) },
        { ACTION_CONSTRUCT,          _( "You can't construct while you're in your shell." ) },
        { ACTION_CONTROL_VEHICLE,    _( "You can't operate a vehicle while you're in your shell." ) },
    };
}

static const std::set<action_id> actions_disabled_in_incorporeal {
    ACTION_OPEN,
    ACTION_CLOSE,
    ACTION_SMASH,
    ACTION_ADVANCEDINV,
    ACTION_PICKUP,
    ACTION_PICKUP_ALL,
    ACTION_DROP,
    ACTION_DIR_DROP,
    ACTION_GRAB,
    ACTION_HAUL,
    ACTION_HAUL_TOGGLE,
    ACTION_BUTCHER,
    ACTION_CRAFT,
    ACTION_RECRAFT,
    ACTION_LONGCRAFT,
    ACTION_DISASSEMBLE,
    ACTION_CONSTRUCT,
    ACTION_CONTROL_VEHICLE,
};

static std::map<action_id, std::string> get_actions_disabled_in_handless_temporary_shapeshift()
{
    return std::map<action_id, std::string> {
        { ACTION_OPEN,               _( "You can't open things while shapeshifted." ) },
        { ACTION_CLOSE,              _( "You can't close things while shapeshifted." ) },
        { ACTION_ADVANCEDINV,        _( "You can't move mass quantities while shapeshifted." ) },
        { ACTION_PICKUP,             _( "You can't pick anything up while shapeshifted." ) },
        { ACTION_PICKUP_ALL,         _( "You can't pick anything up while shapeshifted." ) },
        { ACTION_GRAB,               _( "You can't grab things while shapeshifted." ) },
        { ACTION_HAUL,               _( "You can't haul things while shapeshifted." ) },
        { ACTION_HAUL_TOGGLE,        _( "You can't haul things while shapeshifted." ) },
        { ACTION_BUTCHER,            _( "You can't butcher while shapeshifted." ) },
        { ACTION_DROP,               _( "You can't drop things while shapeshifted." ) },
        { ACTION_DIR_DROP,           _( "You can't drop things while shapeshifted." ) },
        { ACTION_CRAFT,              _( "You can't craft while shapeshifted." ) },
        { ACTION_RECRAFT,            _( "You can't craft while shapeshifted." ) },
        { ACTION_LONGCRAFT,          _( "You can't craft while shapeshifted." ) },
        { ACTION_DISASSEMBLE,        _( "You can't disassemble while shapeshifted." ) },
        { ACTION_CONSTRUCT,          _( "You can't construct while shapeshifted." ) },
        { ACTION_CONTROL_VEHICLE,    _( "You can't operate a vehicle while shapeshifted." ) },
    };
}

static std::map<action_id, std::string> get_actions_disabled_mounted()
{
    return std::map<action_id, std::string> {
        { ACTION_DISASSEMBLE,        _( "You can't disassemble items while you're riding." ) },
        { ACTION_CONSTRUCT,          _( "You can't construct while you're riding." ) },
        { ACTION_OPEN,               _( "You can't open things while you're riding." ) },
        { ACTION_ADVANCEDINV,        _( "You can't move mass quantities while you're riding." ) },
        { ACTION_PICKUP,             _( "You can't pick anything up while you're riding." ) },
        { ACTION_PICKUP_ALL,         _( "You can't pick anything up while you're riding." ) },
        { ACTION_GRAB,               _( "You can't grab things while you're riding." ) },
        { ACTION_HAUL,               _( "You can't haul things while you're riding." ) },
        { ACTION_HAUL_TOGGLE,        _( "You can't haul things while you're riding." ) },
        { ACTION_BUTCHER,            _( "You can't butcher while you're riding." ) },
        { ACTION_PEEK,               _( "You can't peek around corners while you're riding." ) },
        { ACTION_CRAFT,              _( "You can't craft while you're riding." ) },
        { ACTION_RECRAFT,            _( "You can't craft while you're riding." ) },
        { ACTION_LONGCRAFT,          _( "You can't craft while you're riding." ) },
    };
}

static std::vector<action_id> get_actions_move_mode()
{
    return std::vector<action_id> {
        ACTION_CYCLE_MOVE,
        ACTION_CYCLE_MOVE_REVERSE,
        ACTION_RESET_MOVE,
        ACTION_TOGGLE_RUN,
        ACTION_TOGGLE_CROUCH,
        ACTION_TOGGLE_PRONE,
        ACTION_OPEN_MOVEMENT,
    };
}

bool game::do_regular_action( action_id &act, avatar &player_character,
                              const std::optional<tripoint_bub_ms> &mouse_target )
{
    map &here = get_map();

    item_location weapon = player_character.get_wielded_item();
    const bool in_shell = player_character.has_active_mutation( trait_SHELL2 )
                          || player_character.has_active_mutation( trait_SHELL3 );

    const std::map<action_id, std::string> actions_disabled_mounted = get_actions_disabled_mounted();
    const std::map<action_id, std::string> actions_disabled_in_shell = get_actions_disabled_in_shell();
    const std::map<action_id, std::string> actions_disabled_while_handless_shapeshifted =
        get_actions_disabled_in_handless_temporary_shapeshift();

    if( in_shell && actions_disabled_in_shell.count( act ) > 0 ) {
        add_msg( m_info, actions_disabled_in_shell.at( act ) );
        return true;
    }

    if( u.has_effect( effect_incorporeal ) && actions_disabled_in_incorporeal.count( act ) > 0 ) {
        add_msg( m_info, _( "You lack the substance to affect anything." ) );
        return true;
    }

    if( u.has_flag( json_flag_TEMPORARY_SHAPESHIFT_NO_HANDS ) &&
        actions_disabled_while_handless_shapeshifted.count( act ) > 0 ) {
        add_msg( m_info, _( "You can't do that while shapeshifted." ) );
        return true;
    }

    if( player_character.is_mounted() && actions_disabled_mounted.count( act ) > 0 ) {
        add_msg( m_info, actions_disabled_mounted.at( act ) );
        return true;
    }

    const std::vector<action_id> actions_move_mode = get_actions_move_mode();
    const bool is_actions_move_mode = std::find( actions_move_mode.begin(),
                                      actions_move_mode.end(), act ) != actions_move_mode.end();
    // Are we performing an action that is not a move mode action?
    int desired_move_mode_cost = 0;
    if( player_character.is_waiting_to_change_mode_mode() && !is_actions_move_mode ) {
        move_mode_id desired_move = player_character.get_desired_move_mode();
        if( player_character.can_switch_to( desired_move ) ) {
            desired_move_mode_cost = player_character.move_mode_switch_cost( player_character.move_mode,
                                     desired_move );
            player_character.set_movement_mode( desired_move );
            if( player_character.move_mode == desired_move ) {
                player_character.mod_moves( -desired_move_mode_cost );
            } else {
                debugmsg( "Player unable to change from move_mode(%s) to desired_move_mode(%s)",
                          player_character.move_mode.c_str(), desired_move.c_str() );
            }
        }
    }

    switch( act ) {
        case ACTION_NULL: // dummy entry
        case NUM_ACTIONS: // dummy entry
        case ACTION_ACTIONMENU: // handled above
        case ACTION_MAIN_MENU:
        case ACTION_KEYBINDINGS:
            break;

        case ACTION_TIMEOUT:
            if( check_safe_mode_allowed( false ) ) {
                player_character.pause();
            }
            break;

        case ACTION_PAUSE:
            if( check_safe_mode_allowed() ) {
                player_character.pause();
                openclaw_harness_trace_default_action_dispatch( "invoke_pause", action_ident( act ), act );
            }
            break;

        case ACTION_CYCLE_MOVE:
            player_character.cycle_desired_move_mode();
            break;

        case ACTION_CYCLE_MOVE_REVERSE:
            player_character.cycle_desired_move_mode_reverse();
            break;

        case ACTION_RESET_MOVE:
            player_character.set_walk_mode_desired();
            break;

        case ACTION_TOGGLE_RUN:
            player_character.toggle_run_mode_desired();
            break;

        case ACTION_TOGGLE_CROUCH:
            player_character.toggle_crouch_mode_desired();
            break;

        case ACTION_TOGGLE_PRONE:
            player_character.toggle_prone_mode_desired();
            break;

        case ACTION_OPEN_MOVEMENT:
            open_movement_mode_menu();
            break;

        case ACTION_MOVE_FORTH:
        case ACTION_MOVE_FORTH_RIGHT:
        case ACTION_MOVE_RIGHT:
        case ACTION_MOVE_BACK_RIGHT:
        case ACTION_MOVE_BACK:
        case ACTION_MOVE_BACK_LEFT:
        case ACTION_MOVE_LEFT:
        case ACTION_MOVE_FORTH_LEFT:
        {
            const std::string semantic_action = openclaw_harness_semantic_movement_action_id( act );
            if( player_character.maybe_get_value( "remote_controlling" ) &&
                ( player_character.has_active_item( itype_radiocontrol ) ||
                  player_character.has_active_bionic( bio_remote ) ) ) {
                rcdrive( get_delta_from_movement_action( act, iso_rotate::yes ) );
            } else if( has_vehicle_control( player_character ) ) {
                // vehicle control uses x for steering and y for ac/deceleration,
                // so no rotation needed
                pldrive( get_delta_from_movement_action( act, iso_rotate::no ) );
            } else {
                const int pre_walk_moves = player_character.get_moves();
                point_rel_ms dest_delta = get_delta_from_movement_action( act, iso_rotate::yes );
                if( auto_travel_mode && !player_character.is_auto_moving() ) {
                    const bool use_grab_routing =
                        has_grabbed_single_tile_vehicle( player_character, here );
                    for( int i = 0; i < SEEX; i++ ) {
                        tripoint_bub_ms auto_travel_destination =
                            player_character.pos_bub() + dest_delta * ( SEEX - i );
                        if( use_grab_routing ) {
                            destination_preview = route_with_grab( here, player_character,
                                                                   pathfinding_target::point( auto_travel_destination ),
                                                                   player_character.get_path_avoid() );
                        } else {
                            destination_preview =
                                here.route( player_character, pathfinding_target::point( auto_travel_destination ) );
                        }
                        if( !destination_preview.empty() ) {
                            destination_preview.erase(
                                destination_preview.begin() + 1, destination_preview.end() );
                            player_character.set_destination( destination_preview );
                            break;
                        }
                    }
                    act = player_character.get_next_auto_move_direction();
                    const point_rel_ms dest_next = get_delta_from_movement_action( act, iso_rotate::yes );
                    if( dest_next == point_rel_ms::zero ) {
                        add_msg_debug( debugmode::DF_ACTIVITY,
                                       "auto_move: get_next returned zero, aborting.  pos=(%d,%d)",
                                       player_character.pos_bub().x(), player_character.pos_bub().y() );
                        player_character.abort_automove();
                    }
                    dest_delta = dest_next;
                }
                const tripoint_bub_ms pos_before = player_character.pos_bub();
                const tripoint_abs_ms pos_before_abs = player_character.pos_abs();
                const tripoint_bub_ms dest_tile = pos_before +
                                                  tripoint_rel_ms( dest_delta, 0 );
                const tripoint_abs_ms expected_abs = pos_before_abs +
                                                      tripoint_rel_ms( dest_delta, 0 );
                const ter_id ter_before = here.ter( dest_tile );
                const furn_id furn_before = here.furn( dest_tile );
                int veh_door_part_before = -1;
                if( const optional_vpart_position ovp = here.veh_at( dest_tile ) ) {
                    veh_door_part_before = ovp->vehicle().next_part_to_open(
                                               ovp->part_index(), true );
                }
                const bool move_handled = avatar_action::move(
                                              player_character, here, tripoint_rel_ms( dest_delta, 0 ) );
                const tripoint_bub_ms pos_after = player_character.pos_bub();
                const tripoint_abs_ms pos_after_abs = player_character.pos_abs();
                std::string movement_semantic_action = semantic_action;
                // View-relative movement may have been normalized to a
                // diagonal action enum before this branch even though the
                // native absolute step is cardinal.  The semantic owner has
                // already attested the request; derive that cardinal receipt
                // from the actual absolute delta instead of losing it solely
                // to the intermediate action spelling.
                if( movement_semantic_action.empty() && openclaw_harness_semantic_step_trace_enabled() ) {
                    const tripoint_rel_ms absolute_delta = pos_after_abs - pos_before_abs;
                    if( absolute_delta == tripoint_rel_ms( 0, -1, 0 ) ) {
                        movement_semantic_action = "world.move.north";
                    } else if( absolute_delta == tripoint_rel_ms( 0, 1, 0 ) ) {
                        movement_semantic_action = "world.move.south";
                    } else if( absolute_delta == tripoint_rel_ms( -1, 0, 0 ) ) {
                        movement_semantic_action = "world.move.west";
                    } else if( absolute_delta == tripoint_rel_ms( 1, 0, 0 ) ) {
                        movement_semantic_action = "world.move.east";
                    }
                }
                if( !movement_semantic_action.empty() ) {
                    // The semantic World scope retires its published frame
                    // after writing its owner receipt.  Retain a native frame
                    // for the coordinate completion even when that retirement
                    // has already cleared the pending global.
                    const std::string movement_frame = openclaw_harness_pending_world_frame.empty() ?
                                                       openclaw_harness_semantic_step_frame(
                                                           "world", {}, "movement_receipt" ) :
                                                       openclaw_harness_pending_world_frame;
                    openclaw_harness_semantic_movement_receipt(
                        movement_frame, movement_semantic_action, pos_before_abs, expected_abs,
                        pos_after_abs, here, pos_after, move_handled );
                    openclaw_harness_pending_world_frame = openclaw_harness_semantic_step_frame(
                                "world", {}, "post_step" );
                }
                if( !move_handled ) {
                    // auto-move should be canceled due to a failed move or obstacle
                    add_msg_debug( debugmode::DF_ACTIVITY,
                                   "auto_move: move(%d,%d) FAILED at pos=(%d,%d), aborting",
                                   dest_delta.x(), dest_delta.y(),
                                   player_character.pos_bub().x(), player_character.pos_bub().y() );
                    player_character.abort_automove();
                } else if( player_character.pos_bub() == pos_before &&
                           dest_delta != point_rel_ms::zero ) {
                    // General auto-move safety: catches cases where move() returns true
                    // ("handled") but the player didn't actually move. Covers grabbed
                    // vehicle collisions, NPC interactions, and any future similar cases.
                    // Check if terrain, furniture, or a vehicle part changed (door/trunk
                    // opened) - if so, next step will walk through, so don't abort.
                    const ter_id ter_after = here.ter( dest_tile );
                    const furn_id furn_after = here.furn( dest_tile );
                    bool veh_part_opened = false;
                    if( const optional_vpart_position ovp = here.veh_at( dest_tile ) ) {
                        // If there was an openable part before but not now, a vehicle
                        // door/trunk/hatch was opened by avatar_action::move.
                        const int openable_now = ovp->vehicle().next_part_to_open(
                                                     ovp->part_index(), true );
                        veh_part_opened = ( veh_door_part_before >= 0 && openable_now < 0 ) ||
                                          ( veh_door_part_before >= 0 && openable_now != veh_door_part_before );
                    }
                    if( ter_before != ter_after || furn_before != furn_after || veh_part_opened ) {
                        add_msg_debug( debugmode::DF_ACTIVITY,
                                       "auto_move: pos unchanged but ter/furn/veh changed at (%d,%d), continuing",
                                       dest_tile.x(), dest_tile.y() );
                    } else {
                        add_msg_debug( debugmode::DF_ACTIVITY,
                                       "auto_move: move(%d,%d) returned OK but pos unchanged at (%d,%d), aborting",
                                       dest_delta.x(), dest_delta.y(),
                                       pos_before.x(), pos_before.y() );
                        player_character.abort_automove();
                    }
                }

                // if we changed move modes this action, refund half the cost of changing move mode
                // if their move action was an easy movement to represent combining the two actions
                if( desired_move_mode_cost > 0 ) {
                    const int moves_delta = pre_walk_moves - player_character.get_moves();
                    // add 10% to the easy movement threshold to allow for some minor encumbrance
                    // add 20% if going prone because it has an extra 20% crawling mod
                    const int easy_moves = 110 /
                                           player_character.get_modifier( character_modifier_move_mode_move_cost_mod ) *
                                           ( player_character.is_prone() ? 1.2 : 1 );

                    if( moves_delta > 0 && moves_delta <= easy_moves ) {
                        player_character.mod_moves( desired_move_mode_cost / 2 );
                    }
                }

                if( get_option<bool>( "AUTO_FEATURES" ) && get_option<bool>( "AUTO_MOPPING" ) &&
                    weapon && weapon->has_flag( json_flag_MOP ) ) {
                    const bool is_blind = player_character.is_blind();
                    for( const tripoint_bub_ms &point : here.points_in_radius( player_character.pos_bub(), 1 ) ) {
                        bool did_mop = false;
                        if( is_blind ) {
                            // blind character have a 1/3 chance of actually mopping
                            if( one_in( 3 ) ) {
                                did_mop = here.mop_spills( point );
                            } else {
                                did_mop = here.terrain_moppable( point );
                            }
                        } else {
                            did_mop = here.mop_spills( point );
                        }
                        // iuse::mop costs 15 moves per use
                        if( did_mop ) {
                            player_character.mod_moves( -15 );
                        }
                    }
                }
            }
            break;
        }
        case ACTION_MOVE_DOWN: {
            if( player_character.is_mounted() ) {
                auto *mon = player_character.mounted_creature.get();
                if( !mon->has_flag( mon_flag_RIDEABLE_MECH ) ) {
                    add_msg( m_info, _( "You can't go down stairs while you're riding." ) );
                    break;
                }
            }

            if( has_vehicle_control( player_character ) ) {
                const optional_vpart_position vp = here.veh_at( player_character.pos_bub() );
                if( vp->vehicle().is_rotorcraft( here ) ) {
                    pldrive( tripoint_rel_ms::below );
                    break;
                }
            }

            if( !player_character.in_vehicle ) {
                // We're NOT standing on tiles with stairs, ropes, ladders etc
                if( !here.has_flag( ter_furn_flag::TFLAG_GOES_DOWN, player_character.pos_bub() ) &&
                    !u.has_flag( json_flag_PHASE_MOVEMENT ) ) {
                    std::vector<tripoint_bub_ms> pts;

                    // If levitating, just move straight down if possible.
                    if( player_character.has_flag( json_flag_LEVITATION ) &&
                        here.has_flag( ter_furn_flag::TFLAG_NO_FLOOR, player_character.pos_bub() ) ) {
                        pts.push_back( player_character.pos_bub() );
                    }

                    if( pts.empty() ) {
                        // Check tiles around player character for open air
                        for( const tripoint_bub_ms &p : here.points_in_radius( player_character.pos_bub(), 1 ) ) {
                            if( here.has_flag( ter_furn_flag::TFLAG_NO_FLOOR, p ) ) {
                                pts.push_back( p );
                            }
                        }
                    }

                    // If we found tiles with open air, prompt player with query on direction they want to climb
                    if( !pts.empty() ) {
                        const std::optional<tripoint_bub_ms> pnt = point_selection_menu( pts, false );
                        if( !pnt ) {
                            break;
                        }

                        // If player selected direction, climb down there, and exit from the whole ACTION_MOVE_DOWN case
                        climb_down( *pnt );
                        break;
                    }
                }

                // If we're here, we might or might not be standing on tiles with stairs, ropes, ladders etc
                // In any case, attempt a descend
                vertical_move( -1, u.has_flag( json_flag_PHASE_MOVEMENT ) );
            }
            break;
        }

        case ACTION_MOVE_UP:
            if( player_character.is_mounted() ) {
                auto *mon = player_character.mounted_creature.get();
                if( !mon->has_flag( mon_flag_RIDEABLE_MECH ) ) {
                    add_msg( m_info, _( "You can't go up stairs while you're riding." ) );
                    break;
                }
            }
            if( !player_character.in_vehicle ) {
                vertical_move( 1, u.has_flag( json_flag_PHASE_MOVEMENT ) );
            } else if( has_vehicle_control( player_character ) ) {
                const optional_vpart_position vp = here.veh_at( player_character.pos_bub() );
                if( vp->vehicle().is_rotorcraft( here ) ) {
                    pldrive( tripoint_rel_ms::above );
                }
            }
            break;

        case ACTION_OPEN:
            open( mouse_target );
            break;

        case ACTION_CLOSE:
            if( player_character.is_mounted() ) {
                auto *mon = player_character.mounted_creature.get();
                if( !mon->has_flag( mon_flag_RIDEABLE_MECH ) ) {
                    add_msg( m_info, _( "You can't close things while you're riding." ) );
                }
            } else {
                close( mouse_target );
            }
            break;

        case ACTION_SMASH:
            if( has_vehicle_control( player_character ) ) {
                handbrake( here );
            } else {
                smash( mouse_target );
            }
            break;

        case ACTION_EXAMINE:
        case ACTION_EXAMINE_AND_PICKUP:
            if( mouse_target ) {
                // Examine including item pickup if ACTION_EXAMINE_AND_PICKUP is used
                examine( *mouse_target, act == ACTION_EXAMINE_AND_PICKUP );
            } else {
                examine( act == ACTION_EXAMINE_AND_PICKUP );
            }
            break;

        case ACTION_ADVANCEDINV:
            create_advanced_inv();
            break;

        case ACTION_PICKUP:
        case ACTION_PICKUP_ALL:
            if( mouse_target ) {
                pickup( *mouse_target );
            } else {
                if( act == ACTION_PICKUP_ALL ) {
                    pickup_all();
                } else {
                    pickup();
                }
            }
            break;

        case ACTION_GRAB:
            grab( mouse_target );
            break;

        case ACTION_HAUL:
            haul();
            break;

        case ACTION_HAUL_TOGGLE:
            haul_toggle();
            break;

        case ACTION_BUTCHER:
            butcher( mouse_target );
            break;

        case ACTION_CHAT:
            chat( mouse_target );
            break;

        case ACTION_PEEK:
            if( mouse_target ) {
                peek( *mouse_target );
            } else {
                peek();
            }
            break;

        case ACTION_LIST_ITEMS:
            list_surroundings();
            break;

        case ACTION_ZONES:
            openclaw_harness_trace_default_action_dispatch( "invoke_zone_manager", action_ident( act ), act );
            zone_manager_ui::display_zone_manager();
            break;

        case ACTION_LOOT:
            loot();
            break;

        case ACTION_INVENTORY:
            game_menus::inv::common();
            break;

        case ACTION_COMPARE:
            game_menus::inv::compare( std::nullopt );
            break;

        case ACTION_ORGANIZE:
            game_menus::inv::swap_letters();
            break;

        case ACTION_USE:
            // Shell-users are presumed to be able to mess with their inventories, etc
            // while in the shell.  Eating, gear-changing, and item use are OK.
            avatar_action::use_item( player_character );
            break;

        case ACTION_USE_WIELDED:
            player_character.use_wielded();
            break;

        case ACTION_WEAR:
            wear();
            break;

        case ACTION_TAKE_OFF:
            takeoff();
            break;

        case ACTION_EAT:
            if( !avatar_action::eat_here( player_character ) ) {
                avatar_action::eat_or_use( player_character, game_menus::inv::consume() );
            }
            break;

        case ACTION_OPEN_CONSUME:
            if( !avatar_action::eat_here( player_character ) ) {
                open_consume_item_menu();
            }
            break;

        case ACTION_READ:
            // Shell-users are presumed to have the book just at an opening and read it that way
            read();
            break;

        case ACTION_WIELD: {
            item_location loc = game_menus::inv::wield();
            if( loc ) {
                player_character.wield( loc );
            }
            break;
        }

        case ACTION_PICK_STYLE:
            player_character.martial_arts_data->pick_style( player_character );
            break;

        case ACTION_RELOAD_ITEM:
            reload_item();
            break;

        case ACTION_RELOAD_WEAPON:
            reload_weapon();
            break;

        case ACTION_RELOAD_WIELDED:
            reload_wielded();
            break;

        case ACTION_UNLOAD:
            avatar_action::unload( player_character );
            break;

        case ACTION_MEND:
            avatar_action::mend( player_character, item_location() );
            break;

        case ACTION_THROW: {
            item_location loc;
            avatar_action::plthrow( player_character, loc );
            break;
        }

        case ACTION_THROW_WIELDED: {
            avatar_action::plthrow_wielded( player_character );
            break;
        }

        case ACTION_FIRE:
            fire();
            break;

        case ACTION_CAST_SPELL:
            cast_spell();
            break;

        case ACTION_RECAST_SPELL:
            cast_spell( true );
            break;

        case ACTION_FIRE_BURST: {
            if( weapon ) {
                if( weapon->gun_set_mode( gun_mode_BURST ) || weapon->gun_set_mode( gun_mode_AUTO ) ) {
                    avatar_action::fire_wielded_weapon( player_character );
                }
            }
            break;
        }

        case ACTION_SELECT_FIRE_MODE:
            if( weapon && weapon->is_gun() && !weapon->is_gunmod() ) {
                if( weapon->gun_all_modes().size() > 1 ) {
                    weapon->gun_cycle_mode();
                } else {
                    add_msg( m_info, _( "Your %s has only one firing mode." ), weapon->tname() );
                }
            }
            break;

        case ACTION_SELECT_DEFAULT_AMMO:
            if( weapon && weapon->is_gun() && !weapon->is_gunmod() ) {
                if( weapon->has_flag( flag_RELOAD_ONE ) ||
                    weapon->has_flag( flag_RELOAD_AND_SHOOT ) ) {
                    item::reload_option opt = player_character.select_ammo( weapon, false );
                    if( !opt ) {
                        break;
                    } else if( player_character.ammo_location && opt.ammo == player_character.ammo_location ) {
                        player_character.add_msg_if_player( _( "Cleared ammo preferences for %s." ), weapon->tname() );
                        player_character.ammo_location = item_location();
                    } else if( player_character.has_item( *opt.ammo ) ) {
                        player_character.add_msg_if_player( _( "Selected %s as default ammo for %s." ), opt.ammo->tname(),
                                                            weapon->tname() );
                        player_character.ammo_location = opt.ammo;
                    } else {
                        player_character.add_msg_if_player(
                            _( "You need to keep that ammo on you to select it as default ammo." ) );
                    }
                }
            }
            break;

        case ACTION_INSERT_ITEM:
            insert_item();
            break;

        case ACTION_UNLOAD_CONTAINER:
            // You CAN drop things to your own tile while in the shell.
            unload_container( mouse_target );
            break;

        case ACTION_DROP:
            drop_in_direction( player_character.pos_bub() );
            break;
        case ACTION_DIR_DROP: {
            std::optional<tripoint_bub_ms> pnt = mouse_target;
            if( !pnt ) {
                pnt = choose_adjacent( _( "Drop where?" ) );
            }
            if( pnt ) {
                if( *pnt != player_character.pos_bub() && in_shell ) {
                    add_msg( m_info, _( "You can't drop things to another tile while you're in your shell." ) );
                } else {
                    drop_in_direction( *pnt );
                }
            }
            break;
        }
        case ACTION_BIONICS:
            player_character.power_bionics();
            break;
        case ACTION_MUTATIONS:
            player_character.power_mutations();
            break;

        case ACTION_SORT_ARMOR:
            player_character.worn.sort_armor( player_character );
            break;

        case ACTION_WAIT:
            openclaw_harness_trace_wait_action_dispatch();
            wait();
            break;

        case ACTION_CRAFT:
            player_character.craft();
            break;

        case ACTION_RECRAFT:
            player_character.recraft();
            break;

        case ACTION_LONGCRAFT:
            player_character.long_craft();
            break;

        case ACTION_DISASSEMBLE:
            if( player_character.controlling_vehicle ) {
                add_msg( m_info, _( "You can't disassemble items while driving." ) );
            } else {
                player_character.disassemble();
            }
            break;

        case ACTION_CONSTRUCT:
            if( player_character.in_vehicle ) {
                add_msg( m_info, _( "You can't construct while in a vehicle." ) );
            } else {
                construction_menu( false );
            }
            break;

        case ACTION_SLEEP:
            sleep();
            break;

        case ACTION_CONTROL_VEHICLE:
            if( player_character.is_mounted() ) {
                player_character.dismount();
            } else if( player_character.has_trait( trait_WAYFARER ) ) {
                add_msg( m_info, _( "You refuse to take control of this vehicle." ) );
            } else {
                control_vehicle( mouse_target );
            }
            break;

        case ACTION_TOGGLE_AUTO_TRAVEL_MODE:
            auto_travel_mode = !auto_travel_mode;
            add_msg( m_info, auto_travel_mode ? _( "Auto travel mode ON!" ) : _( "Auto travel mode OFF!" ) );
            break;

        case ACTION_TOGGLE_SAFEMODE:
            if( safe_mode == SAFE_MODE_OFF ) {
                set_safe_mode( SAFE_MODE_ON );
                mostseen = 0;
                add_msg( m_info, _( "Safe mode ON!" ) );
            } else {
                turnssincelastmon = 0_turns;
                set_safe_mode( SAFE_MODE_OFF );
                add_msg( m_info, get_option<bool>( "AUTOSAFEMODE" )
                         ? _( "Safe mode OFF!  (Auto safe mode still enabled!)" ) : _( "Safe mode OFF!" ) );
            }
            if( player_character.has_effect( effect_laserlocked ) ) {
                player_character.remove_effect( effect_laserlocked );
                safe_mode_warning_logged = false;
            }
            break;

        case ACTION_TOGGLE_AUTOSAFE: {
            // Set Auto reactivate safe mode to x
            set_next_option( "AUTOSAFEMODE" );
            break;
        }

        case ACTION_IGNORE_ENEMY:
            if( safe_mode == SAFE_MODE_STOP ) {
                add_msg( m_info, _( "Ignoring enemy!" ) );
                for( auto &elem : player_character.get_mon_visible().new_seen_mon ) {
                    monster &critter = *elem;
                    critter.ignoring = rl_dist( player_character.pos_bub(), critter.pos_bub() );
                }
                set_safe_mode( SAFE_MODE_ON );
            } else if( player_character.has_effect( effect_laserlocked ) ) {
                if( player_character.has_trait( trait_PROF_CHURL ) ) {
                    add_msg( m_warning, _( "You make the sign of the cross." ) );
                } else {
                    add_msg( m_info, _( "Ignoring laser targeting!" ) );
                }
                player_character.remove_effect( effect_laserlocked );
                safe_mode_warning_logged = false;
            }
            break;

        case ACTION_WHITELIST_ENEMY:
            if( safe_mode == SAFE_MODE_STOP && !get_safemode().empty() ) {
                get_safemode().add_rule( get_safemode().lastmon_whitelist, Creature::Attitude::ANY, 0,
                                         rule_state::WHITELISTED );
                add_msg( m_info, _( "Creature whitelisted: %s" ), get_safemode().lastmon_whitelist );
                set_safe_mode( SAFE_MODE_ON );
                mostseen = 0;
            } else {
                get_safemode().show();
            }
            break;

        case ACTION_WORKOUT:
            player_character.assign_activity( workout_activity_actor( player_character.pos_bub() ) );
            break;

        case ACTION_SUICIDE:
            if( query_yn( _( "Abandon this character?" ) ) ) {
                if( query_yn( _( "This will kill your character.  Continue?" ) ) ) {
                    player_character.set_moves( 0 );
                    player_character.place_corpse( &here );
                    uquit = QUIT_SUICIDE;
                }
            }
            break;

        case ACTION_SAVE:
            if( query_yn( _( "Save and quit?" ) ) ) {
                if( save() ) {
                    player_character.set_moves( 0 );
                    uquit = QUIT_SAVED;
                } else if( save_is_dirty && query_yn( _( "Unable to save, quit anyway?" ) ) ) {
                    // Game refused to save because of unsupported game state. But we don't want to trap them here, so at least let give them the option.
                    player_character.set_moves( 0 );
                    uquit = QUIT_NOSAVED;
                }
            }
            break;

        case ACTION_QUICKSAVE:
            quicksave();
            return false;

        case ACTION_QUICKLOAD:
            quickload();
            g->save_is_dirty = true;
            return false;

        case ACTION_PL_INFO:
            player_character.disp_info( true );
            break;

        case ACTION_MAP:
            if( !here.is_outside( player_character.pos_bub() ) ) {
                uistate.overmap_visible_weather = false;
            }
            if( you_know_where_you_are() ) {
                ui::omap::display();
            } else {
                add_msg( m_info, _( "You have no idea where you are." ) );
            }
            break;

        case ACTION_SKY:
            if( here.is_outside( player_character.pos_bub() ) ) {
                ui::omap::display_visible_weather();
            } else {
                add_msg( m_info, _( "You can't see the sky from here." ) );
            }
            break;

        case ACTION_MISSIONS:
            list_missions();
            break;

        case ACTION_DIARY:
            diary::show_diary_ui( u.get_avatar_diary() );
            break;

        case ACTION_FACTIONS:
            faction_manager_ptr->display();
            break;

        case ACTION_MORALE:
            player_character.disp_morale();
            break;

        case ACTION_MEDICAL:
            if( player_character.disp_medical() ) {
                return false;
            }
            break;

        case ACTION_BODYSTATUS:
            display_bodygraph( get_player_character() );
            break;

        case ACTION_MESSAGES:
            Messages::display_messages();
            break;

        case ACTION_HELP:
            get_help().display_help();
            break;

        case ACTION_OPTIONS:
            get_options().show( true );
            break;

        case ACTION_AUTOPICKUP:
            get_auto_pickup().show();
            break;

        case ACTION_AUTONOTES:
            get_auto_notes_settings().show_gui();
            break;

        case ACTION_SAFEMODE:
            get_safemode().show();
            break;

        case ACTION_DISTRACTION_MANAGER:
            get_distraction_manager().show();
            break;

        case ACTION_COLOR:
            all_colors.show_gui();
            break;

        case ACTION_WORLD_MODS:
            world_generator->show_active_world_mods( world_generator->active_world->active_mod_order );
            break;

        case ACTION_EXPORT_BUG_REPORT_ARCHIVE:
            debug_menu::export_save_archive_and_game_report();
            break;

        case ACTION_DEBUG:
            if( MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger() ) {
                break;    //don't do anything when sharing and not debugger
            }
            debug_menu::debug();
            break;

        case ACTION_TOGGLE_FULLSCREEN:
            toggle_fullscreen();
            break;

        case ACTION_TOGGLE_PIXEL_MINIMAP:
            toggle_pixel_minimap();
            break;

        case ACTION_TOGGLE_PANEL_ADM:
            panel_manager::get_manager().show_adm();
            break;

        case ACTION_RELOAD_TILESET:
            reload_tileset();
            break;

        case ACTION_TOGGLE_AUTO_FEATURES:
            // Set Auto Features to x
            set_next_option( "AUTO_FEATURES" );
            break;

        case ACTION_TOGGLE_AUTO_PULP_BUTCHER:
            // Set Auto pulp or butcher to x
            set_next_option( "AUTO_PULP_BUTCHER" );
            auto_features_warn();
            break;

        case ACTION_TOGGLE_AUTO_MINING:
            // Set Auto Mining to x
            set_next_option( "AUTO_MINING" );
            auto_features_warn();
            break;

        case ACTION_TOGGLE_THIEF_MODE:
            if( player_character.get_value( "THIEF_MODE" ).str() == "THIEF_ASK" ) {
                player_character.set_value( "THIEF_MODE", "THIEF_HONEST" );
                //~ Thief mode cycled between THIEF_ASK/THIEF_HONEST/THIEF_STEAL
                add_msg( _( "Thief mode: Always Honest - you will not pick up others' belongings." ) );
            } else if( player_character.get_value( "THIEF_MODE" ).str() == "THIEF_HONEST" ) {
                player_character.set_value( "THIEF_MODE", "THIEF_STEAL" );
                //~ Thief mode cycled between THIEF_ASK/THIEF_HONEST/THIEF_STEAL
                add_msg( _( "Thief mode: Always Steal - you will pick up others' belongings without prompting." ) );
            } else if( player_character.get_value( "THIEF_MODE" ).str() == "THIEF_STEAL" ) {
                player_character.set_value( "THIEF_MODE", "THIEF_ASK" );
                //~ Thief mode cycled between THIEF_ASK/THIEF_HONEST/THIEF_STEAL
                add_msg( _( "Thief mode: Default - you will be prompted when picking up owned items." ) );
            } else {
                // ERROR
                add_msg( _( "THIEF_MODE CONTAINED BAD VALUE [ %s ]!" ),
                         player_character.get_value( "THIEF_MODE" ).to_string() );
            }
            break;

        case ACTION_TOGGLE_AUTO_FORAGING:
            // Set Auto Foraging to x
            set_next_option( "AUTO_FORAGING" );
            auto_features_warn();
            break;

        case ACTION_TOGGLE_AUTO_PICKUP:
            // Set Auto pickup enabled to x
            set_next_option( "AUTO_PICKUP" );
            break;

        case ACTION_TOGGLE_HOUR_TIMER:
            toggle_debug_hour_timer();
            break;

        case ACTION_TOGGLE_DEBUG_MODE:
            if( MAP_SHARING::isCompetitive() && !MAP_SHARING::isDebugger() ) {
                break;    //don't do anything when sharing and not debugger
            }
            handle_debug_mode();
            break;

        case ACTION_TOGGLE_PREVENT_OCCLUSION:
            set_next_option( "PREVENT_OCCLUSION" );
            break;

        case ACTION_ZOOM_IN:
            zoom_in();
            mark_main_ui_adaptor_resize();
            break;

        case ACTION_ZOOM_OUT:
            zoom_out();
            mark_main_ui_adaptor_resize();
            break;

        case ACTION_ITEMACTION:
            item_action_menu();
            break;

        case ACTION_AUTOATTACK:
            avatar_action::autoattack( player_character, here );
            break;

        default:
            break;
    }

    return true;
}

bool game::handle_action()
{
    map &here = get_map();

    std::string action;
    input_context ctxt;
    action_id act = ACTION_NULL;
    user_turn current_turn;
    avatar &player_character = get_avatar();
    std::optional<semantic_surface_manager_session> semantic_session;
    std::optional<semantic_surface_scope> world_semantic_scope;
    bool semantic_action_consumed = false;
    std::optional<character_id> semantic_basecamp_mission_actor;
    // Check if we have an auto-move destination
    if( player_character.has_destination() ) {
        act = player_character.get_next_auto_move_direction();
        if( act == ACTION_NULL ) {
            add_msg( m_info, _( "Auto-move canceled" ) );
            if( player_character.has_distant_destination() ) {
                openclaw_harness_semantic_native_travel_terminal( player_character, "interrupted" );
            }
            player_character.abort_automove();
            return false;
        }
        handle_key_blocking_activity();
    } else if( player_character.has_destination_activity() ) {
        // starts destination activity after the player successfully reached his destination
        player_character.start_destination_activity();
        return false;
    } else if( uistate.open_menu ) {
        // make a copy so that uistate.open_menu can be assigned a new function
        // during the execution of the copied old function
        std::optional<std::function<void()>> open_menu_tmp = std::nullopt;
        std::swap( uistate.open_menu, open_menu_tmp );
        open_menu_tmp.value()();
        return false;
    } else {
        // No auto-move, ask player for input
        const std::string semantic_run_id = openclaw_harness_bound_semantic_run_id();
        if( !semantic_run_id.empty() ) {
            semantic_surface_manager &semantic_manager = openclaw_harness_semantic_surface_manager();
            semantic_session.emplace( semantic_manager );
            if( uquit == QUIT_WATCH && player_character.is_dead_state() ) {
                world_semantic_scope.emplace( semantic_manager, "unsupported", "Death camera",
                                              std::map<std::string, std::string>{
                    { "stop_reason", "death camera lacks semantic bindings" }
                } );
            } else {
                input_context world_context = get_default_mode_input_context();
                const std::vector<std::pair<std::string, std::string>> native_world_actions =
                    openclaw_harness_world_actions( world_context );
                std::vector<semantic_action_descriptor> semantic_actions =
                    semantic_surface_actions( native_world_actions );
                const std::vector<character_id> basecamp_mission_candidates =
                    openclaw_harness_basecamp_mission_candidates();
                for( const character_id candidate : basecamp_mission_candidates ) {
                    const npc *const actor = find_npc( candidate );
                    if( actor != nullptr ) {
                        semantic_actions.push_back( { "world.basecamp_missions",
                                                      std::to_string( candidate.get_value() ),
                                                      string_format( _( "Open Base Missions with %s" ), actor->name ), true } );
                    }
                }
                world_semantic_scope.emplace( semantic_manager, "world", "World",
                                              openclaw_harness_world_payload(),
                                              semantic_actions,
                [ &act, &semantic_basecamp_mission_actor, basecamp_mission_candidates ]( const semantic_action_request &request ) {
                    if( request.action_id == "world.pause" ) {
                        // Pause is an ordinary native one-turn action.  It
                        // reaches do_turn without opening the alarm-clock
                        // wait menus, which lets production cadence remain
                        // bound to its actual World owner.
                        act = ACTION_PAUSE;
                    } else if( request.action_id == "world.wait" ) {
                        // Keep the native action branch authoritative; this merely
                        // selects ACTION_WAIT before handle_action dispatches it.
                        act = ACTION_WAIT;
                    } else if( request.action_id == "world.quicksave" ) {
                        // Use the ordinary save-without-quit action so a native
                        // semantic setup session can persist its own changes
                        // before its separate lifecycle cleanup.
                        act = ACTION_QUICKSAVE;
                    } else if( request.action_id == "world.save_quit" ) {
                        // ACTION_SAVE retains the ordinary native confirmation
                        // prompt.  That child prompt publishes and consumes its
                        // own semantic choice before the saved process exits.
                        act = ACTION_SAVE;
                    } else if( request.action_id == "world.inventory" ) {
                        act = ACTION_INVENTORY;
                    } else if( request.action_id == "world.zone_manager" ) {
                        act = ACTION_ZONES;
                    } else if( request.action_id == "world.basecamp_missions" ) {
                        const std::string candidate_id = request.stable_id.value_or( "" );
                        const auto candidate = std::find_if( basecamp_mission_candidates.begin(),
                        basecamp_mission_candidates.end(), [ &candidate_id ]( const character_id id ) {
                            return std::to_string( id.get_value() ) == candidate_id;
                        } );
                        if( candidate == basecamp_mission_candidates.end() ) {
                            return semantic_action_dispatch_result{ false, "invalid_basecamp_actor", "" };
                        }
                        npc *const actor = g->find_npc( *candidate );
                        if( actor == nullptr || actor->is_dead() || !actor->assigned_camp ||
                            rl_dist( get_avatar().pos_abs_omt(), actor->pos_abs_omt() ) > 3 ) {
                            return semantic_action_dispatch_result{ false, "stale_basecamp_actor", "" };
                        }
                        std::optional<basecamp *> camp = overmap_buffer.find_camp( actor->assigned_camp->xy() );
                        if( !camp || !( *camp )->allowed_access_by( get_avatar() ) ) {
                            return semantic_action_dispatch_result{ false, "unavailable_basecamp", "" };
                        }
                        semantic_basecamp_mission_actor = *candidate;
                    } else if( request.action_id == "world.overmap" ) {
                        act = ACTION_MAP;
                    } else if( request.action_id == "world.chat" ) {
                        act = ACTION_CHAT;
                    } else if( request.action_id == "world.fire" ) {
                        act = ACTION_FIRE;
                    } else if( request.action_id == "world.debug_menu" ) {
                        act = ACTION_DEBUG;
                    } else if( request.action_id == "world.move.north" ) {
                        act = ACTION_MOVE_FORTH;
                    } else if( request.action_id == "world.move.south" ) {
                        act = ACTION_MOVE_BACK;
                    } else if( request.action_id == "world.move.west" ) {
                        act = ACTION_MOVE_LEFT;
                    } else if( request.action_id == "world.move.east" ) {
                        act = ACTION_MOVE_RIGHT;
                    } else {
                        return semantic_action_dispatch_result{ false, "no_native_binding", "" };
                    }
                    return semantic_action_dispatch_result{ true, "", "",
                                                            act == ACTION_INVENTORY || act == ACTION_MAP ||
                                                            act == ACTION_FIRE || act == ACTION_CHAT ||
                                                            semantic_basecamp_mission_actor.has_value() };
                } );
            }
            // The scope constructed above is the actual owner for this input
            // turn.  Let it validate and receipt at most one queued request
            // before the blocking native input path runs.
            semantic_action_consumed = world_semantic_scope->consume_request();
            // A semantic World request selects ACTION_WAIT directly, skipping
            // the physical lookup branch below.  Keep its receipt on the same
            // native action boundary as the physical route so the cockpit can
            // bind the following duration menu to this exact World frame.
            if( semantic_action_consumed && act == ACTION_WAIT ) {
                openclaw_harness_semantic_step_receipt(
                    openclaw_harness_pending_world_frame, "world.wait", true );
                openclaw_harness_pending_world_frame.clear();
            }
            if( semantic_action_consumed && semantic_basecamp_mission_actor ) {
                npc *const actor = find_npc( *semantic_basecamp_mission_actor );
                if( actor == nullptr ) {
                    return false;
                }
                // The World request has only selected the ordinary nearby
                // companion route.  The actual Base Missions selector owns
                // facts, mission availability, and dispatch from here.
                const talk_function::basecamp_mission_resolution resolution =
                    talk_function::basecamp_mission( *actor );
                if( resolution != talk_function::basecamp_mission_resolution::selector_entered ) {
                    DebugLog( D_INFO, DC_ALL ) << "openclaw_basecamp_mission_resolution=" <<
                                               static_cast<int>( resolution );
                }
                return false;
            }
        }
        if( !semantic_action_consumed ) {
            ctxt = get_player_input( action );
        }
    }

    // Remove asynchronous animations if any action taken before the input timeout
    // Otherwise repeated input can cause animations to accumulate as the timeout is never reached
    g->void_async_anim_curses();
#if defined(TILES)
    tilecontext->void_async_anim();
#else
    // Curses does not redraw itself so do it here
    g->invalidate_main_ui_adaptor();
#endif

    bool veh_ctrl = has_vehicle_control( player_character );

    // If performing an action with right mouse button, co-ordinates
    // of location clicked.
    std::optional<tripoint_bub_ms> mouse_target;

    if( uquit == QUIT_WATCH && action == "QUIT" ) {
        uquit = QUIT_DIED;
        return false;
    }

    if( act == ACTION_ACTIONMENU ) {
        if( uquit == QUIT_WATCH ) {
            return false;
        }
        // Semantic dispatch preselects this native action before the physical
        // lookup branch.  It must still enter the same menu owner.
        player_character.clear_destination();
        destination_preview.clear();
        act = handle_action_menu( here );
        if( act == ACTION_NULL ) {
            return false;
        }
    }

    if( act == ACTION_NULL ) {
        act = look_up_action( action );
        openclaw_harness_trace_default_action_dispatch( "lookup", action, act );
        if( act == ACTION_WAIT ) {
            openclaw_harness_semantic_step_receipt(
                openclaw_harness_pending_world_frame, "world.wait", true );
            openclaw_harness_pending_world_frame.clear();
        }

        if( act == ACTION_KEYBINDINGS ) {
            // already handled by input context
            return false;
        }

        if( act == ACTION_MAIN_MENU ) {
            if( uquit == QUIT_WATCH ) {
                return false;
            }
            // No auto-move actions have or can be set at this point.
            player_character.clear_destination();
            destination_preview.clear();
            act = handle_main_menu();
            if( act == ACTION_NULL ) {
                return false;
            }
        }

#if defined(TILES)
        if( gamepad::is_active() ) {
            gamepad::direction dir = gamepad::get_left_stick_direction();
            if( dir != gamepad::direction::NONE ) {
                // List of actions that can use the directional cursor
                if( act == ACTION_INTERACT || act == ACTION_SMASH || act == ACTION_GRAB ||
                    act == ACTION_PEEK || act == ACTION_UNLOAD_CONTAINER || act == ACTION_DIR_DROP ||
                    act == ACTION_EXAMINE || act == ACTION_EXAMINE_AND_PICKUP ||
                    act == ACTION_PICKUP || act == ACTION_PICKUP_ALL || act == ACTION_CONTROL_VEHICLE ) {
                    tripoint offset = gamepad::direction_to_offset( dir );
                    mouse_target = player_character.pos_bub() + tripoint_rel_ms( offset.x, offset.y, 0 );
                }
            }
        }
#endif

        if( act == ACTION_ACTIONMENU ) {
            if( uquit == QUIT_WATCH ) {
                return false;
            }
            // No auto-move actions have or can be set at this point.
            player_character.clear_destination();
            destination_preview.clear();
            act = handle_action_menu( here );
            if( act == ACTION_NULL ) {
                return false;
            }
#if defined(__ANDROID__)
            if( get_option<bool>( "ANDROID_ACTIONMENU_AUTOADD" ) && ctxt.get_category() == "DEFAULTMODE" ) {
                add_best_key_for_action_to_quick_shortcuts( act, ctxt.get_category(), false );
            }
#endif
        }

        if( act == ACTION_INTERACT ) {
            if( !mouse_target ) {
                mouse_target = player_character.pos_bub();
            }
            act = handle_interact( here, *mouse_target );
            if( act == ACTION_NULL ) {
                return false;
            }
        }

        if( act == ACTION_KEYBINDINGS ) {
            player_character.clear_destination();
            destination_preview.clear();
            act = ctxt.display_menu( true );
            if( act == ACTION_NULL ) {
                return false;
            }
        }

        if( can_action_change_worldstate( act ) ) {
            user_action_counter += 1;
        }

        if( act == ACTION_CLICK_AND_DRAG ) {
            // Need to return false to avoid disrupting actions like character mouse movement that require two clicks
            return false;
        }

        if( act == ACTION_SELECT || act == ACTION_SEC_SELECT ) {
            // Mouse button click
            if( veh_ctrl ) {
                // No mouse use in vehicle
                return false;
            }

            if( player_character.is_dead_state() ) {
                // do not allow mouse actions while dead
                return false;
            }

            const std::optional<tripoint_bub_ms> mouse_pos = ctxt.get_coordinates( w_terrain,
                    ter_view_p.raw().xy(),
                    true );
            if( !mouse_pos ) {
                return false;
            }
            if( !player_character.sees( here, *mouse_pos ) ) {
                // Not clicked in visible terrain
                return false;
            }
            mouse_target = mouse_pos;

            if( act == ACTION_SELECT ) {
                // Note: The following has the potential side effect of
                // setting auto-move destination state in addition to setting
                // act.
                if( !try_get_left_click_action( act, *mouse_target ) ) {
                    return false;
                }
            } else if( act == ACTION_SEC_SELECT ) {
                if( !try_get_right_click_action( act, *mouse_target ) ) {
                    return false;
                }
            }
        } else if( act != ACTION_TIMEOUT ) {
            // act has not been set for an auto-move, so clearing possible
            // auto-move destinations. Since initializing an auto-move with
            // the mouse may span across multiple actions, we do not clear the
            // auto-move destination if the action is only a timeout, as this
            // would require the user to double click quicker than the
            // timeout delay.
            player_character.clear_destination();
            destination_preview.clear();
        }
    }

    if( act == ACTION_NULL ) {
        const input_event &&evt = ctxt.get_raw_input();
        if( !evt.sequence.empty() ) {
            const int ch = evt.get_first_input();
            if( !get_option<bool>( "NO_UNKNOWN_COMMAND_MSG" ) ) {
                std::string msg = string_format( _( "Unknown command: \"%s\" (%ld)" ), evt.long_description(), ch );
                if( const std::optional<std::string> hint =
                        press_x_if_bound( ACTION_KEYBINDINGS ) ) {
                    msg = string_format( "%s\n%s", msg,
                                         string_format( _( "%s at any time to see and edit keybindings relevant to "
                                                           "the current context." ),
                                                        *hint ) );
                }
                add_msg( m_info, msg );
            }
        }
        return false;
    }

    // This has no action unless we're in a special game mode.
    gamemode->pre_action( act );

    int before_action_moves = player_character.get_moves();

    // These actions are allowed while deathcam is active. Registered in game::get_player_input
    if( uquit == QUIT_WATCH || !player_character.is_dead_state() ) {
        do_deathcam_action( act, player_character );
    }

    // actions allowed only while alive
    if( !player_character.is_dead_state() ) {
        if( !do_regular_action( act, player_character, mouse_target ) ) {
            return false;
        }
    }
    if( act != ACTION_TIMEOUT ) {
        player_character.mod_moves( -current_turn.moves_elapsed() );
    }
    if( act != ACTION_PAUSE ) {
        player_character.magic->break_channeling( player_character );
    }

    gamemode->post_action( act );

    player_character.movecounter = ( !player_character.is_dead_state() ? ( before_action_moves -
                                     player_character.get_moves() ) : 0 );
    dbg( D_INFO ) << string_format( "%s: [%d] %d - %d = %d", action_ident( act ),
                                    to_turn<int>( calendar::turn ), before_action_moves, player_character.movecounter,
                                    player_character.get_moves() );
    return !player_character.is_dead_state();
}
