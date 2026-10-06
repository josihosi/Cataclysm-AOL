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
#include <vector>

#include "cached_options.h"
#include "avatar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "compatibility.h"
#include "cursesdef.h"
#include "game.h"
#include "main_menu.h"
#include "semantic_surface.h"
#include "worldfactory.h"

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
        avatar previous_avatar = std::move( get_avatar() );
        on_out_of_scope restore_avatar( [&] { get_avatar() = std::move( previous_avatar ); } );

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
#endif
