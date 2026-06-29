# Halo 1 Tag: physics

## Summary

Physics tags are created by importing a JMS with specially-named markers parented to nodes. The markers become mass points.

## Tag Information

Tag group: `physics`.

HaloScript entries:

- `debug_material_effects [boolean]`
- `debug_objects_physics`
- `debug_objects_position_velocity`
- `debug_objects_unit_vectors`

Defined fields and enum values mentioned:

- air friction
- center of mass
- density
- gravity scale
- ground damp fraction
- ground depth
- ground friction
- ground normal k0
- ground normal k1
- inertial matrix and inverse
- inertial matrix and inverse matrix
- inertial matrix and inverse matrix elements
- mass
- mass points
- mass points density
- mass points flags
- mass points flags metallic
- mass points forward
- mass points friction parallel scale
- mass points friction perpendicular scale
- mass points friction type
- mass points friction type forward
- mass points friction type left
- mass points friction type point
- mass points friction type up
- mass points mass
- mass points model node
- mass points name
- mass points position
- mass points powered mass point
- mass points radius
- mass points relative density
- mass points relative mass
- mass points up
- moment scale
- powered mass points
- powered mass points antigrav damp fraction
- powered mass points antigrav height
- powered mass points antigrav normal k0
- powered mass points antigrav normal k1
- powered mass points antigrav offset
- powered mass points antigrav strength
- powered mass points flags
- powered mass points flags air friction
- powered mass points flags air lift
- powered mass points flags antigrav
- powered mass points flags ground friction
- powered mass points flags thrust
- powered mass points flags water friction
- powered mass points flags water lift
- powered mass points name
- radius
- water density
- water depth
- water friction
- xx moment
- yy moment
- zz moment

## Details

**Physics** tags characterize the dynamic physics and propulsion of vehicles. They are essentially a collection of spherical _mass points_. Since vehicles can have both model_ collision_ geometry and physics, each tag is used in different situations:

* Physics are used in collisions between vehicles and is responsible for Halo 1's charactistic "bouncy" vehicles.
* Physics are used in collisions with the BSP. Because the mass points are somewhat "fuzzy" and are not continuous geometry, vehicles can become stuck in thin BSP.
* Physics are used in collisions with device_ machine and scenery.
* Collision geometry is used in collisions with bipeds, items"), and projectiles.
* Note that vehicles cannot collide with device_ light_ fixture.

Physics tags are created by importing a JMS with specially-named markers parented to nodes. The markers become mass points.

### Mass points

Mass points (also known as physics spheres) are spherical volumes with mass and density. They can have various types of friction and may provide powered impulse for flight and driving.

Even though mass points are parented to nodes, they do not move with base, overlay, or aiming animations unlike model_ collision_ geometry. However, when mass points are parented to nodes they can be used to engage a vehicle's suspension animation (e.g. the Warthog and Scorpion). The animation tag specifies the compression and extension depth. The mass points move with the node in this case.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_material_effects [boolean])` Displays cyan spheres wherever material_ effects are being generated, like under the player's feet and where physics spheres intersect with the BSP or model_ collision_ geometry. | Global |
|  | `(debug_objects_physics)` When `debug_objects` is enabled, displays physics mass points as white spheres. | Global |
|  | `(debug_objects_position_velocity)` When `debug_objects` is enabled, displays red, green, and blue object-space reference axes and a yellow velocity vector on each object. | Global |
|  | `(debug_objects_unit_vectors)` When `debug_objects` is enabled, displays white and red vectors on objects. Their meaning is unknown. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| radius | `float` |  |
| moment scale | `float` |  |
| mass | `float` |  |
| center of mass | `Point3D` |  |
| density | `float` |  |
| gravity scale | `float` |  |
| ground friction | `float` | A coefficient which modifies the amount of friction applied to the vehicle when colliding with "ground" surfaces, which may just mean any BSP surface which is not a water plane. It is unknown if this also modifies friction on scenery collisions. A typical value from the Warthog is 0.23. |
| ground depth | `float` |  |
| ground damp fraction | `float` |  |
| ground normal k1 | `float` | Used to determine how steep of a surface the vehicle can climb before its powered mass points no longer have an effect. A typical value is ~0.7, while a value near 1.0 allows vertical climbing assuming sufficient friction. |
| ground normal k0 | `float` | This is also used to determine how steep of a surface the vehicle can climb and seems to be some sort of lower range bound for fading out powered mass points. A typical value is ~0.5, with -1.0 allowing vertical wall climbing. |
| water friction | `float` |  |
| water depth | `float` |  |
| water density | `float` |  |
| air friction | `float` |  |
| xx moment | `float` |  |
| yy moment | `float` |  |
| zz moment | `float` |  |
| inertial matrix and inverse | `Block` | * HEK max count: 2 * Volatile * Min: 2 * Max: 2 |
Field values:
- matrix (`Matrix`)
- elements (`float[9]`)
| powered mass points | `Block` | * HEK max count: 32 |
Flag values:
- name (`TagString`)
- flags (`bitfield`)
- ground friction (`0x1`)
- water friction (`0x2`)
- air friction (`0x4`)
- water lift (`0x8`)
- air lift (`0x10`)
- thrust (`0x20`)
- antigrav (`0x40`)
- antigrav strength (`float`)
- antigrav offset (`float`)
- antigrav height (`float`)
- antigrav damp fraction (`float`)
- antigrav normal k1 (`float`)
- antigrav normal k0 (`float`)
| mass points | `Block` | * HEK max count: 32 |
Option values:
- name (`TagString`)
- powered mass point (`int16`)
- model node (`int16`)
- flags (`bitfield`)
- metallic (`0x1`)
- relative mass (`float`)
- mass (`float`)
- relative density (`float`)
- density (`float`)
- position (`Point3D`)
- forward (`Vector3D`)
- up (`Vector3D`)
- friction type (`enum`)
- point (`0x0`)
- forward (`0x1`)
- left (`0x2`)
- up (`0x3`)
- friction parallel scale (`float`)
- friction perpendicular scale (`float`)
- radius (`float`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `physics`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
