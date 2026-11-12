#include <algorithm>
#include <chrono>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>
#include <ctime>
#include <vector>

#include "cata_catch.h"
#include "harness_light_fixture.h"
#include "cached_options.h"
#include "calendar.h"
#include "avatar.h"
#include "bandit_live_world.h"
#include "bandit_live_world_probe.h"
#include "bodygraph.h"
#include "input.h"
#include "activity_actor_definitions.h"
#include "player_activity.h"
#include "map.h"
#include "map_helpers_tests.h"
#include "mapbuffer.h"
#include "map_helpers.h"
#include "do_turn.h"
#include "messages.h"
#include "move_mode.h"
#include <iostream>
#include "npc.h"
#include "monster.h"
#include "horde_entity.h"
#include "migo_population_native.h"
#include "migo_tower_ai.h"
#include "migo_tower_native.h"
#include "mtype.h"
#include "options_helpers.h"
#include "overmap.h"
#include "overmap_ui.h"
#include "overmapbuffer.h"
#include "worldfactory.h"
#include "effect.h"
#include "effect_on_condition.h"
#include "dialogue.h"
#include "editmap.h"
#include "coordinates.h"
#include "color.h"
#include "cata_scope_helpers.h"
#include "character.h"
#include "cursesdef.h"
#include "debug.h"
#include "json.h"
#include "json_loader.h"
#include "path_info.h"
#include "debug_menu.h"
#include "debug_console.h"
#include "game.h"
#include "game_constants.h"
#include "game_inventory.h"
#include "map_selector.h"
#include "field_type.h"
#include "field.h"
#include "imgui/imgui.h"
#include "inventory_ui.h"
#include "input_popup.h"
#include "input_context.h"
#include "sdl_renderer_recovery.h"
#include "item.h"
#include "popup.h"
#include "player_helpers.h"
#include "physical_navigation.h"
#include "semantic_surface.h"
#include "sounds.h"
#include "raid_decision_trace.h"
#include "raid_sweep.h"
#include "ranged.h"
#include "string_input_popup.h"
#include "type_id.h"
#include "ui_iteminfo.h"
#include "uilist.h"
#include "ui_manager.h"
#include "uistate.h"
#include "veh_interact.h"
#include "veh_type.h"
#include "vehicle.h"

namespace
{
void set_test_environment( const char *name, const char *value, int overwrite = 1 )
{
#if defined(_WIN32)
    // The caller's POSIX overwrite=1 maps to an unconditional assignment here.
    ( void )overwrite;
    _putenv_s( name, value );
#else
    setenv( name, value, overwrite );
#endif


}

void unset_test_environment( const char *name )
{
#if defined(_WIN32)
    _putenv_s( name, "" );
#else
    unsetenv( name );
#endif
}

void assign_test_environment( const std::string &name, const std::optional<std::string> &value )
{
    if( value ) {
        set_test_environment( name.c_str(), value->c_str() );
    } else {
        unset_test_environment( name.c_str() );
    }
}
} // namespace

TEST_CASE( "rider lamp fixture control admits only declared rider fixtures", "[semantic][fixture_lamp]" )
{
    CHECK( harness_rider_light_fixture( "zombie_rider.live_light_memory_continuity_mcw" ) );
    CHECK( harness_rider_light_fixture( "zombie_rider.live_light_memory_continuity_isolated_mcw" ) );
    CHECK_FALSE( harness_rider_light_fixture( nullptr ) );
    CHECK_FALSE( harness_rider_light_fixture( "" ) );
    CHECK_FALSE( harness_rider_light_fixture( "zombie_rider.live_light_memory_continuity_mcw.other" ) );
    CHECK_FALSE( harness_rider_light_fixture( "writhing_stalker.live_light_continuity_mcw" ) );
}

TEST_CASE( "semantic test environment assignment keeps native empty-value behavior and restores on unwind",
           "[semantic_surface][environment_portability_067]" )
{
    constexpr const char *name = "OPENCLAW_HARNESS_TEST_ENVIRONMENT_PORTABILITY_067";
    const char *initial_value = std::getenv( name );
    const std::optional<std::string> initial = initial_value ?
            std::optional<std::string>( initial_value ) : std::nullopt;
    on_out_of_scope restore_initial( [&]() {
        assign_test_environment( name, initial );
    } );

    const std::optional<std::string> prior = GENERATE( std::optional<std::string> {},
            std::optional<std::string>( "prior-value" ) );
    assign_test_environment( name, prior );
    try {
        {
            on_out_of_scope restore_prior( [&]() {
                assign_test_environment( name, prior );
            } );
            assign_test_environment( name, std::string( "outer-value" ) );
            try {
                on_out_of_scope restore_outer( [&]() {
                    assign_test_environment( name, std::string( "outer-value" ) );
                } );
                assign_test_environment( name, std::string( "inner-value" ) );
                throw std::runtime_error( "exercise nested environment restoration during unwind" );
            } catch( const std::runtime_error & ) {
            }
            REQUIRE( std::getenv( name ) != nullptr );
            CHECK( std::string( std::getenv( name ) ) == "outer-value" );
            throw std::runtime_error( "exercise prior environment restoration during unwind" );
        }
    } catch( const std::runtime_error & ) {
        // The prior-value scope has unwound and restored its saved state.
    }
    if( prior ) {
        REQUIRE( std::getenv( name ) != nullptr );
        CHECK( std::string( std::getenv( name ) ) == *prior );
    } else {
        CHECK( std::getenv( name ) == nullptr );
    }

    assign_test_environment( name, std::string() );
#if defined(_WIN32)
    CHECK( std::getenv( name ) == nullptr );
#else
    REQUIRE( std::getenv( name ) != nullptr );
    CHECK( std::string( std::getenv( name ) ).empty() );
#endif
    assign_test_environment( name, std::nullopt );
    CHECK( std::getenv( name ) == nullptr );
}

TEST_CASE( "redraw debug errors interrupt input without completing its pending action",
           "[semantic_surface][debug_error_prompt_067]" )
{
    const bool enclosing_session = GENERATE( false, true );
    const bool ignore_warning = GENERATE( false, true );
    const std::vector<std::string> keys = { "OPENCLAW_HARNESS_RUN_ID", "OPENCLAW_HARNESS_SEMANTIC_RUN_ID",
                                            "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH"
                                          };
    std::map<std::string, std::optional<std::string>> previous;
    for( const std::string &key : keys ) {
        const char *value = std::getenv( key.c_str() );
        previous[key] = value ? std::optional<std::string>( value ) : std::nullopt;
    }
    on_out_of_scope restore_environment( [&]() {
        for( const auto &entry : previous ) {
            if( entry.second ) {
                set_test_environment( entry.first.c_str(), entry.second->c_str(), 1 );
            } else {
                unset_test_environment( entry.first.c_str() );
            }
        }
    } );
    const std::string run = std::string( enclosing_session ? "redraw-enclosed-" :
                                         "redraw-between-owners-" ) +
                            std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() );
    const std::string warning_file = run + ".cpp";
    const std::string chosen_action = ignore_warning ? "prompt.ignore" : "prompt.acknowledge";
    set_test_environment( keys[0].c_str(), run.c_str(), 1 );
    set_test_environment( keys[1].c_str(), run.c_str(), 1 );
    unset_test_environment( keys[2].c_str() );
#if defined(TILES)
    software_render_fixture renderer;
    REQUIRE( renderer.available() );
    renderer_recovery_test_support::setup_software_ui();
#else
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_renderer( [&]() {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
#endif
    semantic_surface_manager local_manager( run );
    semantic_surface_manager &manager = enclosing_session ? local_manager :
                                        openclaw_harness_semantic_surface_manager();
    on_out_of_scope clear_observers( [&]() {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
    } );
    std::optional<semantic_surface_manager_session> session;
    if( enclosing_session ) {
        session.emplace( manager );
    }
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    int original_dispatches = 0;
    {
        semantic_surface_scope world( manager, "world", "World", {},
        { { "world.move.north", "", "North", true } },
        [&]( const semantic_action_request & request ) {
            ++original_dispatches;
            REQUIRE( manager.withhold_parent_authority_until_recreated( request.surface_id ) );
            return semantic_action_dispatch_result{ true, "", "" };
        } );
        const auto owner = *manager.top();
        REQUIRE( manager.submit_request( { manager.run_id(), owner.surface_id, owner.frame_id,
                                           "north", "world.move.north", std::nullopt, {} } ) );
        REQUIRE( world.consume_request() );
    }
    REQUIRE_FALSE( manager.top() );
    REQUIRE( receipts.empty() );
    std::vector<semantic_surface_descriptor> prompts;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & owner ) {
        if( owner.payload.count( "native_owner" ) == 0 ||
            owner.payload.at( "native_owner" ) != "DEBUG_MSG" ) {
            return;
        }
        prompts.push_back( owner );
        CHECK( active_semantic_surface_manager() == &manager );
        CHECK( std::none_of( receipts.begin(), receipts.end(), []( const auto & receipt ) {
            return receipt.request_id == "north";
        } ) );
        CHECK( owner.payload.at( "text" ).find( "controlled redraw error" ) != std::string::npos );
        REQUIRE( owner.valid_actions.size() == 2 );
        CHECK( owner.valid_actions[0].id == "prompt.acknowledge" );
        CHECK( owner.valid_actions[1].id == "prompt.ignore" );
        const auto suffix = std::to_string( prompts.size() );
        REQUIRE( manager.submit_request( { "foreign-run", owner.surface_id, owner.frame_id,
                                           "foreign-continue" + suffix, chosen_action, std::nullopt, {} } ) );
        REQUIRE( manager.submit_request( { manager.run_id(), owner.surface_id, owner.frame_id,
                                           "parameter-continue" + suffix, chosen_action, std::nullopt,
        { { "ignore_future_errors", "true" } } } ) );
        REQUIRE( manager.submit_request( { manager.run_id(), owner.surface_id, "stale-frame",
                                           "stale-continue" + suffix, chosen_action, std::nullopt, {} } ) );
        REQUIRE( manager.submit_request( { manager.run_id(), owner.surface_id, owner.frame_id,
                                           "continue" + suffix, chosen_action, std::nullopt, {} } ) );
        // Once a real semantic owner exists, exercise its native redraw too.
        test_mode = false;
    } );

    // The public buffer producer lets the actual redraw-safe prompt run under
    // the test input guard: old raw-only code fails instead of waiting on keys.
    {
        restore_on_out_of_scope<catacurses::window> restore_screen( catacurses::stdscr );
        restore_on_out_of_scope<bool> restore_mode( test_mode );
        catacurses::stdscr = catacurses::window();
        test_mode = false;
        for( int repeat = 0; repeat < 2; ++repeat ) {
            CHECK( capture_debugmsg_during( [&]() {
                realDebugmsg( warning_file.c_str(), "1", "redraw_control", "controlled redraw error" );
            } ) == "controlled redraw error" );
        }
        CHECK( capture_debugmsg_during( [&]() {
            realDebugmsg( warning_file.c_str(), "2", "redraw_control", "controlled redraw error" );
        } ) == "controlled redraw error" );
    }
    bool invoked = false;
    ui_adaptor drawing;
    drawing.position_from_window( catacurses::stdscr );
    drawing.on_redraw( [&]( const ui_adaptor & ) {
        if( !invoked ) {
            invoked = true;
            restore_on_out_of_scope<bool> input_guard( test_mode );
            test_mode = true;
            replay_buffered_debugmsg_prompts();
        }
    } );
    restore_on_out_of_scope<bool> restore_mode( test_mode );
    test_mode = false;
    REQUIRE_NOTHROW( ui_manager::redraw() );
    REQUIRE( invoked );
    // Continue never grants future Ignore. Explicit Ignore suppresses only
    // the repeated source line; the other source line still needs input.
    const std::size_t expected_prompts = ignore_warning ? 2 : 3;
    REQUIRE( prompts.size() == expected_prompts );
    CHECK( prompts.front().payload.at( "source_line" ) == "1" );
    CHECK( prompts.back().payload.at( "source_line" ) == "2" );
    REQUIRE( receipts.size() == expected_prompts * 4 );
    for( std::size_t offset = 0; offset < receipts.size(); offset += 4 ) {
        CHECK_FALSE( receipts[offset].accepted );
        CHECK( receipts[offset].rejection_reason == "wrong_run" );
        CHECK_FALSE( receipts[offset + 1].accepted );
        CHECK( receipts[offset + 1].rejection_reason == "unadvertised_action" );
        CHECK_FALSE( receipts[offset + 2].accepted );
        CHECK( receipts[offset + 2].rejection_reason == "stale_frame" );
        CHECK( receipts[offset + 3].accepted );
        CHECK( receipts[offset + 3].outcome == ( ignore_warning ? "debug_error_ignored" :
                "debug_error_continued" ) );
        CHECK( receipts[offset + 3].resulting_frame_id.empty() );
    }
    REQUIRE_FALSE( manager.top() );
    CHECK( active_semantic_surface_manager() == ( enclosing_session ? &manager : nullptr ) );
    {
        semantic_surface_scope successor( manager, "world", "World after redraw" );
        REQUIRE( receipts.size() == expected_prompts * 4 + 1 );
        CHECK( receipts.back().request_id == "north" );
        CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
        CHECK( receipts.back().resulting_frame_id != prompts[0].frame_id );
        CHECK( original_dispatches == 1 );
        const auto original_receipt = manager.reject_request( { manager.run_id(), "", "",
                                      "north", "world.move.north", std::nullopt, {} } );
        CHECK( original_receipt.resulting_frame_id == receipts.back().resulting_frame_id );
        REQUIRE( manager.submit_request( { manager.run_id(), prompts.back().surface_id,
                                           prompts.back().frame_id, "retired-prompt", "prompt.acknowledge", std::nullopt, {} } ) );
        CHECK_FALSE( successor.consume_request() );
        CHECK( receipts.back().rejection_reason == "wrong_surface" );
        CHECK( original_dispatches == 1 );
    }
}

TEST_CASE( "diagnostic scopes preserve pending child ownership across nested owners",
           "[semantic_surface][debug_error_prompt_067]" )
{
    const bool withheld = GENERATE( false, true );
    semantic_surface_manager manager( "interrupted-child" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const auto & receipt ) {
        receipts.push_back( receipt );
    } );
    int parent_dispatches = 0;
    int acknowledgements = 0;
    semantic_surface_scope parent( manager, "inventory", "Inventory", {},
    { { "inventory.select", "item:1", "Select", true } },
    [&]( const semantic_action_request & request ) {
        ++parent_dispatches;
        if( withheld ) {
            REQUIRE( manager.withhold_parent_authority_until_recreated( request.surface_id ) );
        }
        return semantic_action_dispatch_result{ true, "", "", true };
    } );
    const auto parent_owner = *manager.top();
    REQUIRE( manager.submit_request( { manager.run_id(), parent_owner.surface_id, parent_owner.frame_id,
                                       "select", "inventory.select", "item:1", {} } ) );
    REQUIRE( parent.consume_request() );
    REQUIRE( receipts.empty() );
    {
        semantic_surface_scope diagnostic( manager, "prompt", "Debug error", {},
        { { "prompt.acknowledge", "", "Continue", true } },
        [&]( const semantic_action_request & ) {
            ++acknowledgements;
            return semantic_action_dispatch_result{ true, "", "", false, false };
        }, true );
        REQUIRE( receipts.empty() );
        const auto diagnostic_owner = *manager.top();
        {
            semantic_surface_scope child( manager, "menu", "Nested owner" );
            const auto child_owner = *manager.top();
            CHECK_FALSE( diagnostic.close() );
            REQUIRE( manager.top() );
            CHECK( manager.top()->surface_id == child_owner.surface_id );
            CHECK( manager.top()->frame_id == child_owner.frame_id );
            REQUIRE( manager.submit_request( { manager.run_id(), diagnostic_owner.surface_id,
                                               diagnostic_owner.frame_id, "parent-while-child", "prompt.acknowledge", std::nullopt, {} } ) );
            CHECK_FALSE( diagnostic.consume_request() );
            CHECK_FALSE( child.consume_request() );
            CHECK( receipts.back().rejection_reason == "wrong_surface" );
        }
        const auto fresh_diagnostic = *manager.top();
        REQUIRE( manager.submit_request( { manager.run_id(), fresh_diagnostic.surface_id,
                                           fresh_diagnostic.frame_id, "ack", "prompt.acknowledge", std::nullopt, {} } ) );
        REQUIRE( diagnostic.consume_request() );
        CHECK( acknowledgements == 1 );
        CHECK( receipts.size() == 2 );
        // Move ownership once; the moved-from scope must not restore twice.
        semantic_surface_scope moved( std::move( diagnostic ) );
        REQUIRE( moved.close() );
    }
    CHECK( receipts.size() == 2 );
    if( withheld ) {
        REQUIRE_FALSE( manager.top() );
    } else {
        REQUIRE( manager.top() );
        CHECK( manager.top()->frame_id == parent_owner.frame_id );
        CHECK( parent.publish( { { "redraw", "true" } },
        { { "inventory.select", "item:1", "Select", true } } ) );
        CHECK( receipts.size() == 2 );
    }
    {
        semantic_surface_scope real_successor( manager, "menu", "Actual selection child" );
        REQUIRE( receipts.size() == 3 );
        CHECK( receipts.back().request_id == "select" );
        CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
        CHECK( parent_dispatches == 1 );
    }
}

TEST_CASE( "activity input survives polling but not a different owner",
           "[semantic_surface][activity_poll]" )
{
    semantic_surface_manager manager( "activity-run" );
    int pauses = 0;
    const auto consumer = [&]( const semantic_action_request & ) {
        ++pauses;
        return semantic_action_dispatch_result{ true, "", "", false, false };
    };
    const auto poll = [&]( const std::string & generation ) {
        return manager.push( "activity_wait", "Activity in progress",
        { { "activity_generation", generation } },
        { { "activity.pause", "", "Pause activity", true } }, consumer );
    };
    std::string surface = poll( "1" );
    const semantic_surface_descriptor observed = *manager.top();
    REQUIRE( manager.pop( surface ) );
    surface = poll( "1" );
    CHECK( manager.top()->frame_id == observed.frame_id );
    REQUIRE( manager.submit_request( { observed.run_id, observed.surface_id, observed.frame_id,
                                       "pause-1", "activity.pause", std::nullopt, {} } ) );
    CHECK( manager.consume_top_request() );
    CHECK( pauses == 1 );
    REQUIRE( manager.pop( surface ) );

    SECTION( "a replacement activity rejects the previous grant" ) {
        poll( "2" );
    }
    SECTION( "a prompt retires the grant even if the activity resumes" ) {
        const std::string prompt = manager.push( "yes_no", "Stop?" );
        REQUIRE( manager.pop( prompt ) );
        poll( "1" );
    }
    SECTION( "a withheld activity unwinds after its cancellation prompt" ) {
        const std::string activity = poll( "1" );
        REQUIRE( manager.withhold_parent_authority_until_recreated( activity ) );
        const std::string prompt = manager.push( "yes_no", "Stop?" );
        REQUIRE( manager.pop( prompt ) );
        REQUIRE_FALSE( manager.top() );
        REQUIRE( manager.pop( activity ) );
        poll( "1" );
    }
    CHECK( manager.top()->frame_id != observed.frame_id );
    REQUIRE( manager.submit_request( { observed.run_id, observed.surface_id, observed.frame_id,
                                       "stale-pause", "activity.pause", std::nullopt, {} } ) );
    CHECK_FALSE( manager.consume_top_request() );
    CHECK( pauses == 1 );
}

TEST_CASE( "failed diagnostic construction restores the pending action and native stack",
           "[semantic_surface][diagnostic_constructor_exception_067]" )
{
    semantic_surface_manager manager( "failed-diagnostic" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const auto & receipt ) {
        receipts.push_back( receipt );
    } );
    int dispatches = 0;
    {
        semantic_surface_scope world( manager, "world", "World", {},
        { { "world.move.north", "", "North", true } },
        [&]( const semantic_action_request & request ) {
            ++dispatches;
            REQUIRE( manager.withhold_parent_authority_until_recreated( request.surface_id ) );
            return semantic_action_dispatch_result{ true, "", "" };
        } );
        const auto owner = *manager.top();
        REQUIRE( manager.submit_request( { manager.run_id(), owner.surface_id, owner.frame_id,
                                           "north", "world.move.north", std::nullopt, {} } ) );
        REQUIRE( world.consume_request() );
    }
    manager.set_descriptor_observer( []( const auto & ) {
        throw std::runtime_error( "descriptor publication failed" );
    } );
    CHECK_THROWS_WITH( semantic_surface_scope( manager, "prompt", "Debug error", {}, {}, {}, true ),
                       "descriptor publication failed" );
    CHECK( manager.stack().empty() );
    CHECK_FALSE( manager.top() );
    CHECK( receipts.empty() );
    manager.set_descriptor_observer( {} );
    {
        semantic_surface_scope successor( manager, "world", "Actual successor" );
        CHECK( receipts.size() == 1 );
        if( !receipts.empty() ) {
            CHECK( receipts.back().request_id == "north" );
            CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
        }
        CHECK( dispatches == 1 );
    }
    // Also clean the original implementation's partially pushed orphan after
    // recording the failed assertions, so the negative control stays bounded.
    while( !manager.stack().empty() ) {
        const auto owners = manager.stack();
        REQUIRE( manager.pop( owners.back().surface_id ) );
    }
}

TEST_CASE( "diagnostic unwinding never publishes or throws from its destructor",
           "[semantic_surface][diagnostic_destructor_exception_067]" )
{
    const bool unwinding = GENERATE( false, true );
    semantic_surface_manager manager( "diagnostic-unwind" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const auto & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope parent( manager, "world", "World", {},
    { { "world.move.north", "", "North", true } }, []( const auto & ) {
        return semantic_action_dispatch_result{ true, "", "" };
    } );
    const auto owner = *manager.top();
    REQUIRE( manager.submit_request( { manager.run_id(), owner.surface_id, owner.frame_id,
                                       "north", "world.move.north", std::nullopt, {} } ) );
    REQUIRE( parent.consume_request() );
    // The broken noexcept destructor would terminate the negative runner.
    // Keep that a truthful failed exit without producing a core/crash archive.
    const auto previous_terminate = std::set_terminate( []() {
        std::fputs( "FAILED: diagnostic destructor propagated observer exception\n", stderr );
        std::fflush( stderr );
        std::_Exit( 86 );
    } );
    on_out_of_scope restore_terminate( [&]() {
        std::set_terminate( previous_terminate );
    } );
    const auto native_body = [&]() {
        semantic_surface_scope diagnostic( manager, "prompt", "Debug error", {}, {}, {}, true );
        manager.set_descriptor_observer( []( const auto & ) {
            throw std::runtime_error( "parent publication failed" );
        } );
        // The caller is unwinding its native input/render operation. Cleanup
        // must not run another potentially throwing external observer.
        if( unwinding ) {
            throw std::runtime_error( "native input interrupted" );
        }
    };
    if( unwinding ) {
        CHECK_THROWS_WITH( native_body(), "native input interrupted" );
    } else {
        native_body();
    }
    manager.set_descriptor_observer( {} );
    CHECK( manager.stack().size() == 1 );
    CHECK_FALSE( manager.top() );
    CHECK( receipts.empty() );
    {
        semantic_surface_scope successor( manager, "world", "Actual successor" );
        REQUIRE( receipts.size() == 1 );
        CHECK( receipts.back().request_id == "north" );
        CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
    }
}

TEST_CASE( "failed diagnostic close restores custody before the native owner retries publication",
           "[semantic_surface][diagnostic_close_exception_067]" )
{
    semantic_surface_manager manager( "failed-close" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const auto & receipt ) {
        receipts.push_back( receipt );
    } );
    int dispatches = 0;
    semantic_surface_scope parent( manager, "inventory", "Inventory", {},
    { { "inventory.select", "item:1", "Select", true } }, [&]( const auto & ) {
        ++dispatches;
        return semantic_action_dispatch_result{ true, "", "", dispatches == 1 };
    } );
    const auto owner = *manager.top();
    REQUIRE( manager.submit_request( { manager.run_id(), owner.surface_id, owner.frame_id,
                                       "select", "inventory.select", "item:1", {} } ) );
    REQUIRE( parent.consume_request() );
    std::optional<semantic_surface_descriptor> failed_owner;
    manager.set_descriptor_observer( [&]( const auto & descriptor ) {
        failed_owner = descriptor;
        throw std::runtime_error( "construction observer failed" );
    } );
    CHECK_THROWS_WITH( semantic_surface_scope( manager, "prompt", "Debug error", {}, {}, {}, true ),
                       "construction observer failed" );
    REQUIRE( failed_owner );
    REQUIRE( manager.stack().size() == 1 );
    CHECK( manager.stack().front().surface_id == owner.surface_id );
    REQUIRE_FALSE( manager.top() );
    REQUIRE( manager.submit_request( { manager.run_id(), failed_owner->surface_id, failed_owner->frame_id,
                                       "failed-prompt", "prompt.acknowledge", std::nullopt, {} } ) );
    CHECK_FALSE( parent.consume_request() );
    CHECK( receipts.empty() );
    manager.set_descriptor_observer( {} );
    REQUIRE( parent.publish( {}, owner.valid_actions ) );
    CHECK_FALSE( parent.consume_request() );
    CHECK( receipts.back().rejection_reason == "wrong_surface" );
    CHECK( dispatches == 1 );
    {
        semantic_surface_scope child( manager, "menu", "Actual first selection child" );
        REQUIRE( receipts.back().request_id == "select" );
        CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
    }
    // Complete the awaited child before exercising an ordinary in-place
    // action. Await-child suppression must not be waived just to throw.
    const std::map<std::string, std::string> current_facts = { { "stage", "after-child" } };
    REQUIRE( parent.publish( current_facts, owner.valid_actions ) );
    const auto ready_parent = *manager.top();
    REQUIRE( manager.submit_request( { manager.run_id(), ready_parent.surface_id, ready_parent.frame_id,
                                       "second-select", "inventory.select", "item:1", {} } ) );
    REQUIRE( parent.consume_request() );
    CHECK( dispatches == 2 );
    {
        semantic_surface_scope diagnostic( manager, "prompt", "Debug error", {},
        { { "prompt.acknowledge", "", "Continue", true } }, []( const auto & ) {
            return semantic_action_dispatch_result{ true, "", "", false, false };
        }, true );
        const auto prompt = *manager.top();
        REQUIRE( manager.submit_request( { manager.run_id(), prompt.surface_id, prompt.frame_id,
                                           "continue", "prompt.acknowledge", std::nullopt, {} } ) );
        REQUIRE( diagnostic.consume_request() );
        manager.set_descriptor_observer( [&]( const auto & descriptor ) {
            failed_owner = descriptor;
            throw std::runtime_error( "parent observer failed" );
        } );
        CHECK_THROWS_WITH( diagnostic.close(), "parent observer failed" );
        REQUIRE( manager.stack().size() == 1 );
        CHECK( manager.stack().front().surface_id == owner.surface_id );
        REQUIRE_FALSE( manager.top() );
        CHECK( diagnostic.close() );
    }
    REQUIRE( failed_owner );
    REQUIRE( manager.submit_request( { manager.run_id(), failed_owner->surface_id, failed_owner->frame_id,
                                       "failed-parent", "inventory.select", "item:1", {} } ) );
    CHECK_FALSE( parent.consume_request() );
    manager.set_descriptor_observer( {} );
    // Even identical native facts need a fresh descriptor after failed output.
    REQUIRE( parent.publish( current_facts, owner.valid_actions ) );
    REQUIRE( manager.top() );
    CHECK( manager.top()->frame_id != failed_owner->frame_id );
    CHECK_FALSE( parent.consume_request() );
    CHECK( receipts.back().rejection_reason == "stale_frame" );
    CHECK( dispatches == 2 );
    for( const auto &request_id : {
             "select", "second-select"
         } ) {
        CHECK( std::count_if( receipts.begin(), receipts.end(), [&]( const auto & receipt ) {
            return receipt.request_id == request_id;
        } ) == 1 );
    }
    const auto completed = manager.reject_request( { manager.run_id(), ready_parent.surface_id,
                           ready_parent.frame_id, "second-select", "inventory.select", "item:1", {} } );
    CHECK( completed.resulting_frame_id == manager.top()->frame_id );
    CHECK( dispatches == 2 );
}

namespace
{

class scoped_imgui_context
{
    public:
        scoped_imgui_context() : owns_context( ImGui::GetCurrentContext() == nullptr ) {
            if( owns_context ) {
                ImGui::CreateContext();
                ImGuiIO &io = ImGui::GetIO();
                io.DisplaySize = ImVec2( 800.0f, 600.0f );
                io.DeltaTime = 1.0f / 60.0f;
                io.Fonts->AddFontDefault();
                io.Fonts->Build();
                ImGui::NewFrame();
            }
        }

        ~scoped_imgui_context() {
            if( owns_context ) {
                ImGui::EndFrame();
                ImGui::DestroyContext();
            }
        }

    private:
        bool owns_context;
};

class test_inventory_selector : public inventory_selector
{
    public:
        using inventory_selector::handle_semantic_request;
        using inventory_selector::semantic_actions;

        explicit test_inventory_selector( Character &who ) : inventory_selector( who ) {}
};

} // namespace

TEST_CASE( "DebugConsole advertises native close and restores its input parent once",
           "[semantic_surface][debug_console_close_067]" )
{
    scoped_imgui_context imgui;
    // A semantic close is an explicit owner command, including when the
    // console's editor currently owns ordinary keyboard text input.
    restore_on_out_of_scope<bool> typing( ImGui::GetIO().WantTextInput );
    ImGui::GetIO().WantTextInput = true;
    semantic_surface_manager manager( "console-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    const auto before = *manager.top();
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    bool opened = false;
    semantic_action_request close;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind == "world" || opened ) {
            return;
        }
        opened = true;
        REQUIRE( descriptor.payload.at( "native_owner" ) == "DEBUG_CONSOLE" );
        REQUIRE( descriptor.valid_actions.size() == 1 );
        REQUIRE( descriptor.valid_actions.front().id == "debug_console.close" );
        close = { descriptor.run_id, descriptor.surface_id, descriptor.frame_id,
                  "close-console", "debug_console.close", std::nullopt, {}
                };
        auto wrong = close;
        wrong.request_id = "wrong-console";
        wrong.surface_id = before.surface_id;
        REQUIRE( manager.submit_request( wrong ) );
        CHECK_FALSE( manager.consume_top_request() );
        CHECK( manager.top()->surface_id == descriptor.surface_id );
        {
            semantic_surface_scope child( manager, "string_prompt", "Console editor" );
            auto parent_request = close;
            parent_request.request_id = "parent-while-editor-open";
            REQUIRE( manager.submit_request( parent_request ) );
            CHECK_FALSE( manager.consume_top_request() );
            CHECK( manager.top()->kind == "string_prompt" );
        }
        auto stale = close;
        stale.request_id = "old-console-frame";
        REQUIRE( manager.submit_request( stale ) );
        CHECK_FALSE( manager.consume_top_request() );
        close.frame_id = manager.top()->frame_id;
        REQUIRE( manager.submit_request( close ) );
    } );
    debug_menu::open_console();
    REQUIRE( opened );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == before.surface_id );
    CHECK( manager.top()->frame_id != before.frame_id );
    REQUIRE( receipts.size() == 4 );
    CHECK_FALSE( receipts.front().accepted );
    CHECK( receipts.front().rejection_reason == "wrong_surface" );
    CHECK( receipts[1].rejection_reason == "wrong_surface" );
    CHECK( receipts[2].rejection_reason == "stale_frame" );
    CHECK( receipts.back().accepted );
    CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
    CHECK_FALSE( manager.submit_request( close ) );
    CHECK_FALSE( manager.consume_top_request() );
    CHECK( manager.top()->kind == "world" );
    CHECK( receipts.size() == 5 );
    CHECK( receipts.back().accepted ); // Immutable receipt replay, no new close.
    CHECK( receipts.back().resulting_frame_id == receipts[3].resulting_frame_id );
}

TEST_CASE( "native compatibility observations do not duplicate input-owner render maps",
           "[semantic_surface][lean_native_observation_067]" )
{
    namespace fs = std::filesystem;
    const fs::path directory = fs::temp_directory_path() /
                               ( "lean-native-observation-" + std::to_string(
                                     std::chrono::steady_clock::now().time_since_epoch().count() ) );
    fs::create_directories( directory );
    const fs::path trace = directory / "native.jsonl";
    const std::vector<std::string> keys = { "OPENCLAW_HARNESS_RUN_ID", "OPENCLAW_HARNESS_SEMANTIC_RUN_ID",
                                            "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH"
                                          };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &key : keys ) {
        const char *value = std::getenv( key.c_str() );
        previous[key] = value ? std::optional<std::string>( value ) : std::nullopt;
    }
    on_out_of_scope cleanup( [&] {
        for( const auto &entry : previous )
        {
            if( entry.second ) {
                set_test_environment( entry.first.c_str(), entry.second->c_str(), 1 );
            } else {
                unset_test_environment( entry.first.c_str() );
            }
        }
        fs::remove_all( directory );
    } );
    const auto run = directory.filename().string();
    set_test_environment( keys[0].c_str(), run.c_str(), 1 );
    set_test_environment( keys[1].c_str(), run.c_str(), 1 );
    set_test_environment( keys[2].c_str(), trace.string().c_str(), 1 );
    const auto context = get_default_mode_input_context();
    openclaw_harness_semantic_initial_world_frame_if_ready( &context, true, true, true );
    auto &manager = openclaw_harness_semantic_surface_manager();
    // Publish the actual native HUD maps once through the input-owning
    // descriptor. The callback is a controlled transaction, not gameplay.
    std::map<std::string, std::string> payload;
    {
        std::ifstream initial( trace );
        std::string row;
        const std::string prefix = "openclaw_harness_semantic_step: ";
        while( std::getline( initial, row ) ) {
            std::istringstream body( row.substr( prefix.size() ) );
            TextJsonIn jsin( body );
            const auto object = jsin.get_object();
            object.allow_omitted_members();
            if( object.get_string( "event" ) == "frame" ) {
                const auto observation = object.get_object( "observation" );
                observation.allow_omitted_members();
                payload["minimap"] = observation.get_object( "minimap" ).str();
                payload["overmap"] = observation.get_object( "overmap" ).str();
                payload["visible_local"] = observation.get_array( "visible_local" ).str();
                payload["messages"] = "[]";
            }
        }
    }
    REQUIRE( payload.size() == 4 );
    int actions = 0;
    semantic_surface_scope world( manager, "world", "World", payload,
    { { "world.pause", "", "Pause", true } }, [&]( const semantic_action_request & ) {
        ++actions;
        return semantic_action_dispatch_result{ true, "", "", false, false };
    } );
    const auto issued = *manager.top();
    REQUIRE( manager.submit_request( { issued.run_id, issued.surface_id,
                                       issued.frame_id, "pause", "world.pause", std::nullopt, {} } ) );
    REQUIRE( world.consume_request() );
    CHECK( actions == 1 );
    for( int index = 0; index < 3; ++index ) {
        openclaw_harness_semantic_activity_distraction();
        openclaw_harness_semantic_world_after_activity_distraction();
        openclaw_harness_semantic_wait_activity_complete();
    }
    std::ifstream input( trace );
    const std::string prefix = "openclaw_harness_semantic_step: ";
    std::string line;
    int startup = 0;
    int activity = 0;
    int receipts = 0;
    int descriptors = 0;
    std::size_t startup_bytes = 0;
    std::size_t activity_bytes = 0;
    while( std::getline( input, line ) ) {
        REQUIRE( line.compare( 0, prefix.size(), prefix ) == 0 );
        std::istringstream body( line.substr( prefix.size() ) );
        TextJsonIn jsin( body );
        const auto object = jsin.get_object();
        object.allow_omitted_members();
        if( object.get_string( "event" ) == "surface_receipt" ) {
            CHECK( object.get_bool( "accepted" ) );
            CHECK( object.get_string( "request_id" ) == "pause" );
            ++receipts;
        }
        if( object.get_string( "event" ) == "surface_descriptor" ) {
            const auto published = object.get_object( "payload" );
            published.allow_omitted_members();
            CHECK( published.get_string( "minimap" ) == payload.at( "minimap" ) );
            CHECK( published.get_string( "overmap" ) == payload.at( "overmap" ) );
            CHECK( object.get_string( "frame_id" ) == issued.frame_id );
            ++descriptors;
        }
        if( object.get_string( "event" ) != "frame" ) {
            continue;
        }
        const auto observation = object.get_object( "observation" );
        observation.allow_omitted_members();
        CHECK( observation.has_member( "visible_local" ) );
        CHECK( observation.has_member( "visible_entities" ) );
        CHECK( observation.has_member( "visible_zones" ) );
        if( object.get_string( "producer", "" ) == "hud_world_ready" ) {
            ++startup;
            startup_bytes += line.size() + 1;
            CHECK( observation.has_member( "minimap" ) );
            CHECK( observation.has_member( "overmap" ) );
        } else {
            ++activity;
            activity_bytes += line.size() + 1;
            CHECK_FALSE( observation.has_member( "minimap" ) );
            CHECK_FALSE( observation.has_member( "overmap" ) );
        }
    }
    CHECK( startup == 1 );
    CHECK( activity == 9 );
    CHECK( receipts == 1 );
    CHECK( descriptors == 1 );
    std::cout << "R067_NATIVE_OBSERVATION startup_bytes=" << startup_bytes
              << " activity_records=" << activity << " activity_bytes=" << activity_bytes
              << " persisted_bytes=" << fs::file_size( trace ) << std::endl;
}

TEST_CASE( "semantic surface scopes hide parent actions and republish after pop",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    semantic_surface_scope world( manager, "world", "World", {}, {
        { "world.wait", "", "Wait", true }
    } );
    REQUIRE( manager.top() );
    const std::string world_surface = manager.top()->surface_id;
    const std::string world_frame = manager.top()->frame_id;

    {
        semantic_surface_scope child( manager, "menu", "Inventory", {}, {
            { "menu.cancel", "", "Cancel", true }
        } );
        REQUIRE( manager.top() );
        CHECK( manager.top()->kind == "menu" );
        CHECK( manager.top()->breadcrumbs == std::vector<std::string> { "World", "Inventory" } );
        CHECK( manager.top()->valid_actions.front().id == "menu.cancel" );
        CHECK_FALSE( world.publish( {}, { { "world.wait", "", "Wait", true } } ) );
        CHECK( manager.stack().size() == 2 );
    }

    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == world_surface );
    CHECK( manager.top()->frame_id != world_frame );
    CHECK( manager.top()->valid_actions.front().id == "world.wait" );
}

TEST_CASE( "a native child popup does not republish an expired activity owner",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_surface_descriptor> descriptors;
    std::vector<semantic_action_receipt> receipts;
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor & descriptor ) {
        descriptors.push_back( descriptor );
    } );
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );

    std::optional<semantic_surface_scope> activity;
    activity.emplace( manager, "activity_distraction", "Activity distraction",
                      std::map<std::string, std::string> {},
    std::vector<semantic_action_descriptor> {
        { "activity.continue", "", "Continue", true }
    } );
    const std::string activity_surface = manager.top()->surface_id;

    // This mirrors the non-semantic branch of cancel_activity_or_ignore_query:
    // the native popup, not the parent activity surface, owns its input.  Its
    // retired parent stays private while the actionless native-return
    // successor supplies the deferred receipt.
    REQUIRE( manager.withhold_parent_authority_until_recreated( activity_surface ) );
    {
        semantic_surface_scope prompt( manager, "prompt", "Stop waiting?", {}, {
            { "prompt.choose", "prompt-option:1", "YES", true }
        }, []( const semantic_action_request & ) {
            return semantic_action_dispatch_result{ true };
        } );
        REQUIRE( manager.top() );
        CHECK( manager.top()->kind == "prompt" );
        const semantic_surface_descriptor prompt_frame = *manager.top();
        REQUIRE( manager.submit_request( { "test-run", prompt_frame.surface_id, prompt_frame.frame_id,
                                           "ignore", "prompt.choose", "prompt-option:1", {} } ) );
        REQUIRE( prompt.consume_request() );
    }

    CHECK_FALSE( manager.top() );
    CHECK( receipts.empty() );
    std::string return_frame;
    {
        semantic_surface_scope activity_return( manager, "activity_resumed", "Activity resumed" );
        REQUIRE( manager.top() );
        return_frame = manager.top()->frame_id;
        CHECK( manager.top()->valid_actions.empty() );
    }

    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().request_id == "ignore" );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().resulting_frame_id == return_frame );
    CHECK( std::none_of( descriptors.begin(), descriptors.end(),
    [&activity_surface]( const semantic_surface_descriptor & descriptor ) {
        return descriptor.surface_id == activity_surface &&
               descriptor.frame_id != "test-run:frame:1";
    } ) );
}

TEST_CASE( "activity pause receipts publish before the cancellation prompt unwinds",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );

    std::optional<semantic_surface_scope> activity;
    activity.emplace( manager, "activity_wait", "Activity in progress",
                      std::map<std::string, std::string> {},
    std::vector<semantic_action_descriptor> {
        { "activity.pause", "", "Pause activity", true }
    }, []( const semantic_action_request & request ) {
        if( request.action_id != "activity.pause" ) {
            return semantic_action_dispatch_result{ false, "unadvertised_action", "" };
        }
        // DEFAULTMODE already owns the native pause action.  The cancellation
        // query is a separate prompt owner, so receipt this action immediately.
        return semantic_action_dispatch_result{ true, "", "", false, false };
    } );
    const semantic_surface_descriptor frame = *manager.top();
    REQUIRE( manager.submit_request( { "test-run", frame.surface_id, frame.frame_id,
                                       "pause-activity", "activity.pause", std::nullopt, {} } ) );
    REQUIRE( activity->consume_request() );

    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().action_id == "activity.pause" );
    CHECK( receipts.front().resulting_frame_id.empty() );

    REQUIRE( manager.withhold_parent_authority_until_recreated( frame.surface_id ) );
    {
        semantic_surface_scope prompt( manager, "prompt", "Stop waiting?", {}, {
            { "prompt.choose", "prompt-option:1", "YES", true }
        } );
        CHECK( manager.top()->kind == "prompt" );
        CHECK( receipts.size() == 1 );
    }
    CHECK_FALSE( manager.top() );
    CHECK( receipts.size() == 1 );
}

TEST_CASE( "dialogue semantic receipts are emitted before a native modal",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World" );

    {
        semantic_surface_scope dialogue( manager, "dialogue", "NPC", {}, {
            { "dialogue.choose", "response-1", "Continue", true }
        }, []( const semantic_action_request & ) {
            return semantic_action_dispatch_result{ true, "", "", true, false };
        } );
        const semantic_surface_descriptor dialogue_frame = *manager.top();
        REQUIRE( manager.submit_request( { "test-run", dialogue_frame.surface_id,
                                           dialogue_frame.frame_id, "choose", "dialogue.choose",
                                           "response-1", {} } ) );
        REQUIRE( dialogue.consume_request() );
        REQUIRE( receipts.size() == 1 );
        CHECK( receipts.front().request_id == "choose" );
        CHECK( receipts.front().accepted );
        CHECK( receipts.front().consuming_frame_id == dialogue_frame.frame_id );
        CHECK( receipts.front().resulting_frame_id.empty() );
    }

    REQUIRE( manager.top() );
    CHECK( manager.top()->kind == "world" );
    CHECK( receipts.size() == 1 );

    semantic_surface_scope successor( manager, "dialogue", "NPC", {}, {
        { "dialogue.cancel", "", "Goodbye", true }
    } );
    CHECK( receipts.size() == 1 );
    CHECK( manager.top()->kind == "dialogue" );
}

TEST_CASE( "terminal main menu intent is receipted without a GUI successor",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    bool native_quit_selected = false;
    semantic_surface_scope menu( manager, "main_menu", "Main menu", {}, {
        { "main_menu.quit", "", "Quit", true }
    }, [&native_quit_selected]( const semantic_action_request & request ) {
        if( request.action_id != "main_menu.quit" || request.stable_id ) {
            return semantic_action_dispatch_result{ false, "unadvertised_action", "" };
        }
        native_quit_selected = true;
        return semantic_action_dispatch_result{ true, "", "", false, false };
    } );
    const semantic_surface_descriptor frame = *manager.top();

    REQUIRE( manager.submit_request( { "test-run", frame.surface_id, frame.frame_id,
                                       "quit", "main_menu.quit", std::nullopt, {} } ) );
    REQUIRE( menu.consume_request() );
    REQUIRE( native_quit_selected );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().request_id == "quit" );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().resulting_frame_id.empty() );

    REQUIRE( manager.submit_request( { "test-run", "other-surface", frame.frame_id,
                                       "wrong-owner", "main_menu.quit", std::nullopt, {} } ) );
    CHECK_FALSE( menu.consume_request() );
    REQUIRE( receipts.size() == 2 );
    CHECK( receipts.back().rejection_reason == "wrong_surface" );

    REQUIRE( manager.submit_request( { "test-run", frame.surface_id, "stale-frame",
                                       "stale", "main_menu.quit", std::nullopt, {} } ) );
    CHECK_FALSE( menu.consume_request() );
    REQUIRE( receipts.size() == 3 );
    CHECK( receipts.back().rejection_reason == "stale_frame" );
}

TEST_CASE( "a repeated overmap destination request receipts without an unchanged successor",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "repeat-terminal-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );

    {
        semantic_surface_scope overmap( manager, "overmap", "Overmap", {}, {
            { "overmap.choose_destination", "omt:1,1,0", "Choose destination", true }
        }, []( const semantic_action_request & ) {
            // The native route selection may preserve this exact descriptor.
            // Its receipt therefore cannot depend on a future republish.
            return semantic_action_dispatch_result{ true, "", "", false, false };
        } );
        const semantic_surface_descriptor first = *manager.top();
        REQUIRE( manager.submit_request( { manager.run_id(), first.surface_id, first.frame_id,
                                           "first", "overmap.choose_destination", "omt:1,1,0", {} } ) );
        REQUIRE( overmap.consume_request() );
        REQUIRE( receipts.size() == 1 );
        CHECK( receipts.front().request_id == "first" );
        CHECK( receipts.front().resulting_frame_id.empty() );

        const semantic_surface_descriptor second = *manager.top();
        REQUIRE( manager.submit_request( { manager.run_id(), second.surface_id, second.frame_id,
                                           "second", "overmap.choose_destination", "omt:1,1,0", {} } ) );
        REQUIRE( overmap.consume_request() );
        CHECK( receipts.size() == 2 );
    }

    REQUIRE( receipts.size() == 2 );
    CHECK( receipts.back().request_id == "second" );
    CHECK( receipts.back().accepted );
    CHECK( receipts.back().resulting_frame_id.empty() );
}

TEST_CASE( "a queued semantic request re-wakes its native owner after FIFO wake loss",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "queued-wake-run" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope overmap( manager, "overmap", "Overmap", {}, {
        { "overmap.choose_destination", "omt:1,1,0", "Choose destination", true }
    }, []( const semantic_action_request & ) {
        return semantic_action_dispatch_result{ true, "", "", false, false };
    } );
    const semantic_surface_descriptor frame = *manager.top();

    // This models the JSONL record already being queued after an inner
    // context consumed its FIFO byte.  No new transport bytes are available
    // on the next SDL poll, so pending work must still wake the true owner.
    REQUIRE( manager.submit_request( { manager.run_id(), frame.surface_id, frame.frame_id,
                                       "queued-after-wake-loss", "overmap.choose_destination",
                                       "omt:1,1,0", {} } ) );
    CHECK( poll_active_semantic_surface_request() );
    CHECK( take_active_semantic_surface_wake() );
    REQUIRE( overmap.consume_request() );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().request_id == "queued-after-wake-loss" );
    CHECK( receipts.front().accepted );
}

TEST_CASE( "semantic transport records JSONL offsets through a lost FIFO wake",
           "[semantic_surface][semantic_transport_bytes_067]" )
{
    const std::string mode = GENERATE( "crlf_successive", "crlf_batch", "lf_successive" );
    CAPTURE( mode );
    const bool batch = mode == "crlf_batch";
    const std::string newline = mode == "lf_successive" ? "\n" : "\r\n";
    const std::size_t first_end = 477 + newline.size();
    const std::size_t final_end = first_end + 503 + newline.size();
    const std::filesystem::path transport_path = std::filesystem::temp_directory_path() /
            ( "semantic-surface-transport-" + std::to_string(
                  std::chrono::steady_clock::now().time_since_epoch().count() ) + ".jsonl" );
    const char *const previous_path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    const bool had_previous_path = previous_path != nullptr;
    const std::string saved_path = had_previous_path ? previous_path : "";
    on_out_of_scope cleanup( [&] {
        if( had_previous_path )
        {
            set_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", saved_path.c_str(), 1 );
        } else
        {
            unset_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
        }
        std::filesystem::remove( transport_path );
    } );
    set_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", transport_path.string().c_str(),
                          1 );

    semantic_surface_manager manager( "transport-run" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_request_transport_event> transport_events;
    std::vector<semantic_action_receipt> receipts;
    std::vector<std::string> dispatched;
    manager.set_transport_observer( [&transport_events]( const semantic_request_transport_event &
    event ) {
        transport_events.push_back( event );
    } );
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope overmap( manager, "overmap", "Overmap", {}, {
        { "overmap.choose_destination", "omt:1,1,0", "Choose destination", true }
    }, [&dispatched]( const semantic_action_request & request ) {
        dispatched.push_back( request.request_id );
        return semantic_action_dispatch_result{ true, "", "", false, false };
    } );
    const semantic_surface_descriptor first = *manager.top();
    const auto append_record = [&]( const semantic_surface_descriptor & frame,
    const std::string & request_id, std::size_t body_size ) {
        std::string record = "{\"run_id\":\"transport-run\",\"surface_id\":\"" + frame.surface_id +
                             "\",\"frame_id\":\"" + frame.frame_id + "\",\"request_id\":\"" + request_id +
                             "\",\"action_id\":\"overmap.choose_destination\",\"stable_id\":\"omt:1,1,0\"}";
        REQUIRE( record.size() <= body_size );
        record.resize( body_size, ' ' );
        record += newline;
        std::ofstream stream( transport_path, std::ios::binary | std::ios::app );
        REQUIRE( stream );
        stream.write( record.data(), static_cast<std::streamsize>( record.size() ) );
        REQUIRE( stream );
    };
    append_record( first, "first", 477 );
    if( batch ) {
        append_record( first, "second", 503 );
    }
    REQUIRE( manager.poll_request_transport() );
    const auto first_record = std::find_if( transport_events.begin(), transport_events.end(),
    []( const semantic_request_transport_event & event ) {
        return event.event == "jsonl_record" && event.request_id == "first";
    } );
    REQUIRE( first_record != transport_events.end() );
    CHECK( first_record->queued );
    CHECK( first_record->offset_before == 0 );
    CHECK( first_record->offset_after == first_end );
    CHECK( first_record->transport_end == ( batch ? final_end : first_end ) );

    // A lost FIFO wake must not hide queued work.  This also polls the empty
    // tail: a CRLF text reader on Windows used to advance one byte here.
    REQUIRE( poll_active_semantic_surface_request() );
    REQUIRE( take_active_semantic_surface_wake() );
    REQUIRE( overmap.consume_request() );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().request_id == "first" );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().requested_run_id == first.run_id );
    CHECK( receipts.front().requested_surface_id == first.surface_id );
    CHECK( receipts.front().requested_frame_id == first.frame_id );
    CHECK( std::any_of( transport_events.begin(), transport_events.end(),
    []( const semantic_request_transport_event & event ) {
        return event.event == "wake_marked";
    } ) );
    CHECK( std::any_of( transport_events.begin(), transport_events.end(),
    []( const semantic_request_transport_event & event ) {
        return event.event == "request_consumed" && event.request_id == "first";
    } ) );

    semantic_surface_descriptor second = first;
    if( !batch ) {
        CHECK_FALSE( manager.poll_request_transport() );
        REQUIRE( overmap.publish( { { "route_destination", "1,1,0" } }, first.valid_actions ) );
        second = *manager.top();
        append_record( second, "second", 503 );
        REQUIRE( manager.poll_request_transport() );
    }
    const auto second_record = std::find_if( transport_events.begin(), transport_events.end(),
    []( const semantic_request_transport_event & event ) {
        return event.event == "jsonl_record" && event.request_id == "second";
    } );
    REQUIRE( second_record != transport_events.end() );
    CHECK( second_record->queued );
    CHECK( second_record->offset_before == ( batch ? 0 : first_end ) );
    CHECK( second_record->offset_after == final_end );
    CHECK( second_record->transport_end == final_end );
    CHECK( std::filesystem::file_size( transport_path ) == final_end );
    REQUIRE( poll_active_semantic_surface_request() );
    REQUIRE( take_active_semantic_surface_wake() );
    REQUIRE( overmap.consume_request() );
    REQUIRE( receipts.size() == 2 );
    CHECK( receipts.back().request_id == "second" );
    CHECK( receipts.back().accepted );
    CHECK( receipts.back().requested_run_id == second.run_id );
    CHECK( receipts.back().requested_surface_id == second.surface_id );
    CHECK( receipts.back().requested_frame_id == second.frame_id );
    CHECK( ( dispatched == std::vector<std::string> { "first", "second" } ) );
    CHECK_FALSE( manager.poll_request_transport() );
    CHECK_FALSE( poll_active_semantic_surface_request() );
    CHECK_FALSE( overmap.consume_request() );
    CHECK( receipts.size() == 2 );
    CHECK( dispatched.size() == 2 );
}

TEST_CASE( "item info is a semantic child and restores its selector parent",
           "[semantic_surface]" )
{
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope inventory( manager, "inventory", "Inventory", {}, {
        { "inventory.details", "rock-uid", "Details", true }
    }, []( const semantic_action_request & ) {
        return semantic_action_dispatch_result{ true, "", "", true };
    } );
    const semantic_surface_descriptor inventory_descriptor = *manager.top();
    REQUIRE( manager.submit_request( { "test-run", inventory_descriptor.surface_id,
                                       inventory_descriptor.frame_id, "details", "inventory.details",
                                       "rock-uid", {} } ) );
    REQUIRE( inventory.consume_request() );
    CHECK( receipts.empty() );

    item_info_data data( "rock", "rock", {
        iteminfo( "BASE", "Weight: ", "<num> kg", iteminfo::lower_is_better, 1 ),
        iteminfo( "DESCRIPTION", "A palm-sized stone." ),
        iteminfo( "DESCRIPTION", "Undisplayed description", iteminfo::no_name ),
        iteminfo( "ARMOR", "Protection;Bash\nTorso;2", iteminfo::is_table )
    }, {
        iteminfo( "BASE", "Weight: ", "<num> kg", iteminfo::lower_is_better, 2 ),
        iteminfo( "DESCRIPTION", "Comparison-only description" )
    } );
    std::string details_frame;
    manager.set_descriptor_observer( [&manager, &data, &details_frame](
    const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind != "item_info" ) {
            return;
        }
        REQUIRE( descriptor.breadcrumbs == std::vector<std::string> { "Inventory", "Item info" } );
        REQUIRE( descriptor.valid_actions.size() == 1 );
        CHECK( descriptor.valid_actions.front().id == "item_info.close" );
        REQUIRE( descriptor.payload.count( "item_info_text" ) == 1 );
        const std::string &text = descriptor.payload.at( "item_info_text" );
        CHECK( text == format_item_info( data.get_item_display(), data.get_item_compare() ) );
        CHECK( text.find( "Weight: " ) != std::string::npos );
        CHECK( text.find( "<color_c_light_green>1</color> kg" ) != std::string::npos );
        CHECK( text.find( "A palm-sized stone." ) != std::string::npos );
        CHECK( text.find( "Protection: Bash\n  Torso: 2" ) != std::string::npos );
        CHECK( text.find( "Undisplayed description" ) == std::string::npos );
        CHECK( text.find( "Comparison-only description" ) == std::string::npos );
        CHECK( descriptor.payload.at( "item_info_text_source" ) ==
               "format_item_info(item_info_data::get_item_display(), item_info_data::get_item_compare())" );
        REQUIRE( details_frame.empty() );
        details_frame = descriptor.frame_id;
        REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                           "close-details", "item_info.close", std::nullopt, {} } ) );
    } );
    iteminfo_window details( data, point::zero, 80, 0 );
    details.execute();

    REQUIRE( receipts.size() == 2 );
    CHECK( receipts.front().request_id == "details" );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().resulting_frame_id != inventory_descriptor.frame_id );
    CHECK( receipts.back().request_id == "close-details" );
    CHECK( receipts.back().accepted );
    // The redraw before consumption must retain the frame carrying these facts.
    CHECK( receipts.back().consuming_frame_id == details_frame );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == inventory.surface_id() );
    CHECK( manager.top()->frame_id != inventory_descriptor.frame_id );
}

TEST_CASE( "inventory selector resolves semantic item controls by live UID", "[semantic_surface]" )
{
    Character &avatar = get_player_character();
    clear_avatar();
    item_location selected = avatar.i_add( item( itype_id( "2x4" ) ), false );
    REQUIRE( selected );

    test_inventory_selector selector( avatar );
    selector.add_character_items( avatar );
    const std::string selected_id = std::to_string( selected->uid().get_value() );
    const auto actions = selector.semantic_actions( {
        { "inventory.commit", "", "Select", true },
        { "inventory.toggle", "", "Toggle selection", true }
    } );
    const auto select = std::find_if( actions.begin(), actions.end(), [&selected_id](
    const semantic_action_descriptor & action ) {
        return action.id == "inventory.select" && action.stable_id == selected_id;
    } );
    REQUIRE( select != actions.end() );
    CHECK( std::any_of( actions.begin(), actions.end(), [&selected_id](
    const semantic_action_descriptor & action ) {
        return action.id == "inventory.toggle" && action.stable_id == selected_id;
    } ) );

    std::optional<inventory_input> native_input;
    const semantic_action_dispatch_result selected_result = selector.handle_semantic_request( {
        "test-run", "inventory", "frame", "select", "inventory.select", selected_id, {}
    }, native_input );
    CHECK( selected_result.accepted );
    CHECK( selected_result.await_child_successor );
    REQUIRE( native_input );
    CHECK( native_input->action == "CONFIRM" );
    CHECK( native_input->semantic_target == selected );

    native_input.reset();
    const semantic_action_dispatch_result details_result = selector.handle_semantic_request( {
        "test-run", "inventory", "frame", "details", "inventory.details", selected_id, {}
    }, native_input );
    CHECK( details_result.accepted );
    CHECK( details_result.await_child_successor );
    REQUIRE( native_input );
    CHECK( native_input->action == "EXAMINE" );
    CHECK( native_input->semantic_target == selected );

    avatar.i_rem( &*selected );
    native_input.reset();
    const semantic_action_dispatch_result stale_result = selector.handle_semantic_request( {
        "test-run", "inventory", "frame", "stale", "inventory.select", selected_id, {}
    }, native_input );
    CHECK_FALSE( stale_result.accepted );
    CHECK( stale_result.rejection_reason == "invalid_stable_id" );
    CHECK_FALSE( native_input );
}

TEST_CASE( "inventory selector keeps UID controls bound across common actions",
           "[semantic_surface]" )
{
    Character &avatar = get_player_character();
    clear_avatar();
    item backpack( itype_id( "backpack" ) );
    item plank( itype_id( "2x4" ) );
    item rock( itype_id( "rock" ) );
    item another_rock( itype_id( "rock" ) );
    item bottle( itype_id( "bottle_plastic" ) );
    another_rock.set_var( "item_note", "second rock" );
    REQUIRE( backpack.put_in( plank, pocket_type::CONTAINER ).success() );
    REQUIRE( backpack.put_in( rock, pocket_type::CONTAINER ).success() );
    REQUIRE( backpack.put_in( another_rock, pocket_type::CONTAINER ).success() );
    REQUIRE( backpack.put_in( bottle, pocket_type::CONTAINER ).success() );
    avatar.wield( backpack );
    REQUIRE( avatar.get_wielded_item()->uid().is_valid() );
    const std::vector<item_location> live_items = avatar.all_items_loc();
    const auto live_rock = std::find_if( live_items.begin(),
    live_items.end(), []( const item_location & location ) {
        return location && location->typeId() == itype_id( "rock" );
    } );
    const auto second_rock = std::find_if( std::next( live_rock ),
    live_items.end(), []( const item_location & location ) {
        return location && location->typeId() == itype_id( "rock" );
    } );
    const auto live_bottle = std::find_if( live_items.begin(),
    live_items.end(), []( const item_location & location ) {
        return location && location->typeId() == itype_id( "bottle_plastic" );
    } );
    REQUIRE( live_rock != live_items.end() );
    REQUIRE( second_rock != live_items.end() );
    REQUIRE( live_bottle != live_items.end() );
    const std::string rock_id = std::to_string( ( *live_rock )->uid().get_value() );
    const std::string second_rock_id = std::to_string( ( *second_rock )->uid().get_value() );
    const std::string bottle_id = std::to_string( ( *live_bottle )->uid().get_value() );

    // These live entries have different native item types, so the selector
    // must not aggregate them before it advertises their UIDs.
    test_inventory_selector selector( avatar );
    selector.add_character_items( avatar );
    const auto actions = selector.semantic_actions( {
        { "inventory.toggle", "", "Toggle selection", true },
        { "inventory.wield", "", "Wield", true },
        { "inventory.wear", "", "Wear", true }
    } );
    const auto advertised_for = [&actions]( const std::string & action_id,
    const std::string & stable_id ) {
        return std::any_of( actions.begin(), actions.end(), [&action_id, &stable_id](
        const semantic_action_descriptor & action ) {
            return action.id == action_id && action.stable_id == stable_id && action.enabled;
        } );
    };
    CHECK( advertised_for( "inventory.select", rock_id ) );
    CHECK( advertised_for( "inventory.select", second_rock_id ) );
    CHECK( advertised_for( "inventory.details", rock_id ) );
    CHECK( advertised_for( "inventory.contents", bottle_id ) );
    CHECK( advertised_for( "inventory.toggle", rock_id ) );
    CHECK( advertised_for( "inventory.wield", rock_id ) );
    CHECK_FALSE( advertised_for( "inventory.wear", rock_id ) );

    std::optional<inventory_input> native_input;
    const auto details = selector.handle_semantic_request( {
        "test-run", "inventory", "frame", "details", "inventory.details", rock_id, {}
    }, native_input );
    CHECK( details.accepted );
    REQUIRE( native_input );
    CHECK( native_input->action == "EXAMINE" );

    native_input.reset();
    const auto contents = selector.handle_semantic_request( {
        "test-run", "inventory", "frame", "contents", "inventory.contents", bottle_id, {}
    }, native_input );
    CHECK( contents.accepted );
    REQUIRE( native_input );
    CHECK( native_input->action == "EXAMINE_CONTENTS" );

    const auto filtered = selector.handle_semantic_request( {
        "test-run", "inventory", "frame", "filter", "inventory.filter", std::nullopt,
        { { "text", "rock" } }
    }, native_input );
    CHECK( filtered.accepted );
    CHECK( selector.get_filter() == "rock" );
    const auto reset = selector.handle_semantic_request( {
        "test-run", "inventory", "frame", "reset", "inventory.reset_filter", std::nullopt, {}
    }, native_input );
    CHECK( reset.accepted );
    CHECK( selector.get_filter().empty() );

    native_input.reset();
    const auto cancel = selector.handle_semantic_request( {
        "test-run", "inventory", "frame", "cancel", "inventory.cancel", std::nullopt, {}
    }, native_input );
    CHECK( cancel.accepted );
    REQUIRE( native_input );
    CHECK( native_input->action == "QUIT" );

    native_input.reset();
    const auto second_rock_result = selector.handle_semantic_request( {
        "test-run", "inventory", "frame", "second-rock", "inventory.select", second_rock_id, {}
    }, native_input );
    CHECK( second_rock_result.accepted );
    REQUIRE( native_input );
    CHECK( native_input->semantic_target == *second_rock );

    const auto disabled_actions = selector.semantic_actions( {
        { "inventory.wield", "", "Wield", false }
    } );
    CHECK( std::any_of( disabled_actions.begin(), disabled_actions.end(), [&second_rock_id](
    const semantic_action_descriptor & action ) {
        return action.id == "inventory.wield" && action.stable_id == second_rock_id && !action.enabled;
    } ) );

    std::vector<item_location> item_locations = avatar.all_items_loc();
    auto rock_location = std::find_if( item_locations.begin(), item_locations.end(),
    [&rock_id]( const item_location & location ) {
        return location && std::to_string( location->uid().get_value() ) == rock_id;
    } );
    REQUIRE( rock_location != item_locations.end() );
    rock_location->remove_item();
    native_input.reset();
    const auto moved = selector.handle_semantic_request( {
        "test-run", "inventory", "frame", "moved", "inventory.select", rock_id, {}
    }, native_input );
    CHECK_FALSE( moved.accepted );
    CHECK( moved.rejection_reason == "invalid_stable_id" );
    CHECK_FALSE( native_input );
}

TEST_CASE( "unsupported semantic surface is actionless and rejects requests", "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    semantic_surface_scope world( manager, "world", "World", {}, {
        { "world.wait", "", "Wait", true }
    } );
    semantic_surface_scope unsupported( manager, "unsupported", "Legacy prompt", {
        { "stop_reason", "missing explicit semantic scope" }
    } );
    REQUIRE( manager.top() );
    CHECK( manager.top()->valid_actions.empty() );

    const semantic_action_receipt receipt = manager.reject_request( {
        "test-run", manager.top()->surface_id, manager.top()->frame_id,
        "request-1", "world.wait", std::nullopt, {}
    } );
    CHECK_FALSE( receipt.accepted );
    CHECK( receipt.rejection_reason == "no_native_binding" );
    CHECK( receipt.consuming_surface_id == unsupported.surface_id() );
}

TEST_CASE( "semantic surface manager sessions restore their enclosing owner",
           "[semantic_surface]" )
{
    semantic_surface_manager world_manager( "world-run" );
    semantic_surface_manager child_manager( "child-run" );
    CHECK( active_semantic_surface_manager() == nullptr );
    {
        semantic_surface_manager_session world_session( world_manager );
        CHECK( active_semantic_surface_manager() == &world_manager );
        {
            semantic_surface_manager_session child_session( child_manager );
            CHECK( active_semantic_surface_manager() == &child_manager );
        }
        CHECK( active_semantic_surface_manager() == &world_manager );
    }
    CHECK( active_semantic_surface_manager() == nullptr );
}

TEST_CASE( "semantic surface transport observes exact descriptors and rejections",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_surface_descriptor> descriptors;
    std::vector<semantic_action_receipt> receipts;
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor & descriptor ) {
        descriptors.push_back( descriptor );
    } );
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );

    semantic_surface_scope world( manager, "world", "World", {}, {
        { "world.wait", "", "Wait", true }
    } );
    REQUIRE( descriptors.size() == 1 );
    CHECK( descriptors.back().surface_id == world.surface_id() );
    CHECK( descriptors.back().frame_id == manager.top()->frame_id );

    const semantic_action_receipt receipt = manager.reject_request( {
        "test-run", world.surface_id(), manager.top()->frame_id,
        "request-1", "world.wait", std::nullopt, {}
    } );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.back().request_id == receipt.request_id );
    CHECK( receipts.back().consuming_frame_id == receipt.consuming_frame_id );
}

TEST_CASE( "actual map editor toggles publish and receipt without editing the map",
           "[semantic_surface][editmap_native_toggle]" )
{
    const std::string action = GENERATE( std::string( "editmap.toggle_advanced_info" ),
                                        std::string( "editmap.toggle_fast_scroll" ) );
    const std::string payload_key = action == "editmap.toggle_advanced_info" ?
                                    "advanced_info" : "fast_scroll";
    CAPTURE( action );
    clear_avatar();
    clear_map();
    avatar &u = get_avatar();
    map &here = get_map();
    u.setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    restore_on_out_of_scope restore_view( u.view_offset );
    u.view_offset = tripoint_rel_ms::zero;
    restore_on_out_of_scope restore_editor( uistate.editmap_state );
    editmap_uistate &state = uistate.editmap_state;
    state = editmap_uistate{};
    state.selected.fill( false );
    // An unintended brush confirmation must be observable, even though these
    // two native actions should only change editor display settings.
    state.brush.drawing_terrain = true;
    state.brush.selected_terrain = ter_str_id( "t_wall" ).id();
    const tripoint_bub_ms target = u.pos_bub();
    const field_type_id fire = field_type_str_id( "fd_fire" );
    const field_type_id smoke = field_type_str_id( "fd_smoke" );
    REQUIRE( here.add_field( target + point::north, fire, 1, 0_turns, false ) );
    REQUIRE( here.add_field( target + point::east, smoke, 2, 0_turns, false ) );
    here.furn_set( target + point::north, furn_str_id( "f_brazier" ) );
    struct tile_snapshot {
        tripoint_bub_ms position;
        ter_id terrain;
        furn_id furniture;
        int fire_intensity;
        int smoke_intensity;
        time_duration fire_age;
        time_duration smoke_age;
    };
    std::vector<tile_snapshot> before;
    for( int y = -1; y <= 1; ++y ) {
        for( int x = -1; x <= 1; ++x ) {
            const auto p = target + point( x, y );
            before.push_back( { p, here.ter( p ), here.furn( p ),
                               here.get_field_intensity( p, fire ), here.get_field_intensity( p, smoke ),
                               here.get_field_age( p, fire ), here.get_field_age( p, smoke ) } );
        }
    }
    const auto turn_before = calendar::turn;
    const auto position_before = u.pos_abs();
    const char *transport_path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    const std::optional<std::string> previous_path = transport_path ?
            std::optional<std::string>( transport_path ) : std::nullopt;
    unset_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    on_out_of_scope restore_transport( [&]() {
        assign_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", previous_path );
    } );
    // The real loop dispatches queued semantic actions before reading keys.
    // If a toggle fails to publish its successor, the existing test input
    // guard throws on the next raw read instead of hanging or fabricating it.
    restore_on_out_of_scope restore_test_mode( test_mode );
    test_mode = true;
    semantic_surface_manager manager( "editmap-native-toggle" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World" );
    std::vector<semantic_surface_descriptor> frames;
    std::vector<semantic_action_request> submitted;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &owner ) {
        if( owner.kind != "map_editor" ) {
            return;
        }
        const std::size_t step = frames.size();
        REQUIRE( step < 3 );
        if( !frames.empty() ) {
            CHECK( owner.surface_id == frames.front().surface_id );
            CHECK( owner.frame_id != frames.back().frame_id );
        }
        CHECK( owner.payload.at( "target" ) == target.to_string() );
        CHECK( owner.payload.at( "origin" ) == target.to_string() );
        CHECK( owner.payload.at( "terrain_enabled" ) == "true" );
        // Check the actual setting as well as its published representation.
        CHECK( state.advanced_info_toggle == ( payload_key == "advanced_info" && step == 1 ) );
        CHECK( state.fast_scroll == ( payload_key == "fast_scroll" && step == 1 ) );
        CHECK( owner.payload.count( payload_key ) == 1 );
        if( owner.payload.count( payload_key ) ) {
            CHECK( owner.payload.at( payload_key ) == ( step == 1 ? "true" : "false" ) );
        }
        const auto enabled = std::find_if( owner.valid_actions.begin(), owner.valid_actions.end(),
        [&]( const semantic_action_descriptor &control ) {
            return control.id == action && control.enabled;
        } );
        REQUIRE( enabled != owner.valid_actions.end() );
        frames.push_back( owner );
        if( step == 0 ) {
            // Exercise the manager's existing guards against this actual
            // editor owner before granting its first valid toggle.
            for( const std::string &reason : std::vector<std::string> {
                     "wrong_run", "wrong_surface", "stale_frame"
                 } ) {
                semantic_action_request rejected{ owner.run_id, owner.surface_id, owner.frame_id,
                                                   reason, action, std::nullopt, {} };
                if( reason == "wrong_run" ) {
                    rejected.run_id = "foreign-run";
                } else if( reason == "wrong_surface" ) {
                    rejected.surface_id = "foreign-surface";
                } else {
                    rejected.frame_id = "stale-frame";
                }
                REQUIRE( manager.submit_request( rejected ) );
                REQUIRE_FALSE( manager.consume_top_request() );
                REQUIRE_FALSE( receipts.empty() );
                CHECK_FALSE( receipts.back().accepted );
                CHECK( receipts.back().rejection_reason == reason );
                CHECK( manager.top()->frame_id == owner.frame_id );
                CHECK_FALSE( state.advanced_info_toggle );
                CHECK_FALSE( state.fast_scroll );
            }
        }
        submitted.push_back( { owner.run_id, owner.surface_id, owner.frame_id,
                               "editor-step-" + std::to_string( step ),
                               step < 2 ? action : "editmap.close", std::nullopt, {} } );
        REQUIRE( manager.submit_request( submitted.back() ) );
    } );
    on_out_of_scope clear_observers( [&]() {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
    } );
    {
        editmap_ui editor;
        REQUIRE_NOTHROW( editor.handle_action() );
    }
    REQUIRE( frames.size() == 3 );
    REQUIRE( submitted.size() == 3 );
    REQUIRE( manager.top() );
    CHECK( manager.top()->kind == "world" );
    REQUIRE( receipts.size() == 6 );
    for( std::size_t i = 0; i < submitted.size(); ++i ) {
        const auto &receipt = receipts[i + 3];
        CHECK( receipt.request_id == submitted[i].request_id );
        CHECK( receipt.action_id == submitted[i].action_id );
        CHECK( receipt.accepted );
        CHECK( receipt.consuming_surface_id == frames[i].surface_id );
        CHECK( receipt.consuming_frame_id == frames[i].frame_id );
        CHECK( receipt.resulting_frame_id ==
               ( i < 2 ? frames[i + 1].frame_id : manager.top()->frame_id ) );
    }
    // Identical request replay only re-emits its cached receipt. It must not
    // enqueue or execute a second native toggle on the editor or its parent.
    const auto world_frame = manager.top()->frame_id;
    for( std::size_t i = 0; i < submitted.size(); ++i ) {
        const auto expected = receipts[i + 3];
        const auto count = receipts.size();
        CHECK_FALSE( manager.submit_request( submitted[i] ) );
        REQUIRE( receipts.size() == count + 1 );
        CHECK( receipts.back().request_id == expected.request_id );
        CHECK( receipts.back().accepted == expected.accepted );
        CHECK( receipts.back().consuming_frame_id == expected.consuming_frame_id );
        CHECK( receipts.back().resulting_frame_id == expected.resulting_frame_id );
        CHECK_FALSE( manager.has_pending_request() );
        CHECK( manager.top()->frame_id == world_frame );
    }
    CHECK_FALSE( state.advanced_info_toggle );
    CHECK_FALSE( state.fast_scroll );
    CHECK( calendar::turn == turn_before );
    CHECK( u.pos_abs() == position_before );
    for( const auto &tile : before ) {
        CAPTURE( tile.position );
        CHECK( here.ter( tile.position ) == tile.terrain );
        CHECK( here.furn( tile.position ) == tile.furniture );
        CHECK( here.get_field_intensity( tile.position, fire ) == tile.fire_intensity );
        CHECK( here.get_field_intensity( tile.position, smoke ) == tile.smoke_intensity );
        CHECK( here.get_field_age( tile.position, fire ) == tile.fire_age );
        CHECK( here.get_field_age( tile.position, smoke ) == tile.smoke_age );
    }
    manager.set_descriptor_observer( {} );
    manager.set_receipt_observer( {} );
}

TEST_CASE( "actual map editor vertical controls use native movement bounds and brush",
           "[semantic_surface][editmap_native_vertical]" )
{
    const int direction = GENERATE( -1, 0, 1 );
    CAPTURE( direction );
    clear_avatar();
    clear_map();
    avatar &u = get_avatar();
    map &here = get_map();
    u.setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    restore_on_out_of_scope restore_view( u.view_offset );
    u.view_offset = tripoint_rel_ms::zero;
    restore_on_out_of_scope restore_editor( uistate.editmap_state );
    auto &state = uistate.editmap_state;
    state = editmap_uistate{};
    state.selected.fill( false );
    state.brush.drawing_terrain = true;
    state.brush.selected_terrain = ter_str_id( "t_flat_roof" ).id();
    const auto ground = u.pos_bub();
    const auto upper = ground + tripoint::above;
    const auto ground_terrain = here.ter( ground );
    const auto upper_terrain = here.ter( upper );
    const auto position_before = u.pos_abs();
    const auto turn_before = calendar::turn;
    const auto moves_before = u.get_moves();
    const char *transport = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    const std::optional<std::string> previous = transport ? std::optional<std::string>( transport ) :
        std::nullopt;
    unset_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    on_out_of_scope restore_transport( [&]() {
        assign_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", previous );
    } );
    restore_on_out_of_scope restore_test_mode( test_mode );
    test_mode = true;
    semantic_surface_manager manager( "editor-native-levels" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const auto & receipt ) {
        receipts.push_back( receipt );
    } );
    std::vector<semantic_action_request> requests;
    std::vector<std::pair<std::string, std::string>> steps;
    std::vector<int> expected_z{ 0 };
    if( direction == 0 ) {
        steps = { { "editmap.move_target", "level_up" }, { "editmap.confirm", "" },
            { "editmap.move_target", "level_down" }, { "editmap.close", "" }
        };
        expected_z = { 0, 1, 1, 0 };
    } else {
        const int limit = direction > 0 ? OVERMAP_HEIGHT : -OVERMAP_DEPTH;
        for( int i = 0; i < std::abs( limit ) + 2; ++i ) {
            steps.emplace_back( "editmap.move_target", direction > 0 ? "level_up" : "level_down" );
            expected_z.push_back( direction * std::min( i + 1, std::abs( limit ) ) );
        }
        steps.emplace_back( "editmap.close", "" );
    }
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & owner ) {
        if( owner.kind != "map_editor" ) {
            return;
        }
        const auto step = requests.size();
        REQUIRE( step < steps.size() );
        CHECK( state.brush.target == ground + tripoint( 0, 0, expected_z[step] ) );
        CHECK( owner.payload.at( "target" ) == state.brush.target.to_string() );
        for( const auto &id : {
                 "level_up", "level_down"
             } ) {
            REQUIRE( std::any_of( owner.valid_actions.begin(), owner.valid_actions.end(),
            [&]( const auto & action ) {
                return action.id == "editmap.move_target" && action.stable_id == id && action.enabled;
            } ) );
        }
        if( step == 0 ) {
            semantic_action_request invalid{ owner.run_id, owner.surface_id, owner.frame_id,
                                             "invalid-level", "editmap.move_target", "arbitrary_z", {} };
            REQUIRE( manager.submit_request( invalid ) );
            CHECK_FALSE( manager.consume_top_request() );
            CHECK_FALSE( receipts.back().accepted );
            CHECK( state.brush.target == ground );
            invalid = { owner.run_id, owner.surface_id, "old-frame", "stale-level",
                        "editmap.move_target", "level_up", {}
                      };
            REQUIRE( manager.submit_request( invalid ) );
            CHECK_FALSE( manager.consume_top_request() );
            CHECK( receipts.back().rejection_reason == "stale_frame" );
            CHECK( state.brush.target == ground );
        }
        requests.push_back( { owner.run_id, owner.surface_id, owner.frame_id,
                              "level-step-" + std::to_string( step ), steps[step].first,
                              steps[step].second.empty() ? std::nullopt :
                              std::optional<std::string>( steps[step].second ), {} } );
        REQUIRE( manager.submit_request( requests.back() ) );
    } );
    on_out_of_scope clear_observers( [&]() {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
    } );
    editmap_ui editor;
    REQUIRE_NOTHROW( editor.handle_action() );
    REQUIRE( requests.size() == steps.size() );
    REQUIRE( receipts.size() == steps.size() + 2 );
    for( std::size_t i = 2; i < receipts.size(); ++i ) {
        CHECK( receipts[i].accepted );
    }
    REQUIRE( manager.top() );
    CHECK( manager.top()->kind == "world" );
    CHECK( here.ter( ground ) == ground_terrain );
    CHECK( here.ter( upper ) == ( direction == 0 ? state.brush.selected_terrain : upper_terrain ) );
    CHECK( u.pos_abs() == position_before );
    CHECK( calendar::turn == turn_before );
    CHECK( u.get_moves() == moves_before );
    const auto restored_frame = manager.top()->frame_id;
    CHECK_FALSE( manager.submit_request( requests.front() ) );
    CHECK_FALSE( manager.has_pending_request() );
    CHECK( manager.top()->frame_id == restored_frame );
}

TEST_CASE( "actual vehicle interaction close preserves native state and restores World",
           "[semantic_surface][vehicle_interaction_native_close]" )
{
    clear_avatar();
    clear_map();
    avatar &u = get_avatar();
    map &here = get_map();
    u.setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    vehicle *veh = here.add_vehicle( vproto_id( "car" ), tripoint_bub_ms( 65, 65, 0 ),
                                     -90_degrees, 100, veh_spawn_status::UNDAMAGED );
    REQUIRE( veh != nullptr );
    veh->set_owner( u );
    on_out_of_scope remove_vehicle( [&]() {
        here.destroy_vehicle( veh );
    } );
    const auto serialize = []( const auto & object ) {
        std::ostringstream out;
        JsonOut json( out );
        object.serialize( json );
        return out.str();
    };
    const auto vehicle_before = serialize( *veh );
    const auto body_before = serialize( u );
    const auto turn_before = calendar::turn;
    const char *transport = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    const std::optional<std::string> previous = transport ? std::optional<std::string>( transport ) :
        std::nullopt;
    unset_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    on_out_of_scope restore_transport( [&]() {
        assign_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", previous );
    } );
    restore_on_out_of_scope restore_test_mode( test_mode );
    test_mode = true;
    semantic_surface_manager manager( "vehicle-native-close" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const auto & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_action_request close;
    int frames = 0;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & owner ) {
        if( owner.kind != "vehicle_interaction" ) {
            return;
        }
        REQUIRE( ++frames == 1 );
        REQUIRE( owner.payload.at( "native_owner" ) == "VEH_INTERACT" );
        REQUIRE( owner.valid_actions.size() == 1 );
        CHECK( owner.valid_actions.front().id == "vehicle_interaction.close" );
        CHECK( owner.valid_actions.front().stable_id.empty() );
        close = { owner.run_id, owner.surface_id, owner.frame_id, "close-vehicle",
                  "vehicle_interaction.close", std::nullopt, {}
                };
        for( const std::string negative : {
                 "install", "wrong_run", "stale_frame", "wrong_surface"
             } ) {
            auto rejected = close;
            rejected.request_id = negative;
            if( negative == "install" ) {
                rejected.action_id = "vehicle_interaction.install";
            } else if( negative == "wrong_run" ) {
                rejected.run_id = "foreign";
            } else if( negative == "wrong_surface" ) {
                rejected.surface_id = "foreign";
            } else {
                rejected.frame_id = "old";
            }
            REQUIRE( manager.submit_request( rejected ) );
            CHECK_FALSE( manager.consume_top_request() );
            CHECK_FALSE( receipts.back().accepted );
            CHECK( serialize( *veh ) == vehicle_before );
        }
        REQUIRE( manager.submit_request( close ) );
    } );
    on_out_of_scope clear_observers( [&]() {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
    } );
    REQUIRE_NOTHROW( veh_interact::run( here, *veh, point_rel_ms::zero ) );
    CHECK( frames == 1 );
    REQUIRE( receipts.size() == 5 );
    CHECK( receipts.back().accepted );
    REQUIRE( manager.top() );
    CHECK( manager.top()->kind == "world" );
    CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
    CHECK( serialize( *veh ) == vehicle_before );
    CHECK( serialize( u ) == body_before );
    CHECK( calendar::turn == turn_before );
    CHECK_FALSE( manager.submit_request( close ) );
    CHECK_FALSE( manager.has_pending_request() );
    CHECK( frames == 1 );
    CHECK( serialize( *veh ) == vehicle_before );
}

#if !defined(TILES)
extern "C" int ungetch( int );
extern "C" int flushinp();

TEST_CASE( "vehicle close cannot escape through waited native help input",
           "[semantic_surface][vehicle_interaction_nested_input]" )
{
    clear_avatar();
    clear_map();
    avatar &u = get_avatar();
    map &here = get_map();
    u.setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    vehicle *veh = here.add_vehicle( vproto_id( "car" ), tripoint_bub_ms( 65, 65, 0 ),
                                     -90_degrees, 100, veh_spawn_status::UNDAMAGED );
    REQUIRE( veh != nullptr );
    veh->set_owner( u );
    on_out_of_scope remove_vehicle( [&]() {
        here.destroy_vehicle( veh );
    } );
    const auto serialize = []( const auto &object ) {
        std::ostringstream out;
        JsonOut json( out );
        object.serialize( json );
        return out.str();
    };
    const auto vehicle_before = serialize( *veh );
    const auto body_before = serialize( u );
    const auto turn_before = calendar::turn;
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&]() {
        flushinp();
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope restore_mode( test_mode );
    test_mode = false;
    const auto path = std::filesystem::temp_directory_path() /
                      ( "vehicle-help-owner-" + std::to_string(
                            std::chrono::steady_clock::now().time_since_epoch().count() ) + ".jsonl" );
    const char *old_path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    const std::optional<std::string> previous = old_path ? std::optional<std::string>( old_path ) :
        std::nullopt;
    set_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", path.string().c_str(), 1 );
    on_out_of_scope restore_transport( [&]() {
        assign_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", previous );
        std::filesystem::remove( path );
    } );
    semantic_surface_manager manager( "vehicle-help-owner" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    const auto queue_key = [&]( const std::string &action, const std::string &category ) {
        const auto &keys = inp_mngr.get_input_for_action( action, category );
        const auto key = std::find_if( keys.begin(), keys.end(), []( const input_event &event ) {
            return event.type == input_event_t::keyboard_char && event.sequence.size() == 1 &&
                   event.get_first_input() >= 0 && event.get_first_input() < 128;
        } );
        REQUIRE( key != keys.end() );
        REQUIRE( ungetch( key->get_first_input() ) != -1 );
    };
    const auto append = [&]( const semantic_surface_descriptor &owner, const std::string &id ) {
        std::ofstream stream( path, std::ios::app );
        stream << "{\"run_id\":\"" << owner.run_id << "\",\"surface_id\":\"" << owner.surface_id
               << "\",\"frame_id\":\"" << owner.frame_id << "\",\"request_id\":\"" << id
               << "\",\"action_id\":\"vehicle_interaction.close\"}\n";
    };
    std::thread writer;
    on_out_of_scope join_writer( [&]() {
        if( writer.joinable() ) {
            writer.join();
        }
    } );
    int parents = 0;
    int children = 0;
    int restorations = 0;
    std::vector<semantic_action_receipt> receipts;
    semantic_surface_descriptor parent;
    manager.set_transport_observer( [&]( const semantic_request_transport_event &event ) {
        if( event.event == "request_consumed" && event.request_id == "parent-during-help" ) {
            // Native-thread physical exit also makes the faulty deferred-receipt
            // owner finish honestly instead of trapping the causal control.
            queue_key( "QUIT", "HELP_KEYBINDINGS" );
        }
    } );
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
        if( receipt.request_id == "parent-during-help" ) {
            CHECK_FALSE( receipt.accepted );
            CHECK( receipt.rejection_reason == "wrong_surface" );
            REQUIRE( manager.top() );
            CHECK( manager.top()->kind == "unsupported" );
            CHECK( manager.top()->payload.count( "owner" ) == 1 );
            if( manager.top()->payload.count( "owner" ) ) {
                CHECK( manager.top()->payload.at( "owner" ) == "HELP_KEYBINDINGS" );
            }
        }
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &owner ) {
        if( owner.kind == "unsupported" ) {
            CHECK( owner.payload.at( "owner" ) == "HELP_KEYBINDINGS" );
            CHECK( owner.valid_actions.empty() );
            ++children;
        } else if( owner.kind == "vehicle_interaction" ) {
            if( owner.surface_id == parent.surface_id ) {
                CHECK( owner.frame_id != parent.frame_id );
                ++restorations;
                return;
            }
            ++parents;
            if( parents == 1 ) {
                parent = owner;
                queue_key( "HELP_KEYBINDINGS", "VEH_INTERACT" );
                // Only immutable strings/file bytes cross threads. The manager,
                // curses, callbacks and all assertions stay on the native thread.
                writer = std::thread( [&, owner]() {
                    std::this_thread::sleep_for( std::chrono::milliseconds( 150 ) );
                    append( owner, "parent-during-help" );
                } );
            } else if( parents == 2 ) {
                CHECK( children >= 1 );
                CHECK( restorations >= 1 );
                append( owner, "parent-after-help" );
            } else {
                FAIL( "vehicle owner failed to close after help" );
            }
        }
    } );
    on_out_of_scope clear_observers( [&]() {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
        manager.set_transport_observer( {} );
    } );
    REQUIRE_NOTHROW( veh_interact::run( here, *veh, point_rel_ms::zero ) );
    CHECK( parents == 2 );
    CHECK( children >= 1 );
    CHECK( restorations >= 1 );
    REQUIRE( receipts.size() == 2 );
    CHECK_FALSE( receipts.front().accepted );
    CHECK( receipts.back().request_id == "parent-after-help" );
    CHECK( receipts.back().accepted );
    REQUIRE( manager.top() );
    CHECK( manager.top()->kind == "world" );
    CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
    CHECK( serialize( *veh ) == vehicle_before );
    CHECK( serialize( u ) == body_before );
    CHECK( calendar::turn == turn_before );
}

TEST_CASE( "vehicle close authority is suspended for sibling native input categories",
           "[semantic_surface][vehicle_interaction_nested_input]" )
{
    const std::string category = GENERATE( "STRING_INPUT", "HELP_KEYBINDINGS", "CONFIRMATION" );
    CAPTURE( category );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&]() {
        flushinp();
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    restore_on_out_of_scope restore_mode( test_mode );
    test_mode = false;
    semantic_surface_manager manager( "vehicle-sibling-input" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    int dispatches = 0;
    semantic_surface_scope vehicle_owner( manager, "vehicle_interaction", "Vehicle interaction", {},
    { { "vehicle_interaction.close", "", "Close", true } },
    [&]( const semantic_action_request & ) {
        ++dispatches;
        return semantic_action_dispatch_result{ true, "", "", false, false };
    } );
    const auto parent = *manager.top();
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const auto &receipt ) {
        receipts.push_back( receipt );
    } );
    int children = 0;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &owner ) {
        if( owner.kind != "unsupported" ) {
            return;
        }
        ++children;
        CHECK( owner.payload.at( "owner" ) == category );
        CHECK( owner.valid_actions.empty() );
        REQUIRE( manager.submit_request( { parent.run_id, parent.surface_id, parent.frame_id,
                                           "parent-in-child", "vehicle_interaction.close", std::nullopt, {} } ) );
        manager.mark_transport_wake();
    } );
    on_out_of_scope clear_observers( [&]() {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
    } );
    input_context child( category );
    child.register_action( "ANY_INPUT" );
    CHECK( child.handle_input( 5 ) == "TIMEOUT" );
    CHECK( children == 1 );
    CHECK( dispatches == 0 );
    REQUIRE( receipts.size() == 1 );
    CHECK_FALSE( receipts.front().accepted );
    CHECK( receipts.front().rejection_reason == "wrong_surface" );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == parent.surface_id );
    CHECK( manager.top()->frame_id != parent.frame_id );
    const auto restored = *manager.top();
    REQUIRE( manager.submit_request( { restored.run_id, restored.surface_id, restored.frame_id,
                                       "restored-parent", "vehicle_interaction.close", std::nullopt, {} } ) );
    manager.mark_transport_wake();
    input_context native_parent( "VEH_INTERACT" );
    CHECK( native_parent.handle_input( 5 ) == "ERROR" );
    CHECK( dispatches == 1 );
    REQUIRE( receipts.size() == 2 );
    CHECK( receipts.back().accepted );
}
#endif

TEST_CASE( "actual map editor receipts mutation and no-op completion before close",
           "[semantic_surface][editmap_native_completion]" )
{
    const bool remove_fire_first = GENERATE( false, true );
    CAPTURE( remove_fire_first );
    clear_avatar();
    clear_map();
    avatar &u = get_avatar();
    map &here = get_map();
    u.setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    restore_on_out_of_scope restore_view( u.view_offset );
    u.view_offset = tripoint_rel_ms::zero;
    restore_on_out_of_scope restore_editor( uistate.editmap_state );
    editmap_uistate &state = uistate.editmap_state;
    state = editmap_uistate{};
    state.selected.fill( false );
    state.brush.drawing_terrain = true;
    state.brush.selected_terrain = ter_str_id( "t_wall" ).id();
    const tripoint_bub_ms target = u.pos_bub();
    const auto original_terrain = here.ter( target );
    REQUIRE( original_terrain != state.brush.selected_terrain );
    const field_type_id fire = field_type_str_id( "fd_fire" );
    REQUIRE( here.add_field( target, fire, 1, 0_turns, false ) );
    const auto neighbor = target + point::east;
    const auto neighbor_terrain = here.ter( neighbor );
    const auto furniture = here.furn( target );
    const auto turn_before = calendar::turn;
    const auto position_before = u.pos_abs();
    const char *transport_path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    const std::optional<std::string> previous_path = transport_path ?
            std::optional<std::string>( transport_path ) : std::nullopt;
    unset_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    on_out_of_scope restore_transport( [&]() {
        assign_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", previous_path );
    } );
    restore_on_out_of_scope restore_test_mode( test_mode );
    test_mode = true;
    semantic_surface_manager manager( "editmap-native-completion" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World" );
    std::vector<std::string> actions;
    if( remove_fire_first ) {
        actions.emplace_back( "editmap.remove_field" );
    }
    // The first confirm changes real terrain. The second is a valid no-op on
    // that same tile, which must still finish and leave close usable.
    actions.insert( actions.end(), { "editmap.confirm", "editmap.confirm", "editmap.close" } );
    const std::size_t confirms_begin = remove_fire_first ? 1 : 0;
    std::vector<semantic_surface_descriptor> frames;
    std::vector<semantic_action_request> submitted;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &owner ) {
        if( owner.kind != "map_editor" ) {
            return;
        }
        const std::size_t step = frames.size();
        REQUIRE( step < actions.size() );
        CHECK( here.ter( target ) == ( step > confirms_begin ? state.brush.selected_terrain :
                                      original_terrain ) );
        CHECK( here.get_field_intensity( target, fire ) ==
               ( remove_fire_first && step > 0 ? 0 : 1 ) );
        CHECK( calendar::turn == turn_before );
        CHECK( u.pos_abs() == position_before );
        if( !frames.empty() ) {
            CHECK( owner.surface_id == frames.front().surface_id );
            CHECK( owner.frame_id != frames.back().frame_id );
        }
        // Re-observation alone must never manufacture fresh input authority.
        const auto frame = owner.frame_id;
        REQUIRE( manager.publish( owner.surface_id, owner.payload, owner.valid_actions ) );
        CHECK( manager.top()->frame_id == frame );
        frames.push_back( owner );
        const auto stable_id = actions[step] == "editmap.remove_field" ?
                               std::optional<std::string>( "fd_fire" ) : std::nullopt;
        if( step == 0 ) {
            for( const std::string &reason : std::vector<std::string> {
                     "wrong_run", "wrong_surface", "stale_frame"
                 } ) {
                semantic_action_request rejected{ owner.run_id, owner.surface_id, owner.frame_id,
                                                   reason, actions[step], stable_id, {} };
                if( reason == "wrong_run" ) {
                    rejected.run_id = "foreign-run";
                } else if( reason == "wrong_surface" ) {
                    rejected.surface_id = "foreign-surface";
                } else {
                    rejected.frame_id = "stale-frame";
                }
                REQUIRE( manager.submit_request( rejected ) );
                REQUIRE_FALSE( manager.consume_top_request() );
                REQUIRE_FALSE( receipts.empty() );
                CHECK_FALSE( receipts.back().accepted );
                CHECK( receipts.back().rejection_reason == reason );
                CHECK( manager.top()->frame_id == owner.frame_id );
                CHECK( here.ter( target ) == original_terrain );
                CHECK( here.get_field_intensity( target, fire ) == 1 );
            }
        }
        submitted.push_back( { owner.run_id, owner.surface_id, owner.frame_id,
                               "editor-completion-" + std::to_string( step ), actions[step], stable_id, {} } );
        REQUIRE( manager.submit_request( submitted.back() ) );
    } );
    on_out_of_scope clear_observers( [&]() {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
    } );
    {
        editmap_ui editor;
        REQUIRE_NOTHROW( editor.handle_action() );
    }
    REQUIRE( frames.size() == actions.size() );
    REQUIRE( submitted.size() == actions.size() );
    REQUIRE( receipts.size() == actions.size() + 3 );
    REQUIRE( manager.top() );
    CHECK( manager.top()->kind == "world" );
    const auto world_frame = manager.top()->frame_id;
    for( std::size_t i = 0; i < submitted.size(); ++i ) {
        const auto expected = receipts[i + 3];
        CHECK( expected.accepted );
        CHECK( expected.request_id == submitted[i].request_id );
        CHECK( expected.consuming_frame_id == frames[i].frame_id );
        CHECK( expected.resulting_frame_id ==
               ( i + 1 < frames.size() ? frames[i + 1].frame_id : world_frame ) );
        const auto count = receipts.size();
        CHECK_FALSE( manager.submit_request( submitted[i] ) );
        REQUIRE( receipts.size() == count + 1 );
        CHECK( receipts.back().resulting_frame_id == expected.resulting_frame_id );
        CHECK_FALSE( manager.has_pending_request() );
        CHECK( manager.top()->frame_id == world_frame );
    }
    CHECK( here.ter( target ) == state.brush.selected_terrain );
    CHECK( here.ter( neighbor ) == neighbor_terrain );
    CHECK( here.furn( target ) == furniture );
    CHECK( here.get_field_intensity( target, fire ) == ( remove_fire_first ? 0 : 1 ) );
    CHECK( calendar::turn == turn_before );
    CHECK( u.pos_abs() == position_before );
}

TEST_CASE( "native overmap editor owns catalog placement cancellation and receipts",
           "[semantic_surface][overmap_native_placement]" )
{
    // The real catalog and placement loops must both remain operable. No
    // direct placement helper stands in for either native modal owner.
    const int scenario = GENERATE( 0, 1, 2, 3, 4 );
    CAPTURE( scenario );
    const bool debug_editor = scenario != 4;
    const bool special = scenario == 3;
    const bool cancel_menu = scenario == 0;
    const bool cancel_child = scenario == 1;
    restore_on_out_of_scope restore_debug_mode( debug_mode );
    debug_mode = false;
    clear_avatar();
    clear_map();
    avatar &u = get_avatar();
    const auto origin = u.pos_abs_omt();
    const auto turn_before = calendar::turn;
    const auto position_before = u.pos_abs();
    const auto moves_before = u.get_moves();
    overmap &om = *overmap_buffer.get_om_global( origin ).om;
    std::ostringstream saved_map;
    std::ostringstream saved_view;
    om.serialize( saved_map );
    om.serialize_view( saved_view );
    const auto saved_global = overmap_buffer.global_state;
    on_out_of_scope restore_map( [&]() {
        std::istringstream map_input( saved_map.str() );
        om.unserialize( map_input );
        const auto view = saved_view.str();
        om.unserialize_view( json_loader::from_string( view.substr( view.find( '\n' ) +
                1 ) ).get_object() );
        overmap_buffer.global_state = saved_global;
    } );
    restore_on_out_of_scope restore_overmap_data( g->overmap_data );
    restore_on_out_of_scope restore_blink( uistate.overmap_blinking );
    uistate.overmap_blinking = false;
    const char *request_path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    const std::optional<std::string> old_request_path = request_path ?
        std::optional<std::string>( request_path ) : std::nullopt;
    unset_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    on_out_of_scope restore_transport( [&]() {
        assign_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", old_request_path );
    } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&]() {
        ui::omap::force_quit();
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope restore_test_mode( test_mode );
    test_mode = false;
    ui_adaptor fixture_root;
    fixture_root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw();
    semantic_surface_manager manager( "native-overmap-placement" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    std::vector<semantic_action_request> submitted;
    std::vector<semantic_surface_descriptor> successors;
    int serial = 0;
    const auto request = [&]( const semantic_surface_descriptor & owner, const std::string & action,
                              const std::string & target,
    const std::map<std::string, std::string> &parameters = {} ) {
        return semantic_action_request{ owner.run_id, owner.surface_id, owner.frame_id,
                                        "placement-" + std::to_string( ++serial ), action, target, parameters };
    };
    const auto send = [&]( const semantic_surface_descriptor & owner, const std::string & action,
                           const std::string & target,
    const std::map<std::string, std::string> &parameters = {} ) {
        REQUIRE( std::any_of( owner.valid_actions.begin(), owner.valid_actions.end(),
        [&]( const semantic_action_descriptor & control ) {
            return control.id == action && control.stable_id == target && control.enabled;
        } ) );
        submitted.push_back( request( owner, action, target, parameters ) );
        REQUIRE( manager.submit_request( submitted.back() ) );
    };
    const auto reject = [&]( semantic_action_request invalid, const std::string & reason ) {
        REQUIRE( manager.top() );
        const auto frame = manager.top()->frame_id;
        const auto terrain = overmap_buffer.ter( g->overmap_data.cursor_pos );
        const auto metadata = overmap_buffer.overmap_special_at_existing( g->overmap_data.cursor_pos );
        REQUIRE( manager.submit_request( invalid ) );
        REQUIRE_FALSE( manager.consume_top_request() );
        REQUIRE_FALSE( receipts.empty() );
        CHECK_FALSE( receipts.back().accepted );
        CHECK( receipts.back().rejection_reason == reason );
        CHECK( manager.top()->frame_id == frame );
        CHECK( overmap_buffer.ter( g->overmap_data.cursor_pos ) == terrain );
        CHECK( overmap_buffer.overmap_special_at_existing( g->overmap_data.cursor_pos ) == metadata );
        CHECK( calendar::turn == turn_before );
    };
    int outer_frames = 0;
    int menu_frames = 0;
    std::size_t child_frames = 0;
    bool entry_available = false;
    struct overmap_tile_before {
        tripoint_abs_omt position;
        oter_id terrain;
        om_vision_level seen;
        std::optional<overmap_special_id> special;
    };
    std::vector<overmap_tile_before> unchanged;
    const auto record_region = [&]() {
        for( int z = 0; z <= 1; ++z ) {
            for( int y = -2; y <= 3; ++y ) {
                for( int x = -2; x <= 3; ++x ) {
                    const auto p = origin + tripoint( x, y, z );
                    unchanged.push_back( { p, overmap_buffer.ter( p ), overmap_buffer.seen( p ),
                                           overmap_buffer.overmap_special_at_existing( p ) } );
                }
            }
        }
    };
    const std::vector<std::string> child_actions = cancel_child ?
        std::vector<std::string> { "overmap.close" } :
        special ?
        std::vector<std::string> { "overmap.rotate", "overmap.move_cursor", "overmap.change_level",
                                   "overmap.change_level", "overmap.place_and_close"
                                 } :
        std::vector<std::string> { "overmap.move_cursor", "overmap.change_level",
                                   "overmap.change_level", "overmap.place", "overmap.place", "overmap.close"
                                 };
    std::vector<semantic_surface_descriptor> child_owners;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & owner ) {
        if( owner.kind == "world" ) {
            successors.push_back( owner );
            return;
        }
        CHECK( calendar::turn == turn_before );
        CHECK( u.pos_abs() == position_before );
        CHECK( u.get_moves() == moves_before );
        if( owner.kind == "overmap" && owner.payload.count( "editor_mode" ) == 0 ) {
            successors.push_back( owner );
            if( outer_frames++ == 0 ) {
                const std::string entry = special ? "overmap.place_special" : "overmap.place_terrain";
                const auto found = std::find_if( owner.valid_actions.begin(), owner.valid_actions.end(),
                [&]( const semantic_action_descriptor & control ) {
                    return control.id == entry;
                } );
                entry_available = found != owner.valid_actions.end();
                CHECK( entry_available == debug_editor );
                record_region();
                if( !debug_editor ) {
                    reject( request( owner, entry, "omt:" + origin.to_string() ), "unadvertised_action" );
                } else if( entry_available ) {
                    reject( request( owner, entry, found->stable_id, { { "type", "foreign" } } ),
                    "invalid_parameters" );
                    send( owner, entry, found->stable_id );
                    return;
                }
            } else {
                CHECK( owner.payload.at( "completed_editor_actions" ) == "1" );
            }
            send( owner, "overmap.close", "" );
        } else if( owner.kind == "menu" ) {
            ++menu_frames;
            successors.push_back( owner );
            if( cancel_menu ) {
                send( owner, "menu.cancel", "" );
            } else {
                const std::string id = special ? "special:bandit_camp" : "terrain:forest";
                const bool offered = std::any_of( owner.valid_actions.begin(), owner.valid_actions.end(),
                [&]( const semantic_action_descriptor & control ) {
                    return control.id == "menu.choose" && control.stable_id == id;
                } );
                if( !offered ) {
                    REQUIRE( menu_frames == 1 );
                    // Native terrain labels colorize the ID. Filter the
                    // actual catalog text, then choose its advertised ID.
                    send( owner, "menu.filter", "", { { "text", special ? "bandit_camp" :
                                                         "id: [ " + colorize( "forest", c_white ) + " ]" } } );
                } else {
                    reject( request( owner, "menu.choose", "special:nonexistent" ), "invalid_stable_id" );
                    send( owner, "menu.choose", id );
                }
            }
        } else if( owner.kind == "overmap" ) {
            successors.push_back( owner );
            const std::size_t step = child_frames++;
            REQUIRE( step < child_actions.size() );
            CHECK( owner.payload.at( "editor_mode" ) == "placement" );
            CHECK( owner.payload.at( "placement_id" ) == ( special ? "bandit_camp" : "forest" ) );
            CHECK( owner.payload.at( "completed_actions" ) == std::to_string( step ) );
            const auto cursor = g->overmap_data.cursor_pos;
            CHECK( owner.payload.at( "cursor_position" ) == cursor.to_string() );
            CHECK( owner.payload.at( "level" ) == std::to_string( cursor.z() ) );
            CHECK( cursor == origin + tripoint( step >= ( special ? 2 : 1 ) ? 1 : 0,
                                                0, step == ( special ? 3 : 2 ) ? 1 : 0 ) );
            if( !child_owners.empty() ) {
                CHECK( owner.surface_id == child_owners.front().surface_id );
                CHECK( owner.frame_id != child_owners.back().frame_id );
            }
            if( step == 0 ) {
                for( const std::string reason : {
                         "wrong_run", "wrong_surface", "stale_frame"
                     } ) {
                    auto invalid = request( owner, "overmap.place", owner.payload.at( "selected_stable_id" ) );
                    if( reason == "wrong_run" ) {
                        invalid.run_id = "foreign";
                    } else if( reason == "wrong_surface" ) {
                        invalid.surface_id = "foreign";
                    } else {
                        invalid.frame_id = "stale";
                    }
                    reject( invalid, reason );
                }
                reject( request( owner, "overmap.place", "foreign:type@coordinate" ), "invalid_target" );
                reject( request( owner, "overmap.place", owner.payload.at( "selected_stable_id" ),
                { { "x", "9999" } } ), "invalid_parameters" );
                if( !special ) {
                    reject( request( owner, "overmap.rotate", owner.payload.at( "selected_stable_id" ) ),
                            "unadvertised_action" );
                }
            } else if( owner.payload.at( "selected_stable_id" ) !=
                       child_owners.front().payload.at( "selected_stable_id" ) ) {
                reject( request( owner, "overmap.place", child_owners.front().payload.at( "selected_stable_id" ) ),
                        "invalid_target" );
            }
            if( !special && step >= 4 ) {
                CHECK( overmap_buffer.ter( cursor ) == oter_str_id( "forest" ).id() );
                CHECK( owner.payload.at( "last_placement_result" ) == "applied" );
            }
            child_owners.push_back( owner );
            const auto &action = child_actions[step];
            const std::string target = action == "overmap.move_cursor" ? "east" :
                                       action == "overmap.change_level" ? ( step == ( special ? 2 : 1 ) ? "up" : "down" ) :
                                       action == "overmap.close" ? "" : owner.payload.at( "selected_stable_id" );
            send( owner, action, target );
        } else {
            FAIL( "unexpected native editor child: " + owner.kind );
        }
    } );
    on_out_of_scope clear_observers( [&]() {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
    } );
    if( debug_editor ) {
        ui::omap::display_editor();
    } else {
        ui::omap::display();
    }
    REQUIRE( manager.top() );
    CHECK( manager.top()->kind == "world" );
    CHECK( outer_frames == ( debug_editor && entry_available ? 2 : 1 ) );
    CHECK( child_frames == ( debug_editor && !cancel_menu &&
                             entry_available ? child_actions.size() : 0 ) );
    if( scenario <= 1 || !debug_editor ) {
        for( const auto &tile : unchanged ) {
            CHECK( overmap_buffer.ter( tile.position ) == tile.terrain );
            CHECK( overmap_buffer.seen( tile.position ) == tile.seen );
            CHECK( overmap_buffer.overmap_special_at_existing( tile.position ) == tile.special );
        }
    } else if( entry_available ) {
        const auto placed = origin + point::east;
        if( special ) {
            CHECK( overmap_buffer.overmap_special_at_existing( placed ) ==
                   overmap_special_id( "bandit_camp" ) );
            CHECK( overmap_buffer.ter( placed ) == oter_str_id( "bandit_camp_1_east" ).id() );
        } else {
            CHECK( overmap_buffer.ter( placed ) == oter_str_id( "forest" ).id() );
        }
    }
    std::ostringstream map_before_replay;
    om.serialize( map_before_replay );
    const auto final_world_frame = manager.top()->frame_id;
    for( const auto &sent : submitted ) {
        const auto receipt = std::find_if( receipts.begin(), receipts.end(), [&]( const auto & candidate ) {
            return candidate.request_id == sent.request_id;
        } );
        REQUIRE( receipt != receipts.end() );
        CHECK( receipt->accepted );
        CHECK( receipt->consuming_frame_id == sent.frame_id );
        CHECK( receipt->resulting_frame_id != sent.frame_id );
        const auto successor = std::find_if( successors.begin(), successors.end(), [&]( const auto & successor ) {
            return successor.frame_id == receipt->resulting_frame_id;
        } );
        REQUIRE( successor != successors.end() );
        if( sent.action_id == "overmap.place_terrain" || sent.action_id == "overmap.place_special" ) {
            CHECK( successor->kind == "menu" );
        } else if( sent.action_id == "menu.choose" ) {
            CHECK( successor->kind == "overmap" );
            CHECK( successor->payload.at( "editor_mode" ) == "placement" );
        } else if( sent.action_id == "menu.cancel" || sent.action_id == "overmap.place_and_close" ) {
            CHECK( successor->kind == "overmap" );
            CHECK( successor->payload.count( "editor_mode" ) == 0 );
        }
        const auto count = receipts.size();
        const auto expected = *receipt;
        CHECK_FALSE( manager.submit_request( sent ) );
        REQUIRE( receipts.size() == count + 1 );
        CHECK( receipts.back().resulting_frame_id == expected.resulting_frame_id );
        CHECK_FALSE( manager.has_pending_request() );
        CHECK( manager.top()->kind == "world" );
        CHECK( manager.top()->frame_id == final_world_frame );
    }
    std::ostringstream map_after_replay;
    om.serialize( map_after_replay );
    CHECK( map_after_replay.str() == map_before_replay.str() );
    CHECK( uistate.place_terrain == nullptr );
    CHECK( uistate.place_special == nullptr );
    CHECK( calendar::turn == turn_before );
    CHECK( u.pos_abs() == position_before );
    CHECK( u.get_moves() == moves_before );
}

TEST_CASE( "unchanged semantic surface publication preserves its frame", "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    semantic_surface_scope menu( manager, "menu", "Menu", { { "filter", "" } }, {
        { "menu.choose", "entry-a", "Entry A", true }
    } );
    const std::string initial_frame = manager.top()->frame_id;

    CHECK( menu.publish( { { "filter", "" } }, {
        { "menu.choose", "entry-a", "Entry A", true }
    } ) );
    CHECK( manager.top()->frame_id == initial_frame );

    CHECK( menu.publish( { { "filter", "changed" } }, {
        { "menu.choose", "entry-a", "Entry A", true }
    } ) );
    CHECK( manager.top()->frame_id != initial_frame );
}

TEST_CASE( "only the descriptor-bound top owner consumes a semantic request",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    int actions_consumed = 0;
    semantic_surface_scope world( manager, "world", "World", {}, {
        { "world.wait", "", "Wait", true }
    }, [&actions_consumed]( const semantic_action_request & request ) {
        ++actions_consumed;
        return semantic_action_dispatch_result{ true, "", request.frame_id };
    } );
    semantic_surface_descriptor descriptor = *manager.top();
    CHECK( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                     "request-1", "world.wait", std::nullopt, {} } ) );
    CHECK( world.consume_request() );
    CHECK( actions_consumed == 1 );

    CHECK( world.publish( { { "state", "updated" } },
    { { "world.wait", "", "Wait", true } } ) );
    CHECK( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                     "request-2", "world.wait", std::nullopt, {} } ) );
    CHECK_FALSE( world.consume_request() );
    CHECK( actions_consumed == 1 );

    const semantic_surface_descriptor fresh_descriptor = *manager.top();
    CHECK( manager.submit_request( { "test-run", fresh_descriptor.surface_id,
                                     fresh_descriptor.frame_id, "request-3", "world.move.n", std::nullopt,
                                     {} } ) );
    CHECK_FALSE( world.consume_request() );
    CHECK( actions_consumed == 1 );
}

TEST_CASE( "yielded parent frame fails closed while a native child owns input",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    semantic_surface_scope world( manager, "world", "World" );
    std::vector<semantic_action_receipt> receipts;
    std::vector<semantic_surface_descriptor> descriptors;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor & descriptor ) {
        descriptors.push_back( descriptor );
    } );

    std::optional<semantic_surface_scope> editor;
    const auto make_editor = [&manager, &editor]() {
        editor.emplace( manager, "map_editor", "Map editor", std::map<std::string, std::string> {},
        std::vector<semantic_action_descriptor> {
            { "editmap.confirm", "", "Confirm", true }
        } );
    };
    make_editor();
    const semantic_surface_descriptor retired_editor = *manager.top();

    // The editmap handoff must not briefly restore World.  That transient
    // descriptor can otherwise be mistaken for the post-selection owner.
    REQUIRE( manager.withhold_parent_authority_until_recreated( editor->surface_id() ) );
    editor.reset();
    CHECK_FALSE( manager.top() );
    {
        semantic_surface_scope field_menu( manager, "menu", "Select field", {}, {
            { "menu.choose", "fd_fire", "fire", true }
        } );
        const semantic_surface_descriptor field_descriptor = *manager.top();
        CHECK( field_descriptor.breadcrumbs == std::vector<std::string> { "World", "Select field" } );
        REQUIRE( manager.submit_request( { "test-run", retired_editor.surface_id,
                                           retired_editor.frame_id, "stale-editor", "editmap.confirm",
                                           std::nullopt, {} } ) );
        CHECK_FALSE( field_menu.consume_request() );
        REQUIRE( receipts.size() == 1 );
        CHECK_FALSE( receipts.back().accepted );
        // The retired editor descriptor names a different native owner, not
        // merely an earlier frame of the current child.  Keep that ownership
        // rejection exact while the parent remains withheld.
        CHECK( receipts.back().rejection_reason == "wrong_surface" );
        CHECK( receipts.back().consuming_surface_id == field_descriptor.surface_id );

        // Selecting the field immediately opens its intensity owner.  Keep
        // World private until the edit loop has recreated its scope.
        REQUIRE( manager.withhold_parent_authority_until_recreated( field_menu.surface_id() ) );
    }
    CHECK_FALSE( manager.top() );
    {
        semantic_surface_scope intensity_menu( manager, "menu", "Select intensity" );
        CHECK( manager.top()->breadcrumbs == std::vector<std::string> { "World", "Select intensity" } );
        REQUIRE( manager.withhold_parent_authority_until_recreated( intensity_menu.surface_id() ) );
    }
    CHECK_FALSE( manager.top() );

    // The edit loop recreates a fresh owner only after the nested menu exits.
    make_editor();
    REQUIRE( manager.top() );
    CHECK( manager.top()->kind == "map_editor" );
    CHECK( manager.top()->surface_id != retired_editor.surface_id );
    CHECK( std::none_of( descriptors.begin(), descriptors.end(),
    []( const semantic_surface_descriptor & descriptor ) {
        return descriptor.kind == "world" && descriptor.frame_id != "";
    } ) );
}

TEST_CASE( "semantic requests reject wrong identities and duplicate IDs without redispatch",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    int actions_consumed = 0;
    semantic_surface_scope world( manager, "world", "World", {}, {
        { "world.wait", "", "Wait", true }
    }, [&actions_consumed]( const semantic_action_request & ) {
        ++actions_consumed;
        return semantic_action_dispatch_result{ true, "", "" };
    } );
    semantic_surface_descriptor descriptor = *manager.top();
    CHECK( manager.submit_request( { "other-run", descriptor.surface_id, descriptor.frame_id,
                                     "wrong-run", "world.wait", std::nullopt, {} } ) );
    CHECK_FALSE( world.consume_request() );
    CHECK( manager.submit_request( { "test-run", "other-surface", descriptor.frame_id,
                                     "wrong-surface", "world.wait", std::nullopt, {} } ) );
    CHECK_FALSE( world.consume_request() );
    CHECK( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                     "once", "world.wait", std::nullopt, {} } ) );
    CHECK( world.consume_request() );
    CHECK_FALSE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                           "once", "world.wait", std::nullopt, {} } ) );
    CHECK( actions_consumed == 1 );
}

TEST_CASE( "accepted semantic receipts bind the native successor frame",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World", {}, {
        { "world.wait", "", "Wait", true }
    }, []( const semantic_action_request & ) {
        return semantic_action_dispatch_result{ true, "", "" };
    } );
    const semantic_surface_descriptor world_descriptor = *manager.top();

    CHECK( manager.submit_request( { "test-run", world_descriptor.surface_id,
                                     world_descriptor.frame_id, "request-1", "world.wait",
                                     std::nullopt, {} } ) );
    CHECK( world.consume_request() );
    CHECK( receipts.empty() );

    semantic_surface_scope wait_choice( manager, "menu", "Wait choice", {}, {
        { "wait.duration_menu", "", "Wait", true }
    } );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.back().accepted );
    CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
    CHECK( manager.top()->surface_id == wait_choice.surface_id() );
}

TEST_CASE( "zone manager semantic scope keeps a zone mutation bound to its displayed state",
           "[semantic_surface][zone_manager]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    const std::string zone_id = "zone-42";
    const std::string faction = "your_followers";
    int revision = 7;
    bool enabled = false;
    std::string native_action;
    const auto zone_consumer = [&zone_id, &faction, &revision, &enabled, &native_action]
    ( const semantic_action_request & request ) {
        if( request.stable_id.value_or( "" ) != zone_id ) {
            return semantic_action_dispatch_result{ false, "invalid_zone_id", "" };
        }
        if( request.parameters.at( "faction" ) != faction ) {
            return semantic_action_dispatch_result{ false, "wrong_faction", "" };
        }
        if( request.parameters.at( "revision" ) != std::to_string( revision ) ) {
            return semantic_action_dispatch_result{ false, "stale_revision", "" };
        }
        if( request.action_id != "zone.enable" || enabled ) {
            return semantic_action_dispatch_result{ false, "unavailable", "" };
        }
        native_action = "ENABLE_ZONE";
        return semantic_action_dispatch_result{ true, "", "" };
    };
    semantic_surface_scope zone_scope( manager, "zone_manager", "Zone Manager", {
        { "zone_id", zone_id }, { "faction", faction }, { "enabled", "false" }, { "revision", "7" }
    }, {
        { "zone.enable", zone_id, "Enable zone", true },
        { "zone.disable", zone_id, "Disable zone", false }
    }, zone_consumer );
    semantic_surface_descriptor descriptor = *manager.top();
    CHECK( descriptor.kind == "zone_manager" );
    CHECK( descriptor.payload.at( "zone_id" ) == zone_id );
    CHECK_FALSE( descriptor.valid_actions[1].enabled );

    CHECK( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                     "wrong-owner", "zone.enable", zone_id,
    { { "faction", faction }, { "revision", "7" } } } ) );
    {
        semantic_surface_scope other_owner( manager, "menu", "Other" );
        CHECK_FALSE( other_owner.consume_request() );
    }
    REQUIRE_FALSE( receipts.empty() );
    CHECK( receipts.back().rejection_reason == "wrong_surface" );
    descriptor = *manager.top();

    CHECK( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                     "bad-id", "zone.enable", "other-zone",
    { { "faction", faction }, { "revision", "7" } } } ) );
    CHECK_FALSE( zone_scope.consume_request() );
    CHECK( receipts.back().rejection_reason == "invalid_zone_id" );
    CHECK( native_action.empty() );
    CHECK( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                     "wrong-faction", "zone.enable", zone_id,
    { { "faction", "other_faction" }, { "revision", "7" } } } ) );
    CHECK_FALSE( zone_scope.consume_request() );
    CHECK( receipts.back().rejection_reason == "wrong_faction" );
    CHECK( native_action.empty() );
    CHECK( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                     "stale", "zone.enable", zone_id,
    { { "faction", faction }, { "revision", "6" } } } ) );
    CHECK_FALSE( zone_scope.consume_request() );
    CHECK( receipts.back().rejection_reason == "stale_revision" );
    CHECK( native_action.empty() );
    CHECK( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                     "enable", "zone.enable", zone_id,
    { { "faction", faction }, { "revision", "7" } } } ) );
    CHECK( zone_scope.consume_request() );
    CHECK( native_action == "ENABLE_ZONE" );
    CHECK_FALSE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                           "enable", "zone.enable", zone_id,
    { { "faction", faction }, { "revision", "7" } } } ) );
    enabled = true;
    ++revision;
    semantic_surface_scope successor( manager, "zone_manager", "Zone Manager", {
        { "zone_id", zone_id }, { "faction", faction }, { "enabled", "true" }, { "revision", "8" }
    }, { { "zone.disable", zone_id, "Disable zone", true } }, zone_consumer );
    REQUIRE_FALSE( receipts.empty() );
    CHECK( receipts.back().accepted );
    CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );

    const semantic_surface_descriptor successor_descriptor = *manager.top();
    CHECK( manager.submit_request( { "test-run", successor_descriptor.surface_id,
                                     successor_descriptor.frame_id, "already-enabled", "zone.enable", zone_id,
    { { "faction", faction }, { "revision", "8" } } } ) );
    CHECK_FALSE( successor.consume_request() );
    CHECK( receipts.back().rejection_reason == "unadvertised_action" );
    CHECK( native_action == "ENABLE_ZONE" );
}

TEST_CASE( "top-only request consumption emits an exact rejection without a native binding",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope unsupported( manager, "unsupported", "Legacy prompt", {
        { "stop_reason", "missing explicit semantic scope" }
    } );
    const semantic_surface_descriptor descriptor = *manager.top();

    CHECK( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                     "request-1", "world.wait", std::nullopt, {} } ) );
    CHECK_FALSE( unsupported.consume_request() );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().request_id == "request-1" );
    CHECK( receipts.front().requested_surface_id == descriptor.surface_id );
    CHECK( receipts.front().requested_frame_id == descriptor.frame_id );
    CHECK( receipts.front().consuming_surface_id == descriptor.surface_id );
    CHECK( receipts.front().consuming_frame_id == descriptor.frame_id );
    CHECK_FALSE( receipts.front().accepted );
    CHECK( receipts.front().rejection_reason == "unadvertised_action" );
    CHECK_FALSE( unsupported.consume_request() );
    CHECK( receipts.size() == 1 );
}

TEST_CASE( "uilist consumes a stable semantic choice through its native query loop",
           "[semantic_surface]" )
{
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_descriptor menu_descriptor;
    bool selection_requested = false;
    bool choice_requested = false;
    manager.set_descriptor_observer( [&manager, &menu_descriptor, &selection_requested,
                                                &choice_requested](
    const semantic_surface_descriptor & descriptor ) {
        menu_descriptor = descriptor;
        if( descriptor.kind != "menu" ) {
            return;
        }
        const auto action = std::find_if( descriptor.valid_actions.begin(),
        descriptor.valid_actions.end(), []( const semantic_action_descriptor & candidate ) {
            return candidate.label == "second";
        } );
        REQUIRE( action != descriptor.valid_actions.end() );
        if( !selection_requested ) {
            REQUIRE( action->id == "menu.select" );
            selection_requested = true;
            REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                               "select-second", action->id, action->stable_id, {} } ) );
        } else if( !choice_requested ) {
            const auto action = std::find_if( descriptor.valid_actions.begin(),
            descriptor.valid_actions.end(), []( const semantic_action_descriptor & candidate ) {
                return candidate.id == "menu.choose" && candidate.label == "second";
            } );
            REQUIRE( action != descriptor.valid_actions.end() );
            choice_requested = true;
            REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                               "choose-second", "menu.choose", action->stable_id, {} } ) );
        }
    } );

    uilist menu;
    menu.entries = {
        uilist_entry( 1, true, 'a', "first" ),
        uilist_entry( 2, true, 'b', "second" )
    };
    menu.query();

    CHECK( menu.ret == 2 );
    CHECK( menu.selected == 1 );
    CHECK( menu_descriptor.payload.at( "selected_stable_id" ) == menu.entries[1].semantic_stable_id );
}

TEST_CASE( "large searchable uilist requires a native semantic filter before choices",
           "[semantic_surface]" )
{
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );

    uilist menu;
    for( int index = 0; index < 3000; ++index ) {
        menu.entries.emplace_back( index, true, 0,
                                   "terrain-" + std::to_string( index ) + "-ordinary-entry-name" );
    }
    menu.entries[1777].txt = "opaque barrier terrain";
    const std::string stale_id = menu.entries[1777].semantic_stable_id;
    bool stale_requested = false;
    bool filter_requested = false;
    bool choice_requested = false;
    manager.set_descriptor_observer( [&manager, &stale_requested, &filter_requested, &choice_requested,
              stale_id]( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind != "menu" ) {
            return;
        }
        const bool filter_required = descriptor.payload.at( "filter_required" ) == "true";
        if( filter_required && !stale_requested ) {
            stale_requested = true;
            REQUIRE( std::none_of( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            []( const semantic_action_descriptor & action ) {
                return action.id == "menu.choose";
            } ) );
            REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                               "stale-pre-filter", "menu.choose", stale_id, {} } ) );
            filter_requested = true;
            REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                               "filter", "menu.filter", std::nullopt,
            { { "text", "opaque barrier" } } } ) );
        } else if( !filter_required && !choice_requested ) {
            const auto choice = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            []( const semantic_action_descriptor & action ) {
                return action.id == "menu.choose" && action.label == "opaque barrier terrain";
            } );
            REQUIRE( choice != descriptor.valid_actions.end() );
            choice_requested = true;
            REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                               "choose-filtered", choice->id, choice->stable_id, {} } ) );
        }
    } );

    menu.query();
    REQUIRE( stale_requested );
    REQUIRE( filter_requested );
    REQUIRE( choice_requested );
    REQUIRE_FALSE( receipts.empty() );
    CHECK_FALSE( receipts.front().accepted );
    CHECK( receipts.front().rejection_reason == "unadvertised_action" );
    CHECK( menu.ret == 1777 );
}

TEST_CASE( "uilist can defer a selected receipt to its native child", "[semantic_surface]" )
{
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World" );
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind != "menu" ) {
            return;
        }
        const auto action = std::find_if( descriptor.valid_actions.begin(),
        descriptor.valid_actions.end(), []( const semantic_action_descriptor & candidate ) {
            return candidate.id == "menu.choose";
        } );
        REQUIRE( action != descriptor.valid_actions.end() );
        REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                           "choose-child", action->id, action->stable_id, {} } ) );
    } );

    uilist menu;
    menu.semantic_await_child_successor = true;
    menu.entries = { uilist_entry( 1, true, 'a', "open child" ) };
    menu.query();

    CHECK( menu.ret == 1 );
    CHECK( receipts.empty() );
    semantic_surface_scope child( manager, "dialogue", "NPC" );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().request_id == "choose-child" );
    CHECK( receipts.front().resulting_frame_id == manager.top()->frame_id );
}

TEST_CASE( "child-owned receipt ignores a redraw of its selecting owner", "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World" );
    {
        semantic_surface_scope selector( manager, "menu", "Talk to whom?", {}, {
            { "menu.choose", "talker", "Talk to…", true }
        }, []( const semantic_action_request & ) {
            return semantic_action_dispatch_result{ true, "", "", true };
        } );
        const semantic_surface_descriptor selector_frame = *manager.top();
        REQUIRE( manager.submit_request( { "test-run", selector_frame.surface_id,
                                           selector_frame.frame_id, "choose-talker", "menu.choose",
                                           "talker", {} } ) );
        REQUIRE( selector.consume_request() );
        REQUIRE( manager.publish( selector.surface_id(), { { "selected", "talker" } },
        selector_frame.valid_actions ) );
        CHECK( receipts.empty() );
    }

    semantic_surface_scope dialogue( manager, "dialogue", "NPC" );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().request_id == "choose-talker" );
    CHECK( receipts.front().resulting_frame_id == manager.top()->frame_id );
}

TEST_CASE( "a deferred World chat receipt resolves only at its child owner",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    std::vector<semantic_surface_descriptor> descriptors;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor & descriptor ) {
        descriptors.push_back( descriptor );
    } );

    std::optional<semantic_surface_scope> world;
    world.emplace( manager, "world", "World", std::map<std::string, std::string> {},
    std::vector<semantic_action_descriptor> {
        { "world.chat", "", "Chat", true }
    }, []( const semantic_action_request & ) {
        return semantic_action_dispatch_result{ true, "", "", true };
    } );
    const semantic_surface_descriptor world_frame = *manager.top();
    REQUIRE( manager.submit_request( { "test-run", world_frame.surface_id, world_frame.frame_id,
                                       "chat-child", "world.chat", std::nullopt, {} } ) );
    REQUIRE( world->consume_request() );
    world.reset();

    CHECK_FALSE( manager.top() );
    CHECK( receipts.empty() );
    semantic_surface_scope chat_menu( manager, "menu", "What do you want to do?" );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().resulting_frame_id == manager.top()->frame_id );
    CHECK( std::none_of( descriptors.begin(), descriptors.end(),
    [&world_frame]( const semantic_surface_descriptor & descriptor ) {
        return descriptor.surface_id == world_frame.surface_id &&
               descriptor.frame_id != world_frame.frame_id;
    } ) );
}

TEST_CASE( "a sentence prompt receipt survives its nested chat unwind", "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );

    std::optional<semantic_surface_scope> world;
    world.emplace( manager, "world", "World" );
    std::optional<semantic_surface_scope> prompt;
    prompt.emplace( manager, "string_prompt", "Text input", std::map<std::string, std::string> {},
    std::vector<semantic_action_descriptor> { { "prompt.submit", "", "Submit", true } },
    []( const semantic_action_request & ) {
        return semantic_action_dispatch_result{ true, "", "", true };
    } );
    const semantic_surface_descriptor prompt_frame = *manager.top();
    REQUIRE( manager.submit_request( { "test-run", prompt_frame.surface_id, prompt_frame.frame_id,
                                       "submit-sentence", "prompt.submit", std::nullopt, {} } ) );
    REQUIRE( prompt->consume_request() );
    prompt.reset();
    world.reset();

    CHECK_FALSE( manager.top() );
    CHECK( receipts.empty() );
    semantic_surface_scope next_world( manager, "world", "World" );
    REQUIRE( manager.top() );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().resulting_frame_id == manager.top()->frame_id );
}

TEST_CASE( "native intent handoff is single-use and run-bound", "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    const semantic_action_request request{ "test-run", "main-menu", "frame", "quit-request",
                                           "main_menu.quit", std::nullopt, {} };
    CHECK( manager.queue_native_intent( request, "main_menu.quit" ) );
    CHECK_FALSE( manager.queue_native_intent( request, "main_menu.quit" ) );
    CHECK_FALSE( manager.take_native_intent( "wrong-intent" ) );
    CHECK( manager.take_native_intent( "main_menu.quit" ) );
    CHECK_FALSE( manager.take_native_intent( "main_menu.quit" ) );
    CHECK_FALSE( manager.queue_native_intent( { "wrong-run", "main-menu", "frame", "stale-request",
                 "main_menu.quit", std::nullopt, {} }, "main_menu.quit" ) );
}

TEST_CASE( "item-use restoration publishes only its recreated menu owner", "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_surface_descriptor> descriptors;
    std::vector<semantic_action_receipt> receipts;
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor & descriptor ) {
        descriptors.push_back( descriptor );
    } );
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World" );
    {
        semantic_surface_scope item_menu( manager, "inventory_item_menu", "lighter", {}, {
            { "inventory.item_menu.choose", "activate", "activate", true }
        }, []( const semantic_action_request & ) {
            return semantic_action_dispatch_result{ true, "", "" };
        } );
        const semantic_surface_descriptor item_menu_frame = *manager.top();
        REQUIRE( manager.submit_request( { "test-run", item_menu_frame.surface_id,
                                           item_menu_frame.frame_id, "activate-lighter",
                                           "inventory.item_menu.choose", "activate", {} } ) );
        REQUIRE( item_menu.consume_request() );
        REQUIRE( manager.withhold_parent_authority_until_recreated( item_menu.surface_id() ) );
        {
            semantic_surface_scope direction( manager, "direction", "Light where?", {}, {
                { "direction.choose", "north", "North", true }
            }, []( const semantic_action_request & ) {
                return semantic_action_dispatch_result{ true, "", "" };
            } );
            const semantic_surface_descriptor direction_frame = *manager.top();
            REQUIRE( manager.submit_request( { "test-run", direction_frame.surface_id,
                                               direction_frame.frame_id, "choose-north",
                                               "direction.choose", "north", {} } ) );
            REQUIRE( direction.consume_request() );
        }
        CHECK_FALSE( manager.top() );
    }

    // The outer native loop may publish World while the old menu is between
    // destruction and recreation.  That publication stays private.
    REQUIRE( manager.publish( world.surface_id(), {}, {} ) );

    semantic_surface_scope recreated_item_menu( manager, "inventory_item_menu", "lighter" );
    const std::string recreated_frame = manager.top()->frame_id;
    REQUIRE( !descriptors.empty() );
    CHECK( descriptors.back().surface_id == recreated_item_menu.surface_id() );
    CHECK( descriptors.back().kind == "inventory_item_menu" );
    CHECK( std::none_of( descriptors.begin() + 2,
    descriptors.end(), []( const semantic_surface_descriptor & d ) {
        return d.kind == "world";
    } ) );
    REQUIRE( receipts.size() == 2 );
    CHECK( receipts.back().request_id == "choose-north" );
    CHECK( receipts.back().resulting_frame_id == recreated_frame );
}

TEST_CASE( "direction selection receipts bind to its real confirmation successor",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_surface_descriptor> descriptors;
    std::vector<semantic_action_receipt> receipts;
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor & descriptor ) {
        descriptors.push_back( descriptor );
    } );
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World" );
    {
        semantic_surface_scope item_menu( manager, "inventory_item_menu", "lighter" );
        REQUIRE( manager.withhold_parent_authority_until_recreated( item_menu.surface_id() ) );
        {
            semantic_surface_scope direction( manager, "direction", "Light where?", {}, {
                { "direction.choose", "north", "North", true }
            }, []( const semantic_action_request & ) {
                return semantic_action_dispatch_result{ true, "", "", true };
            } );
            const semantic_surface_descriptor direction_frame = *manager.top();
            REQUIRE( manager.submit_request( { "test-run", direction_frame.surface_id,
                                               direction_frame.frame_id, "choose-north",
                                               "direction.choose", "north", {} } ) );
            REQUIRE( direction.consume_request() );
        }
        CHECK( receipts.empty() );
        semantic_surface_scope prompt( manager, "prompt", "Light firewood?", {}, {
            { "prompt.choose", "no", "No", true }
        } );
        REQUIRE( receipts.size() == 1 );
        CHECK( receipts.front().request_id == "choose-north" );
        CHECK( receipts.front().resulting_frame_id == manager.top()->frame_id );
    }
    CHECK( std::none_of( descriptors.begin() + 2,
    descriptors.end(), []( const semantic_surface_descriptor & d ) {
        return d.kind == "world" || d.kind == "inventory_item_menu";
    } ) );
}

TEST_CASE( "withheld main-menu quit releases its owner before publishing confirmation",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_surface_descriptor> descriptors;
    std::vector<semantic_action_receipt> receipts;
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor & descriptor ) {
        descriptors.push_back( descriptor );
    } );
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );

    std::optional<semantic_surface_scope> main_menu;
    main_menu.emplace( manager, "main_menu", "Main menu", std::map<std::string, std::string> {},
    std::vector<semantic_action_descriptor> { { "main_menu.quit", "", "Quit", true } },
    [&manager]( const semantic_action_request & request ) {
        if( !manager.withhold_parent_authority_until_recreated( request.surface_id ) ) {
            return semantic_action_dispatch_result{ false, "native_parent_withhold_failed", "" };
        }
        return semantic_action_dispatch_result{ true, "", "", true };
    } );
    const semantic_surface_descriptor main_menu_frame = *manager.top();
    REQUIRE( manager.submit_request( { "test-run", main_menu_frame.surface_id,
                                       main_menu_frame.frame_id, "quit", "main_menu.quit", "", {} } ) );
    REQUIRE( main_menu->consume_request() );

    // This matches the current-iteration quit path: no stale main-menu
    // descriptor may stand between the accepted action and query_yn's prompt.
    main_menu.reset();
    semantic_surface_scope confirmation( manager, "prompt", "YESNO", {
        { "text", "Really quit?" }
    }, { { "prompt.choose", "yes", "YES", true } } );

    REQUIRE( manager.top() );
    CHECK( manager.top()->kind == "prompt" );
    CHECK( manager.top()->breadcrumbs == std::vector<std::string> { "YESNO" } );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().request_id == "quit" );
    CHECK( receipts.front().resulting_frame_id == manager.top()->frame_id );
    CHECK( std::none_of( descriptors.begin() + 1, descriptors.end(),
    []( const semantic_surface_descriptor & descriptor ) {
        return descriptor.kind == "main_menu";
    } ) );
}

TEST_CASE( "uilist rejects disabled and duplicate stable semantic choices",
           "[semantic_surface]" )
{
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );

    uilist disabled;
    disabled.entries = { uilist_entry( 1, false, 'd', "disabled" ) };
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind == "menu" ) {
            const semantic_action_descriptor &action = descriptor.valid_actions.front();
            REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                               "disabled-choice", action.id, action.stable_id, {} } ) );
        }
    } );
    disabled.query( false );
    REQUIRE( receipts.size() == 1 );
    CHECK_FALSE( receipts.back().accepted );
    CHECK( disabled.ret == UILIST_WAIT_INPUT );

    receipts.clear();
    uilist duplicate;
    duplicate.entries = {
        uilist_entry( 1, true, 'a', "first" ),
        uilist_entry( 2, true, 'b', "second" )
    };
    duplicate.entries[1].semantic_stable_id = duplicate.entries[0].semantic_stable_id;
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind == "menu" ) {
            const semantic_action_descriptor &action = descriptor.valid_actions.front();
            REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                               "duplicate-choice", action.id, action.stable_id, {} } ) );
        }
    } );
    duplicate.query( false );
    REQUIRE( receipts.size() == 1 );
    CHECK_FALSE( receipts.back().accepted );
    CHECK( receipts.back().rejection_reason == "duplicate_stable_id" );
}

TEST_CASE( "uilist semantic identity survives caller presentation reorder",
           "[semantic_surface]" )
{
    uilist_entry first( 1, true, 'a', "duplicate label" );
    uilist_entry second( 2, true, 'b', "duplicate label" );
    const std::string first_id = first.semantic_stable_id;
    const std::string second_id = second.semantic_stable_id;

    std::vector<uilist_entry> entries = { second, first };
    CHECK( entries[0].semantic_stable_id == second_id );
    CHECK( entries[1].semantic_stable_id == first_id );
    CHECK( entries[0].semantic_stable_id != entries[1].semantic_stable_id );
}

TEST_CASE( "ordinary debug menu chooses a child action by semantic stable ID",
           "[semantic_surface]" )
{
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind != "menu" ) {
            return;
        }
        if( descriptor.payload.at( "text" ) == "Debug Functions" ) {
            REQUIRE_FALSE( descriptor.breadcrumbs.empty() );
            CHECK( descriptor.breadcrumbs.back() == "Debug Functions" );
        }
        const std::string target = descriptor.payload.at( "text" ) == "Debug Functions" ? "Game…" :
                                   "Show debug message";
        const auto action = std::find_if( descriptor.valid_actions.begin(),
        descriptor.valid_actions.end(), [&target]( const semantic_action_descriptor & candidate ) {
            return candidate.id == "menu.choose" && candidate.label == target;
        } );
        REQUIRE( action != descriptor.valid_actions.end() );
        REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                           descriptor.payload.at( "text" ), "menu.choose",
                                           action->stable_id, {} } ) );
    } );

    const std::optional<debug_menu::debug_menu_index> action = debug_menu::choose_action();
    REQUIRE( action );
    CHECK( *action == debug_menu::debug_menu_index::SHOW_MSG );
}

TEST_CASE( "debug map submenu publishes a semantic successor for Map editor",
           "[semantic_surface]" )
{
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    bool saw_map_menu = false;
    manager.set_descriptor_observer( [&manager,
    &saw_map_menu]( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind != "menu" ) {
            return;
        }
        const std::string &text = descriptor.payload.at( "text" );
        const std::string target = text == "Debug Functions" ? "Map…" : "Map editor";
        const auto action = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
        [&target]( const semantic_action_descriptor & candidate ) {
            return candidate.id == "menu.choose" && candidate.label == target;
        } );
        REQUIRE( action != descriptor.valid_actions.end() );
        if( text == "Map…" ) {
            saw_map_menu = true;
            CHECK( descriptor.breadcrumbs.back() == "Map…" );
        }
        REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                           text, "menu.choose", action->stable_id, {} } ) );
    } );

    const std::optional<debug_menu::debug_menu_index> action = debug_menu::choose_action();
    REQUIRE( action );
    CHECK( saw_map_menu );
    CHECK( *action == debug_menu::debug_menu_index::MAP_EDITOR );
}

TEST_CASE( "query popup consumes its bound semantic option in test mode",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind == "prompt" ) {
            const semantic_action_descriptor &action = descriptor.valid_actions.front();
            REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                               "accept-popup", "prompt.choose", action.stable_id, {} } ) );
        }
    } );

    const query_popup::result result = query_popup().message( "%s",
                                       "Continue?" ).option( "YES" ).query();
    CHECK( result.action == "YES" );
}

TEST_CASE( "auto-move cancellation popup binds YES to its native successor frame",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_manager_session session( manager );
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind != "prompt" ) {
            return;
        }
        const semantic_action_descriptor &yes = descriptor.valid_actions.front();
        REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                           "cancel-auto-move", "prompt.choose", yes.stable_id, {} } ) );
    } );

    const query_popup::result result = query_popup()
                                       .context( "YESNO" )
                                       .message( "%s", "feral human spotted! Cancel auto move?" )
                                       .option( "YES" )
                                       .option( "NO" )
                                       .await_semantic_successor()
                                       .query();
    CHECK( result.action == "YES" );
    {
        semantic_surface_scope successor( manager, "auto_move_canceled", "Auto move canceled" );
        REQUIRE( receipts.size() == 1 );
        CHECK( receipts.front().request_id == "cancel-auto-move" );
        CHECK( receipts.front().accepted );
        CHECK_FALSE( receipts.front().resulting_frame_id.empty() );
        CHECK( receipts.front().resulting_frame_id == manager.top()->frame_id );
    }
}

TEST_CASE( "string prompt accepts constraint-valid structured semantic text",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind == "string_prompt" ) {
            REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                               "submit-text", "prompt.submit", std::nullopt,
            { { "text", "42" } } } ) );
        }
    } );

    const catacurses::window window = catacurses::newwin( 3, 30, point::zero );
    REQUIRE( window );
    string_input_popup prompt;
    prompt.window( window, point( 1, 1 ),
                   28 ).title( "Number" ).text( "" ).max_length( 2 ).only_digits( true );
    CHECK( prompt.query_string() == "42" );
    CHECK( prompt.confirmed() );
    CHECK_FALSE( prompt.canceled() );
}

TEST_CASE( "imgui string prompt owns semantic coordinate submission and rejects stale requests",
           "[semantic_surface]" )
{
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    bool saw_prompt = false;
    bool rejected_wrong_surface = false;
    bool rejected_stale_frame = false;
    std::vector<std::string> rejection_reasons;
    manager.set_receipt_observer( [&rejection_reasons]( const semantic_action_receipt & receipt ) {
        if( !receipt.accepted ) {
            rejection_reasons.push_back( receipt.rejection_reason );
        }
    } );
    manager.set_descriptor_observer( [&manager, &saw_prompt, &rejected_wrong_surface,
                                                &rejected_stale_frame]
    ( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind != "string_prompt" ) {
            return;
        }
        saw_prompt = true;
        CHECK( descriptor.valid_actions[0].id == "prompt.submit" );
        REQUIRE( manager.submit_request( { "test-run", "wrong-surface", descriptor.frame_id,
                                           "wrong", "prompt.submit", std::nullopt,
        { { "text", "0'152,0'49,0" } } } ) );
        rejected_wrong_surface = !manager.consume_top_request();
        REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, "stale-frame",
                                           "stale", "prompt.submit", std::nullopt,
        { { "text", "0'152,0'49,0" } } } ) );
        rejected_stale_frame = !manager.consume_top_request();
        REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                           "submit", "prompt.submit", std::nullopt,
        { { "text", "0'152,0'49,0" } } } ) );
    } );

    string_input_popup_imgui popup( 65, "", "Teleport where?" );
    CHECK( popup.query() == "0'152,0'49,0" );
    CHECK( saw_prompt );
    CHECK( rejected_wrong_surface );
    CHECK( rejected_stale_frame );
    CHECK( rejection_reasons == std::vector<std::string> { "wrong_surface", "stale_frame" } );
}

TEST_CASE( "rejected transport stays inside the native input owner until a real successor",
           "[semantic_surface][shared_input_owner_067]" )
{
    const bool same_poll = GENERATE( false, true );
    const std::string rejection = GENERATE( "wrong_run", "wrong_surface", "stale_frame", "duplicate" );
    const auto path = std::filesystem::temp_directory_path() /
                      ( "shared-input-owner-" + std::to_string(
                            std::chrono::steady_clock::now().time_since_epoch().count() ) + ".jsonl" );
    const char *old_path = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    const std::optional<std::string> previous = old_path ? std::optional<std::string>( old_path ) :
            std::nullopt;
    on_out_of_scope cleanup( [&]() {
        if( previous ) {
            set_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", previous->c_str(), 1 );
        } else {
            unset_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
        }
        std::filesystem::remove( path );
    } );
    set_test_environment( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", path.string().c_str(), 1 );
#if defined(TILES)
    software_render_fixture renderer;
    REQUIRE( renderer.available() );
    renderer_recovery_test_support::setup_software_ui();
#else
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    } else {
        catacurses::refresh();
    }
    on_out_of_scope close_renderer( [&]() {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
#endif
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    semantic_surface_manager manager( "shared-input-owner" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope world( manager, "world", "World" );
    int native_dispatches = 0;
    std::vector<semantic_surface_descriptor> publications;
    std::vector<semantic_action_receipt> receipts;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & descriptor ) {
        publications.push_back( descriptor );
    } );
    semantic_surface_scope parent( manager, "inventory", "Inventory", {},
    { { "inventory.select", "item:1", "Select", true } },
    [&]( const semantic_action_request & ) {
        ++native_dispatches;
        return semantic_action_dispatch_result{ true, "", "", true, true };
    } );
    const auto original = *manager.top();
    const auto append = [&]( const std::string & run, const std::string & surface,
    const std::string & frame, const std::string & request ) {
        std::ofstream stream( path, std::ios::app );
        stream << "{\"run_id\":\"" << run << "\",\"surface_id\":\"" << surface
               << "\",\"frame_id\":\"" << frame << "\",\"request_id\":\"" << request
               << "\",\"action_id\":\"inventory.select\",\"stable_id\":\"item:1\"}\n";
    };
    if( rejection == "duplicate" ) {
        REQUIRE( manager.submit_request( { "foreign-run", original.surface_id, original.frame_id,
                                           "inert", "inventory.select", "item:1", {} } ) );
        REQUIRE_FALSE( manager.consume_top_request() );
    }
    manager.set_receipt_observer( [&]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
        if( receipt.request_id == "inert" ) {
            CHECK_FALSE( receipt.accepted );
            CHECK( receipt.rejection_reason == ( rejection == "duplicate" ? "wrong_run" : rejection ) );
            REQUIRE( manager.top() );
            CHECK( manager.top()->surface_id == original.surface_id );
            CHECK( manager.top()->frame_id == original.frame_id );
            if( !same_poll ) {
                append( manager.run_id(), original.surface_id, original.frame_id, "select" );
            }
        }
    } );
    append( rejection == "wrong_run" || rejection == "duplicate" ? "foreign-run" : manager.run_id(),
            rejection == "wrong_surface" ? "foreign-surface" : original.surface_id,
            rejection == "stale_frame" ? "foreign-frame" : original.frame_id, "inert" );
    if( same_poll ) {
        append( manager.run_id(), original.surface_id, original.frame_id, "select" );
    }
    input_context input( "INVENTORY" );
    input.register_action( "QUIT" );
    input.handle_input( 100 );
    CHECK( native_dispatches == 1 );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == original.surface_id );
    CHECK( manager.top()->frame_id == original.frame_id );
    if( native_dispatches != 1 ) {
        return;
    }
    REQUIRE( receipts.size() == 1 );
    {
        bool closed = false;
        semantic_surface_scope child( manager, "prompt", "Confirmation", {},
        { { "prompt.acknowledge", "", "Acknowledge", true } },
        [&]( const semantic_action_request & ) {
            closed = true;
            return semantic_action_dispatch_result{ true, "", "" };
        } );
        REQUIRE( receipts.size() == 2 );
        CHECK( receipts.back().accepted );
        CHECK( receipts.back().request_id == "select" );
        CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
        const auto child_frame = *manager.top();
        REQUIRE( manager.submit_request( { manager.run_id(), child_frame.surface_id,
                                           child_frame.frame_id, "ack", "prompt.acknowledge", std::nullopt, {} } ) );
        manager.mark_transport_wake();
        input.handle_input( 100 );
        CHECK( closed );
    }
    REQUIRE( receipts.size() == 3 );
    CHECK( receipts.back().request_id == "ack" );
    CHECK( receipts.back().accepted );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == original.surface_id );
    CHECK( manager.top()->frame_id != original.frame_id );
    REQUIRE( publications.size() >= 3 );
    CHECK( publications.back().frame_id == manager.top()->frame_id );
    REQUIRE( manager.submit_request( { manager.run_id(), original.surface_id, original.frame_id,
                                       "old-parent", "inventory.select", "item:1", {} } ) );
    CHECK_FALSE( parent.consume_request() );
    CHECK( receipts.back().rejection_reason == "stale_frame" );
    CHECK( native_dispatches == 1 );
}

#if !defined(TILES)
TEST_CASE( "debug prompt retains native keyboard continuation outside semantic mode",
           "[semantic_surface][debug_error_prompt_067]" )
{
    REQUIRE( active_semantic_surface_manager() == nullptr );
    const char *old_run = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID" );
    const std::optional<std::string> previous = old_run ? std::optional<std::string>( old_run ) :
            std::nullopt;
    unset_test_environment( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID" );
    on_out_of_scope restore_environment( [&]() {
        if( previous ) {
            set_test_environment( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", previous->c_str(), 1 );
        } else {
            unset_test_environment( "OPENCLAW_HARNESS_SEMANTIC_RUN_ID" );
        }
    } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_renderer( [&]() {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    restore_on_out_of_scope<bool> restore_mode( test_mode );
    test_mode = false;
    {
        restore_on_out_of_scope<catacurses::window> restore_screen( catacurses::stdscr );
        catacurses::stdscr = catacurses::window();
        CHECK( capture_debugmsg_during( [&]() {
            realDebugmsg( "off-mode-control.cpp", "1", "keyboard_control", "native keyboard error" );
        } ) == "native keyboard error" );
    }
    REQUIRE_FALSE( openclaw_harness_semantic_session_active() );
    // Queue at the raw debug prompt's intended read, after renderer setup;
    // no inventory editor/setup loop consumes or resets this native input.
    REQUIRE( ungetch( ' ' ) != -1 );
    REQUIRE_NOTHROW( replay_buffered_debugmsg_prompts() );
    CHECK( active_semantic_surface_manager() == nullptr );
}

TEST_CASE( "shared input ownership preserves bounded ordinary keyboard reads",
           "[semantic_surface][shared_input_owner_keyboard_067]" )
{
    const bool bound = GENERATE( false, true );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    } else {
        catacurses::refresh();
    }
    on_out_of_scope close_owned_terminal( [&]() {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    semantic_surface_manager manager( "physical-keyboard" );
    std::optional<semantic_surface_manager_session> session;
    std::optional<semantic_surface_scope> owner;
    if( bound ) {
        session.emplace( manager );
        owner.emplace( manager, "inventory", "Native inventory" );
    }
    input_context input( "INVENTORY" );
    input.register_action( "ANY_INPUT" );
    REQUIRE( ungetch( '1' ) != -1 );
    input.handle_input( 100 );
    CHECK( input.get_raw_input().type == input_event_t::keyboard_char );
    CHECK( input.get_raw_input().get_first_input() == '1' );
    if( bound ) {
        CHECK( manager.top()->surface_id == owner->surface_id() );
    } else {
        CHECK( active_semantic_surface_manager() == nullptr );
    }
}
#endif

TEST_CASE( "native journal bounds diagnostics while preserving current authority",
           "[semantic_surface][native_log_window]" )
{
    namespace fs = std::filesystem;
    const fs::path directory = fs::temp_directory_path() /
                               ( "caol-native-window-" + std::to_string(
                                     std::chrono::steady_clock::now().time_since_epoch().count() ) );
    fs::create_directories( directory );
    const fs::path trace = directory / "semantic.native.events.jsonl";
    const std::vector<std::string> keys = { "OPENCLAW_HARNESS_RUN_ID", "OPENCLAW_HARNESS_SEMANTIC_RUN_ID",
                                            "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", "OPENCLAW_HARNESS_RAID_ACTOR_TRACE"
                                          };
    std::map<std::string, std::optional<std::string>> previous;
    for( const std::string &key : keys ) {
        const char *value = std::getenv( key.c_str() );
        previous[key] = value ? std::optional<std::string>( value ) : std::nullopt;
    }
    on_out_of_scope cleanup( [&] {
        for( const auto &entry : previous )
        {
            if( entry.second ) {
                set_test_environment( entry.first.c_str(), entry.second->c_str(), 1 );
            } else {
                unset_test_environment( entry.first.c_str() );
            }
        }
        fs::remove_all( directory );
    } );
    const std::string run = directory.filename().string();
    set_test_environment( keys[0].c_str(), run.c_str(), 1 );
    set_test_environment( keys[1].c_str(), run.c_str(), 1 );
    set_test_environment( keys[2].c_str(), trace.string().c_str(), 1 );
    set_test_environment( keys[3].c_str(), run.c_str(), 1 );
    raid_decision_trace::recorder diagnostic_recorder( { run, run, run, trace.string() } );
    semantic_surface_manager &manager = openclaw_harness_semantic_surface_manager();
    int dispatched = 0;
    std::vector<semantic_action_descriptor> actions;
    for( int index = 0; index < 200; ++index ) {
        actions.push_back( { "inventory.pick", "item:" + std::to_string( index ),
                             std::string( 4096, 'x' ), true } );
    }
    const std::string surface = manager.push( "inventory", "Inventory", {}, actions,
    [&]( const semantic_action_request & ) {
        ++dispatched;
        return semantic_action_dispatch_result{ true, "", "", false, true, "picked" };
    } );
    on_out_of_scope pop( [&] { manager.pop( surface ); } );
    const semantic_surface_descriptor issued = *manager.top();
    const semantic_action_request request = { run, issued.surface_id, issued.frame_id,
                                              "pick-once", "inventory.pick", "item:199", {}
                                            };
    REQUIRE( manager.submit_request( request ) );
    REQUIRE( manager.consume_top_request() );
    CHECK( dispatched == 1 );
    REQUIRE( manager.publish( surface, { { "selection", "item:199" } }, actions ) );
    // An action catalogue is lossless, including a non-front physical item.
    const std::string diagnostic( 96 * 1024, 'd' );
    for( const std::string state : {
             "active", "hostile_boundary", "resumed", "progress", "completed_cleared"
         } ) {
        REQUIRE( write_native_semantic_event( "{\"event\":\"travel\",\"run_id\":\"" + run +
                                              "\",\"travel_id\":\"trip-1\",\"receipt_id\":\"trip-1:" + state +
                                              "\",\"state\":\"" + state + "\",\"destination\":[129,149,0]," +
                                              "\"destination_present\":" + ( state == "completed_cleared" ? "false" : "true" ) +
                                              ",\"destination_cleared\":" + ( state == "completed_cleared" ? "true" : "false" ) + "}" ) );
    }
    std::set<std::string> travel_states;
    REQUIRE( write_native_diagnostic_event( "{\"event\":\"raid_performance_interval\",\"run_id\":\"" + run +
                                           "\",\"retention_scope\":\"max_simulation_row_only\",\"turn\":11}" ) );
    REQUIRE( write_native_diagnostic_event( "{\"event\":\"raid_performance_interval\",\"run_id\":\"" + run +
                                           "\",\"retention_scope\":\"max_simulation_row_only\",\"turn\":12}" ) );
    REQUIRE( write_native_diagnostic_event( "{\"event\":\"raid_performance_interval\",\"run_id\":\"" + run +
                                           "\",\"turn\":13}" ) );
    REQUIRE( write_native_diagnostic_event( "{\"event\":\"native_performance_extrema\",\"schema\":\"caol-native-performance-extrema-v1\",\"run_id\":\"" + run + "\",\"native_process_instance\":\"native-a\",\"native_pid\":73,\"snapshot_sequence\":1}" ) );
    REQUIRE( write_native_diagnostic_event( "{\"event\":\"native_performance_extrema\",\"schema\":\"caol-native-performance-extrema-v1\",\"run_id\":\"" + run + "\",\"native_process_instance\":\"native-a\",\"native_pid\":73,\"snapshot_sequence\":2}" ) );
    REQUIRE( write_native_diagnostic_event( "{\"event\":\"native_slow_turn_receipts\",\"schema\":\"caol-native-slow-turn-receipts-v1\",\"run_id\":\"" + run + "\",\"native_process_instance\":\"native-a\",\"native_pid\":73,\"snapshot_sequence\":1}" ) );
    REQUIRE( write_native_diagnostic_event( "{\"event\":\"native_slow_turn_receipts\",\"schema\":\"caol-native-slow-turn-receipts-v1\",\"run_id\":\"" + run + "\",\"native_process_instance\":\"native-a\",\"native_pid\":73,\"snapshot_sequence\":2}" ) );
    REQUIRE( write_native_diagnostic_event( "{\"event\":\"native_slow_turn_receipts\",\"schema\":\"caol-native-slow-turn-receipts-v1\",\"run_id\":\"" + run + "\",\"native_process_instance\":\"foreign\",\"native_pid\":73,\"snapshot_sequence\":3}" ) );
    for( int index = 0; index < 100; ++index ) {
        std::ostringstream event;
        event << "{\"event\":\"turn\",\"run_id\":\"" << run
              << "\",\"diagnostic\":\"" << diagnostic << "\"}";
        REQUIRE( write_native_diagnostic_event( event.str() ) );
        diagnostic_recorder.record( "actor", "raid_actor_action", index,
                                    "\"action\":\"Pause\",\"npc_id\":5",
                                    std::to_string( index ) );
    }
    // These all use the same real recorder, including names without raid_.
    // A diagnostic resembling authority must not replace the current owner.
    const std::set<std::string> diagnostic_kinds = {
        "bandit_shakedown_emission", "bandit_shakedown_delivery", "bandit_shakedown_contact",
        "bandit_shakedown_fight_reply", "npc_alarm", "npc_attack_callback",
        "scout_perceived_threat", "scout_alarm_response", "surface_descriptor"
    };
    for( const std::string &kind : diagnostic_kinds ) {
        diagnostic_recorder.record_edge( kind, 100, "\"frame_id\":\"diagnostic-not-input\"" );
    }
    std::set<std::string> retained_diagnostic_kinds;
    std::ifstream input( trace );
    std::string line;
    std::string accepted_frame;
    bool accepted = false;
    bool current = false;
    bool raid_diagnostic = false;
    std::set<int> retained_performance_turns;
    std::set<int> retained_extrema_snapshots;
    std::set<int> retained_slow_snapshots;
    std::size_t discarded = 0;
    while( std::getline( input, line ) ) {
        const std::string prefix = "openclaw_harness_semantic_step: ";
        const bool semantic = line.compare( 0, prefix.size(), prefix ) == 0;
        std::istringstream body( semantic ? line.substr( prefix.size() ) : line );
        TextJsonIn jsin( body );
        const TextJsonObject object = jsin.get_object();
        object.allow_omitted_members();
        const std::string kind = object.get_string( "event" );
        if( !semantic ) {
            CHECK( ( kind.compare( 0, 5, "raid_" ) == 0 || kind == "turn" ||
                     kind == "native_performance_extrema" || kind == "native_slow_turn_receipts" || diagnostic_kinds.count( kind ) ) );
            raid_diagnostic = raid_diagnostic || kind.compare( 0, 5, "raid_" ) == 0;
            if( kind == "raid_performance_interval" ) {
                retained_performance_turns.insert( object.get_int( "turn" ) );
            }
            if( kind == "native_performance_extrema" ) {
                retained_extrema_snapshots.insert( object.get_int( "snapshot_sequence" ) );
            }
            if( kind == "native_slow_turn_receipts" ) {
                retained_slow_snapshots.insert( object.get_int( "snapshot_sequence" ) );
            }
            if( diagnostic_kinds.count( kind ) ) {
                retained_diagnostic_kinds.insert( kind );
            }
        } else if( kind == "trace_generation" ) {
            discarded = object.get_int( "recent_records_discarded" );
        } else if( kind == "travel" ) {
            CHECK( object.get_string( "travel_id" ) == "trip-1" );
            travel_states.insert( object.get_string( "state" ) );
        } else if( kind == "surface_descriptor" ) {
            CHECK( object.get_array( "valid_actions" ).size() == 200 );
            current = object.get_string( "frame_id" ) == manager.top()->frame_id;
        } else if( kind == "surface_receipt" && object.get_bool( "accepted" ) ) {
            CHECK( object.get_string( "request_id" ) == "pick-once" );
            CHECK( object.get_string( "action_id" ) == "inventory.pick" );
            accepted_frame = object.get_string( "resulting_frame_id" );
            accepted = true;
        }
    }
    CHECK( discarded > 0 );
    CHECK( retained_extrema_snapshots == std::set<int> { 2 } );
    CHECK( retained_slow_snapshots == std::set<int> { 2 } );
    CHECK( travel_states == std::set<std::string> { "active", "hostile_boundary", "progress", "completed_cleared" } );
    CHECK( raid_diagnostic );
    CHECK( retained_performance_turns == std::set<int> { 12 } );
    CHECK( retained_diagnostic_kinds == diagnostic_kinds );
    CHECK( current );
    CHECK( accepted );
    CHECK( accepted_frame == manager.top()->frame_id );
    CHECK( fs::file_size( trace ) < 8 * 1024 * 1024 );
    CHECK_FALSE( fs::exists( trace.string() + ".next" ) );
    // A later redraw is current authority; replay must still retain the old
    // successor named by the accepted receipt after that row leaves recent.
    REQUIRE( manager.publish( surface, { { "selection", "redrawn" } }, actions ) );
    CHECK( manager.top()->frame_id != accepted_frame );
    REQUIRE( write_native_semantic_event( "{\"event\":\"travel\",\"run_id\":\"" + run +
                                          "\",\"travel_id\":\"trip-2\",\"receipt_id\":\"trip-2:active\"," +
                                          "\"state\":\"active\",\"destination\":[130,149,0]," +
                                          "\"destination_present\":true,\"destination_cleared\":false}" ) );
    manager.submit_request( request );
    manager.consume_top_request();
    CHECK( dispatched == 1 );
    input.close();
    for( int index = 0; index < 100; ++index ) {
        REQUIRE( write_native_diagnostic_event( "{\"event\":\"turn\",\"run_id\":\"" + run +
                                                "\",\"diagnostic\":\"" + diagnostic + "\"}" ) );
    }
    std::ifstream replayed( trace );
    bool named_successor = false;
    current = false;
    while( std::getline( replayed, line ) ) {
        const std::string prefix = "openclaw_harness_semantic_step: ";
        std::istringstream body( line.compare( 0, prefix.size(), prefix ) == 0 ?
                                 line.substr( prefix.size() ) : line );
        TextJsonIn jsin( body );
        const TextJsonObject object = jsin.get_object();
        object.allow_omitted_members();
        if( object.get_string( "event" ) == "surface_descriptor" ) {
            const std::string frame = object.get_string( "frame_id" );
            named_successor = named_successor || frame == accepted_frame;
            current = current || frame == manager.top()->frame_id;
        } else if( object.get_string( "event" ) == "travel" ) {
            CHECK( object.get_string( "travel_id" ) == "trip-2" );
            CHECK( object.get_string( "state" ) == "active" );
        }
    }
    CHECK( named_successor );
    CHECK( current );
    CHECK( fs::file_size( trace ) < 8 * 1024 * 1024 );
    replayed.close();
    // Actual output failure must recover through a complete replacement, not
    // append behind an incomplete record left by a failed write.
    fs::remove( trace );
    fs::create_directory( trace );
    const bool previous_error = debug_has_error_been_observed();
    CHECK_FALSE( write_native_diagnostic_event( "{\"event\":\"turn\",\"run_id\":\"" + run + "\"}" ) );
    CHECK( debug_has_error_been_observed() );
    if( !previous_error ) {
        debug_reset_error_observed();
    }
    fs::remove( trace );
    {
        std::ofstream broken( trace );
        broken << "openclaw_harness_semantic_step: {";
    }
    REQUIRE( write_native_diagnostic_event( "{\"event\":\"turn\",\"run_id\":\"" + run + "\"}" ) );
    std::ifstream recovered( trace );
    current = accepted = false;
    while( std::getline( recovered, line ) ) {
        const std::string prefix = "openclaw_harness_semantic_step: ";
        std::istringstream body( line.compare( 0, prefix.size(), prefix ) == 0 ?
                                 line.substr( prefix.size() ) : line );
        TextJsonIn jsin( body );
        const TextJsonObject object = jsin.get_object();
        object.allow_omitted_members();
        if( object.get_string( "event" ) == "surface_descriptor" ) {
            CHECK( object.get_array( "valid_actions" ).size() == 200 );
            current = object.get_string( "frame_id" ) == manager.top()->frame_id;
        } else if( object.get_string( "event" ) == "surface_receipt" && object.get_bool( "accepted" ) ) {
            accepted = object.get_string( "request_id" ) == request.request_id;
        }
    }
    CHECK( current );
    CHECK( accepted );
    CHECK( dispatched == 1 );
    const auto before = fs::file_size( trace );
    const bool prior_identity_error = debug_has_error_been_observed();
    CHECK_FALSE(
        write_native_diagnostic_event( "{\"event\":\"npc_alarm\",\"run_id\":\"foreign-run\"}" ) );
    CHECK( fs::file_size( trace ) == before );
    if( !prior_identity_error ) {
        debug_reset_error_observed();
    }
    unset_test_environment( keys[1].c_str() );
    CHECK_FALSE( write_native_semantic_event( "{\"event\":\"turn\",\"run_id\":\"" + run + "\"}" ) );
    CHECK_FALSE( write_native_diagnostic_event( "{\"event\":\"turn\",\"run_id\":\"" + run + "\"}" ) );
    CHECK( fs::file_size( trace ) == before );
}


TEST_CASE( "native journal rejects invalid records and initial temporary IO before valid retry",
           "[semantic_surface][native_journal_failure_067]" )
{
    namespace fs = std::filesystem;
    const int failure = GENERATE( 0, 1, 2, 3, 4, 5 );
    CAPTURE( failure );
    const fs::path directory = fs::temp_directory_path() /
                               ( "native-journal-failure-" + std::to_string(
                                     std::chrono::steady_clock::now().time_since_epoch().count() ) );
    fs::create_directories( directory / "config" );
    const fs::path trace = directory / "native.jsonl";
    const fs::path temporary = trace.string() + ".next";
    const std::vector<std::string> keys = { "OPENCLAW_HARNESS_RUN_ID", "OPENCLAW_HARNESS_SEMANTIC_RUN_ID",
                                            "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH"
                                          };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &key : keys ) {
        const char *value = std::getenv( key.c_str() );
        previous[key] = value ? std::optional<std::string>( value ) : std::nullopt;
    }
    const std::string original_directory = PATH_INFO::user_dir();
    const bool previous_error = debug_has_error_been_observed();
    on_out_of_scope cleanup( [&] {
        for( const auto &entry : previous )
        {
            if( entry.second ) {
                set_test_environment( entry.first.c_str(), entry.second->c_str(), 1 );
            } else {
                unset_test_environment( entry.first.c_str() );
            }
        }
        deinitDebug();
        PATH_INFO::init_user_dir( original_directory );
        PATH_INFO::set_standard_filenames();
        setupDebug( DebugOutput::std_err );
        if( !previous_error )
        {
            debug_reset_error_observed();
        }
        fs::remove_all( directory );
    } );
    deinitDebug();
    PATH_INFO::init_user_dir( directory.string() + "/" );
    PATH_INFO::set_standard_filenames();
    setupDebug( DebugOutput::file );
    const fs::path log = fs::path( PATH_INFO::debug() );
    const std::string run = directory.filename().string();
    set_test_environment( keys[0].c_str(), run.c_str(), 1 );
    set_test_environment( keys[1].c_str(), run.c_str(), 1 );
    set_test_environment( keys[2].c_str(), trace.string().c_str(), 1 );
    const std::string good = "{\"event\":\"frame\",\"run_id\":\"" + run +
                             "\",\"frame_id\":\"first-frame\"}";
    std::string record;
    const char *expected_stage = "";
    if( failure == 0 ) {
        record = "{\"event\":@}";
        expected_stage = "json_parse";
    } else if( failure == 1 ) {
        record = "{\"event\":7,\"run_id\":\"" + run + "\"}";
        expected_stage = "record_type field=event";
    } else if( failure == 2 ) {
        record = "{\"event\":\"frame\",\"run_id\":false}";
        expected_stage = "record_type field=run_id";
    } else if( failure == 3 ) {
        record = "{\"event\":\"frame\",\"run_id\":\"foreign\"}";
        expected_stage = "record_run_mismatch";
    } else if( failure == 4 ) {
        // A real temporary-target open failure, not a permission assumption.
        fs::create_directory( temporary );
        record = good;
        expected_stage = "temporary_open";
    } else {
        // First compaction writes successfully, then the actual rename fails.
        fs::create_directory( trace );
        record = good;
        expected_stage = "rename";
    }
    CHECK_FALSE( write_native_semantic_event( record ) );
    CHECK_FALSE( fs::is_regular_file( trace ) );
    deinitDebug();
    std::ifstream errors( log );
    const std::string text( ( std::istreambuf_iterator<char>( errors ) ),
                            std::istreambuf_iterator<char>() );
    CHECK( text.find( std::string( "stage=" ) + expected_stage ) != std::string::npos );
    CHECK( text.find( record ) == std::string::npos );
    if( failure <= 2 ) {
        CHECK( text.find( "line=-1 column=-1" ) == std::string::npos );
    }
    if( failure == 5 ) {
        CHECK( text.find( "error_code=0" ) == std::string::npos );
    }
    errors.close();
    setupDebug( DebugOutput::file );
    if( failure == 4 ) {
        fs::remove( temporary );
    }
    if( failure == 5 ) {
        fs::remove( trace );
    }
    REQUIRE( write_native_semantic_event( good ) );
    CHECK( fs::file_size( trace ) > 0 );
    CHECK_FALSE( fs::exists( temporary ) );
    std::ifstream published( trace );
    std::string row;
    int frames = 0;
    while( std::getline( published, row ) ) {
        const std::string prefix = "openclaw_harness_semantic_step: ";
        REQUIRE( row.compare( 0, prefix.size(), prefix ) == 0 );
        std::istringstream body( row.substr( prefix.size() ) );
        TextJsonIn input( body );
        const auto object = input.get_object();
        object.allow_omitted_members();
        CHECK( object.get_string( "run_id" ) == run );
        if( object.get_string( "event" ) == "frame" ) {
            ++frames;
        }
    }
    // Temporary IO remembers the complete failed row for replacement retry;
    // parser/run refusals never entered retention. No malformed row publishes.
    CHECK( frames == ( failure >= 4 ? 2 : 1 ) );
}

TEST_CASE( "initial HUD World is emitted only after actual journal publication",
           "[semantic_surface][initial_world_publication_retry_067]" )
{
    namespace fs = std::filesystem;
    const fs::path directory = fs::temp_directory_path() /
                               ( "native-initial-retry-" + std::to_string(
                                     std::chrono::steady_clock::now().time_since_epoch().count() ) );
    fs::create_directories( directory );
    const fs::path trace = directory / "native.jsonl";
    const fs::path temporary = trace.string() + ".next";
    const std::vector<std::string> keys = { "OPENCLAW_HARNESS_RUN_ID", "OPENCLAW_HARNESS_SEMANTIC_RUN_ID",
                                            "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH"
                                          };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &key : keys ) {
        const char *value = std::getenv( key.c_str() );
        previous[key] = value ? std::optional<std::string>( value ) : std::nullopt;
    }
    const bool previous_error = debug_has_error_been_observed();
    on_out_of_scope cleanup( [&] {
        for( const auto &entry : previous )
        {
            if( entry.second ) {
                set_test_environment( entry.first.c_str(), entry.second->c_str(), 1 );
            } else {
                unset_test_environment( entry.first.c_str() );
            }
        }
        if( !previous_error )
        {
            debug_reset_error_observed();
        }
        fs::remove_all( directory );
    } );
    const auto run = directory.filename().string();
    set_test_environment( keys[0].c_str(), run.c_str(), 1 );
    set_test_environment( keys[1].c_str(), run.c_str(), 1 );
    set_test_environment( keys[2].c_str(), trace.string().c_str(), 1 );
    const auto context = get_default_mode_input_context();
    fs::create_directory( temporary );
    openclaw_harness_semantic_initial_world_frame_if_ready( &context, true, true, true );
    CHECK_FALSE( fs::exists( trace ) );
    fs::remove( temporary );
    // SAME run/context and no gameplay input: failed output must not latch emitted.
    openclaw_harness_semantic_initial_world_frame_if_ready( &context, true, true, true );
    REQUIRE( fs::exists( trace ) );
    const auto bytes = fs::file_size( trace );
    openclaw_harness_semantic_initial_world_frame_if_ready( &context, true, true, true );
    CHECK( fs::file_size( trace ) == bytes );
    std::ifstream published( trace );
    std::string row;
    bool ready = false;
    while( std::getline( published, row ) ) {
        const std::string prefix = "openclaw_harness_semantic_step: ";
        std::istringstream body( row.substr( prefix.size() ) );
        TextJsonIn input( body );
        const auto object = input.get_object();
        object.allow_omitted_members();
        ready = ready || ( object.get_string( "event" ) == "frame" &&
                           object.get_string( "producer", "" ) == "hud_world_ready" &&
                           object.get_bool( "initial_world_ready", false ) );
    }
    CHECK( ready );
}


TEST_CASE( "rejected receipt and travel types cannot alter later journal retention",
           "[semantic_surface][native_journal_rejected_retention_067]" )
{
    namespace fs = std::filesystem;
    const int invalid = GENERATE( 0, 1, 2, 3 );
    CAPTURE( invalid );
    const fs::path directory = fs::temp_directory_path() /
                               ( "native-rejected-retention-" + std::to_string(
                                     std::chrono::steady_clock::now().time_since_epoch().count() ) );
    fs::create_directories( directory );
    const fs::path trace = directory / "native.jsonl";
    const std::vector<std::string> keys = { "OPENCLAW_HARNESS_RUN_ID", "OPENCLAW_HARNESS_SEMANTIC_RUN_ID",
                                            "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH"
                                          };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &key : keys ) {
        const char *value = std::getenv( key.c_str() );
        previous[key] = value ? std::optional<std::string>( value ) : std::nullopt;
    }
    const bool previous_error = debug_has_error_been_observed();
    on_out_of_scope cleanup( [&] {
        for( const auto &entry : previous )
        {
            if( entry.second ) {
                set_test_environment( entry.first.c_str(), entry.second->c_str(), 1 );
            } else {
                unset_test_environment( entry.first.c_str() );
            }
        }
        if( !previous_error )
        {
            debug_reset_error_observed();
        }
        fs::remove_all( directory );
    } );
    const auto run = directory.filename().string();
    set_test_environment( keys[0].c_str(), run.c_str(), 1 );
    set_test_environment( keys[1].c_str(), run.c_str(), 1 );
    set_test_environment( keys[2].c_str(), trace.string().c_str(), 1 );
    const std::string base = "\"run_id\":\"" + run + "\",";
    REQUIRE( write_native_semantic_event( "{" + base +
                                          "\"event\":\"surface_descriptor\",\"frame_id\":\"frame-0\"}" ) );
    REQUIRE( write_native_semantic_event( "{" + base +
                                          "\"event\":\"surface_receipt\",\"request_id\":\"accepted-once\","
                                          "\"accepted\":true,\"resulting_frame_id\":\"frame-0\"}" ) );
    for( const auto state : {
             "active", "hostile_boundary", "progress"
         } ) {
        REQUIRE( write_native_semantic_event( "{" + base +
                                              "\"event\":\"travel\",\"travel_id\":\"current-trip\","
                                              "\"state\":\"" + state + "\"}" ) );
    }
    std::string rejected;
    if( invalid == 0 ) {
        rejected = "{" + base + "\"event\":\"surface_receipt\",\"request_id\":\"rejected-record\","
                   "\"accepted\":true,\"resulting_frame_id\":7}";
    } else if( invalid == 1 ) {
        rejected = "{" + base + "\"event\":\"surface_receipt\",\"request_id\":\"rejected-record\","
                   "\"accepted\":7,\"resulting_frame_id\":\"frame-0\"}";
    } else if( invalid == 2 ) {
        rejected = "{" + base + "\"event\":\"travel\",\"travel_id\":\"rejected-record\",\"state\":7}";
    } else {
        rejected = "{" + base + "\"event\":\"travel\",\"travel_id\":7,\"state\":\"rejected-record\"}";
    }
    REQUIRE_FALSE( write_native_semantic_event( rejected ) );
    // Evict the earlier rows from the existing 256-record recent window so
    // retry tests the authority pins, not a surviving diagnostic copy.
    for( int index = 0; index <= 256; ++index ) {
        REQUIRE( write_native_diagnostic_event( "{" + base +
                                                "\"event\":\"turn\",\"index\":" + std::to_string( index ) + "}" ) );
    }
    // Use the already-supported real output failure/replacement route to
    // compel compaction, without calling a private journal or extra parser.
    fs::remove( trace );
    fs::create_directory( trace );
    REQUIRE_FALSE( write_native_diagnostic_event( "{" + base + "\"event\":\"turn\"}" ) );
    fs::remove( trace );
    REQUIRE( write_native_semantic_event( "{" + base +
                                          "\"event\":\"surface_descriptor\",\"frame_id\":\"frame-1\"}" ) );
    std::ifstream published( trace );
    std::string row;
    bool accepted = false;
    bool current_frame = false;
    std::set<std::string> travel_states;
    while( std::getline( published, row ) ) {
        CHECK( row.find( "rejected-record" ) == std::string::npos );
        const std::string prefix = "openclaw_harness_semantic_step: ";
        std::istringstream body( row.compare( 0, prefix.size(), prefix ) == 0 ?
                                 row.substr( prefix.size() ) : row );
        TextJsonIn input( body );
        const auto object = input.get_object();
        object.allow_omitted_members();
        CHECK( object.get_string( "run_id" ) == run );
        const auto kind = object.get_string( "event" );
        if( kind == "surface_descriptor" ) {
            current_frame = current_frame || object.get_string( "frame_id" ) == "frame-1";
        } else if( kind == "surface_receipt" ) {
            if( object.get_string( "request_id" ) == "accepted-once" ) {
                CHECK( object.get_bool( "accepted" ) );
                CHECK( object.get_string( "resulting_frame_id" ) == "frame-0" );
                accepted = true;
            }
        } else if( kind == "travel" ) {
            CHECK( object.get_string( "travel_id" ) == "current-trip" );
            travel_states.insert( object.get_string( "state" ) );
        }
    }
    CHECK( accepted );
    CHECK( current_frame );
    CHECK( travel_states == std::set<std::string> { "active", "hostile_boundary", "progress" } );
    CHECK_FALSE( fs::exists( trace.string() + ".next" ) );
    std::cout << "R067_REJECTED_RETENTION invalid=" << invalid
              << " accepted=" << accepted << " current_frame=" << current_frame
              << " retained_travel_states=" << travel_states.size() << '\n';
}

TEST_CASE( "debug file is bounded during one long running process", "[debug][native_log_window]" )
{
    CHECK_FALSE( raid_perf_debug_sink_active() );
    namespace fs = std::filesystem;
    const fs::path directory = fs::temp_directory_path() /
                               ( "caol-debug-window-" + std::to_string(
                                     std::chrono::steady_clock::now().time_since_epoch().count() ) );
    fs::create_directories( directory / "config" );
    const std::string original = PATH_INFO::user_dir();
    on_out_of_scope cleanup( [&] {
        deinitDebug();
        PATH_INFO::init_user_dir( original );
        PATH_INFO::set_standard_filenames();
        setupDebug( DebugOutput::std_err );
        fs::remove_all( directory );
    } );
    deinitDebug();
    PATH_INFO::init_user_dir( directory.string() + "/" );
    PATH_INFO::set_standard_filenames();
    setupDebug( DebugOutput::file );
    const fs::path log = fs::path( PATH_INFO::debug() );
    for( int index = 0; index < 100; ++index ) {
        DebugLog( D_INFO, D_MAIN ) << std::string( 100 * 1024, 'd' );
    }
    DebugLog( D_INFO, D_MAIN ) << "latest-current-diagnostic";
    deinitDebug();
    REQUIRE( fs::exists( log ) );
    CHECK( fs::file_size( log ) <= 8 * 1024 * 1024 );
    CHECK_FALSE( fs::exists( log.string() + ".prev" ) );
    CHECK_FALSE( fs::exists( log.string() + ".next" ) );
    std::ifstream input( log );
    const std::string retained( ( std::istreambuf_iterator<char>( input ) ),
                                std::istreambuf_iterator<char>() );
    CHECK( retained.find( "Earlier diagnostic log window discarded" ) != std::string::npos );
    CHECK( retained.find( "latest-current-diagnostic" ) != std::string::npos );
}


#if !defined(TILES)
TEST_CASE( "avatar activity driver publishes its actual tired travel decision",
           "[semantic_surface][activity_driver_lifetime_067]" )
{
    const bool enclosing = GENERATE( false, true );
    const int choice = GENERATE( 1, 2, 3, 4 ); // 4 is native Escape from the tired query.
    const bool canceled = choice == 2 || choice == 4;
    const bool escaped = choice == 4;
    CAPTURE( enclosing, choice );
    override_option autosave( "AUTOSAVE", "false" );
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    clear_avatar();
    clear_map();
    clear_npcs();
    if( canceled ) {
        // Set up the ordinary native turn mode through registered save/load;
        // handle_action is private and must be reached by its actual caller.
        REQUIRE( world_generator->active_world );
        const auto world = world_generator->active_world->world_name;
        REQUIRE( g->save() );
        REQUIRE( turn_handler::cleanup_at_end() );
        world_generator->set_active_world( nullptr );
        REQUIRE( g->load( world ) );
        clear_avatar();
        clear_map();
        clear_npcs();
    }
    avatar &u = get_avatar();
    map &here = get_map();
    u.setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    u.set_stamina( u.get_stamina_max() / 4 );
    u.set_moves( 100 );
    const auto current = u.pos_abs_omt();
    u.omt_path = { current + point::east, current };
    u.assign_activity( player_activity( activity_id( "ACT_TRAVELLING" ) ) );
    const std::vector<std::string> keys = { "OPENCLAW_HARNESS_RUN_ID",
                                            "OPENCLAW_HARNESS_SEMANTIC_RUN_ID",
                                            "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH"
                                          };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &key : keys ) {
        const char *value = std::getenv( key.c_str() );
        previous[key] = value ? std::optional<std::string>( value ) : std::nullopt;
    }
    const std::string run = "activity-driver-" +
                            std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() );
    set_test_environment( keys[0].c_str(), run.c_str(), 1 );
    set_test_environment( keys[1].c_str(), run.c_str(), 1 );
    unset_test_environment( keys[2].c_str() );
    on_out_of_scope restore( [&]() {
        for( const auto &entry : previous ) {
            if( entry.second ) {
                set_test_environment( entry.first.c_str(), entry.second->c_str(), 1 );
            } else {
                unset_test_environment( entry.first.c_str() );
            }
        }
        u.cancel_activity();
        u.clear_destination();
        u.omt_path.clear();
    } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&]() {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    ui_adaptor fixture_root;
    fixture_root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw(); // native IMTUI frame/fonts before uilist::calc_data
    semantic_surface_manager nested( run );
    semantic_surface_manager &manager = enclosing ? nested :
                                        openclaw_harness_semantic_surface_manager();
    std::optional<semantic_surface_manager_session> session;
    if( enclosing ) {
        session.emplace( manager );
    }
    std::vector<semantic_action_receipt> receipts;
    std::vector<semantic_action_receipt> world_receipts;
    int menus = 0;
    int world_frames = 0;
    semantic_action_request selected;
    std::optional<semantic_surface_descriptor> successor;
    semantic_surface_descriptor old_overmap;
    {
        // The native action owner has already returned before either activity
        // driver executes. Its old close is never consent to the new decision.
        semantic_surface_scope overmap( manager, "overmap", "Overmap", {},
        { { "overmap.close", "", "Close", true } } );
        old_overmap = *manager.top();
    }
    manager.set_receipt_observer( [&]( const auto & receipt ) {
        if( receipt.request_id.find( "ordinary-world-pause-" ) == 0 ) {
            world_receipts.push_back( receipt );
        } else {
            receipts.push_back( receipt );
        }
    } );
    on_out_of_scope remove_observers( [&]() {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
        if( enclosing ) {
            openclaw_harness_semantic_surface_manager().set_descriptor_observer( {} );
            openclaw_harness_semantic_surface_manager().set_receipt_observer( {} );
        }
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & d ) {
        if( d.kind == "world" ) {
            ++world_frames;
            const auto action = std::find_if( d.valid_actions.begin(), d.valid_actions.end(),
            []( const auto & a ) {
                return a.id == "world.pause";
            } );
            REQUIRE( action != d.valid_actions.end() );
            REQUIRE( action->enabled );
            REQUIRE( openclaw_harness_semantic_surface_manager().submit_request( { run, d.surface_id, d.frame_id, "ordinary-world-pause-" + std::to_string( world_frames ),
                     action->id, action->stable_id, {} } ) );
            return;
        }
        if( d.kind == "activity_wait" ) {
            successor = d;
        }
        if( d.kind != "menu" ) {
            return;
        }
        ++menus;
        CHECK( active_semantic_surface_manager() == &manager );
        CHECK( d.payload.at( "text" ).find( "You struggle to continue" ) != std::string::npos );
        CHECK( d.frame_id != old_overmap.frame_id );
        REQUIRE( manager.submit_request( { run, old_overmap.surface_id, old_overmap.frame_id,
                                           "stale-close", "overmap.close", "", {} } ) );
        if( escaped ) {
            return;
        }
        const std::string label = choice == 1 ? "Continue after a break." :
                                  choice == 2 ? "Maybe later." : "Finish it.";
        const auto action = std::find_if( d.valid_actions.begin(), d.valid_actions.end(),
        [&]( const auto & a ) {
            return a.id == "menu.choose" && a.label == label;
        } );
        REQUIRE( action != d.valid_actions.end() );
        CHECK( action->enabled );
        selected = { run, d.surface_id, d.frame_id, "tired-choice", action->id, action->stable_id, {} };
        REQUIRE( manager.submit_request( selected ) );
    } );
    if( enclosing ) {
        // Ordinary handle_action owns the global manager even when the earlier
        // activity driver preserved an enclosing modal manager.
        auto &world_manager = openclaw_harness_semantic_surface_manager();
        world_manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & d ) {
            if( d.kind == "world" ) {
                ++world_frames;
                const auto action = std::find_if( d.valid_actions.begin(), d.valid_actions.end(),
                []( const auto & a ) {
                    return a.id == "world.pause";
                } );
                REQUIRE( action != d.valid_actions.end() );
                REQUIRE( action->enabled );
                REQUIRE( openclaw_harness_semantic_surface_manager().submit_request( { run, d.surface_id, d.frame_id, "ordinary-world-pause-" + std::to_string( world_frames ),
                         action->id, action->stable_id, {} } ) );
                return;
            }
        } );
        world_manager.set_receipt_observer( [&]( const auto & receipt ) {
            world_receipts.push_back( receipt );
        } );
    }
    // Disposable curses fixture only: makes the pre-fix missing-menu control
    // terminate honestly, rather than hanging. Semantic dispatch takes priority.
    REQUIRE( ungetch( escaped ? 27 : 'm' ) != -1 );
    process_avatar_activity_turn( u ); // shared by pre-input and post-action loops
    flushinp();
    CHECK( active_semantic_surface_manager() == ( enclosing ? &manager : nullptr ) );
    CHECK( menus == 1 );
    if( choice == 1 ) {
        CHECK( u.activity.id() == activity_id( "ACT_WAIT_STAMINA" ) );
    } else {
        CHECK_FALSE( u.activity );
    }
    if( canceled ) {
        CHECK_FALSE( u.has_destination() );
        CHECK_FALSE( u.has_destination_activity() );
        CHECK_FALSE( u.is_auto_moving() );
        CHECK( u.get_next_auto_move_direction() == ACTION_NULL );
    } else {
        CHECK( u.has_destination() );
    }
    if( menus == 0 ) {
        return;    // Preserve failure without manufacturing a choice.
    }
    const size_t menu_receipts = escaped ? 1 : 2;
    REQUIRE( receipts.size() == menu_receipts );
    CHECK_FALSE( receipts.front().accepted );
    CHECK( receipts.front().request_id == "stale-close" );
    if( !escaped ) {
        CHECK( receipts.back().accepted );
        CHECK( receipts.back().request_id == "tired-choice" );
        CHECK( receipts.back().consuming_frame_id == selected.frame_id );
    }
    // A terminal uilist choice receipts on pop; Escape has no choice receipt.
    // Continuing choices publish the resulting blocking owner; explicit
    // cancellation leaves no activity or auto-move owner to publish here.
    handle_key_blocking_activity();
    REQUIRE( receipts.size() == menu_receipts );
    if( canceled ) {
        CHECK_FALSE( successor );
        CHECK_FALSE( u.has_destination() );
        CHECK( u.get_next_auto_move_direction() == ACTION_NULL );
    } else {
        REQUIRE( successor );
        CHECK( successor->kind == "activity_wait" );
        CHECK( successor->frame_id != selected.frame_id );
    }
    const auto activity_after = u.activity.id();
    const auto route_after = u.omt_path;
    if( canceled ) {
        const tripoint_abs_ms position_after_cancel = u.pos_abs();
        // Exercise the next real World input owner, without issuing another
        // travel command.  Pause is a native nonmoving action used only to
        // let the caller return after proving no route is consumed.
        u.set_moves( 100 );
        CHECK_FALSE( g->do_turn() );
        // The next genuine owner publication settles the deferred pause
        // receipt; a dequeue alone must not be counted as acceptance.
        u.set_moves( 100 );
        CHECK_FALSE( g->do_turn() );
        CHECK( world_frames == 2 );
        CHECK( menus == 1 );
        CHECK( u.pos_abs() == position_after_cancel );
        CHECK_FALSE( u.has_destination() );
        CHECK( u.get_next_auto_move_direction() == ACTION_NULL );
        CHECK( u.omt_path == route_after ); // Strategic route memory remains inert.
        CHECK( std::count_if( world_receipts.begin(), world_receipts.end(), []( const auto & r ) {
            return r.accepted && r.request_id == "ordinary-world-pause-1";
        } ) == 1 );
        REQUIRE( receipts.size() == menu_receipts );
        CHECK( active_semantic_surface_manager() == ( enclosing ? &manager : nullptr ) );
    }
    if( !escaped ) {
        CHECK_FALSE( manager.submit_request( selected ) );
        CHECK( receipts.size() == menu_receipts + 1 );
        CHECK( receipts.back().request_id == "tired-choice" ); // cached, no second native choice
    }
    CHECK( u.activity.id() == activity_after );
    CHECK( u.omt_path == route_after );
    CHECK( menus == 1 );
}

TEST_CASE( "avatar activity driver leaves harness-off input unbound",
           "[semantic_surface][activity_driver_lifetime_067]" )
{
    clear_avatar();
    clear_map();
    avatar &u = get_avatar();
    u.setpos( get_map(), tripoint_bub_ms( 60, 60, 0 ), false );
    u.set_stamina( u.get_stamina_max() / 4 );
    u.set_moves( 100 );
    u.omt_path = { u.pos_abs_omt() + point::east, u.pos_abs_omt() };
    u.assign_activity( player_activity( activity_id( "ACT_TRAVELLING" ) ) );
    const std::vector<std::string> keys = { "OPENCLAW_HARNESS_RUN_ID", "OPENCLAW_HARNESS_SEMANTIC_RUN_ID" };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &key : keys ) {
        const char *value = std::getenv( key.c_str() );
        previous[key] = value ? std::optional<std::string>( value ) : std::nullopt;
        unset_test_environment( key.c_str() );
    }
    on_out_of_scope restore( [&]() {
        for( const auto &entry : previous ) {
            if( entry.second ) {
                set_test_environment( entry.first.c_str(), entry.second->c_str(), 1 );
            }
        }
        u.cancel_activity();
        u.clear_destination();
        u.omt_path.clear();
    } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&]() {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    ui_adaptor fixture_root;
    fixture_root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw();
    CHECK( active_semantic_surface_manager() == nullptr );
    REQUIRE( ungetch( 'm' ) != -1 );
    process_avatar_activity_turn( u );
    CHECK( active_semantic_surface_manager() == nullptr );
    CHECK_FALSE( u.activity );
    flushinp();
}
#endif

TEST_CASE( "NPC pulp selection reconciles ordinary and residual destination states",
           "[npc][pulp_destination_diagnosis_067]" )
{
    const std::string mode = GENERATE( "corpse", "empty", "reachable_destination",
                                       "blocked_destination",
                                       "danger", "high_backlog" );
    CAPTURE( mode );
    clear_avatar();
    clear_map();
    clear_npcs();
    set_time_to_day();
    avatar &u = get_avatar();
    u.setpos( get_map(), tripoint_bub_ms( 60, 60, 0 ), false );
    npc &actor = spawn_npc( { 61, 60 }, "test_talker" );
    clear_character( actor );
    actor.set_attitude( NPCATT_FOLLOW );
    actor.rules.set_flag( ally_rule::allow_pulp );
    actor.set_stamina( actor.get_stamina_max() );
    const auto corpse_pos = actor.pos_bub();
    actor.pulp_location = actor.pos_abs(); // ordinary remembered corpse target
    if( mode != "empty" ) {
        get_map().add_item_or_charges( corpse_pos, item::make_corpse( mtype_id( "mon_zombie" ) ) );
    }
    // Discover and execute through npc::move, not a direct activity/execute_action call.
    actor.set_moves( 100 );
    actor.move();
    CAPTURE( actor.activity.id().str(), actor.get_moves() );
    if( mode == "reachable_destination" || mode == "blocked_destination" ) {
        const auto destination = corpse_pos + point( 3, 0 );
        if( mode == "blocked_destination" ) {
            get_map().ter_set( destination, ter_id( "t_rock" ) );
        }
        actor.set_destination( { destination } );
        actor.update_path( destination );
        const bool viable = !actor.path.empty();
        std::cout << "pulp_destination mode=" << mode << " route_viable=" << viable << '\n';
        CHECK( viable == ( mode == "reachable_destination" ) );
    }
    if( mode == "danger" ) {
        spawn_test_monster( "mon_zombie", corpse_pos + point::south );
    }
    restore_on_out_of_scope<bool> restore_debug( debug_mode );
    debug_mode = true;
    int zero_move_calls = 0;
    const auto initial_turn = calendar::turn;
    for( int iteration = 0; iteration < 12; ++iteration ) {
        // A same-pass no-op retains its move budget and clock. Real expenditure
        // starts the next turn; this distinguishes pulping work from guard spins.
        if( iteration == 0 || actor.get_moves() < 100 ) {
            actor.set_moves( mode == "high_backlog" ? 6000 : 100 );
            calendar::turn += 1_turns;
        }
        const int before = actor.get_moves();
        const auto activity_before = actor.activity.id().str();
        const auto left_before = actor.activity.moves_left;
        Messages::clear_messages();
        actor.move();
        if( actor.get_moves() == before ) {
            ++zero_move_calls;
        }
        std::cout << "pulp_control mode=" << mode << " iteration=" << iteration
                  << " turn=" << to_turns<int>( calendar::turn - initial_turn )
                  << " moves=" << before << "->" << actor.get_moves()
                  << " activity=" << activity_before << "->" << actor.activity.id().str()
                  << " moves_left=" << left_before << "->" << actor.activity.moves_left
                  << " destination=" << actor.has_destination()
                  << " destination_activity=" << actor.has_destination_activity()
                  << " path_size=" << actor.path.size() << " can_pulp=" << actor.can_do_pulp() << '\n';
        for( const auto &message : Messages::recent_messages( 40 ) ) {
            if( message.second.find( "NPC didn't use its moves." ) != std::string::npos ||
                message.second.find( "address_needs " ) != std::string::npos ) {
                std::cout << "pulp_selection " << message.second << '\n';
            }
        }
    }
    CAPTURE( zero_move_calls, actor.activity.id().str(), actor.has_destination(),
             actor.get_ai_danger() );
    const bool residual_destination = mode == "blocked_destination" || mode == "reachable_destination";
    if( residual_destination ) {
        // Reproduces the existing defect; this passing mechanism control is
        // explicitly not a gameplay fix or historical Casey attribution.
        CHECK( zero_move_calls == 12 );
        CHECK( actor.activity.id() == activity_id( "ACT_PULP" ) );
        CHECK( actor.has_destination() );
    } else {
        CHECK( zero_move_calls < 9 );
    }
    const std::string warning = capture_debugmsg_during( [&]() {
        process_monsters_and_npcs_turn_for_test();
    } );
    std::cout << "pulp_guard mode=" << mode << " warning=" << warning
              << " suspended=" << actor.has_effect( efftype_id( "npc_suspend" ) ) << '\n';
    if( residual_destination ) {
        CHECK( actor.has_effect( efftype_id( "npc_suspend" ) ) );
        CHECK( warning.find( "entered infinite loop" ) != std::string::npos );
        CHECK( warning.find( "ACT_PULP" ) != std::string::npos );
    } else {
        CHECK_FALSE( actor.has_effect( efftype_id( "npc_suspend" ) ) );
        CHECK( warning.empty() );
    }
    clear_npcs();
}

#if !defined(TILES)
TEST_CASE( "World movement mode opens its native choices and restores ordinary input",
           "[semantic_surface][movement_mode_067]" )
{
    const std::string choice = GENERATE( "walk", "run", "crouch", "prone", "cycle", "cancel" );
    const bool enclosing = GENERATE( false, true );
    CAPTURE( choice, enclosing );
    override_option autosave( "AUTOSAVE", "false" );
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    clear_avatar();
    clear_map();
    clear_npcs();
    REQUIRE( world_generator->active_world );
    const auto world = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world ) );
    clear_avatar();
    clear_map();
    clear_npcs();
    avatar &u = get_avatar();
    u.setpos( get_map(), tripoint_bub_ms( 60, 60, 0 ), false );
    u.set_walk_mode_desired();
    const auto position = u.pos_abs();
    input_context unregistered( "movement-mode-unregistered" );
    const auto unavailable = openclaw_harness_world_actions( unregistered );
    CHECK( std::none_of( unavailable.begin(), unavailable.end(), []( const auto & a ) {
        return a.first == "world.movement_mode";
    } ) );
    const auto available = openclaw_harness_world_actions( get_default_mode_input_context() );
    CHECK( std::count_if( available.begin(), available.end(), []( const auto & a ) {
        return a.first == "world.movement_mode" && a.second == "open_movement";
    } ) == 1 );
    const std::vector<std::string> keys = { "OPENCLAW_HARNESS_RUN_ID",
                                            "OPENCLAW_HARNESS_SEMANTIC_RUN_ID",
                                            "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH"
                                          };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &key : keys ) {
        const char *value = std::getenv( key.c_str() );
        previous[key] = value ? std::optional<std::string>( value ) : std::nullopt;
    }
    const std::string run = "movement-mode-" +
                            std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() );
    set_test_environment( keys[0].c_str(), run.c_str(), 1 );
    set_test_environment( keys[1].c_str(), run.c_str(), 1 );
    unset_test_environment( keys[2].c_str() );
    on_out_of_scope restore_environment( [&]() {
        for( const auto &entry : previous ) {
            if( entry.second ) {
                set_test_environment( entry.first.c_str(), entry.second->c_str(), 1 );
            } else {
                unset_test_environment( entry.first.c_str() );
            }
        }
    } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&]() {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    ui_adaptor root;
    root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw();
    semantic_surface_manager &manager = openclaw_harness_semantic_surface_manager();
    std::optional<semantic_surface_manager_session> session;
    if( enclosing ) {
        session.emplace( manager );
    }
    std::vector<semantic_action_receipt> receipts;
    std::optional<semantic_surface_descriptor> original_world;
    semantic_action_request selected;
    int worlds = 0;
    int menus = 0;
    manager.set_receipt_observer( [&]( const auto & r ) {
        receipts.push_back( r );
    } );
    on_out_of_scope remove_observers( [&]() {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & d ) {
        if( d.kind == "world" ) {
            ++worlds;
            const std::string action_id = worlds == 1 ? "world.movement_mode" : "world.pause";
            const auto action = std::find_if( d.valid_actions.begin(), d.valid_actions.end(),
            [&]( const auto & a ) {
                return a.id == action_id;
            } );
            REQUIRE( action != d.valid_actions.end() );
            REQUIRE( action->enabled );
            if( worlds == 1 ) {
                original_world = d;
            } else {
                CHECK( d.surface_id != original_world->surface_id );
            }
            REQUIRE( manager.submit_request( { run, d.surface_id, d.frame_id,
                                               "mode-world-" + std::to_string( worlds ), action->id,
                                               action->stable_id, {} } ) );
        } else if( d.kind == "menu" ) {
            ++menus;
            CHECK( active_semantic_surface_manager() == &manager );
            CHECK( d.payload.at( "text" ) == "Change to which movement mode?" );
            REQUIRE( original_world );
            // Retired World authority cannot become another movement request
            // while the real native uilist owns input.
            REQUIRE( manager.submit_request( { run, original_world->surface_id,
                                               original_world->frame_id, "stale-mode-world",
                                               "world.movement_mode", "", {} } ) );
            for( const auto &mode : move_modes_by_speed() ) {
                const auto entry = std::find_if( d.valid_actions.begin(), d.valid_actions.end(),
                [&]( const auto & a ) {
                    return a.id == "menu.choose" && a.label.find( mode->name() ) == 0;
                } );
                REQUIRE( entry != d.valid_actions.end() );
                CHECK( entry->enabled == u.can_switch_to( mode ) );
            }
            const std::string label = choice == "cycle" ? "Cycle move mode" :
                                      choice == "cancel" ? "" : move_mode_id( choice )->name();
            const auto entry = std::find_if( d.valid_actions.begin(), d.valid_actions.end(),
            [&]( const auto & a ) {
                return choice == "cancel" ? a.id == "menu.cancel" :
                       a.id == "menu.choose" && a.label.find( label ) == 0;
            } );
            REQUIRE( entry != d.valid_actions.end() );
            REQUIRE( entry->enabled );
            selected = { run, d.surface_id, d.frame_id, "mode-choice", entry->id,
                         entry->stable_id, {}
                       };
            REQUIRE( manager.submit_request( selected ) );
        }
    } );
    u.set_moves( 100 );
    CHECK_FALSE( g->do_turn() );
    u.set_moves( 100 );
    CHECK_FALSE( g->do_turn() );
    REQUIRE( menus == 1 );
    CHECK( worlds >= 2 );
    CHECK( u.pos_abs() == position );
    CHECK_FALSE( u.activity );
    CHECK_FALSE( u.has_destination() );
    if( choice == "cycle" ) {
        CHECK( u.get_desired_move_mode() != move_mode_id( "walk" ) );
    } else {
        CHECK( u.get_desired_move_mode() == move_mode_id( choice == "cancel" ? "walk" : choice ) );
    }
    CHECK( u.current_movement_mode() == u.get_desired_move_mode() );
    CHECK( std::count_if( receipts.begin(), receipts.end(), []( const auto & r ) {
        return r.accepted && r.request_id == "mode-world-1";
    } ) == 1 );
    CHECK( std::count_if( receipts.begin(), receipts.end(), []( const auto & r ) {
        return r.accepted && r.request_id == "mode-choice";
    } ) == 1 );
    CHECK( std::count_if( receipts.begin(), receipts.end(), []( const auto & r ) {
        return !r.accepted && r.request_id == "stale-mode-world";
    } ) == 1 );
    CHECK( std::count_if( receipts.begin(), receipts.end(), []( const auto & r ) {
        return r.accepted && r.request_id == "mode-world-2";
    } ) == 1 );
    CHECK_FALSE( manager.submit_request( selected ) );
    CHECK( menus == 1 );
    CHECK( active_semantic_surface_manager() == ( enclosing ? &manager : nullptr ) );
    flushinp();
}
#endif

#if !defined(TILES)
TEST_CASE( "bodygraph Close owns the concrete modal across timeout and nested return",
           "[semantic_surface][bodygraph_owner_067]" )
{
    const std::string mode = GENERATE( "direct", "nested", "native_quit" );
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    clear_avatar();
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&]() {
        flushinp();
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "bodygraph-owner-control" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const auto & receipt ) {
        receipts.push_back( receipt );
    } );
    on_out_of_scope clear_observers( [&]() {
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
    } );
    semantic_surface_descriptor parent;
    semantic_surface_descriptor child;
    int parents = 0;
    int children = 0;
    const auto queue_key = [&]( const std::string & action ) {
        const auto &keys = inp_mngr.get_input_for_action( action, "BODYGRAPH" );
        const auto key = std::find_if( keys.begin(), keys.end(), []( const input_event & event ) {
            return event.type == input_event_t::keyboard_char && event.sequence.size() == 1 &&
                   event.get_first_input() >= 0 && event.get_first_input() < 128;
        } );
        REQUIRE( key != keys.end() );
        REQUIRE( ::ungetch( key->get_first_input() ) != -1 );
    };
    const auto close = [&]( const semantic_surface_descriptor & owner,
    const std::string & request_id ) {
        REQUIRE( manager.submit_request( { manager.run_id(), owner.surface_id, owner.frame_id,
                                           request_id, "bodygraph.close", std::nullopt, {} } ) );
    };
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & descriptor ) {
        if( descriptor.kind != "bodygraph" ) {
            return;
        }
        CHECK( descriptor.payload.at( "native_owner" ) == "BODYGRAPH" );
        CHECK( descriptor.payload.at( "character_id" ) ==
               std::to_string( get_avatar().getID().get_value() ) );
        if( descriptor.payload.at( "bodygraph_id" ) == "full_body" ) {
            ++parents;
            if( parents == 1 ) {
                parent = descriptor;
                input_context idle( "BODYGRAPH" );
                idle.register_action( "ANY_INPUT" );
                CHECK( idle.handle_input( 1 ) == "TIMEOUT" );
                REQUIRE( manager.top() );
                CHECK( manager.top()->surface_id == parent.surface_id );
                CHECK( manager.top()->frame_id == parent.frame_id );
                if( mode == "direct" ) {
                    close( descriptor, "parent-close" );
                } else {
                    queue_key( "RIGHT" );
                }
            } else {
                CHECK( descriptor.surface_id == parent.surface_id );
                CHECK( descriptor.frame_id != parent.frame_id );
                close( child, "closed-child" );
                close( parent, "stale-parent" );
                close( descriptor, "parent-close" );
            }
        } else {
            ++children;
            child = descriptor;
            CHECK( child.surface_id != parent.surface_id );
            close( parent, "covered-parent" );
            if( mode == "native_quit" ) {
                queue_key( "QUIT" );
            } else {
                close( descriptor, "child-close" );
            }
        }
    } );
    display_bodygraph( get_avatar() );
    CHECK( parents == ( mode == "direct" ? 1 : 2 ) );
    CHECK( children == ( mode == "direct" ? 0 : 1 ) );
    CHECK_FALSE( manager.top() );
    const auto receipt_for = [&]( const std::string & request_id ) {
        return std::find_if( receipts.begin(), receipts.end(), [&]( const auto & receipt ) {
            return receipt.request_id == request_id;
        } );
    };
    const auto accepted = receipt_for( "parent-close" );
    REQUIRE( accepted != receipts.end() );
    CHECK( accepted->accepted );
    if( mode != "direct" ) {
        for( const std::string request_id : {
                 "covered-parent", "closed-child", "stale-parent"
             } ) {
            const auto rejected = receipt_for( request_id );
            REQUIRE( rejected != receipts.end() );
            CHECK_FALSE( rejected->accepted );
        }
        if( mode == "nested" ) {
            const auto closed = receipt_for( "child-close" );
            REQUIRE( closed != receipts.end() );
            CHECK( closed->accepted );
        }
    }
}
#endif

#if !defined(TILES)
TEST_CASE( "turn driven dialogue input is excluded without replaying its effect",
           "[.manual][semantic_surface][performance_dialogue_input]" )
{
    override_option diagnostic( "RAID_PERFORMANCE_DIAGNOSTIC", "normal" );
    override_option autosave( "AUTOSAVE", "false" );
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    clear_avatar();
    clear_map();
    clear_npcs();
    REQUIRE( world_generator->active_world );
    const auto world = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world ) );
    clear_avatar();
    clear_map();
    clear_npcs();
    avatar &u = get_avatar();
    u.setpos( get_map(), tripoint_bub_ms( 60, 60, 0 ), false );
    const auto root_path = std::filesystem::path( PATH_INFO::user_dir() );
    const auto trace = root_path / "dialogue-timing-control.jsonl";
    const auto requests = root_path / "dialogue-timing-requests.jsonl";
    std::filesystem::remove( trace );
    std::filesystem::remove( requests );
    const std::string run = "dialogue-timing-control";
    const std::map<std::string, std::optional<std::string>> settings = {
        { "OPENCLAW_HARNESS_RUN_ID", run }, { "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", trace.string() },
        { "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", requests.string() }
    };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &entry : settings ) {
        const char *value = std::getenv( entry.first.c_str() );
        previous[entry.first] = value ? std::optional<std::string>( value ) : std::nullopt;
        assign_test_environment( entry.first, entry.second );
    }
    on_out_of_scope restore_environment( [&] {
        for( const auto &entry : previous ) {
            assign_test_environment( entry.first, entry.second );
        }
        std::filesystem::remove( requests );
    } );
    const std::string previous_locale = std::setlocale( LC_CTYPE, nullptr );
    REQUIRE( std::setlocale( LC_CTYPE, "en_US.UTF-8" ) != nullptr );
    on_out_of_scope restore_locale( [&] { std::setlocale( LC_CTYPE, previous_locale.c_str() ); } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&] {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    ui_adaptor root;
    root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw();
    const auto pause = get_default_mode_input_context().first_keyboard_character_for_action( "pause" );
    REQUIRE( pause );
    REQUIRE( pause->size() == 1 );
    // This manual control may need a real terminal Pause at the initial World
    // prompt; the delayed dialogue response below is supplied by the test.
    REQUIRE( ungetch( pause->front() ) != -1 );
    u.set_moves( 100 );
    REQUIRE_FALSE( g->do_turn() );

    const JsonObject topic = json_loader::from_string( R"({"id":"TALK_TEST_NATIVE_TIMING_067",
        "dynamic_line":"Wait for an answer.","replace_built_in_responses":true,
        "responses":[{"text":"Bye.","topic":"TALK_DONE"}]})" );
    load_talk_topic( topic, "test" );
    const JsonObject source = json_loader::from_string( R"({"eoc":{
        "id":"EOC_TEST_NATIVE_TIMING_067","effect":[
        {"open_dialogue":{"topic":"TALK_TEST_NATIVE_TIMING_067"}},
        {"u_add_var":"native_timing_effect","value":"done"}]}})" );
    const auto eoc = effect_on_conditions::load_inline_eoc( source.get_member( "eoc" ), "test" );
    effect_on_conditions::queue_effect_on_condition( 0_turns, eoc, u, {} );
    const int previous_timeout = inp_mngr.get_timeout();
    inp_mngr.set_timeout( 250 );
    on_out_of_scope restore_timeout( [&] { inp_mngr.set_timeout( previous_timeout ); } );
    semantic_surface_manager &manager = openclaw_harness_semantic_surface_manager();
    std::thread writer;
    std::atomic<bool> wrote{ false };
    bool dialogue_seen = false;
    double dialogue_open_wall = 0;
    double dialogue_receipt_wall = 0;
    bool pause_queued = false;
    int accepted = 0;
    on_out_of_scope finish( [&] {
        if( writer.joinable() ) { writer.join(); }
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
    } );
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        if( receipt.request_id == "dialogue-timing-close" && receipt.accepted ) {
            ++accepted;
            dialogue_receipt_wall = std::chrono::duration<double>(
                                        std::chrono::system_clock::now().time_since_epoch() ).count();
        }
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &owner ) {
        if( owner.kind == "dialogue" && !dialogue_seen ) {
            dialogue_seen = true;
            dialogue_open_wall = std::chrono::duration<double>(
                                     std::chrono::system_clock::now().time_since_epoch() ).count();
            const auto choice = std::find_if( owner.valid_actions.begin(), owner.valid_actions.end(),
            []( const semantic_action_descriptor &action ) { return action.id == "dialogue.choose" && action.enabled; } );
            REQUIRE( choice != owner.valid_actions.end() );
            const std::string stable = choice->stable_id;
            writer = std::thread( [&, owner, stable] {
                std::this_thread::sleep_for( std::chrono::milliseconds( 150 ) );
                std::ofstream out( requests, std::ios::app );
                out << "{\"run_id\":\"" << run << "\",\"surface_id\":\"" << owner.surface_id
                    << "\",\"frame_id\":\"" << owner.frame_id << "\",\"request_id\":\"dialogue-timing-close\","
                    << "\"action_id\":\"dialogue.choose\",\"stable_id\":\"" << stable << "\"}\n";
                out.close();
                wrote = static_cast<bool>( out );
            } );
        } else if( owner.kind == "world" && dialogue_seen && !pause_queued ) {
            pause_queued = true;
            REQUIRE( ungetch( pause->front() ) != -1 );
        }
    } );
    u.set_moves( 100 );
    REQUIRE_FALSE( g->do_turn() );
    REQUIRE( writer.joinable() );
    writer.join();
    CHECK( wrote );
    CHECK( dialogue_seen );
    CHECK( accepted == 1 );
    CHECK( u.get_value( "native_timing_effect" ) == "done" );
    double turn_start = 0, input_start = 0, input_end = 0, turn_end = 0, simulation = 0;
    double longest_input = 0;
    double dialogue_input_overlap = 0;
    std::ifstream input( trace );
    std::string line;
    while( std::getline( input, line ) ) {
        const auto start = line.find( '{' );
        if( start == std::string::npos || line.find( "\"event\":\"turn" ) == std::string::npos ) { continue; }
        std::istringstream text( line.substr( start ) );
        TextJsonIn jsin( text );
        const TextJsonObject event = jsin.get_object();
        event.allow_omitted_members();
        const auto kind = event.get_string( "event" );
        const auto stage = event.get_string( "stage" );
        const auto phase = event.get_string( "phase" );
        const double wall = event.get_float( "wall_time_seconds" );
        if( kind == "turn" && stage == "start" ) { turn_start = wall; input_start = input_end = 0; longest_input = 0; }
        if( kind == "turn_phase" && stage == "begin" && phase == "input" ) { input_start = wall; }
        if( kind == "turn_phase" && stage == "end" && input_start != 0 ) {
            input_end = wall;
            longest_input = std::max( longest_input, input_end - input_start );
            dialogue_input_overlap += std::max( 0.0, std::min( input_end, dialogue_receipt_wall ) -
                                               std::max( input_start, dialogue_open_wall ) );
            input_start = 0;
        }
        if( kind == "turn" && stage == "end" && longest_input >= .1 ) {
            turn_end = wall;
            simulation = event.get_float( "simulation_seconds" );
        }
    }
    CHECK( longest_input >= .1 );
    CHECK( dialogue_receipt_wall > dialogue_open_wall );
    CHECK( dialogue_input_overlap >= .1 );
    CHECK( turn_end > turn_start );
    CHECK( turn_end - turn_start - simulation >= .09 );
}
#endif

#if !defined(TILES)
TEST_CASE( "performance interval output preserves the real World successor",
           "[semantic_surface][raid_performance_output_route]" )
{
    const std::string mode = GENERATE( "normal", "quiet_signal", "off" );
    override_option diagnostic( "RAID_PERFORMANCE_DIAGNOSTIC", mode );
    override_option autosave( "AUTOSAVE", "false" );
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    clear_avatar();
    clear_map();
    clear_npcs();
    REQUIRE( world_generator->active_world );
    const auto world = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world ) );
    clear_avatar();
    clear_map();
    clear_npcs();
    avatar &u = get_avatar();
    u.setpos( get_map(), tripoint_bub_ms( 60, 60, 0 ), false );
    const auto position = u.pos_abs();

    // This actual native output is consumed by the Python strict-reader control
    // before its disposable test userdir is removed. No live-game input is used.
    const std::filesystem::path trace = std::filesystem::path( PATH_INFO::user_dir() ) /
                                        ( "raid-performance-route-" + mode + ".jsonl" );
    std::filesystem::remove( trace );
    const auto receipt_session = std::filesystem::path( PATH_INFO::user_dir() ) /
                                 ( "slow-receipt-config-" + mode );
    std::filesystem::create_directories( receipt_session );
    {
        std::ofstream config( receipt_session / "performance-turn-config.json" );
        config << R"({"schema":"caol-performance-turn-config-v1","provenance":"native control","window_turns":4,"tail_percentile":0.95,"spike_seconds":0.000000001,"slow_turn_seconds":0.000000001,"sustained_turns":2,"incomplete_turn_seconds":1,"stalled_observation_seconds":1,"expected_progress":true})";
    }
    const std::string run = "raid-performance-output-control-" + mode;
    const std::map<std::string, std::optional<std::string>> settings = {
        { "OPENCLAW_HARNESS_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", trace.string() },
        { "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", std::nullopt },
        { "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR", receipt_session.string() },
        { "OPENCLAW_HARNESS_TURN_RESOURCES", "1" }
    };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &entry : settings ) {
        const char *value = std::getenv( entry.first.c_str() );
        previous[entry.first] = value ? std::optional<std::string>( value ) : std::nullopt;
        assign_test_environment( entry.first, entry.second );
    }
    on_out_of_scope restore_environment( [&] {
        for( const auto &entry : previous ) {
            assign_test_environment( entry.first, entry.second );
        }
    } );
    const std::string previous_locale = std::setlocale( LC_CTYPE, nullptr );
    REQUIRE( std::setlocale( LC_CTYPE, "en_US.UTF-8" ) != nullptr );
    on_out_of_scope restore_locale( [&] { std::setlocale( LC_CTYPE, previous_locale.c_str() ); } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&] {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    deinitDebug();
    setupDebug( DebugOutput::file );
    on_out_of_scope restore_debug_sink( [] {
        deinitDebug();
        setupDebug( DebugOutput::std_err );
    } );
    ui_adaptor root;
    root.on_redraw( []( ui_adaptor & ) {
        DebugLog( D_INFO, D_MAIN ) << "bounded sink timing control";
    } );
    ui_manager::redraw();
    // Keep the production descriptor/receipt writers installed. Native input
    // uses its currently configured Pause key, never a synthesized descriptor.
    const auto pause = get_default_mode_input_context().first_keyboard_character_for_action( "pause" );
    REQUIRE( pause );
    REQUIRE( pause->size() == 1 );
    const int old_scan = overmap_buffer.global_state.migo_towers.last_signal_scan;
    on_out_of_scope restore_scan( [&] { overmap_buffer.global_state.migo_towers.last_signal_scan = old_scan; } );
    overmap_buffer.global_state.migo_towers.last_signal_scan = -1;
    reset_live_light_sample_cache();
    for( int turn = 0; turn < 4; ++turn ) {
        assign_test_environment( "OPENCLAW_HARNESS_TURN_RESOURCES", turn == 1 ? "0" : "1" );
        root.invalidate_ui();
        REQUIRE( ungetch( pause->front() ) != -1 );
        u.set_moves( 100 );
        const auto before = calendar::turn;
        CHECK_FALSE( g->do_turn() );
        // Reentry exposes the saved turn; later turns advance normally.
        CHECK( calendar::turn == before + ( turn == 0 ? 0_turns : 1_turns ) );
        CHECK( u.pos_abs() == position );
    }
    flush_native_slow_turn_receipts();
    // Disabling a temporary diagnostic discards its still-open final row,
    // rather than publishing old data under mode off or leaking into later tests.
    int disabled_game_turn = 0;
    {
        override_option disabled( "RAID_PERFORMANCE_DIAGNOSTIC", "off" );
        REQUIRE( ungetch( pause->front() ) != -1 );
        u.set_moves( 100 );
        const auto before = calendar::turn;
        CHECK_FALSE( g->do_turn() );
        CHECK( calendar::turn == before + 1_turns );
        disabled_game_turn = to_turns<int>( calendar::turn - calendar::turn_zero );
        CHECK( u.pos_abs() == position );
    }

    int extrema_snapshots = 0;
    int slow_snapshots = 0;
    bool observed_slow_record = false;
    int highest_slow_count = 0;
    int intervals = 0;
    int receiver_scans = 0;
    int journal_calls = 0;
    int sink_write_calls = 0;
    int sink_flush_calls = 0;
    int sink_enabled_rows = 0;
    int sink_disabled_rows = 0;
    int redraw_attempts = 0;
    int npc_resource_attempts = 0;
    int item_resource_attempts = 0;
    std::ifstream input( trace );
    std::string line;
    while( std::getline( input, line ) ) {
        if( line.find( "native_slow_turn_receipts" ) != std::string::npos ) {
            std::istringstream record( line );
            TextJsonIn jsin( record );
            const TextJsonObject object = jsin.get_object();
            object.allow_omitted_members();
            CHECK( object.get_string( "schema" ) == "caol-native-slow-turn-receipts-v1" );
            CHECK( object.get_string( "run_id" ) == run );
            CHECK( line.size() <= 32768 );
            const auto records = object.get_array( "records" );
            observed_slow_record = observed_slow_record || records.size() > 0;
            for( const TextJsonObject &item : records ) {
                item.allow_omitted_members();
                highest_slow_count = std::max( highest_slow_count, item.get_int( "count" ) );
            }
            ++slow_snapshots;
        }
        if( line.find( "native_performance_extrema" ) != std::string::npos ) {
            std::istringstream record( line );
            TextJsonIn jsin( record );
            const TextJsonObject object = jsin.get_object();
            object.allow_omitted_members();
            CHECK( object.get_string( "schema" ) == "caol-native-performance-extrema-v1" );
            CHECK( object.get_string( "run_id" ) == run );
            REQUIRE( object.has_object( "individual_max" ) );
            if( mode == "off" ) {
                const auto sample = object.get_object( "individual_max" );
                sample.allow_omitted_members();
                CHECK( sample.get_string( "metric_unavailable_reason" ) == "native_diagnostic_mode_off" );
            }
            if( object.has_object( "window_max" ) ) {
                const auto window = object.get_object( "window_max" );
                window.allow_omitted_members();
                CHECK( window.get_int( "sample_count" ) == 100 );
            }
            ++extrema_snapshots;
        }
        if( line.find( "raid_performance_interval" ) == std::string::npos ) {
            continue;
        }
        ++intervals;
        CHECK( line.rfind( "openclaw_harness_semantic_step: ", 0 ) != 0 );
        std::istringstream record( line.substr( line.find( '{' ) ) );
        TextJsonIn jsin( record );
        const TextJsonObject object = jsin.get_object();
        object.allow_omitted_members();
        CHECK( object.get_string( "mode" ) == mode );
        CHECK( object.get_string( "run_id" ) == run );
        const auto names = object.get_string_array( "stage_names" );
        for( const std::string name : {
                 "npc_site_bootstrap", "npc_signal_copy", "npc_channel_audit",
                 "camp_locker_downtime", "camp_locker_collection", "camp_locker_service",
                 "npc_find_item", "npc_pick_up_item", "npc_decision_trace", "npc_decision_publish",
                 "semantic_descriptor_build", "semantic_receipt_build", "native_journal_publish",
                 "native_journal_compact",
                 "pre_input_redraw", "render_present", "npc_process_turn", "npc_normal_move", "npc_body_update",
                 "debug_sink_write", "debug_sink_flush"
             } ) {
            CHECK( std::count( names.begin(), names.end(), name ) == 1 );
        }
        CHECK_FALSE( object.get_array( "rows" ).empty() );
        for( const TextJsonValue row_value : object.get_array( "rows" ) ) {
            const TextJsonObject row = row_value.get_object();
            row.allow_omitted_members();
            const bool sink_enabled = row.get_bool( "debug_sink_timing_enabled" );
            sink_enabled_rows += sink_enabled;
            sink_disabled_rows += !sink_enabled;
            CHECK( row.get_array( "stages_seconds" ).size() == names.size() );
            const auto stage_calls = row.get_array( "stage_calls" );
            const auto stage_index = [&]( const std::string & name ) {
                return std::distance( names.begin(), std::find( names.begin(), names.end(), name ) );
            };
            journal_calls += stage_calls.get_int( stage_index( "native_journal_publish" ) );
            const int writes = stage_calls.get_int( stage_index( "debug_sink_write" ) );
            const int flushes = stage_calls.get_int( stage_index( "debug_sink_flush" ) );
            sink_write_calls += writes;
            sink_flush_calls += flushes;
            if( !sink_enabled ) {
                CHECK( writes == 0 );
                CHECK( flushes == 0 );
            }
            for( const auto &window : {
                     std::make_pair( "npc_action_resources", &npc_resource_attempts ),
                     std::make_pair( "item_resources", &item_resource_attempts )
                 } ) {
                if( row.has_object( window.first ) ) {
                    const auto resources = row.get_object( window.first );
                    resources.allow_omitted_members();
                    CHECK( resources.get_string( "scope" ) ==
                           "process-wide deltas in native owner window; other threads may contribute; disk bytes are not wait time; samples are opt-in" );
                    const int attempts = resources.get_int( "attempts" );
                    *window.second += attempts;
                    CHECK( attempts > 0 );
                    CHECK( resources.get_int( "cpu_samples" ) <= attempts );
                    CHECK( resources.get_float( "probe_fence_wall_seconds" ) >= 0 );
#if defined(__APPLE__)
                    CHECK( resources.get_int( "resource_samples" ) <= attempts );
                    const int fault_samples = resources.get_int( "fault_samples" );
                    const int vm_samples = resources.get_int( "vm_samples" );
                    CHECK( fault_samples <= attempts );
                    CHECK( vm_samples <= attempts );
                    CHECK( resources.has_member( "faults" ) == ( fault_samples == attempts ) );
                    CHECK( resources.has_member( "decompressions" ) == ( vm_samples == attempts ) );
                    CHECK( resources.has_member( "swapin_ledger_delta" ) == ( vm_samples == attempts ) );
#endif
                }
            }
            if( row.has_object( "redraw_resources" ) ) {
                const auto resources = row.get_object( "redraw_resources" );
                resources.allow_omitted_members();
                CHECK( resources.get_string( "scope" ) ==
                       "process-wide deltas during pre-input redraw only; other threads may contribute; disk bytes are not wait time; samples are opt-in" );
                const int attempts = resources.get_int( "attempts" );
                redraw_attempts += attempts;
                CHECK( attempts > 0 );
                CHECK( resources.get_int( "cpu_samples" ) <= attempts );
                CHECK( resources.get_float( "probe_fence_wall_seconds" ) >= 0 );
#if defined(__APPLE__)
                CHECK( resources.get_int( "resource_samples" ) <= attempts );
                const int fault_samples = resources.get_int( "fault_samples" );
                const int vm_samples = resources.get_int( "vm_samples" );
                CHECK( fault_samples <= attempts );
                CHECK( vm_samples <= attempts );
                CHECK( resources.has_member( "faults" ) == ( fault_samples == attempts ) );
                CHECK( resources.has_member( "decompressions" ) == ( vm_samples == attempts ) );
                CHECK( resources.has_member( "swapin_ledger_delta" ) == ( vm_samples == attempts ) );
#endif
            }
            if( row.has_member( "migo_receiver_scan" ) ) {
                ++receiver_scans;
                const auto scan = row.get_object( "migo_receiver_scan" );
                scan.allow_omitted_members();
                CHECK( scan.get_bool( "entered" ) );
                CHECK( scan.get_bool( "result_available" ) );
                CHECK( scan.get_int( "turn" ) == row.get_int( "turn" ) );
                CHECK( scan.get_int( "byte_bound" ) <= 65536 );
                CHECK( scan.get_int( "retained_storage_bytes" ) <= 65536 );
                CHECK( scan.has_array( "records" ) );
            }
        }
    }
    const char *const slow_option = std::getenv( "OPENCLAW_HARNESS_SLOW_RECEIPTS" );
    const bool slow_disabled = slow_option && std::string( slow_option ) == "0";
    if( mode == "off" || slow_disabled ) {
        CHECK( slow_snapshots == 0 );
    } else {
        CHECK( slow_snapshots >= 1 );
        CHECK( observed_slow_record );
        CHECK( highest_slow_count >= 1 );
    }
    CHECK( ( mode == "off" ? intervals == 0 : intervals >= 1 ) );
    CHECK( ( mode == "off" ? receiver_scans == 0 : receiver_scans == 1 ) );
    CHECK( ( mode == "off" ? journal_calls == 0 : journal_calls > 0 ) );
    CHECK( ( mode == "off" ? sink_write_calls == 0 : sink_write_calls > 0 ) );
    CHECK( ( mode == "off" ? sink_flush_calls == 0 : sink_flush_calls > 0 ) );
    CHECK( ( mode == "off" ? sink_enabled_rows == 0 : sink_enabled_rows > 0 ) );
    CHECK( ( mode == "off" ? sink_disabled_rows == 0 : sink_disabled_rows > 0 ) );
    CHECK( ( mode == "off" ? redraw_attempts == 0 : redraw_attempts > 0 ) );
    CHECK( ( mode == "off" ? npc_resource_attempts == 0 : npc_resource_attempts > 0 ) );
    CHECK( ( mode == "off" ? item_resource_attempts == 0 : item_resource_attempts > 0 ) );
    CHECK( extrema_snapshots >= 1 );
    input.close();
    // Extend this same native process to a complete non-overlapping window.
    for( int turn = 0; turn < 98; ++turn ) {
        REQUIRE( ungetch( pause->front() ) != -1 );
        u.set_moves( 100 );
        CHECK_FALSE( g->do_turn() );
        CHECK( u.pos_abs() == position );
    }
    flush_native_slow_turn_receipts();
    // Check the ordinary intervals before rotation; only the extrema are pinned.
    const std::string filler( 96 * 1024, 'd' );
    for( int index = 0; index < 50; ++index ) {
        REQUIRE( write_native_diagnostic_event( "{\"event\":\"rotation_control\",\"run_id\":\"" +
                                               run + "\",\"data\":\"" + filler + "\"}" ) );
    }
    std::ifstream rotated( trace );
    int pinned = 0;
    int slow_pinned = 0;
    while( std::getline( rotated, line ) ) {
        if( line.find( "native_slow_turn_receipts" ) != std::string::npos ) {
            std::istringstream record( line );
            TextJsonIn jsin( record );
            const TextJsonObject object = jsin.get_object();
            object.allow_omitted_members();
            int count = 0;
            for( const TextJsonObject &item : object.get_array( "records" ) ) {
                item.allow_omitted_members();
                if( item.get_int( "alarm_kind" ) == 1 ) {
                    count += item.get_int( "count" );
                }
            }
            // Distinct dominant stages have separate buckets. If capacity is
            // exhausted, their evicted counts are reported as dropped.
            CHECK( count >= 1 );
            CHECK( count + object.get_int( "dropped_alarm_count" ) >= 100 );
            ++slow_pinned;
        }
        if( line.find( "native_performance_extrema" ) != std::string::npos ) {
            std::istringstream record( line );
            TextJsonIn jsin( record );
            const TextJsonObject object = jsin.get_object();
            object.allow_omitted_members();
            CHECK( object.get_string( "run_id" ) == run );
            REQUIRE( object.has_object( "window_max" ) );
            const auto window = object.get_object( "window_max" );
            window.allow_omitted_members();
            CHECK( window.get_int( "sample_count" ) == 100 );
            for( const TextJsonObject &sample : { object.get_object( "individual_max" ),
                                                  window.get_object( "dominant_turn" ) } ) {
                sample.allow_omitted_members();
                if( mode == "off" ) {
                    CHECK( sample.get_string( "detailed_diagnostic_unavailable_reason" ) ==
                           "native_diagnostic_unavailable" );
                } else if( !sample.has_object( "detailed_diagnostic" ) ) {
                    // The intentional single off-mode turn can itself be the
                    // maximum; its missing detail must remain explicit.
                    CHECK( sample.get_int( "game_turn" ) == disabled_game_turn );
                    CHECK( sample.get_string( "detailed_diagnostic_unavailable_reason" ) ==
                           "native_diagnostic_unavailable" );
                } else {
                    REQUIRE( sample.has_object( "detailed_diagnostic" ) );
                    const auto detail = sample.get_object( "detailed_diagnostic" );
                    detail.allow_omitted_members();
                    const auto names = detail.get_string_array( "stage_names" );
                    for( const std::string name : {
                             "camp_locker_downtime", "camp_locker_collection",
                             "camp_locker_service", "npc_find_item", "npc_pick_up_item",
                             "npc_decision_trace", "npc_decision_publish", "semantic_descriptor_build",
                             "semantic_receipt_build", "native_journal_publish", "native_journal_compact",
                             "pre_input_redraw", "render_present", "npc_process_turn", "npc_normal_move", "npc_body_update"
                         } ) {
                        CHECK( std::count( names.begin(), names.end(), name ) == 1 );
                    }
                    const auto row = detail.get_object( "row" );
                    row.allow_omitted_members();
                    CHECK( row.get_int( "turn" ) == sample.get_int( "game_turn" ) );
                    CHECK( row.get_array( "stages_seconds" ).size() == names.size() );
                    CHECK( row.get_array( "stage_calls" ).size() == names.size() );
                    CHECK( row.get_string( "migo_receiver_scan_unavailable_reason" ) ==
                           "bounded_extrema_does_not_retain_scan_records" );
                }
            }
            ++pinned;
        }
    }
    CHECK( pinned == 1 );
    CHECK( slow_pinned == ( mode == "off" || slow_disabled ? 0 : 1 ) );
}
#endif

#if !defined(TILES)
TEST_CASE( "watch diagnostics separate actual optical and recording owners without changing admission",
           "[semantic_surface][raid_performance_watch_split]" )
{
    const std::string mode = GENERATE( "normal", "quiet_signal", "off" );
    const std::string branch = GENERATE( "watch_record", "watch_summary", "watch_invalid_detail",
                                        "watch_probe", "watch_no_record", "nonwatch" );
    CAPTURE( mode, branch );
    override_option diagnostic( "RAID_PERFORMANCE_DIAGNOSTIC", mode );
    override_option autosave( "AUTOSAVE", "false" );
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    clear_avatar();
    clear_map();
    clear_npcs();
    REQUIRE( world_generator->active_world );
    const auto world = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world ) );
    clear_avatar();
    clear_map();
    clear_npcs();
    avatar &u = get_avatar();
    u.setpos( get_map(), tripoint_bub_ms( 60, 60, 0 ), false );
    const auto position = u.pos_abs();
    const auto saved_turn = calendar::turn;
    on_out_of_scope restore_turn( [&]() { set_time( saved_turn ); } );
    // Keep the real scheduler off its periodic cadence. This test explicitly
    // calls the real preflight from a World-publication hook to exercise the
    // current row, not to claim natural dispatch or native performance data.
    set_time( calendar::start_of_cataclysm + 1000_minutes + 7_seconds );
    const auto target = u.pos_abs_omt() + tripoint( 10, 0, 0 );
    std::vector<std::pair<tripoint_abs_omt, oter_id>> terrain_before;
    for( int y = -3; y <= 3; ++y ) {
        for( int x = -6; x <= 3; ++x ) {
            const auto p = target + tripoint( x, y, 0 );
            terrain_before.emplace_back( p, overmap_buffer.ter( p ) );
            // Watchers stand in the concealed three-OMT ring, while the
            // intervening footprint stays visible under the current terrain
            // sight budget. A solid forest corridor blocks every read before
            // the diagnostic path this control is intended to exercise.
            const bool watch_ring = std::max( std::abs( x ), std::abs( y ) ) == 3;
            overmap_buffer.ter_set( p, oter_str_id( watch_ring ? "forest" : "field" ).id() );
        }
    }
    on_out_of_scope restore_terrain( [&]() {
        for( const auto &tile : terrain_before ) {
            overmap_buffer.ter_set( tile.first, tile.second );
        }
    } );
    bandit_live_world::site_record site;
    site.site_id = "test:watch-performance-owner";
    site.anchor = target + tripoint( -5, 0, 0 );
    bandit_live_world::structural_outing_plan plan;
    plan.lead_id = "structural_bounty:watch-performance";
    plan.target_omt = target;
    plan.job = branch == "nonwatch" ? bandit_dry_run::job_template::scavenge :
               bandit_dry_run::job_template::scout;
    bandit_live_world::camp_map_lead lead;
    lead.lead_id = plan.lead_id;
    lead.kind = bandit_live_world::camp_lead_kind::structural_bounty;
    lead.omt = target;
    site.intelligence_map.leads.push_back( lead );
    const auto site_bytes = [&]() {
        bandit_live_world::world_state fixture;
        fixture.sites.push_back( site );
        std::ostringstream out;
        JsonOut json( out );
        fixture.serialize( json );
        return out.str();
    };
    const auto before_site = site_bytes();
    int baseline_budget = 8;
    const auto baseline = live_bandit_structural_route_read_for_test( site, plan, baseline_budget );

    const std::string run = "watch-split-" + mode + "-" + branch;
    const auto trace = std::filesystem::path( PATH_INFO::user_dir() ) / ( run + ".jsonl" );
    const auto transitions = std::filesystem::path( PATH_INFO::user_dir() ) /
                             ( run + ".transitions.jsonl" );
    std::filesystem::remove( trace );
    std::filesystem::remove( transitions );
    const std::map<std::string, std::optional<std::string>> settings = {
        { "OPENCLAW_HARNESS_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", trace.string() },
        { "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", std::nullopt },
        { "OPENCLAW_HARNESS_TRANSITION_EVENT_PATH", branch == "watch_record" || branch == "watch_summary" ||
          branch == "watch_invalid_detail" ?
          std::optional<std::string>( transitions.string() ) : std::nullopt },
        { "OPENCLAW_HARNESS_WATCH_GEOMETRY_DETAIL", branch == "watch_record" ?
          std::optional<std::string>( "1" ) : branch == "watch_invalid_detail" ?
          std::optional<std::string>( "0" ) : std::nullopt }
    };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &setting : settings ) {
        const char *old = std::getenv( setting.first.c_str() );
        previous[setting.first] = old ? std::optional<std::string>( old ) : std::nullopt;
        assign_test_environment( setting.first, setting.second );
    }
    on_out_of_scope restore_environment( [&]() {
        for( const auto &setting : previous ) {
            assign_test_environment( setting.first, setting.second );
        }
    } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&]() {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope restore_test_mode( test_mode );
    test_mode = false;
    ui_adaptor root;
    root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw();
    const auto pause = get_default_mode_input_context().first_keyboard_character_for_action( "pause" );
    REQUIRE( pause );
    REQUIRE( pause->size() == 1 );
    // Saved reentry's first turn does not advance and has no diagnostic row.
    // Arm the branch hook only after that ordinary first Pause has completed.
    REQUIRE( ungetch( pause->front() ) != -1 );
    u.set_moves( 100 );
    REQUIRE_FALSE( g->do_turn() );
    auto &manager = openclaw_harness_semantic_surface_manager();
    bool called = false;
    int probe_geometry_events = 0;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &owner ) {
        if( owner.kind != "world" || called ) {
            return;
        }
        called = true;
        const auto time_before = calendar::turn;
        const auto moves_before = u.get_moves();
        const auto trace_bytes_before = std::filesystem::file_size( trace );
        int budget = 8;
        std::unique_ptr<bandit_live_world_probe::session> detail_probe;
        if( branch == "watch_probe" ) {
            detail_probe = std::make_unique<bandit_live_world_probe::session>(
                               bandit_live_world_probe::collection_mode::transition_events );
        }
        const auto read = live_bandit_structural_route_read_for_test( site, plan, budget );
        if( detail_probe ) {
            for( const auto &event : detail_probe->result().transition_events ) {
                probe_geometry_events += event.transition == "watch_geometry_read";
            }
        }
        // Timing/counters stay in the current row. Only the unchanged native
        // transition recorder may write its separate, explicitly bound file.
        CHECK( std::filesystem::file_size( trace ) == trace_bytes_before );
        CHECK( read.summary == baseline.summary );
        CHECK( read.reachable == baseline.reachable );
        CHECK( read.complete_route_cost == baseline.complete_route_cost );
        CHECK( read.watch_geography_supplied == baseline.watch_geography_supplied );
        CHECK( read.watch_candidates.size() == baseline.watch_candidates.size() );
        CHECK( read.watch_shared_route == baseline.watch_shared_route );
        CHECK( budget == baseline_budget );
        CHECK( calendar::turn == time_before );
        CHECK( u.get_moves() == moves_before );
        CHECK( u.pos_abs() == position );
        CHECK( site_bytes() == before_site );
        CHECK( plan.member_ids.empty() );
    } );
    on_out_of_scope clear_observer( [&]() { manager.set_descriptor_observer( {} ); } );
    for( int i = 0; i < 3; ++i ) {
        REQUIRE( ungetch( pause->front() ) != -1 );
        u.set_moves( 100 );
        REQUIRE_FALSE( g->do_turn() );
    }
    REQUIRE( called );
    // Discard the open final row via the same supported off transition.
    {
        override_option off( "RAID_PERFORMANCE_DIAGNOSTIC", "off" );
        REQUIRE( ungetch( pause->front() ) != -1 );
        u.set_moves( 100 );
        REQUIRE_FALSE( g->do_turn() );
    }
    int intervals = 0;
    int preflights = 0;
    int reads = 0;
    int record_calls = 0;
    int eligible_calls = 0;
    int cached_views = 0;
    std::ifstream input( trace );
    std::string line;
    while( std::getline( input, line ) ) {
        if( line.find( "raid_performance_interval" ) == std::string::npos ) {
            continue;
        }
        ++intervals;
        const auto object = json_loader::from_string( line.substr( line.find( '{' ) ) ).get_object();
        object.allow_omitted_members();
        CHECK( object.get_string( "mode" ) == mode );
        CHECK( object.get_string( "stage_scope" ).find( "disjoint" ) != std::string::npos );
        const auto names = object.get_string_array( "stage_names" );
        const auto stage_index = [&]( const std::string &name ) {
            REQUIRE( std::count( names.begin(), names.end(), name ) == 1 );
            return std::distance( names.begin(), std::find( names.begin(), names.end(), name ) );
        };
        const auto parent = stage_index( "watch_preflight" );
        const auto optical = stage_index( "watch_optical_read" );
        const auto record = stage_index( "watch_record_view" );
        for( const auto value : object.get_array( "rows" ) ) {
            const auto row = value.get_object();
            row.allow_omitted_members();
            const auto stages = row.get_array( "stages_seconds" );
            REQUIRE( stages.size() == names.size() );
            CHECK( stages.get_float( parent ) >= stages.get_float( optical ) + stages.get_float( record ) );
            if( row.get_int( "watch_preflights" ) > 0 ) {
                CHECK( stages.get_float( parent ) > 0 );
                CHECK( stages.get_float( optical ) > 0 );
                CHECK( stages.get_float( record ) > 0 );
            } else {
                CHECK( stages.get_float( parent ) == 0 );
                CHECK( stages.get_float( optical ) == 0 );
                CHECK( stages.get_float( record ) == 0 );
            }
            preflights += row.get_int( "watch_preflights" );
            reads += row.get_int( "watch_optical_read_calls" );
            record_calls += row.get_int( "watch_record_view_calls" );
            eligible_calls += row.get_int( "watch_record_view_eligible_calls" );
            cached_views += row.get_int( "watch_cached_views" );
        }
    }
    if( mode == "off" ) {
        CHECK( intervals == 0 );
    } else {
        CHECK( intervals > 0 );
        CHECK( preflights == ( branch == "nonwatch" ? 0 : 1 ) );
        CHECK( ( reads > 0 ) == ( branch != "nonwatch" ) );
        CHECK( record_calls == reads );
        CHECK( eligible_calls == ( branch == "watch_record" || branch == "watch_probe" ? reads : 0 ) );
        CHECK( ( cached_views > 0 ) == ( branch != "nonwatch" ) );
    }
    int written_events = 0;
    std::ifstream transition_input( transitions );
    while( std::getline( transition_input, line ) ) {
        const auto event = json_loader::from_string( line ).get_object();
        event.allow_omitted_members();
        CHECK( event.get_string( "transition" ) == "watch_geometry_read" );
        CHECK( event.get_string( "run_id" ) == run );
        ++written_events;
    }
    CHECK( ( written_events > 0 ) == ( branch == "watch_record" ) );
    CHECK( ( probe_geometry_events > 0 ) == ( branch == "watch_probe" ) );
    if( mode != "off" ) {
        CHECK( written_events == ( branch == "watch_record" ? reads : 0 ) );
    }
    CHECK( u.pos_abs() == position );
    CHECK( site_bytes() == before_site );
}

TEST_CASE( "Mi-go diagnostics follow actual local and abstract route owners without extra work",
           "[semantic_surface][raid_performance_migo_split][raid_performance_navigation_inner]" )
{
    const std::string mode = GENERATE( "normal", "quiet_signal", "off" );
    const std::string branch = GENERATE( "local", "abstract", "unowned", "remote", "remote_unknown",
                                         "remote_shared", "remote_unknown_shared", "deferred_old_member",
                                         "deferred_new_member" );
    CAPTURE( mode, branch );
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_bandits = overmap_buffer.global_state.bandit_live_world;
    on_out_of_scope restore_world( [&]() {
        clear_creatures();
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.global_state.bandit_live_world = old_bandits;
    } );
    override_option diagnostic( "RAID_PERFORMANCE_DIAGNOSTIC", mode );
    override_option autosave( "AUTOSAVE", "false" );
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    clear_avatar();
    clear_map();
    clear_npcs();
    REQUIRE( world_generator->active_world );
    const auto world = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world ) );
    clear_avatar();
    clear_map();
    clear_npcs();
    avatar &u = get_avatar();
    u.setpos( get_map(), tripoint_bub_ms( 60, 60, 0 ), false );
    clear_creatures();
    overmap_buffer.global_state.migo_populations = {};
    overmap_buffer.global_state.migo_towers = {};
    overmap_buffer.global_state.bandit_live_world = {};
    u.set_mutation( trait_id( "DEBUG_CLOAK" ) );
    const auto position = u.pos_abs();
    const auto saved_turn = calendar::turn;
    on_out_of_scope restore_turn( [&]() { set_time( saved_turn ); } );
    // Exercise the actual owners under the real current-row/output lifetime.
    // Explicit leases and calls below are component setup, not a native journey
    // or a performance measurement of the live Windows scheduler.
    set_time( calendar::start_of_cataclysm + 1000_minutes + 7_seconds );

    const std::string run = "migo-split-" + mode + "-" + branch;
    const auto trace = std::filesystem::path( PATH_INFO::user_dir() ) / ( run + ".jsonl" );
    std::filesystem::remove( trace );
    const std::map<std::string, std::optional<std::string>> settings = {
        { "OPENCLAW_HARNESS_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", trace.string() },
        { "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", std::nullopt },
        { "OPENCLAW_HARNESS_TRANSITION_EVENT_PATH", std::nullopt }
    };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &setting : settings ) {
        const char *old_value = std::getenv( setting.first.c_str() );
        previous[setting.first] = old_value ? std::optional<std::string>( old_value ) : std::nullopt;
        assign_test_environment( setting.first, setting.second );
    }
    on_out_of_scope restore_environment( [&]() {
        for( const auto &setting : previous ) {
            assign_test_environment( setting.first, setting.second );
        }
    } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&]() {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope restore_test_mode( test_mode );
    test_mode = false;
    ui_adaptor root;
    root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw();
    const auto pause = get_default_mode_input_context().first_keyboard_character_for_action( "pause" );
    REQUIRE( pause );
    REQUIRE( pause->size() == 1 );
    // A real unbound reader must not leak counters into a later bound row.
    CHECK_FALSE( raid_perf_query_active() );
    monster unbound( mtype_id( "mon_mi_go" ) );
    unbound.set_pos_abs_only( position );
    const auto unbound_moves = unbound.get_moves();
    const auto unbound_turn = calendar::turn;
    physical_navigation::read_step( unbound, position + point::east );
    CHECK( unbound.pos_abs() == position );
    CHECK( unbound.get_moves() == unbound_moves );
    CHECK( calendar::turn == unbound_turn );
    // Saved reentry's first turn does not advance and has no diagnostic row.
    // Arm the branch hook only after that ordinary first Pause has completed.
    REQUIRE( ungetch( pause->front() ) != -1 );
    u.set_moves( 100 );
    REQUIRE_FALSE( g->do_turn() );
    auto &manager = openclaw_harness_semantic_surface_manager();

    bool called = false;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &surface ) {
        if( surface.kind != "world" || called ) {
            return;
        }
        called = true;
        map &here = get_map();
        const tripoint_bub_ms start( 64, 64, 0 );
        const bool shared = branch == "remote_shared" || branch == "remote_unknown_shared";
        const bool unknown = branch == "remote_unknown" || branch == "remote_unknown_shared";
        const bool remote = branch == "remote" || branch == "remote_unknown" || shared;
        const bool deferred = branch == "deferred_old_member" || branch == "deferred_new_member";
        const bool abstract = branch == "abstract" || remote || deferred;
        const auto anchor = project_to<coords::omt>( here.get_abs( start ) ) +
                            ( remote ? point( 12, unknown ? 12 : 0 ) : point::zero );
        const auto home = remote ? project_to<coords::ms>( anchor ) + point( 12, 12 ) :
                          here.get_abs( start );
        const auto goal = home + point::east * 4;
        const auto old_terrain = overmap_buffer.ter_existing( anchor );
        const tripoint_abs_omt upper_anchor( anchor.xy(), 1 );
        const auto old_upper_terrain = overmap_buffer.ter_existing( upper_anchor );
        std::optional<tinymap> remote_terrain;
        std::optional<tinymap> upper_terrain;
        if( remote ) {
            REQUIRE_FALSE( here.inbounds( home ) );
            REQUIRE_FALSE( here.inbounds( goal ) );
            if( unknown ) {
                REQUIRE_FALSE( MAPBUFFER.lookup_submap_existing( project_to<coords::sm>( home ) ) );
            } else {
                overmap_buffer.ter_set( anchor, oter_str_id( "field" ) );
                remote_terrain.emplace();
                remote_terrain->load( anchor, false );
                overmap_buffer.ter_set( upper_anchor, oter_str_id( "field" ) );
                upper_terrain.emplace();
                upper_terrain->load( upper_anchor, false );
                for( int z = 0; z <= 1; ++z ) {
                    tinymap &terrain = z == 0 ? *remote_terrain : *upper_terrain;
                    for( const auto &tile : terrain.points_in_rectangle( tripoint_omt_ms( 0, 0, z ),
                            tripoint_omt_ms( 23, 23, z ) ) ) {
                        terrain.ter_set( tile, ter_str_id( "t_floor" ) );
                        terrain.furn_set( tile, furn_str_id::NULL_ID() );
                    }
                }
            }
        } else {
            for( int dx = 0; dx <= 4; ++dx ) {
                here.ter_set( start + point( dx, 0 ), ter_str_id( "t_floor" ) );
            }
            here.invalidate_map_cache( 0 );
        }
        on_out_of_scope restore_geometry( [&]() {
            if( remote ) {
                overmap_buffer.ter_set( anchor, old_terrain );
                if( upper_terrain ) {
                    overmap_buffer.ter_set( upper_anchor, old_upper_terrain );
                }
                MAPBUFFER.clear_outside_reality_bubble();
            }
        } );
        const std::string id = "diagnostic-real-owner";
        migo_population::member_record member;
        member.token = {id, "member", 1, anchor};
        member.type = mtype_id( "mon_mi_go" );
        member.home = home;
        member.owner = abstract ? migo_population::representation::abstract :
                       migo_population::representation::local;
        member.assignment = migo_population::duty::dispatched;
        member.hp = member.type->hp;
        migo_population::site_record population;
        population.id = id;
        population.anchor = anchor;
        auto peer = member;
        peer.token.member_id = "peer";
        peer.type = mtype_id( "mon_mi_go_scout" );
        peer.hp = peer.type->hp;
        peer.home = home + point::south;
        std::vector<migo_population::member_record> seeds{member};
        if( shared ) {
            seeds.push_back( peer );
        }
        REQUIRE( population.commit_seed( seeds, std::nullopt, "", 0, true, false ) );
        overmap_buffer.global_state.migo_populations.sites.push_back( population );
        // The fixture inserts directly into the backing vector; invalidate the
        // production spatial index so this site participates in activation.
        overmap_buffer.global_state.migo_populations.invalidate_positions();
        migo_tower::site_state site;
        site.site_id = id;
        site.anchor = anchor;
        migo_tower::operation_record operation;
        operation.generation = 1;
        operation.source.source_omt = anchor;
        operation.members = {{member.token.member_id, false, false, home}};
        if( shared ) {
            operation.members.push_back( {peer.token.member_id, false, false, peer.home} );
        }
        operation.sweep.source = anchor;
        operation.sweep.generation = 1;
        operation.sweep.candidates = {{goal}};
        if( deferred ) {
            raid_sweep::candidate excluded;
            excluded.position = home + point::east * 2;
            excluded.state = raid_sweep::disposition::unreachable;
            if( branch == "deferred_old_member" ) {
                excluded.available_members_at_exclusion.push_back( member.token.member_id );
            }
            operation.sweep.candidates.insert( operation.sweep.candidates.begin(), excluded );
            operation.sweep.cursor = 1;
        }
        operation.overdue_after_active = 100000;
        operation.advance( to_turn<int>( calendar::turn ) - 1, 0 );
        site.operation = operation;
        overmap_buffer.global_state.migo_towers.sites.push_back( site );
        monster original( member.type );
        original.population_identity() = member.token;
        original.set_pos_abs_only( home );
        original.set_moves( 100 );
        migo_tower::member_intent intent;
        intent.generation = 1;
        intent.home = home;
        original.set_migo_intent( intent );
        overmap *retained_owner = nullptr;
        monster *body = nullptr;
        monster *second_body = nullptr;
        if( abstract ) {
            retained_owner = &overmap_buffer.get( project_to<coords::om>( home ).xy() );
            auto *entity = retained_owner->retain_monster_payload( home, original );
            REQUIRE( entity );
            body = entity->monster_data.get();
            if( shared ) {
                monster second( peer.type );
                second.population_identity() = peer.token;
                second.set_pos_abs_only( peer.home );
                second.set_moves( 100 );
                auto second_intent = intent;
                second_intent.home = peer.home;
                second.set_migo_intent( second_intent );
                auto *second_entity = retained_owner->retain_monster_payload( peer.home, second );
                REQUIRE( second_entity );
                second_body = second_entity->monster_data.get();
            }
        } else {
            body = g->place_critter_at( member.type, start );
            REQUIRE( body );
            body->population_identity() = member.token;
            body->set_migo_intent( intent );
            body->set_moves( 100 );
            if( branch == "unowned" ) {
                ++body->population_identity().epoch;
            }
        }
        on_out_of_scope clear_fixture( [&]() {
            if( retained_owner ) {
                CHECK( retained_owner->release_monster_payload( home, member.token.member_id ) );
                if( shared ) {
                    CHECK( retained_owner->release_monster_payload( peer.home, peer.token.member_id ) );
                }
            }
            clear_creatures();
            overmap_buffer.global_state.migo_populations = {};
            overmap_buffer.global_state.migo_towers = {};
        } );
        REQUIRE( body );
        if( branch != "unowned" ) {
            // This fixture creates its authenticated bodies after the world's
            // one-time position bootstrap. Publish their real positions as
            // the normal owner does, so range-gated service can find them.
            REQUIRE( overmap_buffer.global_state.migo_populations.confirm_position(
                         member.token, member.owner, body->pos_abs() ) );
            if( shared ) {
                REQUIRE( overmap_buffer.global_state.migo_populations.confirm_position(
                             peer.token, peer.owner, second_body->pos_abs() ) );
            }
        }
        const auto time_before = calendar::turn;
        const auto avatar_moves = u.get_moves();
        const auto trace_bytes = std::filesystem::file_size( trace );
        const auto hp = body->get_hp();
        const auto buffered_cells = [&]() {
            std::vector<tripoint_abs_sm> cells;
            for( const auto &entry : MAPBUFFER ) {
                cells.push_back( entry.first );
            }
            return cells;
        };
        const auto cells_before = buffered_cells();
        {
            // Same parent as the production do_turn service call; no synthetic
            // child scope or duplicate route is installed by this control.
            raid_perf_scope parent( raid_perf_stage::operations );
            migo_tower::service_operations( to_turn<int>( calendar::turn ), 1 );
        }
        CHECK( body->pos_abs() == home );
        CHECK( body->get_moves() == 100 );
        if( deferred ) {
            const auto &sweep = overmap_buffer.global_state.migo_towers.find( id )->operation->sweep;
            CHECK( sweep.candidates.front().reconsidered == ( branch == "deferred_new_member" ) );
            CHECK( sweep.cursor == ( branch == "deferred_new_member" ? 0 : 1 ) );
        }
        if( shared ) {
            REQUIRE( second_body );
            CHECK( second_body->pos_abs() == peer.home );
            CHECK( second_body->get_moves() == 100 );
            CHECK( second_body->get_hp() == peer.hp );
            CHECK( second_body->population_identity().epoch == peer.token.epoch );
            CHECK( second_body->migo_intent()->generation == intent.generation );
        }
        const auto globals = [&]() {
            std::ostringstream out;
            JsonOut json( out );
            json.start_array();
            overmap_buffer.global_state.migo_populations.serialize( json );
            overmap_buffer.global_state.migo_towers.serialize( json );
            json.end_array();
            return out.str();
        };
        auto expected_population = overmap_buffer.global_state.migo_populations;
        auto expected_towers = overmap_buffer.global_state.migo_towers;
        if( abstract ) {
            // Real retained payload and authenticated abstract prepare owner;
            // this control does not claim an abstract scheduler movement tick.
            raid_perf_scope parent( raid_perf_stage::horde_movement );
            const auto read = migo_tower::prepare_step( *body, member.owner );
            if( unknown ) {
                CHECK( read.next == physical_navigation::action::none );
                CHECK_FALSE( read.known );
                CHECK( read.reason == "origin_geometry_unknown" );
                CHECK( read.position == home );
            } else {
                CHECK( read.next == physical_navigation::action::walk );
                CHECK( read.position == home + point::east );
            }
            CHECK( body->pos_abs() == home );
            CHECK( body->get_moves() == 100 );
        } else {
            raid_perf_scope parent( raid_perf_stage::creatures );
            if( branch == "local" ) {
                body->plan();
            } else {
                CHECK_FALSE( migo_tower::plan_local( *body, false, nullptr ) );
            }
            CHECK( body->pos_abs() == home );
            CHECK( body->get_moves() == 100 );
            if( branch == "local" ) {
                body->move();
                // Native movement commits the authenticated member's new
                // position. Model that one intended publication explicitly;
                // all remaining serialized owner state must still match.
                REQUIRE( expected_population.confirm_position( member.token, member.owner,
                         body->pos_abs() ) );
                auto *expected_home = expected_population.find( id );
                auto *expected_site = expected_towers.find( id );
                REQUIRE( expected_home );
                REQUIRE( expected_site );
                REQUIRE( expected_site->operation );
                expected_home->activity.evaluate( to_turn<int>( calendar::turn ), true );
                expected_site->operation->activity.evaluate( to_turn<int>( calendar::turn ), true );
            } else {
                CHECK_FALSE( migo_tower::move_local( *body ) );
            }
            CHECK( body->pos_abs() == ( branch == "local" ? home + point::east : home ) );
            CHECK( body->get_moves() == ( branch == "local" ? 0 : 100 ) );
            if( branch == "local" ) {
                // A real committed step can trim the positive prefix. Every
                // stale ownership/lifetime key then requires a cold solve;
                // the immediately repeated authenticated read must reuse it.
                const auto committed = body->pos_abs();
                auto &navigation = *body->migo_intent();
                REQUIRE( migo_tower::prepare_step( *body, member.owner ).next ==
                         physical_navigation::action::walk );
                for( int key = 0; key < 8; ++key ) {
                    switch( key ) {
                        case 0:
                            navigation.navigation_identity.site_id = "old-site";
                            break;
                        case 1:
                            navigation.navigation_identity.member_id = "old-member";
                            break;
                        case 2:
                            ++navigation.navigation_identity.epoch;
                            break;
                        case 3:
                            navigation.navigation_identity.site_anchor =
                                navigation.navigation_identity.site_anchor + point::east;
                            break;
                        case 4:
                            ++navigation.navigation_generation;
                            break;
                        case 5:
                            navigation.navigation_phase = migo_tower::member_phase::returning;
                            break;
                        case 6:
                            navigation.navigation_owner = -1;
                            break;
                        case 7:
                            navigation.navigation_world -= 2;
                            break;
                    }
                    const auto first = migo_tower::prepare_step( *body, member.owner );
                    const auto repeated = migo_tower::prepare_step( *body, member.owner );
                    CHECK( first.next == physical_navigation::action::walk );
                    CHECK( repeated.position == first.position );
                    CHECK( navigation.navigation_identity.site_id == member.token.site_id );
                    CHECK( navigation.navigation_identity.member_id == member.token.member_id );
                    CHECK( navigation.navigation_identity.epoch == member.token.epoch );
                    CHECK( navigation.navigation_identity.site_anchor == member.token.site_anchor );
                    CHECK( navigation.navigation_generation == navigation.generation );
                    CHECK( navigation.navigation_phase == navigation.phase );
                    CHECK( navigation.navigation_owner == static_cast<int>( member.owner ) );
                    CHECK( navigation.navigation_world == to_turn<int>( calendar::turn ) );
                    CHECK( body->pos_abs() == committed );
                    CHECK( body->get_moves() == 0 );
                }
                // Real reader qualification, with each read conserving body
                // and clock. Setup changes affect a copy, never the owner.
                physical_navigation::segment hint;
                monster probe( *body );
                const auto check_read = [&]( monster &actor, const tripoint_abs_ms &target,
                                             const std::function<bool( const tripoint_abs_ms & )> &occupied = {} ) {
                    const auto pos = actor.pos_abs();
                    const auto moves = actor.get_moves();
                    const auto health = actor.get_hp();
                    const auto turn = calendar::turn;
                    const auto result = physical_navigation::read_step( actor, target, occupied, nullptr, &hint );
                    CHECK( actor.pos_abs() == pos );
                    CHECK( actor.get_moves() == moves );
                    CHECK( actor.get_hp() == health );
                    CHECK( calendar::turn == turn );
                    return result;
                };
                REQUIRE( check_read( probe, goal ).next == physical_navigation::action::walk );
                CHECK( check_read( probe, goal ).next == physical_navigation::action::walk );
                const auto alternate = goal + point::west;
                REQUIRE( check_read( probe, alternate ).next == physical_navigation::action::walk );
                CHECK( check_read( probe, alternate, [&]( const tripoint_abs_ms &p ) {
                    return p == alternate;
                } ).next == physical_navigation::action::none );
                REQUIRE( check_read( probe, alternate ).next == physical_navigation::action::walk );
                monster different( mtype_id( "mon_bear_mutant_3headed" ) );
                different.set_pos_abs_only( probe.pos_abs() );
                different.set_moves( 100 );
                REQUIRE( check_read( different, alternate ).next == physical_navigation::action::walk );
                different.set_pos_abs_only( home ); // neither cached origin nor its first point
                const auto relocated = check_read( different, alternate );
                REQUIRE( relocated.next == physical_navigation::action::walk );
                const auto changed = here.get_bub( relocated.position );
                const auto old = here.ter( changed );
                on_out_of_scope restore_terrain( [&]() { here.ter_set( changed, old ); } );
                here.ter_set( changed, ter_str_id( "t_dirtmound" ) );
                CHECK( check_read( different, alternate ).next == physical_navigation::action::walk );
                monster arrived( different );
                arrived.set_pos_abs_only( alternate );
                CHECK( physical_navigation::read_step( arrived, alternate ).reason == "goal_reached" );
                CHECK( arrived.pos_abs() == alternate );
                CHECK( arrived.get_moves() == 100 );
            }
        }
        if( remote ) {
            // Identical reads exercise the real remote wrapper, without
            // execution or a diagnostic-only cached route.
            std::ostringstream before_body;
            { JsonOut json( before_body ); body->serialize( json ); }
            const auto first = physical_navigation::read_step( *body, goal );
            const auto repeated = physical_navigation::read_step( *body, goal );
            CHECK( repeated.next == first.next );
            CHECK( repeated.position == first.position );
            CHECK( repeated.move_cost == first.move_cost );
            CHECK( repeated.reason == first.reason );
            CHECK( repeated.known == first.known );
            CHECK( repeated.hazard_revision == first.hazard_revision );
            if( remote_terrain ) {
                // Occupancy refuses the destination without movement. The
                // existing refusal-only geometry route remains diagnostic.
                const auto occupied = physical_navigation::read_step( *body, goal,
                [&]( const tripoint_abs_ms &p ) { return p == goal; } );
                CHECK( occupied.next == physical_navigation::action::none );
                CHECK( occupied.reason == "no_existing_safe_native_route" );
                map &native_map = *remote_terrain->cast_to_map();
                const auto native_goal = rebase_bub( remote_terrain->get_omt( goal ) );
                for( const bool neighbor : {false, true} ) {
                    CAPTURE( neighbor );
                    const auto tile = remote_terrain->get_omt( neighbor ? goal + point::south : goal );
                    REQUIRE( remote_terrain->add_field( tile, field_type_id( "fd_fire" ), 1 ) );
                    const auto *raw_fire = remote_terrain->field_at( tile ).find_field( field_type_id( "fd_fire" ) );
                    REQUIRE( raw_fire );
                    REQUIRE( raw_fire->get_field_intensity() == 1 );
                    REQUIRE( native_map.get_field_intensity( rebase_bub( tile ), field_type_id( "fd_fire" ) ) == 1 );
                    REQUIRE( body->will_move_to( &native_map, native_goal ) );
                    // Native danger checks inspect the field directly. The
                    // borrowed view must also honor its field-presence cache.
                    CHECK( body->know_danger_at( &native_map, native_goal ) == neighbor );
                    const auto fire = physical_navigation::read_step( *body, goal );
                    CHECK( fire.next == physical_navigation::action::none );
                    CHECK( fire.reason == "approach_fire_hazard" );
                    CHECK( fire.hazard_revision != 0 );
                    monster on_goal( *body );
                    on_goal.set_pos_abs_only( goal );
                    const auto at_goal = physical_navigation::read_step( on_goal, goal );
                    CHECK( at_goal.next == physical_navigation::action::none );
                    CHECK( at_goal.reason == "approach_fire_hazard" );
                    CHECK( at_goal.hazard_revision == raid_sweep::fire_revision( native_map, {native_goal} ) );
                    CHECK( raw_fire->get_field_intensity() == 1 );
                    remote_terrain->delete_field( tile, field_type_id( "fd_fire" ) );
                }
                const auto extinguished = physical_navigation::read_step( *body, goal );
                CHECK( extinguished.next == physical_navigation::action::walk );
                CHECK( extinguished.hazard_revision == 0 );
                const auto contained_tile = remote_terrain->get_omt( goal + point::south );
                remote_terrain->furn_set( contained_tile, furn_str_id( "f_brazier" ) );
                REQUIRE( remote_terrain->add_field( contained_tile, field_type_id( "fd_fire" ), 1 ) );
                const auto contained = physical_navigation::read_step( *body, goal );
                CHECK( contained.next == physical_navigation::action::walk );
                CHECK( contained.hazard_revision == 0 );
                CHECK( remote_terrain->field_at( contained_tile ).find_field( field_type_id( "fd_fire" ) ) );
                remote_terrain->delete_field( contained_tile, field_type_id( "fd_fire" ) );
                remote_terrain->furn_set( contained_tile, furn_str_id::NULL_ID() );
                // A real matching connection exercises the stair-route call
                // site; reaching the stairs still precedes any floor change.
                const auto stairs = home + point::east * 2;
                const auto upper_stairs = stairs + tripoint_rel_ms( 0, 0, 1 );
                remote_terrain->ter_set( remote_terrain->get_omt( stairs ), ter_str_id( "t_stairs_up" ) );
                upper_terrain->ter_set( upper_terrain->get_omt( upper_stairs ), ter_str_id( "t_stairs_down" ) );
                const auto stair = physical_navigation::read_step( *body, goal + tripoint_rel_ms( 0, 0, 1 ) );
                CHECK( stair.next == physical_navigation::action::walk );
                CHECK( stair.position == home + point::east );
                CHECK( stair.position.z() == home.z() );
                CHECK( remote_terrain->ter( remote_terrain->get_omt( stairs ) ) == ter_str_id( "t_stairs_up" ) );
                CHECK( upper_terrain->ter( upper_terrain->get_omt( upper_stairs ) ) == ter_str_id( "t_stairs_down" ) );
            }
            std::ostringstream after_body;
            { JsonOut json( after_body ); body->serialize( json ); }
            CHECK( after_body.str() == before_body.str() );
        }
        CHECK( buffered_cells() == cells_before ); // no new physical map generation
        std::ostringstream expected_globals;
        {
            JsonOut json( expected_globals );
            json.start_array();
            expected_population.serialize( json );
            expected_towers.serialize( json );
            json.end_array();
        }
        CHECK( globals() == expected_globals.str() );
        CHECK( body->get_hp() == hp );
        CHECK( body->population_identity().site_id == id );
        CHECK( body->population_identity().member_id == member.token.member_id );
        CHECK( body->population_identity().epoch == ( branch == "unowned" ? 2 : 1 ) );
        CHECK( calendar::turn == time_before );
        CHECK( u.get_moves() == avatar_moves );
        CHECK( u.pos_abs() == position );
        CHECK( std::filesystem::file_size( trace ) == trace_bytes );
    } );
    on_out_of_scope clear_observer( [&]() { manager.set_descriptor_observer( {} ); } );
    for( int i = 0; i < 3; ++i ) {
        REQUIRE( ungetch( pause->front() ) != -1 );
        u.set_moves( 100 );
        REQUIRE_FALSE( g->do_turn() );
    }
    REQUIRE( called );
    {
        override_option off( "RAID_PERFORMANCE_DIAGNOSTIC", "off" );
        REQUIRE( ungetch( pause->front() ) != -1 );
        u.set_moves( 100 );
        REQUIRE_FALSE( g->do_turn() );
    }
    const std::vector<std::string> stages = {
        "migo_service_local_navigation", "migo_service_abstract_navigation", "migo_local_plan",
        "migo_local_move", "migo_local_prepare_navigation", "migo_abstract_prepare_navigation",
        "physical_navigation_read", "physical_navigation_view_build", "physical_navigation_native_route"
    };
    const std::vector<int> expected = branch == "local" ? std::vector<int>{ 1, 0, 1, 1, 19, 0, 29, 0, 17 } :
                                      branch == "abstract" ? std::vector<int>{ 0, 1, 0, 0, 0, 1, 2, 0, 2 } :
                                      branch == "deferred_old_member" ? std::vector<int>{ 0, 2, 0, 0, 0, 1, 3, 0, 3 } :
                                      branch == "deferred_new_member" ? std::vector<int>{ 0, 2, 0, 0, 0, 1, 3, 0, 2 } :
                                      branch == "remote" ? std::vector<int>{ 0, 1, 0, 0, 0, 1, 12, 12, 10 } :
                                      branch == "remote_unknown" ? std::vector<int>{ 0, 1, 0, 0, 0, 1, 4, 4, 0 } :
                                      branch == "remote_shared" ? std::vector<int>{ 0, 2, 0, 0, 0, 1, 13, 12, 11 } :
                                      branch == "remote_unknown_shared" ? std::vector<int>{ 0, 2, 0, 0, 0, 1, 5, 4, 0 } :
                                      std::vector<int>( stages.size(), 0 );
    std::vector<int> totals( stages.size(), 0 );
    std::map<std::string, int> query_totals;
    int intervals = 0;
    int lookup_calls = 0;
    int level_calls = 0;
    std::ifstream input( trace );
    for( std::string line; std::getline( input, line ); ) {
        if( line.find( "raid_performance_interval" ) == std::string::npos ) {
            continue;
        }
        ++intervals;
        const auto object = json_loader::from_string( line.substr( line.find( '{' ) ) ).get_object();
        object.allow_omitted_members();
        CHECK( object.get_string( "mode" ) == mode );
        CHECK( object.get_string( "stage_scope" ).find(
                   "retained service-workspace destruction belongs to the inclusive migo_operations parent" ) !=
               std::string::npos );
        const auto names = object.get_string_array( "stage_names" );
        const auto index = [&]( const std::string &name ) {
            REQUIRE( std::count( names.begin(), names.end(), name ) == 1 );
            return std::distance( names.begin(), std::find( names.begin(), names.end(), name ) );
        };
        for( const auto value : object.get_array( "rows" ) ) {
            const auto row = value.get_object();
            row.allow_omitted_members();
            const auto times = row.get_array( "stages_seconds" );
            const auto calls = row.get_array( "stage_calls" );
            const auto counts = row.get_object( "nav_view_counts" );
            counts.allow_omitted_members();
            const auto queries = row.get_object( "nav_query_counts" );
            const std::vector<std::string> inputs = {"no_segment", "segment_empty", "segment_present"};
            const std::vector<std::string> outcomes = {"walk", "open_door", "goal_reached", "unknown_geometry",
                    "fire_refused", "no_route", "other_refusal"};
            int input_count = 0;
            int outcome_count = 0;
            for( const auto &name : inputs ) {
                input_count += queries.get_int( name );
            }
            for( const auto &name : outcomes ) {
                outcome_count += queries.get_int( name );
            }
            CHECK( input_count == calls.get_int( index( "physical_navigation_read" ) ) );
            CHECK( outcome_count == input_count );
            for( const JsonMember &member : queries ) {
                query_totals[member.name()] += member.get_int();
            }
            queries.allow_omitted_members();
            const int lookups = calls.get_int( index( "nav_view_lookup" ) );
            const int setups = calls.get_int( index( "nav_view_level_setup" ) );
            lookup_calls += lookups;
            level_calls += setups;
            CHECK( lookups == counts.get_int( "resident_hit" ) + counts.get_int( "resident_null" ) +
                   counts.get_int( "buffer_miss" ) );
            CHECK( calls.get_int( index( "nav_view_miss_load" ) ) == counts.get_int( "buffer_miss" ) );
            if( ( branch == "remote" || branch == "remote_unknown" ||
                  branch == "remote_shared" || branch == "remote_unknown_shared" ) &&
                counts.get_int( "buffer_miss" ) > 0 ) {
                // Distinct requested submaps in this real bounded view share
                // a serialized segment archive. Preparation may reuse that
                // handle, never infer known geometry from an absent quad.
                CHECK( counts.get_int( "archive_open_attempts" ) > 0 );
                CHECK( counts.get_int( "archive_open_attempts" ) < counts.get_int( "buffer_miss" ) );
            }
            CHECK( setups == counts.get_int( "level_fresh" ) + counts.get_int( "level_reused" ) );
            CHECK( times.get_float( index( "nav_view_lookup" ) ) >=
                   times.get_float( index( "nav_view_miss_load" ) ) );
            CHECK( times.get_float( index( "physical_navigation_view_build" ) ) >=
                   times.get_float( index( "nav_view_lookup" ) ) +
                   times.get_float( index( "nav_view_level_setup" ) ) );
            REQUIRE( times.size() == names.size() );
            REQUIRE( calls.size() == names.size() );
            for( std::size_t i = 0; i < stages.size(); ++i ) {
                const auto slot = index( stages[i] );
                totals[i] += calls.get_int( slot );
                CHECK( ( times.get_float( slot ) > 0 ) == ( calls.get_int( slot ) > 0 ) );
            }
            CHECK( times.get_float( index( "migo_operations" ) ) >=
                   times.get_float( index( "migo_service_local_navigation" ) ) +
                   times.get_float( index( "migo_service_abstract_navigation" ) ) );
            CHECK( times.get_float( index( "creatures" ) ) >=
                   times.get_float( index( "migo_local_plan" ) ) +
                   times.get_float( index( "migo_local_move" ) ) );
            CHECK( times.get_float( index( "migo_local_plan" ) ) +
                   times.get_float( index( "migo_local_move" ) ) >=
                   times.get_float( index( "migo_local_prepare_navigation" ) ) );
            CHECK( times.get_float( index( "horde_movement" ) ) >=
                   times.get_float( index( "migo_abstract_prepare_navigation" ) ) );
            CHECK( times.get_float( index( "physical_navigation_read" ) ) >=
                   times.get_float( index( "physical_navigation_view_build" ) ) +
                   times.get_float( index( "physical_navigation_native_route" ) ) );
        }
    }
    // The supported off transition discards the still-open final row.
    CHECK( ( intervals > 0 ) == ( mode != "off" ) );
    CHECK( totals == ( mode == "off" ? std::vector<int>( stages.size(), 0 ) : expected ) );
    if( mode == "off" ) {
        CHECK( query_totals.empty() );
    } else if( branch == "local" ) {
        CHECK( query_totals["no_segment"] == 1 );
        CHECK( query_totals["segment_empty"] == 12 );
        CHECK( query_totals["segment_present"] == 16 );
        CHECK( query_totals["context_empty_reset"] == 1 );
        CHECK( query_totals["context_positive_reset"] == 9 );
        CHECK( query_totals["segment_reused"] == 11 );
        CHECK( query_totals["segment_stored"] == 16 );
        CHECK( query_totals["goal_changed"] == 1 );
        CHECK( query_totals["capability_or_level_changed"] == 1 );
        CHECK( query_totals["origin_changed"] == 1 );
        CHECK( query_totals["cost_changed"] == 1 );
        CHECK( query_totals["validation_failed"] == 1 );
        CHECK( query_totals["walk"] == 27 );
        CHECK( query_totals["goal_reached"] == 1 );
        CHECK( query_totals["no_route"] == 1 );
        CHECK( query_totals["deferred_probe"] == 0 );
    } else if( branch == "deferred_old_member" || branch == "deferred_new_member" ) {
        CHECK( query_totals["deferred_probe"] == 1 );
        CHECK( query_totals["deferred_member_ineligible"] == ( branch == "deferred_old_member" ? 1 : 0 ) );
        CHECK( query_totals["goal_changed"] == ( branch == "deferred_old_member" ? 1 : 0 ) );
        CHECK( query_totals["segment_reused"] == ( branch == "deferred_new_member" ? 1 : 0 ) );
        CHECK( query_totals["walk"] == 3 );
        CHECK( query_totals["context_positive_reset"] == 1 );
    }

    CHECK( lookup_calls == ( mode == "off" ? 0 : ( branch == "remote" || branch == "remote_shared" ) ? 333 :
                            ( branch == "remote_unknown" || branch == "remote_unknown_shared" ) ? 108 : 0 ) );
    CHECK( level_calls == ( mode != "off" && ( branch == "remote" || branch == "remote_shared" ) ? 5 : 0 ) );
}

std::map<std::string, int> exercise_nav_lookup_diagnostics_for_test( bool reuse_archive );
TEST_CASE( "NAV lookup rows classify real serialized loader branches and restore attribution",
           "[semantic_surface][raid_performance_nav_lookup]" )
{
    const auto scenario = GENERATE( std::make_pair( std::string( "normal" ), true ),
                                   std::make_pair( std::string( "normal" ), false ),
                                   std::make_pair( std::string( "quiet_signal" ), true ),
                                   std::make_pair( std::string( "off" ), true ),
                                   std::make_pair( std::string( "unbound" ), true ),
                                   std::make_pair( std::string( "reader" ), true ) );
    const bool reuse_archive = scenario.first == "reader";
    const std::string mode = reuse_archive ? "normal" : scenario.first;
    CAPTURE( mode, scenario.second );
    override_option diagnostic( "RAID_PERFORMANCE_DIAGNOSTIC", mode == "unbound" ? "normal" : mode );
    override_option autosave( "AUTOSAVE", "false" );
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    clear_avatar();
    clear_map();
    clear_npcs();
    REQUIRE( world_generator->active_world );
    const auto world = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world ) );
    clear_avatar();
    clear_map();
    clear_npcs();
    clear_creatures();
    const auto old_populations = overmap_buffer.global_state.migo_populations;
    const auto old_towers = overmap_buffer.global_state.migo_towers;
    const auto old_bandits = overmap_buffer.global_state.bandit_live_world;
    overmap_buffer.global_state.migo_populations = {};
    overmap_buffer.global_state.migo_towers = {};
    overmap_buffer.global_state.bandit_live_world = {};
    on_out_of_scope restore_world( [&]() {
        overmap_buffer.global_state.migo_populations = old_populations;
        overmap_buffer.global_state.migo_towers = old_towers;
        overmap_buffer.global_state.bandit_live_world = old_bandits;
    } );
    avatar &u = get_avatar();
    u.setpos( get_map(), tripoint_bub_ms( 60, 60, 0 ), false );
    const auto saved_turn = calendar::turn;
    on_out_of_scope restore_turn( [&]() { set_time( saved_turn ); } );
    set_time( calendar::start_of_cataclysm + 1000_minutes + 7_seconds );
    const std::string run = "nav-lookup-" + scenario.first + ( scenario.second ? "-compressed" : "-plain" );
    const auto trace = std::filesystem::path( PATH_INFO::user_dir() ) / ( run + ".jsonl" );
    std::filesystem::remove( trace );
    const std::map<std::string, std::optional<std::string>> settings = {
        { "OPENCLAW_HARNESS_RUN_ID", mode == "unbound" ? std::nullopt : std::optional<std::string>( run ) },
        { "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", trace.string() },
        { "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", std::nullopt },
        { "OPENCLAW_HARNESS_TRANSITION_EVENT_PATH", std::nullopt }
    };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &setting : settings ) {
        const char *value = std::getenv( setting.first.c_str() );
        previous[setting.first] = value ? std::optional<std::string>( value ) : std::nullopt;
        assign_test_environment( setting.first, setting.second );
    }
    on_out_of_scope restore_environment( [&]() {
        for( const auto &setting : previous ) {
            assign_test_environment( setting.first, setting.second );
        }
    } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&]() { if( owns_terminal ) { catacurses::endwin(); } } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope restore_test_mode( test_mode );
    test_mode = false;
    ui_adaptor root;
    root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw();
    // Native compression conversion pumps and discards input. Complete this
    // setup before queuing a key, and restore before the UI owner is destroyed.
    const bool previous_compression = world_generator->active_world->has_compression_enabled();
    REQUIRE( world_generator->active_world->set_compression_enabled( scenario.second ) );
    on_out_of_scope restore_compression( [&]() {
        CHECK( world_generator->active_world->set_compression_enabled( previous_compression ) );
    } );
    const auto pause = get_default_mode_input_context().first_keyboard_character_for_action( "pause" );
    REQUIRE( pause );
    REQUIRE( pause->size() == 1 );
    const auto tick = [&]() {
        REQUIRE( ungetch( pause->front() ) != -1 );
        u.set_moves( 100 );
        REQUIRE_FALSE( g->do_turn() );
    };
    tick(); // ordinary post-load first turn has no advancing diagnostic row
    bool called = false;
    std::map<std::string, int> expected_counts;
    auto &manager = openclaw_harness_semantic_surface_manager();
    const auto exercise = [&]() {
        const auto time = calendar::turn;
        const auto position = u.pos_abs();
        const auto moves = u.get_moves();
        const auto bytes = std::filesystem::exists( trace ) ? std::filesystem::file_size( trace ) : 0;
        expected_counts = exercise_nav_lookup_diagnostics_for_test( reuse_archive );
        CHECK( calendar::turn == time );
        CHECK( u.pos_abs() == position );
        CHECK( u.get_moves() == moves );
        CHECK( ( std::filesystem::exists( trace ) ? std::filesystem::file_size( trace ) : 0 ) == bytes );
    };
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &surface ) {
        if( surface.kind == "world" && !called ) {
            called = true;
            exercise();
        }
    } );
    on_out_of_scope clear_observer( [&]() { manager.set_descriptor_observer( {} ); } );
    if( mode == "unbound" ) {
        // No run authority means no World descriptor callback. Exercise the
        // same actual owners directly with no row instead of inventing one.
        CHECK_FALSE( raid_perf_nav_build_active() );
        exercise();
        called = true;
    }
    for( int i = 0; i < 3; ++i ) {
        tick();
    }
    REQUIRE( called );
    { override_option off( "RAID_PERFORMANCE_DIAGNOSTIC", "off" ); tick(); }
    std::map<std::string, int> actual_counts;
    int lookups = 0;
    int misses = 0;
    int intervals = 0;
    std::ifstream input( trace );
    for( std::string line; std::getline( input, line ); ) {
        if( line.find( "raid_performance_interval" ) == std::string::npos ) {
            continue;
        }
        ++intervals;
        const auto object = json_loader::from_string( line.substr( line.find( '{' ) ) ).get_object();
        object.allow_omitted_members();
        const auto names = object.get_string_array( "stage_names" );
        const auto index = [&]( const std::string &name ) {
            REQUIRE( std::count( names.begin(), names.end(), name ) == 1 );
            return std::distance( names.begin(), std::find( names.begin(), names.end(), name ) );
        };
        for( const auto value : object.get_array( "rows" ) ) {
            const auto row = value.get_object();
            row.allow_omitted_members();
            const auto counts = row.get_object( "nav_view_counts" );
            counts.allow_omitted_members();
            for( const auto &entry : expected_counts ) {
                actual_counts[entry.first] += counts.get_int( entry.first );
            }
            const auto calls = row.get_array( "stage_calls" );
            lookups += calls.get_int( index( "nav_view_lookup" ) );
            misses += calls.get_int( index( "nav_view_miss_load" ) );
        }
    }
    const bool enabled = mode != "off" && mode != "unbound";
    CHECK( ( intervals > 0 ) == enabled );
    if( enabled ) {
        CHECK( actual_counts == expected_counts );
    } else {
        CHECK( actual_counts.empty() );
    }
    // Seven actual lookup calls plus one explicitly labelled open-failure
    // context probe above; only the real miss owner contributes six loads.
    CHECK( lookups == ( enabled ? 8 : 0 ) );
    CHECK( misses == ( enabled ? 6 : 0 ) );
}

#endif

#if !defined(TILES)
TEST_CASE( "native targeting input stays outside simulation including resumed aiming",
           "[semantic_surface][raid_performance_target_input]" )
{
    const bool nested = GENERATE( false, true );
    CAPTURE( nested );
    override_option diagnostic( "RAID_PERFORMANCE_DIAGNOSTIC", "quiet_signal" );
    override_option autosave( "AUTOSAVE", "false" );
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    clear_avatar();
    clear_map();
    clear_npcs();
    REQUIRE( world_generator->active_world );
    const std::string world = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world ) );
    clear_avatar();
    clear_map();
    clear_npcs();
    avatar &u = get_avatar();
    u.setpos( get_map(), tripoint_bub_ms( 60, 60, 0 ), false );
    REQUIRE( itype_id( "m9" ).is_valid() );
    item gun( itype_id( "m9" ) );
    REQUIRE( gun.is_gun() );
    gun.ammo_set( itype_id( "9mm" ), 5 );
    REQUIRE( u.wield( gun ) );
    const auto position = u.pos_abs();
    const auto run = std::string( "raid-target-input-" ) + ( nested ? "nested" : "resumed" );
    const std::filesystem::path trace = std::filesystem::path( PATH_INFO::user_dir() ) /
                                        ( run + ".jsonl" );
    const std::filesystem::path requests = std::filesystem::path( PATH_INFO::user_dir() ) /
                                           ( run + ".requests.jsonl" );
    std::filesystem::remove( trace );
    std::filesystem::remove( requests );
    const std::map<std::string, std::optional<std::string>> settings = {
        { "OPENCLAW_HARNESS_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", trace.string() },
        { "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", requests.string() }
    };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &entry : settings ) {
        const char *value = std::getenv( entry.first.c_str() );
        previous[entry.first] = value ? std::optional<std::string>( value ) : std::nullopt;
        assign_test_environment( entry.first, entry.second );
    }
    on_out_of_scope restore_environment( [&] {
        for( const auto &entry : previous ) {
            assign_test_environment( entry.first, entry.second );
        }
        std::filesystem::remove( requests );
    } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&] {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    ui_adaptor root;
    root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw();
    const auto pause = get_default_mode_input_context().first_keyboard_character_for_action( "pause" );
    REQUIRE( pause );
    REQUIRE( pause->size() == 1 );
    REQUIRE( ungetch( pause->front() ) != -1 );
    u.set_moves( 100 );
    REQUIRE_FALSE( g->do_turn() );

    semantic_surface_manager &manager = openclaw_harness_semantic_surface_manager();
    std::thread input_writer;
    std::atomic<bool> wrote_request{ false };
    on_out_of_scope finish_writer( [&] {
        if( input_writer.joinable() ) {
            input_writer.join();
        }
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
        manager.set_transport_observer( {} );
    } );
    bool target_consumed_in_input = false;
    manager.set_transport_observer( [&]( const semantic_request_transport_event &event ) {
        if( event.event != "request_consumed" || event.request_id != "target-close" ) {
            return;
        }
        // Inspect the actual producer at the instant native TARGET consumes
        // the delayed request. Resumed aiming had no input phase here before
        // the correction. This avoids assuming redraw between edge-scroll
        // polls is itself blocked input, or requiring an interactive CI stdin.
        std::string phase;
        std::ifstream events( trace );
        std::string line;
        while( std::getline( events, line ) ) {
            const auto begin = line.find( '{' );
            if( begin == std::string::npos ) {
                continue;
            }
            std::istringstream text( line.substr( begin ) );
            TextJsonIn jsin( text );
            const TextJsonObject value = jsin.get_object();
            value.allow_omitted_members();
            const auto kind = value.get_string( "event" );
            if( kind == "turn" && value.get_string( "stage" ) == "start" ) {
                phase = "simulation";
            } else if( kind == "turn_phase" ) {
                phase = value.get_string( "phase" );
            }
        }
        target_consumed_in_input = phase == "input";
    } );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    bool targeting_seen = false;
    bool parent_entered = false;
    bool pause_queued = false;
    double calculation_cpu = 0.0;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &owner ) {
        if( owner.kind == "target" && !targeting_seen ) {
            targeting_seen = true;
            // Real work before TARGET acquisition must remain simulation when
            // aiming is resumed outside handle_action, rather than masking the
            // whole activity. This controlled CPU work is NOT a latency benchmark.
            const auto cpu_begin = std::clock();
            volatile double calculation = 1.0;
            while( static_cast<double>( std::clock() - cpu_begin ) / CLOCKS_PER_SEC < 0.025 ) {
                calculation = calculation * 1.00000001 + 0.00000001;
            }
            calculation_cpu = static_cast<double>( std::clock() - cpu_begin ) / CLOCKS_PER_SEC;
            const auto descriptor = owner;
            input_writer = std::thread( [&, descriptor] {
                // A real delayed run-bound request reaches the input backend;
                // no callback manufactures an input event or bypasses TARGET.
                std::this_thread::sleep_for( std::chrono::milliseconds( 120 ) );
                const std::string record = "{\"run_id\":\"" + run + "\",\"surface_id\":\"" +
                                           descriptor.surface_id + "\",\"frame_id\":\"" + descriptor.frame_id +
                                           "\",\"request_id\":\"target-close\",\"action_id\":\"target.cancel\"}\n";
                std::ofstream output( requests, std::ios::app );
                output << record;
                output.close();
                wrote_request = static_cast<bool>( output );
            } );
        } else if( owner.kind == "world" && !pause_queued ) {
            if( nested && !parent_entered ) {
                parent_entered = true;
                pause_queued = true;
                aim_activity_actor aim = aim_activity_actor::use_wielded();
                CHECK( target_handler::mode_fire( u, aim ).empty() );
                REQUIRE( manager.top() );
                CHECK( manager.top()->kind == "world" );
                // Closing the nested acquisition must not close the parent's
                // existing input phase. This extra parent wait is still input.
                std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );
            }
            pause_queued = true;
            REQUIRE( ungetch( pause->front() ) != -1 );
        }
    } );
    if( !nested ) {
        u.assign_activity( aim_activity_actor::use_wielded() );
    }
    u.set_moves( 100 );
    const auto before = calendar::turn;
    REQUIRE_FALSE( g->do_turn() );
    REQUIRE( input_writer.joinable() );
    input_writer.join();
    CHECK( wrote_request );
    CHECK( targeting_seen );
    CHECK( target_consumed_in_input );
    CHECK( calendar::turn == before + 1_turns );
    CHECK( u.pos_abs() == position );
    REQUIRE( u.get_wielded_item() );
    CHECK( u.get_wielded_item()->ammo_remaining() == 5 );
    CHECK( std::any_of( receipts.begin(), receipts.end(), []( const auto &receipt ) {
        return receipt.request_id == "target-close" && receipt.accepted;
    } ) );
    // Flush the closed measured row through the production diagnostic writer.
    pause_queued = false;
    u.set_moves( 100 );
    REQUIRE_FALSE( g->do_turn() );

    int input_begins = 0;
    int input_ends = 0;
    bool measured_row = false;
    std::ifstream input( trace );
    std::string line;
    const int measured_turn = to_turn<int>( before + 1_turns );
    while( std::getline( input, line ) ) {
        const auto begin = line.find( '{' );
        if( begin == std::string::npos ) {
            continue;
        }
        std::istringstream record( line.substr( begin ) );
        TextJsonIn jsin( record );
        const TextJsonObject object = jsin.get_object();
        object.allow_omitted_members();
        if( object.get_string( "event" ) == "turn_phase" &&
            object.get_int( "game_turn" ) == measured_turn ) {
            input_begins += object.get_string( "stage" ) == "begin" &&
                            object.get_string( "phase" ) == "input";
            input_ends += object.get_string( "stage" ) == "end";
        }
        if( object.get_string( "event" ) == "raid_performance_interval" ) {
            for( const TextJsonValue row_value : object.get_array( "rows" ) ) {
                const TextJsonObject row = row_value.get_object();
                row.allow_omitted_members();
                if( row.get_int( "turn" ) == measured_turn ) {
                    measured_row = true;
                    CHECK( row.get_float( "input_seconds" ) > 0.0 );
                    if( nested ) {
                        CHECK( row.get_float( "input_seconds" ) >= 0.12 );
                    }
                    if( !nested ) {
                        CHECK( row.get_float( "process_cpu_seconds" ) >= calculation_cpu * 0.8 );
                        CHECK( row.get_float( "simulation_seconds" ) >= calculation_cpu * 0.8 );
                    }
                }
            }
        }
    }
    CHECK( measured_row );
    // Edge-scroll timeouts may reacquire TARGET input repeatedly. Every
    // acquisition is paired, while an enclosing World input remains one span.
    if( nested ) {
        CHECK( input_begins == 1 );
    } else {
        CHECK( input_begins >= 2 );
    }
    CHECK( input_ends == input_begins );
}
#endif

#if !defined(TILES)
TEST_CASE( "native distraction popup acquisition excludes only blocked input",
           "[semantic_surface][raid_performance_prompt_input]" )
{
    const bool nested = GENERATE( false, true );
    CAPTURE( nested );
    override_option diagnostic( "RAID_PERFORMANCE_DIAGNOSTIC", "quiet_signal" );
    override_option autosave( "AUTOSAVE", "false" );
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    clear_avatar();
    clear_map();
    clear_npcs();
    REQUIRE( world_generator->active_world );
    const std::string world = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world ) );
    clear_avatar();
    clear_map();
    clear_npcs();
    avatar &u = get_avatar();
    map &here = get_map();
    u.setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    const auto position = u.pos_abs();
    const auto run = std::string( "raid-popup-input-" ) + ( nested ? "nested" : "noise" );
    const std::filesystem::path trace = std::filesystem::path( PATH_INFO::user_dir() ) /
                                        ( run + ".jsonl" );
    const std::filesystem::path requests = std::filesystem::path( PATH_INFO::user_dir() ) /
                                           ( run + ".requests.jsonl" );
    std::filesystem::remove( trace );
    std::filesystem::remove( requests );
    const std::map<std::string, std::optional<std::string>> settings = {
        { "OPENCLAW_HARNESS_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", trace.string() },
        { "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", requests.string() }
    };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &entry : settings ) {
        const char *value = std::getenv( entry.first.c_str() );
        previous[entry.first] = value ? std::optional<std::string>( value ) : std::nullopt;
        assign_test_environment( entry.first, entry.second );
    }
    on_out_of_scope restore_environment( [&] {
        for( const auto &entry : previous ) {
            assign_test_environment( entry.first, entry.second );
        }
        std::filesystem::remove( requests );
        sounds::reset_sounds();
    } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&] {
        if( owns_terminal ) {
            catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    ui_adaptor root;
    root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw();
    const auto pause = get_default_mode_input_context().first_keyboard_character_for_action( "pause" );
    REQUIRE( pause );
    REQUIRE( pause->size() == 1 );
    REQUIRE( ungetch( pause->front() ) != -1 );
    u.set_moves( 100 );
    REQUIRE_FALSE( g->do_turn() );

    semantic_surface_manager &manager = openclaw_harness_semantic_surface_manager();
    std::thread input_writer;
    std::atomic<bool> wrote_request{ false };
    on_out_of_scope finish_writer( [&] {
        if( input_writer.joinable() ) {
            input_writer.join();
        }
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
        manager.set_transport_observer( {} );
    } );
    bool consumed_in_input = false;
    manager.set_transport_observer( [&]( const semantic_request_transport_event &event ) {
        if( event.event != "request_consumed" || event.request_id != "popup-ignore" ) {
            return;
        }
        std::string phase;
        std::ifstream events( trace );
        std::string line;
        while( std::getline( events, line ) ) {
            const auto begin = line.find( '{' );
            if( begin == std::string::npos ) {
                continue;
            }
            std::istringstream text( line.substr( begin ) );
            TextJsonIn jsin( text );
            const TextJsonObject value = jsin.get_object();
            value.allow_omitted_members();
            const auto kind = value.get_string( "event" );
            if( kind == "turn" && value.get_string( "stage" ) == "start" ) {
                phase = "simulation";
            } else if( kind == "turn_phase" ) {
                phase = value.get_string( "phase" );
            }
        }
        consumed_in_input = phase == "input";
    } );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    bool prompt_seen = false;
    bool parent_entered = false;
    bool pause_queued = false;
    double preparation_cpu = 0.0;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &owner ) {
        if( owner.kind == "prompt" && !prompt_seen ) {
            prompt_seen = true;
            REQUIRE( owner.payload.at( "distraction_type" ) == "noise" );
            const auto selected = std::find_if( owner.valid_actions.begin(), owner.valid_actions.end(),
            []( const semantic_action_descriptor &action ) {
                return action.id == "prompt.choose" && action.label == "IGNORE";
            } );
            REQUIRE( selected != owner.valid_actions.end() );
            REQUIRE_FALSE( selected->stable_id.empty() );
            // Real preparation before acquisition must remain measured. This
            // controlled CPU work is not a machine-speed benchmark.
            const auto cpu_begin = std::clock();
            volatile double calculation = 1.0;
            while( static_cast<double>( std::clock() - cpu_begin ) / CLOCKS_PER_SEC < 0.025 ) {
                calculation = calculation * 1.00000001 + 0.00000001;
            }
            preparation_cpu = static_cast<double>( std::clock() - cpu_begin ) / CLOCKS_PER_SEC;
            const auto descriptor = owner;
            const std::string stable_id = selected->stable_id;
            input_writer = std::thread( [&, descriptor, stable_id] {
                std::this_thread::sleep_for( std::chrono::milliseconds( 120 ) );
                const std::string record = "{\"run_id\":\"" + run + "\",\"surface_id\":\"" +
                                           descriptor.surface_id + "\",\"frame_id\":\"" + descriptor.frame_id +
                                           "\",\"request_id\":\"popup-ignore\",\"action_id\":\"prompt.choose\",\"stable_id\":\"" +
                                           stable_id + "\"}\n";
                std::ofstream output( requests, std::ios::app );
                output << record;
                output.close();
                wrote_request = static_cast<bool>( output );
            } );
        } else if( owner.kind == "world" && !pause_queued ) {
            if( nested && !parent_entered ) {
                parent_entered = true;
                pause_queued = true;
                u.assign_activity( wait_activity_actor( 30_minutes ) );
                CHECK_FALSE( g->cancel_activity_or_ignore_query( distraction_type::noise,
                             "Heard a test noise!" ) );
                // The inner acquisition must not close native World input.
                std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );
                u.cancel_activity();
            }
            pause_queued = true;
            REQUIRE( ungetch( pause->front() ) != -1 );
        }
    } );
    restore_on_out_of_scope<bool> restore_noise( uistate.distraction_noise );
    uistate.distraction_noise = true;
    if( !nested ) {
        here.ter_set( tripoint_bub_ms( 65, 60, 0 ), ter_id( "t_wall" ) );
        here.build_map_cache( 0, true );
        sounds::reset_sounds();
        sounds::sound( tripoint_bub_ms( 70, 60, 0 ), 60, sounds::sound_t::combat,
                       "test whump" );
        u.assign_activity( wait_activity_actor( 30_minutes ) );
        u.set_moves( 0 );
    } else {
        u.set_moves( 100 );
    }
    const auto before = calendar::turn;
    REQUIRE_FALSE( g->do_turn() );
    REQUIRE( input_writer.joinable() );
    input_writer.join();
    CHECK( wrote_request );
    CHECK( prompt_seen );
    CHECK( consumed_in_input );
    CHECK( calendar::turn == before + 1_turns );
    CHECK( u.pos_abs() == position );
    CHECK( std::count_if( receipts.begin(), receipts.end(), []( const auto &receipt ) {
        return receipt.request_id == "popup-ignore" && receipt.accepted;
    } ) == 1 );
    if( !nested ) {
        CHECK( u.activity.id() == activity_id( "ACT_WAIT" ) );
        CHECK( u.activity.is_distraction_ignored( distraction_type::noise ) );
        u.cancel_activity();
    }
    pause_queued = false;
    u.set_moves( 100 );
    REQUIRE_FALSE( g->do_turn() );
    int input_begins = 0;
    int input_ends = 0;
    bool measured_row = false;
    std::ifstream input( trace );
    std::string line;
    const int measured_turn = to_turn<int>( before + 1_turns );
    while( std::getline( input, line ) ) {
        const auto begin = line.find( '{' );
        if( begin == std::string::npos ) {
            continue;
        }
        std::istringstream record( line.substr( begin ) );
        TextJsonIn jsin( record );
        const TextJsonObject object = jsin.get_object();
        object.allow_omitted_members();
        if( object.get_string( "event" ) == "turn_phase" &&
            object.get_int( "game_turn" ) == measured_turn ) {
            input_begins += object.get_string( "stage" ) == "begin" &&
                            object.get_string( "phase" ) == "input";
            input_ends += object.get_string( "stage" ) == "end";
        }
        if( object.get_string( "event" ) == "raid_performance_interval" ) {
            for( const TextJsonValue value : object.get_array( "rows" ) ) {
                const TextJsonObject row = value.get_object();
                row.allow_omitted_members();
                if( row.get_int( "turn" ) == measured_turn ) {
                    measured_row = true;
                    // Publication/redraw precedes acquisition and stays
                    // measured. Only actual native waiting must be excluded.
                    CHECK( row.get_float( "input_seconds" ) > 0.0 );
                    if( nested ) {
                        CHECK( row.get_float( "input_seconds" ) >= 0.12 );
                    }
                    if( !nested ) {
                        CHECK( row.get_float( "process_cpu_seconds" ) >= preparation_cpu * 0.8 );
                        CHECK( row.get_float( "simulation_seconds" ) >= preparation_cpu * 0.8 );
                    }
                }
            }
        }
    }
    CHECK( measured_row );
    if( nested ) {
        CHECK( input_begins == 1 );
    } else {
        CHECK( input_begins >= 1 );
    }
    CHECK( input_ends == input_begins );
}
TEST_CASE( "native disposal and selector acquisition excludes only blocked input",
           "[semantic_surface][raid_performance_menu_input]" )
{
    const bool selector = GENERATE( false, true );
    const bool nested = GENERATE( false, true );
    const bool cancel = GENERATE( false, true );
    CAPTURE( selector, nested, cancel );
    override_option diagnostic( "RAID_PERFORMANCE_DIAGNOSTIC", "quiet_signal" );
    override_option autosave( "AUTOSAVE", "false" );
    override_option llm( "LLM_INTENT_ENABLE", "false" );
    clear_avatar();
    clear_map();
    clear_npcs();
    REQUIRE( world_generator->active_world );
    const std::string world = world_generator->active_world->world_name;
    REQUIRE( g->save() );
    REQUIRE( turn_handler::cleanup_at_end() );
    world_generator->set_active_world( nullptr );
    REQUIRE( g->load( world ) );
    clear_avatar();
    clear_map();
    clear_npcs();
    avatar &u = get_avatar();
    map &here = get_map();
    u.setpos( here, tripoint_bub_ms( 60, 60, 0 ), false );
    const auto position = u.pos_abs();
    const auto run = std::string( "raid-menu-input-" ) + ( selector ? "selector-" : "dispose-" ) +
                     ( nested ? "nested-" : "activity-" ) + ( cancel ? "cancel" : "commit" );
    const auto trace = std::filesystem::path( PATH_INFO::user_dir() ) / ( run + ".jsonl" );
    const auto requests = std::filesystem::path( PATH_INFO::user_dir() ) / ( run + ".requests.jsonl" );
    std::filesystem::remove( trace );
    std::filesystem::remove( requests );
    const std::map<std::string, std::optional<std::string>> settings = {
        { "OPENCLAW_HARNESS_RUN_ID", run }, { "OPENCLAW_HARNESS_SEMANTIC_RUN_ID", run },
        { "OPENCLAW_HARNESS_SEMANTIC_TRACE_PATH", trace.string() },
        { "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH", requests.string() }
    };
    std::map<std::string, std::optional<std::string>> previous;
    for( const auto &entry : settings ) {
        const char *value = std::getenv( entry.first.c_str() );
        previous[entry.first] = value ? std::optional<std::string>( value ) : std::nullopt;
        assign_test_environment( entry.first, entry.second );
    }
    on_out_of_scope restore_environment( [&] {
for( const auto &entry : previous )
    {
        assign_test_environment( entry.first, entry.second );
        }
        std::filesystem::remove( requests );
    } );
    const bool owns_terminal = !catacurses::stdscr;
    if( owns_terminal ) {
        catacurses::init_interface();
        catacurses::resizeterm();
    }
    on_out_of_scope close_terminal( [&] {
        if( owns_terminal )
    {
        catacurses::endwin();
        }
    } );
    scoped_imgui_context imgui;
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    ui_adaptor root;
    root.on_redraw( []( ui_adaptor & ) {} );
    ui_manager::redraw();
    const auto pause = get_default_mode_input_context().first_keyboard_character_for_action( "pause" );
    REQUIRE( pause );
    REQUIRE( pause->size() == 1 );
    REQUIRE( ungetch( pause->front() ) != -1 );
    u.set_moves( 100 );
    REQUIRE_FALSE( g->do_turn() );

    u.set_wielded_item( item( itype_id( "machete" ) ) );
    const int64_t old_uid = u.get_wielded_item()->uid().get_value();
    item &rock = here.add_item_or_charges( u.pos_bub(), item( itype_id( "rock" ) ) );
    const int64_t rock_uid = rock.uid().get_value();
    item_location target( map_cursor( u.pos_bub() ), &rock );
    semantic_surface_manager &manager = openclaw_harness_semantic_surface_manager();
    std::thread input_writer;
    std::atomic<bool> wrote_request{ false };
    on_out_of_scope finish_writer( [&] {
        if( input_writer.joinable() )
    {
        input_writer.join();
        }
        manager.set_descriptor_observer( {} );
        manager.set_receipt_observer( {} );
        manager.set_transport_observer( {} );
        u.cancel_activity();
    } );
    bool consumed_in_input = false;
    manager.set_transport_observer( [&]( const semantic_request_transport_event & event ) {
        if( event.event != "request_consumed" || event.request_id != "menu-delayed" ) {
            return;
        }
        std::string phase;
        std::ifstream events( trace );
        std::string line;
        while( std::getline( events, line ) ) {
            const auto begin = line.find( '{' );
            if( begin == std::string::npos ) {
                continue;
            }
            std::istringstream text( line.substr( begin ) );
            TextJsonIn jsin( text );
            const TextJsonObject value = jsin.get_object();
            value.allow_omitted_members();
            const auto kind = value.get_string( "event" );
            if( kind == "turn" && value.get_string( "stage" ) == "start" ) {
                phase = "simulation";
            } else if( kind == "turn_phase" ) {
                phase = value.get_string( "phase" );
            }
        }
        consumed_in_input = phase == "input";
    } );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    bool menu_seen = false;
    bool selector_seen = false;
    bool parent_entered = false;
    bool pause_queued = false;
    bool commit_queued = false;
    drop_locations selected;
    double preparation_cpu = 0.0;
    const auto actual_work = [&] {
        const auto begin = std::clock();
        volatile double calculation = 1.0;
        while( static_cast<double>( std::clock() - begin ) / CLOCKS_PER_SEC < 0.025 )
        {
            calculation = calculation * 1.00000001 + 0.00000001;
        }
        preparation_cpu += static_cast<double>( std::clock() - begin ) / CLOCKS_PER_SEC;
    };
    const auto delayed = [&]( const semantic_surface_descriptor & owner, const std::string & action,
    const std::string & stable_id ) {
        REQUIRE_FALSE( input_writer.joinable() );
        actual_work(); // Outside the acquisition scope; must stay measured.
        input_writer = std::thread( [ &, owner, action, stable_id] {
            std::this_thread::sleep_for( std::chrono::milliseconds( 120 ) );
            std::ofstream out( requests, std::ios::app );
            out << "{\"run_id\":\"" << run << "\",\"surface_id\":\"" << owner.surface_id <<
                "\",\"frame_id\":\"" << owner.frame_id <<
                "\",\"request_id\":\"menu-delayed\",\"action_id\":\"" << action <<
                "\",\"stable_id\":\"" << stable_id << "\"}\n";
            out.close();
            wrote_request = static_cast<bool>( out );
        } );
    };
    const auto run_wield = [&] {
        u.assign_activity( wield_activity_actor( target, 0 ) );
        process_avatar_activity_turn( u );
    };
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & owner ) {
        if( owner.kind == "menu" && !menu_seen ) {
            menu_seen = true;
            REQUIRE( owner.payload.at( "text" ).find( "Stop wielding" ) != std::string::npos );
            if( selector ) {
                // Real selector owner reached while ACT_WIELD owns the turn,
                // before disposal begins acquisition. This is a caller control,
                // not a claim that ordinary world.drop lacks its parent scope.
                selected = game_menus::inv::multidrop( u );
                actual_work(); // Post-selector processing also stays measured.
                REQUIRE( manager.top() );
                const auto resumed = *manager.top();
                REQUIRE( resumed.kind == "menu" );
                REQUIRE( manager.submit_request( { run, resumed.surface_id, resumed.frame_id,
                                                   "cancel-disposal", "menu.cancel", std::nullopt, {} } ) );
            } else {
                const auto drop = std::find_if( owner.valid_actions.begin(), owner.valid_actions.end(),
                []( const auto & a ) {
                    return a.id == "menu.choose" && a.label.find( "Drop item" ) == 0;
                } );
                REQUIRE( drop != owner.valid_actions.end() );
                delayed( owner, cancel ? "menu.cancel" : "menu.choose", cancel ? "" : drop->stable_id );
            }
        } else if( owner.kind == "inventory" && selector && !selector_seen ) {
            selector_seen = true;
            const std::string id = std::to_string( old_uid );
            REQUIRE( std::any_of( owner.valid_actions.begin(),
            owner.valid_actions.end(), [&]( const auto & a ) {
                return a.id == "inventory.toggle" && a.stable_id == id && a.enabled;
            } ) );
            delayed( owner, cancel ? "inventory.cancel" : "inventory.toggle", cancel ? "" : id );
        } else if( owner.kind == "inventory" && selector_seen && !cancel && !commit_queued ) {
            commit_queued = true;
            REQUIRE( manager.submit_request( { run, owner.surface_id, owner.frame_id,
                                               "selector-commit", "inventory.commit", std::nullopt, {} } ) );
        } else if( owner.kind == "world" && !pause_queued ) {
            pause_queued = true;
            if( nested && !parent_entered ) {
                parent_entered = true;
                run_wield();
                // A nested child must never close its enclosing World wait.
                std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );
            }
            REQUIRE( ungetch( pause->front() ) != -1 );
        }
    } );
    u.set_moves( 100 );
    if( !nested ) {
        u.assign_activity( wield_activity_actor( target, 0 ) );
    }
    const auto before = calendar::turn;
    REQUIRE_FALSE( g->do_turn() );
    const bool measured_world_wait = pause_queued;
    REQUIRE( input_writer.joinable() );
    input_writer.join();
    CHECK( wrote_request );
    CHECK( menu_seen );
    CHECK( selector_seen == selector );
    CHECK( consumed_in_input );
    CHECK( u.pos_abs() == position );
    CHECK( calendar::turn == before + 1_turns );
    CHECK_FALSE( u.activity );
    if( selector || cancel ) {
        REQUIRE( u.get_wielded_item() );
        CHECK( u.get_wielded_item()->uid().get_value() == old_uid );
        REQUIRE( target );
        CHECK( target->uid().get_value() == rock_uid );
    } else {
        REQUIRE( u.get_wielded_item() );
        CHECK( u.get_wielded_item()->typeId() == itype_id( "rock" ) );
        CHECK_FALSE( target );
        // Native dispose Drop copies into the ground stack before removing
        // the original body item. Its copy acquires a fresh UID; this metric
        // correction must preserve that existing quantity/type behavior.
        CHECK( std::count_if( here.i_at( u.pos_bub() ).begin(), here.i_at( u.pos_bub() ).end(),
        []( const item & it ) {
            return it.typeId() == itype_id( "machete" );
        } ) == 1 );
    }
    if( selector && !cancel ) {
        REQUIRE( selected.size() == 1 );
        CHECK( selected.front().first == u.get_wielded_item() );
        CHECK( selected.front().second == 1 );
    } else {
        CHECK( selected.empty() );
    }
    CHECK( std::count_if( receipts.begin(), receipts.end(), []( const auto & r ) {
        return r.request_id == "menu-delayed" && r.accepted;
    } ) == 1 );
    pause_queued = false;
    u.set_moves( 100 );
    REQUIRE_FALSE( g->do_turn() );
    int input_begins = 0;
    int input_ends = 0;
    std::optional<double> input_started;
    double acquired_wall = 0.0;
    bool overlapping_input = false;
    bool unmatched_end = false;
    bool measured_row = false;
    std::ifstream input( trace );
    std::string line;
    const int measured_turn = to_turn<int>( before + 1_turns );
    while( std::getline( input, line ) ) {
        const auto begin = line.find( '{' );
        if( begin == std::string::npos ) {
            continue;
        }
        std::istringstream text( line.substr( begin ) );
        TextJsonIn jsin( text );
        const auto object = jsin.get_object();
        object.allow_omitted_members();
        if( object.get_string( "event" ) == "turn_phase" &&
            object.get_int( "game_turn" ) == measured_turn ) {
            if( object.get_string( "stage" ) == "begin" && object.get_string( "phase" ) == "input" ) {
                ++input_begins;
                overlapping_input |= input_started.has_value();
                input_started = object.get_float( "wall_time_seconds" );
            } else if( object.get_string( "stage" ) == "end" ) {
                ++input_ends;
                unmatched_end |= !input_started.has_value();
                if( input_started ) {
                    acquired_wall += object.get_float( "wall_time_seconds" ) - *input_started;
                    input_started.reset();
                }
            }
        }
        if( object.get_string( "event" ) == "raid_performance_interval" ) {
            for( const TextJsonValue value : object.get_array( "rows" ) ) {
                const auto row = value.get_object();
                row.allow_omitted_members();
                if( row.get_int( "turn" ) == measured_turn ) {
                    measured_row = true;
                    // Publication delay includes redraw/poll work between
                    // timed acquisitions. Reconcile the actual spans, not the
                    // entire 120ms response delay. Trace WALL timestamps and
                    // steady-clock accounting are taken at adjacent boundaries.
                    CHECK( row.get_float( "input_seconds" ) > 0.0 );
                    CHECK( row.get_float( "input_seconds" ) ==
                           Approx( acquired_wall ).margin( 0.001 + input_begins * 0.000002 ) );
                    if( !nested ) {
                        CHECK( row.get_float( "simulation_seconds" ) >= preparation_cpu * 0.8 );
                        CHECK( row.get_float( "process_cpu_seconds" ) >= preparation_cpu * 0.8 );
                    }
                }
            }
        }
    }
    CHECK( measured_row );
    // A native menu may poll more than once. A child outside World input
    // must add an acquisition; a nested child must not split its parent.
    if( nested ) {
        CHECK( input_begins == 1 );
    } else {
        CHECK( input_begins >= 1 + static_cast<int>( measured_world_wait ) );
    }
    CHECK( input_ends == input_begins );
    CHECK_FALSE( overlapping_input );
    CHECK_FALSE( unmatched_end );
    CHECK_FALSE( input_started );
}


#endif
