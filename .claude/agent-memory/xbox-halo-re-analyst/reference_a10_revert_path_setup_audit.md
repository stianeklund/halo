---
name: a10-revert-path-setup-audit
description: 2026-06-28 SETUP/RESTORE-path audit driven by the NEW revert clue (fresh-load WORKS patched, death->checkpoint REVERT BREAKS patched, unpatched always works). Maps the game_state_revert machinery (0x1bf8a0 FAITHFUL, 13 after-load callbacks, none in ai.obj), proves the AI/actor/perception state is restored by UNPORTED deserialize 0x1c0450, and reframes the bug as INPUT-DEPENDENT divergence in a ported per-actor activation feeder (actor_activate 0x3ec80 chain). actor_activate body + gate FUN_0003d9f0 handle-handling = FAITHFUL+salt-safe; remaining suspects = ported sensory/input feeders FUN_0003dc20 / FUN_0003bb50 / FUN_0003be90 / FUN_0003e7a0.
metadata:
  type: reference
---

CONFIRMED vs cachebeta.xbe disasm (XBE base 0x10000; .text VA 0x12000 raw 0x2000; full VAs).
Supersedes the per-tick-only framing for the a10 FIRING residual. The ADVANCE bug was the
override-branch FUN_0005e0d0 arg fix (fc2b77d5, settled). This is the FIRING residual.

## THE NEW CLUE (user, 2026-06-28, primary evidence)
Fresh first-checkpoint load (patched) => grunts fire (WORKS). death->checkpoint REVERT (patched)
=> a6 passive, ZERO perception props, aw(+0x268)=0 (BROKEN). Unpatched always works incl. revert.
A patched-SAVED checkpoint loaded UNPATCHED also works. => save/serialize direction is fine and the
blob is faithful (clue 4 refutes any ported-side serialization-layout divergence). => the divergence
is a ported function that runs on the LOAD/RESTORE-or-after path and behaves differently for
RESTORED actor state than for FRESH-SPAWNED state. NOT "exposed latent per-tick bug" (faithful code
has no latent bug); it is INPUT-DEPENDENT divergence in a ported feeder.

## REVERT MACHINERY (saved_games/game_state.c) -- all dispatch FAITHFUL
- game_revert (death/checkpoint, in-session autosave) and console core_load both funnel through the
  SAME machinery: `(*0x32eaa4)()` [dispose cb] -> deserialize -> 13 after-load callbacks `0x32eaa8[]`.
- game_state_revert 0x1bf8a0 (ported): disasm VERIFIED faithful. Guard
  `[0x4ea9a5]==0 && [0x5054e8]==0 -> jmp 0x1002a0(restart)`; else call[0x32eaa4]; call 0x1c0450
  (deserialize, UNPORTED); loop esi=0x32eaa8 edi=13 {call[esi]; esi+=4; dec edi} -- EXACTLY 13, not
  inverted, right table. game_state_call_after_load_procs 0x1bf790 identical 13-loop. NO dispatcher bug.
- deserialize 0x1c0450 = UNPORTED (heap blob restore, flat). Runs identical both builds.

## THE 13 AFTER-LOAD CALLBACKS @0x32eaa8 (revert restore-fixup) -- NONE in ai.obj
[0]0x18ecd0 scenario.obj UNP ; [1]0x1cd540 sound_dsound_xbox UNP ; [2]0x1c7160 game_sound UNP ;
[3]0x8a480 observer PORTED ; [4]0xb9880 decals UNP ; [5]0x17ca90 decals UNP ;
[6]0x94c70 recorded_animations UNP ; [7]0x4c0f0 ai_debug_initialize_for_new_map PORTED ;
[8]0x1939e0 objects PORTED?(clears 1 byte [*0x4d8ea0+0x520e]=0, no actor touch) ;
[9]0x1bfbd0 cache_files UNP ; [10]0xba970 players UNP ; [11]0x877e0 director UNP ;
[12]0xd5120 hud_messaging PORTED.
dispose cb @0x32eaa4 = 0x1c70b0 game_sound UNP (iterates a datum pool to dispose sound).
=> NONE of the after-load procs nor the dispose cb writes the actor pool / AI globals / encounter
pool / perception props. The ONLY ai-domain after-load proc is ai_debug (display only).

## KEY STRUCTURAL FACT: ai.obj init/create runs ONLY on FRESH LOAD, never on revert
ai_initialize_for_new_map 0x41090 and ai_place 0x3f760 (->encounters_create_for_new_map) are called
ONLY from game_initialize_for_new_map (game.c:603/613 = New Game / fresh map). NOT from
game_state_revert / game_state_load_core. ai_reconnect_to_structure_bsp 0x40f80 has NO revert caller.
FUN_00040570 (deferred actor-create) is per-tick (ai_update ai.c:1103), not revert.
=> on REVERT, the entire AI/actor/encounter/perception state comes verbatim from the blob (unported
restore); the only ported AI code that runs is the PER-TICK chain (already proven faithful) PLUS the
per-actor ACTIVATION chain that fires when actor+0x6a>0.

## RECONCILES the ai.obj bisect "contradiction"
The 2026-06-26 single-object bisect that fingered ai.obj used artifacts/ai_regression/*.bin HEAP
CAPTURES (core/console-style), which PREDATE the 2026-06-27 faithful-repro discovery (boot-init doc
S4: degenerate core/console repro shows passive in BOTH builds). So the ai.obj verdict is repro-
contaminated; DOWNWEIGHT the ai.obj constraint. Follow the revert clue's own logic.

## THE REAL SUSPECT (per-actor activation feeder) -- INPUT-DEPENDENT divergence
actor_activate FUN_0003ec80 (actors.obj, ported, @<esi> actor_handle) = full per-actor AI init.
Sole ported caller = ai.obj 0x3f5f0 (per-tick sweep; gate actor+0x6a>0). Its exoneration came from
the SAME contaminated bisect => VOID. It runs per-tick on the revert path and was never disasm-
audited like the ai.obj 35.
- actor_activate BODY: disasm VERIFIED faithful (datum_get x2, 0x2c8728=handle, debug-focus clear,
  gate FUN_0003d9f0, then 0x3bb50/0x3dc20/0x355f0/0x303f0/0x32cb0, reload, memset actor+0x3e8 0x84,
  set 0x418/0x42c/0x42e=0xffff, 0x3be90/actor_action_update, actor+0x13 dormant gate, actor+0x6 swarm
  gate->0x3a8a0, then 0x3bbf0/...8 cdecl.../0x3e7a0). Handle used uniformly via datum_get (salt-safe).
- GATE FUN_0003d9f0 (actors.obj, ported): biped-flag + deactivation block disasm VERIFIED faithful:
  `biped=(actor+0x34!=-1)?datum_get(*0x5ab270,actor+0x34):0; flags=actor[0xa]; if(biped)flags|=biped[0xc];
   if(actor+0x12==0 || flags!=0) set_dormant`. All handle resolves use datum_get (salt-safe). The
  encounter-validity (actor+0x270 datum_get *0x5ab23c) + in-vehicle (actor+0x470) also datum_get.
  No raw index/salt mis-compare found in the audited blocks.
- REMAINING UN-AUDITED ported feeders (the smoking-gun surface; large/FPU-heavy, need dedicated lift
  audit each): FUN_0003dc20 actor_input_update (populates actor+0x120..0x1c4 sensory/threat/aim/look
  vectors -- EXACTLY the perception-chain inputs; reads biped via object_get_and_verify_type(actor+0x18),
  BSP via FUN_0018f3e0->actor+0x15d, vehicle handles, facing/aiming/looking/up/right vectors);
  FUN_0003bb50 (@eax pre-init); FUN_0003be90; FUN_0003e7a0 (@eax post-init). Any of these mis-handling a
  RESTORED biped/object handle or a restored BSP/cluster value -> wrong perception INPUT -> unported
  0x33440/0x416e0 correctly computes prop+0x30=0 -> never promotes -> aw=0 -> exact footprint.

## AUDIT PROGRESS (2026-06-28, asymmetry filter: only HANDLE/index-salt resolution or blob-carried
## field that diverges patched-vs-unpatched AND fresh-vs-restored survives; build-symmetric logic +
## ALL FPU vector math are EXCLUDED -- they'd break fresh-load too)
- actor_activate 0x3ec80 BODY: VERIFIED FAITHFUL (handle uniform via datum_get).
- gate FUN_0003d9f0 biped-flag+deactivation block: VERIFIED FAITHFUL, salt-safe (datum_get).
- FUN_0003dc20 actor_input_update NORMAL-path handle resolves (object_get_and_verify_type(actor+0x18,3)
  @0x3ddb6; biped+0xcc parent; object_get_and_verify_type(parent,-1) @0x3dddb; FUN_0003bde0 call args
  (actor_handle,actor+0x18,actor+0x120) @0x3ddf2): VERIFIED FAITHFUL, all salt-validating accessors.
  (vehicle path has an index-vs-handle `actor+0x34 & 0xffff` mask but it's the vehicle-seat branch,
   NOT the on-foot grunt path -- a10 door grunts are on-foot, excluded.)
- FUN_0003bde0 actor_fill_unit_input_block (0x6a bytes, fills input_block actor+0x120: world pos+0xc,
  obj+0x24 vel->+0x18, head pos+0, root loc+0x2c, root obj+0x48/0x4c->+0x24/0x28): VERIFIED FAITHFUL
  full instruction-level; all handle resolves via object_get_and_verify_type / object_get_root_parent.
  NO index-vs-handle, NO register aliasing.

## ALL 3 REMAINING FEEDERS AUDITED 2026-06-28 = FAITHFUL (handle-only lens)
- FUN_0003bb50 (0x3bb50, 0x82 bytes): VERIFIED FAITHFUL. Only handle use = datum_get (salt-safe).
  Branches on actor+0x6c/0xa0 (action/sub-action) + score actor+0x4a vs ai_globals+4/+6 (SIGNED
  cmp dx faithful). NO handle-derived branch.
- FUN_0003be90 (0x3be90, 0xa6 bytes): VERIFIED FAITHFUL. datum_get once (salt-safe). Decision loop on
  actor+0x6c/0x70/0x4 (state). `actor+0x34 & 0xffff` used as tag_block ENCOUNTER INDEX only on the
  ERROR/assert path (deliberate index extraction, not salt confusion). Ring index edi in separate var.
- FUN_0003e7a0 (0x3e7a0, 0x302 bytes): VERIFIED FAITHFUL. datum_get + object_get_and_verify_type
  (actor+0x18,3) salt-safe; all unit_* calls pass full handle actor+0x18; gates are sentinel checks
  (unit+0x1c8!=-1, actor+7, actor+0x6ec!=0xffff). Body = control-struct build (FPU/vector, excluded).

## LINCHPIN RESOLVED: REVERT RESTORES DATUMS VERBATIM -- NO SALT REMAP (decisive)
game_state_malloc (0x1bfbf0) carves ALL game-state allocations LINEARLY from one contiguous region
(base_address + cpu_allocation_size bump-pointer). actor_data pool, prop_data pool, AI globals
(0x632574 = game_state_malloc("ai globals",0,0x8dc)), perception prop pool ALL live inside this single
0x345000-byte region. save = 0x1c0570(name,*0x4ea994,0x345000) flat block; restore =
0x1c0680(name,*0x4ea994,0x345000) flat block. => restore is a VERBATIM memcpy of the whole game-state
heap; datum salts/indices/handles are restored EXACTLY as saved. NO datum re-alloc, NO salt bump. =>
"handle-sensitive branch differs post-revert" is REFUTED: a restored actor has the IDENTICAL salt/index
it had pre-save, so handle-derived branches evaluate the SAME as during live gameplay (when grunts WERE
firing).

## BOTTOM LINE (2026-06-28): NO LIFT DEFECT on the revert/restore/activation/per-tick path.
Entire surface FAITHFUL: revert dispatch 0x1bf8a0, 13 after-load callbacks + dispose cb (none AI),
deserialize UNPORTED, ai.obj init/create fresh-load-only, actor_activate 0x3ec80 + gate 0x3d9f0 + all 5
ported feeders (0x3dc20/0x3bde0/0x3bb50/0x3be90/0x3e7a0), per-tick chain (prior). Salts verbatim => no
handle-asymmetry mechanism. The ONLY evidence OUR code is the culprit is the 2026-06-26 ai.obj
single-object toggle-bisect, whose repro used heap-capture .bin (core/console) -- boot-init S4 says that
repro is passive in BOTH builds. THE ONE REMAINING ACTION: re-confirm/refute that bisect via the
FAITHFUL repro (NewGame->play->die->REVERT, live byte-classify ai.obj redirects with
verify_toggles_live.py). If it does NOT reproduce on faithful revert, the symptom is faithful/stochastic
(matches reference_a10_grunt_aim_faithful_no_defect) and there is NO ported defect to fix here.

## (superseded) MECHANISM B note retained:
2. MECHANISM B IS LOGICALLY EXCLUDED (advisor): a fresh-load-only init that revert skips cannot cause a
   patched-only-on-revert asymmetry -- it RAN successfully on the preceding fresh-load (door works fresh
   patched) and its out-of-blob result persists through death->revert unchanged. So encounters_init /
   ai_comm_init / FUN_53650/5dfa0/64150/540d0 are CLEARED by logic; do NOT cross-ref them.
3. CAVEAT to re-confirm cheaply if feeders all clear: is the ai.obj single-object toggle-bisect (the
   sole evidence putting the bug in OUR code at all) reproducible via the FAITHFUL repro
   (NewGame->play->die->REVERT)? Its original verdict used heap-capture .bin (core/console) which
   boot-init S4 says is passive in BOTH builds. If the bisect does NOT reproduce on faithful revert,
   the "ported code is the culprit" premise itself is in question.
4. Live (only after static names a field): hold door scene (aw>=1), read suspect on a6 actor slot6 +
   PARENT prop (orphan+0xc), patched vs unpatched, AFTER a death->checkpoint REVERT (faithful repro,
   not core_load). Signal = prop+0x30 on the PARENT prop (proven chokepoint), never firing rate.
