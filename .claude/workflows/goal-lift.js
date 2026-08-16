export const meta = {
  name: 'goal-lift',
  description: 'Goal-mode auto-lift: lift, verify, review-gate, and commit Halo CE Xbox functions until N are committed at >=90% VC71 or the queue is exhausted',
  phases: [
    { title: 'Select',   detail: 'Frontier scoring + liftability filtering' },
    { title: 'Research', detail: 'Parallel Ghidra context gathering (read-only)' },
    { title: 'Lift',     detail: 'Serial: lift -> verify (goal90 bands) -> permute/escalate -> mechanical/review gate -> commit' },
    { title: 'Improve',  detail: 'Re-lift parked sub-bar functions with a different model, promote or re-park' },
    { title: 'Report',   detail: 'Summary: committed, skipped, parked, gate holds' },
  ],
}

// This workflow exists so that the two required subagents are GUARANTEED to
// run: a prose-only slash command can't force tool calls, but agent() here is
// real code. xbox-halo-re-analyst does Phase-1 RE + implementation;
// xbox-halo-lift-reviewer is the fail-closed gate every commit must clear.

const GOAL         = (args && args.goal) || 20
const STOP_ON_FAIL = (args && args.stopOnFail) || 3
const DRY_RUN      = !!(args && args.dryRun)
// --improve: skip Select/Research/Lift over the frontier; instead drain the
// parked ledger (artifacts/parked/), re-lifting sub-bar functions with the
// improve model for perspective diversity. This is the payoff of never
// discarding sub-bar work — see tools/lift/park.py.
const IMPROVE      = !!(args && args.improve)
// --cacheContext: also run llm_auto_lift.py cache-context per target so the
// enrichment_hook injects callee/struct-offset tables during research decompiles.
// Off by default — it is a per-target Ghidra sweep; the free decompile_hook
// neighbor/hazard injection is the primary retrieval signal.
const CACHE_CONTEXT = !!(args && args.cacheContext)

// Model/effort policy (single point of control). Rationale:
// - Opus-low costs ~the same as Sonnet-low but gives better results, so every
//   structured-extraction/mechanical-but-consequential stage uses Opus-low
//   rather than Sonnet at any effort (Sonnet-high costs MORE than Opus-high).
// - Reasoning stages (lift, review) use Opus-high.
// - Cheap deterministic tool-runs (revert, permute-run, equiv-run, redelink,
//   park, report) use Haiku-low.
// - The escalation / improve tune defaults to Opus, climbing reasoning EFFORT
//   (ladder medium -> xhigh -> max; most targets stop at the first rung).
//   Fable is opt-in only (--improveModel fable), per user policy 2026-08-10:
//   never route to fable unless explicitly requested. For reference,
//   routing_stats.py measured fable-high at an 80% promote rate with a +14.7pp
//   mean score gain over 16 improve handoffs, vs 52% for opus-high
//   (2026-08-07) — so it is worth requesting for the hardest parked targets.
const IMPROVE_MODEL = (args && args.improveModel) || 'opus'
// Effort ladder for the in-place score tune. Each rung re-runs the optimizer at
// a higher effort, but only for a target still below the pass bar, not capped,
// and while budget remains. Override as a comma list, e.g. --improveEfforts
// "medium,max". For a non-opus model, default to a single 'high' rung.
const IMPROVE_EFFORTS = (args && args.improveEfforts)
  ? String(args.improveEfforts).split(',').map(s => s.trim()).filter(Boolean)
  : (IMPROVE_MODEL === 'opus' ? ['medium', 'xhigh', 'max'] : ['high'])
// Do NOT start an escalation rung below this remaining-budget floor (an
// optimizer run can be sizable). Higher than the batch-loop floor (80k) so a
// rung never strands the loop. Override with --escalationBudgetFloor.
const ESCALATION_BUDGET_FLOOR = (args && args.escalationBudgetFloor) || 120000
// Max targets that may enter the escalation ladder per goal-lift run, bounding
// token blast radius regardless of how many land in [65,85). 0 = unlimited.
// Override with --maxEscalations.
const MAX_ESCALATIONS = (args && args.maxEscalations != null) ? args.maxEscalations : 3
const M = {
  mechanical: { model: 'haiku', effort: 'low'  },  // tool-run + parse
  // select + research. Deliberately NOT downgraded to haiku, though research is
  // schema-shaped and was the biggest single agent count: just-in-time research
  // (see RESEARCH_LOOKAHEAD) already cut its volume ~80%, from ~30 briefs per
  // batch to GOAL+2, so the model saving left on the table is small -- while the
  // brief is the highest-leverage input the lifter gets, and its pre_screen
  // verdict decides whether a target is lifted at all. Small saving, expensive
  // failure mode. Revisit only with a measured A/B on pre_screen accuracy.
  extract:    { model: 'opus',  effort: 'low'  },  // select, research (schema-shaped)
  // Commit runs a fixed 6-command script whose only judgement is "does the
  // build log contain an error: line" -- the same shape as the 19 sites already
  // on `mechanical`. Measured 31 agents / ~4% of session spend on opus for it.
  commit:     { model: 'haiku', effort: 'low'  },  // runs the clean-build gate
  reason:     { model: 'opus',  effort: 'high' },  // lift, review
  improve:    { model: IMPROVE_MODEL, effort: IMPROVE_EFFORTS[0] },  // improve-pass base rung
}
// --reviewEffort: A/B lever for reviewer cost (docs/plans/agent-model-routing-2026-08.md
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
// --liftRegArgs: lift @<reg>-defined/reg-arg targets instead of pre-screen
// dropping them. The lift phase already handles @reg (CALLEE PREP step 2 +
// @<reg> step 4); the VC71 comparator models the @reg-DEFINED prologue
// (lift-learnings §32), so scores are honest. Proven 2026-07-14 (players.obj
// 11/13) via a hand-edited copy; this makes it a first-class flag.
const LIFT_REG_ARGS = !!(args && args.liftRegArgs)
// --minCommitScore: floor for the mechanical NEEDS_RUNTIME+equiv acceptance
// in reviewThenCommit (default 85, the historical lane). Set 88 to enforce
// "no sub-88 equiv-backed commits" policy.
const MIN_COMMIT = Number((args && args.minCommitScore) || 85)

// ── Schemas ───────────────────────────────────────────────────────────────────

// Facts come straight from llm_auto_lift.py select --json (authoritative). The
// code-side pre-screen below uses has_reg_args / lane / addr to drop unsuitable
// targets BEFORE research, instead of re-deriving them in 6 Opus research agents
// (which drift). See LiftTarget/SelectedTarget in tools/llm_auto_lift.py.
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
          parked_status:      { type: 'string'  },  // park.py ledger: parked|promoted
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

const BRIEF_SCHEMA = {
  type: 'object',
  properties: {
    addr:            { type: 'string' },
    name:            { type: 'string' },
    obj:             { type: 'string' },
    source_path:     { type: 'string' },
    kb_entry:        { type: 'string' },
    decompiled:      { type: 'string' },
    disasm_notes:    { type: 'string' },
    callees:         { type: 'string' },
    hazards:         { type: 'string' },
    // Worked examples captured from the decompile_hook retrieval injection:
    // most-similar already-ported function(s) with their VC71 % + C source, and
    // hazard briefs. Threaded into the lift prompt so the lifter matches the
    // winning idiom instead of re-deriving. Empty if the server was cold.
    neighbors:       { type: 'string' },
    delinked_exists: { type: 'boolean' },
    // ok | skip_reg_args | skip_trivial | skip_seh | skip_nt_import | skip_already_in_source | infra_blocked
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

// ── Prompt builders ───────────────────────────────────────────────────────────

const AGENT_RULES =
  `OPERATING RULES (read first):
[WORKTREE] If your CWD is a git WORKTREE, do ALL work here with RELATIVE paths.
NEVER \`cd /mnt/g/dev/halo\` (the user's main checkout) and NEVER run git
mutations (stash/checkout/commit/reset) against any repo but this one.
mcp__ghidra-live__export_delinked_object writes its .obj to the MAIN repo
delinked/ (path like G:\\dev\\halo\\delinked\\...); if you are in a worktree,
COPY the exported file into THIS worktree instead of cd-ing to main.
[STALL] Any single shell command that can run >2 min (permuter, unicorn
equivalence, a full clean build) MUST be wrapped so it cannot run silently
past 180s and trip the harness stall detector that kills the whole run:
  timeout 150 <cmd> 2>&1 || echo "[timed-out]"
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

const researchPrompt = (t) =>
  `Gather Ghidra context for ${t.name} at ${t.addr} (${t.obj}). READ-ONLY — no edits.

1. Ghidra MCP preflight already ran globally for this batch — do NOT re-run
   check_ghidra_mcp.py.
   TWO ways to reach Ghidra; the CLI pack (step 3a) is ALWAYS available because it
   talks to the bridge over HTTP, independent of this session's MCP attachment:
     (a) Ghidra MCP tools (mcp__ghidra__decompile_function etc.) — preferred when present.
     (b) the cached context pack written by step 3a — the fallback.
   A missing/erroring MCP tool is NOT infra_blocked on its own. Only return
   pre_screen="infra_blocked", skip_reason="ghidra_unavailable" when step 3a ALSO
   fails to produce a pack with a non-empty decompile_c or disassembly.

2. KB LOOKUP: rtk jq '[.. | objects | select(.addr? == "${t.addr}")] | .[0]' kb.json

2b. SOURCE CHECK (before Ghidra, to avoid wasted decompile tokens):
    addr_no0x=$(printf '%08x' $((16#${t.addr.replace('0x', '')})))
    rtk rg "^[a-zA-Z_][a-zA-Z0-9_*]+ FUN_\${addr_no0x}\\b" src/ --no-heading -l 2>/dev/null
    Also check the real name if the kb entry has one:
    rtk rg "^[a-zA-Z_][a-zA-Z0-9_*]+ ${t.name}\\b" src/ --no-heading -l 2>/dev/null
    If either grep returns a .c file path, the function is already implemented:
    → pre_screen="skip_already_in_source", skip_reason="already implemented: <file>"
    Return immediately — no Ghidra call needed.

3a. ENRICH + FALLBACK SOURCE (run BEFORE decompile so the enrichment hook can
    inject callee/struct tables, and so a pack exists if MCP tools are absent):
    timeout 240 rtk python3 tools/llm_auto_lift.py cache-context --target ${t.addr} --force 2>&1 || echo "[cache-context-skip]"
    The pack lands at artifacts/auto_lift/context_cache/${t.name}.json with fields:
    decompile_c, disassembly, callees, callers, callee_details, call_site_audit,
    struct_offsets, buffer_alias, buffer_warnings. Read what you need with rtk jq, e.g.
      rtk jq -r '.decompile_c' artifacts/auto_lift/context_cache/${t.name}.json
    If the pack has a non-empty decompile_c or disassembly, you have Ghidra context —
    proceed normally even if every MCP tool below is unavailable.

3. DECOMPILE: Ghidra MCP decompile_function at ${t.addr}
   If that tool is not exposed in your toolset or it errors, use the step-3a pack's
   .decompile_c instead (and .disassembly for anything the decompile leaves unclear).
   Note in disasm_notes which source you used.
   This fires a PostToolUse retrieval hook that injects a system message with
   similar ALREADY-PORTED functions (worked examples: decl + C source + their
   VC71 %) and hazard warnings from similar functions that FAILED.
   CAPTURE the 1-2 most-similar high-VC71 worked examples (decl + C body + VC71 %)
   and any hazard briefs into the "neighbors" field, VERBATIM. This is the main
   channel by which the lift agent learns the winning idiom, and it is LOST if you
   do not copy it into the brief. If NO retrieval system message appeared, the
   server was cold — you MAY run it once (wrapped; ~75s on a cold model load):
     printf '%s' "<decompiled body>" > /tmp/${t.addr.replace('0x','')}.decomp.c
     timeout 120 rtk python3 tools/retrieval/query.py --file /tmp/${t.addr.replace('0x','')}.decomp.c --obj-name ${t.obj} --min-vc71 85 --prompt 2>&1 || echo "[retrieval-timeout]"

4. PRE-SCREEN (return immediately with pre_screen=<reason> if any match):
${LIFT_REG_ARGS ? `   - (reg-arg lifting ENABLED for this run: do NOT skip on unaff_/in_EAX/in_ECX
     or @<reg> callees. Instead, record in disasm_notes the EXACT register each
     implicit input arrives in — verified against the disassembly prologue, not
     just Ghidra's decompile — plus each @<reg> callee's register contract.)` : `   - Decompile has unaff_, in_EAX, in_ECX               → "skip_reg_args"
   - Any callee has @<reg> and is NOT in kb.json          → "skip_reg_args"`}
   - Body is 1-3 lines wrapping one FUN_ unchanged        → "skip_trivial"
   - Contains __SEH_prolog / __SEH_epilog                 → "skip_seh"
   - Calls xboxkrnl NT/kernel imports (Nt*/Ob*/Ke*/Rtl*/Ex*/Ps*/Io*/Hal*)
     OR addr in 0x1d0000-0x1de000 CRT region              → "skip_nt_import"
   Otherwise pre_screen="ok"

5. CALLEES: Ghidra MCP get_function_callees at ${t.addr}.
   If unavailable, use the step-3a pack: rtk jq '.callees, .callee_details' \\
     artifacts/auto_lift/context_cache/${t.name}.json
   Return JSON array: [{addr,name,has_reg_args,in_kb}]

6. DELINKED CHECK (for the EQUIVALENCE lane only — VC71 scoring never reads
   delinked/): objdump -t delinked/*.obj 2>/dev/null | grep -i "${t.name.replace('FUN_', '')}"
   delinked_exists=true if found.

7. HAZARD SCAN: rtk python3 tools/audit/check_lift_hazards.py 2>&1 | grep -A2 "${t.addr.replace('0x', '')}"

8. DISASM NOTES (only if FPU ops, struct access, or >2 CALLs): Ghidra MCP disassemble_function.
   If unavailable, use the step-3a pack's .disassembly (rtk jq -r '.disassembly' ...).
   Return key observations only: push order per CALL, FPU subtraction direction, buffer sizes. Max 400 words.

Return full brief. Use field name "source_path" for the intended source file path
(create-target guess if the function has no source yet). Populate "neighbors" with
the captured retrieval worked-examples + hazard briefs (empty string if none).`

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
CONTEXT — do NOT re-call Ghidra:
  KB:       ${brief.kb_entry}
  Decomp:   ${brief.decompiled}
  Disasm:   ${brief.disasm_notes || 'none'}
  Callees:  ${brief.callees}
  Hazards:  ${brief.hazards}

SCORE CONTEXT (if a prior VC71 run scored this function, read it FIRST — it is
already-computed diagnostic data, cheaper than re-deriving the same conclusion
from the objdiff/disasm yourself):
  rtk jq '{scores, frame, classification}' artifacts/score_context/${brief.name}.json 2>/dev/null || true
  classification[].action names the specific fix for each detected pattern
  (loadw_field_width, frame_mismatch, anchor_collapse, etc.) — apply those
  actions before trying any other hypothesis. Missing file = no prior run
  recorded yet; proceed normally.

WORKED EXAMPLES (similar functions already ported, with their VC71 %; match their
idioms — casts, x87 order, struct-store shape — and expect a comparable score.
A near-identical neighbor capped below 90% is evidence THIS one is capped too):
${brief.neighbors || '  (none — retrieval server was cold)'}

STEPS:
1. REFERENCE — nothing to do. The VC71 reference is DERIVED: the pristine XBE's
   bytes for this function, bounded by the committed tools/verify/function_bounds.json.
   Make NO ghidra-live calls to export one, and do not treat a low score as a
   reference problem. (A prefetch stage may have exported delinked/functions/<addr_no_0x>.obj
   — that object is for the EQUIVALENCE lane, which executes the oracle and needs
   real relocations. It has no effect on the VC71 %.)

2. CALLEE PREP — for any callee with has_reg_args=true and in_kb=false:
   add to kb.json with @<reg> + update tools/kb_reg_baseline.json.

3. IMPLEMENT — C89 in ${brief.source_path} at address-ordered position.
   Rules: C89 only, no inline ASM, preserve control flow + side-effect order.
   MSVC intrinsics → C: _ftol2→(int)cast, _chkstk→normal locals, _allmul→(int64_t)a*b.
   Trace every CALL: first PUSH is last arg. Check FPU subtraction direction and cross-product order.
   SKILLS (mandatory doctrine — read each SKILL.md and apply its checklist):
   .claude/skills/lift-decompiler-traps, .claude/skills/lift-arg-hazards,
   .claude/skills/lift-frame-hazards, .claude/skills/lift-silent-bugs.

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
   timeout 165 rtk python3 tools/lift_pipeline.py --target ${brief.name} --no-metadata-update --verify-policy goal90 2>&1 || echo "[lift_pipeline timed-out]"
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

7. STRUCTURAL-CAP CLASSIFY (only if vc71_score is in [65,84]) — do NOT eyeball it;
   run the deterministic classifier (explicit rules: @reg-defining prologue,
   parked-ledger confirmed/prior cap, float-arg-lowering diff signature). Pass
   --score-context if artifacts/score_context/${brief.name}.json exists (the
   verify step writes it) — it lets the classifier prove a float-arg-lowering
   cap from THIS attempt's own diff, not just from repeat history:
     rtk python3 tools/analysis/classify_cap.py --name ${brief.name} --addr ${brief.addr} \\
       --score <vc71_score> --decl '<this function's kb declaration>' \\
       --score-context artifacts/score_context/${brief.name}.json
   - If it returns "cap_confidence":"high" → this is an AUTHORITATIVE cap: report
     capped=true, cap_reason=its cap_reason, cap_confidence="high". Do NOT escalate.
   - If it returns "cap_confidence":"inconclusive" → the script cannot prove a cap;
     apply YOUR OWN judgment against the patterns below, set capped/cap_reason, and
     report cap_confidence="inconclusive".
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
timeout 165 rtk python3 tools/lift_pipeline.py --target ${name} --no-metadata-update --verify-policy goal90 2>&1 || echo "[timed-out]"

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
  rtk jq '{scores, frame, classification}' artifacts/score_context/${name}.json

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

// Recipe-atlas short-circuit (docs/plans/agent-model-routing-2026-08.md §5/§7.2).
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

FIRST, run exactly one command — read the existing score-context pack:
  rtk jq '{scores, frame, classification}' artifacts/score_context/${name}.json
If that file is missing, or classification is empty/null, or every entry is a
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

Gather your own evidence before deciding:
- Source diff: rtk git diff -- ${srcFile} kb.json
- ABI audit: rtk python3 tools/audit/audit_reg_abi.py (or reuse the pass already run by generate_lift_commit.py)
- Hazard scan: rtk python3 tools/audit/check_lift_hazards.py --files ${srcFile}
- Caller/callee/disassembly context around each CALL touched by this lift (Ghidra MCP)
- Relevant kb.json declarations and register args for ${brief.name}

Apply your decision policy and return your verdict.`

// Cheap mechanical gate — runs the same hazard/ABI checks the commit stage will
// enforce, so a clean high-% lift can skip the Opus-high reviewer entirely.
// Report booleans ONLY; do not edit or fix anything.
const mechGatePrompt = (brief, srcFile) =>
  `Mechanical pre-commit gate for ${brief.name} (${brief.addr}). Report booleans ONLY —
do NOT edit, fix, build, or commit anything.

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
Return hazards_clean, abi_clean, risky_calls, warns, and a one-line detail per non-clean finding.`

const commitPrompt = (name, sourceFile, reason) =>
  `${AGENT_RULES}

Commit the lift of ${name}${reason ? ' (' + reason + ')' : ''}.

FIRST verify a CLEAN FULL build — the per-function lift build is incremental and
can miss cross-TU breakage:
  timeout 165 rtk python3 tools/build/build.py -q --target halo 2>&1 | tee /tmp/glbuild.txt
  Build PASSED if /tmp/glbuild.txt has NO "error:" line and NO "Error 2" / "*** " marker.
  If FAILED:
    rtk git checkout -- src/ kb.json tools/kb_reg_baseline.json
    Return exactly "BUILD_FAILED: <first error line>" and do NOT commit.

If ${sourceFile} is a NEW translation unit, it must also be registered in
src/CMakeLists.txt or it is never linked: the function stays ported=true in
kb.json with no body in the XBE, so the ORIGINAL Xbox code keeps running. The
build above still exits 0 either way, so this is silent. Check and fix before
committing:
  grep -c "$(basename ${sourceFile})" src/CMakeLists.txt
  If 0: add the path (repo-relative from src/, e.g. halo/rasterizer/xbox/foo.c)
  to the source list in src/CMakeLists.txt, in alphabetical position.

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
  rtk git commit -F "$MSG" && rm -f "$MSG"
Then, if this function had a parked record from an earlier attempt, mark it
promoted so the improve pass won't re-pick it (ignore errors if none exists):
  rtk python3 tools/lift/park.py promote --name ${JSON.stringify(name)} --commit "$(git rev-parse --short HEAD)" 2>/dev/null || true
Return the short commit hash.`

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
const parkToolPrompt = (name, addr, obj, srcFile, score, attemptME, reason, capHyp, notes) =>
  `${AGENT_RULES}

Preserve the sub-bar lift of ${name} (${addr}, ${score}% VC71) for a later improve
pass, then clean the tree. Run exactly this one command:
rtk python3 tools/lift/park.py park --name ${JSON.stringify(name)} --addr ${JSON.stringify(addr || '')} --obj ${JSON.stringify(obj || '')} --source ${JSON.stringify(srcFile || '')} --score ${score} --model ${JSON.stringify(attemptME.model)} --effort ${JSON.stringify(attemptME.effort)} --reason ${JSON.stringify(reason || '')}${capHyp ? ' --cap-hypothesis ' + JSON.stringify(capHyp) : ''}${notes ? ' --notes ' + JSON.stringify(String(notes).slice(0, 2000)) : ''} --revert-tree
park.py saves the git diff to artifacts/parked/, records the attempt (with
history), and reverts src/ kb.json tools/kb_reg_baseline.json to HEAD. Return the
tool's "parked ..." stdout line.`

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
// non-empty retrieval instead of racing a cold start 6 ways.
const warmRetrievalPrompt = () =>
  `${AGENT_RULES}

Ensure the retrieval query server is up (best-effort — research still works if not).
Run exactly:
  if [ -S /tmp/retrieval_server.sock ]; then echo "already-up"; else
    PY=python3; [ -x .venv/bin/python3 ] && PY=.venv/bin/python3;
    nohup "$PY" tools/retrieval/server.py > /tmp/retrieval_server.log 2>&1 &
    for i in $(seq 1 16); do sleep 5; [ -S /tmp/retrieval_server.sock ] && break; done;
  fi
  [ -S /tmp/retrieval_server.sock ] && echo "up" || echo "cold"
Return exactly one word: "up" or "cold".`

// ── Helpers ───────────────────────────────────────────────────────────────────

// goal90 bands: see docs/lift-policy.md §goal90-pass-fail-bands
function classifyBand(score) {
  if (score >= 90) return 'pass'
  if (score >= 85) return 'pass_permute'
  if (score >= 65) return 'fail_check_cap'
  return 'fail_revert'
}

async function maybePermute(name, phaseTitle) {
  const p = await agent(permutePrompt(name), { label: `permute:${name}`, phase: phaseTitle, ...M.mechanical, schema: SCORE_SCHEMA })
  return p ? p.vc71_score : null
}

const equivNote = (confidence, reason) =>
  `+equiv_${confidence || 'unknown'} [equivalence detail: ${String(reason || '').slice(0, 500)} — a 0-divergence pass on the live-state infection_swarm snapshot (populated datum tables, real actor handles) is accepted runtime behavioral evidence for the sub-90% band per the state-snapshot equivalence lane in CLAUDE.md]`

// Phase 3 — the fail-closed review gate. Every commit in this workflow goes
// through here; nothing is committed on VC71 match alone. `preEquiv` is the
// lift agent's own in-context equivalence result (step 6d) — when it passed,
// the FIRST review already carries the runtime evidence, so the
// NEEDS_RUNTIME → equiv agent → re-review round-trip is skipped.
async function reviewThenCommit(brief, score, srcFile, path, phaseTitle, preEquiv) {
  const havePreEquiv = !!(preEquiv && preEquiv.equiv_passes)
  if (havePreEquiv) path = `${path}${equivNote(preEquiv.equiv_confidence, preEquiv.equiv_reason)}`
  let review = await agent(reviewPrompt(brief, score, srcFile, path), {
    label: `review:${brief.name}`, phase: phaseTitle,
    // Model stays M.reason's; effort is the --reviewEffort A/B lever (see above).
    agentType: 'xbox-halo-lift-reviewer', model: M.reason.model, effort: REVIEW_EFFORT, schema: REVIEW_SCHEMA,
  })
  if (!review) return { committed: false, verdict: 'infra_blocked', rationale: 'review_agent_returned_null' }

  // When the lift agent's own equivalence (step 6d) was already in the review
  // path, the reviewer adjudicated WITH runtime evidence — respect its verdict
  // (a NEEDS_RUNTIME then means the evidence itself was judged insufficient,
  // e.g. early-exit-only coverage). Only the no-preEquiv case produces the
  // evidence now and applies the mechanical rule.
  if (review.verdict === 'NEEDS_RUNTIME' && !havePreEquiv) {
    const eq = await agent(equivalencePrompt(brief.name), { label: `equiv-for-review:${brief.name}`, phase: phaseTitle, ...M.mechanical, schema: EQUIV_SCHEMA })
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
    return { committed: false, verdict: review ? review.verdict : 'infra_blocked', rationale: review ? review.rationale : 'review_agent_returned_null' }
  }

  if (DRY_RUN) {
    return { committed: false, verdict: 'AUTO_ACCEPT', rationale: 'dry-run: would have committed', dryRun: true }
  }

  await agent(commitPrompt(brief.name, srcFile, path), { label: `commit:${brief.name}`, phase: phaseTitle, ...M.commit })
  return { committed: true, verdict: 'AUTO_ACCEPT', rationale: path }
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
    const g = await agent(mechGatePrompt(brief, srcFile), {
      label: `gate:${brief.name}`, phase: phaseTitle, ...M.mechanical, schema: MECH_GATE_SCHEMA,
    })
    const clean   = g && g.hazards_clean && g.abi_clean
    const mechPass = clean && (score >= 95 || (!g.risky_calls && !g.warns))
    if (mechPass) {
      if (DRY_RUN) return { committed: false, verdict: 'AUTO_ACCEPT', rationale: `dry-run: mechanical gate (${score}% clean)`, dryRun: true }
      await agent(commitPrompt(brief.name, srcFile, `mechanical:${score}% ${path}`), { label: `commit:${brief.name}`, phase: phaseTitle, ...M.commit })
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
async function parkBuilt(brief, srcFile, score, attemptME, reason, capHyp, phaseTitle, notes) {
  await agent(parkToolPrompt(brief.name, brief.addr, brief.obj, srcFile, score, attemptME, reason, capHyp, notes),
    { label: `park:${brief.name}`, phase: phaseTitle || 'Lift', ...M.mechanical })
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

  // Sync the shared parked ledger before draining it (see Select phase for why).
  await agent(
    `Run these two commands and return the last summary line of EACH (two lines total):
rtk python3 tools/lift/park.py reconcile --apply 2>&1 || true
rtk python3 tools/lift/park.py migrate --apply 2>&1 || true`,
    { label: 'ledger-sync', phase: 'Improve', ...M.mechanical })

  // Warm retrieval so the improve re-research decompiles get worked-example neighbors.
  await agent(warmRetrievalPrompt(), { label: 'retrieval-warm', phase: 'Improve', ...M.mechanical })

  const improved = []
  const seen = new Set()
  let promoted = 0
  let noProgress = 0
  let istop = 'ledger_drained'

  while (promoted < GOAL) {
    if (budget.total && budget.remaining() < 80000) { istop = 'budget_low'; break }
    if (noProgress >= STOP_ON_FAIL) { istop = 'no_progress'; break }

    const nx = await agent(nextPrompt(XM), { label: 'improve-next', phase: 'Improve', ...M.mechanical, schema: NEXT_SCHEMA })
    if (!nx || !nx.found || !nx.name) { istop = 'ledger_drained'; break }
    if (seen.has(nx.name)) { istop = 'ledger_not_advancing'; break }  // cycle guard
    seen.add(nx.name)

    const rec = { name: nx.name, addr: nx.addr || '', obj: nx.obj || '', source_path: nx.source_path || '', best_score: nx.best_score || 0 }
    log(`[improve ${promoted}/${GOAL}] ${rec.name} (${rec.addr}) parked at ${rec.best_score}% — tried by: ${nx.tried_models || '?'}`)

    // 1. Fresh Ghidra context (the improve model starts cold).
    const brief = await agent(researchPrompt(rec), { label: `research:${rec.name}`, phase: 'Improve', ...M.extract, schema: BRIEF_SCHEMA })
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
    const ap = await agent(applyPrompt(rec.name), { label: `apply:${rec.name}`, phase: 'Improve', ...M.mechanical, schema: APPLY_SCHEMA })
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
    if (warm && classifyBand(rec.best_score) === 'fail_check_cap') {
      const mo = await agent(matchOptimizerPrompt(rec.name, rec.addr, liftBrief.obj, liftBrief.source_path, rec.best_score, liftBrief.neighbors), {
        label: `improve-optimize:${rec.name}`, phase: 'Improve', agentType: 'vc71-match-optimizer', ...M.improve, schema: MATCH_OPTIMIZER_SCHEMA,
      })
      if (mo && typeof mo.vc71_score === 'number' && mo.vc71_score > rec.best_score) {
        a = { status: 'needs_verify', vc71_score: mo.vc71_score, source_file: liftBrief.source_path, reason: mo.reason || '' }
        log(`  ${rec.name} improve-optimize: ${rec.best_score}% → ${mo.vc71_score}% (skipping full re-lift)`)
      } else {
        log(`  ${rec.name} improve-optimize made no improvement over ${rec.best_score}% — falling back to full re-lift (${IMPROVE_MODEL})`)
      }
    }
    if (!a) {
      a = await agent(liftPrompt(liftBrief, true, rec.best_score, warm, priorNotes), {
        label: `improve-lift:${rec.name}`, phase: 'Improve', agentType: 'xbox-halo-re-analyst', ...M.improve, schema: LIFT_RESULT_SCHEMA,
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
      if (ps !== null) { score = ps; band = classifyBand(score) }
    }

    if (band === 'pass' || band === 'pass_permute') {
      const outcome = await gateThenCommit(liftBrief, score, srcFile, `improve:${warm ? 'warm' : 'cold'}`, 'Improve')
      if (outcome.committed) {
        promoted++; noProgress = 0
        improved.push({ ...rec, status: 'promoted', vc71_score: score, reason: outcome.rationale })
        log(`✓ promoted ${rec.name} ${score}% (was ${rec.best_score}%)`); continue
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
  const proms = improved.filter(r => r.status === 'promoted')
  log(`\n── Improve pass complete (${istop}) ─────────────────`)
  log(`Promoted:   ${proms.length}${DRY_RUN ? ` (dry-run; ${improved.filter(r => r.status === 'would_promote').length} would-promote)` : ''}${proms.length ? ' — ' + proms.map(p => `${p.name} ${p.vc71_score}%`).join(', ') : ''}`)
  log(`Re-parked:  ${improved.filter(r => r.status === 're_parked').length}`)
  log(`Already landed: ${improved.filter(r => r.status === 'already_landed').length}`)
  if (budget.total) log(`Budget remaining: ~${Math.round(budget.remaining() / 1000)}k tokens`)

  await agent(
    `Append an improve-pass summary to artifacts/auto_lift/goal_progress.md (create if missing).

## Improve pass — ${proms.length} promoted (${istop}), model=${XM}

| function | addr | was% | now% | action | reason |
|---|---|---|---|---|---|
${improved.map(r => `| ${r.name} | ${r.addr || '-'} | ${r.best_score ?? '-'} | ${r.vc71_score ?? '-'} | ${r.status} | ${r.reason || ''} |`).join('\n')}`,
    { label: 'improve-log', phase: 'Report', ...M.mechanical })

  return {
    mode: 'improve',
    improve_model: XM,
    stop_reason: istop,
    promoted: proms.length,
    would_promote: improved.filter(r => r.status === 'would_promote').length,
    re_parked: improved.filter(r => r.status === 're_parked').length,
    already_landed: improved.filter(r => r.status === 'already_landed').length,
    results: improved,
  }
}

// ── Phase 1: Select ───────────────────────────────────────────────────────────

phase('Select')
log(`Goal: lift ${GOAL} functions at >=90% VC71${DRY_RUN ? ' (dry run — no commits)' : ''}`)
if (OBJECTS) log(`Object filter (hard): ${OBJECTS.join(', ')}`)
if (CRITERIA) log(`Extra criteria (soft): ${CRITERIA}`)

// Sync the shared parked ledger before selecting: reconcile drops records for
// functions that landed via another path since they were parked, and migrate
// upgrades any pre-shared-root legacy records. Neither is ever invoked
// automatically otherwise, so the ledger silently accumulates stale entries.
await agent(
  `Run these two commands and return the last summary line of EACH (two lines total):
rtk python3 tools/lift/park.py reconcile --apply 2>&1 || true
rtk python3 tools/lift/park.py migrate --apply 2>&1 || true`,
  { label: 'ledger-sync', phase: 'Select', ...M.mechanical })

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
  ? OBJECTS.map(o => `rtk python3 tools/llm_auto_lift.py -q select --object ${o} --min-score 0 --limit ${RETURN_CAP} --json 2>&1`)
  : [`rtk python3 tools/llm_auto_lift.py -q select --limit ${BATCH_LIMIT} --json 2>&1`]

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
    : `Filter: keep lane=="auto-lift" (also allow "cache-context");`}
skip hs_runtime.obj (C99/VC71 violations unfixed) and xbox_crt.obj (NT-import/CRT wrappers).
Do NOT drop prior_fail entries yourself — return them with the flag set and let
the code-side pre-screen decide, so it can keep them when the queue would
otherwise run dry.
${OBJECTS
    ? `HARD RESTRICTION: only return candidates whose obj is one of: ${OBJECTS.join(', ')}. Discard everything else (this is also enforced in code afterward, so don't waste entries on other objects).`
    : `Prefer, in order: game_engine.obj, lruv_cache.obj, hud.obj, items.obj, input_xbox.obj —
sort those to the front, then the rest by score descending.`}
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
  selection = await agent(selectPrompt, {
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
log(`Selected ${targets.length} candidates across ${new Set(targets.map(t => t.obj)).size} objects`)

// ── Code-side pre-screen — drop targets the SELECTOR already proved unsuitable,
// using authoritative facts (has_reg_args / lane / addr) rather than re-deriving
// them in 6 Opus research agents that drift. Saves the research tokens entirely.
const CRT_LO = 0x1d0000, CRT_HI = 0x1de000
const codeSkips = []
// Prior-failure de-duplication. A parked target stays at the head of the
// selector's ranking forever (the score does not know it failed), so without
// this every session re-researches the same already-parked functions and
// commits nothing. Enforced in code, not in the select prompt, so it cannot
// drift -- but only while enough fresh candidates remain, because a park is
// often recoverable and we must not starve the queue.
const PRIOR_FAIL_KEEP_FLOOR = 10
// Attempts (from the park.py ledger) after which a walled target stops being
// served to a cold lift. 2 = "tried twice, same wall both times".
const PARKED_ATTEMPT_CAP = 2
// ...and only when its best score is below this. Calibrated on outcomes, not
// intuition: FUN_00173b40 landed at 90.3% on attempt 7 with a parked best of
// 88.0, so a 90 floor would have suppressed a real success. See the pre-screen.
const PARKED_WALL_PCT = 85
const freshCount = targets.filter(t => t.prior_fail !== true).length
const dropPriorFails = freshCount >= PRIOR_FAIL_KEEP_FLOOR
targets = targets.filter(t => {
  const a = parseInt((t.addr || '0').replace(/^0x/i, ''), 16)
  const pinnedAddr = ADDRS && ADDRS.has(a)
  if (t.prior_fail === true && dropPriorFails && !pinnedAddr) { codeSkips.push({ ...t, status: 'skipped', reason: `skip_prior_fail (parked before, ${freshCount} fresh candidates available)` }); return false }
  // Repeat-resistant target: several attempts already made and the best of
  // them is still FAR from the 90% bar -- a real wall, not a near miss.
  // parse_string has 7 attempts at best 50.7%; FUN_000f5660 has 4 at 74.4%.
  // The right lane for those is the IMPROVE pass, which warm-starts from the
  // parked best patch and varies the model, not another cold lift.
  //
  // The floor is 85, NOT 90, on direct evidence: FUN_00173b40 sat at best 88.0%
  // after SIX attempts (81.8 -> 84.3 -> 84.5 -> 88.0 -> 77.7 -> 84.7) and looked
  // exactly like a hopeless target -- then attempt 7 landed it at 90.3%. A
  // <90 floor would have blocked that. Above ~85 the next attempt can still
  // cross the bar, so only a target that has repeatedly failed to get CLOSE is
  // worth dropping. Pinned addrs always bypass -- an explicit --addrs is an
  // operator override.
  // Confirmed structural cap: never re-serve to a cold lift, even in the
  // 85-89 near-miss band. shader_environment_texture_animation_evaluate sat
  // at 86.2% x10 because the 85 wall treated the fucompp-assert cap as
  // recoverable. Improve-pass cannot move these either.
  if (!pinnedAddr && !IMPROVE &&
      (t.parked_status === 'capped_confirmed' || t.parked_status === 'confirmed_cap')) {
    codeSkips.push({ ...t, status: 'skipped', reason: `skip_confirmed_cap (best ${t.parked_best_score}%)` })
    return false
  }
  if (!pinnedAddr && !IMPROVE && t.parked_status === 'parked' &&
      (t.parked_attempts || 0) >= PARKED_ATTEMPT_CAP &&
      Number.isFinite(t.parked_best_score) && t.parked_best_score < PARKED_WALL_PCT) {
    codeSkips.push({ ...t, status: 'skipped', reason: `skip_parked_repeat (${t.parked_attempts} attempts, best ${t.parked_best_score}% < 90 — use the improve pass)` })
    return false
  }
  if (t.has_reg_args === true && !LIFT_REG_ARGS) { codeSkips.push({ ...t, status: 'skipped', reason: 'skip_reg_args (selector: @reg-defined prologue → sub-bar)' }); return false }
  if (Number.isFinite(a) && a >= CRT_LO && a < CRT_HI) { codeSkips.push({ ...t, status: 'skipped', reason: 'skip_nt_import (CRT/SEH region 0x1d0000-0x1de000)' }); return false }
  // Pinned targets bypass the lane gate: an explicit --addrs entry is a
  // deliberate operator choice, often of a manual-lift/cache-context lane fn.
  const pinned = ADDRS && ADDRS.has(a)
  if (!pinned && t.lane && t.lane !== 'auto-lift' && t.lane !== 'cache-context') { codeSkips.push({ ...t, status: 'skipped', reason: `lane=${t.lane} (not auto-liftable)` }); return false }
  return true
})
if (codeSkips.length) log(`Code pre-screen dropped ${codeSkips.length} before research (${codeSkips.filter(s => s.reason.startsWith('skip_prior_fail')).length} prior-fail, ${codeSkips.filter(s => s.reason.startsWith('skip_parked_repeat')).length} parked-repeat, ${codeSkips.filter(s => s.reason.startsWith('skip_confirmed_cap')).length} confirmed-cap, ${codeSkips.filter(s => s.reason.startsWith('skip_reg_args')).length} reg-args, ${codeSkips.filter(s => s.reason.startsWith('skip_nt_import')).length} CRT/SEH, ${codeSkips.filter(s => s.reason.startsWith('lane=')).length} lane)`)
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

phase('Research')

// Single global Ghidra preflight — short-circuit the whole run if MCP is down,
// instead of discovering it in 6 parallel research agents (each of which used to
// run check_ghidra_mcp.py redundantly).
const preflight = await agent(
  `Run: rtk python3 tools/audit/check_ghidra_mcp.py 2>&1
Return ok=true if it passes, else ok=false with the first error line as reason.`,
  { label: 'ghidra-preflight', phase: 'Research', ...M.mechanical,
    schema: { type: 'object', properties: { ok: { type: 'boolean' }, reason: { type: 'string' } }, required: ['ok'] } })
if (preflight && preflight.ok === false) {
  log(`Ghidra MCP preflight FAILED (${preflight.reason || 'unavailable'}) — aborting before research`)
  return { committed: 0, goal: GOAL, reached_goal: false, skipped: codeSkips.length, reverted: 0,
           infra_blocked: targets.length, reason: 'ghidra_unavailable' }
}

// ── P2 liftability gate — drop switch-case fragments (non-callable → porting one
// crashes on the parent's mid-flight stack) and confirmed structural caps BEFORE
// any research/lift agent spawns. Runs post-preflight because fragment detection
// needs live Ghidra xref shape. Fail-open: no classification → proceed unchanged.
if (targets.length) {
  const lc = await agent(liftabilityGatePrompt(targets), {
    label: 'liftability-gate', phase: 'Research', ...M.mechanical, schema: LIFTABILITY_SCHEMA,
  })
  if (lc && Array.isArray(lc.classified) && lc.classified.length) {
    const cls = new Map(lc.classified.map(r => [(r.addr || '').toLowerCase(), r]))
    const kept = []
    for (const t of targets) {
      const r = cls.get((t.addr || '').toLowerCase())
      if (r && (r.liftable_class === 'fragment' || r.liftable_class === 'known_cap')) {
        codeSkips.push({ ...t, status: 'skipped', reason: `skip_${r.liftable_class} (${r.class_reason || ''})` })
      } else {
        kept.push(t)
      }
    }
    const dropped = targets.length - kept.length
    if (dropped) log(`Liftability gate dropped ${dropped} (${codeSkips.filter(s => s.reason.startsWith('skip_fragment')).length} fragment, ${codeSkips.filter(s => s.reason.startsWith('skip_known_cap')).length} known_cap) before research`)
    targets = kept
  } else {
    log('Liftability gate: no classification returned — proceeding without it (fail-open)')
  }
  if (targets.length === 0) {
    log('No viable targets after liftability gate')
    return { committed: 0, goal: GOAL, reached_goal: false, skipped: codeSkips.length, reverted: 0, reason: 'empty_queue_after_liftability' }
  }
}

// Warm the retrieval server before any research decompiles so the auto-firing
// decompile_hook serves worked-example neighbors warm (not a cold 6-way race).
await agent(warmRetrievalPrompt(), { label: 'retrieval-warm', phase: 'Research', ...M.mechanical })

const RESEARCH_BATCH = 6

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
async function researchMore(want) {
  const fresh = []
  while (fresh.length < want && nextTarget < targets.length) {
    const take  = Math.min(RESEARCH_BATCH, Math.max(1, want - fresh.length))
    const batch = targets.slice(nextTarget, nextTarget + take)
    nextTarget += batch.length
    log(`Research: ${batch.length} target(s) [queue ${nextTarget}/${targets.length}]`)
    const bb = await parallel(batch.map(t => () => agent(researchPrompt(t), {
      label: `research:${t.name}`, phase: 'Research', ...M.extract, schema: BRIEF_SCHEMA,
    })))
    for (const b of bb.filter(Boolean)) {
      briefs.push(b)
      if (b.pre_screen === 'ok') {
        okBriefs.push(b); fresh.push(b)
      } else if (b.pre_screen === 'infra_blocked') {
        pendingInfra++
        results.push({ addr: b.addr, name: b.name, obj: b.obj, status: 'infra_blocked', reason: b.skip_reason || 'ghidra_unavailable' })
      } else {
        results.push({ addr: b.addr, name: b.name, obj: b.obj, status: 'skipped', reason: b.skip_reason || b.pre_screen })
      }
    }
  }
  if (fresh.length) await delinkPrefetch(fresh)
  return fresh
}

for (const s of codeSkips) {
  results.push({ addr: s.addr, name: s.name, obj: s.obj, status: 'skipped', reason: s.reason })
}

await researchMore(GOAL + RESEARCH_LOOKAHEAD)
log(`Research window: ${okBriefs.length} viable of ${briefs.length} briefed (${targets.length - nextTarget} target(s) held back)`)

// ── Phase 3: Serial lift loop, gated to reach GOAL or exhaust the queue ──────

phase('Lift')

let consecutiveFails = 0
let consecutiveInfra = pendingInfra
let stopReason = 'queue_exhausted'

let liftIdx = 0
let escalationsThisRun = 0   // targets that entered the escalation ladder (bounded by MAX_ESCALATIONS)
while (true) {
  const committed = results.filter(r => r.status === 'committed')
  if (committed.length >= GOAL) { stopReason = 'goal_reached'; break }

  if (consecutiveInfra >= 2) { stopReason = 'infra_blocked_twice'; break }
  if (consecutiveFails >= STOP_ON_FAIL) { stopReason = 'stop_on_fail_reached'; break }

  if (budget.total && budget.remaining() < 80000) { stopReason = 'budget_low'; break }

  // Ran out of briefed targets — top up rather than stopping, unless the
  // selector's queue is genuinely spent.
  if (liftIdx >= okBriefs.length) {
    pendingInfra = 0
    const added = await researchMore(GOAL - committed.length + RESEARCH_LOOKAHEAD)
    consecutiveInfra += pendingInfra
    if (!added.length) { stopReason = 'queue_exhausted'; break }
    continue
  }

  const brief = okBriefs[liftIdx++]

  log(`[${committed.length}/${GOAL} committed] next: ${brief.name} (${brief.addr})`)

  // ── Attempt 1: Opus lift (sonnet stall-loops under the workflow watchdog) ─
  const a1 = await agent(liftPrompt(brief, false, null), {
    label: `lift1:${brief.name}`, phase: 'Lift', agentType: 'xbox-halo-re-analyst', ...M.reason, schema: LIFT_RESULT_SCHEMA,
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
  const verifySkipped =
    a1.status === 'needs_verify' &&
    (a1.vc71_measured === false || (score === 0 && a1.vc71_measured !== true))
  if (verifySkipped) {
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
  const treatAsCapped = a1.capped === true
  const capProvenance = a1.cap_confidence === 'high' ? 'deterministic(classify_cap.py)' : 'agent-judgment'

  // ── Atlas-rule short-circuit (docs/plans/agent-model-routing-2026-08.md §5,
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
      const al = await agent(atlasLeverPrompt(brief.name, brief.addr, brief.obj, srcFile, score), {
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
    await parkBuilt(brief, srcFile, score, M.reason, 'pre_escalation', a1.cap_reason || '', 'Lift', a1.reason || '')

    // Effort ladder (Opus, NOT Fable): start at the cheap rung and step up to
    // xhigh/max ONLY for a target still below the 85% pass bar, not documented-
    // capped, and while budget remains. Most targets stop at the first rung.
    // The optimizer edits srcFile in place, so each rung builds on the last.
    let capReason  = ''
    let rungCapped = false
    for (let ri = 0; ri < IMPROVE_EFFORTS.length; ri++) {
      const eff = IMPROVE_EFFORTS[ri]
      const ME  = { model: IMPROVE_MODEL, effort: eff }
      log(`  ${brief.name} ${score}% — vc71-match-optimizer ${IMPROVE_MODEL}-${eff} [rung ${ri + 1}/${IMPROVE_EFFORTS.length}] (not a structural cap: ${a1.cap_confidence || 'n/a'})`)
      const mo = await agent(matchOptimizerPrompt(brief.name, brief.addr, brief.obj, srcFile, score, brief.neighbors), {
        label: `match-optimize:${brief.name}:${eff}`, phase: 'Lift', agentType: 'vc71-match-optimizer', ...ME, schema: MATCH_OPTIMIZER_SCHEMA,
      })
      if (mo && typeof mo.vc71_score === 'number') {
        // Never let a lower/garbled number regress the ledger's best.
        score  = Math.max(score, mo.vc71_score)
        band   = classifyBand(score)
        path   = 'escalated+optimize'
        lastME = ME
        lift   = { ...lift, reason: mo.reason || lift.reason }
        if (mo.capped === true) { rungCapped = true; capReason = mo.cap_reason || 'unclassified'; break }
      } else {
        log(`  ${brief.name} ${score}% — match-optimizer (${eff}) returned no usable score`)
      }
      if (band !== 'fail_check_cap') break                               // crossed the pass bar — done climbing
      if (budget.total && budget.remaining() < ESCALATION_BUDGET_FLOOR) { // out of budget mid-ladder
        log(`  ${brief.name} ${score}% — budget floor reached, stopping ladder at ${eff}`)
        break
      }
    }
    if (rungCapped) {
      // Optimizer hit a documented ceiling (its own classify_cap equivalent) —
      // treat exactly like the attempt-1 structural-cap path.
      log(`  ${brief.name} ${score}% capped [agent-judgment:optimizer]: ${capReason} — parked, no further escalation`)
      await parkBuilt(brief, srcFile, score, lastME, 'structural_cap', capReason, 'Lift', lift.reason || '')
      consecutiveFails++
      results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, reason: `structural_cap[agent-judgment:optimizer]: ${capReason}` })
      continue
    }
  } else if (band === 'fail_check_cap' && treatAsCapped) {
    // Structural cap — a future model may still beat it, so PARK (with the cap
    // hypothesis) rather than discard. Not confirm-cap: that would end retries.
    log(`  ${brief.name} ${score}% capped [${capProvenance}]: ${a1.cap_reason || 'unclassified'} — parked, no escalation`)
    await parkBuilt(brief, srcFile, score, M.reason, 'structural_cap', a1.cap_reason || 'unclassified', 'Lift', a1.reason || '')
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, reason: `structural_cap[${capProvenance}]: ${a1.cap_reason || 'unclassified'}` })
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
  } else if (score >= 85) {
    // Near-miss: lift is structurally sound, only runtime evidence blocked it.
    // Park (recoverable ledger) and do NOT count toward the consecutive-fail
    // stop — this is a deferred work item, not a failed lift.
    await parkBuilt(brief, srcFile, score, lastME, `${outcome.verdict}: ${outcome.rationale}`, '', 'Lift', lift.reason || '')
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, source_file: srcFile, reason: `${outcome.verdict}: ${outcome.rationale}` })
    log(`◐ ${brief.name} ${score}% parked (review gate: ${outcome.verdict}; patch in artifacts/parked/)`)
  } else {
    // Below 85 and review-blocked: still preserve the work (a different model
    // may push it over later) rather than checkout-discarding it.
    await parkBuilt(brief, srcFile, score, lastME, `${outcome.verdict}: ${outcome.rationale}`, '', 'Lift', lift.reason || '')
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, source_file: srcFile, reason: `review<85: ${outcome.verdict}: ${outcome.rationale}` })
    log(`◐ ${brief.name} ${score}% parked (review gate <85: ${outcome.verdict})`)
  }
}

// ── Phase 4: Report ───────────────────────────────────────────────────────────

phase('Report')

const committed      = results.filter(r => r.status === 'committed')
const wouldCommit    = results.filter(r => r.status === 'would_commit')
const skipped        = results.filter(r => r.status === 'skipped')
const revertedVerify = results.filter(r => r.status === 'reverted_verify')
const revertedReview = results.filter(r => r.status === 'reverted_review')
const parked         = results.filter(r => r.status === 'parked')
const infra          = results.filter(r => r.status === 'infra_blocked')

log(`\n── Run complete (${stopReason}) ─────────────────────`)
log(`Committed:            ${committed.length}${DRY_RUN ? ` (dry-run: ${wouldCommit.length} would-commit)` : ''}`)
log(`Skipped (pre-screen): ${skipped.length}`)
log(`Reverted (verify):    ${revertedVerify.length}`)
log(`Reverted (review gate): ${revertedReview.length}`)
log(`Parked (>=85%, needs runtime evidence): ${parked.length}${parked.length ? ' — ' + parked.map(p => `${p.name} ${p.vc71_score}%`).join(', ') : ''}`)
log(`Infra-blocked:         ${infra.length}`)
if (budget.total) log(`Budget remaining: ~${Math.round(budget.remaining() / 1000)}k tokens`)

await agent(
  `Append a run summary to artifacts/auto_lift/goal_progress.md (create if missing).

## Goal-lift run — ${committed.length}/${GOAL} committed (${stopReason})

| function | addr | obj | vc71 | action | reason |
|---|---|---|---|---|---|
${results.map(r => `| ${r.name} | ${r.addr} | ${r.obj || '-'} | ${r.vc71_score ?? '-'} | ${r.status} | ${r.reason || ''} |`).join('\n')}`,
  { label: 'progress-log', phase: 'Report', ...M.mechanical }
)

return {
  goal: GOAL,
  reached_goal: committed.length >= GOAL,
  stop_reason: stopReason,
  committed: committed.length,
  would_commit: wouldCommit.length,
  skipped: skipped.length,
  reverted_verify: revertedVerify.length,
  reverted_review: revertedReview.length,
  parked: parked.length,
  infra_blocked: infra.length,
  results,
}
