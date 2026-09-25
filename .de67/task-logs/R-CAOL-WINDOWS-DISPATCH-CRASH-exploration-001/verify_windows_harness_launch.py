"""Run the focused startup suite with the existing Windows resource shim."""

from pathlib import Path
import sys
import types
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.modules.setdefault("resource", types.ModuleType("resource"))
sys.path.insert(0, str(ROOT / "tools/openclaw_harness"))

suite = unittest.defaultTestLoader.loadTestsFromName(
    "startup_hud_run_binding_test.StartupHudRunBindingTest."
    "test_tiles_launch_without_posix_wake_fd_uses_windows_jsonl_poll"
)
result = unittest.TextTestRunner(verbosity=2).run(suite)
raise SystemExit(0 if result.wasSuccessful() else 1)
