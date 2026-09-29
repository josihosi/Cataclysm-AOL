"""R046 clean-exit reconciliation on retained R043 runs and fail-closed controls."""
from __future__ import annotations

import contextlib
import copy
import hashlib
import io
import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path

import startup_harness as harness

ROOT = next(parent for parent in Path(__file__).resolve().parents
            if (parent / "tools/openclaw_harness/startup_harness.py").exists())
CASES = {
    "quiet_repeat": (
        ".userdata/first-smoke-043-quiet-repeat-20260929/harness_runs/20260929_195925_d3bea9b65a8c4cdc8355b6a25e8ee320",
        ".userdata/openclaw_harness/bridge-sessions/selected-b511d9f5a4f04304bccef5dd7f6641e7",
    ),
    "quiet_individual": (
        ".userdata/first-smoke-043-quiet-individual-20260929/harness_runs/20260929_200930_010d63c818b04df8948e7bdbab23d44a",
        ".userdata/openclaw_harness/bridge-sessions/selected-e936edce4fcd43d9ba0a2b5e1e57456c",
    ),
    "combat_tab": (
        ".userdata/first-smoke-043-combat-tab-20260929/harness_runs/20260929_201507_1c5afffa9e2c473bae7474ec1e443e86",
        ".userdata/openclaw_harness/bridge-sessions/selected-efd014adfaab4db8868583a3b857af0c",
    ),
}


def load_case(name):
    run, bridge = CASES[name]
    run_dir, bridge_dir = ROOT / run, ROOT / bridge
    return json.loads((run_dir / "probe.report.json").read_text()), run_dir, bridge_dir


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class CleanExitReportTest(unittest.TestCase):
    def test_retained_runs_reconcile_without_changing_original_receipts(self):
        for name in CASES:
            with self.subTest(name=name):
                report, run_dir, bridge_dir = load_case(name)
                report_path = run_dir / "probe.report.json"
                before = sha(report_path)
                self.assertEqual(report["verdict"], "blocked_feature_phase_process_exited")
                early = harness.build_feature_debug_guard(
                    debug_capture=report["feature_debug_guard"]["debug_capture"],
                    screen_probe=report["feature_debug_guard"]["screen_probe"],
                    screen_summary=report["screen"]["after"], process_alive=False,
                    require_screen_observability=False,
                    expected_clean_terminal_exit=False,
                )
                self.assertEqual(early["verdict"], "red_feature_phase_process_exited")
                result = harness.reconcile_completed_cockpit_exit(
                    report, run_dir=run_dir, bridge_session_dir=bridge_dir)
                self.assertEqual(result["status"], "accepted", result)
                self.assertEqual(report["verdict"], "artifacts_matched")
                self.assertEqual(report["feature_debug_guard"]["status"], "green")
                self.assertEqual(report["step_ledger_summary"]["status"], "green_step_local_proof")
                self.assertTrue(report["feature_proof"])
                self.assertEqual(report["evidence_class"], "feature-path")
                self.assertEqual(sha(report_path), before)

    def test_failure_controls_remain_red(self):
        base, run_dir, bridge_dir = load_case("quiet_repeat")
        quit_request = json.loads((run_dir / "cockpit.player_quit.json").read_text())
        process = json.loads((bridge_dir / "game-process.json").read_text())
        exit_record = json.loads((bridge_dir / "game-process-exit.json").read_text())
        terminal = harness.cockpit_terminal_step_receipts(run_dir)
        for name in ("crash", "nonzero", "unrequested", "wrong_binding", "incomplete_cleanup", "missing_native_quit"):
            with self.subTest(name=name):
                report, quit_copy, process_copy, exit_copy, terminal_copy = copy.deepcopy(
                    (base, quit_request, process, exit_record, terminal))
                if name == "crash":
                    exit_copy["exit_code"] = -11
                elif name == "nonzero":
                    exit_copy["exit_code"] = 1
                elif name == "unrequested":
                    report["steps"][-1]["cockpit_live_session"]["final"]["termination_requested"] = False
                    quit_copy = {}
                elif name == "wrong_binding":
                    exit_copy["binding_id"] = "another-binding"
                elif name == "incomplete_cleanup":
                    report["cleanup"]["status"] = "deferred_to_scenario_terminalization"
                elif name == "missing_native_quit":
                    terminal_copy[-2]["accepted"] = False
                result = harness.assess_completed_cockpit_exit(
                    report, quit_request=quit_copy, bridge_process=process_copy,
                    bridge_exit=exit_copy, terminal_steps=terminal_copy)
                self.assertEqual(result["status"], "rejected", result)
                self.assertTrue(result["issues"])
                self.assertEqual(report["verdict"], "blocked_feature_phase_process_exited")

    def test_immediate_already_exited_cockpit_cleanup_is_also_intentional(self):
        report, run_dir, bridge_dir = load_case("quiet_repeat")
        report["steps"][-1]["cockpit_live_session"]["final"]["cleanup"]["status"] = "already_exited"
        result = harness.reconcile_completed_cockpit_exit(
            report, run_dir=run_dir, bridge_session_dir=bridge_dir)
        self.assertEqual(result["status"], "accepted", result)
        self.assertEqual(report["verdict"], "artifacts_matched")

    def test_missing_exit_artifact_rejects(self):
        report, run_dir, bridge_dir = load_case("quiet_repeat")
        with tempfile.TemporaryDirectory() as temporary:
            bridge_copy = Path(temporary)
            shutil.copyfile(bridge_dir / "game-process.json", bridge_copy / "game-process.json")
            result = harness.reconcile_completed_cockpit_exit(
                report, run_dir=run_dir, bridge_session_dir=bridge_copy)
        self.assertEqual(result["status"], "rejected")
        self.assertEqual(report["verdict"], "blocked_feature_phase_process_exited")

    def test_real_feature_error_remains_red_after_clean_exit(self):
        report, run_dir, bridge_dir = load_case("quiet_repeat")
        report["feature_debug_guard"]["debug_capture"]["error_evidence_lines"] = [
            "ERROR : unrelated feature failure"]
        result = harness.reconcile_completed_cockpit_exit(
            report, run_dir=run_dir, bridge_session_dir=bridge_dir)
        self.assertEqual(result["status"], "accepted")
        self.assertEqual(report["verdict"], "blocked_feature_phase_error_logged")
        self.assertEqual(report["feature_debug_guard"]["status"], "red")
        self.assertFalse(report["feature_proof"])

    def test_finalizer_uses_completed_cleanup_before_report_seal(self):
        report, run_dir, bridge_dir = load_case("quiet_repeat")
        with tempfile.TemporaryDirectory() as temporary:
            run_copy, bridge_copy = Path(temporary) / "run", Path(temporary) / "bridge"
            run_copy.mkdir()
            bridge_copy.mkdir()
            for file in ("cockpit.player_quit.json", "semantic.steps.jsonl"):
                shutil.copyfile(run_dir / file, run_copy / file)
            for file in ("game-process.json", "game-process-exit.json"):
                shutil.copyfile(bridge_dir / file, bridge_copy / file)
            old = os.environ.get("OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR")
            os.environ["OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR"] = str(bridge_copy)
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    harness.finalize_probe_report(run_copy, report, report_filename="corrected.json")
            finally:
                if old is None:
                    os.environ.pop("OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR", None)
                else:
                    os.environ["OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR"] = old
            sealed = json.loads((run_copy / "corrected.json").read_text())
            sidecar = json.loads((run_copy / "probe.step_ledger.json").read_text())
            self.assertEqual(sealed["verdict"], "artifacts_matched")
            self.assertEqual(sidecar["step_ledger_summary"]["status"], "green_step_local_proof")
            self.assertEqual(sidecar["feature_debug_guard"]["status"], "green")

    def test_finalizer_nonzero_exit_preserves_red_verdict(self):
        report, run_dir, bridge_dir = load_case("quiet_repeat")
        with tempfile.TemporaryDirectory() as temporary:
            run_copy, bridge_copy = Path(temporary) / "run", Path(temporary) / "bridge"
            run_copy.mkdir()
            bridge_copy.mkdir()
            for file in ("cockpit.player_quit.json", "semantic.steps.jsonl"):
                shutil.copyfile(run_dir / file, run_copy / file)
            shutil.copyfile(bridge_dir / "game-process.json", bridge_copy / "game-process.json")
            exit_record = json.loads((bridge_dir / "game-process-exit.json").read_text())
            exit_record["exit_code"] = 1
            (bridge_copy / "game-process-exit.json").write_text(json.dumps(exit_record))
            old = os.environ.get("OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR")
            os.environ["OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR"] = str(bridge_copy)
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    harness.finalize_probe_report(run_copy, report, report_filename="nonzero.json")
            finally:
                if old is None:
                    os.environ.pop("OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR", None)
                else:
                    os.environ["OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR"] = old
            sealed = json.loads((run_copy / "nonzero.json").read_text())
            self.assertEqual(sealed["verdict"], "blocked_feature_phase_process_exited")
            self.assertFalse(sealed["feature_proof"])
            self.assertEqual(sealed["clean_terminal_exit_reconciliation"]["status"], "rejected")
            self.assertIn("native_exit_nonzero_or_unproved",
                          sealed["clean_terminal_exit_reconciliation"]["issues"])


if __name__ == "__main__":
    unittest.main()
