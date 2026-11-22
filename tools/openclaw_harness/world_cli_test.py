"""Exercise world authoring and bookkeeping through the public CLI."""
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from world_cli import _recipe
from startup_harness import build_parser, repo_root


CLI = Path(__file__).with_name("startup_harness.py")
FIXTURE = "live-debug:r050_migo_tower_remote_initial_v1"


class WorldCommandTest(unittest.TestCase):
    def test_quick_setup_expresses_the_same_recipe_as_json(self):
        args = build_parser().parse_args(["world", "prepare", FIXTURE, "new", "--home", "cannibal_camp",
                                          "20", "0", "-1", "--give", "brazier", "1", "--give", "log", "3"])
        recipe = _recipe(args)
        self.assertEqual(args.destination, Path("new"))
        self.assertEqual(recipe["locations"]["home_1"]["offset_omt"], [20, 0, -1])
        self.assertEqual(recipe["specials"], [{"id": "cannibal_camp", "at": "home_1"}])
        self.assertEqual(recipe["items"], [{"id": "brazier", "count": 1}, {"id": "log", "count": 3}])

    @unittest.skipUnless(sys.platform in {"win32", "darwin"}, "requires a supported native save helper")
    def test_disposable_prepare_inspect_and_blocked_attempt_retention(self):
        fixture = CLI.parent / "fixtures/saves/live-debug/r050_migo_tower_remote_initial_v1"
        if not (fixture / "save").is_dir() or not (repo_root() / ("zzip.exe" if os.name == "nt" else "zzip")).is_file():
            self.skipTest("materialized R050 fixture and native save helper required")

        def call(*args, code=0):
            proc = subprocess.run([sys.executable, "-B", str(CLI), "world", *map(str, args)], capture_output=True)
            self.assertEqual(proc.returncode, code, proc.stderr)
            return proc.stdout

        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            world = base / "mixed"
            task = base / "recipe.json"
            task.write_text(json.dumps({"requires": [{"staffed": "cannibal_camp", "at": "home_1"}]}))
            prepared = json.loads(call("prepare", FIXTURE, task, world, "--home", "cannibal_camp", "20", "0", "-1",
                                       "--give", "brazier", "1", "--give", "log", "3"))
            self.assertFalse(prepared["requirements_met"])
            readback = json.loads(call("inspect", world, "--zoom", "--check", world / "preparation.json", "--json", code=2))
            home, = [row for row in readback["sites"] if row["faction"] == "cannibal"]
            self.assertEqual(home["anchor"], [165, 149, 0])
            self.assertFalse(home["registered"])
            self.assertEqual(readback["requirements"], prepared["requirements"])
            registry = base / "runs.sqlite"
            attempt = json.loads(call("scratch", world, "--executable", base / "missing", "--registry", registry,
                                      "--question", "Investigate the brazier controls", code=1))
            self.assertEqual(attempt["status"], "blocked")
            run = attempt["run_id"]
            call("keep", run, "--reason", "Build unavailable before play", "--registry", registry)
            rows = json.loads(call("history", "brazier", "--kept", "--registry", registry, "--json"))["runs"]
            self.assertEqual([row["run_id"] for row in rows], [run])
            call("unkeep", run, "--registry", registry)
            rows = json.loads(call("history", "brazier", "--registry", registry, "--json"))["runs"]
            self.assertEqual([row["outcome"] for row in rows], ["blocked"])


if __name__ == "__main__":
    unittest.main()
