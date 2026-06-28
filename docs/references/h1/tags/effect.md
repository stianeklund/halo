# Halo 1 Tag: effect

## Summary

Thanks to the following individuals for their research or contributions to this topic:

## Tag Information

Tag group: `effect`.

Defined fields and enum values mentioned:

- events
- events delay bounds
- events delay bounds max
- events delay bounds min
- events duration bounds
- events particles
- events particles a scales values
- events particles a scales values angular velocity
- events particles a scales values angular velocity delta
- events particles a scales values count
- events particles a scales values count delta
- events particles a scales values distribution radius
- events particles a scales values distribution radius delta
- events particles a scales values particle radius
- events particles a scales values particle radius delta
- events particles a scales values tint
- events particles a scales values velocity
- events particles a scales values velocity cone angle
- events particles a scales values velocity delta
- events particles angular velocity
- events particles b scales values
- events particles count
- events particles count max
- events particles count min
- events particles create
- events particles create in
- events particles create in first person if possible
- events particles create independent of camera mode
- events particles create only in first person
- events particles create only in third person
- events particles distribution function
- events particles distribution function buildup
- events particles distribution function buildup and falloff
- events particles distribution function constant
- events particles distribution function end
- events particles distribution function falloff
- events particles distribution function start
- events particles distribution radius
- events particles flags
- events particles flags across the long hue path
- events particles flags interpolate tint as hsv
- events particles flags random initial angle
- events particles flags stay attached to marker
- events particles flags tint from object color
- events particles location
- events particles particle type
- events particles radius
- events particles relative direction
- events particles relative direction pitch
- events particles relative direction vector
- events particles relative direction yaw
- events particles relative offset
- events particles tint lower bound
- events particles tint upper bound
- events particles velocity
- events particles velocity cone angle
- events particles violence mode
- events parts
- events parts a scales values
- events parts a scales values angular velocity
- events parts a scales values angular velocity delta
- events parts a scales values type specific scale
- events parts a scales values velocity
- events parts a scales values velocity cone angle
- events parts a scales values velocity delta
- events parts angular velocity bounds
- events parts angular velocity bounds max
- events parts angular velocity bounds min
- events parts b scales values
- events parts create in
- events parts create in air only
- events parts create in any environment
- events parts create in space only
- events parts create in water only
- events parts flags
- events parts flags face down regardless of location decals
- events parts flags make effect work
- events parts flags unused
- events parts location
- events parts radius modifier bounds

## Details

**Effects** are a multi-purpose tag used for responses to various events like material impacts, sound effects, projectile detonations, and more. They are made up of _parts_ which can can spawn sounds, particles, lights, decals, and even new objects").

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- deleted when attachment deactivates (`0x1`)
- required for gameplay cannot optimize out (`0x2`)
- must be deterministic (`0x4`): * Cache only * H1A only Prevents culling of effects that are important for synchronous networking determinism.
- disabled in remastered by blood setting (`0x8`): * H1A only
| loop start event | `uint16`→ |  |
| loop stop event | `uint16`→ |  |
| maximum damage radius | `float` | * Cache only |
| locations | `Block` | * HEK max count: 32 |
Field values:
- marker name (`TagString`)
| events | `Block` | * HEK max count: 32 |
Flag values:
- skip fraction (`float`)
- delay bounds (`Bounds`): * Unit: seconds
- min (`float`)
- max (`float`)
- duration bounds (`Bounds`?): * Unit: seconds
- parts (`Block`): * HEK max count: 32
- create in (`enum`)
- any environment (`0x0`)
- air only (`0x1`)
- water only (`0x2`)
- space only (`0x3`)
- violence mode (`enum`)
- either mode (`0x0`)
- violent mode only (`0x1`)
- nonviolent mode only (`0x2`)
- location (`uint16`→)
- flags (`bitfield`)
- face down regardless of location decals (`0x1`)
- unused (`0x2`): * Cache only
- make effect work (`0x4`): * Cache only
- type class (`uint32`): * Cache only
- type (`TagDependency` (6) * damage_effect * object * particle_system * sound * decal * light)
- velocity bounds (`Bounds`?): * Unit: world units per second
- velocity cone angle (`float`)
- angular velocity bounds (`Bounds`): * Unit: degrees per second
- min (`float`)
- max (`float`)
- radius modifier bounds (`Bounds`?)
- a scales values (`bitfield`)
- velocity (`0x1`)
- velocity delta (`0x2`)
- velocity cone angle (`0x4`)
- angular velocity (`0x8`)
- angular velocity delta (`0x10`)
- type specific scale (`0x20`)
- b scales values (`bitfield`?)
- particles (`Block`): * HEK max count: 32
- * Processed during compile (Field): Type
- Comments (): ---
- create in (`enum`?)
- violence mode (`enum`?)
- create (`enum`)
- independent of camera mode (`0x0`)
- only in first person (`0x1`)
- only in third person (`0x2`)
- in first person if possible (`0x3`)
- location (`uint16`→)
- relative direction (`Euler2D`)
- yaw (`float`): Rotation to the left or right around the Z (vertical) axis.
- pitch (`float`): Rotation up or down.
- relative offset (`Point3D`)
- relative direction vector (`Vector3D`): * Cache only
- particle type (`TagDependency`: particle)
- flags (`bitfield`)
- stay attached to marker (`0x1`)
- random initial angle (`0x2`)
- tint from object color (`0x4`)
- interpolate tint as hsv (`0x8`)
- across the long hue path (`0x10`)
- distribution function (`enum`)
- start (`0x0`)
- end (`0x1`)
- constant (`0x2`)
- buildup (`0x3`)
- falloff (`0x4`)
- buildup and falloff (`0x5`)
- count (`Bounds`)
- min (`uint16`)
- max (`uint16`)
- distribution radius (`Bounds`?): * Unit: world units
- velocity (`Bounds`?): * Unit: world units per second
- velocity cone angle (`float`)
- angular velocity (`Bounds`?): * Unit: degrees per second
- radius (`Bounds`?): * Unit: world units
- tint lower bound (`ColorARGB`)
- tint upper bound (`ColorARGB`)
- a scales values (`bitfield`)
- velocity (`0x1`)
- velocity delta (`0x2`)
- velocity cone angle (`0x4`)
- angular velocity (`0x8`)
- angular velocity delta (`0x10`)
- count (`0x20`)
- count delta (`0x40`)
- distribution radius (`0x80`)
- distribution radius delta (`0x100`)
- particle radius (`0x200`)
- particle radius delta (`0x400`)
- tint (`0x800`)
- b scales values (`bitfield`?)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `effect`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
