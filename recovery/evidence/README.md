# Struct-recovery evidence tables

Committed output of the `struct-recovery` skill, one JSON artifact per struct
(`<struct>.json`). It is the **source of truth** for a recovered layout: the
`struct-assert` skill renders a C89 skeleton from it, and any divergence between
the artifact and the struct placed in `src/types.h` (or a binary-proven header)
is a defect — fix the artifact and re-render.

```bash
rtk python3 tools/recovery/evidence_table.py validate recovery/evidence/packet_definition.json
rtk python3 tools/recovery/evidence_table.py render   recovery/evidence/packet_definition.json
```

## Schema (`"schema": 1`)

| key | required | meaning |
|---|---|---|
| `struct` | yes | C identifier — a real name or `unknown_<addr>` |
| `sources` | yes | provenance strings: functions/addresses/asserts consulted |
| `fields` | yes | offset-ordered list (below) |
| `size` / `stride` | no | `{value, evidence}`; without `size` no `cs()` is rendered |
| `unions` | no | proven overlays: `{offset, size, name, discriminator, arms[]}` |
| `notes` | no | free-form caveats |

Field: `offset` (hex **string**), `width` (1/2/4/8 bytes), `signed` (bool,
required for integers — MOVSX vs MOVZX, never guessed), `kind`
(`int` default / `float` / `float64` / `pointer` + `points_to`), `array_len`,
`name`, `confidence` (`named` = string/PDB name evidence, `typed` = width+sign
proven, `gap` = padding), `evidence` (one-line citation, required for
`named`/`typed`).

Rules the validator enforces: strictly increasing, non-overlapping offsets; a
`named` field needs both a name and evidence; a `gap` may not be named; width 8
integers are rejected as CONCAT-packed pairs (lift-learnings §13); the last
field must fit inside `size`. Unnamed fields render as `field_<off>`, gaps and
holes as explicit `pad_<off>[n]`, and every artifact-declared entry gets a
`co()` — derived padding does not.

`packet_definition.json` is the worked example: back-derived from the already
asserted struct at `src/types.h:777-794`, so `render` can be compared against a
layout the compiler already proves.
