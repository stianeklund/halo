# Halo 1 Tag: shader

## Summary

Shader tags define the visual appearance of models in the game world. The _shader_ tag class itself is abstract and cannot be created directly, but is extended by the various child shader types like shader_ model and shader_ environment.

## Tag Information

Tag group: `shader`.

Defined fields and enum values mentioned:

- color of emitted light
- detail level
- detail level high
- detail level low
- detail level medium
- detail level turd
- material type
- material type cyborg armor
- material type cyborg energy shield
- material type dirt
- material type elite
- material type elite energy shield
- material type engineer force field
- material type engineer skin
- material type flood carrier form
- material type flood combat form
- material type force field
- material type glass
- material type grunt
- material type human armor
- material type human skin
- material type hunter armor
- material type hunter shield
- material type hunter skin
- material type ice
- material type jackal
- material type jackal energy shield
- material type leaves
- material type metal hollow
- material type metal thick
- material type metal thin
- material type monitor
- material type plastic
- material type rubber
- material type sand
- material type sentinel
- material type snow
- material type stone
- material type water
- material type wood
- physics flags
- physics flags unused
- power
- shader flags
- shader flags ignore normals
- shader flags simple parameterization
- shader flags transparent lit
- shader type
- tint color

## Details

Shader tags define the visual appearance of models in the game world. The _shader_ tag class itself is abstract and cannot be created directly, but is extended by the various child shader types like shader_ model and shader_ environment.

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| shader flags | `bitfield` |  |
Flag values:
- simple parameterization (`0x1`): Causes lightmap UVs for this material to be based on existing texture UVs rather than generated completely anew by Tool/Sapien. This can be used to fix squiggly or jagged shadowing artifacts often seen on large complex surfaces using a single material. Note that any overlapping UV faces may cause lighting artifacts because one part of the lightmap texture is being used to represent different locations.
- ignore normals (`0x2`): Lighting will ignore surface normals (the direction faces point). Use this for shaders where the surface normals are not a good representation of lighting directionality, such as foliage mapped to flat planes for trees and bushes.
- transparent lit (`0x4`)
| detail level | `enum` | Determines how detailed lightmaps will be for surfaces using this shader. This affects various radiosity tessellation parameters. |
Option values:
- high (`0x0`): The surface will receive the highest possible quality of lightmaps. Shadows will be sharper and there will be greater detail in local variations in lighting.
- medium (`0x1`): The surface will receive a medium quality of lightmaps.
- low (`0x2`): The surface will receive a low quality of lightmaps.
- turd (`0x3`): The surface will receive the worst possible quality of lightmaps. Use this for shaders which will be used in areas which do not need detailed shadows or the player is unlikely to visit in order to save texture space.
| power | `float` |  |
| color of emitted light | `ColorRGB` |  |
| tint color | `ColorRGB` | Tints the light passing through this surface during radiosity, resulting in coloured shadows. Black completely blocks light and white is totally transparent. Tinting is supported on all shader types that can be used in the BSP, but is only applicable when the transparent (`#`) or double-sided (`%`) BSP material symbols are set. |
| physics flags | `bitfield` | * Hidden |
Flag values:
- unused (`0x1`)
| material type | `enum` |  |
Option values:
- dirt (`0x0`)
- sand (`0x1`)
- stone (`0x2`)
- snow (`0x3`)
- wood (`0x4`)
- metal hollow (`0x5`)
- metal thin (`0x6`)
- metal thick (`0x7`)
- rubber (`0x8`)
- glass (`0x9`): Required for breakable surfaces using the `-` symbol.
- force field (`0xA`)
- grunt (`0xB`)
- hunter armor (`0xC`)
- hunter skin (`0xD`)
- elite (`0xE`)
- jackal (`0xF`)
- jackal energy shield (`0x10`)
- engineer skin (`0x11`)
- engineer force field (`0x12`)
- flood combat form (`0x13`)
- flood carrier form (`0x14`)
- cyborg armor (`0x15`)
- cyborg energy shield (`0x16`)
- human armor (`0x17`)
- human skin (`0x18`)
- sentinel (`0x19`)
- monitor (`0x1A`)
- plastic (`0x1B`)
- water (`0x1C`)
- leaves (`0x1D`)
- elite energy shield (`0x1E`)
- ice (`0x1F`)
- hunter shield (`0x20`)
| shader type | `uint16` | * Volatile |

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `shader`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
