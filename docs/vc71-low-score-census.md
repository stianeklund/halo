# VC71 Low-Score Census

Snapshot of `tools/verify/vc71_scores.json` at commit `674b06962` (2026-08-13),
cross-referenced by function name against the 5,209 score-context packs in
`artifacts/score_context/`. The join finds contexts for 4,608 of the 4,612 floor
rows, including all 221 rows below 70%. Counts below are measured from that
snapshot unless explicitly described as interpretation.

The question this investigates: **of the functions that score badly, which are
actually bad lifts?** Score data alone cannot answer that. It does show that low
official score is weak evidence: some rows have reference-attribution problems,
some are incomplete implementations, and some are especially sensitive to the
mnemonic aligner. Diagnostic rules identify review leads, not mutually
exclusive or proven causes.

## Distribution

4,612 scored functions:

| Band | Count | Share |
|---|---|---|
| 100% | 1,687 | 36.6% |
| 90–99% | 1,502 | 32.6% |
| 80–89% | 868 | 18.8% |
| 70–79% | 334 | 7.2% |
| 60–69% | 128 | 2.8% |
| 50–59% | 50 | 1.1% |
| 40–49% | 13 | 0.3% |
| 30–39% | 9 | 0.2% |
| 20–29% | 5 | 0.1% |
| 10–19% | 1 | 0.0% |
| 0–9% | 15 | 0.3% |

69% of functions are at 90% or better. The tail worth discussing is the 221
functions below 70%.

## Reference Issue — One-Instruction Functions

There are 115 floor rows whose reference contains one instruction. Of these,
101 score 100.0% and 14 score below 70%; 12 of the latter score exactly 0.0%.
Only 34 of the 115 rows currently carry `kind: "thunk"`, so this population
must not be treated as one proven category without checking reference opcode
and target.

Many references disassemble to a single 5-byte `jmp`. In those cases the debug
XBE has a forwarding entry and the real body lives at the jump target:

```
0017c920  jmp 0x155a70    rasterizer_frame_end   -> _rasterizer_frame_end   (ported)
00184bb0  jmp 0x18b000    render_dispose         -> FUN_0018B000            (ported)
0008e370  jmp 0x1d0581    system_milliseconds    -> (CRT, not in kb.json)
001ed620  jmp 0x1efd80    D3DResource_BlockUntilNotBusy -> (not in kb.json)
001edc10  jmp 0x1f4140    D3DTexture_GetLevelDesc -> Get2DSurfaceDesc
```

The 14 low rows need attribution review. Several place a target body or wrapper
in the forwarding entry's kb.json slot, so the scorer compares that code against
one `jmp`:

| Function | Ours | Ref | Score |
|---|---|---|---|
| `D3DResource_BlockUntilNotBusy` | 57 insns | 1 | 0.0% |
| `render_initialize` | 17 | 1 | 0.0% |
| `j__render_dispose_from_old_map` | 10 | 1 | 0.0% |
| `render_initialize_for_new_map` | 5 | 1 | 0.0% |
| `D3DTexture_GetLevelDesc` | 4 | 1 | 40.0% |
| `rasterizer_frame_end`, `rasterizer_window_end`, `rasterizer_windows_begin`, `rasterizer_windows_end`, `render_dispose`, `scenario_dispose_from_old_map`, `system_milliseconds` | 2 | 1 | 0.0% |
| `system_exit` | 2 | 1 | 66.7% |
| `thunk_FUN_0015b960` | 1 | 1 | 0.0% (mnemonic differs: `ret` vs `jmp`) |

**Why it scores 0%:** `SequenceMatcher` over mnemonic sequences finds no common
block between a body and a lone `jmp`, so the ratio is exactly zero.

**What it means for correctness:** the score does not compare equivalent entry
shapes. That warrants attribution review before source edits, not a correctness
claim. `thunk_FUN_0015b960` is a clear shape mismatch: our implementation
returns where the original jumps.

**Fix shape:** verify reference instruction, jump target, kb.json entry, and
whether forwarding semantics must be preserved. Then score forwarding entry as
a thunk and target as a body, or report both. Source work is premature until
that audit is complete.

## Incomplete Implementations

| Function | Ours | Ref | Score |
|---|---|---|---|
| `collision_log_render` | 1 insn | 465 | 0.4% |
| `FUN_00180d10` | 1 insn | 242 | 0.8% |

These scores measure incomplete implementations rather than subtle codegen
drift. They are not the same kind of omission. `collision_log_render` is a
ported debug-only stub whose guard is lifted and body is documented as omitted:

```c
void collision_log_render(void)
{
  if (*(char *)0x4761d0 == '\0')
    return;
  /* Debug rendering body omitted — runs only in debug mode */
}
```

`FUN_00180d10` is different: it is a vertex-buffer compression routine, its C
body is empty, and its kb.json entry remains `ported: false`. It is not a debug
display path. Original behavior still runs, so the low score is not a current
patched-runtime defect, but porting it is functional work whose reachability and
verification requirements must be assessed.

## Metric Issue — Greedy Anchor Sensitivity

This is the largest single distortion in the table and the one most likely to
send someone chasing a phantom.

The official percentage is `SequenceMatcher.ratio()` over mnemonic-only
sequences. It is **not** an LCS: its greedy longest-block anchor can pair the
candidate's first assert block against the reference's last once both use an
identical idiom, and everything after that mis-anchors. `vc71_verify` also
records an exact dynamic-programming mnemonic LCS (`scores.dp_lcs_pct`) which
does not have this greedy-anchor failure mode. Neither metric compares operands,
immediate values, branch targets, memory offsets, or side effects, so DP-LCS is
an anchor-resistant alignment signal, not a correctness score.

**68 functions have `dp_lcs_pct` more than 10pp above `official_pct`, 53 of them
in the sub-70% band, totalling 1,126pp of metric disagreement.** The classifier
uses a stricter `>12pp` threshold, which fires on 48 functions overall and 40
below 70%. The 10pp population is used for census reporting; the 12pp population
is used in rule tables below.

| Function | Official | DP mnemonic LCS | Gap | TU |
|---|---|---|---|---|
| `rasterizer_text_cache_character` | 17.6% | 58.4% | +40.8 | rasterizer_text.c |
| `FUN_000cb230` | 32.1% | 70.2% | +38.1 | hs_runtime.c |
| `FUN_001800b0` | 37.1% | 74.5% | +37.4 | rasterizer_text.c |
| `hs_get_thread_script_name` | 33.3% | 63.9% | +30.6 | hs_runtime.c |
| `FUN_00077ff0` | 55.5% | 85.1% | +29.6 | bitmap_utilities.c |
| `FUN_000b1760` | 50.2% | 77.1% | +26.9 | game_engine.c |
| `weapon_start_effect` | 59.3% | 84.7% | +25.4 | weapons.c |
| `FUN_000c7b10` | 47.4% | 71.8% | +24.4 | hs_compile.c |
| `sound_start` | 53.5% | 77.5% | +24.0 | sound_manager.c |
| `input_state_process_packet` | 60.4% | 81.2% | +20.8 | input_xbox.c |

`rasterizer_text_cache_character` is the clearest aligner case: 240 candidate
instructions against 236 reference instructions, but official score is 17.6%
while DP mnemonic LCS is 58.4%. This proves severe anchor sensitivity. It does
not prove source correctness: operand-normalized similarity is only 25.6%, and
the context carries two load-width warnings.

The trigger is repeated near-identical idioms, which in this codebase means
assert sites. Functions with many `display_assert` calls that share a shape are
the ones that collapse. It self-corrects once enough surrounding regions
converge — the actor_look_update precedent went 76.6% → 9.8% → 80.4% while the
true LCS only ever rose.

**Practical rule:** never read an official-score cliff as a regression on its
own. Track official score, DP mnemonic LCS, operand-normalized score, warnings,
instruction count, and behavioral evidence together. For `@<reg>` functions,
the official score may strip candidate-only phantom parameter loads while the
stored DP score uses raw mnemonic sequences.

## Overlapping Diagnostic Triggers

The score-context classifier fires each rule independently. Counts are
diagnostic prevalence, not a partition and not proof that a trigger caused the
score gap.

Sub-70% band (221 functions):

| Rule | Count |
|---|---|
| `frame_mismatch` | 42 |
| `anchor_collapse` | 40 |
| `fcom_bound_sense` | 40 |
| `loadw_field_width` | 29 |
| `imm_wrong_literal` | 19 |
| `fpu_operand_order` | 15 |
| `regarg_structural_ceiling` | 6 |

70–89% band (1,202 functions):

| Rule | Count | Share of band |
|---|---|---|
| `frame_mismatch` | 213 | 18% |
| `fcom_bound_sense` | 161 | 13% |
| `fpu_operand_order` | 83 | 7% |
| `loadw_field_width` | 72 | 6% |
| `regarg_structural_ceiling` | 40 | 3% |
| `imm_wrong_literal` | 24 | 2% |
| `anchor_collapse` | 8 | 1% |

Coverage matters: among 221 sub-70 functions, only 115 receive any rule, 106
receive no rule, and 51 receive more than one. The table contains 191 total rule
hits. Even after excluding one-instruction references, two incomplete
implementations, and 53 functions with a >10pp metric gap, 89 of 152 remaining
functions hit none of the five warning/frame rules.

`frame_mismatch` is the most frequent trigger. It means detected candidate and
reference stack reservations differ; it does not prove why functions diverge.
Frame-shape levers can still be productive: FUN_001ac680 went 46.1% → 97.3% on
shape changes. Recipes are in `.claude/skills/lift-score-improve/SKILL.md`.

`fcom_bound_sense` at 13–18% is the one to treat carefully rather than
mechanically: the compare direction is load-bearing for NaN behaviour, so
flipping `<=` to `<` to match TEST/Jcc bits can improve the score while breaking
the unordered path. The FCOM-parity bug that dropped every biped through the
world came from exactly this shape.

## `@<reg>` Structural-Ceiling Candidates

`regarg_structural_ceiling` fires on 58 current score-context packs joined to
the floor: 6 below 70%, 40 in the 70–89% band, and 12 at 90% or better. The
often-quoted count of 46 is the below-90% population, not the full population.
The rule detects at least three aligned `push`↔`mov` replacements and labels
them as likely register-argument call sites; it does not inspect callee ABI.
Treat it as a candidate diagnosis requiring call-site verification.

Two distinct mechanisms may hide under the rule and have different prognoses:

- **Late first use of an own register param** — recoverable. Forcing an early
  register-load hint took FUN_001a1a10 from 80% to 91.5%.
- **Calling `@<reg>`-arg callees** — potentially a verifier-lane ceiling. The VC71
  verify lane cannot pass arguments in `eax`/`esi`/`edi`/`ebx`, so each such
  call site costs about one `pushl`-vs-`movl` mismatch. On a function with ~20
  such sites that is a ~6pp floor.

`xbox_texture_cache_setup_d3d_texture` (80.5%, worked in detail this session) is
a manually investigated example of register-parameter frame overhead, but its
current automated classification is `imm_wrong_literal`, not
`regarg_structural_ceiling`. It should not validate that detector by itself.
Both its parameters are `@<reg>`, so the original has **no EBP frame at all**
(`push ebx … pop ebx; ret`) while our build must emit a full frame plus
`[ebp+N]` param reads. The extra register pressure then forces `texture` to be
reloaded from `[ebp+0xc]` in the else branch where the original just keeps it in
EDI. 132 candidate instructions against 116 — and about 12 of those 16 extra
instructions are frame overhead the lane imposes.

That function also demonstrates a **non-cause worth documenting**, so nobody
re-derives it: VC71 distributes the shift-accumulate pack chains, turning
`((p-1)<<12 | (h-1))<<12` into `(p<<24 - 0x1000000) | (h<<12 - 0x1000)` and
merging two `desc <<= 4` steps into one `shl 8`. Both transforms are
unconditionally valid and cost the *same instruction count* as the reference's
sequential `dec`/`shl`/`or` form, so which one VC71 emits is a
scheduling-driven canonicalization. Three distinct source forms — the sequential
chain, the flat absolute-shift form (`<<24`/`<<12`/`<<0`), and pre-decremented
`height`/`width` locals — compile to byte-identical code. No source lever was
found in that experiment; this is not general proof that every function carrying
the rule is source-unrecoverable.

## Hotspot TUs

TUs with the most sub-80% functions. `framediff` counts functions whose
candidate and reference frame sizes disagree.

| TU | sub-80 | framediff | mean | min | dominant rules |
|---|---|---|---|---|---|
| `game/game_engine.c` | 52 | 8 | 70.3 | 50.2 | frame_mismatch 8, anchor_collapse 6, fcom 4 |
| `units/units.c` | 29 | 7 | 74.5 | 58.1 | frame_mismatch 7, fcom 6, loadw 3 |
| `networking/network_server_manager.c` | 21 | — | 73.0 | 61.6 | — |
| `rasterizer/rasterizer_text.c` | 19 | 10 | 62.9 | 0.8 | frame_mismatch 10, anchor_collapse 4 |
| `structures/structures.c` | 17 | — | 65.7 | 7.1 | — |
| `interface/hud_messaging.c` | 17 | — | 72.1 | 63.5 | — |
| `math/real_math.c` | 15 | 4 | 67.7 | 42.4 | fcom 5, fpu_operand_order 4, frame 4 |
| `ai/actor_moving.c` | 14 | — | 67.6 | 50.0 | — |
| `effects/effects.c` | 12 | — | 68.5 | 37.3 | — |
| `hs/hs_runtime.c` | 12 | — | 64.4 | 32.1 | — |
| `interface/hud.c` | 7 | — | 53.8 | 29.6 | — |
| `effects/decals.c` | 7 | — | 56.2 | 0.0 | — |

`game_engine.c` is the largest concentration of remaining score work by a factor
of two. `rasterizer_text.c` has the worst mean of any large TU (62.9) and the
highest frame-mismatch density (10 of 19), which makes it the best candidate for
a frame-shape campaign — and two of its functions are top-3 anchor-collapse
cases, so its true state is better than 62.9 suggests.

## Best real targets

Functions under 60% whose instruction count is within 25% of the reference, and
which are longer than 80 instructions — i.e. structurally close enough that the
gap is shape, not logic:

| Function | Official | DP mnemonic LCS | Ours/Ref | Rules | TU |
|---|---|---|---|---|---|
| `rasterizer_text_cache_character` | 17.6% | 58.4% | 240/236 | loadw, anchor_collapse | rasterizer_text.c |
| `FUN_000cb230` | 32.1% | 70.2% | 467/542 | anchor_collapse | hs_runtime.c |
| `FUN_001800b0` | 37.1% | 74.5% | 356/355 | anchor_collapse | rasterizer_text.c |
| `fast_vector_intersects_sphere` | 42.4% | 57.6% | 96/102 | fcom, frame, anchor | real_math.c |
| `FUN_0018b990` | 45.0% | 63.4% | 224/252 | imm, fcom, frame, anchor | scenario.c |
| `FUN_000b1760` | 50.2% | 77.1% | 139/136 | loadw, frame, anchor | game_engine.c |
| `XapiFormatFATVolume` | 52.5% | 64.3% | 317/311 | loadw, frame | xcontent.c |
| `sound_start` | 53.5% | 77.5% | 307/310 | fpu, loadw, frame, anchor | sound_manager.c |
| `FUN_0002b830` | 53.7% | 67.0% | 226/210 | fpu, fcom, frame, anchor | actor_moving.c |
| `FUN_00077ff0` | 55.5% | 85.1% | 374/376 | loadw, anchor_collapse | bitmap_utilities.c |
| `FUN_0009dcf0` | 56.0% | 63.3% | 449/369 | fcom, frame | effects.c |

Many carry `anchor_collapse`. For `FUN_00077ff0` (85.1% DP mnemonic LCS) and
`FUN_001800b0` (74.5%), official score likely understates mnemonic alignment.
That makes them useful aligner/scorer test cases, but source proximity still
requires operand diagnostics and behavioral evidence.

## Summary

Of the 221 functions below 70%:

- **14** have one-instruction references and score below 70%. They need
  target-by-target reference and forwarding-entry attribution review.
- **2** are incomplete C implementations: one ported debug-only omission and
  one inactive vertex-compression stub.
- **53** have DP mnemonic LCS more than 10pp above official score, up to 40.8pp.
  This establishes aligner sensitivity, not correctness.
- Diagnostic rules cover **115** low-score functions, leave **106** unclassified,
  and overlap on **51**. Frame shape is the most frequent trigger, followed by
  FPU compare sense, field load width, immediates, and FPU operand order.
- **58** current contexts carry the heuristic `@<reg>` ceiling rule; **46** of
  those score below 90%. Call-site and ABI evidence is required before declaring
  any individual gap unrecoverable.

The two widest tooling opportunities are not source edits: report an
anchor-resistant mnemonic metric alongside official score, and resolve
forwarding-entry references explicitly. Neither change validates a lift; both
make subsequent source and behavioral investigation better targeted.
