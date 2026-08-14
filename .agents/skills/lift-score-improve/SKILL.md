---
name: lift-score-improve
description: "structural ceiling, vc71, low match, score improve, improve match, 65%, 84%, looks structural: Checklist for recovering VC71 match before declaring a structural ceiling. Invoke when score is 65–84% and the gap looks \"structural\", or at ANY score when the diff shows systematic frame/layout divergence rather than wrong logic. Organized as a recipe atlas keyed by score-context classification rule id."
---

# Lift Score Improvement — Recipe Atlas

**Invoke this skill when:**
- VC71 score is 65–84% and you are about to write "structural ceiling"
- ANY score (even <50%) where the diff shows systematic frame/layout divergence
  (wrong frame size, whole blocks relocated, store/reload vs ST-resident values)
  rather than wrong logic — FUN_001ac680 went 46.1% → 97.3% purely on shape
  levers (`frame_mismatch` recipe below)
- Permuter returned no improvement (may be vacuous — see §12 of lift-learnings)
- A function with `static` large buffers has a suspiciously low score
- `artifacts/score_context/<func>.json` has a non-empty `classification[]` array
  — go straight to the matching recipe below by `rule` id

Source: `docs/lift-learnings.md` §19, §20, §24, §27, §38; classification rules
in `tools/verify/vc71_verify.py::_classify_score_context`.

This is score/byte-accuracy content only. Naming, comments, and readability
cleanups are out of scope for this skill — see the `source-recovery` family instead.

---

## Step -2 — Delegate to Subagent (Recommended)

Once you have identified a specific function to try to improve, delegate the optimization to the `vc71-match-optimizer` subagent. The subagent should inherit the current parent model unless a different model is explicitly requested.

Call the `task` tool:
- **`subagent_type`**: `"vc71-match-optimizer"`
- **`description`**: `"Optimize <func_name> score"`
- **`prompt`**: `"Optimize the function <func_name> in <file.c> to improve its VC71 score. Current score: <score>."`

---

## Step -1 — Run the automated fixer first

Before any manual work, run the mechanical score-improvement tool.  It reads
score-context packs, finds fixable diagnostics (wrong float literals, narrow
field widths, comparison sense bugs), applies candidates one at a time, and
gates each with a VC71 recompile.  Only improvements are kept.

```bash
# Census — what's fixable vs what needs an LLM:
rtk python3 tools/recovery/score_structize.py scan --source <file.c>

# Apply all mechanical fixes (per-edit VC71 gate):
rtk python3 tools/recovery/score_structize.py fix --source <file.c>

# Restrict to one rule or function:
rtk python3 tools/recovery/score_structize.py fix --source <file.c> --rule imm_wrong_literal
rtk python3 tools/recovery/score_structize.py fix --source <file.c> --function <func>
```

The `scan` output splits diagnostics into `fixable` (mechanical) and `needs_llm`
(structural, requires judgment).  After `fix` completes, read the `needs_llm`
list and proceed to the manual recipes below for those.

Currently automated: `imm_wrong_literal` (exact float/int constant fixes).
Reported but not yet auto-edited: `loadw_field_width`, `fcom_bound_sense`,
`fpu_operand_order` — use the manual recipes below for these.

---

## Step 0 — Read the score-context pack first

Before any manual diffing, check whether `vc71_verify.py` already recorded a
score-context pack for this function — it is the same diagnostic data you'd
otherwise re-derive from `--show-diffs` by hand, pre-classified:

```bash
rtk jq '{scores, frame, classification}' artifacts/score_context/<func_name>.json
```

If the file exists, `classification[]` entries each carry `rule`, `evidence`,
and `action`. Jump directly to the matching recipe in the atlas below — each
recipe corresponds 1:1 to a `rule` id. `frame.cand_frame_bytes` vs
`frame.ref_frame_bytes` is a fast frame-size signal; `warnings.loadw`/`.fpu`/
`.imm`/`.fcom` flag field-width, operand-order, and immediate-literal defects
directly, independent of whether the summary classifier fired. Missing file
just means no VC71 run has scored this function yet — run it, or proceed with
the manual recipes below (the atlas entries work without a pack too).

- **Triage near-zero scores (<5%) first**: If `scores.official_pct` is <5% and `n_cand_insns` is 1–5 vs hundreds of `n_ref_insns` (e.g. `collision_log_render` at 0.4%), check if the function was originally stubbed or had its body omitted (`/* Debug body omitted */`). Lift the full decompilation first before trying shape levers.

### Preserve only measured gains

Before changing an existing lift specifically to improve its score, record a
whole-TU baseline. Then make exactly one evidence-backed source change and
gate it before trying another lever:

```bash
rtk python3 tools/verify/score_improve.py baseline \
  --source <file.c> --output artifacts/score_improve/<func>-baseline.json

# Apply one candidate change, then:
rtk python3 tools/verify/score_improve.py check \
  --baseline artifacts/score_improve/<func>-baseline.json \
  --source <file.c> --target <func> \
  --output artifacts/score_improve/<func>-check.json
```

`check` passes only when the target improves by at least 0.01pp, every scored
function remains present, no score falls, and the warning count does not grow.
On failure, restore only the candidate change; never accumulate neutral or
regressive experiments. For an isolated candidate worktree, `score_improve.py
trial --worktree <path> --candidate-cmd <command> ...` runs the command before
the same gate.

### Preconditions worth checking once, before trusting any score

- **`@<reg>` annotation formatting**: no leading/trailing spaces in `kb.json`
  (`param@<esi>`, not `param @<esi>`) — a space breaks `vc71_verify`'s regparm
  parser and silently misscoures every call site.
- **Return type register**: `bool`/`char` → `AL`; `int`/`uint32_t` → `EAX`;
  `float` → `ST(0)`. A mismatched kb.json return type reads the wrong
  register and produces a phantom mismatch that no source-level lever fixes.
- **Regenerate the decl header after any kb.json edit**:
  ```bash
  rtk python3 -c "import sys; sys.path.insert(0, 'tools/verify'); import vc71_regression; vc71_regression.regen_decl_header()"
  ```

---

## Recipe atlas — by classification rule id

Each entry: what the signature looks like, the lever(s) to apply, the
expected gain where measured, and the verify command to confirm it.

### `chkstk_static_buffer`

**Signature:** `frame.ref_has_chkstk_call == true` and
`frame.cand_has_chkstk_call == false` — the reference disassembly calls
`_chkstk`/`alloca_probe` for a large stack frame; the candidate doesn't.
Fastest check in the atlas — do this first, it takes 5 seconds:

```bash
grep -rn 'static.*avoid.*_chkstk\|static.*_chkstk' src/
```

**Lever:** convert that `static` local declaration to a plain stack
declaration. `_chkstk` is now a no-op stub in `xbox_crt.c` — the linker error
that originally motivated the `static` workaround is gone.

**Expected gain:** +10pp (77.3% → 87.1% on FUN_00025c10).

**Verify:** `rtk python3 tools/verify/vc71_verify.py <file.c> --function <func>`
— confirm `frame.cand_has_chkstk_call` flips to `true` and frame bytes move
toward `frame.ref_frame_bytes`.

---

### `fpu_operand_order`

**Signature:** `warnings.fpu` non-empty / `[FPU-WARN]` lines — cross-product
argument order or `FSUB`/`FSUBR` operand direction differs from the
reference.

**Levers:**
- **Cross-product subtraction order.** Verify against disassembly:
  `cross(A,B)[0] = A[1]*B[2] - A[2]*B[1]`. Reversing the order negates the
  vector — same instructions, wrong sign, still shows as an FPU-WARN.
- **FSUB vs FSUBR direction.** Trace the operand order in the reference
  disassembly line by line; `a - b` and `b - a` select different x87
  mnemonics for the same C-level subtraction depending on which operand is
  already on the FPU stack.

**Verify:**
```bash
rtk python3 tools/verify/vc71_verify.py <file.c> --function <func> --fpu-only
```

---

### `loadw_field_width`

**Signature:** `warnings.loadw` non-empty / `[LOADW-WARN]` — the candidate
loads a field at `int` (dword) width where the reference loads `int16_t`/
`int8_t` (word/byte).

**Root cause:** MSVC 7.1 allocates registers by exact C variable size:
1-byte vars (`bool`, `char`, `uint8_t`) → `AL`/`BL`/`CL`/`DL`; 2-byte vars
(`short`, `uint16_t`) → `AX`/`BX`/`CX`/`DX`; 4-byte vars → the full 32-bit
register.

**Levers:**
1. Local boolean/status flags: `int success = 0;` → `bool success = false;`
   (or `char`) so MSVC switches from `ESI`/`EDI`-class allocation to `BL`.
2. Loop counters/bounds over word- or short-sized arrays: `int i;` → `short
   i;` so MSVC emits `testw %cx, %cx` instead of a dword compare.
3. Parameters: narrow to `char`/`short` if the disassembly loads
   `movb 0x8(%ebp), %al` / `movw 0x8(%ebp), %ax`.

**Verify:**
```bash
rtk python3 tools/verify/vc71_verify.py <file.c> --function <func> --loadw-only
```
docs/lift-learnings.md §24 has the field-width detector detail.

---

### `imm_wrong_literal`

**Signature:** `warnings.imm` non-empty / `[IMM-WARN]` — a numeric literal
in the candidate doesn't match the immediate value or const-pool reference
in the disassembly.

**Levers:**
1. **Address-deref vs literal.** Ghidra often renders a constant as a global
   dereference (`*(float *)0x2533c8`) when the original is a plain literal
   (`1.0f`). `MOV dword ptr [x],0x3f800000` (an immediate MOV) comes from a
   literal store; `x = *(const float *)0x2533c8;` compiles to a load+store
   pair and never matches an immediate MOV. Pick per-site from the disasm —
   don't default to one form everywhere.
2. **Compares are the opposite of stores.** `FCOMP [FLOAT_002533c0]` needs
   the explicit `*(const float *)0xADDR` operand form, not a bare literal —
   verify which side of each comparison is a memory operand in the
   reference.
3. Prefer a named constant over a magic literal once the value is confirmed
   (see `feedback_prefer_named_constants_over_magic` — doesn't change the
   score but prevents the next agent from re-deriving the same IMM-WARN).

**Verify:**
```bash
rtk python3 tools/verify/vc71_verify.py <file.c> --function <func> --imm-only
```

---

### `fcom_bound_sense`

**Signature:** `warnings.fcom` non-empty / `[FCOM-WARN]` — an FPU-guard
bound sense is lifted wrong: `<=` written as `<`, `>=` as `>`, or the form
negated.

**Levers:**
1. **Never flip a compare direction just to match TEST/Jcc bits.** VC71's
   x87 masks treat unordered (NaN) operands differently per idiom, but
   clang applies strict IEEE C semantics — `x <= 1.0f` and `!(1.0f < x)`
   differ for NaN. Trace the *original's* unordered path (which branch does
   `TEST AH,imm`/`JP`|`JNE` take on NaN?) and keep the C form whose clang
   codegen preserves that path, even at a 1-instruction VC71 cost.
2. **Spell asserts exactly as the assert string reads.** `if (!(cond))` with
   `cond` spelled like the source assert: `!(x >= 0)` → `TEST AH,1;JE`-skip;
   `!(x < 0)` → `TEST AH,5;JNP`-skip. Plain `x < 0` spellings give both the
   wrong mask AND wrong unordered (NaN) behavior.
3. **FCOM operand order mirrors source operand order.** `0.0f > x` loads the
   constant first — match the literal's position in the source expression,
   not just the comparison operator.

**Verify:**
```bash
rtk python3 tools/verify/vc71_verify.py <file.c> --function <func> --fcom-only
```
docs/lift-learnings.md §38 has the FPU-guard sense writeup.

---

### `frame_mismatch`

**Signature:** `frame.cand_frame_bytes != frame.ref_frame_bytes` — right
instructions, wrong frame *shape*: different `sub esp, N`, values held in ST
where the reference stores/reloads, or whole blocks in a different order.
All four levers below proven together on FUN_001ac680 (46.1% → 97.3%).

**Levers:**
1. **`volatile float` local = store-once / reload-each-use.** If the
   reference does `FSTP [slot]` + `FLD [slot]` at every use but the
   candidate keeps the value FPU-enregistered (in ST across branches),
   declare the local `volatile float`. This is also the numerically
   faithful choice — each reload rounds to float where ST would keep double
   precision. VC71's allocator may even place it in a dead param home slot
   (`[EBP+0x18]`) exactly like the original. Caveat: two reads of the
   volatile in one expression = two loads; factor `x = d*cv; return x + x;`
   to get the original's single-load `FADD ST,ST0` doubling.
2. **NEVER take `&param`** (or alias a param through `uint8_t *p = ...`) to
   smuggle a value into a param slot: VC71 then assumes every store through
   the derived pointer clobbers the base, so it pre-computes and SPILLS
   every field address (`lea; mov [ebp-N]` barrage), blows the frame
   (4 → 0x18 bytes observed, ~25pp loss), and spills char flags out of BL.
   Keep field access as `*(int *)(param + 0xN)` on the int-typed param for
   `[esi+N]` addressing.
3. **Code placed after the epilogue** (main path after validate+RET,
   backward jmp) comes from `if (c) {small} else {huge}` — VC71 sinks the
   huge `else` past the join. Writing `if (!c) goto L;` moves blocks the
   OPPOSITE way (goto target inlined, fallthrough sunk). Bonus: a value
   tested in the condition and used in both arms stays ST-resident
   (non-popping FCOMS) if it is a single-assignment non-volatile local.
4. Assert-form comparisons (`if (!(cond))`) also affect frame shape when the
   assert path allocates spill slots — see `fcom_bound_sense` recipe #2
   above for the exact spelling rule; apply both together when a function
   fires both rules.

**Not source-controllable (document, don't chase — ~2pp):** VC71-elided
`x = x;` slot self-moves (two originals coalesced into one slot; clang
blocks the workaround with `-Werror,-Wself-assign`), operand preload
hoisting into both branch successors, FDIV vs FDIVR selection, FLD/FXCH
scheduling.

**Verify:**
```bash
rtk python3 tools/verify/vc71_verify.py <file.c> --function <func>
```
Check `frame.cand_frame_bytes` vs `frame.ref_frame_bytes` converge; docs/lift-learnings.md §27.

---

### `regarg_structural_ceiling`

**Signature:** N `replace`-kind diff ops pairing a `pushl` mnemonic against a
`movl` mnemonic (or vice versa) — VC71 default lane can't put `@<reg>` call
arguments in `eax`/`esi`/`edi`/`ebx`, only push them, while the candidate
(if it has its own register-arg callee) may load instead.

Two distinct causes fire this rule — diagnose which one before picking a
lever:

1. **This function's OWN `@<reg>` params have a late first use.** If a
   register param's first *use* is many lines down from entry, MSVC may
   spill it and reload later, producing extra load instructions. This is
   recoverable — force an early register-load hint right after the
   declarations, before any other logic:
   ```c
   /* Force early register load to match MSVC's register flow */
   float dir0 = direction[0];
   float dir1 = direction[1];
   float dir2 = direction[2];
   ```
   **Expected gain: +11.5pp (FUN_001a1a10, 80% → 91.5%).**

2. **This function CALLS `@<reg>`-arg callees.** Each such call site costs
   ~1 `pushl`-vs-`movl` mnemonic mismatch under the default verify lane
   (VC71 cannot pass args in `eax`/`esi`/`edi`/`ebx`; `--regcall-elide` only
   maps `ecx`/`edx` fastcall args, and that's a verify-tooling flag, not a
   source lever). On a function with ~20 such call sites this is a ~6pp
   structural ceiling. **Not source-fixable — document, don't chase
   further.**

**Verify:**
```bash
rtk python3 tools/verify/vc71_verify.py <file.c> --function <func>
```
Inspect `diff.ops` around the `@<reg>` call sites to tell #1 from #2 —
#1's mismatches cluster near function entry; #2's cluster at each call site
throughout the body.

---

### `anchor_collapse`

**Signature:** `scores.dp_lcs_pct` sits well above `scores.official_pct` —
the official score (`SequenceMatcher.ratio()` over mnemonic-only sequences)
is not a true LCS: its greedy longest-block anchor can mis-pair regions
(e.g. the candidate's FIRST assert block against the reference's LAST, once
both use an identical idiom), collapsing the reported score even though the
code got strictly closer.

**Lever:** this is a scoring artifact, not a code bug. Trust `dp_lcs_pct`
(or the count of `diff.ops`) as the true progress metric while iterating;
keep fixing the regions `--show-diffs` highlights. The official score
re-anchors once enough regions converge — never conclude a regression from
an official-score cliff alone.

**Observed example:** actor_look_update — nesting `display_assert(csprintf(...))`
made 11 assert sites near-identical; official dropped 76.6% → 9.8% while a
true DP-LCS rose 80.1% → 83.1%. Fixing more regions (byte-flag split)
re-anchored it to official 80.4%, agreeing with LCS again.

**Verify:** compare `scores.official_pct` vs `scores.dp_lcs_pct` in the pack
after each iteration; continue if `dp_lcs_pct` is rising even when
`official_pct` looks flat or worse.

---

## No-rule-fired levers (no classification rule yet — apply by inspection, ordered by expected/observed gain)

These are real, measured levers that the automatic classifier in
`vc71_verify.py::_classify_score_context` doesn't yet detect. Check for them
manually via `--show-diffs` when a function's classification array is empty
or doesn't explain the remaining gap.

1. **Pointer-base aliasing for consecutive stores.** Search for 3+ stores to
   consecutive offsets through the same base:
   ```bash
   grep -n '\*(float\*)(.* + 0x[0-9a-f]\+)' src/<file>.c | head -20
   ```
   `*(float *)(obj+0x30)=a; *(float *)(obj+0x34)=b; *(float *)(obj+0x38)=c;`
   hurts — MSVC generates `FSTP [EDI]; FSTP [EDI+4]; FSTP [EDI+8]` only when
   it sees a single base pointer. Fix:
   ```c
   float *up = (float *)(obj + 0x30);
   up[0] = a; up[1] = b; up[2] = c;
   ```
   Applies to both read and write sides. **Recovered ~10 instructions on
   FUN_001a2160.**

2. **VC71 cross-jump tail merging (accumulator variable).** If the reference
   repeats a short conditional-call idiom at two sites (e.g.
   `x = 0; if (ref != -1) x = tag_get(...);` in both an if- and
   else-branch) but the candidate is SHORTER at that spot in
   `--show-diffs`, VC71 merged the two identical tails into one block. The
   reference kept them separate because the original inlined a helper
   there. Do NOT add a real `static` helper (VC71 refuses to inline it → a
   new CALL → score drops). Instead give ONE branch a distinct accumulator
   variable:
   ```c
   void *sky_ptr;             /* mimics the inlined helper's return slot */
   sky_ptr = (void *)0;
   if (tag_ref != -1) sky_ptr = tag_get(...);
   sky_tag = (int)sky_ptr;
   ```
   **Recovered +3.5pp on FUN_0018fbc0 (92.3% → 95.8%).**

3. **cos()/sin() intrinsification.** Does the function use `x87_fcos`,
   `x87_fsin`, or `x87_fcos_mul`?
   ```bash
   grep -n 'x87_fcos\|x87_fsin' src/<file>.c
   ```
   If yes, wrap the call sites:
   ```c
   #if defined(_MSC_VER) && !defined(__clang__)
     result = (float)cos((double)x);   /* VC71 /Oi inlines as FCOS; shares ST0 */
   #else
     result = x87_fcos(x);
   #endif
   ```
   When the same variable feeds both `cos` and `sin`, MSVC shares it on the
   FPU stack as `FLD ST0` — the `x87_*` helpers each do their own
   `FLD [mem]`, so the patterns diverge. **Recovered ~5 instructions on
   FUN_001a2160.**

4. **Inlined call argument evaluation order (right-to-left push).**
   When passing an expression to a function call (especially string formatting / variadic calls like `crt_sprintf`), separating the computation into a temporary evaluates it *before* pushing the other arguments. Inlining the expression in the call argument forces C's right-to-left evaluation order, pushing trailing format arguments first and evaluating the leading expression last:
   ```c
   /* Bad: evaluates csstrlen FIRST, then pushes format arguments */
   char *end = dst + csstrlen(dst);
   crt_sprintf(end, "%d %d", a, b);

   /* Good: pushes b, a, format string first, evaluates dst + csstrlen(dst) last */
   crt_sprintf(dst + csstrlen(dst), "%d %d", a, b);
   ```
   **Recovered +11.2pp on FUN_0014da20 (85.7% → 96.9%).**

5. **Native 64-bit (`int64_t`) arithmetic vs manual 32-bit hi/lo carry chaining.**
   Ghidra frequently decompiles 64-bit integer math (e.g. `QueryPerformanceCounter` or timestamp deltas) as manual 32-bit addition/subtraction with borrow/carry bit checks. Replace them with standard `int64_t` operations:
   ```c
   /* Bad: manual 32-bit carry math */
   elapsed_lo = cur[0] - start_lo;
   elapsed_hi = (int)cur[1] - start_hi - (unsigned int)(cur[0] < start_lo);

   /* Good: native 64-bit arithmetic */
   int64_t elapsed = current - *(int64_t *)&start_lo;
   *(int64_t *)(buf + offset) += elapsed;
   ```
   MSVC 7.1 automatically generates the compact native `sub/sbb` and `add/adc` chains.
   **Recovered +13.8pp on collision_log_add_time (70.6% → 84.4%).**

6. **Inlined `qmemcpy` (`REP MOVSD` intrinsic) vs `csmemcpy` function call.**
   `csmemcpy(...)` compiles to an external function `CALL` (`call 0x...; add esp, 0xc`), whereas `qmemcpy(...)` (or `xbox_memcpy`) expands under VC71 into inlined `REP MOVSD` (or unrolled `MOV` pairs for small sizes).
   ```c
   /* Bad: emits runtime function CALL */
   csmemcpy(dst, src, 0xb88);

   /* Good: emits inlined REP MOVSD */
   qmemcpy(dst, src, 0xb88);
   ```
   **Recovered +14.1pp on collision_log_end_period (72.7% → 86.8%).**

7. **Sentinel return structure (`if (val != -1) { ... return res; } return -1;`).**
   Ghidra often decompiles sentinel exits as `if (val == -1) return -1; elem = lookup(val); return elem->field;`. When the pristine disassembly shows `JZ exit_block` jumping to `exit_block: OR EAX, -1; RET` at the bottom, invert the condition:
   ```c
   /* Bad: emits JNE over early return */
   if (index == (int16_t)-1) return -1;
   elem = lookup(index);
   return elem->field;

   /* Good: emits JZ to bottom OR EAX, -1 */
   if (index != (int16_t)-1) {
     elem = lookup(index);
     return elem->field;
   }
   return -1;
   ```
   **Recovered +21.1pp on FUN_0014da80 (78.9% → 100.0%).**

8. **Nested if-trees → switch restoration.** Ghidra decompiles MSVC
   jump-table `switch` statements into nested `if (code < ...)` branches.
   Signature: reference disassembly shows an indirect jump through a table
   (`jmp [table + reg*4]`) while the candidate is an if/else chain.
   Restoring a single C `switch (var)` block eliminates dozens of redundant
   compare/branch instructions the if-tree emits. Unquantified in pp terms
   — magnitude scales with branch count.

9. **Float literal vs const-pool variable-type tuning.** General case of
   the `imm_wrong_literal` recipe plus the `loadw_field_width` register-size
   rules applied together — when a function has several small mismatches
   that don't individually trip a warning threshold, sweep every local's
   declared width/signedness against its disassembly load/store width in
   one pass rather than one variable at a time. Unquantified in pp terms.

---

## Step — measure after each lever (fast single-function path)

Use the fast per-function compile+diff loop for iteration, not a full TU
`vc71_verify.py` run — it recompiles only the target function against the
TU's headers instead of the whole translation unit (measured ~13x faster on
a 5800-line TU: 1.06s vs 14.0s for the same function).

```bash
# one-time, only if artifacts/mizuchi/prompts/<func>/settings.yaml is missing
rtk python3 tools/mizuchi/gen_prompts.py --target <func_name>

# per-iteration
rtk python3 tools/mizuchi/compile_and_view.py <func_name> --c-file <candidate_fn.c>
```
`<candidate_fn.c>` is just the one function's current text, not the whole
TU. Re-run a full `vc71_verify.py --function <func>` only when you need a
fresh score-context pack (new `classification[]`, `warnings`, `frame` data)
— typically at the start of a session and after resolving whatever the
current pack pointed at.

## Step — run the permuter on the improved source

Only run the permuter AFTER applying the applicable levers above. The search
space is much smaller on improved source, and permuter cannot recover from a
genuine structural bug below ~85%.

```bash
rtk python3 tools/permuter/run.py -q --target <function_name> --attempts 100
```
Exit 3 = vacuous run (candidate never actually compiled/searched), exit 4 =
baseline mismatch — in both cases don't trust the reported "no improvement,"
investigate instead (§12 of lift-learnings). Verify the run is real by
running once WITHOUT `-q` and confirming it prints accruing iteration
counts (hundreds), not an instant exit.

---

## Decision after working the atlas

| After applying applicable recipes | Action |
|------------------------------------|--------|
| Score ≥ 90% | Commit |
| Score 85–89% | Commit; note permuter recommendation |
| Score < 85% | NOW it's a genuine structural ceiling — document which specific unmatched instructions remain (FPU comparison idiom, `@<reg>` prologue preamble, FLD ST(1) depth ref) so future sessions don't re-investigate |
