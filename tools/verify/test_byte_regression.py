#!/usr/bin/env python3
"""Byte regressions, coverage losses and stale-evidence rejection."""

import copy
import importlib.util
import json
import os
import subprocess
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch
from types import SimpleNamespace

SPEC = importlib.util.spec_from_file_location("byte_regression", Path(__file__).with_name("byte_regression.py"))
gate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(gate)


def record(matched=8, compared=10, function="example"):
    return {
        "address": "0x00012000", "function": function,
        "source": {"path": "src/example.c", "sha256": "source"},
        "reference": {"sha256": "xbe", "start": "0x00012000", "end": "0x0001200a",
                      "sha256_span": "span", "bound_kind": "auto", "bound_provenance": "table"},
        "tool": {"opt": "/O2", "decl_sha256": "decl", "bounds_sha256": "bounds"},
        "literal_byte_match": "different",
        "aligned_byte_match": {"status": "scored", "matching_bytes": matched,
                               "compared_bytes": compared, "uncertain_relocation_bytes": 0,
                               "mismatched_relocations": 0, "unpaired_relocations": 0,
                               "alignment_ambiguous_steps": 0},
    }


class ByteRatchet(unittest.TestCase):
    def result(self, before, after):
        return gate.compare({0x12000: before}, {0x12000: after})

    def test_unchanged_and_improved_pass(self):
        for new in (record(), record(9)):
            self.assertFalse(self.result(record(), new)["errors"])

    def test_rename_uses_address(self):
        self.assertFalse(self.result(record(), record(function="renamed"))["errors"])

    def test_one_byte_regression_fails(self):
        self.assertIn("bytes 8/10 -> 7/10", self.result(record(), record(7))["errors"][0])

    def test_larger_denominator_fails_even_with_same_matching_count(self):
        self.assertTrue(self.result(record(), record(8, 11))["errors"])

    def test_fewer_matches_fail_even_when_fraction_improves(self):
        self.assertTrue(self.result(record(), record(7, 7))["errors"])

    def test_tiny_fraction_loss_is_not_rounded_away(self):
        self.assertTrue(self.result(record(100000, 100001), record(100000, 100002))["errors"])

    def test_literal_address_replaced_by_resolved_symbol_is_neutral(self):
        after = record(4, 6)
        after["aligned_byte_match"]["masked_relocation_bytes"] = 4
        self.assertFalse(self.result(record(8, 10), after)["errors"])

    def test_resolved_relocation_does_not_hide_a_real_byte_loss(self):
        after = record(3, 6)
        after["aligned_byte_match"]["masked_relocation_bytes"] = 4
        self.assertTrue(self.result(record(8, 10), after)["errors"])

    def test_uncertain_relocation_is_still_a_loss(self):
        after = record(4, 10)
        after["aligned_byte_match"]["uncertain_relocation_bytes"] = 4
        self.assertTrue(self.result(record(8, 10), after)["errors"])

    def test_improvement_elsewhere_does_not_hide_loss(self):
        before = {1: record(8), 2: record(1)}
        after = {1: record(7), 2: record(10)}
        self.assertTrue(gate.compare(before, after)["errors"])

    def test_removed_or_deactivated_function_fails(self):
        self.assertTrue(gate.compare({1: record()}, {})["errors"])

    def test_lost_measurement_fails(self):
        after = record()
        after.pop("aligned_byte_match")
        self.assertTrue(self.result(record(), after)["errors"])

    def test_reference_changes_are_invalid_comparisons(self):
        for key in ("sha256", "start", "end", "sha256_span", "bound_kind", "bound_provenance"):
            with self.subTest(key=key):
                after = record(10)
                after["reference"][key] = "changed"
                self.assertIn("reference span changed", self.result(record(), after)["errors"][0])

    @staticmethod
    def spanless(reason="candidate has 0 COFF function symbols for example"):
        base = record()
        base.pop("aligned_byte_match")
        base["reference"] = {"sha256": "xbe"}
        base["reason"] = reason
        return base

    def test_spanless_base_may_become_measured(self):
        result = self.result(self.spanless(), record())
        self.assertFalse(result["errors"])
        self.assertEqual(result["existing_gaps"], [])

    def test_spanless_base_may_become_measured_under_other_options(self):
        after = record()
        after["tool"]["opt"] = "/O1"
        self.assertFalse(self.result(self.spanless(), after)["errors"])

    def test_unverified_bound_may_be_verified_and_measured(self):
        base = record()
        base.pop("aligned_byte_match")
        base["reference"]["bound_kind"] = "no_terminator"
        after = record()
        after["reference"].update({"end": "0x00012008", "bound_kind": "auto"})
        self.assertFalse(self.result(base, after)["errors"])

    def test_spanless_base_still_requires_same_xbe(self):
        after = record()
        after["reference"]["sha256"] = "other"
        self.assertIn("reference span changed", self.result(self.spanless(), after)["errors"][0])

    def test_unscored_base_with_span_still_rejects_span_change(self):
        base = record()
        base.pop("aligned_byte_match")
        base["reference"]["bound_provenance"] = "computed"
        after = record()
        after["reference"]["end"] = "0x00012008"
        self.assertIn("reference span changed", self.result(base, after)["errors"][0])

    def test_spanless_base_unmeasured_candidate_with_new_reason_fails(self):
        after = record()
        after.pop("aligned_byte_match")
        after["reason"] = "different failure"
        self.assertIn("unmeasured result changed", self.result(self.spanless(), after)["errors"][0])

    def test_changed_options_are_invalid_comparisons(self):
        after = record()
        after["tool"]["opt"] = "/O1"
        self.assertTrue(self.result(record(), after)["errors"])

    def test_uncertainty_or_wrong_targets_fail_despite_byte_gains(self):
        for key in ("uncertain_relocation_bytes", "mismatched_relocations", "unpaired_relocations",
                    "alignment_ambiguous_steps"):
            with self.subTest(key=key):
                after = record(10)
                after["aligned_byte_match"][key] = 1
                self.assertTrue(self.result(record(), after)["errors"])

    def test_literal_exactness_cannot_be_lost(self):
        before, after = record(10), record(10)
        before["literal_byte_match"] = "exact"
        self.assertTrue(self.result(before, after)["errors"])

    def test_new_port_requires_measurement(self):
        self.assertFalse(gate.compare({}, {1: record()})["errors"])
        new = record()
        new.pop("aligned_byte_match")
        self.assertTrue(gate.compare({}, {1: new})["errors"])

    def test_existing_gaps_are_reported_without_claiming_a_pass(self):
        gap = record()
        gap.pop("aligned_byte_match")
        gap["reason"] = "unsupported encoding"
        result = gate.compare({1: gap, 2: record()}, {1: gap, 2: record()})
        self.assertFalse(result["errors"])
        self.assertEqual(result["checked_functions"], 1)
        self.assertEqual(len(result["existing_gaps"]), 1)
        self.assertTrue(gate.compare({1: gap}, {1: gap})["errors"])
        changed = copy.deepcopy(gap)
        changed["reason"] = "different failure"
        self.assertTrue(self.result(gap, changed)["errors"])

    def test_runtime_computed_bounds_cannot_count_as_measured(self):
        new = record()
        new["reference"]["bound_provenance"] = "computed"
        self.assertIsNone(gate.counts(new))

    def test_invalid_counts_rejected(self):
        for matched, compared in ((11, 10), (0, 0), (-1, 10), (float("nan"), 10), (True, 10)):
            with self.subTest(matched=matched, compared=compared):
                with self.assertRaises(ValueError):
                    gate.counts(record(matched, compared))


class SnapshotValidation(unittest.TestCase):
    def setUp(self):
        self.inputs = {"src/example.c": "source", "build/generated/decl.h": "decl",
                       "tools/verify/function_bounds.json": "bounds"}
        self.plan = {"sources": ["src/example.c"]}
        self.snapshot = {"schema_version": 1, "commit": "commit", "run_id": "current-run",
                         "inputs": self.inputs, "plan": self.plan, "xbe_sha256": "xbe",
                         "records": [record()]}
        self.manifest = patch.object(gate, "input_manifest", return_value=self.inputs).start()
        self.hash = patch.object(gate, "digest", return_value="xbe").start()
        self.addCleanup(patch.stopall)

    def validate(self):
        return gate.indexed(self.snapshot, "commit", "current-run", Path("/unused"), self.plan)

    def test_current_snapshot_accepted(self):
        self.assertEqual(list(self.validate()), [0x12000])

    def test_old_run_commit_schema_and_scope_rejected(self):
        for key, value in (("run_id", "old-run"), ("commit", "old-commit"), ("schema_version", 0),
                           ("plan", {"sources": None})):
            with self.subTest(key=key):
                original = self.snapshot[key]
                self.snapshot[key] = value
                with self.assertRaisesRegex(ValueError, "provenance"):
                    self.validate()
                self.snapshot[key] = original

    def test_changed_header_or_kb_rejects_snapshot(self):
        self.manifest.return_value = {**self.inputs, "src/header.h": "new"}
        with self.assertRaisesRegex(ValueError, "input hashes"):
            self.validate()

    def test_changed_xbe_rejected(self):
        self.hash.return_value = "different"
        with self.assertRaisesRegex(ValueError, "XBE hash"):
            self.validate()

    def test_stale_per_function_source_and_declarations_rejected(self):
        for section, key in (("source", "sha256"), ("tool", "decl_sha256"),
                             ("tool", "bounds_sha256"), ("reference", "sha256")):
            with self.subTest(section=section, key=key):
                self.snapshot["records"] = [record()]
                self.snapshot["records"][0][section][key] = "old"
                with self.assertRaises(ValueError):
                    self.validate()

    def test_duplicate_and_empty_snapshots_fail(self):
        self.snapshot["records"].append(record())
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.validate()
        self.snapshot["records"] = []
        with self.assertRaisesRegex(ValueError, "empty"):
            self.validate()
        self.assertEqual(gate.indexed(self.snapshot, "commit", "current-run", Path("/unused"),
                                      self.plan, allow_empty=True), {})


class HeaderNoise(unittest.TestCase):
    def test_classified_losses_are_reported_but_do_not_fail(self):
        result = gate.compare({1: record()}, {1: record(7)}, lambda before, after: True)
        self.assertFalse(result["errors"])
        self.assertIn("bytes 8/10 -> 7/10", result["header_noise"][0])
        result = gate.compare({1: record()}, {1: record(7)}, lambda before, after: False)
        self.assertTrue(result["errors"])
        self.assertFalse(result["header_noise"])

    def classifier(self, changed="kb.json\nsrc/other.c", names=("changed_fn",), symbols=("_other",),
                   header_names=(), root=Path("/unused")):
        def parse(path, function):
            if path == "missing.obj":
                raise OSError("missing")
            return b"", [{"symbol": {"name": name}} for name in symbols], {}
        raw = SimpleNamespace(_parse_coff=parse, _c_name=lambda name: name[1:])
        with patch.object(gate, "git", return_value=changed), \
                patch.object(gate, "changed_kb_names", return_value=set(names)), \
                patch.object(gate, "changed_header_names", return_value=set(header_names)), \
                patch.dict("sys.modules", {"raw_xbe_structural": raw}):
            return gate.header_noise_filter(root, "base", "head")

    def after(self, **changes):
        after = record(7)
        after["candidate"] = {"path": "example.obj"}
        after.update(changes)
        return after

    def test_unrelated_function_is_noise(self):
        self.assertTrue(self.classifier()(record(), self.after()))

    def test_caller_of_changed_declaration_is_a_regression(self):
        is_noise = self.classifier(symbols=("_other", "_changed_fn"))
        self.assertFalse(is_noise(record(), self.after()))

    def test_changed_function_source_or_object_is_a_regression(self):
        is_noise = self.classifier(names=("example",))
        self.assertFalse(is_noise(record(), self.after()))
        is_noise = self.classifier()
        self.assertFalse(is_noise(record(), self.after(source={"path": "src/example.c",
                                                              "sha256": "edited"})))
        self.assertFalse(is_noise(record(), self.after(candidate={"path": "missing.obj"})))

    def test_other_shared_inputs_disable_classification(self):
        for changed in ("kb.json\nsrc/types.h\nthird_party/xbox/x.h", "CMakeLists.txt",
                        "tools/analysis/knowledge.py"):
            with self.subTest(changed=changed):
                self.assertIsNone(self.classifier(changed=changed))

    def test_unrelated_function_is_noise_when_types_header_changed(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "src/example.c"
            source.parent.mkdir()
            source.write_text("/* object_datum_t */\nint other(void) { return 0; }\n"
                              "int example(int x)\n{\n  return x + 1;\n}\n")
            is_noise = self.classifier(changed="src/types.h", header_names=("object_datum_t", "vitality"),
                                       root=Path(tmp))
            self.assertTrue(is_noise(record(), self.after()))

    def test_function_using_a_changed_header_name_is_a_regression(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "src/example.c"
            source.parent.mkdir()
            source.write_text("int example(object_datum_t *o)\n{\n  return o->vitality[0];\n}\n")
            is_noise = self.classifier(changed="src/types.h", header_names=("vitality",), root=Path(tmp))
            self.assertFalse(is_noise(record(), self.after()))

    def test_hex_literal_digits_are_not_identifiers(self):
        self.assertEqual(gate._IDENTIFIER_RE.findall("a = 0x34 + b2 + 0x5a8d30;"), ["a", "b2"])

    def test_function_source_not_found_is_a_regression(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "src/example.c"
            source.parent.mkdir()
            source.write_text("int unrelated(void) { return 0; }\n")
            is_noise = self.classifier(changed="src/types.h", header_names=("vitality",), root=Path(tmp))
            self.assertFalse(is_noise(record(), self.after()))

    def test_function_text_matches_definition_not_prototype_or_call(self):
        text = ("int example(int x);\nint caller(void)\n{\n  return example(1);\n}\n"
                "static int example(int x)\n{\n  if (x) { return 1; }\n  return 0;\n}\n")
        body = gate.function_text(text, "example")
        self.assertTrue(body.startswith("static int example"))
        self.assertTrue(body.endswith("return 0;\n}"))
        self.assertIsNone(gate.function_text("int other(void) { return 0; }\n", "example"))

    def test_changed_header_names_collects_changed_lines_and_enclosing_declaration(self):
        diff = ("diff --git a/src/types.h b/src/types.h\n--- a/src/types.h\n+++ b/src/types.h\n"
                "@@ -3273 +3273,3 @@ typedef struct object_datum_t {\n"
                "-  char pad_48[0xb6 - 0x48];\n+  char pad_48[0x88 - 0x48];\n+  real vitality[4];\n")
        with patch.object(gate, "git", return_value=diff):
            names = gate.changed_header_names(Path("/unused"), "base", "head")
        self.assertTrue({"object_datum_t", "pad_48", "vitality"} <= names)
        self.assertFalse(names & {"char", "real"})

    def test_changed_header_names_ignores_same_line_comment_words(self):
        diff = ("--- a/src/types.h\n+++ b/src/types.h\n"
                "@@ -10 +10,2 @@ typedef struct {\n"
                "-  uint8_t pad_24[0x90];  ///< offset=0x24  not accessed here\n"
                "+  uint8_t pad_24[0x10];  ///< offset=0x24  not accessed here\n"
                "+  uint8_t pad_34[0x80]; /* offset tail */\n")
        with patch.object(gate, "git", return_value=diff):
            names = gate.changed_header_names(Path("/unused"), "base", "head")
        self.assertTrue({"pad_24", "pad_34"} <= names)
        self.assertFalse(names & {"offset", "not", "accessed", "here", "tail"})

    def test_changed_kb_names_collects_changed_entries_only(self):
        def kb(decl, source="a.c"):
            return json.dumps({"md5": "m", "objects": [{
                "name": "a.obj", "source": source,
                "functions": [{"addr": "0x10", "decl": decl},
                              {"addr": "0x20", "decl": "void same(void);"}]}]})
        with patch.object(gate.subprocess, "check_output",
                          side_effect=[kb("void f(void);"), kb("int f(short value);")]):
            names = gate.changed_kb_names(Path("/unused"), "base", "head")
        self.assertIn("f", names)
        self.assertNotIn("same", names)
        with patch.object(gate.subprocess, "check_output",
                          side_effect=[kb("void f(void);"), kb("void f(void);", "b.c")]):
            self.assertIsNone(gate.changed_kb_names(Path("/unused"), "base", "head"))


class ScopeAndInputs(unittest.TestCase):
    def test_c_only_checks_whole_changed_tus_including_deleted_files(self):
        with patch.object(gate, "git", return_value="src/a.c\nsrc/deleted.c\ndocs/readme.md"):
            self.assertEqual(gate.scope(Path("/unused"), "base", "head")["sources"],
                             ["src/a.c", "src/deleted.c"])

    def test_shared_inputs_force_full_coverage(self):
        for name in ("src/types.h", "kb.json", "CMakeLists.txt", "src/CMakeLists.txt",
                     "third_party/xbox/xbox.h", "toolchains/llvm.cmake",
                     "tools/analysis/knowledge.py", "tools/verify/raw_xbe_structural.py",
                     ".github/workflows/vc71-regression.yml"):
            with self.subTest(name=name), patch.object(gate, "git", return_value=name):
                self.assertIsNone(gate.scope(Path("/unused"), "base", "head")["sources"])

    def test_docs_only_needs_no_compilation(self):
        with patch.object(gate, "git", return_value="README.md\ndocs/example.md"):
            self.assertEqual(gate.scope(Path("/unused"), "base", "head")["sources"], [])

    def test_input_manifest_detects_header_changes_with_real_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            names = ["src/example.c", "src/header.h", "build/generated/decl.h", *gate.EVALUATOR_FILES]
            for name in names:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("original")
            with patch.object(gate, "git", return_value="src/example.c\nsrc/header.h"):
                first = gate.input_manifest(root)
                (root / "src/header.h").write_text("changed")
                self.assertNotEqual(first, gate.input_manifest(root))

    def test_working_tree_staging_skips_gitignored_input_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.check_call(["git", "-C", str(root), "init", "-q"])
            (root / ".gitignore").write_text("local.json\n")
            (root / "local.json").write_text("{}")
            (root / "tracked.json").write_text("{}")
            (root / "src").mkdir()
            (root / "src/example.c").write_text("original")
            paths = ("src", "tracked.json", "local.json")
            with patch.object(gate, "INPUT_PATHS", paths):
                self.assertEqual(gate.stageable_inputs(root), ["src", "tracked.json"])
                subprocess.check_call(["git", "-C", str(root), "add", "-A", "--", *gate.stageable_inputs(root)])

    def test_wrong_revision_dirty_inputs_and_untracked_headers_rejected(self):
        for responses in (("wrong",), ("commit", "src/example.c"),
                          ("commit", "", "src/new.h")):
            with self.subTest(responses=responses), patch.object(gate, "git", side_effect=responses):
                with self.assertRaises(ValueError):
                    gate.ensure_revision(Path("/unused"), "commit")


class SnapshotOrchestration(unittest.TestCase):
    def test_staged_and_working_snapshots_preserve_user_work(self):
        for working, reuse in ((False, "0"), (True, "0"), (False, "1"), (True, "1")):
            with self.subTest(working=working, reuse=reuse), tempfile.TemporaryDirectory() as directory, \
                    patch.dict(gate.os.environ, {"HALO_BYTE_GATE_REUSE_TREES": reuse}):
                root = Path(directory)
                def git(*args):
                    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()
                git("init", "-q")
                (root / ".gitignore").write_text("artifacts/\nhalo-patched/\n")
                for name in (*gate.EVALUATOR_FILES, "src/example.c"):
                    path = root / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text("original")
                git("add", ".")
                git("-c", "user.name=Test", "-c", "user.email=test@localhost", "commit", "-qm", "base")
                xbe = root / "halo-patched/cachebeta.xbe"
                xbe.parent.mkdir()
                xbe.write_bytes(b"reference")
                original_head = git("rev-parse", "HEAD")
                original_worktrees = git("worktree", "list", "--porcelain")
                source = root / "src/example.c"
                run = subprocess.run
                # A reused checkout must come back clean: no overlay, no strays.
                rounds = ("staged", "restaged") if reuse == "1" else ("staged",)
                for staged in rounds:
                    source.write_text(staged)
                    git("add", "src/example.c")
                    source.write_text("unstaged")
                    original_index = git("write-tree")
                    def external(command, **kwargs):
                        if command[0] == gate.sys.executable:
                            candidate = Path(kwargs["cwd"])
                            self.assertEqual((candidate / "src/example.c").read_text(),
                                             "unstaged" if working else staged)
                            baseline = Path(command[command.index("--base-root") + 1])
                            self.assertEqual((baseline / "src/example.c").read_text(), "original")
                            self.assertFalse((baseline / "stray.txt").exists())
                            self.assertEqual(len(str(candidate)), len(str(baseline)))
                            (baseline / "stray.txt").write_text("left behind")
                            return SimpleNamespace(returncode=0)
                        return run(command, **kwargs)
                    args = SimpleNamespace(base_ref="HEAD", head_ref=None, working_tree=working, workers=1)
                    with patch.object(gate, "ROOT", root), patch.object(gate, "INPUT_PATHS", ("src",)), \
                            patch.object(gate.subprocess, "run", side_effect=external):
                        self.assertEqual(gate.local(args), 0)
                    self.assertEqual(git("rev-parse", "HEAD"), original_head)
                    self.assertEqual(git("write-tree"), original_index)
                    self.assertEqual(source.read_text(), "unstaged")
                trees = git("worktree", "list", "--porcelain")
                if reuse == "1":
                    self.assertEqual(trees.count(gate.REUSABLE_TREES + "/"), 2)
                else:
                    self.assertEqual(trees, original_worktrees)


class ReusableTrees(unittest.TestCase):
    def test_stale_checkout_is_rebuilt(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "repo"
            root.mkdir()
            def git(*args):
                return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()
            git("init", "-q")
            (root / "file.c").write_text("one")
            git("add", ".")
            git("-c", "user.name=Test", "-c", "user.email=test@localhost", "commit", "-qm", "one")
            first = git("rev-parse", "HEAD")
            (root / "file.c").write_text("two")
            git("-c", "user.name=Test", "-c", "user.email=test@localhost", "commit", "-qam", "two")
            second = git("rev-parse", "HEAD")
            tree = Path(directory) / "trees" / "head-tree"
            with patch.object(gate, "ROOT", root):
                gate.reset_tree(tree, first, dict(gate.os.environ))
                self.assertEqual((tree / "file.c").read_text(), "one")
                (tree / "file.c").write_text("dirty")
                (tree / "stray.o").write_text("x")
                gate.reset_tree(tree, second, dict(gate.os.environ))
                self.assertEqual((tree / "file.c").read_text(), "two")
                self.assertFalse((tree / "stray.o").exists())
                (tree / ".git").write_text("gitdir: /nonexistent/worktree\n")
                gate.reset_tree(tree, first, dict(gate.os.environ))
                self.assertEqual((tree / "file.c").read_text(), "one")
                self.assertEqual(subprocess.check_output(["git", "-C", str(tree), "rev-parse", "HEAD"],
                                                         text=True).strip(), first)


class PruneRuns(unittest.TestCase):
    def make(self, parent, name, age, checkout=False):
        path = parent / name
        (path / "measurements").mkdir(parents=True)
        if checkout:
            (path / ".git").write_text("gitdir: elsewhere\n")
        stamp = time.time() - age
        os.utime(path, (stamp, stamp))
        return path

    def test_prunes_old_runs_but_never_reusable_cache_or_checkouts(self):
        with tempfile.TemporaryDirectory() as tmp:
            parent = Path(tmp)
            old = [self.make(parent, "local-old%05d" % i, 86400 + i) for i in range(4)]
            fresh = self.make(parent, "local-fresh001", 60)
            reusable = self.make(parent, gate.REUSABLE_TREES, 10 * 86400)
            cache = self.make(parent, "cache", 10 * 86400)
            checkout = self.make(parent, "local-chkout01", 10 * 86400, checkout=True)
            other = self.make(parent, "local-too-long-name", 10 * 86400)
            gate.prune_runs(parent, keep=1, min_age=3600)
            self.assertTrue(fresh.exists())
            self.assertFalse(any(path.exists() for path in old))
            for kept in (reusable, cache, checkout, other):
                self.assertTrue(kept.exists(), kept.name)

    def test_recent_runs_survive_even_beyond_keep(self):
        with tempfile.TemporaryDirectory() as tmp:
            parent = Path(tmp)
            runs = [self.make(parent, "local-run%05d" % i, 30 + i) for i in range(5)]
            gate.prune_runs(parent, keep=1, min_age=3600)
            self.assertTrue(all(path.exists() for path in runs))


class ByteCache(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name) / "tree"
        for name in gate.CACHE_TOOL_FILES:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("tool")
        self.inputs = {"src/a.c": "a1", "src/b.c": "b1", "src/types.h": "h1", "kb.json": "k1"}
        self.functions = [{"function": "f", "address": 0x10, "source": self.root / "src/a.c"}]

    def tearDown(self):
        self.directory.cleanup()

    def key(self, inputs=None, environment=None, xbe="x", root=None, functions=None):
        root = root or self.root
        inputs = inputs or self.inputs
        common = gate.cache_common_key(root, inputs, environment or {"compiler": "c"}, xbe)
        functions = functions or [dict(item, source=root / "src/a.c") for item in self.functions]
        return gate.tu_cache_key(root, common, root / "src/a.c", functions, inputs)

    def test_key_ignores_other_translation_units_only(self):
        base = self.key()
        self.assertEqual(self.key(inputs=dict(self.inputs, **{"src/b.c": "b2"})), base)
        for change in ({"src/a.c": "a2"}, {"src/types.h": "h2"}, {"kb.json": "k2"}):
            self.assertNotEqual(self.key(inputs=dict(self.inputs, **change)), base)

    def test_key_covers_tools_compiler_xbe_scope_and_path_length(self):
        base = self.key()
        self.assertNotEqual(self.key(environment={"compiler": "d"}), base)
        self.assertNotEqual(self.key(xbe="y"), base)
        self.assertNotEqual(self.key(functions=[{"function": "g", "address": 0x10}]), base)
        (self.root / gate.CACHE_TOOL_FILES[-1]).write_text("edited")
        self.assertNotEqual(self.key(), base)

    def test_key_reused_at_equal_root_length_only(self):
        (self.root / gate.CACHE_TOOL_FILES[-1]).write_text("tool")
        other = Path(self.directory.name) / "abcd"
        longer = Path(self.directory.name) / "longer-tree"
        for root in (other, longer):
            for name in gate.CACHE_TOOL_FILES:
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                (root / name).write_text("tool")
        self.assertEqual(self.key(root=other), self.key())
        self.assertNotEqual(self.key(root=longer), self.key())

    def test_untracked_source_is_not_cached(self):
        common = gate.cache_common_key(self.root, self.inputs, {}, "x")
        self.assertIsNone(gate.tu_cache_key(self.root, common, self.root / "src/new.c",
                                            self.functions, self.inputs))

    def test_store_load_round_trip_and_scope_mismatch(self):
        cache = Path(self.directory.name) / "cache"
        records = [{"address": "0x00000010", "aligned_byte_match": {"matching_bytes": 3}}]
        gate.store_cached_tu(cache, "k", records)
        self.assertEqual(gate.load_cached_tu(cache, "k", self.functions), records)
        self.assertIsNone(gate.load_cached_tu(cache, "k", [{"address": 0x20}]))
        self.assertIsNone(gate.load_cached_tu(cache, "missing", self.functions))
        (cache / "k.json.gz").write_bytes(b"corrupt")
        self.assertIsNone(gate.load_cached_tu(cache, "k", self.functions))


if __name__ == "__main__":
    unittest.main()
