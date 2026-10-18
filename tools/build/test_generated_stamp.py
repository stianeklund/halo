#!/usr/bin/env python3
"""The kb stamp must never be written without every generated artifact.

build.py's _ensure_generated_files and CMake trust build/generated/.kb.sha256
to mean decl.h, halo.xbe.def and thunks.c all match kb.json.  A header-only
regen that also wrote the stamp left a stale .def and broke the link.
"""

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KNOWLEDGE_PY = ROOT / "tools" / "analysis" / "knowledge.py"


def _run(*args):
    env = os.environ.copy()
    env["LOG_LEVEL"] = "WARNING"
    return subprocess.run([sys.executable, str(KNOWLEDGE_PY), *args],
                          cwd=ROOT, env=env, capture_output=True, text=True)


class StampRequiresAllArtifacts(unittest.TestCase):
    def test_partial_regen_cannot_write_stamp(self):
        partial_sets = (
            ("--gen-header",),
            ("--gen-header", "--gen-def"),
            ("--gen-header", "--gen-thunks"),
            ("--gen-def", "--gen-thunks"),
        )
        names = {"--gen-header": "decl.h", "--gen-def": "halo.xbe.def",
                 "--gen-thunks": "thunks.c"}
        for flags in partial_sets:
            with self.subTest(flags=flags), tempfile.TemporaryDirectory() as tmp:
                args = []
                for flag in flags:
                    args += [flag, os.path.join(tmp, names[flag])]
                stamp = os.path.join(tmp, ".kb.sha256")
                r = _run(*args, "--gen-stamp", stamp)
                self.assertEqual(r.returncode, 2, r.stderr)
                self.assertIn("--gen-stamp requires", r.stderr)
                self.assertFalse(os.path.exists(stamp))
                for flag in flags:
                    self.assertFalse(os.path.exists(os.path.join(tmp, names[flag])))

    def test_full_regen_writes_stamp(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = {n: os.path.join(tmp, n)
                   for n in ("decl.h", "halo.xbe.def", "thunks.c", ".kb.sha256")}
            r = _run("--gen-header", out["decl.h"], "--gen-def", out["halo.xbe.def"],
                     "--gen-thunks", out["thunks.c"], "--gen-stamp", out[".kb.sha256"])
            self.assertEqual(r.returncode, 0, r.stderr)
            for path in out.values():
                self.assertTrue(os.path.isfile(path), path)


if __name__ == "__main__":
    unittest.main()
