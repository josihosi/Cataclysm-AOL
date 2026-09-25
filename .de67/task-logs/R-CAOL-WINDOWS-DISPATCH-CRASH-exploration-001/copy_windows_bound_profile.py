"""Create and verify a distinct disposable profile from Josef's original save."""

import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[2]
source = root / ".userdata/windows-camp-20260924"
target = root / ".userdata/windows-dispatch-crash-bound-20260924"
if not source.is_dir() or target.exists():
    raise RuntimeError(f"source absent or target already exists: {source}, {target}")

def inventory(path):
    return {
        str(file.relative_to(path)).replace("\\", "/"): hashlib.sha256(file.read_bytes()).hexdigest()
        for file in path.rglob("*") if file.is_file()
    }

before = inventory(source)
shutil.copytree(source, target)
after = inventory(target)
if before != after:
    raise RuntimeError("disposable profile copy differs from source")
print(json.dumps({"source": str(source), "target": str(target),
                  "file_count": len(before), "files": after}, indent=2))
