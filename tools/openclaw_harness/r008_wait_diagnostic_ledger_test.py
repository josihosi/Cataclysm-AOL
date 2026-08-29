#!/usr/bin/env python3
"""Focused controls for the R-008 zero-credit native-wait diagnostic ledger."""

import tempfile
import unittest
import json
from pathlib import Path
from unittest.mock import patch

from startup_harness import (
    append_wait_diagnostic_record,
    initialize_wait_diagnostic_ledger,
    seal_wait_diagnostic_ledger,
    wait_diagnostic_request_record,
)


class WaitDiagnosticLedgerTest( unittest.TestCase ):
    run_id = "r008-diagnostic-run"

    def initialize( self, root: Path ) -> None:
        executable = root / "cataclysm-tiles"
        executable.write_bytes( b"bound executable" )
        with patch( "startup_harness.runtime_source_binding", return_value={
            "sha256": "bound-source", "worktree_changes": []
        } ):
            result = initialize_wait_diagnostic_ledger(
                root, enabled=True, run_id=self.run_id, executable=executable,
                registry_launch_receipt=(
                    '{"authority_kind":"registry_selection","token_id":"authority",'
                    '"registry_path":"registry.sqlite3"}'
                ),
            )
        self.assertEqual( result["status"], "activated" )

    def record( self, request_id: str, phase: str, *, run_id: str = run_id ) -> dict:
        return wait_diagnostic_request_record(
            run_id=run_id, request_id=request_id, phase=phase,
            semantic_frame="frame-7", pid=0,
            trace={"status": "wait_dispatched", "sdl_event_count": 1,
                   "resolution_count": 1, "dispatch_count": 1,
                   "resolved_action": "wait"},
        )

    def test_activation_precedes_and_retains_one_complete_request( self ) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path( temp )
            self.initialize( root )
            self.assertEqual(
                append_wait_diagnostic_record( root, self.record( "wait-1", "before_input" ) )["status"],
                "retained",
            )
            self.assertEqual(
                append_wait_diagnostic_record( root, self.record( "wait-1", "result" ) )["status"],
                "retained",
            )
            records = [json.loads( line ) for line in
                       ( root / "wait-diagnostic.records.jsonl" ).read_text( encoding="utf-8" ).splitlines()]
            self.assertEqual( records[-1]["binding"]["registry_authority"]["token_id"], "authority" )
            self.assertEqual( records[-1]["binding"]["source"]["sha256"], "bound-source" )
            sealed = seal_wait_diagnostic_ledger( root, {"status": "already_exited"} )
            self.assertEqual( sealed["status"], "sealed" )
            self.assertTrue( sealed["records_sha256"] )

    def test_missing_activation_fails_closed( self ) -> None:
        with tempfile.TemporaryDirectory() as temp:
            self.assertEqual(
                append_wait_diagnostic_record( Path( temp ), self.record( "wait-1", "before_input" ) )["status"],
                "missing_activation",
            )

    def test_wrong_run_fails_closed( self ) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path( temp )
            self.initialize( root )
            self.assertEqual(
                append_wait_diagnostic_record(
                    root, self.record( "wait-1", "before_input", run_id="stale-run" )
                )["status"],
                "wrong_run_rejected",
            )

    def test_reordered_and_incomplete_records_fail_closed( self ) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path( temp )
            self.initialize( root )
            self.assertEqual(
                append_wait_diagnostic_record( root, self.record( "wait-1", "result" ) )["status"],
                "reordered_record_rejected",
            )
            self.assertEqual(
                append_wait_diagnostic_record( root, self.record( "wait-1", "before_input" ) )["status"],
                "retained",
            )
            self.assertEqual(
                seal_wait_diagnostic_ledger( root, {"status": "already_exited"} )["status"],
                "incomplete_records",
            )

    def test_post_cleanup_record_is_rejected( self ) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path( temp )
            self.initialize( root )
            append_wait_diagnostic_record( root, self.record( "wait-1", "before_input" ) )
            append_wait_diagnostic_record( root, self.record( "wait-1", "result" ) )
            self.assertEqual(
                seal_wait_diagnostic_ledger( root, {"status": "already_exited"} )["status"], "sealed"
            )
            self.assertEqual(
                append_wait_diagnostic_record( root, self.record( "wait-2", "before_input" ) )["status"],
                "post_cleanup_rejected",
            )


if __name__ == "__main__":
    unittest.main()
