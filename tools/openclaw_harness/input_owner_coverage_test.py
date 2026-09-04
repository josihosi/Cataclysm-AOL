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
# input-context boundary until a focused adapter is added.
SUPPORTED = {
    "src/game.cpp",
    "src/inventory_ui.cpp",
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
    "src/action.cpp", "src/advanced_inv.cpp", "src/armor_layers.cpp", "src/auto_note.cpp",
    "src/auto_pickup.cpp", "src/bionics_ui.cpp", "src/bodygraph.cpp", "src/character_health.cpp",
    "src/color.cpp", "src/computer_session.cpp", "src/construction.cpp", "src/crafting_gui.cpp",
    "src/debug_console.cpp", "src/diary_ui.cpp", "src/distraction_manager.cpp",
    "src/do_turn.cpp", "src/end_screen.cpp", "src/faction_camp.cpp",
    "src/faction_ui.cpp", "src/game_inventory.cpp", "src/help.cpp", "src/iexamine.cpp",
    "src/imgui_demo.cpp", "src/input_popup.cpp", "src/iuse.cpp", "src/iuse_software_kitten.cpp",
    "src/iuse_software_lightson.cpp", "src/iuse_software_minesweeper.cpp",
    "src/iuse_software_snake.cpp", "src/iuse_software_sokoban.cpp", "src/main_menu.cpp",
    "src/martialarts.cpp", "src/medical_ui.cpp", "src/messages.cpp", "src/mission_companion.cpp",
    "src/mission_ui.cpp", "src/morale.cpp", "src/mutation_ui.cpp", "src/newcharacter.cpp",
    "src/npctalk.cpp", "src/npctalk_rules.cpp", "src/options.cpp", "src/output.cpp",
    "src/panels.cpp", "src/player_display.cpp", "src/proficiency_ui.cpp", "src/ranged.cpp",
    "src/recipe_dictionary.cpp", "src/safemode_ui.cpp", "src/scores_ui.cpp",
    "src/smart_controller_ui.cpp", "src/string_editor_window.cpp", "src/study_zone_ui.cpp", "src/surroundings_menu.cpp",
    "src/ui_extended_description.cpp", "src/ui_iteminfo.cpp", "src/ui_manager.h", "src/veh_interact.cpp",
    "src/veh_shape.cpp", "src/worldfactory.cpp", "src/zone_manager_ui.cpp",
}


def discovered_sources() -> set[str]:
    return {
        path.relative_to( ROOT ).as_posix()
        for path in SOURCE_ROOT.rglob( "*" )
        if path.suffix in { ".cpp", ".h" } and DIRECT_INPUT.search( path.read_text() )
    }


class InputOwnerCoverageTest( unittest.TestCase ):
    def test_every_direct_native_input_source_is_classified( self ) -> None:
        classified = SUPPORTED | HARD_STOP | TRANSPORT
        self.assertSetEqual( discovered_sources(), classified )

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


if __name__ == "__main__":
    unittest.main()
