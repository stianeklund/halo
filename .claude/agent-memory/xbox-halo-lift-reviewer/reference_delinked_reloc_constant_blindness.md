---
name: delinked-reloc-constant-blindness
description: FPU clamp/branch-select float constants all relocate to ds:0x0 in the delinked ref, so VC71 LCS cannot detect a swapped bound or flipped +/- selection — runtime is mandatory
metadata:
  type: reference
---

In delinked reference .obj files, every float/data operand that was a relocated
global address prints as `ds:0x0` (the reloc target is zeroed). `compare_obj`
LCS normalizes relocated operands, so the match % is BLIND to *which* constant
was loaded.

Consequence for FPU clamp / random-branch lifts: a swapped clamp bound (e.g.
clamping `t` to `[-1,1]` instead of `[0,1]`) or a flipped `?+0.01:-0.01`
selection produces IDENTICAL disassembly shape to the correct code — same
`fld ds:0x0` / `fcom ds:0x0` / `test ah,0x5;jp` idiom. VC71 stays high; only
runtime/golden distinguishes.

**How to apply:** When a lift's correctness rests on specific float constants
selected inside a clamp or a `test ax,ax`-gated `fld ds:0x0` pick, do NOT treat
a high VC71 % as clearing that risk. It is exactly the silent-bug class the box
must adjudicate. Combined with the [[project_informative_sub90_needs_runtime_bar]]
rule, a <90% lift with weak/low-coverage equiv over such a body is NEEDS_RUNTIME.
First seen: wind_update 0x18ffe0 (wind.c), 86.3% VC71, equiv weak @8.4% coverage
— body maps 1:1 to ref but every clamp bound + the +/-0.01 pick reads ds:0x0.
