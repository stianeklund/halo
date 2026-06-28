# Halo 1 Tag: meter

## Summary

...

## Tag Information

Tag group: `meter`.

Defined fields and enum values mentioned:

- anchor colors
- anchor colors at both ends
- anchor colors at empty
- anchor colors at full
- empty color
- encoded stencil
- encoded stencil external
- encoded stencil file offset
- encoded stencil pointer
- encoded stencil size
- flags
- flags unused
- full color
- interpolate colors
- interpolate colors faster near empty
- interpolate colors faster near full
- interpolate colors linearly
- interpolate colors through random noise
- mask distance
- source bitmap
- source sequence index
- stencil bitmaps
- stencil sequence index
- unmask distance

## Details

...

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- unused (`0x1`)
| stencil bitmaps | `TagDependency`: bitmap | * Non-cached Don't use this! This gets dereferenced on map build. |
| source bitmap | `TagDependency`: bitmap | * Non-cached Don't use this! This gets dereferenced on map build. |
| stencil sequence index | `uint16` |  |
| source sequence index | `uint16` |  |
| interpolate colors | `enum` |  |
Option values:
- linearly (`0x0`)
- faster near empty (`0x1`)
- faster near full (`0x2`)
- through random noise (`0x3`)
| anchor colors | `enum` |  |
Option values:
- at both ends (`0x0`)
- at empty (`0x1`)
- at full (`0x2`)
| empty color | `ColorARGB` |  |
| full color | `ColorARGB` |  |
| unmask distance | `float` |  |
| mask distance | `float` |  |
| encoded stencil | `TagDataOffset` | * Cache only |
Field values:
- size (`uint32`)
- external (`uint32`)
- file offset (`uint32`)
- pointer (`ptr64`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `meter`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
