# Halo 1 Tag: sound_scenery

## Summary

Sound scenery are a type of object, typically invisible, used to play localized ambient looping sounds. They can be found along shorelines and rivers, at the base of waterfalls, and near noisy machinery. These objects complement the "background" sound_ environment used for BSP clusters.

## Tag Information

Tag group: `sound_scenery`.

HaloScript entries:

- `debug_sound [boolean]`

## Details

Sound scenery are a type of object, typically invisible, used to play localized ambient looping sounds. They can be found along shorelines and rivers, at the base of waterfalls, and near noisy machinery. These objects complement the "background" sound_ environment used for BSP clusters.

### Attaching sounds

When creating a sound scenery tag in Guerilla or other tag editor, simply add an attachment in the attachments block with type `sound_looping`, referring to the desired tag. It is not necessary to specify a model marker.

Sound distances are determined by the actual sound tag rather than the object's bounding radius.

### Difference from scenery

In terms of tag data, sound scenery do not differ from scenery in any way; both are simple extensions of object") which do not add any additional fields. Both can play sounds if attached, and both can have render and collision models. However, sound scenery collide with projectiles but not units") and are thus similar to device_ light_ fixture.

### Known issues

You cannot have a sound scenery and background sound at the same time if both are playing a sound tag that is classed as "music". This will crash Sapien when you enter the audible radius of the scenery object, and will crash the game immediately.

The following are related functions that you can use in your scenario scripts and/or debug globals that you can enter into the developer console for troubleshooting.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_sound [boolean])` Sound sources will be labeled in 3D with their tag path and a their minimum and maximum distances shown as red and yellow spheres, respectively. | Global |

### Structure and fields

_This structure contains no additional fields._

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `sound_scenery`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
