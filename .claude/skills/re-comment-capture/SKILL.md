---
name: re-comment-capture
tier: agent
triggers: ["comment capture", "knowledge capture", "document evidence", "evidence comment", "uncertainty comment", "re knowledge"]
description: Write comments that preserve reverse-engineering knowledge — evidence citations, uncertainty markers, match-sensitive constructs, and hard-won conclusions — in the code itself, plus route generalizable lessons to docs/lift-learnings.md (which must ship a detector). Zero codegen risk; the highest-value-per-risk cleanup category.
---

# Comment & RE Knowledge Capture

A comment in this repo is a claim with a citation. The reader must be able to tell
proven fact from working hypothesis at a glance, and must be warned before "improving"
code whose shape the byte-match depends on.

## What to write

1. **Function block comment** (house rule: block comments per function): original
   address, one-line purpose, evidence basis, and verification state.
   ```c
   /* FUN_0009fd30 @ 0x9fd30 — object placement init.
    * Evidence: assert strings (objects.c), caller FUN_xxxx disasm.
    * VC71 98.2%; equivalence high (50 seeds, mem-trace clean). */
   ```
2. **Evidence citations on non-obvious facts**: `/* size 0x88: callee csmemset @0x13fc4a */`,
   `/* bit5 = at-rest; latched in FUN_xxxx */`. Cite address or tool, not vibes.
3. **Uncertainty markers** — keep unknowns loud:
   - `/* unverified: */` plausible reading, no binary proof yet
   - `/* unknown purpose; always 0 in captures */`
   - `/* NOTE: differs from Ghidra decompile — disasm shows MOVSX (int16) */`
4. **Match-sensitive constructs** — the anti-cleanup fence. Anywhere a future reader
   would be tempted to simplify:
   ```c
   /* keep: NaN-inclusive guard — !(m2 >= eps) catches NaN; do not invert */
   /* keep: redundant self-store matches original codegen (VC71 §self-store) */
   /* keep: separate temp forces MSVC spill/reload; merging drops match */
   ```
5. **Aliasing/overlap facts**: `/* damage_params+0x48 — NOT an independent local (§5) */`.

## What NOT to write

- Narration of the obvious (`i++; /* increment i */`) or of your process
  ("fixed per review", "was wrong before").
- Semantic claims above their evidence tier (see `naming-confidence`) — a comment can
  smuggle a bad name's damage just as easily.
- Duplicates of what kb.json/types.h asserts already state mechanically.

## Where knowledge goes

| Knowledge kind | Destination |
|---|---|
| Fact about THIS function/struct | comment at the site (+ `///< offset=` field docs) |
| Generalizable lift/cleanup pattern | new § in `docs/lift-learnings.md` — **must ship a detector in the same commit or a one-line "not mechanically detectable because …" justification** (repo rule) |
| Ghidra-side | `set_plate_comment`/`batch_set_comments` mirroring the C block comment, so both views agree |
| Session-scale outcome | handover/memory, not code comments |

## Gate

Comment-only diffs are codegen-inert; the gate is a formality that catches accidents:

```bash
rtk python3 tools/verify/vc71_regression.py check --source src/halo/<path>/<file>.c
```

Zero movement required. Commit comments separately from any behavioral change.
