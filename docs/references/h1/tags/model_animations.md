# Halo 1 Tag: model_animations

## Summary

1.   Related HaloScript 2.   Structure and fields     1.   objects     2.   units     3.   weapons     4.   vehicles     5.   devices     6.   unit damage     7.   first person weapons     8.   sound references     9.   nodes     10.   animations

## Tag Information

Tag group: `model_animations`.

HaloScript entries:

- `debug_unit_all_animations [boolean]`
- `debug_unit_animations [boolean]`

Defined fields and enum values mentioned:

- animations
- animations default data
- animations flags
- animations flags 25hz pal
- animations flags compressed data
- animations flags world relative
- animations frame count
- animations frame data
- animations frame info
- animations frame info external
- animations frame info file offset
- animations frame info pointer
- animations frame info size
- animations frame info type
- animations frame info type dx dy
- animations frame info type dx dy dyaw
- animations frame info type dx dy dz dyaw
- animations frame info type none
- animations frame size
- animations key frame index
- animations left foot frame index
- animations loop frame index
- animations main animation index
- animations name
- animations next animation
- animations node count
- animations node list checksum
- animations node rotation flag data
- animations node scale flag data
- animations node transform flag data
- animations offset to compressed data
- animations relative weight
- animations right foot frame index
- animations second key frame index
- animations sound
- animations sound frame index
- animations type
- animations type base
- animations type overlay
- animations type replacement
- animations weight
- devices
- devices animations
- devices animations animation
- first person weapons
- first person weapons animations
- first person weapons animations animation
- flags
- flags compress all animations
- flags force idle compression
- limp body node radius
- nodes
- nodes base vector
- nodes first child node index
- nodes name
- nodes next sibling node index
- nodes node joint flags
- nodes node joint flags ball socket
- nodes node joint flags hinge
- nodes node joint flags no movement
- nodes parent node index
- nodes vector range
- objects
- objects animation
- objects function
- objects function a out
- objects function b out
- objects function c out
- objects function controls
- objects function controls frame
- objects function controls scale
- objects function d out
- sound references
- sound references sound
- unit damage
- unit damage animation
- units
- units animations
- units animations animation
- units down pitch frame count

## Details

1. Related HaloScript
 6. unit damage
 7. first person weapons
 8. sound references

...

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_unit_all_animations [boolean])` Logs lines to the console output as unit animations occur. For example: `cyborg_mp: animation stand pistol move-right`. | Global |
|  | `(debug_unit_animations [boolean])` Shows red console log output whenever a unit animation is missing. For example: `MISSING: cyborg 'G-driver unarned aim-still'`. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| objects | `Block` | * HEK max count: 4 |
Option values:
- animation (`uint16`→)
- function (`enum`)
- a out (`0x0`)
- b out (`0x1`)
- c out (`0x2`)
- d out (`0x3`)
- function controls (`enum`)
- frame (`0x0`)
- scale (`0x1`)
| units | `Block` | * HEK max count: 33 Max limit raised to 512 in a 2023 H1A update, matching OpenSauce. |
Field values:
- label (`TagString`)
- right yaw per frame (`float`)
- left yaw per frame (`float`)
- right frame count (`uint16`)
- left frame count (`uint16`)
- down pitch per frame (`float`)
- up pitch per frame (`float`)
- down pitch frame count (`uint16`)
- up pitch frame count (`uint16`)
- animations (`Block`): * HEK max count: 33
- animation (`uint16`→)
- ik points (`Block`): * HEK max count: 4
- marker (`TagString`)
- attach to marker (`TagString`)
- weapons (`Block`): * HEK max count: 16
- name (`TagString`)
- grip marker (`TagString`)
- hand marker (`TagString`)
- right yaw per frame (`float`)
- left yaw per frame (`float`)
- right frame count (`uint16`)
- left frame count (`uint16`)
- down pitch per frame (`float`)
- up pitch per frame (`float`)
- down pitch frame count (`uint16`)
- up pitch frame count (`uint16`)
- animations (`Block`?): * HEK max count: 55
- ik point (`Block`?): * HEK max count: 4
- weapon types (`Block`): * HEK max count: 10
- label (`TagString`)
- animations (`Block`): * HEK max count: 16
- animation (`uint16`→)
| weapons | `Block` | * HEK max count: 16 |
Field values:
- animations (`Block`): * HEK max count: 11
- animation (`uint16`→)
| vehicles | `Block` | * HEK max count: 1 |
Field values:
- right yaw per frame (`float`)
- left yaw per frame (`float`)
- right frame count (`uint16`)
- left frame count (`uint16`)
- down pitch per frame (`float`)
- up pitch per frame (`float`)
- down pitch frame count (`uint16`)
- up pitch frame count (`uint16`)
- animations (`Block`): * HEK max count: 8
- animation (`uint16`→)
- suspension animations (`Block`): * HEK max count: 8
- mass point index (`uint16`)
- animation (`uint16`→)
- full extension ground depth (`float`)
- full compression ground depth (`float`)
| devices | `Block` | * HEK max count: 2 |
Field values:
- animations (`Block`): * HEK max count: 2
- animation (`uint16`→)
| unit damage | `Block` | * HEK max count: 176 |
Field values:
- animation (`uint16`→)
| first person weapons | `Block` | * HEK max count: 28 |
Field values:
- animations (`Block`): * HEK max count: 28
- animation (`uint16`→): * HEK max count: 176
| sound references | `Block` | * HEK max count: 514 |
Field values:
- sound (`TagDependency`: sound)
| limp body node radius | `float` |  |
| flags | `bitfield` |  |
Flag values:
- compress all animations (`0x1`)
- force idle compression (`0x2`)
| nodes | `Block` | * HEK max count: 64 |
Flag values:
- name (`TagString`)
- next sibling node index (`uint16`→): * Read-only
- first child node index (`uint16`→): * Read-only
- parent node index (`uint16`→): * Read-only
- node joint flags (`bitfield`)
- ball socket (`0x1`)
- hinge (`0x2`)
- no movement (`0x4`)
- base vector (`Vector3D`): * Read-only
- vector range (`float`): * Read-only
| animations | `Block` | * HEK max count: 2048 |
Field values:
- name (`TagString`): * Read-only
- type (`enum`): * Read-only
- base (`0x0`)
- overlay (`0x1`)
- replacement (`0x2`)
- frame count (`uint16`): * Read-only
- frame size (`uint16`): * Read-only
- frame info type (`enum`): * Read-only
- none (`0x0`)
- dx dy (`0x1`)
- dx dy dyaw (`0x2`)
- dx dy dz dyaw (`0x3`)
- node list checksum (`uint32`): * Read-only
- node count (`uint16`): * Read-only
- loop frame index (`uint16`)
- weight (`float`)
- key frame index (`uint16`)
- second key frame index (`uint16`)
- next animation (`uint16`→): * Read-only
- flags (`bitfield`)
- compressed data (`0x1`)
- world relative (`0x2`)
- 25hz pal (`0x4`)
- sound (`uint16`→)
- sound frame index (`uint16`)
- left foot frame index (`int8`)
- right foot frame index (`int8`)
- main animation index (`uint16`): * Cache only
- relative weight (`float`): * Cache only
- frame info (`TagDataOffset`)
- size (`uint32`)
- external (`uint32`)
- file offset (`uint32`)
- pointer (`ptr64`)
- node transform flag data (`uint32[2]`): * Hidden These are two bitfields. Each bits refer to a node to which the transformation applies. The first field refers to the first 32 nodes and the second field is the second 32 nodes.
- node rotation flag data (`uint32[2]`): * Hidden These are two bitfields. Each bits refer to a node to which the transformation applies. The first field refers to the first 32 nodes and the second field is the second 32 nodes.
- node scale flag data (`uint32[2]`): * Hidden These are two bitfields. Each bits refer to a node to which the transformation applies. The first field refers to the first 32 nodes and the second field is the second 32 nodes.
- offset to compressed data (`uint32`): * Read-only
- default data (`TagDataOffset`?)
- frame data (`TagDataOffset`?)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `model_animations`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
