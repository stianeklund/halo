---
name: actions-allow-cover-seeking-sub90-runtime-lane
description: actor_action_allow_cover_seeking 0x1ccc0 86.1% AUTO_ACCEPT via state-snapshot equiv + full dark-branch static decode; "all-seeds-identical-return + weak confidence" is NOT blocking when dark branches decoded 1:1
metadata:
  type: project
---

actor_action_allow_cover_seeking @0x1ccc0 (actions.c) AUTO_ACCEPTED on the RUNTIME lane, second sub-90 actions.obj clear after [[project_actions_0x1d530_sub90_statesnapshot_clear]].

**What cleared it (86.1% VC71, 71/73):**
- Full 1:1 static decode of ENTIRE body vs Ghidra disasm — every offset, branch sense, operand order, width. Pure cdecl predicate `char f(int actor_handle, char param_2, int actor_index)`; param_2 read byte-width (MOV AL [EBP+0xc]) so int→char decl fix is correct.
- datum_get(actor_data, actor_handle): PUSH EAX(handle) then PUSH ECX(actor_data) → last-push=actor_data=arg0. tag_get(0x61637472='actr', *(actor+0x58)).
- FP idioms verified: two `> 0.0f` (FCOMP vs *0x2533c0=0.0; TEST AH,0x41;JNZ) on actr_tag+0x2d8 (cooldown) and actr_tag+0x324 (threat threshold); one `<` (TEST AH,0x5;JP) actor+0x1c0 < actr_tag+0x324. NO FPU-WARN despite 4 FP blocks.
- cooldown ticks: FMUL *0x253394 (=0x41f00000=30.0f, seconds→ticks@30Hz), _ftol2 written as `(short)(int)(...)` cast NOT transcribed; int16 truncation captured by MOVSX word reload at **0x1cd3a** (source comment says 0x1cd9a — TYPO, out of range, non-blocking).
- Dark deny branches decoded (uncovered by runtime): actor+0x6e word signed `>= 7` (JGE), actor+0x378 byte→result=0, actor+0x160 byte→early `return 0`.
- Equiv: 100/0/0 on /tmp/snap_cover.json (infection_swarm copy, arg_overrides actor_handle=0xE36B0001 salt 58219, param_2=0), stub-arg 200 calls 0 mismatch — independently reproduced.

**Reusable bar:** equiv "WARNING: all N seeds returned identical value + confidence weak" is NOT a reject reason when the identical return is explained (live actor never reaches a deny branch) AND all dark deny branches are independently decoded 1:1. Runtime lane = 0-diverge pass on populated-datum snapshot + static decode of coverage-dark ranges (same standard as evade 8c733ea4 / consider_grenade 6cc5191c). Neighbor FUN_0001dab0 16.4% (247/22) FAIL is the documented stale delinked/actions.obj region past ~0x1dab0 — disregard for targets before 0x1420 section offset.

**'actr' tag field offsets (reusable):** actr+0x2d8 = cover cooldown seconds; actr+0x324 = threat threshold float. actor+0x6e = short state counter; actor+0x26c = last cover-seek game_time (-1=none); actor+0x1c0 = current threat scalar; actor+0x1ca = suppress byte; actor+0x378/+0x160 = hard-deny bytes.
