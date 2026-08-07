---
name: struct-recovery
description: "struct recovery, recover struct, identify struct, tag block, pool stride, offsetof, static_assert, sizeof check, define struct, new struct, struct assert: Identify structs from binary evidence, produce evidence-table artifact, and render into C89 struct with cs()/co() asserts."
---

# Data Structure Recovery

Recover layout facts from the binary, not from plausibility. The output is an
**evidence table**, not C code — Phase 2 below renders the C. Every row must cite
where the evidence came from.

Ghidra MCP pre-flight first: `python3 tools/audit/check_ghidra_mcp.py`. Follow Ghidra
token discipline (targeted `read_memory` / `get_function_callees`, not bulk decompiles).

## Evidence sources, strongest first

1. **Assert / debug strings.** They name structs, fields, and sizes verbatim
   (precedent: `sizeof(packet_header)==1` recovered from an assert string,
   `src/types.h:874`). `mcp__ghidra__list_strings` filtered by the subsystem, or
   `batch_string_anchor_report` for `__FILE__` anchors.
2. **Allocation / init sizes.** `memset`/`csmemset` length at an init site = struct
   size (also reveals Ghidra under-sized buffers). For pool elements, the element size
   argument at the `data_new`-style pool constructor is the authoritative stride.
3. **Indexing stride.** `IMUL reg, N` / `LEA reg,[reg*SCALE + base]` in loops over the
   array; `detect_array_bounds` and `analyze_data_region` help bound element count.
4. **Field width & signedness — from disassembly operand sizes only.**
   `MOVSX`=signed narrow, `MOVZX`=unsigned narrow, `MOV word/byte ptr`=16/8-bit store.
   Ghidra's decompiler lies about widths (lift-learnings §24); the listing does not.
   For bulk verification against delinked MSVC 7.1 objects:
   `rtk python3 tools/recovery/verify_conflict.py --binding <id> --offset 0xNN`
   Reports ground-truth operand widths and float-vs-int (FLD vs MOV) from the binary.
5. **Access-site clustering.** `get_field_access_context` / `analyze_struct_field_usage`
   for how each offset is read across functions; `get_xrefs_to` on globals of that type.
6. **kb.json.** `rtk jq` for already-registered globals/decls touching the region —
   someone may have partially recovered it.
7. **Retrieval index / prior work.** `rtk python3 tools/memory/prior_fixes.py "<struct or subsystem>"`
   and existing definitions in `src/types.h` — extend, don't fork, an existing struct.

## Pattern signatures

- **Tag block**: `{int32 count; void* pointer}` pair, consumed by count-bounded loops
  over `pointer + i*elem_size`; often validated against tag data asserts.
- **Object pool (data_t)**: header magic `0x64407440`, element stride from the pool
  constructor; first field of each element is the 16-bit datum salt.
- **Union**: same offset accessed with incompatible types on different branches, with a
  discriminator field tested first. Record both arms + the discriminator.
- **Packed fields**: one 32-bit store built by CONCAT22/shift-or of two 16-bit values
  (§13) — two fields, not one `int`.
- **Overlap trap**: MSVC stack layouts overlap locals with buffer interiors
  (lift-learnings §2); an "offset" seen via a stack base may be a different local, not
  a field. Compute `EBP_offset − buffer_base` before claiming a field.

## Output contract

The evidence table is a **committed artifact**, not chat text — a table that only
ever existed in a transcript is not replayable or reviewable. Write it to
`recovery/evidence/<struct>.json` (schema in `recovery/evidence/README.md`,
worked example `recovery/evidence/packet_definition.json`) and validate it:

```bash
rtk python3 tools/recovery/evidence_table.py validate recovery/evidence/<struct>.json
```

Per field: `offset` (hex string), `width` (1/2/4/8), `signed` (from MOVSX/MOVZX —
never guessed), optional `kind` (`int` default / `float` / `float64` / `pointer`),
`array_len`, `name`, `confidence`, and a one-line `evidence` citation. Confidence is
`named` (string/PDB name evidence — requires both a name and evidence), `typed`
(width+sign proven; a mechanical name is allowed), or `gap` (padding/unobserved —
must stay unnamed). Top-level `size`/`stride` each carry their own `evidence`; omit
them when unproven rather than guessing. Unknowns keep the canonical split: an offset
that **is** accessed with unproven meaning stays `field_<hex>`; a range never observed
accessed stays `pad_<hex>[n]` — no interpolation. `sources` records every function/address consulted.

Then summarize in chat for the human, same shape as before:

```
STRUCT CANDIDATE: <name or unknown_<addr>>   size=<0xNN, evidence> stride=<0xNN, evidence>
| offset | width | sign | access (fn:addr, insn) | name evidence | confidence |
UNKNOWN RANGES: <offset..offset — never seen accessed>
UNIONS/OVERLAYS: <offset: armA type / armB type, discriminator>
```

Naming decisions beyond string/PDB evidence go through `naming-confidence`.

---

## Phase 2 — Render to C struct

After the evidence artifact is committed, render and place the struct.

### Workflow

```bash
rtk python3 tools/recovery/evidence_table.py validate recovery/evidence/<struct>.json
rtk python3 tools/recovery/evidence_table.py render   recovery/evidence/<struct>.json
```

1. **Render** the skeleton — it applies rules below mechanically.
2. **Review** against the rules — the renderer is a starting point, not authority.
3. **Place** it — `src/types.h` or a binary-proven header (see `header-recovery`).
4. **Build** so asserts fire: `rtk python3 tools/build/build.py -q --target halo`.

**The artifact is the source of truth.** If a failing assert shows the table is
wrong, fix `recovery/evidence/<struct>.json` and re-render — never patch only
the placed struct.

### House style

```c
#define cs(t, s)    static_assert(sizeof(t) == s)
#define co(t, f, o) static_assert(offsetof(t, f) == o)

typedef struct object_header {
    int16_t  datum_salt;        ///< offset=0x00
    int16_t  field_02;          ///< offset=0x02
    uint32_t flags;             ///< offset=0x04
    uint8_t  pad_08[4];         ///< offset=0x08  never observed accessed
    float    position[3];       ///< offset=0x0C
} object_header;
cs(object_header, 0x18);
co(object_header, field_02, 0x02);
co(object_header, position, 0x0C);
```

### Rules

1. **Every field width/signedness from evidence** (disasm operand sizes). Never widen
   "for convenience" — `int16_t` read as `int` is the `[LOADW-WARN]` bug class.
2. **Unknowns stay visibly unknown**: `field_XX` (accessed) or `pad_XX[n]` (unaccessed).
3. **Explicit padding, no compiler discretion.** Spell out every gap byte.
4. **`co()` for every field, `cs()` for size.** When size is unproven, assert only
   offsets and comment `/* size unproven */`.
5. **C89**: `typedef struct`, declarations before statements.
6. **Unions**: named union member with discriminator comment; `co()` on union offset.
7. **Placement**: shared → `src/types.h`; single-TU → top of TU. Always check for
   existing partial definitions first — extend, don't fork.
8. **Evidence in comments**: each non-obvious field carries its evidence hook.

### Verify

A failing `co()`/`cs()` is a *finding* — the evidence table was wrong. Fix the
artifact, re-render, re-place. Don't bend the struct to compile.

Defining the struct changes no codegen. Rewriting accesses to use it is a separate
step: `offset-to-struct`.
