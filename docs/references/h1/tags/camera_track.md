# Halo 1 Tag: camera_track

## Summary

7.   Scripting      8.   Maps      9.   Engine systems

## Tag Information

Tag group: `camera_track`.

Defined fields and enum values mentioned:

- control points
- control points orientation
- control points position
- flags
- flags unused

## Details

1. The Reclaimers Library

 1. control points

## camera_ track

**camera_ track (`trak`?)**

Referenced by (2)
* globals
* unit

Workflows
* To/from 3ds Max with: Halo CE Max Toolkit
* From Animation data with: H1-Tool
*
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
| flags | `bitfield` |  |
Flag values:
- unused (`0x1`)
| control points | `Block` | * HEK max count: 16 |
Field values:
- position (`Point3D`)
- orientation (`Quaternion`)

## Acknowledgements

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `camera_track`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
