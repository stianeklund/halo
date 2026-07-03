#!/usr/bin/env python3
"""Probe: print the actual oracle-only/lifted-only/value-diff write ADDRESSES
for one target+snapshot, to classify a mem-trace divergence as benign stack
scratch vs real game-state value diff. Monkeypatches compare_mem_traces to log.
"""
import sys
import os

sys.argv = sys.argv[:1]  # keep argparse in unicorn_diff happy if it peeks
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import state as state_mod  # noqa: E402

_orig = state_mod.compare_mem_traces
_printed = {"n": 0}


def _patched(oracle, lifted):
    td = _orig(oracle, lifted)
    if td.has_differences() and _printed["n"] < 3:
        _printed["n"] += 1
        sys.stderr.write(f"\n[PROBE seed#{_printed['n']}]\n")
        for a, ov, lv in td.value_diffs:
            sys.stderr.write(f"  VALUE-DIFF addr=0x{a:08x} oracle={ov!r} lifted={lv!r}\n")
        for a in td.oracle_only:
            sys.stderr.write(f"  ORACLE-ONLY addr=0x{a:08x}\n")
        for a in td.lifted_only:
            sys.stderr.write(f"  LIFTED-ONLY addr=0x{a:08x}\n")
    return td


state_mod.compare_mem_traces = _patched

import unicorn_diff as U  # noqa: E402
# run_diff does a local `import state as state_mod` — same module object we
# already patched at top-of-file, so the patch is live there too.

func = sys.argv[0] if False else os.environ["PROBE_FUNC"]
snap = os.environ["PROBE_SNAP"]
seeds = int(os.environ.get("PROBE_SEEDS", "5"))

rc = U.run_diff(
    func_name=func,
    num_seeds=seeds,
    allow_stubs=True,
    float_tolerance_ulp=32,
    mem_trace=True,
    state_snapshot=__import__("pathlib").Path(snap),
    verbose=False,
    quiet=True,
    save_log=False,
    record_leaf=False,
)
sys.stderr.write(f"\n[PROBE done rc={rc}]\n")
