---
name: int16-counter-width-ceiling
description: int16_t loop counters cause a VC71 codegen-width mismatch (MSVC 32-bit arith vs original 16-bit 66-prefixed) — a structural ceiling, not a lift bug
metadata:
  type: feedback
---

When a lifted function uses an `int16_t` loop counter that the original
narrows-on-store (e.g. `*(int16_t*)addr = count` after a `do/while` count loop),
VC71 verify can sit a few points below 90% purely from register-width codegen
divergence. Do NOT widen `int16_t`→`int` to chase the score.

**Why:** The original Halo binary frequently emits 16-bit arithmetic on the
counter — `XOR CX,CX` / `INC CX` (`66 33 c9`, the operand-size-prefixed form).
Our clang/MSVC-7.1 compile of the same C often emits 32-bit loop arithmetic
(`xorl %ecx` / `incl %ecx`) with only the final store narrowed to 16-bit. Same
C semantics, same behavior, different register width. Widening the C type does
NOT reliably coax the `66` prefix out — and it changes nothing about behavior.

**How to apply:** When VC71 sits in [85,90)% and `--show-diffs` shows the only
divergences are (a) `xorl/incl %ecx` vs `xorw/incw %cx` width pairs, (b) NOP-pad
placement (`leal (%ebx),%ebx`, `leal (%esp),%esp` — alignment, uncontrollable
from C), and (c) inner-loop pointer-arith hoisting, then the lift is faithful
and this is a structural/codegen ceiling. Confirm faithfulness by reading the
live XBE bytes (`read_memory`) and matching control-flow + call ABI against the
disassembly. Classify `needs_review`, note permuter-territory but expect a
modest ceiling. Verified on ai_communication_initialize (0x42a30): 88% VC71,
4/102 insns, all width+alignment, behaviorally exact to the live binary.

**Diff polarity reminder:** in vc71 `--show-diffs`, the side showing the real
absolute address (e.g. `movw %cx, 0x331f08`) is YOUR candidate; the side with a
`0x0` placeholder is the delinked reference (Ghidra synthesizes symbol+reloc).
Use this to orient which side is which before drawing conclusions. See also
[[feedback_check_disasm]] and [[feedback_vc71_structural_ceiling]].
