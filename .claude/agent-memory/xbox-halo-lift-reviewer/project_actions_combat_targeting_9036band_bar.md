---
name: actions-combat-targeting-90-95-band-bar
description: What cleared actor_action_handle_combat_targeting (0x1dea0) at 93.6% VC71 for AUTO_ACCEPT — the 90-95% band audit bar met with full disasm evidence on a small cdecl body
metadata:
  type: project
---

actor_action_handle_combat_targeting (0x1dea0, actions.c) AUTO_ACCEPTED at 93.6% VC71 (76 cand / 80 ref insns; no FPU/LOADW/IMM warn). This is the reference example of the **90-95% band cleared statically** (unlike the sub-90 band which needs runtime — see [[project_informative_sub90_needs_runtime_bar]]).

**Why AUTO_ACCEPT held at 93.6%:** policy 90-95% band requires (a) mismatch classification, (b) full call-site arg audit, (c) memory-offset/global side-effect audit, (d) no unresolved Uncertain. All four met with Ghidra disasm of 0x1dea0 decoded 1:1 against source:
- All 7 CALLs resolved by addr→kb decl, all cdecl (audit_reg_abi regs=none): datum_get(actor_data,handle) + datum_get(prop_data,*(actor+0x270)) @0x119320, tag_get('actr'=0x61637472,*(actor+0x58)) @0x1ba140, actor_combat_get_firing_variant_definition @0x211f0→char*, get_global_random_seed_address @0x10b0d0, random_math_real @0x10b240→FLOAT-in-ST0 consumed directly by FCOMP (no XCALL/wrong-reg hazard), display_assert @0x8d9f0, system_exit(-1) @0x8e2f0. Arg order/count confirmed via PUSH order + ADD ESP cleanup.
- Both FCOMP compares verified `<` idiom (FLD a; FCOMP b; FNSTSW AX; TEST AH,0x5; JP fail) = source `a < b` fall-through-into-body. FPU-WARN did NOT fire.
- Offsets all matched: 0x58,0x1b0,0x6e(>4 == JL 0x5),0x270,0x11c,0x16c,0x3a8,0x310. Single write = MOV word[ESI+0x310],AX; signed clamp CMP AX,4;MOVSX;JG-else-4 == source `if(<5) retry=4`.
- **Resolved-Uncertain trap:** source computes `prop=datum_get(prop_data,*(actor+0x270))` BEFORE the NONE assert. Disasm confirms the original ALSO calls datum_get @0x1df00 before CMP/JNZ NONE @0x1df10 — faithful shape, not a lift bug. Do not flag ordering here.

**The 4-insn ref>cand delta = benign compiler shape** (reg-alloc/scheduling/EBX-BL return + spill), NOT a dropped body element — established because every semantic token is present 1:1 and no census detector (FPU/LOADW/IMM) fired.

**Bar summary:** a small cdecl body (no @reg args) in the 90-95% band clears AUTO_ACCEPT when the FULL original disasm is decoded against source and every call/offset/FPU-direction/side-effect matches with zero detector warnings. Contrast the sub-90 informative-VC71 band which is a coverage gate → NEEDS_RUNTIME even on a clean static audit.

**Confirming instance — actor_action_handle_active_cover_seeking (0x1e700, actions.c) AUTO_ACCEPTED at 94.4% VC71** (118 cand / 113 ref insns; no FPU/LOADW/IMM warn; hazard --changed-only clean; ABI regs=none; delinked/functions/0001e700.obj + delinked/actions.obj registered). Same TU, same bar. All 7 CALLs cdecl and arg-audited via PUSH order + ADD ESP: datum_get(actor_data,handle)@0x119320, tag_get('actr',actor+0x58)@0x1ba140, game_time_get()@0xb5aa0 (×2), actor_action_try_to_panic(handle)@0x1d6d0 (int16 ret in AX, feeds panic==3||4), actor_action_allow_cover_seeking(handle,0)@0x1ccc0 (decl refined int→(int,int)char THIS diff, matches 2-arg call), actor_action_try_to_seek_cover(handle,1,0)@0x1d350, FUN_0001d3c0(handle,4,actor+0x270,param3)@0x1d3c0. **param3(int)→param_4(char) narrowing is benign**: PUSH ECX pushes full dword, callee reads low byte, matches disasm. FCOMP idiom here is `<=` (FLD actor+0x1bc; FCOMP tag+0x2dc; TEST AH,0x41; JP skip) — verified via C0/C3: actor>tag→both clear→PF=1→skip, so enter-block == actor<=tag. Writes: actor+0x370=now (cooldown), trace(base *0x331f58 + (h&0xffff)*0x657c) +0xb8=1/+0xba=state{0,1,2,3,4,5,6,7}/+0xbc=(int16)elapsed/+0xc0=raw-bits(actor+0x1bc) — all telemetry, all present 1:1. param2 refined int→char (loaded MOV AL,[EBP+0xc]). 5-insn cand>ref delta = result-via-EBP+0xb memory byte + duplicated early-return epilogue + reg-alloc, benign shape.
