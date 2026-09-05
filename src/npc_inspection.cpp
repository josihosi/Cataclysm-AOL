#include "npc_inspection.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <utility>

#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_imgui.h"
#include "color.h"
#include "enum_conversions.h"
#include "game.h"
#include "game_constants.h"
#include "imgui/imgui.h"
#include "input_context.h"
#include "item.h"
#include "item_location.h"
#include "json.h"
#include "npc.h"
#include "npctalk_rules.h"
#include "output.h"
#include "pocket_type.h"
#include "string_formatter.h"
#include "translations.h"
#include "ui_manager.h"
#include "visitable.h"

namespace
{
struct inspected_item {
    const item *value;
    std::string parent_uid;
    std::string slot;
};

std::vector<inspected_item> physical_items( npc &actor )
{
    std::vector<inspected_item> result;
    const std::vector<const item *> pseudo = actor.get_pseudo_items();
    std::set<const item *> visited;
    std::function<void( const item *, const std::string &, const std::string & )> append;
    append = [&]( const item *value, const std::string &parent, const std::string &slot ) {
        if( !visited.insert( value ).second ) {
            return;
        }
        result.push_back( { value, parent, slot } );
        const std::string uid = std::to_string( value->uid().get_value() );
        // visit_items only recurses through CONTAINER pockets.  Inspection also
        // retains magazines, ammunition, installed mods and other stored entries.
        for( int kind = 0; kind < static_cast<int>( pocket_type::LAST ); ++kind ) {
            const pocket_type pocket = static_cast<pocket_type>( kind );
            for( const item *child : value->all_items_top( pocket ) ) {
                append( child, uid, io::enum_to_string( pocket ) );
            }
        }
    };
    actor.visit_items( [&]( item *value, item * ) {
        if( std::find( pseudo.begin(), pseudo.end(), value ) == pseudo.end() ) {
            const std::string slot = actor.is_wielding( *value ) ? "wielded" :
                                     actor.is_worn( *value ) ? "worn" : "inventory";
            append( value, "", slot );
        }
        return VisitResponse::SKIP;
    } );
    return result;
}

const item *owned_item( npc &actor, const std::string &uid )
{
    for( const inspected_item &entry : physical_items( actor ) ) {
        if( std::to_string( entry.value->uid().get_value() ) == uid ) {
            return entry.value;
        }
    }
    // Deliberately do not use find_item_by_uid's cross-owner fallback.
    return nullptr;
}

std::string json_text( const std::function<void( JsonOut & )> &write )
{
    std::ostringstream out;
    JsonOut json( out );
    write( json );
    return out.str();
}

std::map<std::string, std::string> item_facts( npc &actor, const item &value,
        const std::vector<iteminfo> &info )
{
    return {
        { "actor_id", npc_inspection_actor_id( actor ) },
        { "item_uid", std::to_string( value.uid().get_value() ) },
        { "item_name", value.tname() },
        { "type_id", value.typeId().str() },
        { "provenance", "Diagnostic actor inventory; actor ownership revalidated by UID. Item text is native player-evaluated item::info(true), not NPC effectiveness." },
        { "item_info_text", format_item_info( info, {} ) }
    };
}

class inspection_window : public cataimgui::window
{
    public:
        explicit inspection_window( const std::string &title ) :
            cataimgui::window( title, ImGuiWindowFlags_NoNavInputs ) {}
        std::string text;
        std::vector<std::pair<std::string, std::string>> items;
        std::vector<iteminfo> item_details;
        std::optional<std::string> selected;
        bool closed = false;
        cataimgui::scroll scroll = cataimgui::scroll::none;

    protected:
        cataimgui::bounds get_bounds() override {
            return { -1.f, -1.f, static_cast<float>( str_width_to_pixels( FULL_SCREEN_WIDTH ) ),
                     static_cast<float>( str_height_to_pixels( FULL_SCREEN_HEIGHT ) ) };
        }
        void draw_controls() override {
            cataimgui::set_scroll( scroll );
            scroll = cataimgui::scroll::none;
            cataimgui::draw_colored_text( text );
            display_item_info( item_details, {} );
            for( const auto &entry : items ) {
                ImGui::PushID( entry.first.c_str() );
                if( ImGui::Button( entry.second.c_str() ) ) {
                    selected = entry.first;
                }
                ImGui::PopID();
            }
            if( ImGui::Button( _( "Close" ) ) ) {
                closed = true;
            }
        }
};

input_context inspection_context()
{
    input_context ctxt( "default" );
    for( const char *action : { "QUIT", "CONFIRM", "PAGE_UP", "PAGE_DOWN", "HOME", "END" } ) {
        ctxt.register_action( action );
    }
    ctxt.set_timeout( 10 );
    return ctxt;
}

void physical_input( inspection_window &window, input_context &ctxt )
{
    const std::string action = ctxt.handle_input();
    if( action == "QUIT" || action == "CONFIRM" ) {
        window.closed = true;
    } else if( action == "PAGE_UP" ) {
        window.scroll = cataimgui::scroll::page_up;
    } else if( action == "PAGE_DOWN" ) {
        window.scroll = cataimgui::scroll::page_down;
    } else if( action == "HOME" ) {
        window.scroll = cataimgui::scroll::begin;
    } else if( action == "END" ) {
        window.scroll = cataimgui::scroll::end;
    }
}

void show_item( npc &actor, const std::string &uid )
{
    const item *value = owned_item( actor, uid );
    if( value == nullptr ) {
        return;
    }
    inspection_window window( _( "NPC item details" ) );
    window.text = string_format( _( "Diagnostic inventory of %s\n%s\nItem information is evaluated by the native player item-info formatter." ),
                                 actor.get_name(), value->tname() );
    value->info( true, window.item_details );
    const auto facts = item_facts( actor, *value, window.item_details );
    std::optional<semantic_surface_scope> scope;
    if( semantic_surface_manager *manager = active_semantic_surface_manager() ) {
        scope.emplace( *manager, "npc_item_info", _( "NPC item details" ), facts,
                       std::vector<semantic_action_descriptor>{ { "npc_item_info.close", "", _( "Close" ), true } },
        [&window]( const semantic_action_request &request ) {
            if( request.action_id != "npc_item_info.close" ) {
                return semantic_action_dispatch_result{ false, "unadvertised_action", "" };
            }
            window.closed = true;
            return semantic_action_dispatch_result{ true, "", "" };
        } );
    }
    input_context ctxt = inspection_context();
    while( !window.closed && window.get_is_open() ) {
        ui_manager::redraw();
        if( scope ) {
            semantic_surface_manager *manager = active_semantic_surface_manager();
            manager->poll_request_transport();
            const bool pending = manager->has_pending_request();
            if( scope->consume_request() || pending ) {
                continue;
            }
        }
        if( !window.closed ) {
            physical_input( window, ctxt );
        }
    }
}
} // namespace

std::string npc_inspection_actor_id( const npc &actor )
{
    return string_format( "character:%d", actor.getID().get_value() );
}

npc *resolve_npc_inspection_actor( avatar &viewer, const std::string &actor_id )
{
    for( Creature *creature : viewer.get_visible_creatures( SEEX ) ) {
        npc *actor = creature == nullptr ? nullptr : creature->as_npc();
        if( actor != nullptr && !actor->is_dead() && npc_inspection_actor_id( *actor ) == actor_id ) {
            return actor;
        }
    }
    return nullptr;
}

std::vector<semantic_action_descriptor> npc_inspection_world_actions( avatar &viewer )
{
    std::vector<semantic_action_descriptor> actions;
    for( Creature *creature : viewer.get_visible_creatures( SEEX ) ) {
        const npc *actor = creature == nullptr ? nullptr : creature->as_npc();
        if( actor != nullptr && !actor->is_dead() ) {
            actions.push_back( { "world.inspect_npc", npc_inspection_actor_id( *actor ),
                                 string_format( _( "Inspect %s (includes diagnostics)" ), actor->get_name() ), true } );
        }
    }
    return actions;
}

std::map<std::string, std::string> npc_inspection_payload( npc &actor, avatar &viewer )
{
    const auto items = physical_items( actor );
    const auto labels = follower_rules_ui::semantic_labels( actor );
    const auto rules = follower_rules_ui::semantic_payload( actor, labels );
    return {
        { "schema", "caol-npc-inspection-v1" },
        { "actor_id", npc_inspection_actor_id( actor ) },
        { "actor_name", actor.get_name() },
        { "calendar_turn", std::to_string( to_turns<int>( calendar::turn - calendar::turn_zero ) ) },
        { "provenance", "Native NPC inspection; visible facts use npc::print_info getters. diagnostic_* facts are internal state, not avatar knowledge or gameplay proof. Items include every pocket type, excluding virtual pseudo-items." },
        { "item_details_parameters", "npc_inspection.item_details requires parameter item_uid from diagnostic_items; ownership is revalidated against actor_id." },
        { "visible", json_text( [&]( JsonOut & json ) {
                json.start_object();
                json.member( "health", actor.hp_description().first );
                json.member( "attitude", Creature::get_attitude_ui_data( actor.attitude_to( viewer ) ).first.translated() );
                json.member( "wielded", actor.is_armed() ? actor.get_wielded_item()->tname() : "" );
                json.member( "wielded_item_uid", actor.is_armed() ?
                             std::to_string( actor.get_wielded_item()->uid().get_value() ) : "" );
                json.member( "worn" );
                json.start_array();
                for( const item_location &loc : actor.get_visible_worn_items() ) {
                    json.start_object();
                    json.member( "item_uid", std::to_string( loc->uid().get_value() ) );
                    json.member( "name", loc->tname() );
                    json.end_object();
                }
                json.end_array();
                json.end_object();
            } ) },
        { "diagnostic_health", json_text( [&]( JsonOut & json ) {
                json.start_object();
                for( const bodypart_id &part : actor.get_all_body_parts() ) {
                    json.member( part.id().str() );
                    json.start_object();
                    json.member( "name", body_part_name( part ) );
                    json.member( "hp", actor.get_part_hp_cur( part ) );
                    json.member( "hp_max", actor.get_part_hp_max( part ) );
                    json.end_object();
                }
                json.end_object();
            } ) },
        { "diagnostic_orders", json_text( [&]( JsonOut & json ) {
                json.start_object();
                json.member( "attitude", npc_attitude_name( actor.get_attitude() ) );
                json.member( "mission", io::enum_to_string( actor.mission ) );
                json.member( "activity", actor.current_activity_id.str() );
                json.member( "camp_patrol_order", actor.has_camp_patrol_order() );
                json.member( "guard_post", actor.get_guard_post() );
                json.member( "move_target", actor.goto_to_this_pos );
                json.member( "assigned_camp", actor.assigned_camp );
                json.member( "assigned_camp_coordinate_system", "absolute_omt" );
                json.member( "order_coordinate_system", "absolute_ms" );
                json.member( "has_companion_mission", actor.has_companion_mission() );
                if( actor.has_companion_mission() ) {
                    const npc_companion_mission mission = actor.get_companion_mission();
                    json.member( "companion_mission", mission.miss_id.id );
                    json.member( "companion_role", mission.role_id );
                    json.member( "companion_destination", mission.destination );
                }
                json.end_object();
            } ) },
        { "diagnostic_rules", json_text( [&]( JsonOut & json ) {
                json.write( rules );
            } ) },
        { "diagnostic_item_count", std::to_string( items.size() ) },
        { "diagnostic_items", json_text( [&]( JsonOut & json ) {
                json.start_object();
                for( const inspected_item &entry : items ) {
                    const item &value = *entry.value;
                    const std::string uid = std::to_string( value.uid().get_value() );
                    json.member( uid );
                    json.start_object();
                    json.member( "item_uid", uid );
                    json.member( "parent_uid", entry.parent_uid );
                    json.member( "slot", entry.slot );
                    json.member( "name", value.tname() );
                    json.member( "type_id", value.typeId().str() );
                    json.member( "charges", value.charges );
                    json.end_object();
                }
                json.end_object();
            } ) }
    };
}

std::map<std::string, std::string> npc_inspection_item_payload( npc &actor,
        const std::string &item_uid )
{
    const item *value = owned_item( actor, item_uid );
    if( value == nullptr ) {
        return {};
    }
    std::vector<iteminfo> info;
    value->info( true, info );
    return item_facts( actor, *value, info );
}

void show_npc_inspection( character_id actor_id )
{
    npc *actor = g->find_npc( actor_id );
    if( actor == nullptr ) {
        return;
    }
    const std::string identity = npc_inspection_actor_id( *actor );
    if( resolve_npc_inspection_actor( get_avatar(), identity ) != actor ) {
        return;
    }
    const auto facts = npc_inspection_payload( *actor, get_avatar() );
    inspection_window window( _( "NPC inspection" ) );
    window.text = string_format( _( "%s\nDiagnostic inspection: exact health, orders, rules and all stored items. These facts include information the avatar cannot see.\nHealth: %s\nOrders: %s\nSelect an item for its native details." ),
                                 actor->get_name(), actor->hp_description().first,
                                 actor->describe_mission() );
    for( const bodypart_id &part : actor->get_all_body_parts() ) {
        window.text += string_format( "\n%s: %d/%d", body_part_name( part ),
                                      actor->get_part_hp_cur( part ), actor->get_part_hp_max( part ) );
    }
    window.text += string_format( "\n%s: %s", _( "Assigned camp (absolute OMT)" ),
                                  actor->assigned_camp ? actor->assigned_camp->to_string() : _( "None" ) );
    const auto rule_labels = follower_rules_ui::semantic_labels( *actor );
    const auto rule_facts = follower_rules_ui::semantic_payload( *actor, rule_labels );
    for( const auto &rule : ally_rule_strs ) {
        window.text += "\n" + rule_labels.at( actor->rules.has_flag( rule.second.rule ) ?
                                              rule.second.rule_true_text : rule.second.rule_false_text );
    }
    for( const char *key : { "engagement", "aim", "cbm_recharge", "cbm_reserve" } ) {
        window.text += "\n" + rule_facts.at( key );
    }
    const std::vector<semantic_action_descriptor> actions = {
        { "npc_inspection.close", "", _( "Close" ), true },
        { "npc_inspection.item_details", "", _( "Item details (requires item_uid parameter)" ), true }
    };
    for( const inspected_item &entry : physical_items( *actor ) ) {
        const std::string uid = std::to_string( entry.value->uid().get_value() );
        const std::string label = entry.slot + ": " + entry.value->tname();
        window.items.emplace_back( uid, label );

    }
    std::optional<semantic_surface_scope> scope;
    if( semantic_surface_manager *manager = active_semantic_surface_manager() ) {
        scope.emplace( *manager, "npc_inspection", _( "NPC inspection" ), facts, actions,
        [&window, &identity]( const semantic_action_request &request ) {
            if( request.action_id == "npc_inspection.close" ) {
                window.closed = true;
                return semantic_action_dispatch_result{ true, "", "" };
            }
            npc *current = resolve_npc_inspection_actor( get_avatar(), identity );
            if( current == nullptr ) {
                return semantic_action_dispatch_result{ false, "stale_actor_id", "" };
            }
            if( request.action_id != "npc_inspection.item_details" ) {
                return semantic_action_dispatch_result{ false, "unadvertised_action", "" };
            }
            const auto selected = request.parameters.find( "item_uid" );
            if( selected == request.parameters.end() || selected->second.empty() ) {
                return semantic_action_dispatch_result{ false, "missing_item_uid", "" };
            }
            if( owned_item( *current, selected->second ) == nullptr ) {
                return semantic_action_dispatch_result{ false, "stale_actor_item_uid", "" };
            }
            window.selected = selected->second;
            return semantic_action_dispatch_result{ true, "", "", true };
        } );
    }
    input_context ctxt = inspection_context();
    while( !window.closed && window.get_is_open() ) {
        ui_manager::redraw();
        bool pending = false;
        if( scope ) {
            semantic_surface_manager *manager = active_semantic_surface_manager();
            manager->poll_request_transport();
            pending = manager->has_pending_request();
            scope->consume_request();
        }
        if( window.selected ) {
            npc *current = resolve_npc_inspection_actor( get_avatar(), identity );
            if( current != nullptr ) {
                show_item( *current, *window.selected );
            }
            window.selected.reset();
        } else if( !pending && !window.closed ) {
            physical_input( window, ctxt );
        }
    }
}
