#include <string>
#include <vector>

#include "avatar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "end_screen.h"
#include "game.h"
#include "imgui/imgui.h"
#include "player_helpers.h"
#include "semantic_surface.h"

TEST_CASE( "native end screen distinguishes death from preview and confirms",
           "[semantic_surface][end_screen]" )
{
    const int situation = GENERATE( 0, 1, 2, 3 );
    const bool dead = situation == 0 || situation == 2;
    const bool preview = situation == 1 || situation == 2;
    const bool suicide = situation == 3;
    clear_avatar();
    avatar &u = get_avatar();
    restore_on_out_of_scope restore_quit( g->uquit );
    g->uquit = suicide ? QUIT_SUICIDE : QUIT_NO;
    if( dead ) {
        u.set_part_hp_cur( bodypart_id( "head" ), 0 );
    }
    REQUIRE( u.is_dead_state() == dead );
    const bool owns_imgui = ImGui::GetCurrentContext() == nullptr;
    if( owns_imgui ) {
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2( 800.0f, 600.0f );
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
    semantic_surface_manager manager( "end-screen-run" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_surface_descriptor> descriptors;
    std::vector<semantic_action_receipt> receipts;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor & descriptor ) {
        descriptors.push_back( descriptor );
        if( descriptor.kind == "terminal" ) {
            REQUIRE( manager.submit_request( { "end-screen-run", descriptor.surface_id,
                                               descriptor.frame_id, "confirm", "terminal.confirm",
                                               std::nullopt, {} } ) );
        }
    } );
    manager.set_receipt_observer( [&]( const semantic_action_receipt & receipt ) {
        receipts.push_back( receipt );
    } );
    end_screen_data screen;
    screen.draw_end_screen_ui( !preview );
    REQUIRE( descriptors.size() == 1 );
    const semantic_surface_descriptor &terminal = descriptors.front();
    CHECK( terminal.kind == "terminal" );
    CHECK( terminal.payload.at( "avatar_id" ) == "character:" + std::to_string(
               u.getID().get_value() ) );
    CHECK( terminal.payload.at( "avatar_name" ) == u.get_name() );
    CHECK( terminal.payload.at( "avatar_dead" ) == ( dead ? "true" : "false" ) );
    CHECK( terminal.payload.at( "actual_death" ) == ( !preview && ( dead ||
            suicide ) ? "true" : "false" ) );
    CHECK( terminal.payload.at( "preview" ) == ( preview ? "true" : "false" ) );
    CHECK( terminal.payload.at( "suicide" ) == ( suicide ? "true" : "false" ) );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().action_id == "terminal.confirm" );
    CHECK( receipts.front().requested_frame_id == terminal.frame_id );
}
