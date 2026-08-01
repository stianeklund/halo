---
name: recover-goal
tier: user
description: Goal-mode readability recovery — loop the recovery frontier, run the source-recovery ladder per object, park on failure, until N objects are recovered or the queue is exhausted. The unattended driver around /recover-source.
---

# /recover-goal — Goal-Mode Source Recovery

`/recover-source` recovers **one** TU/object. This skill is the driver that runs
it **object after object** unattended: pick the next target off the recovery
frontier, run the full ladder on it, land one commit per category, ratchet the
floors, record the outcome, repeat. It adds no new gates and weakens none — all
verification lives in `source-recovery`.

Usage: `/recover-goal [N] [--min-funcs 10] [--min-score X] [--allow-risky]`

- `N` — objects to **finish** before stopping (default 3).
- `--min-funcs 10` — skip trivially small objects (default 10).
- `--min-score X` — refuse objects below this frontier score (default 0).
- `--allow-risky` — passed through to `plan` so ladder categories 7–8 are
  workable. Without it the manifest refuses those items; that is the safe mode.

State lives in **`recovery/goal_ledger.json`**, owned by
`tools/recovery/recovery_goal.py` (`next` / `start` / `finish` / `park` /
`status`). The ledger is the only thing that keeps the loop from re-taking an
object it already parked — never hand-edit it, and never skip `start`.

## Per-iteration preconditions (hard stop, not a warning)

1. `rtk git status --short` — the tree must be clean except known-noisy paths
   (`README.md`, `recovery/*.json`, `tools/equivalence/leaf_cache.json`).
   **Anything under `src/` or in `kb.json` → stop and surface it**; a recovery
   commit must not pick up someone else's in-flight lift.
2. On the branch you intend to land from, up to date with `main`.
3. `rtk python3 tools/recovery/recovery_goal.py status` — no stale
   `in_progress` entry. One means a previous run died mid-object: resolve it
   (`finish` with its commits, or `park --reason`) before taking anything new.

## The loop

For each iteration until a stop condition fires:

```bash
G=tools/recovery/recovery_goal.py
rtk python3 $G next --min-funcs 10          # exit 3 = queue exhausted → stop
rtk python3 $G start <object.obj>
```

Then run the **full `source-recovery` workflow** on that object exactly as its
`SKILL.md` prescribes — do not restate or reinterpret it here:

- `plan` (add `--allow-risky` only when the user passed it) → `capture` against
  the object's built COFF → `ladder` for the ordering view.
- Work the ladder **in order**, **one category per sequential subagent** (never
  parallel on one TU — each category's commit is the next one's base). Brief each
  agent with: manifest path, category id, its ladder skill, and an explicit
  same-worktree instruction.
- Per category: gate with `check` at that category's gate level, `set-status`
  each item `applied`/`parked`, then before committing:
  `rtk python3 tools/recovery/check_category_purity.py <category> --staged`
  (a category commit must contain only that category's kind of change).
- One commit per category, message prefix
  `recover(<object-stem>): <category> <short what>`.

After the object's last category:

```bash
rtk python3 tools/verify/vc71_regression.py update --source <changed .c files>
rtk python3 $G finish <object.obj> --commit <sha> [--commit <sha> ...] \
    --category <ladder-id> [--category <ladder-id> ...]
```

`update` locks in any improvement so the next session cannot silently give it
back. `finish` refuses an object that was never `start`ed, and refuses a "done"
with no commits unless you pass `--no-commits` deliberately.

## Park on failure — fail closed

Never weaken a gate, never `--no-verify`, never touch `@<reg>` annotations,
`ported` flags, kb.json signatures, or build config to make an object pass. If
recovery needs one of those, that is *lift* work: park and surface it.

- **Category-level failure** (gate red, purity check red, `check` reports a
  codegen delta): revert that category's unit, `set-status ... parked --reason
  <why>` per `source-recovery`, and continue with the remaining independent
  categories. The object can still `finish` with the categories that landed.
- **Object-level blocker** — `capture` fails, no COFF baseline for the object,
  the build is broken before you touched anything, the manifest errors, or
  **more than 2 consecutive category failures**: `rtk git checkout -- <paths>`
  / reset the uncommitted work (already-committed categories stay — they passed
  their gates), then
  `rtk python3 $G park <object.obj> --reason "<specific blocker>"` and move to
  the next object.
- A regression noticed *after* a commit is not a park: stop the loop and run
  `cleanup-regression-triage` (per-category commits make it a one-commit
  bisect).

Reasons are read by humans and by the stop conditions below — write
`"capture: no COFF for hud_weapon.obj"`, not `"failed"`.

## Stop conditions and report

- **N objects finished.**
- **Queue exhausted** — `next` exits 3. Report the counts it printed (eligible /
  already taken / below `--min-score`); an exhausted queue with everything
  parked is a finding, not success.
- **Two consecutive objects parked for the same infra reason** — that is
  systemic (missing delinked refs, broken build, purity tool absent). Stop and
  report instead of burning the queue; the ledger would otherwise fill with
  parks that hide the one real cause.
- **Precondition failure** — dirty `src/`/`kb.json`, or a stale `in_progress`.

Report, in this order: the `recovery_goal.py status` table verbatim; per finished
object its category commits (sha + subject) and `source_recovery.py report`
highlights (applied / parked, gate mode); which files `vc71_regression.py update`
moved and by how much; every park with its reason; the stop condition that ended
the run; and anything surfaced as *lift* work (signature/ABI/`ported`) that was
deliberately **not** done here.

## See also

`source-recovery` is the single source of truth per object (ladder, gate table,
measurement traps, session rules). `cleanup-regression-triage` and
`cleanup-report` cover post-hoc triage and the human-facing write-up.
`goal-lift` / `auto-session` are the same goal-mode shape for *lifting* — mirror
their fail-closed posture, not their target selection.
