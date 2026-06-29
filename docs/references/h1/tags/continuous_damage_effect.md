# Halo 1 Tag: continuous_damage_effect

## Summary

Although this tag contains fields related to damage and damage modifiers, these fields are unused by the game. The camera shake also only applies _while_ a non-firing damage damage_ effect is playing, such as when near a rocket explosion. This is likely a bug.

## Tag Information

Tag group: `continuous_damage_effect`.

Defined fields and enum values mentioned:

- cutoff scale
- cyborg armor
- cyborg energy shield
- damage category
- damage flags
- damage instantaneous acceleration
- damage lower bound
- damage maximum stun
- damage side effect
- damage stun
- damage stun time
- damage upper bound
- damage vehicle passthrough penalty
- dirt
- elite
- elite energy shield
- engineer force field
- engineer skin
- flood carrier form
- flood combat form
- force field
- glass
- grunt
- high frequency
- human armor
- human skin
- hunter armor
- hunter shield
- hunter skin
- ice
- jackal
- jackal energy shield
- leaves
- low frequency
- metal hollow
- metal thick
- metal thin
- monitor
- plastic
- radius
- radius max
- radius min
- random rotation
- random translation
- rubber
- sand
- sentinel
- snow
- stone
- triggers firing effects firing damage
- water
- wobble function
- wobble function cosine
- wobble function cosine variable period
- wobble function diagonal wave
- wobble function diagonal wave variable period
- wobble function jitter
- wobble function noise
- wobble function one
- wobble function period
- wobble function slide
- wobble function slide variable period
- wobble function spark
- wobble function wander
- wobble function zero
- wobble weight
- wood

## Details

**Continuous damage effects** create camera shake and controller vibration for players within a radius of a sound_ looping. They are similar to a damage_ effect but more limited in what they can do. This tag is unused by any of the stock game's maps and may have been leftover from earlier in development.

### Known issues

Although this tag contains fields related to damage and damage modifiers, these fields are unused by the game. The camera shake also only applies _while_ a non-firing damage damage_ effect is playing, such as when near a rocket explosion. This is likely a bug.

Controller vibration cannot be felt in H1CE. It is unknown if the vibration works in H1A.

### Structure and fields

| Field  | Type     | Comments                                                                                                                                                                                                 |
|--------|----------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| radius | `Bounds` | * Unit: world units The minimum radius (falloff start) maximum (cutoff) fade the scale of the effect's camera shake linearly by distance from the sound_ looping source which references this tag class. |
Field values:
- min (`float`)
- max (`float`)
| cutoff scale | `float` | * Unused * Min: 0 * Max: 1 This value does not affect the cutoff radius or effect scale. |
| low frequency | `float` | * Min: 0 * Max: 1 Power of the low frequency controller vibration. This is scaled by distance but not the wobble function. |
| high frequency | `float` | * Min: 0 * Max: 1 Power of the high frequency controller vibration. This is scaled by distance but not the wobble function. |
| random translation | `float` | * Unit: world units The maximum distance that the player view can move for camera shake. It is scaled by distance and wobble. |
| random rotation | `float` | * Unit: degrees The maximum angle that the player view can rotate for camera shake. It is scaled by distance and wobble. |
| wobble function | `enum` |  |
Option values:
- one (`0x0`)
- zero (`0x1`)
- cosine (`0x2`)
- cosine variable period (`0x3`)
- diagonal wave (`0x4`)
- diagonal wave variable period (`0x5`)
- slide (`0x6`)
- slide variable period (`0x7`)
- noise (`0x8`)
- jitter (`0x9`)
- wander (`0xA`)
- spark (`0xB`)
| wobble function period | `float` | * Unit: seconds Controls the repeating duration of a periodic wobble function. |
| wobble weight | `float` | * Min: 0 * Max: 1 Sets the influence of the wobble function in dynamically scaling translational and rotational camera shake. A value of `0.0` signifies that the wobble function has no effect and the shake strength is based purely on distance; a value of `1.0` signifies that the shake strength is scaled by both the wobble function and distance equally. |
| damage side effect | `enum` | * Unused |
| damage category | `enum` | * Unused |
| damage flags | `bitfield` | * Unused |
| damage lower bound | `float` | * Unused |
| damage upper bound | `Bounds` | * Unused |
| damage vehicle passthrough penalty | `float` | * Unused |
| damage stun | `float` | * Unused * Min: 0 * Max: 1 |
| damage maximum stun | `float` | * Unused * Min: 0 * Max: 1 |
| damage stun time | `float` | * Unused * Unit: seconds |
| damage instantaneous acceleration | `float` | * Unused |
| dirt | `float` | * Unused |
| sand | `float` | * Unused |
| stone | `float` | * Unused |
| snow | `float` | * Unused |
| wood | `float` | * Unused |
| metal hollow | `float` | * Unused |
| metal thin | `float` | * Unused |
| metal thick | `float` | * Unused |
| rubber | `float` | * Unused |
| glass | `float` | * Unused |
| force field | `float` | * Unused |
| grunt | `float` | * Unused |
| hunter armor | `float` | * Unused |
| hunter skin | `float` | * Unused |
| elite | `float` | * Unused |
| jackal | `float` | * Unused |
| jackal energy shield | `float` | * Unused |
| engineer skin | `float` | * Unused |
| engineer force field | `float` | * Unused |
| flood combat form | `float` | * Unused |
| flood carrier form | `float` | * Unused |
| cyborg armor | `float` | * Unused |
| cyborg energy shield | `float` | * Unused |
| human armor | `float` | * Unused |
| human skin | `float` | * Unused |
| sentinel | `float` | * Unused |
| monitor | `float` | * Unused |
| plastic | `float` | * Unused |
| water | `float` | * Unused |
| leaves | `float` | * Unused |
| elite energy shield | `float` | * Unused |
| ice | `float` | * Unused |
| hunter shield | `float` | * Unused |

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `continuous_damage_effect`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
