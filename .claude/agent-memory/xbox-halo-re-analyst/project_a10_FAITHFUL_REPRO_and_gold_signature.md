# a10 grunt-passive: FAITHFUL REPRO + GOLD SIGNATURE (2026-06-27)

## DETERMINISTIC EXIT-TRACE (2026-06-27) — g_cdb0_trace global; FIRST msrc=3 = LAB_fail(transient)
Instrumented cdb0 exits into g_cdb0_trace[64] (struct{u8 hit;u8 exit;s8 ret;s16 msrc;f32 distsq}, stride 12).
Address NOT in stripped PE symbols — extract from LIVE impl @0x649b50: CDB0_TRACE writes are absolute-addressed
(msrc@base+4, distsq@base+8); base was 0x775900 then 0x775804 after latch rebuild — SCAN 0x7758xx for hit∈{1,2}/
exit∈1-4/msrc∈-2..8/distsq<1e5 pattern. exit codes 1=early-return 2=LAB_fail 3=LAB_path_ok 4=repath.
LATCHED (hit=2) FIRST msrc=3 per door grunt => ALL THREE exit=2 LAB_fail ret=0 (dist 2.0/1.7/0.5).
BUT: LAB_fail path is FAITHFUL — our LAB_fail (FUN_0002a3a0+return 0) == original 2ce1/LAB_0002d081 (xor bl,bl;
FUN_0002a3a0; ret bl). check_dest on-foot LAB_fails iff (f498==0 && facing(0x494)==-1). Live settled door grunts:
facing=403(NOT -1), f498=0, onfoot=0 => facing was -1 at FIRST msrc=3 (LAB_fail) then became 403 (case-3 resolved a
different order as selector cycled fp 0x470). So FIRST-msrc=3 LAB_fail is a TRANSIENT both builds hit; latch caught the
WRONG attempt. NEED the STEADY-STATE msrc=3 exit (facing=403, check_dest PASSES) — reach test(LAB_path_ok=3) or repath(4).
Re-instrument: latch LAST msrc=3 (overwrite msrc=3 with msrc=3, never with msrc 0/1) + ADD facing+f498 to trace. OR
dual-oracle-by-address (snapshot 1 door-grunt facing=403/msrc=3, call orig vs our cdb0, diff). LESSONS: disasm-faithful
misled ~7x; sampled-patt misled; even deterministic-latch needs the RIGHT attempt. cdb0 IS the bug (toggle) but the exact
divergence is still unpinned. noinline kept (faithful). instrumentation kept (debug, REMOVE before commit).

## ⚠️ NOINLINE FIX FAILED (2026-06-27) — inlined-test was NOT the differentiator; diagnosis incomplete
Applied __declspec(noinline) on actor_test_destination, built+deployed, VERIFIED running on box (live impl @0x649b50
has fixed clang prologue 5589e55357..., inlined fsub[esi+0x12c] tolerance math GONE, cdb0 now CALLs _actor_test_destination).
BUT behavior UNCHANGED: fixed-build first-contact a10/a11 msrc=[1,3], m3.patt=[0], m3.atd=[1], m3.pact=[0] — IDENTICAL
to broken. So calling the standalone test (vs inlining) did NOT fix it. The standalone test_destination (src 96-115:
returns actor[0x484]; sets it=1 if move_src 0/1 OR tol²>|0x488-0x12c|²) returns at-dest in OUR cdb0 but NOT-at-dest in
WORKING(original cdb0 calls same standalone). SAME fn, SAME standalone => INPUT differs OR non-test cdb0 diff.
PROBLEMS with prior diagnosis: (1) patt/atd SAMPLING has timing ambiguity (msrc at sample-time may not match move_src
cdb0 used; atd persists from prior tick). (2) BASELINE FLAW: compared broken FIRST-CONTACT dest vs working SETTLED dest
(grunts at firing pos) — apples-to-oranges. Working first-contact dest may be FARTHER (grunt advances, selector reassigns
closer, settles at 0.1). Never captured working FIRST-CONTACT dest/patt/pact. (3) toggle reverts WHOLE cdb0 => the
differentiator could be ANY part, patt=0/atd=1 is a SYMPTOM. NEXT (advisor instrumentation fallback): write sentinel byte
to scratch global at each cdb0 decision point (early-return / LAB_path_ok / repath-entered / LAB_fail / pathfinder-ret0),
deploy, read after engagement => DETERMINISTIC branch, no sampling ambiguity. noinline fix kept (faithful to original which
calls, low harm) but is NOT the fix. cdb0 still = our lift (broken). Box on fixed-but-still-broken build.

## ★ ROOT CAUSE (SUPERSEDED — see NOINLINE FIX FAILED above): inlined actor_test_destination ★
First-contact patt capture (broken build, /tmp/am_dest.py firstcontact): a10 AND a11 reached msrc=3, and on those
ticks patt=0 (repath NEVER reached) + atd=1 (test said AT-DEST) + pact=0. => (a) THE TEST. Mechanism: door grunt at
dist ~2.0 from CORRECT firing pos, selector issues msrc=3, cdb0's INLINED actor_test_destination wrongly returns
at-dest(1) => cdb0 takes LAB_path_ok early-exit (return 1, NO side effects, patt stays 0) => pathfinding repath never
runs => pact never set => grunt "thinks it arrived" => sits in guard. ORIGINAL cdb0 CALLS standalone FUN_0002a580
(proven correct: working build advanced grunts to dist 0.09-0.79). Our clang cdb0 INLINED it (same TU actor_moving.c)
and MISCOMPILED the inline (standalone 0x2a580 VC71=95.9% FPU-WARN; inlined FPU stack scheduling differs). FIX:
__attribute__((noinline)) on actor_test_destination => cdb0 CALLs standalone (=original, correct). Disasm-diff was
USELESS here (inlined copy looked byte-faithful ~6x); the patt runtime discriminator settled it. EXPECTED after fix:
door grunts advance dist->~0.1, pact=1, aw->10 (gold signature).


## THE REPRO WAS BROKEN 3 WAYS (this is why we went in circles for days)
1. **Re-saved core.bin is degenerate.** The doorway advance is scenario-SCRIPT/trigger driven;
   a manually re-saved core (saved "just before spawn") does NOT restore the script-thread state.
   Loading it => grunts passive in BOTH patched AND unpatched. Worthless repro.
2. **`deploy_xbox.py --xbe-only --skip-build` STILL re-patches `default.xbe`** (deploy_xbox.py:901-908
   runs the `patched_xbe` target whenever the build ELF is newer). So `cp cachebeta.xbe default.xbe`
   then deploy silently uploads the PATCHED xbe. Several "unpatched" deploys this session were patched.
   FIX: upload the pristine original DIRECTLY, bypassing the patch step:
   `python3 tools/xbox/xbdm_rdcp.py --sendfile halo-patched/cachebeta.xbe 'E:\GAMES\halo-patched\default.xbe'`
3. **Console `map_name a10` boot skips the scenario main script thread.** It loads the BSP + statically
   spawns encounters but does NOT issue the squad advance/engage/target orders. => grunts shuffle to
   static posts, never acquire player, never aim, only return fire when shot — in BOTH builds.

## FAITHFUL REPRO = real New Game boot
- Empty `init.txt` (no `map_name`, no `core_load_at_startup`) => boots to main menu.
- User: **main menu -> New Game -> Pillar of Autumn -> Impossible -> play to first doorway.**
- This runs `game_new` + the scenario script thread. CONFIRMED: unpatched grunts advance + aim + fire +
  cycle guard<->fight (user-confirmed 2026-06-27).

## BUILD-IDENTITY VERIFICATION (the box flip-flopped; always verify)
- Live XBE header nsec @ VA 0x1011C: **24 = UNPATCHED**, 30 = patched. Readable even at menu (header@0x10000=XBEH).
- In-game (AI pages loaded) byte at 0x2cdb0: **55 8bec** = unpatched prologue; **68 ..00 c3** = patched `push impl;ret` redirect.
- Tools recreated under /tmp this session (wiped on resume): verify_build2.py, am_state.py, am_all.py, scan.py.

## GOLD SIGNATURE (unpatched New Game, door squad enc 0xee6e0009 = actors a6/a10/a11)
- moved **1.6-2.3u** (advance), aw -> **10**, mode -> **3**, **pact=[0,1]** (pathfinds),
  **ctgt=True** (acquires PLAYER as combat target @0x610 => aims+fires), 6a=3.
- Field offsets (actor stride 0x724 @ *0x6325a4): aw 0x268, action6a 0x6a, behavior 0xc0, mode/move_src 0x46c,
  fp_idx 0x470, at_dest 0x484, path_attempt 0x4a4, path_active 0x4a8, combat_tgt 0x610(!=ffffffff), pos 0x12c/0x130.
- NON-regression squads to IGNORE: a0/a1 enc 0xee860021 (pre-doorway guards, aw=0);
  a2/a4/a7/a8 enc 0xee690004 (pre-positioned, mode=3/aw=0 in all builds).

## A/B RESULT (FAITHFUL New-Game repro, 2026-06-27) — actor_moving CONFIRMED
Door squad a6/a10/a11, gold(unpatched) vs clean-HEAD-patched(d8ad32eb, actors.c edit stashed):
- moved: gold 1.6-2.3u vs patched **0.04-0.06u** (stuck)
- aw: gold ->10 vs patched ->**3** (plateau)
- 6a: gold [3] steady vs patched **[2,3] CYCLES** (guard<->fight = the rejection loop the user sees)
- mode: gold ->3 sustained vs patched [1,3] transient then reverts to 1 (commit attempted, REJECTED)
- **pact (path_active 0x4a8): gold [0,1] vs patched [0] — pathfinder NEVER activates in patched**
- **ctgt (combat target 0x610): True in BOTH** => target acquisition WORKS

CONCLUSIONS:
1. ctgt=True both => PERCEPTION + TARGET-ACQUISITION EXONERATED (not the bug).
2. mode reaches 3 transiently (actor_path_refresh IS called via actor_move_to_firing_position 0x2d900)
   but returns 0 => selector reverts mode->1, pact stays 0, no advance, aw stuck 3.
   => bug is in actor_moving path-build/commit (actor_path_refresh 0x2cdb0 returning 0 where gold returns 1).
   Toggle keystone (moving_only=FIXED) now HOLDS on the faithful repro.

## FILE CONFIRMED: actor_moving (toggle on FAITHFUL repro, 2026-06-27)
Reverted actor_moving.obj (28 fns ported=false), rest patched, New Game door run [movingoff capture]:
door squad a6/a10 moved 1.69/1.73u, pact=[0,1] (pathfinds), aw->10 (a6) — RESTORED advance (matches gold;
patched was 0.04u/pact=0). => actor_moving IS the culprit. Toggle re-validated on faithful repro.
Verify pattern: actor_moving fns (0x2cdb0,0x2d900) read ORIG 55 8bec; others (0x99530,0x14df70) read redirect 68..c3.

## FUNCTION-LOCALIZATION (narrowed; source-vs-disasm, no playthroughs)
VERIFIED FAITHFUL (ruled out): actor_test_destination(0x2a580, sticky 0x484 matches orig disasm),
  actor_move_to_firing_position(0x2d900, sets 0x3bb=0/mode=3/fp; faithful), case-3 dest resolution
  (actor_path_refresh 2179-2204, tag_block_get_element chain matches disasm comment), 0x484 cleared at line 2161.
SUSPECT: actor_path_refresh(0x2cdb0) check_dest (lines 2293-2306). Patched patt=0/pact=0 => pipeline NEVER reached
  => reject is check_dest LAB_fail (NOT the pathfinder). On-foot path: if(f498(0x498)==0.0f) path_found=(facing(0x494)!=-1);
  if !path_found goto LAB_fail. So mode=3 reject = facing==-1 (far fp's order facing handle) OR f498 wrongly 0
  (case 3 does NOT set 0x498; only case 5 does at line 2267 — 0x498 carries stale). Need mode=3-tick f498/facing
  (transient; grunts settle to mode=1 so live capture misses it) OR dual-oracle-by-address on 0x2cdb0, OR
  verify check_dest (2293-2306) + the actor_test_destination result handling (2308-2334) vs disasm 0x2d096-0x2d0b3.
  NOTE: prior actor_looking scorer fix (FUN_00024cf0) may interact — but toggle says actor_moving.

## PATT DISCRIMINATOR + SETTLING EVIDENCE (2026-06-27) — favors (a) inlined TEST
patt(0x4a4): SET=1 at actor_moving.c:2438 (repath convergence, cdb0); CLEARED=0 at actors.c:6751 (FUN_0003d9f0
per-actor update, runs each tick). => patt is PER-TICK not sticky; patt=0 on idle(msrc=1) grunt is uninformative.
Live broken read: all 3 door grunts msrc=[1] guard, patt=0, pact=0, dist a6=1.8 a10=1.1 a11=2.0 from CORRECT dest.
60s provocation did NOT make them re-attempt msrc=3 (stayed guard). KEY INFERENCE: grunts SETTLED to stable guard
at dist 1-2u and WON'T re-attempt firing-pos move => they THINK they ARRIVED => inlined test_destination reports
at-dest at dist 2.0 (wrong; working tolerance ~0.8) => (a) THE TEST. (b) pipeline-fail would keep selector issuing
msrc=3 => cycling, NOT stable guard. So stable-guard + won't-reengage = strong (a). FIX: actor_test_destination
__attribute__((noinline)) so cdb0 CALLS standalone 0x2a580 (correct, =original) not inlined-miscompiled copy. patt
needs a FRESH engagement to read (grunts idle now) => same cost as testing the fix. /tmp/am_dest.py has patt+discriminator.

## RUNTIME DEST CAPTURE (2026-06-27) — DEST IS CORRECT; bug is DOWNSTREAM (test/pipeline), NOT case-3
Probe /tmp/am_dest.py (dest 0x488 vs pos 0x12c + dist/msrc/fp/d480/atd/pact). WORKING(cdb0=ORIG, settled@firing pos)
vs BROKEN(all our lift, fresh New Game):
  a6  WORKING dest(-11.582,-2.888) | BROKEN dest(-11.582,-2.888)=SAME✓ dist 2.0..13.8 msrc[0,1]guard fp[0]
  a10 WORKING dest(-11.675,-4.061) | BROKEN dest(-11.888,-3.463)=a11's(fp cycled) dist 0.5..1.1 msrc[1,3] fp[0,1,9]
  a11 WORKING dest(-11.888,-3.463) | BROKEN dest(-11.888,-3.463)=SAME✓ dist 1.7..2.1 msrc[1,3] fp[0,1,9]
  ALL broken: pact[0], atd[1], d480 NONE(on-foot), f498 0.0.
VERDICT: 0x488 dest is CORRECT (a6,a11 match working exactly; a10 only off because fp_idx 0x470 CYCLED). => case-3
  resolves RIGHT dest. Advisor outcome 2: bug is INLINED TEST or PIPELINE, downstream of dest. SMOKING GUNS:
  (1) a6 dest correct but dist up to 13.8u away, sitting in GUARD (msrc 0/1) — far from correct dest, not moving.
  (2) fp_idx (0x470) CYCLES [0,1,9] on broken vs STABLE per-grunt on working => selector THRASHING, re-assigning
      firing positions because cdb0 keeps reporting failure. fp is set by SELECTOR not cdb0 => downstream of cdb0 fail.
  Two sub-hypotheses: (a) inlined test_destination returns at-dest(atd=1) wrongly at dist 2.0 -> LAB_path_ok -> no repath
  -> return 1 (selector thinks arrived); (b) repath runs but pathfinder FUN_0005ff70 returns 0 -> LAB_fail -> selector
  retries -> fp cycles. fp-cycling favors (b). atd muddied by msrc 1<->3 cycle (msrc=1 early-return also sets atd=1).
  FIX HYPOTHESIS: force actor_test_destination noinline so cdb0 CALLS standalone 0x2a580 (correct, =original) instead of
  inlining (suspect). Low-risk, 1-playthrough testable. But won't fix if bug is pipeline. Box on BROKEN build now.

## CDB0 KEY STRUCTURAL FINDING (2026-06-27): clang INLINED actor_test_destination into cdb0
The ORIGINAL cdb0 CALLS actor_test_destination (FUN_0002a580) at 0x2d1f. Our CLANG cdb0 INLINED it
(both in actor_moving.c same TU). Inlined block clang 4277-42d2: cmp move_src,1;ja (move_src<=1 => set atd=1
shortcut); else actor_destination_tolerance(handle)->tol; compute dx²+dy²+dz² (0x488-0x12c,0x48c-0x130,0x490-0x134);
fucomip tol² vs dist²; jbe 436d if tol²<=dist²(NOT at dest)->436d->(atd==0)->42fc FULL REPATH; else 42d8 atd=1(at dest).
=> CONTRADICTS advisor premise "test_destination is same code in both builds": BROKEN build (cdb0=our lift) uses INLINED
test; FIXED build (cdb0=orig) CALLS standalone 0x2a580 (our lift). They are DIFFERENT code paths. If clang's inlined
test != standalone (inlining codegen bug, or reads stale/wrong data in inline context), THAT flips at-dest=>pact.
Inlined logic LOOKS faithful (correct cmp dir, correct offsets, atd-sticky, reaches full repath 42fc->tag_get->on-foot
pipeline->path_state_build_path=pact). VERIFIED FAITHFUL so far: entry/early-return, case-3 dest copy, 0x498=0 join,
check_dest facing, test/dist/repath structure. NOTE standalone 0x2a580 VC71=95.9% [FPU-WARN] (inlined inherits any FPU bug).
NEXT (advisor guardrail hit: 3+ regions faithful): runtime capture on BROKEN build (restore kb all-true, rebuild, deploy)
of dest 0x488/48c/490 vs pos 0x12c/130/134 (=>dist), move_src 0x46c, fp 0x470, squad 0x34, atd 0x484, pact 0x4a8 on
fresh engagement. Directly shows if cdb0 sees wrong dest OR computes at-dest wrong. OR sharper: diff inlined-test vs
standalone 0x2a580 disasm. Box currently runs cdb0-alone (cdb0=ORIG, working). Disasms: /tmp/cdb0_orig.asm /tmp/cdb0_clang_body.asm.

## CDB0 INTERNAL DIFF PROGRESS (2026-06-27) — entry + on-foot pipeline VERIFIED FAITHFUL (clang vs orig)
Disasm diff orig(delinked actor_moving.obj FUN_0002cdb0) vs clang(build/.../actor_moving.c.obj _actor_path_refresh):
- ON-FOOT PIPELINE (clang 43d1-44a4 vs orig 2e20-2eee): FAITHFUL. All 8 calls present+ordered
  (path_input_new,paths_dispose,set_attractor[gated 0x280>0&&0x28a==0&&tag[4]&0x10==0],get_path_storage,
  path_state_new(local_nav,large_buf,path_state),FUN_0005e0d0(large_buf,&0x488,0x494,0x498),
  FUN_0005ff70(large_buf)=pathfinder,path_state_build_path(large_buf,&0x4a8)=SETS PACT). Spilled ptrs VERIFIED:
  [ebp-0x18]=&actor[0x4a8] (lea@4004/4070), [ebp-0x14]=&actor[0x488] (lea ebx@3fbe). Args correct. NOT the bug.
- ENTRY/EARLY-RETURN (clang 3f5b-3fd9 vs orig 2a32-2a9e): FAITHFUL for door grunt. clang: movzx move_src@0x46c;
  cmp 2;jb 3f8b(early-ret if<2=move_src 0/1); cmp [0x160],0;je continue; cmp move_src,3;jne 4070; cmp [0x3bb],0;
  jne 3f8b(early-ret if 3&&0x3bb!=0); else 3fd9 continue -> 3fe0 atd(0x484)=0. For move_src=3,0x160=0,0x3bb=0
  clang REACHES 3fe0 (atd clear). So early-return does NOT mis-fire => CONTRADICTS advisor "atd-never-0=>early-return".
  Reconcile: atd-never-0 in broken build likely DOWNSTREAM — grunt stuck mode=1(move_src=1)=>correct early-ret(atd=1);
  mode=3 transient 1-tick (missed by 0.25s sample) BECAUSE commit fails downstream (same pattern as e560 net-moved noise).
REMAINING UNVERIFIED codegen in cdb0 on door-grunt path: case-3 dest resolution CODEGEN (src faithful, codegen unchecked:
  3ff8 0x34 chk + tag_block chain + dest copy to 0x488/494), check_dest/test/dist path (2293-2334), override branch
  (2368-2386), path_3d_build mounted branch (2355-2367, not door grunts). Next: case-3 codegen diff OR runtime
  high-freq field capture (move_src,0x160,0x3bb,0x34,dest 0x488,fp 0x470,atd,pact) on BROKEN build fresh engagement.

## CDB0-ALONE TOGGLE (2026-06-27, FAITHFUL New Game) — actor_path_refresh IS THE SOLE CULPRIT
Reverted ONLY 0x2cdb0 (actor_path_refresh); 2d900+2a470+e560+24 others = OUR lift. Toggle verified LIVE:
0x2cdb0=ORIG; 0x2d900/0x2a470/0x2e560=REDIRECT. Door squad enc 0xee6e0009:
  a6  moved 1.69u  aw 0->10  pact[0,1]  (FULL advance = gold = all-orig)
  a10 moved 1.75u  aw 5..7   pact[0,1]  (advances = all-orig 1.73u)
  a11 moved 0.16u  aw 5..7   pact[0,1]  (= all-orig 0.16u EXACTLY — confirms flanker net-moved = noise)
=> cdb0 ALONE restores pact + advance, with path_input_new(2a470) running OUR lift. BUG IS IN cdb0's OWN CODE.
2a470/2d900/e560 EXONERATED. Localization COMPLETE: actor_path_refresh (0x2cdb0).
pact(0x4a8) write mechanism (src 2387-2434 on-foot else-branch): pact set ONLY by
  path_state_build_path(large_buf, &actor[0x4a8]) — gated on FUN_0005ff70(large_buf)!=0 (pathfinder).
  Pipeline: actor_path_input_new(2a470)->paths_dispose->path_input_set_attractor->ai_debug_get_path_storage
  ->path_state_new->FUN_0005e0d0(large_buf,&0x488,0x494,0x498)->FUN_0005ff70->path_state_build_path(&0x4a8).
  Our cdb0: pact never 1 => pathfinder returns 0 OR branch wrong OR codegen bug — IN cdb0 (callees are our-lift & FINE).
NEXT: objdump -dr call-sequence diff orig(/tmp/cdb0_orig.asm) vs clang(/tmp/cdb0_clang.asm) on the on-foot pipeline
  + the switch/case-3 branch leading in. Source audited faithful => suspect clang codegen OR missed branch.
  Box currently runs cdb0=ORIG (working). RESTORE kb to all-true: /tmp/toggle_commit_subset.py --restore.

## COMMIT-SUBSET TOGGLE A/B (2026-06-27, FAITHFUL New Game) — pact GATE CONFIRMED + likely 2nd bug
Reverted ONLY {cdb0 path_refresh, 2d900 move_to_firing_position, 2a470 path_input_new} to original;
e560 + 24 others stay our lift. Toggle verified LIVE in-game: 0x2cdb0/2d900/2a470=ORIG(55 8b ec),
0x2e560=REDIRECT(68..c3). Door squad enc 0xee6e0009:
  a6  moved 1.68u  aw 2->10  pact[0,1]  (= all-orig 1.69u — FULL advance restored)
  a10 moved 0.02u  aw 5..6   pact[0,1]  (all-orig was 1.73u — NOT restored; beh thrashes 0..168)
  a11 moved 0.04u  aw 0..6   pact[0,1]  (NOT restored; beh thrashes)
PATCHED(clean) was pact=[0] for ALL THREE (path never activates).
FINDING 1 (DECISIVE): pact 0->1 RESTORED for all 3 by the commit-subset revert => actor_path_refresh(cdb0)
  WAS returning 0 where orig returns 1. Commit gate = CONFIRMED bug. Advisor hypothesis verified.
FINDING 2 was a PHANTOM (advisor corrected, 2026-06-27) — do NOT chase a 2nd bug in e560:
  - movingoff (ALL 28 reverted, ZERO lift active): a11 already moved only 0.16u. No lift => no lift bug can cause that.
  - a10 STATE signature IDENTICAL across movingoff vs commitsubset (aw 5..6, 6a[2,3] CYCLES, beh thrashing 0..N).
    Only net `moved` differs (1.73 vs 0.02). A broken e560 would diverge STATE, not just displacement.
  - `moved` = NET displacement (pN-p0), NOT path length. Maneuvering flankers (a10/a11) that advance-then-reposition
    read ~0; point grunt a6 (advance+hold) is stable 1.69->1.68. Net-moved on flankers is NOISE, not a 2nd bug.
  DISCRIMINATOR from here = pact[0]->[0,1] (binary) + a6 advance (stable). Ignore a10/a11 net-moved.
NEXT: narrow to cdb0 ALONE (revert only 0x2cdb0; 2d900+2a470 back to our lift). pact->[0,1]+a6 advance => cdb0 is
  THE single culprit (done localizing). pact stays [0] => 2a470 also contributes (re-add). 2d900 already faithful vs disasm.
  Two-bug Q answers itself FREE: after fixing cdb0, clean build NO toggles + 1 faithful run; gold returns => 1 bug.
RESTORE: python3 /tmp/toggle_commit_subset.py --restore. Toggle setter: /tmp/toggle_commit_subset.py (TARGETS set).

## ★★ CRITICAL REPRO CLARIFICATION (2026-06-27, user) — PROVOCATION CONFOUNDS CAPTURES ★★
USER CONFIRMED: (1) the bug = PROACTIVE engagement. Unpatched grunts advance+shoot the INSTANT player crosses the
first door (scripted spawn+engage trigger = weapon-unholster point); NO player firing needed. Patched grunts stay
PASSIVE until SHOT AT. (2) In the earlier cdb0 TOGGLE tests the user did NOT shoot => reverting cdb0 restored GENUINE
proactive advance => TOGGLE IS VALID, cdb0 IS the culprit (not confounded).
CONFOUND: SHOOTING a grunt escalates it via DAMAGE perception (alternate path) => it advances even in the broken build.
So ANY capture where the user "provoked/engaged" the grunts is CONFOUNDED (shows provoked advance, not the broken
proactive behavior). The latch-LAST capture (exit=3 LAB_path_ok, dist 0.3, grunts at firing pos) was PROVOKED (user shot)
=> NOT the bug state. The first-latch (no-shoot) LAB_fail(facing==-1) WAS clean but is the transient.
=> NEED: clean NO-SHOOT latch-LAST run. Player crosses door, does NOT fire, waits ~20-30s (grunts attempt scripted
msrc=3 advance, FAIL, settle passive), then read latch-LAST. That gives the PROACTIVE (script-driven) cdb0 exit with
facing=403 (steady fp) — the real bug state. Provoked dist-0.3/at-dest readings are damage-escalation, ignore them.
NOTE: aw(0x268) escalation: proactive-broken plateaus ~3-5; shot grunts reach higher; unpatched proactive ->10.
cdb0's script-driven move is what's broken; damage-escalation bypasses it. RE-READ all prior "engaged" captures as
possibly provocation-confounded.

## VC71 FULL SWEEP DONE (2026-06-27) — all 28 scored via whole-object ref
KEY METHOD: per-function refs for PORTED fns can't be made by batch_delink (its per-function pass only
exports UNPORTED split-TU fns → exported 0). BUT the whole-object `delinked/actor_moving.obj` (50K) has
ALL 28 .text symbols. So: `vc71_verify.py actor_moving.c --no-cache` (recompiles build/vc71/actor_moving.obj)
then `compare_obj.py build/vc71/actor_moving.obj delinked/actor_moving.obj` → per-function LCS for ALL 28
in one shot. 26/28 reliable; 2 are REF-TRUNCATED (ref incomplete).
Scores (low→high): b5d0 35% (STRUCTURAL trig tables), **e560 43.9% [REF-TRUNCATED]**, a530 50% [trunc tiny],
b490 51, b830 53.7, daa0 60.6, d900 62.5, d850 65.1, d350 69.2, bd80 70 [FPU-WARN], b020 71.1, bab0 77.4,
d9b0 78.1, cdb0 78.9, a860 87, d720 87.3 [FPU-WARN], b310 88.3 [FPU-WARN], a7e0 90.9, ace0 91, ade0 91.4,
a8f0 92.1, ab40 92.4, b400 93.2, a470 95.2, a580 95.9 [FPU-WARN,faithful], a610 96.8, a3a0 100, a540 100, b720 100.
**e560 = actor_move_update = MASTER per-tick mover = the unaudited EXECUTE half = #1 SUSPECT.** Its 43.9% is
a LIE: ref captured only 1025B/305insn but true span is up to 0xC40=3136B (next fn 0x2f1a0; our compiled=871insn
fits). REF IS TRUNCATED (e560 is last fn in obj; whole-obj export stopped at .text end 0x45c1). Need a COMPLETE
e560 ref (range 0x2e560–0x2f1a0) to get its true score. batch_delink won't do ported fns; no direct ghidra MCP
in tool list. Options: extend objects.csv obj range + re-export whole obj; ghidra_live python client
(tools/ghidra_live_mcp/); or live-disasm e560 original bytes off the box (currently runs reverted=orig at 0x2e560).
FPU-WARN unaudited candidates (operand-order = possible real bug): bd80 70%, d720 87.3% actor_move_to_point, b310 88.3%.

## FUNCTION-LOCALIZATION ENDGAME (advisor-endorsed; source-vs-disasm has hit its limit)
The "read fn -> faithful -> must be subtler" loop CANNOT terminate AND is blind to the leading
hypothesis: clang-vs-MSVC x87/codegen divergence (invisible to source-vs-disasm; toggle always "fixes"
it by swapping in original machine code). Decide source-bug vs codegen-bug MECHANICALLY:
1. VC71-SWEEP all 28 actor_moving fns (compiles SOURCE w/ MSVC7.1, no snapshot/reloc issues):
   one low/FPU-WARN -> SOURCE bug, read THAT fn; all high -> codegen bug.
   BLOCKER: only 3/28 delinked refs exist (FUN_0002b5d0, _0002b830, _0002d900). Need
   `batch_delink.py --object actor_moving.obj --per-function-only` (REQUIRES Ghidra/mcp-servers.sh — was DOWN 2026-06-27).
2. UNICORN A/B (user's idea; catches codegen VC71 misses): unicorn_diff runs clang .obj vs MSVC oracle.
   Return/FPU divergence = RELIABLE. Mem-trace (side-effect) divergence = MUDDIED by oracle-reloc gap
   ([[reference_equiv_oracle_reloc_gap]]) — needs a STATE-SNAPSHOT from the faithful engaged repro to map
   globals (dump_xemu_memory.py; finicky, verify datum magic). This bug is a SIDE-EFFECT (movement state,
   not return) so the snapshot is REQUIRED for a trustworthy unicorn verdict. Audited DECIDE half only;
   EXECUTE half (per-tick biped-stepping/velocity/avoidance fns) UNAUDITED — VC71 sweep covers all.
LEAD (ambiguous): FUN_0002b5d0 actor_move_get_avoidance_direction (35% VC71, FPU trig-table init writing
   avoidance tables @0x6327e0/0x6325c0). unicorn: return PASSED 100/100 but mem-trace diverged 100/100
   (likely reloc-gap artifact — 12 unmapped DAT_ relocs). 35% likely STRUCTURAL (fsin/fcos spills), not a
   confirmed bug. Re-eval with a state-snapshot.
NOTE: "pipeline never reached" rests on n=1 patched mode==3 sample — do NOT exclude path-build-setup
   (actor_path_input_new/path_state_new/FUN_0005e0d0) or movement-EXECUTION fns. Don't reopen actor_looking
   (toggle was decisive). If forced live again: ONE patched playthrough, CONTINUOUS provocation, high-freq
   capture of full decide+execute fields during FRESH engagement (mode==3 only fires then).

## NEXT (A/B with the FAITHFUL repro)
- TOGGLE-BISECT VERDICTS FROM CONSOLE/RELOAD REPRO ARE SUSPECT (advisor) — incl. the "moving_only=FIXED /
  actor_moving is sole culprit" keystone. Re-derive with New Game repro.
- Build clean HEAD patched (stash the uncommitted actors.c cross-product edit first), New Game -> doorway,
  capture door squad, diff ctgt/aw/mode/pact vs gold. First differing field names the broken STAGE:
  ctgt=False => target-acquisition (perception/actors); pact/mode broken w/ ctgt ok => movement (actor_moving).
- See [[project_a10_passive_mechanism_enemy_prop_promote]] [[project_a10_passive_culprit_ai_obj]].
