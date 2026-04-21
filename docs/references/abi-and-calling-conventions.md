# ABI and calling conventions

Assume 32-bit x86, MSVC-era Xbox codegen.

## Calling conventions present in the XBE

- **cdecl** — caller cleans stack. First PUSH is last C argument.
- **stdcall** — callee cleans stack (RET N). Cast function pointers as
  `__stdcall` or ESP will drift.
- **fastcall** — first two args in ECX, EDX; callee cleans the rest.
- **thiscall** — `this` in ECX; callee cleans.
- **Custom register-arg** — one or more args passed in arbitrary registers
  (EAX, ECX, EDX, EBX, ESI, EDI, or sub-registers like AX, SI, AL).
  These are annotated in kb.json as `@<reg>` (e.g. `int handle@<eax>`).

## Prototype inference rules

- Use PUSH count and cleanup behavior to constrain prototypes.
- Confirm return behavior from caller use, not just decompiler signatures.
- Watch for hidden this pointers and register-carried values.
- Treat interleaved setup and pre-push sequences carefully.
- Confirm operand widths exactly before choosing integer or pointer types.
- Avoid overcommitting signedness unless the binary supports it.
- Avoid overcommitting struct fields when raw offsets are the only certainty.
- The Ghidra decompiler lies about operand sizes (int vs int16_t). Always
  verify with raw disassembly.

## Register-arg (`@<reg>`) functions — decision tree

`@<reg>` annotations in kb.json describe the **original XBE ABI** — a fact
about the binary. The build uses them to generate thunks that bridge between
the original register-arg convention and standard cdecl C.

There are two directions:

### Forward thunk: your C code calls an original XBE function

When the disassembly before a CALL shows registers being loaded
(`MOV EAX, ...`, `LEA ESI, [...]`) that are NOT pushed onto the stack,
those are register args.

**What to do:**
1. Add the callee to kb.json with `@<reg>` annotations on the correct
   parameters. This is safe even if the callee is not yet reimplemented.
2. Call the function by name from your C code. The build generates a
   forward thunk that moves your cdecl stack args into the correct
   registers before jumping to the original XBE code.

**What NOT to do:**
- Do not use raw function pointer casts (`((void (*)(int))0x1af6b0)(...)`).
  They silently drop register args and cause crashes.
- Do not use inline assembly to set up registers and call the target.
- Do not write separate `.s` assembly files.

### Reverse thunk: original XBE code calls your reimplemented C function

When you reimplement a function that has `@<reg>` annotations, the build
generates a reverse thunk at the original XBE address. This thunk moves
register args onto the stack in cdecl order, then calls your C function.
Original XBE callers hit the thunk transparently.

**Your C function uses a normal cdecl signature.** The `@<reg>` annotation
in kb.json tells the build how to generate the thunk; your C code does not
need to know about registers at all.

### The immutability rule

`@<reg>` annotations describe a permanent fact about the original binary.
They are **never removed or changed**, regardless of reimplementation status.

- Renaming a function: fine. Change the name, keep `@<reg>` unchanged.
- Retyping parameters: fine. Change types, keep `@<reg>` slot assignments.
- Porting the body to C: fine. The reverse thunk becomes dead code once all
  callers are also ported, but it costs nothing to keep.
- Removing `@<reg>`: **forbidden**. The build enforces this via
  `tools/kb_reg_baseline.json`. Any mismatch is a hard build failure with
  no escape hatch.

The reverse thunk is ~20 bytes of dead code in the fully-ported case.
Removing it saves nothing and risks ABI breakage at original-XBE call
boundaries.
