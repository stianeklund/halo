# Handover — bipeds.obj Dormant Activation Attempt (2026-06-10)

## Objective

Fully port all remaining 10 `ported:false` functions from bipeds.obj to ≥88% MSVC or
≥90% genuine equivalence.

## Current State

- **0 new activations.** All 10 remaining functions hit genuine structural walls.
- Session work is committed to main (ec124a20). Worktree is clean.
- 4 per-function delinked COFF refs exported and registered in `objdiff.json`.
- `artifacts/snapshot_FUN_001a2290_actor_veto.json` built — complete pre-work for
  the in-engine dual-oracle on 0x1a2290 (the strongest activation candidate).
- xemu sanity gate PASSED (0x31fc38=0x0028caa8, magic=0x64407440) but game is in
  attract/menu state: 1 non-biped object, 0 actors. No AI bipeds available.

### The 10 dormant functions

| addr | decl | VC71 | block |
|------|------|------|-------|
| 0x1a0b30 | `char FUN_001a0b30(int@<edi>)` | 81.4% | reg-arg; product (object_delete) via stubbed FUN_00140cc0 |
| 0x1a1a10 | `int FUN_001a1a10(float,float*,void*,float*@<eax>,int@<edi>)` | 80.0% | reg-arg keystone; collision-HIT path needs non-stubbed BSP |
| 0x1a1b90 | `int biped_approximate_surface_index(int,float*)` | 78.6% | cdecl; pure thunk → unported 0x1a1a10 |
| 0x1a1e70 | `void FUN_001a1e70(int unit_handle)` | 86.9% | cdecl; `jp/je` FPU idiom + @<edi> keystone; confirmed via compare_obj |
| 0x1a2160 | `void FUN_001a2160(int@<eax>)` | 73.4% | reg-arg structural |
| 0x1a2290 | `char FUN_001a2290(int@<edi>)` | 84.4% | reg-arg; 5 exact velocity writes on main path; actor-veto branch now driven in unicorn but actor_aim_jump still stubbed |
| 0x1a25e0 | `void FUN_001a25e0(int@<ecx>)` | 79.3% | reg-arg structural |
| 0x1a2a60 | `void FUN_001a2a60(int@<edi>,char*)` | 86.5% | reg-arg; pure @<edi> prologue artifacts, no body diff |
| 0x1a2b10 | `void FUN_001a2b10(int@<edi>)` | 84.0% | reg-arg structural; proven ceiling (byte-identical experiments) |
| 0x1a2f40 | `void FUN_001a2f40(void*@<esi>)` | 75.9% | reg-arg; re-lifted 19→76%, capped by x87 scheduling |

## Confirmed

- **VC71 ceilings are structural.** compare_obj on 0x1a1e70 shows `jp` (VC71,
  TEST AH,0x41+parity) vs `je` (orig, TEST AH,0x41+zero) — semantically identical for
  ordered floats, different machine bytes — plus @<edi> keystone call to FUN_001a1a10
  (3–4 extra insns). compare_obj on 0x1a2a60 shows 100% @<edi> prologue artifacts,
  zero body-level differences. Permuter was tried (188 non-vacuous iters, +0.15%)
  post-PYCPARSER fix — score is 86.9%, still below 88%.
- **FUN_001a2290 main path is product-equivalent.** unicorn_diff with combat snapshot
  (BIPED_REAL_TAGS=1, BIPED_HEAP_COMPARE=1, BIPED_SIBLING_RESOLVE=1): 50/50 seeds,
  **5 EXACT writes** (velocity obj+0x18/0x1c/0x20, obj+0x430, obj+0x45c), obj+0x424
  disjoint proven benign (dword vs byte width artifact, same final value). 69% coverage.
- **FUN_001a2290 actor-veto branch IS reached** by actor-veto snapshot (biped idx=538
  from Flood encounter, actor=0xe36b0001, gates open): 50/50 pass, 51.7% coverage.
  BUT actor_aim_jump (FUN_0002ace0, ported=true) is stubbed in unicorn → returns 0 →
  function exits early → **stub-constant 0x0, zero heap writes**. Does NOT constitute
  genuine equivalence (advisor confirmed ×3).
- **Unicorn alone is insufficient for reg-arg activation.** Project reviewer held
  0x1a0e00 at NEEDS_RUNTIME even after all unicorn legs were driven; bar is in-engine
  dual-oracle. This applies to all reg-arg functions including 0x1a2290.
- **xemu sanity gate:** 0x31fc38=0x0028caa8 (≈0x0028xxxx ✓), object_table[0x28]=
  0x64407440 ✓. XBDM reachable via `tools/xbox/xdbm_screenshot.py --png`.
- **Per-function delinked refs** for 0x1a0b30, 0x1a1e70, 0x1a2290, 0x1a2a60 exported
  and confirmed: scores match vc71_scores.json entries.

## Important Changes

- `delinked/biped_FUN_001a{0b30,1e70,2290,2a60}.obj` — per-function COFF refs; needed
  by vc71_verify and compare_obj for future sessions (force-added, `*.obj` is gitignored).
- `objdiff.json` — 4 new units registered (`halo/units/biped_FUN_001a*`); both
  vc71_verify and unicorn_diff now resolve these targets by address.
- `artifacts/snapshot_FUN_001a2290_actor_veto.json` (10 MB) — hybrid snapshot: Flood
  encounter gamestate (has biped idx=538 with actor=0xe36b0001 at offset 0x1a8, all
  gates open) + biped_capture heap/tag data + 0x632000 actor_data pointer page. This is
  the ready-made snapshot for the 0x1a2290 in-engine dual-oracle session.
- `tools/equivalence/leaf_cache.json` — updated: 0x1a0b30 (100% cov, weak) and
  0x1a2290 (69% cov, weak) reflecting current equivalence results.

## Validation

- `python3 tools/build/build.py -q --target halo` → exit 0 (build clean)
- `python3 tools/verify/vc71_verify.py src/halo/units/bipeds.c --function FUN_001a1e70`
  → PASS 86.9% (96/95 insns, confirmed with new per-fn ref)
- `python3 tools/verify/vc71_verify.py ... --function FUN_001a2290` → PASS 84.4%
- `python3 tools/verify/vc71_verify.py ... --function FUN_001a2a60` → PASS 86.5%
- compare_obj on 0x1a1e70: confirmed FPU idiom differences + @<edi> keystone
- compare_obj on 0x1a2a60: confirmed 100% structural/prologue residuals
- unicorn_diff on 0x1a0b30 (tweaked snapshot, kill-Z): 50/50, 100% cov, 0 val_diffs
- unicorn_diff on 0x1a2290 (combat snapshot, actor==-1): 50/50, 69% cov, 5 exact writes
- unicorn_diff on 0x1a2290 (actor-veto snapshot, actor=0xe36b0001): 50/50, 51.7% cov,
  actor_aim_jump stub-constant → NOT activation evidence
- NOT run: any in-engine dual-oracle (requires live AI biped with actor)

## Uncertain / Risks

- The actor-veto snapshot uses a **hybrid** of two different captures (Flood encounter
  gamestate + biped_capture heap). The biped tag data in the heap may not match the
  Flood encounter's bipd tag. If the tag pointer chain differs, BIPED_REAL_TAGS=1 may
  read wrong tag fields. This only matters if actor_aim_jump is de-stubbed; for the
  current purpose (confirming the branch is reached) it's fine.
- 0x1a1e70 gate 6 (`obj+0x459 > 0x1e`) reads 0 for all bipeds in all tested maps
  (the timer setter is not yet lifted). Activating 0x1a1e70 would be safe but inert —
  the function always early-exits in the live build. This is a pre-existing known issue,
  not introduced this session.
- 0x1a2f40 (75.9%) stays dormant. A store bug was fixed in an earlier session; current
  source is the banked re-lift.

## Next Steps

1. **Get xemu into a live combat state** with AI bipeds. Load any singleplayer campaign
   level (e.g. Truth and Reconciliation) and let enemies spawn. Verify with:
   `python3 -c "import socket,struct,time; s=socket.socket(); s.connect(('127.0.0.1',731)); s.recv(256); [s.sendall(f'getmem addr=0x{a:08x} length=0x4\r\n'.encode()) or time.sleep(0.1) for a in [0x5a8d50, 0x6325a4]]; ..."` — actor_count should be > 0.
2. **Build in-engine dual-oracle for 0x1a2290** using `@<edi>` asm thunk (per
   0x1a0e00 precedent in `artifacts/dualoracle/0e00_inengine_match.txt`). Target biped
   is Flood encounter idx=538 (handle=0xe4ce021a, actor=0xe36b0001) — or any live biped
   with `actor_handle_1 (obj+0x1a8) != -1`. Needs two legs: veto (actor returns 0) and
   non-veto (actor returns 1).
3. **If 2290 passes both legs bitwise-identical** → `ported:true` in kb.json → build +
   deploy + smoke test (run `tools/xbox/verify_toggles_live.py` to confirm patch active).
4. 0x1a0b30 and other reg-arg functions: same dual-oracle pattern, lower priority.

## Resume Prompt

> The 10 dormant (`ported:false`) bipeds.obj functions are in main (@ec124a20) as
> inert dead code. See `artifacts/handover_bipeds_dormant_2026-06-10.md` for the
> per-function table. VC71 ceilings confirmed structural (no source fix possible).
> FUN_001a2290 (84.4% VC71) has 5 exact velocity writes proven on main path; the
> actor-veto branch snapshot is built at `artifacts/snapshot_FUN_001a2290_actor_veto.json`
> (biped idx=538, actor=0xe36b0001, all gates open). To activate: load xemu into a
> live combat state (AI bipeds with actors), then build an `@<edi>` asm thunk and
> in-engine dual-oracle for 0x1a2290 per the 0x1a0e00/0x1a0680 precedent. XBDM is
> reachable (`tools/xbox/xdbm_screenshot.py --png` works). Sanity gate fields:
> 0x31fc38=0x0028caa8, magic=0x64407440 (last verified 2026-06-10).
