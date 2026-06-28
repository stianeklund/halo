# Halo 1 Tag: model_collision_geometry

## Summary

Model collision geometry tags contain collision data for an object"). This is in contrast to model/gbxmodel tags, which mostly contain the renderable data. Collision meshes tend to be less detailed than render meshes, and are used to check collisions _within_ the object's bounding radius").

## Tag Information

Tag group: `model_collision_geometry`.

HaloScript entries:

- `collision_debug [boolean]`
- `collision_debug_features [boolean]`
- `collision_debug_phantom_bsp [boolean]`
- `collision_debug_spray [boolean]`
- `debug_objects_collision_models [boolean]`
- `debug_objects_pathfinding_spheres [boolean]`

Defined fields and enum values mentioned:

- area damage effect
- area damage effect threshold
- body damaged effect
- body damaged threshold
- body depleted effect
- body destroyed effect
- body destroyed threshold
- body system shock
- bounding radius
- collision bsp surfaces breakable surface
- failing shield leak fraction
- flags
- flags always shields friendly damage
- flags only damaged by explosives
- flags only damaged while occupied
- flags parent never takes body damage for us
- flags passes area damage to children
- flags takes body damage for children
- flags takes shield damage for children
- friendly damage resistance
- indirect damage material
- localized damage effect
- materials
- materials body damage multiplier
- materials flags
- materials flags head
- materials material type
- materials name
- materials shield damage multiplier
- materials shield leak percentage
- maximum body vitality
- maximum shield vitality
- minimum stun damage
- modifiers
- nodes
- nodes bsps
- nodes bsps bsp2d nodes
- nodes bsps bsp2d nodes left child
- nodes bsps bsp2d nodes plane
- nodes bsps bsp2d nodes plane vector
- nodes bsps bsp2d nodes plane w
- nodes bsps bsp2d nodes right child
- nodes bsps bsp2d references
- nodes bsps bsp2d references bsp2d node
- nodes bsps bsp2d references plane
- nodes bsps bsp3d nodes
- nodes bsps bsp3d nodes back child
- nodes bsps bsp3d nodes front child
- nodes bsps bsp3d nodes plane
- nodes bsps edges
- nodes bsps edges end vertex
- nodes bsps edges forward edge
- nodes bsps edges left surface
- nodes bsps edges reverse edge
- nodes bsps edges right surface
- nodes bsps edges start vertex
- nodes bsps leaves
- nodes bsps leaves bsp2d reference count
- nodes bsps leaves first bsp2d reference
- nodes bsps leaves flags
- nodes bsps leaves flags contains double sided surfaces
- nodes bsps planes
- nodes bsps planes plane
- nodes bsps planes plane vector
- nodes bsps planes plane w
- nodes bsps surfaces
- nodes bsps surfaces breakable surface
- nodes bsps surfaces first edge
- nodes bsps surfaces flags
- nodes bsps surfaces flags breakable
- nodes bsps surfaces flags climbable
- nodes bsps surfaces flags invisible
- nodes bsps surfaces flags two sided
- nodes bsps surfaces material
- nodes bsps surfaces plane
- nodes bsps vertices
- nodes bsps vertices first edge
- nodes bsps vertices point
- nodes first child node
- nodes name

## Details

Model collision geometry tags contain collision data for an object"). This is in contrast to model/gbxmodel tags, which mostly contain the renderable data. Collision meshes tend to be less detailed than render meshes, and are used to check collisions _within_ the object's bounding radius").

Beyond having a collision mesh, these tags can also contain:

* _Pathfinding spheres_ which prevent AI from trying to walk through the object
* Damage ratios for each part of the object (e.g. weak points)
* Shield and health values

Collision geometry, rather than the model, is used to cast scenery shadows in lightmaps.

### Pathfinding spheres

Pathfinding spheres (blue) for a50 shown in Sapien after running `debug_objects_pathfinding_spheres 1`

AI can figure out where to go by checking the pathfinding data on the BSP. However, since objects like scenery and units") are not part of the BSP, Bungie implemented _pathfinding spheres_: spherical markers on objects that AI actively avoid walking into.

By placing these spheres in an object's collision model, artists can tell the AI exactly where _not_ to go. As far as we know, all object types can make use of pathfinding spheres. The object's bounding sphere does not seem to affect AI avoidance of them.

### How to add them

Pathfinding spheres are imported from the collision JMS file of your object. They are marked with `#pathfinder` and their radius is the actual radius that the AI will avoid walking in relation to the mid-point.

Pathfinding spheres can also be created automatically in some cases:

* When an artist doesn't specify any pathfinding spheres, the game will assume one at the object's origin at half the size of the bounding sphere") (which can be either too small or too big and result in bad pathfinding).
* Vehicle mass points (see physics) also count as pathfinding spheres. AI will actively avoid these.
* Bipeds by default also have a pathfinding sphere around their feet with the same width as their physics pill.

### Limits

model_ collision_ geometry tags can only have up to 32 pathfinding spheres in legacy (256 in H1A), up to 8 regions, and up to 33 permutations to a region.

### Animation

Unlike BSPs, collision geometry can have a self-intersecting mesh. However, this is only permitted between meshes parented by different nodes (e.g. limbs of a biped intersecting each other or the torso). Collision geometry cannot have weighted skinning for animations, so rigidly follows parent nodes in animations.

### Phantom BSP

Phantom BSP exists in the collision model of covenant crates.

Although phantom BSP is typically seen in the context of level geometry, it can also affect model collision geometry because this tag uses the same collision data structures as a scenario_ structure_ bsp. In the case of models, phantom BSP is limited to the object's bounding radius.

Like with level geometry, these can be troubleshooted in Sapien by running the console commands:

```
collision_debug 1
collision_debug_phantom_bsp 1
```

To fix them, use similar tricks as fixing level phantom BSP: fixing cases of nearly co-planar faces reported in your WRL file and/or slightly altering the collision model around the problematic location. If changes to the source geometry do not resolve the phantom BSP, you can use H1A Tool with the `fix-phantom-bsp` option enabled to compile the collision.

The following are related functions that you can use in your scenario scripts and/or debug globals that you can enter into the developer console for troubleshooting.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(collision_debug [boolean])` If enabled, a ray is continually shot from the camera (by default) to troubleshoot ray-object and ray-BSP collisions. A red normal-aligned marker will be shown where the ray collides with a surface. The collision surface itself, whether BSP or model, will be outline in red. Information about the collision surface will be shown in the top left corner of the screen, including plane and surface indices from the BSP structure, material type, and how many degrees the surface's normal differs from vertical. The types of surfaces which the test ray hits or ignores can be toggled with the `collision_debug_flag_*` commands. The maximum range of the ray is controlled with `collision_debug_length`. This feature can be frozen in place with `collision_debug_repeat` and also moved directly with the `collision_debug_point_*` and `collision_debug_vector_*` globals. A red ray is visible in these situations when the ray is no longer shot from the camera's point of view. | Global |
|  | `(collision_debug_features [boolean])` Toggles the display of _collision features_ near the camera, which can be spheres (red), cylinders (blue), or prisms (green). Collision size can be adjusted with `collision_debug_width` and `collision_debug_height`. The test point can be frozen in place using `collision_debug_repeat`. | Global |
|  | `(collision_debug_phantom_bsp [boolean])` Causes a floating pink cube and label "phantom bsp" to appear whenever a test ray from the center of the screen intersects with phantom BSP. It can be helpful to pair this with `collision_debug_spray`. | Global |
|  | `(collision_debug_spray [boolean])` Setting this to `true` will cause collision ray tests to be performed in a dense grid from the viewport. This operates independently of the `collision_debug` setting, and only the destination hit markers are shown. Can be affected by collision flags, length, and frozen with `collision_debug_repeat`. | Global |
|  | `(debug_objects_collision_models [boolean])` When `debug_objects` is enabled, displays green meshes for object collision models. This setting defaults to `true` in HEK Sapien but not H1A Sapien. | Global |
|  | `(debug_objects_pathfinding_spheres [boolean])` When `debug_objects` is enabled, displays object pathfinding spheres in blue. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- takes shield damage for children (`0x1`)
- takes body damage for children (`0x2`)
- always shields friendly damage (`0x4`)
- passes area damage to children (`0x8`)
- parent never takes body damage for us (`0x10`)
- only damaged by explosives (`0x20`)
- only damaged while occupied (`0x40`)
| indirect damage material | `uint16` |  |
| maximum body vitality | `float` | Sets the maximum amount of health that a unit") has (damage hit points). For example, the cyborg biped has a value of 75. This value can also be changed at runtime for individual units by using `unit_set_maximum_vitality` in scripts. |
| body system shock | `float` |  |
| friendly damage resistance | `float` | * Min: 0 * Max: 1 |
| localized damage effect | `TagDependency`: effect |  |
| area damage effect threshold | `float` | * Min: 0 * Max: 1 |
| area damage effect | `TagDependency`: effect |  |
| body damaged threshold | `float` |  |
| body damaged effect | `TagDependency`: effect |  |
| body depleted effect | `TagDependency`: effect |  |
| body destroyed threshold | `float` |  |
| body destroyed effect | `TagDependency`: effect |  |
| maximum shield vitality | `float` | Sets the maximum amount of shields that a unit") has (damage hit points). For example, the cyborg biped has a value of 75. This value can also be changed at runtime for individual units by using `unit_set_maximum_vitality` in scripts. |
| shield material type | `enum` | Determines which damage_ effect material modifier is used when applying damage to the shield. |
Option values:
- dirt (`0x0`)
- sand (`0x1`)
- stone (`0x2`)
- snow (`0x3`)
- wood (`0x4`)
- metal hollow (`0x5`)
- metal thin (`0x6`)
- metal thick (`0x7`)
- rubber (`0x8`)
- glass (`0x9`): Required for breakable surfaces using the `-` symbol.
- force field (`0xA`)
- grunt (`0xB`)
- hunter armor (`0xC`)
- hunter skin (`0xD`)
- elite (`0xE`)
- jackal (`0xF`)
- jackal energy shield (`0x10`)
- engineer skin (`0x11`)
- engineer force field (`0x12`)
- flood combat form (`0x13`)
- flood carrier form (`0x14`)
- cyborg armor (`0x15`)
- cyborg energy shield (`0x16`)
- human armor (`0x17`)
- human skin (`0x18`)
- sentinel (`0x19`)
- monitor (`0x1A`)
- plastic (`0x1B`)
- water (`0x1C`)
- leaves (`0x1D`)
- elite energy shield (`0x1E`)
- ice (`0x1F`)
- hunter shield (`0x20`)
| shield failure function | `enum` |  |
Option values:
- linear (`0x0`)
- early (`0x1`)
- very early (`0x2`)
- late (`0x3`)
- very late (`0x4`)
- cosine (`0x5`)
| shield failure threshold | `float` |  |
| failing shield leak fraction | `float` |  |
| minimum stun damage | `float` |  |
| stun time | `float` | * Unit: seconds |
| recharge time | `float` | * Unit: seconds |
| shield damaged threshold | `float` |  |
| shield damaged effect | `TagDependency`: effect |  |
| shield depleted effect | `TagDependency`: effect |  |
| shield recharging effect | `TagDependency`: effect |  |
| shield recharge rate | `float` | * Cache only |
| materials | `Block` | * HEK max count: 32 * Read-only |
Flag values:
- name (`TagString`): * Read-only
- flags (`bitfield`)
- head (`0x1`)
- material type (`enum`?)
- shield leak percentage (`float`)
- shield damage multiplier (`float`)
- body damage multiplier (`float`)
| regions | `Block` | * HEK max count: 8 * Read-only |
Flag values:
- name (`TagString`): * Read-only
- flags (`bitfield`)
- lives until object dies (`0x1`)
- forces object to die (`0x2`)
- dies when object dies (`0x4`)
- dies when object is damaged (`0x8`)
- disappears when shield is off (`0x10`)
- inhibits melee attack (`0x20`): Prevents melee attacks by the unit when this region is destroyed. Used for the Flood's arms, for example.
- inhibits weapon attack (`0x40`)
- inhibits walking (`0x80`)
- forces drop weapon (`0x100`): Causes the unit to drop its held weapon when this region is destroyed, such as shooting the arms off of Flood.
- causes head maimed scream (`0x200`)
- damage threshold (`float`)
- destroyed effect (`TagDependency`: effect)
- permutations (`Block`): * HEK max count: 32 * Read-only
- * Read-only (Field): Type
- Comments (): ---
- name (`TagString`)
| modifiers | `Block` | * Read-only |
| | Field | Type | Comments | | --- | --- | --- | |
| x | `Bounds` |  |
Field values:
- min (`float`)
- max (`float`)
| y | `Bounds`? |  |
| z | `Bounds`? |  |
| pathfinding spheres | `Block` | * HEK max count: 32 * H1A-EK max count: 256 * Volatile Spheres which approximate the shape of the model for AI pathfinding avoidance. The sphere limit was increased to match Reach's in H1A. |
Field values:
- node (`uint16`→)
- center (`Point3D`)
- radius (`float`)
| nodes | `Block` | * HEK max count: 64 * Read-only |
Flag values:
- name (`TagString`)
- region (`uint16`)
- parent node (`uint16`→)
- next sibling node (`uint16`→)
- first child node (`uint16`→)
- name thing (`int16`): * Cache only
- bsps (`Block`): * HEK max count: 32
- * Read-only * Processed during compile (Field): Type
- Comments (): ---
- bsp3d nodes (`Block`): * HEK max count: 131072 A block of nodes used to efficiently find collideable surfaces. Each node divides space with an infinite plane and references two child nodes by index into this block. The first element in the block is the root node. A test point can be recursively tested against planes to find a _leaf_ of potentially colliding surfaces.
- * Read-only (Field): Type
- Comments (): ---
- plane (`uint32`): An index into the _planes_ block. The plane divides 3D space into front and back half-spaces. A point can be tested against this plane to determine which half-space to descend into. Tool will create new planes or use existing surface planes when generating the BSP tree based on a heuristic of how many surfaces remain in the node. For 1024 and above, axis-aligned planes based on vertex bounds are used. Otherwise, the surface within the remaining set whose plane minimizes the difference between front and back surface counts is used. In the case of levels, only collideable surfaces and portal planes are candidates.
- back child (`uint32`): * Flagged Index of the BSP3D node representing space to the back of the dividing plane. A value of `0xFFFFFFFF` is considered null and the space behind the plane is considered _outside_ the sealed BSP (Sapien labels this as "solid" space when using `debug_structure`). Otherwise, if the highest order bit is set (`0x80000000`), the remaining 31 bits (mask `0x7FFFFFFF`) represent an index into the _leaves_ block.
- front child (`uint32`): * Flagged Similar to the _back child_ field.
- planes (`Block`): * HEK max count: 65536 Planes are infinite 2D surfaces in 3D space. They are used both to subdivide the BSP in each bsp3d node and to define collideable surfaces. The first 8 planes in the block seem to serve a special purpose -- the first 4 define the XY bounding box, with the next 4 axis aligned planes unkown. Furthermore, the last 8 planes in the block are used for the first few levels of bsp3d nodes. A single plane may be referenced from multiple bsp3d nodes since the surfaces that planes derive from can also be found in multiple leaves.
- * Read-only (Field): Type
- Comments (): ---
- plane (`Plane3D`): A plane which divides 3D space in two.
- vector (`Vector3D`): A 3-float normal vector.
- w (`float`): Distance from origin (along normal).
- leaves (`Block`): * HEK max count: 65536 Leaves mark the transition between the 3D BSP and a collection of convex collideable surfaces in the same localized area. Each set of co-planar surfaces within this leaf is stored within a child 2D BSP. Note that surfaces may be found under **multiple leaves**, since any surface which is not completely on either side of a 3D plane will need to belong to both child 3D BSP nodes.
- * Read-only (Field): Type
- Comments (): ---
- flags (`bitfield`): Flags used to optimize collision checks at runtime.
- contains double sided surfaces (`0x1`): Indicates if any surface in any of this leaf's 2D BSPs is double-sided (e.g. glass).
- bsp2d reference count (`uint16`): Determines how many contiguous 2D BSP references belong to this leaf.
- first bsp2d reference (`uint32`): Index of the first 2D BSP reference associated with this leaf. It will be followed by a number of other 2D BSP references according to the above count.
- bsp2d references (`Block`): * HEK max count: 131072 Represents either a 2D BSP of surfaces, or a singular surface if the node index is flagged. In either case, test points or 3D line traces should be projected onto the basis plane in order to continue.
- * Read-only (Field): Type
- Comments (): ---
- plane (`uint32`): * Flagged Index for the plane used to decide what basis plane is best to project on (X,Y), (Y,Z) or (X,Z). The basis plane is chosen by the referenced plane's most significant normal component. A flagged plane index means its normal vector is opposite.
- bsp2d node (`uint32`): * Flagged The starting node for the 2D BSP on this plane. If flagged, then the index (masked to `0x7FFFFFFF`) refers to a surface instead. Null if `0xFFFFFFFF`.
- bsp2d nodes (`Block`): * HEK max count: 65535
- * Read-only (Field): Type
- Comments (): ---
- plane (`Plane2D`): A 2D plane (line) subdividing left and right child surfaces. This line is in the space of the 2D BSP reference's basis plane.
- vector (`Vector2D`)
- w (`float`)
- left child (`uint32`): * Flagged refers to a BSP2D node if not signed and a surface if sign bit is set; null if `0xFFFFFFFF`.
- right child (`uint32`): * Flagged refers to a BSP2D node if not signed and a surface if sign bit is set; null if `0xFFFFFFFF`.
- surfaces (`Block`): * HEK max count: 131072 Surfaces are planar collideable polygons. They are not necessarily triangular and may have up to 8 edges. BSP collision surfaces can be visualized in Sapien using `debug_structure 1`. These surfaces are not used for the rendered geometry.
- * Read-only (Field): Type
- Comments (): ---
- plane (`uint32`): * Flagged Index into the planes block for this surface's plane. Note that multiple co-planar surfaces may reference the same plane since this plane index is a copy of the parent bsp2d reference's plane index, even when flagged.
- first edge (`uint32`)
- flags (`bitfield`)
- two sided (`0x1`)
- invisible (`0x2`)
- climbable (`0x4`): Indicates if the surface is a climbable ladder.
- breakable (`0x8`): Indicates if the surface is breakable. The surface must also have a _breakable surface_ index below.
- breakable surface (`int8`): Index into this tag's breakable surfaces block. It is unknown if this is a signed or flagged field, but since it is 8-bit there cannot be more than 256 unique breakable surfaces in a BSP.
- material (`uint16`): Index into this tag's materials block (_collision materials_ for structure BSPs and _materials_ for model_ collision_ geometry). This can be `-1` if the surface has no material, such as `+sky` faces.
- edges (`Block`): * HEK max count: 262144 Edges, surfaces, and vertices form a data structure called a _doubly connected edge list (DCEL)_, also known as a _half-edge data structure_. The edges and their vertices define the boundaries of the collideable surfaces within a leaf.
- * Read-only (Field): Type
- Comments (): ---
- start vertex (`uint32`)
- end vertex (`uint32`)
- forward edge (`uint32`)
- reverse edge (`uint32`)
- left surface (`uint32`)
- right surface (`uint32`)
- vertices (`Block`): * HEK max count: 131072 The 3D coordinates used for edge starting and ending locations.
- * Read-only (Field): Type
- Comments (): ---
- point (`Point3D`)
- first edge (`uint32`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `model_collision_geometry`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
