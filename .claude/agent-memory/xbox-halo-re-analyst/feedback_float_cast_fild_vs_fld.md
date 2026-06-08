---
name: float-cast-fild-vs-fld
description: (float)ptr[N] on an undefined4*/uint* emits int->float FILD; cast the pointer to float* for FLD when the dwords are really floats
metadata:
  type: feedback
---

When Ghidra types a struct pointer as `undefined4*` / `uint*` and you transcribe
`(float)puVar4[N]`, the C compiler emits an **integer-to-float conversion**
(`FILD dword ptr`, plus an unsigned sign-fixup `test/jge/fadds 0x0` branch when the
pointer is unsigned). If the dwords at those offsets are actually **floats**
(e.g. a basis/rotation matrix, a position vector, any IEEE-754 field), this is
wrong codegen: the original loads them with `FLD` (float load), not `FILD`.

**Fix:** cast the pointer once to `float *` and index that:
```c
float *basis = (float *)biped_obj;   /* biped_obj is unsigned int * */
cross[0] = basis[0xe] * basis[0xa] - basis[0xb] * basis[0xd];
```
Do NOT write `(float)biped_obj[0xe]` — that is a numeric conversion of the integer
value, corrupting the float and emitting FILD.

**Why:** A `(float)` cast of an integer lvalue is a *value* conversion. A
`(float*)` pointer cast reinterprets the *bits* as a float, which is what loading
a stored float requires. Ghidra's `(float)puVar4[N]` notation on an `undefined4*`
means "load the float at this dword," not "convert the integer." See also the
related bit-reinterpret rule (use a pointer/union, never a numeric cast).

**How to apply:** Whenever a lift reads `(float)<ptr>[N]` where `<ptr>` is typed
`undefined4*`/`uint*`/`int*` and the field is a float, introduce a `float *`
alias. Detect after VC71 verify: a `fildl`/`FILD` on your side vs `flds`/`FLD`
on the reference (and an extra `test reg,reg; jge; fadds 0x0` triplet per operand)
is this bug. On biped_fix_position (0x1a1430) it cost 68.1% -> 79.3% VC71 in one
fix. Recurs on every struct-of-floats that Ghidra typed as `undefined4*`.

Related: [[feedback_check_disasm]] (decompiler lies about types — verify in disasm).
