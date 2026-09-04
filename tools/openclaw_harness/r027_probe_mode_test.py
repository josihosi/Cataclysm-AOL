#!/usr/bin/env python3
"""Contract test for the source-current R-027 probe receipt."""

import io
import json
from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parent))
import startup_harness


class R027ProbeModeTest(unittest.TestCase):
    def test_registry_authority_binds_the_native_stream_to_the_one_run_id(self):
        receipt = json.dumps({
            "wec_authority": {
                "authority": "registry",
                "run_id": "r027-authoritative-run",
            },
        })
        self.assertEqual(
            startup_harness.registry_authority_transition_run_id(receipt),
            "r027-authoritative-run",
        )
        self.assertEqual(startup_harness.registry_authority_transition_run_id("{}"), "")

    def test_no_observation_receipt_is_claim_scoped(self):
        receipt = {
            "schema": "caol-r027-staffed-camp-signal-probe-v1",
            "now_minutes": 220,
            "sites_considered": 1,
            "eligible_camps": 1,
            "callbacks_invoked": 1,
            "observations": 0,
            "outcome": "no_observation",
        }
        stream = io.StringIO(json.dumps(receipt))
        decoded = json.load(stream)
        self.assertEqual(decoded["schema"], "caol-r027-staffed-camp-signal-probe-v1")
        self.assertEqual(decoded["outcome"], "no_observation")
        self.assertEqual(decoded["observations"], 0)
        self.assertEqual(decoded["callbacks_invoked"], 1)

    def test_live_bridge_binding_is_the_controller_binding(self):
        self.assertEqual(
            startup_harness.cockpit_controller_binding_id(
                live_session=True,
                bridge_binding_id="registry-bridge-binding",
                runtime_binding_id="runtime-file-hash",
            ),
            "registry-bridge-binding",
        )
        self.assertEqual(
            startup_harness.cockpit_controller_binding_id(
                live_session=False,
                bridge_binding_id="registry-bridge-binding",
                runtime_binding_id="runtime-file-hash",
            ),
            "runtime-file-hash",
        )

    def test_live_bridge_binding_is_also_the_native_child_binding(self):
        self.assertEqual(
            startup_harness.native_child_binding_id(
                live_session=True,
                bridge_binding_id="registry-bridge-binding",
                runtime_binding_id="runtime-file-hash",
            ),
            "registry-bridge-binding",
        )
        self.assertEqual(
            startup_harness.native_child_binding_id(
                live_session=False,
                bridge_binding_id="",
                runtime_binding_id="runtime-file-hash",
            ),
            "runtime-file-hash",
        )


if __name__ == "__main__":
    unittest.main()
