#if !defined(TILES)
// Native curses input and renderer, including transport wakes.  No game is
// launched: opening_screen's normal no-game/quit result returns to Catch.
#include "input.h"
// Avoid ncurses macros/global names colliding with gameplay headers.
extern "C" int ungetch( int );

#include <algorithm>
#include <chrono>
#include <clocale>
#if defined(_WIN32)
#include <windows.h>
#endif
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <sstream>
#include <vector>

#include "cached_options.h"
#include "avatar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "compatibility.h"
#include "cursesdef.h"
#include "game.h"
#include "do_turn.h"
#include "ui_manager.h"
#include "faction.h"
#include "npc.h"
#include "map_helpers.h"
#include "map.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "scores_ui.h"
#include "output.h"
#include "input_context.h"
#include <imgui/imgui.h>
#include "main_menu.h"
#include "loading_ui.h"
#include "semantic_surface.h"
#include "worldfactory.h"
#include "json.h"
#include "json_loader.h"
#include "mutation.h"

namespace
{
class scoped_environment
{
    public:
        scoped_environment( const char *name, const std::string &value ) : name( name ) {
            if( const char *old = std::getenv( name ) ) {
                previous = old;
            }
            setenv( name, value.c_str(), 1 );
        }
        ~scoped_environment() {
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
// MainMenu reloads definition storage. A moved avatar retains raw variant/type
// pointers into that old storage, so restore through native deserialization.
std::string snapshot_avatar()
{
    std::ostringstream out;
    JsonOut json( out );
    get_avatar().serialize( json );
    return out.str();
}

void restore_avatar_snapshot( const std::string &snapshot )
{
    get_avatar() = avatar();
    get_avatar().deserialize( json_loader::from_string( snapshot ).get_object() );
}
} // namespace

TEST_CASE( "main menu keeps a logical owner through native polling and retires real transitions",
           "[semantic_surface][main_menu_closeout_067]" )
{
    const std::string mode = GENERATE( std::string( "idle_guards" ), std::string( "selection" ),
                                      std::string( "no_then_yes" ) );
    INFO( mode );
    DYNAMIC_SECTION( mode ) {
        const std::string run = "main-menu-" + mode;
        const std::filesystem::path transport = std::filesystem::temp_directory_path() /
                ( run + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) +
                  ".jsonl" );
        std::ofstream( transport ).close();
        scoped_environment run_env( "OPENCLAW_HARNESS_RUN_ID", run );
        scoped_environment bound_env( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run );
        scoped_environment transport_env( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", transport.string() );
        on_out_of_scope remove_transport( [&] { std::filesystem::remove( transport ); } );
        semantic_surface_manager &manager = openclaw_harness_semantic_surface_manager();
        REQUIRE( manager.stack().empty() );
        semantic_surface_manager outer( "unrelated-outer" );
        semantic_surface_manager_session outer_session( outer );

        // Menu entry resets the player as well as world metadata.  Keep the
        // actual test avatar alive and restore it before Catch weather cleanup.
        const std::string previous_avatar = snapshot_avatar();

        // opening_screen deliberately releases and reloads world metadata.
        // Restore the test world/options before Catch's weather cleanup runs.
        REQUIRE( world_generator->active_world );
        const std::string world_name = world_generator->active_world->world_name;
        const auto world_options = world_generator->active_world->WORLD_OPTIONS;
        const auto world_mods = world_generator->active_world->active_mod_order;
        on_out_of_scope restore_world( [&] {
            WORLD *world = world_generator->get_world( world_name );
            world->WORLD_OPTIONS = world_options;
            world->active_mod_order = world_mods;
            world_generator->set_active_world( world );
            // init_strings loads core data only.  Restore the test world's
            // finalized mod data (including regional/weather definitions).
            g->load_world_modfiles();
            restore_avatar_snapshot( previous_avatar );
        } );

        // Catch initializes gameplay data in the C locale; this renderer fixture
        // requires a real Unicode terminal, rather than acknowledging a setup modal.
        const std::string old_locale = std::setlocale( LC_CTYPE, nullptr );
        REQUIRE( std::setlocale( LC_CTYPE, "en_US.UTF-8" ) != nullptr );
        on_out_of_scope restore_locale( [&] { std::setlocale( LC_CTYPE, old_locale.c_str() ); } );
    #if defined(_WIN32)
        const UINT old_codepage = GetConsoleOutputCP();
        REQUIRE( SetConsoleOutputCP( CP_UTF8 ) );
        on_out_of_scope restore_codepage( [&] { SetConsoleOutputCP( old_codepage ); } );
    #endif

        // Initialize once: a later case reuses the same curses/ImGui client.
        static bool initialized = false;
        if( !initialized ) {
            catacurses::init_interface();
            catacurses::resizeterm();
            initialized = true;
        }
        restore_on_out_of_scope<bool> restore_test_mode( test_mode );
        test_mode = false;
        const int previous_timeout = inp_mngr.get_timeout();
        inp_mngr.set_timeout( 1 );
        on_out_of_scope cleanup( [&] {
            manager.set_descriptor_observer( {} );
            manager.set_receipt_observer( {} );
            inp_mngr.set_timeout( previous_timeout );
            catacurses::endwin();
        } );
        std::vector<semantic_surface_descriptor> descriptors;
        std::vector<semantic_action_receipt> receipts;
        semantic_surface_descriptor first;
        std::optional<semantic_action_request> chosen;
        bool duplicate_checked = false;
        bool stale_sent = false;
        int rejected_wakes = 0;
        int confirmations = 0;
        bool sent_no = false;
        int sequence = 0;
        const auto append = [&]( semantic_action_request request ) {
            std::ofstream file( transport, std::ios::app );
            file << "{\"run_id\":\"" << request.run_id << "\",\"surface_id\":\"" << request.surface_id
                 << "\",\"frame_id\":\"" << request.frame_id << "\",\"request_id\":\"" << request.request_id
                 << "\",\"action_id\":\"" << request.action_id << "\"";
            if( request.stable_id ) {
                file << ",\"stable_id\":\"" << *request.stable_id << "\"";
            }
            file << "}\n";
        };
        const auto quit = [&]( const semantic_surface_descriptor &descriptor ) {
            semantic_action_request request{ run, descriptor.surface_id, descriptor.frame_id,
                                            "quit-" + std::to_string( ++sequence ), "main_menu.quit", std::nullopt, {} };
            chosen = request;
            append( request );
        };
        const auto inert_wake = [&]( int index ) {
            semantic_action_request request{ run, first.surface_id, first.frame_id,
                                            "inert-" + std::to_string( index ), "main_menu.quit", std::nullopt, {} };
            if( index == 0 ) {
                request.run_id = "wrong-run";
            } else if( index == 1 ) {
                request.surface_id = "foreign-owner";
            } else if( index == 2 ) {
                request.frame_id = "stale-frame";
            } else {
                request.action_id = "unadvertised";
            }
            append( request );
        };
        manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
            descriptors.push_back( descriptor );
            if( descriptor.kind == "main_menu" ) {
                if( first.surface_id.empty() ) {
                    first = descriptor;
                    input_context idle( "MAIN_MENU" );
                    idle.register_action( "ANY_INPUT" );
                    CHECK( idle.handle_input( 1 ) == "TIMEOUT" );
                    REQUIRE( manager.top() );
                    CHECK( manager.top()->surface_id == first.surface_id );
                    CHECK( manager.top()->frame_id == first.frame_id );
                    if( mode == "selection" ) {
                        // Real native key mapping/selection, not a semantic fake
                        // or adjusted production menu state.
                        const auto &keys = inp_mngr.get_input_for_action( "RIGHT", "MAIN_MENU" );
                        const auto key = std::find_if( keys.begin(), keys.end(), []( const input_event &event ) {
                            return event.type == input_event_t::keyboard_char && event.sequence.size() == 1 &&
                                   event.get_first_input() >= 0 && event.get_first_input() < 128;
                        } );
                        REQUIRE( key != keys.end() );
                        REQUIRE( ::ungetch( key->get_first_input() ) != -1 );
                    } else {
                        inert_wake( 0 );
                    }
                } else if( mode == "selection" && !stale_sent ) {
                    stale_sent = true;
                    append( { run, first.surface_id, first.frame_id, "stale-selection", "main_menu.quit",
                              std::nullopt, {} } );
                } else if( sent_no && confirmations == 1 ) {
                    REQUIRE( descriptor.surface_id != first.surface_id );
                    quit( descriptor );
                }
            } else if( descriptor.kind == "prompt" ) {
                CHECK( manager.stack().size() == 1 ); // Parent is gone before query_yn.
                CHECK( descriptor.payload.at( "text" ).rfind( "Really quit?", 0 ) == 0 );
                ++confirmations;
                const bool say_no = mode == "no_then_yes" && confirmations == 1;
                const auto option = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
                [&]( const semantic_action_descriptor &action ) {
                    return action.id == "prompt.choose" && action.label == ( say_no ? "NO" : "YES" );
                } );
                REQUIRE( option != descriptor.valid_actions.end() );
                sent_no = say_no || sent_no;
                append( { run, descriptor.surface_id, descriptor.frame_id,
                          "answer-" + std::to_string( confirmations ), "prompt.choose", option->stable_id, {} } );
            }
        } );
        manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
            receipts.push_back( receipt );
            if( receipt.request_id.rfind( "inert-", 0 ) == 0 ) {
                CHECK_FALSE( receipt.accepted );
                ++rejected_wakes;
                if( rejected_wakes < 4 ) {
                    inert_wake( rejected_wakes );
                } else {
                    quit( first );
                }
            } else if( receipt.request_id == "stale-selection" ) {
                CHECK_FALSE( receipt.accepted );
                CHECK( receipt.rejection_reason == "wrong_surface" );
                REQUIRE( manager.top() );
                quit( *manager.top() );
            } else if( chosen && receipt.request_id == chosen->request_id ) {
                if( !receipt.accepted ) {
                    // Let the original producer exit after recording its churn
                    // failure; never retry silently to make the check pass.
                    CHECK( receipt.accepted );
                    REQUIRE( manager.top() );
                    // Do not recursively dispatch from a receipt observer.
                    // A native test-only Quit exits the failing old producer;
                    // the refused advertised intent remains failed evidence.
                    const auto &keys = inp_mngr.get_input_for_action( "QUIT", "MAIN_MENU" );
                    const auto key = std::find_if( keys.begin(), keys.end(), []( const input_event &event ) {
                        return event.type == input_event_t::keyboard_char && event.sequence.size() == 1 &&
                               event.get_first_input() >= 0 && event.get_first_input() < 128;
                    } );
                    REQUIRE( key != keys.end() );
                    REQUIRE( ::ungetch( key->get_first_input() ) != -1 );
                } else if( !duplicate_checked ) {
                    duplicate_checked = true;
                    CHECK_FALSE( manager.submit_request( *chosen ) );
                }
            }
        } );
        main_menu menu;
        CHECK_FALSE( menu.opening_screen() ); // Native Yes returns normal quit/no game.
        CHECK( active_semantic_surface_manager() == &outer );
        CHECK( manager.stack().empty() );
        CHECK_FALSE( manager.has_pending_request() );
        CHECK( duplicate_checked );
        CHECK( confirmations == ( mode == "no_then_yes" ? 2 : 1 ) );
        const auto count_menus = std::count_if( descriptors.begin(), descriptors.end(),
        []( const semantic_surface_descriptor &descriptor ) { return descriptor.kind == "main_menu"; } );
        CHECK( count_menus == ( mode == "idle_guards" ? 1 : 2 ) );
        if( mode != "selection" ) {
            CHECK( rejected_wakes == 4 );
            for( const semantic_action_receipt &receipt : receipts ) {
                if( receipt.request_id.rfind( "inert-", 0 ) == 0 ) {
                    CHECK( receipt.consuming_surface_id == first.surface_id );
                    CHECK( receipt.consuming_frame_id == first.frame_id );
                }
            }
        } else {
            REQUIRE( descriptors.size() >= 2 );
            const auto selected = descriptors[1].payload.find( "selected_menu" );
            const auto initial = first.payload.find( "selected_menu" );
            CHECK( selected != descriptors[1].payload.end() );
            CHECK( initial != first.payload.end() );
            if( selected != descriptors[1].payload.end() && initial != first.payload.end() ) {
                CHECK( selected->second != initial->second );
            }
        }
    }
}
TEST_CASE( "Scores close uses native QUIT and retires before death epilogues",
           "[semantic_surface][scores_closeout_067]" )
{
    const std::string mode = GENERATE( "semantic", "keyboard", "death_chain", "death_no",
                                      "watch_semantic", "watch_keyboard" );
    const bool watching = mode == "watch_semantic" || mode == "watch_keyboard";
    const bool death_chain = mode != "semantic" && mode != "keyboard";
    CAPTURE( mode );
    const std::string run = "scores-" + mode;
    const std::filesystem::path transport = std::filesystem::temp_directory_path() /
            ( run + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) +
              ".jsonl" );
    std::ofstream( transport, std::ios::binary ).close();
    scoped_environment run_env( "OPENCLAW_HARNESS_RUN_ID", run );
    scoped_environment bound_env( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run );
    scoped_environment transport_env( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", transport.string() );
    on_out_of_scope remove_transport( [&] { std::filesystem::remove( transport ); } );
    semantic_surface_manager &manager = openclaw_harness_semantic_surface_manager();
    REQUIRE( manager.stack().empty() );
    semantic_surface_manager_session session( manager );
    const std::string old_locale = std::setlocale( LC_CTYPE, nullptr );
    REQUIRE( std::setlocale( LC_CTYPE, "en_US.UTF-8" ) != nullptr );
    on_out_of_scope restore_locale( [&] { std::setlocale( LC_CTYPE, old_locale.c_str() ); } );
#if defined(_WIN32)
    const UINT old_codepage = GetConsoleOutputCP();
    REQUIRE( SetConsoleOutputCP( CP_UTF8 ) );
    on_out_of_scope restore_codepage( [&] { SetConsoleOutputCP( old_codepage ); } );
#endif
    if( ImGui::GetCurrentContext() == nullptr ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    const int previous_timeout = inp_mngr.get_timeout();
    // This direct JSONL fixture has no broker wake source. Use the same
    // bounded native polling as the MainMenu fixture, including epilogues.
    inp_mngr.set_timeout( 1 );
    on_out_of_scope cleanup( [&] {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
        manager.set_transport_observer( {} );
        inp_mngr.set_timeout( previous_timeout );
        catacurses::endwin();
    } );
    const auto native_quit = []( const std::string &category ) {
        const auto &keys = inp_mngr.get_input_for_action( "QUIT", category );
        const auto key = std::find_if( keys.begin(), keys.end(), []( const input_event &event ) {
            return event.type == input_event_t::keyboard_char && event.sequence.size() == 1 &&
                   event.get_first_input() >= 0 && event.get_first_input() < 128;
        } );
        REQUIRE( key != keys.end() );
        REQUIRE( ::ungetch( key->get_first_input() ) != -1 );
    };
    const auto append = [&]( const semantic_action_request &request ) {
        std::ofstream file( transport, std::ios::binary | std::ios::app );
        file << "{\"run_id\":\"" << request.run_id << "\",\"surface_id\":\"" << request.surface_id
             << "\",\"frame_id\":\"" << request.frame_id << "\",\"request_id\":\"" << request.request_id
             << "\",\"action_id\":\"" << request.action_id << "\"}\n";
    };
    semantic_surface_descriptor scores;
    std::vector<semantic_action_receipt> receipts;
    int score_publications = 0;
    int camera_publications = 0;
    int camera_rejections = 0;
    bool camera_duplicate_checked = false;
    bool camera_blocked_dispatch = false;
    semantic_surface_descriptor camera;
    std::optional<std::thread> camera_delivery;
    on_out_of_scope join_camera_delivery( [&] {
        if( camera_delivery ) {
            camera_delivery->join();
        }
    } );
    const auto camera_request = [&]( const std::string &id ) {
        return semantic_action_request{ run, camera.surface_id, camera.frame_id, id,
                                        "death_camera.close", std::nullopt, {} };
    };
    const auto reject_camera = [&]( int index ) {
        auto request = camera_request( "camera-reject-" + std::to_string( index ) );
        if( index == 0 ) {
            request.run_id = "foreign-run";
        } else if( index == 1 ) {
            request.frame_id = "stale-frame";
        } else if( index == 2 ) {
            request.surface_id = "foreign-owner";
        } else {
            request.action_id = "world.pause";
        }
        REQUIRE( manager.submit_request( request ) );
        CHECK_FALSE( manager.consume_top_request() );
    };
    int epilogues = 0;
    int rejected = 0;
    bool duplicate_checked = false;
    bool stale_successor_sent = false;
    const auto close_request = [&] ( const std::string &id ) {
        return semantic_action_request{ run, scores.surface_id, scores.frame_id, id, "scores.close",
                                        std::nullopt, {} };
    };
    const auto reject_request = [&]( int index ) {
        semantic_action_request request = close_request( "reject-" + std::to_string( index ) );
        if( index == 0 ) {
            request.run_id = "foreign-run";
        } else if( index == 1 ) {
            request.frame_id = "stale-frame";
        } else if( index == 2 ) {
            request.surface_id = "foreign-owner";
        } else {
            request.action_id = "unadvertised";
        }
        append( request );
    };
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( !descriptor.breadcrumbs.empty() && descriptor.breadcrumbs.back() == "Death camera" ) {
            ++camera_publications;
            camera = descriptor;
            REQUIRE( watching );
            CHECK( descriptor.kind == "death_camera" );
            if( descriptor.kind != "death_camera" ) {
                // Keep the before-repair control bounded: the missing semantic
                // capability fails, then ordinary native QUIT closes the fixture.
                native_quit( "DEFAULTMODE" );
                return;
            }
            REQUIRE( descriptor.valid_actions.size() == 1 );
            CHECK( descriptor.valid_actions.front().id == "death_camera.close" );
            CHECK( descriptor.payload.at( "native_owner" ) == "DEFAULTMODE" );
            ui_manager::redraw();
            input_context idle( "DEFAULTMODE" );
            CHECK( idle.handle_input( 1 ) == "TIMEOUT" );
            REQUIRE( manager.top() );
            CHECK( manager.top()->frame_id == camera.frame_id );
            CHECK( manager.top()->surface_id == camera.surface_id );
            if( mode == "watch_keyboard" ) {
                native_quit( "DEFAULTMODE" );
            } else {
                for( int index = 0; index < 4; ++index ) {
                    reject_camera( index );
                }
                const auto request = camera_request( "camera-close" );
                camera_delivery.emplace( [&, request] {
                    std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
                    append( request );
                } );
            }
        } else if( descriptor.kind == "scores" ) {
            ++score_publications;
            scores = descriptor;
            REQUIRE( descriptor.valid_actions.size() == 1 );
            CHECK( descriptor.valid_actions.front().id == "scores.close" );
            CHECK( descriptor.payload.at( "native_owner" ) == "SCORES_UI" );
            // Actual redraw and native timeout leave the one close capability current.
            ui_manager::redraw();
            input_context idle( "SCORES_UI" );
            CHECK( idle.handle_input( 1 ) == "TIMEOUT" );
            REQUIRE( manager.top() );
            CHECK( manager.top()->frame_id == descriptor.frame_id );
            CHECK( manager.top()->surface_id == descriptor.surface_id );
            if( mode == "keyboard" ) {
                native_quit( "SCORES_UI" );
            } else if( mode == "semantic" ) {
                reject_request( 0 );
            } else {
                if( watching ) {
                    append( camera_request( "stale-camera-successor" ) );
                }
                append( close_request( "close" ) );
            }
        } else if( descriptor.kind == "terminal" ) {
            CHECK( descriptor.payload.at( "actual_death" ) == "true" );
            REQUIRE( manager.submit_request( { run, descriptor.surface_id, descriptor.frame_id,
                                               "end-confirm", "terminal.confirm", std::nullopt, {} } ) );
        } else if( descriptor.kind == "message_log" ) {
            append( { run, descriptor.surface_id, descriptor.frame_id, "messages-close",
                      "message_log.close", std::nullopt, {} } );
        } else if( descriptor.kind == "prompt" ) {
            INFO( "Actual prompt: " << descriptor.payload.at( "text" ) );
            const bool diary = descriptor.payload.at( "text" ).find( "Open diary" ) != std::string::npos;
            const bool watch = descriptor.payload.at( "text" ).find( "Watch the last moments" ) == 0;
            const bool switch_character = descriptor.payload.at( "text" ).find(
                                              "You have died.  Continue as one of your followers?" ) == 0;
            CHECK( ( diary || watch || switch_character ||
                     descriptor.payload.at( "text" ).find( "Really quit?" ) == 0 ) );
            const auto no = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            [&]( const semantic_action_descriptor &action ) {
                return action.id == "prompt.choose" && action.label == ( diary || switch_character || ( watch && !watching ) ? "NO" : "YES" );
            } );
            REQUIRE( no != descriptor.valid_actions.end() );
            REQUIRE( manager.submit_request( { run, descriptor.surface_id, descriptor.frame_id,
                                               diary ? "diary-no" : switch_character ? "death-followers-no" : watch ? "watch-choice" : "menu-yes", no->id, no->stable_id, {} } ) );
        } else if( descriptor.kind == "scrollable_text" ) {
            CHECK( descriptor.payload.at( "native_owner" ) == "SCROLLABLE_TEXT" );
            const auto stack = manager.stack();
            CHECK( std::none_of( stack.begin(), stack.end(),
            []( const semantic_surface_descriptor &entry ) { return entry.kind == "scores"; } ) );
            input_context idle( "SCROLLABLE_TEXT" );
            CHECK( idle.handle_input( 1 ) == "TIMEOUT" );
            CHECK( manager.top()->frame_id == descriptor.frame_id );
            if( mode == "keyboard" ) {
                // Native DOWN then QUIT exercise the unchanged paging route.
                native_quit( "SCROLLABLE_TEXT" );
                const auto &keys = inp_mngr.get_input_for_action( "DOWN", "SCROLLABLE_TEXT" );
                REQUIRE_FALSE( keys.empty() );
                REQUIRE( ::ungetch( keys.front().get_first_input() ) != -1 );
            } else {
                CHECK( death_chain );
                ++epilogues;
                if( !stale_successor_sent ) {
                    stale_successor_sent = true;
                    append( close_request( "stale-successor" ) );
                }
                append( { run, descriptor.surface_id, descriptor.frame_id,
                          "epilogue-" + std::to_string( epilogues ), "scrollable_text.close", std::nullopt, {} } );
            }
        } else if( descriptor.kind == "main_menu" ) {
            CHECK( death_chain );
            append( { run, descriptor.surface_id, descriptor.frame_id, "menu-quit",
                      "main_menu.quit", std::nullopt, {} } );
        }
    } );
    manager.set_transport_observer( [&]( const semantic_request_transport_event &event ) {
        if( event.event == "request_consumed" && event.request_id == "camera-close" ) {
            camera_blocked_dispatch = true;
            CHECK( g->uquit == QUIT_WATCH );
            CHECK( inp_mngr.get_timeout() == 125 );
            std::cout << "R067_CAMERA_NATIVE_CONSUME watch=" << ( g->uquit == QUIT_WATCH )
                      << " timeout=" << inp_mngr.get_timeout() << '\n';
        }
    } );
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
        if( receipt.request_id.rfind( "camera-reject-", 0 ) == 0 ) {
            CHECK_FALSE( receipt.accepted );
            CHECK( receipt.consuming_surface_id == camera.surface_id );
            ++camera_rejections;
        } else if( receipt.request_id == "camera-close" && !camera_duplicate_checked ) {
            camera_duplicate_checked = true;
            CHECK( receipt.accepted );
            // The deferred receipt is published on the actual successor,
            // after ordinary QUIT has committed and input timeout was restored.
            CHECK( g->uquit == QUIT_DIED );
            CHECK_FALSE( manager.submit_request( camera_request( "camera-close" ) ) );
        } else if( receipt.request_id == "stale-camera-successor" ) {
            CHECK_FALSE( receipt.accepted );
            CHECK( receipt.rejection_reason == "wrong_surface" );
        } else if( receipt.request_id.rfind( "reject-", 0 ) == 0 ) {
            CHECK_FALSE( receipt.accepted );
            CHECK( receipt.consuming_surface_id == scores.surface_id );
            CHECK( receipt.consuming_frame_id == scores.frame_id );
            if( ++rejected < 4 ) {
                reject_request( rejected );
            } else {
                append( close_request( "close" ) );
            }
        } else if( receipt.request_id == "close" && !duplicate_checked ) {
            CHECK( receipt.accepted );
            duplicate_checked = true;
            CHECK_FALSE( manager.submit_request( close_request( "close" ) ) );
        } else if( receipt.request_id == "stale-successor" ) {
            CHECK_FALSE( receipt.accepted );
            CHECK( receipt.rejection_reason == "wrong_surface" );
        }
    } );
    if( death_chain ) {
        // Catch has no running game mode.  The real save/load finalization
        // installs the ordinary mode required by death_screen's first caller.
        REQUIRE( world_generator->active_world );
        const std::string world_name = world_generator->active_world->world_name;
        REQUIRE( g->save() );
        REQUIRE( turn_handler::cleanup_at_end() );
        world_generator->set_active_world( nullptr );
        REQUIRE( g->load( world_name ) );
        clear_character( get_avatar(), true );
        clear_map();
        clear_npcs();
        npc &follower = spawn_npc( { 60, 60 }, "test_talker" );
        g->add_npc_follower( follower.getID() );
        faction *with_epilogue = nullptr;
        for( const auto &entry : g->faction_manager_ptr->all() ) {
            faction *fac = g->faction_manager_ptr->get( entry.first );
            fac->known_by_u = false;
            if( !with_epilogue && !fac->epilogue().empty() ) {
                with_epilogue = fac;
            }
        }
        REQUIRE( with_epilogue );
        with_epilogue->known_by_u = true;
        override_option retain_world( "WORLD_END", "keep" );
        override_option animations( "ANIMATIONS", "false" );
        override_option deathcam( "DEATHCAM", mode == "death_chain" ? "never" : "ask" );
        restore_on_out_of_scope<quit_status> restore_quit( g->uquit );

        const tripoint_abs_sm previous_map_origin = get_map().get_abs_sub();
        get_avatar().set_part_hp_cur( bodypart_id( "head" ), 0 );
        on_out_of_scope restore_head( [&] {
            get_avatar().set_part_hp_cur( bodypart_id( "head" ),
                                         get_avatar().get_part_hp_max( bodypart_id( "head" ) ) );
        } );
        REQUIRE( g->do_turn() ); // Actual Watch choice, input, and death cleanup chain.
        CHECK( g->uquit == QUIT_DIED );
        CHECK( camera_publications == ( watching ? 1 : 0 ) );
        CHECK( camera_rejections == ( mode == "watch_semantic" ? 4 : 0 ) );
        CHECK( epilogues == 2 );
        CHECK( stale_successor_sent );
        // Death cleanup unloaded the map.  Restore only the HP changed by
        // this fixture, without a full character reset that calls setpos.
        get_avatar().set_part_hp_cur( bodypart_id( "head" ),
                                     get_avatar().get_part_hp_max( bodypart_id( "head" ) ) );
        const auto world_options = world_generator->active_world->WORLD_OPTIONS;
        const auto world_mods = world_generator->active_world->active_mod_order;
        const std::string previous_avatar = snapshot_avatar();
        on_out_of_scope restore_world( [&] {
            WORLD *world = world_generator->get_world( world_name );
            world->WORLD_OPTIONS = world_options;
            world->active_mod_order = world_mods;
            world_generator->set_active_world( world );
            // Restore the whole unloaded test world, including tracker/map
            // ownership, before another generator saves. The dead save itself
            // was moved to the graveyard by the real cleanup chain.
            g->setup();
            restore_avatar_snapshot( previous_avatar );
            g->load_map( previous_map_origin );
            loading_ui::done();
        } );
        main_menu menu;
        CHECK_FALSE( menu.opening_screen() );
        CHECK( std::any_of( receipts.begin(), receipts.end(), []( const semantic_action_receipt &receipt ) {
            return receipt.request_id == "menu-yes" && receipt.accepted;
        } ) );
    } else {
        show_scores_ui();
        if( mode == "keyboard" ) {
            std::string long_text;
            for( int i = 0; i < 100; ++i ) {
                long_text += "Native scroll line " + std::to_string( i ) + "\n";
            }
            scrollable_text( [] { return catacurses::newwin( 10, 40, point::zero ); },
                             "Paging control", long_text );
        }
    }
    CHECK( score_publications == 1 );
    CHECK( camera_publications == ( watching ? 1 : 0 ) );
    CHECK( camera_duplicate_checked == ( mode == "watch_semantic" ) );
    CHECK( camera_blocked_dispatch == ( mode == "watch_semantic" ) );
    CHECK( manager.stack().empty() );
    CHECK_FALSE( manager.has_pending_request() );
    CHECK( rejected == ( mode == "semantic" ? 4 : 0 ) );
    CHECK( duplicate_checked == ( mode != "keyboard" ) );
    CHECK( active_semantic_surface_manager() == &manager );
}

TEST_CASE( "menu fixture restores avatar variant identity after real definition reload",
           "[semantic_surface][main_menu_avatar_reload_067]" )
{
    REQUIRE( world_generator->active_world );
    const auto original = snapshot_avatar();
    on_out_of_scope restore( [&] { restore_avatar_snapshot( original ); } );
    trait_id selected;
    std::string variant_id;
    for( const auto &trait : mutation_branch::get_all() ) {
        if( !trait.variants.empty() ) {
            selected = trait.id;
            variant_id = trait.variants.begin()->first;
            break;
        }
    }
    REQUIRE( selected.is_valid() );
    REQUIRE_FALSE( variant_id.empty() );
    get_avatar().set_mutation( selected, selected->variant( variant_id ) );
    const auto snapshot = snapshot_avatar();
    // This is MainMenu's actual definition reload, followed by the fixture's
    // real mod restoration. Do not read the old avatar's variant afterward.
    g->load_core_data();
    g->load_world_modfiles();
    restore_avatar_snapshot( snapshot );
    const auto restored = get_avatar().get_mutations_variants();
    CHECK( std::any_of( restored.begin(), restored.end(), [&]( const trait_and_var &entry ) {
        return entry.trait == selected && entry.variant == variant_id;
    } ) );
    // Exercise the serialization caller that faulted on Windows, with current
    // registry-owned variants rather than a stale-pointer probe.
    const auto saved = json_loader::from_string( snapshot_avatar() ).get_object();
    saved.allow_omitted_members();
    const auto mutations = saved.get_object( "mutations" );
    mutations.allow_omitted_members();
    const auto mutation = mutations.get_object( selected.str() );
    mutation.allow_omitted_members();
    CHECK( mutation.get_string( "variant-id" ) == variant_id );
    CHECK( mutation.get_string( "variant-parent" ) == selected.str() );
}

#endif
