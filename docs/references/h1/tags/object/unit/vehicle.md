# Halo 1 Reference: Structure and fields

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/object/unit/vehicle/`
- Local path: `tags/object/unit/vehicle/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Vehicles** are a type of unit") which can be driven by bipeds. They are dynamic objects with mass-point physics. While most examples of vehicles are fairly obvious, some more esoteric ones include the cryopods in a10, lifepods and longswords despite only being used in cinematics, and even the Halo ring seen outside the autumn's bridge.

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| vehicle flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | speed wakes physics | `0x1` | | | turn wakes physics | `0x2` | | | driver power wakes physics | `0x4` | | | gunner power wakes physics | `0x8` | | | control opposite speed sets brake | `0x10` | | | slide wakes physics | `0x20` | | | kills riders at terminal velocity | `0x40` | | | causes collision damage | `0x80` | Makes the vehicle damage actors when colliding with them, using _vehicle\_ collision\_ damage_ from globals. | | ai weapon cannot rotate | `0x100` | | | ai does not require driver | `0x200` | | | ai unused | `0x400` | | | ai driver enable | `0x800` | | | ai driver flying | `0x1000` | | | ai driver can sidestep | `0x2000` | | | ai driver hovering | `0x4000` | | |
| vehicle type | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | human tank | `0x0` | | | human jeep | `0x1` | | | human boat | `0x2` | | | human plane | `0x3` | Aircraft capable of VTOL and forward flight like the Pelican. Affected by the BSP _vehicle floor_ and _vehicle ceiling_. | | alien scout | `0x4` | Hovering and strafing-capable like the Ghost. | | alien fighter | `0x5` | Flying vehicle like the Banshee. Affected by the BSP _vehicle floor_ and _vehicle ceiling_. | | turret | `0x6` | Stationary turret like the Shade. | |
| maximum forward speed | `float` |  |
| maximum reverse speed | `float` |  |
| speed acceleration | `float` |  |
| speed deceleration | `float` |  |
| maximum left turn | `float` |  |
| maximum right turn | `float` |  |
| wheel circumference | `float` |  |
| turn rate | `float` |  |
| blur speed | `float` | The minimum speed where the vehicle's gbxmodel blur permutation is used for fake motion blur. An example is the Warthog's tires. |
| vehicle a in | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | speed absolute | `0x1` | | | speed forward | `0x2` | | | speed backward | `0x3` | | | slide absolute | `0x4` | | | slide left | `0x5` | | | slide right | `0x6` | | | speed slide maximum | `0x7` | | | turn absolute | `0x8` | | | turn left | `0x9` | | | turn right | `0xA` | | | crouch | `0xB` | | | jump | `0xC` | | | walk | `0xD` | | | velocity air | `0xE` | | | velocity water | `0xF` | | | velocity ground | `0x10` | | | velocity forward | `0x11` | | | velocity left | `0x12` | | | velocity up | `0x13` | | | left tread position | `0x14` | | | right tread position | `0x15` | | | left tread velocity | `0x16` | | | right tread velocity | `0x17` | | | front left tire position | `0x18` | | | front right tire position | `0x19` | | | back left tire position | `0x1A` | | | back right tire position | `0x1B` | | | front left tire velocity | `0x1C` | | | front right tire velocity | `0x1D` | | | back left tire velocity | `0x1E` | | | back right tire velocity | `0x1F` | | | wingtip contrail | `0x20` | | | hover | `0x21` | | | thrust | `0x22` | | | engine hack | `0x23` | | | wingtip contrail new | `0x24` | | |
| vehicle b in | `enum`? |  |
| vehicle c in | `enum`? |  |
| vehicle d in | `enum`? |  |
| maximum left slide | `float` |  |
| maximum right slide | `float` |  |
| slide acceleration | `float` |  |
| slide deceleration | `float` |  |
| minimum flipping angular velocity | `float` | * Default: 0.2 |
| maximum flipping angular velocity | `float` | * Default: 0.75 |
| fixed gun yaw | `float` |  |
| fixed gun pitch | `float` |  |
| ai sideslip distance | `float` | For floating vehicles, maximum distance for free movement under AI control. |
| ai destination radius | `float` | How close to a destination the pilot will go before searching for another one. |
| ai avoidance distance | `float` | How far the pilot tries to stay away from other units. |
| ai pathfinding radius | `float` | How far the pilot tries to stay away from pathfinding markers. |
| ai charge repeat timeout | `float` | Minimum time between charge attempts. |
| ai strafing abort range | `float` | Pilot aborts a charge when this close to the target. |
| ai oversteering bounds | `Bounds` | When AI pilot's steering angle is between these bounds, use oversteering AI behaviour. |
| | Field | Type | Comments | | --- | --- | --- | | min | `float` | | | max | `float` | | |
| ai steering maximum | `float` | The maximum angle AI pilots can steer at under normal circumstances. |
| ai throttle maximum | `float` | How fast the pilot can attempt to drive their vehicle, as a fraction of the vehicle's max speed. 0 uses max speed. |
| ai move position time | `float` |  |
| suspension sound | `TagDependency`: sound |  |
| crash sound | `TagDependency`: sound |  |
| material effects | `TagDependency`: material_effects |  |
| effect | `TagDependency`: effect | Used for hover/jet thruster effects. |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Galap _(Finding use of \_effect\_ field)_
*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_
*   Vennobennu _(Field documentation)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/object/unit/vehicle/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
