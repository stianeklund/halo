"""In-memory tests for the candidate neutrality guard."""

import unittest

from tools.recovery.coff_candidate_guard import GuardError, compare_snapshots, _self_test


class CandidateGuardTests(unittest.TestCase):
    def test_required_cases(self):
        self.assertEqual(_self_test(), 0)

    def test_malformed_baseline_is_rejected(self):
        with self.assertRaises(GuardError):
            compare_snapshots({}, {})

    def test_referenced_data_change_is_rejected(self):
        target = {"name": "literal", "section": ".rdata#0", "value": 0,
                  "data": {"section": ".rdata#0", "value": 0,
                            "bytes_hex": "6f6c6400"}}
        section = {"id": ".text#0", "name": ".text", "characteristics": 32,
                   "raw_size": 4, "executable": True, "raw_bytes_hex": "e800000000",
                   "relocations": [{"offset": 1, "type": 6, "target": target}]}
        before = {"schema": 1, "kind": "coff-candidate-neutrality",
                  "sections": [section], "assertion_metadata": []}
        changed_target = dict(target, data=dict(target["data"], bytes_hex="6e657700"))
        changed = dict(before, sections=[dict(section, relocations=[dict(
            section["relocations"][0], target=changed_target)])])
        self.assertFalse(compare_snapshots(before, changed)["ok"])


if __name__ == "__main__":
    unittest.main()
