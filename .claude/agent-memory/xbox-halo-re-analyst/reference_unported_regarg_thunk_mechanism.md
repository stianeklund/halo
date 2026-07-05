---
name: unported-regarg-thunk-mechanism
description: How kb.json resolves calls to UNPORTED callees (plain-cdecl via halo.xbe.lib; @reg via naked thunk calling raw literal address — no recursion)
metadata:
  type: reference
---

Calling an UNPORTED kb.json callee by name from ported C is safe for both cdecl and @<reg>
callees — verified by reading tools/analysis/knowledge.py + tools/build/patch.py, and by a
clean build (2026-07-05, FUN_0001b750 lift in action_vehicle.obj).

- Plain cdecl unported callee (e.g. FUN_0001aeb0, 11 stack args): NO thunk emitted. The
  symbol is exported in halo.xbe.lib via build_def() → resolves to the ORIGINAL XBE address.
  Call by name = direct call to original code. Just give it a correct cdecl decl in kb.json.

- @<reg> unported callee (e.g. FUN_0001b280, @ecx+@eax+6 stack): knowledge.py
  build_thunks() emits a `__attribute__((weak, section(".thunks")))` naked thunk named
  after the function for EVERY decl containing `@<`, regardless of `ported`. The thunk
  marshals C stack args → registers and does `call *fn__xbe` where
  `fn__xbe = (void*)0x<addr>` — a RAW LITERAL ADDRESS, not the symbol. So there is NO
  recursion: it jumps straight to the original code. build_def() SKIPS reg-thunk funcs
  (not exported to .def), so the weak thunk is the only definition of the symbol.
  Requirements: (1) add the @<reg> decl to kb.json, (2) add the full decl string to
  tools/kb_reg_baseline.json INSIDE the `functions` dict (patch.py validate_reg_arg_annotations
  hard-fails otherwise; top-level addr keys are auto-migrated but the canonical spot is
  `functions`). Do NOT set `ported` on it (leave absent) — it's external, nothing to deactivate.

CORRECTION to older note "thunks for unimplemented @<reg> → infinite recursion": the current
generator calls the raw address, so unported @reg is NOT recursive. CAVEAT: FUN_0001b280 is
the FIRST unported @reg callee in kb.json (before this there were ZERO). Build compiles the
thunk cleanly; the thunk's runtime behavior for an unported target is not yet on-box verified
(the FUN_0001b750 path is gated behind vehicle-entry conditions). Treat static analysis as
strong-but-unproven until an on-box exercise of the enter-vehicle action confirms it.

Reg-thunk arg order: declare reg params FIRST (index 0 = @ecx actor, index 1 = @eax object),
then the stack args in the original left-to-right push order. Thunk pushes stack args reversed
(cdecl) and loads regs accounting for the push offset — matches the original frame exactly.
