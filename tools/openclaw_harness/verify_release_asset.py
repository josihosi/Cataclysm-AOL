#!/usr/bin/env python3
"""Verify C-AOL release payload, provenance, and platform integrity."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path
from zipfile import ZipFile


EXPECTED_MANUAL_SCENARIOS = {
    "manual.cannibal_night_pack_mcw",
    "manual.intact_camp_shakedown_mcw",
    "manual.mixed_hostile_siege_mcw",
    "manual.writhing_stalker_hit_fade_mcw",
    "manual.zombie_rider_open_field_mcw",
}

MACOS_SYSTEM_DEPENDENCY_PREFIXES = (
    "/System/Library/",
    "/usr/lib/",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"release asset check failed: {message}")


def command_output(proc: subprocess.CompletedProcess[str]) -> str:
    return "\n".join(part.strip() for part in (proc.stdout, proc.stderr) if part.strip())


def run_command(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, check=False)


def require_command(command: list[str], description: str) -> subprocess.CompletedProcess[str]:
    proc = run_command(command)
    require(proc.returncode == 0, f"{description} failed:\n{command_output(proc)}")
    return proc


def safe_extract_zip(archive: Path, destination: Path) -> None:
    with ZipFile(archive) as zf:
        for member in zf.infolist():
            target = destination / member.filename
            require(destination in target.resolve().parents or target.resolve() == destination, f"unsafe zip path: {member.filename}")
        zf.extractall(destination)


def safe_extract_tar(archive: Path, destination: Path) -> None:
    with tarfile.open(archive, "r:gz") as tf:
        for member in tf.getmembers():
            target = destination / member.name
            require(destination in target.resolve().parents or target.resolve() == destination, f"unsafe tar path: {member.name}")
        tf.extractall(destination)


def mount_dmg(archive: Path) -> str:
    proc = run_command(["hdiutil", "attach", "-readonly", "-nobrowse", "-plist", str(archive)])
    require(proc.returncode == 0, f"hdiutil attach failed:\n{proc.stdout}\n{proc.stderr}")
    mount_point = ""
    for line in proc.stdout.splitlines():
        line = line.strip()
        if line.startswith("<string>/Volumes/"):
            mount_point = line.replace("<string>", "").replace("</string>", "")
    require(bool(mount_point), "hdiutil attach did not report a mount point")
    return mount_point


def detach_dmg(mount_point: str) -> None:
    subprocess.run(["hdiutil", "detach", mount_point], check=False, capture_output=True, text=True)


def verify_dmg(archive: Path) -> None:
    require_command(["hdiutil", "verify", str(archive)], "hdiutil DMG verification")


def copy_macos_app_from_dmg(mount_root: Path, destination: Path) -> Path:
    apps = sorted(mount_root.glob("*.app"))
    require(apps, f"no .app bundle found in mounted DMG at {mount_root}")
    require(len(apps) == 1, f"expected one .app bundle in mounted DMG, found: {[app.name for app in apps]}")
    writable_app = destination / apps[0].name
    shutil.copytree(apps[0], writable_app, symlinks=True)
    return writable_app


def find_harness_script(root: Path) -> Path:
    matches = sorted(root.rglob("tools/openclaw_harness/startup_harness.py"))
    require(matches, f"startup_harness.py not found under {root}")
    return matches[0]


def require_version_metadata(
    game_root: Path,
    platform: str,
    expected_branch: str,
    expected_build_number: str,
    expected_commit: str,
) -> dict[str, str]:
    version_file = game_root / "VERSION.txt"
    require(version_file.is_file(), f"missing VERSION.txt under {game_root}")
    metadata: dict[str, str] = {}
    for raw_line in version_file.read_text(encoding="utf-8").splitlines():
        if ":" not in raw_line:
            continue
        key, value = raw_line.split(":", 1)
        metadata[key.strip()] = value.strip()

    for key in ("build type", "build number", "target branch", "commit sha", "commit url"):
        require(bool(metadata.get(key)), f"VERSION.txt is missing {key!r}")
    require(metadata["build type"] == platform, f"VERSION.txt build type is {metadata['build type']!r}, expected {platform!r}")
    if expected_branch:
        require(metadata["target branch"] == expected_branch, f"VERSION.txt target branch is {metadata['target branch']!r}, expected {expected_branch!r}")
    if expected_build_number:
        require(metadata["build number"] == expected_build_number, f"VERSION.txt build number is {metadata['build number']!r}, expected {expected_build_number!r}")
    if expected_commit:
        require(metadata["commit sha"] == expected_commit, f"VERSION.txt commit is {metadata['commit sha']!r}, expected {expected_commit!r}")
        require(metadata["commit url"].endswith(f"/commit/{expected_commit}"), f"VERSION.txt commit URL does not name {expected_commit!r}")
    return metadata


def macos_macho_candidates(app: Path) -> list[Path]:
    candidates: list[Path] = []
    for root in (app / "Contents" / "MacOS", app / "Contents" / "Resources"):
        require(root.is_dir(), f"missing macOS bundle directory: {root}")
        for path in root.rglob("*"):
            if path.is_symlink() or not path.is_file():
                continue
            if path.suffix != ".dylib" and not os.access(path, os.X_OK):
                continue
            file_proc = require_command(["file", "-b", str(path)], f"file probe for {path}")
            if "Mach-O" in file_proc.stdout:
                candidates.append(path)
    require(bool(candidates), f"no Mach-O files found in {app}")
    return sorted(candidates)


def parse_otool_dependencies(output: str) -> list[str]:
    dependencies: list[str] = []
    for line in output.splitlines()[1:]:
        stripped = line.strip()
        if not stripped:
            continue
        dependencies.append(stripped.split(" (compatibility version", 1)[0])
    return dependencies


def parse_macos_rpaths(output: str) -> list[str]:
    rpaths: list[str] = []
    awaiting_path = False
    for line in output.splitlines():
        stripped = line.strip()
        if stripped == "cmd LC_RPATH":
            awaiting_path = True
            continue
        if awaiting_path and stripped.startswith("path "):
            rpaths.append(stripped[5:].rsplit(" (offset ", 1)[0])
            awaiting_path = False
    return rpaths


def is_macos_system_path(path: str) -> bool:
    return any(path == prefix.rstrip("/") or path.startswith(prefix)
               for prefix in MACOS_SYSTEM_DEPENDENCY_PREFIXES)


def path_is_within(path: Path, root: Path) -> bool:
    resolved_path = path.resolve()
    resolved_root = root.resolve()
    return resolved_path == resolved_root or resolved_root in resolved_path.parents


def expand_macos_loader_path(value: str, binary: Path, game_root: Path) -> Path | None:
    if value == "@loader_path":
        return binary.parent.resolve()
    if value.startswith("@loader_path/"):
        return (binary.parent / value.removeprefix("@loader_path/")).resolve()
    if value == "@executable_path":
        return game_root.resolve()
    if value.startswith("@executable_path/"):
        return (game_root / value.removeprefix("@executable_path/")).resolve()
    if value.startswith("/"):
        return Path(value).resolve()
    return None


def resolve_macos_rpaths(app: Path, game_root: Path, binary: Path, rpaths: list[str]) -> list[Path]:
    resolved: list[Path] = []
    for rpath in rpaths:
        require(not rpath.startswith("@rpath"), f"{binary} contains recursive LC_RPATH {rpath!r}")
        candidate = expand_macos_loader_path(rpath, binary, game_root)
        require(candidate is not None, f"{binary} contains unsupported LC_RPATH {rpath!r}")
        if rpath.startswith("/"):
            require(is_macos_system_path(rpath), f"{binary} retains non-system absolute LC_RPATH {rpath!r}")
        else:
            require(path_is_within(candidate, app), f"{binary} LC_RPATH escapes the app bundle: {rpath!r}")
        require(candidate.is_dir(), f"{binary} LC_RPATH does not resolve to a directory: {rpath!r}")
        resolved.append(candidate)
    return resolved


def require_macos_dependencies(
    app: Path,
    game_root: Path,
    binary: Path,
    dependencies: list[str],
    rpaths: list[Path],
    macho_targets: set[Path],
) -> None:
    for dependency in dependencies:
        if is_macos_system_path(dependency):
            continue
        if dependency.startswith("/"):
            require(False, f"{binary} retains non-system absolute dependency: {dependency}")
        if dependency.startswith("@rpath/"):
            suffix = dependency.removeprefix("@rpath/")
            matches = [root / suffix for root in rpaths if (root / suffix).is_file()]
            require(matches, f"{binary} has unresolved @rpath dependency: {dependency}")
            require(
                any(
                    is_macos_system_path(str(match)) or
                    (path_is_within(match, app) and match.resolve() in macho_targets)
                    for match in matches
                ),
                f"{binary} resolves {dependency} outside verified app Mach-O and system roots",
            )
            continue
        candidate = expand_macos_loader_path(dependency, binary, game_root)
        require(candidate is not None, f"{binary} contains unsupported dependency path: {dependency}")
        require(candidate.is_file(), f"{binary} has unresolved loader dependency: {dependency}")
        require(path_is_within(candidate, app), f"{binary} dependency escapes the app bundle: {dependency}")
        require(
            candidate.resolve() in macho_targets,
            f"{binary} loader dependency is not a verified universal Mach-O: {dependency}",
        )


def require_macos_bundle(app: Path, game_root: Path) -> dict[str, object]:
    require_command(
        ["codesign", "--verify", "--deep", "--strict", "--verbose=2", str(app)],
        "post-DMG code signature verification",
    )

    macho_files = macos_macho_candidates(app)
    macho_targets = {path.resolve() for path in macho_files}
    game_binary = game_root / "cataclysm-tiles"
    require(game_binary in macho_files, f"main macOS game binary was not architecture-checked: {game_binary}")

    architecture_rows: dict[str, list[str]] = {}
    dependency_rows: dict[tuple[Path, str], list[str]] = {}
    rpath_rows: dict[tuple[Path, str], list[Path]] = {}
    for binary in macho_files:
        relative = str(binary.relative_to(app))
        lipo_proc = require_command(["lipo", "-archs", str(binary)], f"architecture probe for {relative}")
        architectures = sorted(set(lipo_proc.stdout.split()))
        require("arm64" in architectures, f"{relative} is missing arm64 architecture: {architectures}")
        require("x86_64" in architectures, f"{relative} is missing x86_64 architecture: {architectures}")
        architecture_rows[relative] = architectures

        for architecture in ("arm64", "x86_64"):
            description = f"{relative} ({architecture})"
            otool_proc = require_command(
                ["otool", "-arch", architecture, "-L", str(binary)],
                f"dependency probe for {description}",
            )
            dependencies = parse_otool_dependencies(otool_proc.stdout)
            install_name_proc = run_command(["otool", "-arch", architecture, "-D", str(binary)])
            install_name_lines = [line.strip() for line in install_name_proc.stdout.splitlines() if line.strip()]
            install_name = install_name_lines[1] if install_name_proc.returncode == 0 and len(install_name_lines) > 1 else ""
            if install_name:
                require(
                    install_name.startswith(("@executable_path/", "@loader_path/", "@rpath/")) or
                    is_macos_system_path(install_name),
                    f"{description} retains non-portable install name: {install_name}",
                )
                if dependencies and dependencies[0] == install_name:
                    dependencies = dependencies[1:]
            dependency_rows[(binary, architecture)] = dependencies

            rpath_proc = require_command(
                ["otool", "-arch", architecture, "-l", str(binary)],
                f"LC_RPATH probe for {description}",
            )
            rpath_rows[(binary, architecture)] = resolve_macos_rpaths(
                app,
                game_root,
                binary,
                parse_macos_rpaths(rpath_proc.stdout),
            )

    for architecture in ("arm64", "x86_64"):
        main_rpaths = rpath_rows[(game_binary, architecture)]
        for binary in macho_files:
            require_macos_dependencies(
                app,
                game_root,
                binary,
                dependency_rows[(binary, architecture)],
                list(dict.fromkeys(main_rpaths + rpath_rows[(binary, architecture)])),
                macho_targets,
            )

    signature_proc = require_command(
        ["codesign", "--display", "--verbose=4", str(app)],
        "macOS signature identity inspection",
    )
    signature_text = command_output(signature_proc)
    signature_kind = "ad-hoc" if "Signature=adhoc" in signature_text else "identified"

    gatekeeper_proc = run_command(["spctl", "--assess", "--type", "execute", "--verbose=4", str(app)])
    stapler_proc = run_command(["xcrun", "stapler", "validate", str(app)])
    limitations: list[str] = []
    if gatekeeper_proc.returncode != 0:
        limitations.append("Gatekeeper assessment did not accept this app")
    if stapler_proc.returncode != 0:
        limitations.append("no valid notarization ticket is stapled")
    if signature_kind == "ad-hoc":
        limitations.append("app uses an ad-hoc signature, not a Developer ID identity")

    diagnostics: dict[str, object] = {
        "architecture_checked_files": len(architecture_rows),
        "distribution_trust_ok": gatekeeper_proc.returncode == 0 and stapler_proc.returncode == 0,
        "gatekeeper": {
            "ok": gatekeeper_proc.returncode == 0,
            "output": command_output(gatekeeper_proc),
        },
        "limitations": limitations,
        "signature_kind": signature_kind,
        "stapler": {
            "ok": stapler_proc.returncode == 0,
            "output": command_output(stapler_proc),
        },
    }
    print(json.dumps({"macos_trust_diagnostic": diagnostics}, indent=2), file=sys.stderr)
    return diagnostics


def require_payload(
    root: Path,
    platform: str,
    expected_branch: str,
    expected_build_number: str,
    expected_commit: str,
) -> tuple[Path, dict[str, str]]:
    harness = find_harness_script(root)
    game_root = harness.parents[2]
    fixture_flexbuffers = sorted(
        str(path.relative_to(game_root))
        for path in (harness.parent / "fixtures").rglob("*.fb")
    )
    require(not fixture_flexbuffers, f"fixture flexbuffer caches must not ship: {fixture_flexbuffers[:20]}")
    helper_name = "zzip.exe" if platform == "windows" else "zzip"
    helper = game_root / helper_name
    require(helper.is_file(), f"missing platform zzip helper at {helper}")
    if platform != "windows":
        require(os.access(helper, os.X_OK), f"platform zzip helper is not executable: {helper}")
    game_binary = game_root / ("cataclysm-tiles.exe" if platform == "windows" else "cataclysm-tiles")
    require(game_binary.is_file(), f"missing platform game binary at {game_binary}")
    if platform != "windows":
        require(os.access(game_binary, os.X_OK), f"platform game binary is not executable: {game_binary}")
    require((game_root / "data").is_dir(), f"missing data directory under {game_root}")
    require((game_root / "gfx").is_dir(), f"missing gfx directory under {game_root}")
    require((harness.parent / "requirements.txt").is_file(), "missing tools/openclaw_harness/requirements.txt")

    proc = subprocess.run(
        [sys.executable, str(harness), "list-scenarios"],
        cwd=str(game_root),
        capture_output=True,
        text=True,
        check=False,
    )
    require(proc.returncode == 0, f"list-scenarios failed:\n{proc.stdout}\n{proc.stderr}")
    payload = json.loads(proc.stdout)
    names = {
        str(item.get("name", ""))
        for item in payload.get("scenarios", [])
        if str(item.get("name", "")).startswith("manual.")
    }
    require(names == EXPECTED_MANUAL_SCENARIOS, f"manual scenario set mismatch: expected {sorted(EXPECTED_MANUAL_SCENARIOS)}, got {sorted(names)}")

    for scenario in sorted(EXPECTED_MANUAL_SCENARIOS):
        proc = subprocess.run(
            [sys.executable, str(harness), "handoff", scenario, "--launch-only", "--dry-run", "--compact-stdout"],
            cwd=str(game_root),
            capture_output=True,
            text=True,
            check=False,
        )
        require(proc.returncode == 0, f"launch-only dry-run failed for {scenario}:\n{proc.stdout}\n{proc.stderr}")

    version_metadata = require_version_metadata(
        game_root,
        platform,
        expected_branch,
        expected_build_number,
        expected_commit,
    )
    return game_root, version_metadata


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asset", type=Path)
    parser.add_argument("--platform", choices=["windows", "macos", "linux"], required=True)
    parser.add_argument("--expected-branch", default="")
    parser.add_argument("--expected-build-number", default="")
    parser.add_argument("--expected-commit", default="")
    args = parser.parse_args()

    asset = args.asset.resolve()
    require(asset.is_file(), f"asset not found: {asset}")
    mount_point = ""
    macos_trust: dict[str, object] = {}
    version_metadata: dict[str, str] = {}
    with tempfile.TemporaryDirectory(prefix="caol-release-shape-") as tmp:
        tmp_path = Path(tmp).resolve()
        if args.platform == "windows":
            safe_extract_zip(asset, tmp_path)
            root = tmp_path
        elif args.platform == "linux":
            safe_extract_tar(asset, tmp_path)
            root = tmp_path
        else:
            require(sys.platform == "darwin", "macOS DMG verification must run on macOS")
            verify_dmg(asset)
            mount_point = mount_dmg(asset)
            root = copy_macos_app_from_dmg(Path(mount_point), tmp_path)
        try:
            game_root, version_metadata = require_payload(
                root,
                args.platform,
                args.expected_branch,
                args.expected_build_number,
                args.expected_commit,
            )
            if args.platform == "macos":
                macos_trust = require_macos_bundle(root, game_root)
        finally:
            if mount_point:
                detach_dmg(mount_point)

    print(json.dumps({
        "ok": True,
        "asset": str(asset),
        "macos_trust": macos_trust,
        "platform": args.platform,
        "manual_scenarios": sorted(EXPECTED_MANUAL_SCENARIOS),
        "version": version_metadata,
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
