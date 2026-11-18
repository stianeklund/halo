---
name: reintegrate-to-main
tier: user
description: "Re-integrate a lift/session worktree branch into main: rebase, gate (kb.json whole-object partition, build, no-drop), fast-forward main."
---

# Re-integrate a worktree branch into `main`

**Invoke when:** a lift/session branch (e.g. `integrate-*`, `lift-session-*`,
`goal-lift-*`) is verified and committed, and you want its work on `main`.

**Non-negotiables (why this skill exists):**
- **NEVER merge `kb.json` at function level.** A hand-rolled per-function 3-way
  merge silently dropped 92 functions and duplicated ~530 once, and a subset
  (`⊆`) gate passed the corruption. `kb.json` is integrated at **whole-object**
  granularity, and even a *clean git line-merge* gets a **byte-equality object
  partition** gate before main advances. (`feedback_kb_json_branch_integration_recipe`)
- **Committed history is safe; uncommitted trees are not.** Advancing `main`
  **resets the working tree of whichever worktree has `main` checked out** — any
  uncommitted work there is destroyed. Commit everything, in every involved
  worktree, before advancing. (`feedback_worktree_merge_clobbers_main_tree`)
- **Hooks corrupt rebases.** `prepare-commit-msg` unconditionally rewrites the
  message of any commit touching `kb.json`/`kb_meta.json`/`src/*.c`, and
  `--no-verify` does NOT bypass it. Always rebase with hooks off. The
  per-commit pre-commit ABI audit also re-runs on every replayed commit.
  (`prepare-commit-msg-clobbers-kbjson`)

---

## Step 1 — Map the topology (read-only)

```bash
git worktree list                       # who has main checked out? shared store?
git branch --show-current               # the branch you're integrating
git config --get rerere.enabled         # if true, this is a footgun (see below)
```

Record: the **main worktree path** (the dir whose entry shows `[main]`), whether
it shares this repo's object store (it does if it's in `git worktree list`), and
that you **cannot `git checkout main` here** while it's checked out elsewhere —
the final advance runs *in the main worktree*.

Confirm the main worktree tree is clean of tracked changes (untracked files are
fine unless they collide with files you're landing):

```bash
git -C <main-worktree> status --short   # tracked M/A/D lines = STOP, commit them first
```

## Step 2 — Measure divergence + preview conflicts **without touching any tree**

```bash
git rev-list --left-right --count main...HEAD    # "A<TAB>B" = main-only / branch-only
git merge-base main HEAD                          # the merge base
git merge-base --is-ancestor main HEAD && echo FF-able || echo DIVERGED
git merge-tree --write-tree --name-only main HEAD # PREVIEW: bare tree oid = clean; file list after = conflicts
```

`merge-tree --write-tree` prints a tree OID on line 1. If that's **all** it
prints, the merge is conflict-free. Any paths listed after are conflicting files
— resolve them per Step 3 (kb.json) / by inspection (source).

Also enumerate exactly what the branch changes, so the no-drop gate (Step 5) is
scoped:

```bash
git diff --name-only $(git merge-base main HEAD) HEAD
```

## Step 3 — `kb.json` object-partition discipline

`kb.json` is a list of ~211 `objects`, keyed by **array index** (all versions
share the same object order — verify the count matches on all three of
base/ours/main). Some object **names are duplicated** (`<xdk_stubs>`,
`scenario.obj` appear twice) — a name-keyed router misroutes them, so route by
index, not name.

- **merge-tree clean + branch and main touched disjoint objects** → git's
  line-merge result is trustworthy, but still run the Step 5 partition gate.
- **merge-tree reports `kb.json` conflict** → do NOT resolve by hand hunk-by-hunk.
  Route each object: `ours==base → take main`; `main==base → take ours`;
  identical → either; **both-changed-differently → explicit human decision**
  (surface it). Flat top-level per-addr keys are a disjoint union. Precompute the
  full merged `kb.json`, pass the Step 5 gate, then drop it in at the conflict.
- **Adopt main's deletions.** If main removed/renamed functions (object function
  counts dropped), the merged result must reflect that — dropping main's
  deletions is corruption too.

## Step 4 — Backup, then rebase with hooks OFF

```bash
git branch backup/<branch>-pre-reintegrate HEAD          # cheap undo point
git -c core.hooksPath=/dev/null rebase main              # replays branch commits onto main
```

Hooks off prevents `prepare-commit-msg` from clobbering your lift commit
messages and skips the redundant per-commit ABI audit. If a commit fails to
apply, resolve per Step 3 and `git -c core.hooksPath=/dev/null rebase --continue`.

**Many kb.json-touching commits?** A full rebase re-applies each one's kb.json
patch (fine when disjoint; painful when they collide). For a single resolution +
linear history, squash first:
`git switch -c tmp <tip>; git reset --soft $(git merge-base main HEAD); \
 MSG=$(mktemp /tmp/halo-squash-msg.XXXXXX); \
 python3 tools/audit/generate_lift_commit.py --since $(git merge-base main HEAD) > "$MSG"; \
 git -c core.hooksPath=/dev/null commit -F "$MSG" && rm -f "$MSG"` then
`git -c core.hooksPath=/dev/null rebase --onto main $(git merge-base main HEAD) tmp`.

## Step 5 — Gates (ALL must pass before main advances)

1. **kb.json object-partition (byte gate, not `⊆`):** the rebased `kb.json` must
   contain every object main has (with main's deltas) **and** every object the
   branch has, each byte-equal to its designated source; **0 duplicate addrs**;
   object count == base count adjusted for main's adds/deletes. Quick checks:
   ```bash
   rtk jq '.objects | length' kb.json                       # vs main's count ± net obj changes
   rtk jq '[.objects[].functions[].addr] | length as $n | (unique|length) as $u | {$n,$u}' kb.json  # n==u → no dup addrs
   ```
2. **Reg-baseline drift:** `rtk python3 tools/audit/extract_reg_args.py --check`
   → `0 drift, 0 missing, 0 stale`.
3. **Build gate (mandatory, separate from byte-match):** catches source↔kb.json
   interlock (a chosen kb.json name making a `src/*.c` call undeclared).
   `rtk python3 tools/build/build.py -q --target halo` → clean.
4. **No-drop proof:** every branch-only file is byte-identical between the
   pre-rebase tip (`backup/...`) and the rebased tip:
   ```bash
   for f in $(git diff --name-only $(git merge-base main backup/<branch>-pre-reintegrate) backup/<branch>-pre-reintegrate); do
     git diff --quiet backup/<branch>-pre-reintegrate HEAD -- "$f" || echo "CHANGED BY REBASE: $f"
   done
   ```
   Any file listed that isn't a legitimately-merged both-sides file = investigate.
5. **Behaviour over the whole branch range (fail-closed, no bypass):** gates 1-4
   execute nothing, so without this one the decision that exposes work to `main`
   re-checks no behaviour at all. Runs last (it needs gate 3's fresh `build/`
   objects as the candidate side):
   ```bash
   # hazard scan over every src/*.c the BATCH touched, not one commit's staged set
   rtk python3 tools/audit/check_lift_hazards.py --files \
     $(git diff --name-only $(git merge-base main <branch>) <branch> -- 'src/*.c')
   # the Unicorn differential; the 110-target corpus is fixed, so the branch tip
   # covers every commit in the range
   rtk python3 tools/equivalence/regression_test.py --quick -j8
   ```
   Either failing → PARK (`gate_failed:hazard_scan_errors=N` /
   `gate_failed:regression_test_failed`, with the failing targets in `gate`).
   A missing Unicorn harness parks as `regression_harness_unavailable` — "could
   not run" must never print the same verdict as "passed" on the last gate
   before `main`.

   This is the counterpart to the per-commit deferral: an automated batch commits
   with `HALO_BATCH_COMMIT=1`, which makes
   `tools/hooks/pre-commit-regression-test.sh` skip the differential per commit
   (the hook refuses the skip on `main`, where no land gate follows). Cheap
   per-commit gates — ABI audit, reg-baseline drift, per-TU VC71, the staged-file
   hazard scan — still run on every commit. **Never** hand-FF `main` past a batch
   branch: that route bypasses this gate, and those commits were committed on the
   promise that it would run.

## Step 6 — Advance `main` (fast-forward only)

After the rebase, `main` is an ancestor of the branch tip → FF-able.

```bash
git merge-base --is-ancestor main HEAD && echo FF-OK      # must print FF-OK
```

Advance runs **in the main worktree** (you can't check out main here):

```bash
git -C <main-worktree> merge --ff-only <branch>
```

- **FF-only** guarantees no merge commit and no history rewrite — trivially
  reversible with `git -C <main-worktree> reset --keep <old-main-oid>`.
- If the main worktree is fenced/locked or belongs to another live session, the
  **user runs this command** — hand it to them; do not force the ref.
- Never `git branch -f main` while main is checked out — git refuses, and forcing
  it corrupts that worktree's index.

## Footgun checklist (verify each before advancing)

- [ ] All involved worktrees committed (main worktree tree clean of tracked mods).
- [ ] `rerere.enabled` is `false`; if `true`, run the rebase with
      `-c rerere.enabled=false` and `git rerere forget kb.json` (it auto-replays
      a cached bad kb.json resolution into a plain merge).
- [ ] Rebase ran with `core.hooksPath=/dev/null` (messages intact).
- [ ] kb.json partition gate + build gate + no-drop proof all green.
- [ ] `merge --ff-only` (never a non-FF merge that would rewrite/bubble).
- [ ] Backup branch exists as the undo point.
