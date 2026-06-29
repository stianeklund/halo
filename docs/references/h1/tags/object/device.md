# Halo 1 Reference: Structure and fields

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/object/device/`
- Local path: `tags/object/device/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

1.   Structure and fields

Device is an abstract object") tag and parent of the device_ machine, device_ light_ fixture, and device_ control tags. They are used for objects with on/off states like switches, elevators, and doors.

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| device flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | position loops | `0x1` | | | position not interpolated | `0x2` | | |
| power transition time | `float` | * Unit: seconds |
| power acceleration time | `float` | * Unit: seconds |
| position transition time | `float` | * Unit: seconds |
| position acceleration time | `float` | * Unit: seconds |
| depowered position transition time | `float` | * Unit: seconds |
| depowered position acceleration time | `float` | * Unit: seconds |
| device a in | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | power | `0x1` | | | change in power | `0x2` | | | position | `0x3` | | | change in position | `0x4` | | | locked | `0x5` | | | delay | `0x6` | | |
| device b in | `enum`? |  |
| device c in | `enum`? |  |
| device d in | `enum`? |  |
| open | `TagDependency` * sound * effect |  |
| close | `TagDependency` * sound * effect |  |
| opened | `TagDependency` * sound * effect |  |
| closed | `TagDependency` * sound * effect |  |
| depowered | `TagDependency` * sound * effect |  |
| repowered | `TagDependency` * sound * effect |  |
| delay time | `float` | * Unit: seconds |
| delay effect | `TagDependency` * sound * effect |  |
| automatic activation radius | `float` | * Unit: world units This sets the activation radius for automatic doors. The device must be a device_ machine which operates automatically and has "door" machine type. If this value is 0 or negative, the object bounding radius") is used instead. This field is known not to be used for any other purposes. |
| inverse power acceleration time | `float` | * Cache only |
| inverse power transition time | `float` | * Cache only |
| inverse depowered position acceleration time | `float` | * Cache only |
| inverse depowered position transition time | `float` | * Cache only |
| inverse position acceleration time | `float` | * Cache only |
| inverse position transition time | `float` | * Cache only |
| delay time ticks | `float` | * Cache only |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/object/device/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
