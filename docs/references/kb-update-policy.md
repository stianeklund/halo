# kb update policy

Propose conservative kb.json updates only.

Allowed when evidence is strong:
- function name improvements
- parameter count narrowing
- calling convention narrowing
- high-confidence global meaning notes
- confirmed struct/field offset notes
- import or subsystem mapping supported by evidence

Disallowed or discouraged:
- broad semantic renames based on vibes
- speculative struct repacking
- aggressive type invention
- removing or changing `@<reg>` slot assignments (immutable; see
  `docs/references/abi-and-calling-conventions.md`)
- project-wide naming changes from weak local evidence

When proposing kb changes, separate:
- High-confidence deltas
- Tentative notes
- Things that should remain unknown

## KB workflow

Treat the three KB files as separate layers:

- `kb.json`: runtime contract used by build/patch/thunks (decls, names, addresses)
- `kb_meta.json`: analysis metadata only (status, confidence, inferred/uncertain notes)
- `tools/kb_reg_baseline.json`: pinned `@<reg>` ABI lockfile for protected functions

Practical update flow:

1. Do RE/lift changes in code first.
2. Update `kb.json` only for evidence-backed symbol/prototype/address changes.
3. Update `kb_meta.json` for progress/evidence tracking (`status`, `inferred`, `uncertain`, confidence).
4. If a change touches a protected `@<reg>` declaration:
   - default: keep `kb.json` aligned with `tools/kb_reg_baseline.json`
   - only update baseline for explicit policy changes, with clear justification
5. Validate:
   - `python3 tools/analysis/kb_meta.py validate`
   - `python3 -m unittest tools.test_patch.RegAnnotationBaselineTests`
   - normal build path (`patched_xbe`)

Build guardrails for `@<reg>` entries:

- Any mismatch between `kb.json` and `tools/kb_reg_baseline.json` for pinned addresses is a hard build failure.
- Any current `@<reg>` function in `kb.json` missing from baseline is a hard build failure.
