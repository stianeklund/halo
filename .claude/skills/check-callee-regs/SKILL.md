---
name: check-callee-regs
description: Scan ported code for calls to unported functions that have implicit register arguments not yet annotated in kb.json. Run before porting any function that calls unported callees.
---

# Callee Register-Argument Hazard Check

Run this before porting any function to catch callees that pass arguments via
CPU registers instead of the stack. Missing `@<reg>` annotations cause silent
ABI mismatches and runtime corruption.

## Failure Mode

MSVC sometimes passes arguments through EAX, ECX, EDX, ESI, or EDI. Ghidra may
show these as `in_EAX` style locals. When ported C calls such a function without
the `@<reg>` annotation in `kb.json`, the compiler passes the argument on the
stack while the callee reads garbage from the register.

Canonical bug: `FUN_00115ba0` used EAX as its `bb` pointer argument. Missing
`bb@<eax>` left zlib inflate tables uninitialized and froze map selection.

## Workflow

1. Run `rtk python3 tools/audit/check_callee_reg_args.py`.
2. For every `MISSING @<reg>` entry, verify callee entry disassembly.
3. Confirm the register is read before written and is not a `_chkstk` frame-size probe.
4. Add the correct `@<reg>` annotation to the `kb.json` declaration.
5. Sync the baseline with `rtk python3 tools/audit/extract_reg_args.py --apply`.
6. Update C call sites to pass the register argument in declaration order.
7. Validate with `extract_reg_args.py --check`, ABI audit, and build.

## Do Not

- Do not use raw function pointer casts to original addresses.
- Do not use inline asm to load registers before a call.
- Do not infer the register solely from Ghidra parameter order.
- Do not remove existing `@<reg>` annotations.
