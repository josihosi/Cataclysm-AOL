#include <algorithm>
#include <string>
#include <vector>

#include "cata_catch.h"
#include "character_id.h"
#include "json.h"
#include "json_loader.h"
#include "npc.h"
#include "npctalk_rules.h"
#include "semantic_surface.h"

TEST_CASE( "follower rules semantic controls mutate the bound NPC and publish their outcome",
           "[semantic_surface][npc_rules]" )
{
    npc follower;
    follower.setID( character_id( 42 ), true );
    follower.rules.clear_flags();
    follower.rules.clear_overrides();
    const auto labels = follower_rules_ui::semantic_labels( follower );
    semantic_surface_manager manager( "rules-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope scope( manager, "npc_rules_menu", "Rules",
                                 follower_rules_ui::semantic_payload( follower, labels ),
                                 follower_rules_ui::semantic_actions( follower, labels ),
    [&follower]( const semantic_action_request &request ) {
        return follower_rules_ui::handle_semantic_request( follower, request );
    } );
    const auto publish = [&]() {
        return scope.publish( follower_rules_ui::semantic_payload( follower, labels ),
                              follower_rules_ui::semantic_actions( follower, labels ) );
    };
    const semantic_surface_descriptor before = *manager.top();
    CHECK( before.payload.at( "speaker_id" ) == "42" );
    publish();
    CHECK( manager.top()->frame_id == before.frame_id );
    const JsonArray rows = json_loader::from_string( before.payload.at( "rules" ) );
    bool found_guns = false;
    for( const JsonObject row : rows ) {
        row.allow_omitted_members();
        if( row.get_string( "id" ) == "use_guns" ) {
            found_guns = true;
            CHECK_FALSE( row.get_bool( "enabled" ) );
            CHECK_FALSE( row.get_bool( "base_enabled" ) );
            CHECK_FALSE( row.get_bool( "override_enabled" ) );
            CHECK_FALSE( row.get_string( "label" ).empty() );
        }
    }
    REQUIRE( found_guns );
    REQUIRE( manager.submit_request( { "rules-run", before.surface_id, before.frame_id,
                                      "toggle-1", "npc_rules.toggle", "use_guns", { { "speaker_id", "42" } } } ) );
    REQUIRE( scope.consume_request() );
    CHECK( follower.rules.has_flag( ally_rule::use_guns ) );
    publish();
    REQUIRE_FALSE( receipts.empty() );
    CHECK( receipts.back().accepted );
    CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
    CHECK( manager.top()->frame_id != before.frame_id );

    REQUIRE( manager.submit_request( { "rules-run", before.surface_id, before.frame_id,
                                      "stale-toggle", "npc_rules.toggle", "use_guns", {} } ) );
    CHECK_FALSE( scope.consume_request() );
    CHECK( follower.rules.has_flag( ally_rule::use_guns ) );
    CHECK( receipts.back().rejection_reason == "stale_frame" );

    const semantic_surface_descriptor current = *manager.top();
    REQUIRE( manager.submit_request( { "rules-run", current.surface_id, current.frame_id,
                                      "wrong-npc", "npc_rules.toggle", "use_guns", { { "speaker_id", "99" } } } ) );
    CHECK_FALSE( scope.consume_request() );
    CHECK( follower.rules.has_flag( ally_rule::use_guns ) );
    CHECK( receipts.back().rejection_reason == "wrong_speaker" );

    REQUIRE( manager.submit_request( { "rules-run", current.surface_id, current.frame_id,
                                      "reset-1", "npc_rules.reset", "use_guns", {} } ) );
    REQUIRE( scope.consume_request() );
    publish();
    CHECK_FALSE( follower.rules.has_flag( ally_rule::use_guns ) );
    CHECK( receipts.back().resulting_frame_id == manager.top()->frame_id );
    const auto actions = follower_rules_ui::semantic_actions( follower, labels );
    const auto reset = std::find_if( actions.begin(), actions.end(), []( const auto &action ) {
        return action.id == "npc_rules.reset" && action.stable_id == "use_guns";
    } );
    REQUIRE( reset != actions.end() );
    CHECK_FALSE( reset->enabled );
}

TEST_CASE( "follower rule overrides retain native toggle and default semantics",
           "[semantic_surface][npc_rules]" )
{
    npc follower;
    follower.rules.clear_flags();
    follower.rules.clear_overrides();
    follower.rules.set_specific_override_state( ally_rule::use_guns, true );
    semantic_action_request request;
    request.action_id = "npc_rules.toggle";
    request.stable_id = "use_guns";
    CHECK( follower_rules_ui::handle_semantic_request( follower, request ).accepted );
    CHECK_FALSE( follower.rules.has_flag( ally_rule::use_guns ) );
    CHECK( follower.rules.has_override_enable( ally_rule::use_guns ) );
    CHECK_FALSE( follower.rules.has_flag( ally_rule::use_guns, false ) );
    CHECK_FALSE( follower.rules.has_override( ally_rule::use_guns ) );
    const auto labels = follower_rules_ui::semantic_labels( follower );
    const auto actions = follower_rules_ui::semantic_actions( follower, labels );
    const auto reset = std::find_if( actions.begin(), actions.end(), []( const auto &action ) {
        return action.id == "npc_rules.reset" && action.stable_id == "use_guns";
    } );
    REQUIRE( reset != actions.end() );
    CHECK( reset->enabled );
    request.action_id = "npc_rules.reset";
    CHECK( follower_rules_ui::handle_semantic_request( follower, request ).accepted );
    CHECK_FALSE( follower.rules.has_override_enable( ally_rule::use_guns ) );
    CHECK_FALSE( follower.rules.has_flag( ally_rule::use_guns ) );
    CHECK_FALSE( follower_rules_ui::handle_semantic_request( follower, request ).accepted );
    request.action_id = "npc_rules.toggle";
    request.stable_id = "unknown_rule";
    CHECK_FALSE( follower_rules_ui::handle_semantic_request( follower, request ).accepted );
    CHECK_FALSE( follower.rules.has_flag( ally_rule::use_guns ) );
}
