#!/usr/bin/env python3
"""Focused fail-closed release workflow and artifact verifier contracts."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
from unittest import mock

HARNESS_DIR = Path(__file__).resolve().parent
REPO_ROOT = HARNESS_DIR.parents[1]
sys.path.insert(0, str(HARNESS_DIR))

import verify_release_asset as release_asset  # noqa: E402


def completed(
    command: list[str],
    returncode: int = 0,
    stdout: str = "",
    stderr: str = "",
) -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess(command, returncode, stdout, stderr)


class ReleaseWorkflowContractTest(unittest.TestCase):
    def test_release_is_published_once_after_artifacts_and_checksums(self) -> None:
        workflow = (REPO_ROOT / ".github" / "workflows" / "release.yml").read_text(encoding="utf-8")

        self.assertEqual(workflow.count("gh release create"), 1)
        self.assertNotIn("gh release upload", workflow)
        self.assertIn("name: Publish verified prerelease", workflow)
        self.assertIn("Fail on release or tag collision", workflow)
        self.assertLess(workflow.index("sha256sum --check SHA256SUMS.txt"), workflow.index("gh release create"))
        self.assertIn("--expected-branch '${{ needs.release.outputs.target_ref }}'", workflow)
        self.assertIn("--expected-build-number '${{ needs.release.outputs.timestamp }}'", workflow)
        self.assertIn("--expected-commit '${{ needs.release.outputs.target_sha }}'", workflow)
        self.assertNotIn("npm install", workflow)
        self.assertIn('case "$status" in', workflow)
        self.assertIn("404)", workflow)
        self.assertIn("Unexpected HTTP $status", workflow)
        self.assertIn('release_version="${commit_sha:0:11}"', workflow)
        self.assertIn('echo "CAOL_RELEASE_VERSION=$release_version" >> "$GITHUB_ENV"', workflow)

    def test_release_note_generator_fails_hard(self) -> None:
        script = (REPO_ROOT / "build-scripts" / "generate-release-notes.js").read_text(encoding="utf-8")

        self.assertIn("process.exit( 1 );", script)
        self.assertNotIn("process.exit( 0 );", script)
        self.assertNotIn("@actions/github", script)


class ReleaseVersionOverrideContractTest(unittest.TestCase):
    def test_release_override_precedes_dirty_worktree_stamping(self) -> None:
        makefile = (REPO_ROOT / "Makefile").read_text(encoding="utf-8")
        prebuild = (REPO_ROOT / "msvc-full-features" / "prebuild.cmd").read_text(encoding="utf-8")

        make_override = 'if [ -n "$$CAOL_RELEASE_VERSION" ]; then'
        self.assertIn(make_override, makefile)
        self.assertIn('VERSION_STRING="$$CAOL_RELEASE_VERSION"', makefile)
        self.assertLess(makefile.index(make_override), makefile.index("DIRTYFLAG=$$("))
        self.assertIn('if not "%CAOL_RELEASE_VERSION%"==""', prebuild)
        self.assertIn("set VERSION=%CAOL_RELEASE_VERSION%", prebuild)
        self.assertLess(
            prebuild.index("set VERSION=%CAOL_RELEASE_VERSION%"),
            prebuild.index("git describe --tags --always --dirty"),
        )


class ReleaseVersionMetadataTest(unittest.TestCase):
    def write_version(self, root: Path, commit: str = "a" * 40) -> None:
        (root / "VERSION.txt").write_text(
            "\n".join((
                "build type: macos",
                "build number: 2026-07-18-1200",
                "target branch: port/cdda-master",
                f"commit sha: {commit}",
                f"commit url: https://github.com/josihosi/Cataclysm-AOL/commit/{commit}",
            )) + "\n",
            encoding="utf-8",
        )

    def test_exact_version_metadata_is_accepted(self) -> None:
        commit = "b" * 40
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            self.write_version(root, commit)

            metadata = release_asset.require_version_metadata(
                root,
                "macos",
                "port/cdda-master",
                "2026-07-18-1200",
                commit,
            )

        self.assertEqual(metadata["commit sha"], commit)

    def test_wrong_commit_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            self.write_version(root)

            with self.assertRaisesRegex(SystemExit, "VERSION.txt commit"):
                release_asset.require_version_metadata(
                    root,
                    "macos",
                    "port/cdda-master",
                    "2026-07-18-1200",
                    "b" * 40,
                )


class NativeGameLaunchTest(unittest.TestCase):
    def test_exact_packaged_binary_version_is_accepted(self) -> None:
        commit = "116382dc99d2bacf273d74751d41e2473e5e84fb"
        game_root = Path("release-root")
        game_binary = game_root / "cataclysm-tiles.exe"
        command = [str(game_binary), "--version"]
        version_output = (
            "Cataclysm Dark Days Ahead: "
            "cdda-experimental-2023-04-17-2341-25576-g116382dc99d+SDL3\n\n"
            "+tiles, +sound\n"
        )
        with mock.patch.object(
            release_asset.subprocess,
            "run",
            return_value=completed(command, stdout=version_output),
        ) as run_mock:
            evidence = release_asset.require_game_binary_version(game_binary, game_root, commit)

        self.assertEqual(evidence["commit_abbreviation"], "116382dc99d")
        self.assertEqual(evidence["expected_commit"], commit)
        self.assertTrue(evidence["ok"])
        self.assertEqual(
            evidence["reported_version"],
            "cdda-experimental-2023-04-17-2341-25576-g116382dc99d+SDL3",
        )
        run_mock.assert_called_once_with(
            command,
            cwd=str(game_root),
            capture_output=True,
            text=True,
            check=False,
            timeout=release_asset.GAME_VERSION_TIMEOUT_SECONDS,
        )

    def test_make_packaged_binary_version_is_accepted(self) -> None:
        commit = "116382dc99d2bacf273d74751d41e2473e5e84fb"
        game_root = Path("release-root")
        game_binary = game_root / "cataclysm-tiles"
        command = [str(game_binary), "--version"]
        with mock.patch.object(
            release_asset.subprocess,
            "run",
            return_value=completed(
                command,
                stdout="Cataclysm Dark Days Ahead: 116382dc99d+SDL3\n\n+tiles, +sound\n",
            ),
        ):
            evidence = release_asset.require_game_binary_version(game_binary, game_root, commit)

        self.assertEqual(evidence["commit_abbreviation"], "116382dc99d")

    def test_stale_packaged_binary_version_is_rejected(self) -> None:
        commit = "116382dc99d2bacf273d74751d41e2473e5e84fb"
        game_root = Path("release-root")
        game_binary = game_root / "cataclysm-tiles"
        command = [str(game_binary), "--version"]
        with mock.patch.object(
            release_asset.subprocess,
            "run",
            return_value=completed(
                command,
                stdout="Cataclysm Dark Days Ahead: cdda-experimental-old-gdeadbeef000+SDL3\n",
            ),
        ):
            with self.assertRaisesRegex(SystemExit, "does not identify expected commit"):
                release_asset.require_game_binary_version(game_binary, game_root, commit)

    def test_too_short_commit_abbreviation_is_rejected(self) -> None:
        commit = "116382dc99d2bacf273d74751d41e2473e5e84fb"
        game_root = Path("release-root")
        game_binary = game_root / "cataclysm-tiles"
        command = [str(game_binary), "--version"]
        with mock.patch.object(
            release_asset.subprocess,
            "run",
            return_value=completed(
                command,
                stdout="Cataclysm Dark Days Ahead: 116382dc99+SDL3\n",
            ),
        ):
            with self.assertRaisesRegex(SystemExit, "does not identify expected commit"):
                release_asset.require_game_binary_version(game_binary, game_root, commit)

    def test_dirty_packaged_binary_version_is_rejected(self) -> None:
        commit = "116382dc99d2bacf273d74751d41e2473e5e84fb"
        game_root = Path("release-root")
        game_binary = game_root / "cataclysm-tiles"
        command = [str(game_binary), "--version"]
        with mock.patch.object(
            release_asset.subprocess,
            "run",
            return_value=completed(
                command,
                stdout="Cataclysm Dark Days Ahead: 116382dc99d-dirty+SDL3\n",
            ),
        ):
            with self.assertRaisesRegex(SystemExit, "not an exact source revision"):
                release_asset.require_game_binary_version(game_binary, game_root, commit)

    def test_packaged_binary_launch_failure_is_rejected(self) -> None:
        commit = "116382dc99d2bacf273d74751d41e2473e5e84fb"
        game_root = Path("release-root")
        game_binary = game_root / "cataclysm-tiles"
        command = [str(game_binary), "--version"]
        with mock.patch.object(
            release_asset.subprocess,
            "run",
            return_value=completed(command, returncode=3221225781, stderr="missing runtime library"),
        ):
            with self.assertRaisesRegex(SystemExit, "exited with 3221225781"):
                release_asset.require_game_binary_version(game_binary, game_root, commit)

    def test_packaged_binary_launch_timeout_is_rejected(self) -> None:
        commit = "116382dc99d2bacf273d74751d41e2473e5e84fb"
        game_root = Path("release-root")
        game_binary = game_root / "cataclysm-tiles"
        with mock.patch.object(
            release_asset.subprocess,
            "run",
            side_effect=subprocess.TimeoutExpired([str(game_binary), "--version"], 3),
        ):
            with self.assertRaisesRegex(SystemExit, "timed out after 3 seconds"):
                release_asset.require_game_binary_version(
                    game_binary,
                    game_root,
                    commit,
                    timeout_seconds=3,
                )

    def test_cross_platform_execution_is_rejected(self) -> None:
        with mock.patch.object(release_asset, "native_release_platform", return_value="linux"):
            with self.assertRaisesRegex(SystemExit, "windows release asset must be executed"):
                release_asset.require_native_release_platform("windows")


class MacReleaseAssetTest(unittest.TestCase):
    def make_app(self, root: Path) -> tuple[Path, Path]:
        app = root / "Cataclysm.app"
        game_root = app / "Contents" / "Resources"
        (app / "Contents" / "MacOS").mkdir(parents=True)
        game_root.mkdir(parents=True)
        game_binary = game_root / "cataclysm-tiles"
        game_binary.write_bytes(b"fake Mach-O")
        return app, game_root

    def command_side_effect(
        self,
        command: list[str],
        architectures: str = "x86_64 arm64\n",
        dependencies: str = "cataclysm-tiles:\n\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0)\n",
        rpaths: str = "",
    ) -> subprocess.CompletedProcess[str]:
        if command[:2] == ["codesign", "--verify"]:
            return completed(command)
        if command[:2] == ["codesign", "--display"]:
            return completed(command, stderr="Executable=Cataclysm\nSignature=adhoc\n")
        if command[0] == "lipo":
            return completed(command, stdout=architectures)
        if command[0] == "otool" and "-L" in command:
            return completed(command, stdout=dependencies)
        if command[0] == "otool" and "-D" in command:
            return completed(command, returncode=1, stderr="not an object file with an install name")
        if command[0] == "otool" and "-l" in command:
            return completed(command, stdout=rpaths)
        if command[0] == "spctl":
            return completed(command, returncode=3, stderr="rejected\nsource=no usable signature")
        if command[:2] == ["xcrun", "stapler"]:
            return completed(command, returncode=1, stderr="The validate action failed")
        raise AssertionError(f"unexpected command: {command}")

    def test_hdiutil_verification_failure_is_fatal(self) -> None:
        command = ["hdiutil", "verify", "candidate.dmg"]
        with mock.patch.object(
            release_asset,
            "run_command",
            return_value=completed(command, returncode=1, stderr="invalid checksum"),
        ):
            with self.assertRaisesRegex(SystemExit, "hdiutil DMG verification failed"):
                release_asset.verify_dmg(Path("candidate.dmg"))

    def test_adhoc_gatekeeper_state_is_reported_as_a_limitation(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            app, game_root = self.make_app(Path(temp_dir))
            game_binary = game_root / "cataclysm-tiles"
            with mock.patch.object(release_asset, "macos_macho_candidates", return_value=[game_binary]), \
                    mock.patch.object(release_asset, "run_command", side_effect=self.command_side_effect):
                with redirect_stderr(StringIO()):
                    diagnostics = release_asset.require_macos_bundle(app, game_root)

        self.assertFalse(diagnostics["distribution_trust_ok"])
        self.assertEqual(diagnostics["signature_kind"], "ad-hoc")
        self.assertIn("Gatekeeper assessment did not accept this app", diagnostics["limitations"])
        self.assertIn("no valid notarization ticket is stapled", diagnostics["limitations"])

    def test_thin_macos_binary_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            app, game_root = self.make_app(Path(temp_dir))
            game_binary = game_root / "cataclysm-tiles"

            def thin_side_effect(command: list[str]) -> subprocess.CompletedProcess[str]:
                return self.command_side_effect(command, architectures="arm64\n")

            with mock.patch.object(release_asset, "macos_macho_candidates", return_value=[game_binary]), \
                    mock.patch.object(release_asset, "run_command", side_effect=thin_side_effect):
                with self.assertRaisesRegex(SystemExit, "missing x86_64 architecture"):
                    release_asset.require_macos_bundle(app, game_root)

    def test_local_package_manager_dependency_is_rejected(self) -> None:
        dependencies = "cataclysm-tiles:\n\t/opt/local/lib/libmpg123.dylib (compatibility version 0.0.0)\n"
        with tempfile.TemporaryDirectory() as temp_dir:
            app, game_root = self.make_app(Path(temp_dir))
            game_binary = game_root / "cataclysm-tiles"

            def nonportable_side_effect(command: list[str]) -> subprocess.CompletedProcess[str]:
                return self.command_side_effect(command, dependencies=dependencies)

            with mock.patch.object(release_asset, "macos_macho_candidates", return_value=[game_binary]), \
                    mock.patch.object(release_asset, "run_command", side_effect=nonportable_side_effect):
                with self.assertRaisesRegex(SystemExit, "retains non-system absolute dependency"):
                    release_asset.require_macos_bundle(app, game_root)

    def test_custom_runner_dependency_is_rejected(self) -> None:
        dependencies = "cataclysm-tiles:\n\t/Users/runner/work/sdl3_prefix/lib/libSDL3.dylib (compatibility version 0.0.0)\n"
        with tempfile.TemporaryDirectory() as temp_dir:
            app, game_root = self.make_app(Path(temp_dir))
            game_binary = game_root / "cataclysm-tiles"

            def custom_prefix_side_effect(command: list[str]) -> subprocess.CompletedProcess[str]:
                return self.command_side_effect(command, dependencies=dependencies)

            with mock.patch.object(release_asset, "macos_macho_candidates", return_value=[game_binary]), \
                    mock.patch.object(release_asset, "run_command", side_effect=custom_prefix_side_effect):
                with self.assertRaisesRegex(SystemExit, "retains non-system absolute dependency"):
                    release_asset.require_macos_bundle(app, game_root)

    def test_nonportable_dependency_in_second_architecture_is_rejected(self) -> None:
        nonportable = "cataclysm-tiles:\n\t/Users/runner/work/libSDL3.dylib (compatibility version 0.0.0)\n"
        with tempfile.TemporaryDirectory() as temp_dir:
            app, game_root = self.make_app(Path(temp_dir))
            game_binary = game_root / "cataclysm-tiles"

            def per_architecture_side_effect(command: list[str]) -> subprocess.CompletedProcess[str]:
                dependencies = nonportable
                if "x86_64" not in command:
                    dependencies = "cataclysm-tiles:\n\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0)\n"
                return self.command_side_effect(command, dependencies=dependencies)

            with mock.patch.object(release_asset, "macos_macho_candidates", return_value=[game_binary]), \
                    mock.patch.object(release_asset, "run_command", side_effect=per_architecture_side_effect):
                with self.assertRaisesRegex(SystemExit, "retains non-system absolute dependency"):
                    release_asset.require_macos_bundle(app, game_root)

    def test_unresolved_rpath_dependency_is_rejected(self) -> None:
        dependencies = "cataclysm-tiles:\n\t@rpath/libSDL3.dylib (compatibility version 0.0.0)\n"
        with tempfile.TemporaryDirectory() as temp_dir:
            app, game_root = self.make_app(Path(temp_dir))
            game_binary = game_root / "cataclysm-tiles"

            def unresolved_side_effect(command: list[str]) -> subprocess.CompletedProcess[str]:
                return self.command_side_effect(command, dependencies=dependencies)

            with mock.patch.object(release_asset, "macos_macho_candidates", return_value=[game_binary]), \
                    mock.patch.object(release_asset, "run_command", side_effect=unresolved_side_effect):
                with self.assertRaisesRegex(SystemExit, "unresolved @rpath dependency"):
                    release_asset.require_macos_bundle(app, game_root)

    def test_non_macho_loader_target_is_rejected(self) -> None:
        dependencies = "cataclysm-tiles:\n\t@executable_path/libSDL3.dylib (compatibility version 0.0.0)\n"
        with tempfile.TemporaryDirectory() as temp_dir:
            app, game_root = self.make_app(Path(temp_dir))
            game_binary = game_root / "cataclysm-tiles"
            (game_root / "libSDL3.dylib").write_bytes(b"not Mach-O")

            def non_macho_side_effect(command: list[str]) -> subprocess.CompletedProcess[str]:
                return self.command_side_effect(command, dependencies=dependencies)

            with mock.patch.object(release_asset, "macos_macho_candidates", return_value=[game_binary]), \
                    mock.patch.object(release_asset, "run_command", side_effect=non_macho_side_effect):
                with self.assertRaisesRegex(SystemExit, "not a verified universal Mach-O"):
                    release_asset.require_macos_bundle(app, game_root)

    def test_app_relative_rpath_dependency_is_accepted(self) -> None:
        dependencies = "cataclysm-tiles:\n\t@rpath/libSDL3.dylib (compatibility version 0.0.0)\n"
        rpaths = "Load command 1\n          cmd LC_RPATH\n      cmdsize 48\n         path @executable_path/lib (offset 12)\n"
        with tempfile.TemporaryDirectory() as temp_dir:
            app, game_root = self.make_app(Path(temp_dir))
            game_binary = game_root / "cataclysm-tiles"
            bundled_dir = game_root / "lib"
            bundled_dir.mkdir()
            (bundled_dir / "libSDL3.dylib").write_bytes(b"fake bundled dylib")

            def app_relative_side_effect(command: list[str]) -> subprocess.CompletedProcess[str]:
                return self.command_side_effect(command, dependencies=dependencies, rpaths=rpaths)

            bundled_library = bundled_dir / "libSDL3.dylib"
            with mock.patch.object(
                release_asset,
                "macos_macho_candidates",
                return_value=[game_binary, bundled_library],
            ), \
                    mock.patch.object(release_asset, "run_command", side_effect=app_relative_side_effect):
                with redirect_stderr(StringIO()):
                    diagnostics = release_asset.require_macos_bundle(app, game_root)

        self.assertEqual(diagnostics["architecture_checked_files"], 2)


if __name__ == "__main__":
    unittest.main()
