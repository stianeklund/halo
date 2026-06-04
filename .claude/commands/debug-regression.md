---
description: Investigate and fix Halo CE XBE regressions via git bisect, Ghidra verification, and xemu probing
agent: deep
subtask: true
---

Use `halo-xbox-re` for doctrine, evidence rules, and the review checklist.
Use `halo-verify-debug` for regression-specific workflow and xemu probing.

Investigate and fix a Halo CE XBE regression. Bisect via git history first,
verify root cause against binary and disassembly, then implement the minimal
safe fix.

Argument: $ARGUMENTS (description of the regression symptom or failing test)

## Investigation priority

**Always start with git history.** Most regressions are introduced by a recent
commit. Static analysis is fast, free, and usually sufficient. One thing is even
cheaper and runs first: a doc lookup (Phase 0). It *informs* the bisect — it
does not replace it.

### Phase 0 — Consult prior learnings (near-free, do first)

Many regressions match a documented bug class. `docs/lift-learnings.md` catalogs
recurring failure modes — packed-field (CONCAT) mistranslation, vertex-stride
mismatch, buffer aliasing, cdecl arg mis-grouping, FPU operand order, float-as-
pointer bit-smuggling, stale-map mis-symbolization — each with detection and fix
steps. A match often names the root cause up front and tells you what to look
for in the diff.

If the `qmd` MCP tools are available (the doc vector index over `docs/`):

- `mcp__plugin_qmd_qmd__query` with `searches` describing the symptom — pair a
  `lex` keyword line with a `vec` natural-language line — set `intent` to the
  regression, and scope `collections: ["halo-docs"]`.
- Read the top hit with `mcp__plugin_qmd_qmd__get` at the reported `line`
  (`fromLine = line - 20`, `maxLines = 80`).
- Carry any matching § into Phase 1 as the hypothesis to confirm or refute.

If qmd is unavailable, fall back to `rtk rg -n '<keyword>' docs/lift-learnings.md`.
Either way a doc match is a lead, not proof — confirm it against the binary in
Phase 1/2 before fixing.

### Phase 1 — Git bisection (the investigation backbone)

1. `git log --oneline -20` — identify recent commits touching `kb.json`, C
   source, or types.
2. `git diff HEAD~N -- kb.json src/` — inspect what changed.
3. For each suspicious commit, check:
   - Calling convention (cdecl/stdcall/fastcall/thiscall/register)
   - Argument count vs. `PUSH` count and `ADD ESP,N` cleanup
   - Return type vs. caller use of `EAX`
   - Struct size, packing, field offsets, static asserts
   - Side-effect ordering and initialization
   - `kb.json` `HDATA` pointer-vs-array choice (wrong level of indirection)
   - `@<reg>` entry without a corresponding implementation (→ infinite recursion)
   - Empty stub silently replacing a function with required side effects

### Phase 2 — Binary / Ghidra verification

Cross-check any suspect prototype against Ghidra disassembly. The decompiler
lies about operand sizes, interleaved pushes, and register args. Always verify
with the raw disassembly for each candidate.

### Phase 3 — Live probing (last resort)

Only when static analysis leaves the root cause genuinely ambiguous.

**Prefer XBDM on real Xbox** over xemu whenever a console is available:

- Build and deploy: `/deploy --xbe-only`
- Then probe with `/xbdm status`, `/xbdm context`, or `/xbdm mem <addr> <len>`

**Fallback — xemu probing** (only if no Xbox is reachable):

Use `mcp__xemu__*` tools directly — daemon auto-starts via SessionStart hook:

- `mcp__xemu__xemu_send_monitor_command("info registers")` — EIP/registers after crash
- `mcp__xemu__xemu_send_monitor_command("x /16xw 0x<addr>")` — inspect memory
- `rtk python3 tools/xbox/xdbm_screenshot.py --host 127.0.0.1 --images 5 --png` — capture visible state
- `mcp__xemu__xemu_read_serial()` — assert_halt messages on serial
- `mcp__xemu__xemu_pause()` / `mcp__xemu__xemu_resume()` — pause/resume VM

Fallback to `tools/xbox/xemu_qmp.py` only when the MCP daemon is unavailable.

### Phase 3b — Toggle-bisect for visual/render regressions

For tint / culling / no-draw / no-spawn regressions, static diff and VC71/
instruction-diff do **not** converge — the oracle is the runtime ported-flag
toggle-bisect (memory `feedback_toggle_bisect_visual_regression`): flip
`ported=false` on suspect functions/TUs with `artifacts/toggle_ported.py`,
rebuild, redeploy, then eyeball the symptom.

**Mandatory liveness gate — run after each toggle deploy, before any verdict:**

```
rtk python3 tools/xbox/verify_toggles_live.py
```

The deploy `rev` token is pre-patch: `verify: OK` proves running==built *source*
but does **not** prove your `ported` toggle is live in the running image. The
verifier QMP-reads each function's VA and confirms toggled-off funcs run ORIGINAL
bytes (`55 8B EC…`) while a sampled positive control still shows redirects
(`68 <impl> C3`) — it hard-fails on a stale/cached XBE. On `RESULT: FAIL`,
redeploy and retry; never record an in-game verdict against a failing gate (a
whole session was once burned this way — memory `verify-toggles-live`). Run it
before `git checkout kb.json`, since it compares the live image to the
working-tree kb.json.

## Common regression classes

- Wrong prototype (arg count, arg type, return type)
- Wrong calling convention → ESP drift → crash at `eip=0x4`
- Wrong signedness or operand width
- Struct size/packing/offset drift
- Incorrect `HDATA` indirection level
- Reordered or missing writes / side effects
- Empty stub replacing a function that had required side effects
- `@<reg>` thunk without implementation → infinite recursion

## Fix process

Once the root cause is established:

1. Patch only what is necessary: code, type, layout, prototype, or `kb.json`.
2. Preserve:
   - calling convention and stack behavior
   - field offsets, packing, and static asserts
   - side-effect ordering
   - initialization and teardown behavior
3. Run the build and test in xemu after the fix.

## Allowed changes

- Narrow code corrections
- Prototype fixes
- Struct field/type corrections
- Restoring required side effects
- Conservative `kb.json` fixes
- Targeted static asserts if they prevent recurrence

## Disallowed (unless explicitly requested)

- Broad refactors or naming sweeps
- Style cleanup
- Multiple independent fixes in one patch
- Redesigning subsystem behavior

## Output format

Follow the `halo-xbox-re` output format. Report:

- **Symptom**: what failed and how it was observed
- **Commits investigated**: which changes were examined and why
- **Root cause**: the single most-supported hypothesis
- **Confirmed / Inferred / Uncertain**: label every claim per evidence policy
- **Fix**: exact change made (prototype, `kb.json` field, struct field, side effect)
- **Why minimal**: what was deliberately not changed
- **Validation**: build result, xemu test, callers/callees checked
- **Follow-up**: any remaining uncertainty or recommended next checks

## Hard rules

- Git history first — don't reach for xemu before reading the diff.
- Do not fix more than the evidence supports.
- Do not repack or reorder structs without binary evidence.
- If the hypothesis is too weak to fix safely, say so and stop.
- Unknown is better than wrong.
