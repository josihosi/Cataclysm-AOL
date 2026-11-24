import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

from build_prerequisites import build_tool_status
import build_source_bound_macos as builder


class BuildPrerequisitesTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / "tools").mkdir()
        (self.root / "data/shaders").mkdir(parents=True)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        shutil.copyfile(Path(__file__).resolve().parents[1] / "build_shaders.py",
                        self.root / "tools/build_shaders.py")
        self.env = {**os.environ, "PATH": str(self.bin), "SDL3": "1"}
        for key in ("GLSLANG", "SDL_SHADERCROSS", "BUILD_SHADER_FORMATS", "FRAMEWORK"):
            self.env.pop(key, None)
        for name in ("make", "pkg-config", "python3"):
            self.tool(name)

    def tool(self, name):
        path = self.bin / (name + (".exe" if os.name == "nt" else ""))
        path.touch()
        path.chmod(0o700)
        return path

    def check_tools(self, platform="darwin", **kwargs):
        return build_tool_status(self.root, tiles=True, environment=self.env, platform=platform, **kwargs)

    def test_msl_and_dxil_check_both_tools_and_accept_explicit_executable(self):
        for platform, expected in (("darwin", "msl"), ("win32", "dxil")):
            with self.subTest(platform=platform):
                status = self.check_tools(platform)
                self.assertEqual(status["shader_formats"], expected)
                self.assertEqual([row["tool"] for row in status["missing"]], ["glslangValidator", "shadercross"])
        glslang = self.tool("custom-glslang")
        cross = self.tool("custom-cross")
        self.env.update(GLSLANG=str(glslang.relative_to(self.root)), SDL_SHADERCROSS=str(cross))
        self.assertEqual(self.check_tools()["status"], "ready")
        self.env["SDL_SHADERCROSS"] = str(self.bin)
        self.assertEqual(self.check_tools()["missing"][0]["override"], "SDL_SHADERCROSS")

    def test_spv_override_does_not_require_shadercross(self):
        self.env["BUILD_SHADER_FORMATS"] = "spv"
        self.tool("glslangValidator")
        self.assertEqual(self.check_tools()["status"], "ready")

    def test_shader_stamp_and_native_force_match_make(self):
        stamp = self.root / "data/shaders/build-msl.stamp"
        stamp.touch()
        self.assertEqual(self.check_tools()["status"], "ready")
        self.assertEqual(self.check_tools(always_make=True)["status"], "blocked")
        shader = stamp.with_name("new.frag")
        shader.touch()
        os.utime(shader, ns=(stamp.stat().st_mtime_ns + 1_000_000_000,) * 2)
        self.assertEqual(self.check_tools()["status"], "blocked")

    def test_missing_tools_block_builder_before_make_or_build_logs(self):
        missing = {"status": "blocked", "missing": [{"tool": "shadercross"}]}
        log_dir = self.root / "logs"
        with patch.object(sys, "platform", "darwin"), patch.object(sys, "argv", [
                "build_source_bound_macos.py", "--log-dir", str(log_dir)]), \
                patch.object(builder, "ROOT", self.root), patch.object(builder, "build_tool_status", return_value=missing), \
                patch.object(builder.subprocess, "run") as run, patch("sys.stderr"):
            self.assertEqual(builder.main(), 1)
        run.assert_not_called()
        self.assertFalse(log_dir.exists())


if __name__ == "__main__":
    unittest.main()
