# Halo 1 Tag: material_effects

## Summary

1.   Related HaloScript 2.   Structure and fields     1.   effects

## Tag Information

Tag group: `material_effects`.

HaloScript entries:

- `debug_material_effects [boolean]`

Defined fields and enum values mentioned:

- effects
- effects materials
- effects materials effect
- effects materials sound

## Details

1. Related HaloScript

**Material effects** are used to spawn localized effects when dynamic objects like vehicles and bipeds collide with each other and the BSP.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_material_effects [boolean])` Displays cyan spheres wherever material_ effects are being generated, like under the player's feet and where physics spheres intersect with the BSP or model_ collision_ geometry. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| effects | `Block` | * HEK max count: 13 This block has positional entries for different types of material interactions, such as walking, impacts, and vehicle tire slip. |
Field values:
- materials (`Block`): * HEK max count: 33 This block has positional entries for every hard-coded material type. These are the same material types seen in damage_ effect modifiers.
- effect (`TagDependency`: effect): The effect to locally spawn at the interaction/collision point, which can do things like create decals, particles, lights, etc.
- sound (`TagDependency`: sound): A sound to play at the interaction/collision point. It is also possible to add a sound _part_ to the effect above, but using this field may prevent the game from cutting this sound during effect-intense scenes.

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `material_effects`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
