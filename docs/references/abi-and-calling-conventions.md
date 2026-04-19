# ABI and calling conventions

Assume 32-bit x86, MSVC-era Xbox codegen.

Check for:
- cdecl
- stdcall
- fastcall
- thiscall
- register-passed arguments where decompiler guesses are weak

Rules:
- Use PUSH count and cleanup behavior to constrain prototypes.
- Confirm return behavior from caller use, not just decompiler signatures.
- Watch for hidden this pointers and register-carried values.
- Treat interleaved setup and pre-push sequences carefully.
- Confirm operand widths exactly before choosing integer or pointer types.
- Avoid overcommitting signedness unless the binary supports it.
- Avoid overcommitting struct fields when raw offsets are the only certainty.

Reminder:
- In cdecl, the first PUSH is the last C argument.
