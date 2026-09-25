"""Capture exact Windows Git/subprocess source-readiness inputs."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import types

root = Path(__file__).resolve().parents[2]
toolchain = Path("C:/Users/josef/dev/msys64")
os.environ["PATH"] = os.pathsep.join(
    [str(toolchain / "ucrt64/bin"), str(toolchain / "usr/bin"), os.environ["PATH"]]
)
sys.modules.setdefault("resource", types.ModuleType("resource"))
sys.path.insert(0, str(root / "tools/openclaw_harness"))
import startup_harness

head = "224d7dc9779"
results = {}
for key, args in {
    "git_path": ["where", "git"],
    "cat_type": ["git", "-C", str(root), "cat-file", "-t", head],
    "cat_commit": ["git", "-C", str(root), "cat-file", "-e", f"{head}^{{commit}}"],
    "rev_commit": ["git", "-C", str(root), "rev-parse", "--verify", f"{head}^{{commit}}"],
    "rev_full": ["git", "-C", str(root), "rev-parse", "HEAD"],
}.items():
    result = subprocess.run(args, capture_output=True, text=True, check=False)
    results[key] = {"args": args, "returncode": result.returncode,
                    "stdout": result.stdout[:1000], "stderr": result.stderr[:1000]}
results["shutil_git"] = shutil.which("git")
results["runtime_relevant_changes_since"] = startup_harness.runtime_relevant_changes_since(head)
print(json.dumps(results, indent=2))
