#!/usr/bin/env python3
"""Fail closed when native input-owning source files escape semantic coverage."""

from __future__ import annotations

import pathlib
import re
import unittest


ROOT = pathlib.Path( __file__ ).resolve().parents[2]
SOURCE_ROOT = ROOT / "src"
DIRECT_INPUT = re.compile( r"(?:\.|->)\s*(?:handle_input|get_input_event)\s*\(" )

# This is deliberately a source inventory, rather than a menu quota.  A source
# file enters only after its direct input owner has been classified.  Supported
# files install an explicit scope; hard-stop files are covered by the native
# input-context boundary until a focused adapter is added.  Mixed files are
# classified by their individual direct-input loops below: filename membership
# cannot stand in for the owning loop's actual semantic boundary.
SUPPORTED = {
    "src/game.cpp",
    "src/inventory_ui.cpp",
    # MESSAGE_LOG has a focused scope with navigation, filtering and close
    # actions.  Its filter field is owned by the existing string_prompt child.
    "src/messages.cpp",
    # NPC inspection has two direct native loops.  Both install focused
    # semantic scopes (the inspector and its item-detail child) rather than
    # falling through to the actionless input-context boundary.
    "src/npc_inspection.cpp",
    "src/editmap.cpp",
    "src/debug_menu.cpp",
    "src/overmap_ui.cpp",
    "src/popup.cpp",
    "src/string_input_popup.cpp",
    "src/uilist.cpp",
}
TRANSPORT = {
    "src/animation.cpp",
    "src/debug.cpp",
    "src/input.cpp",
    "src/input_context.cpp",
}
HARD_STOP = {
    "src/advanced_inv.cpp", "src/armor_layers.cpp", "src/auto_note.cpp",
    "src/auto_pickup.cpp", "src/bionics_ui.cpp", "src/bodygraph.cpp", "src/character_health.cpp",
    "src/color.cpp", "src/computer_session.cpp", "src/construction.cpp", "src/crafting_gui.cpp",
    "src/debug_console.cpp", "src/diary_ui.cpp", "src/distraction_manager.cpp",
    "src/do_turn.cpp", "src/end_screen.cpp", "src/faction_camp.cpp",
    "src/faction_ui.cpp", "src/game_inventory.cpp", "src/help.cpp", "src/iexamine.cpp",
    "src/imgui_demo.cpp", "src/input_popup.cpp", "src/iuse.cpp", "src/iuse_software_kitten.cpp",
    "src/iuse_software_lightson.cpp", "src/iuse_software_minesweeper.cpp",
    "src/iuse_software_snake.cpp", "src/iuse_software_sokoban.cpp", "src/main_menu.cpp",
    "src/martialarts.cpp", "src/medical_ui.cpp", "src/mission_companion.cpp",
    "src/mission_ui.cpp", "src/morale.cpp", "src/mutation_ui.cpp", "src/newcharacter.cpp",
    "src/npctalk_rules.cpp", "src/options.cpp", "src/output.cpp",
    "src/panels.cpp", "src/player_display.cpp", "src/proficiency_ui.cpp",
    "src/recipe_dictionary.cpp", "src/safemode_ui.cpp", "src/scores_ui.cpp",
    "src/smart_controller_ui.cpp", "src/string_editor_window.cpp", "src/study_zone_ui.cpp", "src/surroundings_menu.cpp",
    "src/ui_extended_description.cpp", "src/ui_iteminfo.cpp", "src/ui_manager.h", "src/veh_interact.cpp",
    "src/veh_shape.cpp", "src/worldfactory.cpp", "src/zone_manager_ui.cpp",
}

MIXED_OWNER_SITES = {
    "src/action.cpp": {
        "scope": '"direction"',
        "direct_input": "action = ctxt.handle_input();",
        "reject_guard": "if( semantic_request_pending )",
    },
    "src/npctalk.cpp": {
        "scope": '"dialogue"',
        "direct_input": "action = ctxt.handle_input();",
        "reject_guard": "if( semantic_request_pending && !semantic_response_index )",
    },
    "src/ranged.cpp": {
        "scope": '"target"',
        "direct_input": "action = ctxt.handle_input( timeout );",
        "reject_guard": "if( semantic_request_pending && !semantic_native_action )",
    },
}


def discovered_sources() -> set[str]:
    return {
        path.relative_to( ROOT ).as_posix()
        for path in SOURCE_ROOT.rglob( "*" )
        if path.suffix in { ".cpp", ".h" } and DIRECT_INPUT.search( path.read_text() )
    }


class InputOwnerCoverageTest( unittest.TestCase ):
    def test_every_direct_native_input_source_is_classified( self ) -> None:
        classified = SUPPORTED | HARD_STOP | TRANSPORT | set( MIXED_OWNER_SITES )
        self.assertSetEqual( discovered_sources(), classified )

    def test_mixed_direct_input_owners_are_classified_by_loop( self ) -> None:
        for source_path, classification in MIXED_OWNER_SITES.items():
            source = ( ROOT / source_path ).read_text()
            scope_index = source.index( classification["scope"] )
            guard_index = source.index( classification["reject_guard"], scope_index )
            input_index = source.index( classification["direct_input"], guard_index )
            self.assertLess( scope_index, guard_index, source_path )
            self.assertLess( guard_index, input_index, source_path )

    def test_incomplete_owners_have_a_native_actionless_stop( self ) -> None:
        boundary = ( SOURCE_ROOT / "input_context.cpp" ).read_text()
        self.assertIn( "unsupported_semantic_input_owner", boundary )
        self.assertIn( '"unclassified_native_input_owner"', boundary )
        self.assertIn( '"unsupported"', boundary )

    def test_debug_menu_has_a_focused_spell_editor_owner( self ) -> None:
        source = ( SOURCE_ROOT / "debug_menu.cpp" ).read_text()
        self.assertIn( '"debug_spells"', source )
        self.assertIn( '"debug_spells.select"', source )
        self.assertIn( '"debug_spells.close"', source )

    def test_map_editor_has_a_focused_semantic_owner( self ) -> None:
        source = ( SOURCE_ROOT / "editmap.cpp" ).read_text()
        self.assertIn( '"map_editor"', source )
        self.assertIn( '"editmap.move_target"', source )
        self.assertIn( '"editmap.close"', source )

    def test_npc_inspection_has_focused_parent_and_child_owners( self ) -> None:
        source = ( SOURCE_ROOT / "npc_inspection.cpp" ).read_text()
        self.assertIn( '"npc_inspection"', source )
        self.assertIn( '"npc_inspection.close"', source )
        self.assertIn( '"npc_inspection.item_details"', source )
        self.assertIn( '"npc_item_info"', source )
        self.assertIn( '"npc_item_info.close"', source )

    def test_message_log_has_focused_navigation_and_filter_owners( self ) -> None:
        source = ( SOURCE_ROOT / "messages.cpp" ).read_text()
        world_source = ( SOURCE_ROOT / "handle_action.cpp" ).read_text()
        self.assertIn( '"message_log"', source )
        self.assertIn( '"message_log.scroll_up"', source )
        self.assertIn( '"message_log.scroll_down"', source )
        self.assertIn( '"message_log.page_up"', source )
        self.assertIn( '"message_log.page_down"', source )
        self.assertIn( '"message_log.filter"', source )
        self.assertIn( '"message_log.reset_filter"', source )
        self.assertIn( '"message_log.close"', source )
        self.assertIn( 'std::exchange( semantic_action, "" )', source )
        # The filter's string_prompt child must remain its actual owner while
        # a queued prompt receipt is consumed.  A single draw-only query
        # returned to this loop, allowing input_context to consume that child
        # request against a replacement owner as wrong_surface.
        self.assertIn( 'filter.query( active_semantic_surface_manager() != nullptr );', source )
        filter_input = source.index( 'if( filtering ) {' )
        nested_query = source.index( 'filter.query( active_semantic_surface_manager() != nullptr );', filter_input )
        parent_input = source.index( 'ctxt.handle_input()', nested_query )
        self.assertLess( filter_input, nested_query )
        self.assertLess( nested_query, parent_input )
        self.assertIn( '"world.messages", "messages"', world_source )
        self.assertIn( 'request.action_id == "world.messages"', world_source )
        self.assertIn( 'act = ACTION_MESSAGES;', world_source )

    def test_dialogue_rejection_cannot_fall_through_to_native_input( self ) -> None:
        source = ( SOURCE_ROOT / "npctalk.cpp" ).read_text()
        loop_start = source.index( "if( semantic_scope ) {", source.index( "semantic_response_index" ) )
        loop_end = source.index( "if( semantic_response_index ) {", loop_start )
        semantic_gate = source[loop_start:loop_end]
        self.assertIn( "const bool semantic_request_pending = semantic_manager->has_pending_request();", semantic_gate )
        self.assertIn( "if( semantic_request_pending && !semantic_response_index )", semantic_gate )
        self.assertIn( "continue;", semantic_gate )


if __name__ == "__main__":
    unittest.main()
