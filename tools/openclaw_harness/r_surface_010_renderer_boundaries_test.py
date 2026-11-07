#!/usr/bin/env python3
"""Focused renderer and hard-stop proof for the R-SURFACE-010 v2 projection."""

from __future__ import annotations

import sys
import json
import copy
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
    def test_unnamed_dialogue_is_preserved_without_weakening_authority(self) -> None:
        # Native W5 frame 1202 has an unnamed speaker and two distinct
        # dialogue choices plus a cancel action. No speaker is invented.
        native = descriptor("dialogue", "renderer-proof:frame:1202", [
            {"id": "dialogue.choose", "stable_id": "dialogue-response-1",
             "label": "Contact it.", "enabled": True},
            {"id": "dialogue.choose", "stable_id": "dialogue-response-2",
             "label": "Ignore it.", "enabled": True},
            {"id": "dialogue.cancel", "stable_id": "", "label": "Goodbye", "enabled": True},
        ])
        native["breadcrumbs"] = [""]
        named = copy.deepcopy(native)
        named["breadcrumbs"] = ["Named speaker"]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = root / "semantic.native.events.jsonl"

            def read(frame):
                path.write_text(semantic_state.SEMANTIC_STEP_PREFIX + json.dumps(frame) + "\n")
                return semantic_state.read_semantic_step_trace(path, root, "renderer-proof")

            for frame in (native, named):
                events, status = read(frame)
                self.assertEqual(status, "ok")
                self.assertEqual(events[0]["breadcrumbs"], frame["breadcrumbs"])
                observed = cockpit.CockpitRunChannel(lambda: events[0]).observe()
                self.assertEqual(observed["breadcrumbs"], frame["breadcrumbs"])
                self.assertEqual(observed["frame_id"], frame["frame_id"])
                self.assertEqual(observed["advertised_actions"],
                                 ["dialogue.choose", "dialogue.choose", "dialogue.cancel"])

            # Native surfaces inherit each parent's own label, including an
            # unnamed dialogue speaker, even when the child is not dialogue.
            for labels in (["", "Follower rules"], ["", "Follower rules", "Confirm"]):
                child = descriptor("menu", "renderer-proof:frame:child", [{
                    "id": "menu.cancel", "stable_id": "", "label": "Cancel", "enabled": True,
                }])
                child["breadcrumbs"] = labels
                events, status = read(child)
                self.assertEqual(status, "ok")
                observed = cockpit.CockpitRunChannel(lambda: events[0]).observe()
                self.assertEqual(observed["breadcrumbs"], labels)
                self.assertEqual(observed["advertised_actions"], ["menu.cancel"])

            successor = descriptor("world", "renderer-proof:frame:1203", [])
            calls = []
            def dispatch(frame, action_id, stable_id=None):
                calls.append((frame["frame_id"], action_id, stable_id))
                return {"native_receipt": {
                    "requested_frame_id": frame["frame_id"],
                    "requested_surface_id": frame["surface_id"],
                    "consuming_surface_id": frame["surface_id"],
                    "action_id": action_id, "stable_id": stable_id or "", "accepted": True,
                }, "next_frame": successor}
            channel = cockpit.CockpitRunChannel(lambda: native, dispatch)
            observed = channel.observe()
            rejected = channel.act(observation_id=observed["observation_id"],
                                   action_id="dialogue.choose", stable_id="invented")
            self.assertFalse(rejected["ok"])
            self.assertEqual(calls, [])
            acted = channel.act(observation_id=observed["observation_id"],
                                action_id="dialogue.choose", stable_id="dialogue-response-2")
            self.assertTrue(acted["ok"], acted)
            self.assertEqual(calls, [(native["frame_id"], "dialogue.choose", "dialogue-response-2")])
            replay = channel.act(observation_id=observed["observation_id"],
                                 action_id="dialogue.choose", stable_id="dialogue-response-2")
            self.assertEqual(replay["error"], "duplicate_submission")
            self.assertEqual(len(calls), 1)

            invalid = []
            for change in (
                {"run_id": "foreign"}, {"run_id": None}, {"frame_id": ""},
                {"frame_id": 123}, {"surface_id": ""}, {"surface_id": 123},
                {"kind": "menu"}, {"kind": None}, {"breadcrumbs": []}, {"breadcrumbs": [None]},
                {"breadcrumbs": ["Speaker", None]}, {"payload": {"speaker": None}},
                {"kind": "menu", "breadcrumbs": ["", ""]},
            ):
                frame = copy.deepcopy(native); frame.update(change); invalid.append(frame)
            duplicate = copy.deepcopy(native)
            duplicate["valid_actions"].append(copy.deepcopy(duplicate["valid_actions"][0]))
            invalid.append(duplicate)
            wrong_action = copy.deepcopy(native)
            wrong_action["valid_actions"][0]["enabled"] = "true"
            invalid.append(wrong_action)
            for frame in invalid:
                events, status = read(frame)
                self.assertEqual(events, [])
                self.assertIn(status, {"contamination", "malformed_semantic_surface_descriptor"})
            # Cockpit validates identity and action shape independently of the
            # run-bound trace reader; contamination is the reader's boundary.
            for frame in invalid[1:]:
                with self.assertRaises(ValueError):
                    cockpit.CockpitRunChannel._surface_descriptor(frame)

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
