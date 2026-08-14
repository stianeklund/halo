#!/usr/bin/env python3
"""Unit tests for vc71_regression check-mode evidence gates."""

import contextlib
import importlib.util
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


MODULE_PATH = Path(__file__).with_name("vc71_regression.py")
SPEC = importlib.util.spec_from_file_location("vc71_regression", MODULE_PATH)
vc71 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(vc71)


SOURCE = "tools/verify/test_vc71_regression.py"


class TestStrictCheck(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.baseline_path = Path(self.temp_dir.name) / "scores.json"
        self.args = SimpleNamespace(
            source=None, threshold=100.0, quiet=True, strict=True,
        )

    def tearDown(self):
        self.temp_dir.cleanup()

    def write_baseline(self, scores):
        self.baseline_path.write_text(json.dumps({"scores": scores}))

    def run_check(self, scores, runner=None, source=None):
        self.write_baseline(scores)
        args = SimpleNamespace(
            source=source, threshold=self.args.threshold,
            quiet=self.args.quiet, strict=self.args.strict,
        )
        with patch.object(vc71, "BASELINE_PATH", self.baseline_path):
            with patch.object(vc71, "run_vc71_verify", runner or self.runner()):
                with patch.object(vc71, "_func_span", return_value=10):
                    return vc71.cmd_check(args)

    @staticmethod
    def runner(results=None, status="ok", drops=None):
        results = results or {}
        drops = drops or []

        def run(_source, drops_out=None, meta_out=None):
            if drops_out is not None:
                drops_out.extend(drops)
            if meta_out is not None:
                meta_out["status"] = status
            return results

        return run

    def test_empty_baseline_fails_strict(self):
        self.assertNotEqual(self.run_check({}), 0)

    def test_source_filter_without_matches_fails_strict(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        self.assertNotEqual(self.run_check(scores, source=["other.c"]), 0)

    def test_missing_source_fails_strict(self):
        scores = {"fn": {"score": 90, "source": "missing.c"}}
        self.assertNotEqual(self.run_check(scores), 0)

    def test_runner_failures_fail_strict(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        for status in ("subprocess_failed", "compile_failed", "parse_failed"):
            with self.subTest(status=status):
                self.assertNotEqual(
                    self.run_check(scores, self.runner(status=status)), 0
                )

    def test_runner_exception_fails_strict(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}

        def fail(*_args, **_kwargs):
            raise OSError("compiler unavailable")

        self.assertNotEqual(self.run_check(scores, fail), 0)

    def test_missing_parsed_function_and_zero_checked_fail(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        self.assertNotEqual(self.run_check(scores, self.runner()), 0)

    def test_instruction_count_heuristics_are_gone(self):
        """A reference is CUT from the bounds table, so it cannot be truncated
        or bloated relative to the span -- the span is its length.  The old
        n_r*15<span / n_r>span gate rejected both shapes below; a reference that
        genuinely cannot be derived is a DROP now (see the test after this one),
        not a suspicious score."""
        scores = {"fn": {"score": 90, "source": SOURCE}}
        for n_r, span in ((0, 10), (1, 100), (200, 10)):
            with self.subTest(n_r=n_r, span=span):
                result = {"fn": {"score": 90, "n_r": n_r}}
                with patch.object(vc71, "_func_span", return_value=span):
                    self.write_baseline(scores)
                    with patch.object(vc71, "BASELINE_PATH", self.baseline_path):
                        with patch.object(vc71, "run_vc71_verify",
                                          self.runner(result)):
                            self.assertEqual(vc71.cmd_check(self.args), 0)

    def test_missing_reference_drop_fails_strict(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        drop = {"function": "fn", "reason": "missing", "span_bytes": 10}
        result = {"fn": {"score": 90, "n_r": 10}}
        self.assertNotEqual(
            self.run_check(scores, self.runner(result, drops=[drop])), 0
        )

    def test_improvement_passes_and_any_regression_fails(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        improved = {"fn": {"score": 91, "n_r": 10}}
        regressed = {"fn": {"score": 89.9, "n_r": 10}}
        self.assertEqual(self.run_check(scores, self.runner(improved)), 0)
        self.assertNotEqual(self.run_check(scores, self.runner(regressed)), 0)

    def test_strict_flag_is_explicit_parser_option(self):
        args = vc71.build_parser().parse_args(["check", "--strict"])
        self.assertTrue(args.strict)


class TestLegacyCheckSkipsEvidenceGaps(unittest.TestCase):
    """Default (non-strict) mode tolerates exactly two evidence gaps: an empty
    baseline, and a TU that measured NOTHING (compile failure -- a checkout with
    no RXDK/wine VC71 toolchain fails every compile, and a gate that blocks every
    commit there gets disabled).  Every other kind of silence is fatal; see
    TestSilenceIsFailure."""

    def test_empty_baseline_and_missing_result_still_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            baseline = Path(temp) / "scores.json"
            baseline.write_text(json.dumps({"scores": {}}))
            args = SimpleNamespace(source=None, threshold=2.0, quiet=True, strict=False)
            with patch.object(vc71, "BASELINE_PATH", baseline):
                self.assertEqual(vc71.cmd_check(args), 0)

            baseline.write_text(json.dumps({
                "scores": {"fn": {"score": 90, "source": SOURCE}},
            }))
            runner = TestStrictCheck.runner()
            with patch.object(vc71, "BASELINE_PATH", baseline):
                with patch.object(vc71, "run_vc71_verify", runner):
                    with patch.object(vc71, "_func_span", return_value=10):
                        self.assertEqual(vc71.cmd_check(args), 0)


class TestFuncSpanDelegatesWhenImported(unittest.TestCase):
    """_func_span must reach vc71_verify however this module was imported.

    Regression test for the import-path bug: `from vc71_verify import _func_span`
    resolves only when sys.path[0] is tools/verify (the __main__/CLI path).  When
    tools/recovery/source_recovery.py imports this as `tools.verify.vc71_regression`
    with only the repo root on sys.path, that raised ImportError and _func_span
    silently fell back to the kb.json listing gap -- which overshoots wherever the
    listing has a hole, so a correct short reference reads as truncated and the
    function is reported "invalid delinked reference".  Observed on the hud_weapon
    recovery run: FUN_000d8b70/FUN_000d8b80 scored a 16-byte kb gap against a true
    span of 1, failing a check the CLI passed.

    The delegated answer now comes from the committed bounds table
    (tools/verify/function_bounds.json), which is also where the reference itself
    is cut from -- so a gate that disagrees with vc71_verify about a function's
    size is disagreeing about which bytes were scored.

    Delegation is asserted via sys.modules rather than a span value, so the test
    does not depend on kb.json contents: if the delegating import had failed,
    vc71_verify would never be loaded at all.  The values are then pinned against
    the table directly, which catches the subtler failure where both sides
    delegate but the table has stopped being the authority.
    """

    def test_import_as_package_module_still_delegates(self):
        repo_root = Path(__file__).resolve().parent.parent.parent
        probe = (
            "import sys\n"
            "sys.path.insert(0, %r)\n"
            "from tools.verify import vc71_regression as vr\n"
            "assert 'vc71_verify' not in sys.modules, 'precondition: not yet loaded'\n"
            "vr._func_span('FUN_00000000')\n"
            "assert 'vc71_verify' in sys.modules, "
            "'_func_span fell back to the kb gap instead of delegating'\n"
            "import vc71_verify\n"
            "for fn in ('FUN_000d8b70', 'FUN_000d8b80'):\n"
            "    assert vr._func_span(fn) == vc71_verify._func_span(fn), fn\n"
            "print('ok')\n"
        ) % str(repo_root)
        proc = subprocess.run(
            [sys.executable, "-c", probe],
            cwd=str(repo_root), capture_output=True, text=True,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_span_is_the_committed_bound(self):
        """Both sides must report end-start from function_bounds.json."""
        repo_root = Path(__file__).resolve().parent.parent.parent
        sys.path.insert(0, str(repo_root / "tools" / "verify"))
        sys.path.insert(0, str(repo_root / "tools" / "audit"))
        import function_bounds as fb
        import vc71_verify as vv
        table = fb.load_table()
        # A listing-gap case (kb gap 800 for a 102-byte function) and a thunk,
        # the two shapes the old kb-gap rule got wrong in opposite directions.
        for addr in (0x15C2D0, 0x18E300, 0x1A0680):
            entry = table.get(hex(addr))
            self.assertIsNotNone(entry, f"0x{addr:x} missing from the table")
            want = int(entry["end"], 16) - addr
            self.assertEqual(vv._func_span(f"FUN_{addr:08x}"), want)
            self.assertEqual(vc71._func_span(f"FUN_{addr:08x}"), want)


class TestScoreEntrySchema(unittest.TestCase):
    """Entry construction is shared by two writers; pin what it keeps."""

    INFO = {
        "score": 91.2, "n_c": 30, "n_r": 28,
        "addr": "0x100c10", "end": "0x100d54", "kind": "auto",
        "ref_sha": "ab12cd34ef567890", "opnd_percent": 73.3,
        "raw_mnemonic_pct": 89.7, "abi_modeled_mnemonic_pct": 91.2,
        "abi_model": "regparam_stripped", "abi_model_items": 1,
        "ref": "synth",
    }

    def test_provenance_is_written(self):
        e = vc71.make_score_entry(91.2, "src/halo/main/main.c", self.INFO)
        for field in vc71.PROVENANCE_FIELDS:
            self.assertIn(field, e, field)
        self.assertEqual(e["addr"], "0x100c10")
        self.assertEqual(e["n_r"], 28)
        self.assertEqual(e["opnd_percent"], 73.3)
        self.assertEqual(e["raw_mnemonic_pct"], 89.7)
        self.assertEqual(e["abi_modeled_mnemonic_pct"], 91.2)
        self.assertEqual(e["abi_model"], "regparam_stripped")
        # n_c describes the CANDIDATE, not the reference: it must not leak into
        # the committed floor's provenance block.
        self.assertNotIn("n_c", e)

    def test_absent_optional_fields_are_not_written_as_null(self):
        e = vc71.make_score_entry(50.0, "src/x.c", {"score": 50.0})
        self.assertEqual(set(e), {"score", "source"})

    def test_unknown_fields_round_trip(self):
        """A key some other consumer stamped in must survive a score refresh."""
        prev = {"score": 10.0, "source": "src/old.c", "reviewed_by": "someone"}
        e = vc71.make_score_entry(91.2, "src/x.c", self.INFO, previous=prev)
        self.assertEqual(e["reviewed_by"], "someone")
        self.assertEqual(e["score"], 91.2)
        self.assertEqual(e["source"], "src/x.c")

    def test_optional_fields_backfill_without_moving_floor(self):
        baseline = {"fn": {"score": 95.0, "source": "src/x.c"}}
        current = {"fn": vc71.make_score_entry(91.2, "src/x.c", self.INFO)}
        self.assertEqual(vc71.backfill_optional_fields(baseline, current), 1)
        self.assertEqual(baseline["fn"]["score"], 95.0)
        self.assertEqual(baseline["fn"]["raw_mnemonic_pct"], 89.7)
        self.assertEqual(baseline["fn"]["abi_model"], "regparam_stripped")


class TestRefmetaParsing(unittest.TestCase):
    """REFMETA lines must reach the entry; the shape is pinned in
    test_ref_selection, this pins the consumption side."""

    OUT = (
        "  SYNTHREF main_loop\n"
        "  REFMETA main_loop addr=0x00102e40 end=0x001034aa kind=auto "
        "n_r=432 sha=7988df1cab63ec0e\n"
        "  PASS main_loop: 97.8% match (430/432 insns) | opnd 90.3% "
        "(operand-normalized) | raw 96.4% | abi-modeled 97.8% "
        "[regparam_stripped:2]\n"
    )

    def _run(self, stdout):
        completed = SimpleNamespace(returncode=0, stdout=stdout, stderr="")
        with patch.object(vc71.subprocess, "run", return_value=completed):
            with patch.dict("os.environ", {"VC71_NO_MEASURE_MEMO": "1"}):
                return vc71.run_vc71_verify(Path("src/halo/main/main.c"))

    def test_provenance_attached_to_score(self):
        out = self._run(self.OUT)
        entry = out["main_loop"]
        self.assertEqual(entry["addr"], "0x102e40")
        self.assertEqual(entry["end"], "0x1034aa")
        self.assertEqual(entry["kind"], "auto")
        self.assertEqual(entry["ref_sha"], "7988df1cab63ec0e")
        self.assertEqual(entry["n_r"], 432)
        self.assertEqual(entry["score"], 97.8)
        self.assertEqual(entry["raw_mnemonic_pct"], 96.4)
        self.assertEqual(entry["abi_modeled_mnemonic_pct"], 97.8)
        self.assertEqual(entry["abi_model"], "regparam_stripped")
        self.assertEqual(entry["abi_model_items"], 2)

    def test_score_without_refmeta_is_still_kept(self):
        """A scorer that stops printing REFMETA must not blank the baseline."""
        out = self._run(
            "  PASS main_loop: 97.8% match (430/432 insns)\n")
        self.assertEqual(out["main_loop"]["score"], 97.8)
        self.assertEqual(out["main_loop"]["raw_mnemonic_pct"], 97.8)
        self.assertEqual(out["main_loop"]["abi_model"], "raw")
        self.assertIsNone(out["main_loop"].get("ref_sha"))


class TestRebaselineMerge(unittest.TestCase):
    """Raise-only and replace must never be reachable by accident."""

    INFO = {"main_loop": {"score": 80.0, "addr": "0x102e40",
                          "ref_sha": "7988df1cab63ec0e", "n_r": 432}}

    def test_default_merge_refuses_to_lower(self):
        baseline = {"main_loop": {"score": 95.0, "source": "src/x.c"}}
        n, _log = vc71._apply_floor(baseline, "src/x.c", [("main_loop", 80.0)],
                                    force=False, info_map=self.INFO)
        self.assertEqual(n, 0)
        self.assertEqual(baseline["main_loop"]["score"], 95.0)

    def test_rebaseline_replaces_and_stamps_provenance(self):
        baseline = {"main_loop": {"score": 95.0, "source": "src/x.c"}}
        n, _log = vc71._apply_floor(baseline, "src/x.c", [("main_loop", 80.0)],
                                    force=False, info_map=self.INFO,
                                    rebaseline=True)
        self.assertEqual(n, 1)
        self.assertEqual(baseline["main_loop"]["score"], 80.0)
        self.assertEqual(baseline["main_loop"]["ref_sha"], "7988df1cab63ec0e")

    def test_rebaseline_stamps_provenance_on_an_unchanged_score(self):
        """The whole point: a stable function still gains its provenance."""
        baseline = {"main_loop": {"score": 80.0, "source": "src/x.c"}}
        vc71._apply_floor(baseline, "src/x.c", [("main_loop", 80.0)],
                          force=False, info_map=self.INFO, rebaseline=True)
        self.assertEqual(baseline["main_loop"]["addr"], "0x102e40")


class TestBaselineDocument(unittest.TestCase):
    def test_version_2_and_sibling_keys_preserved(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "scores.json"
            path.write_text(json.dumps({
                "version": 1, "note": "keep me",
                "scores": {"fn": {"score": 1.0, "source": "src/x.c"}},
            }))
            with patch.object(vc71, "BASELINE_PATH", path):
                scores = vc71.load_baseline()
                scores["fn2"] = vc71.make_score_entry(50.0, "src/y.c")
                vc71.save_baseline(scores)
                doc = json.loads(path.read_text())
            self.assertEqual(doc["version"], 2)
            self.assertEqual(doc["note"], "keep me")
            self.assertEqual(sorted(doc["scores"]), ["fn", "fn2"])


class TestCurrentSnapshot(unittest.TestCase):
    """The honest snapshot is what the dashboard renders; a scoped pass must
    add to it, never replace it."""

    def test_load_current_roundtrip(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "current.json"
            with patch.object(vc71, "CURRENT_PATH", path):
                self.assertEqual(vc71.load_current(), {})       # absent
                vc71.save_current({"fn": vc71.make_score_entry(80.0, "src/x.c")})
                self.assertEqual(vc71.load_current()["fn"]["score"], 80.0)
                path.write_text("{ not json")                   # corrupt
                self.assertEqual(vc71.load_current(), {})

    def test_scoped_pass_merges_instead_of_truncating(self):
        """A one-TU refresh (the dashboard button) must not delete the other
        5,800 functions from the snapshot the report generator reads."""
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "current.json"
            with patch.object(vc71, "CURRENT_PATH", path):
                vc71.save_current({
                    "other_tu_fn": vc71.make_score_entry(70.0, "src/other.c"),
                    "shared_fn": vc71.make_score_entry(60.0, "src/x.c"),
                })
                merged = vc71._merged_current(
                    {"shared_fn": vc71.make_score_entry(85.0, "src/x.c")})
                vc71.save_current(merged)
                after = vc71.load_current()
            self.assertEqual(sorted(after), ["other_tu_fn", "shared_fn"])
            self.assertEqual(after["other_tu_fn"]["score"], 70.0)  # untouched
            self.assertEqual(after["shared_fn"]["score"], 85.0)    # refreshed


class TestResultKeyJoin(unittest.TestCase):
    """vc71_verify records a function under any of several keys.  Every
    consumer must agree on what counts as 'already scored', or a per-function
    retry fires for a function that scored perfectly well."""

    def test_plain_name(self):
        self.assertEqual(vc71._result_key({"main_loop": {}}, "main_loop"),
                         "main_loop")

    def test_address_alias(self):
        results = {"FUN_00102e40": {}}
        self.assertEqual(vc71._result_key(results, "main_loop", "0x102e40"),
                         "FUN_00102e40")
        self.assertEqual(vc71._result_key(results, "main_loop", 0x102e40),
                         "FUN_00102e40")

    def test_namespace_suffix(self):
        self.assertEqual(vc71._result_key({"ns::main_loop": {}}, "main_loop"),
                         "ns::main_loop")

    def test_miss_returns_none(self):
        self.assertIsNone(vc71._result_key({"other": {}}, "main_loop", "0x1"))
        self.assertIsNone(vc71._result_key({}, "main_loop", "not-hex"))


class TestPerFunctionFallback(unittest.TestCase):
    """A function the whole-TU run drops is retried alone -- and if the retry
    scores it, it must leave the re-delink queue."""

    SRC = vc71.REPO_ROOT / "src" / "halo" / "fake" / "fallback.c"

    def _run(self, *, enabled):
        calls = []

        def fake_verify(src, drops_out=None, meta_out=None, function=None,
                        **kw):
            calls.append(function)
            if function is None:
                if drops_out is not None:
                    drops_out.append({"function": "dropped_fn",
                                      "reason": "no reference",
                                      "span_bytes": 40})
                return {"scored_fn": {"score": 90.0}}
            return {function: {"score": 77.0}}

        with patch.object(vc71, "run_vc71_verify", fake_verify), \
             patch.object(vc71, "_expected_ported_functions",
                          lambda _rel: [{"name": "scored_fn", "addr": "0x1"},
                                        {"name": "dropped_fn", "addr": "0x2"}]):
            out = vc71._measure_source(self.SRC, enabled)
        return out, calls

    def test_disabled_leaves_the_drop_flagged(self):
        (_rel, honest, flagged, _scored, _log), calls = self._run(enabled=False)
        self.assertEqual(calls, [None])                    # no retry
        self.assertEqual(sorted(honest), ["scored_fn"])
        self.assertEqual([f["function"] for f in flagged], ["dropped_fn"])

    def test_enabled_recovers_and_clears_the_queue_entry(self):
        (_rel, honest, flagged, scored, _log), calls = self._run(enabled=True)
        self.assertEqual(calls, [None, "dropped_fn"])       # retried once
        self.assertEqual(sorted(honest), ["dropped_fn", "scored_fn"])
        self.assertEqual(honest["dropped_fn"]["score"], 77.0)
        self.assertEqual(flagged, [])                       # no longer pending
        self.assertIn(("dropped_fn", 77.0), scored)         # reaches the floor

    def test_already_scored_function_is_not_retried(self):
        """The alias join is what prevents a pointless extra compile."""
        def fake_verify(src, drops_out=None, meta_out=None, function=None, **kw):
            self.assertIsNone(function, "retried an already-scored function")
            return {"FUN_00000001": {"score": 90.0}}

        with patch.object(vc71, "run_vc71_verify", fake_verify), \
             patch.object(vc71, "_expected_ported_functions",
                          lambda _rel: [{"name": "scored_fn", "addr": "0x1"}]):
            vc71._measure_source(self.SRC, True)


class TestDiscoveryBreadth(unittest.TestCase):
    """Discovery breadth and floor semantics are separate knobs.  They used to
    be the same flag, so the only way to SEE a kb-only TU was the mode that
    REPLACES (and can lower) every floor it touches."""

    def _discovery_arg_for(self, args):
        seen = {}

        def fake_discover(include_kb_only):
            seen["kb"] = include_kb_only
            return []          # empty -> cmd_populate returns 1 immediately

        with patch.object(vc71, "_discover_scoreable_tus", fake_discover), \
             patch.object(vc71, "regen_decl_header", lambda: None), \
             contextlib.redirect_stdout(io.StringIO()):
            vc71.cmd_populate(args)
        return seen["kb"]

    def test_routine_populate_includes_kb_only_tus(self):
        self.assertTrue(self._discovery_arg_for(
            SimpleNamespace(rebaseline=False, include_kb_only=True,
                            source=None, skip_decl_regen=False)))

    def test_rebaseline_does_not_by_itself_widen_discovery(self):
        self.assertFalse(self._discovery_arg_for(
            SimpleNamespace(rebaseline=True, include_kb_only=False,
                            source=None, skip_decl_regen=False)))


class CheckHarness(unittest.TestCase):
    """Run cmd_check against an in-memory baseline and a scripted scorer."""

    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.baseline_path = Path(self.temp_dir.name) / "scores.json"

    def tearDown(self):
        self.temp_dir.cleanup()

    @staticmethod
    def scorer(results=None, status="ok", drops=None):
        results = results or {}
        drops = drops or []

        def run(_source, drops_out=None, meta_out=None):
            if drops_out is not None:
                drops_out.extend(drops)
            if meta_out is not None:
                meta_out["status"] = status
                meta_out["stderr_tail"] = []
            return results

        return run

    def check(self, scores, results=None, strict=False, threshold=2.0,
              status="ok", drops=None, runner=None):
        """Returns (rc, stdout)."""
        self.baseline_path.write_text(json.dumps({"version": 2, "scores": scores}))
        args = SimpleNamespace(source=None, threshold=threshold, quiet=False,
                               strict=strict)
        run = runner or self.scorer(results, status=status, drops=drops)
        buf = io.StringIO()
        with patch.object(vc71, "BASELINE_PATH", self.baseline_path):
            with patch.object(vc71, "run_vc71_verify", run):
                with contextlib.redirect_stdout(buf):
                    rc = vc71.cmd_check(args)
        return rc, buf.getvalue()


class TestRefShaPin(CheckHarness):
    """A score is only comparable to a floor measured against the SAME bytes."""

    BASE = {"score": 90.0, "source": SOURCE, "addr": "0x100c10",
            "end": "0x100d54", "ref_sha": "aaaa1111bbbb2222"}

    def test_matching_ref_sha_compares_normally(self):
        rc, out = self.check({"fn": dict(self.BASE)},
                             {"fn": {"score": 90.0, "n_r": 10,
                                     "ref_sha": "aaaa1111bbbb2222"}})
        self.assertEqual(rc, 0, out)
        self.assertIn("OK — no regressions", out)

    def test_matching_ref_sha_still_catches_a_regression(self):
        rc, out = self.check({"fn": dict(self.BASE)},
                             {"fn": {"score": 80.0, "n_r": 10,
                                     "ref_sha": "aaaa1111bbbb2222"}})
        self.assertEqual(rc, 1)
        self.assertIn("REGRESSIONS", out)

    def test_mismatched_ref_sha_fails_as_measurement_changed(self):
        """The reference moved: the two numbers describe different bytes, so
        they are NOT compared -- in either direction."""
        rc, out = self.check({"fn": dict(self.BASE)},
                             {"fn": {"score": 99.0, "n_r": 10,
                                     "ref_sha": "cccc3333dddd4444",
                                     "addr": "0x100c10", "end": "0x100d90"}})
        self.assertEqual(rc, 1)
        self.assertIn("MEASUREMENT CHANGED", out)
        self.assertIn("aaaa1111bbbb2222", out)
        self.assertIn("cccc3333dddd4444", out)
        self.assertIn("populate --rebaseline", out)
        # An improvement across a changed reference must not be reported as one.
        self.assertNotIn("Improvements", out)

    def test_mismatch_fails_even_when_the_score_is_identical(self):
        rc, out = self.check({"fn": dict(self.BASE)},
                             {"fn": {"score": 90.0, "n_r": 10,
                                     "ref_sha": "cccc3333dddd4444"}})
        self.assertEqual(rc, 1)
        self.assertIn("MEASUREMENT CHANGED", out)

    def test_baseline_without_ref_sha_is_grandfathered(self):
        """The 47 pre-provenance entries compare as before, with one warning
        per function; a real drop under them still fails."""
        legacy = {"score": 90.0, "source": SOURCE}
        rc, out = self.check({"fn": dict(legacy)},
                             {"fn": {"score": 90.0, "n_r": 10,
                                     "ref_sha": "cccc3333dddd4444"}})
        self.assertEqual(rc, 0, out)
        self.assertIn("without reference provenance", out)
        self.assertIn("fn", out)

        rc, out = self.check({"fn": dict(legacy)},
                             {"fn": {"score": 70.0, "n_r": 10}})
        self.assertEqual(rc, 1)
        self.assertIn("REGRESSIONS", out)

    def test_run_without_refmeta_warns_but_still_compares(self):
        """A scorer that stops printing REFMETA loses the pin, not the score."""
        rc, out = self.check({"fn": dict(self.BASE)},
                             {"fn": {"score": 90.0, "n_r": 10}})
        self.assertEqual(rc, 0, out)
        self.assertIn("NO PROVENANCE IN THIS RUN", out)

        rc, out = self.check({"fn": dict(self.BASE)},
                             {"fn": {"score": 50.0, "n_r": 10}})
        self.assertEqual(rc, 1)
        self.assertIn("REGRESSIONS", out)

    def test_run_without_refmeta_fails_strict(self):
        rc, _out = self.check({"fn": dict(self.BASE)},
                              {"fn": {"score": 90.0, "n_r": 10}}, strict=True)
        self.assertEqual(rc, 1)


class TestSilenceIsFailure(CheckHarness):
    """A baselined function this run did not measure is a failure, not a skip."""

    def test_vanished_function_fails_in_default_mode(self):
        """The stale-object class: FUN_000f56b0 held exactly 81.8% across 13
        attempts with no definition in the tree -- a deleted function kept
        scoring from a stale build/vc71/<tu>.obj, and `check` reported a pass
        because a missing score line was a bare `continue`."""
        scores = {"gone": {"score": 81.8, "source": SOURCE,
                           "ref_sha": "aaaa1111bbbb2222"},
                  "kept": {"score": 90.0, "source": SOURCE,
                           "ref_sha": "bbbb2222cccc3333"}}
        rc, out = self.check(scores, {"kept": {"score": 90.0, "n_r": 10,
                                               "ref_sha": "bbbb2222cccc3333"}})
        self.assertEqual(rc, 1, out)
        self.assertIn("UNMEASURED BASELINED FUNCTIONS", out)
        self.assertIn("gone", out)
        self.assertNotIn("81.8", out.split("UNMEASURED")[0])

    def test_dropped_function_fails_in_default_mode(self):
        """A DROP is silence with a known cause (no bounds entry)."""
        scores = {"fn": {"score": 90.0, "source": SOURCE},
                  "other": {"score": 90.0, "source": SOURCE}}
        drops = [{"function": "fn", "reason": "no bounds entry",
                  "span_bytes": 10}]
        rc, out = self.check(scores,
                             {"other": {"score": 90.0, "n_r": 10}},
                             drops=drops)
        self.assertEqual(rc, 1, out)
        self.assertIn("no valid reference", out)
        self.assertIn("no bounds entry", out)

    def test_drop_for_an_unbaselined_function_is_not_fatal_by_default(self):
        """A helper/static/new port the floor never covered must not block a
        commit -- but --strict still reports it."""
        scores = {"fn": {"score": 90.0, "source": SOURCE}}
        drops = [{"function": "helper", "reason": "no bounds entry",
                  "span_bytes": 10}]
        rc, out = self.check(scores, {"fn": {"score": 90.0, "n_r": 10}},
                             drops=drops)
        self.assertEqual(rc, 0, out)
        rc, _ = self.check(scores, {"fn": {"score": 90.0, "n_r": 10}},
                           drops=drops, strict=True)
        self.assertEqual(rc, 1)

    def test_whole_tu_compile_failure_is_reported_but_not_fatal(self):
        """The one carve-out, and it is explicit: no VC71 toolchain fails every
        compile.  Verified behaviour of the hook around this: `check` returns 0,
        `update` then runs non-blocking, and the commit proceeds -- so the report
        below is the ONLY signal, and it must say the functions were not
        covered."""
        scores = {"a": {"score": 90.0, "source": SOURCE},
                  "b": {"score": 90.0, "source": SOURCE}}
        rc, out = self.check(scores, {}, status="compile_failed")
        self.assertEqual(rc, 0, out)
        self.assertIn("compile failure", out)
        self.assertIn("2 function(s)", out)
        self.assertIn("NOT covered", out)
        self.assertNotIn("ACCOUNTING ERROR", out)

    def test_whole_tu_compile_failure_fails_strict(self):
        scores = {"a": {"score": 90.0, "source": SOURCE}}
        rc, _out = self.check(scores, {}, status="compile_failed", strict=True)
        self.assertEqual(rc, 1)

    def test_every_baselined_function_reaches_a_verdict(self):
        """The accounting residual is itself a failure: a function that reached
        no bucket is the same false negative as silence."""
        scores = {"ok": {"score": 90.0, "source": SOURCE},
                  "gone": {"score": 90.0, "source": SOURCE}}
        rc, out = self.check(scores, {"ok": {"score": 90.0, "n_r": 10}})
        self.assertEqual(rc, 1)
        self.assertNotIn("ACCOUNTING ERROR", out)


class TestBoundsGate(unittest.TestCase):
    """Editing function_bounds.json IS editing the references."""

    OLD = json.dumps({
        "_meta": {"entries": 2, "version": 1},
        "0x100c10": {"end": "0x100d54", "kind": "auto"},
        "0x200000": {"end": "0x200040", "kind": "auto"},
    })
    NEW = json.dumps({
        "_meta": {"entries": 3, "version": 1},
        "0x100c10": {"end": "0x100d90", "kind": "auto"},   # moved
        "0x200000": {"end": "0x200040", "kind": "auto"},   # unchanged
        "0x300000": {"end": "0x300010", "kind": "auto"},   # added
    })
    BASELINE = {
        "moved_fn": {"score": 90.0, "source": "src/halo/main/main.c",
                     "addr": "0x100c10"},
        "stable_fn": {"score": 90.0, "source": "src/halo/main/main.c",
                      "addr": "0x200000"},
        "no_addr_fn": {"score": 90.0, "source": "src/halo/game/game.c"},
    }

    def test_metadata_only_change_moves_nothing(self):
        other = json.loads(self.OLD)
        other["_meta"]["generated"] = "today"
        self.assertEqual(
            vc71.bounds_changed_addrs(self.OLD, json.dumps(other)), set())

    def test_changed_addrs_cover_edits_and_additions(self):
        self.assertEqual(vc71.bounds_changed_addrs(self.OLD, self.NEW),
                         {"0x100c10", "0x300000"})

    def test_removal_counts_as_a_change(self):
        trimmed = json.loads(self.OLD)
        del trimmed["0x200000"]
        self.assertIn("0x200000",
                      vc71.bounds_changed_addrs(self.OLD, json.dumps(trimmed)))

    def test_addresses_join_regardless_of_spelling(self):
        baseline = {"fn": {"score": 1.0, "source": "src/x.c",
                           "addr": "0x0000100C10"}}
        self.assertEqual(vc71.sources_for_bounds_addrs({"0x100c10"}, baseline),
                         {"src/x.c": ["fn"]})

    def test_only_affected_sources_are_returned(self):
        affected = vc71.sources_for_bounds_addrs({"0x100c10", "0x300000"},
                                                 self.BASELINE)
        self.assertEqual(affected, {"src/halo/main/main.c": ["moved_fn"]})

    def _gate(self, staged, old=None, new=None, baseline=None):
        blobs = {f"HEAD:{vc71.BOUNDS_REL}": old if old is not None else self.OLD,
                 f":{vc71.BOUNDS_REL}": new if new is not None else self.NEW}
        buf, err = io.StringIO(), io.StringIO()
        with patch.object(vc71, "staged_paths", return_value=set(staged)):
            with patch.object(vc71, "_git_show", side_effect=lambda s: blobs.get(s, "")):
                with patch.object(vc71, "load_baseline",
                                  return_value=baseline if baseline is not None
                                  else self.BASELINE):
                    with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(err):
                        rc = vc71.cmd_bounds_gate(SimpleNamespace())
        return rc, buf.getvalue(), err.getvalue()

    def test_no_bounds_staged_is_a_no_op(self):
        rc, out, err = self._gate(["src/halo/main/main.c"])
        self.assertEqual((rc, out, err), (0, "", ""))

    def test_bounds_without_rebaselined_scores_fails_with_instructions(self):
        rc, _out, err = self._gate([vc71.BOUNDS_REL])
        self.assertEqual(rc, 1)
        self.assertIn("src/halo/main/main.c", err)
        self.assertIn("populate --rebaseline", err)
        self.assertIn(vc71.SCORES_REL, err)

    def test_bounds_with_scores_prints_the_affected_sources(self):
        rc, out, _err = self._gate([vc71.BOUNDS_REL, vc71.SCORES_REL])
        self.assertEqual(rc, 0)
        self.assertEqual(out.split(), ["src/halo/main/main.c"])

    def test_bounds_change_touching_no_floor_is_a_no_op(self):
        rc, out, err = self._gate([vc71.BOUNDS_REL],
                                  baseline={"fn": {"score": 1.0,
                                                   "source": "src/x.c",
                                                   "addr": "0x999999"}})
        self.assertEqual((rc, out, err), (0, "", ""))


class TestMeasureMemoVersioning(unittest.TestCase):
    """The memo caches decisions, so its rules need their own version."""

    def test_version_is_2(self):
        self.assertEqual(vc71.MEASURE_MEMO_VERSION, 2)

    def test_older_memo_is_discarded_wholesale(self):
        """Version 1 cached a 'no objdiff.json unit' miss as a permanent verdict.
        That route is gone; its entries must not answer for anything."""
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "memo.json"
            path.write_text(json.dumps(
                {"version": 1, "entries": {"key": {"out": {}, "drops": []}}}))
            with patch.object(vc71, "MEASURE_CACHE_PATH", path):
                with patch.object(vc71, "_MEASURE_MEMO", None):
                    self.assertEqual(vc71._load_measure_memo(), {})
            path.write_text(json.dumps(
                {"version": vc71.MEASURE_MEMO_VERSION,
                 "entries": {"key": {"out": {"fn": {"score": 1.0}}}}}))
            with patch.object(vc71, "MEASURE_CACHE_PATH", path):
                with patch.object(vc71, "_MEASURE_MEMO", None):
                    self.assertIn("key", vc71._load_measure_memo())

    def test_empty_measurement_is_never_memoized(self):
        """A TU that scored nothing must re-measure: with the objdiff route gone
        the only way to get here is a broken toolchain, which the key does not
        cover and a re-run may fix."""
        completed = SimpleNamespace(returncode=1, stdout="", stderr="fatal error")
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "memo.json"
            with patch.object(vc71, "MEASURE_CACHE_PATH", path):
                with patch.object(vc71, "_MEASURE_MEMO", {}):
                    with patch.object(vc71, "_measure_key", return_value="k"):
                        with patch.object(vc71.subprocess, "run",
                                          return_value=completed):
                            meta = {}
                            vc71.run_vc71_verify(Path("src/x.c"), meta_out=meta)
                    self.assertEqual(vc71._MEASURE_MEMO, {})


class TestCandidateStaleness(unittest.TestCase):
    """A candidate score must never come from an object older than the source."""

    def setUp(self):
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import vc71_verify
        self.vv = vc71_verify
        self.temp = tempfile.TemporaryDirectory()
        self.dir = Path(self.temp.name)
        self.src = self.dir / "unit.c"
        self.obj = self.dir / "unit.obj"
        self.src.write_text("void fn(void) {}\n")
        self.obj.write_bytes(b"\x00compiled\x00")

    def tearDown(self):
        self.temp.cleanup()

    def _stamp(self, opt="/O2"):
        self.vv.obj_stamp_path(self.obj).write_text(
            self.vv.source_stamp(self.src, opt) + "\n")

    def test_unstamped_object_is_not_current(self):
        self.assertFalse(self.vv.obj_is_current(self.obj, self.src, "/O2"))

    def test_stamped_object_is_current(self):
        self._stamp()
        self.assertTrue(self.vv.obj_is_current(self.obj, self.src, "/O2"))

    def test_edited_source_invalidates_even_at_an_identical_mtime(self):
        """The mtime rule this replaced: /mnt/g is a coarse-timestamp DrvFs
        mount, so an edit landing in the same tick as the compile looked
        current, and content-preserving restores (cp -p, archive extract) made a
        stale object look newer than the source it no longer matches."""
        self._stamp()
        st = self.obj.stat()
        self.src.write_text("void fn(void) { int x = 1; }\n")
        import os
        os.utime(self.src, ns=(st.st_atime_ns, st.st_mtime_ns))
        self.assertEqual(self.src.stat().st_mtime, self.obj.stat().st_mtime)
        self.assertFalse(self.vv.obj_is_current(self.obj, self.src, "/O2"))

    def test_flags_are_part_of_the_identity(self):
        self._stamp(opt="/O2")
        self.assertFalse(self.vv.obj_is_current(self.obj, self.src, "/O2 /Ob1"))

    def test_missing_object_is_not_current(self):
        self._stamp()
        self.obj.unlink()
        self.assertFalse(self.vv.obj_is_current(self.obj, self.src, "/O2"))


if __name__ == "__main__":
    unittest.main()
