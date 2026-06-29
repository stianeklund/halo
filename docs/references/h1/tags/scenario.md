# Halo 1 Tag: scenario

## Summary

This tag page is primarily field documentation. See the tag information below for defined fields, enum values, units, and runtime notes.

## Tag Information

Tag group: `scenario`.

HaloScript entries:

- `volume_test_object "volume_name" <object>`

Defined fields and enum values mentioned:

- actor palette
- actor palette reference
- ai animation references
- ai animation references animation graph
- ai animation references animation name
- ai conversations
- ai conversations flags
- ai conversations flags keep trying to play
- ai conversations flags player must be looking
- ai conversations flags player must be visible
- ai conversations flags stop if alerted to enemy
- ai conversations flags stop if damaged
- ai conversations flags stop if death
- ai conversations flags stop if visible enemy
- ai conversations flags stop other actions
- ai conversations lines
- ai conversations lines addressee
- ai conversations lines addressee none
- ai conversations lines addressee participant
- ai conversations lines addressee player
- ai conversations lines flags
- ai conversations lines flags addressee look at speaker
- ai conversations lines flags everyone look at addressee
- ai conversations lines flags everyone look at speaker
- ai conversations lines flags wait after until told to advance
- ai conversations lines flags wait until everyone nearby
- ai conversations lines flags wait until speaker nearby
- ai conversations lines line delay time
- ai conversations lines participant
- ai conversations lines variant 1
- ai conversations lines variant 2
- ai conversations lines variant 3
- ai conversations lines variant 4
- ai conversations lines variant 5
- ai conversations lines variant 6
- ai conversations name
- ai conversations participants
- ai conversations participants actor type
- ai conversations participants actor type assassin
- ai conversations participants actor type carrier form
- ai conversations participants actor type combat form
- ai conversations participants actor type crew
- ai conversations participants actor type elite
- ai conversations participants actor type engineer
- ai conversations participants actor type grunt
- ai conversations participants actor type hunter
- ai conversations participants actor type infection form
- ai conversations participants actor type jackal
- ai conversations participants actor type marine
- ai conversations participants actor type monitor
- ai conversations participants actor type mounted weapon
- ai conversations participants actor type none
- ai conversations participants actor type player
- ai conversations participants actor type sentinel
- ai conversations participants encounter index
- ai conversations participants encounter name
- ai conversations participants flags
- ai conversations participants flags has alternate
- ai conversations participants flags is alternate
- ai conversations participants flags optional
- ai conversations participants selection type
- ai conversations participants selection type any actor
- ai conversations participants selection type disembodied
- ai conversations participants selection type friendly actor
- ai conversations participants selection type in player s vehicle
- ai conversations participants selection type not in a vehicle
- ai conversations participants selection type prefer sergeant
- ai conversations participants selection type radio sergeant
- ai conversations participants selection type radio unit
- ai conversations participants set new name
- ai conversations participants use this object
- ai conversations participants variant numbers
- ai conversations run to player dist
- ai conversations trigger distance
- ai recording references
- ai recording references recording name
- ai script references
- ai script references script name
- biped palette
- biped palette name

## Details

don't use`TagDependency`: scenario_structure_bspDeprecated field, hidden in H1A Guerilla.
won't use`TagDependency`: scenario_structure_bspDeprecated field, hidden in H1A Guerilla.
can't use`TagDependency`: skyDeprecated field, hidden in H1A Guerilla.
skies`Block`A list of skies available to clusters in this scenario's BSP(s). Level artists can select which sky is used using the special material names`+sky0`, `+sky1`, etc. See also: multiple skies. The first sky in this block also serves as the indoor sky.
| Field | Type | Comments |
| --- | --- | --- |
| sky | `TagDependency`: sky | A reference to a sky tag. |
type`enum`Determines the type of map this scenario represents. This also causes map compilers like Tool and invader to apply certain hardcoded tag patches, but see below in H1A.
| Option | Value | Comments |
| --- | --- | --- |
| singleplayer | `0x0` | The map is meant to be used for singleplayer and loaded with the `map_name` command. Cannot be played in multiplayer. |
| multiplayer | `0x1` | The map is meant for multiplayer and can be loaded with `sv_map`. |
| user interface | `0x2` | The map is meant to serve as ui.map for the game's main menu. |
flags`bitfield`
| Flag | Mask | Comments |
| --- | --- | --- |
| cortana hack | `0x1` |  |
| use demo ui | `0x2` |  |
| color correction ntsc to srgb | `0x4` | * H1A only Unused at this time. |
| do not apply bungie campaign tag patches | `0x8` | * H1A only Opts out of hard-coded tag patches made to the pistol and plasma rifle for singleplayer scenarios. invader-build respects this when building maps targeting Custom Edition. |
child scenarios`Block`
| Field | Type | Comments |
| --- | --- | --- |
| child scenario | `TagDependency`: scenario | This scenario tag will be merged into the main scenario tag (and dereferenced) on map build. |
local north`float`
predicted resources`Block`
| Field | Type | Comments |
| --- | --- | --- |
| type | `enum` |  |
Option values:
- bitmap (`0x0`)
- sound (`0x1`)
| resource index | `uint16` |  |
| tag | `uint32` |  |
functions`Block`Marked deprecated in H1A and no longer appears in H1A tools.
editor scenario data`TagDataOffset`
| Field | Type | Comments |
| --- | --- | --- |
| size | `uint32` |  |
| external | `uint32` |  |
| file offset | `uint32` |  |
| pointer | `ptr64` |  |
comments`Block`
| Field | Type | Comments |
| --- | --- | --- |
| position | `Point3D` |  |
| comment | `TagDataOffset`? |  |
object names`Block`Contains the names of all named objects in the scenario. These names are referenced by index for each type of placed object if that object is named. The console command `debug_objects_names` can display these. The maximum number of names was increased to 640 (from Halo 2) in H1A.
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagString` |  |
| object type | `enum` | * Cache only |
Option values:
- biped (`0x0`)
- vehicle (`0x1`)
- weapon (`0x2`)
- equipment (`0x3`)
- garbage (`0x4`)
- projectile (`0x5`)
- scenery (`0x6`)
- device machine (`0x7`)
- device control (`0x8`)
- device light fixture (`0x9`)
- placeholder (`0xA`)
- sound scenery (`0xB`)
| object index | `uint16` | * Cache only |
scenery`Block`
| Field | Type | Comments |
| --- | --- | --- |
| type | `uint16` |  |
| name | `uint16`→ |  |
| not placed | `bitfield` |  |
Flag values:
- automatically (`0x1`): This object does not exist on map start and must be created by a script.
- on easy (`0x2`): * Unused Has no effect on spawning. Use scripts to control difficulty-specific spawning of objects.
- on normal (`0x4`): * Unused
- on hard (`0x8`): * Unused
| desired permutation | `uint16` |  |
| position | `Point3D` |  |
| rotation | `Euler3D` |  |
Field values:
- yaw (`float`)
- pitch (`float`)
- roll (`float`)
| bsp indices | `uint16` | * Cache only unused in Halo PC; bitmask |
scenery palette`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagDependency`: scenery | * Non-null |
bipeds`Block`Special-purpose bipeds placed in the map, typically used for scripted characters and cutscenes. AI should instead be spawned using encounters, since the actors will be properly difficulty-scaled. Although there is technically a very high limit to this block, unmodified Sapien only supports up to 128 bipeds being placed.
| Field | Type | Comments |
| --- | --- | --- |
| type | `uint16` |  |
| name | `uint16`→ |  |
| not placed | `bitfield`? |  |
| desired permutation | `uint16` |  |
| position | `Point3D` |  |
| rotation | `Euler3D`? |  |
| body vitality modifier | `float` | * Min: 0 |
| flags | `bitfield` |  |
Flag values:
- dead (`0x1`)
biped palette`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagDependency`: biped | * Non-null |
vehicles`Block`Vehicle spawn points, which can be used for both singleplayer and multiplayer modes. Netgame flags are used in race mode. The limit was raised to 256 in H1A.
| Field | Type | Comments |
| --- | --- | --- |
| type | `uint16` |  |
| name | `uint16`→ |  |
| not placed | `bitfield`? |  |
| desired permutation | `uint16` |  |
| position | `Point3D` |  |
| rotation | `Euler3D`? |  |
| body vitality | `float` | * Min: 0 * Max: 1 |
| flags | `bitfield`? |  |
| multiplayer team index | `int8` | Determines which team the vehicle belongs to for gametype settings. Index `0` is for red team, and index `1` is blue. |
| multiplayer spawn flags | `bitfield` |  |
Flag values:
- slayer default (`0x1`): The vehicle will spawn in Slayer when the gametype specifies default vehicle settings.
- ctf default (`0x2`): The vehicle will spawn in CTF when the gametype specifies default vehicle settings.
- king default (`0x4`): The vehicle will spawn in King of the Hill when the gametype specifies default vehicle settings.
- oddball default (`0x8`): The vehicle will spawn in Oddball when the gametype specifies default vehicle settings.
- unused (`0x10`)
- unused1 (`0x20`)
- unused2 (`0x40`)
- unused3 (`0x80`)
- slayer allowed (`0x100`): The vehicle spawn is permitted in Slayer when the gametype is using custom vehicle settings.
- ctf allowed (`0x200`): The vehicle spawn is permitted in CTF when the gametype is using custom vehicle settings.
- king allowed (`0x400`): The vehicle spawn is permitted in King of the Hill when the gametype is using custom vehicle settings.
- oddball allowed (`0x800`): The vehicle spawn is permitted in Oddball when the gametype is using custom vehicle settings.
- unused4 (`0x1000`)
- unused5 (`0x2000`)
- unused6 (`0x4000`)
- unused7 (`0x8000`)
vehicle palette`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagDependency`: vehicle | * Non-null |
equipment`Block`
| Field | Type | Comments |
| --- | --- | --- |
| type | `uint16` |  |
| name | `uint16`→ |  |
| not placed | `bitfield`? |  |
| desired permutation | `uint16` |  |
| position | `Point3D` |  |
| rotation | `Euler3D`? |  |
| flags | `bitfield` |  |
Flag values:
- initially at rest (`0x1`)
- obsolete (`0x2`)
- does accelerate (`0x4`): Can be moved by impulses like explosions. Grenades with this flag can be detonated.
equipment palette`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagDependency`: equipment | * Non-null |
weapons`Block`Weapon placements for singeplayer. Multiplayer weapon spawns should be placed using netgame equipment instead.
| Field | Type | Comments |
| --- | --- | --- |
| type | `uint16` |  |
| name | `uint16`→ |  |
| not placed | `bitfield`? |  |
| desired permutation | `uint16` |  |
| position | `Point3D` |  |
| rotation | `Euler3D`? |  |
| rounds reserved | `uint16` | does not include loaded rounds; cannot go above maximum rounds |
| rounds loaded | `uint16` |  |
| flags | `bitfield`? |  |
weapon palette`Block`The types of weapons that can be placed throughout a singeplayer scenario.
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagDependency`: weapon | * Non-null |
device groups`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagString` | * Non-null |
| initial value | `float` | * Min: 0 * Max: 1 |
| flags | `bitfield` |  |
Flag values:
- can change only once (`0x1`)
machines`Block`
| Field | Type | Comments |
| --- | --- | --- |
| type | `uint16` |  |
| name | `uint16`→ |  |
| not placed | `bitfield`? |  |
| desired permutation | `uint16` |  |
| position | `Point3D` |  |
| rotation | `Euler3D`? |  |
| power group | `uint16`→ |  |
| position group | `uint16`→ |  |
| device flags | `bitfield` |  |
Flag values:
- initially open (`0x1`)
- initially off (`0x2`)
- can change only once (`0x4`)
- position reversed (`0x8`)
- not usable from any side (`0x10`)
| machine flags | `bitfield` |  |
Flag values:
- does not operate automatically (`0x1`): Disables the automatic door feature.
- one sided (`0x2`): Prevents allies from automatically opening a door when on its "forward" side. May have other effects.
- never appears locked (`0x4`)
- opened by melee attack (`0x8`)
machine palette`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagDependency`: device_machine | * Non-null |
controls`Block`
| Field | Type | Comments |
| --- | --- | --- |
| type | `uint16` |  |
| name | `uint16`→ |  |
| not placed | `bitfield`? |  |
| desired permutation | `uint16` |  |
| position | `Point3D` |  |
| rotation | `Euler3D`? |  |
| power group | `uint16`→ |  |
| position group | `uint16`→ |  |
| device flags | `bitfield`? |  |
| control flags | `bitfield` |  |
Flag values:
- usable from both sides (`0x1`)
| no name | `int16` |  |
control palette`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagDependency`: device_control | * Non-null |
light fixtures`Block`
| Field | Type | Comments |
| --- | --- | --- |
| type | `uint16` |  |
| name | `uint16`→ |  |
| not placed | `bitfield`? |  |
| desired permutation | `uint16` |  |
| position | `Point3D` |  |
| rotation | `Euler3D`? |  |
| bsp indices | `uint16` | * Cache only unused in Halo PC; bitmask |
| power group | `uint16`→ |  |
| position group | `uint16`→ |  |
| device flags | `bitfield`? |  |
| color | `ColorRGB` |  |
| intensity | `float` |  |
| falloff angle | `float` |  |
| cutoff angle | `float` |  |
light fixture palette`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagDependency`: device_light_fixture | * Non-null |
sound scenery`Block`
| Field | Type | Comments |
| --- | --- | --- |
| type | `uint16` |  |
| name | `uint16`→ |  |
| not placed | `bitfield`? |  |
| desired permutation | `uint16` |  |
| position | `Point3D` |  |
| rotation | `Euler3D`? |  |
sound scenery palette`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagDependency`: sound_scenery | * Non-null |
player starting profile`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagString` |  |
| starting health modifier | `float` | * Min: 0 * Max: 1 |
| starting shield modifier | `float` | * Min: 0 * Max: 3 Values above 1 give Overshield layers. |
| primary weapon | `TagDependency`: weapon |  |
| primary rounds loaded | `uint16` |  |
| primary rounds reserved | `uint16` | does not include loaded ammo; can go above maximum rounds |
| secondary weapon | `TagDependency`: weapon |  |
| secondary rounds loaded | `uint16` |  |
| secondary rounds reserved | `uint16` | does not include loaded ammo; can go above maximum rounds |
| starting fragmentation grenade count | `int8` |  |
| starting plasma grenade count | `int8` |  |
| starting unknown grenade count 1 | `int8` |  |
| starting unknown grenade count 2 | `int8` |  |
player starting locations`Block`
| Field | Type | Comments |
| --- | --- | --- |
| position | `Point3D` |  |
| facing | `float` |  |
| team index | `uint16` |  |
| bsp index | `uint16`→ | * Unused The game does not read from this field. It is hidden in H1A Guerilla. |
| type 0 | `enum` |  |
Option values:
- none (`0x0`)
- ctf (`0x1`)
- slayer (`0x2`)
- oddball (`0x3`)
- king of the hill (`0x4`)
- race (`0x5`)
- terminator (`0x6`)
- stub (`0x7`)
- ignored1 (`0x8`)
- ignored2 (`0x9`)
- ignored3 (`0xA`)
- ignored4 (`0xB`)
- all games (`0xC`)
- all except ctf (`0xD`)
- all except race ctf (`0xE`)
| type 1 | `enum`? |  |
| type 2 | `enum`? |  |
| type 3 | `enum`? |  |
trigger volumes`Block`Cuboid volumes which can be used in scripts to test for the presence an object: `(volume_test_object "volume_name" <object>)`. An example use case is kill volumes.
| Field | Type | Comments |
| --- | --- | --- |
| type | `enum` |  |
Option values:
- fixed (`0x0`): Indicates that this trigger volume is a simple axis-aligned bounding box. This seems to be an older type unused by stock tags; Sapien will upgrade it to rotational but the game can still test against it.
- rotational (`0x1`): The "normal" trigger volume type which supports rotation; the test point is transformed by the inverse of the rotation matrix derived from the trigger volume's forward and up vectors before comparison with the corner bounds.
| name | `TagString` | The name of the volume which can be used in scripts. |
| parameters | `float[3]` | * Hidden appears to be zero |
| rotation vector forward | `Vector3D` | One of two vectors used to derive this volume's rotation matrix. |
| rotation vector up | `Vector3D` | One of two vectors used to derive this volume's rotation matrix. |
| starting corner | `Point3D` | The 3D world-unit coordinates of one corner of the volume. |
| ending corner offset | `Point3D` | The offset distances from the starting corner to the opposite corner. |
recorded animations`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagString` |  |
| version | `int8` |  |
| raw animation data | `int8` |  |
| unit control data version | `int8` |  |
| length of animation | `uint16` |  |
| recorded animation event stream | `TagDataOffset`? |  |
netgame flags`Block`Multiplayer game mode markers like flag locations, oddball spawns, and teleporters.
| Field | Type | Comments |
| --- | --- | --- |
| position | `Point3D` | The location in 3D space of this netgame flag. |
| facing | `float` | The direction that the netgame flag faces (yaw). How this is used depends on the _type_ of the flag. |
| type | `enum` | Determines which purpose this netgame flag serves. |
Option values:
- ctf flag (`0x0`): Two netgame flags should be placed for the CTF flags, with red team's flag using team index `0`, and blue team `1`. The _facing_ value is not used to orient the flag pole (the skull faces in the world's +X direction). Be careful not to place the CTF flags on the wrong side of the map or it may prevent players from spawning.
- ctf vehicle (`0x1`): _Unused._
- oddball ball spawn (`0x2`): Halo's oddball gamemode settings allow up to 16 simultaneous oddballs, so maps should include 16 unique oddball spawns. The oddball will spawn facing the world +X direction regardless of the _facing_ field. Use team index `0` for the first oddball spawn, which should be in a team-neutral location of the map. Increment the team index for each subsequent oddball spawn (up to index `15`). Halo will spawn balls at each flag _in order_ of their index, so ensure flags are alternated across the map to prevent an unfair advantage to a team already occupying an area.
- race track (`0x3`): Determines the checkpoints for race game modes. The game supports up to 32 race track points. The team index should be numbered sequentially without gaps, starting at `0` in a team-neutral location. In Rally mode, checkpoints will be selected in random order.
- race vehicle (`0x4`): Used by Halo to spawn vehicles near players at the start of Race games, so you should place up to 16 of them near player spawn points used for Race. _Team index_ is not used. The green marker handle should be pointed in the direction the vehicle will face forward.
- vegas bank (`0x5`): _Unused._
- teleport from (`0x6`): Teleporter entry node. _Team index_ determines the "channel" and links entry and exit nodes together.
- teleport to (`0x7`): Teleporter exit node. _Team index_ determines the "channel" and links entry and exit nodes together. The _difference_ in facing direction (yaw) between the entry and exit nodes is added to the player's yaw to determine their facing direction on exit. For example, if the entry node's _facing_ is -90 degrees and the exit's is 0 degrees (a difference of +90 degrees), then a player entering the teleporter facing North would exit facing East.
- hill flag (`0x8`): Defines the location of a King of the Hill game mode hill. The _team index_ should increment for each kill, with index `0` being the initial neutral hill. At least two hills should be placed to properly support moving-hill (Crazy King) mode, though this is not required. Each hill can be placed using either a single netgame flag, in which case it will be a 2x2 world unit square, or 3-8 netgame flags sharing the same _team index_ to define a custom boundary. Note that the actual resulting hill shape will be the convex hull of the points, so you cannot create non-convex shapes. A hill made of 2 points is invalid and causes an exception. The _facing_ field is not used.
| usage id | `uint16` | Also called **team_ index** in legacy HEK. The meaning of this field depends on the netgame flag type, and is not necessarily about teams. See the above type descriptions for more details on usage. |
| weapon group | `TagDependency`: item_collection | * Unused Unused by the game. Hidden in H1A tools. |
netgame equipment`Block`
| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- levitate (`0x1`)
| type 0 | `enum`? |  |
| type 1 | `enum`? |  |
| type 2 | `enum`? |  |
| type 3 | `enum`? |  |
| team index | `uint16` |  |
| spawn time | `uint16` | * Unit: seconds 0 = use default spawn time in item_ collection |
| unknown ffffffff | `uint32` | * Cache only: true always 0xFFFFFFFF? |
| position | `Point3D` |  |
| facing | `float` |  |
| item collection | `TagDependency`: item_collection |  |
starting equipment`Block`
| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- no grenades (`0x1`)
- plasma grenades (`0x2`)
| type 0 | `enum`? |  |
| type 1 | `enum`? |  |
| type 2 | `enum`? |  |
| type 3 | `enum`? |  |
| item collection 1 | `TagDependency`: item_collection |  |
| item collection 2 | `TagDependency`: item_collection |  |
| item collection 3 | `TagDependency`: item_collection |  |
| item collection 4 | `TagDependency`: item_collection |  |
| item collection 5 | `TagDependency`: item_collection |  |
| item collection 6 | `TagDependency`: item_collection |  |
bsp switch trigger volumes`Block`
| Field | Type | Comments |
| --- | --- | --- |
| trigger volume | `uint16`→ |  |
| source | `uint16`→ | The source BSP, stored as an index into the scenario BSPs block. |
| destination | `uint16`→ | The destination BSP, stored as an index into the scenario BSPs block. |
| unknown | `uint16` |  |
decals`Block`
| Field | Type | Comments |
| --- | --- | --- |
| decal type | `uint16`→ |  |
| yaw | `int8` |  |
| pitch | `int8` |  |
| position | `Point3D` |  |
decal palette`Block`
| Field | Type | Comments |
| --- | --- | --- |
| reference | `TagDependency`: decal |  |
detail object collection palette`Block`
| Field | Type | Comments |
| --- | --- | --- |
| reference | `TagDependency`: detail_object_collection |  |
actor palette`Block`
| Field | Type | Comments |
| --- | --- | --- |
| reference | `TagDependency`: actor_variant |  |
encounters`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagString` |  |
| flags | `bitfield` |  |
Flag values:
- not initially created (`0x1`)
- respawn enabled (`0x2`)
- initially blind (`0x4`)
- initially deaf (`0x8`)
- initially braindead (`0x10`)
- 3d firing positions (`0x20`)
- manual bsp index specified (`0x40`)
| team index | `enum` |  |
Option values:
- default by unit (`0x0`): Uses the default team defined in the unit's biped tag.
- player (`0x1`)
- human (`0x2`)
- covenant (`0x3`)
- flood (`0x4`)
- sentinel (`0x5`)
- unused6 (`0x6`)
- unused7 (`0x7`)
- unused8 (`0x8`)
- unused9 (`0x9`)
| one | `int16` | * Cache only |
| search behavior | `enum` |  |
Option values:
- normal (`0x0`)
- never (`0x1`)
- tenacious (`0x2`)
| manual bsp index | `uint16`→ |  |
| respawn delay | `Bounds` | * Unit: seconds |
Field values:
- min (`float`)
- max (`float`)
| precomputed bsp index | `uint16` | * Cache only |
| squads | `Block` | * HEK max count: 64 |
Flag values:
- name (`TagString`)
- actor type (`uint16`→)
- platoon (`uint16`→)
- initial state (`enum`): The squad's or unit's behaviour when initially placed.
- none (`0x0`): Retain behaviour previously defined. Used for starting locations, or empty squads meant for migration.
- sleeping (`0x1`): Actors will only be alerted by sounds or physical contact, and will use sleeping animations if they have any.
- alert (`0x2`): Actors will stand still and look forward.
- moving repeat same position (`0x3`): These and the following behaviours use move positions defined in squad data to determine where to move. Use casual walking animations if applicable.
- moving loop (`0x4`)
- moving loop back and forth (`0x5`)
- moving loop randomly (`0x6`)
- moving randomly (`0x7`)
- guarding (`0x8`): Choose one guard firing position and move there, remaining alert afterward.
- guarding at guard position (`0x9`): Randomly move between guard firing positions, using running animations.
- searching (`0xA`): Randomly move between search firing positions, using searching walking animations.
- fleeing (`0xB`): Randomly move between firing positions, using fleeing animations.
- return state (`enum`?): The squad's or unit's behaviour when it has been alerted but has since completely lost track of any living enemies.
- flags (`bitfield`)
- unused (`0x1`)
- never search (`0x2`)
- start timer immediately (`0x4`)
- no timer delay forever (`0x8`)
- magic sight after timer (`0x10`)
- automatic migration (`0x20`)
- unique leader type (`enum`)
- normal (`0x0`)
- none (`0x1`)
- random (`0x2`)
- sgt johnson (`0x3`)
- sgt lehto (`0x4`)
- maneuver to squad (`uint16`): What squad to switch to when maneuvering is triggered, whether by platoon conditions or by a script.
- squad delay time (`float`)
- attacking (`bitfield`): Which firing positions to use when shooting at enemies. Actors will further discriminate between firing positions based on cover provided, proximity of allies or foes, etc.
- a (`0x1`)
- b (`0x2`)
- c (`0x4`)
- d (`0x8`)
- e (`0x10`)
- f (`0x20`)
- g (`0x40`)
- h (`0x80`)
- i (`0x100`)
- j (`0x200`)
- k (`0x400`)
- l (`0x800`)
- m (`0x1000`)
- n (`0x2000`)
- o (`0x4000`)
- p (`0x8000`)
- q (`0x10000`)
- r (`0x20000`)
- s (`0x40000`)
- t (`0x80000`)
- u (`0x100000`)
- v (`0x200000`)
- w (`0x400000`)
- x (`0x800000`)
- y (`0x1000000`)
- z (`0x2000000`)
- attacking search (`bitfield`?): Which firing positions to use when looking for a target that's broken line of sight with the squad.
- attacking guard (`bitfield`?): Which firing positions to use for the inital/return states "guarding" or "guarding at guard position".
- defending (`bitfield`?): As above, but used in the defending state.
- defending search (`bitfield`?)
- defending guard (`bitfield`?)
- pursuing (`bitfield`?): While game code _does_ reference these, it's not known how and when they're used.
- normal diff count (`uint16`): How many actors to spawn on Easy and Normal.
- insane diff count (`uint16`): How many actors to spawn on Legendary. Heroic uses the average of this and the previous value.
- major upgrade (`enum`): Along with difficulty level, influences how many actors in the squad will spawn as their major variants instead.
- normal (`0x0`)
- few (`0x1`)
- many (`0x2`)
- none (`0x3`)
- all (`0x4`)
- respawn min actors (`uint16`)
- respawn max actors (`uint16`)
- respawn total (`uint16`)
- respawn delay (`Bounds`?): * Unit: seconds
- move positions (`Block`): * HEK max count: 64 Used for actor initial and return states, eg as patrol routes.
- position (`Point3D`)
- facing (`float`)
- weight (`float`)
- time (`Bounds`?): * Unit: seconds
- animation (`uint16`→)
- sequence id (`int8`)
- cluster index (`uint16`): * Cache only
- surface index (`uint32`): * Cache only
- starting locations (`Block`): * HEK max count: 32 Squads need at least one valid starting position for units to spawn. If there are more units than valid locations, they will be reused.
- position (`Point3D`)
- facing (`float`)
- cluster index (`uint16`): * Cache only
- sequence id (`int8`)
- flags (`bitfield`)
- required (`0x1`)
- return state (`enum`?)
- initial state (`enum`?)
- actor type (`uint16`→)
- command list (`uint16`→)
| platoons | `Block` | * HEK max count: 32 |
Flag values:
- name (`TagString`)
- flags (`bitfield`)
- flee when maneuvering (`0x1`)
- say advancing when maneuver (`0x2`)
- start in defending state (`0x4`)
- change attacking defending state when (`enum`)
- Strength is the fraction of total HP across all members of the platoon. (Option): Value
- Comments (): ---
- never (`0x0`)
- 75 strength (`0x1`)
- 50 strength (`0x2`)
- 25 strength (`0x3`)
- anybody dead (`0x4`)
- 25 dead (`0x5`)
- 50 dead (`0x6`)
- 75 dead (`0x7`)
- all but one dead (`0x8`)
- all dead (`0x9`)
- happens to (`uint16`→)
- maneuver when (`enum`?)
- happens to 1 (`uint16`→)
| firing positions | `Block` | * HEK max count: 512 |
Option values:
- position (`Point3D`)
- group index (`enum`)
- a (`0x0`)
- b (`0x1`)
- c (`0x2`)
- d (`0x3`)
- e (`0x4`)
- f (`0x5`)
- g (`0x6`)
- h (`0x7`)
- i (`0x8`)
- j (`0x9`)
- k (`0xA`)
- l (`0xB`)
- m (`0xC`)
- n (`0xD`)
- o (`0xE`)
- p (`0xF`)
- q (`0x10`)
- r (`0x11`)
- s (`0x12`)
- t (`0x13`)
- u (`0x14`)
- v (`0x15`)
- w (`0x16`)
- x (`0x17`)
- y (`0x18`)
- z (`0x19`)
- cluster index (`uint16`): * Cache only
- surface index (`uint32`): * Cache only
| player starting locations | `Block`? | * HEK max count: 256 |
command lists`Block`
Sets of predefined actions for actors to take upon being spawned.

| Field | Type | Comments |
| --- | --- | --- |
| name | `TagString` |  |
| flags | `bitfield` |  |
Flag values:
- allow initiative (`0x1`)
- allow targeting (`0x2`)
- disable looking (`0x4`)
- disable communication (`0x8`)
- disable falling damage (`0x10`)
- manual bsp index (`0x20`)
| manual bsp index | `uint16`→ |  |
| precomputed bsp index | `uint16` | * Cache only |
| commands | `Block` | * HEK max count: 64 * Max: 65534 |
Option values:
- atom type (`enum`)
- pause (`0x0`)
- go to (`0x1`)
- go to and face (`0x2`)
- move in direction (`0x3`)
- look (`0x4`)
- animation mode (`0x5`)
- crouch (`0x6`)
- shoot (`0x7`)
- grenade (`0x8`)
- vehicle (`0x9`)
- running jump (`0xA`)
- targeted jump (`0xB`)
- script (`0xC`)
- animate (`0xD`)
- recording (`0xE`)
- action (`0xF`)
- vocalize (`0x10`)
- targeting (`0x11`)
- initiative (`0x12`)
- wait (`0x13`)
- loop (`0x14`)
- die (`0x15`)
- move immediate (`0x16`)
- look random (`0x17`)
- look player (`0x18`)
- look object (`0x19`)
- set radius (`0x1A`)
- teleport (`0x1B`)
- atom modifier (`int16`)
- parameter1 (`float`)
- parameter2 (`float`)
- point 1 (`uint16`→)
- point 2 (`uint16`→)
- animation (`uint16`→)
- script (`uint16`→)
- recording (`uint16`→)
- command (`uint16`→)
- object name (`uint16`→)
| points | `Block` | * HEK max count: 64 * Max: 65534 |
Field values:
- position (`Point3D`)
- surface index (`uint32`): * Cache only
ai animation references`Block`
| Field | Type | Comments |
| --- | --- | --- |
| animation name | `TagString` |  |
| animation graph | `TagDependency`: model_animations |  |
ai script references`Block`
| Field | Type | Comments |
| --- | --- | --- |
| script name | `TagString` |  |
ai recording references`Block`
| Field | Type | Comments |
| --- | --- | --- |
| recording name | `TagString` |  |
ai conversations`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagString` |  |
| flags | `bitfield` |  |
Flag values:
- stop if death (`0x1`)
- stop if damaged (`0x2`)
- stop if visible enemy (`0x4`)
- stop if alerted to enemy (`0x8`)
- player must be visible (`0x10`)
- stop other actions (`0x20`)
- keep trying to play (`0x40`)
- player must be looking (`0x80`)
| trigger distance | `float` | * Unit: seconds |
| run to player dist | `float` | * Unit: seconds |
| participants | `Block` | * HEK max count: 8 |
Option values:
- flags (`bitfield`)
- optional (`0x1`)
- has alternate (`0x2`)
- is alternate (`0x4`)
- selection type (`enum`)
- friendly actor (`0x0`)
- disembodied (`0x1`)
- in player s vehicle (`0x2`)
- not in a vehicle (`0x3`)
- prefer sergeant (`0x4`)
- any actor (`0x5`)
- radio unit (`0x6`)
- radio sergeant (`0x7`)
- actor type (`enum`)
- elite (`0x0`): Uses the elite actor type definition.
- jackal (`0x1`): Uses the jackal actor type definition.
- grunt (`0x2`): Uses the grunt actor type definition.
- hunter (`0x3`): Uses the hunter actor type definition.
- engineer (`0x4`): Uses the engineer actor type definition.
- assassin (`0x5`): Uses the elite actor type definition (duplicate).
- player (`0x6`): Uses the marine actor type definition (duplicate).
- marine (`0x7`): Uses the marine actor type definition.
- crew (`0x8`): Uses the crew actor type definition.
- combat form (`0x9`): Uses the flood actor type definition.
- infection form (`0xA`): Uses the infection actor type definition.
- carrier form (`0xB`): Uses the flood carrier actor type definition.
- monitor (`0xC`): Uses the sentinel actor type definition (duplicate).
- sentinel (`0xD`): Uses the sentinel actor type definition.
- none (`0xE`): Uses the grunt actor type definition.
- mounted weapon (`0xF`): Uses the mounted_ weapon actor type definition.
- use this object (`uint16`→)
- set new name (`uint16`→)
- variant numbers (`uint16[6]`): * Cache only Determines which kind of variant gets a given line. By default, this matches by tag path (bisenti = 2, fitzgerald/jenkins = 4, aussie = 5, mendoza = 6, sarge/johnson = 100, sarge2/lehto = 101, everything else = 0, null = 65535)
- encounter name (`TagString`)
- encounter index (`uint32`): * Cache only
| lines | `Block` | * HEK max count: 32 |
Option values:
- flags (`bitfield`)
- addressee look at speaker (`0x1`)
- everyone look at speaker (`0x2`)
- everyone look at addressee (`0x4`)
- wait after until told to advance (`0x8`)
- wait until speaker nearby (`0x10`)
- wait until everyone nearby (`0x20`)
- participant (`uint16`→)
- addressee (`enum`→)
- none (`0x0`)
- player (`0x1`)
- participant (`0x2`)
- addressee participant (`uint16`→)
- line delay time (`float`)
- variant 1 (`TagDependency`: sound)
- variant 2 (`TagDependency`: sound)
- variant 3 (`TagDependency`: sound)
- variant 4 (`TagDependency`: sound)
- variant 5 (`TagDependency`: sound)
- variant 6 (`TagDependency`: sound)
script syntax data`TagDataOffset`?Stores the compiled syntax node data for scripts.
script string data`TagDataOffset`?Stores strings used by scripts.
scripts`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagString` |  |
| script type | `enum` |  |
Option values:
- startup (`0x0`)
- dormant (`0x1`)
- continuous (`0x2`)
- static (`0x3`)
- stub (`0x4`)
| return type | `enum` |  |
Option values:
- unparsed (`0x0`)
- special form (`0x1`)
- function name (`0x2`)
- passthrough (`0x3`)
- void (`0x4`)
- boolean (`0x5`)
- real (`0x6`)
- short (`0x7`)
- long (`0x8`)
- string (`0x9`)
- script (`0xA`)
- trigger volume (`0xB`)
- cutscene flag (`0xC`)
- cutscene camera point (`0xD`)
- cutscene title (`0xE`)
- cutscene recording (`0xF`)
- device group (`0x10`)
- ai (`0x11`)
- ai command list (`0x12`)
- starting profile (`0x13`)
- conversation (`0x14`)
- navpoint (`0x15`)
- hud message (`0x16`)
- object list (`0x17`)
- sound (`0x18`)
- effect (`0x19`)
- damage (`0x1A`)
- looping sound (`0x1B`)
- animation graph (`0x1C`)
- actor variant (`0x1D`)
- damage effect (`0x1E`)
- object definition (`0x1F`)
- game difficulty (`0x20`)
- team (`0x21`)
- ai default state (`0x22`)
- actor type (`0x23`)
- hud corner (`0x24`)
- object (`0x25`)
- unit (`0x26`)
- vehicle (`0x27`)
- weapon (`0x28`)
- device (`0x29`)
- scenery (`0x2A`)
- object name (`0x2B`)
- unit name (`0x2C`)
- vehicle name (`0x2D`)
- weapon name (`0x2E`)
- device name (`0x2F`)
- scenery name (`0x30`)
| root expression index | `int32` |  |
globals`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagString` |  |
| type | `enum`? |  |
| initialization expression index | `int32` |  |
references`Block`Stores tag references used by compiled level scripts. In source form, scripts use tag paths but for runtime those paths are compiled to references here.
| Field | Type | Comments |
| --- | --- | --- |
| reference | `TagDependency`: (any) |  |
source files`Block`
| Field | Type | Comments |
| --- | --- | --- |
| name | `TagString` |  |
| source | `TagDataOffset`? |  |
cutscene flags`Block`
| Field | Type | Comments |
| --- | --- | --- |
| unknown | `uint32` | * Cache only |
| name | `TagString` |  |
| position | `Point3D` |  |
| facing | `Euler2D` |  |
Field values:
- yaw (`float`): Rotation to the left or right around the Z (vertical) axis.
- pitch (`float`): Rotation up or down.
cutscene camera points`Block`
| Field | Type | Comments |
| --- | --- | --- |
| unknown | `uint32` | * Cache only |
| name | `TagString` |  |
| position | `Point3D` |  |
| orientation | `Euler3D`? |  |
| field of view | `float` |  |
cutscene titles`Block`
| Field | Type | Comments |
| --- | --- | --- |
| unknown | `uint32` | * Cache only |
| name | `TagString` |  |
| text bounds | `Rectangle2D` |  |
Field values:
- top (`int16`)
- left (`int16`)
- bottom (`int16`)
- right (`int16`)
| string index | `uint16` |  |
| text style | `enum` | Sets the title text style. This field is hidden in HEK Guerilla, but visible in H1A Guerilla. |
Option values:
- plain (`0x0`)
- bold (`0x1`)
- italic (`0x2`)
- condense (`0x3`)
- underline (`0x4`)
| justification | `enum` |  |
Option values:
- left (`0x0`)
- right (`0x1`)
- center (`0x2`)
| text flags | `bitfield` | Positioning flags for the title. This field is hidden in HEK Guerilla, but visible in H1A Guerilla. |
Flag values:
- wrap horizontally (`0x1`)
- wrap vertically (`0x2`)
- center vertically (`0x4`)
- bottom justify (`0x8`)
| text color | `ColorARGBInt` |  |
| RGB Color with alpha, with 8-bit color depth per channel (0-255) | Field | Type | Comments | | --- | --- | --- | | alpha | `uint8` | | | red | `uint8` | | | green | `uint8` | | | blue | `uint8` | | |
| shadow color | `ColorARGBInt`? |  |
| fade in time | `float` |  |
| up time | `float` |  |
| fade out time | `float` |  |
custom object names`TagDependency`: unicode_string_list
ingame help text`TagDependency`: unicode_string_list
hud messages`TagDependency`: hud_message_text
structure bsps`Block`The list of BSPs belonging to this scenario that can be switched between. Multiplayer scenarios typically have just one, while singleplayer scenarios often have multiple. This block also contains information used during map loading.
| Field | Type | Comments |
| --- | --- | --- |
| bsp start | `uint32` | * Cache only |
| bsp size | `uint32` | * Cache only |
| bsp address | `uint32` | * Cache only |
| structure bsp | `TagDependency`: scenario_structure_bsp |  |

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `scenario`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
