#!/usr/bin/env python3
"""Unit tests for the five new lints added to check_lift_hazards.py.

Each check has one positive fixture (should fire) and one negative fixture
(should be silent).  Run with:  python3 -m pytest tools/audit/tests/
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from check_lift_hazards import (
    check_concat_survival,
    check_float_int_bit_smuggling,
    check_addr_value_add,
    check_param_loop_corruption,
    check_discarded_result,
)

_FAKE = '/fake/src/halo/test.c'


def _run(func, src):
    lines = src.split('\n')
    return func(_FAKE, src, lines)


# ---------------------------------------------------------------------------
# §13 CONCAT survival
# ---------------------------------------------------------------------------

def test_concat_survival_positive():
    src = 'short x = CONCAT22(a, b);\n'
    assert _run(check_concat_survival, src), 'should flag CONCAT22'

def test_concat_survival_in_comment_only():
    src = '/* Original: CONCAT22(cursor_y, char_height) */\nshort x = (a << 8) | b;\n'
    assert not _run(check_concat_survival, src), 'CONCAT in comment only — must not flag'

def test_concat_survival_negative():
    src = 'short x = (a << 8) | (b & 0xff);\n'
    assert not _run(check_concat_survival, src), 'no CONCAT token — must not flag'


# ---------------------------------------------------------------------------
# §6 float-as-pointer bit smuggling
# ---------------------------------------------------------------------------

def test_float_smuggle_positive():
    src = (
        'void f(float v) {\n'
        '  short *ps = (short *)(int)(v);\n'
        '  float r = (float)(int)ps;\n'
        '}\n'
    )
    assert _run(check_float_int_bit_smuggling, src), 'should flag smuggled float bits'

def test_float_smuggle_negative_plain_int():
    src = (
        'void f(int count) {\n'
        '  float cf = (float)(int)count;\n'
        '}\n'
    )
    assert not _run(check_float_int_bit_smuggling, src), \
        'plain int-to-float — no pointer store, must not flag'

def test_float_smuggle_suppressed():
    src = (
        'void f(float v) {\n'
        '  short *ps = (short *)(int)(v);\n'
        '  float r = (float)(int)ps; /* hazard-ok: numeric-truncation */\n'
        '}\n'
    )
    assert not _run(check_float_int_bit_smuggling, src), 'hazard-ok suppresses'


# ---------------------------------------------------------------------------
# §17 address offset mis-rendered as value add
# ---------------------------------------------------------------------------

def test_addr_value_add_positive():
    src = 'short w = *(short *)0x506588 + 2;\n'
    results = _run(check_addr_value_add, src)
    assert results, 'should flag *(T*)ADDR + 2'

def test_addr_value_add_inner_paren():
    # double-dereference: correct address-add inside outer cast
    src = 'float *p = *(float *)(*(int *)0x31fc44 + 4);\n'
    assert not _run(check_addr_value_add, src), \
        'inner double-deref — +4 is address math, must not flag'

def test_addr_value_add_counter_lhs():
    src = '*(int *)0x45b1d4 = *(int *)0x45b1d4 + 1;\n'
    assert not _run(check_addr_value_add, src), \
        'counter increment with same address on LHS — must not flag'

def test_addr_value_add_suppressed():
    src = 'short w = *(short *)0x506588 + 2; /* hazard-ok: counter-increment */\n'
    assert not _run(check_addr_value_add, src), 'hazard-ok suppresses'

def test_addr_value_add_large_const():
    src = 'int x = *(int *)0x506588 + 0x20;\n'
    assert not _run(check_addr_value_add, src), 'const >= 0x10 — must not flag'


# ---------------------------------------------------------------------------
# §4 parameter corruption by loop
# ---------------------------------------------------------------------------

def test_param_loop_positive():
    src = (
        'void f(char *dst, char *src) {\n'
        '  for (int i = 0; i < 10; i++) {\n'
        '    *dst++ = *src++;\n'
        '  }\n'
        '  some_call(dst);\n'
        '}\n'
    )
    results = _run(check_param_loop_corruption, src)
    assert results, 'should flag dst used after loop that increments it'

def test_param_loop_negative_local_var():
    src = (
        'void f(char *dst, char *src) {\n'
        '  char *p = dst;\n'
        '  for (int i = 0; i < 10; i++) {\n'
        '    *p++ = *src++;\n'
        '  }\n'
        '  some_call(dst);\n'
        '}\n'
    )
    assert not _run(check_param_loop_corruption, src), \
        'loop uses copy p, not param dst — must not flag'

def test_param_loop_suppressed():
    src = (
        'void f(char *dst, char *src) {\n'
        '  for (int i = 0; i < 10; i++) { /* hazard-ok: intentional-param-advance */\n'
        '    *dst++ = *src++;\n'
        '  }\n'
        '  some_call(dst);\n'
        '}\n'
    )
    assert not _run(check_param_loop_corruption, src), 'hazard-ok suppresses'


# ---------------------------------------------------------------------------
# §8/§11 discarded non-trivial function-call result
# ---------------------------------------------------------------------------

def test_discard_result_var_pattern():
    src = (
        'void f(void) {\n'
        '  int rng = random_range(0, 100);\n'
        '  (void)rng;\n'
        '  do_something();\n'
        '}\n'
    )
    assert _run(check_discarded_result, src), 'should flag (void)rng after assignment'

def test_discard_result_inline_non_exempt():
    src = (
        'void f(int handle) {\n'
        '  (void)do_something(handle);\n'
        '}\n'
    )
    assert _run(check_discarded_result, src), 'should flag (void)named_func(...)'

def test_discard_result_exempt_tag_get():
    src = (
        'void f(int handle) {\n'
        '  (void)tag_get(0x61637472, handle);\n'
        '}\n'
    )
    assert not _run(check_discarded_result, src), 'tag_get is exempted — must not flag'

def test_discard_result_unused_param():
    src = (
        'void f(int a, int b, int c) {\n'
        '  (void)b;\n'
        '  (void)c;\n'
        '}\n'
    )
    assert not _run(check_discarded_result, src), \
        'discarding unused params — no prior call assignment — must not flag'

def test_discard_result_suppressed():
    src = (
        'void f(void) {\n'
        '  int r = some_func();\n'
        '  (void)r; /* hazard-ok: intentional-discard */\n'
        '}\n'
    )
    assert not _run(check_discarded_result, src), 'hazard-ok suppresses'
