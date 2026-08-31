#include <string>
#include <vector>

#include "cata_catch.h"
#include "semantic_surface.h"

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
        CHECK( manager.top()->breadcrumbs == std::vector<std::string>{ "World", "Inventory" } );
        CHECK( manager.top()->valid_actions.front().id == "menu.cancel" );
        CHECK_FALSE( world.publish( {}, { { "world.wait", "", "Wait", true } } ) );
        CHECK( manager.stack().size() == 2 );
    }

    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == world_surface );
    CHECK( manager.top()->frame_id != world_frame );
    CHECK( manager.top()->valid_actions.front().id == "world.wait" );
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
