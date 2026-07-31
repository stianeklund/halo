import tempfile
import unittest
from pathlib import Path

from tools.recovery.assert_metadata_guard import (GuardError, _self_test,
                                                   capture_sources, compare_snapshots)


class AssertMetadataGuardTests(unittest.TestCase):
    def test_required_self_test(self):
        self.assertEqual(_self_test(), 0)

    def test_line_and_condition_changes_fail(self):
        with tempfile.NamedTemporaryFile("w", suffix=".c", dir=Path(__file__).parent) as stream:
            stream.write("assert_halt(value);\n")
            stream.flush()
            before = capture_sources([stream.name])
            Path(stream.name).write_text("\nassert_halt(changed);\n", encoding="utf-8")
            after = capture_sources([stream.name])
        self.assertFalse(compare_snapshots(before, after)["ok"])

    def test_multiline_nested_comments_and_strings(self):
        with tempfile.NamedTemporaryFile("w", suffix=".h", dir=Path(__file__).parent) as stream:
            stream.write('assert_halt(\n'
                         '    outer(1, (2 + 3)) /* comment ) */ &&\n'
                         '    "text, )" && \'x\'\n'
                         ');\n')
            stream.flush()
            record = capture_sources([stream.name])["assertions"][0]
        self.assertEqual(record["line"], 1)
        self.assertEqual(record["condition"], 'outer(1, (2 + 3)) && "text, )" && \'x\'')

    def test_explicit_at_preserves_metadata(self):
        with tempfile.NamedTemporaryFile("w", suffix=".c", dir=Path(__file__).parent) as stream:
            stream.write('assert_halt_at("source.c", 0x22, (a && b));\n')
            stream.flush()
            record = capture_sources([stream.name])["assertions"][0]
        self.assertEqual(record["explicit_file"], '"source.c"')
        self.assertEqual(record["explicit_line"], "0x22")
        self.assertEqual(record["condition"], "(a && b)")

    def test_malformed_baseline_and_unterminated_fail_closed(self):
        with self.assertRaises(GuardError):
            compare_snapshots({}, {})
        with tempfile.NamedTemporaryFile("w", suffix=".c", dir=Path(__file__).parent) as stream:
            stream.write("assert_halt(value\n")
            stream.flush()
            with self.assertRaises(GuardError):
                capture_sources([stream.name])


if __name__ == "__main__":
    unittest.main()
