# Halo 1 Tag: model

## Summary

The model tag contains the render model and shader_ model references for objects") such as vehicles, scenery, and weapons among others. It is not collideable nor animated on its own, and objects may reference additional model_ collision_ geometry and model_ animations tags.

## Tag Information

Tag group: `model`.

Defined fields and enum values mentioned:

- base map u scale
- base map v scale
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
- nodes first child node index
- nodes name
- nodes next sibling node index
- nodes node distance from parent
- nodes parent node index
- nodes rotation

## Details

The model tag contains the render model and shader_ model references for objects") such as vehicles, scenery, and weapons among others. It is not collideable nor animated on its own, and objects may reference additional model_ collision_ geometry and model_ animations tags.

**This tag is specific to Halo 1 for Xbox**, while the gbxmodel tag is the equivalent for the PC port and its derivatives like MCC. Unlike a Gearbox model, this tag's compressed vertices are copied into the map rather than uncompressed vertices. Note that compressed vertices can only address up to 42 nodes.

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
| super low detail node count | `uint16` | * Cache only * Unit: nodes |
| low detail node count | `uint16` | * Cache only * Unit: nodes |
| medium detail node count | `uint16` | * Cache only * Unit: nodes |
| high detail node count | `uint16` | * Cache only * Unit: nodes |
| super high detail node count | `uint16` | * Cache only * Unit: nodes |
| base map u scale | `float` | * Default: 1 |
| base map v scale | `float` | * Default: 1 |
| markers | `Block` | * HEK max count: 256 * Hidden * Max: 0 |
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
| shaders | `Block` | * HEK max count: 32 |
Field values:
- shader (`TagDependency`: shader): * Non-null
- permutation (`uint16`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `model`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
