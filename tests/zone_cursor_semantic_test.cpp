#include <algorithm>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "cata_scope_helpers.h"
#include "debug.h"
#include "clzones.h"
#include "game.h"
#include "imgui/imgui.h"
#include "json.h"
#include "json_loader.h"
#include "map_helpers.h"
#include "player_helpers.h"
#include "output.h"
#include "semantic_surface.h"

namespace
{
class zone_test_imgui
{
    public:
        zone_test_imgui() : restore_width( TERMX ), restore_height( TERMY ),
            restore_full_width( FULL_SCREEN_WIDTH ), restore_full_height( FULL_SCREEN_HEIGHT ),
            owns( ImGui::GetCurrentContext() == nullptr ) {
            // Native curses popups need terminal geometry; test startup does
            // not run game::init_ui and leaves these dimensions at zero.
            TERMX = FULL_SCREEN_WIDTH = 80;
            TERMY = FULL_SCREEN_HEIGHT = 24;
            if( owns ) {
                ImGui::CreateContext();
                ImGuiIO &io = ImGui::GetIO();
                io.DisplaySize = ImVec2( 800, 600 );
                io.DeltaTime = 1.0f / 60.0f;
                io.Fonts->AddFontDefault();
                io.Fonts->Build();
                ImGui::NewFrame();
            }
        }
        ~zone_test_imgui() {
            if( owns ) {
                ImGui::EndFrame();
                ImGui::DestroyContext();
            }
        }
    private:
        restore_on_out_of_scope<int> restore_width;
        restore_on_out_of_scope<int> restore_height;
        restore_on_out_of_scope<int> restore_full_width;
        restore_on_out_of_scope<int> restore_full_height;
        bool owns;
};
}

TEST_CASE( "native bounds cursor publishes movement before confirm or cancel",
           "[semantic_surface][zone_cursor]" )
{
    const bool cancel = GENERATE( false, true );
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    tripoint_bub_ms center = you.pos_bub();
    const tripoint_bub_ms start = center;
    const time_point turn = calendar::turn;
    const int moves = you.get_moves();
    semantic_surface_manager manager( "cursor-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope parent( manager, "world", "World" );
    int publications = 0;
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "look_cursor" ) {
            return;
        }
        ++publications;
        CHECK( descriptor.payload.at( "mode" ) == "zone_bounds" );
        if( publications == 1 ) {
            CHECK( descriptor.payload.at( "relative_to_avatar" ) == "[0,0,0]" );
        } else {
            REQUIRE( publications == 2 );
            CHECK( descriptor.payload.at( "relative_to_avatar" ) == "[1,0,0]" );
        }
        REQUIRE( manager.submit_request( { "cursor-run", descriptor.surface_id, descriptor.frame_id,
                                           std::to_string( publications ), publications == 1 ? "cursor.east" :
                                           cancel ? "cursor.cancel" : "cursor.confirm", std::nullopt, {} } ) );
    } );
    const look_around_result result = g->look_around( false, center, start, false, true, false );
    if( cancel ) {
        CHECK_FALSE( result.position );
    } else {
        REQUIRE( result.position );
        CHECK( *result.position == start + tripoint::east );
    }
    CHECK( publications == 2 );
    REQUIRE( receipts.size() == 2 );
    CHECK( receipts[0].accepted );
    CHECK( receipts[1].accepted );
    CHECK( receipts[0].resulting_frame_id != receipts[0].requested_frame_id );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == parent.surface_id() );
    CHECK( calendar::turn == turn );
    CHECK( you.get_moves() == moves );
}

TEST_CASE( "empty native zone manager offers creation and closes",
           "[semantic_surface][zone_cursor]" )
{
    zone_test_imgui imgui;
    clear_avatar();
    clear_map();
    zone_manager::get_manager().clear();
    semantic_surface_manager manager( "empty-zone-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope parent( manager, "world", "World" );
    bool saw_empty = false;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "zone_manager" ) {
            return;
        }
        saw_empty = true;
        CHECK( descriptor.payload.at( "zones" ) == "[]" );
        CHECK( std::any_of( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
        []( const semantic_action_descriptor &action ) {
            return action.id == "zone.create" && action.enabled;
        } ) );
        REQUIRE( manager.submit_request( { "empty-zone-run", descriptor.surface_id, descriptor.frame_id,
                                           "close", "zone.close", std::nullopt, {} } ) );
    } );
    zone_manager_ui::display_zone_manager();
    CHECK( saw_empty );
    REQUIRE( manager.top() );
    CHECK( manager.top()->surface_id == parent.surface_id() );
}

TEST_CASE( "native zone creation and editing use menus names and both corners",
           "[semantic_surface][zone_cursor]" )
{
    zone_test_imgui imgui;
    clear_avatar();
    clear_map();
    zone_manager &zones = zone_manager::get_manager();
    zones.clear();
    semantic_surface_manager manager( "zone-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope parent( manager, "world", "World" );
    enum class stage { create, naming, corners, created, editing, renamed, deleted, closing };
    stage current = stage::create;
    int sequence = 0;
    int corner_moves = 0;
    std::string zone_id;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        auto submit = [&]( const std::string &action, const std::optional<std::string> &id = std::nullopt,
        const std::map<std::string, std::string> &parameters = {} ) {
            REQUIRE( manager.submit_request( { "zone-run", descriptor.surface_id, descriptor.frame_id,
                                               std::to_string( ++sequence ), action, id, parameters } ) );
        };
        if( descriptor.kind == "zone_manager" ) {
            if( current == stage::create ) {
                current = stage::naming;
                submit( "zone.create" );
            } else if( current == stage::created || current == stage::renamed ) {
                const JsonArray rows = json_loader::from_string( descriptor.payload.at( "zones" ) ).get_array();
                REQUIRE( rows.size() == 1 );
                const JsonObject row = rows.get_object( 0 );
                row.allow_omitted_members();
                CHECK( row.get_string( "type" ) == "CAMP_STORAGE" );
                zone_id = row.get_string( "id" );
                if( current == stage::created ) {
                    CHECK( row.get_string( "name" ) == "Luna camp supplies" );
                    current = stage::editing;
                    submit( "zone.edit", zone_id );
                } else {
                    CHECK( row.get_string( "name" ) == "Renamed camp supplies" );
                    current = stage::deleted;
                    submit( "zone.delete", zone_id );
                }
            } else if( current == stage::deleted ) {
                CHECK( descriptor.payload.at( "zones" ) == "[]" );
                current = stage::closing;
                submit( "zone.close" );
            } else {
                FAIL( "Zone parent published while native child sequence still owns input" );
            }
        } else if( descriptor.kind == "menu" ) {
            const std::string desired = current == stage::editing ? "Edit name" :
                                        zones.get_name_from_type( zone_type_id( "CAMP_STORAGE" ) );
            auto selected = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            [&]( const semantic_action_descriptor &action ) {
                return action.id == "menu.choose" && action.label == desired && action.enabled;
            } );
            if( selected == descriptor.valid_actions.end() ) {
                selected = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
                [&]( const semantic_action_descriptor &action ) {
                    return action.id == "menu.select" && action.label == desired && action.enabled;
                } );
            }
            REQUIRE( selected != descriptor.valid_actions.end() );
            submit( selected->id, selected->stable_id );
        } else if( descriptor.kind == "string_prompt" ) {
            if( current == stage::editing ) {
                current = stage::renamed;
                submit( "prompt.submit", std::nullopt, { { "text", "Renamed camp supplies" } } );
            } else {
                REQUIRE( current == stage::naming );
                current = stage::corners;
                submit( "prompt.submit", std::nullopt, { { "text", "Luna camp supplies" } } );
            }
        } else if( descriptor.kind == "look_cursor" ) {
            REQUIRE( current == stage::corners );
            if( descriptor.payload.at( "selection_stage" ) == "first_point" ) {
                submit( "cursor.confirm" );
            } else if( corner_moves++ == 0 ) {
                submit( "cursor.east" );
            } else {
                CHECK( descriptor.payload.at( "relative_to_avatar" ) == "[1,0,0]" );
                submit( "cursor.confirm" );
            }
        } else if( descriptor.kind == "prompt" ) {
            const bool saving = current == stage::closing;
            const auto choice = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            [&]( const semantic_action_descriptor &action ) {
                return action.id == "prompt.choose" && action.label == ( saving ? "YES" : "NO" );
            } );
            REQUIRE( choice != descriptor.valid_actions.end() );
            if( !saving ) {
                current = stage::created;
            }
            submit( choice->id, choice->stable_id );
        }
    } );
    zone_manager_ui::display_zone_manager();
    CHECK( current == stage::closing );
    CHECK( corner_moves == 2 );
    CHECK_FALSE( zone_id.empty() );
    CHECK( zones.get_zones().empty() );
    zones.clear();
}

TEST_CASE( "zone selection uses stable identity and faction changes only the filter",
           "[semantic_surface][zone_cursor]" )
{
    zone_test_imgui imgui;
    clear_avatar();
    clear_map();
    restore_on_out_of_scope restore_debug( debug_mode );
    debug_mode = true;
    zone_manager &zones = zone_manager::get_manager();
    zones.clear();
    const tripoint_abs_ms position = get_avatar().pos_abs();
    const faction_id faction( "your_followers" );
    zones.add( "First", zone_type_id( "LOOT_UNSORTED" ), faction, false, true, position, position );
    zones.add( "Second", zone_type_id( "LOOT_FOOD" ), faction, false, true, position, position );
    const std::string second = zones.get_zones()[1].get().get_identity();
    semantic_surface_manager manager( "zone-selection-run" );
    semantic_surface_manager_session session( manager );
    semantic_surface_scope parent( manager, "world", "World" );
    int state = 0;
    int requests = 0;
    manager.set_descriptor_observer( [&]( const semantic_surface_descriptor &descriptor ) {
        const auto submit = [&]( const std::string &action,
                                 const std::optional<std::string> &id = std::nullopt,
        const std::map<std::string, std::string> &parameters = {} ) {
            REQUIRE( manager.submit_request( { "zone-selection-run", descriptor.surface_id,
                                               descriptor.frame_id, std::to_string( ++requests ), action, id, parameters } ) );
        };
        if( descriptor.kind == "zone_manager" ) {
            if( state == 0 ) {
                state = 1;
                submit( "zone.select", second );
            } else if( state == 1 ) {
                CHECK( descriptor.payload.at( "zone_id" ) == second );
                state = 2;
                submit( "zone.filter_faction" );
            } else {
                REQUIRE( state == 3 );
                CHECK( descriptor.payload.at( "faction" ) == "unlisted_test_faction" );
                CHECK( descriptor.payload.at( "zones" ) == "[]" );
                state = 4;
                submit( "zone.close" );
            }
        } else if( descriptor.kind == "string_prompt" ) {
            REQUIRE( state == 2 );
            state = 3;
            submit( "prompt.submit", std::nullopt, { { "text", "unlisted_test_faction" } } );
        }
    } );
    try {
        zone_manager_ui::display_zone_manager();
    } catch( const std::exception &error ) {
        FAIL( "Native zone selection/filter stage " << state << " after " << requests <<
              " requests: " << error.what() );
    }
    CHECK( state == 4 );
    REQUIRE( zones.get_zones( faction ).size() == 2 );
    CHECK( zones.get_zones( faction )[1].get().get_identity() == second );
    zones.clear();
}
