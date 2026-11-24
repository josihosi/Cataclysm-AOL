"""Check the selected native Make configuration without starting a build."""
from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


DEFAULT_MSYS_ROOT = "C:/Users/josef/dev/msys64"
DEFAULT_SHADER_FORMATS = {"darwin": "msl", "win32": "dxil"}


def _shader_build_needed(root: Path, formats: str, always_make: bool) -> bool:
    if always_make or set(formats.split(",")) - {"spv", "dxil", "msl"}:
        return True
    shader_dir = root / "data/shaders"
    stamp = shader_dir / ("build-" + formats.replace(",", "-") + ".stamp")
    try:
        stamp_time = stamp.stat().st_mtime_ns
    except FileNotFoundError:
        return True
    inputs = [root / "tools/build_shaders.py", *shader_dir.glob("*.frag"), *shader_dir.glob("*.vert")]
    return any(path.stat().st_mtime_ns > stamp_time for path in inputs)


def build_tool_status(root: Path, *, tiles: bool, environment, platform: str,
                      always_make=False, msys_root: Path | None = None) -> dict:
    missing = []
    path = environment.get("PATH", "")
    if msys_root is not None:
        for relative in ("usr/bin/bash.exe", "ucrt64/bin/g++.exe"):
            tool = msys_root / relative
            if not tool.is_file():
                missing.append({"tool": tool.name, "detail": f"Install the MSYS2 tool at {tool}"})

    def require_on_path(tool):
        if not shutil.which(tool, path=path):
            missing.append({"tool": tool, "detail": f"Put {tool} on PATH for the build."})

    require_on_path("make")
    formats = None
    if environment.get("SDL3", "1" if tiles else "0") == "1":
        if platform != "darwin" or not environment.get("FRAMEWORK"):
            require_on_path("pkg-config")
        formats = environment.get("BUILD_SHADER_FORMATS", DEFAULT_SHADER_FORMATS.get(platform, "spv"))
        if _shader_build_needed(root, formats, always_make):
            require_on_path("python3")
            process = subprocess.run(
                [sys.executable, str(root / "tools/build_shaders.py"), "--formats", formats, "--check-tools"],
                cwd=root, env=environment, capture_output=True, text=True, encoding="utf-8", check=False,
            )
            if process.returncode in (0, 3):
                missing.extend(json.loads(process.stdout)["missing"])
            else:
                return {"status": "blocked", "missing": missing, "shader_formats": formats,
                        "reason": process.stderr.strip() or "Shader tool check failed."}
    return {"status": "blocked" if missing else "ready", "missing": missing, "shader_formats": formats}
