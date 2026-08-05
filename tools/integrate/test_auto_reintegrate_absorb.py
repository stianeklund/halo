#!/usr/bin/env python3
"""Self-test for auto_reintegrate's generated-artifact absorb logic.

The absorb decides whether a dirty `main` worktree is a self-inflicted
regeneration (commit it and carry on) or real human work (park). Both
directions matter:

  - too strict  -> every land parks on a file no human touched. Measured: 4 of
                   8 land attempts in the 2026-08-01 auto-session run.
  - too lax     -> the lander COMMITS someone's in-progress work in a worktree
                   another session is using. That is unrecoverable-looking and
                   far worse than parking.

So this pins the fail-CLOSED boundary: only known generated paths, only
modifications, and README only when its diff is stats-only.

Run: python3 tools/integrate/test_auto_reintegrate_absorb.py
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import auto_reintegrate as ar  # noqa: E402

FAILURES = []


def check(name, got, want):
    if got != want:
        FAILURES.append(f"{name}: got {got!r}, want {want!r}")
        print(f"FAIL {name}: got {got!r}, want {want!r}")
    else:
        print(f"PASS {name}")


def sh(*args, cwd):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True)


def make_repo(tmp):
    sh("git", "init", "-q", "-b", "main", cwd=tmp)
    sh("git", "config", "user.email", "t@t", cwd=tmp)
    sh("git", "config", "user.name", "t", cwd=tmp)
    # A README whose stats block matches the real one's shape.
    (tmp / "README.md").write_text(
        "# Halo\n\nProse that must never drift.\n\n"
        "Ported Functions: 4000 / 8143 (49.1%)\n"
        "[████████░░░░░░░░]\n")
    for p in ("tools/verify", "tools/equivalence", "artifacts/batch_verify"):
        (tmp / p).mkdir(parents=True, exist_ok=True)
    (tmp / "tools/verify/vc71_scores.json").write_text('{"scores": {}}\n')
    (tmp / "tools/objects.csv").write_text("Object,Delink?,addr_range,func_count\n")
    (tmp / "tools/equivalence/leaf_cache.json").write_text("{}\n")
    (tmp / "artifacts/batch_verify/summary.json").write_text("{}\n")
    (tmp / "src").mkdir(exist_ok=True)
    (tmp / "src/units.c").write_text("int f(void){return 0;}\n")
    sh("git", "add", "-A", cwd=tmp)
    sh("git", "commit", "-q", "--no-verify", "-m", "init", cwd=tmp)
    return tmp


def dirty(tmp, path, text):
    (tmp / path).write_text(text)


def main():
    with tempfile.TemporaryDirectory() as td:
        tmp = make_repo(Path(td))
        wt = str(tmp)

        # --- clean tree: nothing to absorb -------------------------------
        check("clean tree -> no absorb", ar._absorbable_paths(wt), None)

        # --- stats-only README regen: the original case ------------------
        dirty(tmp, "README.md",
              "# Halo\n\nProse that must never drift.\n\n"
              "Ported Functions: 4031 / 8143 (49.5%)\n"
              "[█████████░░░░░░░]\n")
        check("README stats-only -> absorbable",
              ar._absorbable_paths(wt), ["README.md"])
        sh("git", "checkout", "--", "README.md", cwd=tmp)

        # --- README PROSE edit must NOT be absorbed (it is human work) ---
        dirty(tmp, "README.md",
              "# Halo\n\nProse that WAS edited by a person.\n\n"
              "Ported Functions: 4000 / 8143 (49.1%)\n"
              "[████████░░░░░░░░]\n")
        check("README prose edit -> park",
              ar._absorbable_paths(wt), None)
        sh("git", "checkout", "--", "README.md", cwd=tmp)

        # --- the newly-absorbed generated artifacts ----------------------
        dirty(tmp, "tools/verify/vc71_scores.json", '{"scores": {"f": 1}}\n')
        check("vc71_scores regen -> absorbable",
              ar._absorbable_paths(wt), ["tools/verify/vc71_scores.json"])

        dirty(tmp, "artifacts/batch_verify/summary.json", '{"n": 1}\n')
        got = ar._absorbable_paths(wt)
        check("vc71_scores + batch_verify -> both absorbable",
              got and sorted(got),
              ["artifacts/batch_verify/summary.json",
               "tools/verify/vc71_scores.json"])

        # --- a source file alongside generated output MUST park ----------
        dirty(tmp, "src/units.c", "int f(void){return 1;}\n")
        check("source file present -> park (fails closed)",
              ar._absorbable_paths(wt), None)
        sh("git", "checkout", "--", "src/units.c", cwd=tmp)

        # --- an ADDED generated file still parks (not a regen) -----------
        sh("git", "checkout", "--", ".", cwd=tmp)
        (tmp / "tools/verify/new_thing.json").write_text("{}\n")
        sh("git", "add", "tools/verify/new_thing.json", cwd=tmp)
        check("added file -> park", ar._absorbable_paths(wt), None)
        sh("git", "rm", "-q", "-f", "--cached", "tools/verify/new_thing.json",
           cwd=tmp)
        (tmp / "tools/verify/new_thing.json").unlink()

        # --- a DELETED tracked file parks --------------------------------
        (tmp / "tools/objects.csv").unlink()
        check("deleted file -> park", ar._absorbable_paths(wt), None)
        sh("git", "checkout", "--", "tools/objects.csv", cwd=tmp)

        # --- end-to-end: absorb actually commits and cleans the tree -----
        dirty(tmp, "tools/verify/vc71_scores.json", '{"scores": {"g": 2}}\n')
        dirty(tmp, "tools/objects.csv",
              "Object,Delink?,addr_range,func_count\nfoo.obj,true,0x1-0x2,1\n")
        check("absorb commits", ar._absorb_readme_stats_refresh(wt), True)
        st = sh("git", "status", "--porcelain", "--untracked-files=no", cwd=tmp)
        check("tree clean after absorb", st.stdout.strip(), "")

        # --- absorb refuses when real work is present --------------------
        dirty(tmp, "src/units.c", "int f(void){return 2;}\n")
        check("absorb refuses real work",
              ar._absorb_readme_stats_refresh(wt), False)
        st = sh("git", "status", "--porcelain", "--untracked-files=no", cwd=tmp)
        check("real work left untouched", "src/units.c" in st.stdout, True)

        # --- _settle_readme_regen keeps its stricter sole-file contract --
        # It DISCARDS README, so it must refuse while anything else is dirty.
        check("settle refuses with other dirt present",
              ar._settle_readme_regen(wt), False)

    # --- no-drop gate: 3-way-merge exemption -----------------------------
    # `main` is shared, so another lane can land while a session branch
    # lifts. The rebase then MUST rewrite every file touched on both sides.
    # Exempting that is only safe if it is proven, not assumed, so pin both
    # directions -- a happy-path-only test would let a real drop through.
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        sh("git", "init", "-q", "-b", "main", cwd=tmp)
        sh("git", "config", "user.email", "t@t", cwd=tmp)
        sh("git", "config", "user.name", "t", cwd=tmp)
        (tmp / "f.c").write_text("a\nb\nc\nd\ne\n")
        sh("git", "add", "-A", cwd=tmp)
        sh("git", "commit", "-q", "--no-verify", "-m", "base", cwd=tmp)
        base = sh("git", "rev-parse", "HEAD", cwd=tmp).stdout.strip()

        # main edits the FIRST line; the branch edits the LAST -> disjoint.
        (tmp / "f.c").write_text("a-main\nb\nc\nd\ne\n")
        sh("git", "commit", "-qa", "--no-verify", "-m", "main edit", cwd=tmp)

        sh("git", "checkout", "-q", "-b", "topic", base, cwd=tmp)
        (tmp / "f.c").write_text("a\nb\nc\nd\ne-branch\n")
        sh("git", "commit", "-qa", "--no-verify", "-m", "branch edit", cwd=tmp)
        ours = sh("git", "rev-parse", "HEAD", cwd=tmp).stdout.strip()

        cwd0 = os.getcwd()
        os.chdir(tmp)
        try:
            # A faithful replay keeps BOTH edits -> exempt.
            sh("git", "checkout", "-q", "-b", "good", "main", cwd=tmp)
            (tmp / "f.c").write_text("a-main\nb\nc\nd\ne-branch\n")
            sh("git", "commit", "-qa", "--no-verify", "-m", "replay", cwd=tmp)
            check("both-sides file, faithful replay -> exempt",
                  ar._matches_three_way("f.c", base, ours, "main", "good"),
                  True)

            # A replay that silently loses the branch's edit must NOT be
            # exempt -- this is the drop the gate exists to catch.
            sh("git", "checkout", "-q", "-b", "dropped", "main", cwd=tmp)
            (tmp / "f.c").write_text("a-main\nb\nc\nd\ne\n")
            sh("git", "commit", "-qa", "--no-verify", "-m", "drop", cwd=tmp)
            check("replay that drops branch edit -> NOT exempt",
                  ar._matches_three_way("f.c", base, ours, "main", "dropped"),
                  False)

            # Nor may extra content sneak in under the exemption.
            sh("git", "checkout", "-q", "-b", "extra", "main", cwd=tmp)
            (tmp / "f.c").write_text("a-main\nb\nc\nd\ne-branch\nINJECTED\n")
            sh("git", "commit", "-qa", "--no-verify", "-m", "extra", cwd=tmp)
            check("replay with injected content -> NOT exempt",
                  ar._matches_three_way("f.c", base, ours, "main", "extra"),
                  False)

            # A path absent on a side is not this case; fail closed.
            check("missing path -> NOT exempt",
                  ar._matches_three_way("nope.c", base, ours, "main", "good"),
                  False)
        finally:
            os.chdir(cwd0)

    print()
    if FAILURES:
        print(f"{len(FAILURES)} FAILURE(S)")
        return 1
    print("all absorb self-tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
