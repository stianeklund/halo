---
name: auto-lift-checklist
version: 1
description: Compact non-mechanical judgment checklist for memoryless auto-lift agents.
---

# Auto-lift judgment checklist v1

Mechanical tooling already owns source presence, KB extraction, fingerprints,
cache validity, fragment/SEH/kernel pre-screening, cap rules, build, ABI, hazard,
and VC71 measurements. Do not re-derive those results in prose.

Before implementation:

1. Read the immutable Ghidra artifact and current target declaration.
2. For each CALL, map binary argument sources to C expressions. Verify cdecl
   push order, register aliases, and push-then-fstp float slots.
3. For output buffers/structs, verify destination offsets and callee write size.
4. Preserve copied loop parameters, side-effect order, odd branches, and x87
   subtraction/cross-product operand order.
5. Use C89 and existing types. Never change `@<reg>` assignments or guess fields.

Escalate on demand:

- Use `lift-decompiler-traps` for ambiguous stack aliases, frames, argument
  lowering, or decompiler/control-flow conflicts.
- Use `lift-silent-bugs` for visual/color/position/effect discrepancies.
- A missing specific call-site fact permits one bounded Ghidra query. A valid
  bundle with complete evidence does not.

After implementation, preserve every existing build, ABI, hazard, VC71,
equivalence/runtime, reviewer, and commit gate. Publish the current score
context through `research_bundle.py prepare --current-attempt`.
