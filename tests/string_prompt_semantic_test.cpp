#include <map>
#include <optional>
#include <string>
#include <vector>

#include "cata_catch.h"
#include "cursesdef.h"
#include "semantic_surface.h"
#include "string_input_popup.h"

TEST_CASE( "string prompt applies a semantic result consumed before its owner resumes",
           "[semantic_surface][string_prompt]" )
{
    const bool consumed_before_owner = GENERATE( false, true );
    bool cancel = false;
    SECTION( "submit a quantity" ) {}
    SECTION( "cancel a quantity" ) {
        cancel = true;
    }

    semantic_surface_manager manager( "prompt-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope parent( manager, "world", "World" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    std::string prompt_frame;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "string_prompt" ) {
            return;
        }
        prompt_frame = descriptor.frame_id;
        const std::string action = cancel ? "prompt.cancel" : "prompt.submit";
        REQUIRE( manager.submit_request( { "prompt-run", descriptor.surface_id,
                                           descriptor.frame_id, "quantity", action, std::nullopt,
                                           cancel ? std::map<std::string, std::string>{} :
                                           std::map<std::string, std::string>{ { "text", "1" } } } ) );
        // input_context::handle_input consumes transport requests itself.
        // Exercise the real popup with exactly that already-consumed state;
        // its local drain must not be required to consume the request twice.
        if( consumed_before_owner ) {
            REQUIRE( manager.consume_top_request() );
            CHECK_FALSE( manager.has_pending_request() );
            CHECK( receipts.empty() );
        }
    } );

    const catacurses::window window = catacurses::newwin( 3, 30, point::zero );
    REQUIRE( window );
    string_input_popup prompt;
    prompt.window( window, point( 1, 1 ), 28 ).title( "How many?" ).text( "1" ).max_length( 20 ).only_digits( true );
    const std::optional<int> quantity = prompt.query_int();
    if( cancel ) {
        CHECK_FALSE( quantity.has_value() );
        CHECK( prompt.canceled() );
        CHECK_FALSE( prompt.confirmed() );
    } else {
        CHECK( quantity == 1 );
        CHECK( prompt.confirmed() );
        CHECK_FALSE( prompt.canceled() );
    }
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().requested_frame_id == prompt_frame );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == parent.surface_id() );
    if( !consumed_before_owner ) {
        // The queued route verifies receipt timing.  Reentrant consumption
        // in the descriptor observer above isolates stored-result handling,
        // while publication of that initial descriptor is still in progress.
        CHECK( receipts.front().resulting_frame_id == manager.top()->frame_id );
    }
}
