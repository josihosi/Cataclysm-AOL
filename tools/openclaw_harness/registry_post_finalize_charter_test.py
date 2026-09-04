#!/usr/bin/env python3
"""Regression coverage for coordinator-charter token finalization."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path
from unittest import mock

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import scenario_registry_cli  # noqa: E402


class RegistryPostFinalizeCharterTest(unittest.TestCase):
    def test_finalizer_forwards_the_launch_witness_charter_to_token_revalidation(self) -> None:
        charter = {"schema": "caol-playtest-witness-charter-v1", "claim": "bound claim"}
        receipt = json.dumps({
            "registry_path": "/tmp/registry.sqlite3", "token_id": "token-a",
            "witness_charter": charter,
        })
        connection = mock.Mock()
        with mock.patch.object(scenario_registry_cli, "open_registry", return_value=connection), \
                mock.patch.object(scenario_registry_cli, "production_binding_adapters", return_value="adapters"), \
                mock.patch.object(scenario_registry_cli, "ingest_token_linked_report_reference",
                                  return_value={"status": "ingested"}) as ingest:
            result = scenario_registry_cli._registry_post_finalize_ingest(receipt)(Path("/tmp/report.json"), {})

        self.assertEqual(result, {"status": "ingested"})
        ingest.assert_called_once_with(
            connection, "token-a", Path("/tmp/report.json"), adapters="adapters", witness_charter=charter,
        )
        connection.close.assert_called_once()


if __name__ == "__main__":
    unittest.main()
