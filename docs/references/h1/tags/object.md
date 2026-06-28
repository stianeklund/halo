# Halo 1 Tag: object

## Summary

Some capabilities available to objects (though not used by every subtype) are:

## Tag Information

Tag group: `object`.

HaloScript entries:

- `debug_object_lights`
- `debug_objects [boolean]`
- `debug_objects_biped_autoaim_pills [boolean]`
- `debug_objects_biped_physics_pills [boolean]`
- `debug_objects_bounding_spheres [boolean]`
- `debug_objects_collision_models [boolean]`
- `debug_objects_names [boolean]`
- `debug_objects_pathfinding_spheres [boolean]`
- `debug_objects_physics`
- `debug_objects_position_velocity`
- `debug_objects_root_node`
- `debug_objects_unit_seats`
- `debug_objects_unit_vectors`
- `object_light_ambient_base`
- `object_light_ambient_scale`
- `object_light_interpolate`
- `object_light_secondary_scale`
- `rasterizer_environment_shadows [boolean]`
- `render_shadows [boolean]`

Defined fields and enum values mentioned:

- a in
- a in alive
- a in body vitality
- a in compass
- a in none
- a in random constant
- a in recent body damage
- a in recent shield damage
- a in recent umbrella shield vitality
- a in region
- a in region 1
- a in region 2
- a in region 3
- a in region 4
- a in region 5
- a in region 6
- a in region 7
- a in shield stun
- a in shield vitality
- a in umbrella shield stun
- a in umbrella shield vitality
- acceleration scale
- animation graph
- attachment 1
- attachments
- attachments change color
- attachments change color a
- attachments change color b
- attachments change color c
- attachments change color d
- attachments change color none
- attachments marker
- attachments primary scale
- attachments primary scale a out
- attachments primary scale b out
- attachments primary scale c out
- attachments primary scale d out
- attachments primary scale none
- attachments secondary scale
- attachments type
- b in
- bounding offset
- bounding radius
- c in
- change colors
- change colors color lower bound
- change colors color upper bound
- change colors darken by
- change colors flags
- change colors flags blend in hsv
- change colors flags more colors
- change colors permutations
- change colors permutations color lower bound
- change colors permutations color upper bound
- change colors permutations weight
- change colors scale by
- collision model
- creation effect
- d in
- default distant light 0 color
- device a in
- flags
- flags brighter than it should be
- flags cast shadow by default
- flags does not cast shadow
- flags does not have remastered geometry
- flags extension of parent
- flags not a pathfinding obstacle
- flags transparent self occlusion
- forced shader permutation index
- framebuffer fade source
- functions
- functions add
- functions bounds
- functions bounds max
- functions bounds min
- functions bounds mode
- functions bounds mode clip
- functions bounds mode clip and normalize
- functions bounds mode scale to fit

## Details

**Objects** are a high-level abstract tag, meaning they serve as a base for many other tag types but cannot be directly created themselves. Generally, they are "things" with a position in the world but are distinct from the "level" itself. Some examples include elevators, trees, warthogs, and the player.

Some capabilities available to objects (though not used by every subtype) are:

* Be rendered with a model
* Have physics and be collideable
* Cast shadows using lightmap data
* Have attachments like particles, sounds, and lights
* Be attached to each other (e.g. pelicans carrying warthogs)

### Functions

_Functions_ are values of objects that can change over time and/or react to external stimulus. They can be combined in complex ways and used to drive dynamic changes across various aspects of the object, like its appearance, sound, and attachments. Some examples are Jackal shields changing colour in reaction to damage, Warthog headlights turning on when you enter the vehicle, Banshee wingtips emitting contrails when banking, and the Assault Rifle's compass pointing north.

Objects can receive up to 4 sources of input from the engine called _A in_, _B in_, _C in_, and _D in_. These can be used as inputs by your _functions_ or _change colors_. Every type of object has access to sources from the _export to functions_ section. However, certain types of objects can override these base sources with additional sources related to that object type. To name a few, devices have access to position and power") while vehicles have access to speed and slide.

The next relevant section is the _functions_ block. This is where you can define functions and how the change over time, and if they use any of the _export to functions_ inputs. Each function produces a simple numeric value over time, typically between `0` and `1`. This could be as simple as an oscillating sine wave or as complex as a noise value scaled by recent damage and clipped to a certain range. You can create up to 4 functions.

The outputs of functions can be used by a variety of other tags related to the object, including but not limited to:

* Set the scales of attachments like lights or sounds.
* Affect values in widgets like glows.
* Scale or darken _change colors_.
* Affect the object's shaders in various ways, like fade or texture animation.

### Shadows and lighting

For most dynamic objects, Halo uses shadow mapping with their render model, unless the object's "does not cast shadow" flag is true or `render_shadows` is disabled. Scenery shadows are baked into the lightmap using the object's collision geometry instead, regardless of the "does not cast shadow" flag. Static objects can be forced to use shadow mapping if the _cast shadow by default_ flag is set.

Objects receive a few parameters from the environment as inputs to their lighting model. These include up to two distant light sources (direction and colour), ambient light, reflection tint, shadow direction, and shadow colour. Lighting is calculated when objects are created and also when dynamic objects move. To do this the game casts a ray straight down to a BSP _ground point_ up to 10 world units away (device_ machine can optionally sample nearby surfaces instead) to determine its lighting and can result in a few scenarios:

1. If a ground point is found within 10 units, that surface's lightmap colour is used to light the object. This is why even an object in direct sunlight can appear dark when it is sampling the ground _beneath_ it. The shadow vector is an interpolation of lightmap vertex normals. In this scenario you will see a coloured vector drawn over objects with `debug_object_lights 1`.
2. If a ground point is not found and the BSP tag has a non-zero red value for _default ambient color_ field, BSP default lighting fields will apply.
3. Otherwise the object receives hard-coded white light from the +x +y direction and casts shadows straight down.

You can test the latter two scenarios by setting `debug_collision_skip_vectors 1` to make the ray cast always fail. Objects which are outside the BSP will fail to find a valid ground point and therefore receive default lighting. Objects above phantom BSP and nearly coplanar faces may similarly fail.

BSP switches do not cause fixed objects to resample lighting.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_objects [boolean])` When enabled, toggles if debug information is visible on objects (such as bounding sphere and collision models). Individual debug features can be toggled with the `debug_objects_*` commands. In H1A this now has `debug_objects_root_node` enabled by default; turn it off if you don't want the orange text. | Global |
|  | `(debug_objects_biped_autoaim_pills [boolean])` When `debug_objects` is enabled, displays biped autoaim pills in red. | Global |
|  | `(debug_objects_biped_physics_pills [boolean])` When `debug_objects` is enabled, displays biped physics pills in white. | Global |
|  | `(debug_objects_bounding_spheres [boolean])` When `debug_objects` is enabled, displays yellow spheres for object bounding radius"). The sphere will be black when the object is _inactive_. This setting defaults to `true` in HEK Sapien but not H1A Sapien. | Global |
|  | `(debug_objects_collision_models [boolean])` When `debug_objects` is enabled, displays green meshes for object collision models. This setting defaults to `true` in HEK Sapien but not H1A Sapien. | Global |
|  | `(debug_objects_names [boolean])` Toggles the display of object names, for named objects. The names are shown in purple. | Global |
|  | `(debug_objects_pathfinding_spheres [boolean])` When `debug_objects` is enabled, displays object pathfinding spheres in blue. | Global |
|  | `(debug_objects_physics)` When `debug_objects` is enabled, displays physics mass points as white spheres. | Global |
|  | `(debug_objects_position_velocity)` When `debug_objects` is enabled, displays red, green, and blue object-space reference axes and a yellow velocity vector on each object. | Global |
|  | `(debug_objects_root_node)` When `debug_objects` is enabled, displays red, green, and blue object-space reference axes and orange text including object ID, class, and tag name on each object. Defaults to `true` in H1A. | Global |
|  | `(debug_objects_unit_seats)` When `debug_objects` is enabled, displays blue markers at unit seat locations, red markers at their entry points, and a yellow marker at the object origin. | Global |
|  | `(debug_objects_unit_vectors)` When `debug_objects` is enabled, displays white and red vectors on objects. Their meaning is unknown. | Global |
|  | `(debug_object_lights)` Shows the incoming light colour and vector for all objects which results from sampling lightmap data at the ground point beneath the object. This data is used to shade the object and cast its shadow. | Global |
|  | `(object_light_ambient_base)` Sets the amount of ambient light all objects receive. Note that this only affects moving objects when their lighting updates, so you will not see any change on scenery. Setting this to `1` makes objects fullbright. Defaults to `0.03`. | Global |
|  | `(object_light_ambient_scale)` Scales ambient light from the lightmap. Defaults to `0.4`. | Global |
|  | `(object_light_interpolate)` Toggles if object lighting transitions smoothly when the object moves between different ground point surfaces or default lighting. | Global |
|  | `(object_light_secondary_scale)` Scales secondary light on objects, but doesn't apply to default lighting. Defaults to `1`. | Global |
|  | `(rasterizer_environment_shadows [boolean])` Toggles dynamic shadow mapping") for objects. Same effect as `render_shadows`. | Global |
|  | `(render_shadows [boolean])` Toggles the display of dynamic object shadows. Same effect as `rasterizer_environment_shadows`. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| object type | `enum` | * Volatile * Hidden |
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
| flags | `bitfield` |  |
Flag values:
- does not cast shadow (`0x1`): Prevents this object from casting dynamic shadows. Does not apply to scenery, which cast shadows in lightmaps using their collision model.
- transparent self occlusion (`0x2`)
- brighter than it should be (`0x4`): Causes the object to have extra ambient lighting so it's easier to see in dark spaces. This is used for items which are important to gameplay like weapons and powerups.
- not a pathfinding obstacle (`0x8`)
- extension of parent (`0x10`): * H1A only * Unused Planned H2 backport, currently unused. In H2 this is documented as "passes all function values to parent and uses parent's markers".
- cast shadow by default (`0x20`): * H1A only Forces static objects like scenery to have dynamic shadows, unless _does not cast shadow_ is also set. This flag does not prevent such objects from also casting lightmapped shadows, so it is best used for objects which have been added to a map after baking like scenery which is scripted to appear.
- does not have remastered geometry (`0x40`): * H1A only The object does not appear in anniversary/remastered graphics mode. Used for campaign moas.
| bounding radius | `float` | * Unit: world units The bounding radius sets the approximate maximum size of the object and is used for various optimizations and gameplay purposes, including but likely not limited to: * Culling objects for rendering which are not partially on screen or visible through portals. * Setting the shadow map size for dynamic object shadows"). * Selecting an appropriate LOD. * Skipping collision checks. * Skipping simulation of antenna and flag widgets on the object when off-screen. * Determining if spawn points are blocked by an object. * The interaction radius of device_ control. * The default activation radius of automatic door device_ machine if unset. * Default pathfinding spheres are half the object's bounding radius. The radius should contain this object completely, but not be too large or else the object will draw unccessarily when off-screen, block spawn points, or prevent AI from pathfinding effectively. Setting it too low will cause the object to have clipped shadows, pop in and out of visibility at the edges of the screen, and have inconsistent collision with other objects. The bounding radius can be visualized in Sapien or Standalone using debug_ objects_ bounding_ spheres. |
| bounding offset | `Point3D` |  |
| origin offset | `Point3D` |  |
| acceleration scale | `float` | * Min: 0 Scales the acceleration of this object from forces like explosions, such as when weapons or powerups are moved by grenades. This does not apply to non-moving object types. If the scale is `0` then the item will not react. |
| scales change colors | `uint32` | * Cache only |
| model | `TagDependency` * gbxmodel * model |  |
| animation graph | `TagDependency`: model_animations |  |
| collision model | `TagDependency`: model_collision_geometry |  |
| physics | `TagDependency`: physics |  |
| modifier shader | `TagDependency`: shader |  |
| creation effect | `TagDependency`: effect |  |
| render bounding radius | `float` | * Unit: world units |
| a in | `enum` |  |
Option values:
- none (`0x0`)
- body vitality (`0x1`)
- shield vitality (`0x2`)
- recent body damage (`0x3`)
- recent shield damage (`0x4`)
- random constant (`0x5`)
- umbrella shield vitality (`0x6`)
- shield stun (`0x7`)
- recent umbrella shield vitality (`0x8`)
- umbrella shield stun (`0x9`)
- region (`0xA`)
- region 1 (`0xB`)
- region 2 (`0xC`)
- region 3 (`0xD`)
- region 4 (`0xE`)
- region 5 (`0xF`)
- region 6 (`0x10`)
- region 7 (`0x11`)
- alive (`0x12`)
- compass (`0x13`)
| b in | `enum`? |  |
| c in | `enum`? |  |
| d in | `enum`? |  |
| hud text message index | `uint16` |  |
| forced shader permutation index | `uint16` |  |
| attachments | `Block` | * HEK max count: 8 |
Option values:
- type (`TagDependency` (6) * light * light_volume * contrail * particle_system * effect * sound_looping)
- marker (`TagString`)
- primary scale (`enum`)
- none (`0x0`)
- a out (`0x1`)
- b out (`0x2`)
- c out (`0x3`)
- d out (`0x4`)
- secondary scale (`enum`?)
- change color (`enum`)
- none (`0x0`)
- a (`0x1`)
- b (`0x2`)
- c (`0x3`)
- d (`0x4`)
| widgets | `Block` | * HEK max count: 4 |
Field values:
- reference (`TagDependency` (5) * antenna * glow * light_volume * lightning * flag)
| functions | `Block` | * HEK max count: 4 * Max: 4 |
Field values:
- flags (`bitfield`)
- invert (`0x1`)
- additive (`0x2`)
- always active (`0x4`)
- period (`float`): * Unit: seconds * Default: 1
- scale period by (`enum`)
- none (`0x0`)
- a in (`0x1`)
- b in (`0x2`)
- c in (`0x3`)
- d in (`0x4`)
- a out (`0x5`)
- b out (`0x6`)
- c out (`0x7`)
- d out (`0x8`)
- function (`enum`)
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
- scale function by (`enum`?)
- wobble function (`enum`?)
- wobble period (`float`): * Unit: seconds
- wobble magnitude (`float`): * Unit: percent
- square wave threshold (`float`)
- step count (`uint16`)
- map to (`enum`)
- linear (`0x0`)
- early (`0x1`)
- very early (`0x2`)
- late (`0x3`)
- very late (`0x4`)
- cosine (`0x5`)
- sawtooth count (`uint16`)
- add (`enum`?)
- scale result by (`enum`?)
- bounds mode (`enum`)
- clip (`0x0`)
- clip and normalize (`0x1`)
- scale to fit (`0x2`)
- bounds (`Bounds`): * Default: 0,1
- min (`float`)
- max (`float`)
- turn off with (`int16`)
- scale by (`float`)
- inverse bounds (`float`): * Cache only
- inverse sawtooth (`float`): * Cache only
- inverse step (`float`): * Cache only
- inverse period (`float`): * Cache only
- usage (`TagString`)
| change colors | `Block` | * HEK max count: 4 * Max: 4 |
Field values:
- darken by (`enum`?)
- scale by (`enum`?)
- flags (`bitfield`)
- blend in hsv (`0x1`)
- more colors (`0x2`)
- color lower bound (`ColorRGB`)
- color upper bound (`ColorRGB`)
- permutations (`Block`): * HEK max count: 8
- weight (`float`): * Default: 1
- color lower bound (`ColorRGB`)
- color upper bound (`ColorRGB`)
| predicted resources | `Block` | * HEK max count: 1024 * Cache only |
Option values:
- type (`enum`)
- bitmap (`0x0`)
- sound (`0x1`)
- resource index (`uint16`)
- tag (`uint32`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `object`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
