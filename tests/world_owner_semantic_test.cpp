#include <map>
#include <optional>
#include <string>
#include <vector>

#include "cata_catch.h"
#include "semantic_surface.h"

TEST_CASE( "retired one-turn World waits for its actual successor across serial child owners",
           "[semantic_surface][world_owner]" )
{
    semantic_surface_manager manager( "world-run" );
    std::vector<semantic_surface_descriptor> frames;
    std::vector<semantic_action_receipt> receipts;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &frame ) {
        frames.push_back( frame );
    } );
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    std::optional<semantic_surface_scope> world;
    world.emplace( manager, "world", "World", std::map<std::string, std::string>{},
                   std::vector<semantic_action_descriptor>{ { "world.debug_menu", "", "Debug", true } },
    [&]( const semantic_action_request &request ) {
        REQUIRE( manager.withhold_parent_authority_until_recreated( request.surface_id ) );
        return semantic_action_dispatch_result{ true, "", "" };
    } );
    const std::string retired_id = world->surface_id();
    const auto submit = [&]( const std::string &id, const std::string &action ) {
        REQUIRE( manager.top() );
        const semantic_surface_descriptor frame = *manager.top();
        REQUIRE( manager.submit_request( { "world-run", frame.surface_id, frame.frame_id,
                                           id, action, std::nullopt, {} } ) );
        REQUIRE( manager.consume_top_request() );
    };
    submit( "debug", "world.debug_menu" );
    CHECK( receipts.empty() );
    {
        semantic_surface_scope menu( manager, "menu", "Spawn item", {}, {
            { "menu.choose", "", "Choose", true }
        }, []( const semantic_action_request & ) {
            return semantic_action_dispatch_result{ true, "", "" };
        } );
        REQUIRE( receipts.size() == 1 );
        CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
        submit( "choose", "menu.choose" );
    }
    CHECK_FALSE( manager.top() );
    CHECK( receipts.size() == 1 );
    {
        semantic_surface_scope prompt( manager, "string_prompt", "How many?", {}, {
            { "prompt.submit", "", "Submit", true }
        }, []( const semantic_action_request & ) {
            return semantic_action_dispatch_result{ true, "", "" };
        } );
        REQUIRE( receipts.size() == 2 );
        CHECK( receipts.back().request_id == "choose" );
        CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
        submit( "quantity", "prompt.submit" );
    }
    CHECK_FALSE( manager.top() );
    CHECK( receipts.size() == 2 );
    {
        semantic_surface_scope menu( manager, "menu", "Spawn item", {}, {
            { "menu.cancel", "", "Cancel", true }
        }, []( const semantic_action_request & ) {
            return semantic_action_dispatch_result{ true, "", "" };
        } );
        REQUIRE( receipts.size() == 3 );
        CHECK( receipts.back().request_id == "quantity" );
        CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
        submit( "cancel", "menu.cancel" );
    }
    CHECK_FALSE( manager.top() );
    CHECK( receipts.size() == 3 );
    world.reset();
    semantic_surface_scope next_world( manager, "world", "World" );
    REQUIRE( receipts.size() == 4 );
    CHECK( receipts.back().request_id == "cancel" );
    CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
    CHECK( next_world.surface_id() != retired_id );
    REQUIRE( frames.size() == 5 );
    CHECK( frames[0].kind == "world" );
    CHECK( frames[1].kind == "menu" );
    CHECK( frames[2].kind == "string_prompt" );
    CHECK( frames[3].kind == "menu" );
    CHECK( frames[4].kind == "world" );
}

TEST_CASE( "direct World action settles only on the recreated input owner",
           "[semantic_surface][world_owner]" )
{
    semantic_surface_manager manager( "world-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    {
        semantic_surface_scope world( manager, "world", "World", {}, {
            { "world.pause", "", "Pause", true }
        }, [&]( const semantic_action_request &request ) {
            REQUIRE( manager.withhold_parent_authority_until_recreated( request.surface_id ) );
            return semantic_action_dispatch_result{ true, "", "" };
        } );
        const semantic_surface_descriptor frame = *manager.top();
        REQUIRE( manager.submit_request( { "world-run", frame.surface_id, frame.frame_id,
                                           "pause", "world.pause", std::nullopt, {} } ) );
        REQUIRE( world.consume_request() );
        CHECK( receipts.empty() );
    }
    CHECK_FALSE( manager.top() );
    CHECK( receipts.empty() );
    semantic_surface_scope next_world( manager, "world", "World" );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().resulting_frame_id == manager.top()->frame_id );
}

TEST_CASE( "pre-modal World dispatch receipts once and cannot replay its native handoff",
           "[semantic_surface][world_owner]" )
{
    semantic_surface_manager manager( "world-run" );
    std::vector<semantic_action_receipt> receipts;
    int modal_dispatches = 0;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World", {}, {
        { "world.basecamp_missions", "2", "Open Base Missions", true }
    }, [&]( const semantic_action_request &request ) {
        ++modal_dispatches;
        REQUIRE( manager.withhold_parent_authority_until_recreated( request.surface_id ) );
        return semantic_action_dispatch_result{ true, "", "", false, false,
                                                "modal_dispatch_queued" };
    } );
    const semantic_surface_descriptor frame = *manager.top();
    const semantic_action_request request{ "world-run", frame.surface_id, frame.frame_id,
                                           "base-missions", "world.basecamp_missions", "2", {} };
    REQUIRE( manager.submit_request( request ) );
    REQUIRE( world.consume_request() );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().outcome == "modal_dispatch_queued" );
    CHECK( modal_dispatches == 1 );

    // A transport retry receives its already-recorded receipt and never
    // becomes another pending request for the native modal handoff.
    CHECK_FALSE( manager.submit_request( request ) );
    CHECK( receipts.size() == 2 );
    CHECK( modal_dispatches == 1 );
}

TEST_CASE( "unavailable World modal dispatch is rejected without retiring World",
           "[semantic_surface][world_owner]" )
{
    semantic_surface_manager manager( "world-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World", {}, {
        { "world.basecamp_missions", "2", "Open Base Missions", true }
    }, []( const semantic_action_request & ) {
        return semantic_action_dispatch_result{ false, "unavailable_basecamp", "" };
    } );
    const semantic_surface_descriptor frame = *manager.top();
    REQUIRE( manager.submit_request( { "world-run", frame.surface_id, frame.frame_id,
                                       "unavailable", "world.basecamp_missions", "2", {} } ) );
    CHECK_FALSE( world.consume_request() );
    REQUIRE( receipts.size() == 1 );
    CHECK_FALSE( receipts.front().accepted );
    CHECK( receipts.front().rejection_reason == "unavailable_basecamp" );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == frame.surface_id );
    CHECK( manager.top()->frame_id == frame.frame_id );
}

TEST_CASE( "rejected World action preserves input and persistent parents still restore",
           "[semantic_surface][world_owner]" )
{
    semantic_surface_manager manager( "world-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World", {}, {
        { "world.inspect_npc", "", "Inspect", true }
    }, []( const semantic_action_request & ) {
        return semantic_action_dispatch_result{ false, "stale_actor_id", "" };
    } );
    const semantic_surface_descriptor frame = *manager.top();
    REQUIRE( manager.submit_request( { "world-run", frame.surface_id, frame.frame_id,
                                       "reject", "world.inspect_npc", std::nullopt, {} } ) );
    CHECK_FALSE( world.consume_request() );
    REQUIRE( receipts.size() == 1 );
    CHECK_FALSE( receipts.front().accepted );
    CHECK( receipts.front().rejection_reason == "stale_actor_id" );
    CHECK( manager.top()->frame_id == frame.frame_id );

    REQUIRE( manager.withhold_parent_authority_until_recreated( world.surface_id() ) );
    semantic_surface_scope persistent( manager, "inventory", "Inventory" );
    const std::string persistent_frame = manager.top()->frame_id;
    {
        semantic_surface_scope child( manager, "item_info", "Details", {}, {
            { "item_info.close", "", "Close", true }
        }, []( const semantic_action_request & ) {
            return semantic_action_dispatch_result{ true, "", "" };
        } );
        const semantic_surface_descriptor child_frame = *manager.top();
        REQUIRE( manager.submit_request( { "world-run", child_frame.surface_id, child_frame.frame_id,
                                           "close", "item_info.close", std::nullopt, {} } ) );
        REQUIRE( child.consume_request() );
    }
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == persistent.surface_id() );
    CHECK( manager.top()->frame_id != persistent_frame );
    REQUIRE( receipts.size() == 2 );
    CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
}
