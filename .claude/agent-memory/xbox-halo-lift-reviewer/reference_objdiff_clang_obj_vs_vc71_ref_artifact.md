---
name: objdiff-clang-obj-vs-vc71-ref-artifact
description: Why objdiff_lift.py on the build/ (clang) .obj vs a VC71 delinked ref gives a bogus low % — do not treat it as the structural match; vc71_verify is authoritative
metadata:
  type: reference
---

Running `objdiff_lift.py --candidate build/CMakeFiles/halo.dir/.../<file>.c.obj`
against a `delinked/functions/<addr>.obj` VC71 reference produces a **garbage
low match %** and should NOT be used as the structural verdict.

**Why:** the repo build compiles with **clang** (`-target i386-pc-win32`), and
the delinked reference is **VC71/CL.exe** shape (the original toolchain). Clang
vs VC71 for the same source differs enormously in instruction count and shape.

Concrete case (scenario_get_sound_environment 0x18f600, 2026-07-04):
- clang build obj symbol `_scenario_get_sound_environment` = **537 insns**
  → objdiff_lift vs the 406-insn VC71 ref = **8.7%** (meaningless cross-compiler diff)
- VC71/CL.exe-compiled candidate (what `vc71_verify.py` builds internally) =
  **407 insns** → LCS vs 406-insn ref = **98.2%** (apples-to-apples, correct)

**How to apply:** the authoritative structural metric for a lift is
`vc71_verify.py` (VC71 candidate vs VC71 ref) — the tool CLAUDE.md mandates.
Confirm it fresh with `--no-cache --function <realname>` (the compiled symbol
is the real decl name, e.g. `scenario_get_sound_environment`, while the ref
symbol is `FUN_<addr>` — vc71_verify maps them via the objdiff.json unit;
plain `compare_obj` positional args mis-resolve the ref in a busy TU).
If a pipeline/objdiff number and the vc71 number disagree by tens of points,
check the candidate's insn count with objdump — if it's ~30%+ larger than the
ref, you're looking at the clang obj, not the VC71 obj. Do not REJECT on the
objdiff number. Related: [[feedback_objdiff_path_and_perfn_ref]],
[[feedback_compare_obj_diff_polarity]].

Counting caveat: `objdump -d | awk` that stops at the first `<...>:` label
UNDERcounts a delinked ref (internal LAB_/reloc labels look like symbols) —
gave "14 insns" for a 406-insn ref. Trust compare_obj/objdiff insn counts over
a naive awk label-boundary count.
