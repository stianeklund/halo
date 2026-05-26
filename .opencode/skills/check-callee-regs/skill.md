---
name: check-callee-regs
description: Scan ported code for calls to unported functions that have implicit register arguments not yet annotated in kb.json. Run before porting any function that calls unported callees.
---

# Callee Register-Argument Hazard Check

Run this before porting any function to catch callees that pass arguments via
CPU registers instead of the stack — a silent ABI mismatch that causes
data corruption without compile-time errors.

## The failure mode

MSVC sometimes compiles functions to receive an argument via EAX, ECX, EDX,
ESI, or EDI instead of the stack. Ghidra labels these `in_EAX` etc. and they
appear as local variables that are immediately dereferenced at the top of the
function. When ported C calls such a function without the `@<reg>` annotation
in kb.json, the compiler passes the argument on the stack while the callee
reads a garbage value from the register.

**Canonical bug:** `FUN_00115ba0` (zlib inflate table builder) used EAX as its
`bb` (bit-count pointer) arg. Three ported callers omitted `@<reg>`, leaving
`s->trees.bb` and `s->trees.tb` uninitialized → DTREE mode read 0xFDFDFD as
a code-length, computed an out-of-range `inflate_mask` index, and froze the
game on every map selection.

## How to use

### 1. Run before lifting (quick check)

```bash
rtk python3 tools/audit/check_callee_reg_args.py
```

Output sections:
- `MISSING @<reg>` — unported callees whose first instructions read a register
  before writing it. **Each entry must be verified before porting its caller.**
- `[chkstk?]` — EAX-read-before-write that looks like a `_chkstk` frame-size
  probe (EAX = frame size, NOT a user argument). Skip unless disassembly
  confirms otherwise.

Exit code 0 if clean, 1 if `MISSING @<reg>` entries found (use `--check` for CI).

### 2. Verify a flagged callee

For each flagged function, open its Ghidra decompile and look at the variable
declarations at the top of the function body. Any variable named `in_EAX`,
`in_ECX`, `in_EDX`, `in_EBX`, `in_ESI`, or `in_EDI` that is **immediately
dereferenced** (`*in_EAX`, `*in_ECX`) is a register argument.

Cross-check with disassembly: the function's first few instructions should
include something like `mov edi, eax` (save register arg to callee-save reg)
or `mov [ebp-N], eax` (spill to local).

### 3. Add the annotation to kb.json

Once confirmed, add the register to the function's `decl` in kb.json:

```json
"decl": "int FUN_00115ba0(unsigned int *bb@<eax>, int *c, unsigned int n, ...);"
```

Then run `extract_reg_args.py --apply` to update the baseline:

```bash
rtk python3 tools/audit/extract_reg_args.py --apply
```

### 4. Update the call site

In the ported caller, pass the register argument as the **first** C argument
(the thunk will load it into the correct register before calling the original):

```c
// Before (wrong — EAX unset, bb lands on stack as param_1):
FUN_00115ba0(c, 0x13, 0x13, 0, 0, (int *)tl, td, &local_8, iVar1);

// After (correct — thunk sets EAX=bb, rest go on stack):
FUN_00115ba0((unsigned int *)bb, c, 0x13, 0x13, 0, 0, (int *)tl, td, &local_8, iVar1);
```

## False positives

The scanner uses disassembly heuristics; some results are not real:

| Pattern | Cause | Action |
|---------|-------|--------|
| `[chkstk?]` tag | EAX holds frame size before `_chkstk` call | Skip unless confirmed |
| SDK addresses auto-filtered | D3D, Bink, XNet functions | Not shown (filtered by address range) |
| REP string functions | `MOVSB`/`SCASB` implicitly reads ECX/EDI/ESI | Unlikely after SDK filter; verify disasm |

## Integration with the lift workflow

Run this tool as part of the pre-lift research phase — after identifying the
target function and before writing any C code:

```
/auto-lift → check callee addresses in source → check-callee-regs → /lift
```

For functions with many callees (render, physics, networking), run this once
per source file. Each newly flagged callee needs a Ghidra verification before
its caller can be safely ported.

## Known findings (as of last scan)

16 game-code functions currently flagged; all require Ghidra verification
before their callers are ported:

- `game_engine_get_variant` (0xa9350) — @<ecx>
- `object_update` (0x1444f0) — @<ecx>
- `object_get_next_cluster` (0x13d5f0) — @<ecx>
- `lruv_block_get_address` (0x11da00), `FUN_0011de10` (0x11de10) — @<ecx>
- `FUN_00138900` (0x138900) — @<ecx> (damage application)
- `ai_conversation_update` (0x46cb0) — @<eax>
- `ticks_to_unicode_time_string` (0xab450) — @<eax>
- `hs_tokens_enumerate` (0xc4580) — @<ecx>
- `xbox_texture_cache_steal_memory` (0x1bea30) — @<eax>
- Several actor/encounter functions — @<ecx>/@<edi>/@<esi>

Re-run the scanner after each porting session to catch newly discovered callees.
