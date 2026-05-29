---
name: thunk-signature-propagation
description: Correcting a callee's decl arg count breaks parameterized tail-call thunks; fix the thunk decl + forward the arg
metadata:
  type: feedback
---

When you correct a function's kb.json decl to add an argument (e.g. `void(void)` → `void(int)`), grep src/ for ALL callers BEFORE rebuilding — a JMP-forwarding tail-call thunk that calls it with no args will fail to compile.

**Why:** Tail-call thunks have the shape `PUSH EBP; MOV EBP,ESP; POP EBP; JMP target`. Because the dispatch is a JMP (not CALL), the thunk forwards its own stack args to the target unchanged. So the thunk's true signature equals the target's. If the target was originally mis-declared `void(void)`, the thunk was too, and both have call sites assuming zero args. Correcting only the target's decl breaks the thunk's `target()` call ("too few arguments").

**How to apply:** After any kb decl arg-count correction, `rtk rg '<FUN_name>\b' src/`. For each tail-call thunk caller: (1) verify it's a JMP-forwarder via Ghidra disasm (`PUSH EBP/MOV EBP,ESP/POP EBP/JMP target`), (2) confirm its own binary caller pushes the arg (`get_function_xrefs` → read call site for `PUSH reg; CALL thunk; ADD ESP,4`), (3) correct the thunk's kb decl to match the target signature, (4) update the thunk's C body to forward the param. Concrete case: FUN_00172590 (rasterizer_xbox_shadows) takes `int param_1`; thunk FUN_0017ccc0 (decals.c) forwards it; binary caller FUN_00123ed0 pushes `LEA [EBP-0x138]`. See also [[maintain-py-invocation]].
