"""Archive corrected executable version metadata without changing the first receipt."""

import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import types

root = Path(__file__).resolve().parents[2]
os.environ["PATH"] = os.pathsep.join(
    [str(Path("C:/Users/josef/dev/msys64/ucrt64/bin")), os.environ["PATH"]]
)
sys.modules.setdefault("resource", types.ModuleType("resource"))
sys.path.insert(0, str(root / "tools/openclaw_harness"))
import startup_harness

executable = (root / "build/windows-dispatch-crash-bound-20260924/cataclysm-tiles.exe").resolve()
manifest = json.loads((Path(__file__).parent / "bound-build-manifest.json").read_text())
old_path = Path(manifest["product_build_receipt"])
old = json.loads(old_path.read_text())
version = subprocess.run([str(executable), "--version"], capture_output=True, text=True)
if version.returncode:
    raise RuntimeError(f"version exit {version.returncode}: {version.stderr}")
match = re.search(
    r"(?<![0-9a-f])(?P<head>[0-9a-f]{10,40})(?P<dirty>-dirty)?(?:\+[^\s]+)?",
    version.stdout + version.stderr,
)
if not match:
    raise RuntimeError(f"unparseable version: {version.stdout!r}")
captured = match.group("head")
if old["executable_path"] != str(executable):
    raise RuntimeError("receipt executable path mismatch")
if old["executable_sha256"] != startup_harness.sha256_file(executable)[0]:
    raise RuntimeError("receipt executable digest mismatch")
if old["product_source_sha256"] != startup_harness.product_source_binding()["sha256"]:
    raise RuntimeError("receipt source digest mismatch")
if not startup_harness.current_head_short().startswith(old["captured_head"]):
    raise RuntimeError("receipt repository head mismatch")
corrected = dict(old)
corrected["captured_head"] = captured
corrected["receipt_correction"] = {
    "original_path": str(old_path),
    "original_sha256": hashlib.sha256(old_path.read_bytes()).hexdigest(),
    "reason": "executable --version reports an 11-digit head; builder used git --short=10",
}
serialized = json.dumps(corrected, indent=2, sort_keys=True) + "\n"
archive_id = hashlib.sha256(serialized.encode()).hexdigest()[:8]
corrected_path = startup_harness.product_build_receipt_archive_path(
    executable, old["executable_sha256"], old["product_source_sha256"], archive_id
)
with corrected_path.open("x", encoding="utf-8") as stream:
    stream.write(serialized)
result = {
    "original_receipt": str(old_path),
    "corrected_receipt": str(corrected_path),
    "captured_head": captured,
    "corrected_receipt_sha256": hashlib.sha256(corrected_path.read_bytes()).hexdigest(),
    "readiness": startup_harness.executable_source_readiness(executable),
}
print(json.dumps(result, indent=2))
