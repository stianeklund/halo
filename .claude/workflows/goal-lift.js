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
// - The escalation / improve pass uses a DIFFERENT model for perspective
//   diversity: Fable-high, falling back to Opus-xhigh when Fable is unavailable.
const IMPROVE_MODEL = (args && args.improveModel) || 'fable'
const M = {
  mechanical: { model: 'haiku', effort: 'low'  },  // tool-run + parse
  extract:    { model: 'opus',  effort: 'low'  },  // select, research (schema-shaped)
  commit:     { model: 'opus',  effort: 'low'  },  // runs the clean-build gate
  reason:     { model: 'opus',  effort: 'high' },  // lift, review
  improve:    { model: IMPROVE_MODEL, effort: IMPROVE_MODEL === 'opus' ? 'xhigh' : 'high' },
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
          delinked:      { type: 'boolean' },  // target.score_details.delinked_ref > 0
          source_exists: { type: 'boolean' },  // target.score_details.source_exists present
          lane:          { type: 'string' },   // item.lane
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
    // false when vc71_verify never produced a per-function % (no delinked
    // reference / "No usable objdiff.json unit" / verify skipped). A skipped
    // verify is an infrastructure gap, NOT a 0% lift — the loop must not park
    // it as below_65pct or count it toward stop_on_fail (see f8e29209/daa39ee6:
    // 9 faithful lifts parked at "0%" across two runs).
    vc71_measured: { type: 'boolean' },
    capped:         { type: 'boolean' },  // matches a known structural-cap signature (see liftPrompt)
    cap_reason:     { type: 'string' },
    cap_confidence: { type: 'string' },    // high (deterministic classify_cap.py) | inconclusive (agent judgment)
    // In-agent follow-up stages (liftPrompt steps 6b-6d): the lift agent does
    // redelink, permute, and state-snapshot equivalence itself, in the same
    // context that produced the lift. The loop's separate redelink/permute/
    // equiv agents only run as FALLBACK when these fields are absent.
    redelinked:       { type: 'boolean' },
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
    found:        { type: 'boolean' },
    name:         { type: 'string' },
    addr:         { type: 'string' },
    obj:          { type: 'string' },
    source_path:  { type: 'string' },
    best_score:   { type: 'number' },
    attempts:     { type: 'number' },
    tried_models: { type: 'string' },
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
   check_ghidra_mcp.py. If any Ghidra MCP call below errors/times out →
   pre_screen="infra_blocked", skip_reason="ghidra_unavailable".

2. KB LOOKUP: rtk jq '[.. | objects | select(.addr? == "${t.addr}")] | .[0]' kb.json

2b. SOURCE CHECK (before Ghidra, to avoid wasted decompile tokens):
    addr_no0x=$(printf '%08x' $((16#${t.addr.replace('0x', '')})))
    rtk rg "^[a-zA-Z_][a-zA-Z0-9_*]+ FUN_\${addr_no0x}\\b" src/ --no-heading -l 2>/dev/null
    Also check the real name if the kb entry has one:
    rtk rg "^[a-zA-Z_][a-zA-Z0-9_*]+ ${t.name}\\b" src/ --no-heading -l 2>/dev/null
    If either grep returns a .c file path, the function is already implemented:
    → pre_screen="skip_already_in_source", skip_reason="already implemented: <file>"
    Return immediately — no Ghidra call needed.

${CACHE_CONTEXT ? `3a. ENRICH (run BEFORE decompile so the enrichment hook can inject callee/struct tables):
    timeout 90 rtk python3 tools/llm_auto_lift.py cache-context --target ${t.addr} --force 2>&1 || echo "[cache-context-skip]"

` : ''}3. DECOMPILE: Ghidra MCP decompile_function at ${t.addr}
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
   - Decompile has unaff_, in_EAX, in_ECX               → "skip_reg_args"
   - Any callee has @<reg> and is NOT in kb.json          → "skip_reg_args"
   - Body is 1-3 lines wrapping one FUN_ unchanged        → "skip_trivial"
   - Contains __SEH_prolog / __SEH_epilog                 → "skip_seh"
   - Calls xboxkrnl NT/kernel imports (Nt*/Ob*/Ke*/Rtl*/Ex*/Ps*/Io*/Hal*)
     OR addr in 0x1d0000-0x1de000 CRT region              → "skip_nt_import"
   Otherwise pre_screen="ok"

5. CALLEES: Ghidra MCP get_function_callees at ${t.addr}.
   Return JSON array: [{addr,name,has_reg_args,in_kb}]

6. DELINKED CHECK: objdump -t delinked/*.obj 2>/dev/null | grep -i "${t.name.replace('FUN_', '')}"
   delinked_exists=true if found.

7. HAZARD SCAN: rtk python3 tools/audit/check_lift_hazards.py 2>&1 | grep -A2 "${t.addr.replace('0x', '')}"

8. DISASM NOTES (only if FPU ops, struct access, or >2 CALLs): Ghidra MCP disassemble_function.
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
- fucompp vs fcomps / int16 movswl / fcos/fsin spill: permanent ~15pp gap, not a bug`

const liftPrompt = (brief, isEscalation, priorScore) =>
  `${AGENT_RULES}

Lift ${brief.name} at ${brief.addr} from Halo CE Xbox (cachebeta.xbe).
Object: ${brief.obj} | Source: ${brief.source_path}
${isEscalation ? `\nESCALATION (prior score ${priorScore}%): focus on FPU operand order, buffer-alias confusion, PUSH trace per CALL, struct field rotation.\n` : ''}
CONTEXT — do NOT re-call Ghidra:
  KB:       ${brief.kb_entry}
  Decomp:   ${brief.decompiled}
  Disasm:   ${brief.disasm_notes || 'none'}
  Callees:  ${brief.callees}
  Hazards:  ${brief.hazards}

WORKED EXAMPLES (similar functions already ported, with their VC71 %; match their
idioms — casts, x87 order, struct-store shape — and expect a comparable score.
A near-identical neighbor capped below 90% is evidence THIS one is capped too):
${brief.neighbors || '  (none — retrieval server was cold)'}

STEPS:
1. DELINKED — a prefetch stage already exported delinked/functions/<addr_no_0x>.obj.
   Do NOT export it yourself and make NO ghidra-live calls here. If the ref is
   missing, VC71 simply scores low and the workflow's cheap redelink stage exports
   it and re-verifies afterward. In-lifter export is a multi-turn ghidra-live dance
   (decompile → find end → export → copy) whose accumulated output is re-read on
   every later turn of this agent — skipping it keeps this agent short and is the
   single biggest per-lift token saving.

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
   rtk python3 tools/audit/check_lift_hazards.py
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
   If verify was SKIPPED (no delinked reference, "No usable objdiff.json unit",
   "no delinked reference existed"), report vc71_measured=false and do NOT invent
   vc71_score=0 as if it were a real match result — first attempt the redelink
   in 6b to create the missing reference.

6b. REDELINK RETRY (if the build passed and vc71_score < 90 OR verify was
   skipped for want of a reference): a stale, missing, or whole-object delink
   boundary often causes a falsely low/absent score. You already
   know the function's exact range from the decompile — re-export a fresh
   per-function reference via mcp__ghidra-live__export_delinked_object with
   selection_mode="range", range="<start_no_0x>-<end_no_0x>" (last instruction
   of THIS function). The export MUST land at
   delinked/functions/<8-hex-lowercase-addr>.obj (zero-padded, e.g.
   000c0f50.obj — vc71_verify discovers per-function refs by that name; copy
   it into this worktree's delinked/functions/ if you exported elsewhere),
   then re-run the step-6 lift_pipeline command; if the pipeline still reports
   no unit, fall back to: rtk python3 tools/verify/vc71_verify.py <source_file>
   -f ${brief.name} --no-cache (per-function refs are auto-discovered).
   Keep the better score and
   report redelinked=true (and vc71_measured=true once a real % exists).

6c. PERMUTE (only if the score is now in [85,89] — skip otherwise):
   timeout 150 rtk python3 tools/permuter/run.py -q --target ${brief.name} --attempts 100 2>&1 || echo "[permuter stopped]"
   then re-run the step-6 lift_pipeline command. Never accept a permutation
   that lowers the score. Report permuted=true.

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
   parked-ledger confirmed/prior cap):
     rtk python3 tools/analysis/classify_cap.py --name ${brief.name} --addr ${brief.addr} \\
       --score <vc71_score> --decl '<this function's kb declaration>'
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

const redelinkPrompt = (name, addr) =>
  `${AGENT_RULES}

Re-export a fresh per-function delinked reference for ${name} and re-run VC71 verify.
This is the primary fix for "badly delinked object" false-low scores.

1. Find the function end address in Ghidra: decompile_function at ${addr}, note the last instruction address.
2. Export via mcp__ghidra-live__export_delinked_object:
   selection_mode="range", range="<start_no_0x>-<end_no_0x>" (exact function body range).
   The file MUST be named delinked/functions/<8-hex-lowercase-addr>.obj (zero-padded,
   e.g. 000c0f50.obj) — vc71_verify discovers per-function references by that exact
   name. Copy it into a worktree's delinked/functions/ if you are operating inside one.
3. Re-run (wrap — see [STALL]):
   timeout 165 rtk python3 tools/lift_pipeline.py --target ${name} --no-metadata-update --verify-policy goal90 2>&1 || echo "[timed-out]"

BOUNDED PASS — this is a mechanical export+verify, NOT a re-lift. Do the three
steps ONCE each: one decompile to find the end address, one export, one verify.
Do NOT re-lift, edit source, try alternate ranges, or iterate — if the fresh
reference does not raise the score, return improved=false with the score you got.
Do NOT paste the objdiff/build log into your reasoning (it inflates every
following turn's re-read). At most ~10 turns.

Return: vc71_score, improved (bool), reason.`

const permutePrompt = (name) =>
  `${AGENT_RULES}

Run the decomp-permuter for ${name}, then re-verify (both wrapped — see [STALL]):
timeout 150 rtk python3 tools/permuter/run.py -q --target ${name} --attempts 100 2>&1 || echo "[permuter stopped at timeout]"
timeout 165 rtk python3 tools/lift_pipeline.py --target ${name} --no-metadata-update --verify-policy goal90 2>&1 || echo "[timed-out]"

BOUNDED PASS — at most 2 permuter invocations total. If a permutation breaks the
build (e.g. -Werror dead variable), fix it minimally and re-verify once. If the
score is still <90% after that, STOP and return the best verified score — do not
keep iterating; the review gate decides acceptance. Never accept a permutation
that lowers the pre-permute score.
Return: vc71_score (after permutation), improved (bool), reason.`

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
- Hazard scan: rtk python3 tools/audit/check_lift_hazards.py --changed-only
- Caller/callee/disassembly context around each CALL touched by this lift (Ghidra MCP)
- Relevant kb.json declarations and register args for ${brief.name}

Apply your decision policy and return your verdict.`

// Cheap mechanical gate — runs the same hazard/ABI checks the commit stage will
// enforce, so a clean high-% lift can skip the Opus-high reviewer entirely.
// Report booleans ONLY; do not edit or fix anything.
const mechGatePrompt = (brief, srcFile) =>
  `Mechanical pre-commit gate for ${brief.name} (${brief.addr}). Report booleans ONLY —
do NOT edit, fix, build, or commit anything.

1. HAZARDS: rtk python3 tools/audit/check_lift_hazards.py --changed-only 2>&1
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

Then commit:
  rtk git add -- ${sourceFile} kb.json tools/kb_reg_baseline.json
  rtk python3 tools/audit/generate_lift_commit.py --batch-name "${name}" > /tmp/commit_msg.txt
  rtk git commit -F /tmp/commit_msg.txt
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
const parkToolPrompt = (name, addr, obj, srcFile, score, attemptME, reason, capHyp) =>
  `${AGENT_RULES}

Preserve the sub-bar lift of ${name} (${addr}, ${score}% VC71) for a later improve
pass, then clean the tree. Run exactly this one command:
rtk python3 tools/lift/park.py park --name ${JSON.stringify(name)} --addr ${JSON.stringify(addr || '')} --obj ${JSON.stringify(obj || '')} --source ${JSON.stringify(srcFile || '')} --score ${score} --model ${JSON.stringify(attemptME.model)} --effort ${JSON.stringify(attemptME.effort)} --reason ${JSON.stringify(reason || '')}${capHyp ? ' --cap-hypothesis ' + JSON.stringify(capHyp) : ''} --revert-tree
park.py saves the git diff to artifacts/parked/, records the attempt (with
history), and reverts src/ kb.json tools/kb_reg_baseline.json to HEAD. Return the
tool's "parked ..." stdout line.`

// Improve pass — pick the closest-to-bar parked function the improve model has
// NOT already attempted (so repeated improve passes drain the ledger instead of
// re-trying the same model on the same function).
const nextPrompt = (excludeModel) =>
  `Pick the next parked function for the improve pass. Run exactly:
rtk python3 tools/lift/park.py next --exclude-model ${JSON.stringify(excludeModel)}
It prints JSON {"found":bool,"record":{...}}.
- If found=false → return found=false.
- Else return found=true with the record's name, addr, obj, source_path, best_score,
  attempts (the length of the attempts array), and tried_models (comma-joined
  attempts[].model values).`

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
    agentType: 'xbox-halo-lift-reviewer', ...M.reason, schema: REVIEW_SCHEMA,
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
    if (eq && eq.passes && score >= 85 && (eq.confidence === 'high' || eq.confidence === 'moderate')) {
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
// attemptME = {model,effort} of the lift attempt being preserved.
async function parkBuilt(brief, srcFile, score, attemptME, reason, capHyp, phaseTitle) {
  await agent(parkToolPrompt(brief.name, brief.addr, brief.obj, srcFile, score, attemptME, reason, capHyp),
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

    // 3. Re-lift with the improve model (escalation framing, prior score to beat).
    const liftBrief = { ...brief, obj: brief.obj || rec.obj, source_path: brief.source_path || rec.source_path }
    const a = await agent(liftPrompt(liftBrief, true, rec.best_score), {
      label: `improve-lift:${rec.name}`, phase: 'Improve', agentType: 'xbox-halo-re-analyst', ...M.improve, schema: LIFT_RESULT_SCHEMA,
    })
    if (!a || a.status === 'infra_blocked') { istop = 'infra_blocked'; improved.push({ ...rec, status: 'infra_blocked', reason: 'agent_null' }); break }
    if (a.status !== 'needs_verify') {
      // build_failed / skipped: re-park records the improve-model attempt (so it
      // won't be re-picked) and reverts, preserving the prior best patch.
      await parkBuilt(liftBrief, a.source_file || liftBrief.source_path, a.vc71_score || 0, M.improve, `improve_${a.status}`, a.cap_reason || '', 'Improve')
      noProgress++; improved.push({ ...rec, status: 're_parked', reason: `improve ${a.status}` }); continue
    }

    let score   = a.vc71_score || 0
    let srcFile = a.source_file || liftBrief.source_path
    let band    = classifyBand(score)
    log(`  improve-lift ${rec.name}: ${score}% (band=${band}, was ${rec.best_score}%, ${warm ? 'warm' : 'cold'}-start)`)

    if (band !== 'pass') {
      const rd = await agent(redelinkPrompt(rec.name, rec.addr), { label: `redelink:${rec.name}`, phase: 'Improve', ...M.mechanical, schema: SCORE_SCHEMA })
      if (rd && rd.vc71_score > score) { score = rd.vc71_score; band = classifyBand(score); log(`  ${rec.name} redelink → ${score}%`) }
    }
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

    await parkBuilt(liftBrief, srcFile, score, M.improve, `improve_pass_${band}`, a.cap_reason || '', 'Improve')
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

const selection = await agent(
  `Select next batch of Halo CE Xbox functions to lift.
${OBJECTS
    ? `Run EACH of these commands (one per allowlisted object) and concatenate all their JSON arrays into one combined list before parsing:\n${selectCmds.join('\n')}`
    : `Run: ${selectCmds[0]}`}
(-q is a GLOBAL flag before the subcommand — it makes the JSON compact.)
This emits a JSON array; each element has: total_score, lane, and target{addr, name,
object_name, has_reg_args, source_path, score_details{...}}. For each element parse:
  addr=target.addr, name=target.name, obj=target.object_name, score=total_score,
  has_reg_args=target.has_reg_args (boolean, verbatim),
  delinked=(target.score_details.delinked_ref is present and > 0),
  source_exists=(target.score_details.source_exists is present),
  lane=lane.
Do NOT invent these booleans — copy them from the JSON. (The code-side pre-screen
depends on has_reg_args/lane being exact.)

Filter: keep lane=="auto-lift" (also allow "cache-context"); skip [skip:prior_fail]
markers unless fewer than 10 non-prior-fail entries remain;
skip hs_runtime.obj (C99/VC71 violations unfixed) and xbox_crt.obj (NT-import/CRT wrappers).
${OBJECTS
    ? `HARD RESTRICTION: only return candidates whose obj is one of: ${OBJECTS.join(', ')}. Discard everything else (this is also enforced in code afterward, so don't waste entries on other objects).`
    : `Prefer, in order: game_engine.obj, lruv_cache.obj, hud.obj, items.obj, input_xbox.obj —
sort those to the front, then the rest by score descending.`}
${CRITERIA ? `\nADDITIONAL USER CRITERIA (apply on top of the rules above): ${CRITERIA}\n` : ''}
Return up to ${RETURN_CAP} entries, each with the parsed fields above.`,
  { label: 'select', phase: 'Select', ...M.extract, schema: TARGETS_SCHEMA }
)

if (!selection || !selection.targets || selection.targets.length === 0) {
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
log(`Selected ${targets.length} candidates across ${new Set(targets.map(t => t.obj)).size} objects`)

// ── Code-side pre-screen — drop targets the SELECTOR already proved unsuitable,
// using authoritative facts (has_reg_args / lane / addr) rather than re-deriving
// them in 6 Opus research agents that drift. Saves the research tokens entirely.
const CRT_LO = 0x1d0000, CRT_HI = 0x1de000
const codeSkips = []
targets = targets.filter(t => {
  const a = parseInt((t.addr || '0').replace(/^0x/i, ''), 16)
  if (t.has_reg_args === true) { codeSkips.push({ ...t, status: 'skipped', reason: 'skip_reg_args (selector: @reg-defined prologue → sub-bar)' }); return false }
  if (Number.isFinite(a) && a >= CRT_LO && a < CRT_HI) { codeSkips.push({ ...t, status: 'skipped', reason: 'skip_nt_import (CRT/SEH region 0x1d0000-0x1de000)' }); return false }
  if (t.lane && t.lane !== 'auto-lift' && t.lane !== 'cache-context') { codeSkips.push({ ...t, status: 'skipped', reason: `lane=${t.lane} (not auto-liftable)` }); return false }
  return true
})
if (codeSkips.length) log(`Code pre-screen dropped ${codeSkips.length} before research (${codeSkips.filter(s => s.reason.startsWith('skip_reg_args')).length} reg-args, ${codeSkips.filter(s => s.reason.startsWith('skip_nt_import')).length} CRT/SEH, ${codeSkips.filter(s => s.reason.startsWith('lane=')).length} lane)`)
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
const briefs = []
for (let i = 0; i < targets.length; i += RESEARCH_BATCH) {
  const batch = targets.slice(i, i + RESEARCH_BATCH)
  log(`Research batch ${Math.floor(i / RESEARCH_BATCH) + 1}/${Math.ceil(targets.length / RESEARCH_BATCH)}`)
  const bb = await parallel(batch.map(t => () => agent(researchPrompt(t), {
    label: `research:${t.name}`, phase: 'Research', ...M.extract, schema: BRIEF_SCHEMA,
  })))
  briefs.push(...bb.filter(Boolean))
}

const okBriefs      = briefs.filter(b => b.pre_screen === 'ok')
const skipBriefs     = briefs.filter(b => b.pre_screen !== 'ok' && b.pre_screen !== 'infra_blocked')
const infraBriefs    = briefs.filter(b => b.pre_screen === 'infra_blocked')
log(`Research done: ${okBriefs.length} viable, ${skipBriefs.length} pre-screened out, ${infraBriefs.length} infra-blocked`)

// ── Phase 2b: Delink prefetch — export all missing per-function references
// upfront in one agent, so each lift starts with a trustworthy VC71 baseline
// and the per-candidate redelink stage rarely fires.
const needDelink = okBriefs.filter(b => !b.delinked_exists)
if (needDelink.length) {
  log(`Delink prefetch: exporting ${needDelink.length} missing references`)
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

// ── Phase 3: Serial lift loop, gated to reach GOAL or exhaust the queue ──────

phase('Lift')

const results = []
for (const s of codeSkips) {
  results.push({ addr: s.addr, name: s.name, obj: s.obj, status: 'skipped', reason: s.reason })
}
for (const brief of skipBriefs) {
  results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'skipped', reason: brief.skip_reason || brief.pre_screen })
}
for (const brief of infraBriefs) {
  results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'infra_blocked', reason: brief.skip_reason || 'ghidra_unavailable' })
}

let consecutiveFails = 0
let consecutiveInfra = infraBriefs.length
let stopReason = 'queue_exhausted'

for (const brief of okBriefs) {
  const committed = results.filter(r => r.status === 'committed')
  if (committed.length >= GOAL) { stopReason = 'goal_reached'; break }

  if (consecutiveInfra >= 2) { stopReason = 'infra_blocked_twice'; break }
  if (consecutiveFails >= STOP_ON_FAIL) { stopReason = 'stop_on_fail_reached'; break }

  if (budget.total && budget.remaining() < 80000) { stopReason = 'budget_low'; break }

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

  let lift    = a1   // the active lift result — carries in-agent redelink/permute/equiv flags
  let score   = a1.vc71_score || 0
  let srcFile = a1.source_file || brief.source_path
  let band    = classifyBand(score)
  let path    = 'pass1' + (a1.redelinked ? '+redelink' : '') + (a1.permuted ? '+permute' : '')
  let lastME  = M.reason   // {model,effort} of the current attempt (for parked records)
  log(`  lift1 ${brief.name}: ${a1.status} ${score}% (band=${band}${a1.capped ? ', capped: ' + (a1.cap_reason || '?') : ''})`)

  // ── VERIFY-SKIPPED GUARD: a build-passing lift whose VC71 was never measured
  // (no delinked reference / no objdiff unit) is an INFRASTRUCTURE gap, not a
  // 0% match. Two runs (f8e29209, daa39ee6) parked 9 faithful lifts as
  // "below_65pct @ 0%" this way, and the bogus fails tripped stop_on_fail.
  // Repair path: one redelink agent (exports the per-function reference and
  // re-scores). If a real % emerges, fall through to normal banding; if not,
  // park as verify_skipped_no_ref WITHOUT counting a consecutive failure.
  const verifySkipped =
    a1.status === 'needs_verify' &&
    (a1.vc71_measured === false || (score === 0 && a1.vc71_measured !== true))
  if (verifySkipped) {
    log(`  ${brief.name}: VC71 never measured (missing reference) — attempting redelink repair`)
    const rd = await agent(redelinkPrompt(brief.name, brief.addr), { label: `redelink:${brief.name}`, phase: 'Lift', ...M.mechanical, schema: SCORE_SCHEMA })
    if (rd && rd.vc71_score > 0) {
      score = rd.vc71_score
      band  = classifyBand(score)
      path  = 'redelink'
      lift  = { ...lift, redelinked: true }
      log(`  ${brief.name} redelink repaired verify → ${score}%`)
    } else {
      await parkBuilt(brief, srcFile, 0, lastME, 'verify_skipped_no_ref', 'VC71 unmeasured: no delinked reference could be produced; not a lift failure')
      results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: null, reason: 'verify_skipped_no_ref (infrastructure — VC71 never measured; do not treat as below_65pct)' })
      continue
    }
  }

  // FALLBACK ONLY — the lift agent normally redelinks in-context (step 6b); this
  // runs only when it did not (lift.redelinked absent). A fresh per-function
  // delinked reference can recover VC71 that was falsely low from a stale or
  // whole-object delink boundary artifact. Two guards keep this fallback from
  // becoming a sink (wf_927b1d1d: redelink was 50% of the run's tokens, 0 gains):
  //   - boundary artifacts cost single-digit %, so a sub-65 (fail_revert) score is
  //     a real structural mismatch, not a delink artifact — re-delinking won't help;
  //   - if a1 already proved a high-confidence structural cap (classify_cap.py),
  //     a fresh delink cannot beat a codegen/register-allocation cap.
  const redelinkWorthwhile =
    !lift.redelinked &&
    (band === 'pass_permute' || band === 'fail_check_cap') &&
    !(a1.capped === true && a1.cap_confidence === 'high')
  if (redelinkWorthwhile) {
    const rd = await agent(redelinkPrompt(brief.name, brief.addr), { label: `redelink:${brief.name}`, phase: 'Lift', ...M.mechanical, schema: SCORE_SCHEMA })
    if (rd && rd.vc71_score > score) {
      score = rd.vc71_score
      band  = classifyBand(score)
      path  = 'redelink'
      log(`  ${brief.name} redelink improved to ${score}%`)
    }
  }

  // ── 65-84%: explicit structural-cap gate (P3). The lift agent ran
  // tools/analysis/classify_cap.py in step 7: a cap_confidence==="high" verdict is
  // an AUTHORITATIVE deterministic cap (@reg-defining prologue / ledger-confirmed /
  // prior-cap-no-improvement) — provably futile to retry, so it MUST park and never
  // escalate. An "inconclusive" verdict falls back to the agent's own CAP_TABLE
  // judgment. Either way "capped" now decides escalation, but its provenance is
  // explicit and logged, not an opaque model boolean.
  const treatAsCapped = a1.capped === true
  const capProvenance = a1.cap_confidence === 'high' ? 'deterministic(classify_cap.py)' : 'agent-judgment'
  if (band === 'fail_check_cap' && !treatAsCapped) {
    log(`  ${brief.name} ${score}% — not a structural cap (${a1.cap_confidence || 'n/a'}), escalating (${IMPROVE_MODEL})`)
    // Preserve the attempt-1 (Opus) work before escalating: park keeps the
    // best-scoring attempt's patch across both, and --revert-tree cleans the
    // tree for the escalation re-lift. Never discard a building lift.
    await parkBuilt(brief, srcFile, score, M.reason, 'pre_escalation', a1.cap_reason || '')
    const a2 = await agent(liftPrompt(brief, true, score), {
      label: `lift2:${brief.name}`, phase: 'Lift', agentType: 'xbox-halo-re-analyst', ...M.improve, schema: LIFT_RESULT_SCHEMA,
    })
    if (a2 && (a2.status === 'needs_verify')) {
      lift    = a2
      score   = a2.vc71_score || score
      srcFile = a2.source_file || srcFile
      band    = classifyBand(score)
      path    = 'escalated' + (a2.redelinked ? '+redelink' : '') + (a2.permuted ? '+permute' : '')
      lastME  = M.improve
    } else if (a2 && a2.status === 'build_failed') {
      consecutiveFails++
      results.push({ ...a2, obj: brief.obj })
      continue
    }
  } else if (band === 'fail_check_cap' && treatAsCapped) {
    // Structural cap — a future model may still beat it, so PARK (with the cap
    // hypothesis) rather than discard. Not confirm-cap: that would end retries.
    log(`  ${brief.name} ${score}% capped [${capProvenance}]: ${a1.cap_reason || 'unclassified'} — parked, no escalation`)
    await parkBuilt(brief, srcFile, score, M.reason, 'structural_cap', a1.cap_reason || 'unclassified')
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, reason: `structural_cap[${capProvenance}]: ${a1.cap_reason || 'unclassified'}` })
    continue
  }

  if (band === 'fail_revert') {
    await parkBuilt(brief, srcFile, score, lastME, 'below_65pct', '')
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, reason: `below_65pct` })
    continue
  }
  if (band === 'fail_check_cap') {
    // escalation ran and is still in [65,84) — park the best attempt for later.
    await parkBuilt(brief, srcFile, score, lastME, 'escalation_exhausted', '')
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
    await parkBuilt(brief, srcFile, score, lastME, `${outcome.verdict}: ${outcome.rationale}`, '')
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'parked', vc71_score: score, source_file: srcFile, reason: `${outcome.verdict}: ${outcome.rationale}` })
    log(`◐ ${brief.name} ${score}% parked (review gate: ${outcome.verdict}; patch in artifacts/parked/)`)
  } else {
    // Below 85 and review-blocked: still preserve the work (a different model
    // may push it over later) rather than checkout-discarding it.
    await parkBuilt(brief, srcFile, score, lastME, `${outcome.verdict}: ${outcome.rationale}`, '')
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
