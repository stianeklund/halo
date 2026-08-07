# FUN_000d8ff0 Score-Recovery Notes

`FUN_000d8ff0` (`src/halo/interface/hud_weapon.c`) draws one weapon HUD's
crosshair hierarchy. It accepts the weapon-HUD tag index in `EAX`, the player
pointer in `ECX`, and the weapon handle plus state buffer on the stack.

## Baseline / progress

- 2026-08-07: **57.4% → 59.1% official** (operand 50.3% → 52.7%), 676→672
  candidate insns. Lever: the two 16-element hierarchy-array clears
  (`level_hud[1..15]`, `level_idx[1..15]`).
- Reference frame `sub esp, 0x318`; candidate `sub esp, 0x30c` (12-byte / 3-dword
  gap). No FPU / load-width / immediate / FCOM / ABI diagnostics.
- Official-score gap vs DP-LCS (72.3%) is partly SequenceMatcher anchor
  collapse; DP-LCS is the more useful progress signal.

## Corrected diagnosis (supersedes the old "explicit loops" premise)

The previous note claimed *"the original uses explicit loops to clear the two
16-element hierarchy arrays"* and rejected loop variants. That premise was
**wrong**. The delinked reference clears each array with an inline
`mov ecx,0xf; lea edi,[&arr[1]]; rep stos` (0x5f5 and 0x608) — i.e. an MSVC
`memset()` intrinsic, not a loop. The old lift used `csmemset()`, which is a
real kb function (0x8db80) and therefore emits a `CALL`, not `rep stos`. That
call-vs-`rep stos` mismatch was the recoverable defect.

Fix: clear the arrays with `memset()` (MSVC 7.1 /Oi inlines it to `rep stos`).
Because clang has no `memset` symbol to link and the game's own `memset` is
`csmemset`, the calls are guarded so the shipped clang build stays on
`csmemset` (behaviorally identical zero-fill):

```c
#if defined(_MSC_VER) && !defined(__clang__)
  memset(&level_hud[1], 0, 15 * sizeof(int *));   /* VC71 -> rep stos, matches ref */
#else
  csmemset(&level_hud[1], 0, 15 * sizeof(int *)); /* clang build: real call, links */
#endif
```

(`extern void *__cdecl memset(...)` declared at the top of the TU, mirroring
`structure_detail_objects.c` / `main.c`.) Also removed a dead `LAB_time_check:`
label (unreferenced; blocked the clang `-Werror` build; byte-neutral).

## Rejected trials

- **Loop variants for the array clears** (indexed `do`/`for`, pointer/count) —
  regressed ~0.1pp. Superseded: the reference is `rep stos`, so `memset` is the
  correct form, not a loop. Do not retry loops.
- **Rect-clear `memset`** (line ~400: `rect[0]=4; memset((char*)rect+2,0,0x22)`).
  The reference rect clear *is* `rep stos` (0x6a9 `mov ecx,8; rep stos; stos
  word`), and `memset` reproduces it — operand-normalized rose to 53.0% — but
  **official dropped to 57.1%** (below baseline) via anchor collapse: the rect
  region re-anchored and mis-paired neighbours. Reverted because the gate uses
  official. Revisit only if the anchor stops collapsing after other regions
  converge.

## Remaining structural gap

After the array-clear fix, the residual mismatch is stack/register allocation:
the 12-byte frame gap and `player_obj` kept EDI-resident in the reference but
spilled in the candidate. These are not tied to a known incorrect operation.
Start future work from the recorded baseline and score-context pack:

```sh
rtk python3 tools/verify/score_improve.py baseline \
  --source src/halo/interface/hud_weapon.c \
  --output artifacts/score_improve/FUN_000d8ff0-baseline.json
rtk jq '{scores, frame, classification, warnings}' \
  artifacts/score_context/FUN_000d8ff0.json
```
