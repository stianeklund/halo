---
name: crash-reg-arg-shift-use-after-free
description: A missing @<reg> annotation on a ported function shifts every stack param by one slot, making it read the wrong arg as a datum handle — surfaces as data.c:78 "unused or changed" use-after-free-looking asserts
metadata:
  type: feedback
---

A ported function whose kb.json decl OMITS a register-arg annotation that the
original ABI used will read the WRONG stack slot for every parameter, because
the lifted impl is plain cdecl (all params on stack) while the original caller
passes arg1 in a register and pushes fewer stack args.

**Why:** The `@<reg>` annotation drives `generate_reverse_thunk` in patch.py —
the redirect at the original addr converts register args to cdecl stack form
before calling our impl. Without it, the redirect is a plain `push impl; ret`,
the caller's register arg is DISCARDED, and stack args land in the wrong slots.
Concrete: `FUN_00018b90(unit_handle@<eax>, actor_handle, scenario_index, ...)`
was declared WITHOUT `@<eax>`. The original caller (FUN_00019110) passed
unit_handle in EAX and pushed [actor_handle, scenario_index, ...]. The lifted
impl read [ebp+8]=unit_handle (got actor_handle), [ebp+0xc]=actor_handle (got
scenario_index — a small index like 13). `datum_get(actor_data, 13)` with a
salt-0 raw index → data.c:78 "actor index #13 (0xd) is unused or changed".

**Diagnostic tells:**
- Assert is the LINE-78 datum_get assert ("unused or changed"), NOT the
  identifier_zero_invalid assert → the array permits salt-0 wildcard lookups.
  So salt-0 is a legal RAW INDEX, not a malformed handle. A small salt-0 value
  (#13) reaching datum_get = a non-handle value mis-fed as a handle. datum salts
  are NEVER 0 (datum_initialize wraps 0→0x8000), so salt-0 is provably not a
  live handle.
- The bad value equals a struct field that is the NEXT cdecl arg over from the
  intended handle (off-by-one-slot).

**How to apply:** When a ported function asserts in datum_get with a tiny/raw
handle, (1) symbolize the chain (see [[crash-symbolization-method]]),
(2) disassemble the ORIGINAL function prologue from the XBE (skip the 6-byte
`push impl; ret` redirect — real body starts at addr+6) and note which EBP
offset / register holds each arg, (3) compare to the kb.json decl's implied
cdecl layout. If the original uses a register for arg1, add `@<reg>` to the
kb.json decl AND to tools/kb_reg_baseline.json (run extract_reg_args.py --check
to confirm no drift). Verify the fix by running patch.py to a /tmp output and
disassembling the generated reverse thunk: it should `push [esp+N]` the stack
args then `push %reg` the register arg, then `call impl`. NO C source change is
needed — only the ABI annotation. VC71 verify is irrelevant (C body unchanged);
the per-function delinked ref may be absent — that pipeline FAIL is an artifact.
