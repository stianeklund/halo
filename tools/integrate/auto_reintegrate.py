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
import re
import subprocess
import sys
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


def _absorb_readme_stats_refresh(main_wt: str) -> bool:
    """Commit a README-only, stats-only regen in the main worktree.

    Returns True only if it actually committed. Fails CLOSED: any other dirty
    path, any non-stats line in the README diff, or any git error leaves the
    tree untouched so the caller still parks.
    """
    st = git("status", "--porcelain", "--untracked-files=no", cwd=main_wt)
    lines = [ln for ln in st.stdout.splitlines() if ln.strip()]
    if len(lines) != 1:
        return False
    # NB: parse by token, not fixed columns -- the porcelain status code is
    # " M" with a LEADING space, which a .strip() on the line silently eats.
    code, _, path = lines[0].strip().partition(" ")
    if path.strip() != "README.md" or "M" not in code:
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
    if not git_ok("add", "--", "README.md", cwd=main_wt):
        return False
    return git_ok("commit", "--no-verify", "-m",
                  "chore: README progress stats refresh (auto, post-land)",
                  "--", "README.md", cwd=main_wt)


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

    # Gate 3 -- clean build (source<->kb.json interlock).
    cp = subprocess.run(
        [sys.executable, "tools/build/build.py", "-q", "--target", "halo"],
        capture_output=True, text=True)
    if cp.returncode != 0:
        detail["build_tail"] = (cp.stdout + cp.stderr).strip().splitlines()[-8:]
        return False, "build_failed", detail

    # Gate 4 -- no-drop proof (only meaningful after a rebase replayed commits).
    if rebased and backup:
        base = git("merge-base", "main", backup).stdout.strip()
        changed = git("diff", "--name-only", base, backup).stdout.splitlines()
        drifted = [f for f in changed
                   if not git_ok("diff", "--quiet", backup, branch, "--", f)]
        if drifted:
            detail["no_drop_drifted"] = drifted
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
        absorbed = _absorb_readme_stats_refresh(main_wt)
        if absorbed:
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
