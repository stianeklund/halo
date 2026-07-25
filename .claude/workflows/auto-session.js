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

const BATCHES    = (args && args.batches) || 6     // max land cycles
const BATCH_GOAL = (args && args.batchGoal) || 4   // functions per goal-lift run
const DRY_RUN    = !!(args && args.dryRun)
const OBJECTS    = (args && args.objects) || undefined
const CRITERIA   = (args && args.criteria) || undefined

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
Return {ok:true, branch:"<name>", mainWorktree:"<path>"} if the current branch is NOT "main" and a main worktree was found.
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

// ── Batch loop ────────────────────────────────────────────────────────────────
phase('Batch')
let batchesLanded = 0
let functionsCommitted = 0
let stoppedReason = 'batches_exhausted'
let parkReason = null
let parkConflicts = null

for (let i = 1; i <= BATCHES; i++) {
  log(`\n── Batch ${i}/${BATCHES} ─────────────────────────────`)

  // 1. Inner loop: goal-lift selects targets, lifts, verifies, review-gates,
  //    and commits to BRANCH. Nesting is one level -> allowed.
  const glArgs = { goal: BATCH_GOAL, dryRun: DRY_RUN }
  if (OBJECTS) glArgs.objects = OBJECTS
  if (CRITERIA) glArgs.criteria = CRITERIA
  const r = await workflow('goal-lift', glArgs)

  const committed = (r && r.committed) || 0
  functionsCommitted += committed
  log(`Batch ${i}: goal-lift committed ${committed} (reason: ${r ? r.reason : 'null'})`)

  // 2. Queue exhausted -> nothing more to do.
  if (committed === 0) {
    stoppedReason = (r && /empty_queue/.test(r.reason || '')) ? 'queue_exhausted' : 'no_commit'
    log(`Stopping: ${stoppedReason}`)
    break
  }

  // 3. Dry run: never land.
  if (DRY_RUN) {
    log(`Batch ${i}: dry run — would land ${committed} function(s) into main`)
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

  // 5. Parked / inconclusive -> stop piling commits that cannot land.
  stoppedReason = land.status  // 'parked' | 'inconclusive'
  parkReason = land.reason
  parkConflicts = land.conflicts || null
  log(`⚠ Batch ${i} did not land (${land.status}: ${land.reason})`)
  if (parkConflicts && parkConflicts.length) log(`  conflicts: ${parkConflicts.join(', ')}`)
  log(`Stopping so the branch does not accumulate un-landable commits.`)
  break
}

// ── Report ────────────────────────────────────────────────────────────────────
phase('Report')
log(`\n── Auto-session complete ─────────────────`)
log(`Batches landed:      ${batchesLanded}`)
log(`Functions committed: ${functionsCommitted}`)
log(`Stop reason:         ${stoppedReason}`)
if (parkReason) log(`Park reason:         ${parkReason}`)
if (budget.total) log(`Budget remaining:    ~${Math.round(budget.remaining() / 1000)}k tokens`)

return {
  branch: BRANCH,
  batches_landed: batchesLanded,
  functions_committed: functionsCommitted,
  stopped_reason: stoppedReason,
  park_reason: parkReason,
  conflicts: parkConflicts,
  dry_run: DRY_RUN,
}
