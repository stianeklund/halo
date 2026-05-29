export const meta = {
  name: 'mass-lift',
  description: 'Autonomous mass decompilation: parallel research briefs, serial lift→verify→commit with re-delink, permuter, Opus escalation, and equivalence acceptance at [87,89%]',
  phases: [
    { title: 'Select',   detail: 'Frontier scoring + liftability filtering' },
    { title: 'Research', detail: 'Parallel Ghidra context gathering (6 concurrent)' },
    { title: 'Lift',     detail: 'Serial lift → re-delink → permute → re-lift → Opus → equiv → commit' },
    { title: 'Report',   detail: 'Summary: committed, skipped, circuit-breaker objects' },
  ],
}

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
    source_file:     { type: 'string' },
    kb_entry:        { type: 'string' },
    decompiled:      { type: 'string' },
    disasm_notes:    { type: 'string' },
    callees:         { type: 'string' },
    hazards:         { type: 'string' },
    delinked_exists: { type: 'boolean' },
    pre_screen:      { type: 'string' },   // ok | skip_reg_args | skip_trivial | skip_seh | skip_nt_import
    skip_reason:     { type: 'string' },
  },
  required: ['addr', 'name', 'pre_screen'],
}

const LIFT_RESULT_SCHEMA = {
  type: 'object',
  properties: {
    addr:        { type: 'string' },
    name:        { type: 'string' },
    status:      { type: 'string' },   // needs_commit | skipped | build_failed | infra_blocked
    source_file: { type: 'string' },
    vc71_score:  { type: 'number' },
    attempts:    { type: 'number' },
    reason:      { type: 'string' },
  },
  required: ['addr', 'name', 'status', 'attempts'],
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

// ── Prompt builders ───────────────────────────────────────────────────────────

// Shared safety preamble for every agent that edits files, runs git, or runs
// long commands. Prevents the two failure modes that killed prior runs:
// (1) agents escaping the worktree to operate on the user's main checkout, and
// (2) silent long commands (permuter/equivalence) tripping the 180s stall timer.
const AGENT_RULES =
  `OPERATING RULES (read first):
[WORKTREE] You run inside a git WORKTREE; your CWD is the worktree root. Do ALL
work here with RELATIVE paths. NEVER \`cd /mnt/g/dev/halo\` (the user's main
checkout) and NEVER run git mutations (stash/checkout/commit/reset) against any
repo but this worktree — the two share one .git, so touching main corrupts the
user's working state. Do NOT use \`git stash\`. mcp__ghidra-live__export_delinked_object
writes its .obj to the MAIN repo delinked/ (path like G:\\dev\\halo\\delinked\\...);
after exporting, COPY the file into THIS worktree instead of cd-ing to main:
  cp /mnt/g/dev/halo/delinked/<rel>.obj ./delinked/<rel>.obj
  cp /mnt/g/dev/halo/delinked/<rel>.obj ./delinked/functions/<addr_no_0x>.obj
then run every verify step here with relative paths.
[STALL] Any single shell command that can run >2 min (permuter, unicorn
equivalence, a full clean build) MUST be wrapped so it cannot run silently past
180s and trip the harness stall detector that kills the whole run:
  timeout 150 <cmd> 2>&1 || echo "[timed-out]"
`

const researchPrompt = (t) =>
  `Gather Ghidra context for ${t.name} at ${t.addr} (${t.obj}). READ-ONLY — no edits.

1. PREFLIGHT: rtk python3 tools/audit/check_ghidra_mcp.py
   If fails → pre_screen="skip_reg_args", skip_reason="ghidra_unavailable"

2. KB LOOKUP: rtk jq '[.. | objects | select(.addr? == "${t.addr}")] | .[0]' kb.json

2b. SOURCE CHECK (run before Ghidra to avoid wasted decompile tokens):
    addr_no0x=$(printf '%08x' $((16#${t.addr.replace('0x', '')})))
    rtk rg "^[a-zA-Z_][a-zA-Z0-9_*]+ FUN_\${addr_no0x}\\b" src/ --no-heading -l 2>/dev/null
    Also check the real name if kb entry has one:
    rtk rg "^[a-zA-Z_][a-zA-Z0-9_*]+ ${t.name}\\b" src/ --no-heading -l 2>/dev/null
    If either grep returns a .c file path (i.e., a definition exists in source):
    → pre_screen="skip_already_in_source"
    → skip_reason="already implemented: <file>"
    Return immediately — no Ghidra call needed.

3. DECOMPILE: Ghidra MCP decompile_function at ${t.addr}

4. PRE-SCREEN (return immediately with pre_screen=<reason> if any match):
   - Decompile has unaff_, in_EAX, in_ECX          → "skip_reg_args"
   - Any callee has @<reg> and is NOT in kb.json     → "skip_reg_args"
   - Body is 1–3 lines wrapping one FUN_ unchanged   → "skip_trivial"
   - Contains __SEH_prolog / __SEH_epilog             → "skip_seh"
   - Calls xboxkrnl NT/kernel imports (Nt*/Ob*/Ke*/Rtl*/Ex*/Ps*/Io*/Hal*)
     OR addr in 0x1d0000–0x1de000 CRT region          → "skip_nt_import"
     (equivalence can't emulate kernel imports; VC71 structurally capped
      ~45-65%, so neither acceptance gate is reachable — these stall the run)
   Otherwise pre_screen="ok"

5. CALLEES: Ghidra MCP get_function_callees at ${t.addr}
   Return JSON array: [{addr,name,has_reg_args,in_kb}]

6. DELINKED CHECK: objdump -t delinked/*.obj 2>/dev/null | grep -i "${t.name.replace('FUN_', '')}"
   delinked_exists=true if found.

7. HAZARD SCAN: rtk python3 tools/audit/check_lift_hazards.py 2>&1 | grep -A2 "${t.addr.replace('0x', '')}"

8. DISASM NOTES (only if FPU ops, struct access, or >2 CALLs): Ghidra MCP disassemble_function.
   Return key observations only: push order per CALL, FPU subtraction direction, buffer sizes. Max 400 words.

Return full brief.`

const liftPrompt = (brief, isEscalation, priorScore) =>
  `${AGENT_RULES}

Lift ${brief.name} at ${brief.addr} from Halo CE Xbox (cachebeta.xbe).
Object: ${brief.obj} | Source: ${brief.source_file}
${isEscalation ? `\nESCALATION (prior score ${priorScore}%): Focus on FPU operand order, buffer-alias confusion, PUSH trace per CALL, struct field rotation.\n` : ''}
CONTEXT — do NOT re-call Ghidra:
  KB:       ${brief.kb_entry}
  Decomp:   ${brief.decompiled}
  Disasm:   ${brief.disasm_notes || 'none'}
  Callees:  ${brief.callees}
  Hazards:  ${brief.hazards}

STEPS:
1. DELINKED — if delinked_exists=false: export via mcp__ghidra-live__export_delinked_object
   for ${brief.addr} (writes to the MAIN delinked/), then COPY the .obj into this
   worktree's delinked/ and delinked/functions/<addr_no_0x>.obj (see [WORKTREE]). Do
   not cd to main. If export fails → status="skipped", reason="delinked_export_failed"

2. CALLEE PREP — for any callee with has_reg_args=true and in_kb=false:
   Add to kb.json with @<reg> + update tools/kb_reg_baseline.json.

3. IMPLEMENT — C89 in ${brief.source_file} at address-ordered position.
   Rules: C89 only, no inline ASM, preserve control flow + side-effect order.
   MSVC intrinsics → C: _ftol2→(int)cast, _chkstk→normal locals, _allmul→(int64_t)a*b.
   Trace every CALL: first PUSH is last arg. Check FPU subtraction direction and cross-product order.

4. UPDATE kb.json — set ported=true; add @<reg> callees with binary evidence only.
   Update tools/kb_reg_baseline.json for any new @<reg>.

5. MAINTAIN + HAZARDS:
   rtk python3 tools/analysis/maintain.py ${brief.source_file}
   rtk python3 tools/audit/check_lift_hazards.py
   Fix any HIGH-RISK hazards.

6. BUILD + VC71 (wrap to avoid the 180s stall timer — see [STALL]):
   timeout 165 rtk python3 tools/lift_pipeline.py --target ${brief.name} --no-metadata-update --verify-policy auto 2>&1 || echo "[lift_pipeline timed-out]"
   Parse VC71 score and build pass/fail. If it timed out, status="needs_review".

7. RETURN (do NOT commit):
   status: "needs_commit" if vc71>=90, else "needs_review".
   If build failed: status="build_failed".`

const redelinkPrompt = (name, addr) =>
  `${AGENT_RULES}

Re-export a fresh per-function delinked reference for ${name} and re-run VC71 verify.
This is the primary fix for "badly delinked object" false-low scores.

1. Find the function end address in Ghidra: decompile_function at ${addr}, note the last instruction address.

2. Export via mcp__ghidra-live__export_delinked_object:
   selection_mode="range", range="<start_no_0x>-<end_no_0x>"
   (use the exact function body range, not the whole object range)
   Then COPY the exported .obj from /mnt/g/dev/halo/delinked/ into this worktree's
   delinked/ and delinked/functions/<addr_no_0x>.obj (see [WORKTREE]). Do not cd to main.

3. Re-run (wrap — see [STALL]):
   timeout 165 rtk python3 tools/lift_pipeline.py --target ${name} --no-metadata-update --verify-policy auto 2>&1 || echo "[timed-out]"
   Parse new VC71 score.

Return: vc71_score, improved (bool), reason.`

const permutePrompt = (name) =>
  `${AGENT_RULES}

Run the decomp-permuter for ${name}, then re-verify. Both commands are wrapped to
stay under the 180s stall threshold (see [STALL]) — the permuter writes its best
candidate as it goes, so a timeout just stops further attempts:
timeout 150 rtk python3 tools/permuter/run.py -q --target ${name} --attempts 100 2>&1 || echo "[permuter stopped at timeout]"
timeout 165 rtk python3 tools/lift_pipeline.py --target ${name} --no-metadata-update --verify-policy auto 2>&1 || echo "[timed-out]"
Return: vc71_score (after permutation), improved (bool), reason.`

const equivalencePrompt = (name) =>
  `${AGENT_RULES}

Run behavioral equivalence test for ${name} (wrapped — see [STALL]):
timeout 150 rtk python3 tools/equivalence/unicorn_diff.py ${name} --seeds 100 --allow-stubs --float-tolerance 32 2>&1 || echo "[equivalence timed-out]"
Passes if result is 100% equivalent or confidence="high" with no divergences.
If it timed out with no verdict, passes=false, reason="equiv_timeout".
Return: passes (bool), confidence, coverage (%), reason.`

const commitPrompt = (name, sourceFile, reason) =>
  `${AGENT_RULES}

Commit the lift of ${name}${reason ? ' (' + reason + ')' : ''}.

FIRST verify a CLEAN FULL build. The per-function lift build is incremental and
can miss cross-TU breakage: giving a callee a real prototype propagates its
signature onto tail-call thunks / callers in OTHER source files, which then fail
to compile. A full --target halo build forces those TUs to recompile against the
regenerated decl.h and surfaces the break before it ships a non-building HEAD.
  timeout 165 rtk python3 tools/build/build.py -q --target halo 2>&1 | tee /tmp/mlbuild.txt
  build.py -q prints ONLY warnings/errors, so a clean build shows just the
  hazard-scan summary line ("intrinsics: 0, ...") and no compiler errors.
  Build PASSED if /tmp/mlbuild.txt contains NO "error:" line and NO "Error 2"
  / "*** " gmake-failure marker. Build FAILED if any such marker appears.
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

Revert changes for ${name}. Run these in THIS worktree only (never cd to main):
rtk git checkout -- src/ kb.json tools/kb_reg_baseline.json
rtk git status --short`

// ── Helpers (called in lift loop) ─────────────────────────────────────────────

// Try permuter, then equivalence if [87,89%] and relift was attempted.
// Returns { score, action } where action is 'commit' | 'commit_equiv' | 'continue'
async function tryPermuterThenEquiv(name, currentScore, triedRelift, liftPhase) {
  const p = await agent(permutePrompt(name), { label: `permute:${name}`, phase: liftPhase, schema: SCORE_SCHEMA })
  const ps = p ? p.vc71_score : currentScore
  if (ps >= 90) return { score: ps, action: 'commit', reason: 'permuter' }
  if (ps >= 87 && triedRelift) {
    const eq = await agent(equivalencePrompt(name), { label: `equiv:${name}`, phase: liftPhase, schema: EQUIV_SCHEMA })
    if (eq && eq.passes) return { score: ps, action: 'commit_equiv', reason: `equiv_${eq.confidence}` }
  }
  return { score: ps, action: 'continue', reason: ps >= 87 ? 'equiv_failed_or_no_relift' : 'permuter_insufficient' }
}

// ── Phase 1: Select ───────────────────────────────────────────────────────────

phase('Select')

const selection = await agent(
  `Select next batch of Halo CE Xbox functions to lift.
Run: rtk python3 tools/llm_auto_lift.py select --limit 30 2>&1

Filter: keep "auto-lift" lane only; skip [skip:prior_fail] unless <10 non-prior-fail remain;
skip Reasons containing unaff_/in_EAX/in_ECX/@esi/@eax; skip hs_runtime.obj and xbox_crt.obj
(NT-import/CRT wrappers stall verification).
Parse: addr, name, obj, score. Return the top ≤15 entries sorted by score descending.`,
  { label: 'select', phase: 'Select', schema: TARGETS_SCHEMA }
)

if (!selection || !selection.targets || selection.targets.length === 0) {
  log('No viable targets in queue')
  return { committed: 0, skipped: 0, reverted: 0, reason: 'empty_queue' }
}
log(`Selected ${selection.targets.length} targets across ${new Set(selection.targets.map(t => t.obj)).size} objects`)

// ── Phase 2: Parallel research (batched 6 at a time) ─────────────────────────

phase('Research')

const BATCH = 6
const briefs = []
for (let i = 0; i < selection.targets.length; i += BATCH) {
  const batch = selection.targets.slice(i, i + BATCH)
  log(`Research batch ${Math.floor(i / BATCH) + 1}/${Math.ceil(selection.targets.length / BATCH)}`)
  const bb = await parallel(batch.map(t => () => agent(researchPrompt(t), {
    label: `research:${t.name}`, phase: 'Research', schema: BRIEF_SCHEMA,
  })))
  briefs.push(...bb.filter(Boolean))
}

const okBriefs   = briefs.filter(b => b.pre_screen === 'ok')
const skipBriefs = briefs.filter(b => b.pre_screen !== 'ok')
log(`Research done: ${okBriefs.length} viable, ${skipBriefs.length} pre-screened out`)

// ── Phase 3: Serial lift loop ─────────────────────────────────────────────────

phase('Lift')

const results = []
const objFailStreak = {}

for (const brief of skipBriefs) {
  results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'skipped', attempts: 0, reason: brief.skip_reason || brief.pre_screen })
}

for (const brief of okBriefs) {
  if (budget.total && budget.remaining() < 80000) {
    log(`Budget low (~${Math.round(budget.remaining() / 1000)}k) — stopping`)
    break
  }

  const streak = objFailStreak[brief.obj] || 0
  if (streak >= 2) {
    log(`Circuit breaker: skipping ${brief.name} (${brief.obj} streak=${streak})`)
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'skipped', attempts: 0, reason: `circuit_breaker:${brief.obj}` })
    continue
  }

  log(`[${results.length + 1}/${okBriefs.length}] ${brief.name} (${brief.addr})`)

  let score = 0
  let srcFile = brief.source_file
  let totalAttempts = 0
  let triedRelift = false
  let committed = false
  let finalReason = ''

  // ── Attempt 1: Opus lift ────────────────────────────────────────────────────
  const a1 = await agent(liftPrompt(brief, false, null), {
    label: `lift1:${brief.name}`, phase: 'Lift', agentType: 'xbox-halo-re-analyst', model: 'opus', schema: LIFT_RESULT_SCHEMA,
  })
  totalAttempts++

  if (!a1 || a1.status === 'infra_blocked') {
    log(`Infra blocked on ${brief.name} — halting`)
    results.push(a1 || { addr: brief.addr, name: brief.name, obj: brief.obj, status: 'infra_blocked', attempts: 1, reason: 'agent_null' })
    break
  }
  if (a1.status === 'skipped') { results.push({ ...a1, obj: brief.obj }); continue }
  if (a1.status === 'build_failed') {
    objFailStreak[brief.obj] = streak + 1
    results.push({ ...a1, obj: brief.obj })
    continue
  }

  score = a1.vc71_score || 0
  srcFile = a1.source_file || brief.source_file

  if (score >= 90) {
    await agent(commitPrompt(brief.name, srcFile, ''), { label: `commit:${brief.name}`, phase: 'Lift' })
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'committed', vc71_score: score, source_file: srcFile, attempts: totalAttempts, reason: 'pass1' })
    objFailStreak[brief.obj] = 0
    log(`✓ ${brief.name} ${score}%`)
    committed = true
  }
  if (committed) continue

  // ── Re-delink: try fresh per-function reference (cheap, fixes bad delinked obj) ──
  log(`  ${brief.name} ${score}% — re-delinking`)
  const rd = await agent(redelinkPrompt(brief.name, brief.addr), { label: `redeLink:${brief.name}`, phase: 'Lift', schema: SCORE_SCHEMA })
  if (rd && rd.vc71_score > score) {
    score = rd.vc71_score
    log(`  re-delink improved to ${score}%`)
  }

  if (score >= 90) {
    await agent(commitPrompt(brief.name, srcFile, 'redeLinked'), { label: `commit:${brief.name}`, phase: 'Lift' })
    results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'committed', vc71_score: score, source_file: srcFile, attempts: totalAttempts, reason: 'redeLink' })
    objFailStreak[brief.obj] = 0
    log(`✓ ${brief.name} ${score}% (re-delink)`)
    committed = true
  }
  if (committed) continue

  // ── Permuter (if score [80,90)) ─────────────────────────────────────────────
  if (score >= 80) {
    log(`  ${brief.name} ${score}% — permuter`)
    const pt = await tryPermuterThenEquiv(brief.name, score, triedRelift, 'Lift')
    score = pt.score
    if (pt.action !== 'continue') {
      await agent(commitPrompt(brief.name, srcFile, pt.reason), { label: `commit:${brief.name}`, phase: 'Lift' })
      results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'committed', vc71_score: score, source_file: srcFile, attempts: totalAttempts + 1, reason: pt.reason })
      objFailStreak[brief.obj] = 0
      log(`✓ ${brief.name} ${score}% (${pt.reason})`)
      committed = true
    }
  }
  if (committed) continue

  // ── Attempt 2: Opus re-lift (using corrected delinked ref) ─────────────────
  if (totalAttempts < 3) {
    log(`  ${brief.name} ${score}% — re-lift (attempt 2)`)
    await agent(revertPrompt(brief.name), { label: `revert-pre-relift:${brief.name}`, phase: 'Lift' })
    const a2 = await agent(liftPrompt(brief, false, null), {
      label: `lift2:${brief.name}`, phase: 'Lift', agentType: 'xbox-halo-re-analyst', model: 'opus', schema: LIFT_RESULT_SCHEMA,
    })
    totalAttempts++
    triedRelift = true

    if (a2 && a2.vc71_score) {
      score = a2.vc71_score
      srcFile = a2.source_file || srcFile
    }

    if (score >= 90) {
      await agent(commitPrompt(brief.name, srcFile, 'relift'), { label: `commit:${brief.name}`, phase: 'Lift' })
      results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'committed', vc71_score: score, source_file: srcFile, attempts: totalAttempts, reason: 'relift' })
      objFailStreak[brief.obj] = 0
      log(`✓ ${brief.name} ${score}% (re-lift)`)
      committed = true
    }

    if (!committed && score >= 80) {
      log(`  ${brief.name} ${score}% — permuter after re-lift`)
      const pt2 = await tryPermuterThenEquiv(brief.name, score, triedRelift, 'Lift')
      score = pt2.score
      if (pt2.action !== 'continue') {
        await agent(commitPrompt(brief.name, srcFile, pt2.reason), { label: `commit:${brief.name}`, phase: 'Lift' })
        results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'committed', vc71_score: score, source_file: srcFile, attempts: totalAttempts + 1, reason: pt2.reason })
        objFailStreak[brief.obj] = 0
        log(`✓ ${brief.name} ${score}% (${pt2.reason})`)
        committed = true
      }
    }
  }
  if (committed) continue

  // ── Attempt 3: Opus re-lift with escalation focus ───────────────────────────
  if (totalAttempts < 4) {
    log(`  ${brief.name} ${score}% — re-lift (attempt 3, escalation focus)`)
    await agent(revertPrompt(brief.name), { label: `revert-pre-attempt3:${brief.name}`, phase: 'Lift' })
    const a3 = await agent(liftPrompt(brief, true, score), {
      label: `lift3:${brief.name}`, phase: 'Lift', agentType: 'xbox-halo-re-analyst', model: 'opus', schema: LIFT_RESULT_SCHEMA,
    })
    totalAttempts++
    triedRelift = true

    if (a3 && a3.vc71_score) {
      score = a3.vc71_score
      srcFile = a3.source_file || srcFile
    }

    if (score >= 90) {
      await agent(commitPrompt(brief.name, srcFile, 'opus'), { label: `commit:${brief.name}`, phase: 'Lift' })
      results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'committed', vc71_score: score, source_file: srcFile, attempts: totalAttempts, reason: 'opus' })
      objFailStreak[brief.obj] = 0
      log(`✓ ${brief.name} ${score}% (Opus)`)
      committed = true
    }

    if (!committed && score >= 80) {
      log(`  ${brief.name} ${score}% — permuter after Opus`)
      const pt3 = await tryPermuterThenEquiv(brief.name, score, triedRelift, 'Lift')
      score = pt3.score
      finalReason = pt3.reason
      if (pt3.action !== 'continue') {
        await agent(commitPrompt(brief.name, srcFile, pt3.reason), { label: `commit:${brief.name}`, phase: 'Lift' })
        results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'committed', vc71_score: score, source_file: srcFile, attempts: totalAttempts + 1, reason: pt3.reason })
        objFailStreak[brief.obj] = 0
        log(`✓ ${brief.name} ${score}% (${pt3.reason})`)
        committed = true
      }
    }
  }
  if (committed) continue

  // ── All attempts exhausted → revert ────────────────────────────────────────
  await agent(revertPrompt(brief.name), { label: `revert:${brief.name}`, phase: 'Lift' })
  objFailStreak[brief.obj] = streak + 1
  results.push({ addr: brief.addr, name: brief.name, obj: brief.obj, status: 'reverted', vc71_score: score, source_file: srcFile, attempts: totalAttempts, reason: finalReason || `exhausted_at_${score}pct` })
  log(`✗ ${brief.name} ${score}% (reverted after ${totalAttempts} attempts)`)
}

// ── Phase 4: Report ───────────────────────────────────────────────────────────

phase('Report')

const committed   = results.filter(r => r.status === 'committed')
const skipped     = results.filter(r => r.status === 'skipped')
const reverted    = results.filter(r => r.status === 'reverted')
const infra       = results.filter(r => r.status === 'infra_blocked')
const cbObjs      = Object.entries(objFailStreak).filter(([, n]) => n >= 2).map(([o]) => o)

log(`\n── Run complete ──────────────────────────────────`)
log(`Committed:  ${committed.length}`)
log(`Skipped:    ${skipped.length}`)
log(`Reverted:   ${reverted.length}`)
log(`Infra:      ${infra.length}`)
if (cbObjs.length) log(`Circuit-break objects (inspect delinked refs): ${cbObjs.join(', ')}`)
if (budget.total)  log(`Budget remaining: ~${Math.round(budget.remaining() / 1000)}k tokens`)

await agent(
  `Append a run summary to artifacts/auto_lift/goal_progress.md (create if missing).

## Auto-lift run — ${committed.length} committed / ${results.length} processed

| function | addr | obj | vc71 | attempts | action | reason |
|---|---|---|---|---|---|---|
${results.map(r => `| ${r.name} | ${r.addr} | ${r.obj || '-'} | ${r.vc71_score ?? '-'} | ${r.attempts} | ${r.status} | ${r.reason || ''} |`).join('\n')}

Circuit-breaker objects: ${cbObjs.length ? cbObjs.join(', ') : 'none'}`,
  { label: 'progress-log', phase: 'Report' }
)

// ── Drift report: surface kb.json entries whose ported flag is stale ──────────
// Functions implemented in src/ but not yet marked ported=true will be
// re-selected on the next run. Detect and log them now so they can be fixed
// before wasting lift attempts.
await agent(
  `Scan for kb.json ported-flag drift and report any findings.

Run:
  python3 - <<'EOF'
import json, re, sys
from pathlib import Path

ROOT = Path(".")
kb = json.loads((ROOT / "kb.json").read_text())
fn_re = re.compile(r'^[A-Za-z_][A-Za-z0-9_*\\s]+\\bFUN_([0-9a-fA-F]{8})\\s*\\(', re.M)

src_addrs = {}
for f in (ROOT / "src").rglob("*.c"):
    try:
        text = f.read_text(errors="ignore")
    except OSError:
        continue
    for m in fn_re.finditer(text):
        addr = "0x" + hex(int(m.group(1), 16))[2:]
        if addr not in src_addrs:
            lineno = text[:m.start()].count("\\n") + 1
            src_addrs[addr] = f"{f.relative_to(ROOT)}:{lineno}"

drift = []
for obj in kb.get("objects", []):
    for func in obj.get("functions", []):
        addr = func.get("addr", "")
        if func.get("ported"):
            continue
        loc = src_addrs.get(addr)
        if loc:
            drift.append({"addr": addr, "name": func.get("decl",""), "loc": loc,
                          "ported": func.get("ported")})

if drift:
    print(f"DRIFT: {len(drift)} function(s) in source but not ported=true in kb.json:")
    for d in drift:
        print(f"  {d['addr']}  {d['loc']}  (ported={d['ported']})")
else:
    print("No kb.json ported-flag drift detected.")
EOF

If drift is found, include the list in the goal_progress.md summary under a
"## Drift warnings" section. These should be fixed (ported set to true) before
the next mass-lift run to avoid re-lifting already-implemented functions.`,
  { label: 'drift-check', phase: 'Report' }
)

return { committed: committed.length, skipped: skipped.length, reverted: reverted.length, infra_blocked: infra.length, circuit_breaker_objects: cbObjs, results }
