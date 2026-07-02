---
name: a30-objd4-funcval-corruption-static-exhaustion
description: a30 render_lights NaN crash — obj+0xd4 funcval = 0xFFFFFFFF on 1 parented object; static hunt exhausted, no source-visible object-0xd4 writer, mechanism = NONE-handle copied into float slot; decisive test is live object type + toggle-bisect
metadata:
  type: project
---

# a30 render_lights NaN crash: obj+0xd4 = -1 static forensics (2026-07-02)

Crash: `assert_valid_real_rgb_color(-1.#QNAN0 x3)` bitmap_utilities.c:2397 in
FUN_0007c270 <- 0x7c490 <- render_lights FUN_0013b380 (0x13b380, ported). blend
NaN = `*(float*)(parent_obj+0xe4+fn_idx*4)` = obj+0xec = 0xFFFFFFFF. Root anomaly
= obj+0xd4 = 0xFFFFFFFF (input funcval slot 0); obj+0xec is compute reading it.
1-of-1023 objects; object parented (obj+0xcc=0xe5ff000b) + 2 attached lights.

## Confirmed byte-faithful / dormant (NOT the bug)
- biped_export 0x1a1080, unit_export 0x1a8010, projectile_export 0xf7ec0 — all
  ported=true, decompiled originals BYTE-FAITHFUL to our C. All default 0.0 and
  SKIP-leave-stale on type/selector 0. Store is inside the `!=0` guard in disasm
  too. biped CLAMPS (NaN cmp false -> writes 0.0, cannot emit -1). unit cases
  1/2/4/5 are UNCLAMPED verbatim copies of unit+0x298/0x2e8/0x2ec/0x2f0.
- object_compute_function_values 0x13e7b0 DORMANT. Original reads input via
  `puVar4[sVar2+0x34]` = obj+0xd0+sVar2*4 (sVar2=1 -> obj+0xd4), multiplies into
  output `puVar4[local_c+0x39]` = obj+0xe4+local_c*4. So obj+0xd4 NaN -> obj+0xec
  NaN by fld/fmul/fstp (0xFFFFFFFF qNaN round-trips intact). ONE root: obj+0xd4.
- object_new 0x143c80, object_header_new 0x13ded0, attachments_new 0x13ecb0,
  object_compute_node_matrices 0x141b70, object_choose_random_*/compute_change_colors
  ALL DORMANT (ported=false) -> creation runs ORIGINAL, zeroes region to 0.0.
- object_new hierarchy init writes -1 to obj+0xa0/0xbc/0xc4/0xc8/0xcc/0xac/0xb0/
  0x120 but NOT 0xd0/0xd4 (funcvals separate). object_detach_from_parent 0x1411c0
  writes byte obj+0xd0=-1 + dword obj+0xcc=-1 only (faithful). object_wake/reset_markers
  operate on light/global pools not obj-data. FUN_001ab8c0 copies parent +0x290/+0x294
  (lighting) not funcvals.

## Key negative results (decisive, from PATCHED artifact)
- PATCHED build = halo-patched/default.xbe. .text.patch VMA 0x643000 raw 0x33d000.
  Symbolize: EXE_VMA = xbe - 0x642000 + 0x400000; build/halo is STRIPPED but has
  PE export table (zip Export Address Table RVAs with Name Pointer Table names).
- Capstone scan of .text.patch for stores to disp 0xc4..0xf0: EVERY -1/int store
  is on the ACTOR pool (actor_clear_flee_target obj+0xd0, actor_clear_aim_target
  +0xdc, FUN_0003c410 actor_new indexed `[esi+edi+0xc4]` stride 0x657c, etc) —
  actor datum, NOT object-data. disp==0xd4 hits are ALL float ops (transform/gaussian
  scratch structs on non-object bases). NO int store to object-data 0xd4 via any
  literal displacement in our build.
- NO `rep movsd`/`movsd` block copies anywhere in .text.patch (clang doesn't emit
  them here). No object-region csmemset with -1 (all -1 memsets are effect datum
  0x5c/actor/global pools).
- render_lights + all 3 exporters PRESENT in patched export table; vehicle/device/
  weapon exporters ABSENT (unported, run original).

## Conclusion (mechanism proven, culprit needs live inputs)
obj+0xd4 = 0xFFFFFFFF is a NONE-handle bit pattern reaching a FLOAT slot by COPY
(fld/fstp preserves the qNaN) or a walked-pointer (disp=0) store — NOT an x87
arithmetic NaN and NOT a literal-disp int store. All source-visible object-0xd4
writers are faithful or dormant. The corruptor is either a ported per-tick
function copying a NONE handle into a unit source field that unit_export reads
unclamped, or collateral from an unrelated ported per-tick write via a base
pointer the disp-scan can't attribute.

## Decisive tests (require live object / xemu — out of static scope)
1. Object TYPE / tag-group at obj+0 (tag idx 0x012c runtime-only, Ghidra-on-cachebeta
   can't resolve). biped/unit -> exporter runs (faithful) -> hunt source-field
   corruptor. scenery/device/weapon/vehicle -> exporter UNPORTED -> -1 is pure
   collateral, pivot away from funcvals.
2. Live: read slot-0 tag type code. 0 = slot skipped (stale/block-copy path).
   nonzero = exporter wrote it (source-field path).
3. Toggle-bisect ported candidates: unit_update 0x1b3690 (per-tick parented units,
   three float[13] matrix bufs into unported callees, prior em-aggregate aliasing
   bug) is #1 suspect.

## WEAPON-type update (2026-07-02, coordinator confirmed obj+0x64=2=WEAPON)
- Weapon func-value exporter = FUN_000fbf00 (UNPORTED, ported=null; runs ORIGINAL).
  Referenced only from vtable DATA 0x323f5c (weapon type-dispatch). Decompiled faithful.
- STRUCTURE: walks parent chain `while(weapon+4 bit0 && weapon+0xcc!=-1) climb`;
  writes to ROOT `puVar5+0x35` (root obj+0xd4). SOURCES ALWAYS read from puVar3=WEAPON
  (not root): case1=weapon+0x1ec, 0xd=+0x1f0, 0xe=+0x1f8, 4=+0x220, 5=+0x244,
  7=+0x224, 8=+0x248 (verbatim (float)dword copies -> only these can carry 0xFFFFFFFF).
  Arithmetic cases (2/3,9,0xa/b/0xc) can't produce exact all-ones.
- CLIMB BIFURCATION: weapon writes its OWN obj+0xd4 only if bit0 CLEAR (doesn't climb).
  If bit0 SET, exporter writes MARINE's obj+0xd4, weapon's stays stale -> weapon obj+0xd4
  =-1 would be FOREIGN/collateral, source-field path DEAD.
- render_lights reads light+0x2c = light's parent = the WEAPON (2 lights attached to weapon)
  -> weapon obj+0xec -> weapon obj+0xd4.
- NEGATIVE: NO ported fn writes -1 to any weapon direct-copy source field. Patched-build
  capstone: only -1 stores to {0x1dc,0x278} are FUN_0003c410=actor_new (actor pool). Weapon
  source fields (0x1ec etc) written by UNPORTED weapon/item update. unit_update sec19
  (per-weapon integrated light) writes ONLY unit+0x2f8, not weapon fields (faithful).
- => Source-field-corruption path has NO ported culprit. Direct-collateral reinstated
  UNLESS a live read shows a corruptible weapon source field is -1.
- LIVE READS NEEDED (coordinator holds state): (1) weapon+4 low byte bit0 = climb?;
  (2) weapon_tag+0x330 int16[0] = slot-0 type code -> case -> offset; (3) that source
  field hex = 0xFFFFFFFF?

## Method notes
- rtk rg/grep MANGLES alternation regex (`+ 0xfc`, `|`) — the hook rewrites even
  `command grep`. Use python3 heredoc for all multi-pattern source searches.
- Ghidra byte-search: disp 0xd4 > 0x7f needs disp32 (`C7 80 D4 00 00 00 ...`),
  NOT disp8 (`C7 40 D4` = [reg-0x2c]). First attempts were vacuous for this reason.
