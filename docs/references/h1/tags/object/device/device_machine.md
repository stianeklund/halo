# Halo 1 Reference: Automatic doors

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/object/device/device_machine/`
- Local path: `tags/object/device/device_machine/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Machines** are used for objects like doors, elevators, light bridges, and the engine covers in _The Maw_. These objects have open and closed states and the unique capability of conveying bipeds which stand upon them while they move.

The states of machines are not synchronized over Halo's multiplayer netcode, so it is not adviseable to include them in multiplayer maps unless they are automatic doors (which are just based on biped proximinity) or use a synchronization workaround.

### Automatic doors

Rather than scripting every door to open and close by trigger volumes, you can use the automatic door feature. If a placed device_ machine's _does not operates automatically_ flag is _not_ set, and its tag has "door" machine type, it will automatically open and close when bipeds approach and leave it.

If the tag's _automatic activation radius_") is non-zero, it controls the distance where the door opens. Otherwise the object _bounding radius_") is used instead. Up to 16 collideable bipeds (not vehicles) will be retrieved from the activation radius then checked for the following conditions:

*   Is alive,
*   Doesn't have the _cannot open doors automatically_ flag set,
*   Is not an ally to the player standing on the forward side of a one-sided door in the fully closed position.

If one of these bipeds passes all the conditions, the device group desired position will be set to 1 which opens the door.

This opening process is not actually performed every tick, but once every 4 ticks as an optimization, with different doors being offset to different ticks so they're not all checked at the same time. When an automatic door is triggered to open, its time spent open counter is set to -3 ticks.

Once a door has remained in the fully open position for its door open time, its desired position is set to closed. This is checked every tick.

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| machine type | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | door | `0x0` | Causes this machine to act like an automatic door. | | platform | `0x1` | | | gear | `0x2` | | |
| machine flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | pathfinding obstacle | `0x1` | | | but not when open | `0x2` | | | elevator | `0x4` | Object lighting") samples the lightmap near the object, rather than the ground point directly below it like most objects do. | |
| door open time | `float` | * Unit: seconds Determines how long a door will stay open for once it has reached a fully open position, even once eligible bipeds have left the activation radius. |
| collision response | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | pause until crushed | `0x0` | | | reverse directions | `0x1` | | |
| elevator node | `uint16` |  |
| door open time ticks | `uint32` | * Cache only This value is derived from _door open time_ and is compared at runtime to the number of ticks a door-typed machine has been fully open. If the door has been open for more ticks than this value, its desired device group position will be set to 0 causing the door to begin closing. |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   kornman00 _(Automatic door behaviour)_
*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/object/device/device_machine/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
