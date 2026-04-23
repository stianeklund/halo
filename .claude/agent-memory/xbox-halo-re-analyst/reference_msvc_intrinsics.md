---
name: MSVC compiler intrinsics in cachebeta.xbe
description: Complete catalog of MSVC runtime helpers with non-standard calling conventions that are dangerous to transcribe as normal C calls when lifting from Ghidra
type: reference
---

## MSVC Compiler Intrinsics / Runtime Helpers in cachebeta.xbe

These functions have non-standard calling conventions that Ghidra's decompiler
often misrepresents. They must NEVER be transcribed as normal C function calls.

### CRITICAL (high call count, non-standard ABI)

| Address    | Name             | Calls | Convention                                                      |
|------------|------------------|-------|-----------------------------------------------------------------|
| 0x1d9068   | _ftol2           | 228   | Reads st(0), returns int64 in EDX:EAX. Consumes FPU TOS.       |
| 0x1d90e0   | _chkstk          | 71    | Input: EAX = bytes needed. Modifies ESP. Returns via xchg.     |
| 0x1dd5c8   | __SEH_prolog     | 74    | Modifies ESP, EBP, fs:[0]. Establishes SEH frame.              |
| 0x1dd601   | __SEH_epilog     | 73    | Restores fs:[0], pops EBX/ESI/EDI, does leave; push ecx; ret.  |

### 64-BIT ARITHMETIC (stdcall ret 0x10)

| Address    | Name             | Calls | Convention                                                      |
|------------|------------------|-------|-----------------------------------------------------------------|
| 0x1dd620   | _allmul           | 10    | stdcall(A_lo, A_hi, B_lo, B_hi). Returns int64 in EDX:EAX.    |
| 0x1dd660   | _aullshr         | 1     | Value in EDX:EAX, shift count in CL. Returns in EDX:EAX.      |
| 0x1dd680   | _aullrem         | 1     | stdcall(A_lo, A_hi, B_lo, B_hi). Returns uint64 in EDX:EAX.   |
| 0x1dd770   | _aulldiv         | 1     | stdcall(A_lo, A_hi, B_lo, B_hi). Returns uint64 in EDX:EAX.   |
| 0x1dd7e0   | _allshr          | 0     | Value in EDX:EAX, shift count in CL. Returns in EDX:EAX. Dead code. |

### SEH INFRASTRUCTURE (not directly called from game code)

| Address    | Name                | Notes                                                        |
|------------|---------------------|--------------------------------------------------------------|
| 0x1dbdec   | __except_handler3   | SEH exception dispatch. Referenced as data ptr by __SEH_prolog. |
| 0x1dbd36   | _local_unwind2      | SEH stack unwinding. Manual fs:[0] manipulation.             |
| 0x1dbcf4   | _global_unwind2     | SEH global unwind.                                           |
| 0x1dbdca   | _NLG_Notify         | Non-local goto notification.                                 |

### NOT PRESENT in this binary

- _ftol (classic) -- only _ftol2 exists (no fnstcw [esp-2] pattern)
- _CIsin, _CIcos, _CIsqrt, _CIpow, _CIatan2 -- NOT USED. Compiler inlines fsin/fcos/fsqrt directly (122/186/many occurrences). CRT math functions (sqrt at 0x1d9c2b, acos at 0x1dbc26) use normal cdecl with doubles on stack.
- _allshl -- not present (no shld edx, eax, cl pattern found)
- _alldiv, _allrem (signed) -- not present
- __security_check_cookie -- not present (no stack canaries)
- _alloca_probe / _alloca_probe_16 -- not present (Xbox has no guard pages)
- _EH_prolog3 / _EH_prolog3_GS -- not present (VS2005+ only)
- _RTC_CheckStackVars -- not present (no runtime checks in release build)

### Why each is dangerous for lifting

- **_ftol2**: Ghidra shows `(int)var` or `_ftol2(var)` -- but the real ABI passes the float in st(0), not as a stack argument. Lifted C code must use a cast `(int)float_expr` which the compiler will emit as `call _ftol2` automatically. Writing `_ftol2(x)` would double-convert.
- **_chkstk**: Takes size in EAX, modifies ESP. Ghidra may show it as a normal call. In lifted code, just declare large locals normally -- the compiler emits _chkstk automatically.
- **__SEH_prolog/__SEH_epilog**: Establish/tear down structured exception frames. Lifted code should use `__try/__except` or avoid SEH entirely for game code.
- **_allmul/_aulldiv/_aullrem/_aullshr/_allshr**: 64-bit arithmetic on 32-bit. Use `unsigned __int64` / `__int64` operations in C -- compiler emits these automatically. Calling them directly would use wrong ABI.
- **__except_handler3**: Never called directly -- it's a function pointer registered in the SEH chain.

### Identification signatures

- _ftol2: `push ebp; mov ebp, esp; sub esp, 0x20; and esp, 0xfffffff0; fld st(0); fst [esp+18h]; fistp qword [esp+10h]`
- _chkstk: `test eax, eax; je +N; neg eax; add eax, esp; add eax, 4; xchg esp, eax; mov eax, [eax]; push eax; ret`
- __SEH_prolog: `push 0x1dbdec; mov eax, fs:[0]; push eax; mov fs:[0], esp`
- __SEH_epilog: `mov ecx, [ebp-10h]; mov fs:[0], ecx; pop ecx; pop edi; pop esi; pop ebx; leave; push ecx; ret`
- _allmul: `mov eax, [esp+8]; mov ecx, [esp+10h]; or ecx, eax; ... ret 0x10`
- _aullshr/_allshr: `cmp cl, 0x40; jae; cmp cl, 0x20; jae; shrd eax, edx, cl; shr/sar edx, cl; ret`
