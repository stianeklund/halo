---
name: biped-equivalence-live-snapshot
description: How to build live-state snapshots + the gated real-tag_get stub for biped equivalence (FUN_001a06xx-22xx); object/tag/game_globals chain + scene limitations
metadata:
  type: reference
---

Equivalence evidence for VC71-capped biped functions (FUN_001a0680/0b30/0e00/1a10/2290)
needs LIVE game state, not zero-fill. Captured via raw QMP `memsave` (VIRTUAL) on
127.0.0.1:4444 — `pmemsave`/XBDM `getmem` read wrong/zero on this xemu. Helper:
`tools/equivalence/_biped_capture.py` (worktree).

## Data-dependency chain (decompile-derived, not reloc-discoverable)
- `object_get_and_verify_type(h,t)` → `datum_get(*0x5a8d50, h)`; entry = arr+idx*0xc,
  obj ptr at entry+8, class at obj+0x64 (biped=0). The harness stub already replicates
  this against snapshot 0x5a8d50 — so the **0x5a8000 page is MANDATORY** in the snapshot
  (without it object_get returns 0 → every function runs against a null object →
  ALL conclusions invalid; symptom: `[obj_get] FAILED handle=... -> 0`).
- `tag_get(group, *obj)` → `FUN_001b9bf0`: base=*0x5054f0; entry=base+(handle&0xffff)*0x20;
  body=*(entry+0x14). Group sigs at entry+0/+4/+8 (e.g. bipd/unit/obje).
- `game_globals_get()` → *0x5064d4; physics block at +0x170, collision at +0x188.
- Required global pages: 0x5a8000 (obj tbl ptr), 0x505000 (tag instances ptr),
  0x4e4000 (tags_loaded flag, must be 1), 0x4e5000 (tag count struct), 0x506000
  (game_globals ptr), 0x2b4000 (kill-Z 0x2b4d1c), 0x5aa000 (flags). Heap: tag table
  inside gamestate 0x800b0000+0x310000; tag bodies + game_globals in 0x80900000+0x200000.

## The pivotal stub fact
`tag_get`'s default stub returns a SYNTHETIC PROJECTILE tag (0x601000, tuned for
FUN_000f9c40) — it zeros the biped tag fields (node count, jump speed, phase
boundaries) that gate the meat of 0680/0e00/2290. Both oracle+candidate use the same
stub so the comparison stays valid, but it's a COVERAGE blocker. Fix: gated real-tag
resolution added to `stubs.py` execute_stub, behind env `BIPED_REAL_TAGS=1` (additive,
falls through to synthetic when unset → projectile path bit-identical; proven via
git-stash A/B on FUN_00038e60). Run biped targets with `BIPED_REAL_TAGS=1`.

## Gate-tweak method (concolic branch coverage, file-based)
`artifacts/biped_capture/tweak_snapshot.py` resolves obj_ptr (datum chain:
arr=*0x5A8D50; data=*(arr+0x34); elem=*(arr+0x22); obj=*(data+(h&0xffff)*elem+8) =
0x800bf3f8 for handle 0xe26f0000) and patches gate bytes at obj+offset inside the
containing region (0x800b0000+0xf3f8). Both sides read the same tweaked snapshot.
- **0680** (BLOCKED-vacuous): tweak obj+0x47c step 20→5 (< max 0x47d=20) runs the
  antr node loop (20 nodes @0x80938ecc). REALIZABLE (mid-transition). But candidate
  CRASHES on intra-obj sibling FUN_001a03c0 (eip=0x1ffffc) until BIPED_SIBLING_RESOLVE=1
  (below); even resolved, the node buffer is written by oracle@harness-base 0x500000 vs
  lifted@real 0x4e49f0 → DISJOINT keys → value_diffs vacuous, AND source is stubbed
  garbage (FUN_0013dfc0 → 0xcccccccc both sides). Body executes (77%) but output NOT
  witnessed. Not bar-met.
- **0b30** (BAR-MET, control-flow+return): tweak obj+0x14 f32 →-3000.0 AND obj+0x4c
  s16 →-1 (the OR sub-condition; "unattached biped fallen past kill-Z -2000",
  realizable). a8e30 is STUBBED→returns 0 so cond A passes (0x456b60 unmapped is moot).
  100% cov, 50/50, 0 value-diffs, 0 trace-diffs. Erase runs in stubbed FUN_00140cc0
  (side-effect not witnessed; return always 0 → "weak" is a return-diversity artifact).
- **2290** (PARTIAL): held scene ALREADY has gate open (obj+0x424=0x20 bit0 clear,
  obj+0x460=0) — NO tweak needed. The memory's old "oracle inlines 0f10" claim was WRONG:
  delinked oracle CALLs FUN_001a0f10 at d40 (intra-obj, resolved via oracle whole-.text);
  candidate single-fn extraction leaves the defined-sibling call unpatched → eip=0x0.
  Fix = BIPED_SIBLING_RESOLVE=1 (below). Then 69% cov, 50/50, 0 value-diffs, 0f10 loaded
  symmetrically from delinked for both sides. Real product (velocity → obj+0x18/1c/20,
  heap ≥0x80M) NOT traced → control-flow+return char only, not full bar.

## BIPED_SIBLING_RESOLVE=1 (intra-obj sibling wall fix — worktree harness)
Root cause of 0680/2290/1430/etc candidate crashes: top-level lifted `patch_rel32_calls`
(unicorn_diff.py ~L1418) was called WITHOUT `include_defined=True`, so a candidate call to
a DEFINED intra-obj sibling (defined in bipeds.c.obj) stays an unpatched rel32 → wild fetch
(eip=0x1ffffc or 0x0). Oracle resolves siblings via oracle_text whole-.text. Added gated
`include_defined=os.environ.get("BIPED_SIBLING_RESOLVE")=="1"`. With --real-callees,
`_load_callee_code` loads the sibling from DELINKED for BOTH sides (same bytes) → symmetric,
valid, lift-isolating. Additive (off → identical to before; proven: 0e00 79.4%, 0b30 100%
unchanged). VALIDITY GATE: only trust a green run if (a) "real callees: N loaded" includes
the sibling, (b) 0 value_diffs, (c) traces symmetric. Disjoint-output funcs (0680) stay
vacuous regardless.

## Sibling-call partition (who is testable)
Clean (no intra-obj sibling on work-path): 0e00 ✓, 0b30 ✓, 2160 (already 100%).
1a10 = partial-on-path (only collision-MISS path, return -1; hit needs non-stubbed
collision_bsp_test_vector). Sibling-walled: 0680→03c0, 2290→0f10, 1430→0890, 1b90→1a10
(pure thunk, vacuous), 1e70→1a10+74d0, 25e0→0890, 2a60/2b10→0f10+UNCONDITIONAL 2800.
0e00 + 2290 write ONLY heap (≥0x80M) → side-effect never trace-witnessed (dual-oracle lane).
NOTE: 25e0/1b90/1e70/2a60/2b10 are UNPORTED (no candidate symbol) → not equiv-testable at
all; only 1430 (biped_fix_position) has a dormant 19.3% lift (oracle crashes in 26-call
graph). 25e0 also hits the jump-table wall eip=0xff86.

## Mem-trace gotcha (advisor-caught)
`compare_mem_traces` keys on (addr,size); `value_diffs` is empty when addresses are
DISJOINT — vacuous. Oracle writes globals to harness base 0x500000+slot; lifted writes
the same globals to real snapshot-mapped .data (e.g. collision_user_depth 0x4761d8,
stack 0x5a8c80). Must PAIR by logical identity and compare VALUES. For 1a10 both =0x1/0x7
and eax=-1 → benign globals-base mapping artifact (distinct from same-addr-ULP). Heap
(0x80xxxxxx) writes are NOT traced at all — so 2290 side-effects are inferred from
control-flow, not measured.

Snapshots: artifacts/snapshot_FUN_001a0*_live.json; assembler _assemble_biped_snaps.py;
mem-trace probes _probe_memtrace.py / _probe_sideeffects.py (all worktree, gitignored).

## 2026-06-07: heap-output WITNESS unlock — 0e00 + 2290 are PRODUCT-EQUIVALENT
The "heap writes NOT traced" blocker above is RESOLVED. `unicorn_diff.py` write hook
dropped all `address >= 0x20000000` writes (FXSAVE region guard), so object-body
outputs (~0x800bxxxx) were never compared. Added gated `BIPED_HEAP_COMPARE=1`: a heap
write is now WITNESSED iff it lands inside a snapshot-mapped `_override_ranges` interval
(shared by oracle+candidate -> SAME address). FXSAVE @0x20000000 is never in a snapshot
region -> excluded for free. Additive (flag-off byte-identical; proven on FUN_00038e60
AND 0e00). `compare_mem_traces` upgraded: field-aware (float-ULP on non-exact 4-byte
writes via float_tolerance_ulp, exact on bytes/ints) + surfaces affirmative `matched`/
`matched_within_tol` lists (was diff-only = vacuous). State module: state.py TraceDiff.
The pivotal same-address fact: object-body writes resolve via the SHARED 0x5a8d50 ->
identical addr both sides (witnessable). Global/static scratch (oracle harness 0x500000
vs candidate real .data) stays DISJOINT by construction -> never witnessable here. So
verdict is per-function by WHERE output lands.

Combat capture (main repo /mnt/g/dev/halo/artifacts/combat_capture/: gamestate.bin
@0x800b0000, heap_809_0..7 @0x80900000+i*0x40000, g_*.bin globals). 468 objects, 68
class==0 bipeds. Assembler: tools/equivalence/_assemble_combat_snaps.py ->
artifacts/snapshot_FUN_001a0*_combat.json. Handle 0xe26f0000 (idx0, obj 0x800bf3f8).
- **0e00 PRODUCT-EQUIVALENT** (50/50, 79.4% high): 3 EXACT object writes obj+0x428=0x00,
  +0x429=0x07, +0x460(2B)=0x0000. Zero disjoint/diffs. No sibling on work-path. Gate met.
- **2290 PRODUCT-EQUIVALENT** (50/50, 69%): 5 EXACT incl VELOCITY VECTOR obj+0x18/1c/20
  (f32 0.00136531/0.000887362/0.07), +0x430=0xffffffff, +0x45c(1B)=0x00. One "DISJOINT"
  at obj+0x424 is BENIGN store-width artifact: oracle dword 0x00000021 vs candidate byte
  0x21; pre-existing snapshot upper bytes (+0x425/6/7)=00 00 00 -> both final = 0x00000021
  (verified). Same with/without --real-callees (6 callees incl 0f10). NOT a bug.
- **0680/0b30/1a10 CONTROL-FLOW-ONLY** (outputs unwitnessable, NOT bugs): 0680 = NO combat
  biped satisfies step<max gate (12 bipeds @20/20, 56 @0/0; 0 with cur<max) so node loop
  never enters; even tweaked open, node buffer is disjoint (0x500000 vs 0x4e49f0) + source
  FUN_0013dfc0 stubbed garbage. 0b30 = 28.5% early-exit, erase in stubbed callee, ret char.
  1a10 = ret -1 (0xffffffff) all seeds = collision-MISS path; out_point/out_vec only on
  HIT path gated behind stubbed collision_bsp_test_vector.
NOTE: no matched_within_tol fired -> all 8 object writes were BIT-EXACT (stronger than
tol); the ULP path is validated additive but was inert on these states, not exercised.
