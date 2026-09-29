#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "avatar.h"
#include "action.h"
#include "cata_catch.h"
#include "cached_options.h"
#include "coordinates.h"
#include "game.h"
#include "iexamine.h"
#include "imgui/imgui.h"
#include "input_context.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "player_helpers.h"
#include "semantic_surface.h"
#include "type_id.h"

std::string openclaw_harness_visible_local_facts( const map &here,
        const tripoint_bub_ms &avatar_pos, int radius, bool include_unknown, bool include_identity );

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

struct fireplace_menu_observation {
    std::vector<semantic_action_descriptor> actions;
    bool extinguish_enabled = false;
    bool extinguish_advertised = false;
    bool selection_submitted = false;
};

fireplace_menu_observation examine_brazier_menu( avatar &you, const tripoint_bub_ms &brazier,
        const bool choose_extinguish )
{
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "brazier-examine-test" );
    semantic_surface_manager_session session( manager );
    fireplace_menu_observation result;
    manager.set_descriptor_observer( [&manager, &result, choose_extinguish](
    const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "menu" || descriptor.payload.find( "text" ) == descriptor.payload.end() ||
            descriptor.payload.at( "text" ) != "Select an action" ) {
            return;
        }
        result.actions = descriptor.valid_actions;
        const auto extinguish = std::find_if( descriptor.valid_actions.begin(),
        descriptor.valid_actions.end(), []( const semantic_action_descriptor &action ) {
            return action.id == "menu.choose" && action.label == "Extinguish fire";
        } );
        result.extinguish_advertised = extinguish != descriptor.valid_actions.end();
        result.extinguish_enabled = result.extinguish_advertised && extinguish->enabled;
        if( result.selection_submitted ) {
            return;
        }
        const semantic_action_descriptor *choice = nullptr;
        if( choose_extinguish && result.extinguish_enabled ) {
            choice = &*extinguish;
        } else {
            const auto cancel = std::find_if( descriptor.valid_actions.begin(),
            descriptor.valid_actions.end(), []( const semantic_action_descriptor &action ) {
                return action.id == "menu.cancel";
            } );
            if( cancel != descriptor.valid_actions.end() ) {
                choice = &*cancel;
            }
        }
        if( choice != nullptr ) {
            result.selection_submitted = true;
            manager.submit_request( { manager.run_id(), descriptor.surface_id, descriptor.frame_id,
                                      "test-brazier-choice", choice->id,
                                      choice->stable_id.empty() ? std::nullopt :
                                      std::optional<std::string>( choice->stable_id ), {} } );
        }
    } );
    iexamine::fireplace( you, brazier );
    return result;
}

} // namespace

TEST_CASE( "semantic World advertises native Examine registered in DEFAULTMODE",
           "[semantic_surface][harness][brazier]" )
{
    const input_context context = get_default_mode_input_context();
    REQUIRE( context.is_registered_action( "examine" ) );
    const std::vector<std::pair<std::string, std::string>> actions =
        openclaw_harness_world_actions( context );
    const auto examine = std::find_if( actions.begin(), actions.end(),
    []( const std::pair<std::string, std::string> &action ) {
        return action.first == "world.examine";
    } );
    REQUIRE( examine != actions.end() );
    CHECK( examine->second == "examine" );
}

TEST_CASE( "brazier semantic Examine uses the native fire menu and state",
           "[semantic_surface][harness][brazier]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms brazier = you.pos_bub( here ) + tripoint::east;
    here.furn_set( brazier, furn_id( "f_brazier" ) );

    SECTION( "lit brazier with a bash-capable item offers and completes extinguishing" ) {
        you.i_add( item( itype_id( "screwdriver" ) ), false );
        REQUIRE( here.add_field( brazier, fd_fire, 1 ) );
        const fireplace_menu_observation menu = examine_brazier_menu( you, brazier, true );
        CHECK( menu.extinguish_advertised );
        CHECK( menu.extinguish_enabled );
        CHECK_FALSE( here.has_field_at( brazier, fd_fire ) );
    }

    SECTION( "unlit brazier has no extinguish choice" ) {
        const fireplace_menu_observation menu = examine_brazier_menu( you, brazier, false );
        CHECK_FALSE( menu.extinguish_advertised );
        CHECK_FALSE( here.has_field_at( brazier, fd_fire ) );
    }

    SECTION( "lit brazier without a bash-capable item disables extinguishing" ) {
        REQUIRE( here.add_field( brazier, fd_fire, 1 ) );
        const fireplace_menu_observation menu = examine_brazier_menu( you, brazier, false );
        const auto disabled = std::find_if( menu.actions.begin(), menu.actions.end(),
        []( const semantic_action_descriptor &action ) {
            return action.id == "menu.choose" && action.label == "Extinguish fire (bashing item required)";
        } );
        REQUIRE( disabled != menu.actions.end() );
        CHECK_FALSE( disabled->enabled );
        CHECK( here.has_field_at( brazier, fd_fire ) );
    }
}

TEST_CASE( "native Examine direction selection cannot reach a non-adjacent brazier",
           "[semantic_surface][harness][brazier]" )
{
    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms player = you.pos_bub( here );
    const tripoint_bub_ms remote_brazier = player + tripoint_rel_ms( 3, 0, 0 );
    here.furn_set( remote_brazier, furn_id( "f_brazier" ) );

    semantic_surface_manager manager( "brazier-adjacency-test" );
    semantic_surface_manager_session session( manager );
    bool direction_selected = false;
    manager.set_descriptor_observer( [&manager, &direction_selected](
    const semantic_surface_descriptor &descriptor ) {
        if( direction_selected || descriptor.kind != "direction" ) {
            return;
        }
        direction_selected = true;
        const auto east = std::find_if( descriptor.valid_actions.begin(),
        descriptor.valid_actions.end(), []( const semantic_action_descriptor &action ) {
            return action.id == "direction.choose" && action.stable_id == "east" && action.enabled;
        } );
        REQUIRE( east != descriptor.valid_actions.end() );
        manager.submit_request( { manager.run_id(), descriptor.surface_id, descriptor.frame_id,
                                  "select-east", east->id, east->stable_id, {} } );
    } );

    const std::optional<tripoint_bub_ms> selected = choose_adjacent_highlight( here, player,
            "Examine terrain or furniture where?", "There is nothing that can be examined nearby.",
    [&remote_brazier]( const tripoint_bub_ms &candidate ) {
        return candidate == remote_brazier;
    }, false, false );
    CHECK( direction_selected );
    CHECK_FALSE( selected.has_value() );
}

TEST_CASE( "avatar-visible minimap reports local passability and brazier state only for clear tiles",
           "[semantic_surface][harness][brazier][minimap]" )
{
    clear_avatar();
    clear_map_with_vision();
    avatar &you = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms player = you.pos_bub( here );
    for( int dy = -2; dy <= 2; ++dy ) {
        here.ter_set( player + tripoint_rel_ms( 1, dy, 0 ), ter_id( "t_brick_wall" ) );
    }
    here.ter_set( player + tripoint::south, ter_id( "t_door_c" ) );
    const tripoint_bub_ms brazier = player + tripoint::west;
    here.furn_set( brazier, furn_id( "f_brazier" ) );
    REQUIRE( here.add_field( brazier, fd_fire, 1 ) );
    set_time_to_day();
    here.invalidate_map_cache( player.z() );
    here.build_map_cache( player.z(), true );
    here.invalidate_visibility_cache();
    here.update_visibility_cache( player.z() );

    const JsonArray cells = json_loader::from_string(
                                openclaw_harness_visible_local_facts( here, player, 2, true, false ) );
    bool saw_player = false;
    bool saw_wall = false;
    bool saw_door = false;
    bool saw_brazier = false;
    bool saw_unknown = false;
    for( const JsonObject cell : cells ) {
        cell.allow_omitted_members();
        const int dx = cell.get_int( "dx" );
        const int dy = cell.get_int( "dy" );
        if( dx == 2 && dy == 0 ) {
            CHECK( cell.get_string( "visibility" ) == "unknown" );
            CHECK_FALSE( cell.has_member( "terrain" ) );
            CHECK_FALSE( cell.has_member( "passable" ) );
            CHECK_FALSE( cell.has_member( "furniture" ) );
            CHECK_FALSE( cell.has_member( "fields" ) );
            saw_unknown = true;
        } else if( dx == 0 && dy == 0 ) {
            CHECK( cell.get_string( "visibility" ) == "clear" );
            CHECK( cell.get_bool( "passable" ) );
            saw_player = true;
        } else if( dx == 1 && dy == 0 ) {
            CHECK( cell.get_string( "visibility" ) == "clear" );
            CHECK_FALSE( cell.get_bool( "passable" ) );
            CHECK( cell.get_string( "terrain" ) == here.tername( player + tripoint::east ) );
            saw_wall = true;
        } else if( dx == 0 && dy == 1 ) {
            CHECK( cell.get_string( "visibility" ) == "clear" );
            CHECK_FALSE( cell.get_bool( "passable" ) );
            CHECK( cell.get_string( "terrain" ) == here.tername( player + tripoint::south ) );
            saw_door = true;
        } else if( dx == -1 && dy == 0 ) {
            CHECK( cell.get_string( "visibility" ) == "clear" );
            CHECK( cell.get_string( "furniture" ) == "f_brazier" );
            const JsonArray fields = cell.get_array( "fields" );
            REQUIRE( fields.size() == 1 );
            CHECK( fields.get_string( 0 ) == "fd_fire" );
            saw_brazier = true;
        }
    }
    CHECK( saw_player );
    CHECK( saw_wall );
    CHECK( saw_door );
    CHECK( saw_brazier );
    CHECK( saw_unknown );
}
