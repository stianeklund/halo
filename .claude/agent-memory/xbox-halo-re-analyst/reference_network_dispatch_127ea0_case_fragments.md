---
name: network-dispatch-127ea0-case-fragments
description: FUN_00127ea0 network message dispatch — its caseD_XX entries (0x1281bf, 0x1281e9, ...) are computed-jump switch fragments, NOT standalone functions; do not port individually
metadata:
  type: project
---

The network message dispatch `FUN_00127ea0` (switch at `0x127f62`, jump table at
`0x1282b4`) is compiled as a computed-jump switch. Ghidra carves each case out as a
fake standalone function `switchD_00127f62::caseD_XX` (e.g. `FUN_001281bf` = caseD_1e
at 0x1281bf-0x1281e8, `caseD_1f` at 0x1281e9-0x128278). kb.json may register some of
these carved addresses as standalone functions — they are NOT.

**Why they cannot be lifted/patched individually (verified on 0x1281bf, 2026-07-05):**
- Reached only by COMPUTED_JUMP from the parent (`get_xrefs_to` = DATA jump-table ref +
  COMPUTED_JUMP from 0x127f62), never by CALL.
- Split prologue/epilogue: 0x1281bf enters `PUSH ESI; PUSH EDI` but exits
  `POP EDI; POP ESI; POP EBX; POP EBP; RET` — EBX/EBP were pushed by the PARENT's
  prologue. The fragment runs on the parent's frame.
- The entry pushes are CALL ARGS, not register saves: `PUSH ESI; PUSH EDI; MOV ESI,ECX;
  CALL 0x127d30; ADD ESP,0x8` means `FUN_00127d30(arg1=EDI, arg2=ESI=ECX)` (2 stack args
  from parent registers) — contradicts any "0-arg char(void)" decl for that call site.
- Success path is `JNZ 0x128272` (into sibling caseD_1f's shared epilogue), not a RET.

`patch.py` redirects an entry with a JMP to a C impl that has its own prologue + normal
RET; doing that here executes the C body on the parent's mid-flight frame and mis-returns
= stack/ABI corruption crash. Only the whole `FUN_00127ea0` (with its jump table) can be
reimplemented as a unit. Skip these carved case addresses. The failure error string for
caseD_1e is "network_game_client_handle_message_server_switch_to_pregame() failed"
(logged via network_game_log 0x12b650); the worker is FUN_00127d30.

**caseD_1f (0x1281e9) re-verified 2026-07-05:** same signature — entry `PUSH ESI;
PUSH EDI; MOV ESI,ECX; CALL 0x127df0; ADD ESP,0x8` (2 stack args from parent regs +
ECX), 2-push/4-pop asymmetry (epilogue `POP EDI;POP ESI;POP EBX;POP EBP;RET`), reached
only by COMPUTED_JUMP from 0x127f62. Worker = FUN_00127df0; error string @0x294320
"network_game_client_handle_message_server_graceful_game_exit_postgame() failed". SKIPPED
a /lift attempt on it — standalone-unliftable, port only via whole parent FUN_00127ea0.
