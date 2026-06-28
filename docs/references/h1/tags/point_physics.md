# Halo 1 Tag: point_physics

## Summary

The fields in this tag should not be considered in isolation. The **radius** of particles using this _point\_ physics_ is not purely visual, and it's important to understand its effect on particle movement:

## Tag Information

Tag group: `point_physics`.

HaloScript entries:

- `debug_point_physics [boolean]`

Defined fields and enum values mentioned:

- air friction
- air gravity scale
- damping
- density
- elasticity
- flags
- flags collides with structures
- flags collides with water surface
- flags flamethrower particle collision
- flags no gravity
- flags uses damped wind
- flags uses simple wind
- local variation weight
- surface friction
- unknown constant
- water friction
- water gravity scale

## Details

**Point physics** control how particles, particle systems, weather, flags, contrails, and antenna interact with the environment. This includes wind, water, density, and collisions with the BSP. Essentially anything that can be suspended in the atmosphere or water is simulated this way.

### Effects of particle radius

The fields in this tag should not be considered in isolation. The **radius** of particles using this _point\_ physics_ is not purely visual, and it's important to understand its effect on particle movement:

* Mass of the particle scales with its _volume_, which increases with the cube of the radius (scaled by density).
* Wind friction scales with _surface area_, which increases with the square of the radius.

In other words, 2x the radius is 4x the surface area but also 8x the mass. A more massive particle is harder to accelerate. You may find that doubling a _weather\_ particle\_ system_ particle radius causes it to enter into a faster freefall because air friction at low speeds can no longer overcome the force of gravity. If the particle has any air friction, it will eventually reach some equilibrium because wind resistance increases with the square of velocity while acceleration due to gravity is constant.

The following are related functions that you can use in your scenario scripts and/or debug globals that you can enter into the developer console for troubleshooting.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_point_physics [boolean])` Renders green or red markers wherever point_ physics are being simulated. This includes flags, antenna, contrails, particles, and particle_ systems. For weather_ particle_ system, markers are only shown in their simulation cube and while the `weather` global not disabled. Red markers indicate point_ physics with the _collides with structures_ flag, which are more computationally expensive. It can help to enable `framerate_throttle` for these markers to render consistently. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- flamethrower particle collision (`0x1`)
- collides with structures (`0x2`): The points will collide with scenario_ structure_ bsp and model_ collision_ geometry. Does not apply to weather_ particle_ system.
- collides with water surface (`0x4`)
- uses simple wind (`0x8`): If enabled, base wind will not be weakened by local variation weight. Guerilla describes this field as "the wind on this point won't have high-frequency variations", however this does not match tested behaviour. Instead, setting this flag causes the particle to treat the wind's _local variation weight_ differently. For example, if the weight was `0.5` and this flag is disabled, then particles will receive 50% influence from base wind and 50% influence from local variation. With this flag enabled, they will receive 100% influence from base wind plus an additional 50% influence from local variation. An additional effect of this flag is that local varation will be scaled by the weather palette entry's wind magnitude.
- uses damped wind (`0x10`): If enabled, wind is scaled by the wind tag's _damping_ factor.
- no gravity (`0x20`): The points will be unaffected by gravity.
| unknown constant | `float` | * Cache only |
| water gravity scale | `float` | * Cache only |
| air gravity scale | `float` | * Cache only |
| density | `float` | * Unit: grams per milliliter Controls the density of the particle, causing it to either rise or fall relative to the air or water, unless _no gravity_ is enabled. A density of `0.0011` is equal to air, so particles will float in place. The greater the density, the greater the particle mass too (which depends on radius). |
| air friction | `float` | A friction coefficient when particles move through the air. This affects how much they are blown by wind and also their resistance to falling under gravity. A value of `0` means they will experience no friction and fall as if there's no atmosphere, accelerating to great speeds. The exact value you use should be tested, because it depends on the density and radius of your particle. |
| water friction | `float` |  |
| surface friction | `float` |  |
| elasticity | `float` |  |

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `point_physics`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
