"""Copy the patched Mac build and issue its immutable dirty-source receipt."""

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

task = Path(__file__).resolve().parent
root = task.parents[2]
sys.path.insert(0, str(root / "tools/openclaw_harness"))
import startup_harness

source_executable = root / "cataclysm"
target = root / "build/mac-dispatch-crash-20260924/cataclysm"
if target.exists():
    raise RuntimeError(f"isolated target already exists: {target}")
target.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(source_executable, target)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

if sha(source_executable) != sha(target):
    raise RuntimeError("copied executable hash mismatch")
version = subprocess.run([str(target), "--version"], cwd=root,
                         capture_output=True, text=True, check=False)
if version.returncode:
    raise RuntimeError(f"version exit {version.returncode}: {version.stderr}")
match = re.search(
    r"(?<![0-9a-f])(?P<head>[0-9a-f]{10,40})(?P<dirty>-dirty)?(?:\+[^\s]+)?",
    version.stdout + version.stderr,
)
if not match:
    raise RuntimeError(f"unparseable version: {version.stdout!r}")
binding = startup_harness.product_source_binding()
if not binding.get("ok"):
    raise RuntimeError(f"source binding failure: {binding}")
receipt = {
    "schema": startup_harness.PRODUCT_BUILD_RECEIPT_SCHEMA,
    "captured_head": match.group("head"),
    "executable_path": str(target.resolve()),
    "executable_sha256": sha(target),
    "product_source_sha256": binding["sha256"],
    "built_at": datetime.now(timezone.utc).isoformat(),
    "command": ["make", "-j4", "cataclysm"],
    "version_command": [str(target), "--version"],
    "logs": {"build": str(task / "mac-native-build.log")},
    "build_configuration": {"platform": "macos", "renderer": "curses",
                            "source_executable": str(source_executable)},
    "log_dir": str(task),
}
serialized = json.dumps(receipt, indent=2, sort_keys=True) + "\n"
receipt_path = startup_harness.product_build_receipt_archive_path(
    target, receipt["executable_sha256"], receipt["product_source_sha256"],
    hashlib.sha256(serialized.encode()).hexdigest()[:16],
)
receipt_path.parent.mkdir(parents=True, exist_ok=True)
with receipt_path.open("x", encoding="utf-8") as stream:
    stream.write(serialized)
result = {
    "isolated_executable": str(target),
    "executable_sha256": sha(target),
    "product_source_sha256": binding["sha256"],
    "receipt": str(receipt_path),
    "readiness": startup_harness.executable_source_readiness(target),
}
(task / "mac-native-build-manifest.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
