"""Safe-mutation guards in third_party/decomp-permuter/src (safety.py).

Each case is a known behavior-changing move.  With PERMUTER_SAFE_MUTATIONS=1
the pass must never produce it; with the guard off it must (so the test
exercises the move it claims to block).
"""

import os
import random
import re
import sys
from pathlib import Path

import pytest

PERMUTER = Path(__file__).resolve().parents[2] / "third_party" / "decomp-permuter"
sys.path.insert(0, str(PERMUTER))

try:
    from src import ast_util, randomizer  # noqa: E402
except Exception as exc:  # pragma: no cover
    pytest.skip("decomp-permuter import failed: %s" % exc, allow_module_level=True)

SEEDS = 400

RGB = """
typedef struct { unsigned char rgba[4]; } s3tc_color_t;
void f(unsigned short *rgb, unsigned char *color)
{
  unsigned short value;
  unsigned char c;
  s3tc_color_t result;
  value = *rgb;
  c = (unsigned char)(value << 3);
  result.rgba[0] = c;
  result.rgba[3] = 0;
  *((s3tc_color_t *) color) = result;
}
"""

NULL_CHECK = """
struct s { int a; };
int g(void);
int f(struct s *p)
{
  int x;
  int y;
  y = g();
  if (p == 0)
    return y;
  x = p->a + 1;
  return x + y;
}
"""

EXPAND = """
int g(int);
int f(int a)
{
  int x;
  x = a + 1;
  a = g(a);
  return x * a;
}
"""

CAST = """
int f(int i, short s)
{
  short x;
  x = (short)i;
  return x + s;
}
"""


def _run(source, fn_name, pass_fn, safe, seeds=SEEDS):
    os.environ["PERMUTER_SAFE_MUTATIONS"] = "1" if safe else "0"
    outputs = []
    try:
        for seed in range(seeds):
            ast = ast_util.parse_c(source)
            fn, _ = ast_util.extract_fn(ast, fn_name)
            ast_util.normalize_ast(fn, ast)
            indices = ast_util.compute_node_indices(fn)
            rng = random.Random(seed)
            region = randomizer.get_randomization_region(fn, indices, rng)
            try:
                pass_fn(fn, ast, indices, region, rng)
            except randomizer.RandomizationFailure:
                continue
            outputs.append(ast_util.to_c(fn))
    finally:
        os.environ.pop("PERMUTER_SAFE_MUTATIONS", None)
    return outputs


def _store_before_alpha(src):
    store = src.find("*((s3tc_color_t *) color) = result")
    alpha = src.find("result.rgba[3] = 0")
    return store != -1 and alpha != -1 and store < alpha


def _deref_before_null_check(src):
    check = src.find("if (p == 0)")
    deref = src.find("p->a")
    return check != -1 and deref != -1 and deref < check


def _expanded_after_overwrite(src):
    return "(a + 1) * a" in src


_NARROWING = re.compile(r"\((unsigned )?char\)|\(unsigned int\)|\(float\)|\(unsigned short\) *s\b")


def _value_changing_cast(src):
    return bool(_NARROWING.search(src))


@pytest.mark.parametrize("source,detector,pass_name", [
    (RGB, _store_before_alpha, "perm_reorder_stmts"),
    (NULL_CHECK, _deref_before_null_check, "perm_temp_for_expr"),
    (EXPAND, _expanded_after_overwrite, "perm_expand_expr"),
    (CAST, _value_changing_cast, "perm_cast_simple"),
])
def test_guard_blocks_known_bad_move(source, detector, pass_name):
    pass_fn = getattr(randomizer, pass_name)
    unsafe = _run(source, "f", pass_fn, safe=False)
    assert any(detector(o) for o in unsafe), "case never produced the bad move unguarded"
    safe = _run(source, "f", pass_fn, safe=True)
    assert not any(detector(o) for o in safe)


def test_safe_expand_still_substitutes_stable_values():
    src = """
int f(int a, int b)
{
  int x;
  int y;
  x = a + 1;
  y = b;
  return x * y;
}
"""
    outs = _run(src, "f", randomizer.perm_expand_expr, safe=True)
    assert any("(a + 1) * y" in o for o in outs)


def test_safe_temp_for_expr_still_hoists():
    outs = _run(NULL_CHECK, "f", randomizer.perm_temp_for_expr, safe=True)
    assert outs


def test_safe_reorder_still_moves_independent_statements():
    src = """
int f(int a, int b)
{
  int x;
  int y;
  x = a + 1;
  y = b + 2;
  return x + y;
}
"""
    outs = _run(src, "f", randomizer.perm_reorder_stmts, safe=True)
    assert any(o.find("y = b + 2") < o.find("x = a + 1") for o in outs)


def test_safe_cast_keeps_value_preserving_casts():
    outs = _run(CAST, "f", randomizer.perm_cast_simple, safe=True)
    assert outs
    assert any("(int) s" in o or "(long) s" in o or "(int) x" in o for o in outs)


def test_safe_mode_never_retypes_locals():
    outs = _run(CAST, "f", randomizer.perm_randomize_internal_type, safe=True)
    assert not outs
