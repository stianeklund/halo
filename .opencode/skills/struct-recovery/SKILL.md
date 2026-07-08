---
name: struct-recovery
description: "struct recovery, recover struct, identify struct, tag block, pool stride, object stride, packed layout, array of structs: Identify structs, arrays of structs, unions, packed layouts, tag blocks, and object-pool strides from binary/disassembly evidence — producing an evidence table (offset, width, signedness, access sites, name evidence) that struct-assert turns into a C definition. Evidence-gathering only; never invents fields."
---

# Data Structure Recovery

Recover layout facts from the binary, not from plausibility. The output is an
**evidence table**, not C code — `struct-assert` writes the C. Every row must cite
where the evidence came from.

Ghidra MCP pre-flight first: `python3 tools/audit/check_ghidra_mcp.py`. Follow Ghidra
token discipline (targeted `read_memory` / `get_function_callees`, not bulk decompiles).

## Evidence sources, strongest first

1. **Assert / debug strings.** They name structs, fields, and sizes verbatim
   (precedent: `sizeof(packet_header)==1` recovered from an assert string,
   `src/types.h:874`). `ghidra_list_strings` filtered by the subsystem, or
   `batch_string_anchor_report` for `__FILE__` anchors.
2. **Allocation / init sizes.** `memset`/`csmemset` length at an init site = struct
   size (also reveals Ghidra under-sized buffers). For pool elements, the element size
   argument at the `data_new`-style pool constructor is the authoritative stride.
3. **Indexing stride.** `IMUL reg, N` / `LEA reg,[reg*SCALE + base]` in loops over the
   array; `detect_array_bounds` and `analyze_data_region` help bound element count.
4. **Field width & signedness — from disassembly operand sizes only.**
   `MOVSX`=signed narrow, `MOVZX`=unsigned narrow, `MOV word/byte ptr`=16/8-bit store.
   Ghidra's decompiler lies about widths (lift-learnings §24); the listing does not.
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

```
STRUCT CANDIDATE: <name or unknown_<addr>>   size=<0xNN, evidence> stride=<0xNN, evidence>
| offset | width | sign | access (fn:addr, insn) | name evidence | confidence |
UNKNOWN RANGES: <offset..offset — never seen accessed>
UNIONS/OVERLAYS: <offset: armA type / armB type, discriminator>
```

Confidence per row: `named` (string/PDB evidence), `typed` (width+sign proven, no name),
`gap` (padding/unobserved). Unobserved ranges stay `field_XX`/pad — no interpolation.

Hand the table to `struct-assert`. Naming decisions beyond string/PDB evidence go
through `naming-confidence`.
