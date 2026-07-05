---
name: actions-consider-grenade-88band-needsruntime
description: actor_action_consider_grenade 0x1fb80 @88.8% cdecl body fully static-verified faithful, but <90% band + no runtime/equiv = NEEDS_RUNTIME (not AUTO_ACCEPT)
metadata:
  type: project
---

actor_action_consider_grenade (0x1fb80, actions.obj / ai/actions.c) reviewed at
88.8% VC71, acceptance path "permute" (no runtime/equiv run). Verdict NEEDS_RUNTIME.

**Why NEEDS_RUNTIME not AUTO_ACCEPT:** 88.8% is < 90%. Policy: <90% requires
golden/runtime behavior verification + classified mismatches; absent that =
NEEDS_RUNTIME. This is a plain cdecl fn (ABI regs=none) so 88.8% is an
*informative* LCS match against a valid ref — NOT a reg-defined-prologue ceiling.
Reinforces [[project_informative_sub90_needs_runtime_bar]]: clean static body
audit does not clear AUTO_ACCEPT below 90%. 88.8 ∈ [87,89] = the equivalence-
acceptance band, but equivalence was never run, so no behavioral evidence exists.

**Why NOT REJECT (static audit fully clean):** full 0x1fb80 disasm decoded 1:1 vs
source. Both FCOMP idioms verified correct polarity:
- cooldown FCOMPP;TEST AH,0x41;JP → return 0 iff `expiry > current_time` (matches)
- roll FCOMP;TEST AH,0x5;JP → return 0 iff `team_mult*throw_chance <= roll`
  (matches; FMUL order even preserved, commutative anyway)
All 7 callees resolved by CALL target addr + cdecl-clean (ADD ESP cleanup visible):
datum_get 0x119320, tag_get 0x1ba140('actv'), game_time_get 0xb5aa0 (=Ghidra
FUN_000b5aa0), FUN_000b55b0(0x17,short@+0x3e), get_global_random_seed_address
0x10b0d0, random_math_real 0x10b240 (=FUN_0010b240), actor_action_test_grenade
0x1d180, actor_action_try_to_throw_grenade 0x1fa60(handle,1). Named substitutions
(game_time_get/random_math_real) verified against their kb addrs — correct, not
wrong-callee. Double-deref global `*(char**)0x632574+0x3b4` matches Ghidra
`DAT_00632574+0x3b4`. All memory offsets (6a0/6a4/5c/3e/180/182/1a0/1a4) matched.

**Companion decl fix (correct & necessary):** actor_action_test_grenade @0x1d180
decl changed `void(void)` → `char(int actor_handle)` — matches the 1-PUSH/AL-read
call site exactly. Old decl was wrong; fix required for this lift.

**Mismatch classes (all benign shape):** short-circuit `&&` vs nested-if for the
cooldown guard; char vs undefined4 return; scratch-slot reuse (EBP-0x4/-0x8);
FCOMP idioms as `>`/`<=` in C. No unclassified control-flow.

**To promote to AUTO_ACCEPT:** run equivalence (unicorn_diff --allow-stubs +
actor/tag state-snapshot, non-leaf AI fn) or golden/dual-oracle. Body is verified
faithful, so a passing equiv/golden run should clear it.

**2026-07-05 re-review — equiv NOW RAN but is VACUOUS (still NEEDS_RUNTIME):**
acceptance path relabeled "permute+equiv_weak" but (a) permute stage ran=false
(skipped, --permute not set) and (b) equivalence.json shows the weak run is the
textbook early-exit-only case, NOT behavioral coverage:
- confidence=weak, coverage_pct=32.6 (273-byte fn), unique_returns=1 — all 100
  seeds hit the SAME return across zero-fill state.
- covered_pcs run 0x400000..0x40004c then jump to 0x40010a..0x400110 (epilogue).
  The entire middle (cooldown FCOMP, roll `team_mult*throw_chance<=roll`,
  test_grenade, try_to_throw, the return-1 window) is DARK.
- global_reads: enable flag @0x3b4 read 0x00 → `if(enable==0) return 0` fires
  immediately; actor+0x6a0 read 0x00; no state-snapshot injected (auto_mapped
  page 0 only). So 100/100 "pass" is 100 identical trivial exits.
This is exactly CLAUDE.md `weak` = "only early-exit path tested, needs live memory
replay." Confirms sub-90 policy: weak/early-exit equiv is NOT the required runtime
verification. FPU/LOADW/IMM warns none; ABI regs=none clean; hazard+buffer-alias
clean. No concrete bug → not REJECT; inadequate behavioral evidence → NEEDS_RUNTIME.
Promotion needs equiv with actor/tag state-snapshot + enable flag non-zero so the
consider/roll/throw paths actually execute (or golden/dual-oracle).

**2026-07-05 PROMOTED to AUTO_ACCEPT — genuine live-snapshot equiv now ran:**
The exact promotion condition above was met. equiv_state_snapshot on the
infection_swarm live capture: actor_handle 0xe36b0001 → real actor datum, salt
58219 = 0xE36B (matches handle high-16 = GENUINE datum hit, not evade-style 0xCC
unseeded artifact). 100/100 seeds, 0 divergences, 0 mem-trace divergences, 0
errors; stub-arg differential 200/200 clean; all callees reloc-mapped 1:1
(game_time_get 0xb5aa0, random_math_real 0x10b240, DAT_00632574 enable, DAT_253394
=30.0f, try_to_throw 0x1fa60, FUN_000b55b0, test_grenade). Coverage jumped 32.6→74.4%
(now exercises cooldown FCOMP + roll + test_grenade middle that was DARK before);
only ~70-byte throw-commit tail unreached (needs stubbed random+test_grenade BOTH
succeed) — and that tail was statically decoded 1:1 with both call args verified.
leaf_cache tier still "weak" but that is a coverage/unique_returns=1 artifact, NOT
vacuity (contrast the prior 32.6% zero-fill trivial-exit run). This is the exact
profile of [[project_actions_0x1d530_sub90_statesnapshot_clear]] and satisfies
CLAUDE.md "0-div pass on live infection_swarm snapshot = accepted sub-90 runtime
evidence." Full static body audit (carried, body byte-identical) + genuine runtime
pass = AUTO_ACCEPT. Body placement between committed 1fa60/1fca0; only new = port flag.
