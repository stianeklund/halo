---
name: lift-decompiler-traps
description: Ghidra decompiler traps at call sites — register aliasing, push-then-fstp floats, struct field rotation, cross-product operand swap, buffer-alias confusion, and MSVC intrinsics mistaken for real calls. Consult before verifying any call site in a new lift.
---

# Decompiler Call-Site Traps

**Invoke this skill when:**
- Verifying a call site against disassembly before writing C
- The decompile shows a function receiving unexpected arguments
- A call passes what looks like a pointer but might be a float, or vice versa
- A callee name matches an MSVC intrinsic (`_ftol2`, `_chkstk`, `_allmul`, etc.)
- The decompile shows a cross product, subtraction, or negation of vectors

These are traps Ghidra commonly gets wrong. Source: CLAUDE.md §pitfalls.

---

## Trap 1 — Register aliasing: wrong variable in EBX/ESI/EDI

Ghidra loses track of callee-saved register values in long functions. It may
substitute the wrong variable for EBX, ESI, or EDI at a call site far from
where those registers were loaded.

**Detection:** In the original disassembly, trace each PUSH backward to its
last `MOV`/`LEA` register load. Do not trust Ghidra's variable name for
EBX/ESI/EDI — read the actual `MOV EBX,[EBP+N]` or `LEA ESI,[EBP-N]`
instruction that set the register.

```bash
# Disassemble the function and grep for the register loads:
# MOV EBX, ... or LEA EBX, ... before the CALL of interest
```

---

## Trap 2 — Push-then-fstp: float argument disguised as pointer

When MSVC passes a float argument via the FPU stack, it emits:
```
PUSH <dummy_int>    ; allocate 4 bytes
FSTP [ESP]          ; store float over the dummy
```

Ghidra sees the PUSH and reports the *dummy integer* (often a pointer) as the
argument. The actual argument is a float computed by the preceding FPU
instructions.

**Detection:** When a CALL's argument looks like a pointer but the preceding
instructions are FPU ops (`FLD`, `FADD`, `FMUL`, etc.), look for the `FSTP
[ESP]` pattern. The argument is that float value, not the PUSH'd dummy.

---

## Trap 3 — Struct field rotation: MSVC reorders stores for scheduling

MSVC reorders field assignments for pipeline scheduling. The decompile
reassembles them in instruction order, producing wrong struct offsets.

**Rule:** Derive every struct field offset from `MOV [EBP±N]` in the raw
disassembly, not from the decompiler's ordering. Never trust the order of
field assignments in the decompile output.

```bash
# Disassemble the region with stores:
# MOV [EBP-N], val  → field at EBP-N
# Cross-check: compute offset as (EBP_param - field_EBP_offset)
```

---

## Trap 4 — Cross-product operand swap

`cross(A, B)` and `cross(B, A)` produce nearly identical decompile output.
The FLD/FMUL order before FSUBP differs but the component names look the same.
Getting it backwards negates the vector.

**Rule:** Always verify the subtraction order against disassembly:
```
cross(A,B)[0] = A[1]*B[2] - A[2]*B[1]
```

If the disasm shows `FLD A[2]; FMUL B[1]; FLD A[1]; FMUL B[2]; FSUBP` then
the result is `A[1]*B[2] - A[2]*B[1]` = correct.
If it shows `FLD B[2]; FMUL A[1]; ...` then the operands are swapped.

**Consequence:** Reversed cross product = negated normal = invisible geometry,
flipped UV mapping, or reflected projections.

---

## Trap 5 — Buffer-alias confusion: local_XX reads that are buffer fields

Ghidra names every stack offset as an independent `local_XX` variable, even
when the offset falls inside a local buffer. After any CALL that takes a buffer
pointer, check whether subsequent `local_XX` reads are buffer fields.

**Procedure:**
1. Find the buffer's base EBP offset from the CALL that initializes it.
2. For any `local_XX` read after the call, compute: `EBP_local_offset - buffer_base_EBP_offset`
3. If the result is in `[0, buffer_size)`, the read is from the buffer — use `buffer[offset/4]`, not `local_XX`.

**Example:** `damage_params` at EBP-0x8C (0xac bytes). After `FUN_00137d20(damage_params, ...)`,
Ghidra showed `local_44` (EBP-0x44 = damage_params+0x48) as an independent local.
Lifting it as a separate variable read `col_result` instead of `damage_params+0x48`.

---

## Trap 6 — MSVC intrinsics mistaken for real function calls

Ghidra shows these as regular calls, but they have non-standard ABIs. **Never
transcribe them as C function calls.** Use the C idiom instead:

| Address | Intrinsic | Ghidra shows | Write in C instead |
|---------|-----------|--------------|-------------------|
| 0x1d90e0 | `_chkstk` | `regparm(1)` call | declare locals normally (now stubbed; no action needed) |
| 0x1d9068 | `_ftol2` | `_ftol2(var)` | `(int)float_expr` |
| 0x1dd5c8 | `__SEH_prolog` | mangled params | `__try { ... } __except(1) { return 0; }` |
| 0x1dd620 | `_allmul` | 4-arg call | `(int64_t)a * b` |
| 0x1dd660 | `_aullshr` | register call | `(uint64_t)val >> shift` |
| 0x1dd680 | `_aullrem` | 4-arg call | `(uint64_t)a % b` |
| 0x1dd770 | `_aulldiv` | 4-arg call | `(uint64_t)a / b` |

**Detection:**
```bash
# After writing a lift, grep for any of these addresses:
grep -n '0x1d9068\|0x1dd5c8\|0x1dd620\|0x1dd660\|0x1dd680\|0x1dd770\|FUN_001d9068\|FUN_001dd' src/<file>.c
```

Any match = intrinsic that was transcribed as a call. Replace with the C idiom.
`check_lift_hazards.py` catches most of these (ERROR level) but not SEH prolog/epilog.
