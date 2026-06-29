# Halo 1 Tag: gbxmodel

## Summary

The Gearbox model tag contains the marker points and render models for objects") such as vehicles, scenery, and weapons among others. It is not collideable nor animated on its own, and objects may reference additional model_ collision_ geometry and model_ animations tags. This tag supports LODs and permutations, and includes shader_ model references.

## Tag Information

Tag group: `gbxmodel`.

HaloScript entries:

- `rasterizer_model_transparents [boolean]`
- `rasterizer_models [boolean]`
- `render_model_index_counts [boolean]`
- `render_model_markers [boolean]`
- `render_model_nodes [boolean]`
- `render_model_vertex_counts [boolean]`

Defined fields and enum values mentioned:

- base map u scale
- base map v scale
- blur speed
- bounding radius
- effect
- flags
- flags blend shared normals
- flags ignore skinning
- flags parts have local nodes
- geometries
- geometries flags
- geometries flags unused
- geometries parts
- geometries parts bullshit
- geometries parts centroid
- geometries parts centroid primary node
- geometries parts centroid primary weight
- geometries parts centroid secondary node
- geometries parts centroid secondary weight
- geometries parts compressed vertices
- geometries parts compressed vertices binormal
- geometries parts compressed vertices node0 index
- geometries parts compressed vertices node0 weight
- geometries parts compressed vertices node1 index
- geometries parts compressed vertices normal
- geometries parts compressed vertices position
- geometries parts compressed vertices tangent
- geometries parts compressed vertices texture coordinate u
- geometries parts compressed vertices texture coordinate v
- geometries parts do not crash the game
- geometries parts do not screw up the model
- geometries parts flags
- geometries parts flags stripped internal
- geometries parts flags zoner
- geometries parts local node count
- geometries parts local node indices
- geometries parts next filthy part index
- geometries parts prev filthy part index
- geometries parts shader index
- geometries parts triangle count
- geometries parts triangle offset
- geometries parts triangle offset 2
- geometries parts triangles
- geometries parts triangles vertex0 index
- geometries parts triangles vertex1 index
- geometries parts triangles vertex2 index
- geometries parts uncompressed vertices
- geometries parts uncompressed vertices binormal
- geometries parts uncompressed vertices node0 index
- geometries parts uncompressed vertices node0 weight
- geometries parts uncompressed vertices node1 index
- geometries parts uncompressed vertices node1 weight
- geometries parts uncompressed vertices normal
- geometries parts uncompressed vertices position
- geometries parts uncompressed vertices tangent
- geometries parts uncompressed vertices texture coords
- geometries parts uncompressed vertices texture coords x
- geometries parts uncompressed vertices texture coords y
- geometries parts vertex count
- geometries parts vertex offset
- high detail cutoff
- high detail node count
- low detail cutoff
- low detail node count
- maps map u scale
- markers
- markers instances
- markers instances node index
- markers instances permutation index
- markers instances region index
- markers instances rotation
- markers instances translation
- markers magic identifier
- markers name
- medium detail cutoff
- medium detail node count
- node list checksum
- nodes
- nodes default rotation
- nodes default translation

## Details

The Gearbox model tag contains the marker points and render models for objects") such as vehicles, scenery, and weapons among others. It is not collideable nor animated on its own, and objects may reference additional model_ collision_ geometry and model_ animations tags. This tag supports LODs and permutations, and includes shader_ model references.

Don't confuse this tag with the Xbox-only model. Gearbox Software created this tag class for the PC port, and it is therefore used in all derivatives of that port, like Mac, Demo, and MCC. Unlike model, this tag uses uncompressed vertices.

### Shaders

Each part of a model can reference a different shader"), like the Warthog's windscreen using a shader_ transparent_ glass while its body uses a shader_ model. While a model can _technically_ reference any kind of shader, referencing a shader_ environment is **not recommended** when targeting H1CE because it renders incorrectly in atmospheric fog.

### Nodes

Nodes can be thought of as the model's "skeleton" and can be animated to move parts of the model. Each vertex can be influenced by up to 2 nodes. H1A and H1CE 1.10 allow up to 63 model nodes. Older versions of H1CE and H1X allow 48.

### Markers

Markers are simple named points with orientation attached to a model. Since they are parented by nodes, they can be animated. Markers can be used for a variety of purposes, such as attaching objects together with scripts (e.g. Pelicans carrying Warthogs), attaching widgets like antenna, or firing projectiles from in the case of weapons. You can view object markers with `render_model_markers 1`.

This tag only contains the marker data but other tags usually determine how they are used. However, certain marker names have special behaviour in-engine:

* `head`:
 * Determines where AI look at when scripted to talk to another character.
 * Base location for the friendly indicator in multiplayer.
 * Used as a ray origin when testing if AI can see their enemy.

* `primary trigger`: Where a weapon's primary trigger projectiles and effects come from. See also the _projectiles use weapon origin_ field.
* `secondary trigger`: As above, for secondary triggers (second trigger slot).
* `body`
* `front`: Possibly used to used to see if you're facing a device_ control, if present.
* `ground point`: Determines the resting point for items").
* `left hand`: Used during the grenade throwing animation.
* `melee`: Where melee damage comes from here. If not present, the engine picks an unknown default location.
* `hover thrusters`:
 * When used on a vehicle with "alien scout" or "alien fighter" vehicle physics type, spawns an effect when the vehicle is hovering close to the ground. This can be seen at a piloted Banshee's wingtips when sitting on the ground.
 * When the vehicle physics type is "human plane", creates a similar dust effect if the marker is pointed at nearby ground. Used for the Pelican's thrusters.

* `jet thrusters`: Can also be used for vehicles with "human plane" physics to create the Pelican's thruster dust effect.

Commonly used marker names without hard-coded behaviour include:

* `primary ejection`: Used to indicate where casings fly out when firing the primary trigger.

Tool only includes markers from the `superhigh` LOD.

### Regions

Regions are named sections of the model which can have multiple permutations. Region names are used by the engine to relate parts of the render model with the collision model. For example, a Flood combat form losing an arm. Some regions have special behaviour in-engine:

* `head`: Sets headshot areas for damage_ effect.

Regions render in the order they are stored in the tag. When naming regions, consider that they will be sorted by name when compiled into the `.gbxmodel`. This can be important for skyboxes and objects with multiple layers of alpha-blended transparent shaders which aren't z-culled and need a correct sorting order to be explicitly defined, assuming the object is viewed mostly from one direction.

### Permutations

A permutation is a variation of a region that can be randomly selected. They are often used to give bipeds visual variety. Some permutations have special behaviour in-engine:

* `~blur`: Switched to depending on weapon rate of fire and vehicle speed to fake motion blur. Used for the Warthog tires and chaingun when spinning fast enough.
* `~damaged`: Switched to depending how much damage the object takes based on Damage Threshold

Permutations can also be set via script or the Desired Permutation field when placing objects in a Scenario. In order to use the Desired Permutation field the model's permutations must be named in a specific way: `Permutation_name-###`

Randomly selected permutations are not network synchronized.

### Level of detail

Low quality LODs shown for the Chief biped and Warthog. Note the reduced geometric detail.

Models can contain multiple levels of detail (LODs), ranging from simplified meshes with reduced shader count to high detail meshes with numerous complex shaders. The game will select a LOD based on the on-screen diameter of the object's bounding sphere") in pixels and this tag's LOD cutoffs. Objects which are very distant or small don't need a lot of geometric detail, so they can be rendered using low quality LODs to keep the framerate high in busy scenes.

Halo CE supports 5 LODs. From best to worst quality:

* super high
* high
* medium
* low
* super low

When rendering first person models, Halo always uses the lowest quality LOD instead of the highest. When creating FP arms or weapons create a separate FP model from your 3P model which only includes a single super high LOD.

LODs are created by using a special naming convention when compiling models with Tool.

LODs are currently not behaving as expected in Halo CE, both in tools and in the Master Chief Collection. This can cause only the super high LOD to be visible at all times, no matter the on-screen diameter of an object's bounding sphere").

|  | Function/global | Type |
| --- | --- | --- |
|  | `(rasterizer_models [boolean])` Toggles the rendering of all models. When disabled, all objects like scenery, units, projectiles, and even the skybox and FP arms will become invisible. The BSP and effects like particles and decals are still visible. | Global |
|  | `(rasterizer_model_transparents [boolean])` Toggles the rendering of transparent shaders in models. For example, the Warthog's windshield. | Global |
|  | `(render_model_index_counts [boolean])` If `true`, displays a red number above each object with its model index count. If `render_model_vertex_counts` is also enabled, the vertex count and index count are separated by a slash like "<vertices>/<indices>". | Global |
|  | `(render_model_markers [boolean])` If enabled, all model markers will be rendered in 3D with their name and rotation axis. | Global |
|  | `(render_model_nodes [boolean])` If enabled, all model skeletons will be rendered. Nodes are shown as axis gizmos and connected to their parents by white lines. | Global |
|  | `(render_model_vertex_counts [boolean])` If `true`, displays a red number above each object with its model vertex count. If `render_model_index_counts` is also enabled, the vertex count and index count are separated by a slash like "<vertices>/<indices>". | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- blend shared normals (`0x1`): On map compilation, vertices with the same positions have their normals linearly averaged.
- parts have local nodes (`0x2`)
- ignore skinning (`0x4`)
| node list checksum | `int32` |  |
| super high detail cutoff | `float` | * Unit: pixels |
| high detail cutoff | `float` | * Unit: pixels |
| medium detail cutoff | `float` | * Unit: pixels |
| low detail cutoff | `float` | * Unit: pixels |
| super low detail cutoff | `float` | * Unit: pixels |
| super low detail node count | `uint16` | * Cache only * Unit: nodes * Unused |
| low detail node count | `uint16` | * Cache only * Unit: nodes * Unused |
| medium detail node count | `uint16` | * Cache only * Unit: nodes * Unused |
| high detail node count | `uint16` | * Cache only * Unit: nodes * Unused |
| super high detail node count | `uint16` | * Cache only * Unit: nodes * Unused |
| base map u scale | `float` | * Default: 1 UV map scale is derived upon model import as a box that encompasses all the vertices in UV space, where 1 unit is 1 tile of the texture. The U scale will be set by the length of the box on the U direction. For example, if the furthest point in the U coordinate is 1.5 texture tiles away from the center, this value will become 1.5. The same thing happens for the V scale. If new geometry parts are added and the model rebuilt, and the vertices are mapped in UV space inside the space already mapped by the existing geometry, the scale in the gbxmodel will not change. If they are further from the origin, the scale will increase. Using tools such as Mozzarilla to import/export geometry in the tag directly will only look correct if the source and destination UV map scale values are the same. Note that shader_ transparent_ chicago has its own UV scale. Values of `0` will default to `1`. |
| base map v scale | `float` | * Default: 1 See _base map u scale_. |
| markers | `Block` | * HEK max count: 256 * Hidden Generated in postprocessing from the markers in the model's permutations. |
Field values:
- name (`TagString`)
- magic identifier (`int16`)
- instances (`Block`): * HEK max count: 32
- * Read-only (Field): Type
- Comments (): ---
- region index (`uint8`)
- permutation index (`uint8`)
- node index (`uint8`)
- translation (`Point3D`)
- rotation (`Quaternion`)
| nodes | `Block` | * HEK max count: 64 * Max: 255 |
Field values:
- name (`TagString`)
- next sibling node index (`uint16`→)
- first child node index (`uint16`→)
- parent node index (`uint16`→)
- default translation (`Point3D`)
- default rotation (`Quaternion`)
- node distance from parent (`float`)
- scale (`float`): * Cache only
- rotation (`Matrix`): * Cache only
- elements (`float[9]`)
- translation (`Point3D`): * Cache only
| regions | `Block` | * HEK max count: 32 * Max: 255 |
Flag values:
- name (`TagString`)
- permutations (`Block`): * HEK max count: 32 * Max: 255
- * Read-only * Processed during compile (Field): Type
- Comments (): ---
- name (`TagString`)
- flags (`bitfield`)
- cannot be chosen randomly (`0x1`)
- permutation number (`uint16`): * Cache only
- super low (`uint16`→)
- low (`uint16`→)
- medium (`uint16`→)
- high (`uint16`→)
- super high (`uint16`→)
- markers (`Block`): * HEK max count: 64
- * Read-only (Field): Type
- Comments (): ---
- name (`TagString`)
- node index (`uint16`→)
- rotation (`Quaternion`)
- translation (`Point3D`)
| geometries | `Block` | * HEK max count: 256 * Max: 65535 |
Field values:
- flags (`bitfield`)
- unused (`0x1`)
- parts (`Block`): * HEK max count: 32
- * Read-only * Processed during compile (Field): Type
- Comments (): ---
- flags (`bitfield`)
- stripped internal (`0x1`)
- zoner (`0x2`)
- shader index (`uint16`→)
- prev filthy part index (`uint8`): * Default: 255 Allows the rendering order of parts to be manually specified for correct transparency sorting. In HEK Tool, this defaults to `-1` (unused) if set to `0`. In H1A Tool, this defaulting only occurs when _both_ filthy part indices are `0`, since `0` should be a useable index.
- next filthy part index (`uint8`): * Default: 255 Sets the next part to render.
- centroid primary node (`uint16`): * Cache only
- centroid secondary node (`uint16`): * Cache only
- centroid primary weight (`float`): * Cache only
- centroid secondary weight (`float`): * Cache only
- centroid (`Point3D`)
- uncompressed vertices (`Block`): * HEK max count: 65535 * Non-cached
- * Read-only (Field): Type
- Comments (): ---
- position (`Point3D`)
- normal (`Vector3D`)
- binormal (`Vector3D`)
- tangent (`Vector3D`)
- texture coords (`Point2D`)
- x (`float`)
- y (`float`)
- node0 index (`uint16`)
- node1 index (`uint16`)
- node0 weight (`float`)
- node1 weight (`float`)
- compressed vertices (`Block`): * HEK max count: 65535 * Non-cached
- * Read-only (Field): Type
- Comments (): ---
- position (`Point3D`)
- normal (`uint32`)
- binormal (`uint32`)
- tangent (`uint32`)
- texture coordinate u (`int16`)
- texture coordinate v (`int16`)
- node0 index (`int8`)
- node1 index (`int8`)
- node0 weight (`uint16`)
- triangles (`Block`): * HEK max count: 65535 * Non-cached
- * Read-only (Field): Type
- Comments (): ---
- vertex0 index (`uint16`)
- vertex1 index (`uint16`)
- vertex2 index (`uint16`)
- do not crash the game (`uint32`): * Cache only
- triangle count (`uint32`): * Cache only
- triangle offset (`uint32`): * Cache only On Xbox: pointer to the triangle indices. On PC: offset to triangles relative to the end of the map's vertex data.
- triangle offset 2 (`uint32`): * Cache only On Xbox: pointer to the entry in the second parts list which points to the triangle indices. On PC: same value as the first triangle offset
- do not screw up the model (`uint32`): * Cache only
- vertex count (`uint32`): * Cache only
- bullshit (`uint32`): * Cache only
- vertex offset (`uint32`): * Cache only On Xbox: pointer to the entry in the second parts list which points to the triangle indices. On PC: offset to first vertex relative to the map's vertex data.
- local node count (`uint8`): * Max: 22 * Read-only * Unused
- local node indices (`uint8[22]`): * Hidden Local nodes are used to indirectly refer to a real node. So, if local nodes are used, a vertex says node #4, and local node #4 refers to node #5, then the vertex is node #5. There really doesn't seem to be any benefit to using local nodes, considering compressed vertices (which can only address 42 nodes) don't even use them. They just make the models unnecessarily more convoluted while taking up more space.
| shaders | `Block` | * HEK max count: 32 |
Field values:
- shader (`TagDependency`: shader): * Non-null
- permutation (`uint16`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `gbxmodel`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
