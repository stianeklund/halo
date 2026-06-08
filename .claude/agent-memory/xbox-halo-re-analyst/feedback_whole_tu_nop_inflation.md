---
name: whole-tu-nop-inflation
description: vc71_verify resolves to the whole-TU obj when it contains the symbol, inflating the denominator with the next function's NOP-sled+labels; export a per-function ref and compare_obj directly
metadata:
  type: feedback
---

`vc71_verify.py choose_unit` returns the whole-TU `<obj>.obj` whenever it
contains the target symbol (the per-function `delinked/functions/<hex8>.obj` is
only a *fallback* when no unit has the symbol). For a function followed by a NOP
alignment sled that contains internal `LAB_*` labels, `compare_obj`'s
`_trim_unlabeled_bleed` can't trim the sled (it requires "no internal label"),
so the reference denominator includes ~15-75 trailing NOP/retl bytes from the
gap before the next function. This crushed `FUN_001a0680` to a reported 61.5%
(82/152) when the TRUE per-function match is 83.2% (82/91). (bipeds.obj, 2026-06-06)

**Why:** the whole-TU symbol boundary is the *next function symbol*, not the
RET; the gap (alignment NOPs + dead label stubs) is attributed to the target.

**How to apply:**
- When `vc71_verify --show-diffs` shows a wall of trailing `+ nop`/`+ retl` on
  the reference side, the % is NOP-inflated. Get the true number by exporting a
  per-function ref (`mcp__ghidra-live__export_delinked_object selection_mode=range
  range=<start>-<end>` using the Ghidra function Body end, NOT the next symbol)
  to `delinked/functions/<hex8>.obj`, then `compare_obj.py <candidate.obj>
  delinked/functions/<hex8>.obj -f FUN_<addr>`.
- Report BOTH numbers and which ref produced each — an orchestrator that
  re-verifies with vc71_verify's default will see the inflated number.
- Do NOT modify the shared resolution logic (multi-agent repo).
