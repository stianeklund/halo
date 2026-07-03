---
name: halt-and-catch-fire-1029a0
description: halt_and_catch_fire 0x1029a0 fatal-halt renderer lift facts + rasterizer thunk table + 0x17c930 forwarder trap
metadata:
  type: reference
---

2026-07-03 verified `halt_and_catch_fire` (0x1029a0, main.obj, src/halo/main/main.c) against pristine `halo-patched/cachebeta.xbe` (capstone; it is the tool tooling treats as pristine). Fatal blue-screen renderer: re-entry guard byte @0x46e392, silences rumble on 4 pads, resolves interface/fallback 'font' tag, then an infinite `do{}while(true)` loop rebuilding a throwaway camera/frustum into `unknown_global_camera` (0x506550, 0x15 dwords=sizeof(camera_t)) and drawing build-version + `error_get()` (0x8f220 → 0x5aa8e8 msg). window_parameters_t = 0x258 (600) bytes; camera@+0x08, frustum[127]@+0x5c; the decompiler's `local_8c..local_70` "locals" are buffer fields frustum[100..106] (buffer base EBP-0x274).

FPATAN: `render_camera_get_adjusted_field_of_view_tangent(1.3962634f)` [0x3fb2b8c2 is a GENUINE float arg pushed as raw bits, NO push-then-fstp] → `fmul dword[0x25afcc]` → `fld qword[0x2573d8]` → `fpatan` = atan2(y=tangent*0x25afcc, x=*(double*)0x2573d8); result doubled (`fadd st,st`) = vertical_fov. z_near/z_far are raw dword copies from 0x325694/0x325698 (NOT int→float).

TRAP (found+fixed a pre-drafted-lift bug): the loop tail calls **0x17c930** with (0,0), which is `rasterizer_dynamic_lit_geometry_draw` — a FRAMELESS forwarder (`push ebp;mov ebp,esp;pop ebp;jmp 0x157e40`). Its real body 0x157e40 USES both args (edi=[ebp+8] tests +0x2c, esi=[ebp+0xc] reads word). This is NOT `render_frame_present` (0x184dc0). kb decl updated `void(void)` → `void(void*,void*)`. Rasterizer thunk table near it: 0x17c7d0→jmp 0x1559a0 (_rasterizer_reset_state), 0x17c8c0→jmp 0x1559d0 (_rasterizer_windows_begin), 0x17c900→jmp 0x158f90, 0x17c910→jmp 0x155a40 (_rasterizer_windows_end), 0x17c920→jmp 0x155a70 (_rasterizer_frame_end); 0x17c7e0 (rasterizer_frame_begin) / 0x17c8d0 (rasterizer_window_begin) are real bodies. Calling the canonical name (which kb maps to the thunk address) is byte-faithful.

Guard callee FUN_001d980b: cdecl 1 int arg (`push [esp+0xc]`, forwards (arg,0,0) → 0x1d9761), decl `void FUN_001d980b(int param_1)`. FUN_00184980: reads BYTE arg (`mov bl,byte[ebp+8]`) → `char param_1`. Guard path does `FUN_001d980b(0); int3;` and in the original does NOT ret (falls into padding); C impl uses `__asm__ volatile("int3"); return;` — immaterial (debugger-only).

VERIFICATION GAP: NO delinked object DEFINES FUN_001029a0 (UNDEF/sec 0 in delinked/main.obj and everywhere) → no VC71 byte-match reference; equivalence 100/100 errors (non-leaf, external `_error_get` reloc, infinite loop). Both are infra gaps, not defects; correctness proven by direct capstone disasm audit. To get a VC71 ref, export a delinked obj covering 0x1029a0 via ghidra-live MCP (not available in the analyst subagent tool set on 2026-07-03).
