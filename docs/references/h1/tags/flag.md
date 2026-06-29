# Halo 1 Tag: flag

## Summary

The **flag** tag describes the behaviour and appearance of a cloth-like material. As a type of _widget_, flags can be attached to objects like weapons using model markers. They are not limited to weapons and can even be attached to bipeds, however the game only supports rendering 2 flags.

## Tag Information

Tag group: `flag`.

HaloScript entries:

- `debug_point_physics [boolean]`

Defined fields and enum values mentioned:

- air friction
- attached edge shape
- attached edge shape concave triangular
- attached edge shape flat
- attachment points
- attachment points height to next attachment
- attachment points marker name
- blue flag shader
- bounding radius
- cell height
- cell width
- density
- elasticity
- flags
- flags collides with structures
- flags no gravity
- flags unused
- height
- netgame flags usage id
- physics
- red flag shader
- trailing edge shape
- trailing edge shape concave triangular
- trailing edge shape convex triangular
- trailing edge shape flat
- trailing edge shape offset
- trailing edge shape trapezoid short bottom
- trailing edge shape trapezoid short top
- width
- wind noise

## Details

The **flag** tag describes the behaviour and appearance of a cloth-like material. As a type of _widget_, flags can be attached to objects like weapons using model markers. They are not limited to weapons and can even be attached to bipeds, however the game only supports rendering 2 flags.

They are essentially a grid of attached point_ physics vertices forming a rectangular shape with configurable density and cell sizes. One edge is deemed the _attached edge_ and can have several _attachment points_ to the object, while the opposite edge is deemed the _trailing edge_ and flows freely. Because they make use of point_ physics, flags are affected by wind.

### Physics

The spaces between flag vertices are called _cells_ and have configurable _cell width_ and _cell height_. The cells try to maintain their size and are capable of stretching. Like antenna, flags can make use of a variety of point physics features:

* If _no gravity_ is set then the flag will float around rather than hang.
* If _collides with structures_ is set, the points may inconsistently collide with the BSP. Depending on the other settings, the flag may come to a rest on the BSP for a time before hanging through it. Collisions with model_ collision_ geometry seem to require much lower point velocities and are barely noticeable.
* _Density_ affects how "floaty" the points are. Low near-zero values will cause the flag to hang in the air, while higher values result in it falling more quickly.
* _Air friction_ is especially important for flags given their interaction with wind and movement when carried.
* _Elasticity_ determines the bounciness of collisions with structures, not how stretchy the flag is.

### Simulation culling

Flags are not simulated when off-screen (based on the bounding sphere") of their object). When they reappear you may notice a slight jiggle as they reach their rest state again.

### Attachment points

Along the _attachment edge_ of the flag you can define a series of 2-4 attachment points at a model marker. The flag's height will span these attachment points and be fixed in place at each of them, with a configurable number of cells between the attachment points. The markers used don't need to be unique; it would be possible for the leading edge to form a loop by reusing a marker for the first and final attachment points. The game requires that the sum of all attachment points' _height to next attachment_ is 1 less than _height_. The final attachment point should use height `0`.

A 9x9 vertex flag with 3 attachment points at markers `flag top` (A), `flag bottom` (B), and `left hand cyborg` (C). A and B have _height to next attachment_ set to `4`.

### Shapes

This tag supports various shapes for the attached and trailing edges of the flag. Different _trailing edge shape_ options result in the trailing edge of the flag being visually cut off (though the vertices still exist). The _trailing edge shape offset_ allows you to slide this mask along the width of the flag by a positive or negative number of vertices.

The _attached edge shape_ only supports _flat_ or _concave triangular_, and occurs between each pair of attachment points.

Examples of various trailing edge shapes. The trailing edge offsets are `0` in most of these examples. In the bottom right image, an attached edge shape is used and the trailing edge offset is `-3`.

### Limits

The game state has only 2 entries for flags in all versions of H1, so any other flags on the map will not render.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_point_physics [boolean])` Renders green or red markers wherever point_ physics are being simulated. This includes flags, antenna, contrails, particles, and particle_ systems. For weather_ particle_ system, markers are only shown in their simulation cube and while the `weather` global not disabled. Red markers indicate point_ physics with the _collides with structures_ flag, which are more computationally expensive. It can help to enable `framerate_throttle` for these markers to render consistently. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- unused (`0x1`)
| trailing edge shape | `enum` | The shape of the trailing edge. |
Option values:
- flat (`0x0`): A simple straight edge, used by the game's stock flag.
- concave triangular (`0x1`): A triangular cutout forms two points at the corners.
- convex triangular (`0x2`): The trailing edge's corners are cut off, forming an arrow.
- trapezoid short top (`0x3`): The trailing edge is cut at an angle with the top edge short.
- trapezoid short bottom (`0x4`): The trailing edge is cut at an angle with the bottom edge short.
| trailing edge shape offset | `int16` | * Unit: vertices Determines the number of vertices the trailing edge shape is offset. A value `0` places the shape exactly on the trailing edge. Positive values will push it off the edge of the flag while negative values will move it inwards. |
| attached edge shape | `enum` | The shape of the attached edge. This will appear between each pair of attachment points. |
Option values:
- flat (`0x0`): A simple straight edge, used by the game's stock flag.
- concave triangular (`0x1`): The edge will have a triangular cutout between attachment points.
| width | `int16` | * Unit: vertices Flag size from attached to trailing edge. This counts the number of vertices (cells+1), not the number of cells. |
| height | `int16` | * Unit: vertices Flag size along the direction of attachment. This counts the number of vertices (cells+1), not the number of cells. This value should be 1 higher than the sum of all attachment points' _height to next attachment_ or else the flag will not appear. This must also be an odd number or else the flag will not appear or be streteched to the world origin (0, 0, 0). |
| cell width | `float` | * Unit: world units Width of the cell between each pair of vertices. |
| cell height | `float` | * Unit: world units Height of the cell between each pair of vertices. |
| red flag shader | `TagDependency`: shader | The appearance of the flag when used for red team (netgame flag team index`0`). |
| physics | `TagDependency`: point_physics | The physics to use for each vertex of the flag. |
| wind noise | `float` | * Unit: world units per second |
| blue flag shader | `TagDependency`: shader | The appearance of the flag when used for blue team (netgame flag team index`1`). |
| attachment points | `Block` | * HEK max count: 4 A series of attachment points for the flag's attached edge. There must be at least 2 attachment points or the flag will be ignored. |
Field values:
- height to next attachment (`int16`): * Unit: vertices The number of cells between this attachment point and the next. The final attachment point should use the value `0`. For example, a flag with 3 attachment points and _height_ of `9` vertices should use attachment point heights of `4`, `4`, and `0` respectively.
- marker name (`TagString`): Name of the model marker where this point attaches. If the name is empty or the marker does not exist then the point will attach to the object's root node.

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `flag`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
