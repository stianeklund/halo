#!/usr/bin/env python3
"""Self-test for the Step 4 real-frame value corpus (concolic injection).

Run with:
    python3 tools/equivalence/test_value_corpus.py

Tests the CONSUMER half (the half exercised in this design):
  1. load_value_corpus normalizes hex/int keys and values; empty on missing.
  2. With a corpus, generate_memory_injections injects the real engine value.
  3. Without a corpus, it falls back to invented constants (no real value).

NOTE (scope): this proves the consumer logic. The PRODUCER
(halorec_frame_sweep.py --emit-value-corpus) harvests global_reads from
full-snapshot sweep runs, a configuration that suppresses the zero-valued
globals concolic targets — so populating a non-empty corpus in practice is
unverified and likely needs a snapshot-less probe pass. See
docs/halorec-timeseries-leverage.md.
"""

import json
import os
import struct
import sys
import tempfile
import types

sys.path.insert(0, os.path.dirname(__file__))

from concolic import generate_memory_injections, load_value_corpus

A = 0x26C118  # a plausible non-spurious global address


def test_load_normalizes():
    d = tempfile.mkdtemp()
    p = os.path.join(d, "c.json")
    json.dump({"0x26c118": ["0x5", 3], "0x5aa6d4": [0]}, open(p, "w"))
    c = load_value_corpus(p)
    assert c == {0x26C118: [5, 3], 0x5AA6D4: [0]}, c
    assert load_value_corpus(None) == {}
    assert load_value_corpus("/no/such/file") == {}
    print("  [1] load_value_corpus normalizes hex/int + empty-safe   OK")


def _fake_uncovered():
    br = types.SimpleNamespace(jcc_id=0, cmp_imm=0, cmp_op_size=4,
                              has_mem_operand=False, mem_disp=0)
    return [types.SimpleNamespace(branch=br, untaken_is_target=True)]


def test_corpus_value_injected():
    want = struct.pack("<I", 0x1234)
    injs = generate_memory_injections(_fake_uncovered(), {A: (4, 0)}, 0x10000,
                                      value_corpus={A: [0x1234]})
    assert any(inj.get(A) == want for inj in injs), ("real value not injected", injs)
    print("  [2] corpus -> injects real engine value 0x1234            OK")


def test_no_corpus_fallback():
    want = struct.pack("<I", 0x1234)
    injs = generate_memory_injections(_fake_uncovered(), {A: (4, 0)}, 0x10000)
    assert injs, "expected fallback injections"
    assert not any(inj.get(A) == want for inj in injs), injs
    print("  [3] no corpus -> invented constants only (no 0x1234)      OK")


def main():
    print("test_value_corpus:")
    test_load_normalizes()
    test_corpus_value_injected()
    test_no_corpus_fallback()
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
