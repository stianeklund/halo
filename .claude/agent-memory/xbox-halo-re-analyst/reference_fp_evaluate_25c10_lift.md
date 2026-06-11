---
name: fp-evaluate-25c10-lift
description: FUN_00025c10 firing-position evaluator final lift facts — actr-tag vs encounter base corrections, scorer-table ABI, 6-arg path_state_estimated_distance, 87.1% landed; equiv inconclusive (oracle relocs + uninit fill)
metadata:
  type: reference
---

FUN_00025c10 (actor_firing_position evaluate/select) landed at 87.1% VC71
(d01852a4) after a full disasm audit corrected 14 issues in the earlier 74%
draft. The decompile has "type propagation not settling" — every PUSH is
modeled as a local_NN write, so base-pointer attributions from the decompiler
are unreliable; only `[EBP-0xNN]` slots in raw disasm are trustworthy.

Binary-confirmed facts (kb.json now reflects these):
- `[EBP-0x28]` = ACTR TAG pointer, not the encounter element. The prop-scan
  gate flags (+0 sign bit, +4 bit0), the seed weights (+0x3c8 first-seed,
  +0x3cc group-scan), the path_input_new arg2 (+0x8c), and the attractor
  gate (+4 & 0x10) all read the actr tag. The stash draft misattributed all
  of these to encounter_atoms.
- Scorer table at 0x254bf8 (8-byte entries, mask word at -4): entries are
  called `fn(actor_handle, ctx, rec_count, records)` — the broken push-slot
  model reads this REVERSED.
- `path_state_estimated_distance` (0x5f550) is 6-arg cdecl (ADD ESP,0x18 at
  all 3 sites): (path_state, pos, pos[0x14], out_dist, opt_min_clearance,
  opt_out_vec). Main loop passes (huge_buf, pos, pos[0x14], rec+0x8,
  rec+0x1c, ctx[0x40]?rec+0xc:NULL). kb.json had a 4-arg decl; the already-
  ported actor_has_accessible_firing_position was fixed to pass NULL,NULL
  and to load arg3 as a raw int (`*(int *)(fp_elem + 5)`, not
  `(int)fp_elem[5]` which emits a float->int conversion).
- `unit_estimate_position` in the scripted-target branch takes
  estimate_mode=1 (PUSH 0x1 at 0x25e34); args 4/5 are genuine NULLs (the
  hazard duplicate-args WARN there is a verified false positive).
- Discard test in the select loop is `<=` (TEST AH,0x41; JP):
  rec[0x38]+ctx[0x660] <= best.
- fp-eval debug name table: {fight, panic, cover, uncover, guard, pursue}.
- Record init zeroes rec+0x2c, +0x34 AND +0x38; the local path-eval buffer
  is 0x1408c at EBP-0x1c91c and is DISTINCT from the huge_buf param.

VC71 idioms that moved the score 77.3 -> 87.1 on this function:
- memcpy(dst, src, N*4) -> REP MOVSD (matches original debug-mirror copies).
- `*(vector3_t *)dst = *(vector3_t *)src` -> the LEA-pair 3-dword copy
  groups the original emits everywhere.
- Direct-indexed stores (`records + ri + ofs` with `ri = (short)n * 0x3c`)
  instead of a cached `rec` pointer for the record-init block.
- Re-reading a ctx counter per field group (no cached element pointer)
  reproduces the original's recomputed IMUL/LEA addressing.
- `<= 3` / `<= 5` instead of `< 4` / `< 6` to get JG instead of JGE.
- Physical block order: emit the fall-through branch first (scripted-target
  before guard); FPU axis ladders restructured to `>` primitives with the
  use-branch first (else-if chain).

Equivalence is inconclusive for this whole family: the oracle crashes on
unresolved data relocs on most seeds, and remaining divergences trace to
uninitialized-fill differences (0xcc oracle stack vs zeroed candidate)
flowing through the STUBBED owner-indices initializer — not lift behavior.
Trust VC71 + disasm audit here.

Note: a __chkstk runtime stub now exists (src/halo/cseries/xbox_crt.c,
lift-learnings section 20), so large frames use plain stack buffers under
both compilers — the old `#if _MSC_VER / static` split is obsolete.
