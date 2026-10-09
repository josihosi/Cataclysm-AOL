#include <algorithm>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "game_inventory.h"
#include "faction.h"
#include "imgui/imgui.h"
#include "item.h"
#include "item_location.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "output.h"
#include "player_helpers.h"
#include "semantic_surface.h"
#include "translations.h"
#include "ui_manager.h"
#include "uistate.h"

TEST_CASE( "pickup explains a native pocket denial and permits read-only details",
           "[semantic_surface][pickup_denial]" )
{
    std::string operation = "select";
    SECTION( "details and storage selection" ) {}
    SECTION( "wield an item that cannot be stored" ) {
        operation = "inventory.wield";
    }
    SECTION( "wear an item that cannot be stored" ) {
        operation = "inventory.wear";
    }
    clear_avatar();
    clear_map();
    on_out_of_scope clear_reopened_menu( []() {
        uistate.open_menu.reset();
    } );
    restore_on_out_of_scope<int> restore_width( TERMX );
    restore_on_out_of_scope<int> restore_height( TERMY );
    restore_on_out_of_scope<int> restore_full_width( FULL_SCREEN_WIDTH );
    restore_on_out_of_scope<int> restore_full_height( FULL_SCREEN_HEIGHT );
    TERMX = FULL_SCREEN_WIDTH = 80;
    TERMY = FULL_SCREEN_HEIGHT = 24;
    const bool owns_imgui = ImGui::GetCurrentContext() == nullptr;
    if( owns_imgui ) {
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2( 800, 600 );
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->AddFontDefault();
        io.Fonts->Build();
        ImGui::NewFrame();
    }
    on_out_of_scope cleanup_imgui( [&]() {
        if( owns_imgui ) {
            ImGui::EndFrame();
            ImGui::DestroyContext();
        }
    } );
    avatar &you = get_avatar();
    // clear_avatar is naked: supply an ordinary pocket so this fixture can
    // distinguish an oversized item from a small, genuinely storable one.
    you.wear_item( item( itype_id( "pants" ) ) );
    REQUIRE( you.is_wearing( itype_id( "pants" ) ) );
    map &here = get_map();
    const tripoint_bub_ms position = you.pos_bub();
    const itype_id large_type( operation == "inventory.wear" ? "backpack" : "log" );
    item &log = here.add_item_or_charges( position, item( large_type ) );
    const std::string log_uid = std::to_string( log.uid().get_value() );
    const std::string log_name = log.tname();
    REQUIRE_FALSE( you.can_pickVolume_partial( log, false, nullptr, false, true ) );
    REQUIRE( you.can_pickWeight_partial( log, false ) );
    item &small = here.add_item_or_charges( position, item( itype_id( "smart_phone" ) ) );
    const std::string small_uid = std::to_string( small.uid().get_value() );
    REQUIRE( you.can_pickVolume_partial( small, false, nullptr, false, true ) );
    const time_point turn = calendar::turn;
    const int moves = you.get_moves();
    semantic_surface_manager manager( "pickup-denial-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope parent( manager, "world", "World" );
    int state = 0;
    int request_sequence = 0;
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        auto submit = [&]( const std::string &action, const std::optional<std::string> &uid = std::nullopt ) {
            REQUIRE( manager.submit_request( { "pickup-denial-run", descriptor.surface_id,
                                               descriptor.frame_id, std::to_string( ++request_sequence ), action, uid, {} } ) );
        };
        if( descriptor.kind == "inventory" ) {
            const auto advertised = [&]( const std::string &action, const std::string &uid ) ->
            const semantic_action_descriptor & {
                const auto found = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
                [&]( const semantic_action_descriptor &candidate ) {
                    return candidate.id == action && candidate.stable_id == uid;
                } );
                REQUIRE( found != descriptor.valid_actions.end() );
                return *found;
            };
            const auto &blocked = advertised( "inventory.select", log_uid );
            CHECK_FALSE( blocked.enabled );
            CHECK( blocked.label == log_name + " — " + _( "Does not fit in any pocket!" ) );
            CHECK( advertised( "inventory.details", log_uid ).enabled );
            CHECK( advertised( "inventory.wield", log_uid ).enabled );
            CHECK( advertised( "inventory.wear", log_uid ).enabled == ( operation == "inventory.wear" ) );
            CHECK( advertised( "inventory.select", small_uid ).enabled );
            if( operation != "select" ) {
                REQUIRE( state == 0 );
                state = 4;
                submit( operation, log_uid );
            } else if( state == 0 ) {
                CHECK( descriptor.payload.at( "selected_items" ) == "{}" );
                state = 1;
                submit( "inventory.details", log_uid );
            } else if( state == 2 ) {
                CHECK( descriptor.payload.at( "selected_items" ) == "{}" );
                state = 3;
                submit( "inventory.toggle", small_uid );
            } else {
                REQUIRE( state == 3 );
                const JsonObject selected = json_loader::from_string( descriptor.payload.at( "selected_items" ) );
                CHECK_FALSE( selected.has_member( log_uid ) );
                REQUIRE( selected.has_member( small_uid ) );
                selected.allow_omitted_members();
                state = 4;
                submit( "inventory.commit" );
            }
        } else if( descriptor.kind == "item_info" ) {
            REQUIRE( state == 1 );
            CHECK( descriptor.payload.at( "item_name" ) == log_name );
            state = 2;
            submit( "item_info.close" );
        }
    } );
    const drop_locations selected = game_menus::inv::pickup( { position } );
    if( operation != "select" ) {
        CHECK( selected.empty() );
        CHECK( state == 4 );
        REQUIRE( receipts.size() == 1 );
        CHECK( receipts.front().accepted );
        REQUIRE_FALSE( you.activity.is_null() );
        process_activity( you );
        if( operation == "inventory.wield" ) {
            REQUIRE( you.get_wielded_item() );
            CHECK( you.get_wielded_item()->typeId() == large_type );
        } else {
            CHECK( you.is_wearing( large_type ) );
        }
        REQUIRE( here.i_at( position ).size() == 1 );
        CHECK( here.i_at( position ).begin()->uid() == small.uid() );
        return;
    }
    REQUIRE( selected.size() == 1 );
    CHECK( std::to_string( selected.front().first->uid().get_value() ) == small_uid );
    CHECK( selected.front().second == 1 );
    CHECK( state == 4 );
    REQUIRE( receipts.size() == 4 );
    for( const semantic_action_receipt &receipt : receipts ) {
        CHECK( receipt.accepted );
        CHECK_FALSE( receipt.resulting_frame_id.empty() );
    }
    CHECK( calendar::turn == turn );
    CHECK( you.get_moves() == moves );
}

#if !defined(TILES)
#include <clocale>
#include <cstdlib>
#include <optional>
#include <map>
#include <fstream>
#include "compatibility.h"
#include "cached_options.h"
#include "do_turn.h"
#include "options_helpers.h"
#include "worldfactory.h"
#include "cursesdef.h"
#include "game.h"
#include "input.h"
#include "player_activity.h"
extern "C" int ungetch( int );
extern "C" int flushinp();

namespace
{
class pickup_environment
{
    public:
        pickup_environment( const char *name, const std::string &value ) : name( name ) {
            if( const char *old = std::getenv( name ) ) {
                previous = old;
            }
            setenv( name, value.c_str(), 1 );
        }
        ~pickup_environment() {
            if( previous ) {
                setenv( name.c_str(), previous->c_str(), 1 );
            } else {
                unsetenv( name.c_str() );
            }
        }
    private:
        std::string name;
        std::optional<std::string> previous;
};
}

TEST_CASE( "queued pickup after Wield owns a fresh native cancel and World successor",
           "[semantic_surface][pickup_denial][queued_pickup_067]" )
{
    const bool outer = GENERATE( false, true );
    CAPTURE( outer );
    const std::string run = outer ? "queued-pickup-outer" : "queued-pickup-bare";
    pickup_environment run_env( "OPENCLAW_HARNESS_RUN_ID", run );
    pickup_environment bound_env( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run );
    pickup_environment request_env( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", "" );
    auto &manager = openclaw_harness_semantic_surface_manager();
    REQUIRE( manager.stack().empty() );
    semantic_surface_manager enclosing( run + "-enclosing" );
    semantic_surface_manager &pickup_manager = outer ? enclosing : manager;
    std::optional<semantic_surface_manager_session> enclosing_session;
    if( outer ) {
        enclosing_session.emplace( enclosing );
    }
    const auto expected_manager = active_semantic_surface_manager();
    clear_avatar();
    clear_map();
    // The public turn caller needs the ordinary native game mode, which Catch
    // has not started. Restore it through the established save/load path.
    REQUIRE( world_generator->active_world );
    const std::string world_name = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world_name ) );
    clear_avatar();
    clear_map();
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    restore_on_out_of_scope<time_point> restore_turn( calendar::turn );
    restore_on_out_of_scope<decltype( g->uquit )> restore_quit( g->uquit );
    g->uquit = QUIT_NO;
    const std::string locale = std::setlocale( LC_CTYPE, nullptr );
    REQUIRE( std::setlocale( LC_CTYPE, "en_US.UTF-8" ) );
    on_out_of_scope restore_locale( [&]() { std::setlocale( LC_CTYPE, locale.c_str() ); } );
    if( !imclient ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    const int timeout = inp_mngr.get_timeout();
    inp_mngr.set_timeout( 1 );
    on_out_of_scope cleanup( [&]() {
        uistate.open_menu.reset();
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
        enclosing.set_descriptor_observer( {} );
        enclosing.set_receipt_observer( {} );
        inp_mngr.set_timeout( timeout );
        ::flushinp();
        catacurses::endwin();
    } );
    avatar &you = get_avatar();
    you.wear_item( item( itype_id( "pants" ) ) );
    map &here = get_map();
    const auto position = you.pos_bub();
    item &large = here.add_item_or_charges( position, item( itype_id( "log" ) ) );
    const auto large_uid = std::to_string( large.uid().get_value() );
    item &small = here.add_item_or_charges( position, item( itype_id( "smart_phone" ) ) );
    const auto small_uid = std::to_string( small.uid().get_value() );
    REQUIRE_FALSE( you.can_pickVolume_partial( large, false, nullptr, false, true ) );
    semantic_surface_descriptor first;
    semantic_surface_descriptor reopened;
    std::map<std::string, semantic_action_receipt> receipts;
    int phase = 0;
    const auto observe = [&]( const semantic_surface_descriptor &descriptor ) {
        auto &owner = descriptor.kind == "world" ? manager : pickup_manager;
        const auto submit = [&]( const std::string &id, const std::string &action,
                                 std::optional<std::string> uid = std::nullopt ) {
            REQUIRE( owner.submit_request( { descriptor.run_id, descriptor.surface_id,
                                             descriptor.frame_id, id, action, uid, {} } ) );
        };
        if( descriptor.kind == "inventory" && phase == 0 ) {
            first = descriptor;
            phase = 1;
            submit( "wield-once", "inventory.wield", large_uid );
        } else if( descriptor.kind == "inventory" && phase == 2 ) {
            reopened = descriptor;
            phase = 3;
            CHECK( descriptor.surface_id != first.surface_id );
            auto invalid = semantic_action_request{ descriptor.run_id, first.surface_id,
                           first.frame_id, "stale-original", "inventory.cancel", std::nullopt, {} };
            REQUIRE( owner.submit_request( invalid ) );
            CHECK_FALSE( owner.consume_top_request() );
            invalid.surface_id = descriptor.surface_id;
            invalid.frame_id = descriptor.frame_id;
            invalid.run_id = "foreign-run";
            invalid.request_id = "foreign-run";
            REQUIRE( owner.submit_request( invalid ) );
            CHECK_FALSE( owner.consume_top_request() );
            CHECK_FALSE( owner.submit_request( { first.run_id, first.surface_id, first.frame_id,
                                                "wield-once", "inventory.wield", large_uid, {} } ) );
            CHECK_FALSE( owner.has_pending_request() );
            submit( "reopened-cancel", "inventory.cancel" );
        } else if( descriptor.kind == "world" && phase >= 2 && phase < 5 ) {
            const bool second_turn = phase == 4;
            phase = second_turn ? 5 : 4;
            submit( second_turn ? "world-next-turn" : "world-successor", "world.pause" );
        }
    };
    const auto receipt = [&]( const semantic_action_receipt &value ) {
        receipts[value.request_id] = value;
    };
    manager.set_descriptor_observer( observe );
    manager.set_receipt_observer( receipt );
    enclosing.set_descriptor_observer( observe );
    enclosing.set_receipt_observer( receipt );
    {
        semantic_surface_manager_session initial( pickup_manager );
        CHECK( game_menus::inv::pickup( { position } ).empty() );
        REQUIRE( phase == 1 );
        REQUIRE_FALSE( you.activity.is_null() );
        process_activity( you );
    }
    REQUIRE( you.get_wielded_item() );
    CHECK( you.get_wielded_item()->typeId() == itype_id( "log" ) );
    REQUIRE( uistate.open_menu );
    REQUIRE( active_semantic_surface_manager() == expected_manager );
    phase = 2;
    // A normal keyboard cancellation bounds the unfixed before control. It
    // neither creates semantic authority nor repairs an unknown native action.
    REQUIRE( ::ungetch( 27 ) != -1 );
    you.set_moves( 100 );
    CHECK_FALSE( g->do_turn() );
    ::flushinp();
    REQUIRE_FALSE( reopened.surface_id.empty() );
    REQUIRE( phase == 4 );
    CHECK_FALSE( uistate.open_menu );
    CHECK( active_semantic_surface_manager() == expected_manager );
    REQUIRE( receipts.count( "reopened-cancel" ) == 1 );
    CHECK( receipts.at( "reopened-cancel" ).accepted );
    CHECK_FALSE( receipts.at( "stale-original" ).accepted );
    CHECK_FALSE( receipts.at( "foreign-run" ).accepted );
    CHECK( you.activity.is_null() );
    REQUIRE( here.i_at( position ).size() == 1 );
    CHECK( std::to_string( here.i_at( position ).begin()->uid().get_value() ) == small_uid );
    you.set_moves( 100 );
    CHECK_FALSE( g->do_turn() );
    CHECK( phase == 5 );
    CHECK( manager.stack().empty() );
    CHECK( pickup_manager.stack().empty() );
    CHECK( active_semantic_surface_manager() == expected_manager );
    REQUIRE( receipts.count( "world-successor" ) == 1 );
    CHECK( receipts.at( "world-successor" ).accepted );
}

TEST_CASE( "pickup wield activity publishes its native disposal successor outside input ownership",
           "[semantic_surface][pickup_disposal_diagnostic_067][pickup_disposal_067]" )
{
    const bool retained_session = GENERATE( false, true );
    const std::string choice = GENERATE( std::string( "drop" ), std::string( "cancel" ),
                                        std::string( "steal-no" ), std::string( "steal-drop" ),
                                        std::string( "off-drop" ) );
    const bool stealing = choice == "steal-no" || choice == "steal-drop";
    const bool off_mode = choice == "off-drop";
    const bool semantic_disposal = choice != "steal-no" && ( !off_mode || retained_session );
    const bool changed_weapon = choice == "drop" || choice == "steal-drop" || off_mode;
    CAPTURE( retained_session, choice );
    const std::string run = std::string( retained_session ? "disposal-enclosing-" :
                                       "disposal-native-" ) + choice;
    pickup_environment run_env( "OPENCLAW_HARNESS_RUN_ID", run );
    pickup_environment bound_env( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run );
    pickup_environment request_env( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", "" );
    auto &manager = openclaw_harness_semantic_surface_manager();
    REQUIRE( manager.stack().empty() );
    REQUIRE( active_semantic_surface_manager() == nullptr );
    semantic_surface_manager enclosing( run );
    clear_avatar();
    clear_map();
    REQUIRE( world_generator->active_world );
    const std::string world_name = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world_name ) );
    clear_avatar();
    clear_map();
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    override_option autosave( "AUTOSAVE", "false" );
    restore_on_out_of_scope<time_point> restore_turn( calendar::turn );
    restore_on_out_of_scope<decltype( g->uquit )> restore_quit( g->uquit );
    g->uquit = QUIT_NO;
    const std::string locale = std::setlocale( LC_CTYPE, nullptr );
    REQUIRE( std::setlocale( LC_CTYPE, "en_US.UTF-8" ) );
    on_out_of_scope restore_locale( [&]() { std::setlocale( LC_CTYPE, locale.c_str() ); } );
    if( !imclient ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    const int timeout = inp_mngr.get_timeout();
    inp_mngr.set_timeout( 1 );
    on_out_of_scope cleanup( [&]() {
        uistate.open_menu.reset();
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
        enclosing.set_descriptor_observer( {} );
        enclosing.set_receipt_observer( {} );
        inp_mngr.set_timeout( timeout );
        ::flushinp();
        catacurses::endwin();
    } );
    avatar &you = get_avatar();
    you.wear_item( item( itype_id( "pants" ) ) );
    item old_gun( itype_id( "m240" ) );
    old_gun.set_owner( you );
    REQUIRE( you.wield( old_gun ) );
    const int64_t old_uid = you.get_wielded_item()->uid().get_value();
    map &here = get_map();
    const auto position = you.pos_bub();
    item gun( itype_id( "m240" ) );
    if( stealing ) {
        gun.set_owner( faction_id( "hells_raiders" ) );
    } else {
        gun.set_owner( you );
    }
    you.set_value( "THIEF_MODE", "THIEF_ASK" );
    item &replacement = here.add_item_or_charges( position, std::move( gun ) );
    const int64_t new_uid = replacement.uid().get_value();
    const std::string new_stable = std::to_string( new_uid );
    here.add_item_or_charges( position, item( itype_id( "smart_phone" ) ) );
    REQUIRE_FALSE( you.can_stash( *you.get_wielded_item() ) );
    REQUIRE( you.can_wield( replacement ).success() );
    std::ofstream evidence;
    if( const char *path = std::getenv( "CAOL_R067_DISPOSAL_EVIDENCE" ) ) {
        evidence.open( path, std::ios::app );
    }
    const auto record = [&]( const std::string &stage, const std::string &detail ) {
        if( evidence ) {
            evidence << run << " " << stage << " " << detail << "\n";
            evidence.flush();
        }
    };
    int phase = 0;
    int disposal_descriptors = 0;
    int stealing_descriptors = 0;
    int accepted_disposals = 0;
    int guards = 0;
    std::map<std::string, semantic_action_receipt> receipts;
    const auto receipt = [&]( const semantic_action_receipt &value ) {
        receipts[value.request_id] = value;
        if( value.request_id == "dispose-choice" && value.accepted ) {
            ++accepted_disposals;
        }
        record( "receipt", value.request_id + " accepted=" + ( value.accepted ? "true" : "false" ) + " reason=" + value.rejection_reason );
    };
    const auto observe = [&]( const semantic_surface_descriptor &descriptor ) {
        record( "descriptor", descriptor.kind + " " + descriptor.surface_id + " " + descriptor.frame_id );
        auto *owner = active_semantic_surface_manager();
        REQUIRE( owner != nullptr );
        const auto submit = [&]( const std::string &id, const std::string &action,
                                 std::optional<std::string> stable = std::nullopt ) {
            REQUIRE( owner->submit_request( { run, descriptor.surface_id, descriptor.frame_id,
                                               id, action, stable, {} } ) );
        };
        if( descriptor.kind == "world" && phase == 0 ) {
            phase = 1;
            submit( "pickup", "world.pickup" );
        } else if( descriptor.kind == "inventory" && phase == 1 ) {
            phase = 2;
            for( const auto &action : descriptor.valid_actions ) {
                if( action.id == "inventory.wield" ) {
                    record( "wield-action", action.stable_id + " enabled=" +
                            ( action.enabled ? "true" : "false" ) );
                }
            }
            record( "requested-uid", new_stable );
            submit( "replace-wield", "inventory.wield", new_stable );
            // This is a scheduling control, not action authority. The native
            // input consumes the request and assigns its own wield actor; the
            // next real turn executes it after handle_action has returned.
            you.set_moves( 0 );
        } else if( descriptor.kind == "prompt" && phase == 2 ) {
            ++stealing_descriptors;
            REQUIRE( stealing );
            CHECK( owner == ( retained_session ? &enclosing : &manager ) );
            CHECK( descriptor.payload.at( "text" ).find( "stealing" ) != std::string::npos );
            const auto answer = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            [&]( const semantic_action_descriptor &action ) {
                return action.id == "prompt.choose" && action.enabled &&
                       action.label == ( choice == "steal-no" ? "NO" : "YES" );
            } );
            REQUIRE( answer != descriptor.valid_actions.end() );
            submit( "stealing-choice", answer->id, answer->stable_id );
        } else if( descriptor.kind == "menu" && phase == 2 ) {
            ++disposal_descriptors;
            CHECK( owner == ( retained_session ? &enclosing : &manager ) );
            CHECK( descriptor.payload.at( "text" ).find( "Stop wielding" ) != std::string::npos );
            const auto drop = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            []( const semantic_action_descriptor &action ) {
                return action.id == "menu.choose" && action.enabled &&
                       action.label.find( "Drop item" ) != std::string::npos;
            } );
            REQUIRE( drop != descriptor.valid_actions.end() );
            // Exercise the actual current modal's refusal path without a
            // keyboard fallback or mutating the native selected option.
            const auto reject = [&]( semantic_action_request request ) {
                REQUIRE( owner->submit_request( std::move( request ) ) );
                CHECK_FALSE( owner->consume_top_request() );
                ++guards;
            };
            reject( { "wrong-run", descriptor.surface_id, descriptor.frame_id,
                      "foreign-disposal", drop->id, drop->stable_id, {} } );
            reject( { run, descriptor.surface_id, "stale-frame", "stale-disposal",
                      drop->id, drop->stable_id, {} } );
            const auto store = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            []( const semantic_action_descriptor &action ) {
                return action.id == "menu.choose" && !action.enabled &&
                       action.label.find( "Store in inventory" ) != std::string::npos;
            } );
            REQUIRE( store != descriptor.valid_actions.end() );
            reject( { run, descriptor.surface_id, descriptor.frame_id, "denied-store",
                      store->id, store->stable_id, {} } );
            record( "disposal", descriptor.payload.at( "text" ) );
            if( choice == "cancel" ) {
                submit( "dispose-choice", "menu.cancel" );
            } else {
                submit( "dispose-choice", drop->id, drop->stable_id );
            }
        } else if( descriptor.kind == "inventory" &&
                   ( phase == 3 || ( phase == 2 && you.activity.is_null() ) ) ) {
            phase = 4;
            submit( "reopened-cancel", "inventory.cancel" );
        } else if( descriptor.kind == "world" && phase >= 3 ) {
            phase = 5;
            submit( "world-successor", "world.pause" );
        }
    };
    manager.set_receipt_observer( receipt );
    manager.set_descriptor_observer( observe );
    enclosing.set_receipt_observer( receipt );
    enclosing.set_descriptor_observer( observe );
    test_mode = false;
    // Public World -> Pickup -> actual item dispatch. Ending moves in the
    // descriptor observer separates input ownership from next-turn execution.
    you.set_moves( 100 );
    CHECK_FALSE( g->do_turn() );
    REQUIRE( phase == 2 );
    REQUIRE_FALSE( you.activity.is_null() );
    REQUIRE( you.get_wielded_item()->uid().get_value() == old_uid );
    REQUIRE( active_semantic_surface_manager() == nullptr );
    record( "before-do-turn", "active_manager=null activity=" + you.activity.id().str() );
    {
        std::optional<pickup_environment> ordinary_binding;
        if( off_mode ) {
            ordinary_binding.emplace( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", "" );
            record( "mode", "unbound native keyboard control" );
            if( !retained_session ) {
                // Ordinary off-mode input, not a fallback for a bound session.
                REQUIRE( ::ungetch( '2' ) != -1 );
            }
        }
        std::optional<semantic_surface_manager_session> control_session;
        if( retained_session ) {
            control_session.emplace( enclosing );
        }
        you.set_moves( 1 );
        // Actual pre-input activity caller; no copied activity loop or
        // private handle_action access shim is used.
        CHECK_FALSE( g->do_turn() );
        CHECK( active_semantic_surface_manager() == ( retained_session ? &enclosing : nullptr ) );
        ::flushinp();
    }
    REQUIRE( ( phase == 2 || phase == 5 ) );
    record( "after-activity", "menu_descriptors=" + std::to_string( disposal_descriptors ) );
    REQUIRE( you.get_wielded_item() );
    CHECK( you.get_wielded_item()->typeId() == itype_id( "m240" ) );
    CHECK( ( you.get_wielded_item()->uid().get_value() != old_uid ) == changed_weapon );
    CHECK( std::none_of( here.i_at( position ).begin(), here.i_at( position ).end(),
    [&]( const item &it ) { return it.uid().get_value() == new_uid; } ) == changed_weapon );
    CHECK( you.activity.is_null() );
    CHECK( std::count_if( here.i_at( position ).begin(), here.i_at( position ).end(),
    [&]( const item &it ) { return it.typeId() == itype_id( "m240" ); } ) == 1 );
    if( phase == 2 ) {
        REQUIRE( uistate.open_menu );
        phase = 3;
        you.set_moves( 100 );
        CHECK_FALSE( g->do_turn() );
    }
    CHECK( phase == 5 );
    CHECK_FALSE( uistate.open_menu );
    CHECK( manager.stack().empty() );
    CHECK( active_semantic_surface_manager() == nullptr );
    CHECK( disposal_descriptors == ( semantic_disposal ? 1 : 0 ) );
    CHECK( stealing_descriptors == ( stealing ? 1 : 0 ) );
    CHECK( accepted_disposals == ( semantic_disposal ? 1 : 0 ) );
    CHECK( guards == ( semantic_disposal ? 3 : 0 ) );
    if( semantic_disposal ) {
        auto &owner = retained_session ? enclosing : manager;
        const auto &accepted = receipts.at( "dispose-choice" );
        CHECK_FALSE( owner.submit_request( { run, accepted.requested_surface_id,
                                            accepted.requested_frame_id, "dispose-choice",
                                            "menu.cancel", std::nullopt, {} } ) );
        CHECK_FALSE( owner.has_pending_request() );
        CHECK( receipts.at( "foreign-disposal" ).rejection_reason == "wrong_run" );
        CHECK( receipts.at( "stale-disposal" ).rejection_reason == "stale_frame" );
        CHECK_FALSE( receipts.at( "denied-store" ).accepted );
    }
    REQUIRE( receipts.count( "replace-wield" ) == 1 );
    CHECK( receipts.at( "replace-wield" ).accepted );
    REQUIRE( receipts.count( "reopened-cancel" ) == 1 );
    CHECK( receipts.at( "reopened-cancel" ).accepted );
}
#endif
