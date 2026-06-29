# Halo 1 Reference: Structure and fields

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/object/item/`
- Local path: `tags/object/item/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Items** are simple moveable objects and the parent tag of weapon, garbage, and equipment. Although they can have a hitbox in the form of a model_ collision_ geometry, when moving they are simulated as simple point-like objects that can bounce off the BSP and non-item objects. Unlike units"), they cannot be controlled or die (though they can despawn).

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| item flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | always maintains z up | `0x1` | Forces the item to remain upright even if it has been launched in an explosion or thrown. This is used for the multiplayer flag weapon to prevent it from tumbling when dropped. | | destroyed by explosions | `0x2` | Allows the item (e.g. grenade) to be detonated if receiving a damage_ effect with a _detonates explosives_ flag enabled. | | unaffected by gravity | `0x4` | | |
| pickup text index | `uint16` |  |
| sort order | `int16` |  |
| scale | `float` |  |
| hud message value scale | `int16` |  |
| item a in | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | body vitality | `0x1` | | | shield vitality | `0x2` | | | recent body damage | `0x3` | | | recent shield damage | `0x4` | | | random constant | `0x5` | | | umbrella shield vitality | `0x6` | | | shield stun | `0x7` | | | recent umbrella shield vitality | `0x8` | | | umbrella shield stun | `0x9` | | | region | `0xA` | | | region 1 | `0xB` | | | region 2 | `0xC` | | | region 3 | `0xD` | | | region 4 | `0xE` | | | region 5 | `0xF` | | | region 6 | `0x10` | | | region 7 | `0x11` | | | alive | `0x12` | | | compass | `0x13` | | |
| item b in | `enum`? |  |
| item c in | `enum`? |  |
| item d in | `enum`? |  |
| material effects | `TagDependency`: material_effects |  |
| collision sound | `TagDependency`: sound |  |
| detonation delay | `Bounds` | * Unit: seconds |
| | Field | Type | Comments | | --- | --- | --- | | min | `float` | | | max | `float` | | |
| detonating effect | `TagDependency`: effect |  |
| detonation effect | `TagDependency`: effect |  |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/object/item/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
