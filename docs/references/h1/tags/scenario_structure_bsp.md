# Halo 1 Tag: scenario_structure_bsp

## Summary

The **scenario structure BSP** tag, commonly just called the **BSP**, contains level geometry, weather data, material assignments, AI pathfinding information, lightmaps, and other data structures. You can think of the BSP as the "stage" where the game takes place objects") are placed within it. Aside from sounds and bitmaps, the BSP tends to be one of the largest tags in a map. Singleplayer scenarios often use multiple BSPs which are switched between at loading zones.

## Tag Information

Tag group: `scenario_structure_bsp`.

HaloScript entries:

- `<void> debug_pvs <boolean>`
- `collision_debug [boolean]`
- `collision_debug_features [boolean]`
- `collision_debug_flag_structure [boolean]`
- `collision_debug_phantom_bsp [boolean]`
- `collision_debug_spray [boolean]`
- `debug_bsp [boolean]`
- `debug_camera [boolean]`
- `debug_no_frustum_clip [boolean]`
- `debug_portals [boolean]`
- `debug_sound_environment [boolean]`
- `debug_structure [boolean]`
- `debug_structure_automatic [boolean]`
- `structures_use_pvs_for_vs`

Defined fields and enum values mentioned:

- background sound palette
- background sound palette background sound
- background sound palette name
- background sound palette scale function
- breakable surfaces
- breakable surfaces centroid
- breakable surfaces collision surface index
- breakable surfaces radius
- cluster data
- cluster portals
- cluster portals back cluster
- cluster portals bounding radius
- cluster portals centroid
- cluster portals flags
- cluster portals flags ai can simply not hear through all this amazing stuff darn it
- cluster portals front cluster
- cluster portals plane index
- cluster portals vertices
- cluster portals vertices point
- clusters
- clusters background sound
- clusters decal count
- clusters first decal index
- clusters first lens flare marker index
- clusters fog
- clusters lens flare marker count
- clusters mirrors
- clusters mirrors plane
- clusters mirrors shader
- clusters mirrors vertices
- clusters mirrors vertices point
- clusters portals
- clusters portals portal
- clusters predicted resources
- clusters predicted resources resource index
- clusters predicted resources tag
- clusters predicted resources type
- clusters predicted resources type bitmap
- clusters predicted resources type sound
- clusters sky
- clusters sound environment
- clusters subclusters
- clusters subclusters surface indices
- clusters subclusters surface indices index
- clusters subclusters world bounds x
- clusters subclusters world bounds y
- clusters subclusters world bounds z
- clusters surface indices
- clusters transition structure bsp
- clusters weather
- collision bsp
- collision bsp bsp2d nodes
- collision bsp bsp2d nodes left child
- collision bsp bsp2d nodes plane
- collision bsp bsp2d nodes plane vector
- collision bsp bsp2d nodes plane w
- collision bsp bsp2d nodes right child
- collision bsp bsp2d references
- collision bsp bsp2d references bsp2d node
- collision bsp bsp2d references plane
- collision bsp bsp3d nodes
- collision bsp bsp3d nodes back child
- collision bsp bsp3d nodes front child
- collision bsp bsp3d nodes plane
- collision bsp edges
- collision bsp edges end vertex
- collision bsp edges forward edge
- collision bsp edges left surface
- collision bsp edges reverse edge
- collision bsp edges right surface
- collision bsp edges start vertex
- collision bsp leaves
- collision bsp leaves bsp2d reference count
- collision bsp leaves first bsp2d reference
- collision bsp leaves flags
- collision bsp leaves flags contains double sided surfaces
- collision bsp planes
- collision bsp planes plane
- collision bsp planes plane vector
- collision bsp planes plane w

## Details

The **scenario structure BSP** tag, commonly just called the **BSP**, contains level geometry, weather data, material assignments, AI pathfinding information, lightmaps, and other data structures. You can think of the BSP as the "stage" where the game takes place objects") are placed within it. Aside from sounds and bitmaps, the BSP tends to be one of the largest tags in a map. Singleplayer scenarios often use multiple BSPs which are switched between at loading zones.

The term "BSP" stands for Binary Space Partitioning, a technique where space within a sealed static mesh is recursively subdivided by planes into convex_leaf nodes_. The resulting _BSP tree_ can be used to efficiently answer geometric queries, such as which surfaces should be collision-tested for physics objects.

### Compilation

After level geometry is exported to JMS format from your 3D software of choice, it can be compiled into a BSP tag using Tool's structure verb.

### BSP transitions

While a scenario can reference multiple BSPs, Halo can only have a single BSP loaded at a time. Transitions between BSPs can be scripted (`switch_bsp`), e.g. using trigger volumes. Objects in unloaded BSPs are not simulated.

Although multiple BSPs are intended for singleplayer maps and do not synchronize, some custom multiplayer maps have used nearly identical BSPs which only differ in lighting to add a day/night switch scripted by a button in the escape menu.

### Shaders

The most commonly used shader") type for BSPs is shader_ environment, suitable for most opaque surfaces and alpha-tested grates or billboard trees (as in _Timberland_). This shader type supports the blending between multiple detail maps, often used for ground maps with dirt and grass areas.

Transparent shaders can also be used, for example:

* shader_ transparent_ chicago for flowing rivers or waterfalls
* shader_ transparent_ chicago_ extended for waterfalls
* shader_ transparent_ water for lakes and oceans

The shader_ model type will not be rendered by the game since it is intended for use with object models.

### Portals

If you think of the BSP as a sealed volume, _portals_ are like doorways that split that space into distinct sealed "rooms" called _clusters_. This lets the game know what areas of a level can be culled to maintain good framerates, and the resulting clusters can each have unique properties assigned like background sound.

You can create portals in a few ways:

1. Surfaces which cut through the level and use the material name `+portal`. The portal planes can be connected along edges and even form box shapes but cannot have "leaky" gaps between the spaces they are meant to seal. Portal planes must not intersect each other.
2. Surfaces which exactly match at their boundary with the vertices and eges in level geometry (e.g. around a doorway) and use the material name `+exactportal`. The faces of the exact portal do not need to be coplanar.
3. Existing materials in the level that happen to consistently seal off closed volumes wherever they're used as if they were exact portals, like floor grates or glass windows covering a pit. Add the `.` material symbol, e.g. `floor_grate.%`.

It may be necessary to add multiple portals to close a space. For example, if a building had multiple doors and windows you would need to add portals to each of them for the inside of the building to be considered sealed off from the outside. For help troubleshooting portals, see here.

### Clusters and cluster data

Clusters are sealed volumes of a BSP separated by portal planes. They are used both as a rendering optimization and artistically; map authors can assign weather_ particle_ system, wind, sound_ environment, and sound_ looping tags to define local atmospheric and ambience qualities for each section of the map. Different clusters can even reference different skies. The level will contain a single large cluster if no portals were created.

Note that it may still be desirable to reference weather for indoor clusters if there are outdoor areas visible from them, otherwise snow and rain particles will abruptly disappear. To mask weather in such clusters, use weather polyhedra.

### Indoor vs. outdoor clusters

Clusters are either _outdoor/exterior_ or _indoor/interior_. When a cluster contains `+sky` faces it is an outdoor cluster and has a sky index of `0` or greater. Furthermore, any clusters with potentially visibility of the cluster containing sky faces will become outdoor clusters too. The material `+seamsealer` does not contribute to a cluster becoming outdoor so is typically used in BSP transition areas.

All other clusters are indoor. These have a sky index of `-1` instead and use the indoor parameters of sky index `0` (the first sky). For example, indoor clusters will use use its _indoor fog color_ rather than its _outdoor fog color_. The indoor parameters of any other skies are unused.

When the game transitions between indoor and outdoor clusters the fog colour fades based on cumulative camera movement, not time. This effect can be seen easily in Danger Canyon: load it in Sapien and fly the camera through the hallways while `debug_pvs 1` and `rasterizer_wireframe 1` are enabled.

### Potentially visible set

The _potentially visible set_ (PVS) data is precomputed when a BSP is compiled and helps the engine determine which clusters are likely visible from each other. A cluster can "see" any other cluster behind portals visible from itself plus one level of clusters further. Any clusters beyond that will not be rendered.

Tool also takes into account the indoor sky's _indoor fog opaque distance_ and _indoor fog maximum density_ when computing the PVS. If the density is `1.0` (fully opaque) then Tool knows that indoor clusters cannot see beyond the opaque distance even if there are clusters within a line of sight. Tool logs the indoor maximum world units when the BSP is imported (if there a sky referenced).

In addition to using the static PVS, the game may dynamically cull objects and parts of clusters using portal frustums or other criteria. To disable this behaviour, set `structures_use_pvs_for_vs` to `true` (this causes inconsistent object rendering though).

### Potentially audible set

Like the PVS, the _potentially audible set_ (PAS) data encodes which clusters can hear sounds from other clusters. This allows the engine to cull sounds without having to perform a costlier obstruction check. It is unknown what criteria make clusters potentially audible.

### Fog planes

Areas of a map which need a fog layer can be marked using _fog planes_. These are 2D surfaces which reference fog tags, not to be confused with atmospheric fog which is part of the sky tag.

Create a fog plane by modeling a **flat plane** and giving its faces the material symbol`$`, either on an existing material (like `my_level_ocean!$`) or as `+unused$` if the fog exists in isolation. The fog will exist behind/antinormal to the plane. It must be completely flat and it is invalid for any cluster to be able to see multiple fog planes (more details), so adjust the size and shape of the plane accordingly. It's also important to know that the fog plane is rendered as if it's infinite, no matter of how the fog plane is actually shaped when you model it. The shape of the plane in your model is used to know which clusters intersect it.

You can even create slanted or upside-down fog planes as long as they follow the rules. Once your BSP is imported, you'll be able to assign fog to your plane(s) in Sapien.

### Weather polyhedra

Weather polys extracted from _Assault on the Control Room_.

Weather polyhedra are artist-defined volumes where weather particle systems will not render, such as under overhangs where you would not expect to see rain.

To create them, simply model outwardly facing convex volumes where all faces use the `+weatherpoly` material name and Tool will generate the weather polyhedra when importing your BSP. The volumes can overlap and up to 8 can be visible at any time before the game starts ignoring some (Sapien will print warnings when this happens).

It is important that you still create weather polyhedra even if you have portals separating inside and outside spaces. Simply not assigning weather to the clusters which are under cover is not enough to prevent rain from appearing there. This is because the game renders weather based on the camera's current cluster, so if the player is outside a building looking in through a doorway they will still see rain indoors because the camera is currently located outside. Conversely, if the cluster within the building has no weather assigned then players will not see rain outdoors when looking out the doorway from inside. The solution is still assigning weather to covered clusters, then masking those areas with large weather polyhedra. This can be seen in the example figure.

Within the tag, the polyhedra are represented as a center point, bounding radius, and up to 16 planes which enclose a volume. Therefore your polyhedra technically don't need to be _sealed_ volumes because they are limited to their bounding radius. A polyhedron can be created with as little as 1 plane.

### Lens flare markers

In a10, lens flare markers were generated for fluorescent lights.

When a BSP shader references a lens_ flare, _lens flare markers_ are automatically created and stored in the BSP tag during each structure import or updated with structure-lens-flares. These are used to give lights a "glowy" appearance. If the shader has a _lens flare spacing_ of `0`, a single lens flare is placed on the surface. Otherwise, the lens flares are evenly spaced within the surface according to the spacing value (world units).

A BSP can contain up to 65535 lens flare markers, and up to 256 types of lens flares. However, there is a much lower limit to how many the game will draw at a given time, exactly how many is unknown.

### Marker placement

Lens flare placements shown in red with `rasterizer_lens_flares_occlusion_debug 1`.

Tool evenly spaces BSP lens flare markers within the 2D convex hull of each set of connected coplanar faces sharing the same shader. This means if you create relatively complex shapes for lens flares to be placed within, lens flares may end up outside those faces. An example would be a lighting ring bordering a room's floor. Tool will fill the entire room's floor with BSP lens flares, not just within the faces of the border lights.

You can prevent this from happening by either keeping lens flare generating geometry separated or convex, or duplicating the shader and alternating which shader is used for each segment of the lighting strip.

The rotational alignment of the lens flares is not well defined. They are sometimes axis-aligned and sometimes rotated at an angle.

### Lightmaps

_Lightmaps_ are the visual representation of the BSP, and are stored in a separate representation from its collision data. The lightmaps data includes the renderable triangles and a precalculated radiosity bitmap.

_See main page: Lightmaps._

### Collision artifacts

### Phantom BSP

Danger Canyon contains at least two prevalent cases of phantom BSP. The Warthog and bullets are both colliding with invisible extensions of nearby surfaces.

The phantom BSP here is caused by nearly coplanar faces on the nearby ramp.

Phantom BSP is a collision artifact sometimes produced when compiling BSPs. It manifests itself as invisible surfaces which projectiles and vehicles collide with (but not players), and mostly appears around sharp corners near cases of "nearly coplanar faces" warnings in your WRL file. It can also cause objects above the problem area to fall back to BSP default lighting").

Bungie was aware of this artifact and implemented a feature to help spot it (`collision_debug_phantom_bsp 1` in Sapien or Standalone). If you find phantom BSP in your map, there are a few steps you can take to resolve it:

1. Preemptively, keep your geometry simple and robust without an abundance of dense, complex, or spiky shapes. Flat surfaces like floors and walls should be kept as flat as possible by using alignment tools when modeling rather than "eyeballing it".
2. Fix any "nearly coplanar" warnings in your source model by scaling affected faces to 0 along their normal axis or using alignment. Since Tool slightly rounds vertex coordinates when compiling BSPs, sometimes this warning cannot be resolved for surfaces which are not axis-aligned.
3. There is an element of chance to phantom BSP appearing which depends on how your geometry is recursively subdivided to form a BSP tree. Modifying unrelated parts of your level like adding portals or moving vertices can sometimes affect how the level is subdivided and make phantom BSP disappear or appear in new places.
4. Using H1A Tool's fix-phantom-bsp option to compile your BSP will prevent _most_ phantom BSP at the cost of slightly increasing the tag size. There have been reports that this may not resolve all phantom BSP.
5. If you do not have access to source JMS, and are trying to fix a BSP tag, you will need to import the tag into Blender or 3ds Max and rework the model to make it export-ready and free of nearly coplanar surfaces.

On a technical level, cases of phantom BSP are dividing planes where a child index is `-1`, but the space on that side of the plane is not actually _completely_ outside the level. The artifact is bounded by all parent dividing planes.

### BSP holes

Video 3

This location in Derelict has a small collision hole where items can fall through the map.

BSP holes or leaks are another type of collision artifact where items or players can fall through the map. It is not known what causes this, but it can be resolved by altering triangulation around the affected area (rotating edges). Compiling the BSP with H1A Tool's fix-phantom-bsp option also prevents this.

### Pathfinding data

The BSP contains data on traversable surfaces which aid AI in pathfinding (walking to a destination). This data is generated automatically during BSP compilation and is retained even in when the BSP is compiled into multiplayer maps.

_See more about the pathfinding system._

The following are related functions that you can use in your scenario scripts and/or debug globals that you can enter into the developer console for troubleshooting.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(collision_debug [boolean])` If enabled, a ray is continually shot from the camera (by default) to troubleshoot ray-object and ray-BSP collisions. A red normal-aligned marker will be shown where the ray collides with a surface. The collision surface itself, whether BSP or model, will be outline in red. Information about the collision surface will be shown in the top left corner of the screen, including plane and surface indices from the BSP structure, material type, and how many degrees the surface's normal differs from vertical. The types of surfaces which the test ray hits or ignores can be toggled with the `collision_debug_flag_*` commands. The maximum range of the ray is controlled with `collision_debug_length`. This feature can be frozen in place with `collision_debug_repeat` and also moved directly with the `collision_debug_point_*` and `collision_debug_vector_*` globals. A red ray is visible in these situations when the ray is no longer shot from the camera's point of view. | Global |
|  | `(collision_debug_features [boolean])` Toggles the display of _collision features_ near the camera, which can be spheres (red), cylinders (blue), or prisms (green). Collision size can be adjusted with `collision_debug_width` and `collision_debug_height`. The test point can be frozen in place using `collision_debug_repeat`. | Global |
|  | `(collision_debug_flag_structure [boolean])` Toggles if collision debug rays collide with the structure BSP. Collisions with model_ collision_ geometry BSPs are unaffected. | Global |
|  | `(collision_debug_phantom_bsp [boolean])` Causes a floating pink cube and label "phantom bsp" to appear whenever a test ray from the center of the screen intersects with phantom BSP. It can be helpful to pair this with `collision_debug_spray`. | Global |
|  | `(collision_debug_spray [boolean])` Setting this to `true` will cause collision ray tests to be performed in a dense grid from the viewport. This operates independently of the `collision_debug` setting, and only the destination hit markers are shown. Can be affected by collision flags, length, and frozen with `collision_debug_repeat`. | Global |
|  | `(debug_bsp [boolean])` Toggles the display of structure BSP node traversal for the camera location. At each level, the node and plane indices are shown as well as a `+` or `-` symbol indicating if the camera was on the front or back side of the plane. | Global |
|  | `(debug_camera [boolean])` Shows debug information about the camera in the top left corner, including: * 3D coordinates in world units * BSP leaf indices * Cluster indices, e.g. `(#2 [2])`. If the camera leaves the BSP then the first index will be `-1` while the second index tracks the last known valid cluster. This is what allows the game to keep rendering part of the BSP while the camera is outside it. * Ground point coordinates and BSP surface index (point directly below the camera on the ground) * Facing direction (yaw angle) * Tag path of the shader pointed at by the camera. The surface must be collideable and this feature cannot be configured with collision debug flags. | Global |
|  | `(debug_no_frustum_clip [boolean])` Disables portal-based occlusion culling for objects and BSP faces (use `rasterizer_wireframe 1` to see this). In addition to the PVS, which determines the set of clusters which are potentially visible, groups of faces and objects within those clusters can still be culled further using the limited view from the camera's location through the series of portals leading to those clusters (a portal-diminished frustum). The game uses object bounding spheres for this and likely subcluster bounding boxes. Disabling this type of culling does not cause the game to ignore the PVS and it will still cull entire clusters if no portals to them are on-screen. To force the game to render all clusters and subclusters in the PVS, enable `structures_use_pvs_for_vs`. | Global |
|  | `(debug_portals [boolean])` Draws BSP portals as red outlines. You may wish to pair this with `rasterizer_wireframe 1` to help you understand how portals result in culling parts of the BSP from rendering. The related function `debug_pvs` will enable/disable this global and `structures_use_pvs_for_vs`. | Global |
|  | `(<void> debug_pvs <boolean>)` Calling this function with `true` enables both the `debug_portals` and `structures_use_pvs_for_vs` globals, which has the effect of enabling portal borders and rendering all faces in the PVS. Call with `false` to disable these globals. | Function |
|  | `(debug_sound_environment [boolean])` If enabled, shows the tag path of the cluster's current sound_ environment. | Global |
|  | `(debug_structure [boolean])` When enabled, all scenario_ structure_ bsp collision surfaces will be rendered with green outlines. A red bounding box surrounds renderable surfaces. | Global |
|  | `(debug_structure_automatic [boolean])` Like `debug_structure`, but only appears when the camera is in "solid" space (outside the BSP). This can be handy for finding your way back to the level when switching BSPs. It is set to `true` by default in Sapien. This is a backported feature from later Halos and only available in the newer MCC tools. | Global |
|  | `(structures_use_pvs_for_vs)` If enabled, forces the renderer to fully render all clusters and subclusters in the potentially visible set (PVS) without any culling of occluded faces, and even if portals into those clusters are off-screen. Pair with `rasterizer_wireframe 1` to see the effects. Use this to debug which clusters are in the PVS of the camera's cluster, which can help you understand why a cluster is considered indoor or outdoor. When this debug option is enabled, objects may sometimes disappear while in view. This global differs from the related `debug_no_frustum_clip`, which disables subcluster culling by portal-diminished frustrums but still allows for entire clusters to be culled when no portals into them are visible. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| lightmap bitmaps | `TagDependency`: bitmap | A reference to the bitmap storing the level's baked lightmaps. The level will not be visible without this. |
| vehicle floor | `float` | * Unit: world units If set to a non-zero value, adds a soft barrier at this world-space Z-axis height that occupied flying vehicles cannot fly below. For example, the map Gephyrophobia uses this to prevent players from flying too far below the bridge. This affects vehicles with the _human plane_ or _alien fighter_vehicle type. The vehicle must also be occupied and physics-active (has not come to a rest) to be repelled. Other vehicle types, or unoccupied flying vehicles, will fall below this floor and rest upon a second hard-coded floor at -256 WU. It is possible to spawn or teleport vehicles below these floors, however as soon as the vehicle becomes physics-active, e.g. by driving it or disturbing it with a melee, the vehicle will experience a repulsive force proportional to its distance below the floor. This can result in the vehicle shooting into the sky at immense speeds. |
| vehicle ceiling | `float` | * Unit: world units If set to a non-zero value, adds a soft barrier at this world-space Z-axis height that occupied flying vehicles cannot fly above. For example, the map Blood Gulch uses this to prevent players from flying too high. You may use this for gameplay reasons or to avoid players seeing out of bounds. The ceiling behaves essentially the same as the _vehicle floor_. However, there is no second hard-coded ceiling. Other vehicle types, and unoccupied flying vehicles, are free to move above the ceiling. Once occupied, flying vehicles will be forced below the ceiling proportionally to their height above it. |
| default ambient color | `ColorRGB` | If the red component of this colour is non-zero, this and all following default lighting fields will be used to light objects") when a ground point cannot be found within 10wu below them, including when objects are outside the BSP or above phantom BSP. Otherwise, objects sample lightmaps data at their ground point. |
| default distant light 0 color | `ColorRGB` | Primary distant light colour when default lighting applies. |
| default distant light 0 direction | `Vector3D` | Primary distant light direction when default lighting applies. |
| default distant light 1 color | `ColorRGB` | Secondary distant light colour when default lighting applies. |
| default distant light 1 direction | `Vector3D` | Secondary distant light direction when default lighting applies. |
| default reflection tint | `ColorARGB` | Reflection tint when default lighting applies. |
| default shadow vector | `Vector3D` | Shadow vector when default lighting applies. |
| default shadow color | `ColorRGB` | Shadow colour when default lighting applies. |
| collision materials | `Block` | * HEK max count: 512 |
Option values:
- shader (`TagDependency`: shader): * Non-null
- material (`enum`): * Cache only
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
| collision bsp | `Block` | * HEK max count: 1 |
Flag values:
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
| nodes | `Block` | * HEK max count: 131072 |
Field values:
- node stuff (`uint16[3]`)
| world bounds x | `Bounds` |  |
Field values:
- min (`float`)
- max (`float`)
| world bounds y | `Bounds`? |  |
| world bounds z | `Bounds`? |  |
| leaves | `Block` | * HEK max count: 65536 |
Field values:
- vertices (`uint16[3]`)
- cluster (`uint16`)
- surface reference count (`uint16`): The number of surfaces.
- surface references (`int32`): Index into the _leaf surfaces_ block.
| leaf surfaces | `Block` | * HEK max count: 262144 |
Field values:
- surface (`int32`): Index into the _surfaces_ block. This value may change after radiosity is run, as surfaces may be reordered.
- node (`int32`): Index into the _nodes_ block.
| surfaces | `Block` | * HEK max count: 131072 A list of renderable triangles, represented as vertex indices. The indices are used to look up vertex information within the vertex data referenced by lightmap materials. |
Field values:
- vertex0 index (`uint16`): The triangle's first vertex index.
- vertex1 index (`uint16`): The triangle's second vertex index.
- vertex2 index (`uint16`): The triangle's third vertex index.
| lightmaps | `Block` | * HEK max count: 128 Each lightmap contains a set of renderable triangles, which may be mapped to a single bitmap index (or "page") of the referenced _lightmaps bitmaps_. On initial structure import this will contain just two lightmaps with `-1` bitmap index: the first containing all lightmapped materials (shader_ environment and shader_ transparent_ glass) and the second containing all other shaders which will not be lightmapped (transparent generics and chicagos). The final entry will always exist but may have no _materials_. After radiosity is run and Tool has segmented the level into charts, the first entry is typically split into multiple independent lightmaps with distinct atlases. |
Field values:
- bitmap (`uint16`): Indicates which index of bitmap data within the lightmap bitmaps is used for the faces of these materials. In other words, the baked lightmaps can be split across multiple bitmap "pages" and this index tells the game what page the faces of these materials use. An index of `-1` means these faces are not lightmapped, like if radiosity has not been run yet or, once it has, for additive blended transparent shaders like lights (which Tool groups in the last item of this block).
- materials (`Block`): * HEK max count: 2048
- * Read-only * Processed during compile (Field): Type
- Comments (): ---
- shader (`TagDependency`: shader): * Non-null Tag reference to the shader which is rendered for this material.
- shader permutation (`uint16`): Controls the permutation index of bitmaps referenced by the above shader. This will affect which reflection cubemap permutation is used if it has multiple, such as the Elite's cubemap. Defaults to `0`.
- flags (`bitfield`)
- coplanar (`0x1`): Indicates that the surfaces in this material are coplanar.
- fog plane (`0x2`)
- surfaces (`int32`): Indicates the starting index of the _surfaces_ block where this material's surfaces are located. These surfaces will contain vertex indices for the vertex data pointed to by this material.
- surface count (`int32`): The number of surfaces following the above offset which belong to this material.
- centroid (`Point3D`): * Unit: world units For _coplanar_ materials, the centroid is the world-space coordinates for the center of the material's axis-aligned bounding box. Otherwise zeroed.
- ambient color (`ColorRGB`): For lightmapped materials (_bitmap_ index >= 0), always set to RGB `(0.2, 0.2, 0.2)` when radiosity is run. Otherwise zeroed.
- distant light count (`uint16`): For lightmapped materials (_bitmap_ index >= 0), always set to `2` when radiosity is run. Otherwise zeroed.
- distant light 0 color (`ColorRGB`): For lightmapped materials (_bitmap_ index >= 0), always set to RGB `(1, 1, 1)` when radiosity is run. Otherwise zeroed.
- distant light 0 direction (`Vector3D`): For lightmapped materials (_bitmap_ index >= 0), always set to IJK `(-0.577, -0.577, -0.577)` when radiosity is run. Otherwise zeroed.
- distant light 1 color (`ColorRGB`): For lightmapped materials (_bitmap_ index >= 0), always set to RGB `(0.4, 0.4, 0.5)` when radiosity is run. Otherwise zeroed.
- distant light 1 direction (`Vector3D`): For lightmapped materials (_bitmap_ index >= 0), always set to IJK `(0, 0, 1)` when radiosity is run. Otherwise zeroed.
- reflection tint (`ColorARGB`): For lightmapped materials (_bitmap_ index >= 0), always set to ARGB `(0.5, 1, 1, 1)` when radiosity is run. Otherwise zeroed.
- shadow vector (`Vector3D`): For lightmapped materials (_bitmap_ index >= 0), always set to IJK `(0, 0, -1)` when radiosity is run. Otherwise zeroed.
- shadow color (`ColorRGB`): For lightmapped materials (_bitmap_ index >= 0), always set to RGB `(0, 0, 0)` when radiosity is run. Otherwise zeroed.
- plane (`Plane3D`?): For _coplanar_ materials, the world-space plane definition of its faces. Otherwise zeroed.
- breakable surface (`uint16`): Index into the _breakable surfaces_ block if this surface is breakable. Otherwise `-1`.
- rendered vertices type (`enum`): * Cache only Indicates how rendered vertices are stored.
- structure bsp uncompressed rendered vertices (`0x0`)
- structure bsp compressed rendered vertices (`0x1`)
- structure bsp uncompressed lightmap vertices (`0x2`)
- structure bsp compressed lightmap vertices (`0x3`)
- model uncompressed (`0x4`)
- model compressed (`0x5`)
- rendered vertices count (`uint32`): The number of rendered vertices for this material, found at _rendered vertices offset_. This count is always equal to _lightmap vertices count_ when the bitmap index is not `-1`. The number of vertices may increase after lightmaps are generated, as Tool may need to introduce lightmap UV seams.
- rendered vertices offset (`uint32`): Always `0`.
- unknown pointer (`ptr32`): on PC, this gets changed to a pointer to something when the BSP is loaded in-game
- rendered vertices index pointer (`ptr32`): * H1X only * Cache only (Xbox only) an indirect pointer to the rendered vertices
- lightmap vertices type (`enum`?): * Cache only Indicates how lightmap vertices are stored.
- lightmap vertices count (`uint32`): The number of lightmap vertices for this material, found at _lightmap vertices offset_. For lit materials, this count is always equal to _rendered vertices count_ when the bitmap index is not `-1`. Otherwise for unlit materials this will be `0`.
- lightmap vertices offset (`uint32`): Always `0`.
- lightmap vertices index pointer (`ptr32`): * H1X only * Cache only (Xbox only) an indirect pointer to the lightmap vertices
- uncompressed vertices (`TagDataOffset`): A data block storing uncompressed rendered and lightmap vertices. Both vertex types are stored in the same continuous block beginning with all rendered vertices, each 56 bytes: * Position * Normal * Binormal * Tangent * Texture UV This is followed by all lightmap vertices, each 20 bytes: * Incident radiosity vector (used for dynamic object shadows and environmental bump mapping) * Lightmap UV (for lightmap page indicated by _bitmap_ index)
- size (`uint32`)
- external (`uint32`)
- file offset (`uint32`)
- pointer (`ptr64`)
- compressed vertices (`TagDataOffset`?): A data block storing compressed rendered and lightmap vertices. Both vertex types are stored in the same continuous block beginning with all rendered vertices, each 32 bytes: * Position (uncompressed Vector3D) * Normal (encoded in a 32-bit integer) * Binormal (encoded in a 32-bit integer) * Tangent (encoded in a 32-bit integer) * Texture UV (uncompressed Vector2D) This is followed by all lightmap vertices, each 8 bytes: * Incident radiosity vector (encoded in a 32-bit integer) * Lightmap UV (encoded in a 32-bit integer)
| lens flares | `Block` | * HEK max count: 256 Lists the types of lens flares used by this BSP's _lens flare markers_. |
Field values:
- lens (`TagDependency`: lens_flare): Tag reference to a lens flare.
| lens flare markers | `Block` | * HEK max count: 65536 Points within the BSP where a _lens flare_ will be rendered. |
Field values:
- position (`Point3D`): The 3D position of the lens flare.
- direction i component (`int8`)
- direction j component (`int8`)
- direction k component (`int8`)
- lens flare index (`int8`): Determines which flare from the _lens flares_ block is rendered at this position.
| clusters | `Block` | * HEK max count: 8192 |
Option values:
- sky (`uint16`): Sets which sky is visible when within this cluster. Indexes the _skies_ block of the scenario tag, so a value of `0` would mean the first sky, `1` the second, etc. A value of `-1` marks this cluster as indoor/interior for the purposes of certain sky tag fields. Tool appears to mark clusters as indoor when there are no outdoor clusters visible from it according to the PVS. Tool validates that all sky faces in a cluster are for the same sky. For example, a cluster cannot contain both `+sky0` and `+sky1` materials. When a player moves between clusters with different skies, fog colour will smoothly transition by cumulative camera movement over approximately 20 world units.
- fog (`uint16`)
- background sound (`uint16`)
- sound environment (`uint16`)
- weather (`uint16`)
- transition structure bsp (`uint16`)
- first decal index (`uint16`): * Cache only
- decal count (`uint16`): * Cache only
- predicted resources (`Block`): * HEK max count: 1024 * Cache only
- type (`enum`)
- bitmap (`0x0`)
- sound (`0x1`)
- resource index (`uint16`)
- tag (`uint32`)
- subclusters (`Block`): * HEK max count: 4096
- * Read-only (Field): Type
- Comments (): ---
- world bounds x (`Bounds`?): X axis bounds for the surfaces in this subcluster.
- world bounds y (`Bounds`?): Y axis bounds for the surfaces in this subcluster.
- world bounds z (`Bounds`?): Z axis bounds for the surfaces in this subcluster.
- surface indices (`Block`): Indices of renderable surfaces (triangles) present in this subcluster.
- * Read-only (Field): Type
- Comments (): ---
- index (`int32`): The index of a _surface_ in this subcluster. This value may change after radiosity is run if surfaces are reordered.
- first lens flare marker index (`uint16`)
- lens flare marker count (`uint16`)
- surface indices (`Block`): * HEK max count: 32768 * Unused Always empty, appears unused by the game. This was likely replaced by subcluster surfaces indices at some point in development.
- mirrors (`Block`): * HEK max count: 16
- * Read-only (Field): Type
- Comments (): ---
- plane (`Plane3D`?)
- shader (`TagDependency`: shader)
- vertices (`Block`): * HEK max count: 512
- * Read-only (Field): Type
- Comments (): ---
- point (`Point3D`)
- portals (`Block`): * HEK max count: 128
- * Read-only (Field): Type
- Comments (): ---
- portal (`uint16`)
| cluster data | `TagDataOffset`? | Contains the cluster PVS, a symmetric lookup table of inter-cluster visibility. The data structure can be thought of as a sequence of "rows", one per cluster. Each row is a wide bitfield indicating which other clusters are visible, with the row length being the lowest multiple of 32 bits able to hold the number of clusters in the BSP. For example, to test if cluster `a` can see cluster `b`: ``` let row_size = num_clusters.div_ceil(32) * 4; let col_index = b / 8; let mask: u8 = 1 << (b % 8); let visible = (data[a * row_size + col_index] & mask) != 0; ``` For example, Timberland has 133 clusters. This means it needs 20 bytes per row (`5 * 32 = 160` bits). At 20 bytes per cluster row, this gives `133 * 20 = 2660` total bytes of PVS data. |
| cluster portals | `Block` | * HEK max count: 512 |
Flag values:
- front cluster (`uint16`)
- back cluster (`uint16`)
- plane index (`int32`)
- centroid (`Point3D`)
- bounding radius (`float`)
- flags (`bitfield`)
- ai can simply not hear through all this amazing stuff darn it (`0x1`)
- vertices (`Block`): * HEK max count: 128
- * Read-only (Field): Type
- Comments (): ---
- point (`Point3D`)
| breakable surfaces | `Block` | * HEK max count: 256 |
Field values:
- centroid (`Point3D`)
- radius (`float`)
- collision surface index (`int32`)
| fog planes | `Block` | * HEK max count: 32 |
Field values:
- front region (`uint16`)
- material type (`enum`?): * Cache only
- plane (`Plane3D`?)
- vertices (`Block`): * HEK max count: 2048
- * Read-only (Field): Type
- Comments (): ---
- point (`Point3D`)
| fog regions | `Block` | * HEK max count: 32 |
Field values:
- fog (`uint16`→)
- weather palette (`uint16`→)
| fog palette | `Block` | * HEK max count: 32 * Max: 65534 |
Field values:
- name (`TagString`)
- fog (`TagDependency`: fog)
- fog scale function (`TagString`)
| weather palette | `Block` | * HEK max count: 32 Weather properties combining wind and weather particles which can be assigned to clusters. Levels typically have separate entries for indoor and outdoor locations so the amount of wind will vary. Any applicable weather particle systems may need to be assigned to both indoor and outdoor palette entries, with weather polyhedra used to mask weather from indoor locations. |
Field values:
- name (`TagString`): Gives a name to this palette entry so it can be easily recognized when assigning weather to clusters.
- particle system (`TagDependency`: weather_particle_system): References the weather_ particle_ system tag used by the palette entry, which controls the appearance and behaviour of weather particles such as snow or rain.
- particle system scale function (`TagString`)
- wind (`TagDependency`: wind): References the wind tag used by the palette entry, which controls the base speed and directional variability of the wind.
- wind direction (`Vector3D`): Controls the direction of wind in the assigned cluster. The `i`, `j`, and `k` components correspond to the `x` and `y` and `z` world axes, with positive values meaning the wind moves in the positive world axis direction. For example, a wind vector of `1 1 0` blows at the angle between the +x and +y directions, while `0 0 -1` blows straight down (-z). This vector is normalized by the engine, so larger numbers do not increase the speed (magnitude) of the wind; the direction `2 4 0` is the same as `1 2 0`. If this vector is `0 0 0` then the game will use `1 0 0` as the default.
- wind magnitude (`float`): Scales the base wind speed of the palette entry's wind tag. For example, if the wind tag specifies a _velocity_ of 2 wu/s, and this magnitude is 3, then the overall wind speed will be 6 wu/s. This number can be negative to reverse the wind direction, though it's clearer to change the direction vector above. This also scales the wind's local variation but only when acting on particles with _use simple wind_ point_ physics.
- wind scale function (`TagString`): It's unknown how to use this field, or if it's even used by the engine. Various string values seem to have no effect on the wind.
| weather polyhedra | `Block` | * HEK max count: 32 Convex polyhedral volumes where weather particle systems will not render. These should not be edited by hand, but rather by assigning the material name`+weatherpoly` to these volumes when modeling your level. |
Field values:
- bounding sphere center (`Point3D`): The world space location of the weather polyhedron's center point.
- bounding sphere radius (`float`): The radius around the center point where the polyhedron has an effect. A polyhedron's plane culling will apply _globally_ if its bounding sphere is within range of the current weather_ particle_ system's _fade out end distance_.
- planes (`Block`): * HEK max count: 16 A series of planes which define the shape of the volume. A weather particle is displayed if it's on the back side of any of these planes. If no planes are present then no particles are displayed if the polyhedron is in range.
- * Read-only (Field): Type
- Comments (): ---
- plane (`Plane3D`?): The plane, defined in terms of a normal vector and distance from the polyhedron center.
| pathfinding surfaces | `Block` | * HEK max count: 131072 This block has equal length to collision _surfaces_, and specifies which corresponding surfaces can be used for pathfinding by AI. |
Field values:
- data (`int8`): Has the value `0` if the collision surface cannot be used for pathfinding (e.g. too steep), and `64` otherwise. It's unknown if this is a bitfield and if other values may appear depending on the surface type.
| pathfinding edges | `Block` | * HEK max count: 262144 * Unused Always empty, likely unused by the game. |
| background sound palette | `Block` | * HEK max count: 64 |
Field values:
- name (`TagString`)
- background sound (`TagDependency`: sound_looping)
- scale function (`TagString`)
| sound environment palette | `Block` | * HEK max count: 64 |
Field values:
- name (`TagString`)
- sound environment (`TagDependency`: sound_environment)
| sound pas data | `TagDataOffset`? | Contains the _potentially audible set_ (PAS) data. Similar to the PVS with visibility, this encodes which clusters can potentially hear sounds from other clusters. Custom Edition doesn't make use of this data but H1A does. |
| markers | `Block` | * HEK max count: 1024 Named points with orientation within the BSP. They are similar to gbxmodel markers but are not to be confused with scenario _cutscene flags_. These markers become visible in Sapien when enabling _snap to markers_ in the tool window. |
Field values:
- name (`TagString`)
- rotation (`Quaternion`)
- position (`Point3D`)
| detail objects | `Block` | * HEK max count: 1 Storage location for detail_ object_ collection painted onto this BSP. |
Field values:
- cells (`Block`): * HEK max count: 262144 Cells organize detail objects instances into 8x8 world unit groups.
- cell x (`int16`)
- cell y (`int16`)
- cell z (`int16`)
- offset z (`int16`)
- valid layers flags (`int32`)
- start index (`int32`): An index into _instances_ where this cell's individual detail objects start.
- count index (`int32`): An index into _counts_. The indexed element determines how many detail objects belong to this cell.
- instances (`Block`): * HEK max count: 2097152 Contains all individual detail objects. Cells index into this block with to get the list of detail objects in that cell.
- position x (`int8`)
- position y (`int8`)
- position z (`int8`)
- data (`int8`)
- color (`int16`)
- counts (`Block`): * HEK max count: 8388608 Contains counts of detail object instances for each cell. This block is indexed by each cell's _count index_. It's not clear why this layer of indirection exists. Cells with identical instance counts do not share count elements.
- count (`int16`): The number of detail object instances in the cell.
- z reference vectors (`Block`): * HEK max count: 262144
- z reference i (`float`)
- z reference j (`float`)
- z reference k (`float`)
- z reference l (`float`)
- bullshit (`uint8`): * Cache only
| runtime decals | `Block` | * HEK max count: 6144 * Cache only |
Field values:
- position (`Point3D`)
- decal type (`uint16`)
- yaw (`int8`)
- pitch (`int8`)
| leaf map leaves | `Block` | * HEK max count: 65536 |
Field values:
- faces (`Block`): * HEK max count: 256
- * Read-only (Field): Type
- Comments (): ---
- node index (`int32`)
- vertices (`Block`): * HEK max count: 64
- * Read-only (Field): Type
- Comments (): ---
- vertex (`Point2D`)
- x (`float`)
- y (`float`)
- portal indices (`Block`): * HEK max count: 256
- * Read-only (Field): Type
- Comments (): ---
- portal index (`int32`)
| leaf map portals | `Block` | * HEK max count: 524288 |
Field values:
- plane index (`int32`)
- back leaf index (`int32`)
- front leaf index (`int32`)
- vertices (`Block`)
- * Read-only (Field): Type
- Comments (): ---
- point (`Point3D`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `scenario_structure_bsp`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
