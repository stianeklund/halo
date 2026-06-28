# Halo 1 Tag: actor_variant

## Summary

This tag page is primarily field documentation. See the tag information below for defined fields, enum values, units, and runtime notes.

## Tag Information

Tag group: `actor_variant`.

Defined fields and enum values mentioned:

- actor definition
- berserk burst duration
- berserk burst separation
- berserk firing ranges
- berserk melee abort range
- berserk melee range
- berserk projectile error
- berserk rate of fire
- body vitality
- bombardment range
- burst angular velocity
- burst duration
- burst origin angle
- burst origin radius
- burst return angle
- burst return length
- burst separation
- change colors
- change colors color lower bound
- change colors color upper bound
- collateral damage radius
- crouch time
- crouch time max
- crouch time min
- custom crouch gun offset
- custom stand gun offset
- damage per second
- death fire wildly chance
- death fire wildly time
- desired combat range
- dont drop grenades chance
- drop weapon ammo
- drop weapon loaded
- encounter grenade timeout
- enemy radius
- equipment
- first burst delay time
- flags
- flags active camouflage
- flags can shoot while flying
- flags cannot use ranged weapons
- flags has unlimited grenades
- flags interpolate color in hsv
- flags moveswitch stay w friends
- flags prefer passenger seat
- flags super active camouflage
- forced shader permutation
- grenade chance
- grenade check time
- grenade count
- grenade count max
- grenade count min
- grenade ranges
- grenade stimulus
- grenade stimulus never
- grenade stimulus seek cover
- grenade stimulus visible target
- grenade type
- grenade type covenant plasma
- grenade type human fragmentation
- grenade velocity
- initial crouch chance
- initial velocity
- major variant
- max vision distance
- maximum body vitality
- maximum firing distance
- melee abort range
- melee range
- metagame class
- metagame class giant vehicle
- metagame class heavy vehicle
- metagame class hero
- metagame class infantry
- metagame class leader
- metagame class light vehicle
- metagame class specialist
- metagame class standard vehicle
- metagame type
- metagame type banshee

## Details

`flags` `bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| can shoot while flying | `0x1` | Also allows grounded AIs to shoot while jumping or falling. |
| interpolate color in hsv | `0x2` | Determines if change colors upper and lower bounds are interpolated in HSV space as opposed to RGB. |
| has unlimited grenades | `0x4` | Gives the actor an unlimited supply of grenades to throw. |
| moveswitch stay w friends | `0x8` |  |
| active camouflage | `0x10` | Gives the actor permanent active camouflage, e.g. stealth Elites. |
| super active camouflage | `0x20` | When enabled, makes the active camouflage the _hyper stealth_ variant from globals, which can have a different appearance. |
| cannot use ranged weapons | `0x40` | Disables ranged combat settings (beginning at the _weapon_ reference). Even if the AI is holding a weapon, it will not fire it. |
| prefer passenger seat | `0x80` | The actor will prefer to enter a vehicle's passenger seat over a gunner seat, as determined by the unit's _seat_ flags"). |
`actor definition` `TagDependency`: actor
`unit` `TagDependency`: unit Specifies which biped this variant will spawn as.
`major variant` `TagDependency`: actor_variant Specifies the next highest "rank" of this actor variant. This allows enemies to "upgrade" in higher difficulties, with the chance being controlled by both the per-difficulty _major upgrade_globals multipliers and the _major upgrade_ squad setting in the scenario.
`metagame type` `enum` Used for kill scoring and achievements in MCC. Since this field was padding in pre-MCC editions, maps compiled for MCC should use MCC-native _actor\_ variant_ tags or set these fields, or else enemies may be scored as Brutes or other unexpected types due to having zeroed-out or garbage data.
| Option | Value | Comments |
| --- | --- | --- |
| brute | `0x0` |  |
| grunt | `0x1` |  |
| jackal | `0x2` |  |
| skirmisher | `0x3` |  |
| marine | `0x4` |  |
| spartan | `0x5` |  |
| bugger | `0x6` | Also called "drone". |
| hunter | `0x7` |  |
| flood infection | `0x8` |  |
| flood carrier | `0x9` |  |
| flood combat | `0xA` |  |
| flood pure | `0xB` |  |
| sentinel | `0xC` |  |
| elite | `0xD` |  |
| engineer | `0xE` | Also called "huragok". |
| mule | `0xF` |  |
| turret | `0x10` |  |
| mongoose | `0x11` |  |
| warthog | `0x12` |  |
| scorpion | `0x13` |  |
| hornet | `0x14` |  |
| pelican | `0x15` |  |
| revenant | `0x16` |  |
| seraph | `0x17` |  |
| shade | `0x18` |  |
| watchtower | `0x19` |  |
| ghost | `0x1A` |  |
| chopper | `0x1B` |  |
| mauler | `0x1C` | Also called "prowler". |
| wraith | `0x1D` |  |
| banshee | `0x1E` |  |
| phantom | `0x1F` |  |
| scarab | `0x20` |  |
| guntower | `0x21` |  |
| tuning fork | `0x22` | Also called "spirit". |
| broadsword | `0x23` |  |
| mammoth | `0x24` |  |
| lich | `0x25` |  |
| mantis | `0x26` |  |
| wasp | `0x27` |  |
| phaeton | `0x28` |  |
| bishop | `0x29` | Also called "watcher". |
| knight | `0x2A` |  |
| pawn | `0x2B` | Also called "crawler". |
`metagame class` `enum` Used for kill scoring and achievements in MCC.
| Option | Value | Comments |
| --- | --- | --- |
| infantry | `0x0` |  |
| leader | `0x1` |  |
| hero | `0x2` |  |
| specialist | `0x3` |  |
| light vehicle | `0x4` |  |
| heavy vehicle | `0x5` |  |
| giant vehicle | `0x6` |  |
| standard vehicle | `0x7` |  |
`movement type` `enum`
| Option | Value | Comments |
| --- | --- | --- |
| always run | `0x0` |  |
| always crouch | `0x1` |  |
| switch types | `0x2` |  |
`initial crouch chance` `float`
`crouch time` `Bounds`
| Field | Type | Comments |
| --- | --- | --- |
| min | `float` |  |
| max | `float` |  |
`run time` `Bounds` ?
`weapon` `TagDependency`: weapon The _weapon_ that the actor spawns with.
`maximum firing distance` `float` The maximum range, in world units, where the unit can fire at a target.
`rate of fire` `float` This lets you pick the firing rate for this actor. Which affects firing behavior in the following ways:

* Any above `0` rate of fire is limited by the referenced _weapon's_ minimum _rounds per second_ as the actor will only tap the trigger not hold, not causing the firing speed to climb.

* When set to `0`, the actor will hold the trigger causing it to use the weapon's windup behavior and allows it to reach the weapon's max firing rate.

* If `0` and the weapon is a charging weapon the actor will hold the charge until the burst ends or the weapon's overcharged action behavior kicks in.

* If not `0` charging the weapon will fire their uncharged projectiles if they have them.

* Setting a number higher than the weapon's minimum rate of fire can prompt the actor to fire their gun slower than the weapon's minimum because it will try to tap fire at every `1/rate_of_fire` causing it to not be able to fire again immediately after the firing delay.
`projectile error` `float` Error angle in degrees for projectiles fired by the actor. A value of 0 will cause the actor to use the weapon's error values instead (i.e. the projectile spread behavior will be the same as it is when the player is using the weapon.
`first burst delay time` `Bounds` ? The delay in seconds before the actor starts its first burst on a new target.
`new target firing pattern time` `float` How long this AI follows "New target" firing modifiers when first engaging a target.
`surprise delay time` `float`
`surprise fire wildly time` `float`
`death fire wildly chance` `float` Sets the chance that the actor will fire their weapon while dying.
`death fire wildly time` `float` Controls how long the actor will continue firing after death.
`desired combat range` `Bounds` ? The distance in world units the actor will try to be in to attack; lower bounds is the minimum range and upper is the maximum. The actor may still fire outside this range, but will move to attain it.
`custom stand gun offset` `Vector3D`
`custom crouch gun offset` `Vector3D`
`target tracking` `float` Determines how closely the actor will follow moving targets when firing. bursts. A value of `0` means only firing at the target's location at the start of the burst, while while `1` means following the target perfectly. with each shot. A value in-between can be provided.
`target leading` `float` Determines how accurately to predict lead on moving targets. A value of `0` means not leading at all and shooting directly at the target, while `1` means perfectly predicting how far in front of the target to lead based on projectile speed and distance. A value in-between can be provided.
`weapon damage modifier` `float` Allows weapon damage to be buffed or nerfed for this actor variant. A value of 0 disables this modifier.
`damage per second` `float` Overrides weapon damage if set and _weapon damage modifier_ is 0.
`burst origin radius` `float` The starting point of the burst, randomly to the left or right of the target in world units.
`burst origin angle` `float` Controls the maximum angle from horizontal that the orgin can be, from the point of view of the actor. For example, a value of `0` indicates the origin will alway be directly to the left or right of the target, while `90` would allow the origin to be above or below the target as well.
`burst return length` `Bounds` ? How far the burst point moves back towards the target. This can be negative to make the burst lead _away_ from the target.
`burst return angle` `float` Controls how close to horizontal the return motion can be.
`burst duration` `Bounds` ? Cotrols the length of burst, during which multiple shots are fired.
`burst separation` `Bounds` ? Controls how long to wait between bursts.
`burst angular velocity` `float` Sets the maximum rotational rate that the burst can sweep. A value of `0` means unlimited. For example, if the _burst origin radius_ is large and the _burst duration_ is short, the return burst will cover a large area in a short amount of time unless limited by this angular velocity.
`special damage modifier` `float` A damage modifier for special weapon fire (e.g. overcharged shots and secondary triggers), applied in addition to the normal damage modifier. No effect if `0`.
`special projectile error` `float` An error angle, applied in addition to normal error, for special fire.
`new target burst duration` `float` Multiplier for _burst duration_ in the new target state. No effect if `0`.
`new target burst separation` `float` Multiplier for _burst separation_ in the new target state. No effect if `0`.
`new target rate of fire` `float` Multiplier for _rate of fire_ in the new target state. No effect if `0`.
`new target projectile error` `float` Multiplier for _projectile error_ in the new target state. No effect if `0`.
`moving burst duration` `float` Multiplier for _burst duration_ in the moving state. No effect if `0`.
`moving burst separation` `float` Multiplier for _burst separation_ in the moving state. No effect if `0`.
`moving rate of fire` `float` Multiplier for _rate of fire_ in the moving state. No effect if `0`.
`moving projectile error` `float` Multiplier for _projectile error_ in the moving state. No effect if `0`.
`berserk burst duration` `float` Multiplier for _burst duration_ in the berserk state. No effect if `0`.
`berserk burst separation` `float` Multiplier for _burst separation_ in the berserk state. No effect if `0`.
`berserk rate of fire` `float` Multiplier for _rate of fire_ in the berserk state. No effect if `0`.
`berserk projectile error` `float` Multiplier for _projectile error_ in the berserk state. No effect if `0`.
`super ballistic range` `float`
`bombardment range` `float` When non-zero, causes the burst target to be offset by a random distance in this range when the enemy is _not visible_.
`modified vision range` `float` Overrides the actor's vision range. Normal if `0`.
`special fire mode` `enum` If set, allows the actor to use their weapon in alternate ways.
| Option | Value | Comments |
| --- | --- | --- |
| none | `0x0` | No special fire will be used. |
| overcharge | `0x1` | The actor will hold down the primary trigger for an overcharged shot. Jackals use this with the plasma pistol. |
| secondary trigger | `0x2` | The actor will fire the weapon's secondary trigger. |
`special fire situation` `enum` Determines the situation in which the actor has a chance of using special fire.
| Option | Value | Comments |
| --- | --- | --- |
| never | `0x0` | The actor will never special fire their weapon. |
| enemy visible | `0x1` | Special fire happens only when the target is visible. |
| enemy out of sight | `0x2` | Special fire happens only when the target is behind cover. |
| strafing | `0x3` |  |
`special fire chance` `float` How likely the actor will use their weapon's special fire mode. Setting this to `0` is equivalent to using the _never_ situation.
`special fire delay` `float` How long the actor must wait between uses of special fire mode.
`melee range` `float` Sets how close an enemy must get to trigger a melee charge by the actor.
`melee abort range` `float` The actor will stop trying to melee the enemy goes outside this range.
`berserk firing ranges` `Bounds` ? When berserking and outside the maximum range, the actor will advance towards the target and stop at the minimum range.
`berserk melee range` `float` Similar to _melee range_, but used when the actor is berserking.
`berserk melee abort range` `float` Similar to _melee abort range_, but used when the actor is berserking.
`grenade type` `enum` Sets which type of grenade the actor throws, assuming _grenade stimulus_ is not _never_.
| Option | Value | Comments |
| --- | --- | --- |
| human fragmentation | `0x0` |  |
| covenant plasma | `0x1` |  |
`trajectory type` `enum`
| Option | Value | Comments |
| --- | --- | --- |
| toss | `0x0` |  |
| lob | `0x1` |  |
| bounce | `0x2` |  |
`grenade stimulus` `enum` What causes the actor to consider throwing a grenade. It is still dependent upon _grenade chance_.
| Option | Value | Comments |
| --- | --- | --- |
| never | `0x0` | The actor never throws grenades. |
| visible target | `0x1` |  |
| seek cover | `0x2` |  |
`minimum enemy count` `uint16` How many enemies must be within the grenade's radius before the actor considers throwing there.
`enemy radius` `float` Only enemies within this radius of the actor are considered when choosing where to throw a grenade.
`grenade velocity` `float` This is responsibile for giving the grenade it's thrown velocity, rather than the projectile _initial velocity_ or the unit _grenade velocity_").
`grenade ranges` `Bounds` ? The minimum and maximum ranges that the actor will consider throwing a grenade.
`collateral damage radius` `float` If there are friendlies within this radius of the target, grenades will not be thrown.
`grenade chance` `float` How likely the actor is to throw a grenade when a _grenade stimulus_ applies.
`grenade check time` `float` How often to consider throwing a grenade while a continuous _grenade stimulus_, like _visible target_, applies.
`encounter grenade timeout` `float` The AI will not throw a grenade if another AI in the encounter threw one within this timeout. This prevents AI from all throwing grenades at the same time.
`equipment` `TagDependency`: equipment References equipment that the actor will drop on death. Note that their weapon will already be dropped and does not need to be repeated here.
`grenade count` `Bounds` Determines how many grenades the actor spawns with, with the type determined by the _grenade type_ field. The actor will use up these grenades unless _has unlimited grenades_ is true. On death, the grenades are dropped.
| Field | Type | Comments |
| --- | --- | --- |
| min | `uint16` |  |
| max | `uint16` |  |
`don't drop grenades chance` `float` The chance that the actor drops no grenades on death, even if they have some.
`drop weapon loaded` `Bounds` ? A random range for how loaded the dropped weapon is, as a fraction of its magazine size or charge. Plasma weapons are dropped at 100% charge no matter this field's value under special circumstances: * If a model region was destroyed and that region's _forces drop weapon_ flag is enabled, such as shooting the arm off a Flood actor. * If the actor has a chance of feigning death") and reviving, but not if the damage exceeds the _hard death threshold_. ---
`drop weapon ammo` `Bounds` ? The total number of reserve ammo rounds included with the dropped weapon. Ignored for energy weapons.
`body vitality` `float` Overrides the biped's collision geometry _maximum body vitality_ for a different amount of health.
`shield vitality` `float` Overrides the biped's collision geometry _maximum shield vitality_ for a different amount of shields.
`shield sapping radius` `float`
`forced shader permutation` `uint16` If non-zero, forces the bitmap index for any shader the biped uses. For example, the Elite major uses the value `1` which forces its shaders to use the second (red) cubemap in `characters\elite\bitmaps\cubemaps.bitmap`. Bitmap tags which do not have a bitmap for this index will instead use the first map (index `0`).
`change colors` `Block` Overrides the bipeds color change permutations.
| Field | Type | Comments |
| --- | --- | --- |
| color lower bound | `ColorRGB` |  |
| color upper bound | `ColorRGB` |  |

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `actor_variant`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
