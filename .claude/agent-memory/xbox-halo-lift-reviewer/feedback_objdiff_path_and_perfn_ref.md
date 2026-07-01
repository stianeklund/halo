---
name: objdiff-path-and-perfn-ref
description: objdiff config lives at REPO-ROOT /objdiff.json (NOT tools/verify/objdiff.json); real_math lifts use gitignored per-fn delinked/functions/<addr>.obj refs — check those before declaring "no structural evidence"
metadata:
  type: feedback
---

Before ruling NEEDS_RUNTIME / REJECT on "no delinked reference / no structural
evidence", check the RIGHT artifacts. Two traps that produced a wrong
NEEDS_RUNTIME on deflate 0x110ed0 (2026-06-30):

1. **objdiff config path:** it is at the **repo root** `/mnt/g/dev/halo-bugs/objdiff.json`,
   NOT `tools/verify/objdiff.json` (that path does not exist; grep returns nothing
   and looks like "absent" when really you queried a nonexistent file). Always
   `rtk fd objdiff` or grep `/objdiff.json` directly.

2. **Per-function delinked refs:** real_math (and other session) lifts do NOT rely
   on the whole-file `delinked/<obj>.obj`. They use a **dedicated per-function ref**
   `delinked/functions/<addr_no_0x>.obj` (e.g. `delinked/functions/00110ed0.obj`),
   registered in `/objdiff.json` as `name: "halo/FUN_<addr>", base_path:
   "delinked/functions/<addr>.obj"`. The whole-file `delinked/real_math.obj` is
   STALE (covers only the 0x1acb0–0x1b750 window) and will NOT contain a freshly
   lifted zlib-cluster function — that absence is EXPECTED and is NOT evidence the
   target has no reference.

**The per-fn `.obj` is gitignored** (repo convention). Reproducibility mechanism =
Ghidra DB + the committed objdiff registration, NOT a committed `.obj`. So the
correct reproduction check is:
```
grep -nE "<addr>" /mnt/g/dev/halo-bugs/objdiff.json          # registration present
objdump -t delinked/functions/<addr>.obj | grep FUN_<addr>   # ref has the body
rtk python3 tools/verify/vc71_verify.py <src.c> --no-cache --function FUN_<addr>
# -> "Comparing against <addr>.obj... PASS FUN_<addr>: NN% match (cand/ref insns)"
```
If the `--function` run prints `Comparing against <addr>.obj` and a ref-insn count
matching the real body size (not a neighbor's), the score scored the target, not a
silent fallback. That IS reproducible structural evidence.

**Why:** A correct semantic disasm audit was nearly blocked by checking the stale
whole-file ref + a wrong objdiff path. The lift was sound (91.6%, all PASS) and
belonged at AUTO_ACCEPT. Related: [[project_real_math_zlib_disasm_audit_bar]],
[[compare_obj_diff_polarity]].

**How to apply:** When the whole-file delinked ref lacks the target, do not jump to
"no structural evidence" — look for `delinked/functions/<addr>.obj` + its `/objdiff.json`
registration and reproduce with `--function`. Only fall back to the static
disasm-audit bar if BOTH the per-fn ref and registration are genuinely absent.
