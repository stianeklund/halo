# Halo 1 Tag: virtual_keyboard

## Summary

1.   Structure and fields     1.   virtual keys

## Tag Information

Tag group: `virtual_keyboard`.

Defined fields and enum values mentioned:

- background bitmap
- display font
- special key labels string list
- virtual keys
- virtual keys active background bitmap
- virtual keys caps character
- virtual keys caps symbols character
- virtual keys keyboard key
- virtual keys keyboard key 0
- virtual keys keyboard key 1
- virtual keys keyboard key 2
- virtual keys keyboard key 3
- virtual keys keyboard key 4
- virtual keys keyboard key 5
- virtual keys keyboard key 6
- virtual keys keyboard key 7
- virtual keys keyboard key 8
- virtual keys keyboard key 9
- virtual keys keyboard key a
- virtual keys keyboard key b
- virtual keys keyboard key backspace
- virtual keys keyboard key c
- virtual keys keyboard key caps lock
- virtual keys keyboard key d
- virtual keys keyboard key done
- virtual keys keyboard key e
- virtual keys keyboard key f
- virtual keys keyboard key g
- virtual keys keyboard key h
- virtual keys keyboard key i
- virtual keys keyboard key j
- virtual keys keyboard key k
- virtual keys keyboard key l
- virtual keys keyboard key left
- virtual keys keyboard key m
- virtual keys keyboard key n
- virtual keys keyboard key o
- virtual keys keyboard key p
- virtual keys keyboard key q
- virtual keys keyboard key r
- virtual keys keyboard key right
- virtual keys keyboard key s
- virtual keys keyboard key shift
- virtual keys keyboard key space
- virtual keys keyboard key symbols
- virtual keys keyboard key t
- virtual keys keyboard key u
- virtual keys keyboard key v
- virtual keys keyboard key w
- virtual keys keyboard key x
- virtual keys keyboard key y
- virtual keys keyboard key z
- virtual keys lowercase character
- virtual keys selected background bitmap
- virtual keys shift caps character
- virtual keys shift character
- virtual keys shift symbols character
- virtual keys sticky background bitmap
- virtual keys symbols character
- virtual keys unselected background bitmap

## Details

1. virtual keys

...

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| display font | `TagDependency`: font |  |
| background bitmap | `TagDependency`: bitmap |  |
| special key labels string list | `TagDependency`: unicode_string_list |  |
| virtual keys | `Block` | * HEK max count: 44 |
Option values:
- keyboard key (`enum`)
- 1 (`0x0`)
- 2 (`0x1`)
- 3 (`0x2`)
- 4 (`0x3`)
- 5 (`0x4`)
- 6 (`0x5`)
- 7 (`0x6`)
- 8 (`0x7`)
- 9 (`0x8`)
- 0 (`0x9`)
- a (`0xA`)
- b (`0xB`)
- c (`0xC`)
- d (`0xD`)
- e (`0xE`)
- f (`0xF`)
- g (`0x10`)
- h (`0x11`)
- i (`0x12`)
- j (`0x13`)
- k (`0x14`)
- l (`0x15`)
- m (`0x16`)
- n (`0x17`)
- o (`0x18`)
- p (`0x19`)
- q (`0x1A`)
- r (`0x1B`)
- s (`0x1C`)
- t (`0x1D`)
- u (`0x1E`)
- v (`0x1F`)
- w (`0x20`)
- x (`0x21`)
- y (`0x22`)
- z (`0x23`)
- done (`0x24`)
- shift (`0x25`)
- caps lock (`0x26`)
- symbols (`0x27`)
- backspace (`0x28`)
- left (`0x29`)
- right (`0x2A`)
- space (`0x2B`)
- lowercase character (`int16`)
- shift character (`int16`)
- caps character (`int16`)
- symbols character (`int16`)
- shift caps character (`int16`)
- shift symbols character (`int16`)
- caps symbols character (`int16`)
- unselected background bitmap (`TagDependency`: bitmap)
- selected background bitmap (`TagDependency`: bitmap)
- active background bitmap (`TagDependency`: bitmap)
- sticky background bitmap (`TagDependency`: bitmap)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `virtual_keyboard`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
