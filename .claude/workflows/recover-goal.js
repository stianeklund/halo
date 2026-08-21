export const meta = {
  name: 'recover-goal',
  description: 'Goal-mode source recovery: pick the next object off the recovery frontier, work the source-recovery ladder one category per sequential Opus agent (one commit each), ratchet the VC71 floors, record the outcome in the ledger, repeat until N objects are recovered or the queue is exhausted.',
  phases: [
    { title: 'Guard',   detail: 'Working tree must be clean under src/, kb.json, tools/, .claude/, recovery/' },
    { title: 'Select',  detail: 'recovery_goal.py next -> start; baseline the object (build + plan + capture + ladder)' },
    { title: 'Recover', detail: 'Per object: one sequential Opus agent per ladder category, purity-gated commit each; then finalize (floors + report + finish)' },
    { title: 'Report',  detail: 'Summary: per-object category outcomes, commits, floors raised, stop reason, ledger status' },
  ],
}

// WHY THIS EXISTS
// ----------------
// The recovery machinery (recovery_goal.py ledger, source_recovery.py manifest,
// check_category_purity.py gate) already exists and is deterministic. What was
// prose-only was the DRIVER: `.claude/skills/recover-goal/SKILL.md` described a
// loop, but a prose command can only *suggest* tool calls. Two things kept
// getting skipped in practice, and both are correctness-critical:
//
//   1. ONE AGENT PER CATEGORY, SEQUENTIALLY. Each category's commit is the next
//      category's base. A single agent doing "comments + renames" produces a
//      commit that check_category_purity.py rejects, and a reviewer can no
//      longer say "commit 3 is renames-only" (source-recovery Separation rule).
//   2. THE LEDGER. Skipping `start`/`finish`/`park` makes the loop re-take an
//      object it already parked, forever.
//
// Here `agent()` and the category `for` loop are real code, so neither can be
// skipped. Everything ABOUT recovery (ladder order, gate table, measurement
// traps, fidelity rules) stays in .claude/skills/source-recovery/SKILL.md — this
// script deliberately does not restate it, it just guarantees it gets read.
//
// This workflow adds no gates and weakens none.

const GOAL         = (args && args.goal) || 1
const MIN_FUNCS    = (args && args.minFuncs !== undefined) ? Number(args.minFuncs) : 10
const MIN_SCORE    = (args && args.minScore !== undefined) ? Number(args.minScore) : null
const FIRST_OBJECT = (args && args.object) ? String(args.object).trim() : null
const ALLOW_RISKY  = !!(args && args.allowRisky)
const DRY_RUN      = !!(args && args.dryRun)

// Mechanical agents run fixed commands and parse their output — no judgement, so
// no reasoning budget. All the cost belongs to the category agents, which do the
// actual source edits and must apply a skill's gate table; they get the same
// model policy goal-lift.js gives its lift agents (M.reason = opus/high).
const MECH    = { model: 'haiku', effort: 'high'  }
const RECOVER = { model: 'opus',  effort: 'high' }

// The ladder, in mandatory order, with each category's leaf skill. Order is NOT
// a preference: offset rewrites need the cs()/co() asserts that struct-define
// lands, and renames before rewrites keep the diffs reviewable. Categories may
// be skipped when inapplicable (pending == 0), never reordered.
const LADDER = [
  { id: 'comments',        skill: 're-comment-capture' },
  { id: 'local-renames',   skill: 'local-var-cleanup' },
  { id: 'symbol-names',    skill: 'naming-confidence' },
  // Naming an absolute-address deref is NOT a symbol-names edit: it replaces a
  // multi-token cast-deref, and a kb.json HDATA extern would compile to a load
  // through [__imp__X]. The neutral form is the tree's existing convention,
  // `#define NAME (*(T *)0xADDR)`, checked by the global-names purity checker.
  { id: 'global-names',    skill: 'naming-confidence' },
  { id: 'const-enum',      skill: 'const-enum-recovery' },
  // Rungs 6/7 have a deterministic path: tools/recovery/structize.py does the
  // transcription and refuses where a human would guess. The leaf skill stays
  // listed because the tool's REFUSALS are that skill's actual work.
  { id: 'struct-define',   skill: 'struct-recovery + struct-assert', mech: 'split' },
  { id: 'offset-to-field', skill: 'offset-to-struct',                mech: 'converge' },
  // Risky (codegen can legitimately move) — only reachable with --allow-risky,
  // which is also what the manifest itself requires before `set-status applied`
  // will accept an item in these categories.
  { id: 'expr-simplify',   skill: 'expr-simplify',        risky: true },
  { id: 'control-flow',    skill: 'control-flow-cleanup', risky: true },
]
const CATEGORIES = LADDER.filter(c => ALLOW_RISKY || !c.risky)

// Adding a LINE to a .c is not free. Measured 2026-08-21 on
// particle_systems.c: two `#define` lines at the top of the file moved every
// assert below them (line 1572 -> 1574), and the COFF gate correctly reported
// "code bytes changed". The same 15 substitutions with the defines in an
// already-included header came out byte-identical.
const LINE_SHIFT_WARNING = `Asserts expand \`__FILE__\` and \`__LINE__\`, so ANY line added to the .c above an
assert changes .text and fails the COFF gate. It is not the substitution that
breaks — it is the line. Put the definitions in a header the TU ALREADY
includes, below that header's last assert-bearing inline function. Adding a
fresh \`#include\` line to the .c has exactly the same problem as adding the
defines directly, unless the file's asserts carry hardcoded line numbers rather
than \`__LINE__\` (some lifted TUs do — check before assuming).`

// Per-category levers. Each exists in the tools but is not obvious from the
// leaf skill, and its absence showed up as a recurring hand-written park.
const LEVERS = {
  'symbol-names': `

LEVER — renaming a symbol that .text relocations point at.
A rename changes the COFF symbol NAME, so a plain \`check\` reports "relocations
changed". That is what \`--rename-map\` is for:
  \`source_recovery.py check <manifest> --object <obj> --rename-map '{"FUN_00123456":"new_name"}'\`
It excuses the symbol NAME only — offset, type, target section, target value, the
.text bytes and the assertion metadata must still match exactly, so a relocation
aimed at a genuinely different symbol still fails. Supply the map you verified
against the staged diff, never a whole-file map. "Renaming it changes the
relocation" is NOT a valid park reason; a missing kb.json name or absent T1/T2
evidence is.
A rename that also lands in kb.json needs the vc71_scores.json floor key re-keyed
from the old name to the new one, in the same commit.`,

  'global-names': `

PLACEMENT — read this before you add a single line.
${LINE_SHIFT_WARNING}

LEVER — the only neutral way to name an absolute-address dereference.
Use the convention already in the tree (\`src/common.h\`, 82 sites):
  \`#define update_client_globals_initialized (*(uint8_t *)0x45b1d0)\`
then substitute the identifier at every site. The macro body must be EXACTLY the
token run it replaces — same cast type, same address — because that is what makes
it codegen-neutral, and \`check_category_purity.py global-names --staged\` verifies
precisely that.

Do NOT reach for a kb.json \`objects.data\` extern: those are emitted \`HDATA\` =
\`__declspec(dllimport)\`, so the reference compiles to a load through
\`[__imp__X]\`. That is real codegen movement, not a too-strict gate, and it fails
the COFF check for a good reason.

Evidence still governs the NAME (\`naming-confidence\` tiers): a \`data_new()\` tag
string, an assert string, or an existing kb.json \`objects.data\` entry at that
address is T1/T2. With no evidence, park the item — do not invent a name. Putting
the defines in a recovered header is fine; if the header already exists and this
diff does not touch it, pass it with \`--macro-source <header>\`.`,

  'const-enum': `

LEVER — \`magic_fourcc:<literal>\` items are the only literals this rung
proposes, and the evidence is inside the value: \`0x61637472\` spells 'actr'.
Name it, define it once, substitute every site. Matching literals against the
\`#define\`s already in \`src/**.h\` was measured and is useless (\`6\` matches
\`_actor_action_guard\` at 192 sites), and frequency is no better — the most
repeated literals in a TU are \`0.0f\`, \`1.0f\`, and struct offsets that belong
to \`offset-to-field\`. Do not name a literal this detector did not propose.

PLACEMENT — read this before you add a single line.
${LINE_SHIFT_WARNING}`,

  'struct-define': `

LEVER — \`struct_bases:<name>\` items are CLUSTERS, not sites.
Each one names a base variable, every offset the file reads off it, and the
widths used. That set is the evidence for one struct. Bind it in
recovery/bindings.json and let \`structize.py\` do the transcription; a cluster
whose widths disagree is a \`verify_conflict.py\` question, not a guess.
This rung exists so \`offset-to-field\` has a co()/cs()-asserted struct to bind to.
"No asserted struct" is the *reason this category runs*, not a park reason for it.`,
}

// Infra failures (API 529 killing an agent, a broken build we did not cause) are
// transient and must not read as "no work left". Bounded retries across the whole
// run, refilled after any productive object — same policy as auto-session.js.
const MAX_INFRA_RETRIES = 2
// >2 consecutive category failures on one object means the object, not the
// category, is the problem (source-recovery: object-level blocker).
const MAX_CATEGORY_FAILURES = 2
// Guard against a runaway loop when parks/skips keep the goal unreachable.
const MAX_ITERATIONS = GOAL * 4 + 4

// ── Schemas ───────────────────────────────────────────────────────────────────

const S   = { type: 'string' }
const N   = { type: 'number' }
const B   = { type: 'boolean' }
const SL  = { type: 'array', items: S }
const obj = (properties, required) => ({ type: 'object', properties, required })

const GUARD_SCHEMA    = obj({ ok: B, branch: S, dirty: SL, reason: S }, ['ok'])
const SELECT_SCHEMA   = obj({ ok: B, object: S, sourceFiles: SL, score: N, exhausted: B, reason: S }, ['ok'])
const BASELINE_SCHEMA = obj({ ok: B, manifest: S, manifests: SL, reason: S,
  // One numeric pending-count property per ladder category, derived from LADDER so
  // the schema cannot drift from the loop that consumes it.
  categories: obj(LADDER.reduce((a, c) => { a[c.id] = N; return a }, {})) }, ['ok'])
// status: applied | skipped | parked | failed
const CATEGORY_SCHEMA = obj({ status: S, commit: S, appliedCount: N, parkedCount: N, matchNote: S, reason: S }, ['status'])
const FINALIZE_SCHEMA = obj({ ok: B, commits: SL, floorsRaised: N, reason: S }, ['ok'])

// ── Shared agent preamble ─────────────────────────────────────────────────────
// Same worktree discipline as goal-lift's AGENT_RULES: a subagent that cd'd to
// the user's main checkout has silently discarded recovery work before.

const AGENT_RULES =
  `OPERATING RULES (read first):
[WORKTREE] Do ALL work in your CWD with RELATIVE paths. NEVER \`cd\` to another
checkout; never run git mutations against any repo but this one.
[STALL] Any single RE-RUNNABLE long command (full build, vc71 verify, equivalence)
MUST be wrapped so it cannot run silently past 180s and trip the harness stall
detector that kills the whole run:
  timeout 150 <cmd> 2>&1 || echo "[timed-out]"
A timeout is NOT a verdict — re-run the wrapped command; never record it as a
failure while the work may still have completed.
[STALL EXCEPTION — \`git commit\`] NEVER wrap \`git commit\` in \`timeout 150\`, and
never leave it on the default 120s Bash timeout. The pre-commit chain on a large
TU costs ~270s (measured 2026-08-21 on game_engine.c, 237 fns): regression-test
159s + lift-audit 63s + vc71-regression 44s + 12 smaller hooks ~22s. Any ceiling
under that kills the commit mid-hook, every time, forever — it is not a hang and
retrying at the same ceiling cannot succeed. Instead pass the Bash tool's own
\`timeout: 600000\` on the commit call and let it run in the FOREGROUND.
A killed commit is NOT harmless: \`pre-commit-vc71-regression.sh\` auto-stages a
refreshed vc71_scores.json partway through, so a mid-chain kill can leave the
index mutated with no commit. After ANY commit that times out or reports nothing,
read real state before touching anything: \`git log --oneline -1\` (did HEAD move?)
then \`git status --short\` (what is staged now?). Re-stage from that state; never
blind-retry.
[SERIAL] Work in the FOREGROUND, one command at a time. Never drive commits from
background scripts, chained Monitors, or parallel "group drivers", and never run
two git-index-mutating commands at once. Concurrent git add/commit race for
.git/index.lock; the loser aborts while the winner's commit still lands, so you
see a failed command AND a moved HEAD, and mis-read that as a hook bug. This has
already wedged this workflow: the drivers were killed by the background time
limit, re-ran concurrently, aborted on the lock, and the agent then spent 20
minutes polling for events from Monitors it had killed itself. Per unit:
stage -> gate -> commit -> confirm HEAD moved -> next. If you are ever waiting on
a Monitor or a background task, stop waiting and check real state directly
(git log --oneline -1, git status --short, pgrep).
The answer to a SLOW commit is a longer tool timeout (see [STALL EXCEPTION]), not
backgrounding it. \`setsid nohup git commit &\` + a poll loop does eventually land
the commit, but it violates this rule, races the index, and cost one agent 13
minutes on 2026-08-21 before it worked.
[TOKENS] Never re-read a file after a successful edit (Edit confirms it); never
paste large tool output back into your reasoning — extract the one number you need.
`

// ── Helpers ───────────────────────────────────────────────────────────────────

const stemOf = (obj) => String(obj || '').replace(/\.obj$/, '')

// Coarse park-reason class, used ONLY to detect a systemic failure (two objects
// in a row parked for the same kind of reason => stop instead of burning the
// whole queue on one root cause). Substrings first, then the leading word, so an
// unclassified reason still groups with a literally identical one.
const PARK_CLASSES = ['build', 'capture', 'plan', 'manifest', 'ladder',
                      'delink', 'purity', 'agent_null', 'category_failures',
                      'no_applicable_items', 'finalize']
function parkClass(reason) {
  const r = String(reason || 'unknown').toLowerCase()
  for (const c of PARK_CLASSES) if (r.indexOf(c) !== -1) return c
  return r.split(/[\s:,]+/)[0] || 'unknown'
}

// ── Guard ─────────────────────────────────────────────────────────────────────
phase('Guard')
log(`Recover-goal: finish ${GOAL} object(s) (min-funcs ${MIN_FUNCS}${MIN_SCORE !== null ? `, min-score ${MIN_SCORE}` : ''})` +
    `${ALLOW_RISKY ? ', risky ladder opted in' : ''}${DRY_RUN ? ' — DRY RUN (no ledger writes, no commits)' : ''}`)

const guard = await agent(
  `Run \`rtk git status --short\` and \`rtk git branch --show-current\` (read-only) and
report as JSON. Modify nothing.

ok=false, and list the offending lines in \`dirty\`, if ANY modified/staged/untracked
path is under \`src/\`, \`tools/\`, \`.claude/\`, \`recovery/\`, or is \`kb.json\` — a
recovery commit must not pick up someone else's in-flight lift.

TOLERATE (never fail on): \`README.md\`, anything under \`artifacts/\`, and untracked
files OUTSIDE those trees. This repo has a post-commit dashboard hook that rewrites
README.md, so that one is expected noise. \`tools/equivalence/leaf_cache.json\` is
touched by a cron but lives under tools/ and IS a blocker — report it so a human can
commit it first.

Return {ok:true, branch:"<name>", dirty:[]} or
{ok:false, branch:"<name>", dirty:[...], reason:"dirty: <paths>"}.`,
  { label: 'guard', phase: 'Guard', ...MECH, schema: GUARD_SCHEMA })

if (!guard || !guard.ok) {
  const reason = guard ? (guard.reason || `dirty: ${(guard.dirty || []).join(', ')}`) : 'guard_agent_null'
  log(`Guard failed: ${reason}`)
  return {
    objects_finished: 0, objects_parked: 0, objects: [], commits: [],
    stopped_reason: 'guard_failed', park_reason: reason, resumable: false, dry_run: DRY_RUN,
  }
}
log(`Branch: ${guard.branch || '?'} — tree clean for recovery`)

// ── Main loop ─────────────────────────────────────────────────────────────────
phase('Select')

let finished = 0
let parkedObjects = 0
let stoppedReason = 'goal_reached'
let parkReason = null
let resumable = false
let infraRetries = 0
let lastParkClass = null
const rows = []          // one per object attempted
const allCommits = []
const seen = []          // dry-run only: `next` cannot exclude, so detect repeats

// Record a parked object and answer "should the whole run stop?". Two objects in a
// row parked for the same CLASS of reason is systemic (missing delinked refs, a
// broken build, the purity tool absent) — keep going and the ledger just fills with
// parks that hide the one real cause. Called from all three park paths so the
// streak logic exists exactly once.
function recordPark(object, reason, categories, commits) {
  parkedObjects++
  parkReason = reason
  rows.push({ object, status: 'parked', reason, categories: categories || [], commits: commits || [] })
  const cls = parkClass(reason)
  if (lastParkClass === cls) {
    stoppedReason = `systemic_${cls}`
    log(`Two consecutive objects parked for a "${cls}" reason — stopping instead of burning the queue.`)
    return true
  }
  lastParkClass = cls
  return false
}

let iteration = 0
let explicitObject = FIRST_OBJECT

while (finished < GOAL) {
  iteration++
  if (iteration > MAX_ITERATIONS) { stoppedReason = 'iteration_cap'; break }
  log(`\n── Object ${iteration} (finished ${finished}/${GOAL}) ────────────────`)

  // 1. SELECT ────────────────────────────────────────────────────────────────
  // The ledger is what stops the loop re-taking a parked object, so `start` is
  // not optional... except in a dry run, which must leave the ledger untouched.
  const minScoreFlag = MIN_SCORE !== null ? ` --min-score ${MIN_SCORE}` : ''
  const TAKE = DRY_RUN
    ? 'This is a DRY RUN: do NOT run `recovery_goal.py start` — leave the ledger untouched.'
    : `Then take it: \`rtk python3 tools/recovery/recovery_goal.py start <object.obj>\`. If it reports the object is already in progress or done, return {ok:false, reason:"ledger: <message>"} — do NOT pass --force.`
  const selectPrompt = explicitObject
    ? `${AGENT_RULES}
Your object is already chosen: \`${explicitObject}\`. Do NOT run \`recovery_goal.py next\`.
Resolve its source files from the frontier ranking. NOTE \`--json\` takes a PATH and
WRITES the ranking to it — it does not print to stdout:
  \`rtk python3 tools/recovery/recovery_frontier.py --json artifacts/recovery_frontier.json\`
Then read that file and find the \`eligible\` entry whose \`object\` == "${explicitObject}":
\`files\` is the source list, \`score\` the frontier score. Not in \`eligible\` =>
{ok:false, reason:"not_eligible: <the gate it failed>"}.
${TAKE}
Return {ok:true, object:"${explicitObject}", sourceFiles:["src/..."], score:<n>}.`
    : `${AGENT_RULES}
Pick the next recovery target and take it in the ledger.
\`rtk python3 tools/recovery/recovery_goal.py next --min-funcs ${MIN_FUNCS}${minScoreFlag} --json\`
  - EXIT CODE 3 = queue exhausted. Return {ok:false, exhausted:true, reason:"<the JSON's message field>"} and stop.
  - Exit 0 prints {"object":"...","row":{...},"stats":{...}}: \`row.files\` is the array
    of repo-relative .c paths, \`row.score\` the frontier score.
${TAKE}
Return {ok:true, object:"<object.obj>", sourceFiles:["src/..."], score:<row.score>}.
On any other failure return {ok:false, reason:"<what failed>"}.`

  const sel = await agent(selectPrompt,
    { label: `select:${iteration}`, phase: 'Select', ...MECH, schema: SELECT_SCHEMA })

  if (!sel || !sel.ok) {
    const reason = sel ? (sel.reason || 'select_failed') : 'select_agent_null'
    if (sel && sel.exhausted) {
      stoppedReason = 'queue_exhausted'
      parkReason = reason
      log(`Queue exhausted: ${reason}`)
      break
    }
    // Not exhausted => infra (agent died, frontier tool broke). Bounded retry.
    if (infraRetries < MAX_INFRA_RETRIES) {
      infraRetries++
      log(`Select failed (${reason}) — retrying ${infraRetries}/${MAX_INFRA_RETRIES}`)
      continue
    }
    stoppedReason = 'infra_blocked'
    parkReason = `select: ${reason} (after ${infraRetries} retries)`
    resumable = true   // nothing is wrong with the WORK; a resume replays cached agents
    log(`Stopping: ${stoppedReason} (${parkReason})`)
    break
  }

  const object = sel.object
  const stem = stemOf(object)
  const sourceFiles = (sel.sourceFiles && sel.sourceFiles.length) ? sel.sourceFiles : []
  explicitObject = null   // --object applies to the first iteration only

  // `next` has no exclude flag and a dry run never calls `start`, so the queue
  // hands back the same object every time. Detect it rather than loop forever.
  if (DRY_RUN && seen.indexOf(object) !== -1) {
    stoppedReason = 'dry_run_queue_repeat'
    log(`Dry run: \`next\` returned ${object} again (no ledger writes to advance the queue) — stopping`)
    break
  }
  seen.push(object)

  log(`Target: ${object} (score ${sel.score !== undefined ? sel.score : '?'}) — ${sourceFiles.length} source file(s): ${sourceFiles.join(', ') || '(none reported)'}`)
  if (!sourceFiles.length) {
    // Without a source file there is nothing to plan a manifest against.
    const reason = 'select: frontier reported no source files'
    if (!DRY_RUN) await agent(
      `${AGENT_RULES}\nRun exactly: \`rtk python3 tools/recovery/recovery_goal.py park ${object} --reason ${JSON.stringify(reason)}\`. Report its output.`,
      { label: `park:${stem}`, phase: 'Select', ...MECH })
    if (recordPark(object, reason)) break
    continue
  }

  // 2. BASELINE ──────────────────────────────────────────────────────────────
  phase('Recover')
  const baseline = await agent(
    `${AGENT_RULES}

Baseline object ${object} for source recovery. Source file(s): ${sourceFiles.join(' ')}
Do NOT edit any source file and do NOT commit anything.

STEP 1 — build, so a COFF baseline exists to capture against:
  \`rtk python3 tools/build/build.py -q --target halo\`
  This routinely exceeds the default 120s Bash timeout — pass \`timeout: 600000\`. If
  it still exceeds that, re-run with run_in_background and poll the output file; do
  NOT report a timeout as a failure while the build may still be running, and never
  start a second build concurrently. FAILED = an "error:" line or an "Error 2"/"*** "
  marker. A pre-existing broken build is an object-level blocker: return
  {ok:false, reason:"build: <first error line>"} and change nothing.

STEP 2 — for EACH source file, in order (<stem> = basename without \`.c\`, so
src/halo/interface/hud_weapon.c -> recovery/hud_weapon.c.json):
  a. \`rtk python3 tools/recovery/source_recovery.py plan --source <file> -o recovery/<stem>.c.json${ALLOW_RISKY ? ' --allow-risky' : ''}\`
  b. Find its built COFF: \`rtk fd '<stem>.c.obj' build/CMakeFiles\` (normally
     build/CMakeFiles/halo.dir/<source path>.c.obj). If \`fd\` finds NOTHING, do not
     guess a path and do not skip the capture — return
     {ok:false, reason:"capture: no COFF for <file> under build/CMakeFiles"}.
     Several hits => use the one whose path contains the source's directory.
  c. \`rtk python3 tools/recovery/source_recovery.py capture recovery/<stem>.c.json --object <that .obj>\`

STEP 3 — \`source_recovery.py ladder recovery/<stem>.c.json\` for EACH manifest. Lines
look like \` 1 comments  re-comment-capture  pending=12 applied=0 parked=0\`. Sum
\`pending=\` per category ACROSS all manifests.

Return {ok:true, manifest:"<first manifest>", manifests:["recovery/<stem>.c.json", ...],
        categories:{comments:<n>, "local-renames":<n>, "symbol-names":<n>,
                    "global-names":<n>, "const-enum":<n>, "struct-define":<n>,
                    "offset-to-field":<n>, "expr-simplify":<n>,
                    "control-flow":<n>}}  (omit or 0 when a category has no pending items).`,
    { label: `baseline:${stem}`, phase: 'Recover', ...MECH, schema: BASELINE_SCHEMA })

  if (!baseline || !baseline.ok) {
    const reason = baseline ? (baseline.reason || 'baseline_failed') : 'baseline: agent_null'
    log(`Baseline failed for ${object}: ${reason}`)
    if (!DRY_RUN) await agent(
      `${AGENT_RULES}\nRun exactly: \`rtk python3 tools/recovery/recovery_goal.py park ${object} --reason ${JSON.stringify(reason)}\`. Report its output.`,
      { label: `park:${stem}`, phase: 'Recover', ...MECH })
    if (recordPark(object, reason)) break
    continue
  }

  const manifests = (baseline.manifests && baseline.manifests.length)
    ? baseline.manifests
    : (baseline.manifest ? [baseline.manifest] : [])
  const counts = baseline.categories || {}
  const pendingOf = (id) => Number(counts[id] || 0)

  log(`Ladder debt: ${CATEGORIES.map(c => `${c.id}=${pendingOf(c.id)}`).join(' ')}`)
  if (!ALLOW_RISKY) {
    const riskyPending = LADDER.filter(c => c.risky && pendingOf(c.id) > 0)
    if (riskyPending.length) log(`  (risky not opted in — leaving ${riskyPending.map(c => `${c.id}=${pendingOf(c.id)}`).join(' ')} untouched)`)
  }

  // 3. DRY RUN short-circuit ─────────────────────────────────────────────────
  // Nothing above this point wrote the ledger or edited a source file. plan and
  // capture DID write recovery/<stem>.c.json manifests — that is deliberate and
  // is the only tree effect of a dry run.
  if (DRY_RUN) {
    const would = CATEGORIES.filter(c => pendingOf(c.id) > 0)
    log(`Dry run — would run ${would.length} category agent(s) sequentially on ${object}:`)
    for (const c of would) log(`  • ${c.id} (${pendingOf(c.id)} pending) via ${c.skill} → 1 commit`)
    for (const c of CATEGORIES.filter(c => pendingOf(c.id) === 0)) log(`  · skip ${c.id} (0 pending)`)
    log(`  then: vc71_regression update, source_recovery report, recovery_goal finish`)
    rows.push({
      object, status: 'dry_run', reason: 'dry_run',
      categories: would.map(c => ({ category: c.id, status: 'would_run', pending: pendingOf(c.id) })),
      commits: [],
    })
    continue
  }

  // 4. CATEGORY LOOP ─────────────────────────────────────────────────────────
  // SEQUENTIAL, IN ORDER, ONE AGENT EACH. Never parallel: each category's commit
  // is the next category's base, so two agents on one TU collide, and their
  // combined diff fails check_category_purity.py.
  const catRows = []
  const objCommits = []
  let categoryFailures = 0
  let aborted = null

  for (const cat of CATEGORIES) {
    const pending = pendingOf(cat.id)
    if (pending === 0) {
      log(`  · ${cat.id}: 0 pending — skipped (no agent spawned)`)
      catRows.push({ category: cat.id, status: 'skipped', appliedCount: 0, parkedCount: 0 })
      continue
    }

    // Rungs 6/7: put the deterministic path in front of the agent, so it spends
    // its reasoning on the tool's refusals rather than on retyping offsets.
    const mechBlock = !cat.mech ? '' : `

MECHANICAL PATH — this category is transcription, not judgement. Use the tool.
Do NOT hand-edit offsets or hand-write field declarations; a wrong offset is a
wrong decompilation, and the tool refuses exactly where you would be guessing.

STEP 1 — Find the binding. Bindings are in recovery/bindings.json; each maps
a struct name to the base variable name(s) used in source. List them:
  rtk python3 tools/recovery/structize.py discover --binding <id>
If no binding exists for this source file's struct, create one in bindings.json.

STEP 2 — Run structize on this source file (use --binding for automatic
--base/--struct lookup):
  rtk python3 tools/recovery/structize.py run --binding <id> --source <f.c>
This does census → split → re-census → converge in one call. Exit 0 = work done,
1 = failed (file restored), 2 = converged but rewrote nothing.

STEP 3 — Resolve conflicts. \`run\` reports conflicts (offsets accessed at
disagreeing widths/signedness), with \`functions_by_type\` showing which functions
use which width. Use verify-conflict to check the BINARY operand widths:
  rtk python3 tools/recovery/verify_conflict.py --binding <id> --offset 0xNN
This reads the delinked MSVC 7.1 object and reports the ground-truth type.
Verdicts: \`uniform\` = binary proves the type (edit types.h to match);
\`genuine-conflict\` = multiple widths in the binary (union/sub-struct, park it).
Do NOT guess a type the binary does not confirm.

${cat.mech === 'split'
  ? `\`split\` (inside \`run\`) subdivides pad_ runs into field_XX at every offset the
lifted source demonstrably reads. Total span is preserved, so cs()/co() still hold.

YOUR JUDGEMENT GOES INTO THE CONFLICT LIST, not the edits. Conflicts ranked by
how many call sites each unblocks. For each: run \`verify-conflict\` to get the
binary truth, update the field in types.h, re-run \`run\`. Resolving conflicts
is the highest-value work available in this category.`
  : `\`converge\` (inside \`run\`) rewrites every eligible site, compiles, diffs at
FUNCTION granularity, re-applies excluding any function whose code moved, and
proves the rest byte-identical. It restores the file untouched if it cannot
converge, so a failed run is safe.

\`parked_functions\` are NOT your failure: \`#pragma pack(1)\` gives a member
alignment 1 where the original cast asserted natural alignment, which can change
-O3 instruction scheduling and register allocation. Semantically identical, not
byte-identical, so the neutral gate correctly refuses them. Park them with that
reason and move on — do not chase them, and never relax the gate.`}

After resolving conflicts, re-run \`structize.py run\` — resolved offsets
convert automatically, and a stale census hides them.`

    // Category levers that exist in the tools but were being re-derived (and
    // usually mis-derived) by every agent. Each answers a park reason that
    // recurred across many objects in recovery/goal_ledger.json.
    const leverBlock = LEVERS[cat.id] || ''

    const res = await agent(
      `${AGENT_RULES}

You are the \`${cat.id}\` category agent for source recovery of ${object}.${mechBlock}${leverBlock}

READ FIRST, before touching anything:
  1. \`.claude/skills/source-recovery/SKILL.md\` — the ladder, gate table, measurement
     traps, session rules. It is the source of truth: follow it exactly, do not
     reinterpret it, and do not restate its gates from memory.
  2. \`.claude/skills/${cat.skill.split(' + ')[0]}/SKILL.md\`${cat.skill.indexOf('+') !== -1 ? ` and \`.claude/skills/${cat.skill.split(' + ')[1].trim()}/SKILL.md\`` : ''} — this category's leaf skill.

SCOPE — exactly the \`${cat.id}\` items (${pending} pending) in these manifest(s), nothing else:
  ${manifests.join('\n  ')}
Source file(s): ${sourceFiles.join(' ')}
Items in OTHER categories belong to other agents: do not touch them, do not "fix
them while you're here". Enumerate yours with \`source_recovery.py ladder <manifest>\`
plus the manifest JSON (per-item ids and line numbers).

PROCEDURE — small units, gate after EACH, per source-recovery:
  • one small change → that category's gate at the level the gate table specifies
    (\`source_recovery.py check <manifest> --object <built .obj>\` plus whatever extra
    check the table names for this category)
  • pass → \`source_recovery.py set-status <manifest> <item-id> applied\`
  • fail → revert THAT unit, \`set-status <manifest> <item-id> parked --reason "<specific
    why>"\`, continue with the remaining independent items. Parked items are normal.

COMMIT — exactly one, for this category only:
  \`rtk git add -- <source files you touched> <manifest(s)>\`
  \`rtk python3 tools/recovery/check_category_purity.py ${cat.id} --staged\`
    exit 0 = pure; exit 2 = not mechanically checkable (also a PASS); exit 1 = the
    staged diff contains changes outside this category's edit shape → split the commit
    or revert the stray change. NEVER bypass it and never \`--no-verify\`.
  \`rtk git commit -m "recover(${stem}): ${cat.id} — <short summary>"\` → report the short sha.
    Pass \`timeout: 600000\` on this Bash call — the hook chain costs ~270s on a large
    TU and the 120s default kills it mid-hook. See [STALL EXCEPTION] above; do NOT
    wrap it in \`timeout 150\` and do NOT background it.

HARD BANS — these are *lift* work, not recovery. If one is needed, park and say so:
\`@<reg>\` annotations, \`ported\` flags, kb.json signatures, build config, \`--no-verify\`,
and weakening or skipping any gate to make progress.

Return exactly one of:
  {status:"applied", commit:"<sha>", appliedCount:<n>, parkedCount:<n>, matchNote:"<gate mode + score movement>"}
  {status:"parked",  appliedCount:0, parkedCount:<n>, reason:"<why every item parked>"}
  {status:"skipped", reason:"<why the items turned out inapplicable>"}
  {status:"failed",  reason:"<blocker>"}  — only when the WHOLE category cannot proceed
    (build broken, no COFF baseline, manifest errors, purity tool absent).`,
      { label: `recover:${stem}:${cat.id}`, phase: 'Recover', agentType: 'halo-source-recovery', ...RECOVER, schema: CATEGORY_SCHEMA })

    const status = (res && res.status) || 'failed'
    const reason = (res && res.reason) || (res ? '' : 'agent_null')

    if (status === 'applied') {
      categoryFailures = 0
      if (res.commit) { objCommits.push(res.commit); allCommits.push(res.commit) }
      log(`  ✓ ${cat.id}: applied ${res.appliedCount || 0}, parked ${res.parkedCount || 0} → ${res.commit || '(no sha reported)'}${res.matchNote ? ` [${res.matchNote}]` : ''}`)
      catRows.push({ category: cat.id, status, commit: res.commit || null,
                     appliedCount: res.appliedCount || 0, parkedCount: res.parkedCount || 0,
                     matchNote: res.matchNote || null })
      continue
    }

    if (status === 'parked' || status === 'skipped') {
      // A parked or inapplicable category is a real, expected outcome and says
      // nothing about the object: the later categories are independent.
      categoryFailures = 0
      log(`  ◐ ${cat.id}: ${status}${reason ? ` (${reason})` : ''}`)
      catRows.push({ category: cat.id, status, appliedCount: res.appliedCount || 0,
                     parkedCount: res.parkedCount || 0, reason: reason || null })
      continue
    }

    // 'failed' — infra/blocker, not a judgement about the items.
    categoryFailures++
    log(`  ✗ ${cat.id}: failed (${reason}) — ${categoryFailures}/${MAX_CATEGORY_FAILURES} consecutive`)
    catRows.push({ category: cat.id, status: 'failed', reason })
    if (categoryFailures >= MAX_CATEGORY_FAILURES) {
      aborted = `category_failures: ${categoryFailures} consecutive (last: ${reason})`
      break
    }
  }

  // 5. Object-level abort ────────────────────────────────────────────────────
  if (aborted) {
    log(`Aborting ${object}: ${aborted}`)
    // Already-committed categories stay — they passed their own gates. Only the
    // uncommitted edits from the failed category are discarded. Manifest parked
    // statuses are deliberately KEPT: they are the record of what was tried.
    await agent(
      `${AGENT_RULES}
Recovery of ${object} was aborted. Leave every existing COMMIT alone.
1. \`rtk git status --short\` — see what is uncommitted.
2. Discard uncommitted edits to the source file(s) ONLY:
   \`rtk git checkout -- ${sourceFiles.join(' ')}\`
   Do NOT revert the manifest(s) (${manifests.join(' ')}) — their parked statuses are
   the record of what was attempted. Do NOT touch kb.json, tools/, or anything else.
3. \`rtk python3 tools/recovery/recovery_goal.py park ${object} --reason ${JSON.stringify(aborted)}\`
4. \`rtk git status --short\` again; confirm no uncommitted edits remain under src/.
Report the final status output.`,
      { label: `abort:${stem}`, phase: 'Recover', ...MECH })
    if (recordPark(object, aborted, catRows, objCommits)) break
    continue
  }

  // 6. FINALIZE ──────────────────────────────────────────────────────────────
  const appliedCats = catRows.filter(c => c.status === 'applied')
  const fin = await agent(
    `${AGENT_RULES}

Finalize recovery of ${object}. ${appliedCats.length} category(ies) landed a commit${objCommits.length ? `: ${objCommits.join(' ')}` : ''}.
Source file(s): ${sourceFiles.join(' ')}    Manifest(s): ${manifests.join(' ')}
Do NOT edit any source file and never \`--no-verify\`. On failure return
{ok:false, reason:"finalize: <what failed>"}.

${appliedCats.length === 0
  ? `NO category applied anything, so there is nothing to ratchet or record as done:
1. \`rtk python3 tools/recovery/source_recovery.py report <each manifest>\` — highlights.
2. \`rtk python3 tools/recovery/recovery_goal.py park ${object} --reason "no_applicable_items: every ladder category was skipped or fully parked"\`
Return {ok:true, commits:[], floorsRaised:0, reason:"no_applicable_items"}.`
  : `1. Ratchet the VC71 floors so this session's gains cannot be silently given back:
   \`rtk python3 tools/verify/vc71_regression.py update --source ${sourceFiles.join(' ')}\`
   It compiles with VC71 — pass \`timeout: 600000\` or background+poll. It RAISES floors
   for improved functions and never lowers them. Report how many it raised (0 is a
   perfectly normal answer for the neutral categories).
2. \`rtk python3 tools/recovery/source_recovery.py report <each manifest>\` — the deliverable.
3. Commit the manifest(s) and any floor-file change:
   \`rtk git add -- ${manifests.join(' ')} tools/verify/vc71_scores.json\` (drop
   vc71_scores.json if unchanged). If \`rtk git diff --cached --quiet\` says nothing is
   staged, SKIP the commit; else \`rtk git commit -m "recover(${stem}): session report"\`.
4. \`rtk python3 tools/recovery/recovery_goal.py finish ${object} ${objCommits.map(s => `--commit ${s}`).join(' ')} ${appliedCats.map(c => `--category ${c.category}`).join(' ')}\`
   (append \`--commit <sha>\` for the session-report commit too, if you made one)
Return {ok:true, commits:["<every sha you passed to finish>"], floorsRaised:<n>}.`}`,
    { label: `finalize:${stem}`, phase: 'Recover', ...MECH, schema: FINALIZE_SCHEMA })

  const finCommits = (fin && fin.commits) || []
  for (const sha of finCommits) if (allCommits.indexOf(sha) === -1) allCommits.push(sha)
  const floors = (fin && fin.floorsRaised) || 0

  if (appliedCats.length === 0) {
    // Every category was skipped or fully parked: the finalize agent parked the
    // object in the ledger rather than recording a hollow "done".
    log(`◐ ${object}: parked — no_applicable_items`)
    if (recordPark(object, 'no_applicable_items', catRows)) break
    continue
  }

  if (!fin || !fin.ok) {
    // The category commits are already landed and gated; only the bookkeeping
    // failed. Surface it rather than silently counting the object as finished.
    const reason = fin ? (fin.reason || 'finalize_failed') : 'finalize: agent_null'
    log(`⚠ ${object}: ${appliedCats.length} category commit(s) landed but finalize failed (${reason})`)
    rows.push({ object, status: 'unfinalized', reason, categories: catRows,
                commits: objCommits, floorsRaised: floors })
    stoppedReason = 'finalize_failed'
    parkReason = reason
    break
  }

  finished++
  infraRetries = 0      // a productive object refills the infra-retry budget
  lastParkClass = null  // ...and breaks any systemic-park streak
  log(`✓ ${object} recovered — ${appliedCats.length} category commit(s), ${floors} floor(s) raised`)
  rows.push({ object, status: 'finished', categories: catRows,
              commits: objCommits.concat(finCommits.filter(s => objCommits.indexOf(s) === -1)),
              floorsRaised: floors })
}

// ── Report ────────────────────────────────────────────────────────────────────
phase('Report')

const ledger = await agent(
  `${AGENT_RULES}
Run exactly \`rtk python3 tools/recovery/recovery_goal.py status\` and report its
table verbatim as plain text. Run nothing else and change nothing.`,
  { label: 'ledger-status', phase: 'Report', ...MECH })

log(`\n── Recover-goal complete ─────────────────`)
for (const r of rows) {
  const cats = (r.categories || [])
    .map(c => `${c.category}:${c.status === 'would_run' ? `would(${c.pending})` : c.status}` +
              (c.appliedCount ? `+${c.appliedCount}` : '') +
              (c.parkedCount ? `-${c.parkedCount}` : ''))
    .join(' ')
  log(`${r.status === 'finished' ? '✓' : r.status === 'dry_run' ? '○' : '◐'} ${r.object}` +
      `${r.reason && r.status !== 'finished' ? ` [${r.reason}]` : ''}`)
  if (cats) log(`    ${cats}`)
  if (r.commits && r.commits.length) log(`    commits: ${r.commits.join(' ')}`)
  if (r.floorsRaised) log(`    floors raised: ${r.floorsRaised}`)
}
log(`Objects finished: ${finished}/${GOAL}`)
log(`Objects parked:   ${parkedObjects}`)
log(`Commits:          ${allCommits.length}`)
log(`Stop reason:      ${stoppedReason}`)
if (parkReason) log(`Park reason:      ${parkReason}`)
if (DRY_RUN) log(`Dry run: no ledger writes and no commits. The only tree effect is the recovery/<stem>.c.json manifest(s) written by plan/capture.`)
if (ledger) log(`\n${typeof ledger === 'string' ? ledger : JSON.stringify(ledger)}`)
if (budget.total) log(`Budget remaining: ~${Math.round(budget.remaining() / 1000)}k tokens`)

return {
  objects_finished: finished,
  objects_parked: parkedObjects,
  objects: rows,
  commits: allCommits,
  stopped_reason: stoppedReason,
  park_reason: parkReason,
  // True only when the run died on infra (agent returned null / API outage), i.e.
  // resuming replays the succeeded agents from cache instead of re-paying for
  // them. Wait for the outage to clear first — an immediate resume just re-fails.
  resumable,
  dry_run: DRY_RUN,
}
