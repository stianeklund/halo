export const meta = {
  name: 'goal-lift',
  description: 'Goal-mode auto-lift: lift, verify, review-gate, and commit Halo CE Xbox functions until N are committed at >=90% VC71 or the queue is exhausted',
  phases: [
    { title: 'Select',   detail: 'Frontier scoring + liftability filtering' },
    { title: 'Research', detail: 'Parallel Ghidra context gathering (read-only)' },
    { title: 'Lift',     detail: 'Serial: lift -> verify (goal90 bands) -> permute/escalate -> review gate -> commit' },
    { title: 'Report',   detail: 'Summary: committed, skipped, reverted, review-gate holds' },
  ],
}

// This workflow exists so that the two required subagents are GUARANTEED to
// run: a prose-only slash command can't force tool calls, but agent() here is
// real code. xbox-halo-re-analyst does Phase-1 RE + implementation;
// xbox-halo-lift-reviewer is the fail-closed gate every commit must clear.

const GOAL         = (args && args.goal) || 20
const STOP_ON_FAIL = (args && args.stopOnFail) || 3
const DRY_RUN      = !!(args && args.dryRun)

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

const TARGETS_SCHEMA = {
  type: 'object',
  properties: {
    targets: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          addr:  { type: 'string' },
          name:  { type: 'string' },
          obj:   { type: 'string' },
          score: { type: 'number' },
        },
        required: ['addr', 'name', 'obj'],
      },
    },
  },
  required: ['targets'],
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
    capped:      { type: 'boolean' },  // matches a known structural-cap signature (see liftPrompt)
    cap_reason:  { type: 'string' },
    reason:      { type: 'string' },
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
`

const researchPrompt = (t) =>
  `Gather Ghidra context for ${t.name} at ${t.addr} (${t.obj}). READ-ONLY — no edits.

1. PREFLIGHT: rtk python3 tools/audit/check_ghidra_mcp.py
   If it fails → pre_screen="infra_blocked", skip_reason="ghidra_unavailable"

2. KB LOOKUP: rtk jq '[.. | objects | select(.addr? == "${t.addr}")] | .[0]' kb.json

2b. SOURCE CHECK (before Ghidra, to avoid wasted decompile tokens):
    addr_no0x=$(printf '%08x' $((16#${t.addr.replace('0x', '')})))
    rtk rg "^[a-zA-Z_][a-zA-Z0-9_*]+ FUN_\${addr_no0x}\\b" src/ --no-heading -l 2>/dev/null
    Also check the real name if the kb entry has one:
    rtk rg "^[a-zA-Z_][a-zA-Z0-9_*]+ ${t.name}\\b" src/ --no-heading -l 2>/dev/null
    If either grep returns a .c file path, the function is already implemented:
    → pre_screen="skip_already_in_source", skip_reason="already implemented: <file>"
    Return immediately — no Ghidra call needed.

3. DECOMPILE: Ghidra MCP decompile_function at ${t.addr}

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
(create-target guess if the function has no source yet).`

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

STEPS:
1. DELINKED — if delinked_exists=false: export via mcp__ghidra-live__export_delinked_object
   for ${brief.addr}. Copy into a worktree's delinked/ + delinked/functions/<addr_no_0x>.obj
   if you are operating inside one. If export fails → status="skipped", reason="delinked_export_failed"

2. CALLEE PREP — for any callee with has_reg_args=true and in_kb=false:
   add to kb.json with @<reg> + update tools/kb_reg_baseline.json.

3. IMPLEMENT — C89 in ${brief.source_path} at address-ordered position.
   Rules: C89 only, no inline ASM, preserve control flow + side-effect order.
   MSVC intrinsics → C: _ftol2→(int)cast, _chkstk→normal locals, _allmul→(int64_t)a*b.
   Trace every CALL: first PUSH is last arg. Check FPU subtraction direction and cross-product order.

4. UPDATE kb.json — set ported=true; add @<reg> callees with binary evidence only.
   Update tools/kb_reg_baseline.json for any new @<reg>.

5. MAINTAIN + HAZARDS:
   rtk python3 tools/analysis/maintain.py ${brief.source_path}
   rtk python3 tools/audit/check_lift_hazards.py
   Fix any HIGH-RISK hazards.

6. BUILD + VC71 (wrap to avoid the 180s stall timer — see [STALL]):
   timeout 165 rtk python3 tools/lift_pipeline.py --target ${brief.name} --no-metadata-update --verify-policy goal90 2>&1 || echo "[lift_pipeline timed-out]"
   Parse VC71 score and build pass/fail. If it timed out, status="needs_review", vc71_score=0.

7. SELF-ASSESS STRUCTURAL CAP (only if vc71_score is in [65,84]):
${CAP_TABLE}
   Set capped=true + cap_reason if the objdiff output matches one of these
   patterns. Otherwise capped=false.

8. RETURN (do NOT commit, do NOT run the review gate — that happens later):
   status: "needs_verify" if build passed (regardless of score), else "build_failed".
   Always report vc71_score, source_file (the actual path written), capped, cap_reason.`

const redelinkPrompt = (name, addr) =>
  `${AGENT_RULES}

Re-export a fresh per-function delinked reference for ${name} and re-run VC71 verify.
This is the primary fix for "badly delinked object" false-low scores.

1. Find the function end address in Ghidra: decompile_function at ${addr}, note the last instruction address.
2. Export via mcp__ghidra-live__export_delinked_object:
   selection_mode="range", range="<start_no_0x>-<end_no_0x>" (exact function body range).
   Copy the exported .obj into a worktree's delinked/ if you are operating inside one.
3. Re-run (wrap — see [STALL]):
   timeout 165 rtk python3 tools/lift_pipeline.py --target ${name} --no-metadata-update --verify-policy goal90 2>&1 || echo "[timed-out]"

Return: vc71_score, improved (bool), reason.`

const permutePrompt = (name) =>
  `${AGENT_RULES}

Run the decomp-permuter for ${name}, then re-verify (both wrapped — see [STALL]):
timeout 150 rtk python3 tools/permuter/run.py -q --target ${name} --attempts 100 2>&1 || echo "[permuter stopped at timeout]"
timeout 165 rtk python3 tools/lift_pipeline.py --target ${name} --no-metadata-update --verify-policy goal90 2>&1 || echo "[timed-out]"
Return: vc71_score (after permutation), improved (bool), reason.`

const equivalencePrompt = (name) =>
  `${AGENT_RULES}

Run behavioral equivalence for ${name} (wrapped — see [STALL]):
timeout 150 rtk python3 tools/equivalence/unicorn_diff.py ${name} --seeds 100 --allow-stubs --float-tolerance 32 2>&1 || echo "[equivalence timed-out]"
Passes if the result is 100% equivalent, or confidence="high" with no divergences.
If it timed out with no verdict, passes=false, reason="equiv_timeout".
Return: passes (bool), confidence, coverage (%), reason.`

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
Return the short commit hash.`

const revertPrompt = (name) =>
  `${AGENT_RULES}

Revert changes for ${name}:
rtk git checkout -- src/ kb.json tools/kb_reg_baseline.json
rtk git status --short`

// ── Helpers ───────────────────────────────────────────────────────────────────

// goal90 bands: see docs/lift-policy.md §goal90-pass-fail-bands
function classifyBand(score) {
  if (score >= 90) return 'pass'
  if (score >= 85) return 'pass_permute'
  if (score >= 65) return 'fail_check_cap'
  return 'fail_revert'
}

async function maybePermute(name, phaseTitle) {
  const p = await agent(permutePrompt(name), { label: `permute:${name}`, phase: phaseTitle, schema: SCORE_SCHEMA })
  return p ? p.vc71_score : null
}

// Phase 3 — the fail-closed review gate. Every commit in this workflow goes
// through here; nothing is committed on VC71 match alone.
async function reviewThenCommit(brief, score, srcFile, path, phaseTitle) {
  let review = await agent(reviewPrompt(brief, score, srcFile, path), {
    label: `review:${brief.name}`, phase: phaseTitle,
    agentType: 'xbox-halo-lift-reviewer', schema: REVIEW_SCHEMA,
  })
  if (!review) return { committed: false, verdict: 'infra_blocked', rationale: 'review_agent_returned_null' }

  if (review.verdict === 'NEEDS_RUNTIME') {
    const eq = await agent(equivalencePrompt(brief.name), { label: `equiv-for-review:${brief.name}`, phase: phaseTitle, schema: EQUIV_SCHEMA })
    if (eq && eq.passes) {
      review = await agent(reviewPrompt(brief, score, srcFile, `${path}+equiv_${eq.confidence}`), {
        label: `review2:${brief.name}`, phase: phaseTitle,
        agentType: 'xbox-halo-lift-reviewer', schema: REVIEW_SCHEMA,
      }) || review
    }
  }

  if (!review || review.verdict !== 'AUTO_ACCEPT') {
    return { committed: false, verdict: review ? review.verdict : 'infra_blocked', rationale: review ? review.rationale : 'review_agent_returned_null' }
  }

  if (DRY_RUN) {
    return { committed: false, verdict: 'AUTO_ACCEPT', rationale: 'dry-run: would have committed', dryRun: true }
  }

  await agent(commitPrompt(brief.name, srcFile, path), { label: `commit:${brief.name}`, phase: phaseTitle })
  return { committed: true, verdict: 'AUTO_ACCEPT', rationale: path }
}

// ── Phase 1: Select ───────────────────────────────────────────────────────────

phase('Select')
log(`Goal: lift ${GOAL} functions at >=90% VC71${DRY_RUN ? ' (dry run — no commits)' : ''}`)
if (OBJECTS) log(`Object filter (hard): ${OBJECTS.join(', ')}`)
if (CRITERIA) log(`Extra criteria (soft): ${CRITERIA}`)

const BATCH_LIMIT = Math.min(60, Math.max(30, GOAL * 3))

const selection = await agent(
  `Select next batch of Halo CE Xbox functions to lift.
Run: rtk python3 tools/llm_auto_lift.py select --limit ${BATCH_LIMIT} 2>&1

Filter: keep "auto-lift" lane only; skip [skip:prior_fail] unless fewer than 10
non-prior-fail entries remain; skip Reasons containing unaff_/in_EAX/in_ECX/@esi/@eax;
skip hs_runtime.obj (C99/VC71 violations unfixed) and xbox_crt.obj (NT-import/CRT wrappers).
${OBJECTS
    ? `HARD RESTRICTION: only return candidates whose obj is one of: ${OBJECTS.join(', ')}. Discard everything else (this is also enforced in code afterward, so don't waste entries on other objects).`
    : `Prefer, in order: game_engine.obj, lruv_cache.obj, hud.obj, items.obj, input_xbox.obj —
sort those to the front, then the rest by score descending.`}
${CRITERIA ? `\nADDITIONAL USER CRITERIA (apply on top of the rules above): ${CRITERIA}\n` : ''}
Parse: addr, name, obj, score. Return up to ${BATCH_LIMIT} entries.`,
  { label: 'select', phase: 'Select', schema: TARGETS_SCHEMA }
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

// ── Phase 2: Parallel research (read-only, batched 6) ────────────────────────

phase('Research')

const RESEARCH_BATCH = 6
const briefs = []
for (let i = 0; i < targets.length; i += RESEARCH_BATCH) {
  const batch = targets.slice(i, i + RESEARCH_BATCH)
  log(`Research batch ${Math.floor(i / RESEARCH_BATCH) + 1}/${Math.ceil(targets.length / RESEARCH_BATCH)}`)
  const bb = await parallel(batch.map(t => () => agent(researchPrompt(t), {
    label: `research:${t.name}`, phase: 'Research', schema: BRIEF_SCHEMA,
  })))
  briefs.push(...bb.filter(Boolean))
}

const okBriefs      = briefs.filter(b => b.pre_screen === 'ok')
const skipBriefs     = briefs.filter(b => b.pre_screen !== 'ok' && b.pre_screen !== 'infra_blocked')
const infraBriefs    = briefs.filter(b => b.pre_screen === 'infra_blocked')
log(`Research done: ${okBriefs.length} viable, ${skipBriefs.length} pre-screened out, ${infraBriefs.length} infra-blocked`)

// ── Phase 3: Serial lift loop, gated to reach GOAL or exhaust the queue ──────

phase('Lift')

const results = []
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

  // ── Attempt 1: Sonnet lift ──────────────────────────────────────────────
  const a1 = await agent(liftPrompt(brief, false, null), {
    label: `lift1:${brief.name}`, phase: 'Lift', agentType: 'xbox-halo-re-analyst', model: 'sonnet', schema: LIFT_RESULT_SCHEMA,
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

  let score   = a1.vc71_score || 0
  let srcFile = a1.source_file || brief.source_path
  let band    = classifyBand(score)
  let path    = 'pass1'

  // Cheap fix before anything expensive: a fresh per-function delinked
  // reference often recovers VC71 that was falsely low from a stale or
  // whole-object delink boundary artifact.
  if (band !== 'pass') {
    const rd = await agent(redelinkPrompt(brief.name, brief.addr), { label: `redelink:${brief.name}`, phase: 'Lift', schema: SCORE_SCHEMA })
    if (rd && rd.vc71_score > score) {
      score = rd.vc71_score
      band  = classifyBand(score)
      path  = 'redelink'
      log(`  ${brief.name} redelink improved to ${score}%`)
    }
  }

  // ── 65-84%: check structural cap, one Opus escalation if not capped ────
  if (band === 'fail_check_cap' && !a1.capped) {
    log(`  ${brief.name} ${score}% — not a known cap, escalating to Opus`)
    await agent(revertPrompt(brief.name), { label: `revert-pre-escalate:${brief.name}`, phase: 'Lift' })
    const a2 = await agent(liftPrompt(brief, true, score), {
      label: `lift2:${brief.name}`, phase: 'Lift', agentType: 'xbox-halo-re-analyst', schema: LIFT_RESULT_SCHEMA,
    })
    if (a2 && (a2.status === 'needs_verify')) {
      score   = a2.vc71_score || score
      srcFile = a2.source_file || srcFile
      band    = classifyBand(score)
      path    = 'escalated'
    } else if (a2 && a2.status === 'build_failed') {
      consecutiveFails++
      results.push({ ...a2, obj: brief.obj })
      continue
    }
  } else if (band === 'fail_check_cap' && a1.capped) {
    await agent(revertPrompt(brief.name), { label: `revert:${brief.name}`, phase: 'Lift' })
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'reverted_verify', vc71_score: score, reason: `structural_cap: ${a1.cap_reason || 'unclassified'}` })
    continue
  }

  if (band === 'fail_revert') {
    await agent(revertPrompt(brief.name), { label: `revert:${brief.name}`, phase: 'Lift' })
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'reverted_verify', vc71_score: score, reason: `below_65pct` })
    continue
  }
  if (band === 'fail_check_cap') {
    // escalation ran and is still in [65,84) or dropped below 65
    await agent(revertPrompt(brief.name), { label: `revert:${brief.name}`, phase: 'Lift' })
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'reverted_verify', vc71_score: score, reason: 'escalation_exhausted' })
    continue
  }

  // ── 85-89%: one permuter pass before the review gate ───────────────────
  if (band === 'pass_permute') {
    log(`  ${brief.name} ${score}% — permuter pass`)
    const ps = await maybePermute(brief.name, 'Lift')
    if (ps !== null) score = ps
    path = path === 'escalated' ? 'escalated+permute' : 'permute'
  }

  // ── Phase 3: review gate, then commit ───────────────────────────────────
  const outcome = await reviewThenCommit(brief, score, srcFile, path, 'Lift')
  if (outcome.committed) {
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'committed', vc71_score: score, source_file: srcFile, reason: outcome.rationale })
    consecutiveFails = 0
    log(`✓ ${brief.name} ${score}% (${outcome.rationale})`)
  } else if (outcome.dryRun) {
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'would_commit', vc71_score: score, source_file: srcFile, reason: outcome.rationale })
    await agent(revertPrompt(brief.name), { label: `revert-dry-run:${brief.name}`, phase: 'Lift' })
    log(`○ ${brief.name} ${score}% (dry-run, would commit — reverted for clean state)`)
  } else {
    await agent(revertPrompt(brief.name), { label: `revert-post-review:${brief.name}`, phase: 'Lift' })
    consecutiveFails++
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'reverted_review', vc71_score: score, source_file: srcFile, reason: `${outcome.verdict}: ${outcome.rationale}` })
    log(`✗ ${brief.name} ${score}% (review gate: ${outcome.verdict})`)
  }
}

// ── Phase 4: Report ───────────────────────────────────────────────────────────

phase('Report')

const committed      = results.filter(r => r.status === 'committed')
const wouldCommit    = results.filter(r => r.status === 'would_commit')
const skipped        = results.filter(r => r.status === 'skipped')
const revertedVerify = results.filter(r => r.status === 'reverted_verify')
const revertedReview = results.filter(r => r.status === 'reverted_review')
const infra          = results.filter(r => r.status === 'infra_blocked')

log(`\n── Run complete (${stopReason}) ─────────────────────`)
log(`Committed:            ${committed.length}${DRY_RUN ? ` (dry-run: ${wouldCommit.length} would-commit)` : ''}`)
log(`Skipped (pre-screen): ${skipped.length}`)
log(`Reverted (verify):    ${revertedVerify.length}`)
log(`Reverted (review gate): ${revertedReview.length}`)
log(`Infra-blocked:         ${infra.length}`)
if (budget.total) log(`Budget remaining: ~${Math.round(budget.remaining() / 1000)}k tokens`)

await agent(
  `Append a run summary to artifacts/auto_lift/goal_progress.md (create if missing).

## Goal-lift run — ${committed.length}/${GOAL} committed (${stopReason})

| function | addr | obj | vc71 | action | reason |
|---|---|---|---|---|---|
${results.map(r => `| ${r.name} | ${r.addr} | ${r.obj || '-'} | ${r.vc71_score ?? '-'} | ${r.status} | ${r.reason || ''} |`).join('\n')}`,
  { label: 'progress-log', phase: 'Report' }
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
  infra_blocked: infra.length,
  results,
}
