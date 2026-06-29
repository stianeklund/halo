# Halo 1 Reference: Physics pill

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/object/unit/biped/`
- Local path: `tags/object/unit/biped/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Biped** tags represent the "bodies" of AI- and player-driven characters. They can take on a wide range of forms, like flood infection forms, sentinels, marines, grunts, and scripted characters like Cortana. Invisible bipeds are even used to pilot dropships and detecting explosive damage to the ship's reactor in _The Maw_. Bipeds are a type of unit").

On its own, a biped will not _do_ anything unless being controlled by a player or actor_ variant. This tag defines key physical characteristics like height, speed, collision, and appearance.

### Physics pill

Biped bodies are approximated by vertically-oriented capsule/pill shapes) for their physical interactions with the level BSP, the model_ collision_ geometry of objects (like scenery, devices, vehicles), and with other biped physics pills. Note that the physics pill is _not_ used as the "hitbox" for projectiles; a biped's model_ collision_ geometry is tested for projectile ray casts.

Its width depend on this tag's collision radius. The height depends on standing and crouching collision heights only if the biped uses player physics, and otherwise has 0 height (making the pill a sphere).

You can enable `debug_objects` and `debug_objects_biped_physics_pills` to visualize physics pills.

### Autoaim pill

Two Grunts, the player, and an Elite shown with just their node skeletons, physics pills (white), and autoaim pills (red).

Autoaim pills are part of the autoaim system which causes projectiles to fire towards a biped when the pill is with in a weapon's autoaim angle and range. The width of this pill is controlled within this tag, but its height and placement depends on the model:

*   If the biped has head and pelvis nodes...
    *   If the spherical flag is set, then the pill is a sphere at the midpoint between head and pelvis.
    *   Otherwise, the capsule spans between the head and pelvis nodes.

*   If the biped _doesn't_ have both of these nodes, the autoaim pill is vertically-aligned and its height is a fraction of the physics pill's. It will be slightly elevated from the physics pill's base.

You can enable `debug_objects` and `debug_objects_biped_autoaim_pills` to visualize autoaim pills.

### NTSC vs PAL physics

Halo CE for the Xbox was originally released under two different analog TV standards: NTSC in the Americas and PAL in Europe. The NTSC version ran at 30 FPS, while PAL ran at 25 FPS. Since the simulation tick rate was tied to the frame rate, this meant time in the PAL edition would run slower than the NTSC version. To compensate, the developers increased speeds and firing rates across a variety of tags, including player speed in globals.

This tag's _unit uses old ntsc player physics_ flag was a hack added for the PAL edition and remained in the engine through the PC port, H1A, and MCC. It causes the engine to override the player globals values with the following hard-coded values:

*   _walking speed_: `0.512`
*   _run forward speed_: `2.25`
*   _run backward speed_: `2.0`
*   _run sideways speed_: `2.0`
*   _run acceleration_: `0.32`

These values are identical to the globals in all non-PAL versions of the game. The flag only has a visible effect on the Xbox PAL version due to the difference in globals there, and is only used on the `cyborg_cinematic.biped`. This is presumably to fix recorded animations which assumed NTSC physics.

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| moving turning speed | `float` |  |
| biped flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | turns without animating | `0x1` | | | uses player physics | `0x2` | The physics pill height will be controlled by standing and crouching collision heights rather being a simple sphere. | | flying | `0x4` | | | physics pill centered at origin | `0x8` | | | spherical | `0x10` | The autoaim pill is a sphere at the midpoint between the head and pelvis nodes, rather than a capsule spanning the head and pelvis nodes. | | passes through other bipeds | `0x20` | | | can climb any surface | `0x40` | | | immune to falling damage | `0x80` | | | rotate while airborne | `0x100` | | | uses limp body physics | `0x200` | | | has no dying airborne | `0x400` | | | random speed increase | `0x800` | | | unit uses old ntsc player physics | `0x1000` | Causes the game to use hard-coded NTSC speed and acceleration globals for player physics, used in the original Xbox PAL release on `cyborg_cinematic.biped`. This flag can be ignored in modern modding setups. | |
| stationary turning threshold | `float` | * Unit: degrees per second |
| biped a in | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | flying velocity | `0x1` | | |
| biped b in | `enum`? |  |
| biped c in | `enum`? |  |
| biped d in | `enum`? |  |
| don't use | `TagDependency`: damage_effect | * Unused |
| bank angle | `float` |  |
| bank apply time | `float` | * Unit: seconds |
| bank decay time | `float` | * Unit: seconds |
| pitch ratio | `float` |  |
| max velocity | `float` | * Unit: world units per second |
| max sidestep velocity | `float` | * Unit: world units per second |
| acceleration | `float` | * Unit: world units per second squared |
| deceleration | `float` | * Unit: world units per second squared |
| angular velocity maximum | `float` | * Unit: degrees per second |
| angular acceleration maximum | `float` | * Unit: degrees per second squared |
| crouch velocity modifier | `float` | * Min: 0 * Max: 1 |
| maximum slope angle | `float` |  |
| downhill falloff angle | `float` |  |
| downhill cutoff angle | `float` |  |
| downhill velocity scale | `float` |  |
| uphill falloff angle | `float` |  |
| uphill cutoff angle | `float` |  |
| uphill velocity scale | `float` |  |
| footsteps | `TagDependency`: material_effects |  |
| jump velocity | `float` | * Unit: world units per tick |
| maximum soft landing time | `float` | * Unit: seconds |
| maximum hard landing time | `float` | * Unit: seconds |
| minimum soft landing velocity | `float` | * Unit: world units per second |
| minimum hard landing velocity | `float` | * Unit: world units per second |
| maximum hard landing velocity | `float` | * Unit: world units per second |
| death hard landing velocity | `float` | * Unit: world units per second |
| standing camera height | `float` | * Unit: world units |
| crouching camera height | `float` | * Unit: world units |
| crouch transition time | `float` | * Unit: seconds Controls how long it takes to transition between standing and crouching positions. |
| standing collision height | `float` | * Unit: world units Sets the height of the physics pill when the biped _uses player physics_ and is standing. |
| crouching collision height | `float` | * Unit: world units Sets the height of the physics pill when the biped _uses player physics_ and is crouching. |
| collision radius | `float` | * Unit: world units Sets the radius of the physics pill/sphere. |
| autoaim width | `float` | * Unit: world units |
| cosine stationary turning threshold | `float` | * Cache only |
| crouch camera velocity | `float` | * Cache only |
| cosine maximum slope angle | `float` | * Cache only |
| negative sine downhill falloff angle | `float` | * Cache only |
| negative sine downhill cutoff angle | `float` | * Cache only |
| sine uphill falloff angle | `float` | * Cache only |
| sine uphill cutoff angle | `float` | * Cache only |
| head model node index | `uint16` | * Cache only Index of the "head" model node used to create the biped's autoaim pill. The pill also uses the "bip01 pelvis" node. |
| pelvis model node index | `uint16` | * Cache only Index of the "bip01 pelvis" model node used to create the biped's autoaim pill. The pill also uses the "bip01 head" node. |
| contact point | `Block` | * HEK max count: 2 |
| | Field | Type | Comments | | --- | --- | --- | | marker name | `TagString` | | |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Aerocatia _(NTSC player physics research)_
*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/object/unit/biped/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
