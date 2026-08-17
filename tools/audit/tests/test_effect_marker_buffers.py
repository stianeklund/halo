#!/usr/bin/env python3
"""Unit tests for check_effect_marker_buffers (lift-learnings §48).

Positive fixture reconstructs the FUN_000f8920 grenade-scorch bug
(marker_count=2, float pos[3]). Negative fixtures cover the fixed
shape, the 5*3 hit-effect form, and pointer-parameter call sites.
Also pins the live FUN_000f8920 copy so the overlap fill cannot regress.
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from check_lift_hazards import check_effect_marker_buffers

_FAKE = '/fake/src/halo/test.c'
_ROOT = Path(__file__).resolve().parents[3]


def _run(src):
    return check_effect_marker_buffers(_FAKE, src, src.split('\n'))


def test_short_points_array_flags():
    src = (
        'void FUN_000f8920(int h) {\n'
        '  float pos[3];\n'
        '  float fwd[6];\n'
        '  effect_new_unattached_from_markers(tag, obj, 0, 2, names, pos, fwd,\n'
        '                                     0.0f, 0.0f, 0.0f, 0.0f, 1);\n'
        '}\n'
    )
    hits = _run(src)
    assert hits, 'marker_count=2 with float pos[3] must flag'
    assert 'pos[3]' in hits[0]
    assert 'marker_points' in hits[0]


def test_short_forwards_array_flags():
    src = (
        'void f(void) {\n'
        '  float pos[6];\n'
        '  float fwd[3];\n'
        '  effect_new_unattached_from_markers(tag, obj, 0, 2, names, pos, fwd,\n'
        '                                     0.0f, 0.0f, 0.0f, 0.0f, 1);\n'
        '}\n'
    )
    hits = _run(src)
    assert hits, 'marker_count=2 with float fwd[3] must flag'
    assert 'fwd[3]' in hits[0]
    assert 'marker_forwards' in hits[0]


def test_full_slots_silent():
    src = (
        'void FUN_000f8920(int h) {\n'
        '  float pos[6];\n'
        '  float fwd[6];\n'
        '  pos[3] = pos[0];\n'
        '  pos[4] = pos[1];\n'
        '  pos[5] = pos[2];\n'
        '  effect_new_unattached_from_markers(tag, obj, 0, 2, names, pos, fwd,\n'
        '                                     0.0f, 0.0f, 0.0f, 0.0f, 1);\n'
        '}\n'
    )
    assert not _run(src), 'pos[6]/fwd[6] with count=2 must be silent'


def test_product_dim_silent():
    src = (
        'void hit(void) {\n'
        '  float marker_points[5 * 3];\n'
        '  float marker_forwards[5 * 3];\n'
        '  effect_new_unattached_from_markers(tag, obj, 0, 5, names,\n'
        '      marker_points, marker_forwards, 0.0f, 0.0f, 0.0f, 0.0f, 1);\n'
        '}\n'
    )
    assert not _run(src), 'float[5 * 3] with count=5 must be silent'


def test_pointer_param_skipped():
    src = (
        'void FUN_000f7e60(float *marker_points, float *marker_forwards) {\n'
        '  effect_new_unattached_from_markers(tag, obj, NULL, 5, names,\n'
        '      marker_points, marker_forwards, 0.0f, 0.0f, 0.0f, 0.0f, 1);\n'
        '}\n'
    )
    assert not _run(src), 'pointer params have unknown size — skip'


def test_count_one_skipped():
    src = (
        'void f(void) {\n'
        '  float pos[3];\n'
        '  float fwd[3];\n'
        '  effect_new_unattached_from_markers(tag, -1, NULL, 1, NULL,\n'
        '      pos, fwd, 0.0f, 0.0f, 0.0f, 0.0f);\n'
        '}\n'
    )
    assert not _run(src), 'marker_count=1 only needs 3 floats'


def test_hazard_ok_suppresses():
    src = (
        'void f(void) {\n'
        '  float pos[3];\n'
        '  float fwd[6];\n'
        '  effect_new_unattached_from_markers(tag, obj, 0, 2, names, pos, fwd,\n'
        '      0.0f, 0.0f, 0.0f, 0.0f, 1); /* hazard-ok: marker-buf */\n'
        '}\n'
    )
    assert not _run(src), 'hazard-ok on the call must suppress'


def test_attached_short_points_flags():
    src = (
        'void f(void) {\n'
        '  float pts[3];\n'
        '  float fwd[15];\n'
        '  effect_new_attached_from_markers(tag, obj, obj, 0, 5, names,\n'
        '      pts, fwd, 1.0f, 0.0f, 0.0f, 0.0f);\n'
        '}\n'
    )
    hits = _run(src)
    assert hits, 'attached count=5 with pts[3] must flag'


def test_fun_000f8920_gravity_slot_filled():
    src = (_ROOT / 'src/halo/items/projectiles.c').read_text()
    start = src.index('void FUN_000f8920')
    end = src.index('\nvoid ', start + 1)
    body = src[start:end]
    assert 'float pos[6]' in body
    assert 'pos[3] = pos[0]' in body
    assert 'pos[4] = pos[1]' in body
    assert 'pos[5] = pos[2]' in body
    hits = check_effect_marker_buffers(
        str(_ROOT / 'src/halo/items/projectiles.c'), src, src.split('\n')
    )
    assert not hits, 'live FUN_000f8920 must not trip the detector:\n' + '\n'.join(hits)


if __name__ == '__main__':
    tests = [
        test_short_points_array_flags,
        test_short_forwards_array_flags,
        test_full_slots_silent,
        test_product_dim_silent,
        test_pointer_param_skipped,
        test_count_one_skipped,
        test_hazard_ok_suppresses,
        test_attached_short_points_flags,
        test_fun_000f8920_gravity_slot_filled,
    ]
    failed = 0
    for fn in tests:
        try:
            fn()
            print('  PASS  %s' % fn.__name__)
        except Exception as exc:
            failed += 1
            print('  FAIL  %s: %s' % (fn.__name__, exc))
    if failed:
        raise SystemExit(1)
    print('ok')
