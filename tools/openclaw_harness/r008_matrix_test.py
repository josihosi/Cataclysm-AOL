#!/usr/bin/env python3
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from r008_matrix import R008_MATRIX, validate_r008_matrix


class R008MatrixTest(unittest.TestCase):
    def test_matrix_is_finite_and_covers_both_ecologies(self):
        result = validate_r008_matrix()
        self.assertEqual(result, {"status": "green", "row_count": 4, "errors": []})

    def test_duplicate_or_partial_rows_fail_closed(self):
        duplicate = list(R008_MATRIX) + [dict(R008_MATRIX[0])]
        self.assertIn("duplicate_row_identity", validate_r008_matrix(duplicate)["errors"])
        partial = dict(R008_MATRIX[0])
        partial.pop("receipt")
        self.assertEqual(validate_r008_matrix([partial])["status"], "red")


if __name__ == "__main__":
    unittest.main()
