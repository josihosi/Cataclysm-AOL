#include <map>
#include <optional>
#include <string>
#include <vector>

#include "cata_catch.h"
#include "imgui/imgui.h"
#include "input_popup.h"
#include "semantic_surface.h"
#include "string_input_popup.h"

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
} // namespace

TEST_CASE( "string prompt applies a semantic result consumed before its owner resumes",
           "[semantic_surface][string_prompt]" )
{
    scoped_imgui_context imgui;
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
    int prompt_descriptor_count = 0;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "string_prompt" ) {
            return;
        }
        ++prompt_descriptor_count;
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

    string_input_popup_imgui prompt( 20, "1", "How many?" );
    const std::string quantity = prompt.query();
    if( cancel ) {
        CHECK( quantity == "1" );
        CHECK( prompt.cancelled() );
    } else {
        CHECK( quantity == "1" );
        CHECK_FALSE( prompt.cancelled() );
    }
    REQUIRE( receipts.size() == 1 );
    CHECK( prompt_descriptor_count == 1 );
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

TEST_CASE( "string prompt rejects malformed text payloads before accepting its bound submission",
           "[semantic_surface][string_prompt]" )
{
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "prompt-validation-run" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "string_prompt" ) {
            return;
        }
        REQUIRE( descriptor.valid_actions.size() == 2 );
        CHECK( descriptor.valid_actions[0].id == "prompt.submit" );
        CHECK( descriptor.valid_actions[0].enabled );
        CHECK( descriptor.valid_actions[1].id == "prompt.cancel" );
        CHECK( descriptor.valid_actions[1].enabled );
        REQUIRE( manager.submit_request( { "prompt-validation-run", "wrong-surface", descriptor.frame_id,
                                           "wrong-owner", "prompt.submit", std::nullopt,
                                           { { "text", "12" } } } ) );
        CHECK_FALSE( manager.consume_top_request() );
        REQUIRE( manager.submit_request( { "prompt-validation-run", descriptor.surface_id, "stale-frame",
                                           "stale-owner", "prompt.submit", std::nullopt,
                                           { { "text", "12" } } } ) );
        CHECK_FALSE( manager.consume_top_request() );
        REQUIRE( manager.submit_request( { "prompt-validation-run", descriptor.surface_id, descriptor.frame_id,
                                           "missing-text", "prompt.submit", std::nullopt, {} } ) );
        CHECK_FALSE( manager.consume_top_request() );
        REQUIRE( manager.submit_request( { "prompt-validation-run", descriptor.surface_id, descriptor.frame_id,
                                           "extra-field", "prompt.submit", std::nullopt,
                                           { { "text", "12" }, { "other", "no" } } } ) );
        CHECK_FALSE( manager.consume_top_request() );
        REQUIRE( manager.submit_request( { "prompt-validation-run", descriptor.surface_id, descriptor.frame_id,
                                           "submit", "prompt.submit", std::nullopt,
                                           { { "text", "12" } } } ) );
    } );

    string_input_popup_imgui prompt( 20, "", "Set hour to?" );
    CHECK( prompt.query() == "12" );
    CHECK_FALSE( prompt.cancelled() );
    // Rejections are acknowledged immediately.  The accepted request is
    // consumed by the prompt owner and demonstrated by its returned value.
    REQUIRE( receipts.size() == 4 );
    CHECK_FALSE( receipts[0].accepted );
    CHECK( receipts[0].rejection_reason == "wrong_surface" );
    CHECK_FALSE( receipts[1].accepted );
    CHECK( receipts[1].rejection_reason == "stale_frame" );
    CHECK_FALSE( receipts[2].accepted );
    CHECK( receipts[2].rejection_reason == "invalid_parameters" );
    CHECK_FALSE( receipts[3].accepted );
    CHECK( receipts[3].rejection_reason == "invalid_parameters" );
}

TEST_CASE( "numeric STRING_INPUT prompt advertises and consumes a semantic submission",
           "[semantic_surface][string_prompt]" )
{
    scoped_imgui_context imgui;
    const bool consumed_by_input_boundary = GENERATE( false, true );
    semantic_surface_manager manager( "numeric-prompt-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope parent( manager, "menu", "Select time point" );
    bool submission_queued = false;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "string_prompt" ) {
            return;
        }
        CHECK( descriptor.payload.at( "numeric" ) == "true" );
        // The owner publishes once on construction and again at the start of
        // its input loop.  A semantic client submits exactly once against the
        // first currently advertised frame.
        if( submission_queued ) {
            return;
        }
        submission_queued = true;
        REQUIRE( manager.submit_request( { "numeric-prompt-run", descriptor.surface_id,
                                           descriptor.frame_id, "set-hour", "prompt.submit", std::nullopt,
                                           { { "text", "23" } } } ) );
        if( consumed_by_input_boundary ) {
            // input_context can consume an accepted transport wake before
            // number_input_popup regains its loop.  The popup must still
            // return the submitted native value instead of blocking again.
            REQUIRE( manager.consume_top_request() );
        }
    } );

    number_input_popup<int> prompt( 0, 12, "Set hour to?" );
    CHECK( prompt.query() == 23 );
    CHECK_FALSE( prompt.cancelled() );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == parent.surface_id() );
}
