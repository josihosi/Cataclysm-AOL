from __future__ import annotations

import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "symbol_codec.py"
SPEC = importlib.util.spec_from_file_location("de67_symbol_codec", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
codec = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = codec
SPEC.loader.exec_module(codec)


class SymbolCodecTests(unittest.TestCase):
    def test_nested_policy_value_round_trips_canonically(self) -> None:
        value = {
            "rules": [
                {"action": "wait", "facts": ["live_task", "live_task"], "priority": 7},
                {"action": "stop", "enabled": True, "note": None, "priority": -1},
            ]
        }
        encoded = codec.encode(value)
        self.assertEqual(codec.decode(encoded), value)
        self.assertEqual(codec.encode(codec.decode(encoded)), encoded)

    def test_repeated_language_is_interned_once(self) -> None:
        repeated = "disposition_relevant_suggestions"
        value = {"a": [repeated] * 20, "b": repeated}
        encoded = codec.encode(value)
        self.assertEqual(encoded.count(repeated.encode("utf-8")), 1)
        self.assertLess(len(encoded), len(json.dumps(value).encode("utf-8")))

    def test_truncation_noncanonical_table_and_trailing_data_fail(self) -> None:
        encoded = codec.encode({"a": "b"})
        for candidate in (encoded[:-1], encoded + b"x", b"\x02\x01b\x01a\x00"):
            with self.subTest(candidate=candidate):
                with self.assertRaises(codec.CodecError):
                    codec.decode(candidate)


if __name__ == "__main__":
    unittest.main()
