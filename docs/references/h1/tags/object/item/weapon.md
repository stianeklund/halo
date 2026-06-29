# Halo 1 Reference: tags/object/item/weapon

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/object/item/weapon/`
- Local path: `tags/object/item/weapon/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

`flags` `bitfield` Various flags that change the behavior of the given trigger.
| Flag | Mask | Comments |
| --- | --- | --- |
| tracks fired projectile | `0x1` |  |
| random firing effects | `0x2` | If false the firing effects in the firing effects block will be picked in order. If true they will be picked randomly. |
| can fire with partial ammo | `0x4` | The weapon will be able to fire as long as the magazine content is above `0`. |
| does not repeat automatically | `0x8` | Disables repeat fires while holding the trigger. If true you need to release the trigger before it can fire again. Bug: This breaks if the weapon is a charge weapon with the _discharging spews_ behavior. When the player is a client in multiplayer it will immediately start charging again as soon as it is fired potentially causing automatic firing behavior. |
| locks in on off state | `0x10` | Makes the trigger behave like a switch. Tap it to turn it on. Tap it again to turn it off. While on, weapon will continue firing while meleeing or throwing grenades, when dropped, and when switched, firing in unexpected directions in the latter two states. |
| projectiles use weapon origin | `0x20` | If true the projectiles will spawn at the third person `primary trigger` or `secondary trigger` marker instead of originating from the center of the first person view. |
| sticks when dropped | `0x40` | If true the weapon's trigger will be held down when it is dropped, regardless of whether it was held down by the wielder at the time. These dropped weapons will expend ammunition but not build up age. |
| ejects during chamber | `0x80` |  |
| discharging spews | `0x100` | If true and the trigger is set to discharge on overcharge, will automatically fire every tick for the duration of spew time. |
| analog rate of fire | `0x200` |  |
| use error when unzoomed | `0x400` | Makes the trigger always fire completely accurately when zoomed in, disregarding the triggers' defined error values. |
| projectile vector cannot be adjusted | `0x800` | Forces projectiles to be fired in the direction the gun (more precisely the gun's trigger marker) is facing, rather than the direction the wielder is looking. Most relevant for vehicle-mounted weapons. |
| projectiles have identical error | `0x1000` | Makes it so that all projectiles that are fired simultaneously move in the same direction. Only affects triggers that have multiple projectiles per shot. |
| projectile is client side only | `0x2000` | Makes it so that in multiplayer the projectile is not syncronized over the network and is instead created by client trigger simulation. |
| use unit adjust projectile ray from halo1 | `0x4000` | * H1A only By default, the engine now uses the logic found in later Halos to address projectile bugs. This flag forces the original H1 behavior and is used for the Covenant dropship turret. |
`maximum rate of fire` `Bounds` ?This determines the maximum number of times this trigger can be fired per second. The first value is initial rate of fire and the second value is the final rate of fire. Weapons cannot fire faster than once per tick, and non-positive (0 or less) rate of fire results in firing once per tick. Because weapons cannot fire in between ticks, fire rate is also effectively rounded down to the nearest tickrate/n for any positive integer n (so at 30 ticks per second: 30, 15, 10, 7.5, 6, 5, 4.288, 3.75, 3.333, 3, etc.)
`acceleration time` `float` The number of seconds required to hold the trigger in order to reach the final rate of fire. A value of `0` means the trigger will always use the final rate of fire.
`deceleration time` `float` The number of seconds required to release the trigger in order to reach the initial rate of fire. A value of `0` means the trigger will revert to the initial rate of fire the moment the trigger is released.
`blurred rate of fire` `float` The rate of fire needed to switch the object to its ~blur permutation.
`magazine` `uint16` →The index of the magazine which this trigger draws ammunition from. If no magazine is given (null), then the weapon effectively has an unlimited amount of rounds.
`rounds per shot` `int16` This is the amount of ammo each trigger fire uses. Unless the "can fire with partial ammo" flag is set, the trigger cannot be fired if there is less than this much ammo remaining and will, instead, reload automatically. Firing with partial ammo with the partial ammo flag will result in the loaded ammo reaching 0 rather than a negative amount. A negative value results in the magazine gaining loaded rounds upon firing.
`minimum rounds loaded` `int16` The magazine will automatically reload if less than this much ammo remains in the magazine, and the weapon cannot fire unless this much ammo is loaded. Unlike _rounds per shot_, this ignores the "can fire with partial ammo" flag.
`projectiles between contrails` `int16` Contrails will not be created until this many projectiles have been fired. This resets every time the trigger is released, and the first projectile will always have a contrail.
`firing noise` `enum` How loudly AI will perceive this trigger when it fires, determining how they will react (if at all).
| Option | Value | Comments |
| --- | --- | --- |
| silent | `0x0` | The sound is not perceived at all. |
| medium | `0x1` | This is the volume of the sniper rifle. The exact radius is unknown. |
| loud | `0x2` |  |
| shout | `0x3` | Every AI in the BSP can hear this. |
| quiet | `0x4` |  |
`error` `Bounds` The function of this field is unknown. It is **unused by the game**. Although labeled in Guerilla as a range [0, 1], the pistol has a max value of 2. Setting this to 0 does not affect projectile spread.
`error acceleration time` `float` Affects how long it takes for the weapon to reach its maximum projectile error angle while holding the trigger. Can be negated using quick trigger taps.
`error deceleration time` `float` Affects how long it takes for the weapon to reach its minimum projectile error angle during a pause in firing.
`charging time` `float` The amount of time the user has to hold the trigger before the weapon is considered charged.
`charged time` `float` The amount of time it takes _after_ the weapon becomes charged before the overcharged behavior takes effect. **Unused if _overcharged action_ is set to none.**
`overcharged action` `enum` The reaction the weapon will have to the charged time running out.
| Option | Value | Comments |
| --- | --- | --- |
| none | `0x0` | Nothing happens and the gun will stay charged until the trigger is released. |
| explode | `0x1` | The weapon will detonate using its item _detonation effect_. |
| discharge | `0x2` | Forces the gun to fire as it would have had the user let go of the trigger. |
`charged illumination` `float`
`spew time` `float` The amount of time the weapon will be forced to fire at its given firerate after overcharging.
`charging effect` `TagDependency`

*   sound
*   effect
`distribution function` `enum`
| Option | Value | Comments |
| --- | --- | --- |
| point | `0x0` | Causes projectiles' firing angle to be randomly offset by a number between the minimum error and the current error angle (see below). |
| horizontal fan | `0x1` | Causes projectiles to be spread out in a fan, which actually seems to be vertical. |
`projectiles per shot` `int16` Affects how many projectiles are produced by a single shot. For example, the shotgun has this value set to 15. Each projectile is scattered by error angle independently.
`distribution angle` `float` Affects the spread of projectiles when _projectiles per shot_ is greater than 1 and _distribution function_ is _fan_.
`minimum error` `float` Floor value for error angle. Even if the minimum error angle is 0, this value takes precedence. Note that this field receives a hard-coded singleplayer adjustment at map compile time for the pistol.
`error angle` `Bounds` Determines the minimum and maximum angle of the "error cone". The min angle is for when you begin firing, while the max is reached after a period of sustained fire. The values are shown in degrees in Guerilla, but internally stored in radians. Note that these fields receives hard-coded singleplayer adjustments at map compile time for the pistol and plasma rifle.
| Field | Type | Comments |
| --- | --- | --- |
| min | `float` |  |
| max | `float` |  |
`first person offset` `Point3D` The relative offset from the first person camera the projectile spawns. X is front (+) and back (-), Y is left (+) and right (-), Z is up (+) and down (-).
`projectile object` `TagDependency`: object This is the object that spawns when firing the weapon. The object will spawn facing the same direction as the first person camera (along with any error angle applied, if necessary) with the velocity of the shooter. This reference should not be NULL to prevent crashes in H1CE.
`ejection port recovery time` `float`
`illumination recovery time` `float`
`heat generated per round` `float`
`age generated per round` `float`
`overload time` `float`
`illumination recovery rate` `float`
`ejection port recovery rate` `float`
`firing acceleration rate` `float` Pre-calculated rate of firing rate increase, based on the _acceleration time_ field.
`firing deceleration rate` `float` Pre-calculated rate of firing rate decrease, based on the _deceleration time_ field.
`error acceleration rate` `float` Pre-calculated rate of error angle increase, based on the _error acceleration time_ field.
`error deceleration rate` `float` Pre-calculated rate of error angle decrease, based on the _error deceleration time_ field.
`firing effects` `Block`
| Field | Type | Comments |
| --- | --- | --- |
| shot count lower bound | `uint16` |  |
| shot count upper bound | `uint16` |  |
| firing effect | `TagDependency` * sound * effect |  |
| misfire effect | `TagDependency` * sound * effect |  |
| empty effect | `TagDependency` * sound * effect |  |
| firing damage | `TagDependency`: damage_effect | Damage effect applied to the wielder when a shot is normally fired. Used for camera shaking, etc. |
| misfire damage | `TagDependency`: damage_effect |  |
| empty damage | `TagDependency`: damage_effect |  |

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/object/item/weapon/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
