---
name: short-counter-reg-width-ceiling
description: int16_t loop counters often compile to 32-bit xorl/incl under VC71 vs the original's 16-bit xorw/incw — a permanent few-instruction ceiling, not a source bug
metadata:
  type: feedback
---

When a lifted function counts table entries with an `int16_t`/`short` loop
counter, VC71 frequently allocates that counter to a full 32-bit register and
emits `xorl %ecx,%ecx` / `incl %ecx`, whereas the original binary kept it
16-bit (`xorw %cx,%cx` / `incw %cx`). The values, control flow, and side
effects are identical; only the register width of the increment differs.

**Why:** VC71's register allocator chooses 16-bit vs 32-bit based on the
counter's live range and downstream uses (e.g. a later `(int)count << 4` for a
malloc size, which it lowers as `movswl %cx,%eax; shl`). The choice is a
compiler heuristic, not something the C source dictates — both `int16_t` and a
narrower scope still let VC71 pick `ecx`.

**How to apply:** Treat a VC71 diff whose *only* content differences are
`xorl/incl <reg>` vs `xorw/incw <reg16>` on a loop counter as a structural
ceiling. Do NOT churn the source trying to coax 16-bit selection — it is
speculative, risks regressing a passing lift, and yields no behavioral change.
The permuter does not reach register-width selection (not an AST property), so
do not spend cycles on it for this class. Example: ai_communication_initialize
(0x42a30) sits at 88% (98/102 insns) entirely from this; two count loops × two
ops = 4 insns. Related: [[feedback-vc71-structural-ceiling]].
