#!/usr/bin/env python3
"""End-to-end and adversarial tests for structize.py.

test_structize.py tests the pure functions -- parsing, classification, split
planning. Those tests can all pass while the tool still corrupts a real file,
because they never invoke a compiler.

This file tests the part that actually matters: the claim that a converge run
either produces byte-identical machine code or produces nothing. It does that
by compiling real C with the project's real flags, and -- more importantly --
by deliberately breaking things and asserting the tool notices.

A gate that has never been shown to FAIL is not evidence of anything. Every
test here that ends in `_is_caught` or `_is_refused` exists to prove the gate
is not vacuous.

Run:  python3 -m tools.recovery.test_structize_e2e
"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.recovery.structize import (  # noqa: E402
    StructizeError,
    apply_rewrites,
    build_flags,
    census,
    converge,
    diverging_functions,
    compile_tu,
    gate,
    _parse_layout,
)

HAVE_CLANG = shutil.which("clang") is not None
HAVE_OBJDUMP = shutil.which("objdump") is not None
HAVE_FLAGS = (ROOT / "build" / "CMakeFiles" / "halo.dir" / "flags.make").is_file()
TOOLCHAIN = HAVE_CLANG and HAVE_OBJDUMP and HAVE_FLAGS
WHY_SKIP = "needs clang + objdump + a configured build/ (flags.make)"

# --------------------------------------------------------------------------
# Fixture translation unit.
#
# Self-contained on purpose: no types.h, no engine headers. The point is to
# exercise the compile/diff/converge machinery on code whose correct answer we
# know by construction, not to re-test actor_t.
#
# `#pragma pack(1)` mirrors types.h, because the one known source of
# divergence (alignment metadata changing -O3 scheduling) only exists under
# packing. Two same-width integer fields sit adjacent at 0x1c and 0x20 so that
# a deliberately wrong offset still compiles cleanly -- that is what makes the
# wrong-offset test meaningful rather than a compile-error test in disguise.
# --------------------------------------------------------------------------
FIXTURE_H = """\
#ifndef FIXTURE_H
#define FIXTURE_H
#pragma pack(push, 1)
typedef struct {
    short salt;            /* 0x00 */
    char  pad_02[0xE];     /* 0x02 */
    float vec[3];          /* 0x10 */
    unsigned int flags;    /* 0x1c */
    unsigned int owner;    /* 0x20 */
} fx_t;
#pragma pack(pop)
#endif
"""

FIXTURE_C = """\
#include "fixture.h"

unsigned int fx_flags(char *p)
{
    return *(unsigned int *)(p + 0x1c);
}

unsigned int fx_owner(char *p)
{
    return *(unsigned int *)(p + 0x20);
}

float fx_vy(char *p)
{
    return *(float *)(p + 0x14);
}

short fx_salt(char *p)
{
    return *(short *)(p + 0x0);
}

unsigned int fx_sum(char *p)
{
    return *(unsigned int *)(p + 0x1c) + *(unsigned int *)(p + 0x20);
}
"""

# Every site the census should find, and the field each must resolve to.
EXPECTED_FIELDS = {
    0x1C: "flags",
    0x20: "owner",
    0x14: "vec[1]",
    0x00: "salt",
}


def fixture_layout(directory):
    """Ask clang for the fixture's layout, so the test never hardcodes offsets.

    Mirrors struct_layout(): clang only emits a record layout for a type it is
    forced to complete, so a probe TU declares a variable of it. Reuses the
    tool's own parser, so a fixture/parser disagreement fails here rather than
    as a confusing downstream error.
    """
    directory = Path(directory)
    probe = directory / "probe.c"
    # `sizeof` is what forces clang to compute (and therefore dump) the layout.
    # A tentative definition alone is not enough under -fsyntax-only; types.h
    # gets this for free from its cs() size assertions.
    probe.write_text('#include "fixture.h"\n'
                     'char _probe_size[sizeof(fx_t)];\n', encoding="ascii")
    cmd = [shutil.which("clang")]
    cmd += build_flags()
    cmd += ["-I", str(directory)]
    cmd += ["-fsyntax-only", "-Xclang", "-fdump-record-layouts", str(probe)]
    proc = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ROOT))
    if not proc.stdout:
        raise AssertionError("clang emitted no record layout:\n%s" % proc.stderr[:2000])
    return _parse_layout(proc.stdout, "fx_t")


# Fixtures must live under ROOT (census records a repo-relative path), but not
# AT the root -- a failed run there leaves untracked junk in `git status`.
# build/ is already generated and ignored, so residue is harmless and one
# directory removes all of it.
FIXTURE_ROOT = ROOT / "build" / "structize-e2e"


def sweep_fixtures():
    """Remove residue from earlier runs, including the old root-level layout."""
    shutil.rmtree(FIXTURE_ROOT, ignore_errors=True)
    for stale in ROOT.glob("structize-e2e-*"):
        if stale.is_dir():
            shutil.rmtree(stale, ignore_errors=True)


class _FixtureCase(unittest.TestCase):
    """Base: a fixture TU under build/, cleaned up even if setUp fails."""

    def setUp(self):
        FIXTURE_ROOT.mkdir(parents=True, exist_ok=True)
        self.dir = Path(tempfile.mkdtemp(prefix="fx-", dir=str(FIXTURE_ROOT)))
        # addCleanup, not tearDown: unittest skips tearDown when setUp raises,
        # which is exactly when a fixture directory gets orphaned.
        self.addCleanup(shutil.rmtree, self.dir, ignore_errors=True)
        (self.dir / "fixture.h").write_text(FIXTURE_H, encoding="utf-8")
        self.source = self.dir / "fixture.c"
        self.source.write_text(FIXTURE_C, encoding="utf-8")
        self.original = self.source.read_text(encoding="utf-8")
        self.layout = fixture_layout(self.dir)

    def take_census(self):
        return census(self.source, "p", "fx_t", layout=self.layout)

    def assert_source_untouched(self):
        self.assertEqual(self.source.read_text(encoding="utf-8"), self.original,
                         "source was not restored to its original bytes")


@unittest.skipUnless(TOOLCHAIN, WHY_SKIP)
class FixtureSanityTests(_FixtureCase):
    """The fixture itself is correct -- otherwise later failures are noise."""

    def test_layout_matches_the_offsets_the_fixture_documents(self):
        by_name = {f["name"]: f["offset"] for f in self.layout["fields"].values()}
        self.assertEqual(by_name["salt"], 0x00)
        self.assertEqual(by_name["vec"], 0x10)
        self.assertEqual(by_name["flags"], 0x1C)
        self.assertEqual(by_name["owner"], 0x20)
        self.assertEqual(self.layout["size"], 0x24)

    def test_census_resolves_every_site(self):
        data = self.take_census()
        got = {}
        for site in data["sites"]:
            if site["verdict"] == "rewrite":
                got[site["offset"]] = site["field"]
        self.assertEqual(got, EXPECTED_FIELDS)

    def test_fixture_compiles_clean_under_project_flags(self):
        compile_tu(self.source, self.dir / "sanity.obj")


@unittest.skipUnless(TOOLCHAIN, WHY_SKIP)
class ConvergeHappyPathTests(_FixtureCase):

    def test_converge_rewrites_and_stays_byte_identical(self):
        report = converge(self.take_census(), source=self.source)
        self.assertTrue(report["ok"], report)
        self.assertFalse(report["vacuous"])
        # 6 sites: flags, owner, vec[1], salt, and two inside fx_sum.
        self.assertEqual(report["sites_rewritten"], 6)
        self.assertEqual(report["parked_functions"], [])
        self.assertEqual(report["rounds"][0]["functions_changed"], [])

    def test_converged_source_actually_uses_fields(self):
        converge(self.take_census(), source=self.source)
        text = self.source.read_text(encoding="utf-8")
        self.assertIn("((fx_t *)p)->flags", text)
        self.assertIn("((fx_t *)p)->vec[1]", text)
        self.assertNotIn("(p + 0x1c)", text)

    def test_base_declaration_is_never_retyped(self):
        # Retyping `char *p` to `fx_t *p` would rescale every remaining
        # `p + 0xNN` by sizeof(fx_t). The tool must not do it.
        converge(self.take_census(), source=self.source)
        text = self.source.read_text(encoding="utf-8")
        self.assertIn("fx_flags(char *p)", text)
        self.assertNotIn("fx_flags(fx_t *p)", text)

    def test_running_twice_finds_no_remaining_work(self):
        converge(self.take_census(), source=self.source)
        second = census(self.source, "p", "fx_t", layout=self.layout)
        self.assertEqual([s for s in second["sites"] if s["verdict"] == "rewrite"], [])


@unittest.skipUnless(TOOLCHAIN, WHY_SKIP)
class WrongRewriteIsCaughtTests(_FixtureCase):
    """The load-bearing tests: a wrong rewrite must never reach the file.

    This is the failure mode that matters. Everything else the tool does is
    recoverable; silently reading the wrong struct field is a bug that looks
    like correct code forever.
    """

    def _corrupt(self, data, from_offset, to_field, in_function):
        """Point one site at the wrong field -- same width, so it compiles."""
        hits = 0
        for site in data["sites"]:
            if (site["verdict"] == "rewrite" and site["offset"] == from_offset
                    and site.get("function") == in_function):
                site["field"] = to_field
                hits += 1
        self.assertEqual(hits, 1, "test setup: expected exactly one site to corrupt")
        return data

    def test_wrong_offset_is_caught_and_the_function_is_parked(self):
        # fx_flags should read `flags` (0x1c); make it read `owner` (0x20).
        # Same type, compiles clean, different memory -- only the compiled
        # output can tell the difference.
        data = self._corrupt(self.take_census(), 0x1C, "owner", "fx_flags")
        report = converge(data, source=self.source)

        # The run still succeeds, because round 2 drops the offending function.
        self.assertTrue(report["ok"], report)
        # ... but the corrupted function was excluded, not shipped.
        self.assertIn("fx_flags", report["parked_functions"])
        # ... and the file that survives does NOT contain the wrong field.
        text = self.source.read_text(encoding="utf-8")
        self.assertIn("*(unsigned int *)(p + 0x1c)", text)
        self.assertNotIn("((fx_t *)p)->owner;\n}\n\nunsigned int fx_owner", text)

    def test_the_other_functions_still_convert(self):
        # A single bad site must cost one function, not the whole file.
        data = self._corrupt(self.take_census(), 0x1C, "owner", "fx_flags")
        report = converge(data, source=self.source)
        self.assertTrue(report["ok"])
        self.assertNotIn("fx_owner", report["parked_functions"])
        self.assertIn("((fx_t *)p)->vec[1]", self.source.read_text(encoding="utf-8"))

    def test_wrong_field_in_every_function_converges_to_a_no_op(self):
        # Corrupt so much that nothing can be kept. The tool must end up
        # rewriting nothing rather than shipping anything wrong.
        data = self.take_census()
        for site in data["sites"]:
            if site["verdict"] == "rewrite":
                site["field"] = "owner" if site["offset"] != 0x20 else "flags"
        report = converge(data, source=self.source)
        if report["ok"]:
            # Converged by excluding everything -- that is a vacuous run.
            self.assertTrue(report["vacuous"], report)
            self.assert_source_untouched()
        else:
            # Or it failed to converge in two rounds, which also restores.
            self.assert_source_untouched()


@unittest.skipUnless(TOOLCHAIN, WHY_SKIP)
class CompileFailureTests(_FixtureCase):
    """A build error must surface as an error, never as a silent pass.

    This regression-tests a real bug from development: stderr was discarded
    and the diff ran against an object that had never been written, which
    reported a confident, meaningless result.
    """

    def test_nonexistent_field_raises_and_restores(self):
        data = self.take_census()
        for site in data["sites"]:
            if site["verdict"] == "rewrite":
                site["field"] = "no_such_field"
        with self.assertRaises(StructizeError) as caught:
            converge(data, source=self.source)
        self.assertIn("compile failed", str(caught.exception))
        self.assert_source_untouched()

    def test_compile_error_names_the_file(self):
        bad = self.dir / "broken.c"
        bad.write_text("int f(void) { return nope; }\n", encoding="utf-8")
        with self.assertRaises(StructizeError) as caught:
            compile_tu(bad, self.dir / "broken.obj")
        self.assertIn("broken.c", str(caught.exception))

    def test_objdump_on_a_missing_object_raises(self):
        # The old bug compared against a file that was never produced.
        with self.assertRaises(StructizeError):
            diverging_functions(self.dir / "nope-a.obj", self.dir / "nope-b.obj")


@unittest.skipUnless(TOOLCHAIN, WHY_SKIP)
class VacuityTests(_FixtureCase):
    """"Byte-identical" is trivially true when nothing was rewritten."""

    def test_a_census_with_no_eligible_sites_is_flagged_vacuous(self):
        data = self.take_census()
        for site in data["sites"]:
            site["verdict"] = "refuse"
        report = converge(data, source=self.source)
        self.assertTrue(report["ok"])
        self.assertTrue(report["vacuous"], "a no-op run must not look like progress")
        self.assertEqual(report["sites_rewritten"], 0)
        self.assert_source_untouched()

    def test_real_work_is_not_flagged_vacuous(self):
        report = converge(self.take_census(), source=self.source)
        self.assertFalse(report["vacuous"])

    def test_cli_exit_code_distinguishes_no_op_from_work(self):
        import json

        from tools.recovery.structize import main

        data = self.take_census()
        for site in data["sites"]:
            site["verdict"] = "refuse"
        census_path = self.dir / "empty.json"
        census_path.write_text(json.dumps(data), encoding="utf-8")

        stdout = os.dup(1)
        sink = os.open(os.devnull, os.O_WRONLY)
        try:
            os.dup2(sink, 1)
            code = main(["converge", "--census", str(census_path)])
            sys.stdout.flush()   # or the report leaks after the fd is restored
        finally:
            os.dup2(stdout, 1)
            os.close(sink)
            os.close(stdout)
        self.assertEqual(code, 2, "a vacuous converge must not exit 0")


@unittest.skipUnless(TOOLCHAIN, WHY_SKIP)
class RestoreOnFailureTests(_FixtureCase):
    """Whatever goes wrong, the tree must look untouched afterwards."""

    def test_exception_midway_restores_the_file(self):
        data = self.take_census()
        real = compile_tu
        calls = {"n": 0}

        def explode(source, out_obj, extra=()):
            calls["n"] += 1
            if calls["n"] == 2:      # after the file has been rewritten
                raise StructizeError("simulated toolchain failure")
            return real(source, out_obj, extra)

        with mock.patch("tools.recovery.structize.compile_tu", explode):
            with self.assertRaises(StructizeError):
                converge(data, source=self.source)
        self.assert_source_untouched()

    def test_dry_run_rewrite_never_writes(self):
        apply_rewrites(self.take_census(), source=self.source, apply=False)
        self.assert_source_untouched()


class GateStaleObjectTests(unittest.TestCase):
    """A failed build leaves the previous .obj on disk.

    Gating against it compares the last good build to its own baseline and
    passes -- a green gate for source that does not compile. No toolchain
    needed: this tests the return-code check itself.
    """

    def test_build_failure_refuses_to_gate(self):
        done = subprocess.CompletedProcess(args=[], returncode=1,
                                           stdout="", stderr="error: boom")
        with mock.patch("tools.recovery.structize.subprocess.run", return_value=done):
            with self.assertRaises(StructizeError) as caught:
                gate("halo/ai/actors.c", "unused-baseline.json", build=True)
        message = str(caught.exception)
        self.assertIn("build failed", message)
        self.assertIn("stale", message)
        self.assertIn("boom", message)

    def test_no_build_mode_skips_the_check(self):
        # --no-build gates an object the caller built; it must not raise the
        # build-failure error, only the missing-object one.
        with self.assertRaises(StructizeError) as caught:
            gate("nonexistent/tu.c", "unused-baseline.json", build=False)
        self.assertIn("object not built", str(caught.exception))


if __name__ == "__main__":
    sweep_fixtures()
    unittest.main(verbosity=2)
