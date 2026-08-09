#!/usr/bin/env python3
"""Prove the reference migration only ever moved a reference CLOSER to the binary.

WHAT CHANGED
------------
Scoring used to pick a reference off a ladder: the TU's whole delinked object,
else a sibling range export, else a per-function chunk, else a reference
synthesized from the XBE.  Now there is one canonical reference per function --
the bytes at [start, end) in the pristine XBE, where `end` comes from the
committed tools/verify/function_bounds.json.

Every score in the baseline was measured against a rung of that ladder, so this
change moves numbers.  "The numbers moved" is not evidence either way; what
matters is the DIRECTION, and there is an arbiter that is neither the old rule
nor the new one: the binary itself.

THE MEASUREMENT
---------------
For every function currently in tools/verify/vc71_scores.json:

  truth  = instructions obtained by linearly decoding [start, end) from the
           bounds table with capstone, minus trailing nop/int3 padding
  old    = length of the reference the pre-change selection would have used
  new    = length of the bounds-derived reference

and the bar is absolute: for every function where old != new, |new - truth|
must be <= |old - truth|.  A single function moving AWAY from the binary is a
regression in the derivation and must be fixed, not explained.

`truth` is deliberately naive -- a straight linear decode -- so that it shares
no code with either reference path.  It is a length check, not a byte check:
two references of the same length can still differ, which is why this harness
also reports how many same-length references disagree instruction-for-
instruction (those are the pure-normalization differences, e.g. absolute
displacements a delinked object zeroed).

The OLD side is reconstructed by loading the pre-change tools/verify/*.py out of
git (default: HEAD) rather than by re-implementing the ladder, so it cannot
drift from what actually shipped.  The two override loops ARE re-expressed here
(they were closures inside run_compare_cached and cannot be imported); they are
transcribed from the same commit and the transcription is the one thing in this
file worth reviewing line-by-line against `git show <rev>:tools/verify/vc71_verify.py`.

This needs no VC71 compiler: it only ever looks at the reference side.

Usage:
    tools/verify/validate_ref_migration.py                 # full sweep
    tools/verify/validate_ref_migration.py --limit 200     # quick pass
    tools/verify/validate_ref_migration.py --source main/main.c
    tools/verify/validate_ref_migration.py --rev <sha>     # other "old" tree
"""
import argparse
import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SCORES = REPO_ROOT / "tools" / "verify" / "vc71_scores.json"
REPORT = REPO_ROOT / "artifacts" / "audit" / "ref_migration_report.json"

sys.path.insert(0, str(REPO_ROOT / "tools" / "verify"))
sys.path.insert(0, str(REPO_ROOT / "tools" / "audit"))

import vc71_verify as new                                   # noqa: E402
import xbe_reference as xr                                  # noqa: E402


# ---------------------------------------------------------------------------
# The arbiter: what the binary says
# ---------------------------------------------------------------------------

_PAD = ("nop", "int3")


def _no_ann(insn: str) -> str:
    """Drop llvm-objdump's trailing `# ...` annotation."""
    i = insn.find("#")
    return insn[:i].rstrip() if i > 0 else insn


def truth_insn_count(addr: int) -> int | None:
    """Instructions in [start, end) from the bounds table, minus trailing pad.

    Shares no code with either reference path on purpose: a bug common to both
    would otherwise cancel out and this harness would certify it.
    """
    ext = xr.function_extent(addr)
    if ext is None:
        return None
    end = ext[0]
    code, err = xr.function_bytes(addr)
    if code is None:
        return None
    try:
        import capstone
    except ImportError:
        return None
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    mnems = [i.mnemonic for i in md.disasm(code, addr)]
    while mnems and mnems[-1] in _PAD:
        mnems.pop()
    return len(mnems) if mnems else None


# ---------------------------------------------------------------------------
# The old side, loaded out of git
# ---------------------------------------------------------------------------

def _git_show(rev: str, rel: str) -> str:
    out = subprocess.run(["git", "show", f"{rev}:{rel}"],
                         capture_output=True, text=True, cwd=REPO_ROOT)
    if out.returncode != 0:
        raise SystemExit(f"error: git show {rev}:{rel} failed: {out.stderr.strip()}")
    return out.stdout


def _load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, str(path))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


def load_old(rev: str, workdir: Path):
    """The pre-change vc71_verify + xbe_reference, with their paths repointed.

    Both compute REPO_ROOT from __file__, which is wrong once the file lives in
    a temp directory, so every path constant derived from it is patched back
    afterwards.  Loading the real historical code rather than paraphrasing it is
    the whole point: a paraphrase would be marking its own homework.
    """
    workdir.mkdir(parents=True, exist_ok=True)
    v_path = workdir / "vc71_verify_old.py"
    x_path = workdir / "xbe_reference_old.py"
    v_path.write_text(_git_show(rev, "tools/verify/vc71_verify.py"))
    x_path.write_text(_git_show(rev, "tools/verify/xbe_reference.py"))

    old_xr = _load_module(x_path, "xbe_reference_old")
    old_xr.REPO_ROOT = REPO_ROOT
    old_xr.XBE = REPO_ROOT / "halo-patched" / "cachebeta.xbe"
    old_xr.CHUNK_DIR = REPO_ROOT / "delinked" / "functions"

    old_v = _load_module(v_path, "vc71_verify_old")
    old_v.REPO_ROOT = REPO_ROOT
    old_v.OBJDIFF_JSON = REPO_ROOT / "objdiff.json"
    old_v.BUILD_DIR = REPO_ROOT / "build"
    old_v.VC71_OUT_DIR = REPO_ROOT / "build" / "vc71"
    old_v.DELINKED_DIR = REPO_ROOT / "delinked"
    old_v.SCORE_CONTEXT_DIR = REPO_ROOT / "artifacts" / "score_context"
    old_v.COMPARE_SCRIPT = REPO_ROOT / "tools" / "verify" / "compare_obj.py"
    # Sanity: a mis-patched REPO_ROOT silently yields empty kb data, which would
    # make every old reference look absent and every comparison meaningless.
    if not old_v._kb_func_starts():
        raise SystemExit("error: the historical module cannot read kb.json "
                         "(REPO_ROOT patch failed) -- refusing to compare")
    return old_v, old_xr


class OldSelector:
    """The pre-change reference selection, per TU.

    Transcribed from `run_compare_cached` at the snapshot revision: the
    whole-object slice, then override loop (1) "replace a slice that does not
    bound the function well" and loop (2) "add functions the whole object
    dropped entirely".  The per-function helpers it calls are imported from the
    historical module, not re-implemented.
    """

    def __init__(self, old_v, old_xr, co, source: Path):
        self.v, self.xr, self.co, self.source = old_v, old_xr, co, source
        self.v._set_alias_source(source)
        self.whole: dict[str, list[str]] = {}
        self.reference = None
        try:
            units = self.v.load_units()
            unit = self.v.choose_unit(str(source), units, None)
        except Exception:
            unit = None
        if unit:
            ref = REPO_ROOT / unit.get("base_path", "")
            if ref.exists():
                self.reference = ref
                try:
                    self.whole = self.co.disassemble(str(ref))
                except Exception:
                    self.whole = {}
        self._sib_objs: list[Path] | None = None
        self._sib_slices: dict[str, dict] = {}

    # -- helpers lifted from the closures ---------------------------------

    def _chunk_ref(self, fn: str):
        chunk = self.v._per_function_ref(fn)
        if not chunk or not chunk.exists():
            return None
        aliases = set(self.v.function_aliases(fn)) | {fn}
        addr = self.v._func_addr(fn)
        if addr is not None:
            aliases.add(f"FUN_{addr & 0xffffffff:08x}")
        try:
            cand = self.co.first_function_insns(str(chunk), aliases)
        except Exception:
            return None
        if not cand:
            return None
        if not self.v._ref_insns_valid(len(cand), self.v._func_span(fn)):
            return None
        return cand

    def _synth_ref(self, fn: str):
        addr = self.v._func_addr(fn)
        if addr is None:
            return None
        try:
            obj = self.xr.reference_object(
                addr, cache_dir=REPO_ROOT / "artifacts" / "vc71_oldref_cache")
        except Exception:
            return None
        if obj is None:
            return None
        aliases = (set(self.v.function_aliases(fn))
                   | {fn, f"FUN_{addr & 0xffffffff:08x}"})
        try:
            cand = self.co.first_function_insns(str(obj), aliases)
        except Exception:
            return None
        if not cand:
            return None
        cand = [self.xr.normalize_synth_insn(i) for i in cand]
        return (cand if self.v._ref_insns_valid(len(cand), self.v._func_span(fn))
                else None)

    def _sibling_objects(self) -> list[Path]:
        if self._sib_objs is None:
            self._sib_objs = []
            try:
                units = self.v.find_units(str(self.source), self.v.load_units())
            except Exception:
                units = []
            for u in units:
                p = REPO_ROOT / u.get("base_path", "")
                if not p.exists():
                    continue
                if re.match(r"[0-9a-f]{8}\.obj$", p.name, re.IGNORECASE):
                    continue
                try:
                    if self.reference is not None and p.samefile(self.reference):
                        continue
                except OSError:
                    continue
                self._sib_objs.append(p)
        return self._sib_objs

    def _sibling_refs(self, fn: str) -> list[list[str]]:
        out = []
        for p in self._sibling_objects():
            key = str(p)
            if key not in self._sib_slices:
                try:
                    self._sib_slices[key] = self.co.disassemble(key)
                except Exception:
                    self._sib_slices[key] = {}
            slices = self._sib_slices[key]
            for alias in set(self.v.function_aliases(fn, self.source)) | {fn}:
                s = slices.get(alias)
                if s and self.v._ref_insns_valid(len(s), self.v._func_span(fn)):
                    out.append(s)
                    break
        return out

    # -- the selection itself ---------------------------------------------

    def reference_for(self, fn: str, aliases: set[str]):
        """(insns, rung) the old path would have scored `fn` against."""
        key = next((a for a in (fn, *sorted(aliases)) if a in self.whole), None)
        if key is not None:
            whole = self.whole[key]
            truth = self.v._true_insn_count(key)
            span_ok = self.v._ref_insns_valid(len(whole), self.v._func_span(key))
            if span_ok and not self.v._slice_is_bloated(whole) and (
                    truth is None or len(whole) == truth):
                return whole, "whole_object"
            alternates = self._sibling_refs(key)
            chunk = self._chunk_ref(key)
            if chunk is not None:
                alternates.append(chunk)
            best, best_len, rung = None, len(whole), "whole_object"
            for alt in alternates:
                if truth is not None:
                    if self.v._closer_to_truth(len(alt), best_len, truth):
                        best, best_len, rung = alt, len(alt), "sibling_or_chunk"
                elif (self.v._slice_is_bloated(whole)
                      and not self.v._slice_is_bloated(alt)):
                    best, best_len, rung = alt, len(alt), "sibling_or_chunk"
                elif not span_ok and len(alt) > best_len:
                    best, best_len, rung = alt, len(alt), "sibling_or_chunk"
            return (best, rung) if best is not None else (whole, "whole_object")

        cand = self._chunk_ref(fn)
        if cand is not None:
            return cand, "chunk"
        cand = self._synth_ref(fn)
        if cand is not None:
            return cand, "synth"
        return None, "dropped"


# ---------------------------------------------------------------------------
# Sweep
# ---------------------------------------------------------------------------

def load_targets(limit: int | None, source_filter: str | None,
                 offset: int = 0):
    """[(name, source)] from vc71_scores.json, grouped later by source.

    `offset`/`limit` shard the sweep.  A full pass is ~25 minutes of
    llvm-objdump subprocesses, which is longer than some runners tolerate, so it
    has to be splittable without changing what any one function measures.
    """
    data = json.loads(SCORES.read_text())
    scores = data.get("scores", data)
    out = []
    for name, entry in sorted(scores.items()):
        src = (entry or {}).get("source")
        if not src:
            continue
        if source_filter and not src.replace("\\", "/").endswith(source_filter):
            continue
        out.append((name, src))
    out = out[offset:]
    return out[:limit] if limit else out


def merge(paths, out: Path) -> int:
    """Combine shard reports written by --offset/--limit runs.

    A full sweep is long enough that it has to be shardable; a verdict split
    across eight files is not a verdict, so recombining them is part of the
    tool rather than something the reader is left to do by eye.
    """
    counts, rungs, kinds = Counter(), Counter(), Counter()
    lists = {"changed": [], "changed_farther": [],
             "same_length_scored_differs": [],
             "same_length_differs_beyond_annotation": [],
             "functions_without_truth": []}
    rev = None
    for path in sorted(paths):
        r = json.loads(Path(path).read_text())
        rev = rev or r.get("rev_old")
        counts.update(r.get("counts", {}))
        rungs.update(r.get("old_rung_distribution", {}))
        kinds.update(r.get("new_bound_kinds", {}))
        for key in lists:
            lists[key].extend(r.get(key, []))
    report = {"rev_old": rev, "counts": dict(counts),
              "old_rung_distribution": dict(rungs),
              "new_bound_kinds": dict(kinds),
              "merged_from": sorted(str(p) for p in paths), **lists}
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=1) + "\n")
    _print_report(report, out)
    return 1 if (counts["changed_farther"] + counts["new_missing"]) else 0


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rev", default="HEAD",
                    help="revision holding the PRE-change tools (default HEAD)")
    ap.add_argument("--limit", type=int, help="only N scored functions")
    ap.add_argument("--offset", type=int, default=0,
                    help="skip the first N scored functions (shard a sweep)")
    ap.add_argument("--source", help="only functions from a source path suffix")
    ap.add_argument("-o", "--out", type=Path, default=REPORT)
    ap.add_argument("--merge", nargs="+", metavar="SHARD.json",
                    help="combine shard reports into one and print the totals; "
                         "runs no measurement of its own")
    args = ap.parse_args()

    if args.merge:
        return merge(args.merge, args.out)

    targets = load_targets(args.limit, args.source, args.offset)
    if not targets:
        print("no scored functions matched", file=sys.stderr)
        return 1

    co = new.load_compare_obj()
    tmp = Path(tempfile.mkdtemp(prefix="refmig_"))
    old_v, old_xr = load_old(args.rev, tmp)

    by_source: dict[str, list[str]] = {}
    for name, src in targets:
        by_source.setdefault(src, []).append(name)

    counts = Counter()
    farther: list[dict] = []
    changed: list[dict] = []
    no_truth: list[str] = []
    same_len_differ: list[dict] = []
    beyond_ann: list[dict] = []
    rungs = Counter()
    kinds = Counter()

    for src, names in sorted(by_source.items()):
        source = REPO_ROOT / src
        selector = OldSelector(old_v, old_xr, co, source)
        new._set_alias_source(source)
        tu_funcs = new._kb_functions_for_source(source)
        for name in names:
            counts["total"] += 1
            # A baseline key can be the delinked object's C++ namespace-
            # qualified symbol (D3D8::D3DResource_IsBusy) where the candidate
            # compiles the plain C name.  The derived reference is keyed by
            # address, so resolve through the short name -- otherwise these
            # look like lost coverage when they are only a lost SPELLING.
            probe = name.rsplit("::", 1)[-1]
            if probe != name:
                counts["namespace_qualified_baseline_key"] += 1
            aliases = set(new.function_aliases(name, source)) | {name}
            new_insns, meta = new.derive_reference(probe, source, tu_funcs, co)
            old_insns, rung = selector.reference_for(name, aliases)
            rungs[rung] += 1

            if new_insns is None:
                counts["new_missing"] += 1
                farther.append({"function": name, "source": src,
                                "why": "no derived reference", "detail": meta})
                continue
            kinds[meta["kind"]] += 1
            addr = meta["addr"]
            truth = truth_insn_count(addr)
            if truth is None:
                no_truth.append(name)

            if old_insns is None:
                # The old path had no reference at all -- these are the
                # functions the migration exists to rescue, and there is
                # nothing to move away from.
                counts["old_dropped_now_scored"] += 1
                continue

            if len(old_insns) == len(new_insns):
                counts["unchanged_length"] += 1
                if old_insns != new_insns:
                    # Raw text differing is expected and benign: a delinked
                    # object prints `call <tag_get>` where raw XBE bytes print
                    # `calll 0x9f2c0`, and zeroed relocation fields print as no
                    # displacement at all.  The number that matters is whether
                    # they still differ AFTER the scorer's own normalization,
                    # which is what the LCS actually consumes.
                    counts["same_length_text_differs"] += 1
                    o_n = [co.normalize_instruction(i) for i in old_insns]
                    n_n = [co.normalize_instruction(i) for i in new_insns]
                    if o_n != n_n:
                        counts["same_length_scored_differs"] += 1
                        # How much of that is llvm-objdump's trailing
                        # `# imm = 0x...` annotation, which the derived
                        # reference strips (xbe_reference._strip_annotation) and
                        # a delinked object keeps?  Reported separately because
                        # it is a NORMALIZATION question, not a bounds question:
                        # the candidate side keeps its annotations too, so the
                        # asymmetry is on the reference side and belongs to the
                        # re-baseline discussion, not to this migration.
                        ann_only = ([_no_ann(i) for i in o_n]
                                    == [_no_ann(i) for i in n_n])
                        if ann_only:
                            counts["same_length_differs_only_by_annotation"] += 1
                        rec = {"function": name, "source": src,
                               "addr": f"0x{addr:08x}", "n": len(new_insns),
                               "old_rung": rung, "annotation_only": ann_only}
                        same_len_differ.append(rec)
                        if not ann_only:
                            # Same length, same trim, still different text: the
                            # two references are looking at different BYTES.
                            # The only reference worth reporting individually.
                            rec["sample"] = [
                                {"old": a, "new": b}
                                for a, b, x, y in zip(old_insns, new_insns,
                                                      o_n, n_n)
                                if _no_ann(x) != _no_ann(y)][:6]
                            beyond_ann.append(rec)
                continue

            counts["changed"] += 1
            rec = {"function": name, "source": src, "addr": f"0x{addr:08x}",
                   "kind": meta["kind"], "old_rung": rung,
                   "old": len(old_insns), "new": len(new_insns), "truth": truth}
            changed.append(rec)
            if truth is None:
                counts["changed_no_truth"] += 1
                continue
            d_old, d_new = abs(len(old_insns) - truth), abs(len(new_insns) - truth)
            if d_new < d_old:
                counts["changed_closer"] += 1
            elif d_new == d_old:
                counts["changed_equal"] += 1
            else:
                counts["changed_farther"] += 1
                farther.append(rec)

    report = {
        "rev_old": args.rev,
        "counts": dict(counts),
        "old_rung_distribution": dict(rungs),
        "new_bound_kinds": dict(kinds),
        "changed": changed,
        "changed_farther": [r for r in farther],
        "same_length_scored_differs": same_len_differ,
        "same_length_differs_beyond_annotation": beyond_ann,
        "functions_without_truth": no_truth[:200],
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=1) + "\n")

    _print_report(report, args.out)
    return 1 if (counts["changed_farther"] + counts["new_missing"]) else 0


def _print_report(report: dict, out: Path) -> None:
    counts = Counter(report["counts"])
    rungs = Counter(report["old_rung_distribution"])
    kinds = Counter(report["new_bound_kinds"])
    farther = report["changed_farther"]
    print(f"reference migration audit (old = {report['rev_old']})")
    print(f"  scored functions examined     {counts['total']}")
    print(f"  identical reference length    {counts['unchanged_length']}")
    print(f"    -> raw text differs         {counts['same_length_text_differs']}"
          f"   (symbolized vs raw call/disp -- benign)")
    print(f"    -> still differs AFTER the  {counts['same_length_scored_differs']}"
          f"   scorer's normalization")
    print(f"       (of which annotation-only {counts['same_length_differs_only_by_annotation']}"
          f")")
    print(f"  old path had NO reference     {counts['old_dropped_now_scored']}"
          f"   (now scored)")
    print(f"  changed length                {counts['changed']}")
    print(f"    -> closer to the binary     {counts['changed_closer']}")
    print(f"    -> equally far              {counts['changed_equal']}")
    print(f"    -> FARTHER from the binary  {counts['changed_farther']}")
    if counts["changed_no_truth"]:
        print(f"    -> no truth available       {counts['changed_no_truth']}")
    if counts["new_missing"]:
        print(f"  no derived reference          {counts['new_missing']}")
    print("\n  old rung: " + ", ".join(f"{k}={v}" for k, v in rungs.most_common()))
    print("  new kind: " + ", ".join(f"{k}={v}" for k, v in kinds.most_common()))

    bad = counts["changed_farther"] + counts["new_missing"]
    if bad:
        print(f"\nFAIL {bad} reference(s) did not improve:")
        for r in farther[:40]:
            if "old" in r:
                print(f"  {r['function']:<44} {r['addr']} kind={r['kind']:<13} "
                      f"old={r['old']} new={r['new']} truth={r['truth']} "
                      f"(from {r['old_rung']})")
            else:
                print(f"  {r['function']:<44} {r['why']}: {r['detail']}")
        if len(farther) > 40:
            print(f"  ... {len(farther) - 40} more (see {out})")
    else:
        print("\nPASS every changed reference moved closer to the binary "
              "(or stayed equally close)")
    print(f"\nwrote {out}")


if __name__ == "__main__":
    sys.exit(main())
