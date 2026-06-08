# Handover — Improve the 12 dormant (ported=false) bipeds.obj functions

## Objective
Raise the accuracy / VC71 match of the 12 below-bar bipeds.obj functions that
landed in `main` as `ported=false` (inert), so they can eventually be activated.

## Current State
- The bipeds.obj lift is **in main** (`ddabe71d`, FF'd 2026-06-08). 27 functions
  are active (≥88% VC71 or equiv); **12 are dormant (`ported=false`)** — the
  C bodies exist in `src/halo/units/bipeds.c` but the **original machine code
  runs** (patch.py JMP tail-call). Improving them changes nothing at runtime
  until you flip `ported=true`.
- All 12 are in `src/halo/units/bipeds.c`. A delinked reference exists at
  `delinked/bipeds.obj` (except 0x1a0680 needs an exact-range re-export — see below).

## Confirmed (kb.json, queried this session)
The 12 dormant functions, with declarations and register-arg annotations:

| addr | name / decl | calling conv | VC71* |
|------|-------------|--------------|-------|
| 0x1a0680 | `char FUN_001a0680(int unit_handle)` | **cdecl** | 61.5 raw / ~82 real |
| 0x1a0b30 | `char FUN_001a0b30(int unit_handle@<edi>)` | fastcall @edi | 81.4 |
| 0x1a0e00 | `void FUN_001a0e00(float threshold, int unit_handle@<eax>)` | reg @eax | 82.3 |
| 0x1a1a10 | `int FUN_001a1a10(float scale, float *out_point, void *out_vec, float *direction@<eax>, int unit_handle@<edi>)` | reg @eax+@edi | 80.0 |
| 0x1a1b90 | `int biped_approximate_surface_index(int unit_handle, float *out_point)` | **cdecl** | 78.6 |
| 0x1a1e70 | `void FUN_001a1e70(int unit_handle)` | **cdecl** | 86.9 |
| 0x1a2160 | `void FUN_001a2160(int unit_handle@<eax>)` | reg @eax | 73.4 |
| 0x1a2290 | `char FUN_001a2290(int unit_handle@<edi>)` | fastcall @edi | 84.4 |
| 0x1a25e0 | `void FUN_001a25e0(int unit_handle@<ecx>)` | fastcall @ecx | 79.3 |
| 0x1a2a60 | `void FUN_001a2a60(int unit_handle@<edi>, char *state)` | fastcall @edi | 86.5 |
| 0x1a2b10 | `void FUN_001a2b10(int unit_handle@<edi>)` | fastcall @edi | 84.0 |
| 0x1a2f40 | `void FUN_001a2f40(void *physics@<esi>)` | reg @esi | 19.3 |

\* VC71 scores were recorded at integration time (prior session). `vc71_scores.json`
was observed **stale** — re-run `vc71_verify.py` per function before trusting a number.

## Three categories (this drives strategy)

**A. cdecl — genuinely improvable by VC71 (0x1a0680, 0x1a1b90, 0x1a1e70).**
No frameless-fastcall preamble penalty. These can plausibly reach ≥88% byte-match.
- `0x1a0680` is a **measurement artifact, not a lift bug.** The raw 61.5% comes
  from delink trailing-NOP padding inflating the reference (documented pattern:
  243-byte fn at offset 0x4b0, next symbol at 0x5f0 → ~77 phantom padding bytes →
  reference 91→152 insns → score tanks; real LCS match ~82%). **First action:**
  export an exact-range per-function ref and re-score with `compare_obj.py` — its
  real number may already clear the bar.
- `0x1a1e70` (86.9) is in the **permuter band [85,98]** → `/verify permute` candidate.
- `0x1a1b90` (78.6) is below the permuter floor → investigate the lift (control
  flow / FPU operand order) before permuting.

**B. register-arg — VC71 structurally capped (the other 8).**
`@edi/@eax/@ecx/@esi` callees are **frameless fastcall** in MSVC (no `push ebp`),
while our cdecl-shaped C emits a frame → permanent preamble mismatch (~80–87% ceiling).
Chasing VC71 here is mostly futile. The right evidence is **equivalence**:
- Several already had 100/100 equiv at integration time (per commit messages).
- These could be **activated via the equivalence clause** (the way `biped_fix_position`
  /0x1a1430 was) if you accept that bar — re-run `/verify equivalence` to confirm,
  then flip `ported=true`. The true in-engine dual-oracle (call original by address
  vs verbatim candidate) is the strongest proof but needs a per-fn **asm thunk** to
  place the arg in-register (cdecl C cast can't) — see memory note below.

**C. real defect — needs a proper re-lift (0x1a2f40).**
19.3% is **not** a cap. Two known problems: (1) a buffer-alias store bug — the
front half **wrote `new_velocity` where it should write `proj`** (wrong [EBP-N]
identity); (2) the back half was marked `unknown` (partial faithful front-half
only). Decode the `@<esi>` + stack args, prove each `[EBP±N]` identity against
disassembly, and complete the back half before re-scoring.

## Important Changes
- None this session beyond the FF into main. This handover is investigation-planning
  only; no lift edits were made.

## Validation
- `rtk jq` confirmed the 12 dormant decls + reg-args above (this session).
- `main` is at `ddabe71d` with all 12 present as `ported=false` (confirmed this session).
- NOT run this session: any `vc71_verify.py`, permuter, or equivalence re-runs.
  Treat the score column as prior-session data needing re-verification.

## Uncertain / Risks
- The category-B VC71 ceilings are **inferred** from the `@<reg>` annotations +
  the documented frameless-fastcall pattern, not re-measured this session.
- Activating any dormant function flips runtime behavior. Several are **register-arg
  callees of already-active functions** — that's why they keep bodies (clean
  `ported=false` tail-call; an empty `@<reg>` thunk would infinite-recurse). When
  you activate one, build + smoke-test the active callers.
- Per-function delinked refs may need re-export for accurate scoring (0x1a0680
  definitely; check others if a score looks artifact-like).

## Next Steps
1. **0x1a0680 first** — cheapest win. Export exact-range ref:
   `mcp__ghidra-live__export_delinked_object selection_mode=range range=<start>-<end>`,
   then `compare_obj.py build/vc71/bipeds.obj <ref>.obj -f FUN_001a0680`. If ≥88% → activate.
2. **0x1a1e70 (86.9)** — `rtk python3 tools/verify/vc71_verify.py src/halo/units/bipeds.c`
   to confirm, then `/verify permute` (band [85,98]).
3. **0x1a1b90 (78.6)** — re-decompile, diff against current C, fix structure/FPU before permuting.
4. **Category B (8 reg-arg fns)** — `rtk python3 tools/equivalence/unicorn_diff.py <fn>
   --seeds 100 --allow-stubs --float-tolerance 32`; if high-confidence equiv, decide
   whether to activate under the equivalence clause (same precedent as 0x1a1430).
5. **0x1a2f40** — fix the `proj` vs `new_velocity` store bug + complete the back half
   (full re-lift, not permuter).

## Resume Prompt
> Work in a worktree off `main` (@ddabe71d). Goal: improve/activate the 12 dormant
> (`ported=false`) bipeds.obj functions in `src/halo/units/bipeds.c`. Read
> `docs/handover-bipeds-dormant.md` for the per-function plan. Start with 0x1a0680
> (suspected delink-padding artifact — re-export exact-range ref + compare_obj
> before assuming a lift bug), then permute 0x1a1e70, then pursue equivalence for
> the register-arg group. Use `/lift` and `/verify` per CLAUDE.md; never lower the
> ≥88% VC71 bar — register-arg functions activate on equivalence evidence instead.
> Relevant memories: feedback_vc71_mismatch_delink_check (0680 artifact + fastcall
> cap), feedback_true_dual_oracle_by_address (reg-arg equivalence path),
> feedback_aliasing_verify_buffer_identity (2f40 wrong-store bug),
> feedback_vc71_structural_ceiling.
