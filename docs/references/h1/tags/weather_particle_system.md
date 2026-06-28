# Halo 1 Tag: weather_particle_system

## Summary

However, if you intend to have acceleration and it changes with _acceleration\_ change\_ rate_, **ensure the minimum magnitude is non-zero**. For some reason, once particles randomly reach an acceleration of 0, they get stuck there and will not change back to higher values in the range, resulting in all acceleration eventually stopping in the weather particle system. This happens sooner at higher FPS. acceleration turning rate`float`Controls how quickly the particles change their forward direction. Setting this to `0` means they will always accelerate in the same direction, while numbers like `0.2` cause the particles to change direction and "wander". Turning is unaffected by the particle's visual _rotation rate_ field.

## Tag Information

Tag group: `weather_particle_system`.

Defined fields and enum values mentioned:

- air friction
- particle types acceleration change rate
- particle types acceleration magnitude
- particle types acceleration turning rate
- particle types anchor
- particle types anchor with primary
- particle types anchor with screen space
- particle types anchor zsprite
- particle types animation rate
- particle types bitmap
- particle types color lower bound
- particle types color upper bound
- particle types fade in end distance
- particle types fade in end height
- particle types fade in start distance
- particle types fade in start height
- particle types fade out end distance
- particle types fade out end height
- particle types fade out start distance
- particle types fade out start height
- particle types flags
- particle types flags 1
- particle types flags along long hue path
- particle types flags interpolate colors in hsv
- particle types flags random rotation
- particle types framebuffer blend function
- particle types framebuffer blend function add
- particle types framebuffer blend function alpha blend
- particle types framebuffer blend function alpha multiply add
- particle types framebuffer blend function component max
- particle types framebuffer blend function component min
- particle types framebuffer blend function double multiply
- particle types framebuffer blend function multiply
- particle types framebuffer blend function subtract
- particle types framebuffer fade mode
- particle types framebuffer fade mode fade when parallel
- particle types framebuffer fade mode fade when perpendicular
- particle types framebuffer fade mode none
- particle types map flags
- particle types map flags unfiltered
- particle types name
- particle types not broken
- particle types particle count
- particle types particle count max
- particle types particle count min
- particle types particle radius
- particle types physics
- particle types render direction source
- particle types render direction source from acceleration
- particle types render direction source from velocity
- particle types render mode
- particle types render mode parallel to direction
- particle types render mode perpendicular to direction
- particle types render mode screen facing
- particle types rotation animation center
- particle types rotation animation center x
- particle types rotation animation center y
- particle types rotation animation function
- particle types rotation animation period
- particle types rotation animation phase
- particle types rotation animation scale
- particle types rotation animation source
- particle types rotation rate
- particle types rotation rate max
- particle types rotation rate min
- particle types shader flags
- particle types shader flags dont overdraw fp weapon
- particle types shader flags nonlinear tint
- particle types shader flags sort bias
- particle types sprite bitmap
- particle types sprite size
- particle types u animation function
- particle types u animation function cosine
- particle types u animation function cosine variable period
- particle types u animation function diagonal wave
- particle types u animation function diagonal wave variable period
- particle types u animation function jitter
- particle types u animation function noise
- particle types u animation function one
- particle types u animation function slide

## Details

name`TagString`A name for the particle type. This can be anything and is only used to make identifying the type in Guerilla easier.
flags`bitfield`Boolean settings for this particle type.
| Flag | Mask | Comments |
| --- | --- | --- |
| interpolate colors in hsv | `0x1` | When the game gives particles a random color between the _color lower bound_ and _color upper bound_, it will interpolate in HSV space rather than RGB. This results in intermediate hues being represented rather than the muddy mix that sometimes comes from blending in RGB. |
| along long hue path | `0x2` | When _interpolate colors in hsv_ is enabled, this causes the hue interpolation to go the longer direction. For example, if blue and green are bounds then purple, red, orange, and yellow will be present. |
| random rotation | `0x4` | Particles spawn with random rotation, allowing for more visual variation despite using the same sprite. This applies no matter the _render mode_. |
fade in start distance`float`The distance at which particles begin to fade in. This should be fairly close to the camera/player, but avoid having no fade at all or a distance of 0 since it can distracting for particles to come very close to the camera.
fade in end distance`float`The fade in distance where particles have reached full opacity.
fade out start distance`float`The distance where distant particles begin to fade out.
fade out end distance`float`The distance where distant particles fully fade out. This determines the dimensions of the particle type's simulation cube and affects the maximum achievable particle density.
fade in start height`float`A world-space Z height where particles begin to fade in. Think of this as the lower bound. If you want the weather to be present throughout the level, set this and the _fade in end height_ to some large negative number like `-500`.
fade in end height`float`A world-space Z height where particles reach full opacity.
fade out start height`float`A world-space Z height where particles begin to fade out again.
fade out end height`float`A world-space Z height where particles fully fade out. If you want the weather to be present throughout the level, set this and the _fade out start height_ to some large positive number like `500`.
particle count`Bounds`Sets the density of particles for this type. All particle types must share the 512 particle budget, so arbitrarily high densities cannot be achieved for a given _fade out end distance_.
| Field | Type | Comments |
| --- | --- | --- |
| min | `float` |  |
| max | `float` |  |
physics`TagDependency`: point_physicsA reference to a point_ physics which controls physical properties of the weather particles, such as their density, air friction, and more.
acceleration magnitude`Bounds`?The amount of "forward" acceleration for each particle, used for particles that can accelerate on their own like c10's flying insects. Particles will be initialized with a random acceleration in this range. The particles' _point\_ physics_ should have some _air friction_ to avoid the particles accelerating indefinitely. Set the min/max to `0` if you don't want the particles to accelerate on their own (e.g. wind and rain).

However, if you intend to have acceleration and it changes with _acceleration\_ change\_ rate_, **ensure the minimum magnitude is non-zero**. For some reason, once particles randomly reach an acceleration of 0, they get stuck there and will not change back to higher values in the range, resulting in all acceleration eventually stopping in the weather particle system. This happens sooner at higher FPS.
acceleration turning rate`float`Controls how quickly the particles change their forward direction. Setting this to `0` means they will always accelerate in the same direction, while numbers like `0.2` cause the particles to change direction and "wander". Turning is unaffected by the particle's visual _rotation rate_ field.

This rate is framerate-dependent so particles will turn faster at higher FPS. This may result in particles not wandering as desired since they do not point in a given direction for long enough. Values up to `0.5` are recommended when targeting non-Xbox platforms so the turning is noticeble.
acceleration change rate`float`Sets the rate of change in the amount of _acceleration magnitude_, between the bounds specified. If set to `0` the particles will only have the acceleration they were initialized with, while a value like `0.2` means they will sometimes speed up or slow down.

Like the turning rate, this is framerate-dependent. Values higher than `0.5` at high FPS result in the acceleration changing too frequently for noticeable speed changes in the particles.
particle radius`Bounds`?Scales the size of the particles, with the size being chosen randomly from this range. This is not purely a visual setting but affects the mass and surface area of the simulated particle, and therefore how it moves.
animation rate`Bounds`?If the _sprite bitmap_ has sequences, this determines the framerate bounds of the animation. Avoid negative values, which cause flickering in the particles.
rotation rate`Bounds`Sets the rotation rate of sprites, which will randomly be in either clockwise or counter-clockwise direction. This applies no matter the _render mode_, and does not affect acceleration if present.
| Field | Type | Comments |
| --- | --- | --- |
| min | `float` |  |
| max | `float` |  |
color lower bound`ColorARGB`Lower bound for random particle color interpolation. The colors are multiplied against the sprite texture, and the alpha controls opacity of the particle. Set these colors to white if you don't want any color variation.
color upper bound`ColorARGB`Upper bound for random particle color interpolation.
sprite size`float`
sprite bitmap`TagDependency`: bitmapA bitmap used for the particles, which must be _sprites_ type. This bitmap can contain multiple sequences for random variation and animated particles.
render mode`enum`Sets the orientation of particle sprites.
| Option | Value | Comments |
| --- | --- | --- |
| screen facing | `0x0` | Particles will face directly toward the viewer, ideal for effects like smoke and snow. |
| parallel to direction | `0x1` | Particles will be oriented along the direction of movement, facing the camera but only rotating around the axis of movement. This type is ideal for particles with trails like rain and sparks. In the sprite texture, right is the forward direction. |
| perpendicular to direction | `0x2` | Particles will be oriented perpendicular to the direction of movement, so will be seen face-on when the particle is moving toward or away from the camera. |
render direction source`enum`Determines the direction to use for directional sprite orientations (parallel and perpendicular).
| Option | Value | Comments |
| --- | --- | --- |
| from velocity | `0x0` | Particles sprites are oriented relative to their velocity vector. This is generally what you want, even for particles with acceleration. The random hard-coded motion added to weather particles |
| from acceleration | `0x1` | Particle sprites are oriented relative to their acceleration vectors, making it look like the sprite is pointing in the direction it's trying to move. This should only be used when _acceleration turning rate_ is low or else the rotation of sprites will be very jittery. The flying insect particles for c10 _don't_ use this setting. |
not broken`uint32`
shader flags`bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| sort bias | `0x1` |  |
| nonlinear tint | `0x2` |  |
| don't overdraw fp weapon | `0x4` |  |
framebuffer blend function`enum`Sets how the particle will blend against the background. You'll usually want _alpha blend_ for solid particles like snow, or _add_ for glowing particles like embers. For _alpha blend_ make sure your sprite bitmap has an alpha channel.
| Option | Value | Comments |
| --- | --- | --- |
| alpha blend | `0x0` |  |
| multiply | `0x1` |  |
| double multiply | `0x2` |  |
| add | `0x3` |  |
| subtract | `0x4` |  |
| component min | `0x5` |  |
| component max | `0x6` |  |
| alpha multiply add | `0x7` |  |
framebuffer fade mode`enum`Sets how sprites fade based on viewing angle. For directional orientations, _fade when perpendicular_ can help avoid seeing the sprite as a spinning 2d plane when viewed edge-on.
| Option | Value | Comments |
| --- | --- | --- |
| none | `0x0` | The faces do not fade by viewing angle. |
| fade when perpendicular | `0x1` | The faces fade out when viewed edge-on. |
| fade when parallel | `0x2` | The faces fade out when viewed flat to the camera. |
map flags`bitfield`Flag controlling if the texture is filtered.
| Flag | Mask | Comments |
| --- | --- | --- |
| unfiltered | `0x1` | Texture sampling is unfiltered, resulting in aliasing at a distance and a blocky pixelated look up close. |
bitmap`TagDependency`: bitmapSecondary map reference used to color the particles, by multiply. This map's UV coordinates can be animated along each axis and rotated, with the fields working as you would expect from a shader tag.
anchor`enum`Determines how the secondary map is positioned. If _with primary_, the sprite is multiplied atop each weather particle and follows its _render mode_ orientation. The _zsprite_ setting has unknown usage and may not apply to weather particles.
| Option | Value | Comments |
| --- | --- | --- |
| with primary | `0x0` | Positioned relative to each particle. |
| with screen space | `0x1` | Positioned relative to the screen. |
| zsprite | `0x2` |  |
flags 1`bitfield`?Flag controlling if the texture is filtered.
u animation source`enum`Function source for U animation. It's unclear what this would be connected to and may be unused.
| Option | Value | Comments |
| --- | --- | --- |
| none | `0x0` |  |
| a out | `0x1` |  |
| b out | `0x2` |  |
| c out | `0x3` |  |
| d out | `0x4` |  |
u animation function`enum`An animation function which allows the secondary bitmap to scroll in the U axis.
| Option | Value | Comments |
| --- | --- | --- |
| one | `0x0` |  |
| zero | `0x1` |  |
| cosine | `0x2` |  |
| cosine variable period | `0x3` |  |
| diagonal wave | `0x4` |  |
| diagonal wave variable period | `0x5` |  |
| slide | `0x6` |  |
| slide variable period | `0x7` |  |
| noise | `0x8` |  |
| jitter | `0x9` |  |
| wander | `0xA` |  |
| spark | `0xB` |  |
u animation period`float`
u animation phase`float`
u animation scale`float`
v animation source`enum`?Function source for U animation. It's unclear what this would be connected to and may be unused.
v animation function`enum`?An animation function which allows the secondary bitmap to scroll in the V axis.
v animation period`float`
v animation phase`float`
v animation scale`float`
rotation animation source`enum`?
rotation animation function`enum`?
rotation animation period`float`
rotation animation phase`float`
rotation animation scale`float`
rotation animation center`Point2D`
| Field | Type | Comments |
| --- | --- | --- |
| x | `float` |  |
| y | `float` |  |
zsprite radius scale`float`

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `weather_particle_system`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
