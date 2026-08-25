#!/usr/bin/env python3
"""Tests for compare_recomp_functions.py.

Pure Python; no XBE or Ghidra. Run:
    python3 tools/analysis/test_compare_recomp_functions.py
"""
from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import compare_recomp_functions as crf


def _disasm_rec(start, section, size=32, prologue=True, method="prologue"):
    return {
        "start": start,
        "end": hex(int(start, 16) + size),
        "size": size,
        "name": "sub_" + start[2:].upper().zfill(8),
        "section": section,
        "confidence": 0.95,
        "detection_method": method,
        "has_prologue": prologue,
        "calls_to": [],
        "called_by": [],
    }


def _vtable_rec(start):
    return {
        "start": start,
        "end": hex(int(start, 16) + 32),
        "size": 32,
        "name": "sub_" + start[2:].upper().zfill(8),
        "section": ".text",
        "category": "game_vtable",
        "method": "vtable_thunk",
        "vtable_addr": "0x002C5244",
        "vtable_index": 0,
    }


class TestLoadTheirs(unittest.TestCase):
    def test_refuses_vtable_inventory(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "identified_functions.json"
            path.write_text(json.dumps([_vtable_rec("0x00012080")]))
            with self.assertRaises(crf.VtableInventoryError) as ctx:
                crf.load_theirs(path)
            self.assertIn("vtable", str(ctx.exception).lower())

    def test_accepts_disasm_inventory(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "functions.json"
            recs = [_disasm_rec("0x00012000", ".text"),
                    _disasm_rec("0x001ECA10", "D3D")]
            path.write_text(json.dumps(recs))
            loaded = crf.load_theirs(path)
            self.assertEqual(len(loaded), 2)


class TestCompareAndImport(unittest.TestCase):
    def setUp(self):
        self.ours = {
            0x12000: {"obj": "vector_math.obj", "decl": "void FUN_00012000(void);"},
            0x1E6AE0: {"obj": "<xdk_stubs>", "decl": "int __stdcall D3DDevice_CreateTexture();"},
        }
        self.theirs = [
            _disasm_rec("0x00012000", ".text"),
            _disasm_rec("0x0007AE4D", ".text", size=35, prologue=False, method="cc_boundary"),
            _disasm_rec("0x001E6AE0", "D3D"),
            _disasm_rec("0x001ECA10", "D3D", size=199),
            _disasm_rec("0x00204F23", "DSOUND", size=28),
            _disasm_rec("0x00222E48", "XNET", size=135),
            _disasm_rec("0x001F1960", "D3D", size=8, prologue=True),  # below min-size
            _disasm_rec("0x002053D4", "DSOUND", size=37, prologue=False, method="call_target"),
        ]
        self.bounds = {0x12000: 0x1207E}

    def test_compare_splits_text_from_xdk(self):
        summary = crf.compare(self.theirs, self.ours, self.bounds)
        self.assertEqual(summary["ours"], 2)
        self.assertEqual(summary["theirs"], 8)
        self.assertEqual(summary["both"], 2)
        self.assertEqual(summary["only_them"], 6)
        self.assertEqual(summary["text_only_them"], 1)
        self.assertEqual(summary["text_gap_prologue"], 0)
        self.assertEqual(summary["by_section"]["D3D"]["only_them_prologue"], 2)

    def test_import_defaults_to_prologue_d3d_dsound(self):
        cands = crf.xdk_candidates(self.theirs, self.ours, {"D3D", "DSOUND"}, 16)
        addrs = [int(r["start"], 16) for r in cands]
        self.assertEqual(addrs, [0x1ECA10, 0x204F23])
        # already in kb, too small, no prologue, other section
        self.assertNotIn(0x1E6AE0, addrs)
        self.assertNotIn(0x1F1960, addrs)
        self.assertNotIn(0x2053D4, addrs)
        self.assertNotIn(0x222E48, addrs)

    def test_include_call_targets_takes_no_prologue_xdk(self):
        cands = crf.xdk_candidates(
            self.theirs, self.ours, {"D3D", "DSOUND"}, 16,
            require_prologue=False,
        )
        addrs = [int(r["start"], 16) for r in cands]
        self.assertIn(0x1ECA10, addrs)
        self.assertIn(0x2053D4, addrs)
        self.assertNotIn(0x7AE4D, addrs)  # .text cc_boundary
        self.assertNotIn(0x1E6AE0, addrs)

    def test_apply_appends_to_last_xdk_stubs(self):
        kb = {
            "md5": "c7869590a1c64ad034e49a5ee0c02465",
            "objects": [
                {"name": "<xdk_stubs>", "functions": [
                    {"addr": "0x1e6ae0", "decl": "int __stdcall D3DDevice_CreateTexture();"}
                ]},
                {"name": "actors.obj", "functions": []},
                {"name": "<xdk_stubs>", "functions": [
                    {"addr": "0x24d009", "decl": "void XID_fCloseDevice(void);"}
                ]},
            ],
        }
        cands = crf.xdk_candidates(self.theirs, self.ours, {"D3D", "DSOUND"}, 16)
        stats = crf.apply_import(kb, cands)
        self.assertEqual(stats["added"], 2)
        last = kb["objects"][-1]
        self.assertEqual(last["name"], "<xdk_stubs>")
        addrs = [fn["addr"] for fn in last["functions"]]
        self.assertEqual(addrs[0], "0x24d009")
        self.assertIn("0x1eca10", addrs)
        self.assertIn("0x204f23", addrs)
        self.assertTrue(all("FUN_" in fn["decl"] for fn in last["functions"][1:]))
        # first object untouched
        self.assertEqual(len(kb["objects"][0]["functions"]), 1)

    def test_apply_skips_existing_in_target_object(self):
        kb = {
            "objects": [{
                "name": "<xdk_stubs>",
                "functions": [{"addr": "0x1eca10", "decl": "void already(void);"}],
            }],
        }
        cands = crf.xdk_candidates(self.theirs, self.ours, {"D3D", "DSOUND"}, 16)
        stats = crf.apply_import(kb, cands)
        self.assertEqual(stats["added"], 1)
        self.assertEqual(stats["skipped"], 1)

    def test_main_apply_roundtrip(self):
        with tempfile.TemporaryDirectory() as td:
            td = Path(td)
            theirs = td / "functions.json"
            kb_path = td / "kb.json"
            theirs.write_text(json.dumps(self.theirs))
            kb = {
                "md5": "x",
                "objects": [{
                    "name": "<xdk_stubs>",
                    "functions": [
                        {"addr": "0x1e6ae0",
                         "decl": "int __stdcall D3DDevice_CreateTexture();"},
                    ],
                }],
            }
            kb_path.write_text(json.dumps(kb, indent=1) + "\n")
            rc = crf.main(["--theirs", str(theirs), "--kb", str(kb_path),
                           "--import-xdk", "--apply", "--json"])
            self.assertEqual(rc, 0)
            written = json.loads(kb_path.read_text())
            addrs = {int(fn["addr"], 16)
                     for fn in written["objects"][0]["functions"]}
            self.assertIn(0x1ECA10, addrs)
            self.assertIn(0x204F23, addrs)
            self.assertNotIn(0x222E48, addrs)


if __name__ == "__main__":
    unittest.main()
