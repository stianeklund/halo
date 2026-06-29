# Halo 1 Tag: antenna

## Summary

The bitmap referenced by an antenna is a sprite sheet that can contain multiple sequences. Different parts of the antenna can set the _sequence index_ to change their texture. Only the first sprite in a sequence is used, so sequences need just a single sprite.

## Tag Information

Tag group: `antenna`.

HaloScript entries:

- `debug_point_physics [boolean]`

Defined fields and enum values mentioned:

- attachment marker name
- bitmaps
- bounding radius
- cutoff pixels
- density
- elasticity
- falloff pixels
- flags collides with structures
- flags collides with water surface
- flags no gravity
- length
- physics
- spring strength coefficient
- surface friction
- vertices
- vertices angles
- vertices angles pitch
- vertices angles yaw
- vertices color
- vertices length
- vertices lod color
- vertices offset
- vertices sequence index
- vertices spring strength coefficient
- widgets

## Details

**Antennas** describe a series of springy vertices rendered with a bitmap texture. As an object _widget_"), they attach to model markers. They are only used on the Warthog and Scorpion vehicles, but during Halo's development they were also present on the player biped.

### Bitmaps

The bitmap referenced by an antenna is a sprite sheet that can contain multiple sequences. Different parts of the antenna can set the _sequence index_ to change their texture. Only the first sprite in a sequence is used, so sequences need just a single sprite.

Sprites should be oriented horizontally and will be centered at the base of each antenna segment (vertex) with twice the defined length. This means the left half of each sprite should be filled with dummy space to avoid overlapping the previous segment.

### Physics

Antennas are modeled as a series of up to 20 fixed-length segments with springy joints. Points at the end of each segment use point_ physics so can be affected by gravity and wind.

Disabling gravity results in the antenna coming to rest in a straight line rather than bending under gravity. Similarly, changing the global strength of gravity with `physics_set_gravity` will also result in the antenna bending more or less.

Although it's not understood exactly how density is used, low air-like densities result in flowing tail-like antenna behaviour while high densities result in stiffer but more energetic ones.

Antennas with _point\_ physics_ flags _collides with structures_ may interact with collision BSPs (both scenario_ structure_ bsp and model_ collision_ geometry) under the right conditions, but typically don't because high spring coefficients result in the antenna being too stiff to rest on the BSP, passing through it instead. Antennas with near-zero spring coefficients act more like a rope and may rest on or swing against the BSP, but tend to be too energetically unstable to remain there or partially hang through it. When antennas do interact with the BSP, high elasticity and (counterintuitively) high surface friction increase instability. The same applies to _collides with water surface_.

### Simulation culling

Antenna physics are only simulated when on-screen (based on the bounding sphere") of the object they're a widget on). This is particularly noticeable when a low but non-zero _spring strength coefficient_ is used, since the antenna will wiggle when reappearing on screen before coming to a rest state again.

### Limits

The game state has limited space for simulated antennas (12 in legacy, 24 in H1A). Extra antennas in the map will not be rendered.

Antenna segments should not be 0-length or else an assertion will be hit:

`EXCEPTION halt in c:\mcc\main\h1\code\h1a2\sources\render\render_sprite.c,#446: mode==_build_sprite_normal || (untransformed_direction && magnitude_squared3d(untransformed_direction))`

|  | Function/global                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | Type   |
|--|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------|
|  | `(debug_point_physics [boolean])` Renders green or red markers wherever point_ physics are being simulated. This includes flags, antenna, contrails, particles, and particle_ systems. For weather_ particle_ system, markers are only shown in their simulation cube and while the `weather` global not disabled. Red markers indicate point_ physics with the _collides with structures_ flag, which are more computationally expensive. It can help to enable `framerate_throttle` for these markers to render consistently. | Global |

### Structure and fields

| Field                       | Type                           | Comments                                                                                                                                                                                                                                                                                                                                                                |
|-----------------------------|--------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| attachment marker name      | `TagString`                    | The model marker name this antenna is attached to. If the model has multiple markers with this name, **only one** will have the antenna. If this field is blank or the marker does not exist then the antenna will be attached to the object's root node.                                                                                                               |
| bitmaps                     | `TagDependency`: bitmap        | A sprites bitmap containing segments of the antenna. The bitmap can contain multiple sequences which you can index with the vertex _sequence index_ to give different parts of the antenna different appearances. The alpha channel controls transparency of the segment.                                                                                               |
| physics                     | `TagDependency`: point_physics | Physics settings for each vertex of the antenna. See antenna physics.                                                                                                                                                                                                                                                                                                   |
| spring strength coefficient | `float`                        | Strength of the spring. Larger values make the string stronger, with `1` being totally rigid and `0` being rope-like with no stiffness at all.                                                                                                                                                                                                                          |
| falloff pixels              | `float`                        | Assumed to be the on-screen size where antennas begin to fade out, but doesn't seem to work correctly. In H1A and CE, setting this value over 100 results in the antenna being partially faded out but not by distance/on screen size. This may have worked on Xbox but testing is needed to confirm that. You should set this value to `100` like stock tags.          |
| cutoff pixels               | `float`                        | Assumed to be the on-screen size where antennas fade out entirely or transition to low-LOD line primitives. Again, this doesn't seem to work correctly in H1A and MCC. You should set this to `60` like stock tags.                                                                                                                                                     |
| length                      | `float`                        | * Cache only                                                                                                                                                                                                                                                                                                                                                            |
| vertices                    | `Block`                        | * HEK max count: 20 Vertices represent the start of each antenna segment and define the segment's springiness, angle, length, and appearance. The first element in the block is the part of the antenna attached to the marker and the final element is where it ends. This block must have at least 2 elements (or the antenna won't exist), and can contain up to 20. |
Field values:
- spring strength coefficient (`float`): Scales the tag-level _spring strength coefficient_, allowing for an antenna which varies stiffness over its length. For most antennas with uniform spring strength this should just be `1` for each vertex. Note that this field controls the spring strength of the **previous** segment's base point. For example: * For vertex index `0` this field has no effect. * For vertex index `1` this field affects the joint at the model marker. * For vertex index `2` this field affects the joint at the start of the second segment. * There is no way to scale the strength at the base of the final segment.
- angles (`Euler2D`): Adds a "bend" to the antenna at this point, with yaw determining the radial angle and pitch determing the amount of bend away from straight. Note that the yaw is not adjusted for the object's current "forward" direction and is instead global.
- yaw (`float`): Rotation to the left or right around the Z (vertical) axis.
- pitch (`float`): Rotation up or down.
- length (`float`): * Unit: world units Sets the length of this segment. The bitmap is scaled proportionally, so doubling the length doubles the width. The length must not be `0` or an assertion will be raised during sprite rendering, even if the sequence index doesn't exist or alpha is `0`.
- sequence index (`uint16`): Sets the sequence from the antenna bitmap used for this segment. This allows the antenna to have a different texture for different parts of its length. For example, the Warthog antenna has an unused second sequence (index `1`) which would add a red dot to the end. If the sequence doesn't exist then the segment will be invisible.
- color (`ColorARGB`): Sets a colour for this segment which is multiplied against the bitmap (including alpha). For uncoloured antennas you should set this to white with full alpha (`1` for every subfield).
- lod color (`ColorARGB`): Described by Guerilla as "color at this vertex for the low-LOD line primitives". However, stock tags do not make use of this field (all zero alpha) and antenna never revert to a line primitive regardless of _cutoff/falloff pixels_ values, at least in CE and H1A. This may be an Xbox-only feature or was never implemented.
- offset (`Point3D`): * Cache only

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `antenna`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
