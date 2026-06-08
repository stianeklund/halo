#!/usr/bin/env python3
"""Probe 2290 (and any crashing target): capture oracle vs lifted memory-write
traces even when the lifted side crashes after performing its real side effects.
Wraps _run_function to stash the last oracle/lifted CPUState, then diffs writes
at the meaningful object offsets.

Usage:
  PROBE_FUNC=FUN_001a2290 PROBE_SNAP=artifacts/snapshot_FUN_001a2290_live.json \
  PROBE_OBJ=0x800bf3f8 BIPED_REAL_TAGS=1 python3 _probe_sideeffects.py
"""
import os
import sys
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import unicorn_diff as U  # noqa: E402

_states = {"oracle": None, "lifted": None}
_orig_run = U._run_function


def _wrap(code, abi, seed_vec, **kw):
    s = _orig_run(code, abi, seed_vec, **kw)
    key = "lifted" if kw.get("lifted") else "oracle"
    # keep the FIRST seed's states (deterministic given snapshot)
    if _states[key] is None:
        _states[key] = s
    return s


U._run_function = _wrap

func = os.environ["PROBE_FUNC"]
snap = os.environ["PROBE_SNAP"]
obj = int(os.environ.get("PROBE_OBJ", "0"), 16)

U.run_diff(func_name=func, num_seeds=1, allow_stubs=True,
           float_tolerance_ulp=32, mem_trace=True,
           state_snapshot=Path(snap), verbose=False, quiet=True,
           save_log=False, record_leaf=False)

orc = _states["oracle"]
lft = _states["lifted"]


def finals(st):
    d = {}
    if st is None:
        return d
    for w in st.mem_writes:
        d[(w.address, w.size)] = w.value
    return d


fo = finals(orc)
fl = finals(lft)
print(f"oracle: error={orc.error if orc else 'NONE'} writes={len(fo)}")
print(f"lifted: error={lft.error if lft else 'NONE'} writes={len(fl)}")

# Key 2290 side effects (relative to object base):
KEY = {0x18: "vel.x", 0x1c: "vel.y", 0x20: "vel.z",
       0x424: "flag_launched", 0x45c: "clear_byte", 0x430: "reset_i32"}
print("\n-- side-effect writes at object offsets --")
for rel, name in sorted(KEY.items()):
    a = obj + rel
    ov = {sz: v for (aa, sz), v in fo.items() if aa == a}
    lv = {sz: v for (aa, sz), v in fl.items() if aa == a}
    print(f"  obj+0x{rel:03x} {name:14s} oracle={ov} lifted={lv} "
          f"{'MATCH' if ov == lv else ('DIFF' if (ov and lv) else 'one-side')}")

# All object-datum writes (region 0x800b0000..0x80400000), to see the full picture
print("\n-- all writes into object/heap regions (>=0x80000000) --")
ks = sorted(set([k for k in fo if k[0] >= 0x80000000] +
                [k for k in fl if k[0] >= 0x80000000]))
for (a, sz) in ks:
    ov = fo.get((a, sz)); lv = fl.get((a, sz))
    tag = "MATCH" if ov == lv else ("DIFF" if (ov is not None and lv is not None) else "one-side")
    print(f"  0x{a:08x}/{sz} oracle={ov} lifted={lv}  {tag}")
