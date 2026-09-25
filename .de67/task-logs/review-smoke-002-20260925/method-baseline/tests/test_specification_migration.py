import tempfile
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from specification import SpecificationError, render_functional_specification, resolve, resolve_path


class SpecificationMigrationTests(unittest.TestCase):
    def test_fs_resolves_without_companion_file_and_can_be_refrozen(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            fs = root / "FS.md"
            fs.write_text("# Functional\nStatus: Frozen\n", encoding="utf-8")
            self.assertEqual(resolve(root).path, fs)
            fs.write_text("# Functional\nStatus: Refrozen\n", encoding="utf-8")
            self.assertIn("Refrozen", resolve(root).text)
            self.assertFalse((root / "DFS.md").exists())

    def test_legacy_file_is_never_a_specification_fallback(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            legacy = root / "DFS.md"
            legacy.write_text("# Historical content\n", encoding="utf-8")
            with self.assertRaisesRegex(SpecificationError, "Missing functional specification"):
                resolve(root)
            with self.assertRaisesRegex(SpecificationError, "Use FS.md"):
                resolve_path(legacy)
            (root / "FS.md").write_text("# Canonical content\n", encoding="utf-8")
            self.assertEqual(resolve(root).text, "# Canonical content\n")

    def test_functional_render_removes_delivery_status_only(self):
        text = "# FS\nImplementation status:\n- [x] delivered\n<!-- DE67:DFS-SLICE:END claim=R-X -->\n"
        rendered = render_functional_specification(text)
        self.assertNotIn("Implementation status:", rendered)
        self.assertIn("# FS", rendered)

    def test_functional_render_preserves_contract_before_status_tracking(self):
        text = (
            "<!-- DE67:DFS-SLICE:BEGIN id=R-X-S001 claim=R-X -->\n"
            "The adapter binds sender, workspace, and revision before delivery.\n\n"
            "Implementation status:\n"
            "- [x] R-X — historical delivery projection\n"
            "<!-- DE67:DFS-SLICE:END id=R-X-S001 claim=R-X -->\n"
        )
        rendered = render_functional_specification(text)
        self.assertIn("The adapter binds sender, workspace, and revision", rendered)
        self.assertNotIn("Implementation status:", rendered)
        self.assertNotIn("historical delivery projection", rendered)


if __name__ == "__main__":
    unittest.main()
