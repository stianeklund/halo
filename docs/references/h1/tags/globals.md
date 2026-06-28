# Halo 1 Tag: globals

## Summary

The **globals** tag contains settings for player control, difficulty, grenade types, rasterizer data, the HUD, materials types, and more. In other words, things that only need to be defined once and are rarely edited.

## Tag Information

Tag group: `globals`.

HaloScript entries:

- `<void> cheat_all_powerups`

Defined fields and enum values mentioned:

- camera
- camera default unit camera track
- cheat powerups
- cheat powerups powerup
- difficulty
- difficulty easy burst error
- difficulty easy burst separation
- difficulty easy enemy damage
- difficulty easy enemy recharge
- difficulty easy enemy shield
- difficulty easy enemy vitality
- difficulty easy friend damage
- difficulty easy friend recharge
- difficulty easy friend shield
- difficulty easy friend vitality
- difficulty easy grenade chance scale
- difficulty easy grenade timer scale
- difficulty easy guidance vs player
- difficulty easy infection forms
- difficulty easy major upgrade
- difficulty easy major upgrade 1
- difficulty easy major upgrade 2
- difficulty easy melee delay base
- difficulty easy melee delay scale
- difficulty easy new target delay
- difficulty easy overcharge chance
- difficulty easy projectile error
- difficulty easy rate of fire
- difficulty easy special fire delay
- difficulty easy target leading
- difficulty easy target tracking
- difficulty hard burst error
- difficulty hard burst separation
- difficulty hard enemy damage
- difficulty hard enemy recharge
- difficulty hard enemy shield
- difficulty hard enemy vitality
- difficulty hard friend damage
- difficulty hard friend recharge
- difficulty hard friend shield
- difficulty hard friend vitality
- difficulty hard grenade chance scale
- difficulty hard grenade timer scale
- difficulty hard guidance vs player
- difficulty hard infection forms
- difficulty hard major upgrade
- difficulty hard major upgrade 1
- difficulty hard major upgrade 2
- difficulty hard melee delay base
- difficulty hard melee delay scale
- difficulty hard new target delay
- difficulty hard overcharge chance
- difficulty hard projectile error
- difficulty hard rate of fire
- difficulty hard special fire delay
- difficulty hard target leading
- difficulty hard target tracking
- difficulty imposs burst error
- difficulty imposs burst separation
- difficulty imposs enemy damage
- difficulty imposs enemy recharge
- difficulty imposs enemy shield
- difficulty imposs enemy vitality
- difficulty imposs friend damage
- difficulty imposs friend recharge
- difficulty imposs friend shield
- difficulty imposs friend vitality
- difficulty imposs grenade chance scale
- difficulty imposs grenade timer scale
- difficulty imposs guidance vs player
- difficulty imposs infection forms
- difficulty imposs major upgrade
- difficulty imposs major upgrade 1
- difficulty imposs major upgrade 2
- difficulty imposs melee delay base
- difficulty imposs melee delay scale
- difficulty imposs new target delay
- difficulty imposs overcharge chance
- difficulty imposs projectile error
- difficulty imposs rate of fire

## Details

The **globals** tag contains settings for player control, difficulty, grenade types, rasterizer data, the HUD, materials types, and more. In other words, things that only need to be defined once and are rarely edited.

This tag and its dependencies are also included in a map when compiled, and the engine is hard-coded to reference it.

The following are related functions that you can use in your scenario scripts and/or debug globals that you can enter into the developer console for troubleshooting.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(<void> cheat_all_powerups)` Drops all powerups near player. The set of powerups is controlled by the globals tag. | Function |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| sounds | `Block` | * HEK max count: 2 * Min: 2 |
Field values:
- sound (`TagDependency`: sound)
| camera | `Block` | * HEK max count: 1 * Min: 1 |
Field values:
- default unit camera track (`TagDependency`: camera_track)
| player control | `Block` | * HEK max count: 1 * Min: 1 |
Field values:
- magnetism friction (`float`)
- magnetism adhesion (`float`)
- inconsequential target scale (`float`)
- look acceleration time (`float`): * Unit: seconds
- look acceleration scale (`float`)
- look peg threshold (`float`)
- look default pitch rate (`float`)
- look default yaw rate (`float`)
- look autolevelling scale (`float`)
- minimum weapon swap ticks (`uint16`)
- minimum autolevelling ticks (`uint16`)
- minimum angle for vehicle flipping (`float`)
- look function (`Block`)
- scale (`float`)
| difficulty | `Block` | * HEK max count: 1 * Min: 1 |
Field values:
- easy enemy damage (`float`)
- normal enemy damage (`float`)
- hard enemy damage (`float`)
- imposs enemy damage (`float`)
- easy enemy vitality (`float`)
- normal enemy vitality (`float`)
- hard enemy vitality (`float`)
- imposs enemy vitality (`float`)
- easy enemy shield (`float`)
- normal enemy shield (`float`)
- hard enemy shield (`float`)
- imposs enemy shield (`float`)
- easy enemy recharge (`float`)
- normal enemy recharge (`float`)
- hard enemy recharge (`float`)
- imposs enemy recharge (`float`)
- easy friend damage (`float`)
- normal friend damage (`float`)
- hard friend damage (`float`)
- imposs friend damage (`float`)
- easy friend vitality (`float`)
- normal friend vitality (`float`)
- hard friend vitality (`float`)
- imposs friend vitality (`float`)
- easy friend shield (`float`)
- normal friend shield (`float`)
- hard friend shield (`float`)
- imposs friend shield (`float`)
- easy friend recharge (`float`)
- normal friend recharge (`float`)
- hard friend recharge (`float`)
- imposs friend recharge (`float`)
- easy infection forms (`float`)
- normal infection forms (`float`)
- hard infection forms (`float`)
- imposs infection forms (`float`)
- easy rate of fire (`float`)
- normal rate of fire (`float`)
- hard rate of fire (`float`)
- imposs rate of fire (`float`)
- easy projectile error (`float`)
- normal projectile error (`float`)
- hard projectile error (`float`)
- imposs projectile error (`float`)
- easy burst error (`float`)
- normal burst error (`float`)
- hard burst error (`float`)
- imposs burst error (`float`)
- easy new target delay (`float`)
- normal new target delay (`float`)
- hard new target delay (`float`)
- imposs new target delay (`float`)
- easy burst separation (`float`)
- normal burst separation (`float`)
- hard burst separation (`float`)
- imposs burst separation (`float`)
- easy target tracking (`float`)
- normal target tracking (`float`)
- hard target tracking (`float`)
- imposs target tracking (`float`)
- easy target leading (`float`)
- normal target leading (`float`)
- hard target leading (`float`)
- imposs target leading (`float`)
- easy overcharge chance (`float`)
- normal overcharge chance (`float`)
- hard overcharge chance (`float`)
- imposs overcharge chance (`float`)
- easy special fire delay (`float`)
- normal special fire delay (`float`)
- hard special fire delay (`float`)
- imposs special fire delay (`float`)
- easy guidance vs player (`float`)
- normal guidance vs player (`float`)
- hard guidance vs player (`float`)
- imposs guidance vs player (`float`)
- easy melee delay base (`float`)
- normal melee delay base (`float`)
- hard melee delay base (`float`)
- imposs melee delay base (`float`)
- easy melee delay scale (`float`)
- normal melee delay scale (`float`)
- hard melee delay scale (`float`)
- imposs melee delay scale (`float`)
- easy grenade chance scale (`float`)
- normal grenade chance scale (`float`)
- hard grenade chance scale (`float`)
- imposs grenade chance scale (`float`)
- easy grenade timer scale (`float`)
- normal grenade timer scale (`float`)
- hard grenade timer scale (`float`)
- imposs grenade timer scale (`float`)
- easy major upgrade (`float`)
- normal major upgrade (`float`)
- hard major upgrade (`float`)
- imposs major upgrade (`float`)
- easy major upgrade 1 (`float`)
- normal major upgrade 1 (`float`)
- hard major upgrade 1 (`float`)
- imposs major upgrade 1 (`float`)
- easy major upgrade 2 (`float`)
- normal major upgrade 2 (`float`)
- hard major upgrade 2 (`float`)
- imposs major upgrade 2 (`float`)
| grenades | `Block` | * HEK max count: 2 * Min: 2 * Max: 2 |
Field values:
- maximum count (`int16`): * Min: 0 * Max: 127
- mp spawn default (`int16`): * Min: 0 * Max: 127
- throwing effect (`TagDependency`: effect)
- hud interface (`TagDependency`: grenade_hud_interface)
- equipment (`TagDependency`: equipment)
- projectile (`TagDependency`: projectile)
| rasterizer data | `Block` | * HEK max count: 1 * Min: 1 Contains global renderer settings and 2D lookup tables. |
Flag values:
- distance attenuation (`TagDependency`: bitmap): * Non-null
- vector normalization (`TagDependency`: bitmap): * Non-null This bitmap is used as a lookup table of precalculated vector normalizations for visual effects like reflections. In H1CE it is also incorrectly used as the reflection cube map when a shader_ transparent_ glass uses bump-mapped reflections.
- atmospheric fog density (`TagDependency`: bitmap): * Non-null
- planar fog density (`TagDependency`: bitmap): * Non-null
- linear corner fade (`TagDependency`: bitmap): * Non-null
- active camouflage distortion (`TagDependency`: bitmap): * Non-null This bitmap is used as a lookup table for active camouflage distortion angle calculations. Its alpha channel masks _tint color_ on Gearbox-derived ports of the game.
- glow (`TagDependency`: bitmap): * Non-null Used by `sun_glow_*` FX shaders when rendering _Ray of Buddha_.
- default 2d (`TagDependency`: bitmap): * Non-null
- default 3d (`TagDependency`: bitmap): * Non-null
- default cube map (`TagDependency`: bitmap): * Non-null
- test 0 (`TagDependency`: bitmap): * Non-null
- test 1 (`TagDependency`: bitmap): * Non-null
- test 2 (`TagDependency`: bitmap)
- test 3 (`TagDependency`: bitmap)
- video scanline map (`TagDependency`: bitmap): * Non-null
- video noise map (`TagDependency`: bitmap): * Non-null
- flags (`bitfield`): Optional settings for active camouflage.
- tint edge density (`0x1`): Adds the _tint color_ to the background.
- refraction amount (`float`): How much to distort the background of active camouflage. This is measured in **world units** since the effect is not resolution-dependent. Lower values distort more, while higher values distort less.
- distance falloff (`float`): The distance in world units where the active camouflage effect fades completely and the object is entirely invisible.
- tint color (`ColorRGB`): The color which is multiplied over the background for active camouflage objects.
- hyper stealth refraction (`float`): Refaction amount used when _super active camouflage_ is enabled for an actor_ variant.
- hyper stealth distance falloff (`float`): Falloff distance for _super active camouflage_.
- hyper stealth tint color (`ColorRGB`): Tint color used for _super active camouflage_.
- distance attenuation 2d (`TagDependency`: bitmap)
| interface bitmaps | `Block` | * HEK max count: 1 * Min: 1 |
Field values:
- font system (`TagDependency`: font)
- font terminal (`TagDependency`: font)
- screen color table (`TagDependency`: color_table)
- hud color table (`TagDependency`: color_table)
- editor color table (`TagDependency`: color_table)
- dialog color table (`TagDependency`: color_table)
- hud globals (`TagDependency`: hud_globals): * Non-null
- motion sensor sweep bitmap (`TagDependency`: bitmap): * Non-null
- motion sensor sweep bitmap mask (`TagDependency`: bitmap): * Non-null
- multiplayer hud bitmap (`TagDependency`: bitmap): * Non-null
- localization (`TagDependency`: string_list)
- hud digits definition (`TagDependency`: hud_number)
- motion sensor blip bitmap (`TagDependency`: bitmap): * Non-null
- interface goo map1 (`TagDependency`: bitmap): * Non-null
- interface goo map2 (`TagDependency`: bitmap): * Non-null
- interface goo map3 (`TagDependency`: bitmap): * Non-null
| weapon list | `Block` | * HEK max count: 20 |
Field values:
- weapon (`TagDependency`: item)
| cheat powerups | `Block` | Defines the set of powerups dropped when the function cheat_ all_ powerups is used. |
Field values:
- powerup (`TagDependency`: equipment)
| multiplayer information | `Block` | This block is stripped by Tool when compiling singleplayer or UI maps. |
Field values:
- flag (`TagDependency`: weapon): * Non-null
- unit (`TagDependency`: unit)
- vehicles (`Block`)
- vehicle (`TagDependency`: unit)
- hill shader (`TagDependency`: shader): * Non-null
- flag shader (`TagDependency`: shader)
- ball (`TagDependency`: weapon): * Non-null
- sounds (`Block`?)
| player information | `Block` | * Min: 1 |
Field values:
- unit (`TagDependency`: unit)
- walking speed (`float`): * Unit: world units per second
- double speed multiplier (`float`)
- run forward (`float`): * Unit: world units per second
- run backward (`float`): * Unit: world units per second
- run sideways (`float`): * Unit: world units per second
- run acceleration (`float`): * Unit: world units per second squared
- sneak forward (`float`): * Unit: world units per second
- sneak backward (`float`): * Unit: world units per second
- sneak sideways (`float`): * Unit: world units per second
- sneak acceleration (`float`): * Unit: world units per second squared
- airborne acceleration (`float`): * Unit: world units per second squared
- speed multiplier (`float`): * Unit: multiplayer only
- grenade origin (`Point3D`)
- stun movement penalty (`float`)
- stun turning penalty (`float`)
- stun jumping penalty (`float`)
- minimum stun time (`float`): * Unit: seconds
- maximum stun time (`float`): * Unit: seconds
- first person idle time (`Bounds`): * Unit: seconds
- min (`float`)
- max (`float`)
- first person skip fraction (`float`): * Min: 0 * Max: 1
- coop respawn effect (`TagDependency`: effect)
| first person interface | `Block` | * Min: 1 |
Field values:
- first person hands (`TagDependency` * gbxmodel * model): This is the model used for hands when a weapon with first person animatinos is held.
- base bitmap (`TagDependency`: bitmap)
- shield meter (`TagDependency`: meter)
- shield meter origin (`Point2DInt`)
- x (`int16`)
- y (`int16`)
- body meter (`TagDependency`: meter)
- body meter origin (`Point2DInt`?)
- night vision on effect (`TagDependency`: effect): This effect is played when enabling night vision.
- night vision off effect (`TagDependency`: effect): This effect is played when disabling night vision.
| falling damage | `Block` | This block is stripped by Tool when compiling UI maps. |
Field values:
- harmful falling distance (`Bounds`?): * Unit: world units
- falling damage (`TagDependency`: damage_effect)
- maximum falling distance (`float`): * Unit: world units
- distance damage (`TagDependency`: damage_effect)
- vehicle environment collision damage (`TagDependency`: damage_effect)
- vehicle killed unit damage (`TagDependency`: damage_effect)
- vehicle collision damage (`TagDependency`: damage_effect)
- flaming death damage (`TagDependency`: damage_effect)
- maximum falling velocity (`float`): * Cache only
- harmful falling velocity (`Bounds`?): * Cache only
| materials | `Block` |  |
Flag values:
- ground friction scale (`float`)
- ground friction normal k1 scale (`float`)
- ground friction normal k0 scale (`float`)
- ground depth scale (`float`)
- ground damp fraction scale (`float`)
- maximum vitality (`float`)
- effect (`TagDependency`: effect)
- sound (`TagDependency`: sound)
- particle effects (`Block`)
- particle type (`TagDependency`: particle)
- flags (`bitfield`)
- interpolate color in hsv (`0x1`)
- more colors (`0x2`)
- density (`float`): * Unit: world units
- velocity scale (`Bounds`?)
- angular velocity (`Bounds`?): * Unit: degrees per second
- radius (`Bounds`?): * Unit: world units
- tint lower bound (`ColorARGB`)
- tint upper bound (`ColorARGB`)
- melee hit sound (`TagDependency`: sound)
| playlist members | `Block` |  |
Field values:
- map name (`TagString`)
- game variant (`TagString`)
- minimum experience (`uint32`)
- maximum experience (`uint32`): * Default: 10
- minimum player count (`uint32`): * Default: 1
- maximum player count (`uint32`): * Default: 16
- rating (`uint32`): * Default: 100

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `globals`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
