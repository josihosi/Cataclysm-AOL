from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest


SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))
from instruction_context import FALLBACK_GUIDANCE, common_guidance  # noqa: E402


class InstructionContextTests(unittest.TestCase):
    def write_config(self, root: Path, source: Path, *, effective: bool = True) -> None:
        state = root / ".de67/state"
        state.mkdir(parents=True)
        (state / "workspace.json").write_text(json.dumps({"version": 1, "guidance": {
            "source": str(source),
            "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "effective": effective,
            "fallback": not effective,
        }}), encoding="utf-8")

    def test_audited_unchanged_source_needs_no_duplicate_runtime_guidance(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "effective-AGENTS.md"
            source.write_text("already delivered\n", encoding="utf-8")
            self.write_config(root, source)
            self.assertEqual(common_guidance(root), "")

    def test_missing_or_changed_audited_source_returns_minimal_fallback(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "effective-AGENTS.md"
            source.write_text("original\n", encoding="utf-8")
            self.write_config(root, source)
            source.write_text("changed\n", encoding="utf-8")
            self.assertEqual(common_guidance(root), FALLBACK_GUIDANCE)
            source.unlink()
            self.assertEqual(common_guidance(root), FALLBACK_GUIDANCE)

    def test_unavailable_helper_wording_is_honest(self) -> None:
        self.assertIn("if none is available, retrieve the needed source yourself", FALLBACK_GUIDANCE)
        self.assertNotIn("must use Luna", FALLBACK_GUIDANCE)


if __name__ == "__main__":
    unittest.main()
