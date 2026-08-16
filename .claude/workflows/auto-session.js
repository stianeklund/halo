export const meta = {
  name: 'auto-session',
  description: 'Fully-automated lift session: run goal-lift in batches on the current isolated worktree branch, and after each batch land the branch into main (auto fast-forward, park-on-conflict) via the mechanical auto_reintegrate.py gate.',
  phases: [
    { title: 'Guard',  detail: 'Confirm we are on an isolated branch (not main) and resolve the main worktree' },
    { title: 'Batch',  detail: 'Per batch: goal-lift (select+lift+verify+commit) -> auto_reintegrate gate -> FF main or park' },
    { title: 'Report', detail: 'Summary: batches landed, functions committed, stop reason, park detail' },
  ],
}

// WHY THIS EXISTS
// ----------------
// goal-lift already does the whole inner loop (select its own targets, lift,
// verify, review-gate, commit to the current branch). What was missing was the
// OUTER loop that lands each batch into `main`. This workflow supplies it.
//
// A workflow cannot switch the session cwd (no EnterWorktree from inside a
// workflow), so we do NOT create per-batch worktrees. Instead we run on this
// session's already-isolated branch and fast-forward `main` up to the branch
// tip after each batch. The branch stays ahead of main, so it stays FF-able
// unless another session races `main` -- which is exactly the park case.
//
// The dangerous merge logic lives in tools/integrate/auto_reintegrate.py (a
// testable, fail-closed tool), NOT in agent prose. Agents here only invoke it
// and parse its JSON. This workflow never hand-resolves kb.json.

// Measured 2026-08-04 (run 8 profile): per-batch fixed overhead is ~7 min of
// serial agents (select 3.2 + retrieval-warm 1.6 + delink-prefetch 1.1 +
// liftability 0.6 + progress-log 0.5 + preflight 0.2). With noLand the small-
// batch "land often" rationale does not apply, so fewer/bigger batches save
// ~25-30 min per session.
const BATCHES    = (args && args.batches) || 2      // max land cycles
const BATCH_GOAL = (args && args.batchGoal) || 12   // functions per goal-lift run
const DRY_RUN    = !!(args && args.dryRun)
// Lift and commit normally, but never attempt to land. For when `main` is
// known to be in use by another workstream: the land gate would fail on its
// dirty worktree and stop the run, wasting the remaining batches (observed
// 2026-08-02: a batch-1 park cost the other 5). Commits accumulate on the
// branch and go in as one FF later. Unlike dryRun, this still commits.
const NO_LAND    = !!(args && args.noLand)
const OBJECTS    = (args && args.objects) || undefined
const CRITERIA   = (args && args.criteria) || undefined
// Hard pin-list of target addresses, forwarded verbatim to goal-lift, which
// parses and enforces it after selection (and bypasses its own pre-screens for
// pinned addrs). Without this pass-through a caller can only steer selection by
// object/criteria, and a specific low-scoring function never surfaces.
const ADDRS      = (args && args.addrs) || undefined
// Opt-in: let goal-lift lift register-argument targets instead of dropping them
// at its two pre-screens. The remaining frontier is almost entirely @<reg>
// fragments, so without this a run selects, decompiles, and then skips nearly
// everything. goal-lift's own gates (build / VC71 / audit_reg_abi / reviewer /
// revert-on-fail) still apply, so a wrong @<reg> decl reverts rather than ships.
const LIFT_REG_ARGS = !!(args && args.liftRegArgs)
// Routine improve pass after the batch loop: drain up to this many parked
// sub-bar functions via goal-lift's improve mode. 0 (default) skips it --
// opt-in, since it adds a full nested workflow run after the batch loop.
const IMPROVE_GOAL = (args && args.improveGoal) || 0

// Resolving the child by NAME uses a workflow registry snapshotted at session
// start, so mid-session edits to goal-lift.js are silently ignored -- the agents
// keep running the stale script. Passing goalLiftPath makes the child resolve by
// scriptPath instead, which is read from disk at invoke time. Left optional so
// the workflow stays portable across worktrees (no hardcoded absolute path).
const GOAL_LIFT = (args && args.goalLiftPath)
  ? { scriptPath: args.goalLiftPath }
  : 'goal-lift'

// Deterministic tool-runs only -- all the reasoning cost is inside the nested
// goal-lift workflow, which sets its own (Opus-high) model policy.
const MECH = { model: 'haiku', effort: 'low' }

const GUARD_SCHEMA = {
  type: 'object',
  properties: {
    ok:           { type: 'boolean' },
    branch:       { type: 'string' },
    mainWorktree: { type: 'string' },
    reason:       { type: 'string' },
    // Advisory only — a dirty main worktree does NOT block the run (it lands as
    // 'inconclusive' and the batches keep going), but the operator should learn
    // it at minute 1 rather than after the last batch.
    mainDirty:      { type: 'boolean' },
    mainDirtyFiles: { type: 'array', items: { type: 'string' } },
  },
  required: ['ok', 'branch'],
}

const LAND_SCHEMA = {
  type: 'object',
  properties: {
    status:    { type: 'string' },   // landed | parked | inconclusive
    reason:    { type: 'string' },
    conflicts: { type: 'array', items: { type: 'string' } },
    main_old:  { type: 'string' },
    main_new:  { type: 'string' },
  },
  required: ['status', 'reason'],
}

// ── Guard ─────────────────────────────────────────────────────────────────────
phase('Guard')
log(`Auto-session: up to ${BATCHES} batches x ${BATCH_GOAL} functions${DRY_RUN ? ' (dry run — no commits, no land)' : ''}`)

const guard = await agent(
  `Run these read-only git commands from the current working directory and report the result as JSON.
1. \`rtk git branch --show-current\` — the current branch name.
2. \`rtk git worktree list\` — find the entry marked \`[main]\`; its path is the main worktree.
3. \`rtk git -C <that main worktree path> status --short\` — report mainDirty=true and
   list the paths in mainDirtyFiles (tracked modifications only; ignore untracked \`??\`
   lines) if it prints anything. This is ADVISORY: a dirty main does not block the run,
   it only means batches will accumulate unlanded until main goes quiet. Still return
   ok:true.
Return {ok:true, branch:"<name>", mainWorktree:"<path>", mainDirty:<bool>, mainDirtyFiles:[...]} if the current branch is NOT "main" and a main worktree was found.
Return {ok:false, branch:"<name>", reason:"..."} if the current branch IS "main" (we must run on an isolated branch, never commit lifts onto main) or no main worktree exists.`,
  { label: 'guard', phase: 'Guard', ...MECH, schema: GUARD_SCHEMA })

if (!guard || !guard.ok) {
  const reason = guard ? (guard.reason || `on branch ${guard.branch}`) : 'guard_agent_null'
  log(`Guard failed: ${reason}`)
  return { batches_landed: 0, functions_committed: 0, stopped_reason: 'guard_failed', park_reason: reason }
}
const BRANCH = guard.branch
const MAIN_WT = guard.mainWorktree || 'auto'
log(`Branch: ${BRANCH}  |  main worktree: ${MAIN_WT}`)

// Advisory, not a gate. On 2026-08-08 a 6-batch run committed 21 functions and
// landed none: main was dirty before batch 1 and stayed dirty, so every land
// returned 'inconclusive'. That is the designed behaviour (the commits stay
// landable), but the operator only found out at the end. Say it up front so
// they can quiet main, or re-run with noLand and land once, deliberately.
if (guard.mainDirty && !DRY_RUN && !NO_LAND) {
  const files = (guard.mainDirtyFiles || []).slice(0, 5).join(', ')
  log(`⚠ main worktree is dirty${files ? ` (${files})` : ''} — batches will accumulate unlanded until it is clean.`)
  log(`  This does not stop the run. To land deliberately later instead, re-run with noLand:true.`)
}

// ── Batch loop ────────────────────────────────────────────────────────────────
phase('Batch')
let batchesLanded = 0
let functionsCommitted = 0
let stoppedReason = 'batches_exhausted'
let parkReason = null
let parkConflicts = null
// Batches whose lift work is committed but whose land was blocked by something
// environmental (dirty/locked main worktree). They remain landable.
let unlandedBatches = 0
// True when the run stopped on an API/infra failure rather than on anything
// about the work, i.e. resuming this run recovers it. See the isInfra branch.
let resumable = false

// Infra failures (API 529 killing an agent, Ghidra bridge down) are transient
// and must NOT be treated as "no work left". Retry the batch a bounded number of
// times across the whole run; reset the budget after any batch that commits, so
// a long run that hits trouble late still gets fresh attempts.
const MAX_INFRA_RETRIES = 2
let infraRetries = 0

for (let i = 1; i <= BATCHES; i++) {
  log(`\n── Batch ${i}/${BATCHES} ─────────────────────────────`)

  // 1. Inner loop: goal-lift selects targets, lifts, verifies, review-gates,
  //    and commits to BRANCH. Nesting is one level -> allowed.
  const glArgs = { goal: BATCH_GOAL, dryRun: DRY_RUN }
  if (OBJECTS) glArgs.objects = OBJECTS
  if (CRITERIA) glArgs.criteria = CRITERIA
  if (ADDRS) glArgs.addrs = ADDRS
  if (LIFT_REG_ARGS) glArgs.liftRegArgs = true
  const r = await workflow(GOAL_LIFT, glArgs)

  const committed = (r && r.committed) || 0
  functionsCommitted += committed
  log(`Batch ${i}: goal-lift committed ${committed} (reason: ${r ? (r.stop_reason || r.reason) : 'null'})`)

  // 2. Zero commits -> is this REAL (frontier empty) or INFRA (select agent died
  //    on an API 529, Ghidra bridge down)? These used to collapse into
  //    "queue_exhausted", so one transient error abandoned every remaining
  //    batch. Retry infra; only stop for a genuinely empty queue.
  if (committed === 0) {
    // goal-lift's early-exit returns (queue empty before the loop starts) key
    // this `reason`; its post-loop returns (Phase 4 / improve mode) key it
    // `stop_reason`. Reading only `reason` silently defaulted every full-run
    // zero-commit batch to 'agent_null' and misclassified it as infra_blocked
    // even when goal-lift completed cleanly (e.g. stop_on_fail_reached).
    const reason = (r && (r.stop_reason || r.reason)) || 'agent_null'
    const isEmpty = /empty_queue/.test(reason)
    const isInfra = !isEmpty &&
      (!r || /select_agent_null|infra_blocked|ghidra_unavailable|agent_null/.test(reason))

    if (isInfra && infraRetries < MAX_INFRA_RETRIES) {
      infraRetries++
      log(`Batch ${i}: infra failure (${reason}) — retrying batch ${infraRetries}/${MAX_INFRA_RETRIES}`)
      i--            // redo this batch index (bounded by infraRetries)
      continue
    }

    stoppedReason = isEmpty ? 'queue_exhausted' : isInfra ? 'infra_blocked' : 'no_commit'
    log(`Stopping: ${stoppedReason} (${reason})`)
    if (isInfra) {
      parkReason = `infra: ${reason} (after ${infraRetries} retries)`
      // An infra stop is the ONE case where nothing is wrong with the work:
      // the API was unavailable, so agents returned null. Re-running from
      // scratch re-pays for every agent that already succeeded; resuming
      // replays those from cache and re-runs only the failures. Measured on
      // run wf_6c878a7b-e6d: 1.94M tokens for the original attempt vs 170K
      // for the resume. Flag it so the caller resumes instead of restarting.
      //
      // Caveat: the run cache is SESSION-scoped. Once this session ends the
      // cache is gone and the work must be redone from scratch, so a resume
      // is worth attempting promptly -- but only after the outage has passed,
      // since an immediate resume just re-fails (observed twice here).
      resumable = true
    }
    break
  }

  // A productive batch refills the infra-retry budget.
  infraRetries = 0

  // 3. Dry run: never land.
  if (DRY_RUN) {
    log(`Batch ${i}: dry run — would land ${committed} function(s) into main`)
    continue
  }

  // 3b. Landing suppressed: keep lifting, leave the commits on the branch.
  if (NO_LAND) {
    unlandedBatches++
    log(`Batch ${i}: noLand — ${committed} function(s) held on ${BRANCH}`)
    continue
  }

  // 4. Land the batch via the mechanical gate.
  const land = await agent(
    `Run this command from the current working directory and return its stdout JSON verbatim as your structured result:
\`rtk python3 tools/integrate/auto_reintegrate.py --branch ${BRANCH} --main-worktree ${MAIN_WT} --json\`

IMPORTANT — this gate runs a full clean build and routinely takes MORE than the
default 120s Bash timeout. Pass an explicit long timeout on the Bash call
(timeout: 600000). If it still exceeds that, re-run it with run_in_background
and poll the output file until the JSON appears; do NOT report a timeout as a
park reason while the gate may still be running, and do NOT re-run the gate
concurrently with itself.

The tool prints a single JSON object with keys: status (landed|parked|inconclusive), reason, and optionally conflicts, main_old, main_new. Do not modify it. Do not run any other git commands. Parse that JSON and return it.`,
    { label: `land:batch-${i}`, phase: 'Batch', ...MECH, schema: LAND_SCHEMA })

  if (!land) {
    stoppedReason = 'land_agent_null'
    parkReason = 'land_agent_returned_null'
    log(`Batch ${i}: landing agent returned null — stopping`)
    break
  }

  if (land.status === 'landed') {
    batchesLanded++
    log(`✓ Batch ${i} landed: main ${land.main_old || '?'} → ${land.main_new || '?'}`)
    continue
  }

  // 5. The batch did not land. The gate distinguishes two very different
  //    cases and we must not collapse them:
  //
  //    'parked'       -> a real signal about THIS WORK: merge/rebase conflict,
  //                      a failed gate, or a non-FF result. Every further
  //                      commit makes the eventual resolution harder, so stop.
  //
  //    'inconclusive' -> environmental and unrelated to the work: the main
  //                      worktree is dirty or locked (another lane is mid-edit
  //                      there), missing, or the branch is not ahead. The
  //                      commits ARE landable; the only thing missing is a
  //                      moment when main is quiet. Stopping here threw away
  //                      five of six batches per session for a reason that had
  //                      nothing to do with the lifts. Keep lifting and retry
  //                      the land next batch -- total growth is already bounded
  //                      by BATCHES, and the work stays on the branch either way.
  parkReason = land.reason
  parkConflicts = land.conflicts || null

  if (land.status === 'inconclusive') {
    unlandedBatches++
    log(`… Batch ${i} could not land yet (inconclusive: ${land.reason})`)
    log(`  Continuing — ${unlandedBatches} batch(es) now waiting to land.`)
    continue
  }

  stoppedReason = land.status  // 'parked'
  log(`⚠ Batch ${i} did not land (${land.status}: ${land.reason})`)
  if (parkConflicts && parkConflicts.length) log(`  conflicts: ${parkConflicts.join(', ')}`)
  log(`Stopping so the branch does not accumulate un-landable commits.`)
  break
}

// Finishing every batch while landing was merely blocked is NOT a clean run:
// the work is committed but still sitting on the branch. Say so explicitly
// rather than reporting 'batches_exhausted' as if everything had landed.
if (stoppedReason === 'batches_exhausted' && unlandedBatches > 0) {
  stoppedReason = 'completed_with_unlanded'
}

// Routine improve pass: after the batch loop, drain up to IMPROVE_GOAL parked
// sub-bar functions with goal-lift's improve mode (different model, warm-start
// from the best parked patch). Skipped when the run was cut short by an infra
// failure (resumable) -- that case should resume the batch loop first, not
// spend budget draining the ledger on a run that didn't finish its own work.
let improvePromoted = 0
if (IMPROVE_GOAL > 0 && !resumable) {
  log(`\n── Improve pass — draining up to ${IMPROVE_GOAL} parked function(s) ─────`)
  const ir = await workflow(GOAL_LIFT, { improve: true, goal: IMPROVE_GOAL, dryRun: DRY_RUN })
  improvePromoted = (ir && ir.promoted) || 0
  functionsCommitted += improvePromoted
  log(`Improve pass: promoted ${improvePromoted} (stop reason: ${ir ? ir.stop_reason : 'null'})`)
}

// ── Report ────────────────────────────────────────────────────────────────────
phase('Report')
log(`\n── Auto-session complete ─────────────────`)
log(`Batches landed:      ${batchesLanded}`)
if (unlandedBatches) log(`Batches unlanded:    ${unlandedBatches} (committed on ${BRANCH}, landing was blocked)`)
log(`Functions committed: ${functionsCommitted}`)
if (improvePromoted) log(`Improve promoted:    ${improvePromoted}`)
log(`Stop reason:         ${stoppedReason}`)
if (parkReason) log(`Park reason:         ${parkReason}`)
if (budget.total) log(`Budget remaining:    ~${Math.round(budget.remaining() / 1000)}k tokens`)

return {
  branch: BRANCH,
  batches_landed: batchesLanded,
  batches_unlanded: unlandedBatches,
  functions_committed: functionsCommitted,
  improve_promoted: improvePromoted,
  stopped_reason: stoppedReason,
  park_reason: parkReason,
  conflicts: parkConflicts,
  // Output tokens spent by this run (main-loop + all nested workflows share the
  // pool, so this is the run's share as observed at return time). Consumed by
  // /campaign's per-run ledger (artifacts/campaigns/campaigns.jsonl).
  tokens_spent: budget.spent(),
  // Resume with Workflow({scriptPath, resumeFromRunId}) rather than launching a
  // fresh run: succeeded agents replay from cache, only failures re-run. Wait
  // for the outage to clear first -- an immediate resume re-fails.
  resumable,
  dry_run: DRY_RUN,
  no_land: NO_LAND,
}
