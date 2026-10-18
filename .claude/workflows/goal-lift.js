export const meta = {
  name: 'goal-lift',
  description: 'Goal-mode auto-lift: lift, verify, review-gate, and commit Halo CE Xbox functions until N are committed at >=90% VC71 or the queue is exhausted',
  phases: [
    { title: 'Select',   detail: 'Frontier scoring + liftability filtering' },
    { title: 'Research', detail: 'Fingerprint validation + mechanical evidence prefetch' },
    { title: 'Lift',     detail: 'Serial: lift -> verify (goal90 bands) -> permute/escalate -> mechanical/review gate -> commit' },
    { title: 'Improve',  detail: 'Re-lift parked sub-bar functions with a different model, promote or re-park' },
    { title: 'Report',   detail: 'Summary: committed, skipped, parked, gate holds' },
  ],
}

// This workflow exists so that the two required subagents are GUARANTEED to
// run: a prose-only slash command can't force tool calls, but agent() here is
// real code. The dedicated auto-lift profiles are memoryless; manual RE keeps
// the existing project-memory-enabled profiles.

const GOAL         = (args && args.goal) || 20
// Raised 3→6 2026-09-01: structural caps (rungCapped/treatAsCapped) counted
// as fails even though they're deterministic, not model error — starved
// runs early on TUs with a cluster of capped fns. Both cap branches now try
// equivalence-commit first (capEquivCommit) and skip the fail-counter on a
// confirmed high-confidence cap either way.
const STOP_ON_FAIL = (args && args.stopOnFail) || 6
const DRY_RUN      = !!(args && args.dryRun)
// --improve: skip Select/Research/Lift over the frontier; instead drain the
// parked ledger (artifacts/parked/), re-lifting sub-bar functions with the
// improve model for perspective diversity. This is the payoff of never
// discarding sub-bar work — see tools/lift/park.py.
const IMPROVE      = !!(args && args.improve)
// Model/effort selection (single point of control).
// - Reasoning stages (select, lift, review) use Opus low/medium/high.
//   Override with --extractModel / --reasonModel.
// - Cheap deterministic tool-runs (revert, permute-run, equiv-run, redelink,
//   park, report) use Haiku-low.
// - The escalation / improve tune follows REASON_MODEL (so: Opus by default),
//   climbing reasoning EFFORT (ladder high -> xhigh, for every improve model —
//   see IMPROVE_EFFORTS below). Fable is opt-in only (--improveModel fable).
// Provider/model names are opaque workflow arguments, so switching models is a
// routing change, not a forked workflow or lost attempt history.
const MECHANICAL_MODEL = (args && args.mechanicalModel) || 'haiku'
const EXTRACT_MODEL = (args && args.extractModel) || 'opus'
const REASON_MODEL = (args && args.reasonModel) || 'opus'
const COMMIT_MODEL = (args && args.commitModel) || MECHANICAL_MODEL
const IMPROVE_MODEL = (args && args.improveModel) || REASON_MODEL
// Effort ladder for the in-place score tune. Each rung re-runs the optimizer at
// a higher effort, but only for a target still below the pass bar, not capped,
// and while budget remains. The match-optimizer runs at
// 'high' and 'xhigh' only. --improveEfforts may reorder or drop a rung (e.g.
// "xhigh"); any other effort is discarded.
const IMPROVE_EFFORTS_OK = ['high', 'xhigh']
const IMPROVE_EFFORTS = (() => {
  if (!(args && args.improveEfforts)) return IMPROVE_EFFORTS_OK.slice()
  const raw = String(args.improveEfforts).split(',').map(s => s.trim()).filter(Boolean)
  const kept = raw.filter(e => IMPROVE_EFFORTS_OK.includes(e))
  if (kept.length !== raw.length) {
    log(`--improveEfforts: dropped ${raw.filter(e => !kept.includes(e)).join('/')} (allowed: ${IMPROVE_EFFORTS_OK.join('/')})`)
  }
  return kept.length ? kept : IMPROVE_EFFORTS_OK.slice()
})()
// Do NOT start an escalation rung below this remaining-budget floor (an
// optimizer run can be sizable). Higher than the batch-loop floor (80k) so a
// rung never strands the loop. Override with --escalationBudgetFloor.
const ESCALATION_BUDGET_FLOOR = (args && args.escalationBudgetFloor) || 120000
// Max targets that may enter the escalation ladder per goal-lift run, bounding
// token blast radius regardless of how many land in [65,85). 0 = unlimited.
// Override with --maxEscalations.
const MAX_ESCALATIONS = (args && args.maxEscalations != null) ? args.maxEscalations : 3
// Adaptive ladder: after this many escalations IN A ROW end without clearing
// the pass bar, later escalations run only the first (cheapest) rung. The
// ladder's xhigh/max rungs are the most expensive calls in the run, and a
// streak of misses means the frontier is in a capped tail where they rarely pay
// (runs committing 1-2 fns cost 1.2-3M tokens/fn in campaigns.jsonl). A clear
// resets the streak. 0 = never shorten. Override with --escalationMissLimit.
const ESCALATION_MISS_LIMIT = (args && args.escalationMissLimit != null) ? Number(args.escalationMissLimit) : 2
// --perCommitBuild: force the commit agent's full halo build on EVERY commit
// (the pre-2026-09-30 behavior). By default the build is skipped when the
// lift's own lift_pipeline run already built this exact tree (pass1 / permute
// paths — lift_pipeline's build step IS `build.py -q --target halo`), and one
// batch-end build (buildCheckAndBisect) backstops the whole run.
const PER_COMMIT_BUILD = !!(args && args.perCommitBuild)
const M = {
  mechanical: { model: MECHANICAL_MODEL, effort: 'high'  },  // tool-run + parse
  // Selection and one-shot score levers still need constrained judgment.
  // Per-target research is mechanical and routes through M.mechanical below.
  extract:    { model: EXTRACT_MODEL, effort: 'low'  },  // select + classified score lever
  // Commit runs a fixed 6-command script whose only judgement is "does the
  // build log contain an error: line" -- the same shape as the 19 sites already
  // on `mechanical`.
  commit:     { model: COMMIT_MODEL, effort: 'high'  },  // runs the clean-build gate
  reason:     { model: REASON_MODEL, effort: 'medium' },  // lift, review
  improve:    { model: IMPROVE_MODEL, effort: IMPROVE_EFFORTS[0] },  // improve-pass base rung
}
// --reviewEffort: A/B lever for reviewer cost (docs/history/agent-model-routing-2026-08.md
// §6/§7.4). The review gate is fail-closed CLASSIFICATION of evidence that other
// tools already produced (VC71/objdiff/hazard/ABI -> AUTO_ACCEPT | NEEDS_RUNTIME |
// REJECT), not open-ended reasoning, so it may not need M.reason's 'high'. Default
// stays 'high': measure false-accept/false-reject over a full session before
// lowering it — one bad accept costs far more than the effort saved.
const REVIEW_EFFORTS_OK = ['low', 'medium', 'high', 'xhigh', 'max']
const REVIEW_EFFORT = (() => {
  const raw = (args && args.reviewEffort) ? String(args.reviewEffort).trim() : 'high'
  if (!REVIEW_EFFORTS_OK.includes(raw)) {
    log(`--reviewEffort "${raw}" is not one of ${REVIEW_EFFORTS_OK.join('/')} — using high`)
    return 'high'
  }
  return raw
})()

// agent() is documented to return null once the harness has exhausted its own
// internal retries after a terminal API error -- every null-check downstream
// (Select's retry loop above, lift1's infra_blocked handling, improve-lift's
// infra_blocked handling) is written against that contract. But a subagent
// that finishes its turn WITHOUT calling the required StructuredOutput tool
// throws instead of returning null, bypassing all of that and crashing the
// whole workflow run. Observed exclusively on the auto-lift-analyst lift
// calls (lift1, improve-lift) 2026-08-25/26 -- 3 of 5 campaign runs killed by
// it, mid-batch, after several functions had already committed. Route those
// two call sites through this wrapper: one immediate retry (workflow scripts
// have no sleep primitive), then degrade to null like any other agent death
// so the existing infra_blocked handling takes over instead of crashing.
//
// Widened 2026-09-02: the crash is not specific to the lift calls -- ANY
// schema-bearing agent() can miss its StructuredOutput call and take the whole
// run down mid-batch. EVERY call site that passes `schema:` now goes through
// here, and each caller already treats null as "this step produced nothing"
// (see the null-checks immediately after each one).
async function schemaAgent(prompt, opts, retries) {
  retries = retries == null ? 1 : retries
  for (let attempt = 0; attempt <= retries; attempt++) {
    try {
      return await agent(prompt, opts)
    } catch (e) {
      const msg = String((e && e.message) || e)
      if (!/StructuredOutput/.test(msg)) throw e
      if (attempt === retries) {
        log(`${(opts && opts.label) || 'agent'}: StructuredOutput failure, out of retries — returning null`)
        return null
      }
      log(`${(opts && opts.label) || 'agent'}: StructuredOutput failure (attempt ${attempt + 1}/${retries + 1}) — retrying`)
    }
  }
}

// --objects: hard allowlist, enforced in code (not just prompted) — see the
// filter applied to selection.targets below.
// --criteria: freeform string appended to the Select prompt; the agent is
// asked to honor it but nothing in code enforces it.
const OBJECTS = (() => {
  const raw = args && args.objects
  if (!raw) return null
  const arr = Array.isArray(raw) ? raw : String(raw).split(',')
  const norm = arr.map(s => s.trim()).filter(Boolean)
  return norm.length ? norm : null
})()
const CRITERIA = (args && args.criteria) ? String(args.criteria).trim() : null

// --addrs: hard pin-list of target addresses (enforced in code after selection,
// like OBJECTS). Solves "low-scoring fresh function never surfaces in top-N".
const ADDRS = (() => {
  const raw = args && args.addrs
  if (!raw) return null
  const arr = Array.isArray(raw) ? raw : String(raw).split(',')
  const norm = arr.map(s => parseInt(String(s).trim().replace(/^0x/i, ''), 16))
    .filter(Number.isFinite)
  return norm.length ? new Set(norm) : null
})()
// --excludeAddrs: cross-batch dedupe. auto-session runs goal-lift in BATCHES
// within one session, and nothing was shared between them, so a target that
// batch 1 already attempted (and parked) was re-selected and re-lifted by
// batch 2. auto-session collects each batch's `attempted` list (returned
// below) and passes the union back in here; every address in it is dropped in
// the code-side pre-screen. Same input shape as --addrs.
const EXCLUDE_ADDRS = (() => {
  const raw = args && args.excludeAddrs
  if (!raw) return null
  const arr = Array.isArray(raw) ? raw : String(raw).split(',')
  const norm = arr.map(s => parseInt(String(s).trim().replace(/^0x/i, ''), 16))
    .filter(Number.isFinite)
  return norm.length ? new Set(norm) : null
})()
// Canonical lowercase 0x form for the `attempted` contract above.
const normAddr = a => {
  const n = parseInt(String(a || '').trim().replace(/^0x/i, ''), 16)
  return Number.isFinite(n) ? '0x' + n.toString(16) : null
}
// --liftRegArgs: lift @<reg>-defined/reg-arg targets instead of pre-screen
// dropping them. The lift phase already handles @reg (CALLEE PREP step 2 +
// @<reg> step 4); the VC71 comparator models the @reg-DEFINED prologue
// (lift-learnings §32), so scores are honest. Proven 2026-07-14 (players.obj
// 11/13) via a hand-edited copy; this makes it a first-class flag.
const LIFT_REG_ARGS = !!(args && args.liftRegArgs)
// --liftManual: also admit lane=="manual-lift" targets. The selector's
// auto-lift lane is a small slice of the frontier (2026-10-14: 18 of the top
// 200, vs 46 manual-lift), so without this the queue starves after the
// pre-screens. The reg-arg / parked / CRT / XAPILIB pre-screens still apply.
const LIFT_MANUAL = !!(args && args.liftManual)
// --minCommitScore: floor for the mechanical NEEDS_RUNTIME+equiv acceptance
// in reviewThenCommit (default 85, the historical lane). Set 88 to enforce
// "no sub-88 equiv-backed commits" policy.
const MIN_COMMIT = Number((args && args.minCommitScore) || 85)
// --minCappedCommitScore: floor for capEquivCommit — the equivalence-backed
// commit path for structurally-capped lifts (rungCapped/treatAsCapped) that
// never reach reviewThenCommit because their score sits under 85. Added
// 2026-09-01: investigation found the 65-84% cap band had NO equivalence
// fallback anywhere in the pipeline, so every capped lift in that band
// parked even when equivalence would have confirmed it correct.
const MIN_CAPPED_COMMIT_SCORE = Number((args && args.minCappedCommitScore) || 65)
const runTokenStart = budget.spent()
const phaseTokens = { select: 0, research: 0, lift: 0, improve: 0, report: 0 }
const cacheMetrics = { hits: 0, misses: 0, ghidra_builds: 0 }

// ── Schemas ───────────────────────────────────────────────────────────────────

// Facts come straight from llm_auto_lift.py select --json (authoritative). The
// code-side pre-screen below uses has_reg_args / lane / addr to drop unsuitable
// targets BEFORE research, instead of re-deriving them in 6 Opus research agents
// (which drift). See LiftTarget/SelectedTarget in tools/lift/llm_auto_lift.py.
const TARGETS_SCHEMA = {
  type: 'object',
  properties: {
    targets: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          addr:          { type: 'string' },
          name:          { type: 'string' },
          obj:           { type: 'string' },
          score:         { type: 'number' },
          has_reg_args:  { type: 'boolean' },  // target.has_reg_args
          // VC71-scoreable: score_details.delinked_ref > 0. The key kept its
          // historical name, but the selector now derives it from bounds-table
          // membership (tools/verify/function_bounds.json), not from delinked/.
          delinked:      { type: 'boolean' },
          source_exists: { type: 'boolean' },  // target.score_details.source_exists present
          lane:          { type: 'string' },   // item.lane
          // Prior-attempt state. These MUST be declared here even though the
          // select prompt already asks for them: a field absent from the schema
          // is dropped from the structured output, so the code-side pre-screen
          // reads undefined and its skip silently never fires. `prior_fail` sat
          // in exactly that state -- prompted for, used at the pre-screen, and
          // dead for as long as the schema omitted it.
          prior_fail:         { type: 'boolean' },  // item.prior_fail
          parked_attempts:    { type: 'number'  },  // park.py ledger: attempts so far
          parked_best_score:  { type: 'number'  },  // park.py ledger: best VC71 seen
          parked_status:      { type: 'string'  },  // park.py ledger: parked|promoted|blocked_review|...
        },
        required: ['addr', 'name', 'obj'],
      },
    },
  },
  required: ['targets'],
}

// P2 liftability gate — one cheap mechanical agent stamps each candidate so the
// workflow can drop non-liftable targets before any research/lift agent spawns.
const LIFTABILITY_SCHEMA = {
  type: 'object',
  properties: {
    classified: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          addr:           { type: 'string' },
          liftable_class: { type: 'string' },  // liftable | fragment | known_cap
          class_reason:   { type: 'string' },
        },
        required: ['addr', 'liftable_class'],
      },
    },
  },
  required: ['classified'],
}

const BUNDLE_SCHEMA = {
  type: 'object',
  properties: {
    addr:            { type: 'string' },
    name:            { type: 'string' },
    obj:             { type: 'string' },
    source_path:     { type: 'string' },
    fingerprint:     { type: 'string' },
    attempt_fingerprint: { type: 'string' },
    artifacts: {
      type: 'object',
      properties: { ghidra: { type: 'string' }, score_context: { type: 'string' } },
    },
    artifact_paths: {
      type: 'object',
      properties: { ghidra: { type: 'string' }, score_context: { type: 'string' } },
    },
    verdicts: { type: 'array', items: { type: 'object', properties: {
      rule_id: { type: 'string' }, verdict: { type: 'string' },
      confidence: { type: 'string' }, evidence: { type: 'array', items: { type: 'string' } },
    } } },
    cache: { type: 'object', properties: {
      context: { type: 'string' }, ghidra_builds: { type: 'number' },
    } },
    retrieval: { type: 'object', properties: {
      cohort: { type: 'string' }, neighbor_ids: { type: 'array', items: { type: 'string' } },
      neighbor: { type: 'object', properties: {
        addr: { type: 'string' }, name: { type: 'string' },
        decl: { type: 'string' }, c_source: { type: 'string' },
      } },
    } },
    pre_screen:      { type: 'string' },
    skip_reason:     { type: 'string' },
  },
  required: ['addr', 'name', 'pre_screen'],
}

const LIFT_RESULT_SCHEMA = {
  type: 'object',
  properties: {
    addr:        { type: 'string' },
    name:        { type: 'string' },
    status:      { type: 'string' },   // needs_verify | skipped | build_failed | infra_blocked
    source_file: { type: 'string' },
    vc71_score:  { type: 'number' },
    // false when vc71_verify never produced a per-function %. Since references
    // are derived from the pristine XBE + the committed bounds table, that now
    // means one of: the address is absent from tools/verify/function_bounds.json,
    // or the TU failed to compile under VC71 — NOT a missing Ghidra export. (The
    // legacy strings "no delinked reference" / "No usable objdiff.json unit" are
    // still treated the same way if they ever appear.) A skipped verify is an
    // infrastructure gap, NOT a 0% lift — the loop must not park it as
    // below_65pct or count it toward stop_on_fail (see f8e29209/daa39ee6:
    // 9 faithful lifts parked at "0%" across two runs).
    vc71_measured: { type: 'boolean' },
    capped:         { type: 'boolean' },  // matches a known structural-cap signature (see liftPrompt)
    cap_reason:     { type: 'string' },
    cap_confidence: { type: 'string' },    // high (deterministic classify_cap.py) | inconclusive (agent judgment)
    // In-agent follow-up stages (liftPrompt steps 6b-6d): the lift agent does
    // redelink, permute, and state-snapshot equivalence itself, in the same
    // context that produced the lift. The loop's separate redelink/permute/
    // equiv agents only run as FALLBACK when these fields are absent.
    redelinked:       { type: 'boolean' },   // retired: no export can change a VC71 score; always false
    permuted:         { type: 'boolean' },
    equiv_passes:     { type: 'boolean' },
    equiv_confidence: { type: 'string' },
    equiv_reason:     { type: 'string' },
    reason:         { type: 'string' },
  },
  required: ['addr', 'name', 'status'],
}

const SCORE_SCHEMA = {
  type: 'object',
  properties: {
    vc71_score: { type: 'number' },
    improved:   { type: 'boolean' },
    reason:     { type: 'string' },
  },
  required: ['vc71_score'],
}

// SCORE_SCHEMA plus an explicit structural-cap flag, so the match-optimizer
// escalation (below) can tell "escalation_exhausted" apart from "hit a
// documented ceiling" without inventing new SCORE_SCHEMA fields.
const MATCH_OPTIMIZER_SCHEMA = {
  type: 'object',
  properties: {
    vc71_score: { type: 'number' },
    improved:   { type: 'boolean' },
    capped:     { type: 'boolean' },
    cap_reason: { type: 'string' },
    cap_confidence: { type: 'string' },
    reason:     { type: 'string' },
  },
  required: ['vc71_score'],
}

const EQUIV_SCHEMA = {
  type: 'object',
  properties: {
    passes:     { type: 'boolean' },
    confidence: { type: 'string' },
    coverage:   { type: 'number' },
    reason:     { type: 'string' },
  },
  required: ['passes'],
}

const REVIEW_SCHEMA = {
  type: 'object',
  properties: {
    verdict:              { type: 'string' },  // AUTO_ACCEPT | NEEDS_RUNTIME | REJECT
    rationale:             { type: 'string' },
    mismatch_classes:      { type: 'string' },
    call_argument_audit:   { type: 'string' },
    memory_offset_audit:   { type: 'string' },
    abi_audit:             { type: 'string' },
    blocked_by_callee:     { type: 'string' },  // REJECT only: callee addr whose kb.json decl is the sole blocker
  },
  required: ['verdict', 'rationale'],
}

// Mechanical pre-commit gate — cheap Haiku scan that lets clean high-% lifts
// commit WITHOUT the Opus-high reviewer (the user cares about VC71 %, not a
// prose code review). Only ambiguous/flagged lifts still pay for the reviewer.
const MECH_GATE_SCHEMA = {
  type: 'object',
  properties: {
    hazards_clean: { type: 'boolean' },  // no HIGH-RISK/ERROR hazard in the touched file
    abi_clean:     { type: 'boolean' },  // audit_reg_abi passes for this function
    risky_calls:   { type: 'boolean' },  // lift ADDED a raw fn-ptr cast / XCALL-to-ported / inline __asm
    warns:         { type: 'boolean' },  // any WARN-level hazard in the touched file
    detail:        { type: 'string' },
  },
  required: ['hazards_clean', 'abi_clean'],
}

// park.py next → the closest-to-bar parked function the improve model hasn't tried.
const NEXT_SCHEMA = {
  type: 'object',
  properties: {
    found:         { type: 'boolean' },
    name:          { type: 'string' },
    addr:          { type: 'string' },
    obj:           { type: 'string' },
    source_path:   { type: 'string' },
    best_score:    { type: 'number' },
    attempts:      { type: 'number' },
    tried_models:  { type: 'string' },
    last_notes:    { type: 'string' },
    tried_summary: { type: 'string' },
  },
  required: ['found'],
}

const APPLY_SCHEMA = {
  type: 'object',
  properties: {
    applied: { type: 'boolean' },
    reason:  { type: 'string' },
  },
  required: ['applied'],
}

const SQUASH_SCHEMA = {
  type: 'object',
  properties: {
    ok:       { type: 'boolean' },
    squashed: { type: 'number' },   // count of object-stretches actually collapsed
    shas:     { type: 'array', items: { type: 'string' } },
    reason:   { type: 'string' },   // set when ok=false
  },
  required: ['ok'],
}


// ── Prompt builders ───────────────────────────────────────────────────────────

const AGENT_RULES =
  `OPERATING RULES (read first):
[WORKTREE] If your CWD is a git WORKTREE, do ALL work here with RELATIVE paths.
NEVER \`cd\` to the user's main checkout (the repo root outside your worktree) and NEVER run git
mutations (stash/checkout/commit/reset) against any repo but this one.
mcp__ghidra-live__export_delinked_object writes its .obj to the MAIN repo
delinked/ (a Windows-form path under the main checkout); if you are in a worktree,
COPY the exported file into THIS worktree instead of cd-ing to main.
[STALL] Any single RE-RUNNABLE long command (permuter, unicorn equivalence, a
full clean build) MUST be wrapped so it cannot run silently past 180s and trip
the harness stall detector that kills the whole run:
  timeout 150 <cmd> 2>&1 || echo "[timed-out]"
[STALL EXCEPTION — \`git commit\`] NEVER wrap \`git commit\` in \`timeout 150\` and
never leave it on the default 120s Bash timeout. The pre-commit chain still
outruns it: measured 2026-08-21 at ~270s on a large TU (regression-test 159s +
lift-audit 63s + vc71-regression 44s + 12 smaller hooks ~22s). Two of those have
since been cut -- lift-audit runs --gate-only and the regression differential is
deferred to the land gate by the HALO_BATCH_COMMIT=1 prefix below -- but
vc71-regression alone still exceeds 120s on a large TU. Any ceiling under the
real cost kills the commit mid-hook every time, and retrying at the same ceiling
cannot succeed.
Pass the Bash tool's \`timeout: 600000\` and run it in the FOREGROUND — do not
background it. \`pre-commit-vc71-regression.sh\` auto-stages a refreshed
vc71_scores.json partway through, so a killed commit can leave the index mutated
with no commit: after any commit that times out, check \`git log --oneline -1\`
then \`git status --short\` and re-stage from that state rather than blind-retrying.
[TOKENS] Your ENTIRE context is re-read on every turn, so token cost grows with
turn count (a long agent costs quadratically; a short one is linear). Minimize
turns and quoted volume: do NOT re-read a file after a successful edit (the Edit
tool already confirms success), do NOT re-run a command just to re-check, and do
NOT paste large tool output (build logs, full objdiff, decompile dumps) back into
your reasoning — pull out only the specific line or number you need and move on.
`

// P2: classify candidates as liftable / fragment / known_cap using live Ghidra
// xref shape (fragment) + the parked ledger (known_cap). One mechanical agent,
// one batched get_bulk_xrefs call. Fail-open: if Ghidra can't answer, return
// classified=[] and the workflow proceeds without dropping anything.
const liftabilityGatePrompt = (targets) =>
  `Classify goal-lift candidates as liftable / fragment / known_cap so the workflow
can drop non-liftable ones BEFORE spawning expensive research/lift agents. READ-ONLY.

Candidates (addr — name):
${targets.map(t => `  ${t.addr} — ${t.name}`).join('\n')}

STEPS:
1. Get xref shapes for ALL candidate addresses. First try one batched call:
   mcp__ghidra__get_bulk_xrefs addresses="${targets.map(t => t.addr).join(',')}" program="cachebeta.xbe"
   If that tool errors, fall back to mcp__ghidra__get_xrefs_to per address.
   If EVERY Ghidra call fails → return {"classified": []} (fail-open; do not guess).
2. Build a JSON object mapping each "0x..." addr to its raw xref result text, and
   write it: printf '%s' '<json>' > /tmp/glift_xrefs.json
3. Write the candidate array to /tmp/glift_cands.json:
   printf '%s' '${JSON.stringify(targets.map(t => ({ addr: t.addr, name: t.name })))}' > /tmp/glift_cands.json
4. Run: rtk python3 tools/analysis/classify_liftability.py --candidates /tmp/glift_cands.json --xrefs /tmp/glift_xrefs.json
5. Return classified = the script's JSON array verbatim (each element has addr,
   liftable_class, and class_reason). Do NOT invent classes — copy the script output.`

const bundlePrompt = (t, currentAttempt = false) =>
  `Prepare the fingerprinted mechanical evidence bundle for ${t.name} at ${t.addr}.
Run exactly this command and return its JSON object verbatim; do not summarize it:
timeout 300 rtk python3 tools/lift/research_bundle.py prepare --target ${JSON.stringify(t.addr)} --json${LIFT_REG_ARGS ? ' --allow-reg-args' : ''}${currentAttempt ? ' --current-attempt' : ''}

The tool performs the live source check, KB extraction, fingerprint validation,
structural pre-screen, cap classification, and stale-only Ghidra build. A cache hit
must not be supplemented with any Ghidra call. If the command fails, return
pre_screen="infra_blocked" and its first error line as skip_reason.`

const CAP_TABLE =
  `Known structural-cap patterns (report capped=true with cap_reason if the VC71
gap matches one of these, rather than treating it as a fixable bug):
- Register args (@eax/@esi callers): ~65-80% ceiling
- Trivial tail-call wrapper: ~40% ceiling
- MSVC ternary/log scheduling difference: ~87% ceiling
- MSVC loop-unroll vs rep stosd: ~65-70% ceiling
- @<reg>-defining function's own prologue: permanent sub-bar (VC71 can't emit it)
- fucompp vs fcomps / int16 movswl / fcos/fsin spill: permanent ~15pp gap, not a bug
- Float-arg lowering (classify_cap.py R4 proves this mechanically when
  --score-context is available — prefer that over eyeballing): a forwarded
  float lvalue arg is marshalled by the reference via x87 push-then-store
  (subl esp,N / flds / fstps) but by our clang build via a plain GPR dword
  copy (movl / pushl) — bit-identical value, different instruction sequence.
   Cost is ~1 ref instruction per FSTP-slot float: ~94% for 1 float, ~83.6%
   for 2-3 floats (measured across the hs.obj forwarding-handler family).
   Not reachable by re-spelling the load (struct field, int* pun, volatile,
   double round-trip all measured zero movement) — do not re-attempt those.
- Float-equality assert (classify_cap.py R5): assert_halt_msg_at(x != 0.0f)
   is reference fcomps[0.0]+jp vs candidate flds+fucompp+bool-materialize.
   ~86% (shader_environment_texture_animation_evaluate). FCOM-WARN / loadw
   on this shape are false leads. Permuter 0.00pp. Do not re-spell.`

const liftPrompt = (brief, isEscalation, priorScore, warmStarted, priorNotes) =>
  `${AGENT_RULES}

Lift ${brief.name} at ${brief.addr} from Halo CE Xbox (cachebeta.xbe).
Object: ${brief.obj} | Source: ${brief.source_path}
${isEscalation ? `\nESCALATION (prior score ${priorScore}%): focus on FPU operand order, buffer-alias confusion, PUSH trace per CALL, struct field rotation.\n` : ''}${warmStarted ? `
WARM START — READ THIS BEFORE WRITING ANY CODE.
${brief.source_path} ALREADY CONTAINS the best prior attempt for ${brief.name},
scoring ${priorScore}%. It was restored into the tree for you. It embodies fixes
that were individually measured and are easy to lose by accident.

  - Read the EXISTING implementation in ${brief.source_path} first. That body,
    not the Ghidra decompile below, is your starting point.
  - Make TARGETED EDITS to it. Do NOT rewrite it from the decompile. A rewrite
    that happens to score lower is a regression, and re-deriving has repeatedly
    scored WORSE than the patch it replaced (one function went
    91.8% -> 86.8% over six such re-rolls, losing a verified load-width fix and a
    named-call conversion each time).
  - The decompile/disasm in CONTEXT is REFERENCE for the specific defect you are
    fixing — not a template to retype.
  - Change ONE thing per measurement. Re-run VC71 after each change. ${priorScore}% is
    the floor: if an edit drops the score, revert THAT edit and try the next
    hypothesis. Never submit below ${priorScore}%.
  - If prior review notes name an exact next step, do that step and nothing else.
` : ''}${priorNotes && priorNotes.notes ? `
PRIOR ATTEMPT NOTES (from earlier attempts on this function — read before coding):
${priorNotes.notes}
Tried so far: ${priorNotes.tried || 'none'}
` : ''}
IMMUTABLE MECHANICAL EVIDENCE — read before editing:
  Context fingerprint: ${brief.fingerprint || 'missing'}
  Ghidra artifact:      ${(brief.artifact_paths && brief.artifact_paths.ghidra) || 'missing'}
  Score artifact:       ${(brief.artifact_paths && brief.artifact_paths.score_context) || 'none for this exact attempt'}
  Mechanical verdicts:  ${JSON.stringify(brief.verdicts || [])}

Read the Ghidra artifact with rtk jq. It contains the decompile, disassembly,
callers/callees, call-site audit, struct offsets, and buffer evidence. Do NOT call
Ghidra when those fields are present. Query Ghidra only when this fingerprinted
artifact is invalid or evidence for a specific touched CALL is absent.

SCORE CONTEXT: use only the score artifact named above. It is reusable only when
the full source/KB/compiler/verifier/reference fingerprint matches. A missing
artifact means no reusable score evidence. Do not route from a worktree-local
legacy score file that lacks the current attempt fingerprint.

WORKED EXAMPLES (similar functions already ported, with their VC71 %; match their
idioms — casts, x87 order, struct-store shape. A neighbor is a bounded style
example, not binary evidence or a cap/acceptance verdict for this target):
${brief.neighbors || '  (control cohort or no current-index neighbor)'}

STEPS:
1. REFERENCE — nothing to do. The VC71 reference is DERIVED: the pristine XBE's
   bytes for this function, bounded by the committed tools/verify/function_bounds.json.
   Make NO ghidra-live calls to export one, and do not treat a low score as a
   reference problem. (A prefetch stage may have exported delinked/functions/<addr_no_0x>.obj
   — that object is for the EQUIVALENCE lane, which executes the oracle and needs
   real relocations. It has no effect on the VC71 %.)

2. CALLEE PREP — for any callee with has_reg_args=true and in_kb=false:
   add to kb.json with @<reg> + update tools/kb_reg_baseline.json.
   BEFORE writing any fresh blocker analysis for an un-registerable callee
   (or skipping this target over one), run:
     rtk python3 tools/lift/park.py followups --check <callee addr>
   If it returns an entry, that callee's blocker is ALREADY diagnosed — cite
   the entry's action in ONE line as your skip_reason instead of re-deriving
   the ABI analysis (repeat analyses of the same callee have burned 100+
   agent passes; the queue is the single source of truth).

3. IMPLEMENT — C89 in ${brief.source_path} at address-ordered position.
   Rules: C89 only, no inline ASM, preserve control flow + side-effect order.
   MSVC intrinsics → C: _ftol2→(int)cast, _chkstk→normal locals, _allmul→(int64_t)a*b.
   Trace every CALL: first PUSH is last arg. Check FPU subtraction direction and cross-product order.
   CHECKLIST (mandatory): read .claude/skills/auto-lift-checklist/SKILL.md once.
   Load detailed lift-decompiler-traps or lift-silent-bugs only when the compact
   checklist routes you there.

4. UPDATE kb.json — set ported=true; add @<reg> callees with binary evidence only.
   Update tools/kb_reg_baseline.json for any new @<reg>.

5. MAINTAIN + HAZARDS:
   rtk python3 tools/analysis/maintain.py ${brief.source_path}
   maintain.py is NOT scoped to the path you pass it — it relocates functions
   across every TU whose kb.json object mapping disagrees with its current file.
   On 2026-08-08 this left 25 unrelated files modified (rasterizer_xbox_water.c
   and shader_transparent_generic_preprocessor.c folded into rasterizer.c,
   +926/-924) in the shared worktree, where a later agent staged one of them
   into an unrelated lift commit. After running it, check:
     rtk git status --short -- src/
   If files OTHER than ${brief.source_path} changed, do NOT stage them and do
   NOT revert them (they may be another lane's work) — report them in your
   return value so the operator can commit them separately.
   rtk python3 tools/audit/check_lift_hazards.py --files ${brief.source_path}
   Fix any HIGH-RISK hazards.

6. BUILD + VC71 — run the build-fix loop at MOST twice in this agent (initial run,
   then ONE fix pass if the build fails or a HIGH-RISK hazard flags). Do NOT grind
   more than one fix pass here, then proceed to the follow-up stages (6b-6d) and the
   cap classifier (step 7) below. If the score is still short after those, the
   workflow re-spawns a FRESH escalation agent rather than extending this one (this
   agent's whole context is re-read every turn, so a long agent costs quadratically
   in tokens — a short one is linear).
   timeout 165 rtk python3 tools/lift/lift_pipeline.py --target ${brief.name} --no-metadata-update --verify-policy goal90${brief.artifact_paths && brief.artifact_paths.ghidra ? ` --ghidra-context ${JSON.stringify(brief.artifact_paths.ghidra)}` : ''} 2>&1 || echo "[lift_pipeline timed-out]"
   Parse the VC71 % line and build pass/fail ONLY. Do NOT paste the full objdiff or
   build log into your reasoning — quoting large tool output back inflates every
   following turn's re-read. If it timed out, status="needs_review", vc71_score=0.
   vc71_measured: report true ONLY if a per-function VC71 % was actually produced.
   If verify was SKIPPED, report vc71_measured=false and do NOT invent
   vc71_score=0 as if it were a real match result. There are only two real causes
   now — the address is missing from tools/verify/function_bounds.json (the skip
   message names it), or the TU failed to compile under VC71 (fix the C89 error).
   Neither is fixed by exporting anything from Ghidra.

6b. (retired) There is no redelink retry. The reference is derived from the
   pristine XBE + the committed bounds table, so no Ghidra export can raise or
   restore a VC71 score. A low score is a lift problem; a *missing* score is a
   bounds-table or VC71-compile problem — report it, do not chase it. Leave
   redelinked=false.

6c. PERMUTE (only if the score is now in [85,89] — skip otherwise):
   timeout 150 rtk python3 tools/permuter/run.py -q --target ${brief.name} --attempts 100 2>&1 || echo "[permuter stopped]"
   then re-run the step-6 lift_pipeline command. Never accept a permutation
   that lowers the score. Report permuted=true.
   Exit 3 = VACUOUS RUN (0 candidate iterations — permuter setup problem, not a
   real result; treat as if permute did not run). Exit 4 = BASELINE MISMATCH
   (the permuter's own scoring of the unmodified base disagrees with the
   pipeline's baseline — do not trust any candidate score from that run).
   The search is now ranked by mnemonic-LCS against the reference, but every
   surviving candidate is still a semantic mutation — read the diff before
   accepting it, same as any other code change.

6d. EQUIVALENCE (only if the FINAL score is in [85,89] — the review gate will
   demand runtime evidence for this band, so produce it now while you still
   know what every parameter means):
   - Copy artifacts/snapshots/infection_swarm.json to /tmp/snap_${brief.name}.json
     and rewrite its "arg_overrides" dict (python3 json load/dump, NOT sed) so
     the keys exactly match this function's kb.json decl param names. You just
     lifted this function — pick semantically valid values (handle params:
     0xe36b0001 is a valid object/actor handle in this snapshot; out-pointers
     get harness scratch automatically, omit them).
   - timeout 165 rtk python3 tools/equivalence/unicorn_diff.py ${brief.name} --seeds 100 --allow-stubs --float-tolerance 32 --mem-trace --state-snapshot /tmp/snap_${brief.name}.json 2>&1 || echo "[equivalence timed-out]"
   - If that errors or is not_applicable, fall back to zero-fill (same command
     without --state-snapshot, timeout 150).
   Report equiv_passes (true only on 0 divergences AND 0 stub-arg mismatches),
   equiv_confidence, and equiv_reason (state whether the live-state snapshot
   was used and which paths were exercised).

6e. PUBLISH ATTEMPT EVIDENCE after VC71 writes its local score-context pointer:
    rtk python3 tools/lift/research_bundle.py prepare --target ${brief.addr} --json --current-attempt${LIFT_REG_ARGS ? ' --allow-reg-args' : ''}
    This is a fingerprinted context hit (zero Ghidra calls) and publishes the
    score context only when its embedded candidate-source hash matches this tree.

7. STRUCTURAL-CAP CLASSIFY (only if vc71_score is in [65,84]): use the JSON
   returned by step 6e. Only a verdict with confidence="high" and an immutable
   score evidence id may set capped=true/cap_confidence="high". Legacy ledger
   prose, notes, failure files, and your own pattern judgment are hints only;
   report them as inconclusive and do not use them to stop escalation.
${CAP_TABLE}

8. RETURN (do NOT commit, do NOT run the review gate — that happens later):
   status: "needs_verify" if build passed (regardless of score), else "build_failed".
   Always report vc71_score, source_file (the actual path written), capped,
   cap_reason, and cap_confidence ("high" | "inconclusive"), plus
   redelinked/permuted/equiv_passes/equiv_confidence/equiv_reason for whichever
   of steps 6b-6d ran.`

// (retired) redelinkPrompt lived here.  Re-exporting a per-function delinked
// reference can no longer change a VC71 score: vc71_verify derives THE
// reference from the pristine XBE, bounded by the committed
// tools/verify/function_bounds.json.  delinked/ still backs the equivalence
// lane (unicorn executes the oracle and needs real relocations) — that is
// what delinkPrefetch below exists for.

const permutePrompt = (name) =>
  `${AGENT_RULES}

Run the decomp-permuter for ${name}, then re-verify (both wrapped — see [STALL]):
timeout 150 rtk python3 tools/permuter/run.py -q --target ${name} --attempts 100 2>&1 || echo "[permuter stopped at timeout]"
timeout 165 rtk python3 tools/lift/lift_pipeline.py --target ${name} --no-metadata-update --verify-policy goal90 2>&1 || echo "[timed-out]"

Exit 3 from run.py = VACUOUS RUN (0 candidate iterations ran — a setup problem,
not a real negative result; do not count it against the 2-invocation budget,
fix the cause or give up on permute for this function). Exit 4 = BASELINE
MISMATCH (run.py's own score of the unmodified base disagrees with the
pipeline's baseline score — any candidate score from that run is untrustworthy,
discard it). Candidates are now selected by mnemonic-LCS rank against the
reference, which favors instruction-order matches — that is not the same as
correctness, so read the actual diff of any accepted candidate before trusting
it, same as reviewing any other code change.

BOUNDED PASS — at most 2 permuter invocations total. If a permutation breaks the
build (e.g. -Werror dead variable), fix it minimally and re-verify once. If the
score is still <90% after that, STOP and return the best verified score — do not
keep iterating; the review gate decides acceptance. Never accept a permutation
that lowers the pre-permute score.
Return: vc71_score (after permutation), improved (bool), reason.`

// vc71-match-optimizer escalation — replaces the old cold-rewrite lift2 for
// fail_check_cap (65-84%, classify_cap says NOT capped). The function already
// builds and is already believed faithful; a full re-lift with a different
// model throws that away and re-derives it. This instead tunes the EXISTING
// candidate source one score-recovery lever at a time (recipe atlas in
// .claude/skills/lift-score-improve/SKILL.md), which is cheaper and can't
// regress correctness the way a cold rewrite occasionally has.
const matchOptimizerPrompt = (name, addr, obj, srcFile, priorScore, neighbors) =>
  `${AGENT_RULES}

Improve the VC71 byte-match score for ${name} at ${addr} (object: ${obj}).
Source: ${srcFile} | Current score: ${priorScore}%.

This function already builds and is already believed behaviorally faithful —
your job is ONLY to close the byte-match gap against the delinked MSVC 7.1
reference. Follow your own protocol: fresh score-context pack first, then one
lever per iteration from the lift-score-improve recipe atlas, re-measured via
the fast single-function path, keeping only improvements.
  rtk python3 tools/verify/vc71_verify.py ${srcFile} -f ${name} --no-cache
  rtk python3 tools/lift/research_bundle.py prepare --target ${addr} --json --current-attempt
Read classification only from the returned artifact_paths.score_context.

PROVENANCE BOUNDARY:
Use only the 2276 target disassembly, target call sites, embedded strings and
tables, independently observed runtime behavior, and already target-verified
source. External or cross-build material is not an implementation-shape or
naming lever. If historical notes show such influence, preserve that disclosure
and independently re-derive the relevant branch, expression, ABI, and field
role from 2276 before accepting a change.

WORKED EXAMPLES (similar already-ported functions with their VC71 %; match
their idioms — casts, x87 order, struct-store shape — if a lever here mirrors
one of theirs):
${neighbors || '  (none — retrieval server was cold)'}

Never submit a score below ${priorScore}%. Respect regarg_structural_ceiling
and any other documented structural cap — report it, do not chase it.
Do NOT commit, do NOT run the review gate.
Return: vc71_score (your final best, from the closing full verify — see your
protocol), improved (bool, vs ${priorScore}%), capped (bool — true ONLY if you
hit a documented non-recoverable ceiling like regarg_structural_ceiling, false
if you simply ran out of applicable levers), cap_reason (the ceiling's rule id
when capped is true, else empty string), reason (short: which lever(s) you
kept, and why — capped or not).`

// Recipe-atlas short-circuit (docs/history/agent-model-routing-2026-08.md §5/§7.2).
// vc71_verify's _classify_score_context() writes classification[].rule into
// artifacts/score_context/<name>.json, and those rule ids map 1:1 onto the
// lift-score-improve recipe atlas — i.e. the remaining gap is already NAMED and
// the fix is mechanical. This prompt is the cheap-tier "apply exactly what the
// classifier said" pass; anything not already classified belongs to the ladder.
// The pack read is the FIRST and possibly ONLY command, so a missing pack or an
// empty classification costs one short turn.
const atlasLeverPrompt = (name, addr, obj, srcFile, priorScore) =>
  `${AGENT_RULES}

Apply the ALREADY-CLASSIFIED score-recovery lever(s) for ${name} at ${addr}
(object: ${obj}). Source: ${srcFile} | Current score: ${priorScore}%.

FIRST, publish/validate the current attempt and read only its returned
artifact_paths.score_context:
  rtk python3 tools/lift/research_bundle.py prepare --target ${addr} --json --current-attempt
If score_context is empty, or classification is empty/null, or every entry is a
documented ceiling (regarg_structural_ceiling, anchor_collapse), STOP
IMMEDIATELY and return vc71_score ${priorScore}, improved false, reason
"no_atlas_rule". Do NOT open the source, do NOT run any other command — the
escalation ladder handles that case and is about to.

Otherwise this is a mechanical atlas hit. Apply ONLY the levers the pack names
(classification[].rule / .action — fpu_operand_order, loadw_field_width,
imm_wrong_literal, fcom_bound_sense, frame_mismatch, chkstk_static_buffer, ...),
one lever at a time, re-measuring each with the fast single-function path:
  rtk python3 tools/verify/vc71_verify.py ${srcFile} -f ${name} --no-cache
Keep a change only if it raised the score; revert it otherwise. Do NOT invent
levers beyond the classified ones, do NOT refactor or rename, do NOT commit, do
NOT run the review gate. Never submit a score below ${priorScore}%.

Return: vc71_score (final, from the closing verify), improved (bool vs
${priorScore}%), capped (bool — true ONLY for a documented non-recoverable
ceiling), cap_reason (that ceiling's rule id when capped, else empty string),
reason (which rule ids you applied and kept, or "no_atlas_rule").`

const equivalencePrompt = (name) =>
  `${AGENT_RULES}

Run behavioral equivalence for ${name} — WITH LIVE GAME STATE, not zero-fill.

For actor/object/AI functions, zero-fill memory makes them early-exit (empty datum
tables) and yields confidence=weak, which the review gate then rejects. Use the
proven state snapshot instead (per reference_statesnapshot_recovers_vc71capped):

1. Look up the target's kb.json decl to get its exact param names:
   rtk jq -r '[.. | objects | select(.name? == "${name}")] | .[0].decl' kb.json
2. Copy artifacts/snapshots/infection_swarm.json to /tmp/snap_${name}.json and
   rewrite its "arg_overrides" dict to the target's ACTUAL param names (keys must
   match the decl exactly). For an actor/object handle param use a valid handle
   from the snapshot's actor table (0xe36b0001 works for object functions).
   Use python3 json load/dump for the rewrite, not sed.
3. Run (wrapped — see [STALL]):
   timeout 165 rtk python3 tools/equivalence/unicorn_diff.py ${name} --seeds 100 --allow-stubs --float-tolerance 32 --mem-trace --state-snapshot /tmp/snap_${name}.json 2>&1 || echo "[equivalence timed-out]"
4. If the snapshot run errors or is not_applicable (param names mismatch, non-actor
   function), fall back to zero-fill:
   timeout 150 rtk python3 tools/equivalence/unicorn_diff.py ${name} --seeds 100 --allow-stubs --float-tolerance 32 2>&1 || echo "[equivalence timed-out]"

A timeout is NOT a verdict — step down until you get one: retry the failing
command with --seeds 25, then --seeds 10 --no-concolic. Only if ALL attempts
time out: passes=false, reason="equiv_timeout".
If a run COMPLETES with divergences, passes=false with the divergence as reason —
do not retry a completed run at lower seeds to dodge a real divergence.

Passes if the result is 100% equivalent (0 divergences, 0 stub-arg mismatches), or
confidence="high" with no divergences. A 0-divergence pass on the live-state
snapshot is real behavioral evidence even at moderate confidence — report
state_snapshot=true so the reviewer can weigh it.
Return: passes (bool), confidence, coverage (%), reason (mention whether the
state snapshot was used, seeds used, and which paths were exercised).`

const reviewPrompt = (brief, score, srcFile, path) =>
  `Target: ${brief.name} (${brief.addr}, ${brief.obj})
Source: ${srcFile}
Structural match (VC71/objdiff): ${score}%
Acceptance path so far: ${path}
Immutable context bundle: ${(brief.artifact_paths && brief.artifact_paths.ghidra) || 'missing'}
Context fingerprint: ${brief.fingerprint || 'missing'}

Gather your own evidence before deciding:
- Source diff: rtk git diff -- ${srcFile} kb.json
- ABI audit: rtk python3 tools/audit/audit_reg_abi.py (or reuse the pass already run by generate_lift_commit.py)
- Hazard scan: rtk python3 tools/audit/check_lift_hazards.py --files ${srcFile}
- Fresh Halo build and VC71 result for ${brief.name}
- Caller/callee/disassembly context from the immutable bundle. Query Ghidra only
  if the bundle is fingerprint-invalid or a touched CALL lacks required evidence.
- Relevant kb.json declarations and register args for ${brief.name}

Apply your decision policy and return your verdict.
blocked_by_callee: set it ONLY with verdict REJECT, and ONLY when the sole reason
is that a CALLEE's kb.json declaration (params, @<reg>, return type) is wrong per
the binary, so this lift would be acceptable once that decl is fixed. Value = the
callee's address as 0x<hex>. Otherwise leave it empty. Setting it parks the target
as blocked_review: it is not re-served until that callee's decl changes.`

// Cheap mechanical gate — runs the same hazard/ABI checks the commit stage will
// enforce, so a clean high-% lift can skip the Opus-high reviewer entirely.
// Fused with the commit (2026-09-30): the gate and the commit used to be two
// back-to-back mechanical agents, each paying a full agent start-up for a few
// commands. The pass rule is spelled out literally with this lift's score, and
// gateThenCommit re-derives it from the returned booleans as a cross-check.
// commitAllowed=false (dry run) keeps the old report-only behavior.
const gateCommitPrompt = (brief, srcFile, score, reason, needBuild, commitAllowed) =>
  `${commitAllowed ? AGENT_RULES + '\n' : ''}Mechanical pre-commit gate for ${brief.name} (${brief.addr}), VC71 ${score}%.
Steps 1-3 report booleans ONLY — do NOT edit or fix anything in them.

1. HAZARDS: rtk python3 tools/audit/check_lift_hazards.py --files ${srcFile} 2>&1
   (--files, not --changed-only: this gate only reads findings for ${srcFile},
   and --changed-only rescans every file the branch has touched so far, which
   grows with the batch — ~7.5s and ~20 files by the end of a 21-function run
   versus ~0.4s here, for identical output after the filter.)
   - hazards_clean=true  iff NO HIGH-RISK / ERROR finding references ${srcFile}
   - warns=true          iff any WARN-level finding references ${srcFile}
2. ABI: rtk python3 tools/audit/audit_reg_abi.py 2>&1
   - abi_clean=true iff it reports no failure for ${brief.name}
3. RISKY CALLS in the diff: rtk git diff -- ${srcFile}
   - risky_calls=true iff this lift ADDED any of: a raw function-pointer cast call,
     an XCALL(0x...) whose target is ported, or inline __asm.

4. PASS RULE: pass = hazards_clean AND abi_clean AND ${score >= 95
    ? `(score ${score} >= 95, so risky_calls/warns do not matter)`
    : `NOT risky_calls AND NOT warns (score ${score} < 95)`}.
${commitAllowed
    ? `   If pass=false: STOP. Do NOT commit, do NOT revert — leave the tree exactly as
   it is (a reviewer adjudicates next). Return committed=false.
   If pass=true: commit the lift of ${brief.name} (mechanical:${score}% ${reason}):

${commitSteps(brief.name, srcFile, needBuild)}
${commitTail(brief.name, srcFile)}`
    : `   Dry run: do NOT build or commit regardless of pass. Return committed=false.`}

Return hazards_clean, abi_clean, risky_calls, warns, pass, committed,
build_failed, sha, and a one-line detail per non-clean finding.`

// Whether the commit agent must build before committing. lift_pipeline's build
// step is the SAME incremental `build.py -q --target halo` the commit agent
// used to re-run, so after a pass1/permute path the tree it would build is the
// tree that just built. Only the vc71_verify-only tuning paths (atlas-lever,
// match-optimizer, improve-optimize) edit source without a halo build after.
// The batch-end buildCheckAndBisect backstops every path either way.
const needsBuild = (path) => PER_COMMIT_BUILD || /atlas-lever|optimize/.test(String(path || ''))

const COMMIT_SCHEMA = {
  type: 'object',
  properties: {
    committed:    { type: 'boolean' },
    sha:          { type: 'string' },
    build_failed: { type: 'boolean' },
    detail:       { type: 'string' },   // first build error line, or unstaged stray paths
  },
  required: ['committed'],
}

const GATE_COMMIT_SCHEMA = {
  type: 'object',
  properties: {
    ...MECH_GATE_SCHEMA.properties,
    ...COMMIT_SCHEMA.properties,
    pass: { type: 'boolean' },
  },
  required: ['hazards_clean', 'abi_clean', 'committed'],
}

// The commit recipe, shared by commitPrompt and the fused gateCommitPrompt.
const commitSteps = (name, sourceFile, needBuild) =>
  `${needBuild ? `FIRST verify the full halo build (this path edited source after the lift's
last lift_pipeline build):
  timeout 165 rtk python3 tools/build/build.py -q --target halo 2>&1 | tee /tmp/glbuild.txt
  Build PASSED if /tmp/glbuild.txt has NO "error:" line and NO "Error 2" / "*** " marker.
  If FAILED:
    rtk git checkout -- src/ kb.json tools/kb_reg_baseline.json
    Return committed=false, build_failed=true, detail=<first error line>. Do NOT commit.
` : `No build by default: this lift's own lift_pipeline run already built this tree
with the same command, and a batch-end build backstops the run. The build
command, for the one case below that still requires it:
  timeout 165 rtk python3 tools/build/build.py -q --target halo 2>&1 | tee /tmp/glbuild.txt
  Build PASSED if /tmp/glbuild.txt has NO "error:" line and NO "Error 2" / "*** " marker.
  If FAILED:
    rtk git checkout -- src/ kb.json tools/kb_reg_baseline.json
    Return committed=false, build_failed=true, detail=<first error line>. Do NOT commit.
`}
If ${sourceFile} is a NEW translation unit, it must also be registered in
src/CMakeLists.txt or it is never linked: the function stays ported=true in
kb.json with no body in the XBE, so the ORIGINAL Xbox code keeps running. The
build still exits 0 either way, so this is silent. Check and fix before
committing:
  grep -c "$(basename ${sourceFile})" src/CMakeLists.txt
  If 0: add the path (repo-relative from src/, e.g. halo/rasterizer/xbox/foo.c)
  to the source list in src/CMakeLists.txt, in alphabetical position, THEN run
  the build command above (even if it was skipped) — the new TU has never been
  compiled into halo. On failure handle it exactly as a failed build above.
`

const commitPrompt = (name, sourceFile, reason, needBuild) =>
  `${AGENT_RULES}

Commit the lift of ${name}${reason ? ' (' + reason + ')' : ''}.

${commitSteps(name, sourceFile, needBuild)}
${commitTail(name, sourceFile)}`

const commitTail = (name, sourceFile) =>
  `
Then commit — note src/CMakeLists.txt is in the add list precisely because a
new TU's registration was repeatedly written but left unstaged, which landed
three unlinked translation units on 2026-08-01:
  rtk git add -- ${sourceFile} kb.json tools/kb_reg_baseline.json src/CMakeLists.txt

  STRAY-FILE GATE. Agents have staged unrelated files that happened to be dirty
  in the shared worktree, landing 74+/75- of ai/actor_looking.c inside a commit
  titled "Port cinematic_show_letterbox" (bdaac19d, 2026-08-08). The add above is
  already scoped, so a stray file means something deviated from it -- check
  mechanically rather than trusting the add. src/types.h is allowed because
  struct recovery legitimately lands there:
  rtk git diff --cached --name-only -- src/ | grep -v -e "^${sourceFile}$" -e '^src/types.h$' -e '^src/CMakeLists.txt$'
  If that prints ANY path, unstage it (rtk git restore --staged -- <path>) and
  note it in the return value. Do NOT commit unrelated source files.

  The message file MUST be mktemp'd. A fixed /tmp path is shared by every
  concurrent agent, cron job, and worktree on this box, and they all follow this
  same recipe -- a second writer between the redirect and the commit silently
  commits YOUR staged changes under THEIR message (observed 2026-07-31, commit
  d6caee6b):
  MSG=$(mktemp /tmp/halo-commit-msg.XXXXXX)
  rtk python3 tools/audit/generate_lift_commit.py --batch-name "${name}" > "$MSG"
  HALO_BATCH_COMMIT=1 rtk git commit -F "$MSG" && rm -f "$MSG"

  HALO_BATCH_COMMIT=1 is REQUIRED. It tells pre-commit-regression-test.sh to
  defer the 40-48s Unicorn differential to auto_reintegrate.py's Gate 5, which
  runs it once over the whole branch before main advances. Keep it as a prefix
  on the \`git commit\` line, never an \`export\`, and never on any other
  command. Dropping it is safe but slow, so do not "fix" a failure by removing
  it.
Then, if this function had a parked record from an earlier attempt, mark it
promoted so the improve pass won't re-pick it (ignore errors if none exists):
  rtk python3 tools/lift/park.py promote --name ${JSON.stringify(name)} --commit "$(git rev-parse --short HEAD)" 2>/dev/null || true
Return committed=true, sha=<short commit hash>, build_failed=false, and in
detail any stray path you unstaged. If the commit itself failed (hook error) and
\`git log --oneline -1\` confirms no new commit, run \`rtk git reset -q\` (unstage
only — keep the working tree so the caller can park the work) and return
committed=false, build_failed=false, detail=<first hook error line>.`

const revertPrompt = (name) =>
  `${AGENT_RULES}

Revert changes for ${name} — save a recovery patch FIRST, then revert:
mkdir -p artifacts/auto_lift/failures
rtk proxy git diff -- src/ kb.json tools/kb_reg_baseline.json > "artifacts/auto_lift/failures/${name}-$(date +%s).patch"
rtk git checkout -- src/ kb.json tools/kb_reg_baseline.json
rtk git status --short
(The rtk proxy prefix on the diff is required: plain rtk truncates redirected output.)`

// Preserve ANY sub-bar-but-building lift for a later improve pass — never
// checkout-discard real work. Routes through the workflow-agnostic parked
// ledger (tools/lift/park.py), shared with manual /lift and the improve pass.
// attemptME = the {model,effort} of the lift ATTEMPT (recorded for later
// exclude-model selection), not the park agent's own model.
// keepTree = checkpoint-only park: save the patch + record the attempt but LEAVE
// the working tree alone. Used for the pre-escalation checkpoint, whose whole
// point is that the ladder keeps tuning the very source on disk (a --revert-tree
// there wiped the candidate out from under the optimizer — the empty-patch
// "escalation_exhausted" records with "no C implementation to apply a lever to").
// publish=true: first publish the CURRENT attempt's evidence bundle and park
// with its attempt fingerprint + artifact ids, falling back to the brief's own
// (`fingerprint`/`artifacts`) when the publish fails — same as the separate
// publish-score agent this replaced.
const parkToolPrompt = (name, addr, obj, srcFile, score, attemptME, reason, capHyp, notes, fingerprint, artifacts, keepTree, blockedBy, publish) => {
  const fbEvidence = Object.values(artifacts || {}).filter(Boolean)
  const parkTail = `--name ${JSON.stringify(name)} --addr ${JSON.stringify(addr || '')} --obj ${JSON.stringify(obj || '')} --source ${JSON.stringify(srcFile || '')} --score ${score} --model ${JSON.stringify(attemptME.model)} --effort ${JSON.stringify(attemptME.effort)} --reason ${JSON.stringify(reason || '')} --outcome parked${capHyp ? ' --cap-hypothesis ' + JSON.stringify(capHyp) : ''}${notes ? ' --notes ' + JSON.stringify(String(notes).slice(0, 2000)) : ''}${blockedBy ? ' --blocked-by ' + JSON.stringify(blockedBy) : ''}${keepTree ? '' : ' --revert-tree'}`
  const cmd = publish
    ? `Run this whole block as ONE Bash call, verbatim (bash syntax; it publishes the
current attempt's evidence, then parks with that evidence):
B=$(mktemp /tmp/glpark.XXXXXX)
timeout 300 rtk proxy python3 tools/lift/research_bundle.py prepare --target ${JSON.stringify(addr || '')} --json${LIFT_REG_ARGS ? ' --allow-reg-args' : ''} --current-attempt > "$B" 2>/dev/null || true
FP=$(rtk proxy jq -r '.attempt_fingerprint // .fingerprint // empty' "$B" 2>/dev/null)
EV=$(rtk proxy jq -r '(.artifacts // {}) | .[] | select(. != null and . != "")' "$B" 2>/dev/null)
rm -f "$B"
[ -n "$FP" ] || FP=${JSON.stringify(fingerprint || '')}
[ -n "$EV" ] || EV=${JSON.stringify(fbEvidence.join(' '))}
ARGS=(); [ -n "$FP" ] && ARGS+=(--fingerprint "$FP"); for id in $EV; do ARGS+=(--evidence "$id"); done
rtk python3 tools/lift/park.py park "\${ARGS[@]}" ${parkTail}`
    : `Run exactly this one command:
rtk python3 tools/lift/park.py park ${parkTail}${fingerprint ? ' --fingerprint ' + JSON.stringify(fingerprint) : ''}${fbEvidence.map(id => ' --evidence ' + JSON.stringify(id)).join('')}`
  return `${AGENT_RULES}

Preserve the sub-bar lift of ${name} (${addr}, ${score}% VC71) for a later improve
pass${keepTree ? '' : ', then clean the tree'}. ${cmd}
park.py saves the git diff to artifacts/parked/ and records the attempt (with
history)${keepTree ? '. This is a CHECKPOINT: do NOT revert, reset or checkout anything — the working tree must stay exactly as it is' : ', then reverts src/ kb.json tools/kb_reg_baseline.json to HEAD'}. Return the
tool's "parked ..." stdout line.`
}

// Improve pass — pick the closest-to-bar parked function the improve model has
// NOT already attempted (so repeated improve passes drain the ledger instead of
// re-trying the same model on the same function).
const nextPrompt = (excludeModel) =>
  `Pick the next parked function for the improve pass. Run exactly:
rtk python3 tools/lift/park.py next --exclude-model ${JSON.stringify(excludeModel)}
It prints JSON {"found":bool,"record":{...}}. The record carries "last_notes"
(the most recent attempt's notes string, may be empty) and "attempt_history"
(array of {model, score, notes}).
- If found=false → return found=false.
- Else return found=true with the record's name, addr, obj, source_path, best_score,
  attempts (the length of the attempts array), tried_models (comma-joined
  attempts[].model values), last_notes (the record's last_notes field verbatim —
  this is the prior attempt's diagnosis/rationale and must NOT be summarized or
  dropped), and tried_summary (build a compact "model:score%" list from
  attempt_history, e.g. "opus:71.8, fable:74.2" — one entry per attempt, in order).`

// Warm-start: restore the parked best patch so the improve model refines real
// prior work instead of starting cold. A stale patch (HEAD moved past it) fails
// cleanly → the model re-derives from scratch, which is fine.
const applyPrompt = (name) =>
  `${AGENT_RULES}

Warm-start the improve pass for ${name}: restore its parked best patch into the tree.
Run exactly: rtk python3 tools/lift/park.py apply --name ${JSON.stringify(name)}
- Prints "applied ..." → applied=true.
- Errors (patch does not apply cleanly / HEAD moved / best_patch missing) → applied=false
  with the reason. Do NOT force, --3way, or hand-edit — a cold re-derive is acceptable.
Return applied, reason.`

// Warm the persistent retrieval server ONCE up front. Every research decompile
// fires the decompile_hook, which queries this server for worked-example
// neighbors + hazard briefs. If the server is cold, the hook starts it in the
// background and the first few queries return nothing (the model takes ~75s to
// load) — warming it once here means all research agents get warm (~1-2s),
// non-empty retrieval instead of racing a cold start 6 ways. Run by the fused
// preflight agent in improve mode.
const WARM_RETRIEVAL_CMD =
  `  if [ -S /tmp/retrieval_server.sock ]; then echo "already-up"; else
    PY=python3; [ -x .venv/bin/python3 ] && PY=.venv/bin/python3;
    nohup "$PY" tools/retrieval/server.py > /tmp/retrieval_server.log 2>&1 &
    for i in $(seq 1 16); do sleep 5; [ -S /tmp/retrieval_server.sock ] && break; done;
  fi
  [ -S /tmp/retrieval_server.sock ] && echo "up" || echo "cold"`

// ── Helpers ───────────────────────────────────────────────────────────────────

// goal90 bands: see docs/lift-policy.md §goal90-pass-fail-bands
function classifyBand(score) {
  if (score >= 90) return 'pass'
  if (score >= 85) return 'pass_permute'
  if (score >= 65) return 'fail_check_cap'
  return 'fail_revert'
}

async function maybePermute(name, phaseTitle) {
  const p = await schemaAgent(permutePrompt(name), { label: `permute:${name}`, phase: phaseTitle, ...M.mechanical, schema: SCORE_SCHEMA })
  return p ? p.vc71_score : null
}

const equivNote = (confidence, reason) =>
  `+equiv_${confidence || 'unknown'} [equivalence detail: ${String(reason || '').slice(0, 500)} — a 0-divergence pass on the live-state infection_swarm snapshot (populated datum tables, real actor handles) is accepted runtime behavioral evidence for the sub-90% band per the state-snapshot equivalence lane in AGENTS.md]`

// Phase 3 — the fail-closed review gate. Every commit in this workflow goes
// through here; nothing is committed on VC71 match alone. `preEquiv` is the
// lift agent's own in-context equivalence result (step 6d) — when it passed,
// the FIRST review already carries the runtime evidence, so the
// NEEDS_RUNTIME → equiv agent → re-review round-trip is skipped.
async function reviewThenCommit(brief, score, srcFile, path, phaseTitle, preEquiv) {
  const havePreEquiv = !!(preEquiv && preEquiv.equiv_passes)
  if (havePreEquiv) path = `${path}${equivNote(preEquiv.equiv_confidence, preEquiv.equiv_reason)}`
  let review = await schemaAgent(reviewPrompt(brief, score, srcFile, path), {
    label: `review:${brief.name}`, phase: phaseTitle,
    // Model stays M.reason's; effort is the --reviewEffort A/B lever (see above).
    agentType: 'auto-lift-reviewer', model: M.reason.model, effort: REVIEW_EFFORT, schema: REVIEW_SCHEMA,
  })
  if (!review) return { committed: false, verdict: 'infra_blocked', rationale: 'structured_output_null:review' }

  // When the lift agent's own equivalence (step 6d) was already in the review
  // path, the reviewer adjudicated WITH runtime evidence — respect its verdict
  // (a NEEDS_RUNTIME then means the evidence itself was judged insufficient,
  // e.g. early-exit-only coverage). Only the no-preEquiv case produces the
  // evidence now and applies the mechanical rule.
  if (review.verdict === 'NEEDS_RUNTIME' && !havePreEquiv) {
    const eq = await schemaAgent(equivalencePrompt(brief.name), { label: `equiv-for-review:${brief.name}`, phase: phaseTitle, ...M.mechanical, schema: EQUIV_SCHEMA })
    // Mechanical acceptance rule — no second reviewer pass. A re-review adds no
    // information the first pass didn't have (2026-07-04: FUN_0018ef30 passed
    // equiv 100/100 seeds and was re-rejected on the same structural grounds).
    // NEEDS_RUNTIME means "structure is a near-miss, behavior unproven"; a
    // passing equivalence run at moderate+ confidence IS that proof.
    // MIN_COMMIT gates ONLY this mechanical NEEDS_RUNTIME+equiv acceptance
    // lane — it intentionally does not gate gateThenCommit's >=90%/>=95%
    // mechanical fast path above, nor the reviewer's own direct AUTO_ACCEPT
    // verdict a few lines up. Runtime evidence substituting for byte-match
    // score is a narrower claim than byte-match score alone, so it gets its
    // own (lower, configurable) floor instead of inheriting the others'.
    if (eq && eq.passes && score >= MIN_COMMIT && (eq.confidence === 'high' || eq.confidence === 'moderate')) {
      review = { verdict: 'AUTO_ACCEPT', rationale: `mechanical: NEEDS_RUNTIME + equiv passed (confidence=${eq.confidence}, coverage=${eq.coverage != null ? eq.coverage : '?'}%)` }
    } else if (eq && eq.passes) {
      // Weak-confidence pass: not enough to commit, too good to revert — the
      // caller's >=85% branch parks it as an improve-queue candidate.
      review = { verdict: 'NEEDS_RUNTIME', rationale: `equiv passed but confidence=${eq.confidence || 'weak'} — needs state-snapshot/golden evidence (parked, not rejected)` }
    }
  }

  if (!review || review.verdict !== 'AUTO_ACCEPT') {
    const blocker = review && review.verdict === 'REJECT' && /^0x[0-9a-f]+$/i.test(String(review.blocked_by_callee || '').trim())
      ? String(review.blocked_by_callee).trim().toLowerCase() : ''
    return { committed: false, verdict: review ? review.verdict : 'infra_blocked', rationale: review ? review.rationale : 'structured_output_null:review', blocked_by_callee: blocker }
  }

  if (DRY_RUN) {
    return { committed: false, verdict: 'AUTO_ACCEPT', rationale: 'dry-run: would have committed', dryRun: true }
  }

  const c = await doCommit(brief, srcFile, path, path, phaseTitle)
  if (!c.committed) return commitFailure(c)
  return { committed: true, verdict: 'AUTO_ACCEPT', rationale: path }
}

// Run the commit agent and read its structured result. The commit agent used
// to return free text that nothing checked, so a "BUILD_FAILED: ..." return
// (tree already reverted) was still recorded as a commit.
async function doCommit(brief, srcFile, note, path, phaseTitle) {
  const c = await schemaAgent(commitPrompt(brief.name, srcFile, note, needsBuild(path)), {
    label: `commit:${brief.name}`, phase: phaseTitle, ...M.commit, schema: COMMIT_SCHEMA,
  })
  return c || { committed: false, build_failed: false, detail: 'structured_output_null:commit' }
}

// A failed commit is a build failure (tree reverted by the commit agent) or
// infra (hook error / dead agent — tree kept, so the caller parks it).
const commitFailure = (c) => c.build_failed
  ? { committed: false, verdict: 'build_failed', rationale: `commit build failed: ${c.detail || '?'}` }
  : { committed: false, verdict: 'infra_blocked', rationale: `commit_failed: ${c.detail || '?'}` }

// Equivalence-backed commit fallback for structurally-capped lifts (score
// stuck in [65,84], deterministic cap confirmed) that never reach
// reviewThenCommit — its gate only fires for score>=85 bands. Added
// 2026-09-01: journal evidence showed capped fns in this band are often
// behaviorally correct (the cap is a byte-match ceiling, e.g. frame-shape or
// /Os-vs-/Ot codegen, not a logic bug) but had NO equivalence path anywhere
// in the pipeline, so every one parked unconditionally. Mirrors
// reviewThenCommit's mechanical NEEDS_RUNTIME+equiv rule, minus the review
// agent (a capped lift already has a documented reason for its ceiling; a
// reviewer pass adds no new information the cap classification didn't have).
async function capEquivCommit(brief, score, srcFile, path, phaseTitle, capReason) {
  if (score < MIN_CAPPED_COMMIT_SCORE) return { committed: false, rationale: 'below_min_capped_commit_score' }
  const eq = await schemaAgent(equivalencePrompt(brief.name), { label: `equiv-for-cap:${brief.name}`, phase: phaseTitle, ...M.mechanical, schema: EQUIV_SCHEMA })
  if (!eq || !eq.passes || !(eq.confidence === 'high' || eq.confidence === 'moderate')) {
    return { committed: false, rationale: eq ? `equiv_${eq.passes ? 'weak_confidence' : 'failed'}` : 'equiv_agent_returned_null' }
  }
  const note = `${path}${equivNote(eq.confidence, eq.reason)} [structural_cap: ${capReason}]`
  if (DRY_RUN) return { committed: false, rationale: 'dry-run: would have committed', dryRun: true }
  const c = await doCommit(brief, srcFile, note, path, phaseTitle)
  if (!c.committed) return { committed: false, build_failed: !!c.build_failed, rationale: commitFailure(c).rationale }
  return { committed: true, rationale: note }
}

// The commit gate. The user cares about VC71 byte-accuracy %, not a prose code
// review, so a clean high-% lift commits on a cheap mechanical check alone; the
// Opus-high reviewer fires ONLY for the ambiguous/flagged band. Mechanical
// fast-path acceptance:
//   - score >= 95 AND hazards+ABI clean                              → commit
//   - score >= 90 AND hazards+ABI clean AND no risky calls/WARNs     → commit
// Anything else (flagged despite high %, or < 90) falls through to the reviewer,
// which still gets behavioral proof via equivalence on NEEDS_RUNTIME.
async function gateThenCommit(brief, score, srcFile, path, phaseTitle, preEquiv) {
  if (score >= 90) {
    // One agent: gate, and on a pass commit in the same turn (gateCommitPrompt).
    // Commit-level model: the fused agent does what the commit agent did.
    const g = await schemaAgent(gateCommitPrompt(brief, srcFile, score, path, needsBuild(path), !DRY_RUN), {
      label: `gate+commit:${brief.name}`, phase: phaseTitle, ...M.commit, schema: GATE_COMMIT_SCHEMA,
    })
    const clean   = g && g.hazards_clean && g.abi_clean
    const mechPass = clean && (score >= 95 || (!g.risky_calls && !g.warns))
    if (g && g.committed) {
      if (!mechPass) log(`  ⚠ ${brief.name}: gate agent committed although its booleans fail the pass rule (${g.detail || 'no detail'}) — commit ${g.sha || '?'} kept; review it`)
      return { committed: true, verdict: 'AUTO_ACCEPT', rationale: `mechanical gate: ${score}% clean (${path})` }
    }
    if (mechPass) {
      if (DRY_RUN) return { committed: false, verdict: 'AUTO_ACCEPT', rationale: `dry-run: mechanical gate (${score}% clean)`, dryRun: true }
      if (g.build_failed) return commitFailure(g)
      // Pass but no commit (hook error, or the agent stopped short): commit
      // with the standalone agent rather than paying for a reviewer.
      const c = await doCommit(brief, srcFile, `mechanical:${score}% ${path}`, path, phaseTitle)
      if (!c.committed) return commitFailure(c)
      return { committed: true, verdict: 'AUTO_ACCEPT', rationale: `mechanical gate: ${score}% clean (${path})` }
    }
    // High % but flagged (hazard/ABI/risky/warn) → the reviewer must adjudicate.
    log(`  ${brief.name} ${score}% flagged by mechanical gate (${g ? (g.detail || 'see hazard/abi') : 'gate_null'}) — escalating to reviewer`)
  }
  return await reviewThenCommit(brief, score, srcFile, path, phaseTitle, preEquiv)
}

// Preserve a sub-bar built lift (any score) via park.py and revert the tree.
// attemptME = {model,effort} of the lift attempt being preserved. notes = free-form
// diagnostic/rationale text for this attempt (capped 2000 chars in parkToolPrompt),
// read back by the improve pass via park.py next's last_notes/attempt_history.
// keepTree = record the attempt WITHOUT reverting (checkpoint park); see parkToolPrompt.
// blockedBy = callee addr from a reviewer callee-ABI REJECT → park.py blocked_review.
// One agent (2026-09-30): the current-attempt evidence publish and the park used
// to be two mechanical agents back to back; the park only needed the publish's
// fingerprint + artifact ids, which the shell recipe now pipes across itself.
async function parkBuilt(brief, srcFile, score, attemptME, reason, capHyp, phaseTitle, notes, keepTree, blockedBy) {
  await agent(parkToolPrompt(brief.name, brief.addr, brief.obj, srcFile, score, attemptME, reason, capHyp, notes,
    brief.attempt_fingerprint || brief.fingerprint, brief.artifacts, keepTree, blockedBy, true),
    { label: `publish+park:${brief.name}`, phase: phaseTitle || 'Lift', ...M.mechanical })
}

// Collapse this run's consecutive same-object commits into one, reworded
// "Port N functions (obj)" — the shape a hand-run single-object session
// already produces (9f904e851 "Port xbox_texture_cache.obj", 9 fns/1 commit).
// goal-lift still commits per-function AS IT GOES (the real safety
// checkpoint: a crash mid-object loses nothing) — this only runs once, at the
// end, to tidy history.
//
// Only CONTIGUOUS same-object runs collapse. The default selector ranks by
// global score across all objects, so commits often interleave objects; kb.json
// and kb_meta.json are single shared files every commit touches, so splitting
// an already-squashed multi-object diff back apart is the kb.json hand-merge
// hazard this repo explicitly bans. `git reset --soft` only ever rewrites the
// TOP of history, so squashing pure same-object stretches needs no splitting —
// stretches are processed newest-to-oldest so an earlier reset never touches a
// later stretch that is still separate commits.
// Batch-end build backstop for the per-commit build that needsBuild() now
// skips on pass1/permute paths. One incremental halo build over the run's
// final tree; on failure, bisect this run's commits BY HAND (each build is
// its own ≤165s command — a single `git bisect run` would outlive the [STALL]
// ceiling) and revert the first bad commit, at most twice.
const BUILD_CHECK_SCHEMA = {
  type: 'object',
  properties: {
    ok:             { type: 'boolean' },   // final tree builds
    reverted_names: { type: 'array', items: { type: 'string' } },
    reverted_shas:  { type: 'array', items: { type: 'string' } },
    detail:         { type: 'string' },
  },
  required: ['ok'],
}

async function buildCheckAndBisect(committedResults, runStartSha, phaseTitle) {
  if (DRY_RUN || PER_COMMIT_BUILD || !runStartSha || !committedResults.length) return { ok: true, reverted_names: [] }
  const r = await schemaAgent(
    `${AGENT_RULES}
Batch-end build check for this run's ${committedResults.length} lift commit(s)
(${runStartSha}..HEAD). SERIAL — one git-mutating command at a time.
BUILD = timeout 165 rtk python3 tools/build/build.py -q --target halo 2>&1 | tail -30
  passes iff the output has NO "error:" line and NO "Error 2" / "*** " marker
  (a "[timed-out]"/killed build counts as a FAIL).

1. Run BUILD on HEAD. Passes → return {"ok":true,"reverted_names":[]}. Done.
2. Fails → bisect by hand:
   rtk git bisect start HEAD ${runStartSha}
   Then repeat: run BUILD at the commit git checked out; mark it with
   \`rtk git bisect good\` or \`rtk git bisect bad\`; until git prints
   "<sha> is the first bad commit". If git marks ${runStartSha} itself bad, or
   the first bad commit is not in ${runStartSha}..HEAD, the breakage is not
   this run's: \`rtk git bisect reset\` and return ok=false,
   detail="base_broken: <first error line>".
   rtk git bisect reset
3. Revert the first bad commit (Bash timeout 600000, foreground — see [STALL
   EXCEPTION]):
   HALO_BATCH_COMMIT=1 rtk git revert --no-edit <bad sha>
   On a revert conflict: rtk git revert --abort, return ok=false,
   detail="revert_conflict: <bad sha>".
4. Run BUILD on HEAD again. Passes → done. Fails → repeat steps 2-3 ONCE more
   (bisect from ${runStartSha} again), then stop either way.
Map every reverted commit to its function(s) from its subject
(rtk git log -1 --format=%s <sha>) against this run's committed list:
${committedResults.map(c => `  ${c.name} (${c.obj})`).join('\n')}
Return ok (final HEAD builds), reverted_names, reverted_shas, and detail (first
build error line of each failure).`,
    { label: 'batch-build-check', phase: phaseTitle, ...M.commit, schema: BUILD_CHECK_SCHEMA })
  if (!r) {
    log('⚠ batch-end build check returned null — the land gate\'s clean build is the remaining backstop')
    // Unknown, not broken: don't turn a dead agent into a build_broken stop.
    return { ok: true, unchecked: true, reverted_names: [] }
  }
  if (r.ok && !(r.reverted_names || []).length) log('✓ batch-end halo build ok')
  else log(`${r.ok ? '◐' : '✗'} batch-end build: ${r.ok ? 'fixed by reverting' : 'STILL BROKEN after'} ${(r.reverted_names || []).join(', ') || 'no revert'} — ${r.detail || ''}`)
  return { ...r, reverted_names: r.reverted_names || [] }
}

// Apply a batch-end revert to the result rows (committed → reverted_build).
function markBuildReverted(rows, reverted) {
  const names = new Set(reverted)
  for (const r of rows) {
    if (names.has(r.name) && (r.status === 'committed' || r.status === 'promoted')) {
      r.status = 'reverted_build'
      r.reason = `reverted at batch-end build check (was: ${r.reason || ''})`
    }
  }
}

async function squashByObject(committedResults, runStartSha, phaseTitle) {
  if (DRY_RUN || !runStartSha || committedResults.length < 2) return { squashed: 0 }

  const stretches = []
  for (const r of committedResults) {
    const last = stretches[stretches.length - 1]
    if (last && last.obj === r.obj) last.items.push(r)
    else stretches.push({ obj: r.obj, items: [r] })
  }
  const multi = stretches.filter(s => s.items.length >= 2)
  if (!multi.length) return { squashed: 0 }

  const plan = await schemaAgent(
    `${AGENT_RULES}
Squash this run's per-function commits into per-object commits. SERIAL, one
git-mutating command at a time — do not run two at once.

1. rtk git rev-list --reverse ${runStartSha}..HEAD
   Count the lines. It MUST equal ${committedResults.length} (this run's
   committed-function count, listed below in commit order, oldest first). If
   the count does NOT match, something else committed to this branch during
   the run — STOP, do not touch history, return
   {"ok":false,"reason":"commit_count_mismatch"}.
2. Those SHAs map 1:1 onto this list, in this order (index 0 = oldest):
${committedResults.map((r, i) => `   [${i}] ${r.name} (${r.obj})`).join('\n')}
3. Squash ONLY these stretches of consecutive same-object commits, and
   PROCESS THEM LAST-TO-FIRST (never first-to-last — resetting an earlier
   stretch first would discard later stretches that are still separate
   commits sitting above it):
${multi.map((s, si) => `   stretch ${si}: [${s.items.map(i => i.name).join(', ')}] — object "${s.obj}", ${s.items.length} commits`).join('\n')}

   For each stretch, in that order:
   a. beforeSha = the SHA of the commit immediately before this stretch's
      FIRST item in the numbered list from step 2 (i.e. the previous
      stretch's last commit, or ${runStartSha} if this is the very first
      stretch in the whole run).
   b. rtk git reset --soft <beforeSha>
   c. MSG=$(mktemp /tmp/halo-commit-msg.XXXXXX)
      rtk python3 tools/audit/generate_lift_commit.py --batch-name "<N> functions from <obj>" > "$MSG"
      (N = this stretch's commit count, obj = this stretch's object, e.g.
      "9 functions from xbox_texture_cache.obj")
      HALO_BATCH_COMMIT=1 rtk git commit -F "$MSG" && rm -f "$MSG"
      (Same prefix, same reason: a squash re-commits trees already on this
      branch, so the land gate's single run covers them.)
   d. rtk git rev-parse --short HEAD — record this as the stretch's new sha.
   Do NOT amend, force-push, or touch any commit outside the numbered list.
4. rtk git log --oneline ${runStartSha}..HEAD and sanity-check: every stretch
   above is now exactly one line, every function NOT in any stretch (a
   singleton, different object than its neighbors) is still its own unchanged
   commit. If anything looks wrong, report it in "reason" — do not force
   further changes to fix it.

Return {"ok":true,"squashed":<stretches actually collapsed>,"shas":[<new sha per squashed stretch, in the order you squashed them>]}
or {"ok":false,"reason":"<what went wrong, and current git log --oneline ${runStartSha}..HEAD>"}.`,
    { label: 'squash-by-object', phase: phaseTitle, ...M.commit, schema: SQUASH_SCHEMA })

  if (!plan || !plan.ok) {
    log(`⚠ squash-by-object skipped: ${plan ? plan.reason : 'agent_null'} — per-function commits stand as-is`)
    return { squashed: 0 }
  }
  log(`Squashed ${plan.squashed} object-stretch(es) of commits into one each`)
  return plan
}

// ── Ghidra MCP preflight ─────────────────────────────────────────────────────
// Added 2026-09-01: an MCP outage for the whole run window went undetected
// until each individual lift agent stalled ~165s waiting on a dead bridge,
// burning budget on every target before failing. Check once, up front, and
// abort the whole run (Select/Research/Lift and Improve alike) instead.
// Fused 2026-09-30 with the run-start SHA (squash boundary), the parked-ledger
// sync and, in improve mode, the retrieval warm-up: four mechanical agents that
// each ran one or two commands back to back.
const PREFLIGHT_SCHEMA = {
  type: 'object',
  properties: {
    ok:     { type: 'boolean' },
    detail: { type: 'string' },
    sha:    { type: 'string' },
    ledger: { type: 'string' },
  },
  required: ['ok', 'detail'],
}
let preflightSha = null
{
  const pf = await schemaAgent(
    `Run these steps in order and report each result. Do not attempt any fix.
1. python3 tools/audit/check_ghidra_mcp.py
   ok=true if it exits 0 (Ghidra MCP bridge reachable); ok=false with
   detail="<exact failure output>" if it exits non-zero or fails to run.
   If ok=false, STOP here and return.
2. rtk git rev-parse HEAD — sha = the full 40-char hash, nothing else.
3. Sync the parked ledger; ledger = the last summary line of EACH (two lines):
   rtk python3 tools/lift/park.py reconcile --apply 2>&1 || true
   rtk python3 tools/lift/park.py migrate --apply 2>&1 || true
${IMPROVE ? `4. Warm the retrieval query server (best-effort) — run as ONE Bash call:
${WARM_RETRIEVAL_CMD}
   Append "retrieval=up" or "retrieval=cold" to ledger.
` : ''}Return ok, detail, sha, ledger.`,
    { label: 'preflight', phase: IMPROVE ? 'Improve' : 'Select', ...M.mechanical, schema: PREFLIGHT_SCHEMA })
  if (!pf || !pf.ok) {
    log(`✗ Ghidra MCP preflight failed — aborting run: ${pf ? pf.detail : 'agent_null'}`)
    return { committed: 0, promoted: 0, reason: 'ghidra_mcp_down', detail: pf ? pf.detail : 'agent_null' }
  }
  log('✓ Ghidra MCP preflight ok')
  if (pf.ledger) log(`Ledger sync: ${pf.ledger.replace(/\n/g, ' | ')}`)
  // The squash boundary must be a real hash; a garbled one disables squash.
  preflightSha = (!DRY_RUN && /^[0-9a-f]{40}$/i.test(String(pf.sha || '').trim())) ? String(pf.sha).trim() : null
}

// ── Improve pass ────────────────────────────────────────────────────────────
// Drain the parked ledger: for each parked sub-bar function the improve model
// hasn't tried, re-research (context is lost across the agent boundary),
// warm-start from the parked best patch, re-lift with the improve model, and
// either promote (>=90 / gate-accepted) or re-park (records the attempt so the
// ledger drains model-by-model instead of re-trying the same model forever).
if (IMPROVE) {
  phase('Improve')
  const XM = M.improve.model
  log(`Improve pass: re-lifting up to ${GOAL} parked functions with ${XM}-${M.improve.effort}${DRY_RUN ? ' (dry run — no commits)' : ''}`)
  if (OBJECTS) log(`(object filter not applied in improve mode — park.py next drains globally by score)`)

  // Squash boundary, ledger sync and retrieval warm-up all ran in the fused
  // preflight agent above.
  const runStartSha = preflightSha

  const improved = []
  const seen = new Set()
  let promoted = 0
  let noProgress = 0
  let istop = 'ledger_drained'

  while (promoted < GOAL) {
    if (budget.total && budget.remaining() < 80000) { istop = 'budget_low'; break }
    if (noProgress >= STOP_ON_FAIL) { istop = 'no_progress'; break }

    const nx = await schemaAgent(nextPrompt(XM), { label: 'improve-next', phase: 'Improve', ...M.mechanical, schema: NEXT_SCHEMA })
    if (!nx || !nx.found || !nx.name) { istop = 'ledger_drained'; break }
    if (seen.has(nx.name)) { istop = 'ledger_not_advancing'; break }  // cycle guard
    seen.add(nx.name)

    const rec = { name: nx.name, addr: nx.addr || '', obj: nx.obj || '', source_path: nx.source_path || '', best_score: nx.best_score || 0 }
    log(`[improve ${promoted}/${GOAL}] ${rec.name} (${rec.addr}) parked at ${rec.best_score}% — tried by: ${nx.tried_models || '?'}`)

    // 1. Fingerprint-validated mechanical bundle (no prose research phase).
    const rawBrief = await schemaAgent(bundlePrompt(rec), { label: `bundle:${rec.name}`, phase: 'Improve', ...M.mechanical, schema: BUNDLE_SCHEMA })
    const brief = rawBrief ? { ...rawBrief, neighbors: rawBrief.retrieval && rawBrief.retrieval.neighbor ? JSON.stringify(rawBrief.retrieval.neighbor) : '' } : null
    if (brief && brief.cache && brief.cache.context === 'hit') cacheMetrics.hits++
    if (brief && brief.cache && brief.cache.context === 'miss') cacheMetrics.misses++
    cacheMetrics.ghidra_builds += (brief && brief.cache && brief.cache.ghidra_builds) || 0
    if (!brief || brief.pre_screen === 'infra_blocked') {
      istop = 'infra_blocked'; improved.push({ ...rec, status: 'infra_blocked', reason: 'ghidra_unavailable' }); break
    }
    if (brief.pre_screen === 'skip_already_in_source') {
      // It landed via another path since it was parked — mark the record done.
      await agent(
        `Mark the parked record for ${rec.name} promoted (it is already implemented in source):
rtk python3 tools/lift/park.py promote --name ${JSON.stringify(rec.name)} --commit "$(git rev-parse --short HEAD)" 2>/dev/null || true`,
        { label: `promote-obsolete:${rec.name}`, phase: 'Improve', ...M.mechanical })
      improved.push({ ...rec, status: 'already_landed', reason: brief.skip_reason || 'already in source' })
      continue
    }

    // 2. Warm-start from the parked best patch (stale patch → cold re-derive).
    const ap = await schemaAgent(applyPrompt(rec.name), { label: `apply:${rec.name}`, phase: 'Improve', ...M.mechanical, schema: APPLY_SCHEMA })
    const warm = !!(ap && ap.applied)

    // 2b. Refresh the score-context pack the re-lift model is about to read
    // (liftPrompt's SCORE CONTEXT section, `artifacts/score_context/<name>.json`)
    // now that the warm-started patch is actually on disk. That file may be
    // whatever an attempt weeks ago last wrote — stale classification points
    // the improve model at the wrong fix. Cheap mechanical refresh, same
    // shape as the redelink/permute steps below (mechanical model, no schema
    // needed — the file on disk is the product, not a structured return).
    if (warm) {
      const refreshSrc = brief.source_path || rec.source_path
      await agent(
        `Refresh the VC71 score-context pack for ${rec.name} so it reflects the warm-started patch on disk:
rtk python3 tools/verify/vc71_verify.py ${refreshSrc} -f ${rec.name} --no-cache 2>&1 | tail -5 || true`,
        { label: `refresh-context:${rec.name}`, phase: 'Improve', ...M.mechanical })
    }

    // 3. Re-lift with the improve model (escalation framing, prior score to beat).
    const liftBrief = { ...brief, obj: brief.obj || rec.obj, source_path: brief.source_path || rec.source_path }
    const priorNotes = { notes: nx.last_notes || '', tried: nx.tried_summary || '' }

    // Warm-started AND parked in the pure byte-tuning band (65-84, same band the
    // Lift-phase escalation gates on) → the on-disk candidate already builds and
    // was already believed faithful by whoever parked it; try the improve
    // model's persona on the SAME lever-tuning approach before spending a full
    // cold re-lift. A cold-start record (no prior patch survived to apply) has
    // no existing candidate to tune, so it always takes the full path below.
    let a
    let viaOptimize = false   // optimizer edits are vc71_verify-only → commit must build (needsBuild)
    if (warm && classifyBand(rec.best_score) === 'fail_check_cap') {
      const mo = await schemaAgent(matchOptimizerPrompt(rec.name, rec.addr, liftBrief.obj, liftBrief.source_path, rec.best_score, liftBrief.neighbors), {
        label: `improve-optimize:${rec.name}`, phase: 'Improve', agentType: 'vc71-match-optimizer', ...M.improve, schema: MATCH_OPTIMIZER_SCHEMA,
      })
      if (mo && typeof mo.vc71_score === 'number' && mo.vc71_score > rec.best_score) {
        a = { status: 'needs_verify', vc71_score: mo.vc71_score, source_file: liftBrief.source_path, reason: mo.reason || '' }
        viaOptimize = true
        log(`  ${rec.name} improve-optimize: ${rec.best_score}% → ${mo.vc71_score}% (skipping full re-lift)`)
      } else {
        log(`  ${rec.name} improve-optimize made no improvement over ${rec.best_score}% — falling back to full re-lift (${IMPROVE_MODEL})`)
      }
    }
    if (!a) {
      a = await schemaAgent(liftPrompt(liftBrief, true, rec.best_score, warm, priorNotes), {
        label: `improve-lift:${rec.name}`, phase: 'Improve', agentType: 'auto-lift-analyst', ...M.improve, schema: LIFT_RESULT_SCHEMA,
      })
    }
    if (!a || a.status === 'infra_blocked') { istop = 'infra_blocked'; improved.push({ ...rec, status: 'infra_blocked', reason: 'agent_null' }); break }
    if (a.status !== 'needs_verify') {
      // build_failed / skipped: re-park records the improve-model attempt (so it
      // won't be re-picked) and reverts, preserving the prior best patch.
      await parkBuilt(liftBrief, a.source_file || liftBrief.source_path, a.vc71_score || 0, M.improve, `improve_${a.status}`, a.cap_reason || '', 'Improve', a.reason || '')
      noProgress++; improved.push({ ...rec, status: 're_parked', reason: `improve ${a.status}` }); continue
    }

    let score   = a.vc71_score || 0
    let srcFile = a.source_file || liftBrief.source_path
    let band    = classifyBand(score)
    log(`  improve-lift ${rec.name}: ${score}% (band=${band}, was ${rec.best_score}%, ${warm ? 'warm' : 'cold'}-start)`)

    // (retired) A redelink retry ran here for any non-passing band. VC71 scores
    // are derived from the pristine XBE + the committed bounds table, so a fresh
    // Ghidra export cannot move the number.
    if (band === 'pass_permute') {
      const ps = await maybePermute(rec.name, 'Improve')
      if (ps !== null) { score = ps; band = classifyBand(score); viaOptimize = false }
    }

    if (band === 'pass' || band === 'pass_permute') {
      // A permute pass re-runs lift_pipeline, so it clears viaOptimize (above).
      const ipath = `improve:${warm ? 'warm' : 'cold'}${viaOptimize ? '+optimize' : ''}`
      const outcome = await gateThenCommit(liftBrief, score, srcFile, ipath, 'Improve')
      if (outcome.committed) {
        promoted++; noProgress = 0
        improved.push({ ...rec, status: 'promoted', vc71_score: score, reason: outcome.rationale })
        log(`✓ promoted ${rec.name} ${score}% (was ${rec.best_score}%)`); continue
      }
      if (outcome.verdict === 'build_failed') {
        // Tree already reverted by the commit agent; the parked best patch stands.
        noProgress++
        improved.push({ ...rec, status: 're_parked', vc71_score: score, reason: outcome.rationale })
        log(`✗ ${rec.name} ${score}% — ${outcome.rationale}`); continue
      }
      if (outcome.dryRun) {
        improved.push({ ...rec, status: 'would_promote', vc71_score: score, reason: outcome.rationale })
        await agent(revertPrompt(rec.name), { label: `revert-dry-run:${rec.name}`, phase: 'Improve', ...M.mechanical })
        log(`○ ${rec.name} ${score}% (dry-run, would promote)`); noProgress++; continue
      }
      // gate held despite passing band → re-park with the improve attempt recorded.
    }

    await parkBuilt(liftBrief, srcFile, score, M.improve, `improve_pass_${band}`, a.cap_reason || '', 'Improve', a.reason || a.equiv_reason || '')
    noProgress++
    improved.push({ ...rec, status: 're_parked', vc71_score: score, reason: `improve→${score}% (${band})` })
    log(`◐ ${rec.name} re-parked at ${score}% (was ${rec.best_score}%)`)
  }

  phase('Report')
  const ibuild = await buildCheckAndBisect(improved.filter(r => r.status === 'promoted'), runStartSha, 'Report')
  markBuildReverted(improved, ibuild.reverted_names)
  if (!ibuild.ok) istop = `build_broken (${ibuild.detail || 'see batch-build-check'})`
  const proms = improved.filter(r => r.status === 'promoted')
  log(`\n── Improve pass complete (${istop}) ─────────────────`)
  log(`Promoted:   ${proms.length}${DRY_RUN ? ` (dry-run; ${improved.filter(r => r.status === 'would_promote').length} would-promote)` : ''}${proms.length ? ' — ' + proms.map(p => `${p.name} ${p.vc71_score}%`).join(', ') : ''}`)
  log(`Re-parked:  ${improved.filter(r => r.status === 're_parked').length}`)
  log(`Already landed: ${improved.filter(r => r.status === 'already_landed').length}`)
  if (budget.total) log(`Budget remaining: ~${Math.round(budget.remaining() / 1000)}k tokens`)

  const sq = ibuild.reverted_names.length ? { squashed: 0 } : await squashByObject(proms, runStartSha, 'Report')

  await agent(
    `Append an improve-pass summary to artifacts/auto_lift/goal_progress.md (create if missing).

## Improve pass — ${proms.length} promoted (${istop}), model=${XM}

| function | addr | was% | now% | action | reason |
|---|---|---|---|---|---|
${improved.map(r => `| ${r.name} | ${r.addr || '-'} | ${r.best_score ?? '-'} | ${r.vc71_score ?? '-'} | ${r.status} | ${r.reason || ''} |`).join('\n')}

Then regenerate the actionable-unblock queue:
rtk python3 tools/lift/park.py followups --write --limit 50
Report its "wrote N follow-up(s)" line.`,
    { label: 'improve-log', phase: 'Report', ...M.mechanical })

  phaseTokens.improve = Math.max(0, budget.spent() - runTokenStart)
  return {
    mode: 'improve',
    improve_model: XM,
    stop_reason: istop,
    promoted: proms.length,
    would_promote: improved.filter(r => r.status === 'would_promote').length,
    re_parked: improved.filter(r => r.status === 're_parked').length,
    already_landed: improved.filter(r => r.status === 'already_landed').length,
    commits_squashed: sq.squashed || 0,
    phase_token_deltas: phaseTokens,
    cache: cacheMetrics,
    ghidra_builds: cacheMetrics.ghidra_builds,
    final_outcome: istop,
    tokens_spent: budget.spent() - runTokenStart,
    // Cross-batch dedupe contract — see --excludeAddrs.
    attempted: [...new Set(improved
      .filter(r => r.status !== 'skipped')
      .map(r => normAddr(r.addr))
      .filter(Boolean))],
    results: improved,
  }
}

// ── Phase 1: Select ───────────────────────────────────────────────────────────

phase('Select')
log(`Goal: lift ${GOAL} functions at >=90% VC71${DRY_RUN ? ' (dry run — no commits)' : ''}`)
if (OBJECTS) log(`Object filter (hard): ${OBJECTS.join(', ')}`)
if (CRITERIA) log(`Extra criteria (soft): ${CRITERIA}`)

// Squash boundary: only commits made from here on are ours to collapse.
// Captured, together with the parked-ledger sync (reconcile drops records for
// functions that landed via another path since they were parked, migrate
// upgrades legacy records), by the fused preflight agent above.
const runStartSha = preflightSha

const BATCH_LIMIT = Math.min(60, Math.max(30, GOAL * 3))

// When an --objects allowlist is set, query the selector PER OBJECT
// (`select --object <name> --min-score 0`) instead of a single global top-N
// `select --limit N`. The global select ranks by score across ALL objects, so a
// low-scoring or freshly-started object (no source file yet, no delinked ref →
// functions score ~30) never appears in the top BATCH_LIMIT, and the code-side
// object post-filter then yields an empty queue — even though the object has
// many perfectly liftable functions. Per-object select surfaces every candidate
// in the allowlisted object(s), so a fresh-object goal-lift actually gets work.
const RETURN_CAP = OBJECTS ? 200 : BATCH_LIMIT
const selectCmds = OBJECTS
  ? OBJECTS.map(o => `rtk python3 tools/lift/llm_auto_lift.py -q select --object ${o} --min-score 0 --limit ${RETURN_CAP} --json 2>&1`)
  : [`rtk python3 tools/lift/llm_auto_lift.py -q select --limit ${BATCH_LIMIT} --json 2>&1`]

const selectPrompt =
  `Select next batch of Halo CE Xbox functions to lift.
${OBJECTS
    ? `Run EACH of these commands (one per allowlisted object) and concatenate all their JSON arrays into one combined list before parsing:\n${selectCmds.join('\n')}`
    : `Run: ${selectCmds[0]}`}
(-q is a GLOBAL flag before the subcommand — it makes the JSON compact.)
This emits a JSON array; each element has: total_score, lane, prior_fail,
prior_fail_attempts, and target{addr, name, object_name, has_reg_args,
source_path, score_details{...}}. For each element parse:
  addr=target.addr, name=target.name, obj=target.object_name, score=total_score,
  has_reg_args=target.has_reg_args (boolean, verbatim),
  prior_fail=prior_fail (boolean, verbatim — top-level, NOT under target),
  parked_attempts=parked_attempts (number, verbatim — OMIT the field entirely if
    absent from the JSON; do NOT substitute 0, which reads as "never attempted"),
  parked_best_score=parked_best_score (number, verbatim — omit if absent),
  parked_status=parked_status (string, verbatim — omit if absent),
  delinked=(target.score_details.delinked_ref is present and > 0),
  source_exists=(target.score_details.source_exists is present),
  lane=lane.
Do NOT invent these booleans — copy them from the JSON. (The code-side pre-screen
depends on has_reg_args/lane/prior_fail/parked_* being exact.)

${ADDRS
    ? `Filter: an explicit ADDRESS PIN-LIST is in force for this run. Return EVERY element
whose target.addr matches one of these (hex, case-insensitive), REGARDLESS of its lane —
manual-lift and defer entries included, they are a deliberate operator choice:
  ${[...ADDRS].map(a => '0x' + a.toString(16)).join(', ')}
Return nothing else. Do NOT apply any lane filter; the code-side pre-screen handles it.`
    : `Filter: keep lane=="auto-lift" (also allow "cache-context"${LIFT_MANUAL ? ' and "manual-lift"' : ''});`}
skip xbox_crt.obj (NT-import/CRT wrappers).
Do NOT drop prior_fail entries yourself — return them with the flag set and let
the code-side pre-screen decide, so it can keep them when the queue would
otherwise run dry.
${OBJECTS
    ? `HARD RESTRICTION: only return candidates whose obj is one of: ${OBJECTS.join(', ')}. Discard everything else (this is also enforced in code afterward, so don't waste entries on other objects).`
    : `Keep the selector's own order (total_score descending) — do NOT re-sort. The
selector already ranks by each TU's demonstrated VC71 history (selector v2).`}
${CRITERIA ? `\nADDITIONAL USER CRITERIA (apply on top of the rules above): ${CRITERIA}\n` : ''}
Return up to ${RETURN_CAP} entries, each with the parsed fields above.`

// Select is the ONE serial single point of failure in this workflow: every other
// stage runs inside parallel()/pipeline(), where a dead agent degrades to null
// and the batch carries on. A single API 529 here used to abort the whole
// auto-session run, so retry before giving up.
//
// These retries are immediate -- workflow scripts have no sleep primitive
// (Date.now/Math.random throw by design, to keep runs resumable). That is
// acceptable because agent() only returns null AFTER the harness has exhausted
// its own internal retries, so the backoff has already elapsed. Against a
// sustained outage this burns through its attempts quickly rather than waiting
// the outage out; that case is meant to surface, not be papered over.
const SELECT_ATTEMPTS = 3
let selection = null
for (let attempt = 1; attempt <= SELECT_ATTEMPTS; attempt++) {
  selection = await schemaAgent(selectPrompt, {
    label: attempt === 1 ? 'select' : `select-retry-${attempt}`,
    phase: 'Select', ...M.extract, schema: TARGETS_SCHEMA,
  })
  if (selection) break
  log(`select returned null (attempt ${attempt}/${SELECT_ATTEMPTS}) — infra failure, retrying`)
}

// Distinguish an infra failure from a genuinely empty frontier. Collapsing the
// two into "empty_queue" made auto-session report a transient API 529 as
// "queue_exhausted" and skip every remaining batch (2026-07-27: 1 function
// landed, 3 batches abandoned, frontier was nowhere near empty).
if (!selection || !selection.targets) {
  log(`Select failed after ${SELECT_ATTEMPTS} attempts — infra, NOT an empty queue`)
  return { committed: 0, goal: GOAL, reached_goal: false, skipped: 0, reverted: 0, reason: 'select_agent_null' }
}

if (selection.targets.length === 0) {
  log('No viable targets in queue')
  return { committed: 0, goal: GOAL, reached_goal: false, skipped: 0, reverted: 0, reason: 'empty_queue' }
}

// Mechanical enforcement of --objects — don't rely on the agent alone to
// honor the hard restriction stated in the prompt above.
let targets = selection.targets
if (OBJECTS) {
  const wanted = new Set(OBJECTS.map(o => o.toLowerCase()))
  targets = targets.filter(t => wanted.has((t.obj || '').toLowerCase()))
  log(`Object filter kept ${targets.length}/${selection.targets.length} candidates`)
  if (targets.length === 0) {
    log('No viable targets in queue after object filter')
    return { committed: 0, goal: GOAL, reached_goal: false, skipped: 0, reverted: 0, reason: 'empty_queue_after_filter' }
  }
}
if (ADDRS) {
  const before = targets.length
  targets = targets.filter(t => ADDRS.has(parseInt((t.addr || '0').replace(/^0x/i, ''), 16)))
  log(`Address pin-list kept ${targets.length}/${before} candidates (${ADDRS.size} pinned)`)
  const found = new Set(targets.map(t => parseInt((t.addr || '0').replace(/^0x/i, ''), 16)))
  const missing = [...ADDRS].filter(a => !found.has(a))
  if (missing.length) log(`⚠ ${missing.length} pinned addr(s) not surfaced by the selector: ${missing.map(a => '0x' + a.toString(16)).join(', ')} — raise selector --limit or check lane/score filters`)
  if (targets.length === 0) {
    log('No viable targets in queue after addr pin-list')
    return { committed: 0, goal: GOAL, reached_goal: false, skipped: 0, reverted: 0, reason: 'empty_queue_after_filter' }
  }
}
log(`Selected ${targets.length} candidates across ${new Set(targets.map(t => t.obj)).size} objects: ${[...new Set(targets.map(t => t.obj))].join(', ')}`)

// ── Code-side pre-screen — drop targets the SELECTOR already proved unsuitable,
// using authoritative facts (has_reg_args / lane / addr) rather than re-deriving
// them in 6 Opus research agents that drift. Saves the research tokens entirely.
const CRT_LO = 0x1d0000, CRT_HI = 0x1de000, XAPI_LO = 0x1cf900
const codeSkips = []
targets = targets.filter(t => {
  const a = parseInt((t.addr || '0').replace(/^0x/i, ''), 16)
  // Already attempted by an earlier batch of this auto-session run — a second
  // attempt in the same session has the same inputs and the same model, so it
  // can only reproduce the first outcome at full cost.
  // An operator --addrs pin beats the machine-supplied exclusion.
  const pinnedAddr = ADDRS && ADDRS.has(a)
  if (EXCLUDE_ADDRS && EXCLUDE_ADDRS.has(a) && !pinnedAddr) {
    codeSkips.push({ ...t, status: 'skipped', reason: 'skip_excluded_prior_batch (attempted earlier this session)' })
    return false
  }
  // Confirmed structural cap: never re-serve to a cold lift, even in the
  // 85-89 near-miss band. shader_environment_texture_animation_evaluate sat
  // at 86.2% x10 because the 85 wall treated the fucompp-assert cap as
  // recoverable. Improve-pass cannot move these either. Pinned addrs always
  // bypass -- an explicit --addrs is an operator override.
  //
  // Removed 2026-09-01: skip_parked_repeat (attempts>=2 & best<85% skip).
  // tools/lift/llm_auto_lift.py's own selector docs (lines ~1379-1385) say this
  // signal doesn't separate parked-forever from later-promoted targets — it
  // already applies a -15 ranking penalty upstream, which is the correct
  // place for this signal to act. The code-side hard skip additionally
  // starved the IMPROVE pass of targets and, per FUN_00173b40 (90.3% on
  // attempt 7 after 6 sub-88% attempts), can suppress genuine late successes.
  if (!pinnedAddr && !IMPROVE &&
      (t.parked_status === 'capped_confirmed' || t.parked_status === 'confirmed_cap')) {
    codeSkips.push({ ...t, status: 'skipped', reason: `skip_confirmed_cap (best ${t.parked_best_score}%)` })
    return false
  }
  // Reviewer rejected over a callee's kb.json decl: re-lifting reproduces the
  // same REJECT (race_team_can_win_game: 19 attempts at 93%). park.py reconcile
  // (ledger-sync above) releases it once that decl changes.
  if (!pinnedAddr && !IMPROVE && t.parked_status === 'blocked_review') {
    codeSkips.push({ ...t, status: 'skipped', reason: `skip_blocked_review (best ${t.parked_best_score}%)` })
    return false
  }
  if (t.has_reg_args === true && !LIFT_REG_ARGS) { codeSkips.push({ ...t, status: 'skipped', reason: 'skip_reg_args (selector: @reg-defined prologue → sub-bar)' }); return false }
  if (Number.isFinite(a) && a >= CRT_LO && a < CRT_HI) { codeSkips.push({ ...t, status: 'skipped', reason: 'skip_nt_import (CRT/SEH region 0x1d0000-0x1de000)' }); return false }
  // Pinned targets bypass the lane gate: an explicit --addrs entry is a
  // deliberate operator choice, often of a manual-lift/cache-context lane fn.
  const pinned = ADDRS && ADDRS.has(a)
  // XAPILIB thread/handle exports (CloseHandle, SetThreadPriority, ...) sit just
  // below the CRT region, misattributed to d3d_intimacy.obj. Research agents
  // rejected them as xbox_kernel_import 54 times across the 09-2x campaigns,
  // burning ~4 slots per batch. Pinned targets still get through (0x1cf97c
  // UnhandledExceptionFilter was lifted deliberately).
  if (!pinned && Number.isFinite(a) && a >= XAPI_LO && a < CRT_LO) { codeSkips.push({ ...t, status: 'skipped', reason: 'skip_nt_import (XAPILIB region 0x1cf900-0x1d0000)' }); return false }
  if (!pinned && t.lane && t.lane !== 'auto-lift' && t.lane !== 'cache-context' && !(LIFT_MANUAL && t.lane === 'manual-lift')) { codeSkips.push({ ...t, status: 'skipped', reason: `lane=${t.lane} (not auto-liftable)` }); return false }
  return true
})
if (codeSkips.length) log(`Code pre-screen dropped ${codeSkips.length} before research (${codeSkips.filter(s => s.reason.startsWith('skip_confirmed_cap')).length} confirmed-cap, ${codeSkips.filter(s => s.reason.startsWith('skip_blocked_review')).length} blocked-review, ${codeSkips.filter(s => s.reason.startsWith('skip_reg_args')).length} reg-args, ${codeSkips.filter(s => s.reason.startsWith('skip_nt_import')).length} CRT/SEH, ${codeSkips.filter(s => s.reason.startsWith('lane=')).length} lane, ${codeSkips.filter(s => s.reason.startsWith('skip_excluded_prior_batch')).length} excluded-prior-batch)`)
if (targets.length === 0) {
  log('No viable targets after code pre-screen')
  return { committed: 0, goal: GOAL, reached_goal: false, skipped: codeSkips.length, reverted: 0, reason: 'empty_queue_after_prescreen' }
}

// Reachability check — a 20-goal against a 7-candidate queue burns hours before
// the ceiling is discovered (scenario.obj run, 2026-07-04). Say it up front.
if (targets.length < GOAL) {
  log(`⚠ REACHABILITY: only ${targets.length} candidates for a goal of ${GOAL} — even at 100% yield this run cannot reach the goal; effective ceiling is ${targets.length}. Consider widening --objects or lowering --goal.`)
}

// ── Phase 2: Parallel research (read-only, batched 6) ────────────────────────

phaseTokens.select = Math.max(0, budget.spent() - runTokenStart)
phase('Research')

// Fingerprint validation now owns Ghidra preflight and fragment xrefs. A valid
// bundle hit performs no Ghidra call; a stale/missing entry preflights, rebuilds,
// and publishes the immutable artifact before any analyst starts.

const RESEARCH_BATCH = 2

// ── Just-in-time research ────────────────────────────────────────────────────
// This used to research EVERY selected target up front (the selector returns
// ~30) before the lift loop ran. But the loop stops the instant GOAL commits
// land, so most of that work was thrown away: measured across the four
// 2026-08-01 auto-session runs, 143 of 207 research agents (69%) briefed a
// target that was never lifted — roughly 20% of total session spend, and the
// single largest waste item.
//
// Now: research a small window (GOAL + lookahead), and top it up only when the
// lift loop actually runs dry. A run that hits GOAL on its first GOAL targets
// pays for GOAL + LOOKAHEAD briefs instead of 30. Worst case (every target
// pre-screens out) is unchanged — we still walk the whole queue.
const RESEARCH_LOOKAHEAD = 2

const briefs    = []
const okBriefs  = []
const results   = []
const bundleByName = new Map()
let   nextTarget = 0
let   pendingInfra = 0

// Export missing per-function delinked references for the EQUIVALENCE lane.
// (Not for VC71: scoring derives its reference from the pristine XBE + the
// committed bounds table and never reads delinked/. unicorn_diff EXECUTES the
// oracle, so it still needs an object with real relocations.)
// Called per research window rather than once for the whole queue.
async function delinkPrefetch(newOk) {
  const needDelink = newOk.filter(b => !b.delinked_exists)
  if (!needDelink.length) return
  log(`Delink prefetch: exporting ${needDelink.length} missing reference(s)`)
  await agent(
    `${AGENT_RULES}

Export per-function delinked references via mcp__ghidra-live__export_delinked_object
for each function below. For each: find the exact body end address in Ghidra
(get_function_by_address / decompile), then export selection_mode="range",
range="<start_no_0x>-<end_no_0x>" to delinked/functions/<addr_no_0x>.obj.
The exporter writes to the MAIN repo delinked/ — copy into this checkout's
delinked/functions/ if they differ. Verify each with objdump -t.

${needDelink.map(b => `- ${b.name} at ${b.addr}`).join('\n')}

Return one line per function: <name> exported|failed <path-or-reason>.`,
    { label: 'delink-prefetch', phase: 'Research', ...M.mechanical }
  )
}

// Research forward until `want` NEW viable briefs exist or the queue is spent.
// Non-viable briefs are recorded into `results` as they are discovered, so the
// final report is identical to the eager version's.
// Returns { fresh, infra } (infra = briefs that came back infra_blocked).
// background=true: the lift loop started this call WITHOUT awaiting it, to
// overlap the next window's research with the current lift (see prefetch in
// the Lift loop). Its tokens are then left to the Lift phase's residual count,
// because a budget.spent() delta taken across concurrent agents would also
// swallow the lift's own spend. Safe to overlap: nextTarget advances
// synchronously before the first await, and only one call is ever in flight.
async function researchMore(want, background) {
  const tokenBefore = budget.spent()
  const fresh = []
  let infra = 0
  while (fresh.length < want && nextTarget < targets.length) {
    const take  = Math.min(RESEARCH_BATCH, Math.max(1, want - fresh.length))
    const batch = targets.slice(nextTarget, nextTarget + take)
    nextTarget += batch.length
    log(`Research: ${batch.length} target(s) [queue ${nextTarget}/${targets.length}]`)
    const bb = await parallel(batch.map(t => () => schemaAgent(bundlePrompt(t), {
      label: `bundle:${t.name}`, phase: 'Research', ...M.mechanical, schema: BUNDLE_SCHEMA,
    })))
    for (const raw of bb.filter(Boolean)) {
      const b = {
        ...raw,
        neighbors: raw.retrieval && raw.retrieval.neighbor
          ? JSON.stringify(raw.retrieval.neighbor) : '',
      }
      bundleByName.set(b.name, b)
      if (b.cache && b.cache.context === 'hit') cacheMetrics.hits++
      if (b.cache && b.cache.context === 'miss') cacheMetrics.misses++
      cacheMetrics.ghidra_builds += (b.cache && b.cache.ghidra_builds) || 0
      briefs.push(b)
      if (b.pre_screen === 'ok') {
        okBriefs.push(b); fresh.push(b)
      } else if (b.pre_screen === 'infra_blocked') {
        infra++
        results.push({ addr: b.addr, name: b.name, obj: b.obj, status: 'infra_blocked', reason: b.skip_reason || 'ghidra_unavailable' })
      } else {
        results.push({ addr: b.addr, name: b.name, obj: b.obj, status: 'skipped', reason: b.skip_reason || b.pre_screen })
      }
    }
  }
  if (fresh.length) await delinkPrefetch(fresh)
  if (!background) phaseTokens.research += Math.max(0, budget.spent() - tokenBefore)
  return { fresh, infra }
}

for (const s of codeSkips) {
  results.push({ addr: s.addr, name: s.name, obj: s.obj, status: 'skipped', reason: s.reason })
}

pendingInfra = (await researchMore(GOAL + RESEARCH_LOOKAHEAD)).infra
log(`Research window: ${okBriefs.length} viable of ${briefs.length} briefed (${targets.length - nextTarget} target(s) held back)`)

// ── Phase 3: Serial lift loop, gated to reach GOAL or exhaust the queue ──────

phase('Lift')

let consecutiveFails = 0
let consecutiveInfra = pendingInfra
let stopReason = 'queue_exhausted'

let liftIdx = 0
let escalationsThisRun = 0   // targets that entered the escalation ladder (bounded by MAX_ESCALATIONS)
let escalationMisses = 0     // consecutive escalations that ended below the bar (ESCALATION_MISS_LIMIT)
let prefetch = null          // in-flight background researchMore() promise, or null
while (true) {
  const committed = results.filter(r => r.status === 'committed')
  if (committed.length >= GOAL) { stopReason = 'goal_reached'; break }

  if (consecutiveInfra >= 2) { stopReason = 'infra_blocked_twice'; break }
  if (consecutiveFails >= STOP_ON_FAIL) { stopReason = 'stop_on_fail_reached'; break }

  if (budget.total && budget.remaining() < 80000) { stopReason = 'budget_low'; break }

  // Ran out of briefed targets — top up rather than stopping, unless the
  // selector's queue is genuinely spent.
  if (liftIdx >= okBriefs.length) {
    // Prefer the window already being researched in the background.
    const r = prefetch ? await prefetch : await researchMore(GOAL - committed.length + RESEARCH_LOOKAHEAD)
    prefetch = null
    consecutiveInfra += r.infra
    // researchMore only returns empty once the selector queue is spent.
    if (!r.fresh.length) { stopReason = 'queue_exhausted'; break }
    continue
  }

  const brief = okBriefs[liftIdx++]

  // This is the last briefed target: research the next window WHILE it lifts,
  // instead of stalling the loop on research after it. Bundles only write
  // artifacts/ (plus delinked/ via delinkPrefetch), never src/ or kb.json. A
  // bundle for a target in the TU being lifted can fingerprint mid-edit source;
  // that only costs a stale-fingerprint rebuild when its own lift starts.
  if (!prefetch && liftIdx >= okBriefs.length && nextTarget < targets.length && GOAL - committed.length > 1) {
    prefetch = researchMore(GOAL - committed.length - 1 + RESEARCH_LOOKAHEAD, true)
  }

  log(`[${committed.length}/${GOAL} committed] next: ${brief.name} (${brief.addr})`)

  // ── Attempt 1: Opus lift ─
  const a1 = await schemaAgent(liftPrompt(brief, false, null), {
    label: `lift1:${brief.name}`, phase: 'Lift', agentType: 'auto-lift-analyst', ...M.reason, schema: LIFT_RESULT_SCHEMA,
  })

  if (!a1 || a1.status === 'infra_blocked') {
    consecutiveInfra++
    results.push(a1 ? { ...a1, obj: brief.obj } : { addr: brief.addr, name: brief.name, obj: brief.obj, status: 'infra_blocked', reason: 'agent_null' })
    continue
  }
  consecutiveInfra = 0

  if (a1.status === 'skipped') { results.push({ ...a1, obj: brief.obj }); continue }
  if (a1.status === 'build_failed') {
    consecutiveFails++
    results.push({ ...a1, obj: brief.obj })
    continue
  }

  let lift    = a1   // the active lift result — carries in-agent permute/equiv flags
  let score   = a1.vc71_score || 0
  let srcFile = a1.source_file || brief.source_path
  let band    = classifyBand(score)
  let path    = 'pass1' + (a1.redelinked ? '+redelink' : '') + (a1.permuted ? '+permute' : '')
  let lastME  = M.reason   // {model,effort} of the current attempt (for parked records)
  log(`  lift1 ${brief.name}: ${a1.status} ${score}% (band=${band}${a1.capped ? ', capped: ' + (a1.cap_reason || '?') : ''})`)

  // ── VERIFY-SKIPPED GUARD: a build-passing lift whose VC71 was never measured
  // is an INFRASTRUCTURE gap, not a 0% match. Two runs (f8e29209, daa39ee6)
  // parked 9 faithful lifts as "below_65pct @ 0%" this way, and the bogus fails
  // tripped stop_on_fail.
  // The cause used to be a missing delinked reference and the repair was a
  // redelink agent. References are now DERIVED from the pristine XBE + the
  // committed tools/verify/function_bounds.json, so no export can restore a
  // score: an unmeasured function is either absent from the bounds table or its
  // TU does not compile under VC71 — both need a human, not another agent turn.
  // Park WITHOUT counting a consecutive failure, same as before.
  // Broadened 2026-09-01: originally gated on status === 'needs_verify', but
  // journal evidence showed score===0 lifts landing under OTHER status labels
  // (build_failed/skipped mislabels) that re-measured at 95.9/88.6/94.1% —
  // real ports, not below_65pct fails. A score of exactly 0 with
  // vc71_measured !== true is never a genuine match result (real 0% matches
  // still set vc71_measured === true), so treat it as unmeasured regardless
  // of status.
  // Hardened 2026-09-02 (mechanical, agent-independent): the previous form
  // still trusted the agent's own `vc71_measured` flag to VETO the score===0
  // signal, so one agent asserting vc71_measured:true alongside a 0 score
  // parked a never-verified lift as below_65pct AND charged a consecutive
  // fail. The flag is now only ever used to ADD to the unmeasured set, never
  // to subtract from it. The one thing that can prove a real measured 0.0% is
  // pipeline text: vc71_verify prints "VC71 match: 0.0%" for a genuine
  // zero-match, and a SKIP / "no reference" / "not scored" marker when it
  // never scored the function. `reason` is the only free text
  // LIFT_RESULT_SCHEMA carries back, so that is what we grep; with no such
  // text, a 0 score is unmeasured, full stop.
  const scoreUnusable = !Number.isFinite(score) || score === 0
  const pipelineText  = String(a1.reason || '')
  const textSaysSkip  = /\b(skip(ped)?|no reference|no delinked reference|not scored|no usable objdiff)\b/i.test(pipelineText)
  const textSaysZero  = /VC71 match:\s*0\.0\s*%/i.test(pipelineText) && !textSaysSkip
  const mechUnmeasured = scoreUnusable && !textSaysZero
  const verifySkipped = a1.vc71_measured === false || mechUnmeasured
  if (verifySkipped) {
    if (mechUnmeasured && a1.vc71_measured !== false) {
      log(`  ${brief.name}: verify artifact (0%) — not counted as fail (agent reported vc71_measured=${String(a1.vc71_measured)})`)
    }
    log(`  ${brief.name}: VC71 never measured — bounds-table entry missing or VC71 compile failed`)
    await parkBuilt(brief, srcFile, 0, lastME, 'verify_skipped_no_ref', 'VC71 unmeasured: no bounds entry (tools/verify/function_bounds.json) or the TU failed to compile under VC71; not a lift failure', 'Lift', a1.reason || '')
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: null, reason: 'verify_skipped_no_ref (infrastructure — VC71 never measured; do not treat as below_65pct)' })
    continue
  }

  // (retired) The redelink fallback lived here. A fresh Ghidra export can no
  // longer change a VC71 score — scoring reads the XBE, not delinked/ — so the
  // stage was pure token spend (wf_927b1d1d: 50% of a run's tokens, 0 gains).
  // delinked/ exports still matter for the equivalence lane; see delinkPrefetch.

  // ── 65-84%: explicit structural-cap gate (P3). The lift agent ran
  // tools/analysis/classify_cap.py in step 7: a cap_confidence==="high" verdict is
  // an AUTHORITATIVE deterministic cap (@reg-defining prologue / ledger-confirmed /
  // prior-cap-no-improvement) — provably futile to retry, so it MUST park and never
  // escalate. An "inconclusive" verdict falls back to the agent's own CAP_TABLE
  // judgment. Either way "capped" now decides escalation, but its provenance is
  // explicit and logged, not an opaque model boolean.
  // Agent prose is never a routing gate. Only a high-confidence verdict backed
  // by the current immutable attempt evidence may stop escalation.
  const treatAsCapped = a1.capped === true && a1.cap_confidence === 'high'
  const capProvenance = a1.cap_confidence === 'high' ? 'deterministic(classify_cap.py)' : 'agent-judgment'

  // ── Atlas-rule short-circuit (docs/history/agent-model-routing-2026-08.md §5,
  // §7.2). When the score-context classifier already produced a concrete recipe-
  // atlas rule id for this function, the remaining gap is a KNOWN mechanical
  // lever, not open-ended reasoning — apply it once at the cheap M.extract tier
  // before paying for an optimizer rung. Deliberately placed BEFORE the budget /
  // MAX_ESCALATIONS gate below and it does NOT increment escalationsThisRun: a
  // classified lever costs a fraction of a rung, so charging it a slot would let
  // the cheapest fixes crowd out the expensive ones. Clearing the bar here drops
  // straight through to the commit gate; a partial gain simply becomes the
  // ladder's new baseline for rung-gain comparison. The agent self-aborts after
  // one jq when the pack is missing or names no rule, and the try/catch means a
  // missing/unparseable pack can never fail the loop.
  if (band === 'fail_check_cap' && !treatAsCapped) {
    try {
      const al = await schemaAgent(atlasLeverPrompt(brief.name, brief.addr, brief.obj, srcFile, score), {
        label: `atlas-lever:${brief.name}`, phase: 'Lift', agentType: 'vc71-match-optimizer', ...M.extract, schema: MATCH_OPTIMIZER_SCHEMA,
      })
      if (al && typeof al.vc71_score === 'number' && al.vc71_score > score) {
        score  = al.vc71_score
        band   = classifyBand(score)
        path   = `${path}+atlas-lever`
        lastME = M.extract
        lift   = { ...lift, reason: al.reason || lift.reason }
        log(`  ${brief.name} atlas-lever (${M.extract.model}-${M.extract.effort}) → ${score}%${band === 'fail_check_cap' ? ' (still sub-bar — ladder continues from here)' : ' — cleared the bar, no escalation slot charged'}`)
      }
    } catch (e) {
      log(`  ${brief.name} atlas-lever skipped: ${(e && e.message) || e}`)
    }
  }

  if (band === 'fail_check_cap' && !treatAsCapped) {
    // Escalation caps (token discipline): the tune below can climb an effort
    // ladder (several optimizer passes), so bound both how many targets enter
    // it per run and whether we can afford to start one. When gated, park
    // attempt-1 — it is landable and the opt-in improve pass can drain it
    // later — instead of spending here. A budget/cap defer is NOT a lift
    // failure, so it does not increment consecutiveFails.
    const budgetOk = !budget.total || budget.remaining() >= ESCALATION_BUDGET_FLOOR
    const capOk    = MAX_ESCALATIONS <= 0 || escalationsThisRun < MAX_ESCALATIONS
    if (!budgetOk || !capOk) {
      const why = !budgetOk ? 'budget_floor' : 'escalation_cap'
      log(`  ${brief.name} ${score}% — escalation skipped (${why}); parked for the improve pass`)
      await parkBuilt(brief, srcFile, score, M.reason, `escalation_skipped_${why}`, a1.cap_reason || '', 'Lift', a1.reason || '')
      results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, reason: `escalation_skipped_${why}` })
      continue
    }
    escalationsThisRun++

    // Preserve the attempt-1 work before tuning: park keeps the best-scoring
    // patch as the outer safety net (the optimizer also reverts-on-regression
    // internally). Unlike the old cold-rewrite escalation this does NOT re-lift
    // — attempt 1 already builds and is believed faithful, so the optimizer
    // tunes THAT source in place, one score-recovery lever at a time.
    // CHECKPOINT ONLY (keepTree): the patch is saved and the attempt recorded,
    // but the tree is NOT reverted — every rung below edits srcFile in place and
    // the commit gate at the end commits from that same live tree. Reverting
    // here deleted the candidate before the first optimizer rung ever read it.
    // The terminal parks (structural_cap / escalation_exhausted / review-blocked
    // below) still revert, so a ladder that never clears the bar leaves a clean tree.
    await parkBuilt(brief, srcFile, score, M.reason, 'pre_escalation', a1.cap_reason || '', 'Lift', a1.reason || '', true)

    // Effort ladder (Opus, NOT Fable): start at high and step up to
    // xhigh ONLY for a target still below the 85% pass bar, not documented-
    // capped, and while budget remains. Most targets stop at the first rung.
    // The optimizer edits srcFile in place, so each rung builds on the last.
    let capReason  = ''
    let rungCapped = false
    const shortLadder = ESCALATION_MISS_LIMIT > 0 && escalationMisses >= ESCALATION_MISS_LIMIT && IMPROVE_EFFORTS.length > 1
    const rungs = shortLadder ? IMPROVE_EFFORTS.slice(0, 1) : IMPROVE_EFFORTS
    if (shortLadder) log(`  ${brief.name}: ${escalationMisses} escalations in a row missed the bar — first rung only (${rungs[0]})`)
    for (let ri = 0; ri < rungs.length; ri++) {
      const eff = rungs[ri]
      const ME  = { model: IMPROVE_MODEL, effort: eff }
      log(`  ${brief.name} ${score}% — vc71-match-optimizer ${IMPROVE_MODEL}-${eff} [rung ${ri + 1}/${rungs.length}] (not a structural cap: ${a1.cap_confidence || 'n/a'})`)
      const mo = await schemaAgent(matchOptimizerPrompt(brief.name, brief.addr, brief.obj, srcFile, score, brief.neighbors), {
        label: `match-optimize:${brief.name}:${eff}`, phase: 'Lift', agentType: 'vc71-match-optimizer', ...ME, schema: MATCH_OPTIMIZER_SCHEMA,
      })
      if (mo && typeof mo.vc71_score === 'number') {
        // Never let a lower/garbled number regress the ledger's best.
        score  = Math.max(score, mo.vc71_score)
        band   = classifyBand(score)
        path   = 'escalated+optimize'
        lastME = ME
        lift   = { ...lift, reason: mo.reason || lift.reason }
        if (mo.capped === true && mo.cap_confidence === 'high') {
          rungCapped = true; capReason = mo.cap_reason || 'unclassified'; break
        }
      } else {
        log(`  ${brief.name} ${score}% — match-optimizer (${eff}) returned no usable score`)
      }
      if (band !== 'fail_check_cap') break                               // crossed the pass bar — done climbing
      if (budget.total && budget.remaining() < ESCALATION_BUDGET_FLOOR) { // out of budget mid-ladder
        log(`  ${brief.name} ${score}% — budget floor reached, stopping ladder at ${eff}`)
        break
      }
    }
    // A documented cap is deterministic, not a sign of a capped-tail frontier
    // the ladder is wasting rungs on, so it leaves the streak alone.
    if (band !== 'fail_check_cap') escalationMisses = 0
    else if (!rungCapped) escalationMisses++
    if (rungCapped) {
      // Optimizer hit a documented ceiling (its own classify_cap equivalent).
      // Before parking, try an equivalence-backed commit — a confirmed cap is
      // deterministic (not model error), so it never counts as a fail either way.
      log(`  ${brief.name} ${score}% capped [fingerprinted-mechanical:optimizer]: ${capReason} — trying equivalence before parking`)
      const ce = await capEquivCommit(brief, score, srcFile, 'escalated+optimize', 'Lift', capReason)
      if (ce.committed) {
        log(`  ${brief.name} ${score}% — capped but equivalence-confirmed, committed: ${ce.rationale}`)
        results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'committed', vc71_score: score, reason: ce.rationale })
        continue
      }
      if (ce.build_failed) {
        // The commit agent already reverted the tree; nothing left to park.
        consecutiveFails++
        results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'build_failed', vc71_score: score, reason: ce.rationale })
        log(`✗ ${brief.name} ${score}% — ${ce.rationale}`)
        continue
      }
      log(`  ${brief.name} ${score}% capped [fingerprinted-mechanical:optimizer]: ${capReason} — parked (${ce.rationale}), no further escalation`)
      await parkBuilt(brief, srcFile, score, lastME, 'structural_cap', capReason, 'Lift', lift.reason || '')
      results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, reason: `structural_cap[fingerprinted-mechanical:optimizer]: ${capReason} (${ce.rationale})` })
      continue
    }
  } else if (band === 'fail_check_cap' && treatAsCapped) {
    // Structural cap — try equivalence-backed commit first (see capEquivCommit);
    // only park (with the cap hypothesis, for a future model to retry) if that
    // doesn't pass. Not confirm-cap: that would end retries.
    log(`  ${brief.name} ${score}% capped [${capProvenance}]: ${a1.cap_reason || 'unclassified'} — trying equivalence before parking`)
    const ce = await capEquivCommit(brief, score, srcFile, path, 'Lift', a1.cap_reason || 'unclassified')
    if (ce.committed) {
      log(`  ${brief.name} ${score}% — capped but equivalence-confirmed, committed: ${ce.rationale}`)
      results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'committed', vc71_score: score, reason: ce.rationale })
      continue
    }
    if (ce.build_failed) {
      consecutiveFails++
      results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'build_failed', vc71_score: score, reason: ce.rationale })
      log(`✗ ${brief.name} ${score}% — ${ce.rationale}`)
      continue
    }
    log(`  ${brief.name} ${score}% capped [${capProvenance}]: ${a1.cap_reason || 'unclassified'} — parked (${ce.rationale}), no escalation`)
    await parkBuilt(brief, srcFile, score, M.reason, 'structural_cap', a1.cap_reason || 'unclassified', 'Lift', a1.reason || '')
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, reason: `structural_cap[${capProvenance}]: ${a1.cap_reason || 'unclassified'} (${ce.rationale})` })
    continue
  }

  if (band === 'fail_revert') {
    await parkBuilt(brief, srcFile, score, lastME, 'below_65pct', '', 'Lift', lift.reason || '')
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, reason: `below_65pct` })
    continue
  }
  if (band === 'fail_check_cap') {
    // escalation ran and is still in [65,84) — park the best attempt for later.
    await parkBuilt(brief, srcFile, score, lastME, 'escalation_exhausted', '', 'Lift', lift.reason || '')
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, reason: 'escalation_exhausted' })
    continue
  }

  // ── 85-89%: one permuter pass before the review gate ───────────────────
  // FALLBACK ONLY — the lift agent normally permutes in-context (step 6c).
  if (band === 'pass_permute' && !lift.permuted) {
    log(`  ${brief.name} ${score}% — permuter pass (fallback)`)
    const ps = await maybePermute(brief.name, 'Lift')
    if (ps !== null && ps > score) score = ps
    path = `${path}+permute`
  }

  // ── Phase 3: commit gate (mechanical fast-path, reviewer on ambiguity) ──
  // `lift` carries the in-context equivalence result (step 6d) when it ran.
  const outcome = await gateThenCommit(brief, score, srcFile, path, 'Lift', lift)
  if (outcome.committed) {
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'committed', vc71_score: score, source_file: srcFile, reason: outcome.rationale })
    consecutiveFails = 0
    log(`✓ ${brief.name} ${score}% (${outcome.rationale})`)
  } else if (outcome.dryRun) {
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'would_commit', vc71_score: score, source_file: srcFile, reason: outcome.rationale })
    await agent(revertPrompt(brief.name), { label: `revert-dry-run:${brief.name}`, phase: 'Lift', ...M.mechanical })
    log(`○ ${brief.name} ${score}% (dry-run, would commit — reverted for clean state)`)
  } else if (outcome.verdict === 'build_failed') {
    // The commit agent reverted the tree after a failed halo build.
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'build_failed', vc71_score: score, source_file: srcFile, reason: outcome.rationale })
    log(`✗ ${brief.name} ${score}% — ${outcome.rationale}`)
  } else if (outcome.verdict === 'infra_blocked') {
    // The gate agent died (terminal API error or a missed StructuredOutput call
    // that schemaAgent already retried once). That is infra, not a verdict on
    // the lift: preserve the built candidate for the improve pass and move on
    // WITHOUT charging consecutiveFails, exactly like the verify-skipped guard.
    await parkBuilt(brief, srcFile, score, lastME, `structured_output_null:review`, '', 'Lift', lift.reason || '')
    consecutiveInfra++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, source_file: srcFile, reason: `structured_output_null:review (${outcome.rationale})` })
    log(`⚠ ${brief.name} ${score}% parked — commit-gate agent returned null (infra, not counted as a fail)`)
  } else if (score >= 85) {
    // Near-miss: lift is structurally sound, only runtime evidence blocked it.
    // Park (recoverable ledger) and do NOT count toward the consecutive-fail
    // stop — this is a deferred work item, not a failed lift.
    await parkBuilt(brief, srcFile, score, lastME, `${outcome.verdict}: ${outcome.rationale}`, '', 'Lift', lift.reason || '', false, outcome.blocked_by_callee)
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, source_file: srcFile, reason: `${outcome.verdict}: ${outcome.rationale}` })
    log(`◐ ${brief.name} ${score}% parked (review gate: ${outcome.verdict}${outcome.blocked_by_callee ? `, blocked_review on callee ${outcome.blocked_by_callee}` : ''}; patch in artifacts/parked/)`)
  } else {
    // Below 85 and review-blocked: still preserve the work (a different model
    // may push it over later) rather than checkout-discarding it.
    await parkBuilt(brief, srcFile, score, lastME, `${outcome.verdict}: ${outcome.rationale}`, '', 'Lift', lift.reason || '', false, outcome.blocked_by_callee)
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, source_file: srcFile, reason: `review<85: ${outcome.verdict}: ${outcome.rationale}` })
    log(`◐ ${brief.name} ${score}% parked (review gate <85: ${outcome.verdict})`)
  }
}

// A background research window may still be running when the loop stops; let
// it finish so no agent outlives the run (its skips still land in results).
if (prefetch) { await prefetch; prefetch = null }

// ── Phase 4: Report ───────────────────────────────────────────────────────────

const reportTokenStart = budget.spent()
phaseTokens.lift = Math.max(0, reportTokenStart - runTokenStart - phaseTokens.select - phaseTokens.research - phaseTokens.improve)
phase('Report')

const buildCheck = await buildCheckAndBisect(results.filter(r => r.status === 'committed'), runStartSha, 'Report')
markBuildReverted(results, buildCheck.reverted_names)
if (!buildCheck.ok && stopReason !== 'budget_low') stopReason = `build_broken (${buildCheck.detail || 'see batch-build-check'})`

const committed      = results.filter(r => r.status === 'committed')
const wouldCommit    = results.filter(r => r.status === 'would_commit')
const skipped        = results.filter(r => r.status === 'skipped')
const revertedVerify = results.filter(r => r.status === 'reverted_verify')
const revertedReview = results.filter(r => r.status === 'reverted_review')
const parked         = results.filter(r => r.status === 'parked')
const infra          = results.filter(r => r.status === 'infra_blocked')
const measuredResults = results.map(r => {
  const bundle = bundleByName.get(r.name) || {}
  return {
    ...r,
    fingerprint: bundle.fingerprint || '',
    retrieval_cohort: (bundle.retrieval && bundle.retrieval.cohort) || 'none',
  }
})
const retrievalCohorts = measuredResults.reduce((acc, r) => {
  acc[r.retrieval_cohort] = (acc[r.retrieval_cohort] || 0) + 1
  return acc
}, {})

log(`\n── Run complete (${stopReason}) ─────────────────────`)
log(`Committed:            ${committed.length}${DRY_RUN ? ` (dry-run: ${wouldCommit.length} would-commit)` : ''}`)
log(`Skipped (pre-screen): ${skipped.length}`)
log(`Reverted (verify):    ${revertedVerify.length}`)
log(`Reverted (review gate): ${revertedReview.length}`)
log(`Parked (>=85%, needs runtime evidence): ${parked.length}${parked.length ? ' — ' + parked.map(p => `${p.name} ${p.vc71_score}%`).join(', ') : ''}`)
log(`Infra-blocked:         ${infra.length}`)
log(`Evidence cache:        ${cacheMetrics.hits} hit / ${cacheMetrics.misses} miss / ${cacheMetrics.ghidra_builds} Ghidra build(s)`)
log(`Retrieval cohorts:     ${JSON.stringify(retrievalCohorts)}`)
if (budget.total) log(`Budget remaining: ~${Math.round(budget.remaining() / 1000)}k tokens`)

await agent(
  `Append a run summary to artifacts/auto_lift/goal_progress.md (create if missing).

## Goal-lift run — ${committed.length}/${GOAL} committed (${stopReason})

| function | addr | obj | vc71 | action | reason |
|---|---|---|---|---|---|
${measuredResults.map(r => `| ${r.name} | ${r.addr} | ${r.obj || '-'} | ${r.vc71_score ?? '-'} | ${r.status} | ${r.reason || ''} [cohort=${r.retrieval_cohort}] |`).join('\n')}

Then regenerate the actionable-unblock queue from this run's park/skip reasons
(dedupes callee-ABI blockers and untried score levers across all runs):
rtk python3 tools/lift/park.py followups --write --limit 50
Report its "wrote N follow-up(s)" line.`,
  { label: 'progress-log', phase: 'Report', ...M.mechanical }
)

const outcomeRows = measuredResults.filter(r => r.retrieval_cohort === 'retrieval' || r.retrieval_cohort === 'control')
if (outcomeRows.length) {
  const perTargetTokens = Math.round(Math.max(0, budget.spent() - runTokenStart) / outcomeRows.length)
  await agent(
    `Record retrieval experiment outcomes. Run each command exactly; these are metrics only:\n${outcomeRows.map(r => {
      const outcome = (r.status === 'reverted_review' || String(r.reason || '').includes('REJECT'))
        ? 'reviewer_rejected' : (String(r.reason || '').includes('runtime_failed') ? 'runtime_failed' : r.status)
      const routeModel = IMPROVE ? M.improve : M.reason
      return `rtk python3 tools/lift/research_bundle.py record-outcome --target ${JSON.stringify(r.addr)} --cohort ${r.retrieval_cohort} --outcome ${JSON.stringify(outcome)} --tokens ${perTargetTokens} --fingerprint ${JSON.stringify(r.fingerprint || '')} --model-id ${JSON.stringify(routeModel.model)} --effort ${JSON.stringify(routeModel.effort)} --route ${JSON.stringify(IMPROVE ? 'improve' : 'goal_lift')} --json`
    }).join('\n')}`,
    { label: 'retrieval-outcomes', phase: 'Report', ...M.mechanical })
}

// A batch-end revert adds commits the squash's 1:1 count check would reject.
const sq = buildCheck.reverted_names.length
  ? (log('squash-by-object skipped: batch-end build check reverted commits'), { squashed: 0 })
  : await squashByObject(committed, runStartSha, 'Report')

phaseTokens.report = Math.max(0, budget.spent() - reportTokenStart)

return {
  goal: GOAL,
  reached_goal: committed.length >= GOAL,
  stop_reason: stopReason,
  committed: committed.length,
  would_commit: wouldCommit.length,
  skipped: skipped.length,
  reverted_verify: revertedVerify.length,
  reverted_review: revertedReview.length,
  commits_squashed: sq.squashed || 0,
  reverted_build: results.filter(r => r.status === 'reverted_build').length,
  parked: parked.length,
  infra_blocked: infra.length,
  phase_token_deltas: phaseTokens,
  cache: cacheMetrics,
  ghidra_builds: cacheMetrics.ghidra_builds,
  retrieval_cohorts: retrievalCohorts,
  final_outcome: stopReason,
  tokens_spent: budget.spent() - runTokenStart,
  // Cross-batch dedupe contract: every address this run actually spent a lift
  // attempt on — committed, parked (any reason, including verify-skipped),
  // build-failed, or infra-blocked. Pre-screen skips are NOT included: they
  // never cost an attempt and a later batch under different flags may well
  // want them. auto-session unions these across batches and feeds them back
  // as --excludeAddrs.
  attempted: [...new Set(measuredResults
    .filter(r => r.status !== 'skipped')
    .map(r => normAddr(r.addr))
    .filter(Boolean))],
  results: measuredResults,
}
