#!/usr/bin/env python3
"""Guard: the leaf_cache write must not be keyed by a loop-clobbered variable.

`run_diff` binds the target's address once near the top, then ~1100 lines later
records coverage/confidence to leaf_cache.json. In between it runs six loops
spelled `for addr, ... in` (the global_reads map and the five mem-trace diff
lists). Python `for` targets are ordinary assignments at function scope, so any
of those rebinds the same `addr` the recorder later reads -- and the
measurement gets filed under a *memory* address.

That is not hypothetical: leaf_cache.json accumulated the keys 0x3f800034
(the float bits of 1.0f, +0x34), 0xccccccda (MSVC's 0xCC stack fill) and
0x700804, none of which is a function in kb.json. The cost is two-sided --
junk keys in a committed file, and the real target's entry silently never
updating, which then feeds the selector's eq_high_conf boost and the pipeline's
confidence display.

An AST check rather than a behavioural one, because the trigger depends on
whether a given run happens to observe global reads or trace differences: the
shape is always wrong, even on runs where it does not bite.
"""
import ast
import sys
import unittest
from pathlib import Path

SRC = Path(__file__).resolve().parent / "unicorn_diff.py"
RECORDER = "_record_confidence"
FUNC = "run_diff"


def _loop_bound_names(node):
    """Every name bound by a `for` target anywhere inside `node`."""
    names = set()
    for sub in ast.walk(node):
        if isinstance(sub, (ast.For, ast.AsyncFor)):
            for t in ast.walk(sub.target):
                if isinstance(t, ast.Name):
                    names.add(t.id)
    return names


def _find_func(tree, name):
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == name:
            return node
    return None


class LeafCacheKeyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tree = ast.parse(SRC.read_text(encoding="utf-8"))
        cls.fn = _find_func(cls.tree, FUNC)

    def test_run_diff_exists(self):
        self.assertIsNotNone(self.fn, f"{FUNC} not found in {SRC.name}")

    def test_recorder_arg_is_not_loop_clobbered(self):
        calls = [n for n in ast.walk(self.fn)
                 if isinstance(n, ast.Call)
                 and isinstance(n.func, ast.Name)
                 and n.func.id == RECORDER]
        self.assertTrue(calls, f"no {RECORDER}() call inside {FUNC}")
        clobbered = _loop_bound_names(self.fn)
        for call in calls:
            self.assertTrue(call.args, f"{RECORDER}() called with no arguments")
            first = call.args[0]
            self.assertIsInstance(
                first, ast.Name,
                f"{RECORDER}()'s address argument should be a plain name so this "
                f"check can reason about it; got {type(first).__name__}")
            self.assertNotIn(
                first.id, clobbered,
                f"{RECORDER}() is keyed on '{first.id}', which a `for` loop in "
                f"{FUNC} also binds -- the coverage measurement will be filed "
                f"under whatever that loop left behind. Capture the target "
                f"address in a name no loop touches (target_addr).")

    def test_the_loops_that_motivate_this_still_exist(self):
        """If the `for addr, ...` loops ever go away, this guard is vacuous --
        fail loudly rather than pass for the wrong reason."""
        self.assertIn(
            "addr", _loop_bound_names(self.fn),
            f"no loop in {FUNC} binds 'addr' any more; re-point or retire this "
            f"test rather than leaving it passing vacuously.")


if __name__ == "__main__":
    unittest.main(verbosity=2)
