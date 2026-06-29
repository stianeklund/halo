# Halo 1 Tag: glow

## Summary

attachment marker`TagString`The name of the gbxmodel marker(s) which make up the glow path. The model can have up 5 markers with the same name. A model can have multiple sets of markers, e.g. `glow 1` and `glow 2`.

## Tag Information

Tag group: `glow`.

Defined fields and enum values mentioned:

- attachment 0
- attachment 1
- attachment 2
- attachment 3
- attachment 4
- attachment 5
- attachment marker
- boundary effect
- boundary effect bounce
- boundary effect wrap
- color bound 0
- color bound 1
- color rate of change
- distance to object mul high
- distance to object mul low
- effect rot vel mul high
- effect rot vel mul low
- effect rotational velocity
- effect trans vel mul high
- effect trans vel mul low
- effect translational velocity
- fading percentage of glow
- glow flags
- glow flags modify particle color in range
- glow flags partices move in both directions
- glow flags particles move backwards
- glow flags trailing particles fade over time
- glow flags trailing particles shrink over time
- glow flags trailing particles slow over time
- lifetime of trailing particles
- max distance particle to object
- min distance particle to object
- normal particle distribution
- normal particle distribution distributed randomly
- normal particle distribution distributed uniformly
- number of particles
- particle generation freq
- particle rot vel mul high
- particle rot vel mul low
- particle rotational velocity
- particle size bounds
- particle size bounds max
- particle size bounds min
- scale color 0
- scale color 1
- size attachment multiplier
- texture
- trailing particle distribution
- trailing particle distribution emit normal up
- trailing particle distribution emit randomly
- trailing particle distribution emit vertically
- trailing particle maximum t
- trailing particle minimum t
- type sprites
- velocity of trailing particles

## Details

attachment marker`TagString`The name of the gbxmodel marker(s) which make up the glow path. The model can have up 5 markers with the same name. A model can have multiple sets of markers, e.g. `glow 1` and `glow 2`.

If no markers have this name, or the field is left empty, a marker will be derived from the object's root node as the default.

When the path is only defined by a single marker (including the root default) then then normal particles will not be generated. Trailing particles can still be generated.
number of particles`uint16`How many normal particles to distribute along the path. The game is limited to 512 glow particles total, across up to 8 glow effects, so budget according to how many glow effects you expect to exist simultaneously.

You can set this to `0` if you only want to use trailing particles.
boundary effect`enum`Determines the behaviour of particles which reach the end of the path.
| Option | Value | Comments |
| --- | --- | --- |
| bounce | `0x0` | When glow particles reach the end of the path, they will change direction and return. |
| wrap | `0x1` | When glow particles reach the end of the path, they will reappear at its start. |
normal particle distribution`enum`Controls how particles are distributed along the glow path.
| Option | Value | Comments |
| --- | --- | --- |
| distributed randomly | `0x0` | Particles will be randomly spaced along the glow path. |
| distributed uniformly | `0x1` | Particles will be evenly spaced along the glow path. Glow markers do not need to be uniformly distanced for this to work; the game can calculate the total path length and space particles accordingly. |
trailing particle distribution`enum`Controls how trailing particles are generated from the path segment defined at the end of the tag. Unaffected by _particles move backwards_ and _particles move in both directions_.
| Option | Value | Comments |
| --- | --- | --- |
| emit vertically | `0x0` | Trailing particles emit in the global Z direction. |
| emit normal up | `0x1` | Trailing particles emit in the marker-local Z direction. |
| emit randomly | `0x2` | Trailing particles emit spherically in random directions. |
glow flags`bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| modify particle color in range | `0x1` | If set, the particle color will fade as they translate along the path. The fade behaviour is somewhat unintuitive; rather than fade between _color\_ bound\_ 0_ and _color\_ bound\_ 1_ from start to end, each color channel fades at a different rate depending on both _color rate of change_ and the per-channel difference between color bounds. The color `c` at distance `d` from the path start, having color bound values `c0` and `c1`, and _color rate of change_`v`, is calculated with: `let c = (c0 + v * d * (c1 - c0)) % 1` For example: assume a bound 0 red of `0.8`, a bound 1 red of `1`, rate of change `10`, and distance `1.37`. The red value will be: ``` c = (0.8 + 10 * 1.37 * (1 - 0.8)) % 1 c = 3.54 % 1 c = 0.54 ``` If not set, particle colors are randomly selected between the color bounds or interpolated via function attachment. |
| particles move backwards | `0x2` | Reverses the direction particles translate along the path. |
| partices move in both directions | `0x4` | Every second particle travels in the reverse direction. |
| trailing particles fade over time | `0x8` | Trailing particles fade out to `0` opacity over their lifetime. |
| trailing particles shrink over time | `0x10` | If set, trailing particles will shrink from an initial randomly-selected bounds size to `0` over their lifetime, at which point for a single frame the particle texture will render at actual resolution before disappearing. This seems to be due to the size `0` defaulting to actual pixel scale rather than scaled by distance. This can be mitigated by also using _trailing particles fade over time_ so the particle is invisible for that frame anyway. If _not set_, trailing particles are always shown at actual resolution. |
| trailing particles slow over time | `0x20` | Trailing particles gradually slow down and come to a stop at the end of their lifetime. |
attachment 0`enum`Seemingly unused. Setting this to an object function has no effect.
particle rotational velocity`float`No effect on normal or trailing particles. With a non-zero value, particle sprites did not rotate in screen space or radially around the glow path. Neither an object function attachment nor multiplier values changed this.
particle rot vel mul low`float`No visible effect.
particle rot vel mul high`float`No visible effect.
attachment 1`enum`?Sets an object function which controls the rotational velocity.
effect rotational velocity`float`Controls how quickly normal particles rotate radially around the path, in radians/sec.
effect rot vel mul low`float`Rotational velocity multipler when the function value is at 0. This only takes effect when a function attachment is set. It's not known exactly how this multiplier works; it has a gradually varying effect over the length of the glow path.
effect rot vel mul high`float`As above, but for a high function value.
attachment 2`enum`?Sets an object function which controls the translational velocity.
effect translational velocity`float`Controls how quickly particles move along the path, in world units/sec. This is not used for trailing particles, which instead use _velocity of trailing particles_.
effect trans vel mul low`float`Multiplier for the translational velocity for the function low value. Must be non-zero when a function is being used, or else the game will crash on `render_cameras.c,#1086: bounds->x0<=bounds->x1`.
effect trans vel mul high`float`Multiplier for the translational velocity for the function high value. This value can be `0` safely.
attachment 3`enum`?Allows the radial distance of normal particles to be scaled by an object function. No effect on trailing particles.
min distance particle to object`float`When the distance attachment function is NONE, sets the minimum radial distance from the glow path that particles will be randomly placed. When distance is scaled by a function, sets the minimum distance of all particles. No effect on trailing particles.

Radial distances are perpendicular to the path markers' +X axes, meaning marker rotation matters. Distance cannot vary over the length of the path, even when using a function, but you can twist some of the markers in the path to introduce a pinching effect.
max distance particle to object`float`When the distance attachment function is NONE, sets the maximum radial distance from the glow path that particles will be randomly placed. When distance is scaled by a function, sets the maximum distance of all particles. No effect on trailing particles.
distance to object mul low`float`When distance is scaled by an attachment function, determines the how a function value of `0` maps to a linear interpolation between min and max distances. For example, a multiplier of `0` means when the function is low, the distance will be equal to _min distance particle to object_.
distance to object mul high`float`When distance is scaled by an attachment function, determines the how a function value of `1` maps to a linear interpolation between min and max distances. For example, a multiplier of `1` means when the function is high, the distance will be equal to _max distance particle to object_.
attachment 4`enum`?Intended to make particle size scale with a function, but does not work. When not NONE, all particle spites render at their actual resolution regardless of distance rather than scale with the function.

This does not affect trailing particles, which continue to be randomly selected between size bounds if _trailing particles shrink over time_ is set.
particle size bounds`Bounds`When particle size is not scaled by a function attachment, particle sizes will be randomly chosen from this range. When scaled by a function, or when size bounds are both `0`, particle sprites are rendered at their actual resolution regardless of distance.

Trailing particles will always render at actual resolution unless _trailing particles shrink over time_ is set, in which case size is selected randomly from these bounds regardless of any function attachment.
| Field | Type | Comments |
| --- | --- | --- |
| min | `float` |  |
| max | `float` |  |
size attachment multiplier`Bounds`Does not work as intended. When a function is used, these values don't affect the sprite size.
attachment 5`enum`?If set to a function, all normal particles will blend between _color bound 0_ and _color bound 1_ according to the function. Otherwise particle colors are randomly selected from the range. When _modify particle color in range_ is set, both cases are overridden and particles blend over the length of the glow path according to _color rate of change_.

No effect on trailing particles, which always randomly select from the bounds.
color bound 0`ColorARGB`Sets a lower bound for particle color selection. Particle colors are interpolated in RGB space, not HSL. The chosen color is multiplied with the sprite texture. The alpha value has no effect, unlike in some tags where it controls tint vs. modulation.
color bound 1`ColorARGB`Sets an upper bound for particle color selection.
scale color 0`ColorARGB`No visible effect in any color selection modes.
scale color 1`ColorARGB`No visible effect in any color selection modes.
color rate of change`float`When _modify particle color in range_ is set, controls how many times the per-channel difference between color bounds is added per world unit of distance along the glow path.
fading percentage of glow`float`Controls the distance that particles fade in and out at the path boundaries. A value of `0` means they do not fade in/out at all, while a value of `1` means the particles reach full opacity only at the middle of the path before beginning to fade out again.
particle generation freq`float`The frequency that trailing particles are generated, in particles/sec. This is likely limited by the tick rate since values over 30 have no visual difference.
lifetime of trailing particles`float`How long trailing particles live. If _particle generation freq_ is non-zero, then this field must be too. An undefined lifetime will cause a crash on `render_cameras.c,#1086: bounds->x0<=bounds->x1`.
velocity of trailing particles`float`Initial trailing particle velocity at generation. Although this is labeled "wu/s", testing found that the the actual speed is roughly 1/40th the value given here. The exact factor or what factors might apply is unknown. Unaffected by _effect translational velocity_.
trailing particle minimum t`float`Trailing particles will spawn from this minimum distance along the glow path, where `0` is the start of the path and `1` is the end of the path. This has no effect if the path is made of a single marker.
trailing particle maximum t`float`Trailing particles will spawn up to this maximum distance along the glow path, where `0` is the start of the path and `1` is the end of the path. This has no effect if the path is made of a single marker.
texture`TagDependency`: bitmapThe sprite texture of particles. The referenced bitmap must have type _sprites_. It will not be randomly selected from sprite sheets (just the first). The particle is always shown camera-facing and with additive framebuffer blending. Alpha channel is ignored.

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `glow`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
