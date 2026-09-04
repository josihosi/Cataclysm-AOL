#include <string>
#include <vector>

#include "cata_catch.h"
#include "character.h"
#include "debug_menu.h"
#include "imgui/imgui.h"
#include "inventory_ui.h"
#include "input_popup.h"
#include "item.h"
#include "popup.h"
#include "player_helpers.h"
#include "semantic_surface.h"
#include "string_input_popup.h"
#include "type_id.h"
#include "ui_iteminfo.h"
#include "uilist.h"

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

TEST_CASE( "dialogue semantic receipts are emitted before a native modal",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
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

TEST_CASE( "item info is a semantic child and restores its selector parent",
           "[semantic_surface]" )
{
    scoped_imgui_context imgui;
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
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

    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "item_info" ) {
            return;
        }
        REQUIRE( descriptor.breadcrumbs == std::vector<std::string>{ "Inventory", "Item info" } );
        REQUIRE( descriptor.valid_actions.size() == 1 );
        REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                           "close-details", "item_info.close", std::nullopt, {} } ) );
    } );
    item_info_data data( "rock", "rock", {}, {} );
    iteminfo_window details( data, point::zero, 80, 0 );
    details.execute();

    REQUIRE( receipts.size() == 2 );
    CHECK( receipts.front().request_id == "details" );
    CHECK( receipts.front().accepted );
    CHECK( receipts.front().resulting_frame_id != inventory_descriptor.frame_id );
    CHECK( receipts.back().request_id == "close-details" );
    CHECK( receipts.back().accepted );
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
    const semantic_action_descriptor &action ) {
        return action.id == "inventory.select" && action.stable_id == selected_id;
    } );
    REQUIRE( select != actions.end() );
    CHECK( std::any_of( actions.begin(), actions.end(), [&selected_id](
    const semantic_action_descriptor &action ) {
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

TEST_CASE( "inventory selector keeps UID controls bound across common actions", "[semantic_surface]" )
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
    const auto live_rock = std::find_if( live_items.begin(), live_items.end(), []( const item_location &location ) {
        return location && location->typeId() == itype_id( "rock" );
    } );
    const auto second_rock = std::find_if( std::next( live_rock ), live_items.end(), []( const item_location &location ) {
        return location && location->typeId() == itype_id( "rock" );
    } );
    const auto live_bottle = std::find_if( live_items.begin(), live_items.end(), []( const item_location &location ) {
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
    const auto advertised_for = [&actions]( const std::string &action_id,
    const std::string &stable_id ) {
        return std::any_of( actions.begin(), actions.end(), [&action_id, &stable_id](
        const semantic_action_descriptor &action ) {
            return action.id == action_id && action.stable_id == stable_id && action.enabled;
        } );
    };
    CHECK( advertised_for( "inventory.select", rock_id ) );
    CHECK( advertised_for( "inventory.select", second_rock_id ) );
    CHECK( advertised_for( "inventory.details", rock_id ) );
    CHECK( advertised_for( "inventory.contents", bottle_id ) );
    CHECK( advertised_for( "inventory.toggle", rock_id ) );
    CHECK( advertised_for( "inventory.wield", rock_id ) );
    CHECK( advertised_for( "inventory.wear", rock_id ) );

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
    const semantic_action_descriptor &action ) {
        return action.id == "inventory.wield" && action.stable_id == second_rock_id && !action.enabled;
    } ) );

    std::vector<item_location> item_locations = avatar.all_items_loc();
    auto rock_location = std::find_if( item_locations.begin(), item_locations.end(),
    [&rock_id]( const item_location &location ) {
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
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor &descriptor ) {
        descriptors.push_back( descriptor );
    } );
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
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
    }, [&actions_consumed]( const semantic_action_request &request ) {
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
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor &descriptor ) {
        descriptors.push_back( descriptor );
    } );

    std::optional<semantic_surface_scope> editor;
    const auto make_editor = [&manager, &editor]() {
        editor.emplace( manager, "map_editor", "Map editor", std::map<std::string, std::string>{},
                        std::vector<semantic_action_descriptor>{
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
        CHECK( field_descriptor.breadcrumbs == std::vector<std::string>{ "World", "Select field" } );
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
        CHECK( manager.top()->breadcrumbs == std::vector<std::string>{ "World", "Select intensity" } );
        REQUIRE( manager.withhold_parent_authority_until_recreated( intensity_menu.surface_id() ) );
    }
    CHECK_FALSE( manager.top() );

    // The edit loop recreates a fresh owner only after the nested menu exits.
    make_editor();
    REQUIRE( manager.top() );
    CHECK( manager.top()->kind == "map_editor" );
    CHECK( manager.top()->surface_id != retired_editor.surface_id );
    CHECK( std::none_of( descriptors.begin(), descriptors.end(),
    []( const semantic_surface_descriptor &descriptor ) {
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
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
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
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    const std::string zone_id = "zone-42";
    const std::string faction = "your_followers";
    int revision = 7;
    bool enabled = false;
    std::string native_action;
    const auto zone_consumer = [&zone_id, &faction, &revision, &enabled, &native_action]
    ( const semantic_action_request &request ) {
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
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
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
    manager.set_descriptor_observer( [&manager, &menu_descriptor, &selection_requested, &choice_requested](
    const semantic_surface_descriptor &descriptor ) {
        menu_descriptor = descriptor;
        if( descriptor.kind != "menu" ) {
            return;
        }
        const auto action = std::find_if( descriptor.valid_actions.begin(),
        descriptor.valid_actions.end(), []( const semantic_action_descriptor &candidate ) {
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
            descriptor.valid_actions.end(), []( const semantic_action_descriptor &candidate ) {
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
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
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
    stale_id]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "menu" ) {
            return;
        }
        const bool filter_required = descriptor.payload.at( "filter_required" ) == "true";
        if( filter_required && !stale_requested ) {
            stale_requested = true;
            REQUIRE( std::none_of( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
            []( const semantic_action_descriptor &action ) {
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
            []( const semantic_action_descriptor &action ) {
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
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    semantic_surface_scope world( manager, "world", "World" );
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "menu" ) {
            return;
        }
        const auto action = std::find_if( descriptor.valid_actions.begin(),
        descriptor.valid_actions.end(), []( const semantic_action_descriptor &candidate ) {
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
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
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
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor &descriptor ) {
        descriptors.push_back( descriptor );
    } );

    std::optional<semantic_surface_scope> world;
    world.emplace( manager, "world", "World", std::map<std::string, std::string>{},
                   std::vector<semantic_action_descriptor>{
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
    [&world_frame]( const semantic_surface_descriptor &descriptor ) {
        return descriptor.surface_id == world_frame.surface_id &&
               descriptor.frame_id != world_frame.frame_id;
    } ) );
}

TEST_CASE( "a sentence prompt receipt survives its nested chat unwind", "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_action_receipt> receipts;
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );

    std::optional<semantic_surface_scope> world;
    world.emplace( manager, "world", "World" );
    std::optional<semantic_surface_scope> prompt;
    prompt.emplace( manager, "string_prompt", "Text input", std::map<std::string, std::string>{},
                    std::vector<semantic_action_descriptor>{ { "prompt.submit", "", "Submit", true } },
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
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor &descriptor ) {
        descriptors.push_back( descriptor );
    } );
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
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
    CHECK( std::none_of( descriptors.begin() + 2, descriptors.end(), []( const semantic_surface_descriptor &d ) {
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
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor &descriptor ) {
        descriptors.push_back( descriptor );
    } );
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
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
    CHECK( std::none_of( descriptors.begin() + 2, descriptors.end(), []( const semantic_surface_descriptor &d ) {
        return d.kind == "world" || d.kind == "inventory_item_menu";
    } ) );
}

TEST_CASE( "withheld main-menu quit releases its owner before publishing confirmation",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    std::vector<semantic_surface_descriptor> descriptors;
    std::vector<semantic_action_receipt> receipts;
    manager.set_descriptor_observer( [&descriptors]( const semantic_surface_descriptor &descriptor ) {
        descriptors.push_back( descriptor );
    } );
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );

    std::optional<semantic_surface_scope> main_menu;
    main_menu.emplace( manager, "main_menu", "Main menu", std::map<std::string, std::string>{},
    std::vector<semantic_action_descriptor>{ { "main_menu.quit", "", "Quit", true } },
    [&manager]( const semantic_action_request &request ) {
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
    CHECK( manager.top()->breadcrumbs == std::vector<std::string>{ "YESNO" } );
    REQUIRE( receipts.size() == 1 );
    CHECK( receipts.front().request_id == "quit" );
    CHECK( receipts.front().resulting_frame_id == manager.top()->frame_id );
    CHECK( std::none_of( descriptors.begin() + 1, descriptors.end(),
    []( const semantic_surface_descriptor &descriptor ) {
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
    manager.set_receipt_observer( [&receipts]( const semantic_action_receipt &receipt ) {
        receipts.push_back( receipt );
    } );

    uilist disabled;
    disabled.entries = { uilist_entry( 1, false, 'd', "disabled" ) };
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor &descriptor ) {
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
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor &descriptor ) {
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
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor &descriptor ) {
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
        descriptor.valid_actions.end(), [&target]( const semantic_action_descriptor &candidate ) {
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
    manager.set_descriptor_observer( [&manager, &saw_map_menu]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "menu" ) {
            return;
        }
        const std::string &text = descriptor.payload.at( "text" );
        const std::string target = text == "Debug Functions" ? "Map…" : "Map editor";
        const auto action = std::find_if( descriptor.valid_actions.begin(), descriptor.valid_actions.end(),
        [&target]( const semantic_action_descriptor &candidate ) {
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
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind == "prompt" ) {
            const semantic_action_descriptor &action = descriptor.valid_actions.front();
            REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                               "accept-popup", "prompt.choose", action.stable_id, {} } ) );
        }
    } );

    const query_popup::result result = query_popup().message( "%s", "Continue?" ).option( "YES" ).query();
    CHECK( result.action == "YES" );
}

TEST_CASE( "string prompt accepts constraint-valid structured semantic text",
           "[semantic_surface]" )
{
    semantic_surface_manager manager( "test-run" );
    semantic_surface_manager_session session( manager );
    manager.set_descriptor_observer( [&manager]( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind == "string_prompt" ) {
            REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                               "submit-text", "prompt.submit", std::nullopt,
                                               { { "text", "42" } } } ) );
        }
    } );

    string_input_popup prompt;
    prompt.title( "Number" ).text( "" ).max_length( 2 ).only_digits( true );
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
    manager.set_descriptor_observer( [&manager, &saw_prompt, &rejected_wrong_surface, &rejected_stale_frame]
    ( const semantic_surface_descriptor &descriptor ) {
        if( descriptor.kind != "string_prompt" ) {
            return;
        }
        saw_prompt = true;
        CHECK( descriptor.valid_actions[0].id == "prompt.submit" );
        rejected_wrong_surface = !manager.submit_request( { "test-run", "wrong-surface", descriptor.frame_id,
                                                             "wrong", "prompt.submit", std::nullopt,
                                                             { { "text", "0'152,0'49,0" } } } );
        rejected_stale_frame = !manager.submit_request( { "test-run", descriptor.surface_id, "stale-frame",
                                                           "stale", "prompt.submit", std::nullopt,
                                                           { { "text", "0'152,0'49,0" } } } );
        REQUIRE( manager.submit_request( { "test-run", descriptor.surface_id, descriptor.frame_id,
                                           "submit", "prompt.submit", std::nullopt,
                                           { { "text", "0'152,0'49,0" } } } ) );
    } );

    string_input_popup_imgui popup( 65, "", "Teleport where?" );
    CHECK( popup.query() == "0'152,0'49,0" );
    CHECK( saw_prompt );
    CHECK( rejected_wrong_surface );
    CHECK( rejected_stale_frame );
}
