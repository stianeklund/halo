---
name: naming-confidence
description: "rename field, rename function, rename type, rename global, naming confidence, evidence strength, confidence tier, name fields: Rules for renaming fields, locals, functions, constants, and types according to evidence strength — string/PDB evidence earns semantic names, behavior-only evidence earns mechanical names, and unknowns stay visibly unknown (field_XX, unknown_, FUN_). Governs every rename in lift and cleanup work."
---

# Naming & Confidence Rules

A wrong name is worse than no name: future sessions trust it as evidence. Every rename
must be justified by a tier below, and the name's *shape* must not exceed its tier.

## Confidence tiers

| Tier | Evidence | Allowed name shape |
|---|---|---|
| T1 | Binary strings: assert expression text, `__FILE__` anchors, format strings; PDB corpus match (`punpckhdq_import.py`) | Full semantic name, verbatim from evidence (`actor_set_active`, `vertical_field_of_view`) |
| T2 | Strong structural evidence: writes a T1-named global, mirror of a named PC/CE symbol, callee role proven by disasm across all callers | Semantic name + evidence comment; prefer domain-neutral wording |
| T3 | Behavior only: role clear from code shape, meaning unproven | **Mechanical** name (`count`, `elem_index`, `scale_q`, `out_buf`) — never domain semantics |
| T4 | No evidence | Stays `FUN_<addr>`, `field_XX`, `unknown_<addr>`, `local_NN` |

The cardinal sin is a T3 rename with a T1-shaped name (`player_health` because "it
looks like health"). If you can't cite the evidence in one line, the name is T3 or T4.

## Per-symbol-kind rules

- **Functions (kb.json).** Renames go through the existing pipeline —
  `tools/analysis/fun_pipeline.py` (`reclassify`/`propose`/`apply`) and
  `apply_punpckhdq_renames.py` for PDB-derived names — not ad-hoc kb.json edits.
  `@<reg>` annotations are immutable regardless of rename. After a rename, fix call
  sites via the build-error triage flow (grep `build/generated/decl.h`, `rtk jq` the
  addr) rather than re-reading sources.
- **Struct fields.** Only via a `struct-assert`ed definition; a rename never changes
  width/offset (the `co()` assert pins it). Record the evidence in the `///<` comment.
- **Locals.** See `local-var-cleanup` — T3 mechanical vocabulary by default.
- **Constants/enums.** See `const-enum-recovery` — the *value* is proven by the binary;
  the *name* needs its own tier.
- **Globals.** `rename_global_variable`/kb.json name + a matching Ghidra label so both
  views agree; cite the evidence address.

## Procedure

1. Check prior art first: `rtk rg '<candidate name>' src/ && rtk jq '<name query>' kb.json`
   — reuse existing vocabulary (e.g. `datum_salt`, `tag_index`) instead of coining
   synonyms. Also check the PDB corpus before inventing: a T1 name may already exist.
2. Assign the tier, write the one-line evidence citation.
3. Apply the rename repo-wide in one commit (name changes only — Separation rule).
4. Renames are codegen-neutral: `vc71_regression.py check --source <file>` must show
   zero movement. Any movement means the commit wasn't rename-only — split it.

## Downgrades

When later evidence contradicts a name, **downgrading is mandatory**, not optional:
rename back to mechanical/unknown and note the contradiction in the commit message.
