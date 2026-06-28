# Halo 1 Tag: damage_effect

## Summary

The **damage effect** tag determines the results of damage application in a wide range of use cases, including but not limited to: projectile impacts, detonations, melee attacks, falling, and vehicle collisions. They can be part of an effect.

## Tag Information

Tag group: `damage_effect`.

HaloScript entries:

- `ai_print_damage_modifiers [boolean]`
- `cheat_reflexive_damage_effects [boolean]`
- `debug_damage [boolean]`
- `debug_damage_taken [boolean]`
- `rasterizer_screen_flashes`

Defined fields and enum values mentioned:

- breaking effect forward exponent
- breaking effect forward radius
- breaking effect forward velocity
- breaking effect outward exponent
- breaking effect outward radius
- breaking effect outward velocity
- camera shaking duration
- camera shaking falloff function
- camera shaking random rotation
- camera shaking random translation
- camera shaking wobble function
- camera shaking wobble function cosine
- camera shaking wobble function cosine variable period
- camera shaking wobble function diagonal wave
- camera shaking wobble function diagonal wave variable period
- camera shaking wobble function jitter
- camera shaking wobble function noise
- camera shaking wobble function one
- camera shaking wobble function slide
- camera shaking wobble function slide variable period
- camera shaking wobble function spark
- camera shaking wobble function wander
- camera shaking wobble function zero
- camera shaking wobble period
- camera shaking wobble weight
- color
- cutoff scale
- cyborg armor
- cyborg energy shield
- damage active camouflage damage
- damage aoe core radius
- damage category
- damage category bullet
- damage category falling
- damage category flame
- damage category grenade
- damage category high explosive
- damage category melee
- damage category mounted weapon
- damage category needle
- damage category none
- damage category plasma
- damage category shotgun
- damage category sniper
- damage category vehicle
- damage flags
- damage flags can cause headshots
- damage flags can cause multiplayer headshots
- damage flags causes flaming death
- damage flags damage indicators always point down
- damage flags detonates explosives
- damage flags does not hurt friends
- damage flags does not hurt owner
- damage flags does not ping units
- damage flags infection form pop
- damage flags only hurts one infection form
- damage flags only hurts shields
- damage flags pings resistant units
- damage flags skips shields
- damage instantaneous acceleration
- damage lower bound
- damage maximum stun
- damage side effect
- damage side effect emp
- damage side effect harmless
- damage side effect lethal to the unsuspecting
- damage side effect none
- damage stun
- damage stun time
- damage upper bound
- damage vehicle passthrough penalty
- dirt
- duration
- elite
- elite energy shield
- engineer force field
- engineer skin
- fade function
- fade function cosine
- fade function early

## Details

The **damage effect** tag determines the results of damage application in a wide range of use cases, including but not limited to: projectile impacts, detonations, melee attacks, falling, and vehicle collisions. They can be part of an effect.

A damage effect is not strictly about applying shield and health damage to units"); these tags can have an area of effect, impart acceleration on additional objects like items") and projectiles too, and cause screen effects like colour flashes and shaking.

### Bleedthrough

When damage to shields exceeds the currently shield vitality, damage carries over to health. When this occurs, the damage multiplier of the shields still carries over to the health damage. This even occurs if the current shield vitality is 0. The result is that the initial carryover health damage may be higher than expected with weapons like the plasma rifle, which have a 2x shield multiplier. Subsequent shots will impact the body and use its material type modifiers instead.

### Impact damage formula

When applying a `damage_effect` as a projectile impact, the game may take projectile fields into account as well. When air or water damage ranges are set, the projectile's travel distance will scale damage between damage lower bound and a random selection between upper bounds. If air damage range is not set, it always uses the random high bound.

Firstly, the game randomly picks a value between the two upper bounds (will always be the same if both values are the same). Up to the projectile minimum air damage range, 100% unmodified damage is applied. However, past the minimum and up to the maximum damage ranges, damage linearly scales to the lower bound. Past this distance, the projectile disappears (but does not detonate).

In pseudocode, and assuming within max range:

```
//falloff is 0 to 1 based on travel distance between air damage ranges
let falloff = max(0,
 (travel_distance - air_damage_min) /
 (air_damage_max - air_damage_min)
);

//given a random_value between 0 and 1, interpolate between the two high bounds
let upper_bound_delta = upper_bound_2 - upper_bound_1;
let random_upper_bound = upper_bound_1 + random_value * upper_bound_delta;

//interpolate between lower and random upper bound
let raw_damage = (
 falloff * lower_bound +
 (1.0 - falloff) * random_upper_bound
);
//finally, damage is scaled by the multiplier for the impacted material
let damage_done = material_damage_multiplier * raw_damage;
```

Or, more visually:

The following are related functions that you can use in your scenario scripts and/or debug globals that you can enter into the developer console for troubleshooting.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(ai_print_damage_modifiers [boolean])` When an encounter is selected, damage modifiers are logged at the bottom of the screen. | Global |
|  | `(cheat_reflexive_damage_effects [boolean])` Any damage_ effect applied to other characters, even dead ones, will appear on the player's screen regardless of who applied the damage. For example, if an Elite shoots a flood infection form, the player will see their screen flash blue and damage direction indicators appear. The player takes no _actual_ damage. This can be helpful for testing visual effects. | Global |
|  | `(debug_damage [boolean])` When enabled, looking at any collideable object") and pressing Space will display the object's body and shield vitalities on the HUD. | Global |
|  | `(debug_damage_taken [boolean])` Logs damage to the player as messages at the bottom of the screen. Includes body and shield vitality and the damage source. | Global |
|  | `(rasterizer_screen_flashes)` Toggles the display of screen flashes, such as those from a damage_ effect or powerup equipment. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| radius | `Bounds` | * Unit: world units The minimum radius determines the sphere in which damage is 100% of _damage upper bound_. Damage fades linearly to _damage lower bound_ at the maximum radius. Outside the maximum radius, 0 damage is dealt. The _aoe core radius_ field does not affect this calculation. |
Field values:
- min (`float`)
- max (`float`)
| cutoff scale | `float` | * Min: 0 * Max: 1 |
| flags | `bitfield` |  |
Flag values:
- do not scale damage by distance (`0x1`)
| type | `enum` |  |
Option values:
- none (`0x0`)
- lighten (`0x1`)
- darken (`0x2`)
- max (`0x3`)
- min (`0x4`)
- invert (`0x5`)
- tint (`0x6`)
| priority | `enum` |  |
Option values:
- low (`0x0`)
- medium (`0x1`)
- high (`0x2`)
| duration | `float` | * Unit: seconds |
| fade function | `enum` |  |
Option values:
- linear (`0x0`)
- early (`0x1`)
- very early (`0x2`)
- late (`0x3`)
- very late (`0x4`)
- cosine (`0x5`)
| maximum intensity | `float` | * Min: 0 * Max: 1 * Default: 1 |
| color | `ColorARGB` |  |
| low frequency vibrate frequency | `float` | * Min: 0 * Max: 1 |
| low frequency vibrate duration | `float` | * Unit: seconds |
| low frequency vibrate fade function | `enum`? |  |
| high frequency vibrate frequency | `float` | * Min: 0 * Max: 1 |
| high frequency vibrate duration | `float` | * Unit: seconds |
| high frequency vibrate fade function | `enum`? |  |
| temporary camera impulse duration | `float` | * Unit: seconds |
| temporary camera impulse fade function | `enum`? |  |
| temporary camera impulse rotation | `float` |  |
| temporary camera impulse pushback | `float` | * Unit: world units |
| jitter | `Bounds`? | * Unit: world units |
| permanent camera impulse angle | `float` |  |
| camera shaking duration | `float` | * Unit: seconds Controls how long camera shaking lasts. In H1CE and H1CE, short camera shakes (0.05) get skipped occasionally at high FPS since this feature is still tied to frame rate. This is fixed and interpolated by Chimera and by H1A natively. |
| camera shaking falloff function | `enum`? |  |
| camera shaking random translation | `float` | * Unit: world units |
| camera shaking random rotation | `float` |  |
| camera shaking wobble function | `enum` |  |
Option values:
- one (`0x0`)
- zero (`0x1`)
- cosine (`0x2`)
- cosine variable period (`0x3`)
- diagonal wave (`0x4`)
- diagonal wave variable period (`0x5`)
- slide (`0x6`)
- slide variable period (`0x7`)
- noise (`0x8`)
- jitter (`0x9`)
- wander (`0xA`)
- spark (`0xB`)
| camera shaking wobble period | `float` | * Unit: seconds * Default: 1 |
| camera shaking wobble weight | `float` |  |
| sound | `TagDependency`: sound |  |
| breaking effect forward velocity | `float` | * Unit: world units per second |
| breaking effect forward radius | `float` | * Unit: world units |
| breaking effect forward exponent | `float` |  |
| breaking effect outward velocity | `float` | * Unit: world units per second |
| breaking effect outward radius | `float` | * Unit: world units |
| breaking effect outward exponent | `float` |  |
| damage side effect | `enum` |  |
Option values:
- none (`0x0`)
- harmless (`0x1`)
- lethal to the unsuspecting (`0x2`)
- emp (`0x3`): Completely depletes target units'") energy shields, no matter their amount.
| damage category | `enum` | Sets a damage category which mainly affects AI communication/dialog, but can have secondary effects depending on the category (falling, flame, and vehicle). |
Option values:
- none (`0x0`)
- falling (`0x1`)
- bullet (`0x2`)
- grenade (`0x3`)
- high explosive (`0x4`)
- sniper (`0x5`)
- melee (`0x6`)
- flame (`0x7`): Prevents objects from creating AOE effects (blood splats).
- mounted weapon (`0x8`)
- vehicle (`0x9`)
- plasma (`0xA`)
- needle (`0xB`)
- shotgun (`0xC`)
| damage flags | `bitfield` |  |
Flag values:
- does not hurt owner (`0x1`)
- can cause headshots (`0x2`): If enabled, any headshot against an unshielded enemy will kill it regardless of the amount of damage inflicted. Even a 1-damage pistol still instantly kills a Hunter because of this flag. It is an instant kill even if the shield threshold is met with a headshot (after shields have been reduced to 0 by the damage effect but health is full). For example, even though the pistol deals 25 damage and players have 75 shield vitality, if the final shot is a headshot the player dies. Although there is also a _can cause multiplayer headshots_ flag, it has a separate meaning and this flag applies to both SP and MP. A model's headshot area is determined by its `head`region.
- pings resistant units (`0x4`)
- does not hurt friends (`0x8`)
- does not ping units (`0x10`)
- detonates explosives (`0x20`): If enabled, causes the effect to trigger the explosion of dropped grenades within its radius, used for explosion chaining. This only applies to maps loaded in singleplayer mode. Note that the item") receiving damage (e.g. the grenade equipment) must also have its _destroyed by explosions_ flag enabled. If the grenade has been placed within the scenario in Sapien, the _does accelerate_ flag will also need to be checked in the properties window.
- only hurts shields (`0x40`)
- causes flaming death (`0x80`)
- damage indicators always point down (`0x100`)
- skips shields (`0x200`)
- only hurts one infection form (`0x400`)
- can cause multiplayer headshots (`0x800`): Applies a 2x damage multiplier to headshots in multiplayer (based on how the map was loaded, not scenario type). This multiplier stacks with material modifiers below, so for example a cyborg armor modifier of 2 with this flag enabled results in a 4x headshot damage modifier. A model's headshot area is determined by its `head`region.
- infection form pop (`0x1000`)
| damage aoe core radius | `float` | * Unit: world units Objects within this radius will be damaged regardless of obstructions in the BSP. Outside this radius, there must be a line of sight between the effect and target. Contrary to the Guerilla tooltip, this field does not determine if the damage has an area of effect; see _radius_. |
| damage lower bound | `float` | Sets the minimum amount of damage done when this effect is applied in a scaled way. For example, projectiles can have min and max damage ranges for air and water. |
| damage upper bound | `Bounds`? | Maximum damage will be randomly chosen from this range. |
| damage vehicle passthrough penalty | `float` |  |
| damage active camouflage damage | `float` |  |
| damage stun | `float` | * Min: 0 * Max: 1 |
| damage maximum stun | `float` | * Min: 0 * Max: 1 |
| damage stun time | `float` | * Unit: seconds |
| damage instantaneous acceleration | `float` | Controls how much moveable objects are accelerated by this effect. This is scaled by the _acceleration scale_ field of object"). |
| dirt | `float` |  |
| sand | `float` |  |
| stone | `float` |  |
| snow | `float` |  |
| wood | `float` |  |
| metal hollow | `float` |  |
| metal thin | `float` |  |
| metal thick | `float` |  |
| rubber | `float` |  |
| glass | `float` |  |
| force field | `float` |  |
| grunt | `float` |  |
| hunter armor | `float` |  |
| hunter skin | `float` |  |
| elite | `float` |  |
| jackal | `float` |  |
| jackal energy shield | `float` |  |
| engineer skin | `float` |  |
| engineer force field | `float` |  |
| flood combat form | `float` |  |
| flood carrier form | `float` |  |
| cyborg armor | `float` | Also labeled as simply **cyborg** in Guerilla. Used to modify damage on all body parts of the cyborg (spartan) biped. |
| cyborg energy shield | `float` | Damage modifier for the "cyborg energy shield" material type. See the _shield material type_ field of the unit's model_ collision_ geometry tag. |
| human armor | `float` |  |
| human skin | `float` |  |
| sentinel | `float` |  |
| monitor | `float` |  |
| plastic | `float` |  |
| water | `float` |  |
| leaves | `float` |  |
| elite energy shield | `float` | Damage modifier for the "elite energy shield" material type. See the _shield material type_ field of the unit's model_ collision_ geometry tag. Note that this field receives a hard-coded singleplayer adjustment at map compile time for the pistol's damage effect. |
| ice | `float` |  |
| hunter shield | `float` |  |

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `damage_effect`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
