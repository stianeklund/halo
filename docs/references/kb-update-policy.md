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

## Data global declarations

A data entry is `{ "addr": "0x...", "decl": "<C declaration>" }` in the owning object's
`data` list (or `<common>` when the owner is not yet attributed). The `decl` is emitted
verbatim into `build/generated/decl.h`, so it is a real type contract, not a note.

Declare the **narrowest type the disassembly proves**, and give the global its real
name whenever evidence supports one:

| Kind | Shape | Example |
|------|-------|---------|
| Datum-array handle | `data_t *<name>;` | `data_t * object_header_data;` |
| Typed struct pointer | `<type>_t *<name>;` | `players_globals_t * players_globals;` |
| Typed struct by value | `<type>_t <name>;` | `game_variant_t game_variant_global;` |
| Exact-width scalar | `int16_t`/`char`/`int` | `int16_t game_engine_variant_index;` |
| Opaque pointer (target type unproven) | `void *<name>;` | `void *current_game_engine;` |
| Opaque block (size proven, layout not) | `char <name>[<size>];` | `char player_ui_globals[0x230];` |

Rules:

- **Width comes from the operand size**, same discipline as struct fields — a global
  read via `movsx ax` is `int16_t`, not `int`. Declaring it wider is the `[LOADW-WARN]`
  bug class (`docs/lift-learnings.md` §24) at global scope.
- **Prefer an opaque block over a speculative struct.** `char foo[0x230]` with a proven
  size is honest; a half-invented `foo_globals_t` is a wrong type contract that every
  caller inherits. Promote it to a real struct once `struct-recovery` has evidence.
- **Name unknowns by role, not by address.** We deliberately do **not** use upstream's
  `byte_`/`word_`/`dword_<addr>` scheme. An address-derived name asserts a *width* while
  the `decl` asserts a *type*, and the two then drift: upstream's own gotcha list has to
  warn that `dword_50548c` is declared `int` but actually holds a pointer, requiring a
  cast at every use. Typing the decl correctly (`void *current_game_engine`) removes both
  the cast and the warning. Three legacy holdovers remain in `kb.json`
  (`char byte_325714`, `char byte_457068`, `char byte_457069`) — do not add more; rename
  them when their role is proven.
- Field/struct-level unknown naming is separate — see skill `naming-confidence`
  (`field_<hex>` vs `pad_<hex>[n]`).

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
