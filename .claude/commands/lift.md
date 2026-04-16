Two-phase lift: RE analysis + implementation by the xbox-halo-re-analyst agent,
then build and verify via the pipeline.

Argument: $ARGUMENTS (optional flags + target name or 0x... address)

Flags (strip before passing the target to the agent):
- `--resume` — inject prior session context from `.claude/analyst_session.md`
- `--fresh`  — explicit clean slate, ignore any existing session file (default)

---

## Argument parsing (do this before spawning anything)

1. If `--resume` is present in $ARGUMENTS set `RESUME_MODE=true` and remove
   the flag; otherwise `RESUME_MODE=false`.
2. If `--fresh` is present remove it (it's the default, just explicit).
3. Whatever remains is `TARGET` (may be empty — that means auto-pick).
4. If `RESUME_MODE=true`: read `.claude/analyst_session.md`.
   - If the file exists, call its contents `SESSION_CONTEXT`.
   - If it does not exist, set `SESSION_CONTEXT=""` and note this to the user.

---

## Phase 1 — Analysis + Implementation

Spawn an Agent with `subagent_type: "xbox-halo-re-analyst"` with the following
prompt. Substitute `TARGET` and, when `RESUME_MODE=true`, prepend the
`SESSION_CONTEXT` block shown below.

---

**[If RESUME_MODE=true and SESSION_CONTEXT is non-empty, open the prompt with:]**

> **Prior session context** (carried forward from last run — use this to avoid
> re-analysing already-confirmed facts and to pick up open questions):
>
> ```
> <SESSION_CONTEXT verbatim>
> ```
>
> ---

**[Always include:]**

> **Target:** TARGET (if empty, run `python3 tools/frontier.py --limit 5`
> and pick the top candidate)
>
> Perform a complete lift:
>
> 1. Resolve the target in kb.json: address, name, object, source_path.
> 2. Analyze via Ghidra MCP — decompile + disassemble + cross-check operand
>    sizes, CALL targets, and register args per the CLAUDE.md procedure.
> 3. Produce a structurally faithful C implementation following CLAUDE.md rules.
> 4. Write the implementation directly to the source file using the Edit or
>    Write tool at the correct address-ordered position. Do not write to a temp
>    file — write to the real source file.
> 5. If the kb.json declaration needs updating (prototype, register args, new
>    symbol), update it.
> 6. Run `python3 tools/maintain.py <source_file>` to sort and reformat.
> 7. Commit the changes in such a way it can be easily bisected in the future to identify regressions
> 8. Write 2–4 sentences on **what to test in-game** to catch regressions:
>    what gameplay behaviour exercises this code path, what would visibly break
>    if the implementation is wrong, and any edge cases worth prodding
>    (e.g. specific map, game mode, player count, network condition).
>    Label this section **Regression smoke-test**.
> 9. End your response with exactly one line: `RESOLVED_TARGET: <function_name>`
>
> **Session update (required — always do this last):**
> Write a compact session summary to `/mnt/g/dev/halo/.claude/analyst_session.md`
> (overwrite the file). Keep it under 40 lines. Format:
>
> ```
> # Analyst Session
> Updated: <ISO date>
> Active TU: <source file relative path>
>
> ## Recent functions (newest first, keep last 5)
> - 0x<addr> `<name>`: <1-line confirmed summary>
>
> ## Open questions
> - <unresolved items to carry into next run>
>
> ## Active types
> - <any struct/type discoveries not yet in types.h>
>
> ## Regression smoke-test
> <2–4 sentences: what to exercise in-game to catch regressions from this lift>
> ```
>
> If the file already exists, read it first and merge: prepend the new function
> entry under "Recent functions" and drop the oldest if the list exceeds 5.
> Update "Open questions" and "Active types" in place.

---

## Phase 2 — Build + Verify + Run

After the agent returns:

1. Parse the `RESOLVED_TARGET: <name>` line from the agent's response to get
   the canonical function name.
2. Check whether the agent already ran a successful build (look for a build
   success message in its output). If it did not, run:
   ```
   python3 tools/lift_pipeline.py --target <name> --no-metadata-update
   ```
   No `--candidate` is needed — the agent already wrote to the source file.
   If the pipeline build step fails, stop here and report the error — do not
   proceed to the ISO step.
3. On build success, run:
   ```
   python3 tools/build_and_iso.py
   ```
   This builds the ISO and hot-loads it into xemu (launching xemu if not
   already running). The cmake step is a fast no-op if the agent already
   compiled. Report whether xemu loaded the ISO successfully.
4. Report:
   - Target: name / address / object / source path
   - Agent summary: Confirmed / Inferred / Uncertain items
   - Pipeline stage results (build pass/fail, verify pass/fail)
   - ISO + xemu result (success / warning if xemu load failed)
   - Artifact path from summary.json
   - Session file: confirm `.claude/analyst_session.md` was updated

Notes:
- If Phase 2 build fails, investigate and fix the build error before re-running.
  Do not re-run Phase 1 unless the implementation itself is wrong.
- Use `/lift-verify` for structural verification with `--verify-auto` after a
  successful build.
- Use `/maintain` if ordering or formatting needs a standalone cleanup pass.
- Use `--resume` on the next call to carry forward session context (open
  questions, recent function findings, active type work) without re-analysing
  already-confirmed facts.
- Use `--fresh` (or omit any flag) to start a clean slate — e.g. when switching
  to a different subsystem or after a long break.
