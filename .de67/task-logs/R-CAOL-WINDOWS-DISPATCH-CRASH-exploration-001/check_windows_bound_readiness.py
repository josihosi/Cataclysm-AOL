"""Independently revalidate the isolated Windows product receipt."""

import json
import os
from pathlib import Path
import sys
import types

root = Path(__file__).resolve().parents[2]
os.environ["PATH"] = os.pathsep.join(
    [str(Path("C:/Users/josef/dev/msys64/ucrt64/bin")), os.environ["PATH"]]
)
sys.modules.setdefault("resource", types.ModuleType("resource"))
sys.path.insert(0, str(root / "tools/openclaw_harness"))
import startup_harness

executable = root / "build/windows-dispatch-crash-bound-20260924/cataclysm-tiles.exe"
print(json.dumps(startup_harness.executable_source_readiness(executable), indent=2))
