# Handover — RESOLVED 2026-07-11

## Objective (met)
Find and FIX the bug that made the all-on decomp build black-screen at boot
(`#PF CR2=0xfffffff5`, kernel bugcheck), previously bisected to `FUN_00158df0`.
`ported=true` everywhere; no workarounds.

## Root cause
`D3DDevice_SetRenderState_StencilEnable` (0x1ea300) and
`D3DDevice_SetRenderState_StencilFail` (0x1ea380) are **__stdcall @4** (both
end `RET 4`, arg read at `[ESP+8]`), but kb.json declared them `void f(void)`
(inherited from Ghidra's wrong decompile). `FUN_00158ae0`'s stencil-mode paths
called them arg-less → each call popped 4 bytes never pushed → ESP drift +4.
Clang inlined that body into `FUN_00158df0` (and `FUN_001595c0`), whose
pops-only epilogue (`POP ESI; POP EDI; POP EBP; RET`) shifted one slot:
`POP EBP` ate the return address (0x184f62 found in EBP at fault) and `RET`
jumped onto the thread stack (EIP=0xd00a21cf) → garbage execution → #PF.

Why the bisect paradox: the standalone `FUN_00158ae0` copy was reached only
via early-out paths in the good config, so toggling the *host* function's
`ported` flag flipped the crash while every static compare of `0x158df0`
itself was clean.

Diagnosed by dumping 64MB xemu RAM (`pmemsave 0 0x4000000`, physical scan is
safe) and pattern-scanning for the kernel EXCEPTION_RECORD/CONTEXT
(`00000001 fffffff5`) — no gdbstub needed.

## Fix (committed)
- kb.json: both decls → `void __stdcall ...(uint32_t value);`
- `rasterizer_xbox_decals.c`: case 0 → `StencilEnable(0)`; cases 1–3 →
  `StencilEnable(1)` + `StencilFail(0x1e00)` (values from the pristine body).
- Detector shipped: `tools/audit/check_stdcall_ret.py` (lift-learnings §30).

## Validation
- Build clean, hazard scan clean, reg-baseline no drift.
- VC71: FUN_00158ae0 93.7 → 95.5% (202/202); FUN_00158df0 unchanged 92.0%.
- All-on build deployed (rev verified live) → boots to a10 attract-demo
  gameplay (screenshot `xbdm/screenshots/shot-0001.png`).
- `0x158df0` confirmed `ported:true`; no new deactivations (121 allowlisted
  pre-existing dormant set only).

## Follow-ups
- `artifacts/audit/stdcall_ret_report.txt`: ~564 more `(void)`-decl-over-
  `RET n` functions (XAudio/CMcpx, winsock/xnet, XInput...). 13 are called
  from lifted C today (send/recv/bind/connect/socket, xnet_*, SleepEx,
  SetThreadPriority, GetExitCodeThread, D3DDevice_GetDeviceCaps...) — some
  may be shadowed by correct SDK-header decls; triage before those code
  paths go live. `xnet_send/xnet_recv` decl-vs-binary pop mismatch (16 vs 24)
  deserves a look even though MP currently works.
- `xemu_int.log` (127MB, untracked) + `artifacts/xemu_ram_full.bin` (64MB)
  are diagnostic leftovers; delete freely.
