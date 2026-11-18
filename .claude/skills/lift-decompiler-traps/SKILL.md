---
name: lift-decompiler-traps
tier: agent
triggers: ["call site", "call-site", "add esp", "fstp", "x87", "cross product", "cross-product", "_ftol2", "_chkstk", "__seh", "_allmul", "intrinsic", "decompiler trap", "ghidra wrong", "struct field rotation", "cdecl", "arg hazard", "argument order", "arg order", "operand swap", "stack frame", "frame size", "buffer size", "undersized buffer", "local_", "memset", "memcpy", "stack alias", "buffer alias", "&local_"]
description: "Ghidra decompiler, argument, and frame traps: register aliasing, push-then-fstp, field rotation, ADD ESP grouping, NULL @<reg>, _chkstk, intrinsics. Use when checking call sites/frames."
---

# Decompiler Call-Site & Frame Hazards

**Invoke this skill when:**
- Verifying a call site or stack frame against disassembly before writing C
- The decompile shows a function receiving unexpected arguments or a 0-arg getter with extra args
- `ADD ESP, N` cleanup after a call doesn't match the callee's declared arg count
- A call passes what looks like a pointer but might be a float, or vice versa
- A callee name matches an MSVC intrinsic (`_ftol2`, `_chkstk`, `_allmul`, etc.)
- A function with `@<reg>` args produces wrong output with no crash, or uses NULL placeholders `(float *)0`
- Sizing a local buffer, seeing `_chkstk` in a frame, or passing `&local_XX` to an indexing callee

---

## 1. Call-Site Traps

### Trap 1 — Register aliasing: wrong variable in EBX/ESI/EDI
Ghidra loses track of callee-saved register values in long functions. It may substitute the wrong variable for EBX, ESI, or EDI at a call site far from where those registers were loaded.

**Detection:** In original disassembly, trace each PUSH backward to its last `MOV`/`LEA` register load. Do not trust Ghidra's variable name — read the actual `MOV EBX,[EBP+N]` or `LEA ESI,[EBP-N]` instruction.

### Trap 2 — Push-then-fstp: float argument disguised as pointer
When MSVC passes a float argument via the FPU stack:
```asm
PUSH <dummy_int>    ; allocate 4 bytes
FSTP [ESP]          ; store float over the dummy
```
Ghidra sees the PUSH and reports the dummy integer (often a pointer) as the argument.

**Detection:** When a CALL's argument looks like a pointer but preceding instructions are FPU ops (`FLD`, `FADD`, `FMUL`), look for `FSTP [ESP]`. The argument is that float value, not the PUSH'd dummy.

### Trap 3 — Struct field rotation: MSVC reorders stores for scheduling
MSVC reorders field assignments for pipeline scheduling. The decompile reassembles them in instruction order, producing wrong struct offsets.

**Rule:** Derive every struct field offset from `MOV [EBP±N]` in raw disassembly, not from decompiler ordering.

### Trap 4 — Cross-product operand swap
`cross(A, B)` and `cross(B, A)` produce nearly identical decompile output. Getting it backwards negates the vector, causing invisible geometry, flipped UV mapping, or reflected projections.

**Rule:** Always verify subtraction order against disassembly: `cross(A,B)[0] = A[1]*B[2] - A[2]*B[1]`.

### Trap 5 — Buffer-alias confusion: local_XX reads that are buffer fields
Ghidra names every stack offset as an independent `local_XX` variable, even when the offset falls inside a local buffer. After any CALL taking a buffer pointer, check whether subsequent `local_XX` reads are buffer fields (compute `EBP_offset - buffer_base_EBP_offset`).

---

## 2. Argument Hazards

### Hazard 1 — Ghidra cdecl arg mis-grouping (ADD ESP tell)
MSVC pushes args right-to-left. For `outer(get_x(), a, b, c)` where `get_x` is 0-arg, pushes for `c, b, a` happen **before** `CALL get_x`. Ghidra attributes them to `get_x`, leaving `outer` with only the getter's result.

**Detection — ADD ESP tell:**
Disassemble the function. Count `ADD ESP, N` after the outer CALL. `N/4` = total args belonging to `outer`. If `N/4` > what Ghidra gave `outer`, args were stolen by an inner getter.

### Hazard 2 — NULL placeholder for @<reg> args
After lifting, check for placeholder nulls: `grep -n '(float \*)0\|(void \*)0\|(int \*)0' src/<file>.c`.
**Never pass NULL for `@<reg>` params.** Find the actual value from `dump_caller_regsetup.py 0x<callee_addr>`.

### Hazard 3 — Caller-site register order swap
A callee declared `@<ecx>, @<eax>, @<ebx>` has thunk sending arg1→ECX, arg2→EAX, arg3→EBX. But a specific caller may load EBX before EAX.
**Detection:** Run `rtk python3 tools/audit/dump_caller_regsetup.py <callee_name_or_addr>`. Compare original GPR loads against the thunk convention and swap C arguments for callers that use non-canonical order.

---

## 3. Frame & Buffer Hazards

### Rule 1 — Derive buffer size from _chkstk frame, not Ghidra's guess
When first instruction is `_chkstk(EAX = N)`:
`true_frame_bytes = N` - known named scalars/structs = true buffer size.
Under-sizing buffers causes stack corruption when a callee writes near its tail.

### Rule 2 — &local_XX passed to a callee = must be contiguous buffer
When `&local_XX` is passed to a callee, check if the callee reads `param[N]` (N > 0) or does `memset(param, 0, K)`. If so, separate `local_XX` variables **must** be replaced by a contiguous array of at least `K` bytes.

### Rule 3 — static buffers are a _chkstk workaround
`_chkstk` is stubbed in `xbox_crt.c`. Convert `static` buffer workarounds back to stack buffers to improve VC71 scores (stack buffers match EBP-relative addressing).

---

## 4. MSVC Intrinsics Table

Never declare these in `kb.json` or call them as normal C functions:

| Address | Intrinsic | Write in C as |
|---|---|---|
| `0x1d90e0` | `_chkstk` | declare locals normally |
| `0x1d9068` | `_ftol2` | `(int)float_expr` |
| `0x1dd5c8` | `__SEH_prolog` | `__try / __except` |
| `0x1dd601` | `__SEH_epilog` | paired with prolog |
| `0x1dd620` | `_allmul` | `(int64_t)a * b` |
| `0x1dd660` | `_aullshr` | `(uint64_t)val >> shift` |
| `0x1dd680` | `_aullrem` | `(uint64_t)a % b` |
| `0x1dd770` | `_aulldiv` | `(uint64_t)a / b` |

## 5. Detail Moved From AGENTS.md (2026-09-02)

### SEH wrappers (`__SEH_prolog` / `__SEH_epilog`)
All 74 `__SEH_prolog` callers are LIBCMT/XAPILIB CRT helpers. Use
`__try { <body> } __except(1) { return 0; }` (or the appropriate error return).
The SEH is a safety net — in normal execution the `__try` body runs to
completion. `vc71_verify` will report ~55% match because the frame shape differs
(clang emits an inline SEH frame; the original uses compact thunks). This is
expected and accepted for these CRT wrappers. New source files go in
`src/halo/cseries/xbox_crt.c`; register the object as `XAPILIB:xbox_crt.obj` in
`kb.json`. See `docs/seh-handling.md`.

### Verify callee buffer sizes
Ghidra may under-size local buffers. When a lifted function passes a stack
buffer to a callee, check the callee's `memset`/init size in disassembly — it
reveals the true required size. Example: `FUN_0013fc20` (object placement init)
writes 0x88 bytes; Ghidra showed the caller's buffer as 0x30, causing a stack
overflow.

### MSVC stack-layout overlap hazard
When a lifted function calls an **unlifted** function by pointer (vtable,
callback, function table), the callee may read from offsets within a local array
that MSVC placed overlapping with other local variables. Our clang build uses a
different stack layout, so those offsets contain garbage instead of the expected
data.
- **Detection:** decompile the unlifted callee and check every offset it reads
  from the array parameter. Cross-reference with the original function's
  `[EBP±N]` disassembly to see if those offsets land on other locals.
- **Fix:** explicitly copy the required data into the array at the expected
  offsets before the call.
- **Example:** `FUN_0009fd30` passes `marker_buf` to creation physics (vtable
  `0x26ab10`). The callee reads position from `marker_buf+0x60`, which in the
  original MSVC layout overlaps with `local_position`. Fix was to copy
  `local_position` into `marker_buf+0x60` explicitly.

### Worked examples for the five call-site traps
- **Cross-product operand swap:** `cross(A,B)[0] = A[1]*B[2] - A[2]*B[1]`.
  Getting it backwards negates the vector, which can cause invisible geometry,
  flipped UV mapping, or reflected projections.
- **Buffer-alias confusion:** `FUN_000f90d0` had `damage_params` at EBP-0x8C
  (0xac bytes). After `FUN_00137d20(damage_params, ...)`, Ghidra showed
  `local_44` (EBP-0x44 = damage_params+0x48) and `local_40` (EBP-0x40 =
  damage_params+0x4C) as independent locals. The lift incorrectly read these
  from `col_result` instead of `damage_params`, causing wrong impact effects on
  all objects.

### XCALL type-mismatch example
`real_a_rgb_color_to_pixel32` (0x99530) returns `uint32_t` in EAX; an XCALL cast
`float(*)` read ST(0) garbage instead — the plasma-pistol overcharge orb was
invisible for weeks. Run `rtk python3 tools/audit/check_xcall_types.py` after
adding or modifying any XCALL macro.
