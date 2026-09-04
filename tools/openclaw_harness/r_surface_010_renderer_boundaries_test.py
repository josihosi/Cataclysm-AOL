#!/usr/bin/env python3
"""Focused renderer and hard-stop proof for the R-SURFACE-010 v2 projection."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import cockpit  # noqa: E402
import cockpit_tui  # noqa: E402
import semantic_state  # noqa: E402


def descriptor(kind: str, frame_id: str, actions: list[dict[str, object]]) -> dict[str, object]:
    return {
        "event": "surface_descriptor", "schema_version": 1, "run_id": "renderer-proof",
        "surface_id": f"renderer-proof:{kind}", "frame_id": frame_id, "kind": kind,
        "breadcrumbs": ["World", kind], "payload": {"owner": kind}, "valid_actions": actions,
    }


class RendererUnsupportedBoundariesTest(unittest.TestCase):
    def test_terminal_projection_preserves_the_tiles_v2_surface_verbatim(self) -> None:
        frames = [
            descriptor("inventory", "renderer-proof:inventory", [{
                "id": "inventory.cancel", "stable_id": "", "label": "Cancel", "enabled": True,
            }]),
            descriptor("direction", "renderer-proof:direction", [{
                "id": "direction.choose", "stable_id": "north", "label": "north", "enabled": True,
            }]),
        ]
        index = [0]
        channel = cockpit.CockpitRunChannel(lambda: frames[index[0]])
        for expected in frames:
            observed = channel.observe()
            terminal = cockpit_tui.render_state(observed, {"state": "active"})
            self.assertEqual(terminal["schema"], "caol-cockpit-tui-v2")
            self.assertEqual(terminal["active_surface"], observed["surface"])
            self.assertEqual(terminal["breadcrumbs"], observed["breadcrumbs"])
            self.assertEqual(
                [command["action_id"] for command in terminal["commands"] if command["kind"] == "primitive"],
                observed["advertised_actions"],
            )
            self.assertEqual(observed["frame_id"], expected["frame_id"])
            index[0] += 1

    def test_unsupported_stops_without_parent_or_legacy_action_fallback(self) -> None:
        unsupported = descriptor("unsupported", "renderer-proof:unsupported", [])
        dispatches: list[tuple[str, str]] = []

        def dispatch(frame: dict[str, object], action_id: str) -> dict[str, object]:
            dispatches.append((str(frame["frame_id"]), action_id))
            return {}

        channel = cockpit.CockpitRunChannel(lambda: unsupported, dispatch)
        observed = channel.observe()
        terminal = cockpit_tui.render_state(observed, {"state": "active"})
        rejected = channel.act(
            observation_id=observed["observation_id"], action_id="world.inventory",
        )

        self.assertEqual(observed["surface"]["actions"], [])
        self.assertEqual(observed["advertised_actions"], [])
        self.assertEqual(observed["automation"], {
            "state": "stopped", "reason": "native_unsupported_surface",
        })
        self.assertEqual([item for item in terminal["commands"] if item["kind"] == "primitive"], [])
        self.assertEqual(rejected["error"], "action_not_advertised")
        self.assertEqual(dispatches, [])

    def test_trace_rejects_unsupported_with_a_disabled_cached_parent_action(self) -> None:
        invalid = descriptor("unsupported", "renderer-proof:unsupported", [{
            "id": "world.inventory", "stable_id": "", "label": "Inventory", "enabled": False,
        }])
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = Path(temporary)
            trace = run_dir / "semantic.native.log"
            trace.write_text(
                semantic_state.SEMANTIC_STEP_PREFIX + __import__("json").dumps(invalid) + "\n",
                encoding="utf-8",
            )
            events, status = semantic_state.read_semantic_step_trace(trace, run_dir, "renderer-proof")
        self.assertEqual(events, [])
        self.assertEqual(status, "malformed_semantic_surface_descriptor")


if __name__ == "__main__":
    unittest.main()
