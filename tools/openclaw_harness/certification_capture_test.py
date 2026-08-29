import json
import tempfile
import unittest
from pathlib import Path

import startup_harness


class CertificationCaptureTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        root = Path(self.temp.name)
        self.root = root
        for name in ("executable", "scenario", "fixture", "profile"):
            (root / name).write_text(name, encoding="utf-8")
        for name in ("data", "harness", "world"):
            (root / name).mkdir()
            (root / name / "content").write_text(name, encoding="utf-8")
        self.player_save = "player.sav"
        (root / "world" / self.player_save).write_text("save", encoding="utf-8")
        (root / "player.json").write_text(json.dumps({"player": {"id": "p"}}), encoding="utf-8")
        (root / "actors.json").write_text(json.dumps({"actors": [{"actor_id": "a"}]}), encoding="utf-8")
        self.base = {"repo_root": str(root), "executable": str(root / "executable"),
                     "runtime_paths": ["scenario"], "data_config_roots": [str(root / "data")],
                     "harness_roots": [str(root / "harness")], "scenario_path": str(root / "scenario"),
                     "fixture_path": str(root / "fixture"), "profile_path": str(root / "profile"),
                     "world_dir": str(root / "world"), "player_save": self.player_save}

    def tearDown(self):
        self.temp.cleanup()

    def test_capture_reads_artifacts_not_inline_objects(self):
        manifest = dict(self.base, captured_artifacts={
            "saved_player_payload": str(self.root / "player.json"),
        })
        path = self.root / "inputs.json"; path.write_text(json.dumps(manifest), encoding="utf-8")
        result = startup_harness.capture_certification_inputs(str(path))
        self.assertEqual(result["saved_player_payload"]["player"]["id"], "p")
        self.assertIsNone(result["ecology_audit"])

    def test_inline_identity_objects_and_missing_capture_fail_closed(self):
        inline = dict(self.base, saved_player_payload={"player": {"id": "p"}}, ecology_audit={"actors": [{"actor_id": "a"}]})
        path = self.root / "inline.json"; path.write_text(json.dumps(inline), encoding="utf-8")
        with self.assertRaisesRegex(Exception, "captured_artifacts paths"):
            startup_harness.capture_certification_inputs(str(path))
        missing = dict(self.base, captured_artifacts={"saved_player_payload": str(self.root / "none"), "ecology_audit": str(self.root / "none2")})
        path.write_text(json.dumps(missing), encoding="utf-8")
        with self.assertRaises(Exception):
            startup_harness.capture_certification_inputs(str(path))


if __name__ == "__main__":
    unittest.main()
