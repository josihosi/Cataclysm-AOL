#include <string>

#include "cached_options.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "input_context.h"
#include "semantic_surface.h"
#include "uilist.h"

TEST_CASE( "uilist preserves results consumed inside the native input boundary",
           "[semantic_surface][semantic_wake]" )
{
    const std::string action = GENERATE( std::string( "menu.choose" ),
                                         std::string( "menu.cancel" ),
                                         std::string( "unadvertised" ) );
    semantic_surface_manager manager( "wake-test" );
    semantic_surface_manager_session session( manager );
    uilist menu;
    menu.allow_anykey = true;
    menu.addentry( 42, true, 'a', "entry" );
    REQUIRE( menu.query_setup() );
    semantic_surface_scope scope( manager, "menu", "Wake test", {}, {
        { "menu.choose", "entry", "entry", true },
        { "menu.cancel", "", "Cancel", true }
    }, [&menu]( const semantic_action_request & request ) {
        menu.ret = request.action_id == "menu.cancel" ? UILIST_CANCEL : 42;
        return semantic_action_dispatch_result{ true, "", "" };
    } );
    const semantic_surface_descriptor descriptor = *manager.top();
    REQUIRE( manager.submit_request( { manager.run_id(), descriptor.surface_id,
                                       descriptor.frame_id, "wake-request", action,
                                       action == "menu.choose" ? std::optional<std::string>( "entry" ) :
                                       std::nullopt, {} } ) );
    input_context ctxt( "UILIST" );
    ctxt.register_action( "ANY_INPUT" );
    // Reproduce the renderer-neutral wake notification emitted by the native
    // backend.  input_context consumes the real queued request while query_once
    // is waiting, before that context can interpret the backend input event.
    manager.mark_transport_wake();
    restore_on_out_of_scope<bool> restore_test_mode( test_mode );
    test_mode = false;
    menu.query_once( ctxt, 0 );
    CHECK( menu.ret_act == "ERROR" );
    CHECK_FALSE( manager.has_pending_request() );
    CHECK( menu.ret == ( action == "menu.choose" ? 42 :
                         action == "menu.cancel" ? UILIST_CANCEL : UILIST_WAIT_INPUT ) );
}
