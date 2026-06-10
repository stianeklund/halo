# Workflow Improvement Plan: Lift Pipeline Prevention, Efficiency, and Model Cost

Status: PROPOSED (2026-06-10). Source: deep evaluation of `docs/lift-learnings.md`,
`.claude/commands/*`, `.claude/skills/*`, `tools/audit/*`, `tools/lift/*`,
`tools/lift_pipeline.py`, `tools/llm_auto_lift.py`, and verify caches.

This document is written for an implementing LLM agent. Each task is self-contained:
context, exact files, implementation sketch, and acceptance criteria. Tasks are
ordered by priority. Do them as **separate small commits** (repo rule: one slice per
commit). Always prefix shell commands with `rtk`. Never Read `kb.json` directly —
use `rtk jq`. All lifted C is C89; the tools below are Python and unaffected.

## Motivating findings (verified 2026-06-10)

- Of the 17 bug classes in `docs/lift-learnings.md`: **2 have FULL automated
  detection** (§12 permuter-vacuous, §16 void-returns-EAX), **5 PARTIAL**
  (§1 XCALL, §2 stack aliasing, §3 NULL reg-args, §7 arg mis-group, §10 reg order),
  **10 NONE** — yet ≥6 of the NONE class are mechanically detectable with patterns
  the doc itself specifies.
- VC71 score distribution (2,382 scored functions in `tools/verify/vc71_scores.json`):
  664 @100%, 61 @98–100, 611 @90–98, 269 @85–90, 577 @65–85, 200 <65.
  → 777 functions sit below the repo's own 85% "structural bug" threshold;
  880 sit in the permuter-eligible [85,98) band.
- Behavioral (equivalence) coverage: `tools/equivalence/leaf_cache.json` has only
  **95 entries (24 high-confidence, 0 Z3-proven)** vs ~2,771 ported functions (~3%).
  Per lift-learnings §9, instruction-faithful ≠ behavior-faithful: the costliest
  bug classes (§2, §6, §8, §13, §17) are invisible to VC71 instruction diffing.
- Ghidra context cache: 197 packs vs ~4,702 unported functions (**4% coverage**).
- `check_lift_hazards.py::VOID_BUT_RETURNS_EAX` has **one hardcoded entry** (0xa9380).
- The call-site **ARG_COUNT audit was removed** from context enrichment in
  `tools/llm_auto_lift.py` (FPU-only now), despite §7 arg mis-grouping being
  "the dominant defect class in the objects.obj mass lift."
- `tools/lift/buffer_alias_detector.py` is **not wired into any pipeline/hook**
  (manual invocation only) and is documented false-positive-prone (§9).
- Skill duplication: `/auto-lift` and `/goal-lift` carry near-identical copies of
  the Opus escalation flow (~90 lines), Phase-1 subagent prompt, and pass/fail
  thresholds. `/mass-lift` is listed in the session skill registry but has **no
  definition** in `.claude/skills/`, `.claude/commands/`, or `~/.claude/`.
- Cruft: `.claude/commands/frontier.md~`; legacy `review`/`promote` subcommands
  documented in `auto-lift.md`; `tools/verify/verify_option3.py` likely stale.

---

## Task 1 — Add five cheap lints to `check_lift_hazards.py` (P0)

**Goal:** convert lift-learnings §4, §6, §13, §17, and the §8/§11 `(void)`-discard
tell into build-gate checks. These patterns each cost a full debugging session and
the doc already specifies their detection recipe.

**File:** `tools/audit/check_lift_hazards.py` (existing check functions like
`check_void_eax_returns()` show the structure: scan source files, return
ERROR/WARN findings; ERROR exits non-zero and blocks the build/commit via the
lift-audit pre-commit hook).

Scan scope: lifted C under `src/halo/` (same file set the existing checks walk).
Skip comments and string literals when matching.

### 1a. §6 float-as-pointer bit smuggling — ERROR
Pattern: `(float)(int)` cast chain (and `(float)(unsigned int)`), i.e. regex
`\(float\)\s*\(\s*(unsigned\s+)?int\s*\)` applied to expressions.
Per lift-learnings.md §6 (≈line 266): every instance is a potential bug; the
correct idiom is `memcpy(&dst, &src, 4)` or a union. Allow suppression with an
inline `/* hazard-ok: numeric-truncation */` comment for the rare genuine
truncate-then-float case.

### 1b. §17 address offset rendered as value add — ERROR
Pattern from lift-learnings.md §17 (≈line 792):
`\*\(\w+\s*\*\)\s*0x[0-9a-fA-F]+\s*\+\s*[0-9]` — a dereference of a literal
address followed by a small integer add. Heuristic refinement to cut false
positives: only flag when the added constant is < 0x10 (field-offset sized) and
the line is not already parenthesized as `*(T*)(0xADDR + N)`. Same
`/* hazard-ok */` suppression escape hatch.

### 1c. §13 CONCAT survival — ERROR
No `CONCAT16|CONCAT22|CONCAT44|CONCAT*` token may appear in lifted C.
`tools/lift/draft_decompiler.py` rewrites these in the *draft*; nothing checks the
*output*. This is a pure token grep — zero false positives expected.

### 1d. §4 parameter corruption by loop — WARN
Detect: a function **parameter** assigned/incremented inside a loop body
(`param = param + N`, `param++`, `param +=`) AND referenced after the loop ends.
Implementation: lightweight per-function scan — find parameter names from the
function signature, track brace depth of `for`/`while`/`do` bodies, flag
param-mutation inside a loop when the same identifier appears after the loop's
closing brace. Imperfect parsing is fine: WARN level, not ERROR. Consider
`rtk ast-grep` if a structural pattern is easier than hand-rolled scanning.

### 1e. §8/§11 discarded non-trivial result — WARN
Pattern: `(void)\s*\w+\s*;` where the variable was assigned from a function call
in the preceding ~5 lines, or directly `(void)\s*\w+\(`. Per §8: "a `(void)x;` on
a non-trivial result is a red flag, not a cleanup." WARN with the §8/§11 doc
reference in the message.

**Acceptance criteria:**
- `rtk python3 tools/audit/check_lift_hazards.py` runs clean on the current tree
  (fix or `hazard-ok`-annotate any true positives it finds — investigate each one
  first; a hit on existing code may be a live bug worth its own commit).
- Each new check has a unit-style self-test (the file already has a pattern for
  this, or add `tools/audit/tests/test_check_lift_hazards.py` with positive and
  negative fixtures per check).
- ERROR-level checks block: verify the lift-audit pre-commit hook path still
  invokes the full suite.
- Update the check list in `CLAUDE.md`/`AGENTS.md` Hazard Scan bullet and add a
  one-line "Detection: automated (check_lift_hazards.py)" note to §4/§6/§13/§17
  in `docs/lift-learnings.md`.

---

## Task 2 — Wire `buffer_alias_detector.py` into the pipeline (P0)

**Goal:** §2 stack aliasing is the deepest recurring class; its detector exists
but is manual-only.

**Files:** `tools/lift_pipeline.py` (stage list around the existing
`hazard_scan` stage), `tools/lift/buffer_alias_detector.py`.

1. Add a pipeline stage (or fold into `hazard_scan`) that runs
   `buffer_alias_detector.py` against the target's source file. HIGH-RISK hits →
   stage failure under `--verify-policy strict`; WARN otherwise (the tool is
   documented false-positive-prone, lift-learnings §9: "it groups adjacent scalar
   locals as one buffer").
2. Reduce false positives before gating hard: known gaps are (a) it only tracks
   `local_XX` names and misses `acStack_XXXX` variants; (b) containment is
   offset-range heuristic. Fix (a) (regex extension) in this task; leave (b) for
   Task 3.
3. Add a `--quiet` JSON output mode if the tool only emits annotated source today,
   so the pipeline can consume a machine-readable hit list.

**Acceptance:** pipeline run on a known-good target passes; seeding a synthetic
aliased-locals fixture produces a HIGH-RISK stage failure; documented in the
pipeline `--help`.

---

## Task 3 — Frame-map contract for §2 (P1, design+implement)

**Goal:** replace offset-range guessing with a checkable contract derived from the
original binary.

**New file:** `tools/lift/frame_map.py`.

1. From the original function's disassembly (use cached Ghidra context packs in
   `artifacts/auto_lift/context_cache/` when present; else Ghidra MCP
   `disassemble_function`), derive the MSVC frame map:
   - frame size from `SUB ESP, N` or `_chkstk` argument;
   - every `[EBP-N]` / `[EBP+N]` access with its operand width (byte/word/dword/
     qword from the instruction);
   - which slots have their **address taken** (`LEA reg,[EBP-N]` followed by a
     PUSH/CALL or register-arg setup).
2. Emit a JSON frame map artifact per function:
   `{"frame_size": ..., "slots": [{"ebp_off": -0x68, "width": 4, "addr_taken": true, "writes": N, "reads": N}]}`.
3. Validator mode: given the lifted C (locals + arrays + their sizeofs, parsed
   loosely), check that every addr-taken slot region in the frame map is covered
   by a **single** C object (array/struct/buffer) at least as large as the span of
   accesses rooted at that slot. Mismatch → ERROR listing the slot, the span, and
   the C local(s) overlapping it.
4. Integrate as an opt-in lift_pipeline stage first (`--frame-map-check`); promote
   to default after a burn-in period across ~20 known-good lifts.

**Why this works:** lift-learnings §2 variants all reduce to "the original frame
made these bytes contiguous; our clang frame did not." The frame map is binary
evidence (repo doctrine: binary is source of truth) and turns "size the buffer
from the frame, not a guess" into a mechanical check.

**Acceptance:** reproduces the documented historical bugs as failures when run
against the *pre-fix* shapes (test fixtures modeled on FUN_0013b380's 14 locals
and FUN_0013ab20's vec3 from §2); passes on their fixed forms.

---

## Task 4 — Batch-discover §16 void-but-returns-EAX functions (P0, one-shot)

**Goal:** the `VOID_BUT_RETURNS_EAX` table has one entry; the class is
discoverable in bulk.

**New script:** `tools/audit/find_void_eax_returns.py`.

1. Enumerate kb.json functions whose `decl` returns `void`
   (`rtk jq` over kb.json — never Read it).
2. For each, fetch the last few instructions before `RET` (prefer cached context
   packs; else Ghidra MCP `read_memory`/`disassemble_bytes` at the tail — keep
   Ghidra calls narrow per token-discipline rules; run
   `rtk python3 tools/audit/check_ghidra_mcp.py` preflight first).
3. Flag functions ending `MOV EAX,[EBP+8]` (returns param_1) or
   `LEA EAX,[EBP-N]` (returns local — rarer, needs manual review).
4. Output a report: address, name, ported?, and whether any **unported caller's**
   decompile assigns the call result to a pointer (strongest evidence; optional
   second pass via Ghidra decompile of callers — only for flagged hits).
5. For each confirmed hit: update kb.json `decl` to return the pointer type, add
   to `VOID_BUT_RETURNS_EAX` in `check_lift_hazards.py`, and if ported, add
   `return param_1;` to the C body (see §16 fix template,
   lift-learnings.md ≈line 744). **Each kb.json decl change is a small separate
   commit** — bad declarations crash the game (memory: feedback_kb_declarations).

**Acceptance:** script runs repo-wide and produces the report; the known entry
0xa9380 is rediscovered by it (positive control); table updated with confirmed
hits only (explicit-unknowns doctrine: do not bulk-change decls without the
caller-consumes-EAX evidence or disasm tail evidence).

---

## Task 5 — Reinstate call-site arg-count audit with stack-cleanup cross-check (P0)

**Goal:** §7 cdecl arg mis-grouping is the dominant defect class; the ARG_COUNT
hazard was removed from context enrichment (presumably for false positives).
Reinstate it with the discriminator that makes it precise.

**Files:** `tools/llm_auto_lift.py` (enrichment phases), possibly a standalone
`tools/audit/check_arg_counts.py` so it's runnable outside enrichment.

Algorithm (per lift-learnings §7 "stack-cleanup tell", ≈line 326):
1. For each CALL in the target's disassembly, parse the following `ADD ESP, N`
   (single cleanup) → callee consumes N/4 stack args (cdecl).
2. Compare N/4 against the callee's kb.json decl arg count (account for `@<reg>`
   args, which don't occupy stack slots, and stdcall callees, which clean up
   themselves — skip those).
3. Flag when cleanup-derived count ≠ decl count. Special-case the §7 signature:
   a 0-arg-declared callee immediately preceded by pushes that the *next* CALL's
   cleanup accounts for → emit the specific "inner getter swallowed outer's args"
   warning with both call sites named.
4. Surface in the enrichment context pack (so the lifting model sees it in-context)
   AND as a standalone audit with `--changed-only` for pre-commit use. WARN level
   initially; promote the high-confidence 0-arg-getter case to ERROR after burn-in.

**Acceptance:** reproduces the two documented historical cases as hits when run
against their original disasm (FUN_00084ae0 bored_camera, FUN_001417c0 reconnect —
both described in §7); false-positive rate spot-checked on 10 random already-
verified functions.

---

## Task 6 — Extend `audit_reg_abi.py` to check register **order/mapping** (§10) (P1)

**Goal:** the tool confirms which registers are written before a CALL but not
*which value flows to which register*, missing the §10 swapped-args class
(game_engine_hud_update_player bug, live for months).

**File:** `tools/audit/audit_reg_abi.py` (caller-disasm scan ≈lines 119–207).

1. For each caller of a `@<reg>` function, trace the last write to each annotated
   register before the CALL and classify the source: `[EBP+8]`=param_1,
   `[EBP+0xC]`=param_2, `LEA [EBP-N]`=local, immediate, other-register.
2. Build the caller's actual value→register mapping and compare against the thunk
   convention (kb.json `@<reg>` order = C arg order).
3. When a ported caller's C call passes args in an order inconsistent with that
   caller's original register preparation, ERROR with the suggested swap.
4. This only needs to run for callers being lifted/audited (it already receives
   `--caller-disasm-file` from the pipeline) — extend the existing pass rather
   than adding a new tool.

**Acceptance:** synthetic fixture reproducing §10 (ECX=p1, EBX=p2, EAX=p3 against
a `@<ecx>,@<eax>,@<ebx>` decl) is flagged; existing clean lifts still pass.

---

## Task 7 — Learnings-must-ship-a-detector rule (P0, docs-only)

**Goal:** stop the prose-graveyard pattern.

**Files:** `CLAUDE.md` + `AGENTS.md` (keep aligned), `docs/lift-learnings.md`.

Add to the workflow rules:

> **Every new `docs/lift-learnings.md` section must ship, in the same commit,
> either (a) a check wired into `check_lift_hazards.py` / `draft_decompiler.py` /
> `buffer_alias_detector.py` / the call-site audit, or (b) a one-line
> "Automation: not mechanically detectable because …" justification in the
> section.** A documented grep/regex in the section counts as (a) only when it is
> actually implemented as a check.

Also add an `Automation:` status line to each existing section header in
lift-learnings.md (FULL/PARTIAL/NONE + tool name), using the coverage matrix at
the top of this plan so the next reader sees gaps at a glance.

---

## Task 8 — Pre-LLM draft annotations in `draft_decompiler.py` (P1)

**Goal:** the cheapest interception point is the Ghidra draft, before any model
reads it. The tool already emits `/* AUTO: */` rewrites; add hazard *annotations*
for patterns it cannot safely rewrite.

**File:** `tools/lift/draft_decompiler.py`.

Annotate (comment-only, no rewriting):
- **§8 accumulator:** a `[EBP-N]` slot (or `local_XX`) that is both loaded and
  stored inside a loop body, where the draft shows a constant `(float)0`/`0` in
  the same expression → `/* HAZARD §8: slot is a running accumulator (load+store
  in loop) — verify seed and write-back, see lift-learnings §8 */`.
- **§6 bit-smuggle:** `(type *)(int)(float_expr)` assignments →
  `/* HAZARD §6: float bits smuggled through int — use memcpy/union */`.
- **§2 candidate buffers:** any `&local_XX` passed to a call where adjacent
  `local_YY` declarations exist within the inferred span →
  `/* HAZARD §2: address-of-local passed to callee — verify contiguity/frame map */`.

**Acceptance:** running the draft pass on the historical §8 fixture
(FUN_000aca70 shape) and §6 fixture (FUN_0013ab20 shape) emits the annotations;
existing rewrites are unchanged (idempotence test: run twice, identical output).

---

## Task 9 — Deduplicate skill instructions; thresholds live in the enforcer (P1)

**Goal:** `/auto-lift` and `/goal-lift` duplicate ~90 lines of escalation flow,
the Phase-1 subagent prompt, and pass/fail thresholds (with different numbers,
expressed as divergent prose).

**Files:** `.claude/commands/auto-lift.md`, `.claude/commands/goal-lift.md`,
`tools/lift_pipeline.py`, new shared doc `docs/lift-policy.md` (or a
`.claude/skills/halo-re-lift/` reference file).

1. Extract the **Phase-1 subagent briefing** into one canonical template file;
   both skills reference it ("use the briefing template at <path>, filling in
   target/context").
2. Extract the **escalation flow** (revert → re-run without model override →
   re-verify → revert+log) into the same shared doc.
3. Move **numeric thresholds** into `lift_pipeline.py` as named policies
   (`--verify-policy auto|strict|goal90`), so skills say "run with
   `--verify-policy goal90`" instead of restating numbers. The pipeline already
   owns the low-match policy (≈lines 929–995); add the goal-lift band logic
   (≥99 commit / 90–98 commit / 85–89 one-permute / 65–85 cap-check / <65 revert)
   as a policy preset rather than skill prose.
4. While editing: remove or clearly mark the legacy `review`/`promote`
   subcommand docs in `auto-lift.md`; delete `.claude/commands/frontier.md~`.
5. Resolve `/mass-lift`: it appears in the session skill registry with a detailed
   description but has no definition in the repo or `~/.claude`. Determine its
   origin (likely a plugin); either implement it as a thin orchestration doc over
   the now-shared policy presets, or note in `docs/lift-policy.md` that it is
   unimplemented so nobody invokes it expecting the described behavior.

**Acceptance:** both skill files shrink materially; a grep for threshold numbers
(`65%`, `90%`, `85`) in the two skill files returns only policy-preset names or
references; `lift_pipeline.py --help` documents the presets.

---

## Task 10 — Oracle-strength model routing + Haiku lane (P1)

**Goal:** route model choice by **oracle strength**, not difficulty. When a
target has a delinked reference (byte-match oracle) and is a stubbable leaf
(equivalence oracle), a cheap model's mistakes are caught deterministically —
spend Opus/Fable only where the oracle is weak.

**Files:** `tools/llm_auto_lift.py` (selector), `.claude/commands/auto-lift.md`
and `goal-lift.md` (model selection), optionally `tools/mizuchi/`.

1. Add an `oracle_strength` field to `select` output, computed from existing
   signals: `strong` = delinked ref mapped AND (pure-leaf or stubbable per
   `leaf_cache.json`) AND no `@<reg>` args; `medium` = delinked ref only;
   `weak` = no delinked ref, or reg-args, or known structural-ceiling class.
2. Model policy in the skills:
   - `strong` → Phase 1 with `model: "haiku"`; on VC71 < 90, one retry with
     `model: "sonnet"` before the existing Opus escalation.
   - `medium` → current behavior (Sonnet → Opus).
   - `weak` → Sonnet/Opus from the start + mandatory hazard checks; never Haiku.
3. **Pilot before adopting:** run 10 strong-oracle targets through the Haiku lane
   (`--dry-run` or a branch), record VC71 pass rate, escalation rate, and
   wall-clock vs the Sonnet baseline in
   `artifacts/auto_lift/haiku_pilot_report.md`. Adopt only if the
   first-pass ≥90% rate is within ~15pp of Sonnet's (escalations make up the
   difference cheaply).
4. Optional follow-up: point the mizuchi iterate-to-zero-diff loop
   (`tools/mizuchi/compile_and_view.py`) at the Haiku lane — an exact diff-count
   reward signal is the ideal cheap-model harness; today that loop is
   Codex/OpenCode-only.

**Acceptance:** selector emits the field; skills document the routing; pilot
report exists with a clear adopt/reject decision.

---

## Task 11 — Context-cache coverage sweep (P1, mostly mechanical)

**Goal:** only 197/~4,702 unported functions (4%) have cached Ghidra context
packs; cheap models especially depend on pre-packaged context, and cache hits
eliminate per-lift Ghidra MCP round-trips.

**Files:** `tools/llm_auto_lift.py` (`cache-context`), maybe a small batch driver.

1. Add `cache-context --batch-frontier N` (or a loop driver) that runs the
   existing per-target cache for the top-N frontier/selector candidates,
   skipping already-cached and rate-limiting Ghidra MCP calls.
2. Run it for the top ~300 candidates (covers months of lifting at current pace).
   Run preflight `check_ghidra_mcp.py` first; tolerate and log per-target
   failures without aborting the batch.
3. Record cache size/coverage before/after in the commit message.

**Acceptance:** coverage report (cached/total) printed by a `cache-context
--stats` flag; top-300 sweep completed or a documented blocker (e.g. Ghidra MCP
down) recorded.

---

## Task 12 — Retroactive equivalence campaign for shipped lifts (P2, ongoing)

**Goal:** behavioral coverage is ~3% of ported functions while the costliest bug
classes are invisible to VC71. Build the backstop.

**Files/tools:** `tools/equivalence/unicorn_diff.py` (exists),
`tools/equivalence/leaf_cache.json`, a new thin driver
`tools/equivalence/batch_equivalence.py`.

1. Build the priority queue from existing data, highest first:
   a. ported functions with VC71 < 85% (200 at <65 + 577 at 65–85);
   b. FPU-heavy functions (reuse the pipeline's FPU/risk assessment);
   c. functions passing address-of-local buffers (Task 2 detector output);
   d. the 85–98 band last (also feed these to `/permuter-campaign`).
2. Driver runs `unicorn_diff.py <target> --seeds 100 --allow-stubs --mem-trace
   --float-tolerance 32` per target, persists to `leaf_cache.json` (the tool
   already does), and appends a one-line verdict to
   `artifacts/equivalence/campaign_ledger.md`.
3. **Triage rule:** a divergence is *not* automatically a lift bug — check for
   unresolved relocs in the oracle first (memory: reference_equiv_oracle_reloc_gap;
   harness artifacts masquerade as divergences). Real divergences get a task entry
   each; do not batch-fix.
4. Where zero-state coverage is weak, fall back to state snapshots per the
   CLAUDE.md Live Memory Capture workflow — but treat that as per-target manual
   work, not part of the batch driver.

**Acceptance:** driver exists with `--limit N` and resume support; first 50
highest-priority targets processed; ledger committed; any confirmed divergences
filed as individual follow-ups with the function address and the diff evidence.

---

## Task 13 — Verify/result caching gaps (P2)

**Goal:** VC71 has an effective SQLite cache (9,623 entries); objdiff structural
comparison and the call-site audit recompute every run.

**Files:** `tools/verify/objdiff_lift.py`, `tools/verify/vc71_cache.py` (reuse the
keying pattern), `tools/llm_auto_lift.py` (call-site audit).

1. Cache objdiff match results keyed by
   `(ref_obj_sha256, candidate_obj_sha256, comparator_version)`.
2. Cache call-site audit output keyed by `(disasm_sha256, kb_decl_map_sha256)`.
3. Skip re-ranking the frontier per-target inside batch loops when kb.json's
   mtime/hash is unchanged (cache the ranking for the batch).
4. Investigate `tools/verify/verify_option3.py` and `vc71_regression.py`:
   delete if dead (git log + grep for callers), or add a docstring stating their
   role.

**Acceptance:** second consecutive pipeline run on an unchanged target shows the
cached stages as instant in the stage timing output; stale-tool decision recorded
in the commit message.

---

## Suggested execution order and commit slicing

| Order | Task | Size | Type |
|-------|------|------|------|
| 1 | Task 1 (five lints) | S–M | prevention, P0 |
| 2 | Task 7 (doc rule) | XS | process, P0 |
| 3 | Task 2 (wire buffer detector) | S | prevention, P0 |
| 4 | Task 4 (void-EAX sweep) | M | prevention, P0 |
| 5 | Task 5 (arg-count audit) | M | prevention, P0 |
| 6 | Task 9 (skill dedupe + policy presets) | M | hygiene, P1 |
| 7 | Task 11 (context cache sweep) | S | efficiency, P1 |
| 8 | Task 10 (oracle routing + Haiku pilot) | M | cost, P1 |
| 9 | Task 8 (draft annotations) | M | prevention, P1 |
| 10 | Task 6 (reg-order audit) | M | prevention, P1 |
| 11 | Task 3 (frame map) | L | prevention, P1 |
| 12 | Task 12 (equivalence campaign) | L/ongoing | accuracy, P2 |
| 13 | Task 13 (caching gaps) | S | efficiency, P2 |

Rules for the implementing agent:
- One task (or sub-task for Task 1's five lints, if any lint surfaces true
  positives in existing code) per commit. Use normal commit messages for tooling
  work; the `generate_lift_commit.py` flow is only for function ports.
- New detectors start at WARN unless stated ERROR; promote to ERROR only after a
  clean run over the existing tree or a burn-in note.
- Any true positive a new detector finds in already-shipped code is **its own
  investigation** (possible live bug — see the §2 vec3 green-tint precedent);
  do not silence it to make the gate pass.
- Do not touch `@<reg>` annotations in kb.json except via the Task 4/6 evidence
  flow; baseline drift hard-fails commits (see memory: FUN_001171a0 drift).
- Keep `CLAUDE.md` and `AGENTS.md` aligned for every doc change.
