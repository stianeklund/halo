#!/usr/bin/env python3
"""Mechanical, fail-closed branch -> main reintegration gate.

This is the safety core of the `auto-session` pipeline. It encodes the
`reintegrate-to-main` skill as executable code so the dangerous merge logic is
testable and deterministic instead of living in agent prose.

Landing policy: AUTO-FF, PARK-ON-CONFLICT.
  - Advance `main` (fast-forward only) *only* when the merge is conflict-free
    AND every gate is green.
  - On any real merge conflict (kb.json object conflict, source conflict) or a
    dirty/locked main worktree, PARK and surface to a human. Never hand-resolve
    kb.json -- a hand-rolled per-function merge once silently dropped 92
    functions (feedback_kb_json_branch_integration_recipe).

Exit codes:
  0  landed            -- main fast-forwarded to the branch tip (or, with
                          --dry-run, all gates passed and it *would* land).
  3  parked            -- actionable/expected: merge conflict or a failed gate.
                          main is untouched; a human must resolve.
  2  inconclusive      -- environment problem: on main, dirty/locked main
                          worktree, nothing to land, git error. main untouched.

Usage:
  auto_reintegrate.py --branch <branch> [--main-worktree auto|<path>]
                      [--dry-run] [--json]

The script operates on the current working directory as the *branch* worktree
(where the lift commits live) and on the resolved main worktree for the FF.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from collections import Counter

# Exit codes (also emitted as `status` in --json).
LANDED = 0
INCONCLUSIVE = 2
PARKED = 3
_STATUS = {LANDED: "landed", INCONCLUSIVE: "inconclusive", PARKED: "parked"}


def git(*args: str, cwd: str | None = None) -> subprocess.CompletedProcess:
    """Run a git command, capturing output. Never raises on non-zero."""
    return subprocess.run(
        ["git", *args],
        cwd=cwd,
        capture_output=True,
        text=True,
    )


def git_ok(*args: str, cwd: str | None = None) -> bool:
    return git(*args, cwd=cwd).returncode == 0


def _merge_driver(path: str) -> str | None:
    """Return the merge driver git resolves for `path`, or None if unset.

    `git check-attr merge -- <path>` prints "<path>: merge: <value>", where
    <value> is "unspecified" when no attribute applies. Used by the no-drop
    gate to recognise files whose post-rebase content is ALLOWED to differ
    from the pre-rebase branch (merge=ours generated output).
    """
    cp = git("check-attr", "merge", "--", path)
    if cp.returncode != 0:
        return None
    # Split from the right: the path itself may contain ": ".
    parts = cp.stdout.strip().rsplit(": ", 1)
    if len(parts) != 2:
        return None
    value = parts[1].strip()
    return None if value in ("unspecified", "unset") else value


def _matches_three_way(path: str, base: str, ours: str,
                       theirs: str, result: str) -> bool:
    """True iff `path` at `result` is exactly the 3-way merge of the others.

    Used by the no-drop gate. When `main` advances while a session branch is
    lifting, a rebase legitimately rewrites every file BOTH sides touched, so
    "content differs from the pre-rebase branch" cannot by itself mean work was
    dropped -- it is the expected outcome of a correct replay. What separates a
    sound replay from a mangled one is whether the bytes equal the merge git
    itself would compute from (base, ours, theirs). Anything dropped, doubled
    or reordered changes them, so the caller still fails closed.

    Conservative on every edge: a file missing on any side, a conflicting
    merge, or content that does not survive a text round-trip all return False
    and leave the path to be reported as a real drift.
    """
    blobs = {}
    for tag, rev in (("base", base), ("ours", ours),
                     ("theirs", theirs), ("result", result)):
        cp = git("show", f"{rev}:{path}")
        if cp.returncode != 0:
            return False
        blobs[tag] = cp.stdout

    with tempfile.TemporaryDirectory() as td:
        paths = {}
        for tag in ("base", "ours", "theirs"):
            paths[tag] = os.path.join(td, tag)
            with open(paths[tag], "w") as fh:
                fh.write(blobs[tag])
        # -p writes the merged result to stdout; non-zero means conflict.
        cp = git("merge-file", "-q", "-p",
                 paths["ours"], paths["base"], paths["theirs"])
        if cp.returncode != 0:
            return False
        return cp.stdout == blobs["result"]


def resolve_main_worktree() -> tuple[str | None, bool]:
    """Return (path_of_worktree_checked_out_on_main, is_locked).

    Parses `git worktree list --porcelain`; the main worktree is the entry
    whose `branch` is refs/heads/main.
    """
    cp = git("worktree", "list", "--porcelain")
    if cp.returncode != 0:
        return None, False
    path: str | None = None
    locked = False
    cur_path: str | None = None
    cur_locked = False
    for line in cp.stdout.splitlines() + [""]:
        if line.startswith("worktree "):
            cur_path = line[len("worktree "):].strip()
            cur_locked = False
        elif line.startswith("locked"):
            cur_locked = True
        elif line.startswith("branch "):
            ref = line[len("branch "):].strip()
            if ref == "refs/heads/main":
                path = cur_path
                locked = cur_locked
        elif line == "":
            cur_path = None
            cur_locked = False
    return path, locked


def merge_tree_conflicts(base: str, other: str) -> list[str] | None:
    """Preview a merge WITHOUT touching any tree.

    `git merge-tree --write-tree --name-only <base> <other>` prints a tree OID
    on line 1; any paths after it are conflicting files. Returns the conflict
    list ([] when clean), or None if the git version is too old for the flag.
    """
    cp = git("merge-tree", "--write-tree", "--name-only", base, other)
    if cp.returncode == 0:
        # Clean merge: stdout is just the tree OID (possibly with a trailing
        # newline). No conflicts.
        return []
    # Non-zero return means conflicts (git >= 2.38): line 1 = tree OID,
    # remaining lines = conflicting paths. Older git lacks --write-tree and
    # errors on stderr instead -> signal "unknown" with None.
    lines = [l for l in cp.stdout.splitlines() if l.strip()]
    if not lines and cp.stderr.strip():
        return None
    return lines[1:] if len(lines) > 1 else lines


def result(code: int, reason: str, *, as_json: bool, **extra) -> int:
    payload = {"status": _STATUS[code], "reason": reason, **extra}
    if as_json:
        print(json.dumps(payload))
    else:
        print(f"[{_STATUS[code].upper()}] {reason}")
        for k, v in extra.items():
            print(f"  {k}: {v}")
    return code


# ---------------------------------------------------------------------------
# Gates (reintegrate-to-main SKILL.md Steps 4-5)
# ---------------------------------------------------------------------------

def _kb_stats(ref: str | None) -> tuple[int, "Counter"] | None:
    """Return (object_count, Counter of function addrs) for a kb.json.

    ref=None reads the working-tree file (so uncommitted state is caught);
    otherwise reads `git show <ref>:kb.json`.
    """
    try:
        if ref is None:
            with open("kb.json", "r") as fh:
                data = json.load(fh)
        else:
            cp = git("show", f"{ref}:kb.json")
            if cp.returncode != 0:
                return None
            data = json.loads(cp.stdout)
    except Exception:
        return None
    addrs = Counter(
        fn.get("addr")
        for obj in data.get("objects", [])
        for fn in obj.get("functions", [])
        if fn.get("addr") is not None
    )
    return len(data.get("objects", [])), addrs


def _dup_addrs(counter: "Counter") -> dict:
    """{addr: count} for every addr appearing more than once."""
    return {a: c for a, c in counter.items() if c > 1}


# Markers for the generated progress block in README.md. A changed line must
# match one of these to count as "stats only"; anything else means a human
# edited the README and we must NOT auto-commit it.
_README_STAT_MARKERS = (
    "Ported Functions:",
    "Ported Code Bytes:",
    "Average VC71 Match Accuracy:",
    "Equivalence Verified:",
    "img.shields.io/badge/decompilation-",
    "img.shields.io/badge/functions-",
)
# The progress bar line, e.g. "  `[███░░░] 56.82%`". Keyed off the block glyphs
# rather than exact punctuation, which has changed shape before.
_README_BAR_RE = re.compile(r"^[+-][^█░]*[█░]+[^█░]*$")


def _readme_stats_only_dirt(main_wt: str, sole: bool = True) -> bool:
    """True when README's dirt in main is a stats-only regen.

    With sole=True (default) README must additionally be the ONLY dirty tracked
    path -- the contract _settle_readme_regen relies on, since it DISCARDS the
    file and must not discard anything sitting beside it.

    With sole=False the caller has already established that every other dirty
    path is absorbable generated output (see _absorbable_paths), so only the
    README content check applies.

    Fails CLOSED: any unexpected dirty path, any non-stats line in the README
    diff, or any git error returns False so the caller parks.
    """
    st = git("status", "--porcelain", "--untracked-files=no", cwd=main_wt)
    if st.returncode != 0:
        return False
    lines = [ln for ln in st.stdout.splitlines() if ln.strip()]
    if sole and len(lines) != 1:
        return False
    # NB: parse by token, not fixed columns -- the porcelain status code is
    # " M" with a LEADING space, which a .strip() on the line silently eats.
    entries = [ln.strip().partition(" ") for ln in lines]
    readme = [(c, p.strip()) for c, _, p in entries if p.strip() == "README.md"]
    if not readme or "M" not in readme[0][0]:
        return False
    d = git("diff", "--unified=0", "--", "README.md", cwd=main_wt)
    if d.returncode != 0 or not d.stdout.strip():
        return False
    changed = [ln for ln in d.stdout.splitlines()
               if ln[:1] in "+-" and not ln.startswith(("+++", "---"))]
    if not changed:
        return False
    for ln in changed:
        if _README_BAR_RE.match(ln):
            continue
        if not any(m in ln for m in _README_STAT_MARKERS):
            return False
    return True


# Generated artifacts that background lanes rewrite into the main worktree and
# that may therefore be absorbed (committed) rather than parked on.
#
# README.md is handled separately because it gets a CONTENT check
# (_readme_stats_only_dirt): it is prose plus a stats block, so only the stats
# lines may drift. The paths below are wholly machine-generated -- a verify or
# batch-equivalence lane owns every byte -- so path identity is the check.
#
# Keep this list minimal and generated-only. Adding a hand-edited path here
# would let this function silently commit someone's in-progress work.
_ABSORB_EXACT = frozenset({
    "README.md",
    "tools/verify/vc71_scores.json",
    "tools/objects.csv",
    "tools/equivalence/leaf_cache.json",
})
_ABSORB_PREFIXES = ("artifacts/batch_verify/",)


def _absorbable_paths(main_wt: str) -> list[str] | None:
    """Dirty tracked paths in main, iff ALL of them are absorbable generated output.

    Returns the path list, or None when anything else is dirty (caller parks).
    Fails CLOSED on any git error or unrecognised path.
    """
    st = git("status", "--porcelain", "--untracked-files=no", cwd=main_wt)
    if st.returncode != 0:
        return None
    paths = []
    for ln in st.stdout.splitlines():
        if not ln.strip():
            continue
        # NB: parse by token, not fixed columns -- the porcelain status code is
        # " M" with a LEADING space, which a .strip() on the line silently eats.
        code, _, path = ln.strip().partition(" ")
        path = path.strip()
        if "M" not in code:          # added/deleted/renamed = real work, park
            return None
        if path in _ABSORB_EXACT or path.startswith(_ABSORB_PREFIXES):
            paths.append(path)
        else:
            return None
    if not paths:
        return None
    # README earns its absorb only if the diff is stats-only, exactly as before.
    if "README.md" in paths and not _readme_stats_only_dirt(main_wt, sole=False):
        return None
    return paths


def _absorb_readme_stats_refresh(main_wt: str) -> bool:
    """Commit a generated-artifact-only refresh in the main worktree.

    Returns True only if it actually committed. Fails CLOSED: any dirty path
    that is not known generated output, any non-stats line in the README diff,
    or any git error leaves the tree untouched so the caller still parks.

    Originally README-only. Widened 2026-08-01: the same self-inflicted loop
    was measured on tools/verify/vc71_scores.json and artifacts/batch_verify/*,
    which the verify lanes regenerate into main. Those accounted for 4 of 8
    parked land attempts in the auto-session run -- all of them
    `main_worktree_dirty` over files no human had touched.

    NB: committing here fires the repo's post-commit dashboard hook, which
    regenerates README in the background and re-dirties it moments later. The
    caller must therefore re-check with _settle_readme_regen(), not assume the
    tree stays clean.
    """
    paths = _absorbable_paths(main_wt)
    if not paths:
        return False
    if not git_ok("add", "--", *paths, cwd=main_wt):
        return False
    return git_ok("commit", "--no-verify", "-m",
                  "chore: regenerated artifacts refresh (auto, post-land)",
                  "--", *paths, cwd=main_wt)


def _settle_readme_regen(main_wt: str) -> bool:
    """Discard a post-absorb background README regen so main is truly clean.

    The post-commit dashboard hook re-writes README asynchronously after the
    absorb commit, so the tree flaps dirty again with a fresh set of generated
    numbers. README is a generated file and the next regen rewrites it anyway,
    so discarding that drift loses nothing -- and leaving it dirty would make
    the fast-forward refuse to touch the worktree.

    Returns True only if the tree ends up clean of tracked changes. Fails
    CLOSED: a README diff that is not stats-only is left alone so we park.
    """
    if not _readme_stats_only_dirt(main_wt):
        return False
    if not git_ok("checkout", "--", "README.md", cwd=main_wt):
        return False
    st = git("status", "--porcelain", "--untracked-files=no", cwd=main_wt)
    return st.returncode == 0 and not st.stdout.strip()


def run_gates(main_wt: str, *, rebased: bool, backup: str | None,
              branch: str) -> tuple[bool, str, dict]:
    """Run the four reintegration gates. Return (ok, fail_reason, detail)."""
    detail: dict = {}

    # Gate 1 -- kb.json object partition, checked RELATIVE to main (main's
    # kb.json already carries some pre-existing duplicate addrs from its
    # duplicated objects, so an absolute "0 dups" invariant is wrong). We park
    # only on the corruption DIRECTIONS the 92-drop taught us:
    #   * the branch DROPPED objects (branch object count < main's), or
    #   * the branch INTRODUCED new duplicate addrs (a dup whose count exceeds
    #     main's count for that addr) -- the ~530-dup signature.
    # NB: do NOT bind these to `branch`/`main` -- `branch` is this function's
    # str parameter, and shadowing it with a tuple crashed the no-drop gate
    # below (TypeError deep inside subprocess, after the rebase had run).
    branch_kb = _kb_stats(None)
    main_kb = _kb_stats("main")
    if branch_kb is None or main_kb is None:
        return False, "kb_partition_unreadable", detail
    branch_objs, branch_addrs = branch_kb
    main_objs, main_addrs = main_kb
    detail["kb_objects_branch"] = branch_objs
    detail["kb_objects_main"] = main_objs
    # New/worsened dups = addrs duplicated on the branch beyond main's baseline.
    new_dups = {
        a: c for a, c in _dup_addrs(branch_addrs).items()
        if c > main_addrs.get(a, 0)
    }
    detail["kb_preexisting_dups"] = len(_dup_addrs(main_addrs))
    detail["kb_new_dups"] = {hex(a) if isinstance(a, int) else a: c
                             for a, c in new_dups.items()}
    if branch_objs < main_objs:
        return False, f"kb_objects_dropped({main_objs}->{branch_objs})", detail
    if new_dups:
        return False, f"kb_new_duplicate_addrs={len(new_dups)}", detail

    # Gate 2 -- reg-baseline drift (@<reg> ABI immutability).
    cp = subprocess.run(
        [sys.executable, "tools/audit/extract_reg_args.py", "--check"],
        capture_output=True, text=True)
    if cp.returncode != 0:
        detail["reg_drift"] = (cp.stdout + cp.stderr).strip().splitlines()[-5:]
        return False, "reg_baseline_drift", detail

    # Gate 3 -- clean build (source<->kb.json interlock) AND linkage proof.
    #
    # Build `patched_xbe`, NOT `halo`. `halo` only links the ELF; the check that
    # a `ported=true` function actually reaches the XBE lives in patch.py
    # (`missing_exports`, "symbol absent from EXE exports"), and patch.py runs
    # only under the patched_xbe target. Building `halo` here exits 0 whether or
    # not a translation unit is registered in CMakeLists.txt, so a batch that
    # adds a new TU and forgets to stage the CMakeLists entry lands a function
    # marked ported whose body is never linked -- the original Xbox code keeps
    # running, VC71 still scores the source, and nothing fails until someone
    # notices a behaviour regression months later. Observed twice in the
    # 2026-08-01 auto-session run (rasterizer_xbox_vertex_shaders_*.c and
    # rasterizer_xbox_widgets.c); both were caught only incidentally, by the
    # unrelated file-drift check in Gate 4.
    cp = subprocess.run(
        [sys.executable, "tools/build/build.py", "-q", "--target", "patched_xbe"],
        capture_output=True, text=True)
    if cp.returncode != 0:
        out = (cp.stdout + cp.stderr).strip()
        detail["build_tail"] = out.splitlines()[-8:]
        if "absent from EXE exports" in out:
            missing = [ln.strip() for ln in out.splitlines()
                       if "absent from EXE exports" in ln]
            detail["missing_exports"] = missing[:10]
            return False, f"unlinked_ported_symbols={len(missing)}", detail
        return False, "build_failed", detail

    # Gate 4 -- no-drop proof (only meaningful after a rebase replayed commits).
    #
    # The gate exists to prove the rebase did not silently drop or mangle real
    # work. A file declared `merge=ours` in .gitattributes is the one case where
    # post-rebase content differing from the pre-rebase branch is CORRECT rather
    # than suspicious: that driver deliberately keeps the target's copy, because
    # the file is generated output several lanes rewrite independently
    # (README.md's progress block). Policing it here re-blocks exactly what the
    # driver was added to unblock -- observed as
    # gate_failed:rebase_dropped_or_changed_files / no_drop_drifted=['README.md']
    # after all 6 land attempts of a 24-function session had already failed on
    # the dirty-worktree check.
    #
    # The exemption is derived from git's own attribute lookup rather than a
    # hardcoded filename list, so it cannot fall out of sync with the driver
    # config: a path is exempt if and only if git says its merge driver is
    # `ours`. Adding merge=ours to a source file would exempt it too, which is
    # why that attribute should stay restricted to generated output.
    if rebased and backup:
        base = git("merge-base", "main", backup).stdout.strip()
        changed = git("diff", "--name-only", base, backup).stdout.splitlines()
        drifted = [f for f in changed
                   if not git_ok("diff", "--quiet", backup, branch, "--", f)]
        if drifted:
            exempt = [f for f in drifted if _merge_driver(f) == "ours"]
            # Second, narrower exemption: a file both sides legitimately
            # changed. `main` is shared, so another lane can land while this
            # session lifts; the rebase then MUST rewrite every file touched on
            # both sides, and flagging that as a drop parks a sound branch.
            # Exempt only when the result is byte-identical to the 3-way merge
            # git would compute -- proof the replay preserved both sides rather
            # than an assumption that it did.
            merged = [f for f in drifted
                      if f not in exempt
                      and _matches_three_way(f, base, backup, "main", branch)]
            real = [f for f in drifted
                    if f not in exempt and f not in merged]
            if exempt:
                detail["no_drop_exempt_merge_ours"] = exempt
            if merged:
                detail["no_drop_clean_three_way"] = merged
            if real:
                detail["no_drop_drifted"] = real
                return False, "rebase_dropped_or_changed_files", detail

    return True, "", detail


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--branch", required=True, help="branch to land into main")
    ap.add_argument("--main-worktree", default="auto",
                    help="path of the worktree on main, or 'auto' to resolve")
    ap.add_argument("--dry-run", action="store_true",
                    help="run all gates but never advance main")
    ap.add_argument("--json", action="store_true", help="emit a JSON result")
    a = ap.parse_args()
    J = a.json
    branch = a.branch

    # --- Step 1: topology + guards (read-only) --------------------------------
    if branch == "main":
        return result(INCONCLUSIVE, "refuse_to_land_main_into_main", as_json=J)

    if a.main_worktree == "auto":
        main_wt, locked = resolve_main_worktree()
    else:
        main_wt, locked = a.main_worktree, False
    if not main_wt:
        return result(INCONCLUSIVE, "main_worktree_not_found", as_json=J)
    if locked:
        return result(INCONCLUSIVE, "main_worktree_locked",
                      as_json=J, main_worktree=main_wt)

    # main worktree must be clean of TRACKED changes (untracked is fine).
    st = git("status", "--porcelain", "--untracked-files=no", cwd=main_wt)
    if st.returncode != 0:
        return result(INCONCLUSIVE, "main_worktree_status_error", as_json=J)
    if st.stdout.strip():
        # Every successful land changes the ported-function count, so the
        # dashboard's README stats go stale and a background regen rewrites
        # them in main -- dirtying it and parking the NEXT land. That is a
        # self-inflicted loop over a generated file, so absorb it: commit a
        # README-only, stats-only refresh and carry on. Anything else still
        # parks.
        # --dry-run must not write to main. Absorbing COMMITS to the main
        # worktree, so under dry-run just report what it would have absorbed.
        if a.dry_run:
            if _absorbable_paths(main_wt):
                st.stdout = ""          # would be clean after the absorb
            absorbed = False
        else:
            absorbed = _absorb_readme_stats_refresh(main_wt)
        if absorbed:
            st = git("status", "--porcelain", "--untracked-files=no",
                     cwd=main_wt)
            # The absorb commit itself fires the post-commit dashboard hook,
            # which regenerates README in the background. Without settling
            # that, the re-check below sees the flap and parks -- the exact
            # loop the absorb exists to break.
            if st.stdout.strip() and _settle_readme_regen(main_wt):
                st = git("status", "--porcelain", "--untracked-files=no",
                         cwd=main_wt)
    if st.stdout.strip():
        return result(INCONCLUSIVE, "main_worktree_dirty", as_json=J,
                      main_worktree=main_wt,
                      tracked_changes=st.stdout.strip().splitlines()[:10])

    if not git_ok("rev-parse", "--verify", branch):
        return result(INCONCLUSIVE, "branch_not_found", as_json=J, branch=branch)

    # --- Step 2: divergence + conflict preview (no tree touched) -------------
    lr = git("rev-list", "--left-right", "--count", f"main...{branch}")
    if lr.returncode != 0:
        return result(INCONCLUSIVE, "rev_list_failed", as_json=J)
    main_only, branch_only = (int(x) for x in lr.stdout.split())
    if branch_only == 0:
        return result(INCONCLUSIVE, "branch_not_ahead_of_main", as_json=J,
                      main_only=main_only, branch_only=branch_only)

    is_ancestor = git_ok("merge-base", "--is-ancestor", "main", branch)

    conflicts = merge_tree_conflicts("main", branch)
    if conflicts is None:
        return result(INCONCLUSIVE, "merge_tree_unsupported_git", as_json=J)
    if conflicts:
        return result(PARKED, "merge_conflict", as_json=J, conflicts=conflicts,
                      main_only=main_only, branch_only=branch_only)

    # --- Step 3: rebase if diverged (conflict-free only) ---------------------
    need_rebase = main_only > 0 and not is_ancestor
    backup = f"backup/{branch}-pre-reintegrate"
    if need_rebase and not a.dry_run:
        git("branch", "-f", backup, branch)  # cheap undo point
        rb = git("-c", "core.hooksPath=/dev/null", "rebase", "main")
        if rb.returncode != 0:
            git("-c", "core.hooksPath=/dev/null", "rebase", "--abort")
            return result(PARKED, "rebase_conflict", as_json=J,
                          detail=rb.stderr.strip().splitlines()[-5:])
        is_ancestor = git_ok("merge-base", "--is-ancestor", "main", branch)

    # --- Step 4/5: gates -----------------------------------------------------
    ok, fail, detail = run_gates(
        main_wt, rebased=(need_rebase and not a.dry_run),
        backup=backup, branch=branch)
    if not ok:
        return result(PARKED, f"gate_failed:{fail}", as_json=J, gate=detail)

    # --- Step 6: advance (FF-only) -------------------------------------------
    if a.dry_run:
        return result(LANDED, "dry_run_would_land", as_json=J,
                      would_rebase=need_rebase, gate=detail,
                      main_only=main_only, branch_only=branch_only)

    if not git_ok("merge-base", "--is-ancestor", "main", branch):
        return result(PARKED, "not_ff_after_gates", as_json=J)

    main_old = git("rev-parse", "main").stdout.strip()
    ff = git("merge", "--ff-only", branch, cwd=main_wt)
    if ff.returncode != 0:
        return result(PARKED, "ff_merge_failed", as_json=J,
                      detail=ff.stderr.strip().splitlines()[-5:])
    main_new = git("rev-parse", "main").stdout.strip()
    return result(LANDED, "fast_forwarded", as_json=J,
                  main_old=main_old, main_new=main_new,
                  functions=branch_only, main_worktree=main_wt)


if __name__ == "__main__":
    sys.exit(main())
