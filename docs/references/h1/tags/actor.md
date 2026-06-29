# Halo 1 Tag: actor

## Summary

This tag page is primarily field documentation. See the tag information below for defined fields, enum values, units,
and runtime notes.

## Tag Information

Tag group: `actor`.

Defined fields and enum values mentioned:

- attack shield fraction
- attacking crouch threshold
- attacking evasion threshold
- begin moving angle
- berserk damage amount
- berserk damage threshold
- berserk grenade chance
- berserk proximity
- central vision angle
- change facing stand time
- combat idle aiming
- combat idle facing
- combat idle looking
- combat idle speech time
- combat look delta l
- combat look delta r
- combat perception time
- combat position time
- cosine begin moving angle
- cosine maximum aiming deviation
- cosine maximum looking deviation
- cover damage threshold
- cowering time
- crouching gun offset
- danger trigger time
- defending crouch threshold
- defending evasion threshold
- defending hide time modifier
- defensive crouch type
- defensive crouch type any target
- defensive crouch type danger
- defensive crouch type flood shamble
- defensive crouch type hide behind shield
- defensive crouch type low shields
- defensive crouch type never
- dive from grenade chance
- dive into cover chance
- do not use
- do not use 1
- do not use 2
- emerge from cover chance
- encounters squads
- evasion delay time
- evasion seek cover chance
- event look time modifier
- event look time modifier max
- event look time modifier min
- flags
- flags always berserk in attacking mode
- flags always charge at enemies
- flags always charge in attacking mode
- flags avoid friends line of fire
- flags berserking uses panicked movement
- flags can see in darkness
- flags cannot move while crouching
- flags crouch when guarding
- flags crouch when hiding from unopposable
- flags crouch when in line of fire
- flags crouch when not in combat
- flags crouching must move forward
- flags defensive crouch while charging
- flags dive off ledges
- flags fixed crouch facing
- flags flying
- flags gets in vehicles with player
- flags must crouch to shoot
- flags panic when surprised
- flags panicked by unopposable enemy
- flags shoot at targets last location
- flags sneak uncovering pursuit position
- flags sneak uncovering target
- flags stalking freeze if exposed
- flags standing must move forward
- flags start firing before aligned
- flags suicidal melee attack
- flags swarm
- flags try to stay still when crouched
- flags unused
- flags unused 1
- flags use stalking behavior

## Details

`flags` `bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| can see in darkness | `0x1` | |
| sneak uncovering target | `0x2` | |
| sneak uncovering pursuit position | `0x4` | |
| unused | `0x8` | |
| shoot at targets last location | `0x10` | When no target is visible, briefly shoot at where the target was last
seen. |
| try to stay still when crouched | `0x20` | |
| crouch when not in combat | `0x40` | |
| crouch when guarding | `0x80` | |
| unused 1 | `0x100` | |
| must crouch to shoot | `0x200` | |
| panic when surprised | `0x400` | When an enemy unit is newly detected within this actor's surprise distance, panic. |
| always charge at enemies | `0x800` | Always ignore other paramaters such as desired combat ranges and run at
enemies. |
| gets in vehicles with player | `0x1000` | |
| start firing before aligned | `0x2000` | |
| standing must move forward | `0x4000` | |
| crouching must move forward | `0x8000` | |
| defensive crouch while charging | `0x10000` | |
| use stalking behavior | `0x20000` | |
| stalking freeze if exposed | `0x40000` | |
| always berserk in attacking mode | `0x80000` | |
| berserking uses panicked movement | `0x100000` | |
| flying | `0x200000` | |
| panicked by unopposable enemy | `0x400000` | |
| crouch when hiding from unopposable | `0x800000` | |
| always charge in attacking mode | `0x1000000` | When this actor's squad is in attacking mode, ignore other paramaters
such as desired combat ranges and run straight at enemies. |
| dive off ledges | `0x2000000` | Will not attempt to avoid sheer drops when diving away from dangerous objects. |
| swarm | `0x4000000` | |
| suicidal melee attack | `0x8000000` | |
| cannot move while crouching | `0x10000000` | |
| fixed crouch facing | `0x20000000` | |
| crouch when in line of fire | `0x40000000` | |
| avoid friends line of fire | `0x80000000` | |
`more flags` `bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| avoid all enemy attack vectors | `0x1` | |
| must stand to fire | `0x2` | |
| must stop to fire | `0x4` | |
| disallow vehicle combat | `0x8` | |
| pathfinding ignores danger | `0x10` | Will not attempt to avoid live grenades, projectiles, environmental damage, etc
when moving. |
| panic in groups | `0x20` | |
| no corpse shooting | `0x40` | |
`type` `enum` This field acts as an index into an actor type definitions array where hardcoded actor behaviours are defined.
| Option | Value | Comments |
| --- | --- | --- |
| elite | `0x0` | Uses the elite actor type definition. |
| jackal | `0x1` | Uses the jackal actor type definition. |
| grunt | `0x2` | Uses the grunt actor type definition. |
| hunter | `0x3` | Uses the hunter actor type definition. |
| engineer | `0x4` | Uses the engineer actor type definition. |
| assassin | `0x5` | Uses the elite actor type definition (duplicate). |
| player | `0x6` | Uses the marine actor type definition (duplicate). |
| marine | `0x7` | Uses the marine actor type definition. |
| crew | `0x8` | Uses the crew actor type definition. |
| combat form | `0x9` | Uses the flood actor type definition. |
| infection form | `0xA` | Uses the infection actor type definition. |
| carrier form | `0xB` | Uses the flood carrier actor type definition. |
| monitor | `0xC` | Uses the sentinel actor type definition (duplicate). |
| sentinel | `0xD` | Uses the sentinel actor type definition. |
| none | `0xE` | Uses the grunt actor type definition. |
| mounted weapon | `0xF` | Uses the mounted_ weapon actor type definition. |
`max vision distance` `float` Maximum range of sight.
`central vision angle` `float` Horizontal angle within which the actor sees targets out the the maximum range.
`max vision angle` `float` Maximum horizontal angle within which the actor sees targets at range.
`peripheral vision angle` `float` Maximum horizontal angle within which the actor sees targets "out of the corner of its
eye" (Guerilla description).
`peripheral distance` `float`
`standing gun offset` `Vector3D`
`crouching gun offset` `Vector3D`
`hearing distance` `float`
`notice projectile chance` `float`
`notice vehicle chance` `float`
`combat perception time` `float`
`guard perception time` `float`
`non combat perception time` `float`
`inverse combat perception time` `float`
`inverse guard perception time` `float`
`inverse non combat perception time` `float`
`dive into cover chance` `float`
`emerge from cover chance` `float`
`dive from grenade chance` `float`
`pathfinding radius` `float`
`glass ignorance chance` `float`
`stationary movement dist` `float`
`free flying sidestep` `float`
`begin moving angle` `float`
`cosine begin moving angle` `float`
`maximum aiming deviation` `Vector2D`
`maximum looking deviation` `Vector2D`
`noncombat look delta l` `float`
`noncombat look delta r` `float`
`combat look delta l` `float`
`combat look delta r` `float`
`idle aiming range` `Vector2D`
`idle looking range` `Vector2D`
`event look time modifier` `Bounds`
| Field | Type | Comments |
| --- | --- | --- |
| min | `float` | |
| max | `float` | |
`noncombat idle facing` `Bounds` ?
`noncombat idle aiming` `Bounds` ?
`noncombat idle looking` `Bounds` ?
`guard idle facing` `Bounds` ?
`guard idle aiming` `Bounds` ?
`guard idle looking` `Bounds` ?
`combat idle facing` `Bounds` ?
`combat idle aiming` `Bounds` ?
`combat idle looking` `Bounds` ?
`cosine maximum aiming deviation` `Vector2D`
`cosine maximum looking deviation` `Vector2D`
`do not use` `TagDependency`: weapon
`do not use 1` `TagDependency`: projectile
`unreachable danger trigger` `enum`
| Option | Value | Comments |
| --- | --- | --- |
| never | `0x0` | |
| visible | `0x1` | |
| shooting | `0x2` | |
| shooting near us | `0x3` | |
| damaging us | `0x4` | |
| unused | `0x5` | |
| unused1 | `0x6` | |
| unused2 | `0x7` | |
| unused3 | `0x8` | |
| unused4 | `0x9` | |
`vehicle danger trigger` `enum` ?
`player danger trigger` `enum` ?
`danger trigger time` `Bounds` ?
`friends killed trigger` `int16`
`friends retreating trigger` `int16`
`retreat time` `Bounds` ?
`cowering time` `Bounds` ?
`friend killed panic chance` `float`
`leader type` `enum` ?
`leader killed panic chance` `float`
`panic damage threshold` `float`
`surprise distance` `float`
`hide behind cover time` `Bounds` ?
`hide target not visible time` `float`
`hide shield fraction` `float`
`attack shield fraction` `float`
`pursue shield fraction` `float`
`defensive crouch type` `enum`
| Option | Value | Comments |
| --- | --- | --- |
| never | `0x0` | |
| danger | `0x1` | |
| low shields | `0x2` | |
| hide behind shield | `0x3` | |
| any target | `0x4` | |
| flood shamble | `0x5` | |
`attacking crouch threshold` `float`
`defending crouch threshold` `float`
`min stand time` `float`
`min crouch time` `float`
`defending hide time modifier` `float`
`attacking evasion threshold` `float`
`defending evasion threshold` `float`
`evasion seek cover chance` `float`
`evasion delay time` `float`
`max seek cover distance` `float`
`cover damage threshold` `float`
`stalking discovery time` `float`
`stalking max distance` `float`
`stationary facing angle` `float`
`change facing stand time` `float`
`uncover delay time` `Bounds` ?
`target search time` `Bounds` ?
`pursuit position time` `Bounds` ?
`num positions (coord)` `uint16`
`num positions (normal)` `uint16`
`melee attack delay` `float` Minimum time between consecutive melee attempts.
`melee fudge factor` `float` Determines where to start the melee animation:0 begins on top of the target, positive values
begin in front of the target, negative values begin behind the target.
`melee charge time` `float` How long to chase a target to begin a melee attack before giving up.
`melee leap range` `Bounds` ?
`melee leap velocity` `float`
`melee leap chance` `float`
`melee leap ballistic` `float`
`berserk damage amount` `float` When HP is below the berserk damage threshold, the actor may berserk when receiving any
damage that deals this much of the actor's maximum HP in one tick.
`berserk damage threshold` `float` Below this fraction of total HP, the actor may berserk when receiving damage, depending
on their berserk damage amount.
`berserk proximity` `float` When an enemy unit is within this radius of the actor, berserk.
`suicide sensing dist` `float`
`berserk grenade chance` `float` How likely the actor is to berserk when grenades or supercombining projectiles attach to
them.
`guard position time` `Bounds` ?
`combat position time` `Bounds` ?
`old position avoid dist` `float`
`friend avoid dist` `float` Constraints the minimum distance that this actor can be from its allies. Increase this to avoid
actors "clumping" together.
`noncombat idle speech time` `Bounds` ?
`combat idle speech time` `Bounds` ?
`do not use 2` `TagDependency`: actor

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `actor`. Retrieved 2026-06-24. Text is retained locally so this
page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC
BY-SA 3.0 licensing.
