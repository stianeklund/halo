# Halo 1 Reference: Limits

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/object/scenery/`
- Local path: `tags/object/scenery/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Scenery** are non-moving objects placed within maps that are not part of the BSP. Some examples of scenery include boulders, trees, crashed pelicans, and smoke emitters. Scenery objects can be added to a palette in Sapien and placed many times throughout the level, with each scenery implicitly belonging to a particular BSP.

While scenery and their collision models can be animated, they do not have physics like units") and items") and the object technically remains fixed at one location. Doors and elevators are implemented using device_ machine instead.

### Limits

While thousands of scenery can be placed in a scenario, the unmodified game engine only supports rendering at most 256 at any given time. This limit can be increased to 512 using OpenSauce.

### Shadows

Because these objects are non-moving, they cast shadows in lightmaps. A scenery's collision is used to cast shadows rather than its gbxmodel. Scenery can also be forced to use dynamic shadow mapping; see object lighting").

### Structure and fields

| Field | Type       | Comments                                                                        |
|-------|------------|---------------------------------------------------------------------------------|
| flags | `bitfield` | * H1A only This field is padding in legacy Halo. It is visible in H1A Guerilla. |
|       | Flag       | Mask                                                                            | Comments | | --- | --- | --- | | off in pegasus | `0x1` | * Pegasus (China) only Presumably hides the scenery in MCC "Pegasus", the censored Chinese release of the game. This flag is now unused in modern versions. | |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/object/scenery/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
