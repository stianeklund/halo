# Halo 1 Tag: input_device_defaults

## Summary

7.   Scripting      8.   Maps      9.   Engine systems

## Tag Information

Tag group: `input_device_defaults`.

Defined fields and enum values mentioned:

- device id
- device id external
- device id file offset
- device id pointer
- device id size
- device type
- device type full profile definition
- device type joysticks gamepads etc
- device type mouse and keyboard
- profile
- unused

## Details

1. The Reclaimers Library

## input_ device_ defaults

**input_ device_ defaults (`devc`?)**

* H1-Guerilla
* invader-edit
* invader-edit-qt
* OS_ Guerilla
* Mozzarilla
* reclaimer-python

...

## Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| device type | `enum` |  |
Option values:
- mouse and keyboard (`0x0`)
- joysticks gamepads etc (`0x1`)
- full profile definition (`0x2`)
| unused | `int16` |  |
| device id | `TagDataOffset` |  |
Field values:
- size (`uint32`)
- external (`uint32`)
- file offset (`uint32`)
- pointer (`ptr64`)
| profile | `TagDataOffset`? |  |

## Acknowledgements

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `input_device_defaults`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
