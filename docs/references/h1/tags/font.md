# Halo 1 Tag: font

## Summary

1.   Limits 2.   Structure and fields     1.   character tables     2.   characters

## Tag Information

Tag group: `font`.

Defined fields and enum values mentioned:

- ascending height
- bold
- character tables
- character tables character table
- character tables character table character index
- characters
- characters bitmap height
- characters bitmap origin x
- characters bitmap origin y
- characters bitmap width
- characters character
- characters character width
- characters hardware character index
- characters pixels offset
- condense
- descending height
- flags
- italic
- leading height
- leading width
- pixels
- pixels external
- pixels file offset
- pixels pointer
- pixels size
- underline

## Details

1. Limits
 1. character tables

...

### Limits

The resolution of fonts determines their size on-screen, and fonts are pre-rasterized when compiled by Tool. H1CE players can use Chimera to enable sharper runtime font rendering.

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `int32` |  |
| ascending height | `int16` |  |
| descending height | `int16` |  |
| leading height | `int16` |  |
| leading width | `int16` | * Cache only you can't actually set this; it gets overridden with (ascending height + descending height) / 5 on map build |
| character tables | `Block` | * HEK max count: 256 * Cache only * Max: 256 |
Field values:
- character table (`Block`): * HEK max count: 256 * Max: 256
- character index (`uint16`→)
| bold | `TagDependency`: font |  |
| italic | `TagDependency`: font |  |
| condense | `TagDependency`: font |  |
| underline | `TagDependency`: font |  |
| characters | `Block` | * HEK max count: 32000 * Read-only |
Field values:
- character (`uint16`)
- character width (`int16`)
- bitmap width (`int16`)
- bitmap height (`int16`)
- bitmap origin x (`int16`)
- bitmap origin y (`int16`)
- hardware character index (`uint16`)
- pixels offset (`int32`)
| pixels | `TagDataOffset` |  |
Field values:
- size (`uint32`)
- external (`uint32`)
- file offset (`uint32`)
- pointer (`ptr64`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `font`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
