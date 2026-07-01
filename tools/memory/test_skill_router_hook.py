#!/usr/bin/env python3
"""Offline tests for the Claude skill router hook."""
import unittest

from tools.memory import skill_router_hook as router


class ExceptionRouterContract(unittest.TestCase):
    def test_exception_text_is_detected(self):
        prompt = """EXCEPTION halt in c:\\halo\\SOURCE\\tag_files\\tag_groups.c,#3089
EIP=0x0072a2ac CR2=0x00000000
[0] 0x0072a2ac
"""
        self.assertTrue(router.looks_like_exception(prompt))

    def test_plain_debug_word_is_not_exception_text(self):
        self.assertFalse(router.looks_like_exception("debug a wrong color regression"))

    def test_symbolizer_report_is_appended_to_skill_message(self):
        message = router.build_message(
            [(["debug", "crash-triage"], "runtime symptom router")],
            "EIP       0x0072a2ac  compiled      nearest-export    foo+0x10",
        )
        self.assertIn("[exception-symbolizer]", message)
        self.assertIn("symbolize", message)
        self.assertIn("build/halo.map", message)
        self.assertIn("foo+0x10", message)


if __name__ == "__main__":
    unittest.main()
