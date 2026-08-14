#!/usr/bin/env python3
"""End-to-end test for auto_reintegrate's derived-JSON-cache auto-merge.

Two consecutive props.obj sessions (2026-08-14) parked with `merge_conflict`
whose only conflicting path was a generated per-key cache -- both sides append
to `tools/verify/vc71_scores.json` on every land, so it collides every time
while the actual lift work is sound. The lander now resolves those per key via
merge_derived_json.py.

The risk of that convenience is a silent drop: resolving a cache with
--ours/--theirs looks clean and loses the other side's entries. So this pins
both directions on real git repositories with real rebases:

  - derived-cache-only conflict  -> LANDS, and the merged cache contains BOTH
                                    sides' entries (the actual failure mode).
  - source-file conflict         -> still PARKS, main untouched.
  - kb.json                      -> never auto-merged, whatever it conflicts on.
  - resolver drops a key         -> union proof catches it and parks.

Run: python3 tools/integrate/test_auto_reintegrate_derived.py
"""
import json
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


def sh(*args, cwd, env=None):
    e = dict(os.environ)
    e.update({"GIT_AUTHOR_NAME": "t", "GIT_AUTHOR_EMAIL": "t@t",
              "GIT_COMMITTER_NAME": "t", "GIT_COMMITTER_EMAIL": "t@t"})
    if env:
        e.update(env)
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, env=e)


def commit(repo, msg):
    sh("git", "add", "-A", cwd=repo)
    sh("git", "-c", "core.hooksPath=/dev/null", "commit", "-q",
       "--no-verify", "-m", msg, cwd=repo)


def scores(entries):
    return json.dumps({"version": 2, "scores": entries}, indent=2) + "\n"


def make_repo(tmp):
    """main worktree + a linked session worktree, mirroring the real layout.

    This separation is load-bearing, not cosmetic: the lander runs from the
    SESSION worktree and fast-forwards main in ITS worktree. Running both from
    one checkout makes `git rebase main` a no-op and the branch never advances.
    """
    repo = tmp / "repo"
    repo.mkdir()
    sh("git", "init", "-q", "-b", "main", cwd=repo)
    sh("git", "config", "user.email", "t@t", cwd=repo)
    sh("git", "config", "user.name", "t", cwd=repo)
    (repo / "tools" / "verify").mkdir(parents=True)
    (repo / "src").mkdir()
    # Stand-ins for the two gate tools the lander shells out to. The gates
    # themselves are covered elsewhere; what is under test here is the conflict
    # path, so these just have to exist and succeed.
    (repo / "tools" / "audit").mkdir(parents=True)
    (repo / "tools" / "build").mkdir(parents=True)
    (repo / "tools/audit/extract_reg_args.py").write_text(
        'print("Check results: 0 OK, 0 drift, 0 missing, 0 stale")\n')
    (repo / "tools/build/build.py").write_text('print("ok")\n')
    (repo / "kb.json").write_text(json.dumps({"objects": []}, indent=1) + "\n")
    (repo / "src" / "a.c").write_text("int a(void){return 0;}\n")
    (repo / "tools/verify/vc71_scores.json").write_text(
        scores({"base_fn": {"match": 100.0}}))
    commit(repo, "base")
    sh("git", "branch", "session", cwd=repo)
    sess = tmp / "session_wt"
    sh("git", "worktree", "add", "-q", str(sess), "session", cwd=repo)
    return repo, sess


def append_score(repo, name, value):
    p = repo / "tools/verify/vc71_scores.json"
    d = json.loads(p.read_text())
    d["scores"][name] = {"match": value}
    p.write_text(json.dumps(d, indent=2) + "\n")


def run_lander(repo, sess, branch="session"):
    """Run the lander the way it really runs: from the session worktree,
    fast-forwarding main in the separate worktree that has main checked out."""
    cp = subprocess.run(
        [sys.executable, str(Path(ar.__file__).resolve()),
         "--branch", branch, "--main-worktree", str(repo), "--json"],
        cwd=sess, capture_output=True, text=True)
    try:
        return json.loads(cp.stdout.strip().splitlines()[-1]), cp
    except (ValueError, IndexError):
        return {"status": "unparseable", "stdout": cp.stdout,
                "stderr": cp.stderr}, cp


# --- Unit-level: the union proof and the path filter ------------------------

def test_conflict_path_filter():
    lines = ["tools/verify/vc71_scores.json",
             "Auto-merging kb.json",
             "CONFLICT (content): Merge conflict in tools/verify/vc71_scores.json"]
    check("conflict_paths strips merge-tree messages",
          ar._conflict_paths(lines), ["tools/verify/vc71_scores.json"])


def test_kb_json_never_auto_merged():
    check("kb.json not in derived set",
          "kb.json" in ar._DERIVED_JSON_CACHES, False)


def test_key_paths_nested():
    got = ar._json_key_paths({"scores": {"f": {"match": 1}}})
    check("nested key paths",
          got, {("scores",), ("scores", "f"), ("scores", "f", "match")})


# --- End-to-end: real repos, real rebases -----------------------------------

def test_derived_conflict_lands_keeping_both_sides(tmp):
    repo, sess = make_repo(tmp)
    # main appends one entry; the session branch appends a different one.
    append_score(repo, "main_fn", 91.0)
    commit(repo, "main: score for main_fn")
    append_score(sess, "session_fn", 99.0)
    (sess / "src" / "b.c").write_text("int b(void){return 1;}\n")
    commit(sess, "session: lift b")

    res, cp = run_lander(repo, sess)
    check("derived-only conflict lands", res.get("status"), "landed")

    merged = json.loads((repo / "tools/verify/vc71_scores.json").read_text())
    # THE failure mode: --ours/--theirs would silently lose one of these.
    check("main's entry survived", "main_fn" in merged["scores"], True)
    check("session's entry survived", "session_fn" in merged["scores"], True)
    check("base entry survived", "base_fn" in merged["scores"], True)
    tip = sh("git", "log", "-1", "--format=%s", cwd=repo).stdout.strip()
    check("main advanced to the session commit", tip, "session: lift b")


def test_source_conflict_still_parks(tmp):
    repo, sess = make_repo(tmp)
    (repo / "src" / "a.c").write_text("int a(void){return 111;}\n")
    commit(repo, "main: edit a")
    (sess / "src" / "a.c").write_text("int a(void){return 222;}\n")
    commit(sess, "session: edit a differently")
    before = sh("git", "rev-parse", "main", cwd=repo).stdout.strip()

    res, cp = run_lander(repo, sess)
    check("source conflict parks", res.get("status"), "parked")
    check("parks with a real conflict path",
          "src/a.c" in (res.get("conflicts") or []), True)
    after = sh("git", "rev-parse", "main", cwd=repo).stdout.strip()
    check("main untouched on park", after, before)


def test_kb_json_conflict_parks(tmp):
    repo, sess = make_repo(tmp)
    (repo / "kb.json").write_text(
        json.dumps({"objects": [{"name": "main.obj"}]}, indent=1) + "\n")
    commit(repo, "main: kb change")
    (sess / "kb.json").write_text(
        json.dumps({"objects": [{"name": "session.obj"}]}, indent=1) + "\n")
    commit(sess, "session: kb change")
    before = sh("git", "rev-parse", "main", cwd=repo).stdout.strip()

    res, cp = run_lander(repo, sess)
    check("kb.json conflict parks", res.get("status"), "parked")
    check("kb.json named as a real conflict",
          "kb.json" in (res.get("conflicts") or []), True)
    check("main untouched", sh("git", "rev-parse", "main", cwd=repo)
          .stdout.strip(), before)


def test_union_proof_catches_a_dropping_resolver(tmp):
    """If the merge tool loses a key, the proof must park -- not land."""
    repo, sess = make_repo(tmp)
    append_score(repo, "main_fn", 91.0)
    commit(repo, "main: score")
    append_score(sess, "session_fn", 99.0)
    commit(sess, "session: score")

    # Stand in a deliberately lossy "merge tool" (the --ours behaviour that
    # this whole gate exists to catch).
    bad = tmp / "bad_merge.py"
    bad.write_text(
        "import subprocess, sys\n"
        "p = sys.argv[1]\n"
        "out = subprocess.run(['git','show',':2:'+p], capture_output=True,\n"
        "                     text=True).stdout\n"
        "open(p,'w').write(out)\n")
    real = ar._MERGE_DERIVED
    try:
        ar._MERGE_DERIVED = str(bad)
        os.chdir(sess)
        ok, resolved, why = ar._rebase_resolving_derived("session")
    finally:
        ar._MERGE_DERIVED = real
    check("lossy resolver rejected", ok, False)
    check("rejected as a dropped-key failure", "dropped_" in why, True)
    rebasing = subprocess.run(["git", "rev-parse", "--git-path", "rebase-merge"],
                              cwd=sess, capture_output=True, text=True).stdout.strip()
    check("rebase aborted, no half-finished state",
          (Path(sess) / rebasing).exists(), False)


def main():
    cwd = os.getcwd()
    try:
        test_conflict_path_filter()
        test_kb_json_never_auto_merged()
        test_key_paths_nested()
        for fn in (test_derived_conflict_lands_keeping_both_sides,
                   test_source_conflict_still_parks,
                   test_kb_json_conflict_parks,
                   test_union_proof_catches_a_dropping_resolver):
            with tempfile.TemporaryDirectory() as td:
                os.chdir(cwd)
                fn(Path(td))
    finally:
        os.chdir(cwd)
    print()
    if FAILURES:
        print(f"{len(FAILURES)} FAILURE(S)")
        for f in FAILURES:
            print("  " + f)
        return 1
    print("all derived-cache lander tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
